/*
 * Copyright 2014 Integrated Device Technology, Inc.
 *
 * User-space DMA/IBwin buffers mapping test program.
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
#include <sys/wait.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>

#include "rapidio_mport_lib.h"

#define DEFAULT_IBWIN_SIZE (2 * 1024 * 1024)

static int fd;
static uint32_t ibwin_size = DEFAULT_IBWIN_SIZE;

static int fill_segment(uint32_t mport_id, int seg_id, uint64_t seg_handle, uint32_t seg_size)
{
	int fd;
	uint8_t fill;
	void *ibmap;

	printf("FILL process %d (%d) started\n", seg_id, (int)getpid());

	fd = riodp_mport_open(mport_id, 0);
	if (fd < 0) {
		printf("(%d): unable to open mport%d device err=%d\n",
			(int)getpid(), mport_id, errno);
		return -1;
	}

	printf("\t(%d): fd=%d h=0x%x_%x sz=0x%x\n", (int)getpid(), fd,
		(uint32_t)(seg_handle >> 32),
		(uint32_t)(seg_handle & 0xffffffff), seg_size);

	ibmap = mmap(NULL, seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, seg_handle);
	if (ibmap == MAP_FAILED) {
		perror("mmap");
		goto out;
	}

	fill = 0xc0 | (uint8_t)seg_id;
	memset(ibmap, fill, seg_size);

	if (munmap(ibmap, seg_size))
		perror("munmap");

out:
	close(fd);
	return 0;
}


#define SUBWIN_NUM 16

/*
 *
 */
static int do_buf_test(uint32_t mport_id, uint64_t rio_base, uint32_t ib_size,
			 int buf_mode)
{
	int ret;
	uint64_t ib_handle;
	uint64_t seg_handle;
	uint32_t seg_size;
	void *ibmap;
	int i, err_count = 0;;
	int status = 0;
	pid_t pid, wpid;

	if (buf_mode)
		ret = riodp_ibwin_map(fd, &rio_base, ib_size, &ib_handle);
	else
		ret = riodp_dbuf_alloc(fd, ib_size, &ib_handle);

	if (ret) {
		printf("Failed to allocate/map IB buffer err=%d\n", ret);
		return ret;
	}

	ibmap = mmap(NULL, ib_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ib_handle);
	if (ibmap == MAP_FAILED) {
		perror("mmap");
		goto out;
	}

	printf("\tSuccessfully allocated/mapped %s buffer from fd=%d\n",
		buf_mode?"IBwin":"DMA", fd);
	if (buf_mode)
		printf("\t\trio_base=0x%x_%x\n", (uint32_t)(rio_base >> 32),
			(uint32_t)(rio_base & 0xffffffff));

	printf("\t(h=0x%x_%x, loc=%p)\n", (uint32_t)(ib_handle >> 32),
		(uint32_t)(ib_handle & 0xffffffff), ibmap);

	memset(ibmap, 0xee, ib_size);
	__sync_synchronize();

	printf(">>>> Start %d FILL prosesses <<<<\n", SUBWIN_NUM);

	seg_size = ib_size/SUBWIN_NUM;

	for (i = 0; i < SUBWIN_NUM; i++) {

		seg_handle = ib_handle + i*seg_size;


		/* Create child process */
		pid = fork();
		if (pid < 0) {
			perror("BUF_TEST: ERROR on fork()\n");
			exit(1);
		}

		if (pid == 0) {
			/* This is the child process */
			fill_segment(mport_id, i, seg_handle, seg_size);
			exit(0);
		} else {
			/* TBD */
		}
	}

	/* Wait until all child processes termintaed */
	for (; i > 0; i-- ) {
		wpid = wait(&status);
		printf("\t(%d): terminated with status %d\n", wpid, status);
	}

	/* Verify buffer data */
	for (i = 0; i < SUBWIN_NUM; i++) {
		uint32_t j;
		uint8_t *ptr;

		ptr = (uint8_t *)ibmap + i*seg_size;

		for (j = 0; j < seg_size; j++, ptr++) {
			if (*ptr != (0xc0 | (uint8_t)i)) {
				printf("+++ Error in segment %d off 0x%x @ %p (data = 0x%02x)\n",
					i, j, ptr, *ptr);
				err_count++;
				break;
			}
		}
	}

	if (err_count)
		printf("### FAILED with errors in %d segments\n", err_count);
	else
		printf("... Data check OK ...\n");

	printf("\t.... press Enter key to exit ....\n");
	getchar();

	if (munmap(ibmap, ib_size))
		perror("munmap");
out:
	if (buf_mode)
		ret = riodp_ibwin_free(fd, &ib_handle);
	else
		ret = riodp_dbuf_free(fd, &ib_handle);


	if (ret)
		printf("Failed to release IB buffer err=%d\n", ret);

	return 0;
}

static void display_help(char *program)
{
	printf("%s - test DMA buffer mapping by multiple processes\n",	program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("options are:\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -i buffer allocation mode (0 - common DMA, 1 - IBwin mapping\n");
	printf("  --help (or -h)\n");
	printf("  -S xxxx\n");
	printf("  --size xxxx\n");
	printf("    buffer size in bytes (default 0x200000)\n");
	printf("  -R xxxx\n");
	printf("  --ibbase xxxx\n");
	printf("    inbound window base address in RapidIO address space\n");
	printf("\n");
}

int main(int argc, char** argv)
{
	uint32_t mport_id = 0;
	int option;
	int buf_mode = 0; /* 0 = common DMA buffer, 1 = IBwin mapped DMA buffer */
	uint64_t rio_base = RIO_MAP_ANY_ADDR;
	static const struct option options[] = {
		{ "size",   required_argument, NULL, 'S' },
		{ "ibbase", required_argument, NULL, 'R' },
		{ "mport",  required_argument, NULL, 'M' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};
	char *program = argv[0];
	int rc = EXIT_SUCCESS;

	while (1) {
		option = getopt_long_only(argc, argv,
				"hiM:R:S:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'S':
			ibwin_size = strtol(optarg, NULL, 0);
			break;
		case 'i':
			buf_mode = 1;
			break;
		case 'R':
			rio_base = strtoull(optarg, NULL, 0);
			break;
			/* Options common for all modes */
		case 'M':
			mport_id = strtol(optarg, NULL, 0);
			break;
		case 'h':
		default:
			display_help(program);
			exit(EXIT_SUCCESS);
		}
	}

	fd = riodp_mport_open(mport_id, 0);
	if (fd < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, errno);
		exit(EXIT_FAILURE);
	}

	printf("+++ RapidIO Buffer Test +++\n");
	printf("\tmport%d ib_size=0x%x rio_base=0x%x_%x PID:%d\n",
		mport_id, ibwin_size, (uint32_t)(rio_base >> 32),
		(uint32_t)(rio_base & 0xffffffff), (int)getpid());

	do_buf_test(mport_id, rio_base, ibwin_size, buf_mode);
	
	close(fd);
	exit(rc);
}
