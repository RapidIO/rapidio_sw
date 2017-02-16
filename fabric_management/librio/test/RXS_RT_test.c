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
#include "rio_ecosystem.h"
#include "tok_parse.h"
#include "libcli.h"
#include "rapidio_mport_mgmt.h"

#include "src/RXS_RT.c"

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
#define STATIC static

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

STATIC int grp_setup(void **state)
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

STATIC int grp_teardown(void **state)
{
	if (st.real_hw) {
		if (st.mp_h_valid) {
			riomp_mgmt_mport_destroy_handle(&st.mp_h);
		}
	}

	return 0;
	(void)state;
}

STATIC DAR_DEV_INFO_t mock_dev_info;

STATIC uint32_t RXSReadReg(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t *readdata)
{
	uint32_t rc = 0xFFFFFFFF;

	if (NULL == dev_info) {
		return rc;
	}

	// Should only get here when st.real_hw is true, since
	// when real_hw is false, all registers are mocked.
	if (!st.real_hw && DEBUG_PRINTF) {
		printf("\nMissing offset is 0x%x\n", offset);
	}
	assert_true(st.real_hw);
	assert_true(st.mp_h_valid);

	if (0xFF == st.hc) {
		rc = riomp_mgmt_lcfg_read(st.mp_h, offset, 4, readdata);
	} else {
		rc = riomp_mgmt_rcfg_read(st.mp_h, st.destid, st.hc, offset, 4, readdata);
	}

	return rc;
}

STATIC void update_mc_masks(DAR_DEV_INFO_t *dev_info,
			rio_port_t port, uint32_t idx, uint32_t new_mask)
{
	uint32_t reg_idx;

	// Update register values for both the set and
	// clear mask.

	reg_idx = DAR_get_poreg_idx(dev_info,
				RXS_RIO_SPX_MC_Y_S_CSR(port, idx));
	if ((DAR_POREG_BAD_IDX == reg_idx) && DEBUG_PRINTF) {
		printf("\nMissing offset 0x%x port %d idx %d\n",
			RXS_RIO_SPX_MC_Y_S_CSR(port, idx), port, idx);
	}
	assert_int_not_equal(DAR_POREG_BAD_IDX, reg_idx);
	dev_info->poregs[reg_idx].data = new_mask;

	reg_idx = DAR_get_poreg_idx(dev_info,
					RXS_RIO_SPX_MC_Y_C_CSR(port, idx));
	if ((DAR_POREG_BAD_IDX == reg_idx) && DEBUG_PRINTF) {
		printf("\nMissing offset 0x%x port %d idx %d\n",
			RXS_RIO_SPX_MC_Y_C_CSR(port, idx), port, idx);
	}
	assert_int_not_equal(DAR_POREG_BAD_IDX, reg_idx);
	dev_info->poregs[reg_idx].data = new_mask;
}

/* The function tries to find the index of the offset in the dar_reg array and returns the idx,
 * otherwise it returns UPB_DAR_REG.
 */
STATIC void check_write_bc(DAR_DEV_INFO_t *dev_info,
			uint32_t offset, uint32_t writedata)
{
	uint32_t did, mask_idx, mask;
	rio_port_t port;

	if (st.real_hw) {
		return;
	}

	// Handle writes to broadcast set/clear multicast mask registers
	if ((offset >= RXS_RIO_BC_MC_X_S_CSR(0)) &&
		(offset < RXS_RIO_BC_MC_X_C_CSR(RXS2448_MC_MASK_CNT))) {
		uint32_t new_mask;
		bool do_clear = (offset & 4) ? true : false;

		mask_idx = (offset - RXS_RIO_BC_MC_X_S_CSR(0)) / 8;
		assert_in_range(mask_idx, 0, RXS2448_MAX_MC_MASK);

		for (port = 0; port < RXS2448_MAX_PORTS;  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
				RXS_RIO_SPX_MC_Y_S_CSR(port, mask_idx),
				&mask));
			if (do_clear) {
				new_mask = mask & ~writedata;
			} else {
				new_mask = mask | writedata;
			}
			update_mc_masks(dev_info, port, mask_idx, new_mask);
		}
		return;
	}

	// Handle writes to per-port set/clear multicast mask registers
	for (port = 0; port < RXS2448_MAX_PORTS;  port++) {
		if ((offset >= RXS_RIO_SPX_MC_Y_S_CSR(port, 0)) &&
		(offset <= RXS_RIO_SPX_MC_Y_C_CSR(port, RXS2448_MAX_MC_MASK))) {
			uint32_t new_mask;
			bool do_clear = (offset & 4) ? true : false;

			mask_idx = offset - RXS_RIO_SPX_MC_Y_S_CSR(port, 0);
			mask_idx /= 8;
			assert_in_range(mask_idx, 0, RXS2448_MAX_MC_MASK);

			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
				RXS_RIO_SPX_MC_Y_S_CSR(port, mask_idx),
				&mask));
			if (do_clear) {
				new_mask = mask & ~writedata;
			} else {
				new_mask = mask | writedata;
			}
			update_mc_masks(dev_info, port, mask_idx, new_mask);
			return;
		}
	}

	if ((offset >= RXS_RIO_BC_L2_GX_ENTRYY_CSR(0,0)) &&
		(offset <= RXS_RIO_BC_L2_GX_ENTRYY_CSR(0, RIO_RT_GRP_ENTRIES-1))) {
		did = (offset - RXS_RIO_BC_L2_GX_ENTRYY_CSR(0,0)) / 4;

		for (port = 0; port < RXS2448_MAX_PORTS;  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
				RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(port, 0, did),
				writedata));
		}
		return;
	}

	if ((offset >= RXS_RIO_BC_L1_GX_ENTRYY_CSR(0,0)) && 
		(offset <= RXS_RIO_BC_L1_GX_ENTRYY_CSR(0, RIO_RT_GRP_ENTRIES-1))) {
		did = (offset - RXS_RIO_BC_L1_GX_ENTRYY_CSR(0,0)) / 4;

		for (port = 0; port < RXS2448_MAX_PORTS;  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
				RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(port, 0, did),
				writedata));
		}
	}
}

STATIC uint32_t RXSWriteReg(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t writedata)
{
	uint32_t rc = 0xFFFFFFFF;

	if (NULL == dev_info) {
		return rc;
	}

	if (!st.real_hw) {
		check_write_bc(dev_info, offset, writedata);
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

STATIC void RXSWaitSec(uint32_t delay_nsec, uint32_t delay_sec)
{
	if (st.real_hw) {
		uint64_t counter = delay_nsec + ((uint64_t)delay_sec * 1000000000);
		for ( ; counter; counter--);
	}
}

// Count up maximum registers saved.
// Note: only the first level 1 and level 2 routing table groups are supported
// Use *_MAX_PORTS + 1 to account for device broadcast registers
// There are 2 multicast mask registers for each multicast mask:
//    one to set, and one to clear
#define NUM_DAR_REG ((RXS2448_MAX_PORTS * 5) + 3 + \
	((RXS2448_MAX_PORTS + 1) * RXS2448_MC_MASK_CNT * 2) + \
	((RXS2448_MAX_PORTS + 1) * RIO_RT_GRP_ENTRIES * 2))

#define UPB_DAR_REG (NUM_DAR_REG+1)

rio_perf_opt_reg_t mock_dar_reg[UPB_DAR_REG];

/* Create a mock dev_info.
 */
STATIC void rxs_test_setup(void)
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

/* Initialize the mock register structure for different registers.
 */
STATIC void init_mock_rxs_reg(void **state)
{
	// idx is always should be less than UPB_DAR_REG.
	uint32_t port, idev;
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

	// Initialize RXS_RIO_SPX_CTL, RXS_RIO_SPX_CTL2,
	// RXS_RIO_PLM_SPX_IMP_SPEC_CTL,
	// RXS_PLM_SPX_POL_CTL, and RXS_RIO_SPX_ERR_STAT

	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info, RXS_RIO_SPX_CTL(port), 0x00));
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_ERR_STAT(port), 0x00));
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_CTL2(port), 0x00));
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_PLM_SPX_IMP_SPEC_CTL(port), 0x00));
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
			    RXS_PLM_SPX_POL_CTL(port), 0x00));
	}

	// Initialize RXS_RIO_ROUTE_DFLT_PORT
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, 0x00));

	// Initialize RXS_RIO_PKT_TIME_LIVE
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_PKT_TIME_LIVE, 0x00));

	// Initialize RXS_RIO_SP_LT_CTL
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_SP_LT_CTL, 0x00));

	// Initialize RXS_RIO_BC_MC_Y_S_CSR and RXS_RIO_BC_MC_Y_C_CSR
	for (idev = 0; idev < RXS2448_MC_MASK_CNT; idev++) {
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_BC_MC_X_S_CSR(idev),
				0x00));
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_BC_MC_X_C_CSR(idev),
				0x00));
	}

	// Initialize RXS_RIO_SPX_MC_Y_S_CSR and RXS_RIO_SPX_MC_Y_C_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RXS2448_MC_MASK_CNT; idev++) {
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_SPX_MC_Y_S_CSR(port, idev),
					0x00));
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_SPX_MC_Y_C_CSR(port, idev),
					0x00));
		}
	}

	// Initialize RXS_RIO_BC_L2_GX_ENTRYY_CSR
	for (idev = 0; idev < RIO_RT_GRP_ENTRIES; idev++) {
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_BC_L2_GX_ENTRYY_CSR(0, idev),
				0x00));
	}

	// Initialize RXS_RIO_BC_L1_GX_ENTRYY_CSR
	for (idev = 0; idev < RIO_RT_GRP_ENTRIES; idev++) {
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_BC_L1_GX_ENTRYY_CSR(0, idev),
				0x00));
	}

	// Initialize RXS_RIO_SPX_L2_GY_ENTRYZ_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RIO_RT_GRP_ENTRIES; idev++) {
			assert_int_equal(RIO_SUCCESS,
					DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(port, 0, idev),
					0x00));
		}
	}

	// Initialize RXS_RIO_SPX_L1_GY_ENTRYZ_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RIO_RT_GRP_ENTRIES; idev++) {
			assert_int_equal(RIO_SUCCESS,
					DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(port, 0, idev),
					(!idev)? RIO_RTE_LVL_G0 : 0x00));
		}
	}
}

// The setup function which should be called before any unit tests that
// need to be executed.

STATIC int setup(void **state)
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

STATIC void rxs_init_mock_rt(rio_rt_state_t *rt)
{
	memset(rt, 0xFF, sizeof(rio_rt_state_t));
}

STATIC void rxs_reg_dev_dom(uint32_t port, uint32_t rte_num,
			    uint32_t *dom_out, uint32_t *dev_out)
{
	uint32_t dev_rte_base, dom_rte_base;

	if (RIO_ALL_PORTS == port) {
		dev_rte_base = RXS_RIO_BC_L2_GX_ENTRYY_CSR(0, 0);
		dom_rte_base = RXS_RIO_BC_L1_GX_ENTRYY_CSR(0, 0);
	} else {
		dev_rte_base = RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(port, 0, 0);
		dom_rte_base = RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(port, 0, 0);
	}

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, DOM_RTE_ADDR(dom_rte_base, rte_num), dom_out));
	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, DEV_RTE_ADDR(dev_rte_base, rte_num), dev_out));
}

STATIC void rxs_reg_mc_mask(uint32_t port, uint32_t mc_mask_num,
		uint32_t *mc_mask_out)
{
	uint32_t base_mask_addr;

	if (RIO_ALL_PORTS == port) {
		base_mask_addr = RXS_RIO_SPX_MC_Y_S_CSR(0, 0);
	} else {
		base_mask_addr = RXS_RIO_SPX_MC_Y_S_CSR(port, 0);
	}

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, MC_MASK_ADDR(base_mask_addr, mc_mask_num), mc_mask_out));
}

STATIC void check_init_rt_regs_port(
		uint32_t chk_on_port,
		uint32_t chk_dflt_val,
		uint32_t chk_rt_val,
		uint32_t chk_mask,
		uint32_t chk_first_dom_val)
{
	uint32_t rt_num, temp, s_rt_num = 0;
	uint32_t dom_out, dev_out, mask_num, mc_mask_out;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
	assert_int_equal(temp, chk_dflt_val);

	rxs_reg_dev_dom(chk_on_port, s_rt_num, &dom_out, &dev_out);
	assert_int_equal(chk_first_dom_val, dom_out);
	assert_int_equal(chk_rt_val, dev_out);
	s_rt_num++;
	for (rt_num = s_rt_num; rt_num < RIO_RT_GRP_ENTRIES; rt_num++) {
		rxs_reg_dev_dom(chk_on_port, rt_num, &dom_out, &dev_out);
		assert_int_equal(chk_rt_val, dom_out);
		assert_int_equal(chk_rt_val, dev_out);
	}

	// Mask regs are always always set to 0...
	for (mask_num = 0; mask_num < RIO_DSF_MAX_MC_MASK; mask_num++) {
		rxs_reg_mc_mask(chk_on_port, mask_num, &mc_mask_out);
		assert_int_equal(chk_mask, mc_mask_out);
	}
}

STATIC void check_init_rt_regs(void **state, uint32_t port, bool hw,
		rio_rt_initialize_in_t *mock_init_in)
{
	uint32_t st_p = port;
	uint32_t end_p = port;
	uint32_t chk_rt_val = (hw)?mock_init_in->default_route_table_port:0;
	uint32_t chk_dflt_val = (hw)?mock_init_in->default_route:0;
	uint32_t chk_first_idx_dom_val = RIO_DSF_RT_USE_DEVICE_TABLE;
	uint32_t chk_mask = 0;
	RXS_test_state_t *RXS = *(RXS_test_state_t **)state;

	// When running on real hardware, and the hardware has not been
	// updated, it is not possible to check the register values...
	if (RXS->real_hw && !hw) {
		return;
	}
	if (port == RIO_ALL_PORTS) {
		st_p = 0;
		end_p = RXS2448_MAX_PORTS - 1;
	}

	for (port = st_p; port <= end_p; port++) {
		check_init_rt_regs_port(port,
				chk_dflt_val, chk_rt_val,
				chk_mask, chk_first_idx_dom_val);
	}
}

STATIC void check_init_struct(rio_rt_initialize_in_t *mock_init_in)
{
	uint32_t idx;

	assert_int_equal(mock_init_in->default_route,
		mock_init_in->rt->default_route);

	for (idx = 0; idx < RIO_RT_GRP_ENTRIES; idx++) {
		assert_int_equal(mock_init_in->default_route_table_port,
				mock_init_in->rt->dev_table[idx].rte_val);
		if (mock_init_in->update_hw) {
			assert_false(mock_init_in->rt->dev_table[idx].changed);
		} else {
			assert_true(mock_init_in->rt->dev_table[idx].changed);
		}
	}

	for (idx = 1; idx < RIO_RT_GRP_ENTRIES; idx++) {
		assert_int_equal(mock_init_in->default_route_table_port,
				mock_init_in->default_route_table_port);
		if (mock_init_in->update_hw) {
			assert_false(mock_init_in->rt->dom_table[idx].changed);
		} else {
			assert_true(mock_init_in->rt->dom_table[idx].changed);
		}
	}

	for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
		assert_int_equal(0, mock_init_in->rt->mc_masks[idx].mc_destID);
		assert_int_equal(tt_dev8, mock_init_in->rt->mc_masks[idx].tt);
		assert_int_equal(0, mock_init_in->rt->mc_masks[idx].mc_mask);
		assert_false(mock_init_in->rt->mc_masks[idx].in_use);
		assert_false(mock_init_in->rt->mc_masks[idx].allocd);

		if (mock_init_in->update_hw) {
			assert_false(mock_init_in->rt->mc_masks[idx].changed);
		} else {
			assert_true(mock_init_in->rt->mc_masks[idx].changed);
		}
	}
}


STATIC void rxs_init_rt_test_success_all_ports(void **state, bool hw)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_state_t rt;
	uint8_t port;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		init_mock_rxs_reg(state);
		rxs_init_mock_rt(&rt);
		if (RXS2448_MAX_PORTS == port) {
			mock_init_in.set_on_port = RIO_ALL_PORTS;
			mock_init_in.default_route = RIO_DSF_RT_NO_ROUTE;
			mock_init_in.default_route_table_port =
			RIO_DSF_RT_USE_DEFAULT_ROUTE;
		} else {
			mock_init_in.set_on_port = port;
			mock_init_in.default_route = port;
			mock_init_in.default_route_table_port = port;
		}
		mock_init_in.update_hw = hw;
		mock_init_in.rt = &rt;
		memset(&rt, 0, sizeof(rt));

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		check_init_struct(&mock_init_in);
		check_init_rt_regs(state, mock_init_in.set_on_port,
				mock_init_in.update_hw, &mock_init_in);
	}
}

STATIC void rxs_init_rt_test_success(void **state)
{
	rxs_init_rt_test_success_all_ports(state, false);

	(void)state; // unused
}

STATIC void rxs_init_rt_test_success_hw(void **state)
{
	rxs_init_rt_test_success_all_ports(state, true);

	(void)state; // unused
}


STATIC void rxs_init_rt_null_test_success(void **state)
{
	rio_rt_initialize_in_t	  mock_init_in;
	rio_rt_initialize_out_t	 mock_init_out;
	uint32_t temp;
	uint8_t port;
	RXS_test_state_t *RXS = *(RXS_test_state_t **)state;

	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		init_mock_rxs_reg(state);
		assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
		mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
		mock_init_in.update_hw = false;
		mock_init_in.rt		= NULL;

		assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		//Check initialze values
		// It is not possible to predict the default route value
		// on real hardware.
		if (!RXS->real_hw) {
			assert_int_equal(0, mock_init_in.default_route);
		}
		assert_true(!mock_init_in.update_hw);
		assert_null(mock_init_in.rt);

		check_init_rt_regs(state, port, mock_init_in.update_hw, &mock_init_in);
	}
}

STATIC void rxs_init_rt_null_update_hw_test_success(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	uint32_t temp;
	uint8_t port;
	RXS_test_state_t *RXS = *(RXS_test_state_t **)state;

	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		init_mock_rxs_reg(state);
		assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = (uint8_t)(temp
				& RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
		mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
		mock_init_in.update_hw = true;
		mock_init_in.rt = NULL;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		// It is not possible to predict the default route value
		// on real hardware.
		if (!RXS->real_hw) {
			assert_int_equal(0, mock_init_in.default_route);
		}
		//Check initialize values
		assert_true(mock_init_in.update_hw);
		assert_null(mock_init_in.rt);

		check_init_rt_regs(state, port, mock_init_in.update_hw,
				&mock_init_in);
	}

	(void)state; // unused
}

STATIC void rxs_init_rt_test_port_rte(void **state)
{
	rio_rt_initialize_in_t init_in;
	rio_rt_initialize_out_t init_out;
	rio_rt_state_t rt;
	uint32_t temp;
	uint8_t port;
	RXS_test_state_t *RXS = *(RXS_test_state_t **)state;

	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		init_mock_rxs_reg(state);
		rxs_init_mock_rt(&rt);
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		init_in.set_on_port = port;
		init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
		init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
		init_in.update_hw = true;
		init_in.rt		= &rt;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_rt_initialize(&mock_dev_info, &init_in, &init_out));
		assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

		// It is not possible to predict the default route value
		// on real hardware.
		if (!RXS->real_hw) {
			assert_int_equal(0, init_in.rt->default_route);
		}

		check_init_struct(&init_in);
		check_init_rt_regs(state, port, init_in.update_hw, &init_in);
	}
}

STATIC void rxs_init_rt_test_all_port_mc_mask(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_state_t rt;
	uint32_t temp;
	uint8_t port = RIO_ALL_PORTS;

	rxs_init_mock_rt(&rt);
	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
	mock_init_in.set_on_port = port;
	mock_init_in.default_route = RIO_DSF_RT_NO_ROUTE;
	mock_init_in.default_route_table_port = RIO_DSF_RT_USE_DEFAULT_ROUTE;
	mock_init_in.update_hw = true;
	mock_init_in.rt = &rt;

	assert_int_equal(RIO_SUCCESS,
			rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
					&mock_init_out));
	assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

	check_init_struct(&mock_init_in);
	check_init_rt_regs(state, port, mock_init_in.update_hw, &mock_init_in);

	(void)state; // unused
}

STATIC void rxs_init_rt_test_bad_default_route(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_state_t rt;
	uint32_t temp;
	uint8_t port;

	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		init_mock_rxs_reg(state);
		rxs_init_mock_rt(&rt);
		assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = RIO_DSF_RT_USE_DEFAULT_ROUTE;
		mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
		mock_init_in.update_hw = false;
		mock_init_in.rt = &rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		assert_int_equal(RIO_DSF_RT_USE_DEFAULT_ROUTE,
				mock_init_in.default_route);

		rxs_init_mock_rt(&rt);
		assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = RIO_DSF_RT_USE_DEVICE_TABLE;
		mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
		mock_init_in.update_hw = false;
		mock_init_in.rt = &rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
				mock_init_in.default_route);

		rxs_init_mock_rt(&rt);
		assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = RXS2448_MAX_PORTS + 1;
		mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
		mock_init_in.update_hw = false;
		mock_init_in.rt = &rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		assert_int_equal(RXS2448_MAX_PORTS+1,
				mock_init_in.default_route);
	}

	(void)state; // unused
}

STATIC void rxs_init_rt_test_bad_default_route_table(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_state_t rt;
	uint32_t temp;
	uint8_t port;

	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		init_mock_rxs_reg(state);
		rxs_init_mock_rt(&rt);
		assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = (uint8_t)(temp
				& RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
		mock_init_in.default_route_table_port =
				RIO_DSF_RT_USE_DEVICE_TABLE;
		mock_init_in.update_hw = false;
		mock_init_in.rt = &rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
				mock_init_in.default_route_table_port);

		rxs_init_mock_rt(&rt);
		assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
		mock_init_in.set_on_port = port;
		mock_init_in.default_route = (uint8_t)(temp
				& RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
		mock_init_in.default_route_table_port = RXS2448_MAX_PORTS + 1;
		mock_init_in.update_hw = false;
		mock_init_in.rt = &rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		assert_int_equal(RXS2448_MAX_PORTS+1,
				mock_init_in.default_route_table_port);
	}

	(void)state; // unused
}

STATIC void rxs_init_rt_test_bad_port(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_state_t rt;
	uint32_t temp;
	uint8_t port = RXS2448_MAX_PORTS + 1;

	rxs_init_mock_rt(&rt);
	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
	mock_init_in.set_on_port = port;
	mock_init_in.default_route = (uint8_t)(temp
			& RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
	mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
	mock_init_in.update_hw = false;
	mock_init_in.rt = &rt;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
					&mock_init_out));
	assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

	assert_int_equal(RXS2448_MAX_PORTS+1, mock_init_in.set_on_port);

	(void)state; // unused
}

STATIC void rxs_check_change_rte_rt_change(uint32_t rte_num, uint32_t dflt_rte,
		rio_rt_change_rte_in_t *mock_chg_in)
{
	uint32_t chk_rte_num;

	for (chk_rte_num = 0; chk_rte_num < RIO_RT_GRP_ENTRIES;
			chk_rte_num++) {
		if (chk_rte_num) {
			if ((chk_rte_num == rte_num)
					&& mock_chg_in->dom_entry) {
				assert_int_equal(mock_chg_in->rte_value,
						mock_chg_in->rt->dom_table[chk_rte_num].rte_val);
				if (mock_chg_in->rte_value != dflt_rte) {
					assert_true(
							mock_chg_in->rt->dom_table[chk_rte_num].changed);
				} else {
					assert_false(
							mock_chg_in->rt->dom_table[chk_rte_num].changed);
				}
			} else {
				assert_int_equal(dflt_rte,
						mock_chg_in->rt->dom_table[chk_rte_num].rte_val);
				assert_false(
						mock_chg_in->rt->dom_table[chk_rte_num].changed);
			}
			continue;
		}
		// Dom table entry 0 is special, it must always be RIO_DSF_RT_USE_DEVICE_TABLE
		assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
				mock_chg_in->rt->dom_table[chk_rte_num].rte_val);
		assert_false(mock_chg_in->rt->dom_table[chk_rte_num].changed);
	}

	for (chk_rte_num = 0; chk_rte_num < RIO_RT_GRP_ENTRIES;
			chk_rte_num++) {
		if (chk_rte_num) {
			if ((chk_rte_num == rte_num)
					&& !mock_chg_in->dom_entry) {
				assert_int_equal(mock_chg_in->rte_value,
						mock_chg_in->rt->dev_table[chk_rte_num].rte_val);
				if (mock_chg_in->rte_value != dflt_rte) {
					assert_true(
							mock_chg_in->rt->dev_table[chk_rte_num].changed);
				} else {
					assert_false(
							mock_chg_in->rt->dev_table[chk_rte_num].changed);
				}
			} else {
				assert_int_equal(dflt_rte,
						mock_chg_in->rt->dev_table[chk_rte_num].rte_val);
				assert_false(
						mock_chg_in->rt->dev_table[chk_rte_num].changed);
			}
			continue;
		}
	}
}

STATIC void rxs_change_rte_rt_test_success(bool dom)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_change_rte_in_t mock_chg_in;
	rio_rt_change_rte_out_t mock_chg_out;
	rio_rt_state_t rt;
	rio_port_t port, port_idx;
	rio_port_t port_array[] = {0, 12, RXS2448_MAX_PORTS-1, RIO_ALL_PORTS};
	uint32_t num_ports = sizeof(port_array) / sizeof(port_array[0]);
	uint32_t rte_idx, rte_num, chg_rte_val;
	uint32_t rte_array[] = {0, 1, 254, 255};
	uint32_t num_rtes = sizeof(rte_array) / sizeof(rte_array[0]);

	for (port_idx = 0; port_idx < num_ports; port_idx++) {
		port = port_array[port_idx];
		for (rte_idx = 0; rte_idx < num_rtes; rte_idx++) {
			rxs_init_mock_rt(&rt);
			rte_num = rte_array[rte_idx];

			mock_init_in.set_on_port = RIO_ALL_PORTS;
			if (RIO_ALL_PORTS == port) {
				mock_init_in.default_route =
				RIO_DSF_RT_NO_ROUTE;
				mock_init_in.default_route_table_port =
				RIO_DSF_RT_USE_DEFAULT_ROUTE;
				chg_rte_val = RIO_DSF_RT_NO_ROUTE;
			} else {
				mock_init_in.default_route = port;
				mock_init_in.default_route_table_port = port;
				chg_rte_val = port;
			}
			mock_init_in.update_hw = true;
			mock_init_in.rt = &rt;
			memset(&rt, 0, sizeof(rt));

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_initialize(&mock_dev_info,
							&mock_init_in,
							&mock_init_out));
			assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

			mock_chg_in.dom_entry = dom;
			mock_chg_in.idx = rte_num;
			mock_chg_in.rte_value = chg_rte_val;
			mock_chg_in.rt = mock_init_in.rt;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

			rxs_check_change_rte_rt_change(rte_num,
					mock_init_in.default_route_table_port,
					&mock_chg_in);
		}
	}
}

STATIC void rxs_change_rte_rt_test_dom_hw_success(void **state)
{
	rxs_change_rte_rt_test_success(true);
	(void)state; // unused
}

STATIC void rxs_change_rte_rt_test_dev_hw_success(void **state)
{
	rxs_change_rte_rt_test_success(false);
	(void)state; // unused
}

STATIC void rxs_change_rte_rt_null_test(void **state)
{
	rio_rt_change_rte_in_t mock_chg_in;
	rio_rt_change_rte_out_t mock_chg_out;
	uint32_t rte_num, chg_rte_val;
	uint8_t port;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		for (rte_num = 0; rte_num < RIO_RT_GRP_ENTRIES;
				rte_num++) {
			if (RXS2448_MAX_PORTS == port) {
				chg_rte_val = RIO_DSF_RT_NO_ROUTE;
			} else {
				chg_rte_val = port;
			}

			mock_chg_in.dom_entry = true;
			mock_chg_in.idx = rte_num;
			mock_chg_in.rte_value = chg_rte_val;
			mock_chg_in.rt = NULL;

			assert_int_not_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
		}
	}

	(void)state; // unused
}

STATIC void rxs_change_rte_rt_bad_rte_test(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_change_rte_in_t mock_chg_in;
	rio_rt_change_rte_out_t mock_chg_out;
	rio_rt_state_t rt;
	uint32_t rte_num, rte_idx;
	uint32_t rte_nums[] = {0, 1, 7, 127, 128, 195, 255, RIO_RTE_BAD};
	uint8_t port;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		for (rte_idx = 0; RIO_RTE_BAD != rte_nums[rte_idx]; rte_idx++) {
			rxs_init_mock_rt(&rt);

			rte_num = rte_nums[rte_idx];
			if (RXS2448_MAX_PORTS == port) {
				mock_init_in.set_on_port = RIO_ALL_PORTS;
				mock_init_in.default_route =
				RIO_DSF_RT_NO_ROUTE;
				mock_init_in.default_route_table_port =
				RIO_DSF_RT_USE_DEFAULT_ROUTE;
			} else {
				mock_init_in.set_on_port = port;
				mock_init_in.default_route = port;
				mock_init_in.default_route_table_port = port;
			}

			mock_init_in.update_hw = true;
			mock_init_in.rt = &rt;
			memset(&rt, 0, sizeof(rt));

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_initialize(&mock_dev_info,
							&mock_init_in,
							&mock_init_out));
			assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

			mock_chg_in.dom_entry = true;
			mock_chg_in.idx = rte_num;
			mock_chg_in.rte_value = RXS2448_MAX_PORTS + 1;
			mock_chg_in.rt = mock_init_in.rt;

			assert_int_not_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

			mock_chg_in.dom_entry = true;
			mock_chg_in.idx = rte_num;
			mock_chg_in.rte_value = RIO_DSF_RT_USE_DEVICE_TABLE + 1;
			mock_chg_in.rt = mock_init_in.rt;

			assert_int_not_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

			mock_chg_in.dom_entry = true;
			mock_chg_in.idx = rte_num;
			mock_chg_in.rte_value = RIO_DSF_RT_NO_ROUTE + 1;
			mock_chg_in.rt = mock_init_in.rt;

			assert_int_not_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
		}
	}

	(void)state; // unused
}

STATIC void rxs_change_rte_rt_bad_rte_dev_test(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_change_rte_in_t mock_chg_in;
	rio_rt_change_rte_out_t mock_chg_out;
	rio_rt_state_t rt;
	uint32_t rte_num, rte_idx;
	uint32_t rte_nums[] = {0, 1, 7, 127, 128, 195, 255, RIO_RTE_BAD};
	uint8_t port;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		for (rte_idx = 0; RIO_RTE_BAD != rte_nums[rte_idx]; rte_idx++) {
			rxs_init_mock_rt(&rt);

			rte_num = rte_nums[rte_idx];
			if (RXS2448_MAX_PORTS == port) {
				mock_init_in.set_on_port = RIO_ALL_PORTS;
				mock_init_in.default_route =
				RIO_DSF_RT_NO_ROUTE;
				mock_init_in.default_route_table_port =
				RIO_DSF_RT_USE_DEFAULT_ROUTE;
			} else {
				mock_init_in.set_on_port = port;
				mock_init_in.default_route = port;
				mock_init_in.default_route_table_port = port;
			}

			mock_init_in.update_hw = true;
			mock_init_in.rt = &rt;
			memset(&rt, 0, sizeof(rt));

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_initialize(&mock_dev_info,
							&mock_init_in,
							&mock_init_out));
			assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

			mock_chg_in.dom_entry = false;
			mock_chg_in.idx = rte_num;
			mock_chg_in.rte_value = RIO_DSF_RT_USE_DEVICE_TABLE;
			mock_chg_in.rt = mock_init_in.rt;

			assert_int_not_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
		}
	}

	(void)state; // unused
}

STATIC void rxs_check_alloc_mc_rt_change(uint32_t mc_idx,
		rio_rt_alloc_mc_mask_in_t *mock_alloc_in)
{
	uint32_t idx;

	for (idx = 0; idx <= mc_idx; idx++) {
		assert_true(!mock_alloc_in->rt->mc_masks[idx].in_use);
		assert_true(mock_alloc_in->rt->mc_masks[idx].allocd);
		assert_true(!mock_alloc_in->rt->mc_masks[idx].changed);
	}

	for (idx = mc_idx + 1; idx < RXS2448_MAX_MC_MASK; ++idx) {
		assert_true(!mock_alloc_in->rt->mc_masks[idx].in_use);
		assert_true(!mock_alloc_in->rt->mc_masks[idx].allocd);
		assert_true(!mock_alloc_in->rt->mc_masks[idx].changed);
	}
}

STATIC void rxs_alloc_mc_rt_test_success(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_state_t rt;
	uint8_t port, mc_idx;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		rxs_init_mock_rt(&rt);

		if (RXS2448_MAX_PORTS == port) {
			mock_init_in.set_on_port = RIO_ALL_PORTS;
			mock_init_in.default_route =
			RIO_DSF_RT_NO_ROUTE;
			mock_init_in.default_route_table_port =
			RIO_DSF_RT_USE_DEFAULT_ROUTE;
		} else {
			mock_init_in.set_on_port = port;
			mock_init_in.default_route = port;
			mock_init_in.default_route_table_port = port;
		}
		mock_init_in.update_hw = true;
		mock_init_in.rt = &rt;
		memset(&rt, 0, sizeof(rt));

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		mock_alloc_in.rt = mock_init_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			assert_int_equal(RIO_SUCCESS,
					DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
							&mock_alloc_in,
							&mock_alloc_out));
			assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx,
					mock_alloc_out.mc_mask_rte);

			rxs_check_alloc_mc_rt_change(mc_idx, &mock_alloc_in);
		}

		assert_int_not_equal(RIO_SUCCESS,
				DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
						&mock_alloc_in,
						&mock_alloc_out));
		assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
		assert_int_equal(RIO_DSF_BAD_MC_MASK,
				mock_alloc_out.mc_mask_rte);

		rxs_check_alloc_mc_rt_change(mc_idx - 1, &mock_alloc_in);
	}

	(void)state; // unused
}

STATIC void rxs_alloc_mc_rt_null_test(void **state)
{
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;

	mock_alloc_in.rt = NULL;

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_alloc_mc_rt_bad_in_use_allocd_test(void **state)
{
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_state_t rt;
	uint32_t idx;

	rxs_init_mock_rt(&rt);
	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
		mock_alloc_in.rt->mc_masks[idx].in_use = true;
	}

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

	rxs_init_mock_rt(&rt);
	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
		mock_alloc_in.rt->mc_masks[idx].allocd = true;
	}

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

	rxs_init_mock_rt(&rt);
	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
		mock_alloc_in.rt->mc_masks[idx].in_use = true;
		mock_alloc_in.rt->mc_masks[idx].allocd = true;
	}

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

	(void)state; // unused
}

STATIC void rxs_check_change_mc_rt_change(uint32_t mc_idx,
		rio_rt_change_mc_mask_in_t *mock_mc_chg_in)
{
	uint32_t idx, dom_idx, dev_idx;

	for (idx = 0; idx < RXS2448_MAX_MC_MASK; idx++) {
		if (mock_mc_chg_in->mc_info.in_use) {
			if (idx == mc_idx) {
				assert_int_equal(
						mock_mc_chg_in->mc_info.mc_destID,
						mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
				assert_int_equal(mock_mc_chg_in->mc_info.tt,
						mock_mc_chg_in->rt->mc_masks[idx].tt);
				assert_int_equal(
						mock_mc_chg_in->mc_info.mc_mask,
						mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
				assert_true(
						mock_mc_chg_in->rt->mc_masks[idx].in_use);
				assert_true(
						mock_mc_chg_in->rt->mc_masks[idx].changed);
				assert_false(
						mock_mc_chg_in->rt->mc_masks[idx].allocd);
				continue;
			}
			if (idx < mc_idx) {
				assert_true(
						mock_mc_chg_in->rt->mc_masks[idx].changed);
			} else {
				assert_false(
						mock_mc_chg_in->rt->mc_masks[idx].changed);
			}
		} else {
			assert_false(mock_mc_chg_in->rt->mc_masks[idx].changed);
		}
		assert_int_equal(0,
				mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
		assert_int_equal(tt_dev8, mock_mc_chg_in->rt->mc_masks[idx].tt);
		assert_int_equal(0, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
		assert_false(mock_mc_chg_in->rt->mc_masks[idx].in_use);
		assert_false(mock_mc_chg_in->rt->mc_masks[idx].allocd);
	}

	//Check dom_table
	idx = 0;
	assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
			mock_mc_chg_in->rt->dom_table[idx].rte_val);
	assert_false(mock_mc_chg_in->rt->dom_table[idx].changed);

	dom_idx = (mock_mc_chg_in->mc_info.mc_destID & 0xFF00) >> 8;
	for (idx = 1; idx < RIO_RT_GRP_ENTRIES; idx++) {
		if (idx == dom_idx && mock_mc_chg_in->mc_info.in_use) {
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx,
					mock_mc_chg_in->rt->dom_table[idx].rte_val);
			assert_true(mock_mc_chg_in->rt->dom_table[idx].changed);
			continue;
		}
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask,
				mock_mc_chg_in->rt->dom_table[idx].rte_val);
		assert_false(mock_mc_chg_in->rt->dom_table[idx].changed);
	}

	//Check dev_table
	idx = 0;
	dev_idx = mock_mc_chg_in->mc_info.mc_destID & 0x00FF;
	if (!mock_mc_chg_in->mc_info.in_use
			|| (dev_idx && mock_mc_chg_in->mc_info.in_use)) {
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask,
				mock_mc_chg_in->rt->dev_table[idx].rte_val);
		assert_false(mock_mc_chg_in->rt->dev_table[idx].changed);
	} else {
		assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx,
				mock_mc_chg_in->rt->dev_table[idx].rte_val);
		assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
	}

	for (idx = 1; idx < RIO_RT_GRP_ENTRIES; idx++) {
		if (idx == dev_idx && mock_mc_chg_in->mc_info.in_use) {
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx,
					mock_mc_chg_in->rt->dev_table[idx].rte_val);
			assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
			continue;
		}
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask,
				mock_mc_chg_in->rt->dev_table[idx].rte_val);
		assert_false(mock_mc_chg_in->rt->dev_table[idx].changed);
	}
}

STATIC void rxs_change_mc_rt_test_success(bool dom, bool in_use)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t rt;
	uint32_t port, mc_idx, mc_mask_val;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		rxs_init_mock_rt(&rt);

		if (RXS2448_MAX_PORTS == port) {
			mock_init_in.set_on_port = RIO_ALL_PORTS;
			mock_init_in.default_route =
			RIO_DSF_RT_NO_ROUTE;
			mock_init_in.default_route_table_port =
			RIO_DSF_RT_USE_DEFAULT_ROUTE;
			mc_mask_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
		} else {
			mock_init_in.set_on_port = port;
			mock_init_in.default_route = port;
			mock_init_in.default_route_table_port = port;
			mc_mask_val = port;
		}
		mock_init_in.update_hw = true;
		mock_init_in.rt = &rt;
		memset(&rt, 0, sizeof(rt));

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			mock_mc_chg_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK
					+ mc_idx;
			mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
			if (dom) {
				mock_mc_chg_in.mc_info.mc_destID = (port << 8);
			} else {
				mock_mc_chg_in.mc_info.mc_destID = port;
			}
			mock_mc_chg_in.mc_info.tt = tt_dev16;
			mock_mc_chg_in.mc_info.in_use = in_use;
			mock_mc_chg_in.mc_info.allocd = false;
			mock_mc_chg_in.mc_info.changed = false;
			mock_mc_chg_in.rt = mock_init_in.rt;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_change_mc_mask(
							&mock_dev_info,
							&mock_mc_chg_in,
							&mock_mc_chg_out));
			assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

			rxs_check_change_mc_rt_change(mc_idx, &mock_mc_chg_in);
		}

		mock_mc_chg_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;
		mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
		if (dom) {
			mock_mc_chg_in.mc_info.mc_destID = (port << 8);
		} else {
			mock_mc_chg_in.mc_info.mc_destID = port;
		}
		mock_mc_chg_in.mc_info.tt = tt_dev16;
		mock_mc_chg_in.mc_info.in_use = in_use;
		mock_mc_chg_in.mc_info.allocd = false;
		mock_mc_chg_in.mc_info.changed = false;
		mock_mc_chg_in.rt = mock_init_in.rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_change_mc_mask(&mock_dev_info,
						&mock_mc_chg_in,
						&mock_mc_chg_out));
		assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);
	}
}

STATIC void rxs_change_mc_rt_dom_inuse_hw_test_success(void **state)
{
	rxs_change_mc_rt_test_success(true, true);
	(void)state; // unused
}

STATIC void rxs_change_mc_rt_dev_inuse_hw_test_success(void **state)
{
	rxs_change_mc_rt_test_success(false, true);
	(void)state; // unused
}

STATIC void rxs_change_mc_rt_dom_hw_test_success(void **state)
{
	rxs_change_mc_rt_test_success(true, false);
	(void)state; // unused
}

STATIC void rxs_change_mc_rt_dev_hw_test_success(void **state)
{
	rxs_change_mc_rt_test_success(false, false);
	(void)state; // unused
}

STATIC void rxs_check_change_allocd_mc_rt_change(
		rio_rt_change_mc_mask_in_t *mock_mc_chg_in)
{
	uint32_t idx, dom_idx, dev_idx, chg_idx;

	dev_idx = mock_mc_chg_in->mc_info.mc_destID & 0x00FF;
	chg_idx = MC_MASK_IDX_FROM_ROUTE(mock_mc_chg_in->mc_mask_rte);
	for (idx = 0; idx < RXS2448_MAX_MC_MASK; idx++) {
		if (mock_mc_chg_in->mc_info.in_use) {
			if (idx == chg_idx) {
				assert_int_equal(
						mock_mc_chg_in->mc_info.mc_destID,
						mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
				assert_int_equal(mock_mc_chg_in->mc_info.tt,
						mock_mc_chg_in->rt->mc_masks[idx].tt);
				assert_int_equal(
						mock_mc_chg_in->mc_info.mc_mask,
						mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
				assert_true(
						mock_mc_chg_in->rt->mc_masks[idx].in_use);
				assert_true(
						mock_mc_chg_in->rt->mc_masks[idx].changed);
				assert_true(
						mock_mc_chg_in->rt->mc_masks[idx].allocd);
				continue;
			}
		} else {
			assert_false(mock_mc_chg_in->rt->mc_masks[idx].changed);
		}
		if (dev_idx && idx < dev_idx) {
			assert_int_equal(idx,
					mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
			assert_int_equal(tt_dev16,
					mock_mc_chg_in->rt->mc_masks[idx].tt);
			assert_int_equal(mock_mc_chg_in->mc_info.mc_mask,
					mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
			assert_true(mock_mc_chg_in->rt->mc_masks[idx].allocd);
			continue;
		}
		assert_int_equal(0,
				mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
		assert_int_equal(tt_dev8, mock_mc_chg_in->rt->mc_masks[idx].tt);
		assert_int_equal(0, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
		assert_false(mock_mc_chg_in->rt->mc_masks[idx].in_use);
		assert_false(mock_mc_chg_in->rt->mc_masks[idx].allocd);
	}

	//Check dom_table
	assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
			mock_mc_chg_in->rt->dom_table[0].rte_val);
	assert_false(mock_mc_chg_in->rt->dom_table[0].changed);

	dom_idx = (mock_mc_chg_in->mc_info.mc_destID & 0xFF00) >> 8;
	for (idx = 1; idx < RIO_RT_GRP_ENTRIES; idx++) {
		if (idx == dom_idx && mock_mc_chg_in->mc_info.in_use) {
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + chg_idx,
					mock_mc_chg_in->rt->dom_table[idx].rte_val);
			assert_true(mock_mc_chg_in->rt->dom_table[idx].changed);
			continue;
		}
		if (idx < dom_idx) {
			assert_int_equal(RIO_DSF_RT_NO_ROUTE,
					mock_mc_chg_in->rt->dom_table[idx].rte_val);
			assert_true(mock_mc_chg_in->rt->dom_table[idx].changed);
			continue;
		}
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask,
				mock_mc_chg_in->rt->dom_table[idx].rte_val);
		assert_false(mock_mc_chg_in->rt->dom_table[idx].changed);
	}

	//Check dev_table
	for (idx = 0; idx < RIO_RT_GRP_ENTRIES; idx++) {
		if (idx == dev_idx) {
			assert_int_equal(mock_mc_chg_in->mc_mask_rte,
					mock_mc_chg_in->rt->dev_table[idx].rte_val);
			assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
			continue;
		}
		if (idx < dev_idx) {
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + idx,
					mock_mc_chg_in->rt->dev_table[idx].rte_val);
			assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
			continue;
		}
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask,
				mock_mc_chg_in->rt->dev_table[idx].rte_val);
		assert_false(mock_mc_chg_in->rt->dev_table[idx].changed);
	}
}

STATIC void rxs_change_allocd_mc_rt_test_success(bool dom)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_change_rte_in_t mock_chg_in;
	rio_rt_change_rte_out_t mock_chg_out;
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_state_t rt;
	uint32_t port, mc_idx, mc_mask_val, chg_rte_val;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		rxs_init_mock_rt(&rt);

		if (RXS2448_MAX_PORTS == port) {
			mock_init_in.set_on_port = RIO_ALL_PORTS;
			mock_init_in.default_route =
			RIO_DSF_RT_NO_ROUTE;
			mock_init_in.default_route_table_port =
			RIO_DSF_RT_USE_DEFAULT_ROUTE;
			mc_mask_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
			chg_rte_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
		} else {
			mock_init_in.set_on_port = port;
			mock_init_in.default_route = port;
			mock_init_in.default_route_table_port = port;
			mc_mask_val = port;
			chg_rte_val = port;
		}
		mock_init_in.update_hw = true;
		mock_init_in.rt = &rt;
		memset(&rt, 0, sizeof(rt));

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		mock_alloc_in.rt = mock_init_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			assert_int_equal(RIO_SUCCESS,
					DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
							&mock_alloc_in,
							&mock_alloc_out));
			assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

			mock_chg_in.dom_entry = dom;
			mock_chg_in.idx = mc_idx;
			mock_chg_in.rte_value = chg_rte_val;
			mock_chg_in.rt = mock_init_in.rt;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_change_rte(&mock_dev_info,
							&mock_chg_in,
							&mock_chg_out));
			assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

			mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
			mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
			if (dom) {
				if (!port) {
					break;
				}
				mock_mc_chg_in.mc_info.mc_destID =
						(mc_idx << 8);
			} else {
				mock_mc_chg_in.mc_info.mc_destID = mc_idx;
			}
			mock_mc_chg_in.mc_info.tt = tt_dev16;
			mock_mc_chg_in.mc_info.in_use = true;
			mock_mc_chg_in.mc_info.allocd = false;
			mock_mc_chg_in.mc_info.changed = false;
			mock_mc_chg_in.rt = mock_init_in.rt;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_rt_change_mc_mask(
							&mock_dev_info,
							&mock_mc_chg_in,
							&mock_mc_chg_out));
			assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

			rxs_check_change_allocd_mc_rt_change(&mock_mc_chg_in);
		}

		mock_mc_chg_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;
		mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
		if (dom) {
			if (!port)
				continue;
			mock_mc_chg_in.mc_info.mc_destID = (port << 8);
		} else {
			mock_mc_chg_in.mc_info.mc_destID = port;
		}
		mock_mc_chg_in.mc_info.tt = tt_dev16;
		mock_mc_chg_in.mc_info.in_use = true;
		mock_mc_chg_in.mc_info.allocd = false;
		mock_mc_chg_in.mc_info.changed = false;
		mock_mc_chg_in.rt = mock_init_in.rt;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_rt_change_mc_mask(&mock_dev_info,
						&mock_mc_chg_in,
						&mock_mc_chg_out));
		assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);
	}
}

STATIC void rxs_change_allocd_mc_rt_dom_hw_test_success(void **state)
{
	rxs_change_allocd_mc_rt_test_success(true);
	(void)state; // unused
}

STATIC void rxs_change_allocd_mc_rt_dev_hw_test_success(void **state)
{
	rxs_change_allocd_mc_rt_test_success(false);
	(void)state; // unused
}

STATIC void rxs_change_mc_rt_test_rt_null(void **state)
{
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t rt;

	rxs_init_mock_rt(&rt);

	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	assert_int_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
	mock_mc_chg_in.mc_info.mc_mask = 0x01;
	mock_mc_chg_in.mc_info.mc_destID = 0x01;
	mock_mc_chg_in.mc_info.tt = tt_dev16;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = NULL;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_change_mc_rt_test_bad_mc_destID(void **state)
{
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t rt;

	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	assert_int_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
	mock_mc_chg_in.mc_info.mc_mask = 0x01;
	mock_mc_chg_in.mc_info.mc_destID = 0xFFFFFFFF;
	mock_mc_chg_in.mc_info.tt = tt_dev16;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = mock_alloc_in.rt;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_change_mc_rt_test_bad_mc_mask(void **state)
{
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t rt;

	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	assert_int_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
	mock_mc_chg_in.mc_info.mc_mask = 0xFF000000;
	mock_mc_chg_in.mc_info.mc_destID = 0x01;
	mock_mc_chg_in.mc_info.tt = tt_dev16;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = mock_alloc_in.rt;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
	mock_mc_chg_in.mc_info.mc_mask = 0x000000FF;
	mock_mc_chg_in.mc_info.mc_destID = RIO_LAST_DEV8_DESTID + 1;
	mock_mc_chg_in.mc_info.tt = tt_dev8;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = mock_alloc_in.rt;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
	mock_mc_chg_in.mc_info.mc_mask = 0x000000FF;
	mock_mc_chg_in.mc_info.mc_destID = RIO_LAST_DEV16_DESTID + 1;
	mock_mc_chg_in.mc_info.tt = tt_dev16;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = mock_alloc_in.rt;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_change_mc_rt_test_bad_tdev(void **state)
{
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t rt;

	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	assert_int_equal(RIO_SUCCESS,
			DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in,
					&mock_alloc_out));
	assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
	mock_mc_chg_in.mc_info.mc_mask = 0x01;
	mock_mc_chg_in.mc_info.mc_destID = 0x0100;
	mock_mc_chg_in.mc_info.tt = tt_dev8;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = mock_alloc_in.rt;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_change_mc_rt_test_bad_mc_mask_rte(void **state)
{
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t rt;

	mock_mc_chg_in.mc_mask_rte = 0x0FFF;
	mock_mc_chg_in.mc_info.mc_mask = 0x01;
	mock_mc_chg_in.mc_info.mc_destID = 0x01;
	mock_mc_chg_in.mc_info.tt = tt_dev8;
	mock_mc_chg_in.mc_info.in_use = true;
	mock_mc_chg_in.mc_info.allocd = false;
	mock_mc_chg_in.mc_info.changed = false;
	mock_mc_chg_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_rt_change_mc_mask(&mock_dev_info,
					&mock_mc_chg_in, &mock_mc_chg_out));
	assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_check_dealloc_mc_rt_change(rio_rt_initialize_in_t *mock_init_in,
		rio_rt_dealloc_mc_mask_in_t *mock_dealloc_in)
{
	uint32_t idx, dom_rte, dev_rte;

	//Check mc_mask	
	for (idx = 0; idx < RXS2448_MAX_MC_MASK; ++idx) {
		assert_true(mock_dealloc_in->rt->mc_masks[idx].changed);
		assert_false(mock_dealloc_in->rt->mc_masks[idx].allocd);
		assert_false(mock_dealloc_in->rt->mc_masks[idx].in_use);
		assert_int_equal(tt_dev8,
				mock_dealloc_in->rt->mc_masks[idx].tt);
		assert_int_equal(0,
				mock_dealloc_in->rt->mc_masks[idx].mc_destID);
	}

	//Check dom_table
	for (dom_rte = 0; dom_rte < RIO_RT_GRP_ENTRIES; dom_rte++) {
		if (dom_rte == 0) {
			assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
					mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
			assert_false(
					mock_dealloc_in->rt->dom_table[dom_rte].changed);
			continue;
		}
		if (mock_init_in->set_on_port == RIO_ALL_PORTS) {
			assert_int_equal(mock_init_in->default_route_table_port,
					mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
		} else {
			assert_int_equal(mock_dealloc_in->rt->default_route,
					mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
		}
		assert_false(mock_dealloc_in->rt->dom_table[dom_rte].changed);
	}

	//Check dev_table
	for (dev_rte = 0; dev_rte < RIO_RT_GRP_ENTRIES; dev_rte++) {
		if (mock_init_in->set_on_port == RIO_ALL_PORTS) {
			assert_int_equal(mock_init_in->default_route_table_port,
					mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
		} else {
			assert_int_equal(mock_dealloc_in->rt->default_route,
					mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
		}
		assert_false(mock_dealloc_in->rt->dev_table[dev_rte].changed);
	}
}

STATIC void rxs_dealloc_mc_rt_test_success(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_dealloc_mc_mask_in_t mock_dealloc_in;
	rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
	rio_rt_state_t rt;
	uint8_t port, mc_idx;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		rxs_init_mock_rt(&rt);

		if (RXS2448_MAX_PORTS == port) {
			mock_init_in.set_on_port = RIO_ALL_PORTS;
			mock_init_in.default_route =
			RIO_DSF_RT_NO_ROUTE;
			mock_init_in.default_route_table_port =
			RIO_DSF_RT_USE_DEFAULT_ROUTE;
		} else {
			mock_init_in.set_on_port = port;
			mock_init_in.default_route = port;
			mock_init_in.default_route_table_port = port;
		}
		mock_init_in.update_hw = true;
		mock_init_in.rt = &rt;
		memset(&rt, 0, sizeof(rt));

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		mock_alloc_in.rt = mock_init_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			assert_int_equal(RIO_SUCCESS,
					DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
							&mock_alloc_in,
							&mock_alloc_out));
			assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
		}

		//One more alloc to be falied
		assert_int_not_equal(RIO_SUCCESS,
				DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
						&mock_alloc_in,
						&mock_alloc_out));
		assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

		mock_dealloc_in.rt = mock_alloc_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK
					+ mc_idx;

			assert_int_equal(RIO_SUCCESS,
					DSF_rio_rt_dealloc_mc_mask(
							&mock_dev_info,
							&mock_dealloc_in,
							&mock_dealloc_out));
			assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);
		}
		rxs_check_dealloc_mc_rt_change(&mock_init_in, &mock_dealloc_in);

		//One more dealloc to be succceeded
		mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK;

		assert_int_equal(RIO_SUCCESS,
				DSF_rio_rt_dealloc_mc_mask(&mock_dev_info,
						&mock_dealloc_in,
						&mock_dealloc_out));
		assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

		mock_alloc_in.rt = mock_dealloc_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			assert_int_equal(RIO_SUCCESS,
					DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
							&mock_alloc_in,
							&mock_alloc_out));
			assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
		}

		//One more alloc to be failed
		assert_int_not_equal(RIO_SUCCESS,
				DSF_rio_rt_alloc_mc_mask(&mock_dev_info,
						&mock_alloc_in,
						&mock_alloc_out));
		assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	}

	(void)state; // unused
}

STATIC void rxs_check_dealloc_change_mc_rt_change(
		rio_rt_initialize_in_t *init_in,
		rio_rt_dealloc_mc_mask_in_t *dealloc_in)
{
	uint32_t idx, chg_idx, dom_rte, dev_rte, dev_idx;

	chg_idx = MC_MASK_IDX_FROM_ROUTE(dealloc_in->mc_mask_rte);

	//Check mc_mask
	for (idx = 0; idx < RXS2448_MAX_MC_MASK; ++idx) {
		if (idx <= chg_idx) {
			assert_true(dealloc_in->rt->mc_masks[idx].changed);
		} else {
			assert_false(dealloc_in->rt->mc_masks[idx].changed);
		}
		assert_false(dealloc_in->rt->mc_masks[idx].in_use);
		assert_int_equal(tt_dev8,
				dealloc_in->rt->mc_masks[idx].tt);
		assert_int_equal(0,
				dealloc_in->rt->mc_masks[idx].mc_destID);
		assert_false(dealloc_in->rt->mc_masks[idx].allocd);
	}

	//Check dom_table
	for (dom_rte = 0; dom_rte < RIO_RT_GRP_ENTRIES; dom_rte++) {
		if (dom_rte == 0) {
			assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
				dealloc_in->rt->dom_table[dom_rte].rte_val);
			assert_false(dealloc_in->rt->dom_table[dom_rte].changed);
			continue;
		}

		if (init_in->set_on_port && dom_rte <= chg_idx) {
			assert_int_equal(RIO_DSF_RT_NO_ROUTE,
				dealloc_in->rt->dom_table[dom_rte].rte_val);
			assert_true(dealloc_in->rt->dom_table[dom_rte].changed);
			continue;
		}

		if (init_in->set_on_port == RIO_ALL_PORTS) {
			assert_int_equal(init_in->default_route_table_port,
				dealloc_in->rt->dom_table[dom_rte].rte_val);
		} else {
			assert_int_equal(dealloc_in->rt->default_route,
				dealloc_in->rt->dom_table[dom_rte].rte_val);
		}
		assert_false(dealloc_in->rt->dom_table[dom_rte].changed);
	}

	//Check dev_table
	dev_idx = dealloc_in->rt->mc_masks[chg_idx].mc_destID & 0x00FF;
	for (dev_rte = 0; dev_rte < RIO_RT_GRP_ENTRIES; dev_rte++) {
		if (init_in->set_on_port && dev_rte == dev_idx) {
			assert_int_equal(RIO_DSF_RT_NO_ROUTE,
				dealloc_in->rt->dev_table[dev_rte].rte_val);
			assert_true(dealloc_in->rt->dev_table[dev_rte].changed);
			continue;
		}
	}
}

STATIC void rxs_dealloc_change_mc_rt_test_success(void **state)
{
	rio_rt_initialize_in_t mock_init_in;
	rio_rt_initialize_out_t mock_init_out;
	rio_rt_alloc_mc_mask_in_t mock_alloc_in;
	rio_rt_alloc_mc_mask_out_t mock_alloc_out;
	rio_rt_change_rte_in_t mock_chg_in;
	rio_rt_change_rte_out_t mock_chg_out;
	rio_rt_change_mc_mask_in_t mock_mc_chg_in;
	rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_dealloc_mc_mask_in_t mock_dealloc_in;
	rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
	rio_rt_state_t rt;
	uint32_t port, mc_idx, i;
	uint32_t chg_rte_val, mc_mask_val;

	for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
		rxs_init_mock_rt(&rt);

		if (RXS2448_MAX_PORTS == port) {
			mock_init_in.set_on_port = RIO_ALL_PORTS;
			mock_init_in.default_route =
			RIO_DSF_RT_NO_ROUTE;
			mock_init_in.default_route_table_port =
			RIO_DSF_RT_USE_DEFAULT_ROUTE;
			chg_rte_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
			mc_mask_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
		} else {
			mock_init_in.set_on_port = port;
			mock_init_in.default_route = port;
			mock_init_in.default_route_table_port = port;
			chg_rte_val = port;
			mc_mask_val = port;
		}
		mock_init_in.update_hw = true;
		mock_init_in.rt = &rt;
		memset(&rt, 0, sizeof(rt));

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info,
						&mock_init_in, &mock_init_out));
		assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

		for (i = 0; i < 2; i++) {
			mock_alloc_in.rt = mock_init_in.rt;
			for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK;
					mc_idx++) {
				assert_int_equal(RIO_SUCCESS,
						DSF_rio_rt_alloc_mc_mask(
								&mock_dev_info,
								&mock_alloc_in,
								&mock_alloc_out));
				assert_int_equal(RIO_SUCCESS,
						mock_alloc_out.imp_rc);
			}

			for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK;
					mc_idx++) {
				mock_chg_in.dom_entry = true;
				mock_chg_in.idx = mc_idx;
				mock_chg_in.rte_value = chg_rte_val;
				mock_chg_in.rt = mock_init_in.rt;

				assert_int_equal(RIO_SUCCESS,
						rxs_rio_rt_change_rte(
								&mock_dev_info,
								&mock_chg_in,
								&mock_chg_out));
				assert_int_equal(RIO_SUCCESS,
						mock_chg_out.imp_rc);

				mock_mc_chg_in.mc_mask_rte =
						mock_alloc_out.mc_mask_rte;
				mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
				if (!port) {
					break;
				}
				mock_mc_chg_in.mc_info.mc_destID =
						(mc_idx << 8);
				mock_mc_chg_in.mc_info.tt = tt_dev16;
				mock_mc_chg_in.mc_info.in_use = true;
				mock_mc_chg_in.mc_info.allocd = false;
				mock_mc_chg_in.mc_info.changed = false;
				mock_mc_chg_in.rt = mock_init_in.rt;

				assert_int_equal(RIO_SUCCESS,
						rxs_rio_rt_change_mc_mask(
								&mock_dev_info,
								&mock_mc_chg_in,
								&mock_mc_chg_out));
				assert_int_equal(RIO_SUCCESS,
						mock_mc_chg_out.imp_rc);
			}

			mock_dealloc_in.rt = mock_init_in.rt;
			for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK;
					mc_idx++) {
				mock_dealloc_in.mc_mask_rte =
						RIO_DSF_FIRST_MC_MASK + mc_idx;

				assert_int_equal(RIO_SUCCESS,
						DSF_rio_rt_dealloc_mc_mask(
								&mock_dev_info,
								&mock_dealloc_in,
								&mock_dealloc_out));
				assert_int_equal(RIO_SUCCESS,
						mock_dealloc_out.imp_rc);
			}
			rxs_check_dealloc_change_mc_rt_change(&mock_init_in,
					&mock_dealloc_in);

			//One more dealloc to be succceeded
			mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK;

			assert_int_equal(RIO_SUCCESS,
					DSF_rio_rt_dealloc_mc_mask(
							&mock_dev_info,
							&mock_dealloc_in,
							&mock_dealloc_out));
			assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);
		}
	}

	(void)state; // unused
}

STATIC void rxs_dealloc_mc_rt_null_test(void **state)
{
	rio_rt_dealloc_mc_mask_in_t mock_dealloc_in;
	rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;

	mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK;
	mock_dealloc_in.rt = NULL;

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_dealloc_mc_mask(&mock_dev_info,
					&mock_dealloc_in, &mock_dealloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_dealloc_mc_rt_test_bad_mc_mask_rte(void **state)
{
	rio_rt_dealloc_mc_mask_in_t mock_dealloc_in;
	rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
	rio_rt_state_t rt;

	rxs_init_mock_rt(&rt);
	mock_dealloc_in.mc_mask_rte = RIO_DSF_BAD_MC_MASK;
	mock_dealloc_in.rt = &rt;

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_dealloc_mc_mask(&mock_dev_info,
					&mock_dealloc_in, &mock_dealloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

	mock_dealloc_in.mc_mask_rte = RIO_DSF_MAX_MC_MASK;
	mock_dealloc_in.rt = &rt;

	assert_int_not_equal(RIO_SUCCESS,
			DSF_rio_rt_dealloc_mc_mask(&mock_dev_info,
					&mock_dealloc_in, &mock_dealloc_out));
	assert_int_not_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_check_set_changed_regs_change(rio_rt_initialize_in_t *init_in,
				     rio_rt_change_rte_in_t *chg_in,
				     rio_rt_set_changed_in_t *set_chg_in)
{
	uint32_t chk_port, idx, mc_idx;
	uint32_t dev_out, dom_out, mc_mask_out;

	for (chk_port = 0; chk_port < RXS2448_MAX_PORTS; chk_port++) {
                rxs_reg_dev_dom(chk_port, 0, &dom_out, &dev_out);

		// First domain routing table entry requires special treatement,
		// it must always have the value RIO_DSF_RT_USE_DEVICE_TABLE
		assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, dom_out);
		if (((set_chg_in->set_on_port == chk_port) ||
		     (set_chg_in->set_on_port == RIO_ALL_PORTS)) &&
				!chg_in->idx && !chg_in->dom_entry) {
			assert_int_equal(chg_in->rte_value, dev_out);
		} else {
			assert_int_equal(init_in->default_route_table_port,
					dev_out);
		}

		for (idx = 1; idx < RIO_RT_GRP_ENTRIES; idx++)
		{
			rxs_reg_dev_dom(chk_port, idx, &dom_out, &dev_out);
			// If this is not the entry that changed, it should be
			// the same as the initialized value.
			if ((idx != chg_in->idx) ||
				((chk_port != set_chg_in->set_on_port) &&
				(RIO_ALL_PORTS != set_chg_in->set_on_port))) {

				assert_int_equal(dom_out,
					init_in->default_route_table_port);
				assert_int_equal(dev_out,
					init_in->default_route_table_port);
				continue;
			}

			// This is the value that was changed.
			// Check that the dom/dev entry changed
			// and the dev/dom did not.
			if (chg_in->dom_entry) {
				assert_int_equal(dom_out, chg_in->rte_value);
				assert_int_equal(dev_out,
					init_in->default_route_table_port);
			} else {
				assert_int_equal(dom_out,
					init_in->default_route_table_port);
				assert_int_equal(dev_out, chg_in->rte_value);
			}
		}

		// Confirm no multicast values changed.
                for (mc_idx = 0; mc_idx < RIO_DSF_MAX_MC_MASK; mc_idx++) {
			rxs_reg_mc_mask(chk_port, mc_idx, &mc_mask_out);
			assert_int_equal(0, mc_mask_out);
                }
	}
}

STATIC void rxs_check_set_changed_rt_change(rio_rt_initialize_in_t *init_in,
				     rio_rt_change_rte_in_t *chg_in,
				     rio_rt_set_changed_in_t *set_chg_in)
{
        uint32_t idx;

	//Check dev_table is unchanged except the changed entry.
	for (idx = 0; idx < RIO_RT_GRP_ENTRIES; idx++) {
		assert_false(set_chg_in->rt->dev_table[idx].changed);
		if (chg_in->dom_entry || (chg_in->idx != idx)) {
			assert_int_equal(init_in->default_route_table_port,
				set_chg_in->rt->dev_table[idx].rte_val);
			continue;
		}
		assert_int_equal(chg_in->rte_value,
				set_chg_in->rt->dev_table[idx].rte_val);
        }

	//Check dom_table
	for (idx = 0; idx < RIO_RT_GRP_ENTRIES; idx++) {
		assert_false(set_chg_in->rt->dom_table[idx].changed);

		if (!idx) {
			assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE,
					set_chg_in->rt->dom_table[idx].rte_val);
			continue;
		}

		if (!chg_in->dom_entry || (chg_in->idx != idx)) {
			assert_int_equal(init_in->default_route_table_port,
				set_chg_in->rt->dom_table[idx].rte_val);
			continue;
		}
		assert_int_equal(chg_in->rte_value,
					set_chg_in->rt->dom_table[idx].rte_val);
        }
}

STATIC void rxs_set_changed_rt_test_success(void **state, bool dom)
{
	rio_rt_initialize_in_t      init_in;
        rio_rt_initialize_out_t     init_out;
        rio_rt_change_rte_in_t      chg_in;
        rio_rt_change_rte_out_t     chg_out;
	rio_rt_set_changed_in_t     set_chg_in;
        rio_rt_set_changed_out_t    set_chg_out;
        rio_rt_state_t              rt;
        rio_port_t port, port_idx;
        uint32_t chg_rte_val, rte_num, rte_idx;
	rio_port_t port_array[] = {0, 12, RXS2448_MAX_PORTS-1, RIO_ALL_PORTS};
	uint32_t num_ports = sizeof(port_array) / sizeof(port_array[0]);
	uint32_t rte_array[] = {0, 1, 254, 255};
	uint32_t num_rtes = sizeof(rte_array) / sizeof(rte_array[0]);

        for (port_idx = 0; port_idx < num_ports; port_idx++) {
		port = port_array[port_idx];
		for (rte_idx = 0; rte_idx < num_rtes; rte_idx++) {
			rte_num = rte_array[rte_idx];
			if (DEBUG_PRINTF) {
				printf("\nport %d rte_num %d\n", port, rte_num);
			}

			init_mock_rxs_reg(state);

			// Test each port, or ALL_PORTS
			init_in.set_on_port = RIO_ALL_PORTS;
			if (RIO_ALL_PORTS == port) {
				init_in.default_route = RIO_DSF_RT_NO_ROUTE;
				init_in.default_route_table_port =
						RIO_DSF_RT_NO_ROUTE;
				chg_rte_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
			} else {
				init_in.default_route = port;
				init_in.default_route_table_port = port;
				chg_rte_val = RIO_DSF_RT_NO_ROUTE;
			}

			// Initialize routing table
			init_in.update_hw = true;
			init_in.rt        = &rt;
			memset(&rt, 0, sizeof(rt));

			assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info, &init_in,
                                                        &init_out));
			assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

			// Change routing table entry
			chg_in.dom_entry = dom;
			chg_in.idx       = rte_num;
			chg_in.rte_value = chg_rte_val;
			chg_in.rt        = init_in.rt;

			assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_change_rte(&mock_dev_info,
							&chg_in, &chg_out));
			assert_int_equal(RIO_SUCCESS, chg_out.imp_rc);

			// Set the change in hardware
			set_chg_in.set_on_port = init_in.set_on_port;
			set_chg_in.rt = chg_in.rt;

			assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_set_changed(&mock_dev_info,
						&set_chg_in, &set_chg_out));
			assert_int_equal(RIO_SUCCESS, set_chg_out.imp_rc);

			// Check that the state has been updated correctly.
			rxs_check_set_changed_rt_change(&init_in, &chg_in,
								&set_chg_in);
			rxs_check_set_changed_regs_change(&init_in, &chg_in,
								&set_chg_in);
		}
	}
}

STATIC void rxs_set_changed_rt_dom_hw_test_success(void **state)
{
        rxs_set_changed_rt_test_success(state, true);
        (void)state; // unused
}

STATIC void rxs_set_changed_rt_dev_hw_test_success(void **state)
{
        rxs_set_changed_rt_test_success(state, false);
        (void)state; // unused
}

STATIC void rxs_set_changed_rt_dev_hw_bad_rt_test(void **state)
{
        rio_rt_set_changed_in_t     mock_set_chg_in;
        rio_rt_set_changed_out_t    mock_set_chg_out;

        mock_set_chg_in.set_on_port = 0;
        mock_set_chg_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

	mock_set_chg_in.set_on_port = RIO_ALL_PORTS;
        mock_set_chg_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        (void)state; // unused
}

STATIC void rxs_set_changed_rt_dev_hw_bad_port_test(void **state)
{
        rio_rt_set_changed_in_t     mock_set_chg_in;
        rio_rt_set_changed_out_t    mock_set_chg_out;
        rio_rt_state_t              rt;

	rxs_init_mock_rt(&rt);
        mock_set_chg_in.set_on_port = RXS2448_MAX_PORTS+1;
        mock_set_chg_in.rt = &rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        (void)state; // unused
}

STATIC void rxs_set_changed_rt_dev_hw_bad_default_route_test(void **state)
{
        rio_rt_set_changed_in_t     mock_set_chg_in;
        rio_rt_set_changed_out_t    mock_set_chg_out;
        rio_rt_state_t              rt;

	rxs_init_mock_rt(&rt);
        mock_set_chg_in.set_on_port = 0;
        mock_set_chg_in.rt = &rt;
        mock_set_chg_in.rt->default_route = RIO_DSF_RT_USE_DEVICE_TABLE;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        (void)state; // unused
}

STATIC void rxs_check_set_all_regs_change(rio_rt_set_all_in_t *set_all_in)
{
	uint32_t idx, mc_idx;
	uint32_t dev_out, dom_out, mc_mask_out;

	rio_port_t chk_port, st_port, end_port;

	if (RIO_ALL_PORTS == set_all_in->set_on_port) {
		st_port = 0;
		end_port = RXS2448_MAX_PORTS - 1;
	} else {
		st_port = set_all_in->set_on_port;
		end_port = set_all_in->set_on_port;
	}

	for (chk_port = st_port; chk_port <= end_port; chk_port++) {
                rxs_reg_dev_dom(chk_port, 0, &dom_out, &dev_out);

		// When all entries are set, the hardware must exactly match
		// the routing table status.
		for (idx = 0; idx < RIO_RT_GRP_ENTRIES; idx++) {
			rxs_reg_dev_dom(chk_port, idx, &dom_out, &dev_out);
			assert_int_equal(dom_out,
					set_all_in->rt->dom_table[idx].rte_val);
			assert_int_equal(dev_out,
					set_all_in->rt->dev_table[idx].rte_val);
		}

		// Confirm multicast values are programmed correctly
                for (mc_idx = 0; mc_idx < RIO_DSF_MAX_MC_MASK; mc_idx++) {
			rxs_reg_mc_mask(chk_port, mc_idx, &mc_mask_out);
			if (mc_mask_out != 
				set_all_in->rt->mc_masks[mc_idx].mc_mask) {
				printf("\nPort %d mask %d failed",
					chk_port, mc_idx);
			};
			assert_int_equal(mc_mask_out,
				set_all_in->rt->mc_masks[mc_idx].mc_mask);
                }
	}
}

STATIC void rxs_set_all_rt_test_success(void **state, bool dom)
{
	rio_rt_initialize_in_t init_in;
	rio_rt_initialize_out_t init_out;
	rio_rt_change_rte_in_t chg_in;
	rio_rt_change_rte_out_t chg_out;
	rio_rt_set_all_in_t set_all_in;
	rio_rt_set_all_out_t set_all_out;
	rio_rt_state_t rt;
	rio_port_t port;
	uint32_t port_idx, rte_idx;
	uint32_t chg_rte_val, rte_num;
	rio_port_t port_array[] = {0, 12, RXS2448_MAX_PORTS-1, RIO_ALL_PORTS};
	uint32_t num_ports = sizeof(port_array) / sizeof(port_array[0]);
	uint32_t rte_array[] = {0, 1, 254, 255};
	uint32_t num_rtes = sizeof(rte_array) / sizeof(rte_array[0]);

	for (port_idx = 0; port_idx < num_ports; port_idx++) {
		port = port_array[port_idx];
		for (rte_idx = 0; rte_idx < num_rtes; rte_idx++) {
			rte_num = rte_array[rte_idx];
			if (DEBUG_PRINTF) {
				printf("\nport %d rte_num %d\n", port, rte_num);
			}

			init_mock_rxs_reg(state);

			init_in.set_on_port = RIO_ALL_PORTS;
			if (RIO_ALL_PORTS == port) {
				init_in.default_route = RIO_DSF_RT_NO_ROUTE;
				init_in.default_route_table_port =
						RIO_DSF_RT_USE_DEFAULT_ROUTE;
				chg_rte_val = RIO_DSF_RT_NO_ROUTE;
			} else {
				init_in.default_route = port;
				init_in.default_route_table_port = port;
				chg_rte_val = port;
			}

			// Initialize routing table
			init_in.update_hw = true;
			init_in.rt = &rt;
			memset(&rt, 0, sizeof(rt));

			assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_initialize(&mock_dev_info, &init_in,
							&init_out));
			assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

			// Change a routing table entry the conventional way
			chg_in.dom_entry = dom;
			chg_in.idx = rte_num;
			chg_in.rte_value = chg_rte_val;
			chg_in.rt = init_in.rt;

			assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_change_rte(&mock_dev_info,
							&chg_in, &chg_out));
			assert_int_equal(RIO_SUCCESS, chg_out.imp_rc);

			// Change routing table and multicast entries
			// through direct manipulation.

			rt.dev_table[99].rte_val = chg_rte_val;
			rt.dom_table[98].rte_val = chg_rte_val;
			rt.mc_masks[0].mc_mask = 0x000000FF;
			rt.dom_table[100].rte_val = RIO_RTV_MC_MSK(0);
			rt.dev_table[101].rte_val = RIO_RTV_MC_MSK(0);

			// Set all the routing table entries...
			set_all_in.set_on_port = port;
			if (RXS2448_MAX_PORTS == port) {
				set_all_in.set_on_port = RIO_ALL_PORTS;
			} else {
				set_all_in.set_on_port = port;
			}
			set_all_in.rt = chg_in.rt;

			assert_int_equal(RIO_SUCCESS,
				rxs_rio_rt_set_all(&mock_dev_info,
						&set_all_in, &set_all_out));
			assert_int_equal(RIO_SUCCESS, set_all_out.imp_rc);

			// Check that the hardware matches what is in the
			// routing table state structure.
			rxs_check_set_all_regs_change(&set_all_in);
		}
	}
}

STATIC void rxs_set_all_rt_dom_hw_test_success(void **state)
{
	rxs_set_all_rt_test_success(state, true);
	(void)state; // unused
}

STATIC void rxs_set_all_rt_dev_hw_test_success(void **state)
{
	rxs_set_all_rt_test_success(state, false);
	(void)state; // unused
}

STATIC void rxs_set_all_rt_null_test(void **state)
{
	rio_rt_set_all_in_t set_in;
	rio_rt_set_all_out_t set_out;

	set_in.set_on_port = 0;
	set_in.rt = NULL;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_rt_set_all(&mock_dev_info,
					&set_in, &set_out));
	assert_int_not_equal(RIO_SUCCESS, set_out.imp_rc);

	set_in.set_on_port = RIO_ALL_PORTS;
	set_in.rt = NULL;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_rt_set_all(&mock_dev_info, &set_in, &set_out));
	assert_int_not_equal(RIO_SUCCESS, set_out.imp_rc);

	(void)state; // unused
}

STATIC void rxs_set_all_rt_dev_hw_bad_port_test(void **state)
{
	rio_rt_set_all_in_t set_all_in;
	rio_rt_set_all_out_t set_all_out;
	rio_rt_state_t rt;

	init_mock_rxs_reg(state);
	set_all_in.set_on_port = RXS2448_MAX_PORTS+1;
	set_all_in.rt = &rt;

	assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &set_all_in, &set_all_out));
	assert_int_not_equal(RIO_SUCCESS, set_all_out.imp_rc);
}

STATIC void rxs_set_all_rt_dev_hw_bad_default_route_test(void **state)
{
	rio_rt_set_all_in_t set_all_in;
	rio_rt_set_all_out_t set_all_out;
	rio_rt_state_t rt;

	init_mock_rxs_reg(state);
	set_all_in.set_on_port = 0;
	set_all_in.rt = &rt;
	set_all_in.rt->default_route = RIO_DSF_RT_USE_DEVICE_TABLE;

	assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &set_all_in, &set_all_out));
	assert_int_not_equal(RIO_SUCCESS, set_all_out.imp_rc);
}

int main(int argc, char** argv)
{
	const struct CMUnitTest tests[] = {
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_success_hw,
					setup,
					NULL),

			cmocka_unit_test_setup_teardown(
					rxs_init_rt_null_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_null_update_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_port_rte,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_all_port_mc_mask,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_bad_default_route,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_bad_default_route_table,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_init_rt_test_bad_port,
					setup,
					NULL),

			cmocka_unit_test_setup_teardown(
					rxs_change_rte_rt_test_dom_hw_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_rte_rt_test_dev_hw_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_rte_rt_null_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_rte_rt_bad_rte_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_rte_rt_bad_rte_dev_test,
					setup,
					NULL),

			cmocka_unit_test_setup_teardown(
					rxs_alloc_mc_rt_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_alloc_mc_rt_null_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_alloc_mc_rt_bad_in_use_allocd_test,
					setup,
					NULL),

			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_dom_inuse_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_dev_inuse_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_dom_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_dev_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_test_rt_null,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_test_bad_mc_destID,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_test_bad_mc_mask,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_test_bad_tdev,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_mc_rt_test_bad_mc_mask_rte,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_allocd_mc_rt_dom_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_change_allocd_mc_rt_dev_hw_test_success,
					setup,
					NULL),

			cmocka_unit_test_setup_teardown(
					rxs_dealloc_mc_rt_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_dealloc_change_mc_rt_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_dealloc_mc_rt_null_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_dealloc_mc_rt_test_bad_mc_mask_rte,
					setup,
					NULL),

			cmocka_unit_test_setup_teardown(
					rxs_set_changed_rt_dom_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_changed_rt_dev_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_changed_rt_dev_hw_bad_rt_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_changed_rt_dev_hw_bad_port_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_changed_rt_dev_hw_bad_default_route_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_all_rt_dom_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_all_rt_dev_hw_test_success,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_all_rt_null_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_all_rt_dev_hw_bad_port_test,
					setup,
					NULL),
			cmocka_unit_test_setup_teardown(
					rxs_set_all_rt_dev_hw_bad_default_route_test,
					setup,
					NULL),
	};

	memset(&st, 0, sizeof(st));
	st.argc = argc;
	st.argv = argv;

	return cmocka_run_group_tests(tests, grp_setup, grp_teardown);
}

#endif /* RXSx_DAR_WANTED */

#ifdef __cplusplus
}
#endif
