/* RDMA Test App */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "test_macros.h"

#include "librdma.h"

#define MSO_NAME	"server1"
#define MS1_SIZE	1024*1024 /* 1MB */
#define MS2_SIZE	512*1024  /* 512 KB */
#define MS1_FLAGS	0
#define MS2_FLAGS	0

/**
 * Verifies that creation of an mso by the same name fails.
 */
void test_case_a(void)
{
	mso_h 	msoh1;
	mso_h 	msoh2;
	int	status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh1);
	if (status == RDMA_DAEMON_UNREACHABLE) {
		puts("Daemon died/restarted. Retrying");
		status = rdma_create_mso_h(MSO_NAME, &msoh1);
		CHECK_AND_RET(status, "rdma_create_mso_h");
	}

	puts("Press ENTER to create another MSO with same name");
	getchar();

	/* Try to create owner AGAIN */
	if (rdma_create_mso_h(MSO_NAME, &msoh2))
		puts("TEST PASSED. Duplicate mso creation disallowed");
	else
		puts("TEST FAILED. Should not create more than one owner!");

	status = rdma_destroy_mso_h(msoh1);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case_a() */

/**
 * Verify that we disallow creating multiple memory spaces with the same name.
 */
void test_case_b(void)
{
	mso_h 	msoh;
	ms_h	msh1;
	ms_h	msh2;
	int	status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create memory space1 AGAIN */
	status = rdma_create_ms_h("sspace1", msoh, MS2_SIZE, MS2_FLAGS, &msh2, NULL);
	if (status)
		puts("TEST PASSED. Disallowed duplicate ms name");
	else
		puts("TEST FAILED. Allowed duplicate ms name");
destroy_msoh:
	/* Destroying msoh will also destroy msh1 */
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case_b() */

/**
 *  Allocate 3MB of memory spaces. If only one 2MB IBWIN is allocated, this should fail.
 *  If 2 or more are allocated, the 3rd memory space should be in IBWIN 1.
 */
void test_case_c()
{
	mso_h 	msoh;
	ms_h	msh1;
	ms_h	msh2;
	ms_h	msh3;
	int	status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	puts("Press ENTER to create FIRST memory space");
	getchar();

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, 1024*1024, 0, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	puts("Press ENTER to create SECOND memory space");
	getchar();

	/* Create memory space2 */
	status = rdma_create_ms_h("sspace2", msoh, 1024*1024, 0, &msh2, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	puts("Press ENTER to create THIRD memory space");
	getchar();
	getchar();	/* Eat extra <cr> */

	/* Create memory space3 */
	status = rdma_create_ms_h("sspace3", msoh, 1024*1024, 0, &msh3, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	puts("Press ENTER to destroy them all");
	getchar();

destroy_msoh:
	/* Destroy owner */
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");

	puts("Press ENTER to quit");
	getchar();
} /* test_case_c() */

void test_case_d()
{
	mso_h 	msoh;
	ms_h	msh;
	msub_h	msubh;
	int	status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	puts("Press ENTER to create memory space & subspace");
	getchar();

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, 1024*1024, 0, &msh, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create memory sub-space. This subspace will also be created
	 * by rdma_user and at the same offset within the mspace. Then
	 * rdma_user can map the space and access the same data */
	status = rdma_create_msub_h(msh, 0, 4096, 0, &msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	printf("Open 'sspace1' from 'rdma_user', then press ENTER\n");
	getchar();

	puts("press ENTER to destroy the msoh");
	getchar();

destroy_msoh:
	/* Destroy owner */
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");

	puts("Press ENTER to quit");
	getchar();
} /* test_case_d() */

void test_case_f()
{
	mso_h 	msoh;
	ms_h	msh;
	msub_h	loc_msubh;
	msub_h	rem_msubh;
	uint32_t rem_msub_len;
	int	status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	puts("Press ENTER to create memory space & subspace");
	getchar();

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, 1024*1024, 0, &msh, 0);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create memory sub-space. This subspace will also be created
	 * by rdma_user and at the same offset within the mspace. Then
	 * rdma_user can map the space and access the same data */
	status = rdma_create_msub_h(msh, 0, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	/* Accept connection from client */
	status = rdma_accept_ms_h(msh, loc_msubh, &rem_msubh, &rem_msub_len, 3);
	CHECK_AND_GOTO(status, "rdma_accept_ms_h", destroy_msoh);

	puts("press ENTER to ACCEPT AGAIN\n");
	getchar();

	/* Accept connection from client */
	status = rdma_accept_ms_h(msh, loc_msubh, &rem_msubh, &rem_msub_len, 0);
	CHECK_AND_GOTO(status, "rdma_accept_ms_h", destroy_msoh);

	puts("press ENTER to destroy the msoh");
	getchar();

destroy_msoh:
	/* Destroy owner */
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");

	puts("Press ENTER to quit");
	getchar();
} /* test_case_f() */

/**
 * Verifies that creation of an mso by the same name fails.
 */
void test_case_g(void)
{
	mso_h 	msoh1;
	mso_h 	msoh2;
	int	status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh1);
	puts("Kill and restart the RDMA Daemon!!");

	puts("Press ENTER to create another MSO with same name");
	getchar();

	/* Try to create owner AGAIN. It should work since we have restarted the daemon
	 * and that purged the database. */
	if (rdma_create_mso_h(MSO_NAME, &msoh2))
		puts("TEST FAILED. Restarting the daemon should have purged the database");
	else
		puts("TEST PASSED. Database was purged and owner created again.");

	status = rdma_destroy_mso_h(msoh1);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case_g() */

/**
 * This test case tries to map/unmap/map/unmap the same msub to see
 * if something is broken in that functionality.
 */
void test_case0(uint32_t msub_ofs)
{
	mso_h 	msoh;
	ms_h	msh1;
	msub_h	loc_msubh;
	int	status;
	void	*vaddr;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create subspace in memory space 1 */
	status = rdma_create_msub_h(msh1, msub_ofs, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	/* Map local memory subspace to virtual memory */
	status = rdma_mmap_msub(loc_msubh, &vaddr);
	CHECK_AND_GOTO(status, "rdma_mmap_msub", destroy_msoh);

	puts("Mapping SUCCESSful");

	/* Unmap sub-space */
	status = rdma_munmap_msub(loc_msubh, vaddr);
	CHECK_AND_GOTO(status, "rdma_munmap_msub_h", destroy_msoh);

	puts("UnMapping SUCCESSful");

	/* Map local memory subspace to virtual memory */
	status = rdma_mmap_msub(loc_msubh, &vaddr);
	CHECK_AND_GOTO(status, "rdma_mmap_msub", destroy_msoh);

	puts("Re-Mapping SUCCESSful");

	/* Unmap sub-space */
	status = rdma_munmap_msub(loc_msubh, vaddr);
	CHECK_AND_GOTO(status, "rdma_munmap_msub_h", destroy_msoh);

	puts("Re-UnMapping SUCCESSful");

destroy_msoh:
	/* Destroy owner */
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case0() */

/**
 * 1. Create mso
 * 2. Create ms
 * 3. Create msub
 * 4. MMap msub
 * 5. Accept connections to ms
 * 6. Receive DMA data from client
 * 7. Pause for DMA to be read back by client
 * 8. Quit
 */
void test_case1(uint32_t msub_ofs)
{
	mso_h 	msoh;
	ms_h	msh1;
	msub_h	loc_msubh;
	msub_h	rem_msubh;
	uint32_t rem_msub_len;
	int	status;
	void	*vaddr;
	uint8_t *p;
	unsigned i;

	/* Creat owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create subspace in memory space 1 */
	status = rdma_create_msub_h(msh1, msub_ofs, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	/* Map local memory subspace to virtual memory */
	status = rdma_mmap_msub(loc_msubh, &vaddr);
	CHECK_AND_GOTO(status, "rdma_mmap_msub", destroy_msoh);

	puts("Accepting connections..");

	/* Accept connection from client */
	status = rdma_accept_ms_h(msh1, loc_msubh, &rem_msubh, &rem_msub_len, 0);
	CHECK_AND_GOTO(status, "rdma_accept_ms_h", unmap_msub);

	puts("Connected!\nPress ENTER to display received DMA data");
	getchar();

	p = (uint8_t *)vaddr;
	for (i = 0; i < 128; i++) {
		printf("0x%02X-", p[i]);
	}

	puts("\nPress ENTER to destroy owner and quit");
	getchar();

/* NOTE: destroying an msoh does not automatically unmap any msubs
 * with the ms's belonging to the msoh */
unmap_msub:
	/* Unmap sub-space */
	status = rdma_munmap_msub(loc_msubh, vaddr);
	CHECK_AND_GOTO(status, "rdma_munmap_msub_h", destroy_msoh);

destroy_msoh:
	/* Destroy owner */
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case1() */

/**
 * Test case 5 is mainly for testing the 'destroy' functionality.
 */
void test_case5(void)
{
	mso_h 	msoh;
	ms_h	msh1;
	msub_h	loc_msubh;
	msub_h	rem_msubh;
	uint32_t rem_msub_len;
	int	status;

	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	status = rdma_create_ms_h("sspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	status = rdma_create_msub_h(msh1, 0x00, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	puts("Waiting for client to connect...");

	status = rdma_accept_ms_h(msh1, loc_msubh, &rem_msubh, &rem_msub_len, 0);
	CHECK_AND_GOTO(status, "rdma_accept_ms_h", destroy_msoh);

	/* Now destroy memory space and observe that everything cleans up OK */
	puts("Press ENTER to destroy memory space");
	getchar();

	status = rdma_destroy_ms_h(msoh, msh1);
	CHECK_AND_GOTO(status, "rdma_destroy_ms_h", destroy_msoh);

	/* Destroy memory space owner */
	puts("Press ENTER to destroy memory space owner, and quit");
	getchar();

destroy_msoh:
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case5() */

int main()
{
	while (1) {
		char ch;

		puts("Select test case:");
		puts("a Creating multiple msohs from the same app");
		puts("b Creating multiple ms with the same name from the same app");
		puts("c Create 3 memory spaces, 1MB each, then delete the mso");
		puts("d Create memory space to be opened by another app");
		puts("e Create mso/ms/msub and wait for user to open/open/DMA/close/close");
		puts("f Two accept calls from same app on same ms fail\n");
		puts("g Create same mso twice but restart RDMA daemon in between");
		puts("0 Simple msub creation, mapping, unmapping, re-mapping");
		puts("1 Simple accept/DMA transfer");
		puts("2 Simple accept/DMA transfer with server msub offset within ms");
		puts("3 Simple connect/DMA transfer/disconnect with offset in loc msub");
		puts("4 Simple connect/DMA transfer/disconnect with offset in rem msub");
		puts("5 Destroy after accept" );
		puts("6 Simple accept/DMA transfer from multi buffer");
		puts("x Exit RMDA server test app");

		ch = getchar();
		getchar();	/* Eats up the extra \n */


		switch (ch) {

		case 'a':
			test_case_a();
			break;

		case 'b':
			test_case_b();
			break;

		case 'c':
			test_case_c();
			break;

		case 'd':
		case 'e':
			test_case_d();
			break;

		case 'f':
			test_case_f();
			break;
		case 'g':
			test_case_g();
			break;

		case '0':
			test_case0(0x00);
		break;

		case '1':
		case '3': /* Offset of 'push'/'pull' specified in rdma_client */
		case '4': /* Offset of 'push'/'pull' specified in rdma_client */
			test_case1(0x00);
			break;

		case '2':
			test_case1(0x1000);
			break;

		case '5':
			test_case5();
			break;

		case '6':
			test_case1(0x00);
			break;

		case 'x':
			exit(0);

		default:
			printf("Invalid option: %c\n", ch);
		};
	}

	return 0;
}
