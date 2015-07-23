/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <IDT_Tsi721.h>
#include <sched.h>
#include "tsi721_config.h"
#include "time_utils.h"
#include "latency_threads.h"



#include <rapidio_mport_mgmt.h>#include <rapidio_mport_rdma.h>#include <rapidio_mport_sock.h>

#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */

/*
 * Initialization patterns. All bytes in the source buffer has bit 7
 * set, all bytes in the destination buffer has bit 7 cleared.
 *
 * Bit 6 is set for all bytes which are to be copied by the DMA
 * engine. Bit 5 is set for all bytes which are to be overwritten by
 * the DMA engine.
 *
 * The remaining bits are the inverse of a counter which increments by
 * one for each byte address.
 */
#define PATTERN_SRC		0x80
#define PATTERN_DST		0x00
#define PATTERN_COPY		0x40
#define PATTERN_OVERWRITE	0x20
#define PATTERN_COUNT_MASK	0x1f

#define LAST_VAL(X) (PATTERN_DST | PATTERN_OVERWRITE |  \
		(~(X) & PATTERN_COUNT_MASK));
/* Maximum amount of mismatched bytes in buffer to print */
#define MAX_ERROR_COUNT		32

#define TEST_BUF_SIZE (2 * 1024 * 1024)
#define DEFAULT_IBWIN_SIZE (2 * 1024 * 1024)

// Define configuration constants:
// Single Tsi721 in loopback
// Single Tsi721 either TX or RX
// Dual Tsi721 in the same chassis
#define CFG_LPBK 0
#define CFG_1_TX 1
#define CFG_1_RX 2
#define CFG_DUAL 3

struct dma_demo_setup {
	int use_thr;
	int fd;
	uint32_t mport_id;
	struct rio_mport_properties prop;
	int has_dma;
	int has_switch;

	uint32_t tgt_destid;
	uint32_t tgt_addr;
	uint32_t offset;
	int align;
	uint32_t dma_size;

	uint64_t ib_handle;
	volatile uint8_t *ibmap;
	int32_t ib_size;
	uint64_t rio_base;

	volatile uint8_t *buf_src;
	volatile uint8_t *buf_dst;

	int debug;
	int verify;

	int kbuf_mode; /* 0 - user space buffer, 1 = kernel contiguous buffer */
	uint32_t max_sge;
	uint32_t max_size;
	uint32_t max_hw_size;
	uint64_t src_handle;
	uint64_t dst_handle;

	int repeat;
} ;

struct dma_demo_setup ep[2];

static void dmatest_init_srcs(volatile uint8_t *buf, unsigned int start,
			      unsigned int len, unsigned int buf_size)
{
	unsigned int i;

	for (i = 0; i < start; i++)
		buf[i] = PATTERN_SRC | (~i & PATTERN_COUNT_MASK);
	for ( ; i < start + len; i++)
		buf[i] = PATTERN_SRC | PATTERN_COPY
			| (~i & PATTERN_COUNT_MASK);
	for ( ; i < buf_size; i++)
		buf[i] = PATTERN_SRC | (~i & PATTERN_COUNT_MASK);
}

static void dmatest_init_dsts(volatile uint8_t *buf, unsigned int start,
			      unsigned int len, unsigned int buf_size)
{
	unsigned int i;

	for (i = 0; i < start; i++)
		buf[i] = PATTERN_DST | (~i & PATTERN_COUNT_MASK);
	for ( ; i < start + len; i++)
		buf[i] = PATTERN_DST | PATTERN_OVERWRITE
			| (~i & PATTERN_COUNT_MASK);
	for ( ; i < buf_size; i++)
		buf[i] = PATTERN_DST | (~i & PATTERN_COUNT_MASK);
}

static void dmatest_mismatch(uint8_t actual, uint8_t pattern,
			     unsigned int index, unsigned int counter,
			     int is_srcbuf)
{
	uint8_t diff = actual ^ pattern;
	uint8_t expected = pattern | (~counter & PATTERN_COUNT_MASK);

	if (is_srcbuf)
		printf("srcbuf[0x%x] overwritten! Expected %02x, got %02x\n",
			index, expected, actual);
	else if ((pattern & PATTERN_COPY)
			&& (diff & (PATTERN_COPY | PATTERN_OVERWRITE)))
		printf("dstbuf[0x%x] not copied! Expected %02x, got %02x\n",
			index, expected, actual);
	else if (diff & PATTERN_SRC)
		printf("dstbuf[0x%x] was copied! Expected %02x, got %02x\n",
			index, expected, actual);
	else
		printf("dstbuf[0x%x] mismatch! Expected %02x, got %02x\n",
			index, expected, actual);
}

static unsigned int dmatest_verify(volatile uint8_t *buf, unsigned int start,
		unsigned int end, unsigned int counter, uint8_t pattern,
		int is_srcbuf)
{
	unsigned int i;
	unsigned int error_count = 0;
	uint8_t actual;
	uint8_t expected;
	unsigned int counter_orig = counter;

	counter = counter_orig;
	for (i = start; i < end; i++) {
		actual = buf[i];
		expected = pattern | (~counter & PATTERN_COUNT_MASK);
		if (actual != expected) {
			if (error_count < MAX_ERROR_COUNT)
				dmatest_mismatch(actual, pattern, i,
							counter, is_srcbuf);
			error_count++;
		}
		counter++;
	}

	if (error_count > MAX_ERROR_COUNT)
		printf("%u errors suppressed\n", error_count - MAX_ERROR_COUNT);

	return error_count;
}

static void *dmatest_buf_alloc(int fd, uint32_t size, uint64_t *handle)
{
	void *buf_ptr = NULL;
	uint64_t h;
	int ret;

	if (handle) {
		ret = riomp_dma_dbuf_alloc(fd, size, &h);
		if (ret) {
			if (ep[0].debug)
				printf("riomp_dma_dbuf_alloc failed err=%d\n", ret);
			return NULL;
		}

		buf_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
				MAP_SHARED, fd, h);
		if (buf_ptr == MAP_FAILED) {
			perror("mmap");
			buf_ptr = NULL;
			ret = riomp_dma_dbuf_free(fd, handle);
			if (ret && ep[0].debug)
				printf("riomp_dma_dbuf_free failed err=%d\n", ret);
		} else
			*handle = h;
	} else {
		buf_ptr = malloc(size);
		if (buf_ptr == NULL )
			perror("malloc");
	}

	return buf_ptr;
}

static void dmatest_buf_free(int fd, void *buf, uint32_t size, uint64_t *handle)
{
	if (handle && *handle) {
		int ret;

		if (munmap(buf, size))
			perror("munmap");

		ret = riomp_dma_dbuf_free(fd, handle);
		if (ret)
			printf("riomp_dma_dbuf_free failed err=%d\n", ret);
	} else if (buf)
		free(buf);
}

static int config_ibwin(int idx )
{
	int rc = EXIT_FAILURE;
	int ret;

	ret = riomp_dma_ibwin_map(ep[idx].fd, &ep[idx].rio_base, ep[idx].ib_size, 
						&ep[idx].ib_handle);
	if (ret) {
		printf("Failed to allocate IB buffer %d err=%d\n", idx, ret);
		goto exit;
	}

	ep[idx].ibmap = (volatile uint8_t *)mmap(NULL, ep[idx].ib_size, PROT_READ | PROT_WRITE, 
				MAP_SHARED, 
				ep[idx].fd, ep[idx].ib_handle);
	if (ep[idx].ibmap == MAP_FAILED) {
		printf("Failed to mmap IB buffer %d err=%d\n", idx, ret);
		perror("mmap");
		goto exit;
	}

	if (ep[idx].debug)
		printf("\tAllocated/mapped IB buffer %d\n", idx);
		printf("\t(h=0x%x_%x, loc=%p)\n", 
			(uint32_t)((ep[idx].ib_handle) >> 32),
			(uint32_t)((ep[idx].ib_handle) & 0xffffffff),
			ep[idx].ibmap);

	rc = EXIT_SUCCESS;

exit:
	return rc;
}

void cleanup_ibwin(int max_idx)
{
	int idx;
	int ret;

	for (idx = 0; idx <= max_idx; idx++) {
		printf("\nCleaning up ibwin for idx %d mport %d",
				idx, ep[idx].mport_id);
		if (NULL != ep[idx].ibmap) {
			if (munmap((void *)ep[idx].ibmap, ep[idx].ib_size))
				perror("munmap");
		} else {
			printf("\n\tNo ibwin mapping.");
		}

		if (ep[idx].ib_handle != 0xFFFFFFFFFFFFFFFF) {
			ret = riomp_dma_ibwin_free(ep[idx].fd, &ep[idx].ib_handle);
			if (ret)
			   printf("\n\tFailed to free %d IB buffer err=%d\n",
					idx, ret);
		} else {
			printf("\n\tNo ibwin.");
		}
	}
}

static int config_buffs(int max_idx )
{
	int rc = EXIT_FAILURE;
	int e_i;

	for (e_i = 0; e_i <= max_idx; e_i++) {
		if (ep[e_i].kbuf_mode)
			ep[e_i].max_hw_size = ep[e_i].max_size;
		else
			ep[e_i].max_hw_size = (ep[e_i].max_sge == 0)?
				TEST_BUF_SIZE:(ep[e_i].max_sge * getpagesize());

		/* check specified DMA block size */
		if ((ep[e_i].dma_size <= 0) ||
	    		(ep[e_i].dma_size > TEST_BUF_SIZE) || 
			(ep[e_i].dma_size > ep[e_i].max_hw_size)) {
			printf("ERR: invalid buffer size parameter\n");
			printf("     max allowed buffer size: %d bytes\n",
		       		(ep[e_i].max_hw_size > TEST_BUF_SIZE)?
				TEST_BUF_SIZE:ep[e_i].max_hw_size);
			goto exit;
		}

		if (((ep[e_i].dma_size + ep[e_i].offset) > TEST_BUF_SIZE) ||
			(ep[e_i].dma_size == 0)) {
			printf("ERR: invalid transfer size/offset combination\n");
			goto exit;
		} else
			printf("\tdma_size=%d offset=0x%x\n", ep[e_i].dma_size,
				ep[e_i].offset);

		ep[e_i].buf_src = (volatile uint8_t *)dmatest_buf_alloc(ep[e_i].fd, TEST_BUF_SIZE, 
				ep[e_i].kbuf_mode?&ep[e_i].src_handle:NULL);
		if (ep[e_i].buf_src == NULL) {
			printf("DMA Test: error allocating SRC buffer\n");
			goto exit;
		}

		ep[e_i].buf_dst = (volatile uint8_t *)dmatest_buf_alloc(ep[e_i].fd, TEST_BUF_SIZE,
				ep[e_i].kbuf_mode?&ep[e_i].dst_handle:NULL);
		if (ep[e_i].buf_dst == NULL) {
			printf("DMA Test: error allocating DST buffer\n");
			goto exit;
		}
	};

	rc = EXIT_SUCCESS;
exit:
	return rc;
}

void cleanup_buffs(int max_idx)
{	
	int idx;

	printf("\nFreeing source buffers...");
	for (idx = 0; idx <= max_idx; idx++) {
		if (ep[idx].buf_src)
			dmatest_buf_free(ep[idx].fd, (void *)ep[idx].buf_src, 
				TEST_BUF_SIZE,
				ep[idx].kbuf_mode?&ep[idx].src_handle:NULL);
		if (ep[idx].buf_dst)
			dmatest_buf_free(ep[idx].fd, (void *)ep[idx].buf_dst, 
				TEST_BUF_SIZE,
				ep[idx].kbuf_mode?&ep[idx].dst_handle:NULL);
	};
};

int open_mports(int last_idx) 
{
	int idx;
	int rc = EXIT_FAILURE;

	for (idx = 0; idx <= last_idx; idx++) {
		ep[idx].fd = riomp_mgmt_mport_open(ep[idx].mport_id, 0);
		if (ep[idx].fd < 0) {
			printf("DMA Test: unable to open idx %d mport%d device err=%d\n",
				idx, ep[idx].mport_id, errno);
			goto exit;
		}

		if (!riodp_query_mport(ep[idx].fd, &ep[idx].prop)) {
			display_mport_info(&ep[idx].prop);

			if (ep[idx].prop.flags & RIO_MPORT_DMA) {
				ep[idx].align = ep[idx].prop.dma_align;
				ep[idx].max_sge = ep[idx].prop.dma_max_sge;
				ep[idx].max_size = ep[idx].prop.dma_max_size;
			} else {
				ep[idx].has_dma = 0;
				printf("No DMA support. Test aborted.\n\n");
				goto exit;
			};

/*
			if (ep[idx].attr.link_speed == 0) {
				printf("SRIO link is down. Test aborted.\n");
				rc = EXIT_FAILURE;
				goto exit;
			}
*/
		} else {
			printf("Failed to obtain mport information\n");
			printf("Using default configuration\n\n");
		}
	}
	rc = EXIT_SUCCESS;
exit:
	return rc;
};

void close_mports(int last_idx) 
{
	int idx;

	printf("\nClosing master port(s)...\n");
	for (idx = 0; idx <= last_idx; idx++) {
		if (ep[idx].fd > 0)
			close(ep[idx].fd);
	}
};

void fixup_options(int config)
{
	if (CFG_LPBK == config) {
		if ((ep[0].rio_base != ep[0].tgt_addr) ||
		    (ep[0].tgt_destid != ep[0].prop.hdid))
			printf("\nCorrecting parameters for loopback...\n");
		ep[0].rio_base = ep[0].tgt_addr;
		ep[0].tgt_destid = ep[0].prop.hdid;
	};

	if ((CFG_1_TX == config) ||
	    (CFG_1_RX == config)) {
		if (ep[0].repeat > 1) {
			printf("\nCorrecting parameters for one Tsi721...\n");
			printf("NOTE: This test must be run one at a time.\n");
		}
		ep[0].repeat = 1;
		ep[1].repeat = 1;
	};

	if (CFG_DUAL == config) {
		if ((ep[0].rio_base != ep[1].tgt_addr) ||
		    (ep[0].tgt_destid != ep[1].prop.hdid))
			printf("\nCorrecting parameters for dual config...\n");
		ep[0].rio_base = ep[1].tgt_addr;
		ep[0].tgt_destid = ep[1].prop.hdid;
	};
};

static void display_help(char *program)
{
	printf("%s - test DMA data transfers to/from RapidIO device\n",	program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("options are:\n");
	printf("Common:\n");
	printf("  -c 0|1|2\n");
	printf("  --config 0|1|2|3\n");
	printf("    0 - Tsi721 in loopback\n");
	printf("    1 - Tsi721 connected to another chassis, this end TX\n");
	printf("    2 - Tsi721 connected to another chassis, this end RX\n");
	printf("    3 - Two Tsi721 in same chassis, cabled to each other\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -D xxxx\n");
	printf("  --destid xxxx\n");
	printf("    destination ID of target RapidIO device\n");
	printf("  -A xxxx\n");
	printf("  --taddr xxxx\n");
	printf("    memory address in target RapidIO device\n");
	printf("  -S xxxx\n");
	printf("  --size xxxx\n");
	printf("    data transfer size in bytes (default 0x100)\n");
	printf("  -O xxxx\n");
	printf("  --offset xxxx\n");
	printf("    offset in local data src/dst buffers (default=0)\n");
	printf("  -a n\n");
	printf("  --align n\n");
	printf("    data buffer address alignment\n");
	printf("  -T n\n");
	printf("  --repeat n\n");
	printf("    repeat test n times (default=1)\n");
	printf("  -I xxxx\n");
	printf("  --ibwin xxxx\n");
	printf("    inbound window (memory) size in bytes\n");
	printf("  -R xxxx\n");
	printf("  --ibbase xxxx\n");
	printf("    inbound window base address in RapidIO address space\n");
	printf("  -v turn off buffer data verification\n");
	printf("  -d or --debug\n");
	printf("  -h or --help\n");
	printf("  -k use coherent kernel space buffer allocation\n");
	printf("  -t use separate threads for completion measurement\n");
	printf("  -i\n");
	printf("    allocate and map inbound window (memory) using default parameters\n");
	printf("  -W Switch is present between the Tsi721s\n");
	printf("\n");
}


void parse_options(int argc, char** argv, int *config)
{
	char *program = argv[0];
	int idx;
	int option;
	static const struct option options[] = {
		{ "destid", required_argument, NULL, 'D' },
		{ "taddr",  required_argument, NULL, 'A' },
		{ "size",   required_argument, NULL, 'S' },
		{ "offset", required_argument, NULL, 'O' },
		{ "align",  required_argument, NULL, 'a' },
		{ "repeat", required_argument, NULL, 'T' },
		{ "ibwin",  required_argument, NULL, 'I' },
		{ "ibbase", required_argument, NULL, 'R' },
		{ "mport",  required_argument, NULL, 'M' },
		{ "config", required_argument, NULL, 'c' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};

	*config = 0;
	for (idx = 0; idx < 2; idx++) {
		ep[idx].use_thr = 0;
		ep[idx].fd = -1;
		ep[idx].mport_id = 0;
		ep[idx].has_dma = 1;
		ep[idx].has_switch = 0;
		ep[idx].fd = -1;

		ep[idx].tgt_destid = idx ^ 1;
		ep[idx].tgt_addr = 0x01000000;
		ep[idx].offset = 0;
		ep[idx].align = 0;
		ep[idx].dma_size = 0;

		ep[idx].ib_handle = -1;
		ep[idx].ibmap = NULL;
		ep[idx].ib_size = 0;
		ep[idx].rio_base = 0x01000000;

		ep[idx].buf_src = NULL;
		ep[idx].buf_dst = NULL;

		ep[idx].debug = 0;
		ep[idx].verify = 1;

		ep[idx].kbuf_mode = 0;
		ep[idx].max_sge = 32;
		ep[idx].max_size = 4096;
		ep[idx].src_handle = 0;
		ep[idx].dst_handle = 0;

		ep[idx].repeat = 1;
	};

	while (1) {
		option = getopt_long_only(argc, argv,
				"vdhkiWtc:M:D:A:S:O:a:T:I:R:", options, NULL);
		if (option == -1) {
			if (CFG_DUAL == *config) {
				if (ep[0].mport_id)
					ep[1].mport_id = 0;
				else
					ep[1].mport_id = 1;
			}
			return;
		}
		switch (option) {
			/* DMA Data Transfer Mode options*/
		case 'A':
			ep[0].tgt_addr = ep[1].tgt_addr = strtoul(optarg, NULL, 0);
			break;
		case 'a':
			ep[0].align = ep[1].align = strtol(optarg, NULL, 0);
			break;
		case 'c':
			*config = strtol(optarg, NULL, 0);
			if ((*config >= 0) && (*config <= 3))
				break;
			printf("\nIllegal config value: %d", *config);
			goto print_help;
		case 'D':
			ep[0].tgt_destid = ep[1].tgt_destid = strtol(optarg, NULL, 0);
			break;
		case 'O':
			ep[0].offset = ep[1].offset = strtol(optarg, NULL, 0);
			break;
		case 'S':
			ep[0].dma_size = ep[1].dma_size = strtol(optarg, NULL, 0);
			break;
		case 'T':
			ep[0].repeat = ep[1].repeat = strtol(optarg, NULL, 0);
			break;
		case 'k':
			ep[0].kbuf_mode = ep[1].kbuf_mode = 1;
			break;
		case 't':
			ep[0].use_thr = ep[1].use_thr = 1;
			break;
		case 'I':
			ep[0].ib_size = ep[1].ib_size = strtol(optarg, NULL, 0);
			break;
		case 'i':
			ep[0].ib_size = ep[1].ib_size = DEFAULT_IBWIN_SIZE;
			break;
		case 'R':
			ep[0].rio_base = ep[1].rio_base = 
				strtol(optarg, NULL, 0);
			break;
			/* Options common for all modes */
		case 'M':
			ep[0].mport_id = strtol(optarg, NULL, 0);
			break;
		case 'v':
			ep[0].verify = ep[1].verify = 0;
			break;
		case 'd':
			ep[0].debug = ep[1].debug = 1;
			break;
		case 'W':
			ep[0].has_switch = ep[1].has_switch = 1;
			break;
		default:
			printf("\nUnknown option: \'%c\'", option);
		case 'h':
			goto print_help;
			break;
		}
	}
print_help:
	display_help(program);
	exit(EXIT_SUCCESS);
};

void display_pre_dma_debug(int e_i)
{
	uint32_t last_idx = ep[e_i].offset + ep[e_i].dma_size - 1;
	uint8_t last_val = LAST_VAL(last_idx);
	volatile uint8_t *t_buf = ep[e_i].ibmap;
	volatile uint8_t *s_buf = ep[e_i].buf_src;

	if (!ep[e_i].debug)
		return;

	printf("<%d>: len=0x%x src_off=0x%x dst_off=0x%x\n",
		e_i, ep[e_i].dma_size, 
		ep[e_i].offset, ep[e_i].offset);

	printf("\tWrite %d bytes from src offset 0x%x\n", 
		ep[e_i].dma_size, ep[e_i].offset);

	printf("\tsrc  : %x %x %x %x %x %x %x %x Last %x Val %x\n", 
		s_buf[0], s_buf[1], s_buf[2], s_buf[3], 
		s_buf[last_idx-3], s_buf[last_idx-2],
		s_buf[last_idx-1], s_buf[last_idx], 
		last_idx, last_val);
	printf("\tibmap: %x %x %x %x %x %x %x %x Last %x Val %x\n", 
		t_buf[0], t_buf[1],t_buf[2],t_buf[3], 
		t_buf[last_idx-3], t_buf[last_idx-2],
		t_buf[last_idx-1], t_buf[last_idx], 
		last_idx, last_val);
};

sem_t poll_thread_started;
pthread_t poll_thread;
pthread_t main_thread;

struct poll_thread_parms {
	struct timespec ts;
	uint32_t lim;
};

#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

void *poll_for_completion_thread(void *parm)
{
	struct poll_thread_parms *ts_parm = (struct poll_thread_parms *)(parm);
	uint32_t e_i = ts_parm->ts.tv_nsec;
	uint32_t last_idx = ts_parm->ts.tv_sec;
	uint8_t last_val = LAST_VAL(last_idx);
	uint32_t lim = 0, max_lim = ts_parm->lim;
 
	ts_parm->ts = (struct timespec){0, 0};

	if (migrate_thread_to_cpu(&poll_thread, "poll_thread()", 1, 
					ep[0].debug)) {
		printf("\nPoll_thread() could not migrate to CPU 1.  Exit.\n");
		goto exit;
	}
	
	sem_post(&poll_thread_started);
	// Wait to see data appear in memory
	while ((ep[e_i].ibmap[last_idx] == last_val) && (lim++ < max_lim)) {};

__sync_synchronize();
	clock_gettime(CLOCK_MONOTONIC, &ts_parm->ts);
__sync_synchronize();
	ts_parm->lim = lim;

exit:
	return parm;
};

int start_poll_thread(int e_i, int last_idx)
{
	struct poll_thread_parms *ts_p;
	int ret = -1;

	ts_p = (struct poll_thread_parms *)
		(malloc(sizeof(struct poll_thread_parms)));

        sem_init(&poll_thread_started, 0, 0);

	main_thread = pthread_self();

	if (migrate_thread_to_cpu(&main_thread, "main_thread()", 0, 
					ep[0].debug)) {
		printf("\nMain_thread() could not migrate to CPU 0.  Exit.\n");
		goto exit;
	}

	ts_p->ts.tv_nsec = e_i;
	ts_p->ts.tv_sec = last_idx;
	ts_p->lim = 10000000;

	ret = pthread_create( &poll_thread, NULL, poll_for_completion_thread,
                                (void *)(ts_p));
	if (ret) {
		printf("Error - poll_thread rc: %d\n", ret);
	} else {
		ret = sem_wait(&poll_thread_started);
	}

exit:
	return ret;
};

int join_poll_thread(struct timespec *wr_endtime, uint32_t *limit)
{
	void *temp_ptr;
	struct poll_thread_parms *temp_ts_ptr;
	int s;

	wr_endtime->tv_nsec = wr_endtime->tv_sec = 0;
	*limit = 0;
	
	s = pthread_join(poll_thread, &temp_ptr);
	if (s != 0) {
		handle_error_en(s, "pthread_join");
	} else {
		temp_ts_ptr = (struct poll_thread_parms *)(temp_ptr);
		*wr_endtime = temp_ts_ptr->ts;
		*limit = temp_ts_ptr->lim;
		free(temp_ptr);
	}
	return s;
};

void do_dma_xfer(uint32_t e_i, struct timespec *wr_starttime, 
		struct timespec *wr_endtime, uint32_t *limit)
{
	unsigned int last_idx = ep[0].offset + ep[0].dma_size - 1;
	uint8_t last_val = LAST_VAL(last_idx);
	uint32_t lim = 0, max_lim = 1000000000;
	uint32_t ret = -1;

	if (ep[0].use_thr) {
		if (start_poll_thread(e_i, last_idx)) {
			perror("start_poll_thread");
			exit(EXIT_FAILURE);
		};
	};

__sync_synchronize();
	clock_gettime(CLOCK_MONOTONIC, wr_starttime);
__sync_synchronize();

	if (ep[0].kbuf_mode)
		ret = riomp_dma_write_d(ep[0].fd, ep[0].tgt_destid, 
			ep[0].tgt_addr, ep[0].src_handle, ep[0].offset, 
			ep[0].dma_size, RIO_EXCHANGE_NWRITE, RIO_TRANSFER_SYNC);
	else
		ret = riomp_dma_write(ep[0].fd, ep[0].tgt_destid, 
						ep[0].tgt_addr, (void *)(ep[0].buf_src + ep[0].offset),
						ep[0].dma_size, RIO_EXCHANGE_NWRITE, RIO_TRANSFER_SYNC);

	if (ret) {
		printf("\nDMA Transfer failure, err= %d", ret);
		*limit = max_lim;
		ep[e_i].ibmap[last_idx] = last_val; 
		return;
	}

	// Wait to see data appear in memory
	if (ep[0].use_thr) {
		if (join_poll_thread(wr_endtime, limit))
			perror("join_poll_thread");
	} else {
		while ((ep[e_i].ibmap[last_idx] == last_val) && 
			(lim++ < max_lim)) 
			{};
__sync_synchronize();
		clock_gettime(CLOCK_MONOTONIC, wr_endtime);
__sync_synchronize();
		*limit = lim;
	};
};

void do_dma_xfer_1_rx(void)
{
	unsigned int last_idx = ep[0].offset + ep[0].dma_size - 1;
	uint8_t last_val = LAST_VAL(last_idx);
	uint32_t ret;

	// Wait to see data appear in memory
	while (ep[0].ibmap[last_idx] == last_val) {};

	if (ep[0].kbuf_mode)
		ret = riomp_dma_write_d(ep[0].fd, ep[0].tgt_destid, 
			ep[0].tgt_addr, ep[0].src_handle, ep[0].offset, 
			ep[0].dma_size, RIO_EXCHANGE_NWRITE, RIO_TRANSFER_SYNC);
	else
		ret = riomp_dma_write(ep[0].fd, ep[0].tgt_destid, 
						ep[0].tgt_addr, (void *)(ep[0].buf_src + ep[0].offset),
						ep[0].dma_size, RIO_EXCHANGE_NWRITE, RIO_TRANSFER_SYNC);

	if (ret)
		printf("\nDMA Transfer failure, err= %d", ret);
};

void check_buffers(uint32_t e_i, uint32_t limit, uint32_t config)
{
	unsigned int error_count = 0;

	unsigned int last_idx = ep[e_i].offset + ep[e_i].dma_size - 1;
	uint8_t last_val = LAST_VAL(last_idx);

	if (ep[e_i].ibmap[last_idx] == last_val) {
		printf("\tTransfer failed!");
		goto exit;
	}

	if (ep[0].debug)
		printf("\tTransfer succeeded after %d polls\n", limit);
	

	if (!ep[0].verify) {
		if (ep[0].debug)
			printf("\tBuffer verification is turned off!\n");
		goto exit;
	};

	if (CFG_1_RX != config) {
		if (CFG_DUAL == config)
			e_i = 0;
		if (ep[e_i].debug)
			printf("\tVerifying source buffer...\n");
		error_count = dmatest_verify(ep[e_i].buf_src, 0, ep[e_i].offset,
				0, PATTERN_SRC, 1);
		error_count += dmatest_verify(ep[e_i].buf_src, ep[e_i].offset,
			ep[e_i].offset + ep[e_i].dma_size, ep[e_i].offset,
				PATTERN_SRC | PATTERN_COPY, 1);
		error_count += dmatest_verify(ep[0].buf_src, 
				ep[0].offset + ep[0].dma_size,
				TEST_BUF_SIZE, ep[0].offset + ep[0].dma_size,
				PATTERN_SRC, 1);
	};

	if (CFG_1_TX != config) {
		if (CFG_DUAL == config)
			e_i = 1;
		if (ep[e_i].debug)
			printf("\tVerifying destination buffer...\n");
		error_count += dmatest_verify(ep[e_i].ibmap,
				0, 
				ep[e_i].offset,
				0, 
				PATTERN_DST, 
				0);
		error_count += dmatest_verify(ep[e_i].ibmap,
				ep[e_i].offset,
				ep[e_i].offset + ep[e_i].dma_size, 
				ep[e_i].offset, 
				PATTERN_SRC | PATTERN_COPY, 
				0);
		error_count += dmatest_verify(ep[e_i].ibmap,
				ep[e_i].offset + ep[e_i].dma_size,
				TEST_BUF_SIZE, 
				ep[e_i].offset + ep[e_i].dma_size,
				PATTERN_DST,
				0);
	};
	if (ep[0].debug) {
		if (error_count)
			printf("\tBuffer verification failed with %d errors\n",
				error_count);
		else
			printf("\tBuffer verification OK!\n");
	};

exit:
	return;
}

// Supports CFG_LPBK, CFG_1_TX, and CFG_DUAL
// The above configurations require the test to send a DMA
// transaction, and check for reception of the transaction.
//
// Trick: For CFG_DUAL, e_i is 1. This means that for all
// inbound data, should use e_i vlaue.  
void do_dma_test_self_tgt(int e_i, uint32_t config, struct timespec *starttime,
					struct timespec *endtime)
{
	uint32_t limit = 0;

	if (ep[0].debug) {
		switch (config) {
		case CFG_LPBK: printf("\nRapidIO Tsi721 Loopback DMA Test\n");
			break;
		case CFG_1_TX: printf("\nRapidIO Tsi721 One TX DMA Test\n");
			break;
		case CFG_DUAL: printf("\nRapidIO Tsi721 Dual 721 DMA Test\n");
			break;
		default: printf("Software failure...+++\n");
			goto exit;
		};
	};

	dmatest_init_dsts(ep[e_i].ibmap, ep[e_i].offset, ep[e_i].dma_size, 
				TEST_BUF_SIZE);
	dmatest_init_srcs(ep[0].buf_src, ep[0].offset, ep[0].dma_size, 
				TEST_BUF_SIZE);
	dmatest_init_dsts(ep[0].buf_dst, ep[0].offset, ep[0].dma_size, 
				TEST_BUF_SIZE);

	display_pre_dma_debug(e_i);

	do_dma_xfer(e_i, starttime, endtime, &limit);

	display_pre_dma_debug(e_i);

	check_buffers(e_i, limit, config);
exit:
	return;
}

void do_dma_test_one_721_rx(uint32_t config)
{
	printf("\n+++ RapidIO Single Tsi721 DMA Test RX +++\n");

	dmatest_init_dsts(ep[0].ibmap, ep[0].offset, ep[0].dma_size, 
				TEST_BUF_SIZE);
	dmatest_init_srcs(ep[0].buf_src, ep[0].offset, ep[0].dma_size, 
				TEST_BUF_SIZE);
	dmatest_init_dsts(ep[0].buf_dst, ep[0].offset, ep[0].dma_size, 
				TEST_BUF_SIZE);

	display_pre_dma_debug(0);

	do_dma_xfer_1_rx();

	check_buffers(0, 0, config);
}

/* Ctrl-C handler, invoked when the program hangs and user hits Ctrl-C
 * @param sig   unused
 */
void ctrl_c_handler( int sig )
{
	puts("Ctrl-C hit, cleaning up and exiting...");
	cleanup_buffs(1);
	cleanup_ibwin(1);
	close_mports(1);
	exit(1);
} /* ctrl_c_handler() */

int main(int argc, char** argv)
{
	int config;
	int rc = EXIT_SUCCESS;
	int last_idx;
	int i;
	struct timespec starttime, endtime, total_time, min_time, max_time;

	/* Register ctrl-c handler */
	signal(SIGQUIT, ctrl_c_handler);
	signal(SIGINT, ctrl_c_handler);

	total_time.tv_sec = total_time.tv_nsec = 0;
	min_time = max_time = total_time;

	parse_options(argc, argv, &config); /* Succeeds or exits */

	last_idx = (config >= CFG_DUAL)?1:0;

	if (EXIT_FAILURE == open_mports(last_idx)) /* Succeeds or exits */
		goto close_ports;

	fixup_options(config);

	/* First configure all of the mports.
	 * Then, clean them all up by resetting the mport and, if necessary,
	 * resetting the link partner and configuring the switch.
	 */
	for (i = 0; i <= last_idx; i++)
		if (EXIT_FAILURE == config_tsi721(config == CFG_LPBK, 
				ep[i].has_switch, ep[i].fd, 
				ep[i].debug,
				ep[i].prop.link_speed >= RIO_LINK_500 ))
			goto close_buffs;
 
	for (i = 0; i <= last_idx; i++) {
		int rst_lp = (i == last_idx) && 
			((config == CFG_1_TX) || (config == CFG_DUAL));
		if (EXIT_FAILURE == cleanup_tsi721( ep[i].has_switch,
				ep[i].fd, ep[i].debug, 
				ep[i].prop.hdid, rst_lp))
			goto close_buffs;
	}

	/* previous step will clean up if there aren't any ib windows avail */
	if (EXIT_FAILURE == config_ibwin(last_idx)) /* Succeeds or exits */
		goto close_ibwin;

	if (EXIT_FAILURE == config_buffs(last_idx)) /* Succeeds or exits */
		goto close_buffs;

	for (i = 0; i < ep[0].repeat; i++) {
		switch (config) {
		case CFG_LPBK: do_dma_test_self_tgt(last_idx, config, 
					&starttime, &endtime);
			break;
		case CFG_1_RX: do_dma_test_one_721_rx(config);
			break;
		case CFG_1_TX: do_dma_test_self_tgt(last_idx, config, 
					&starttime, &endtime);
			break;
		case CFG_DUAL: do_dma_test_self_tgt(last_idx, config,
					&starttime, &endtime);
			break;
		default: 
			printf("\nProgramming error, config is not 0, 1, 2 or 3.");
			goto close_buffs;
		};
		if (ep[0].debug) {
			struct timespec time = 
				time_difference(starttime, endtime);
			printf("\t\tWR time: %d secs %d nsecs\n",
				(uint32_t)(time.tv_sec), 
				(uint32_t)(time.tv_nsec));
		};
		time_track(i, starttime, endtime, 
				&total_time, &min_time, &max_time);
	};

	if (config == CFG_1_RX)
		goto close_buffs;

	printf("Total time  :\t %9d sec %9d nsec\n", 
			(int)total_time.tv_sec, (int)total_time.tv_nsec);
	printf("Minimum time:\t %9d sec %9d nsec\n",
			(int)min_time.tv_sec, (int)min_time.tv_nsec);
	total_time = time_div(total_time, ep[0].repeat);
	printf("Average time:\t %9d sec %9d nsec\n",
			(int)total_time.tv_sec, (int)total_time.tv_nsec);
	printf("Maximum time:\t %9d sec %9d nsec\n",
			(int)max_time.tv_sec, (int)max_time.tv_nsec);
close_buffs:
	cleanup_buffs(last_idx);
close_ibwin:
	cleanup_ibwin(last_idx);
close_ports:
	close_mports(last_idx);
	exit(rc);
}
