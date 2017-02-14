/*
 ************************************************************************ 
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
l of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this l of conditions and the following disclaimer in the documentation
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
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <stdarg.h>
#include <setjmp.h>
#include "cmocka.h"

#include "RapidIO_Source_Config.h"
#include "RapidIO_Device_Access_Routines_API.h"
#include "RXS2448.h"
#include "RXS_Routing_Table_API.h"
#include "src/RXS_DeviceDriver.c"
#include "src/RXS_RT.c"
#include "rio_ecosystem.h"
#include "tok_parse.h"
#include "libcli.h"
#include "rapidio_mport_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RXSx_DAR_WANTED

static void rxs_not_supported_test(void **state)
{
	(void)state; // not used
}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++;// not used

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(rxs_not_supported_test)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

#endif /* RXSx_DAR_WANTED */

#ifdef RXSx_DAR_WANTED

#define DEBUG_PRINTF 0

typedef struct RXS_test_state_t_TAG {
	int argc;
	char **argv;
	bool real_hw;
	uint32_t mport;
	uint8_t hc;
	uint32_t destid;
	struct rapidio_mport_handle *mp_h;
	bool mp_h_valid;
	uint32_t conn_port;
} RXS_test_state_t;

RXS_test_state_t st;

static int grp_setup(void **state)
{
	*state = (void *)&st;
	char *token_list = (char *)"-m -h -d ";
	char *tok, *parm;
	int tok_idx = 1;
	bool got_mport = false;
	bool got_hc = false;
	bool got_destid = false;

	while (tok_idx < st.argc) {
		tok = st.argv[tok_idx];
		tok_idx++;
		if (!(tok_idx < st.argc)) {
			printf("\nMissing option value.\n");
			goto fail;
		}
		parm = st.argv[tok_idx++];
		switch (parm_idx(tok, token_list)) {
		case 0:
			if (tok_parse_mport_id(parm, &st.mport, 0)) {
				printf("\nFailed tok_parse_mport_id\n");
				goto fail;
			}
			st.real_hw = true;
			got_mport = true;
			if (!got_hc) {
				st.hc = 0xFF;
			}
			break;
		case 1:
			if (tok_parse_hc(parm, &st.hc, 0)) {
				printf("\nFailed tok_parse_hc\n");
				goto fail;
			}
			st.real_hw = true;
			got_hc = true;
			break;
			break;
		case 2:
			if (tok_parse_did(parm, &st.destid, 0)) {
				printf("\nFailed tok_parse_did\n");
				goto fail;
			}
			st.real_hw = true;
			got_destid = true;
			break;
		default:
			printf("\nUnknown option, options are -m -h -d.\n");
			goto fail;
			break;
		}
	}

	if ((st.real_hw) && !(got_mport && got_hc && got_destid)) {
		printf("\nMust enter all of -m, -h and -d to run on real hw\n");
		goto fail;
	}

	if (st.real_hw) {
		if (riomp_mgmt_mport_create_handle(st.mport, 0, &st.mp_h)) {
			printf("\nCould not open mport %d\n", st.mport);
			goto fail;
		}
		st.mp_h_valid = true;
	}

	return 0;
fail:
	return -1;
}

static int grp_teardown(void **state)
{
	if (st.real_hw) {
		if (st.mp_h_valid) {
			riomp_mgmt_mport_destroy_handle(&st.mp_h);
		}
	}

	return 0;
	(void)state;
}

uint32_t rxs_mock_reg_oset[] = {
	RXS_RIO_PRESCALAR_SRV_CLK,
	RXS_RIO_MPM_CFGSIG0
};

#define NUM_DAR_REG (sizeof(rxs_mock_reg_oset)/sizeof(rxs_mock_reg_oset[0]))
#define UPB_DAR_REG (NUM_DAR_REG+1)
#define MOCK_REG_ADDR(x) (x | 1)

static rio_perf_opt_reg_t mock_dar_reg[UPB_DAR_REG];
static DAR_DEV_INFO_t mock_dev_info;

/* Create a mock dev_info.
 */
static void rxs_test_setup(void)
{
	uint8_t idx;

	mock_dev_info.privateData = 0x0;
	mock_dev_info.accessInfo = 0x0;
	strcpy(mock_dev_info.name, "RXS2448");
	mock_dev_info.dsf_h = 0x00380000;
	mock_dev_info.extFPtrForPort = 0;
	mock_dev_info.extFPtrPortType = 0;
	mock_dev_info.extFPtrForLane = 12288;
	mock_dev_info.extFPtrForErr = 0;
	mock_dev_info.extFPtrForVC = 0;
	mock_dev_info.extFPtrForVOQ = 0;
	mock_dev_info.devID = 0x80E60038;
	mock_dev_info.driver_family = RIO_RXS_DEVICE;
	mock_dev_info.devInfo = 0;
	mock_dev_info.assyInfo = 256;
	mock_dev_info.features = 402658623;
	mock_dev_info.swPortInfo = 6146;
	mock_dev_info.swRtInfo = 255;
	mock_dev_info.srcOps = 4;
	mock_dev_info.dstOps = 0;
	mock_dev_info.swMcastInfo = RXS2448_MAX_MC_MASK;

	for (idx = 0; idx < RIO_MAX_PORTS; idx++) {
		mock_dev_info.ctl1_reg[idx] = 0;
	}

	for (idx = 0; idx < MAX_DAR_SCRPAD_IDX; idx++) {
		mock_dev_info.scratchpad[idx] = 0;
	}
}

// The behavior of the performance optimization register access
// must be overridden when registers are mocked.
//
// The elegant way to do this, while retaining transparency in the code,
// is to add 1 to the defined register offset to compute the mocked register
// offset..  This prevents the standard performance optimization register
// support from recognizing the register and updating the poregs array.

static uint32_t rxs_get_poreg_idx(DAR_DEV_INFO_t *dev_info, uint32_t offset)
{
        return DAR_get_poreg_idx(dev_info, MOCK_REG_ADDR(offset));
}

static uint32_t rxs_add_poreg(DAR_DEV_INFO_t *dev_info, uint32_t offset,
                uint32_t data)
{
        return DAR_add_poreg(dev_info, MOCK_REG_ADDR(offset), data);
}

static uint32_t RXSReadReg(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t *readdata)
{
	uint32_t rc = 0xFFFFFFFF;
	uint32_t idx = rxs_get_poreg_idx(dev_info, MOCK_REG_ADDR(offset));

	if (NULL == dev_info) {
		return rc;
	}

	if (!st.real_hw) {
		if ((DAR_POREG_BAD_IDX == idx) && DEBUG_PRINTF) {
			printf("\nMissing offset is 0x%x\n", offset);
			assert_true(st.real_hw);
			idx = 0;
		} else {
			rc = RIO_SUCCESS;
		}
		*readdata = mock_dar_reg[idx].data;
		goto exit;
	}

	// Should only get here when st.real_hw is true, since
	// when real_hw is false, all registers are mocked.
	assert_true(st.real_hw);
	assert_true(st.mp_h_valid);

	if (0xFF == st.hc) {
		rc = riomp_mgmt_lcfg_read(st.mp_h, offset, 4, readdata);
	} else {
		rc = riomp_mgmt_rcfg_read(st.mp_h, st.destid, st.hc,
							offset, 4, readdata);
	}
exit:
	return rc;
}

static void rxs_emulate_reg_write(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t writedata)
{
	uint32_t idx = rxs_get_poreg_idx(dev_info, MOCK_REG_ADDR(offset));
	
	if ((DAR_POREG_BAD_IDX == idx) && DEBUG_PRINTF) {
		printf("\nMissing offset is 0x%x\n", offset);
		assert_int_equal(0xFFFFFFFF, offset);
		assert_true(st.real_hw);
		return;
	}

	mock_dar_reg[idx].data = writedata;
}

static uint32_t RXSWriteReg(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t writedata)
{
	uint32_t rc = 0xFFFFFFFF;

	if (NULL == dev_info) {
		return rc;
	}

	if (!st.real_hw) {
		rxs_emulate_reg_write(dev_info, offset, writedata);
		return RIO_SUCCESS;
	}
	assert_true(st.mp_h_valid);

	if (0xFF == st.hc) {
		rc = riomp_mgmt_lcfg_write(st.mp_h, offset, 4, writedata);
	} else {
		rc = riomp_mgmt_rcfg_write(st.mp_h, st.destid, st.hc, offset, 4, writedata);
	}

	return rc;
}

static void RXSWaitSec(uint32_t delay_nsec, uint32_t delay_sec)
{
	if (st.real_hw) {
		uint64_t counter = delay_nsec + ((uint64_t)delay_sec * 1000000000);
		for ( ; counter; counter--);
	}
}

/* Initialize the mock register structure for different registers.
 */
static void init_mock_rxs_reg(void **state)
{
	uint32_t i;
	RXS_test_state_t *l_st = *(RXS_test_state_t **)state;

	DAR_proc_ptr_init(RXSReadReg, RXSWriteReg, RXSWaitSec);
	if (l_st->real_hw) {
		mock_dev_info.poregs_max = 0;
		mock_dev_info.poreg_cnt = 0;
		mock_dev_info.poregs = NULL;
		return;
	}

	mock_dev_info.poregs_max = UPB_DAR_REG;
	mock_dev_info.poreg_cnt = 0;
	mock_dev_info.poregs = mock_dar_reg;

	// Initialize RXS regs
	for (i = 0; i < NUM_DAR_REG; i++) {
		assert_int_equal(RIO_SUCCESS,
			rxs_add_poreg(&mock_dev_info, rxs_mock_reg_oset[i], 0));
	}
}

/* The setup function which should be called before any unit tests that need to be executed.
 */
static int setup(void **state)
{
	uint32_t sw_port_info;
	uint32_t rc;
	RXS_test_state_t *RXS = *(RXS_test_state_t **)state;

	memset(&mock_dev_info, 0x00, sizeof(rio_sc_dev_ctrs_t));
	rxs_test_setup();
	init_mock_rxs_reg(state);
	if (RXS->real_hw) {
		rc = RXSReadReg(&mock_dev_info, RIO_SW_PORT_INF, &sw_port_info);
		assert_int_equal(rc, 0);
		RXS->conn_port = sw_port_info & RIO_SW_PORT_INF_PORT;
	}

	return 0;
}

/* The teardown function to be called after any tests have finished.
 */
static int teardown(void **state) 
{
	(void)state; //unused
	return 0;
}

typedef struct clk_pd_tests_t_TAG {
	uint32_t ps; // Prescalar
	uint32_t cfgsig0; // Ref clock, clock config
	uint32_t clk_pd; // Expected clock period
} clk_pd_tests_t;

#define LO_LAT     RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_LO_LAT
#define LO_RSVD    RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_RSVD
#define LO_PWR_12G RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_12G
#define LO_PWR_10G RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_10G
#define MHZ_100    RXS_RIO_MPM_CFGSIG0_REFCLK_SELECT_100MHz
#define MHZ_156    RXS_RIO_MPM_CFGSIG0_REFCLK_SELECT_156p25MHz

static clk_pd_tests_t clk_pd_pass[] = {
	{ 42, LO_LAT | MHZ_100, 1001},
	{ 42, LO_LAT | MHZ_156, 1000},
	{ 38, LO_PWR_12G | MHZ_156, 998},
	{ 37, LO_PWR_12G | MHZ_100, 992},
	{ 31, LO_PWR_10G | MHZ_156, 992},
	{ 31, LO_PWR_10G | MHZ_100, 992}
};

static void rxs_rio_pc_clk_pd_success_test(void **state)
{
	RXS_test_state_t *l_st = *(RXS_test_state_t **)state;
	const int num_tests = sizeof(clk_pd_pass) / sizeof(clk_pd_pass[0]);
	uint32_t srv_pd;
	uint32_t i;

	// On real hardware, it is disasterous to mess with clocking
	// parameters, so just check that the current setup passes

	if (l_st->real_hw) {
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));

		return;
	}

	for (i = 0; i < num_tests; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni = %d\n", i);
		}
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_RIO_PRESCALAR_SRV_CLK,
			clk_pd_pass[i].ps));
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_RIO_MPM_CFGSIG0,
			clk_pd_pass[i].cfgsig0));
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));
		assert_int_equal(srv_pd, clk_pd_pass[i].clk_pd);
	}
}

static clk_pd_tests_t clk_pd_fail[] = {
	{ 0x1FF, 0, 0},
	{ 0, 0, 0},
	{ 42, LO_RSVD | MHZ_156, 0},
	{ 42, LO_RSVD | MHZ_100, 0},
	{ 41, LO_LAT | MHZ_100, 1000},
	{ 41, LO_LAT | MHZ_156, 1001},
	{ 37, LO_PWR_12G | MHZ_156, 998},
	{ 36, LO_PWR_12G | MHZ_100, 992},
	{ 30, LO_PWR_10G | MHZ_156, 992},
	{ 32, LO_PWR_10G | MHZ_100, 992}
};

static void rxs_rio_pc_clk_pd_fail_test(void **state)
{
	RXS_test_state_t *l_st = *(RXS_test_state_t **)state;
	const int num_tests = sizeof(clk_pd_fail) / sizeof(clk_pd_fail[0]);
	uint32_t srv_pd;
	uint32_t i;

	// On real hardware, it is disasterous to mess with clocking
	// parameters, so just check that the current setup passes

	if (l_st->real_hw) {
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));

		return;
	}

	for (i = 0; i < num_tests; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni = %d\n", i);
		}
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_RIO_PRESCALAR_SRV_CLK,
			clk_pd_fail[i].ps));
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_RIO_MPM_CFGSIG0,
			clk_pd_fail[i].cfgsig0));
		assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));
		assert_int_equal(srv_pd, 0);
	}
}

int main(int argc, char** argv)
{
	const struct CMUnitTest tests[] = {
			cmocka_unit_test_setup_teardown(
					rxs_rio_pc_clk_pd_success_test, setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_rio_pc_clk_pd_fail_test, setup,
					teardown), };

	memset(&st, 0, sizeof(st));
	st.argc = argc;
	st.argv = argv;

	return cmocka_run_group_tests(tests, grp_setup, grp_teardown);
}

#endif /* RXSx_DAR_WANTED */

#ifdef __cplusplus
}
#endif
