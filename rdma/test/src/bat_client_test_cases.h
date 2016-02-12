#ifndef BAT_CLIENT_TEST_CASES_H
#define BAT_CLIENT_TEST_CASES_H

#include "bat_common.h"
#include "bat_client_private.h"

/* New test cases */
int test_case_a();
int test_case_b();
int test_case_c();
int test_case_d();
int test_case_e();
int test_case_f();
int test_case_g();
int test_case_h(uint32_t destid);
int test_case_i_j_k(char tc, uint32_t destid);
int test_case_l();
int test_case_m(uint32_t destid);
int test_case_n();
int test_case_o(uint32_t destid);

/* Old test cases */
int test_case_t_u(char tc, uint32_t destid);
int test_case_v_w(char tc, uint32_t destid);
int test_case_x();
int test_case_y();

/* DMA test cases 1-6 */
int test_case_dma(char tc,
		  uint32_t destid,
		  uint32_t loc_msub_ofs_in_ms,
		  uint32_t ofs_in_loc_msub,
		  uint32_t ofs_in_rem_msub,
		  rdma_sync_type_t sync_type);
int test_case_7(uint32_t destid);

#endif

