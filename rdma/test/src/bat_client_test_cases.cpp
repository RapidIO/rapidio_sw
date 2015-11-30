#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#include <cinttypes>
#endif

#include <vector>
#include <sstream>
#include <algorithm>

#include <cstdlib>
#include <cstdio>
#include <inttypes.h>

#include "librdma.h"
#include "cm_sock.h"
#include "bat_common.h"
#include "bat_client_private.h"
#include "bat_client_test_cases.h"

using std::vector;
using std::stringstream;

#define BAT_MIN_BLOCK_SIZE	4096

char loc_mso_name[MAX_NAME];
char loc_ms_name[MAX_NAME];
char rem_mso_name[MAX_NAME];
char rem_ms_name1[MAX_NAME];
char rem_ms_name2[MAX_NAME];
char rem_ms_name3[MAX_NAME];

/* ------------------------------- Test Cases -------------------------------*/

/**
 * Create a number of memory space owners. Some have duplicate names and are
 * expected to fail. Others should succeed including ones which are substrings
 * of existing memory space owner names.
 */
int test_case_a(void)
{
#define NUM_MSOS	12

	static char mso_names[][NUM_MSOS] = {
		"January", "February", "March", "April",
		"May", "June", "January", "Feb",
		"September", "October", "Mayor", "January"
	};

	/* Expect failure on creating msos with duplicate names */
	static int ret_codes[NUM_MSOS] = { 0, 0, 0, 0,
					   0, 0, RDMA_DUPLICATE_MSO, 0,
					   0, 0, 0, RDMA_DUPLICATE_MSO
	};

	static mso_h	mso_handles[NUM_MSOS];

	int ret;
	bool failed = false;

	/* Create REMOTE memory space owners and check that they worked
	 * except for the duplicate ones; those should reutnr RDMA_DUPLICATE_MSO
	 */
	for (unsigned i = 0; i < NUM_MSOS; i++) {
		ret = create_mso_f(bat_first_client,
				   bm_first_tx,
				   bm_first_rx,
				   mso_names[i],
				   &mso_handles[i]);
		/* Compare with expected return codes */
		if (ret != ret_codes[i]) { \
			fprintf(log_fp, "%s FAILED, line %d, i = %u\n",
					__func__, __LINE__, i);
			failed = true;
		}
	}

	/* All is good. Just destroy the msos */
	for (unsigned i = 0; i < NUM_MSOS; i++) {
		/* The ones that were created successfully have non-0 handles */
		if (mso_handles[i] != 0) {
			/* Don't overwrite 'ret', use 'rc' */
			ret = destroy_mso_f(bat_first_client,
					   bm_first_tx,
					   bm_first_rx,
					   mso_handles[i]);
			if (ret != 0) {
				fprintf(log_fp, "%s FAILED, line %d, ret=%d\n",
					__func__, __LINE__, ret);
				failed = true;
			}
		}
	}

	/* Clear the handles since we will do a second test */
	memset(mso_handles, 0, sizeof(mso_handles));

	/* Second test, requested by Barry */
#define NUM_UNIQUE_MSOS	10

	static char mso_unique_names[][NUM_UNIQUE_MSOS] = {
		"one", "two", "three", "four", "five", "six", "seven",
		"eight", "nine", "ten"
	};

	/* Create 10 unique LOCAL msos. They should all work */
	for (unsigned i = 0; i < NUM_UNIQUE_MSOS; i++) {
		ret = rdma_create_mso_h(mso_unique_names[i], &mso_handles[i]);
		if (ret != 0) { \
			fprintf(log_fp, "%s FAILED, line %u, i = %d\n",
					__func__, __LINE__, i);
			failed = true;
		}
	}

	/* Create 3 similar msos. One similar to "one", one similar to "six",
	 * and one similar to "ten". All should return RDMA_DUPLICATE_MSO.
	 */
	mso_h	dup_mso;

	ret = rdma_create_mso_h("one", &dup_mso);
	if (ret != RDMA_DUPLICATE_MSO) { \
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}
	ret = rdma_create_mso_h("six", &dup_mso);
	if (ret != RDMA_DUPLICATE_MSO) { \
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}
	ret = rdma_create_mso_h("ten", &dup_mso);
	if (ret != RDMA_DUPLICATE_MSO) { \
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}

	/* Now destroy the LOCAL unique msos */
	for (unsigned i = 0; i < NUM_UNIQUE_MSOS; i++) {
		ret = rdma_destroy_mso_h(mso_handles[i]);
		if (ret) {
			fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
					__func__, __LINE__, ret);
			failed = true;
		}
	}

	if (!failed) {
		BAT_EXPECT_PASS(ret);
	}

	return ret;
#undef NUM_MSOS
#undef NUM_UNIQUE_MSOS
} /* test_case_a() */

/**
 * Create a number of memory spaces. Some have duplicate names and are
 * expected to fail. Others should succeed including ones which are substrings
 * of existing memory space names.
 */
int test_case_b(void)
{
#define NUM_MSS	12
#define MS_SIZE	64*1024		/* 64K */

	static char ms_names[][NUM_MSS] = {
		"January", "February", "March", "April",
		"May", "June", "January", "Feb",
		"September", "October", "Mayor", "January"
	};

	/* Expect failure on creating ms'es with duplicate names */
	static int ret_codes[NUM_MSS] = { 0, 0, 0, 0,
					   0, 0, RDMA_DUPLICATE_MS, 0,
					   0, 0, 0, RDMA_DUPLICATE_MS
	};

	mso_h		msoh;
	static ms_h	ms_handles[NUM_MSS];

	int ret;
	bool failed = false;

	/* Create mso */
	ret = create_mso_f(bat_first_client, bm_first_tx, bm_first_rx,
			   	   	   	   	  rem_mso_name, &msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
						__func__, __LINE__, ret);
		return ret; /* No point testing anything else */
	}

	/* Create 12 memory spaces with some duplicate names */
	for (unsigned i = 0; i < NUM_MSS; i++) {
		uint32_t actual_size;

		ret = create_ms_f(bat_first_client,
				  bm_first_tx,
				  bm_first_rx,
				  ms_names[i],
				  msoh,
				  MS_SIZE,
				  0,
				  &ms_handles[i],
				  &actual_size);

		/* Compare with expected return codes */
		if (ret != ret_codes[i]) { \
			fprintf(log_fp, "%s FAILED, line %d, i = %u\n",
							__func__, __LINE__, i);
			failed = true;
		}
	}

	/* Destroy the memory spaces by destroying the mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}

	/* Second test, requested by Barry */

	/* Create mso */
	ret = create_mso_f(bat_first_client, bm_first_tx, bm_first_rx,
			   	   	   	   	  rem_mso_name, &msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
						__func__, __LINE__, ret);
		return ret; /* No point testing anything else */
	}
#define NUM_UNIQUE_MSS	10
	static char ms_unique_names[][NUM_UNIQUE_MSS] = {
		"one", "two", "three", "four", "five",
		"six", "seven", "eight", "nine", "ten"};

	/* Create 12 memory spaces with some duplicate names */
	for (unsigned i = 0; i < NUM_UNIQUE_MSS; i++) {
		uint32_t actual_size;

		ret = create_ms_f(bat_first_client,
				  bm_first_tx,
				  bm_first_rx,
				  ms_unique_names[i],
				  msoh,
				  MS_SIZE,
				  0,
				  &ms_handles[i],
				  &actual_size);

		/* Compare with expected return codes */
		if (ret != 0) { \
			fprintf(log_fp, "%s FAILED, line %d, i = %u\n",
							__func__, __LINE__, i);
			failed = true;
		}
	}

	/* Now create 3 duplicate mss at positions 0, 6, and 9 */
	uint32_t actual_size;
	ms_h	 dummy_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  "one",
			  msoh,
			  MS_SIZE,
			  0,
			  &dummy_msh,
			  &actual_size);
	if (ret != RDMA_DUPLICATE_MS) { \
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  "six",
			  msoh,
			  MS_SIZE,
			  0,
			  &dummy_msh,
			  &actual_size);
	if (ret != RDMA_DUPLICATE_MS) { \
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  "ten",
			  msoh,
			  MS_SIZE,
			  0,
			  &dummy_msh,
			  &actual_size);
	if (ret != RDMA_DUPLICATE_MS) { \
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}

	/* Destroy the memory spaces by destroying the mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}

	if (!failed) {
		BAT_EXPECT_PASS(ret);
	}

#undef NUM_UNIQUE_MSS
#undef NUM_MSS
#undef MS_SIZE

	return ret;
} /* test_case_b() */


/**
 *
 */
int test_case_c(void)
{
	unsigned  num_ibwins;
	uint32_t  ibwin_size;
	int	  rc;

	/* Determine number of IBWINs and the size of each IBWIN */
	rc = rdma_get_ibwin_properties(&num_ibwins, &ibwin_size);
	BAT_EXPECT_RET(rc, 0, exit);
	printf("%u inbound windows, %uKB each\n", num_ibwins, ibwin_size/1024);

	/* Create a client mso */
	mso_h	client_msoh;
	rc = rdma_create_mso_h("test_case_c_mso", &client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* First off, allocating a memory space > ibwin_size is an error */
	ms_h	oversize_msh;
	rc = rdma_create_ms_h("oversize_ms",
			      client_msoh,
			      ibwin_size + BAT_MIN_BLOCK_SIZE,
			      0,
			      &oversize_msh,
			      NULL);
	if (rc == 0) {
		/* rc should not have been 0 since the space is too large. */
		rc = 1;
		BAT_EXPECT_RET(rc, 0, free_mso);
	}


	struct ms_info_t {
		uint32_t	size;
		ms_h		handle;
		uint64_t	rio_addr;
		ms_info_t(uint32_t size, ms_h handle, uint64_t rio_addr) :
			size(size), handle(handle), rio_addr(rio_addr)
		{}
		bool operator ==(uint32_t size) {
			return this->size == size;
		}
	};
	static vector<ms_info_t>	ms_info;

	/* Now create a number of memory spaces such that they fill each
	 * inbound window with different sizes starting with 1/2 the inbound
	 * window size and ending with 4K which is the minimum block size that
	 * can be mapped.
	 */

	/* First generate the memory space sizes */
	for (unsigned ibwin = 0; ibwin < num_ibwins; ibwin++) {
		auto size = ibwin_size/2;

		while (size >= BAT_MIN_BLOCK_SIZE) {
			ms_info.emplace_back(size, 0, 0);
			size /= 2;
		}
		ms_info.emplace_back(size * 2, 0, 0);
	}

	/* Now create the memory spaces */
	for (unsigned i = 0; i < ms_info.size(); i++) {
		stringstream ms_name;
		ms_name << "mspace" << i;
		rc = rdma_create_ms_h(ms_name.str().c_str(),
				      client_msoh,
				      ms_info[i].size,
				      0,
				      &ms_info[i].handle,
				      &ms_info[i].size);
		BAT_EXPECT_RET(rc, 0, free_mso);
		rdma_get_msh_properties(ms_info[i].handle,
				&ms_info[i].rio_addr, &ms_info[i].size);
		printf("0x%016" PRIx64 ", 0x%016" PRIx64 ", 0x%X\n",
			ms_info[i].handle, ms_info[i].rio_addr, ms_info[i].size);
	}

	puts("Memory spaces created\nPress ENTER to continue...");
	getchar();

	/**
	 * Test: Just ensure that freed memory spaces are re-used.
	 *
	 * 1. Free the memory spaces in WIN1
	 * 2. Find an 8KB space in WIN0 and free it.
	 * 3. Allocate a new 8K space.
	 * 4. Verify that the new space took the freed space and did not end
	 * up in WIN1. Do that by comparing the freed and new RIO addresses
	 */
	{
		/* Find an 8K memory space */
		auto it = find(begin(ms_info), end(ms_info), uint32_t(8*1024));

		/* Read its RIO address */
		uint64_t old_8k_rio_addr;
		uint32_t dummy_size;
		rc = rdma_get_msh_properties(it->handle, &old_8k_rio_addr, &dummy_size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		puts("8K memory space destroyed\nPress ENTER to continue...");
		getchar();

		/* Destroy all IBWIN1 memory spaces */
#if 0
		for (unsigned i = (ms_info.size() / 2); i < ms_info.size(); i++) {
			printf("Destroying mspace%u\n", i);
			rc = rdma_destroy_ms_h(client_msoh, ms_info[i].handle);
			BAT_EXPECT_RET(rc, 0, free_mso);
		}
#endif
		rc = rdma_destroy_ms_h(client_msoh, ms_info[0x0B].handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		puts("IBWIN1 memory spaces destroyed\nPress ENTER to continue...");
		getchar();

		/* Now allocate another 8K memory space */
		ms_h	new_8k_msh;
		rc = rdma_create_ms_h("new_8k_ms", client_msoh,
				 8*1024, 0, &new_8k_msh, &dummy_size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Read back the RIO address */
		uint64_t new_8k_rio_addr;
		printf("Reading back properties of 'new_8k_ms'...\n");
		rc = rdma_get_msh_properties(new_8k_msh, &new_8k_rio_addr, &dummy_size);

		if (new_8k_rio_addr == old_8k_rio_addr) {
			rc = 0;
		} else {
			rc = -1;
		}
	}

free_mso:
	/* Delete the mso */
	rc = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

exit:
	BAT_EXPECT_PASS(rc);

	return 0;
} /* test_case_c() */

/**
 * This is the old test case 'c' which allocates memory remotely on the
 * server. The new test case 'c' will use local allocation and maybe
 * duplicated here to perform a remove version.
 */
int test_case_c_old(void)
{
	int	  ret;


	/* Create mso */
	mso_h	msoh1;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh1);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create first ms */
	ms_h	msh1;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, msoh1, 1024*1024, 0, &msh1, NULL);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create second ms */
	ms_h	msh2;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name2, msoh1, 1024*1024, 0, &msh2, NULL);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create third ms */
	ms_h	msh3;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name3, msoh1, 1024*1024, 0, &msh3, NULL);
	BAT_EXPECT_PASS(ret);

free_mso:
	/* Delete the mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msoh1);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_c() */

/**
 * Create a number of msubs, some overlapping.
 */
int test_case_g(void)
{
	int ret;

	/* Create mso */
	mso_h	msoh1;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh1);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create ms */
	ms_h	msh1;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, msoh1, MS1_SIZE, 0, &msh1, NULL);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create 1st msub */
	msub_h  msubh1;
	ret = create_msub_f(bat_first_client,
		 	    bm_first_tx,
			    bm_first_rx,
			    msh1, 0, 4096, 0, &msubh1);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create 2nd msub */
	msub_h  msubh2;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msh1, 4096, 8192, 0, &msubh2);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create 3rd msub overlapping with 2nd msub */
	msub_h  msubh3;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msh1, 8192, 8192, 0, &msubh3);
	BAT_EXPECT_PASS(ret);

free_mso:
	/* Delete the mso */
	ret = destroy_mso_f(bat_first_client, bm_first_tx, bm_first_rx, msoh1);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return 0;
} /* test_case_g() */

/**
 * Test accept_ms_h()/conn_ms_h()/disc_ms_h()..etc.
 *
 * @ch	if ch is 'h' run test case 'h', else run test case 'i'
 */
int test_case_h_i(char ch, uint32_t destid)
{
	int ret;

	LOG("test_case%c\n", ch);

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name,
			   &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1,
			  server_msoh,
			  MS1_SIZE,
			  0,
			  &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msh, 0, 4096, 0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0, &client_msh,
									NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, 4096, 0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_first_client,
			  bm_first_tx,
			  server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     30);	/* 30 second-timeout */
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Test case 'h' disconnects first. Test case 'i' destroys
	 * the ms on the server and processes the incoming destroy message.
	 */
	if (ch == 'h') {
		/* Now disconnect from server */
		ret = rdma_disc_ms_h(server_msh_rb, client_msubh);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	}


free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	/* If we reach till here without errors, then we have passed */
	BAT_EXPECT_PASS(ret);

	return 0;
} /* test_case_h_i() */

/**
 * Test accept_ms_h()/conn_ms_h() then kill remote app
 * The remote daemon, upon detecting that the remote app has died
 * should self-destroy the remote ms and the local daemon should
 * get notified causing the local database to be cleared of that ms
 * and also of the local msub created to connect to the remote ms.
 *
 * If the remote daemon itself is killed, it, too should self-destroy
 * the ms before dying.
 *
 * ch:	'j' or 'k'
 * 'j'	Kill the remote app
 * 'k'	Kill the remote daemon
 */
int test_case_j_k(char ch, uint32_t destid)
{
	int ret;

	LOG("test_case%c\n", ch);

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name,
			   &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1,
			  server_msoh,
			  MS1_SIZE,
			  0,
			  &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msh, 0, 4096, 0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0, &client_msh,
									NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, 4096, 0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_first_client,
			  bm_first_tx,
			  server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     30);	/* 30 second-timeout */
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	if (ch == 'j') {
		/* Kill remote app */
		puts("Telling remote app to die");
		ret = kill_remote_app(bat_first_client, bm_first_tx);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	} else if (ch == 'k') {
		/* Kill remote daemon */
		puts("Telling remote daemon to die");
		ret = kill_remote_daemon(bat_first_client, bm_first_tx);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	} else {
		ret = -1;	/* FAIL. Wrong parameter */
		BAT_EXPECT_PASS(ret);
	}

	/* Give remote local daemon a chance to detect remote daemon's
	 * death.
	 */
	sleep(2);

	/* If the remote 'server_msh_rb' is not in the database
	 * rdma_disc_ms_h() returns 0 which is a pass. If the database
	 * was nor properly cleared then rdma_disc_ms_h() will fail
	 * at another stage.
	 */
	ret = rdma_disc_ms_h(server_msh_rb, client_msubh);
	BAT_EXPECT_PASS(ret);

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	return 0;

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	/* If we reach till here without errors, then we have passed */
	return 0;
} /* test_case_j_k() */

/**
 * The child creates an mso then dies. The return code of the child indicates
 * that the mso was successfully created. Then with the death of the child
 * the daemon auto-deletes the mso. The parent tries to open the m'so' and gets
 * an error indicating the mso doesn't exit.
 */
int test_case_l()
{
	pid_t child;
	int	ret;

	LOG("test_case%c\t", 'l');

	child = fork();

	if (child == 0) { /* Child */
		if (execl("./bat_child", "bat_child", loc_mso_name, NULL) == -1)
			perror("test_case_l:");
	} else {	/* Parent */
		int status;
		pid_t dead_child;
		mso_h	opened_msoh;

		dead_child = wait(&status);
		if ((dead_child == child) && WEXITSTATUS(status) == 0) {
			ret = rdma_open_mso_h(loc_mso_name, &opened_msoh);
			if (ret) {
				LOG("Can't open mso. PASS\n");
				ret = 0;
			} else {
				LOG("mso wasn't destroyed. FAILED\n");
			}
		} else {
			LOG("Failed during mso creation. FAIL\n");
			ret = WEXITSTATUS(status);
		}

	}

	return ret;
} /* test_case_l() */


/**
 * Kill then restart the daemon. Verify that memory space owners have
 * been cleaned up by creating the same mso with the same name again.
 */
int test_case_m()
{
	int ret;
	pid_t child;

	LOG("test_case%c\t", 'm');

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Kill local daemon */
	ret = rdmad_kill_daemon();
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	/**
	 * When the daemon is killed, it cleans up the memory space owners
	 * and that includes notifying apps that own those memory space owners
	 * that their msos are no longer valid.
	 */

	/* Restart local daemon */
	child = fork();

	if (child == 0) { /* Child */
		if (execl("../rdmad", "rdmad", NULL) == -1)
			perror("test_case_l:");
	} else {
		/* Parent doesn't wait for daemon to die but gives it
		 * 1 second to start up and initialize its sockets..etc.
		 */
		sleep(1);

		/* Create a client mso */
		ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
		BAT_EXPECT_RET(ret, 0, exit);
		LOG("mso created. PASS\n");
		exit(0);
	}

free_client_mso:
	rdma_destroy_mso_h(client_msoh);

exit:
	return ret;
} /* test_case_ms() */

#define DMA_DATA_SIZE	64
#define DMA_DATA_SECTION_SIZE	8

uint8_t dma_data_copy[DMA_DATA_SIZE];

/* Prepare DMA data */
void prep_dma_data(uint8_t *dma_data)
{
	/* 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 3 3 3 3 3 3 3 3 .. etc. */
	for(unsigned i = 0; i < DMA_DATA_SIZE; i += DMA_DATA_SECTION_SIZE) {
		memset(&dma_data[i], i + 1, DMA_DATA_SECTION_SIZE);
	}
	/* Make a copy of the data in the 'copy' buffer */
	memcpy(dma_data_copy, dma_data, sizeof(dma_data_copy));

} /* Prepare DMA data */

/* dump specified data starting at specified offset */
void dump_data(uint8_t *dma_data, unsigned offset)
{
	for(unsigned i = offset; i < DMA_DATA_SIZE; i++) {
		printf("0x%02X - ", dma_data[i]);
	}
	printf("\n");
} /* dump_data() */

int do_dma(msub_h client_msubh,
		  msub_h server_msubh,
		  uint32_t ofs_in_loc_msub,
		  uint32_t ofs_in_rem_msub,
		  rdma_sync_type_t sync_type)
{
	/* Map client msub to virtual memory pointer */
	void *vaddr;
	uint8_t *dma_data;

	int ret = rdma_mmap_msub(client_msubh, &vaddr);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Prep DMA data */
	dma_data = (uint8_t *)vaddr + ofs_in_loc_msub;
	prep_dma_data(dma_data);

	/* Prep RDMA xfer structs */
	struct rdma_xfer_ms_in	 in;
	struct rdma_xfer_ms_out out;
	in.loc_msubh = client_msubh;
	in.loc_offset = ofs_in_loc_msub;
	in.num_bytes = DMA_DATA_SIZE;
	in.rem_msubh = server_msubh;
	in.rem_offset = ofs_in_rem_msub;
	in.priority = 1;
	in.sync_type = sync_type;

	/* Temporarily to determine failure cause */
	LOG("client_msubh = 0x%016" PRIx64 ", server_msubh = 0x%016" PRIx64 "\n",
			client_msubh, server_msubh);
	LOG("ofs_in_loc_msub = 0x%X, ofs_in_rem_msub = 0x%X, ",
			ofs_in_loc_msub, ofs_in_rem_msub);
	LOG("num_bytes = 0x%X, sync_type: %d\n", DMA_DATA_SIZE, sync_type);

	/* Push the RDMA data */
	ret = rdma_push_msub(&in, &out);
	BAT_EXPECT_RET(ret, 0, unmap_msubh);

	/* If async mode, must call rdma_sync_chk_push_pull() */
	if (sync_type == rdma_async_chk) {
		LOG("ASYNC DMA: ");
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, unmap_msubh);
	}

	/* Flush rdma_data */
	memset(dma_data, 0xDD, DMA_DATA_SIZE);

	/* Pull the DMA data back */
	ret = rdma_pull_msub(&in, &out);
	BAT_EXPECT_RET(ret, 0, unmap_msubh);

	/* If async mode, must call rdma_sync_chk_push_pull() */
	if (sync_type == rdma_async_chk) {
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, unmap_msubh);
	}

	/* Dump the data out for debugging */
	dump_data(dma_data, ofs_in_loc_msub);

	/* Now compare recieved data with the copy */
	ret = memcmp(dma_data, dma_data_copy, DMA_DATA_SIZE);
	BAT_EXPECT_PASS(ret);

unmap_msubh:
	ret = rdma_munmap_msub(client_msubh, vaddr);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	return ret;
} /* do_dma() */

int test_case_dma(char tc,
		  uint32_t destid,
		  uint32_t loc_msub_ofs_in_ms,
		  uint32_t ofs_in_loc_msub,
		  uint32_t ofs_in_rem_msub,
		  rdma_sync_type_t sync_type)
{
	int ret;
	const unsigned MSUB_SIZE = 4096;

	LOG("test_case%c\n", tc);

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, server_msoh, MS1_SIZE, 0, &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msh, 0, MSUB_SIZE, 0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0, &client_msh,
									NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, loc_msub_ofs_in_ms, MSUB_SIZE,
							      0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_first_client,
			  bm_first_tx,
			  server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     0);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Do the DMA transfer and comparison */
	ret = do_dma(client_msubh,
		     server_msubh_rb,
		     ofs_in_loc_msub,
		     ofs_in_rem_msub,
		     sync_type);
	BAT_EXPECT_RET(ret, 0, disconnect);

disconnect:
	/* Now disconnect from server */
	ret = rdma_disc_ms_h(server_msh_rb, client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_1() */

/* NOTE: test_case_6() is BROKEN. Need to review ms names ..etc. */
int test_case_6()
{
	int ret = 0;
#if 0
	const unsigned MSUB_SIZE = 4096;

	LOG("test_case%c\n", '6');

	/* First create mso, and ms on server */
	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_first_client,
			      bm_first_tx,
			      bm_first_rx,
			      rem_mso_name,
			      &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, server_msoh, MS1_SIZE, 0, &server_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Now connect to the 'user' application on the second channel */
	connect_to_channel(second_channel,
	                   "second_channel",
			   &bat_second_client,
			   &bm_second_tx,
			   &bm_second_rx);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* On the 'user' application, open the mso & ms */
	mso_h	user_msoh;
	ret = open_mso_f(bat_second_client,
			 bm_second_tx,
			 bm_second_rx,
			 rem_mso_name, &user_msoh);
	BAT_EXPECT_RET(ret, 0, free_second_bat_client);

	ms_h	user_msh;
	uint32_t user_msh_size;
	ret = open_ms_f(bat_second_client,
			bm_second_tx,
			bm_second_rx,
			rem_ms_name1, user_msoh, 0, &user_msh_size, &user_msh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	/* On the 'user' create an msub, and then do accept on the ms */
	msub_h  user_msubh;
	ret = create_msub_f(bat_second_client,
			    bm_second_tx,
			    bm_second_rx,
			    user_msh, 0, MSUB_SIZE, 0, &user_msubh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	ret = accept_ms_f(bat_second_client,
			  bm_second_tx,
			  user_msh, user_msubh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);
	sleep(1);

	/* Now create client mso, ms, and msub */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0,
							&client_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, MSUB_SIZE,
							      0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Connect to user */
	msub_h	user_msubh_rb;
	uint32_t  user_msub_len_rb;
	ms_h	user_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &user_msubh_rb, &user_msub_len_rb,
			     &user_msh_rb,
			     0);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Do the DMA transfer and comparison */
	ret = do_dma(client_msubh,
		     user_msubh_rb,
		     0,
		     0,
		     rdma_sync_chk);
	BAT_EXPECT_RET(ret, 0, disconnect);

disconnect:
	/* Now disconnect from server */
	ret = rdma_disc_ms_h(user_msh_rb, client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

free_user_mso:
	ret = close_mso_f(bat_second_client,
		          bm_second_tx,
		          bm_second_rx,
		          user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_second_bat_client:
	delete bat_second_client;

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
#endif
	return ret;
} /* test_case_6() */
