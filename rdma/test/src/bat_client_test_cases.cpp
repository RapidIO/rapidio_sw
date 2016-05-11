#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <linux/limits.h>

#include <vector>
#include <sstream>
#include <algorithm>
#include <thread>
#include <random>

#include <cinttypes>
#include <cstdlib>
#include <cstdio>

#include "librdma.h"
#include "librdma_db.h"
#include "cm_sock.h"
#include "bat_common.h"
#include "bat_client_private.h"
#include "bat_client_test_cases.h"

using std::vector;
using std::stringstream;
using std::thread;
using std::move;
using std::min;

static constexpr uint16_t  BAT_MIN_BLOCK_SIZE = 4096;

/* ------------------------------- Test Cases -------------------------------*/

/**
 * Create a number of memory space owners. Some have duplicate names and are
 * expected to fail. Others should succeed including ones which are substrings
 * of existing memory space owner names.
 */
int test_case_a(void)
{
	constexpr auto NUM_MSOS = 12;

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

	mso_h	mso_handles[NUM_MSOS];

	int ret;
	bool failed = false;

	/* Create REMOTE memory space owners and check that they worked
	 * except for the duplicate ones; those should reutnr RDMA_DUPLICATE_MSO
	 */
	for (unsigned i = 0; i < NUM_MSOS; i++) {
		ret = create_mso_f(bat_connections[0],
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
			ret = destroy_mso_f(bat_connections[0],
					   mso_handles[i]);
			if (ret != 0) {
				fprintf(log_fp, "%s FAILED, line %d, ret=%d\n",
					__func__, __LINE__, ret);
				failed = true;
			}
		}
	}

	/* As always, make sure the bat_server has finished destroying the
	 * memory space owner, its memory spaces and subspaces before proceeding.
	 */
	sleep(1);

	/* Clear the handles since we will do a second test */
	memset(mso_handles, 0, sizeof(mso_handles));

	/* Second test, requested by Barry */
	constexpr auto NUM_UNIQUE_MSOS = 10;

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
	/* As always, make sure the bat_server has finished destroying the
	 * memory space owner, its memory spaces and subspaces before proceeding.
	 */
	sleep(1);

	if (!failed) {
		BAT_EXPECT_PASS(ret);
	}
	return ret;
} /* test_case_a() */

/**
 * Create a number of memory spaces. Some have duplicate names and are
 * expected to fail. Others should succeed including ones which are substrings
 * of existing memory space names.
 */
int test_case_b(void)
{
	constexpr unsigned NUM_MSS = 12;
	constexpr uint32_t MS_SIZE = 64*1024;		/* 64K */
	constexpr auto REM_MSO_NAME = "test_case_b_mso";
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
	ms_h		ms_handles[NUM_MSS];

	int ret;
	bool failed = false;

	/* Create mso */
	ret = create_mso_f(bat_connections[0], REM_MSO_NAME, &msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
						__func__, __LINE__, ret);
		return ret; /* No point testing anything else */
	}

	/* Create 12 memory spaces with some duplicate names */
	for (unsigned i = 0; i < NUM_MSS; i++) {
		uint32_t actual_size;

		ret = create_ms_f(bat_connections[0], ms_names[i], msoh, MS_SIZE, 0,
				  	  	  &ms_handles[i], &actual_size);

		/* Compare with expected return codes */
		if (ret != ret_codes[i]) { \
			fprintf(log_fp,
			"%s FAILED, line %d, i = %u, expected 0x%X, got 0x%X\n",
					__func__, __LINE__, i, ret_codes[i], ret);
			puts("Check RDMAD now....ENTER to continue....");
			getchar();
			failed = true;
		}
	}

	/* Destroy the memory spaces by destroying the mso */
	ret = destroy_mso_f(bat_connections[0], msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
						__func__, __LINE__, ret);
		failed = true;
	}

	/* As always, make sure the bat_server has finished destroying the
	 * memory space owner, its memory spaces and subspaces before proceeding.
	 */
	sleep(1);

	/* Second test, requested by Barry */

	/* Create mso */
	ret = create_mso_f(bat_connections[0], REM_MSO_NAME, &msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
						__func__, __LINE__, ret);
		return ret; /* No point testing anything else */
	}
	constexpr auto NUM_UNIQUE_MSS	= 10;
	static char ms_unique_names[][NUM_UNIQUE_MSS] = {
		"one", "two", "three", "four", "five",
		"six", "seven", "eight", "nine", "ten"};

	/* Create 12 memory spaces with some duplicate names */
	for (unsigned i = 0; i < NUM_UNIQUE_MSS; i++) {
		uint32_t actual_size;

		ret = create_ms_f(bat_connections[0],
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
	ret = create_ms_f(bat_connections[0],
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
	ret = create_ms_f(bat_connections[0],
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
	ret = create_ms_f(bat_connections[0],
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
	ret = destroy_mso_f(bat_connections[0],
			    msoh);
	if (ret != 0) {
		fprintf(log_fp, "%s FAILED, line %d, ret = %d\n",
				__func__, __LINE__, ret);
		failed = true;
	}

	/* As always, make sure the bat_server has finished destroying the
	 * memory space owner, its memory spaces and subspaces before proceeding.
	 */
	sleep(1);

	if (!failed) {
		BAT_EXPECT_PASS(ret);
	}

	return ret;
} /* test_case_b() */


/**
 * Tests memory space allocation as follows:
 *
 * 1. First try to allocate a memory space > the inbound window size.
 *    This is expected to fail.
 *
 * 2. Create a number of memory spaces in each inbound window always
 * starting with one that is win_size/2 and then win_size/4 and so on
 * until a memory space of 4K is reached. Allocate another 4K memory
 * space to complete the allocation for each window.
 *
 * 3. Free an 8K memory space in IBWIN0, then reallocate
 * an 8K memory space. It should end up in the free space in IBWIN0 not
 * in IBWIN1.
 *
 * Second test:
 *
 * 1. Free up the last 2 4K space, and allocate an 8K space
 * 2. Free up the last 2 8K spaces, and allocate a 16K space
 * 3. And so on and so forth until you have one space per IBWIN
 * that is as large as the IBWIN itself.
 */
int test_case_c(void)
{
	unsigned  num_ibwins;
	uint32_t  ibwin_size;
	int	  rc, ret;

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
	vector<ms_info_t>	ms_info;

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
		if (rc != 0) {
			printf("Failed to create '%s', ret = %d\n",
						ms_name.str().c_str(), rc);
			BAT_EXPECT_RET(rc, 0, free_mso);
		}
		rdma_get_msh_properties(ms_info[i].handle,
				&ms_info[i].rio_addr, &ms_info[i].size);
		printf("0x%016" PRIx64 ", 0x%016" PRIx64 ", 0x%X\n",
			ms_info[i].handle, ms_info[i].rio_addr, ms_info[i].size);
	}

	/**
	 * Test: Just ensure that freed memory spaces are re-used.
	 *
	 * 1. Find an 8KB space in WIN0 and free it.
	 * 2. Allocate a new 8K space.
	 * 3. Verify that the new space took the freed space and did not end
	 * up in WIN1. Do that by comparing the freed and new RIO addresses
	 */
	{
		/* Find an 8K memory space */
		auto it = find(begin(ms_info), end(ms_info), uint32_t(8*1024));

		/* Read its RIO address */
		uint64_t old_8k_rio_addr;
		uint32_t dummy_size;
		rc = rdma_get_msh_properties(it->handle, &old_8k_rio_addr, &dummy_size);
		printf("old_8k_rio_addr=0x%016" PRIx64 "\n", old_8k_rio_addr);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Now allocate another 8K memory space */
		rc = rdma_create_ms_h("new_8k_ms", client_msoh,
				 8*1024, 0, &it->handle, &dummy_size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Read back the RIO address */
		uint64_t new_8k_rio_addr;

		rc = rdma_get_msh_properties(it->handle, &new_8k_rio_addr, &dummy_size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		if (new_8k_rio_addr == old_8k_rio_addr) {
			rc = 0;
		} else {
			rc = -1;
		}
		BAT_EXPECT_RET(rc, 0, free_mso);
	}

	/**
	 * Test: Constantly free up memory spaces and allocate larger ones
	 * until you reach the point where you can allocate a memory space
	 * as large as the entire IBWIN.
	 */
	for (unsigned k = 0; k < num_ibwins; k++) {
		uint32_t size = BAT_MIN_BLOCK_SIZE;

		while (size < ibwin_size) {
			/* Find first memory space of size 'size' */
			auto it1 = find(begin(ms_info) + k*ms_info.size()/2,
					end(ms_info),
					size);
			if (it1 == end(ms_info)) {
				rc = -1;
				BAT_EXPECT_RET(rc, 0, free_mso);
			}

			/* Find second memory space of size 'size' */
			auto it2 = find(it1 + 1, end(ms_info), size);
			if (it2 == end(ms_info)) {
				rc = -1;
				BAT_EXPECT_RET(rc, 0, free_mso);
			}

			/* Destroy both memory spaces */
			rc = rdma_destroy_ms_h(client_msoh, it1->handle);
			BAT_EXPECT_RET(rc, 0, free_mso);

			rc = rdma_destroy_ms_h(client_msoh, it2->handle);
			BAT_EXPECT_RET(rc, 0, free_mso);

			/* Double the size */
			size *= 2;

			/* Now create a memory space that is twice the size.
			 * Store the handle in the space pointed to by 'it1'
			 */
			stringstream ms_name;
			ms_name << "ms" << k << size;
			rc = rdma_create_ms_h(ms_name.str().c_str(),
					      client_msoh,
					      size,
					      0,
					      &it1->handle,
					      &it1->size);
			BAT_EXPECT_RET(rc, 0, free_mso);
		}
	}

free_mso:
	ret = rc;

	/* Delete the mso */
	rc = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

exit:
	if (rc == 0) {
		BAT_EXPECT_PASS(ret);
	}

	return ret;
} /* test_case_c() */

/**
 * Create memory spaces everywhere.
 * Create 3 or fewer subspaces per memory space.
 * Select a memory subspace at random and free it.
 * Re-allocate the subspace and make sure it gets the same RIO address
 */
int test_case_d(void)
{
	unsigned  num_ibwins;
	uint32_t  ibwin_size;
	int	  rc, ret;

	/* Determine number of IBWINs and the size of each IBWIN */
	rc = rdma_get_ibwin_properties(&num_ibwins, &ibwin_size);
	BAT_EXPECT_RET(rc, 0, exit);
	printf("%u inbound windows, %uKB each\n", num_ibwins, ibwin_size/1024);

	/* Create a client mso */
	mso_h	client_msoh;
	rc = rdma_create_mso_h("test_case_d_mso", &client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* struct for holding information about memory spaces to be created */
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

	{
		 vector<ms_info_t>	ms_info;

		/* Now create a number of memory spaces such that they fill each
		 * inbound window with different sizes starting with 1/2 the
		 * inbound window size and ending with 4K which is the minimum
		 * block size that can be mapped.
		 *
		 * Then for each memory space, fill it with subspaces until
		 * the entire memory space is full.
		 */

		/* First generate the memory space sizes which fill up the entire
		 * inbound memory address space. */
		for (unsigned ibwin = 0; ibwin < num_ibwins; ibwin++) {
			auto size = ibwin_size/2;

			while (size >= BAT_MIN_BLOCK_SIZE) {
				ms_info.emplace_back(size, 0, 0);
				size /= 2;
			}
			ms_info.emplace_back(size * 2, 0, 0);
		}

		struct msub_info_t {
			msub_info_t(ms_h msh, msub_h msubh, uint32_t offset) :
				msh(msh), msubh(msubh), offset(offset) {
			}
			ms_h	msh;
			msub_h	msubh;
			uint32_t offset;
		};
		vector<msub_info_t>	msub_info;

		/* Now create the memory spaces, and subspaces that fill up
		 * the entire inbound space */
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

			/* For the current memory space, create 3 subspaces
			 * or fewer if the memory space cannot hold 3 subspaces. */
			size_t max_msubs = ms_info[i].size / BAT_MIN_BLOCK_SIZE;
			max_msubs = min<size_t>(max_msubs, 3);
			for (unsigned j = 0; j < max_msubs; j++) {
				msub_h	msubh;
				uint32_t offset = j * BAT_MIN_BLOCK_SIZE;
				rc = rdma_create_msub_h(ms_info[i].handle,
							 offset,
							 BAT_MIN_BLOCK_SIZE,
							 0,
							 &msubh);
				BAT_EXPECT_RET(rc, 0, free_mso);
				msub_info.emplace_back(ms_info[i].handle, msubh, offset);
			}
		}

		/* Pick 2 random memory sub-spaces */
		unsigned msub1_index = (msub_info.size() / 2) -3;
		unsigned msub2_index = msub_info.size() - 3;

		loc_msub *msub1 = (loc_msub *)msub_info[msub1_index].msubh;
		loc_msub *msub2 = (loc_msub *)msub_info[msub2_index].msubh;

		/* Remember their offsets within the memory space */
		uint32_t old_msub1_offset = msub_info[msub1_index].offset;
		uint32_t old_msub2_offset = msub_info[msub2_index].offset;

		/* Remember their old RIO addresses as well */
		uint64_t old_rio1 = msub1->rio_addr_lo;
		uint64_t old_rio2 = msub2->rio_addr_lo;

		/* Destroy the 2 memory subspaces */
		rc = rdma_destroy_msub_h(msub_info[msub1_index].msh,
					 msub_info[msub1_index].msubh);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_destroy_msub_h(msub_info[msub2_index].msh,
					 msub_info[msub2_index].msubh);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Create two similar msubspaces */
		rc = rdma_create_msub_h(msub_info[msub1_index].msh,
					old_msub1_offset,
					BAT_MIN_BLOCK_SIZE,
					0,
					&msub_info[msub1_index].msubh);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_msub_h(msub_info[msub2_index].msh,
					old_msub2_offset,
					BAT_MIN_BLOCK_SIZE,
					0,
					&msub_info[msub2_index].msubh);
		BAT_EXPECT_RET(rc, 0, free_mso);

		msub1 = (loc_msub *)msub_info[msub1_index].msubh;
		msub2 = (loc_msub *)msub_info[msub2_index].msubh;

		uint64_t new_rio1 = msub1->rio_addr_lo;
		uint64_t new_rio2 = msub2->rio_addr_lo;

		/* Verify that the RIO addresses are the same */
		if (new_rio1 != old_rio1) {
			rc = -1;
			BAT_EXPECT_RET(rc, 0, free_mso);
		}
		if (new_rio2 != old_rio2) {
			rc = -1;
			BAT_EXPECT_RET(rc, 0, free_mso);
		}
	}

free_mso:
	/* Save 'rc' since it will be overwritten while destroying mso */
	ret = rc;

	/* Delete the mso */
	rc = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

exit:
	if (rc == 0) {
		BAT_EXPECT_PASS(ret);
	}
	return ret;
} /* test_case_d() */

/**
 * Test mapping and writing values to neighboring subspaces.
 *
 * Start with mapping memory spaces as in test case d, then
 * fill the memory spaces with subspaces. Map and write values
 * then readback and verify.
 */
int test_case_e()
{
	unsigned  num_ibwins;
	uint32_t  ibwin_size;
	int	  rc, ret;

	/* Determine number of IBWINs and the size of each IBWIN */
	rc = rdma_get_ibwin_properties(&num_ibwins, &ibwin_size);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Create a client mso */
	mso_h	client_msoh;
	rc = rdma_create_mso_h("test_case_e_mso", &client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* struct for holding information about memory spaces to be created */
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

	{
		vector<ms_info_t>	ms_info;

		for (unsigned ibwin = 0; ibwin < num_ibwins; ibwin++) {
			auto size = ibwin_size/2;

			while (size >= BAT_MIN_BLOCK_SIZE) {
				ms_info.emplace_back(size, 0, 0);
				size /= 2;
			}
			ms_info.emplace_back(size * 2, 0, 0);
		}

		struct msub_info_t {
			msub_info_t(unsigned index, ms_h msh, msub_h msubh,
				    uint32_t offset, uint8_t *p) :
				index(index), msh(msh), msubh(msubh), offset(offset), p(p) {
			}
			unsigned index;	/* Index of msub within ms */
			ms_h	msh;
			msub_h	msubh;
			uint32_t offset;
			uint8_t	*p;
		};
		vector<msub_info_t>	msub_info;

		/* Now create the memory spaces, and subspaces that fill up
		 * the entire inbound space */
		for (unsigned i = 0; i < ms_info.size(); i++) {
			stringstream ms_name;
			ms_name << "mspace" << i;

			/* Create the memory space */
			rc = rdma_create_ms_h(ms_name.str().c_str(),
					      client_msoh,
					      ms_info[i].size,
					      0,
					      &ms_info[i].handle,
					      &ms_info[i].size);
			BAT_EXPECT_RET(rc, 0, free_mso);

			/* Obtain memory space properties */
			rdma_get_msh_properties(ms_info[i].handle,
					&ms_info[i].rio_addr, &ms_info[i].size);

			/* For the current memory space, fill it entirely
			 * with subspaces. Map each msub to virtual memory. */
			unsigned max_msubs = ms_info[i].size / BAT_MIN_BLOCK_SIZE;
			for (unsigned j = 0; j < max_msubs; j++) {
				msub_h	 msubh;
				uint8_t	 *p;
				uint32_t offset = j*BAT_MIN_BLOCK_SIZE;

				/* Create the adjacent subspaces */
				rc = rdma_create_msub_h(ms_info[i].handle,
							 offset,
							 BAT_MIN_BLOCK_SIZE,
							 0,
							 &msubh);
				BAT_EXPECT_RET(rc, 0, free_mso);

				/* Map the subspace */
				rc = rdma_mmap_msub(msubh, (void **)&p);
				BAT_EXPECT_RET(rc, 0, free_mso);

				/* Fill it with values. The easiest is to use 'j'
				 * so that every msub has different values */
				memset(p, j & 0xFF, BAT_MIN_BLOCK_SIZE);

				/* Store info about subspace */
				msub_info.emplace_back(j & 0xFF,
						       ms_info[i].handle,
						       msubh, offset, p);
			}
		}

		/* Verify data in all subspaces! */
		size_t	msub_info_size = msub_info.size();
		printf("msub_info_size = %u\n", (unsigned)msub_info_size);
		for(unsigned j = 0; j < msub_info_size; j++) {
			uint8_t *p = msub_info[j].p;
			unsigned index = msub_info[j].index;

			printf("index = %u, p[0] = %u, p[BAT_MIN_BLOCK_SIZE-1] = %u\n",
					index, p[0], p[BAT_MIN_BLOCK_SIZE-1]);

			if ((p[0] != index) || (p[BAT_MIN_BLOCK_SIZE-1] != index)) {
				rc = -1;
				BAT_EXPECT_RET(rc, 0, free_mso);
			}
		}

		/* Must remember to UNMAP all msubs */
		for(unsigned j = 0; j < msub_info_size; j++) {
			rc = rdma_munmap_msub(msub_info[j].msubh, msub_info[j].p);
			if (rc) {
				fprintf(log_fp, "%s FAILED, line %d, rc = %d, j = %u\n",
						__func__, __LINE__, rc, j);
				break;
			}
		}
	}

free_mso:
	/* Save 'rc' since it will be overwritten while destroying mso */
	ret = rc;
	/* Delete the mso */
	rc = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

exit:
	if (rc == 0) {
		BAT_EXPECT_PASS(ret);
	}
	return ret;
} /* test_case_e() */

/**
 * Create a number of msubs, some overlapping. Map and write data in the
 * overlapping areas. Read back and verify data at the boundaries of the
 * msubs.
 */
int test_case_f()
{
	int	rc, ret;
	mso_h	msoh;

	/* Create a client mso */
	rc = rdma_create_mso_h("test_case_f_mso", &msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	{
		ms_h	 msh;
		uint32_t actual_size;

		/* Create a 1MB memory space */
		rc = rdma_create_ms_h("test_case_f_ms",
				      msoh,
				      1024*1024,	// 1MB
				      0,
				      &msh,
				      &actual_size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Try to create an msub at an invalid offset and observe
		 * the error code. */
		msub_h	msubh1, msubh2, msubh3;
		rc = rdma_create_msub_h(msh, 139, 4*1024, 0, &msubh1);
		BAT_EXPECT_RET(rc, RDMA_ALIGN_ERROR, free_mso);

		/* Try to create an msub with an odd size */
		rc = rdma_create_msub_h(msh, 4*1024, 237, 0, &msubh1);
		BAT_EXPECT_RET(rc, RDMA_ALIGN_ERROR, free_mso);

		/* Create 3 overlapping memory subspaces at offsets
		 * 512, 512+16, 512+32.
		 * subspace sizes are 32K, 32K, and 32K
		 * The second subspace ovelaps with 1/2 of the
		 * first subspace. The third subspace starts in the middle
		 * of the second subspace.
		 * The test starts with fillinw msub1 with 0xAA.
		 * When msub2 fills with 0xBB the second half of msub1 now has 0xAA
		 * The first have of msub3 has 0xBB until msub3 is filled with 0xCC
		 * at which point the second half of msub2 now has 0xCC. The macros below
		 * all boundary conditions for each subspace.
		 */
		rc = rdma_create_msub_h(msh, 512*1024, 		 32*1024, 0, &msubh1);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_msub_h(msh, 512*1024 + 16*1024, 32*1024, 0, &msubh2);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_msub_h(msh, 512*1024 + 32*1024, 32*1024, 0, &msubh3);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Map all 3 subspaces */
		uint8_t	*p1, *p2, *p3;

		rc = rdma_mmap_msub(msubh1, (void **)&p1);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_mmap_msub(msubh2, (void **)&p2);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_mmap_msub(msubh3, (void **)&p3);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Fill the subspace with values and check that those values
		 * are correct at the boundaries.
		 */
		memset(p1, 0xAA, 32*1024);
		BAT_EXPECT_RET(p1[0], 0xAA, free_mso);
		BAT_EXPECT_RET(p1[32*1024 - 1], 0xAA, free_mso);

		memset(p2, 0xBB, 32*1024);
		BAT_EXPECT_RET(p1[16*1024], 0xBB, free_mso);
		BAT_EXPECT_RET(p1[32*1024 - 1], 0xBB, free_mso);
		BAT_EXPECT_RET(p2[0], 0xBB, free_mso);
		BAT_EXPECT_RET(p2[32*1024 - 1], 0xBB, free_mso);

		memset(p3, 0xCC, 32*1024);
		BAT_EXPECT_RET(p2[16 * 1024], 0xCC, free_mso);
		BAT_EXPECT_RET(p2[32*1024 - 1], 0xCC, free_mso);
		BAT_EXPECT_RET(p3[0], 0xCC, free_mso);
		BAT_EXPECT_RET(p3[32*1024 - 1], 0xCC, free_mso);

		rc = rdma_munmap_msub(msubh1, p1);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_munmap_msub(msubh2, p2);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_munmap_msub(msubh3, p3);
		BAT_EXPECT_RET(rc, 0, free_mso);
	}

free_mso:
	ret = rc;

	/* Delete the mso */
	rc = rdma_destroy_mso_h(msoh);
	BAT_EXPECT_RET(rc, 0, exit);
exit:
	if (rc == 0) {
		BAT_EXPECT_PASS(ret);
	}
	return ret;
} /* test_case_f() */

/**
 * Test that the new allocation algorithm works for each inbound window.
 * A memory space is allocated in the *smallest* possible freespace that
 * can accomodate it, not the *first* as was before.
 *
 * Procedure:
 *
 * 1. Fill up the entire space with memory spaces in logarithmic sizes
 * as before .
 *
 * 2. Free up select memory spaces in IBWIN1 (which has to be tested with IBWIN0
 * full). Those can be sizes 64K, 16K, 4K and 4K (there are two of those).
 *
 * 3. Try to allocate memory spaces sized 4K, 4K, 16K, and 64K in that order.
 *
 * Any allocation other than the optimal one will lead to failure sinc there is
 * no other way to allocate all those memory spaces
 *
 * 4. Now Free up select memory spaces in IBWIN0 (e.g.128K, 32K, 4K, and 4K).
 *
 * 5. Try to allocate: 4K, 4K, 32K, and 128K.
 *
 * Any allocation other than the optimal one will lead to failure sinc there is
 * no other way to allocate all those memory spaces*
 */
int test_case_g(void)
{
	unsigned  num_ibwins;
	uint32_t  ibwin_size;
	int	  rc, ret;

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
	vector<ms_info_t>	ms_info;

	/* Determine number of IBWINs and the size of each IBWIN */
	rc = rdma_get_ibwin_properties(&num_ibwins, &ibwin_size);
	BAT_EXPECT_RET(rc, 0, exit);
	printf("%u inbound windows, %uKB each\n", num_ibwins, ibwin_size/1024);

	/* Create a client mso */
	mso_h	client_msoh;
	rc = rdma_create_mso_h("test_case_g_mso", &client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

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
		if (rc != 0) {
			printf("Failed to create '%s', ret = %d\n",
						ms_name.str().c_str(), rc);
			BAT_EXPECT_RET(rc, 0, free_mso);
		}
		rdma_get_msh_properties(ms_info[i].handle,
				&ms_info[i].rio_addr, &ms_info[i].size);
		printf("0x%016" PRIx64 ", 0x%016" PRIx64 ", 0x%X\n",
			ms_info[i].handle, ms_info[i].rio_addr, ms_info[i].size);
	}

	{
	/* Find memory spaces sized 64K, 16K, 4K and 4K in ibwin0, and free them */
		/* Find the 64K mspace */
		auto it = find(begin(ms_info), end(ms_info), 64*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Find the 16K mspace */
		it = find(begin(ms_info), end(ms_info), 16*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Find the 4K mspace */
		it = find(begin(ms_info), end(ms_info), 4*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Find the other 4K mspace */
		it = find((it+1), end(ms_info), 4*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* NOTE: Since the 2 4K spaces are contiguous, we should be able
		 * to allocate an 8K space (if the merging of free space actually
		 * works).
		 */
		/* Allocate 8K, then 16K, then 64K */
		ms_h msh;
		uint32_t size;

		rc = rdma_create_ms_h("dummy1", client_msoh,  8*1024, 0, &msh, &size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_ms_h("dummy3", client_msoh, 16*1024, 0, &msh, &size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_ms_h("dummy4", client_msoh, 64*1024, 0, &msh, &size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		ret = rc;

		/* Repeat test for IBWIN1 */
		/* Find the 128K mspace */
		it = find(begin(ms_info) + ms_info.size()/2, end(ms_info), 128*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Find the 32K mspace */
		it = find(begin(ms_info) + ms_info.size()/2, end(ms_info), 32*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Find the 4K mspace */
		it = find(begin(ms_info) + ms_info.size()/2, end(ms_info), 4*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* Find the other 4K mspace */
		it = find((it+1), end(ms_info), 4*1024);
		BAT_EXPECT(it != end(ms_info), free_mso);

		/* Destroy it */
		rc = rdma_destroy_ms_h(client_msoh, it->handle);
		BAT_EXPECT_RET(rc, 0, free_mso);

		/* NOTE: Since the 2 4K spaces are contiguous, we should be able
		 * to allocate an 8K space (if the merging of free space actually
		 * works).
		 */

		/* Now allocate 8K, 32K, and 128K, respectively */
		rc = rdma_create_ms_h("dummy5", client_msoh,  8*1024, 0, &msh, &size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_ms_h("dummy7", client_msoh, 32*1024, 0, &msh, &size);
		BAT_EXPECT_RET(rc, 0, free_mso);

		rc = rdma_create_ms_h("dummy8", client_msoh, 128*1024, 0, &msh, &size);
		BAT_EXPECT_RET(rc, 0, free_mso);
	}

free_mso:
	ret = rc;	/* rc is overwritten when the mso is destroyed */

	/* Delete the mso */
	rc = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);
exit:
	if (rc == 0) {
		BAT_EXPECT_PASS(ret);
	}

	return ret;
} /* test_case_g() */

/**
 * Test cases h involves creating many remote memory spaces and
 * repeatedly connecting to and disconnecting from them.
 */
int test_case_h(uint32_t destid)
{
	constexpr uint32_t MS_SIZE = 64*1024;
	constexpr uint32_t MSUB_SIZE = 8*1024;
	constexpr unsigned NUM_CONNECTIONS = 3;
	constexpr unsigned NUM_ITERATIONS = 3;

	mso_h	server_msoh;
	struct ms_info_t {
		ms_h	msh;
		msub_h	msubh;
		msub_h	  server_msubh_rb;
		uint32_t  server_msub_len_rb;
		ms_h	  server_msh_rb;
	};
	ms_info_t	ms_info[NUM_CONNECTIONS];

	int ret, rc;

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h("loc_mso", &client_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create a client ms of size 16K */
	ms_h	client_msh;
	ret = rdma_create_ms_h("loc_ms", client_msoh, 16*1024, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub of size 4K */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, 4*1024, 0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create server mso */
	ret = create_mso_f(bat_connections[0], "rem_mso", &server_msoh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create remote memory spaces and subspaces */
	for (unsigned c = 0; c < NUM_CONNECTIONS; c++) {
		stringstream ms_name;

		/* Create remote memory space */
		ms_name << "rem_ms" << c;
		ret = create_ms_f(bat_connections[0],
				  ms_name.str().c_str(),
				  server_msoh, MS_SIZE, 0,
				  &ms_info[c].msh, NULL);
		BAT_EXPECT_RET(ret, 0, free_server_mso);

		/* Create msub on remote memory space */
		ret = create_msub_f(bat_connections[0],
				ms_info[c].msh, 0,
				MSUB_SIZE, 0, &ms_info[c].msubh);
		BAT_EXPECT_RET(ret, 0, free_server_mso);
	}

	for (unsigned i = 0; i < NUM_ITERATIONS; i++) {
		/* Put all remote memory spaces in accept mode */
		for (unsigned c = 0; c < NUM_CONNECTIONS; c++) {
			/* Accept on remote ms on the server */
			ret = accept_ms_thread_f(bat_connections[0],
						ms_info[c].msh,
						ms_info[c].msubh);
			BAT_EXPECT_RET(ret, 0, free_server_mso);
			sleep(1);
		}

		/* Now all remote memory spaces are in 'accept' mode.
		 * Connect to all of them. */
		conn_h	connh;
		for (unsigned c = 0; c < NUM_CONNECTIONS; c++) {
			stringstream ms_name;

			/* Connect to remote memory space */
			ms_name << "rem_ms" << c;
			ret = rdma_conn_ms_h(16, destid, ms_name.str().c_str(),
					client_msubh,
					&connh,
					&ms_info[c].server_msubh_rb,
					&ms_info[c].server_msub_len_rb,
					&ms_info[c].server_msh_rb,
					30);
			rc = ret;
			BAT_EXPECT_RET(ret, 0, free_server_mso);
			sleep(1);
		}

		/* Disconnect from all memory spaces */
		for (unsigned c = 0; c < NUM_CONNECTIONS; c++) {
			ret = rdma_disc_ms_h(connh, ms_info[c].server_msh_rb,
					     client_msubh);
			rc = ret;
			BAT_EXPECT_RET(ret, 0, free_server_mso);
			sleep(1);
		}
	}

free_server_mso:
	rc = ret;	/* 'ret' is overwritten during destruction */

	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	if (ret == 0) {
		BAT_EXPECT_PASS(rc);
	}
	return rc;
} /* test_case_h() */

/**
 * Test cases i, j, and k involve creating 3 remote memory spaces
 * and connecting to them.
 *
 * Test case 'i' also disconnects from the 3 memory spaces.
 * Test case 'j' destroys the local 'mso' then the remote 'mso' but does
 * not disconnect first.
 * Test case 'k' destroys the remote 'mso' then the local 'mso' but does
 * not disconnect first.
 */
int test_case_i_j_k(char tc, uint32_t destid)
{
	int ret, rc;

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], "rem_mso", &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms1 of size 64K */
	ms_h	server_msh1;
	ret = create_ms_f(bat_connections[0], "rem_ms1", server_msoh, 64*1024, 0,
			  	  	  	  	  &server_msh1, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub1 of size 4K on server_msh1 */
	msub_h  server_msubh1;
	ret = create_msub_f(bat_connections[0], server_msh1, 0, 4*1024, 0,
							&server_msubh1);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create server ms2 of size 32K */
	ms_h	server_msh2;
	ret = create_ms_f(bat_connections[0], "rem_ms2", server_msoh, 32*1024, 0,
			  	  	  	  	&server_msh2, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub2 of size 8K on server_msh2 */
	msub_h  server_msubh2;
	ret = create_msub_f(bat_connections[0], server_msh2, 0, 8*1024, 0,
							&server_msubh2);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create server ms3 of size 16K */
	ms_h	server_msh3;
	ret = create_ms_f(bat_connections[0], "rem_ms3", server_msoh, 16*1024, 0,
			  	  	  	  	&server_msh3, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub3 of size 16K on server_msh2 */
	msub_h  server_msubh3;
	ret = create_msub_f(bat_connections[0], server_msh3, 0, 16*1024, 0,
							&server_msubh3);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h("loc_mso", &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms of size 16K */
	ms_h	client_msh;
	ret = rdma_create_ms_h("loc_ms", client_msoh, 16*1024, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub of size 4K */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, 4*1024, 0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/**
	 * Now accept/connect to the 3 memory spaces, one after the other.
	 */
	/* Accept on ms1 on the server */
	ret = accept_ms_thread_f(bat_connections[0], server_msh1, server_msubh1);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to ms1 on server */
	msub_h	  server_msubh1_rb;
	uint32_t  server_msub1_len_rb;
	ms_h	  server_msh1_rb;
	conn_h	  connh1;
	ret = rdma_conn_ms_h(16, destid, "rem_ms1", client_msubh, &connh1,
			&server_msubh1_rb, &server_msub1_len_rb,
			&server_msh1_rb, 30);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Accept on ms2 on the server */
	ret = accept_ms_thread_f(bat_connections[0], server_msh2, server_msubh2);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to ms2 on server */
	msub_h	  server_msubh2_rb;
	uint32_t  server_msub2_len_rb;
	ms_h	  server_msh2_rb;
	conn_h	  connh2;
	ret = rdma_conn_ms_h(16, destid, "rem_ms2", client_msubh,
				&connh2, &server_msubh2_rb,
				&server_msub2_len_rb, &server_msh2_rb, 30);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Accept on ms3 on the server */
	ret = accept_ms_thread_f(bat_connections[0], server_msh3, server_msubh3);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to ms3 on server */
	msub_h	  server_msubh3_rb;
	uint32_t  server_msub3_len_rb;
	ms_h	  server_msh3_rb;
	conn_h	  connh3;
	ret = rdma_conn_ms_h(16, destid, "rem_ms3", client_msubh,
				&connh3, &server_msubh3_rb,
				&server_msub3_len_rb, &server_msh3_rb, 30);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* We only disconnect if it is test case 'i' */
	if (tc == 'i') {
		/* Disconnect from ms1 on server */
		ret = rdma_disc_ms_h(connh1, server_msh1_rb, client_msubh);
		BAT_EXPECT_RET(ret, 0, free_client_mso);

		/* Disconnect from ms2 on server */
		ret = rdma_disc_ms_h(connh2, server_msh2_rb, client_msubh);
		BAT_EXPECT_RET(ret, 0, free_client_mso);

		/* Disconnect from ms3 on server */
		ret = rdma_disc_ms_h(connh3, server_msh3_rb, client_msubh);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	}


	/* In test case 'k' we delete the remote (server) mso first */
	if (tc == 'k') {
		/* Delete the server mso */
		ret = destroy_mso_f(bat_connections[0], server_msoh);
		if (ret)
			fprintf(log_fp, "Failed on line %d\n", __LINE__);

		/* Delete the client mso */
		ret = rdma_destroy_mso_h(client_msoh);
		BAT_EXPECT_RET(ret, 0, exit);

		goto exit;
	}

	/* Test case 'j' is just the fall through here and we delete
	 * the local mso before the remote mso.
	 */
free_client_mso:
	rc = ret;	/* 'ret' is overwritten during destruction */

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	if (ret == 0) {
		fprintf(stdout, "test_case %c %s\n",
				tc, (rc == 0) ? "PASSED" : "FAILED");
		fprintf(log_fp, "test_case %c %s\n",
				tc, (rc == 0) ? "PASSED" : "FAILED");
	}

	return rc;
} /* test_case_i_j_k() */

int test_case_l()
{
	unsigned  num_ibwins;
	uint32_t  ibwin_size;
	int	  rc, ret;

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
	vector<ms_info_t>	ms_info;

	/* Determine number of IBWINs and the size of each IBWIN */
	rc = rdma_get_ibwin_properties(&num_ibwins, &ibwin_size);
	BAT_EXPECT_RET(rc, 0, exit);
	printf("%u inbound windows, %uKB each\n", num_ibwins, ibwin_size/1024);

	/* Create a client mso */
	mso_h	client_msoh;
	rc = rdma_create_mso_h("test_case_l_mso", &client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	{
		/* Create 4K memory spaces that fill up both inbound windows */
		unsigned num_mspaces = (num_ibwins * ibwin_size) / BAT_MIN_BLOCK_SIZE;

		/* Create the ms info elements and pre-populate with 'size' */
		for (unsigned i = 0; i < num_mspaces; i++) {
			ms_info.emplace_back(BAT_MIN_BLOCK_SIZE, 0, 0);
		}

		vector<unsigned> ms_indexes;	/* Indexes for use in random deletion */
		for (unsigned i = 0; i < num_mspaces; i++) {
			stringstream ms_name;
			ms_name << "mspace" << i;
			rc = rdma_create_ms_h(ms_name.str().c_str(),
					client_msoh,
					ms_info[i].size,
					0,
					&ms_info[i].handle,
					&ms_info[i].size);
			BAT_EXPECT_RET(rc, 0, free_client_mso);
			ms_indexes.push_back(i);
		}

		/* Now randomly free memory spaces until all are freed */
		std::random_shuffle(begin(ms_indexes), end(ms_indexes));
		for (auto& ms_index : ms_indexes) {
			rc = rdma_destroy_ms_h(client_msoh, ms_info[ms_index].handle);
			BAT_EXPECT_RET(rc, 0, free_client_mso);
		}

		/* Allocate num_ibwins memory spaces each with ibwin_size */
		for (unsigned i = 0; i < num_ibwins; i++) {
			stringstream ms_name;
			ms_name << "mspace" << i;
			rc = rdma_create_ms_h(ms_name.str().c_str(),
					      client_msoh,
					      ibwin_size,
					      0,
					      &ms_info[i].handle,
					      &ms_info[i].size);
			BAT_EXPECT_RET(rc, 0, free_client_mso);
		}
	}

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	if (ret == 0) {
		BAT_EXPECT_PASS(rc);
	}
	return rc;
} /* test_case_l() */

/**
 * Part of test case 'm'
 */
static constexpr unsigned M_NUM_CONNECTIONS = 3;
static int m_accept_rc[M_NUM_CONNECTIONS];
static int m_connect_rc[M_NUM_CONNECTIONS];

/**
 * Thread function -- part of test_case_m.
 */
void m_accept_thread_f(unsigned i)
{
	constexpr uint32_t MS_SIZE   = 64 * 1024;
	constexpr uint32_t MSUB_SIZE =  4 * 1024;
	int		   rc;
	ms_h	server_msh;
	msub_h  server_msubh;
	stringstream ms_name;

	/* Create server mso */
	mso_h	server_msoh;
	rc = create_mso_f(bat_connections[i], "server_mso", &server_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Create server_msh in server_msoh */
	ms_name << "mspace" << i;
	rc = create_ms_f(bat_connections[i], ms_name.str().c_str(), server_msoh,
						MS_SIZE, 0, &server_msh, NULL);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Create server_msubh in server_msh */
	rc = create_msub_f(bat_connections[i], server_msh, 0, MSUB_SIZE, 0,
								&server_msubh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Put server_msh in accept mode */
	rc = accept_ms_thread_f(bat_connections[i], server_msh, server_msubh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Wait for disconnection before destroying the mso */
	sleep(3);

	/* Delete the server mso */
	rc = destroy_mso_f(bat_connections[i], server_msoh);
	BAT_EXPECT_RET(rc, 0, exit);
exit:
	m_accept_rc[i] = rc;
} /* m_accept_thread_f() */

/**
 * Thread function -- part of test_case_m.
 */
void m_connect_thread_f(uint32_t destid, mso_h client_msoh, unsigned i)
{
	constexpr uint32_t  MS_SIZE   = 64 * 1024;
	constexpr uint32_t  MSUB_SIZE =  4 * 1024;
	int rc;
	ms_h	client_msh;
	msub_h	client_msubh;
	msub_h	  server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	  server_msh_rb;
	conn_h	  connh;
	stringstream ms_name;

	ms_name << "mspace" << i;

	/* Create client_msh in client_msoh */
	rc = rdma_create_ms_h(ms_name.str().c_str(), client_msoh, MS_SIZE, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Create a client_msubh in client_msh */
	rc = rdma_create_msub_h(client_msh, 0, MSUB_SIZE, 0, &client_msubh);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Connect to server_msh */
	rc = rdma_conn_ms_h(16, destid, ms_name.str().c_str(), client_msubh,
				&connh, &server_msubh_rb, &server_msub_len_rb,
				&server_msh_rb, 30);
	BAT_EXPECT_RET(rc, 0, exit);

	/* Wait a seconds before disconnecting */
	sleep(1);

	/* Disconnect from ms1 on server */
	rc = rdma_disc_ms_h(connh, server_msh_rb, client_msubh);
	BAT_EXPECT_RET(rc, 0, exit);
exit:
	m_connect_rc[i] = rc;
} /* m_connect_thread_f() */

/**
 * Try to connect to multiple memory spaces from multiple threads
 * at the same time.
 */
int test_case_m(uint32_t destid)
{
	int rc, ret;

	/* Create a client mso */
	mso_h	client_msoh;
	rc = rdma_create_mso_h("client_mso", &client_msoh);
	BAT_EXPECT_RET(rc, 0, exit)
	{
		using threads_list =  vector<thread> ;
		threads_list 	accept_threads;

		/* Create threads for creating memory spaces and accepting */
		for (unsigned i = 0; i < M_NUM_CONNECTIONS; i++) {
			/* Create threads for handling accepts */
			auto m_thread = thread(&m_accept_thread_f, i);

			/* Store handle so we can join at the end of the test case,
			 * and store msoh so we can destroy the mso */
			accept_threads.push_back(move(m_thread));
			sleep(1);	/* Try to get them to accept in order */
		}

		/* Now create the 'connect' threads */
		threads_list connect_threads;

		/* Create threads for connecting to memory spaces */
		for (unsigned i = 0; i < M_NUM_CONNECTIONS; i++) {
			/* Create threads for sending connects */
			auto m_thread = thread(&m_connect_thread_f,
					       destid,
					       client_msoh,
					       i);

			/* Store handle so we can join at the end of the test case */
			connect_threads.push_back(move(m_thread));
		}

		/* Wait for threads to die */
		for (unsigned i = 0; i < M_NUM_CONNECTIONS; i++) {
			accept_threads[i].join();
			if (m_accept_rc[i] != 0)
				rc = m_accept_rc[i];
		}
		puts("Accept threads terminated.");

		/* Wait for threads to die */
		for (unsigned i = 0; i < M_NUM_CONNECTIONS; i++) {
			connect_threads[i].join();
			if (m_connect_rc[i] != 0)
				rc = m_connect_rc[i];
		}
		puts("Connect threads terminated.");
	}

	ret = rc;

	/* Delete the client mso */
	rc = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(rc, 0, exit);
	puts("Client mso destroyed with all its children");

exit:
	BAT_EXPECT_PASS(ret);

	return ret;
} /* test_case_m() */

/**
 * Create/open mso multiple times, close multiple times..etc.
 */
int test_case_n()
{
	int ret, rc;

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], "rem_mso", &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms of size 64K */
	ms_h	server_msh;
	ret = create_ms_f(bat_connections[0], "rem_ms", server_msoh, 64*1024, 0,
			  	  	  	  &server_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Open mso from user */
	mso_h	user_msoh;
	ret = open_mso_f(bat_connections[1], "rem_mso", &user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Open mso AGAIN. Should be harmless */
	ret = open_mso_f(bat_connections[1], "rem_mso", &user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Open ms from user */
	ms_h	user_msh;
	uint32_t user_msh_size;
	ret = open_ms_f(bat_connections[1], "rem_ms", user_msoh, 0,
						&user_msh_size, &user_msh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Close mso from user */
	ret = close_mso_f(bat_connections[1], user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Close mso from user AGAIN. Should be harmless */
	ret = close_mso_f(bat_connections[1], user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Open mso from ANOTHER user. Closing should not have destroyed msoh */
	mso_h	user2_msoh;
	ret = open_mso_f(bat_connections[2], "rem_mso", &user2_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Destroy the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Destroy the server mso AGAIN. Should be harmless */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Cannot open after destruction */
	ret = open_mso_f(bat_connections[1], "rem_mso", &user_msoh);
	if (ret == 0)
		fprintf(log_fp, "%s at line %d\n",
				(ret != 0) ? "PASSED" : "FAILED", __LINE__);
	ret = open_mso_f(bat_connections[2], "rem_mso", &user_msoh);
	if (ret == 0)
		fprintf(log_fp, "%s at line %d\n",
				(ret != 0) ? "PASSED" : "FAILED", __LINE__);

	/* Re-create mso after destruction */
	ret = create_mso_f(bat_connections[0], "rem_mso", &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Re-create ms after mso destruction */
	ret = create_ms_f(bat_connections[0], "rem_ms", server_msoh, 64*1024, 0,
			  	  	  	  &server_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Open mso from user again */
	ret = open_mso_f(bat_connections[1], "rem_mso", &user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Open ms from user again */
	ret = open_ms_f(bat_connections[1], "rem_ms", user_msoh, 0,
						&user_msh_size, &user_msh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub from user */
	msub_h	user_msubh;
	ret = create_msub_f(bat_connections[1], user_msh, 0, 8192, 0,
								&user_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	rc = ret;

	/* Delete the server mso while it is open */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	if (ret)
		rc = ret;
	fprintf(log_fp, "test_case_n %s\n", (rc == 0) ? "PASSED" : "FAILED");
	return rc;
} /* test_case_n() */

/**
 * Test server-side disconnection.
 */
int test_case_o(uint32_t destid)
{
	int ret, rc;

	/* Create server mso */
	mso_h	server_msoh_rb;
	ret = create_mso_f(bat_connections[0], "rem_mso", &server_msoh_rb);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms of size 64K */
	ms_h	server_msh_rb;
	ret = create_ms_f(bat_connections[0], "rem_ms", server_msoh_rb, 64*1024, 0,
			  	  	  	  &server_msh_rb, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub of size 4K on server */
	msub_h  server_msubh_rb;
	ret = create_msub_f(bat_connections[0], server_msh_rb, 0, 4*1024, 0,
								&server_msubh_rb);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h("loc_mso", &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms of size 16K */
	ms_h	client_msh;
	ret = rdma_create_ms_h("loc_ms", client_msoh, 16*1024, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_connections[0], server_msh_rb, server_msubh_rb);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Connect to server */
	msub_h	  server_msubh;
	uint32_t  server_msub_len;
	ms_h	  server_msh;
	conn_h	  connh;
	ret = rdma_conn_ms_h(16, destid, "rem_ms", 0,
				&connh, &server_msubh,
				&server_msub_len, &server_msh, 30);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Do a server-disconnect on the 'ms' */
	ret = server_disconnect_ms(bat_connections[0], server_msh_rb, 0);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	rc = ret;	/* 'ret' is overwritten during destruction */

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh_rb);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	if (ret == 0)
		fprintf(log_fp, "test_case 'o' %s\n",
					(rc == 0) ? "PASSED" : "FAILED");
	return rc;
} /* test_case_o() */

/**
 * Ensure that connecting to a non-existent or non-accepting 'ms' returns
 * RDMA_CONNECT_FAIL
 */
int test_case_p(uint32_t destid)
{
	int ret, rc;

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h("loc_mso", &client_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create a client ms of size 16K */
	ms_h	client_msh;
	ret = rdma_create_ms_h("loc_ms", client_msoh, 16*1024, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Attempt to connect to an ms that doesn't exist */
	msub_h	  server_msubh;
	uint32_t  server_msub_len;
	ms_h	  server_msh;
	conn_h	  connh;
	ret = rdma_conn_ms_h(16, destid, "rem_ms", 0,
				&connh, &server_msubh,
				&server_msub_len, &server_msh, 30);
	BAT_EXPECT_RET(ret, RDMA_CONNECT_FAIL, free_client_mso);

free_client_mso:
	rc = ret;

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	if (rc == RDMA_CONNECT_FAIL)
		fprintf(log_fp, "%s %s\n", __func__,
			(rc == RDMA_CONNECT_FAIL) ? "PASSED" : "FAILED");
	return ret;
} /* test_case_p() */

/**
 * This test case shall try to mimic what RSKT daemon and apps
 * do.
 */
int test_case_r(uint32_t destid)
{
	(void)destid;	// FIXME: Temporary

	int			rc;
	int			ret;
	constexpr auto 		NUM_MS = 8;
	constexpr unsigned 	MS_SIZE = 0x10000;
	constexpr unsigned	MSUB_SIZE = 0x1000;

	/* Client RSKTD */
	mso_h client_rsktd_msoh;
	ms_h client_rsktd_ms_handles[NUM_MS];
	msub_h client_rstkd_msubh;
	msub_h	  server_msubh;
	uint32_t  server_msub_len;
	ms_h	  server_msh;
	conn_h	  connh;

	/* Server RSKTD */
	mso_h server_rsktd_msoh_rb;
	ms_h server_rsktd_ms_handles_rb[NUM_MS];

	/* Server app */
	mso_h server_app_msoh_rb;
	ms_h server_app_msh_rb;
	uint32_t server_app_ms_size_rb;
	msub_h	server_app_msubh_rb;

	/* Initially just create the msos */
	rc = rdma_create_mso_h("CLIENT_MSO", &client_rsktd_msoh);
	BAT_EXPECT_RET(rc, 0, exit);

	rc = create_mso_f(bat_connections[0], "SERVER_MSO", &server_rsktd_msoh_rb);
	BAT_EXPECT_RET(rc, 0, free_client_mso);

	/* Each RSTKD creates 8 x 64K memory spaces. */
	for (auto i = 0; i < NUM_MS; i++) {
		/* Create client memory space */
		stringstream client_ms_name;
		client_ms_name << "CLIENT_MSPACE.00" << i;
		rc = rdma_create_ms_h(client_ms_name.str().c_str(),
				      client_rsktd_msoh,
				      MS_SIZE,
				      0,
				      &client_rsktd_ms_handles[i],
				      NULL);
		BAT_EXPECT_RET(rc, 0, free_server_mso);

		/* Create server memory space */
		stringstream server_ms_name;
		server_ms_name << "SERVER_MSPACE.00" << i;
		rc = create_ms_f(bat_connections[0],
				 server_ms_name.str().c_str(),
				 server_rsktd_msoh_rb,
				 MS_SIZE,
				 0,
				 &server_rsktd_ms_handles_rb[i],
				 NULL);
		BAT_EXPECT_RET(rc, 0, free_server_mso);
	}

	/* MIMIC THE ACTIONS OF rskt_accept() in librskt.c */
	rc = open_mso_f(bat_connections[1], "SERVER_MSO", &server_app_msoh_rb);
	BAT_EXPECT_RET(rc, 0, free_server_mso);
	rc = open_ms_f(bat_connections[1],
			"SERVER_MSPACE.007", /* Random 000 thru 007 are OK */
			server_app_msoh_rb,
			0,
			&server_app_ms_size_rb,
			&server_app_msh_rb);
	BAT_EXPECT_RET(rc, 0, free_server_mso);
	rc = create_msub_f(bat_connections[1],
			    server_app_msh_rb,
			    0,	/* offset */
			    MSUB_SIZE, /* size */
			    0,	/* flags */
			    &server_app_msubh_rb);
	BAT_EXPECT_RET(rc, 0, free_server_mso);
	rc = accept_ms_f(bat_connections[1], server_app_msh_rb, server_app_msubh_rb);
	BAT_EXPECT_RET(rc, 0, free_server_mso);

	sleep(2);

	/* Since there is only 1 bat_client application we will make an
	 * exception here and NOT open an mso/ms (it is not allowed to do so
	 * from the same application that created them). We'll pick an msh,
	 * then create an msubh, and then connect. */
	rc = rdma_create_msub_h(client_rsktd_ms_handles[6],
			0,
			MSUB_SIZE,
			0,
			&client_rstkd_msubh);
	BAT_EXPECT_RET(rc, 0, free_server_mso);
	rc = rdma_conn_ms_h(16,
			    destid,
			    "SERVER_MSPACE.007",
			    0,
			    &connh,
			    &server_msubh,
			    &server_msub_len,
			    &server_msh,
			    30);
	BAT_EXPECT_RET(rc, 0, free_server_mso);

	sleep(2);

	rc = rdma_disc_ms_h(connh, server_msh, 0);
	BAT_EXPECT_RET(rc, 0, free_server_mso);

free_server_mso:
	ret = rc;	/* Save before it gets overwritten */

	/* Delete the server mso */
	rc = destroy_mso_f(bat_connections[0], server_rsktd_msoh_rb);
	BAT_EXPECT_RET(rc, 0, free_client_mso);

free_client_mso:
	rc = rdma_destroy_mso_h(client_rsktd_msoh);
	BAT_EXPECT_RET(rc, 0, exit);
exit:
	if (rc == 0) {
		BAT_EXPECT_PASS(ret);
	}
	return ret;
} /* test_case_r() */

/**
 * Test accept_ms_h()/conn_ms_h()/disc_ms_h()..etc.
 *
 * @note: Since test cases 'i', 'j', 'k', and 'm' test connection/disconnection
 * test cases 't' and 'u' (which were the old test cases 'h' and 'i') will
 * use a 0 client msub handle to test that special case.
 *
 * @ch	if tc is 't' run test case 't', else run test case 'u'
 */
int test_case_t_u(char tc, uint32_t destid)
{
	int ret, rc;

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], "rem_mso", &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms of size 64K */
	ms_h	server_msh;
	ret = create_ms_f(bat_connections[0], "rem_ms", server_msoh, 64*1024, 0,
			  	  	  	  &server_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub of size 4K on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_connections[0], server_msh, 0, 4*1024, 0,
								&server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h("loc_mso", &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms of size 16K */
	ms_h	client_msh;
	ret = rdma_create_ms_h("loc_ms", client_msoh, 16*1024, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_thread_f(bat_connections[0], server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Connect to server */
	msub_h	  server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	  server_msh_rb;
	conn_h	  connh;
	ret = rdma_conn_ms_h(16, destid, "rem_ms", 0,
				&connh, &server_msubh_rb,
				&server_msub_len_rb, &server_msh_rb, 30);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Test case 't' disconnects first. Test case 'u' destroys
	 * the ms on the server and processes the incoming destroy message. */
	if (tc == 't') {
		/* Now disconnect from server */
		ret = rdma_disc_ms_h(connh, server_msh_rb, 0);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	}

free_client_mso:
	rc = ret;	/* 'ret' is overwritten during destruction */

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	if (ret == 0)
		fprintf(log_fp, "test_case %c %s\n",
					tc, (rc == 0) ? "PASSED" : "FAILED");

	return rc;
} /* test_case_t_u() */

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
 * tc:	'v' or 'w'
 * 'v'	Kill the remote app
 * 'w'	Kill the remote daemon
 */
int test_case_v_w(char tc, uint32_t destid)
{
	int ret, rc;
	constexpr auto REM_MSO_NAME = "test_case_v_w_mso_rem";
	constexpr auto REM_MS_NAME1 = "test_case_v_w_ms1_rem";
	constexpr auto LOC_MSO_NAME = "test_case_v_w_mso_loc";
	constexpr auto LOC_MS_NAME = "test_case_v_w_ms_loc";

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], REM_MSO_NAME, &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_connections[0],
			  REM_MS_NAME1,
			  server_msoh,
			  1024*1024,
			  0,
			  &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_connections[0], server_msh, 0, 4096, 0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(LOC_MSO_NAME, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(LOC_MS_NAME, client_msoh, 512*1024, 0,
							&client_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, 4096, 0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_connections[0], server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	conn_h	connh;
	ret = rdma_conn_ms_h(16, destid, REM_MS_NAME1,
			     client_msubh,
			     &connh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     30);	/* 30 second-timeout */
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	if (tc == 'w') {
		/* Kill remote app */
		puts("Telling remote app to die");
		ret = kill_remote_app(bat_connections[0]);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	} else if (tc == 'v') {
		/* Kill remote daemon */
		puts("Telling remote daemon to die");
		ret = kill_remote_daemon(bat_connections[0]);
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
	 * was not properly cleared then rdma_disc_ms_h() will fail
	 * at another stage.
	 */
	ret = rdma_disc_ms_h(connh, server_msh_rb, client_msubh);
	BAT_EXPECT_PASS(ret);

	rc = ret;

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_PASS(ret);

	/* We don't try to delete anything remote since we have either
	 * killed the remote daemon or the remote app and in both cases
	 * the remote entities (owner, mspace..etc.) are dead. */

	fprintf(log_fp, "test_case %c %s\n",
				tc, (rc == 0) ? "PASSED" : "FAILED");
	return rc;

free_client_mso:
	rc = ret;

	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	return rc;
} /* test_case_v_w() */

/**
 * The child creates an mso then dies. The return code of the child indicates
 * that the mso was successfully created. Then with the death of the child
 * the daemon auto-deletes the mso. The parent tries to open the m'so' and gets
 * an error indicating the mso doesn't exit.
 */
int test_case_x()
{
	pid_t child;
	int	ret;
	constexpr auto LOC_MSO_NAME = "test_case_x_mso_loc";

	LOG("test_case%c\t", 'x');

	child = fork();

	if (child == 0) { /* Child */
		if (execl("./bat_child", "bat_child", LOC_MSO_NAME, NULL) == -1)
			perror("test_case_l:");
	} else {	/* Parent */
		int status;
		pid_t dead_child;
		mso_h	opened_msoh;

		dead_child = wait(&status);
		if ((dead_child == child) && WEXITSTATUS(status) == 0) {
			ret = rdma_open_mso_h(LOC_MSO_NAME, &opened_msoh);
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
} /* test_case_x() */


/**
 * Kill then restart the daemon. Verify that memory space owners have
 * been cleaned up by creating the same mso with the same name again.
 */
int test_case_y()
{
	int ret;
	pid_t child;
	constexpr auto LOC_MSO_NAME = "test_case_y_mso_loc";

	LOG("test_case%c\t", 'z');

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(LOC_MSO_NAME, &client_msoh);
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
		ret = rdma_create_mso_h(LOC_MSO_NAME, &client_msoh);
		BAT_EXPECT_RET(ret, 0, exit);
		LOG("mso created. PASS\n");
		exit(0);
	}

free_client_mso:
	rdma_destroy_mso_h(client_msoh);

exit:
	return ret;
} /* test_case_y() */

static constexpr unsigned DMA_DATA_SIZE	 = 64;
static constexpr unsigned DMA_DATA_SECTION_SIZE =  8;

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

	/* Temporary to determine failure cause */
#if 0
	LOG("client_msubh = 0x%016" PRIx64 ", server_msubh = 0x%016" PRIx64 "\n",
			client_msubh, server_msubh);
	LOG("ofs_in_loc_msub = 0x%X, ofs_in_rem_msub = 0x%X, ",
			ofs_in_loc_msub, ofs_in_rem_msub);
	LOG("num_bytes = 0x%X, sync_type: %d\n", DMA_DATA_SIZE, sync_type);
#endif

	/* Push the RDMA data */
	ret = rdma_push_msub(&in, &out);
	BAT_EXPECT_RET(ret, 0, unmap_msubh);

	/* Handle ASYNC and FAF modes appropriately */
	if (sync_type == rdma_async_chk) {
		/* If async mode, must call rdma_sync_chk_push_pull() */
		LOG("ASYNC DMA: ");
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, unmap_msubh);
	} else if (sync_type == rdma_no_wait) {
		/* If FAF mode (no wait), then sleep to allow data to arrive */
		sleep(1);
	}

	/* Flush rdma_data */
	memset(dma_data, 0xDD, DMA_DATA_SIZE);

	/* Pull the DMA data back */
	ret = rdma_pull_msub(&in, &out);
	BAT_EXPECT_RET(ret, 0, unmap_msubh);

	/* Handle ASYNC and FAF modes appropriately */
	if (sync_type == rdma_async_chk) {
		/* If async mode, must call rdma_sync_chk_push_pull() */
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, unmap_msubh);
	} else if (sync_type == rdma_no_wait) {
		/* For FAF we poll for the data till it arrives */
		auto timeout = 10000;
		while (timeout--) {
			if ((dma_data[0x00] == 0x01) &&
			    (dma_data[0x08] == 0x09) &&
			    (dma_data[0x10] == 0x11))
				break;
		}
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
	constexpr auto REM_MSO_NAME = "test_case_dma_mso_rem";
	constexpr auto REM_MS_NAME1 = "test_case_dma_ms1_rem";
	constexpr auto LOC_MSO_NAME = "test_case_dma_mso_loc";
	constexpr auto LOC_MS_NAME = "test_case_dma_ms_loc";
	int ret;
	const unsigned MSUB_SIZE = 4096;

	LOG("test_case%c ", tc);

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], REM_MSO_NAME, &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_connections[0], REM_MS_NAME1, server_msoh, 1024*1024,
							0, &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_connections[0], server_msh, 0, MSUB_SIZE,
							0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(LOC_MSO_NAME, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(LOC_MS_NAME, client_msoh, 512*1024, 0, &client_msh,
									NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, loc_msub_ofs_in_ms, MSUB_SIZE,
							      0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_connections[0], server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	conn_h  connh;
	ret = rdma_conn_ms_h(16, destid, REM_MS_NAME1,
			     client_msubh,
			     &connh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     30);
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
	ret = rdma_disc_ms_h(connh, server_msh_rb, client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_dma() */

int do_dma_buf(void *buf,
	       int num_bytes,
	       msub_h server_msubh,
	       uint32_t ofs_in_server_msub,
	       int priority,
	       rdma_sync_type_t sync_type)
{
	int ret;
	uint8_t *dma_data = (uint8_t *)buf;

	/* Prep DMA data */
	prep_dma_data(dma_data);

	rdma_xfer_ms_out out;

	/* Push the RDMA data */
	ret = rdma_push_buf(buf, num_bytes, server_msubh, ofs_in_server_msub,
			priority, sync_type, &out);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Handle ASYNC and FAF modes appropriately */
	if (sync_type == rdma_async_chk) {
		/* If async mode, must call rdma_sync_chk_push_pull() */
		LOG("ASYNC DMA: ");
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, exit);
	} else if (sync_type == rdma_no_wait) {
		/* If FAF mode (no wait), then sleep to allow data to arrive */
		sleep(1);
	}

	/* Flush rdma_data */
	memset(dma_data, 0xDD, DMA_DATA_SIZE);

	/* Pull the DMA data back */
	ret = rdma_pull_buf(buf, num_bytes, server_msubh,
			ofs_in_server_msub, priority, sync_type, &out);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Handle ASYNC and FAF modes appropriately */
	if (sync_type == rdma_async_chk) {
		/* If async mode, must call rdma_sync_chk_push_pull() */
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, exit);
	} else if (sync_type == rdma_no_wait) {
		/* For FAF we poll for the data till it arrives */
		auto timeout = 10000;
		while (timeout--) {
			if ((dma_data[0x00] == 0x01) &&
			    (dma_data[0x08] == 0x09) &&
			    (dma_data[0x10] == 0x11))
				break;
		}
	}

	/* Dump the data out for debugging */
	dump_data(dma_data, 0);

	/* Now compare recieved data with the copy */
	ret = memcmp(dma_data, dma_data_copy, DMA_DATA_SIZE);
	BAT_EXPECT_PASS(ret);

exit:
	return ret;
} /* do_dma_buf() */

int test_case_dma_buf(char tc,
		      uint32_t destid,
		      uint32_t ofs_in_rem_msub,
		      rdma_sync_type_t sync_type)
{
	constexpr auto REM_MSO_NAME = "test_case_dma_mso_rem";
	constexpr auto REM_MS_NAME1 = "test_case_dma_ms1_rem";
	constexpr auto LOC_MSO_NAME = "test_case_dma_mso_loc";
	constexpr unsigned MSUB_SIZE = 4096;

	int ret;

	LOG("test_case%c ", tc);

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], REM_MSO_NAME, &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_connections[0], REM_MS_NAME1, server_msoh,
					1024*1024, 0, &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_connections[0], server_msh, 0, MSUB_SIZE,
							0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(LOC_MSO_NAME, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Buffer for the data */
	static uint8_t dma_data[MSUB_SIZE];

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_connections[0], server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	conn_h  connh;
	ret = rdma_conn_ms_h(16, destid, REM_MS_NAME1,
			     0,
			     &connh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     30);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Do the DMA transfer and comparison */
	ret = do_dma_buf(dma_data,
		     DMA_DATA_SIZE,
		     server_msubh_rb,
		     ofs_in_rem_msub,
		     0,
		     sync_type);
	BAT_EXPECT_RET(ret, 0, disconnect);

disconnect:
	/* Now disconnect from server */
	ret = rdma_disc_ms_h(connh, server_msh_rb, 0);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_dma_buf() */

/**
 * Create mso, ms on server
 * Open mso, ms, and create msub on user
 * Sync DMA transfer
 * Close and destroy
 */
int test_case_7(uint32_t destid)
{
	int ret = 0;

	constexpr unsigned MSUB_SIZE = 4096;
	constexpr auto LOC_MSO_NAME = "test_case_dma_mso_loc";
	constexpr auto LOC_MS_NAME = "test_case_dma_ms_loc";
	constexpr auto REM_MSO_NAME = "test_case_dma_mso_rem";
	constexpr auto REM_MS_NAME1 = "test_case_dma_ms1_rem";

	LOG("%s ", __func__);

	/* First create mso, and ms on server */
	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_connections[0], REM_MSO_NAME, &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_connections[0], REM_MS_NAME1, server_msoh,
					1024*1024, 0, &server_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* On the 'user' application, open the mso & ms */
	mso_h	user_msoh;
	ret = open_mso_f(bat_connections[1], REM_MSO_NAME, &user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	ms_h	user_msh;
	uint32_t user_msh_size;
	ret = open_ms_f(bat_connections[1], REM_MS_NAME1, user_msoh, 0,
						&user_msh_size, &user_msh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	/* On the 'user' create an msub, and then do accept on the ms */
	msub_h  user_msubh;
	ret = create_msub_f(bat_connections[1], user_msh, 0, MSUB_SIZE, 0, &user_msubh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	ret = accept_ms_thread_f(bat_connections[1], user_msh, user_msubh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);
	sleep(1);

	/* Now create client mso, ms, and msub */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(LOC_MSO_NAME, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(LOC_MS_NAME, client_msoh, 512*1024, 0,
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
	conn_h  user_connh;
	ret = rdma_conn_ms_h(16, destid, REM_MS_NAME1,
			     client_msubh,
			     &user_connh,
			     &user_msubh_rb, &user_msub_len_rb,
			     &user_msh_rb,
			     30);
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
	ret = rdma_disc_ms_h(user_connh, user_msh_rb, client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

free_user_mso:
	ret = close_mso_f(bat_connections[1], user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_connections[0], server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:

	return ret;
} /* test_case_7() */
