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

#define TEST_BUF_SIZE (256 * 1024)
#define DEFAULT_IBWIN_SIZE (2 * 1024 * 1024)

struct dma_async_wait_param {
	uint32_t token;		/* DMA transaction ID token */
	int err;		/* error code returned to caller */
};

static riomp_mport_t mport_hnd;
static uint32_t tgt_destid;
static uint64_t tgt_addr;
static uint32_t offset = 0;
static int align = 0;
static uint32_t copy_size = TEST_BUF_SIZE;
static uint32_t ibwin_size;
static uint32_t tbuf_size = TEST_BUF_SIZE;
static int debug = 0;
static int exit_no_dev;

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

static void *obwtest_buf_alloc(uint32_t size)
{
	void *buf_ptr = NULL;

	buf_ptr = malloc(size);
	if (buf_ptr == NULL )
		perror("malloc");

	return buf_ptr;
}

static void obwtest_buf_free(void *buf)
{
	free(buf);
}

#define U8P uint8_t*

static int do_ibwin_test(uint32_t mport_id, uint64_t rio_base, uint32_t ib_size,
			 int verify)
{
	int ret, fdes;
	uint64_t ib_handle;
	void *ibmap;

	ret = riomp_mgmt_get_fd(mport_hnd, &fdes);
	if (ret) {
		printf("fileio not supported.\n");
		return ret;
	}

	ret = riomp_dma_ibwin_map(mport_hnd, &rio_base, ib_size, &ib_handle);
	if (ret) {
		printf("Failed to allocate/map IB buffer err=%d\n", ret);
		return ret;
	}

	ibmap = mmap(NULL, ib_size, PROT_READ | PROT_WRITE, MAP_SHARED, fdes, ib_handle);
	if (ibmap == MAP_FAILED) {
		perror("mmap");
		goto out;
	}

	printf("\tSuccessfully allocated/mapped IB buffer (rio_base=0x%x_%x)\n",
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
	ret = riomp_dma_ibwin_free(mport_hnd, &ib_handle);
	if (ret)
		printf("Failed to release IB buffer err=%d\n", ret);

	return 0;
}

static int do_obwin_test(int random, int verify, int loop_count)
{
	void *buf_src = NULL;
	void *buf_dst = NULL;
	void *obw_ptr = NULL;
	unsigned int src_off, dst_off, len;
	uint64_t obw_handle = 0;
	int i, ret = 0;
	struct timespec wr_starttime, wr_endtime;
	struct timespec rd_starttime, rd_endtime;
	float totaltime;
	int fdes;

	ret = riomp_mgmt_get_fd(mport_hnd, &fdes);
	if (ret) {
		printf("fileio not supported.\n");
		return ret;
	}

	/* check specified DMA block size */
	if (copy_size > tbuf_size || copy_size == 0) {
		printf("ERR: invalid transfer size parameter\n");
		printf("     max allowed copy size: %d bytes\n", tbuf_size);
		return -1;
	}

	if (random) {
		printf("\tRANDOM mode is selected for size/offset combination\n");
		printf("\t\tmax data transfer size: %d bytes\n", copy_size);
		srand(time(NULL));
	} else if (copy_size + offset > tbuf_size) {
			printf("ERR: invalid transfer size/offset combination\n");
			return -1;
	} else
		printf("\tcopy_size=%d offset=0x%x\n", copy_size, offset);

	buf_src = obwtest_buf_alloc(tbuf_size);
	if (buf_src == NULL) {
		printf("DMA Test: error allocating SRC buffer\n");
		ret = -1;
		goto out;
	}

	buf_dst = obwtest_buf_alloc(tbuf_size);
	if (buf_dst == NULL) {
		printf("DMA Test: error allocating DST buffer\n");
		ret = -1;
		goto out;
	}

	ret = riomp_dma_obwin_map(mport_hnd, tgt_destid, tgt_addr, tbuf_size, &obw_handle);
	if (ret) {
		printf("riomp_dma_obwin_map failed err=%d\n", ret);
		goto out;
	}

	printf("OBW handle 0x%x_%08x\n", (uint32_t)(obw_handle >> 32),
		(uint32_t)(obw_handle & 0xffffffff));

	obw_ptr = mmap(NULL, tbuf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fdes, obw_handle);
	if (obw_ptr == MAP_FAILED) {
		perror("mmap");
		obw_ptr = NULL;
		goto out_unmap;
	}

	for (i = 1; i <= loop_count; i++) { 
		struct timespec time, rd_time;

		if (random) {
			len = rand() % copy_size + 1;
			len = (len >> align) << align;
			if (!len)
				len = 1 << align;
			src_off = rand() % (tbuf_size - len + 1);
			dst_off = rand() % (tbuf_size - len + 1);

			src_off = (src_off >> align) << align;
			dst_off = (dst_off >> align) << align;
		} else {
			len = copy_size;
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
		memcpy(obw_ptr, (U8P)buf_src + src_off, len);
		clock_gettime(CLOCK_MONOTONIC, &wr_endtime);
		if (debug)
			printf("\tRead %d bytes to dst offset 0x%x\n", len, dst_off);

		clock_gettime(CLOCK_MONOTONIC, &rd_starttime);
		memcpy((U8P)buf_dst + dst_off, obw_ptr, len);
		clock_gettime(CLOCK_MONOTONIC, &rd_endtime);

		rd_time = timediff(rd_starttime, rd_endtime);

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
		printf("\t\tWR time: %4f s @ %4.2f MB/s\n",
			totaltime, (len/totaltime)/(1024*1024));
		totaltime = ((double) rd_time.tv_sec + (rd_time.tv_nsec / 1000000000.0));
		printf("\t\tRD time: %4f s @ %4.2f MB/s\n",
			totaltime, (len/totaltime)/(1024*1024));
		time = timediff(wr_starttime, rd_endtime);
		totaltime = ((double) time.tv_sec + (time.tv_nsec / 1000000000.0));
		printf("\t\tFull Cycle time: %4f s\n", totaltime);
	}

	if (munmap(obw_ptr, tbuf_size))
		perror("munmap");
out_unmap:
	ret = riomp_dma_obwin_free(mport_hnd, &obw_handle);
	if (ret)
		printf("Failed to release OB window err=%d\n", ret);
out:
	if (buf_src)
		obwtest_buf_free(buf_src);
	if (buf_dst)
		obwtest_buf_free(buf_dst);
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
	printf("OBW mapping test mode only:\n");
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
	printf("  -B xxxx size of test buffer and OBW aperture (in MB, e.g -B2)\n");
	printf("  -r use random size and local buffer offset values\n");
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
	int verify = 1;
	unsigned int repeat = 1;
	uint64_t rio_base = RIO_MAP_ANY_ADDR;
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
	struct sigaction action;
	int rc = EXIT_SUCCESS;
	int fdes;

	while (1) {
		option = getopt_long_only(argc, argv,
				"rvwdhika:A:D:I:O:M:R:S:T:B:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
			/* Data Transfer Mode options*/
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
			copy_size = strtol(optarg, NULL, 0);
			break;
		case 'T':
			repeat = strtol(optarg, NULL, 0);
			break;
		case 'B':
			tbuf_size = strtol(optarg, NULL, 0) * 1024 * 1024;
			break;
		case 'r':
			do_rand = 1;
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

	rc = riomp_mgmt_mport_create_handle(mport_id, 0, &mport_hnd);
	if (rc < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, rc);
		exit(EXIT_FAILURE);
	}

	if (!riomp_mgmt_query(mport_hnd, &prop)) {
		riomp_mgmt_display_info(&prop);

		if (prop.link_speed == 0) {
			printf("SRIO link is down. Test aborted.\n");
			rc = EXIT_FAILURE;
			goto out;
		}
	} else {
		printf("Failed to obtain mport information\n");
		printf("Using default configuration\n\n");
	}

	rc = riomp_mgmt_get_fd(mport_hnd, &fdes);
	if (rc) {
		printf("fileio not supported.\n");
		rc = EXIT_FAILURE;
		goto out;
	}

	fcntl(fdes, F_SETOWN, getpid());
	fcntl(fdes, F_SETFL, fcntl(fdes, F_GETFL) | FASYNC);

	if (ibwin_size) {
		printf("+++ RapidIO Inbound Window Mode +++\n");
		printf("\tmport%d ib_size=0x%x PID:%d\n",
			mport_id, ibwin_size, (int)getpid());

		do_ibwin_test(mport_id, rio_base, ibwin_size, verify);
	} else {
		printf("+++ RapidIO Outbound Window Mapping Test +++\n");
		printf("\tmport%d destID=%d rio_addr=0x%llx repeat=%d PID:%d\n",
			mport_id, tgt_destid, (unsigned long long)tgt_addr, repeat, (int)getpid());
		printf("\tbuf_size=0x%x\n", tbuf_size);

		do_obwin_test(do_rand, verify, repeat);
	}

out:
	riomp_mgmt_mport_destroy_handle(mport_hnd);
	exit(rc);
}
