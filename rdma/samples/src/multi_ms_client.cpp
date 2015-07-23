/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "librdma.h"

#include "multi_common.h"

#define MSO_NAME	"client_mso"
#define MS_NAME		"client_ms"

void show_help()
{
	puts("multi_ms_client -d<destid>  -i<mspace number> -c<count>| -h");
	puts("-d<destid>     Destination ID of server application owning memory space");
	puts("-i<mspace number>   sspace1, sspace2...etc.");
	puts("-c<count>	     Number of times to send DMA data to memory space");
} /* show_help() */

int run_test(uint32_t destid, unsigned ms_number, unsigned count)
{
	int ret;

	/* Create memory space owner */
	mso_h	msoh;
	ret = rdma_create_mso_h(MSO_NAME, &msoh);
	if(ret) {
		printf("Failed to create mso('%s'), ret = %d\n",
				MSO_NAME, ret);
		return -1;
	}
	puts("Memory space owner created");

	/* Create memory space */
	ms_h	msh;
	puts("Create memory space");
	ret =  rdma_create_ms_h(MS_NAME,
				msoh,
				1024*1024,
				0,
				&msh,
				NULL);
	if (ret) {
		printf("rdma_create_ms_h() for %s failed, ret = %d\n",
							MS_NAME, ret);
		printf("Failed to create ms('%s')\n", MS_NAME);
		ret = rdma_destroy_mso_h(msoh);
		if (ret)
			printf("Failed to destroy mso('%s'), ret = %d\n",
							MSO_NAME, ret);
		return -2;
	}
	puts("Memory space created");

	/* Create subspace for data exchange */
	msub_h	msubh;
	puts("Create subspace");
	ret = rdma_create_msub_h(msh, 0, 4*1024, 0, &msubh );
	if (ret) {
		printf("rdma_create_msub() failed, ret = %d\n", ret);
		ret = rdma_destroy_mso_h(msoh);
		if (ret)
			printf("Failed to destroy mso('%s'), ret = %d\n",
							MSO_NAME, ret);
		return -3;
	}
	puts("Subspace created");

	/* Memory map memory sub-space and put some minimal DMA data */
	void *vaddr;
	ret = rdma_mmap_msub(msubh, &vaddr);
	if (ret) {
		printf("Failed to map msubh, ret = %d\n", ret);
		ret = rdma_destroy_mso_h(msoh);
		if (ret)
			printf("Failed to destroy mso('%s'), ret = %d\n",
							MSO_NAME, ret);
		return -4;
	}
	uint32_t *vaddr32 = (uint32_t *)vaddr;
	*vaddr32 = 0xDEADBEEF;

	/* Prepare server memory space name */
	char ms_name[128];
	sprintf(ms_name, "%s%u", MSPACE_PREFIX, ms_number);

	/* Connect to memory space on server */
	ms_h rem_msh;
	msub_h	rem_msubh;
	uint32_t rem_msub_len;
	ret = rdma_conn_ms_h(16, destid, ms_name, msubh, &rem_msubh,
					&rem_msub_len, &rem_msh, 0);
	if (ret) {
		printf("Failed to connect to '%s' on destid(0x%X), ret = %d\n",
				ms_name, destid, ret);
		ret = rdma_destroy_mso_h(msoh);
		if (ret)
			printf("Failed to destroy mso('%s'), ret = %d\n",
							MSO_NAME, ret);
		return -5;
	}

	/* Push the minimal DMA data to the server */
	struct rdma_xfer_ms_in	 in;
	struct rdma_xfer_ms_out out;
	in.loc_msubh  = msubh;
	in.loc_offset = 0;
	in.num_bytes  = 4*1024;	/* Arbitrary */
	in.rem_msubh  = rem_msubh;
	in.rem_offset = 0;
	in.priority   = 1;
	in.sync_type = rdma_sync_chk;
	for (unsigned i = 0; i < count; i++) {
		ret = rdma_push_msub(&in, &out);
		if (ret) {
			printf("Failed to push data to '%s' on destid(0x%X), ret = %d\n",
					ms_name, destid, ret);
			ret = rdma_destroy_mso_h(msoh);
			if (ret)
				printf("Failed to destroy mso('%s'), ret = %d\n",
							MSO_NAME, ret);
			return -6;
		}
		usleep(100);
	}

	/* Destroy mso (and its ms, and msub children) */
	ret = rdma_destroy_mso_h(msoh);
	if (ret) {
		printf("Failed to destroy mso('%s'), ret = %d\n",
						MSO_NAME, ret);
		return -6;
	}

	return 0;
} /* run_test */


int main(int argc, char *argv[])
{
	char c;
	unsigned i;
	unsigned count;
	uint32_t destid;

	while ((c = getopt(argc, argv, "hd:i:")) != -1)
		switch (c) {

		case 'c':
			count = atoi(optarg);
			printf("Doing DMA transfers %d times\n", count);
			break;

		case 'd':
			destid = atoi(optarg);
			printf("Connecting to destid(0x%X)\n", destid);
			break;

		case 'h':
			show_help();
			exit(1);
			break;

		case 'i':
			i = atoi(optarg);
			printf("Creating %u memory spaces!\n", i);
			break;

		case '?':
			/* Invalid command line option */
			exit(1);
			break;
		default:
			abort();
		}

	return run_test(destid, i, count);
} /* main() */
