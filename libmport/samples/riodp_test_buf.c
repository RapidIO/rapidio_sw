/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * User-space DMA/IBwin buffers mapping test program.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License(GPL) Version 2, or the BSD-3 Clause license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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
#include <rapidio_mport_dma.h>
#include <time.h>
#include <signal.h>

#include <rapidio_mport_mgmt.h>

#define DEFAULT_IBWIN_SIZE (2 * 1024 * 1024)

static riomp_mport_t mport_hnd;
static uint32_t ibwin_size = DEFAULT_IBWIN_SIZE;

static int fill_segment(uint32_t mport_id, int seg_id, uint64_t seg_handle, uint32_t seg_size)
{
	riomp_mport_t mphnd;
	int ret;
	uint8_t fill;
	void *ibmap;

	printf("FILL process %d (%d) started\n", seg_id, (int)getpid());

	ret = riomp_mgmt_mport_create_handle(mport_id, 0, &mphnd);
	if (ret < 0) {
		printf("(%d): unable to open mport%d device err=%d\n",
			(int)getpid(), mport_id, ret);
		return -1;
	}

	printf("\t(%d): h=0x%x_%x sz=0x%x\n", (int)getpid(),
		(uint32_t)(seg_handle >> 32),
		(uint32_t)(seg_handle & 0xffffffff), seg_size);

	ret = riomp_dma_map_memory(mphnd, seg_size, seg_handle, &ibmap);
	if (ret) {
		printf("(%d): map failed err=%d\n",
			(int)getpid(), ret);
		goto out;
	}

	fill = 0xc0 | (uint8_t)seg_id;
	memset(ibmap, fill, seg_size);

	ret = riomp_dma_unmap_memory(mphnd, seg_size, ibmap);

	if (ret)
		perror("munmap");

out:
	riomp_mgmt_mport_destroy_handle(&mphnd);
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
		ret = riomp_dma_ibwin_map(mport_hnd, &rio_base, ib_size, &ib_handle);
	else
		ret = riomp_dma_dbuf_alloc(mport_hnd, ib_size, &ib_handle);

	if (ret) {
		printf("Failed to allocate/map IB buffer err=%d\n", ret);
		return ret;
	}

	ret = riomp_dma_map_memory(mport_hnd, ib_size, ib_handle, &ibmap);
	if (ret) {
		perror("mmap");
		goto out;
	}

	printf("\tSuccessfully allocated/mapped %s buffer\n",
		buf_mode?"IBwin":"DMA");
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

	ret = riomp_dma_unmap_memory(mport_hnd, ib_size, ibmap);
	if (ret)
		perror("munmap");
out:
	if (buf_mode)
		ret = riomp_dma_ibwin_free(mport_hnd, &ib_handle);
	else
		ret = riomp_dma_dbuf_free(mport_hnd, &ib_handle);


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

	rc = riomp_mgmt_mport_create_handle(mport_id, 0, &mport_hnd);
	if (rc < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, rc);
		exit(EXIT_FAILURE);
	}

	printf("+++ RapidIO Buffer Test +++\n");
	printf("\tmport%d ib_size=0x%x rio_base=0x%x_%x PID:%d\n",
		mport_id, ibwin_size, (uint32_t)(rio_base >> 32),
		(uint32_t)(rio_base & 0xffffffff), (int)getpid());

	do_buf_test(mport_id, rio_base, ibwin_size, buf_mode);
	
	riomp_mgmt_mport_destroy_handle(&mport_hnd);
	exit(rc);
}
