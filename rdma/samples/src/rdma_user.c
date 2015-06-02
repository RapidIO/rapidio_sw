/**
 * rmda_user.c
 *
 * A test application that opens a user space created by another application
 * (rdma_server).
 */

#include <stdio.h>
#include <stdlib.h>

#include "test_macros.h"

#include "librdma.h"

#define MSO_NAME	"server1"
#define MS1_SIZE	1024*1024 /* 1MB */
#define MS1_FLAGS	0

/**
 * Corresponds to test_case_d() in rdma_server.c
 */
void test_case_d(void)
{
	int status;
	mso_h	msoh;
	ms_h	msh;
	uint32_t msh_size;
	msub_h	loc_msubh;
	msub_h	rem_msubh;
	uint32_t rem_msub_len;

	/* Open owner */
	status = rdma_open_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	/* Open memory space1 */
	status = rdma_open_ms_h("sspace1", msoh, MS1_FLAGS, &msh_size, &msh);
	CHECK_AND_GOTO(status, "rdma_open_ms_h", close_msoh);

	puts("ENTER to create msub...");
	getchar();

	status = rdma_create_msub_h(msh, 0, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", close_msoh);

	puts("ENTER to accept connection...");
	getchar();

	/* Accept connection from client */
	status = rdma_accept_ms_h(msh, loc_msubh, &rem_msubh, &rem_msub_len, 0);
	CHECK_AND_GOTO(status, "rdma_accept_ms_h", close_msh);

	puts("ENTER to close msh...");
	getchar();

close_msh:
	status = rdma_close_ms_h(msoh, msh);
	CHECK_AND_GOTO(status, "rdma_close_ms_h", close_msoh);

	puts("ENTER to close msoh...");
	getchar();

close_msoh:
	/* Close owner (and memory spaces if applicable) */
	status = rdma_close_mso_h(msoh);
	CHECK(status, "rdma_close_mso_h");
} /* test_case_d() */

int main()
{
	while (1) {
		char ch;

		puts("Select test case:");
		puts("d Open memory space created by another app");
		puts("x Exit RMDA server test app");

		ch = getchar();
		getchar();	/* Eats up the extra \n */

		switch (ch) {

		case 'd':
			test_case_d();
		break;

		case 'x':
			exit(0);
		break;

		default:
			printf("Invalid option: %c\n", ch);
		break;
		};

	}

	return 0;
}
