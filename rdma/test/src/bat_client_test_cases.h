#ifndef BAT_CLIENT_TEST_CASES_H
#define BAT_CLIENT_TEST_CASES_H

#include "bat_common.h"
#include "bat_client_private.h"

#define DMA_DATA_SIZE	64
#define DMA_DATA_SECTION_SIZE	8

#define MAX_NAME	80

#define MS1_SIZE	1024*1024 /* 1MB */
#define MS2_SIZE	512*1024  /* 512 KB */
#define MS1_FLAGS	0
#define MS2_FLAGS	0

extern uint8_t dma_data_copy[DMA_DATA_SIZE];
extern char loc_mso_name[MAX_NAME];
extern char loc_ms_name[MAX_NAME];
extern char rem_mso_name[MAX_NAME];
extern char rem_ms_name1[MAX_NAME];
extern char rem_ms_name2[MAX_NAME];
extern char rem_ms_name3[MAX_NAME];

int test_case_a(void);
int test_case_b(void);
int test_case_c(void);
int test_case_g(void);
int test_case_h_i(char ch, uint32_t destid);
int test_case_j_k(char ch, uint32_t destid);
int test_case_l();
int test_case_m();
int test_case_6();
#if 0
void prep_dma_data(uint8_t *dma_data);

void dump_data(uint8_t *dma_data, unsigned offset);

int do_dma(msub_h client_msubh,
		  msub_h server_msubh,
		  uint32_t ofs_in_loc_msub,
		  uint32_t ofs_in_rem_msub,
		  rdma_sync_type_t sync_type);
#endif
int test_case_dma(char tc,
		  uint32_t destid,
		  uint32_t loc_msub_ofs_in_ms,
		  uint32_t ofs_in_loc_msub,
		  uint32_t ofs_in_rem_msub,
		  rdma_sync_type_t sync_type);

#endif

