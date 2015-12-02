/*
 * ****************************************************************************
 * Copyright (c) 2014, Integrated Device Technology Inc.
 * Copyright (c) 2014, RapidIO Trade Association
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *************************************************************************
 * */

#include <stdio.h>
#include <stdlib.h>
#include "libtime_utils.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_SUCCESS (char *)("test/master_success.cfg")
#define SLAVE_SUCCESS (char *)("test/slave_success.cfg")

/** @brief Test correct operation of ts_init
 * @return int, 0 for success, 1 for failure
 */
int test_case_1(void)
{
	struct seq_ts test_ts;

	if (init_seq_ts(&test_ts, 0))
		goto fail;

	if ((test_ts.max_idx != 0) || (test_ts.ts_idx != 0))
		goto fail;

	if (!init_seq_ts(&test_ts, -1))
		goto fail;

	if (init_seq_ts(&test_ts, MAX_TIMESTAMPS/2))
		goto fail;

	if ((test_ts.max_idx != MAX_TIMESTAMPS/2) || (test_ts.ts_idx != 0))
		goto fail;

	if (init_seq_ts(&test_ts, MAX_TIMESTAMPS))
		goto fail;

	if ((test_ts.max_idx != MAX_TIMESTAMPS) || (test_ts.ts_idx != 0))
		goto fail;

	if (!init_seq_ts(&test_ts, MAX_TIMESTAMPS+1))
		goto fail;

	return 0;
fail:
	return 1;
};

/** @brief Test correct operation of ts_now
 * @return int, 0 for success, 1 for failure
 */
int test_case_2(void)
{
	struct seq_ts ts;

	if (init_seq_ts(&ts, 3))
		goto fail;

	ts_now(&ts);
	if (ts.ts_idx != 1)
		goto fail;

	ts_now(&ts);
	if (ts.ts_idx != 2)
		goto fail;

	ts_now(&ts);
	if (ts.ts_idx != 3)
		goto fail;

	ts_now(&ts);
	if (ts.ts_idx != 3)
		goto fail;

	return 0;
fail:
	return 1;
};

/** @brief Test correct operation of ts_now_mark
 * @return int, 0 for success, 1 for failure
 */
int test_case_3(void)
{
	struct seq_ts ts;

	if (init_seq_ts(&ts, 3))
		goto fail;

	ts_now_mark(&ts, 123);
	if ((ts.ts_idx != 1) || (ts.ts_mkr[0] != 123))
		goto fail;

	ts_now_mark(&ts, 456);
	if ((ts.ts_idx != 2) || (ts.ts_mkr[1] != 456))
		goto fail;

	ts_now_mark(&ts, 789);
	if ((ts.ts_idx != 3) || (ts.ts_mkr[2] != 789))
		goto fail;

	ts_now_mark(&ts, 159);
	if ((ts.ts_idx != 3) || (ts.ts_mkr[2] != 789))
		goto fail;

	return 0;
fail:
	return 1;
};
	
/** @brief Test correct operation of time_difference
 * @return int, 0 for success, 1 for failure
 */
int test_case_4(void)
{
	struct timespec start_ts, end_ts, result_ts;
	
	start_ts.tv_sec = 1;
	start_ts.tv_nsec = 1;
	end_ts.tv_sec = 2;
	end_ts.tv_nsec = 2;

	result_ts = time_difference(start_ts, end_ts);
	if ((result_ts.tv_sec != 1) || (result_ts.tv_nsec != 1))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 500000000;
	end_ts.tv_sec = 2000;
	end_ts.tv_nsec = 499999999;

	result_ts = time_difference(start_ts, end_ts);
	if ((result_ts.tv_sec != 999) || (result_ts.tv_nsec != 999999999))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 999999999;
	end_ts.tv_sec = 2000;
	end_ts.tv_nsec = 999999999;

	result_ts = time_difference(start_ts, end_ts);
	if ((result_ts.tv_sec != 1000) || (result_ts.tv_nsec != 0))
		goto fail;

	start_ts.tv_sec = 0;
	start_ts.tv_nsec = 999999999;
	end_ts.tv_sec = 2000;
	end_ts.tv_nsec = 999999999;

	result_ts = time_difference(start_ts, end_ts);
	if ((result_ts.tv_sec != 2000) || (result_ts.tv_nsec != 0))
		goto fail;

	start_ts.tv_sec = 0;
	start_ts.tv_nsec = 0;
	end_ts.tv_sec = 0;
	end_ts.tv_nsec = 0;

	result_ts = time_difference(start_ts, end_ts);
	if ((result_ts.tv_sec != 0) || (result_ts.tv_nsec != 0))
		goto fail;

	return 0;
fail:
	return 1;
};
	
/** @brief Test correct operation of time_add
 * @return int, 0 for success, 1 for failure
 */
int test_case_5(void)
{
	struct timespec start_ts, end_ts, result_ts;
	
	start_ts.tv_sec = 1;
	start_ts.tv_nsec = 1;
	end_ts.tv_sec = 2;
	end_ts.tv_nsec = 2;

	result_ts = time_add(start_ts, end_ts);
	if ((result_ts.tv_sec != 3) || (result_ts.tv_nsec != 3))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 500000000;
	end_ts.tv_sec = 2000;
	end_ts.tv_nsec = 750000000;

	result_ts = time_add(start_ts, end_ts);
	if ((result_ts.tv_sec != 3001) || (result_ts.tv_nsec != 250000000))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 500000000;
	end_ts.tv_sec = 2000;
	end_ts.tv_nsec = 500000000;

	result_ts = time_add(start_ts, end_ts);
	if ((result_ts.tv_sec != 3001) || (result_ts.tv_nsec != 0))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 499999999;
	end_ts.tv_sec = 2000;
	end_ts.tv_nsec = 500000000;

	result_ts = time_add(start_ts, end_ts);
	if ((result_ts.tv_sec != 3000) || (result_ts.tv_nsec != 999999999))
		goto fail;

	return 0;
fail:
	return 1;
};
	
/** @brief Test correct operation of time_div
 * @return int, 0 for success, 1 for failure
 */
int test_case_6(void)
{
	struct timespec start_ts, result_ts;
	
	start_ts.tv_sec = 1;
	start_ts.tv_nsec = 1;

	result_ts = time_div(start_ts, 1);
	if ((result_ts.tv_sec != 1) || (result_ts.tv_nsec != 1))
		goto fail;

	start_ts.tv_sec = 10000;
	start_ts.tv_nsec = 500000000;

	result_ts = time_div(start_ts, 5000);
	if ((result_ts.tv_sec != 2) || (result_ts.tv_nsec != 100000))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 500000000;

	result_ts = time_div(start_ts, 5000);
	if ((result_ts.tv_sec != 0) || (result_ts.tv_nsec != 200100000))
		goto fail;

	start_ts.tv_sec = 1000;
	start_ts.tv_nsec = 499999999;

	result_ts = time_div(start_ts, 1000);
	if ((result_ts.tv_sec != 1) || (result_ts.tv_nsec != 499999))
		goto fail;

	return 0;
fail:
	return 1;
};
	
/** @brief Test correct operation of time_track
 * @return int, 0 for success, 1 for failure
 */
int test_case_7(void)
{
	struct timespec start_ts, end_ts, totaltime, mintime, maxtime;
	struct timespec test_total_time = {0, 0}, test_mintime, test_maxtime;
	struct timespec start_offset = {0, 4999};
	int i;
	int start_idx = 1, max_idx = 10;
	

	end_ts = start_offset;

	for (i = start_idx; i < max_idx; i++) {
		start_ts = end_ts;
		end_ts.tv_sec += i*i;
		end_ts.tv_nsec += i*i;
		
		test_total_time.tv_sec += i*i;
		test_total_time.tv_nsec += i*i;

		time_track(i-start_idx,
			start_ts, end_ts, &totaltime, &mintime, &maxtime);
	};
	
	test_mintime.tv_sec = start_idx * start_idx;
	test_mintime.tv_nsec = start_idx * start_idx;
	max_idx--;
	test_maxtime.tv_sec = max_idx * max_idx;
	test_maxtime.tv_nsec = max_idx * max_idx;
	
	if ((totaltime.tv_sec != test_total_time.tv_sec) ||
		(totaltime.tv_nsec != test_total_time.tv_nsec))
		goto fail;
	
	if ((mintime.tv_sec != test_mintime.tv_sec) ||
			(mintime.tv_nsec != test_mintime.tv_nsec))
		goto fail;

	if ((maxtime.tv_sec != test_maxtime.tv_sec) ||
			(maxtime.tv_nsec != test_maxtime.tv_nsec))
		goto fail;

	return 0;
fail:
	return 1;
};
	
int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;

	if (0)
		argv[0][0] = argc;

	rdma_log_init("time_utils_test.log", 1);

	g_level = 1;

	if (test_case_1()) {
		printf("\nTest_case_1 FAILED.");
		goto fail;
	};
	printf("\nTest_case_1 passed.");

	if (test_case_2()) {
		printf("\nTest_case_2 FAILED.");
		goto fail;
	};
	printf("\nTest_case_2 passed.");

	if (test_case_3()) {
		printf("\nTest_case_3 FAILED.");
		goto fail;
	};
	printf("\nTest_case_3 passed.");

	if (test_case_4()) {
		printf("\nTest_case_4 FAILED.");
		goto fail;
	};
	printf("\nTest_case_5 passed.");

	if (test_case_5()) {
		printf("\nTest_case_5 FAILED.");
		goto fail;
	};
	printf("\nTest_case_5 passed.");

	if (test_case_6()) {
		printf("\nTest_case_6 FAILED.");
		goto fail;
	};
	printf("\nTest_case_6 passed.");

	if (test_case_7()) {
		printf("\nTest_case_7 FAILED.");
		goto fail;
	};
	printf("\nTest_case_7 passed.");

	rc = EXIT_SUCCESS;
fail:
	printf("\n");
	if (rc != EXIT_SUCCESS)
		rdma_log_dump();
	printf("\n");
	rdma_log_close();
	exit(rc);
};

#ifdef __cplusplus
}
#endif
