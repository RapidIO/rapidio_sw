/*
 * Copyright 2014 Integrated Device Technology, Inc.
 *
 * User-space RapidIO DMA transfer test program.
 *
 * This program uses code fragments from Linux kernel dmaengine
 * framework test driver developed by Atmel and Intel. Please, see
 * drivers/dma/dmatest.c file in Linux kernel source code tree for more
 * details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <rapidio_mport_dma.h>

#include <rapidio_mport_mgmt.h>

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

/* Maximum amount of mismatched bytes in buffer to print */
#define MAX_ERROR_COUNT		32

#define TEST_BUF_SIZE (2 * 1024 * 1024)
#define DEFAULT_IBWIN_SIZE (2 * 1024 * 1024)

struct dma_async_wait_param {
	uint32_t token;		/* DMA transaction ID token */
	int err;		/* error code returned to caller */
};

static int fd;
static uint32_t tgt_destid;
static uint64_t tgt_addr;
static uint32_t offset = 0;
static int align = 0;
static uint32_t dma_size = 0;
static uint32_t ibwin_size;
static int debug = 0;
static int exit_no_dev;
static uint32_t tbuf_size = TEST_BUF_SIZE;

/* Max data block length that can be transferred by DMA channel
 * by default configured for 32 scatter/gather entries of 4K.
 * Size returned by mport driver should be when available.
 * Shall less or eq. TEST_BUF_SIZE.
 */
static uint32_t max_sge = 32;
static uint32_t max_size = 4096;

struct timespec timediff(struct timespec start, struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

static void dmatest_init_srcs(uint8_t *buf, unsigned int start,
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

static void dmatest_init_dsts(uint8_t *buf, unsigned int start,
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

static unsigned int dmatest_verify(uint8_t *buf, unsigned int start,
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
			if (debug)
				printf("riomp_dma_dbuf_alloc failed err=%d\n", ret);
			return NULL;
		}

		buf_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, h);
		if (buf_ptr == MAP_FAILED) {
			perror("mmap");
			buf_ptr = NULL;
			ret = riomp_dma_dbuf_free(fd, handle);
			if (ret && debug)
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

#define U8P uint8_t*

static int do_ibwin_test(uint32_t mport_id, uint64_t rio_base, uint32_t ib_size,
			 int verify)
{
	int ret;
	uint64_t ib_handle;
	void *ibmap;

	ret = riomp_dma_ibwin_map(fd, &rio_base, ib_size, &ib_handle);
	if (ret) {
		printf("Failed to allocate/map IB buffer err=%d\n", ret);
		close(fd);
		return ret;
	}

	ibmap = mmap(NULL, ib_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ib_handle);
	if (ibmap == MAP_FAILED) {
		perror("mmap");
		goto out;
	}

	printf("\tSuccessfully allocated/mapped IB buffer (rio_base=0x%x_%08x)\n",
	       (uint32_t)(rio_base >> 32), (uint32_t)(rio_base & 0xffffffff));

	if (debug)
		printf("\t(h=0x%x_%x, loc=%p)\n", (uint32_t)(ib_handle >> 32),
			(uint32_t)(ib_handle & 0xffffffff), ibmap);
	printf("\t.... press Enter key to exit ....\n");
	getchar();
	if (exit_no_dev)
		printf(">>> Device removal signaled <<<\n");
	if (verify)
		dmatest_verify((U8P)ibmap, 0, ib_size, 0, PATTERN_SRC | PATTERN_COPY, 0);

	if (munmap(ibmap, ib_size))
		perror("munmap");
out:
	ret = riomp_dma_ibwin_free(fd, &ib_handle);
	if (ret)
		printf("Failed to release IB buffer err=%d\n", ret);

	return 0;
}

void *dma_async_wait(void *arg)
{
	struct dma_async_wait_param *param = (struct dma_async_wait_param *)arg;
	int ret;

	ret = riomp_dma_wait_async(fd, param->token, 3000);
	param->err = ret;
	return &param->err;
}

static int do_dma_test(int random, int kbuf_mode, int verify, int loop_count,
		       enum riomp_dma_directio_transfer_sync sync)
{
	void *buf_src = NULL;
	void *buf_dst = NULL;
	unsigned int src_off, dst_off, len;
	uint64_t src_handle = 0;
	uint64_t dst_handle = 0;
	int i, ret = 0;
	uint32_t max_hw_size; /* max DMA transfer size supported by HW */
	enum riomp_dma_directio_transfer_sync rd_sync;
	struct timespec wr_starttime, wr_endtime;
	struct timespec rd_starttime, rd_endtime;
	float totaltime;

	if (kbuf_mode)
		max_hw_size = max_size;
	else
		max_hw_size = (max_sge == 0)?tbuf_size:(max_sge * getpagesize());

	/* check specified DMA block size */
	if (dma_size > 0 &&
	    (dma_size > tbuf_size || dma_size > max_hw_size)) {
		printf("ERR: invalid buffer size parameter\n");
		printf("     max allowed buffer size: %d bytes\n",
		       (max_hw_size > tbuf_size)?tbuf_size:max_hw_size);
		return -1;
	}

	if (random) {
		/* If not specified, set max supported dma_size */
		if (dma_size == 0)
			dma_size = (max_hw_size < tbuf_size)?max_hw_size:tbuf_size;

		printf("\tRANDOM mode is selected for size/offset combination\n");
		printf("\t\tmax data transfer size: %d bytes\n", dma_size);
		srand(time(NULL));
	} else if (dma_size + offset > tbuf_size || dma_size == 0) {
			printf("ERR: invalid transfer size/offset combination\n");
			return -1;
	} else
		printf("\tdma_size=%d offset=0x%x\n", dma_size, offset);

	buf_src = dmatest_buf_alloc(fd, tbuf_size, kbuf_mode?&src_handle:NULL);
	if (buf_src == NULL) {
		printf("DMA Test: error allocating SRC buffer\n");
		ret = -1;
		goto out;
	}

	buf_dst = dmatest_buf_alloc(fd, tbuf_size, kbuf_mode?&dst_handle:NULL);
	if (buf_dst == NULL) {
		printf("DMA Test: error allocating DST buffer\n");
		ret = -1;
		goto out;
	}

	for (i = 1; i <= loop_count; i++) { 
		struct timespec time, rd_time;
		pthread_t wr_thr, rd_thr;
		struct dma_async_wait_param wr_wait, rd_wait;

		if (random) {
			len = rand() % dma_size + 1;
			len = (len >> align) << align;
			if (!len)
				len = 1 << align;
			src_off = rand() % (tbuf_size - len + 1);
			dst_off = rand() % (tbuf_size - len + 1);

			src_off = (src_off >> align) << align;
			dst_off = (dst_off >> align) << align;
		} else {
			len = dma_size;
			src_off = offset;
			dst_off = offset;
		}

		printf("<%d>: len=0x%x src_off=0x%x dst_off=0x%x\n",
			i, len, src_off, dst_off);

		if (verify) {
			dmatest_init_srcs((U8P)buf_src, src_off, len, tbuf_size);
			dmatest_init_dsts((U8P)buf_dst, dst_off, len, tbuf_size);
		}

		if (debug)
			printf("\tWrite %d bytes from src offset 0x%x\n", len, src_off);
		clock_gettime(CLOCK_MONOTONIC, &wr_starttime);

		if (kbuf_mode)
			ret = riomp_dma_write_d(fd, tgt_destid, tgt_addr,
						src_handle, src_off, len,
						RIO_DIRECTIO_TYPE_NWRITE_R, sync);
		else
			ret = riomp_dma_write(fd, tgt_destid, tgt_addr,
					      (U8P)buf_src + src_off, len,
					      RIO_DIRECTIO_TYPE_NWRITE_R, sync);

		if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
			if (ret >=0) {
				wr_wait.token = ret;
				wr_wait.err = -1;
				ret = pthread_create(&wr_thr, NULL, dma_async_wait, (void *)&wr_wait);
				if (ret)
					printf("\tERR: Failed to create WR wait thread. err=%d\n", ret);
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &wr_endtime);
		if (ret) {
			printf("DMA Test: write DMA transfer failed err=%d\n", ret);
			goto out;
		}

		if (debug)
			printf("\tRead %d bytes to dst offset 0x%x\n", len, dst_off);

		/* RIO_TRANSFER_FAF is not available for read operations. Force SYNC instead. */
		rd_sync = (sync == RIO_DIRECTIO_TRANSFER_FAF)?RIO_DIRECTIO_TRANSFER_SYNC:sync;

		clock_gettime(CLOCK_MONOTONIC, &rd_starttime);

		if (kbuf_mode)
			ret = riomp_dma_read_d(fd, tgt_destid, tgt_addr,
					dst_handle, dst_off, len, rd_sync);
		else
			ret = riomp_dma_read(fd, tgt_destid, tgt_addr,
					(U8P)buf_dst + dst_off, len, rd_sync);

		if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
			if (ret >=0) {
				rd_wait.token = ret;
				rd_wait.err = -1;
				ret = pthread_create(&rd_thr, NULL, dma_async_wait, (void *)&rd_wait);
				if (ret)
					printf("\tERR: Failed to create RD wait thread. err=%d\n", ret);
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &rd_endtime);
		if (ret) {
			printf("DMA Test: read DMA transfer failed err=%d\n", ret);
			goto out;
		}

		rd_time = timediff(rd_starttime, rd_endtime);

		if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
			pthread_join(wr_thr, NULL);
			pthread_join(rd_thr, NULL);
			clock_gettime(CLOCK_MONOTONIC, &rd_endtime);
			if (wr_wait.err)
				printf("Wait for DMA_WR: err=%d\n", wr_wait.err);
			if (rd_wait.err)
				printf("Wait for DMA_RD: err=%d\n", rd_wait.err);
			if (wr_wait.err || rd_wait.err)
				goto out;
		}

		if (verify) {
			unsigned int error_count;

			if (debug)
				printf("\tVerifying source buffer...\n");
			error_count = dmatest_verify((U8P)buf_src, 0, src_off,
					0, PATTERN_SRC, 1);
			error_count += dmatest_verify((U8P)buf_src, src_off,
					src_off + len, src_off,
					PATTERN_SRC | PATTERN_COPY, 1);
			error_count += dmatest_verify((U8P)buf_src, src_off + len,
					tbuf_size, src_off + len,
					PATTERN_SRC, 1);

			if (debug)
				printf("\tVerifying destination buffer...\n");
			error_count += dmatest_verify((U8P)buf_dst, 0, dst_off,
					0, PATTERN_DST, 0);
			error_count += dmatest_verify((U8P)buf_dst, dst_off,
					dst_off + len, src_off,
					PATTERN_SRC | PATTERN_COPY, 0);
			error_count += dmatest_verify((U8P)buf_dst, dst_off + len,
					tbuf_size, dst_off + len,
					PATTERN_DST, 0);
			if (error_count) {
				printf("\tBuffer verification failed with %d errors\n", error_count);
				break;
			} else
				printf("\tBuffer verification OK!\n");
		} else
			printf("\tBuffer verification is turned off!\n");

		time = timediff(wr_starttime, wr_endtime);
		totaltime = ((double) time.tv_sec + (time.tv_nsec / 1000000000.0));
		if (sync != RIO_DIRECTIO_TRANSFER_SYNC)
			printf("\t\tWR time: %4f s\n", totaltime);
		else
			printf("\t\tWR time: %4f s @ %4.2f MB/s\n",
				totaltime, (len/totaltime)/(1024*1024));
		totaltime = ((double) rd_time.tv_sec + (rd_time.tv_nsec / 1000000000.0));
		if (sync != RIO_DIRECTIO_TRANSFER_SYNC)
			printf("\t\tRD time: %4f s\n", totaltime);
		else
			printf("\t\tRD time: %4f s @ %4.2f MB/s\n",
				totaltime, (len/totaltime)/(1024*1024));
		time = timediff(wr_starttime, rd_endtime);
		totaltime = ((double) time.tv_sec + (time.tv_nsec / 1000000000.0));
		printf("\t\tFull Cycle time: %4f s\n", totaltime);
	}
out:
	dmatest_buf_free(fd, buf_src, tbuf_size,
				kbuf_mode?&src_handle:NULL);
	dmatest_buf_free(fd, buf_dst, tbuf_size,
				kbuf_mode?&dst_handle:NULL);
	return ret;
}

static void display_help(char *program)
{
	printf("%s - test DMA data transfers to/from RapidIO device\n",	program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("options are:\n");
	printf("Common:\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -v turn off buffer data verification\n");
	printf("  --debug (or -d)\n");
	printf("  --help (or -h)\n");
	printf("DMA test mode only:\n");
	printf("  -D xxxx\n");
	printf("  --destid xxxx\n");
	printf("    destination ID of target RapidIO device\n");
	printf("  -A xxxx\n");
	printf("  --taddr xxxx\n");
	printf("    memory address in target RapidIO device\n");
	printf("  -S xxxx\n");
	printf("  --size xxxx\n");
	printf("    data transfer size in bytes (default 0x100)\n");
	printf("  -B xxxx\n");
	printf("    data buffer size (SRC and DST) in bytes (default 0x200000)\n");
	printf("  -O xxxx\n");
	printf("  --offset xxxx\n");
	printf("    offset in local data src/dst buffers (default=0)\n");
	printf("  -a n\n");
	printf("  --align n\n");
	printf("    data buffer address alignment\n");
	printf("  -T n\n");
	printf("  --repeat n\n");
	printf("    repeat test n times (default=1)\n");
	printf("  -k use coherent kernel space buffer allocation\n");
	printf("  -r use random size and local buffer offset values\n");
	printf("  --faf use FAF DMA transfer mode (default=SYNC)\n");
	printf("  --async use ASYNC DMA transfer mode (default=SYNC)\n");
	printf("Inbound Window mode only:\n");
	printf("  -i\n");
	printf("    allocate and map inbound window (memory) using default parameters\n");
	printf("  -I xxxx\n");
	printf("  --ibwin xxxx\n");
	printf("    inbound window (memory) size in bytes\n");
	printf("  -R xxxx\n");
	printf("  --ibbase xxxx\n");
	printf("    inbound window base address in RapidIO address space\n");
	printf("\n");
}

static void test_sigaction(int sig, siginfo_t *siginfo, void *context)
{
	printf ("SIGIO info PID: %ld, UID: %ld CODE: 0x%x BAND: 0x%lx FD: %d\n",
			(long)siginfo->si_pid, (long)siginfo->si_uid, siginfo->si_code,
			siginfo->si_band, siginfo->si_fd);
	exit_no_dev = 1;
}

int main(int argc, char** argv)
{
	uint32_t mport_id = 0;
	int option;
	int do_rand = 0;
	int kbuf_mode = 0; /* 0 - user space buffer, 1 = kernel contiguous buffer */
	int verify = 1;
	unsigned int repeat = 1;
	uint64_t rio_base = RIO_MAP_ANY_ADDR;
	enum riomp_dma_directio_transfer_sync sync = RIO_DIRECTIO_TRANSFER_SYNC;
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
		{ "faf",    no_argument, NULL, 'F' },
		{ "async",  no_argument, NULL, 'Y' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};
	char *program = argv[0];
	struct riomp_mgmt_mport_properties prop;
	int has_dma = 1;
	struct sigaction action;
	int rc = EXIT_SUCCESS;

	while (1) {
		option = getopt_long_only(argc, argv,
				"rvwdhika:A:D:I:O:M:R:S:T:B:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
			/* DMA Data Transfer Mode options*/
		case 'A':
			tgt_addr = strtoull(optarg, NULL, 0);
			break;
		case 'a':
			align = strtol(optarg, NULL, 0);
			break;
		case 'D':
			tgt_destid = strtol(optarg, NULL, 0);
			break;
		case 'O':
			offset = strtol(optarg, NULL, 0);
			break;
		case 'S':
			dma_size = strtol(optarg, NULL, 0);
			break;
		case 'B':
			tbuf_size = strtol(optarg, NULL, 0);
			break;
		case 'T':
			repeat = strtol(optarg, NULL, 0);
			break;
		case 'k':
			kbuf_mode = 1;
			break;
		case 'r':
			do_rand = 1;
			break;
		case 'F':
			sync = RIO_DIRECTIO_TRANSFER_FAF;
			break;
		case 'Y':
			sync = RIO_DIRECTIO_TRANSFER_ASYNC;
			break;
			/* Inbound Memory (window) Mode options */
		case 'I':
			ibwin_size = strtol(optarg, NULL, 0);
			break;
		case 'i':
			ibwin_size = DEFAULT_IBWIN_SIZE;
			break;
		case 'R':
			rio_base = strtoull(optarg, NULL, 0);
			break;
			/* Options common for all modes */
		case 'M':
			mport_id = strtol(optarg, NULL, 0);
			break;
		case 'v':
			verify = 0;
			break;
		case 'd':
			debug = 1;
			break;
		case 'h':
		default:
			display_help(program);
			exit(EXIT_SUCCESS);
		}
	}

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = test_sigaction;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGIO, &action, NULL);

	fd = riomp_mgmt_mport_open(mport_id, 0);
	if (fd < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, errno);
		exit(EXIT_FAILURE);
	}

	if (!riomp_mgmt_query(fd, &prop)) {
		riomp_mgmt_display_info(&prop);

		if (prop.flags & RIO_MPORT_DMA) {
			align = prop.dma_align;
			max_sge = prop.dma_max_sge;
			max_size = prop.dma_max_size;
		} else
			has_dma = 0;

		if (prop.link_speed == 0) {
			printf("SRIO link is down. Test aborted.\n");
			rc = EXIT_FAILURE;
			goto out;
		}
	} else {
		printf("Failed to obtain mport information\n");
		printf("Using default configuration\n\n");
	}

	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | FASYNC);

	if (ibwin_size) {
		printf("+++ RapidIO Inbound Window Mode +++\n");
		printf("\tmport%d ib_size=0x%x PID:%d\n",
			mport_id, ibwin_size, (int)getpid());

		do_ibwin_test(mport_id, rio_base, ibwin_size, verify);
	} else if (has_dma) {
		printf("+++ RapidIO DMA Test +++\n");
		printf("\tmport%d destID=%d rio_addr=0x%llx align=%d repeat=%d PID:%d\n",
			mport_id, tgt_destid, (unsigned long long)tgt_addr, align, repeat, (int)getpid());
		printf("\tsync=%d (%s)\n", sync,
			(sync == RIO_DIRECTIO_TRANSFER_SYNC)?"SYNC":(sync == RIO_DIRECTIO_TRANSFER_FAF)?"FAF":"ASYNC");

		if (do_dma_test(do_rand, kbuf_mode, verify, repeat, sync)) {
		    printf("DMA test FAILED\n\n");
		    rc = EXIT_FAILURE;
        }
	} else {
		printf("No DMA support. Test aborted.\n\n");
		rc = EXIT_FAILURE;
	}

out:
	close(fd);
	exit(rc);
}
