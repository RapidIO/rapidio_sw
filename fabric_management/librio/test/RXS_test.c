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

typedef struct RXS_test_state_t_TAG {
	int argc;
	char **argv;
	bool real_hw;
	uint32_t mport;
	uint8_t hc;
	uint32_t destid;
	struct rapidio_mport_handle *mp_h;
	bool mp_h_valid;
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
	if (!got_mport || !got_hc || !got_destid) {
		printf("\nMust enter all of -m, -h and -d.\n");
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

static DAR_DEV_INFO_t mock_dev_info;

static uint32_t RXSReadReg(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t *readdata)
{
	uint32_t rc = 0xFFFFFFFF;

	if (NULL == dev_info) {
		return rc;
	}
	// Should only get here when st.real_hw is true, since
	// when real_hw is false, all registers are mocked.
	assert_true(st.real_hw);
	assert_true(st.mp_h_valid);
	if (0xFF == st.hc) {
		rc = riomp_mgmt_lcfg_read(st.mp_h, offset, 4, readdata);
	} else {
		rc = riomp_mgmt_rcfg_read(st.mp_h, st.destid, st.hc, offset, 4, readdata);
	}

	return rc;
}

/* The function tries to find the index of the offset in the dar_reg array and returns the idx,
 * otherwise it returns UPB_DAR_REG.
 */
static void check_write_bc(uint32_t offset, uint32_t writedata)
{
	uint32_t did, port;

	if (st.real_hw) {
		return;
	}
	if ((offset >= RXS_RIO_BC_L2_GX_ENTRYY_CSR(0,0)) &&
		(offset <= RXS_RIO_BC_L2_GX_ENTRYY_CSR(0, RIO_DAR_RT_DEV_TABLE_SIZE-1))) {
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
		(offset <= RXS_RIO_BC_L1_GX_ENTRYY_CSR(0, RIO_DAR_RT_DEV_TABLE_SIZE-1))) {
		did = (offset - RXS_RIO_BC_L1_GX_ENTRYY_CSR(0,0)) / 4;

		for (port = 0; port < RXS2448_MAX_PORTS;  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
				RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(port, 0, did),
				writedata));
		}
	}
}

static uint32_t RXSWriteReg(DAR_DEV_INFO_t *dev_info,
			uint32_t  offset, uint32_t writedata)
{
	uint32_t rc = 0xFFFFFFFF;

	if (NULL == dev_info) {
		return rc;
	}
	if (!st.real_hw) {
		check_write_bc(offset, writedata);
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

#define NUM_DAR_REG ((((RXS2448_MAX_PORTS)*(RXS2448_MAX_SC))*2)+ \
                     (RXS2448_MAX_PORTS*6)+4+ \
                     ((RXS2448_MAX_PORTS)*RIO_DAR_RT_DEV_TABLE_SIZE)+ \
                     (RXS_MAX_L2_GROUP*RIO_DAR_RT_DEV_TABLE_SIZE)+ \
                     (RXS_MAX_L1_GROUP*RIO_DAR_RT_DEV_TABLE_SIZE)+ \
                     ((RXS2448_MAX_PORTS)*RXS_MAX_L2_GROUP*RIO_DAR_RT_DEV_TABLE_SIZE)+ \
                     ((RXS2448_MAX_PORTS)*RXS_MAX_L1_GROUP*RIO_DAR_RT_DEV_TABLE_SIZE) )

#define UPB_DAR_REG (NUM_DAR_REG+1)

rio_perf_opt_reg_t mock_dar_reg[UPB_DAR_REG];

static rio_sc_dev_ctrs_t *mock_dev_ctrs = (rio_sc_dev_ctrs_t *)malloc(sizeof(rio_sc_dev_ctrs_t));
static rio_sc_p_ctrs_val_t *pp_ctrs = (rio_sc_p_ctrs_val_t *)malloc((RIO_MAX_PORTS) * sizeof(rio_sc_p_ctrs_val_t));

/* Create a mock dev_info.
 */
static void rxs_test_setup(void)
{
	uint8_t idx, pnum;
	rio_sc_ctr_val_t init = INIT_RIO_SC_CTR_VAL;

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

	mock_dev_ctrs->num_p_ctrs = RIO_MAX_PORTS;
	mock_dev_ctrs->valid_p_ctrs = RIO_MAX_PORTS;

	for (pnum = 0; pnum < RIO_MAX_PORTS; pnum++) {
		pp_ctrs[pnum].pnum = pnum;
		pp_ctrs[pnum].ctrs_cnt = RXS2448_MAX_SC;
		for (idx = 0; idx < RIO_MAX_SC; idx++) {
			pp_ctrs[pnum].ctrs[idx] = init;
		}
       }
       mock_dev_ctrs->p_ctrs = pp_ctrs;
}

/* Initialize the mock register structure for different registers.
 */
static void init_mock_rxs_reg(void **state)
{
	// idx is always should be less than UPB_DAR_REG.
	uint32_t port, cntr, idev, grp;
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

	// initialize RXS_RIO_SPX_PCNTR_CTL
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (cntr = 0; cntr < RXS2448_MAX_SC; cntr++) {
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_SPX_PCNTR_CTL(port, cntr),
					0x02));
		}
	}

	// Initialize RXS_RIO_SPX_PCNTR_CNTR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (cntr = 0; cntr < RXS2448_MAX_SC; cntr++) {
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, cntr),
					0x10));
		}
	}

	// Initialize RXS_RIO_SPX_PCNTR_CTL, RXS_RIO_SPX_CTL, RXS_RIO_SPX_CTL2, RXS_RIO_PLM_SPX_IMP_SPEC_CTL,
	// RXS_PLM_SPX_POL_CTL, and RXS_RIO_SPX_ERR_STAT
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		assert_int_equal(RIO_SUCCESS,
			DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_EN(port), 0x00));
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

	// Initialize RXS_RIO_PCNTR_CTL
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_PCNTR_CTL, 0x00));

	// Initialize RXS_RIO_ROUTE_DFLT_PORT
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, 0x00));

	// Initialize RXS_RIO_PKT_TIME_LIVE
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_PKT_TIME_LIVE, 0x00));

	// Initialize RXS_RIO_SP_LT_CTL
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_SP_LT_CTL, 0x00));

	// Initialize RXS_RIO_SR_RSP_TO
	assert_int_equal(RIO_SUCCESS,
		DAR_add_poreg(&mock_dev_info, RXS_RIO_SR_RSP_TO, 0x00));

	// Initialize RXS_RIO_SPX_MC_Y_S_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RIO_DAR_RT_DEV_TABLE_SIZE; idev++) {
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_SPX_MC_Y_S_CSR(port, idev),
					0x00));
		}
	}

	// Initialize RXS_RIO_BC_L2_GX_ENTRYY_CSR
	for (grp = 0; grp < RXS_MAX_L2_GROUP; grp++) {
		for (idev = 0; idev < RIO_DAR_RT_DEV_TABLE_SIZE; idev++) {
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_BC_L2_GX_ENTRYY_CSR(grp, idev),
					0x00));
		}
	}

	// Initialize RXS_RIO_BC_L1_GX_ENTRYY_CSR
	for (grp = 0; grp < RXS_MAX_L1_GROUP; grp++) {
		for (idev = 0; idev < RIO_DAR_RT_DEV_TABLE_SIZE; idev++) {
			assert_int_equal(RIO_SUCCESS,
				DAR_add_poreg(&mock_dev_info,
					RXS_RIO_BC_L1_GX_ENTRYY_CSR(grp, idev),
					0x00));
		}
	}

	// Initialize RXS_RIO_SPX_L2_GY_ENTRYZ_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (grp = 0; grp < RXS_MAX_L2_GROUP; grp++) {
			for (idev = 0; idev < RIO_DAR_RT_DEV_TABLE_SIZE; idev++)
			{
				assert_int_equal(RIO_SUCCESS,
					DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(port, grp, idev),
					0x00));
			}
		}
	}

	// Initialize RXS_RIO_SPX_L1_GY_ENTRYZ_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (grp = 0; grp < RXS_MAX_L1_GROUP; grp++) {
			for (idev = 0; idev < RIO_DAR_RT_DEV_TABLE_SIZE; idev++)
			{
				assert_int_equal(RIO_SUCCESS,
					DAR_add_poreg(&mock_dev_info,
				RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(port, grp, idev),
					0x00));
			}
		}
	}
}

/* The setup function which should be called before any unit tests that need to be executed.
 */
static int setup(void **state)
{
        memset(&mock_dev_info, 0x00, sizeof(rio_sc_dev_ctrs_t));
	rxs_test_setup();
        init_mock_rxs_reg(state);

        return 0;
}

/* The teardown function to be called after any tests have finished.
 */
static int teardown(void **state) 
{
        free(mock_dev_ctrs);

        (void)state; //unused
        return 0;
}

void assumptions_test(void **state)
{
	// verify constants
        assert_int_equal(8, RXS2448_MAX_SC);
        assert_int_equal(24, RXS2448_MAX_PORTS);
        assert_int_equal(48, RXS2448_MAX_LANES);

	(void)state; // unused
}

void macros_test(void **state)
{
	assert_int_equal(0x1C100, RXS_RIO_SPX_PCNTR_EN(0x00));
        assert_int_equal(0x1D800, RXS_RIO_SPX_PCNTR_EN(0x17));
        assert_int_equal(0x1C110, RXS_RIO_SPX_PCNTR_CTL(0x00, 0x00));
        assert_int_equal(0x1D82C, RXS_RIO_SPX_PCNTR_CTL(0x17, 0x07));
        assert_int_equal(0x1C130, RXS_RIO_SPX_PCNTR_CNT(0x00, 0x00));
        assert_int_equal(0x1D84C, RXS_RIO_SPX_PCNTR_CNT(0x17, 0x07));
        assert_int_equal(0x30000, RXS_RIO_BC_L0_G0_ENTRYX_CSR(0x00));
        assert_int_equal(0x30028, RXS_RIO_BC_L0_G0_ENTRYX_CSR(0x0A));
        assert_int_equal(0x30400, RXS_RIO_BC_L1_GX_ENTRYY_CSR(0x00, 0x00));
        assert_int_equal(0x30428, RXS_RIO_BC_L1_GX_ENTRYY_CSR(0x00, 0x0A));
        assert_int_equal(0x31000, RXS_RIO_BC_L2_GX_ENTRYY_CSR(0x00, 0x00));
        assert_int_equal(0x31028, RXS_RIO_BC_L2_GX_ENTRYY_CSR(0x00, 0x0A));
        assert_int_equal(0x32000, RXS_RIO_BC_MC_X_S_CSR(0x00));
        assert_int_equal(0x32050, RXS_RIO_BC_MC_X_S_CSR(0x0A));
        assert_int_equal(0x50000, RXS_RIO_SPX_L0_G0_ENTRYY_CSR(0x00, 0x00));
        assert_int_equal(0x64028, RXS_RIO_SPX_L0_G0_ENTRYY_CSR(0x0A, 0x0A));
        assert_int_equal(0x50400, RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(0x00, 0x00, 0x00));
        assert_int_equal(0x64428, RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(0x0A, 0x00, 0x0A));
        assert_int_equal(0x51000, RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(0x00, 0x00, 0x00));
        assert_int_equal(0x65028, RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(0x0A, 0x00, 0x0A));
        assert_int_equal(0x80000, RXS_RIO_SPX_MC_Y_S_CSR(0x00, 0x00));
        assert_int_equal(0x8A050, RXS_RIO_SPX_MC_Y_S_CSR(0x0A, 0x0A));

        assert_int_equal(0x001FF, RIO_DSF_BAD_MC_MASK);

	(void)state; // unused
}

static void rxs_init_ctrs(rio_sc_init_dev_ctrs_in_t *parms_in)
{
        uint8_t pnum;

        parms_in->ptl.num_ports = RIO_ALL_PORTS;
        for (pnum = 0; pnum < RXS2448_MAX_PORTS; pnum++) {
            parms_in->ptl.pnums[pnum] = 0x00;
        }

        parms_in->dev_ctrs = mock_dev_ctrs;
}

void rxs_init_dev_ctrs_test_success(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;
	int i, j;

	// Success case, all ports
        rxs_init_ctrs(&mock_sc_in);

        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        assert_int_equal(24, mock_sc_in.dev_ctrs->valid_p_ctrs);
	for (i = 0; i < 24; i++) {
		assert_int_equal(i, mock_sc_in.dev_ctrs->p_ctrs[i].pnum);
		assert_int_equal(RXS2448_MAX_SC,
			mock_sc_in.dev_ctrs->p_ctrs[i].ctrs_cnt);
		for (j = 0; j < RXS2448_MAX_SC; j++) {
			assert_int_equal(0,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].total);
			assert_int_equal(0,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].last_inc);
			assert_int_equal(rio_sc_disabled,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].sc);
			assert_int_equal(false,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].tx);
			assert_int_equal(true,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].srio);
		}
	}
        (void)state; // unused
}

void rxs_init_dev_ctrs_test_bad_ptrs(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;

	// Test invalid dev_ctrs pointer
	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.dev_ctrs = NULL;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	// Test invalid dev_ctrs->p_ctrs pointer
	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.dev_ctrs->p_ctrs = NULL;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        (void)state; // unused
}

void rxs_init_dev_ctrs_test_bad_p_ctrs(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;

	// Test invalid number of p_ctrs
	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.dev_ctrs->num_p_ctrs = 0;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.dev_ctrs->num_p_ctrs = RXS2448_MAX_PORTS + 1;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.dev_ctrs->valid_p_ctrs = RXS2448_MAX_PORTS + 1;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        (void)state; // unused
}

void rxs_init_dev_ctrs_test_bad_ptl_1(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;

	// Test that a bad Port list is reported correctly.
	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.ptl.num_ports = 3;
	mock_sc_in.ptl.pnums[0] = 1;
	mock_sc_in.ptl.pnums[1] = 3;
	mock_sc_in.ptl.pnums[2] = 24;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        (void)state; // unused
}

void rxs_init_dev_ctrs_test_bad_ptl_2(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;

	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.ptl.num_ports = 3;
	mock_sc_in.ptl.pnums[0] = -1;
	mock_sc_in.ptl.pnums[1] = 3;
	mock_sc_in.ptl.pnums[2] = 5;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        (void)state; // unused
}

void rxs_init_dev_ctrs_test_bad_ptl_3(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;

	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.ptl.num_ports = 3;
	mock_sc_in.ptl.pnums[0] = 5;
	mock_sc_in.ptl.pnums[1] = 3;
	mock_sc_in.ptl.pnums[2] = 5;
        assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        (void)state; // unused
}

void rxs_init_dev_ctrs_test_good_ptl(void **state)
{
        rio_sc_init_dev_ctrs_in_t      mock_sc_in;
        rio_sc_init_dev_ctrs_out_t     mock_sc_out;
	int i, j;

	// Test Port list with a few good entries...
	rxs_init_ctrs(&mock_sc_in);
	mock_sc_in.ptl.num_ports = 3;
	mock_sc_in.ptl.pnums[0] = 1;
	mock_sc_in.ptl.pnums[1] = 3;
	mock_sc_in.ptl.pnums[2] = 23;
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        assert_int_equal(3, mock_sc_in.dev_ctrs->valid_p_ctrs);
        assert_int_equal(1, mock_sc_in.dev_ctrs->p_ctrs[0].pnum);
        assert_int_equal(RXS2448_MAX_SC,
			mock_sc_in.dev_ctrs->p_ctrs[0].ctrs_cnt);
        assert_int_equal(3, mock_sc_in.dev_ctrs->p_ctrs[1].pnum);
        assert_int_equal(RXS2448_MAX_SC,
			mock_sc_in.dev_ctrs->p_ctrs[1].ctrs_cnt);
        assert_int_equal(23, mock_sc_in.dev_ctrs->p_ctrs[2].pnum);
        assert_int_equal(RXS2448_MAX_SC,
			mock_sc_in.dev_ctrs->p_ctrs[2].ctrs_cnt);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < RXS2448_MAX_SC; j++) {
			assert_int_equal(0,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].total);
			assert_int_equal(0,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].last_inc);
			assert_int_equal(rio_sc_disabled,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].sc);
			assert_int_equal(false,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].tx);
			assert_int_equal(true,
				mock_sc_in.dev_ctrs->p_ctrs[i].ctrs[j].srio);
		}
	}

        (void)state; // unused
}

#define MAX_SC_CFG_VAL 21

void test_rxs_cfg_dev_ctr(rio_sc_cfg_rxs_ctr_in_t *mock_sc_in, int sc_cfg)
{
        bool tx = true;
	uint32_t reg_val = 0;
	int ctr_idx;
	rio_sc_cfg_rxs_ctr_out_t mock_sc_out;
	rio_port_t st_pt, end_pt, port;
	bool srio = true;
	bool expect_fail = false;
	uint32_t cdata;

	// Pick out a test value.  Test values cover all valid request
	// parameters, and 3 invalid combinations.
	//
	// Note: The first 8 values are the default configuration for packet
	// counters, used by other packet counter tests...
	mock_sc_in->tx = tx;
	switch (sc_cfg) {
	case 0: mock_sc_in->ctr_type = rio_sc_pkt;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PKT;
		break;
	case 1: mock_sc_in->ctr_type = rio_sc_pkt;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PKT;
		break;
	case 2: mock_sc_in->ctr_type = rio_sc_fab_pkt;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKT;
		srio = false;
		break;
	case 3: mock_sc_in->ctr_type = rio_sc_fab_pkt;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKT;
		srio = false;
		break;
	case 4: mock_sc_in->ctr_type = rio_sc_rio_pload;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PAYLOAD;
		break;
	case 5: mock_sc_in->ctr_type = rio_sc_rio_pload;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PAYLOAD;
		break;
	case 6: mock_sc_in->ctr_type = rio_sc_rio_bwidth;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_TTL_PKTCNTR;
		break;
	case 7: mock_sc_in->ctr_type = rio_sc_disabled;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_DISABLED;
		break;
	case 8: mock_sc_in->ctr_type = rio_sc_fab_pload;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PAYLOAD;
		srio = false;
		break;
	case 9: mock_sc_in->ctr_type = rio_sc_fab_pload;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PAYLOAD;
		srio = false;
		break;
	case 10: mock_sc_in->ctr_type = rio_sc_retries;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RETRIES;
		break;
	case 11: mock_sc_in->ctr_type = rio_sc_retries;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_RETRIES;
		break;
	case 12: mock_sc_in->ctr_type = rio_sc_pna;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_PNA;
		break;
	case 13: mock_sc_in->ctr_type = rio_sc_pna;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_PNA;
		break;
	case 14: mock_sc_in->ctr_type = rio_sc_pkt_drop;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_PKT_DROP;
		break;
	case 15: mock_sc_in->ctr_type = rio_sc_pkt_drop;
		mock_sc_in->tx = !mock_sc_in->tx;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_PKT_DROP;
		break;
	case 16: mock_sc_in->ctr_type = rio_sc_fab_pkt;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKT;
		mock_sc_in->prio_mask = 0;
		expect_fail = true;
		break;
	case 17: mock_sc_in->ctr_type = rio_sc_rio_bwidth;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PAYLOAD;
		mock_sc_in->tx = !mock_sc_in->tx;
		mock_sc_in->prio_mask = 0;
		expect_fail = true;
		break;
	case 18: mock_sc_in->ctr_type = rio_sc_fab_pload;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PAYLOAD;
		mock_sc_in->prio_mask = 0;
		expect_fail = true;
		break;
	case 19: mock_sc_in->ctr_type = rio_sc_pkt_drop;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_PKT_DROP;
		mock_sc_in->prio_mask = 0;
		expect_fail = true;
		break;
	case 20: mock_sc_in->ctr_type = rio_sc_uc_req_pkts;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_DISABLED;
		expect_fail = true;
		break;
	case MAX_SC_CFG_VAL: mock_sc_in->ctr_type = rio_sc_pkt_drop_ttl;
		reg_val = RXS_RIO_SPX_PCNTR_CTL_SEL_DISABLED;
		expect_fail = true;
		break;
	}
	// Get port range to check
	if (RIO_ALL_PORTS == mock_sc_in->ptl.num_ports) {
		st_pt = 0;
		end_pt = RXS2448_MAX_PORTS - 1;
	} else {
		st_pt = end_pt = mock_sc_in->ptl.pnums[0];
	}
	ctr_idx = mock_sc_in->ctr_idx;

	// Initialize test register values for all ports 
	for (port = st_pt; port <= end_pt; port++) {
		// Zero control register for the port
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_EN(port), 0));

		// Set invalid control value
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_CTL(port, ctr_idx),
				RXS_RIO_SPX_PCNTR_CTL_SEL_DISABLED));

		// Set non-zero counter value for the port
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
				0x12345678));
	}

	// If something is expected to fail, do not do any more checking.
	if (expect_fail) {
		mock_sc_out.imp_rc = RIO_SUCCESS;

		assert_int_not_equal(RIO_SUCCESS,
				rio_sc_cfg_rxs_ctr(&mock_dev_info, mock_sc_in,
				&mock_sc_out));
		assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
		return;
	}

	// If something is expected to work, do exhaustive checking
	mock_sc_out.imp_rc = !RIO_SUCCESS;

	assert_int_equal(RIO_SUCCESS, rio_sc_cfg_rxs_ctr(&mock_dev_info,
						mock_sc_in, &mock_sc_out));
	assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);


	for (port = st_pt; port <= end_pt; port++) {
		uint32_t reg_val_temp;
		uint32_t mask_temp;

		// Check counter data structure
		assert_int_equal(port, mock_sc_in->dev_ctrs->p_ctrs[port].pnum);
		assert_int_equal(
			mock_sc_in->dev_ctrs->p_ctrs[port].ctrs[ctr_idx].sc,
			mock_sc_in->ctr_type);
		assert_int_equal(0,
			mock_sc_in->dev_ctrs->p_ctrs[port].ctrs[ctr_idx].total);
		assert_int_equal(0,
			mock_sc_in->dev_ctrs->p_ctrs[port].ctrs[ctr_idx].last_inc);
		assert_int_equal(
			mock_sc_in->tx,
			mock_sc_in->dev_ctrs->p_ctrs[port].ctrs[ctr_idx].tx);
		assert_int_equal(
			srio,
			mock_sc_in->dev_ctrs->p_ctrs[port].ctrs[ctr_idx].srio);
	
		// Check register values.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_EN(port), &cdata));
		assert_int_equal(cdata, RXS_RIO_SPX_PCNTR_EN_ENABLE);

		// Check control value
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_CTL(port, ctr_idx), &cdata));
		mask_temp = mock_sc_in->prio_mask << 8;
		mask_temp &= RXS_RIO_SPC_PCNTR_CTL_PRIO;
		reg_val_temp = reg_val;
		reg_val_temp |= mock_sc_in->tx ? RXS_RIO_SPX_PCNTR_CTL_TX : 0;
		reg_val_temp |= mask_temp;
		assert_int_equal(cdata, reg_val_temp);

		// Check counter value
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx), &cdata));
		assert_int_equal(cdata, 0);
	}
}

void rxs_cfg_dev_ctrs_test_per_port(void **state)
{
	int val;
        rio_sc_init_dev_ctrs_in_t      init_in;
        rio_sc_init_dev_ctrs_out_t     init_out;
	rio_sc_cfg_rxs_ctr_in_t      mock_sc_in;


	// Initialize counters for all ports...
        rxs_init_ctrs(&init_in);

        assert_int_equal(RIO_SUCCESS,
			rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
					&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	// Loop through each counter on each port, and each value,
	// and check counter values and registers...
        mock_sc_in.ptl.num_ports = 1;
        mock_sc_in.dev_ctrs = mock_dev_ctrs;
        mock_sc_in.ctr_en = true;

	for (mock_sc_in.ptl.pnums[0] = 0;
			mock_sc_in.ptl.pnums[0] < RXS2448_MAX_PORTS; 
			mock_sc_in.ptl.pnums[0]++) {
		for (mock_sc_in.ctr_idx = 0;
				mock_sc_in.ctr_idx < RXS2448_MAX_SC;
				++mock_sc_in.ctr_idx) {
			for (val = 0; val < MAX_SC_CFG_VAL; val++) {
        			mock_sc_in.prio_mask = FIRST_BYTE_MASK;
				test_rxs_cfg_dev_ctr(&mock_sc_in, val);
			}
		}
        }
	(void)state; // unused
}

void rxs_cfg_dev_ctrs_test_all_ports(void **state)
{
	int val;
        rio_sc_init_dev_ctrs_in_t      init_in;
        rio_sc_init_dev_ctrs_out_t     init_out;
	rio_sc_cfg_rxs_ctr_in_t      mock_sc_in;

	// Initialize counters for all ports...
        rxs_init_ctrs(&init_in);
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
						&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	// Loop through each counter on each port, and each value,
	// and check counter values and registers...
        mock_sc_in.ptl.num_ports = RIO_ALL_PORTS;
        mock_sc_in.dev_ctrs = mock_dev_ctrs;
        mock_sc_in.ctr_en = true;

        for (mock_sc_in.ctr_idx = 0;
			mock_sc_in.ctr_idx < RXS2448_MAX_SC;
			++mock_sc_in.ctr_idx) {
		for (val = 0; val < MAX_SC_CFG_VAL; val++) {
        		mock_sc_in.prio_mask = FIRST_BYTE_MASK;
			test_rxs_cfg_dev_ctr(&mock_sc_in, val);
		}
	}
	(void)state; // unused
}

// Program counters to default configuration.
//
void rxs_cfg_dev_ctrs_test_default(void **state)
{
        rio_sc_init_dev_ctrs_in_t      init_in;
        rio_sc_init_dev_ctrs_out_t     init_out;
	rio_sc_cfg_rxs_ctr_in_t      mock_sc_in;

	// Initialize counters for all ports...
        rxs_init_ctrs(&init_in);
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
						&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	mock_sc_in.ptl.num_ports = RIO_ALL_PORTS;
        mock_sc_in.dev_ctrs = mock_dev_ctrs;
        mock_sc_in.ctr_en = true;

        for (mock_sc_in.ctr_idx = 0;
			mock_sc_in.ctr_idx < RXS2448_MAX_SC;
			++mock_sc_in.ctr_idx) {
        	mock_sc_in.prio_mask = FIRST_BYTE_MASK;
		test_rxs_cfg_dev_ctr(&mock_sc_in, mock_sc_in.ctr_idx);
	}
	(void)state; // unused
}

static void rxs_init_read_ctrs(rio_sc_read_ctrs_in_t *parms_in)
{
	uint8_t srch_i;
	uint32_t cntr;

	parms_in->ptl.num_ports = RIO_ALL_PORTS;
	parms_in->dev_ctrs = mock_dev_ctrs;
        for (srch_i = 0; srch_i < parms_in->dev_ctrs->valid_p_ctrs; srch_i++) {
		for (cntr = 0; cntr < RXS2448_MAX_SC; cntr++) {
			parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = true;
			switch (cntr) {
			case 0: parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_pkt;
				break;
			case 1:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_fab_pkt;
				break;
			case 2:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_rio_pload;
				break;
			case 3:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_fab_pload;
				break;
			case 4:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_rio_bwidth;
				break;
			case 5:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_retries;
				break;
			case 6:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_pna;
				break;
			default:
                                parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = rio_sc_disabled;
                                break;
			}
		}
	}
}

void rxs_read_dev_ctrs_test(void **state)
{
	rio_sc_read_ctrs_in_t      mock_sc_in;
	rio_sc_read_ctrs_out_t     mock_sc_out;
        rio_sc_init_dev_ctrs_in_t      init_in;
        rio_sc_init_dev_ctrs_out_t     init_out;
	int ctr_idx, port;
	uint32_t cdata;
	uint32_t rval;
	uint32_t st_val = 0x10;

	// Initialize counters structure
        rxs_init_ctrs(&init_in);
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
						&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	// Set up counters 
	rxs_init_read_ctrs(&mock_sc_in);

	// Set up counter registers
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (ctr_idx = 0; ctr_idx < RXS2448_MAX_SC; ctr_idx++) {
			// Set non-zero counter value for the port
			rval = st_val + (port * RXS2448_MAX_SC) + ctr_idx;
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite( &mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
					rval));
		}
	}

	// Check for successful reads...
	assert_int_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	// Check counter values... 
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (ctr_idx = 0; ctr_idx < RXS2448_MAX_SC; ctr_idx++) {
			// Check the counter value for the port...
			assert_int_equal(RIO_SUCCESS,
				DARRegRead( &mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
					&cdata));
			// Do not read disabled counters, they should always
			// be zero.
			rval = st_val + (port * RXS2448_MAX_SC) + ctr_idx;
			if (rio_sc_disabled == pp_ctrs[port].ctrs[ctr_idx].sc) {
				rval = 0;
			}
			assert_int_equal(rval,
				pp_ctrs[port].ctrs[ctr_idx].total);
			assert_int_equal(rval,
				pp_ctrs[port].ctrs[ctr_idx].last_inc);
			if (rval) {
				assert_int_equal(rval, cdata);
			}
		}
	}

	// Increment counter registers
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (ctr_idx = 0; ctr_idx < RXS2448_MAX_SC; ctr_idx++) {
			// Set non-zero counter value for the port
			rval = st_val + (port * RXS2448_MAX_SC) + ctr_idx;
			rval = rval * 3;
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite( &mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
					rval));
		}
	}

	// Check for successful reads...
	assert_int_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	// Check counter values... 
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (ctr_idx = 0; ctr_idx < RXS2448_MAX_SC; ctr_idx++) {
			// Check the counter value for the port...
			rval = st_val + (port * RXS2448_MAX_SC) + ctr_idx;
			assert_int_equal(RIO_SUCCESS,
				DARRegRead( &mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
					&cdata));
			// Do not read disabled counters, they should always
			// be zero.
			if (rio_sc_disabled == pp_ctrs[port].ctrs[ctr_idx].sc) {
				rval = 0;
			}
			assert_int_equal(3 * rval,
				pp_ctrs[port].ctrs[ctr_idx].total);
			assert_int_equal( 2 * rval,
				pp_ctrs[port].ctrs[ctr_idx].last_inc);
			if (rval) {
				assert_int_equal(3 * rval, cdata);
			}
		}
	}

	// Decrement counter registers, check for wrap around handling...
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (ctr_idx = 0; ctr_idx < RXS2448_MAX_SC; ctr_idx++) {
			// Set non-zero counter value for the port
			rval = st_val + (port * RXS2448_MAX_SC) + ctr_idx;
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite( &mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
					rval));
		}
	}

	// Check for successful reads...
	assert_int_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	// Check counter values... 
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (ctr_idx = 0; ctr_idx < RXS2448_MAX_SC; ctr_idx++) {
			uint64_t base = (uint64_t)0x100000000;
			// Check the counter value for the port...
			rval = st_val + (port * RXS2448_MAX_SC) + ctr_idx;
			assert_int_equal(RIO_SUCCESS,
				DARRegRead( &mock_dev_info,
					RXS_RIO_SPX_PCNTR_CNT(port, ctr_idx),
					&cdata));
			// Do not read disabled counters, they should always
			// be zero.
			if (rio_sc_disabled == pp_ctrs[port].ctrs[ctr_idx].sc) {
				rval = 0;
				base = 0;
			}
			assert_int_equal(base + rval,
				pp_ctrs[port].ctrs[ctr_idx].total);
			assert_int_equal(base - (2 * rval),
				pp_ctrs[port].ctrs[ctr_idx].last_inc);
			if (rval) {
				assert_int_equal(rval, cdata);
			}
		}
	}
	(void)state; // unused
}

void rxs_read_dev_ctrs_test_bad_parms1(void **state)
{
	rio_sc_read_ctrs_in_t      mock_sc_in;
	rio_sc_read_ctrs_out_t     mock_sc_out;
        rio_sc_init_dev_ctrs_in_t      init_in;
        rio_sc_init_dev_ctrs_out_t     init_out;

	// Initialize counters structure
        rxs_init_ctrs(&init_in);
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
						&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	// Set up counters 
	rxs_init_read_ctrs(&mock_sc_in);

	// Now try some bad parameters/failure test cases
	mock_sc_in.ptl.num_ports = RXS2448_MAX_PORTS + 1;
	mock_sc_out.imp_rc = RIO_SUCCESS;
	assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
	
	mock_sc_in.ptl.num_ports = 1;
	mock_sc_in.ptl.pnums[0] = RXS2448_MAX_PORTS + 1;
	mock_sc_out.imp_rc = RIO_SUCCESS;
	assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	mock_sc_in.ptl.num_ports = RIO_ALL_PORTS;
	mock_sc_in.dev_ctrs->p_ctrs = NULL;
	mock_sc_out.imp_rc = RIO_SUCCESS;
	assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	mock_sc_in.dev_ctrs = NULL;
	mock_sc_out.imp_rc = RIO_SUCCESS;
	assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
	(void)state; // unused
}

void rxs_read_dev_ctrs_test_bad_parms2(void **state)
{
	rio_sc_read_ctrs_in_t      mock_sc_in;
	rio_sc_read_ctrs_out_t     mock_sc_out;
        rio_sc_init_dev_ctrs_in_t      init_in;
        rio_sc_init_dev_ctrs_out_t     init_out;

	// Initialize counters structure
        rxs_init_ctrs(&init_in);
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
						&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	// Set up counters 
	rxs_init_read_ctrs(&mock_sc_in);

	// Try to read a port that is not in the port list.
        rxs_init_ctrs(&init_in);
	init_in.ptl.num_ports = 3;
	init_in.ptl.pnums[0] = 1;
	init_in.ptl.pnums[1] = 2;
	init_in.ptl.pnums[2] = 3;
	
        assert_int_equal(RIO_SUCCESS, rxs_rio_sc_init_dev_ctrs(&mock_dev_info,
						&init_in, &init_out));
        assert_int_equal(RIO_SUCCESS, init_out.imp_rc);

	rxs_init_read_ctrs(&mock_sc_in);

	mock_sc_in.ptl.num_ports = 1;
	mock_sc_in.ptl.pnums[0] = 5;
	mock_sc_out.imp_rc = RIO_SUCCESS;
	assert_int_not_equal(RIO_SUCCESS, rxs_rio_sc_read_ctrs(&mock_dev_info,
						&mock_sc_in, &mock_sc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
	
	(void)state; // unused
}

void rxs_init_read_dev_ctrs_test(void **state)
{
        rxs_init_dev_ctrs_test_success(state);
        rxs_read_dev_ctrs_test(state);

        (void)state; // unused
}

void rxs_init_cfg_read_dev_ctrs_test(void **state)
{
        rxs_init_dev_ctrs_test_success(state);
	rxs_cfg_dev_ctrs_test_default(state);
        rxs_read_dev_ctrs_test(state);

        (void)state; // unused
}

static void rxs_init_mock_rt(rio_rt_state_t *rt)
{
	memset(rt, 0xFF, sizeof(rio_rt_state_t));
}

static void rxs_reg_dev_dom(uint32_t port, uint32_t rte_num, uint32_t *dom_out, uint32_t *dev_out)
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
		DARRegRead(&mock_dev_info,
			DOM_RTE_ADDR(dom_rte_base, rte_num), dom_out));
        assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info,
			DEV_RTE_ADDR(dev_rte_base, rte_num), dev_out));
}

static void rxs_reg_mc_mask(uint32_t port, uint32_t mc_mask_num, uint32_t *mc_mask_out)
{
        uint32_t base_mask_addr;

        if (RIO_ALL_PORTS == port) {
           base_mask_addr = RXS_RIO_SPX_MC_Y_S_CSR(0, 0);
        } else {
           base_mask_addr = RXS_RIO_SPX_MC_Y_S_CSR(port, 0);
        }

        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, MC_MASK_ADDR(base_mask_addr, mc_mask_num), mc_mask_out));
}

void check_init_rt_regs_port(uint32_t chk_on_port,
		uint32_t chk_dflt_val, uint32_t chk_rt_val, uint32_t chk_mask, uint32_t chk_first_dom_val)
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
	for (rt_num = s_rt_num; rt_num < RIO_DAR_RT_DEV_TABLE_SIZE; rt_num++) { 
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

void check_init_rt_regs(uint32_t port, bool hw,
		rio_rt_initialize_in_t *mock_init_in)
{
	uint32_t st_p = port;
	uint32_t end_p = port;
	uint32_t chk_rt_val = (hw)?mock_init_in->default_route_table_port:0;
	uint32_t chk_dflt_val = (hw)?mock_init_in->default_route:0;
        uint32_t chk_first_idx_dom_val = (hw)?RIO_DSF_RT_USE_DEVICE_TABLE:0;
	uint32_t chk_mask = 0;

	if (port == RIO_ALL_PORTS) {
		st_p = 0;
		end_p = RXS2448_MAX_PORTS - 1;

		 for (port = st_p; port <= end_p; port++) {
			check_init_rt_regs_port(port,
                                chk_dflt_val, chk_rt_val, chk_mask, chk_first_idx_dom_val);
                 }
	}

	for (port = st_p; port <= end_p; port++) {
		check_init_rt_regs_port(port,
				chk_dflt_val, chk_rt_val, chk_mask, chk_first_idx_dom_val);
	}
}

void check_init_struct(rio_rt_initialize_in_t *mock_init_in)
{
        uint32_t idx;

	assert_int_equal(mock_init_in->default_route,
		mock_init_in->rt->default_route);

	for (idx = 0; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
                  assert_int_equal(mock_init_in->default_route_table_port,
				mock_init_in->rt->dev_table[idx].rte_val);
		if (mock_init_in->update_hw) {
			assert_false(mock_init_in->rt->dev_table[idx].changed);
		} else {
			assert_true(mock_init_in->rt->dev_table[idx].changed);
		}
	}

	for (idx = 1; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
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


void rxs_init_rt_test_success_all_ports(void **state, bool hw)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_state_t              rt;
        uint8_t port;

        for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg(state);
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
            mock_init_in.update_hw = hw;
            mock_init_in.rt        = &rt;
		memset(&rt, 0, sizeof(rt));

            assert_int_equal(RIO_SUCCESS,
		rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
							&mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            check_init_struct(&mock_init_in);
            check_init_rt_regs(mock_init_in.set_on_port, mock_init_in.update_hw, &mock_init_in);
        }
}

void rxs_init_rt_test_success(void **state)
{
	rxs_init_rt_test_success_all_ports(state, false);

        (void)state; // unused
}
void rxs_init_rt_test_success_hw(void **state)
{
	rxs_init_rt_test_success_all_ports(state, true);

        (void)state; // unused
}


void rxs_init_rt_null_test_success(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;

        uint32_t temp;
        uint8_t port;

        for (port = 0; port < RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg(state);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
            mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
            mock_init_in.update_hw = false;
            mock_init_in.rt        = NULL;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            //Check initialze values
            assert_int_equal(0, mock_init_in.default_route);
            assert_true(!mock_init_in.update_hw);
            assert_null(mock_init_in.rt);

            check_init_rt_regs(port, mock_init_in.update_hw, &mock_init_in);
        }

        (void)state; // unused
}

void rxs_init_rt_null_update_hw_test_success(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        uint32_t temp;
        uint8_t port;

        for (port = 0; port < RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg(state);
             assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
             mock_init_in.set_on_port = port;
             mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
             mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
             mock_init_in.update_hw = true;
             mock_init_in.rt        = NULL;

             assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
             assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

             //Check initialze values
             assert_int_equal(0, mock_init_in.default_route);
             assert_true(mock_init_in.update_hw);
             assert_null(mock_init_in.rt);

             check_init_rt_regs(port, mock_init_in.update_hw, &mock_init_in);
        }

        (void)state; // unused
}
void rxs_init_rt_test_port_rte(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port;

        for (port = 0; port < RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg(state);
            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
            mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
            mock_init_in.update_hw = true;
            mock_init_in.rt        = &rt;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            assert_int_equal(0, mock_init_in.rt->default_route);

            check_init_struct(&mock_init_in);
	    check_init_rt_regs(port, mock_init_in.update_hw, &mock_init_in);
        }

        (void)state; // unused
}

void rxs_init_rt_test_all_port_mc_mask(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = RIO_ALL_PORTS;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.default_route_table_port = RIO_DSF_RT_USE_DEFAULT_ROUTE;
        mock_init_in.update_hw = true;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        check_init_struct(&mock_init_in);
        check_init_rt_regs(port, mock_init_in.update_hw, &mock_init_in);

        (void)state; // unused
}

void rxs_init_rt_test_bad_default_route(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port;

        for (port = 0; port < RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg(state);
            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = RIO_DSF_RT_USE_DEFAULT_ROUTE;
            mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
            mock_init_in.update_hw = false;
            mock_init_in.rt        = &rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            assert_int_equal(RIO_DSF_RT_USE_DEFAULT_ROUTE, mock_init_in.default_route);

            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = RIO_DSF_RT_USE_DEVICE_TABLE;
            mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
            mock_init_in.update_hw = false;
            mock_init_in.rt        = &rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_init_in.default_route);

            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = RXS2448_MAX_PORTS + 1;
            mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
            mock_init_in.update_hw = false;
            mock_init_in.rt        = &rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            assert_int_equal(RXS2448_MAX_PORTS+1, mock_init_in.default_route);
        }

        (void)state; // unused
}

void rxs_init_rt_test_bad_default_route_table(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port;

        for (port = 0; port < RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg(state);
            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
            mock_init_in.default_route_table_port = RIO_DSF_RT_USE_DEVICE_TABLE;
            mock_init_in.update_hw = false;
            mock_init_in.rt        = &rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_init_in.default_route_table_port);

            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
            mock_init_in.default_route_table_port = RXS2448_MAX_PORTS + 1;
            mock_init_in.update_hw = false;
            mock_init_in.rt        = &rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

           assert_int_equal(RXS2448_MAX_PORTS+1, mock_init_in.default_route_table_port);
        }

        (void)state; // unused
}

void rxs_init_rt_test_bad_port(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = RXS2448_MAX_PORTS + 1;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_not_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        assert_int_equal(RXS2448_MAX_PORTS+1, mock_init_in.set_on_port);

        (void)state; // unused
}

void rxs_check_change_rte_rt_change(uint32_t rte_num, uint32_t dflt_rte, rio_rt_change_rte_in_t *mock_chg_in)
{
        uint32_t chk_rte_num;

	for (chk_rte_num = 0; chk_rte_num < RIO_DAR_RT_DOM_TABLE_SIZE; chk_rte_num++) {
	    if (chk_rte_num) {
		if ((chk_rte_num == rte_num) && mock_chg_in->dom_entry) {
			assert_int_equal(mock_chg_in->rte_value, mock_chg_in->rt->dom_table[chk_rte_num].rte_val);
                        if (mock_chg_in->rte_value != dflt_rte) {
                                assert_true(mock_chg_in->rt->dom_table[chk_rte_num].changed);
                        }
                        else {
                                assert_false(mock_chg_in->rt->dom_table[chk_rte_num].changed);
                        }
		} else {
			assert_int_equal(dflt_rte, mock_chg_in->rt->dom_table[chk_rte_num].rte_val);
			assert_false(mock_chg_in->rt->dom_table[chk_rte_num].changed);
		}
		continue;
	    }
	    // Dom table entry 0 is special, it must always be RIO_DSF_RT_USE_DEVICE_TABLE
            assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_chg_in->rt->dom_table[chk_rte_num].rte_val);
 	    assert_false(mock_chg_in->rt->dom_table[chk_rte_num].changed);
        }

	for (chk_rte_num = 0; chk_rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; chk_rte_num++) {
	    if (chk_rte_num) {
		if ((chk_rte_num == rte_num) && !mock_chg_in->dom_entry) {
			assert_int_equal(mock_chg_in->rte_value, mock_chg_in->rt->dev_table[chk_rte_num].rte_val);
			if (mock_chg_in->rte_value != dflt_rte) {
                                assert_true(mock_chg_in->rt->dev_table[chk_rte_num].changed);
                        }
                        else {
                                assert_false(mock_chg_in->rt->dev_table[chk_rte_num].changed);
                        }
		} else {
			assert_int_equal(dflt_rte, mock_chg_in->rt->dev_table[chk_rte_num].rte_val);
			assert_false(mock_chg_in->rt->dev_table[chk_rte_num].changed);
		}
		continue;
	    }
        }
}

void rxs_change_rte_rt_test_success(bool dom)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        rio_rt_state_t              rt;
        uint32_t                    port;
	uint32_t		    chg_rte_val, rte_num, rte_idx;
	uint32_t rte_nums[] = {0, 1, 7, 127, 128, 195, 255, RIO_RTE_BAD};

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
			chg_rte_val = RIO_DSF_RT_NO_ROUTE;
		} else {
            		mock_init_in.set_on_port = port;
            		mock_init_in.default_route = port;
            		mock_init_in.default_route_table_port = port;
			chg_rte_val = port;
		}
            	mock_init_in.update_hw = true;
            	mock_init_in.rt        = &rt;
		memset(&rt, 0, sizeof(rt));

            	assert_int_equal(RIO_SUCCESS,
			rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
							&mock_init_out));
            	assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

                mock_chg_in.dom_entry = dom;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = chg_rte_val;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, 
			&mock_chg_in, &mock_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

		rxs_check_change_rte_rt_change(rte_num, 
			mock_init_in.default_route_table_port, &mock_chg_in);
	    }
        }
}

void rxs_change_rte_rt_test_dom_hw_success(void **state)
{
        rxs_change_rte_rt_test_success(true);
        (void)state; // unused
}

void rxs_change_rte_rt_test_dev_hw_success(void **state)
{
        rxs_change_rte_rt_test_success(false);
        (void)state; // unused
}

void rxs_change_rte_rt_null_test(void **state)
{
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        uint32_t rte_num, chg_rte_val;
        uint8_t port;

        for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
            for (rte_num = 0; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                if (RXS2448_MAX_PORTS == port) {
                        chg_rte_val = RIO_DSF_RT_NO_ROUTE;
                } else {
                        chg_rte_val = port;
                }

                mock_chg_in.dom_entry = true;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = chg_rte_val;
                mock_chg_in.rt        = NULL;

                assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
                assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
            }
        }

        (void)state; // unused
}

void rxs_change_rte_rt_bad_rte_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        rio_rt_state_t              rt;
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
                mock_init_in.rt        = &rt;
                memset(&rt, 0, sizeof(rt));

                assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
                assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

                mock_chg_in.dom_entry = true;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = RXS2448_MAX_PORTS+1;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
                assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

		mock_chg_in.dom_entry = true;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = RIO_DSF_RT_USE_DEVICE_TABLE+1;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
                assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

		mock_chg_in.dom_entry = true;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = RIO_DSF_RT_NO_ROUTE+1;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
                assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
            }
        }

        (void)state; // unused
}

void rxs_change_rte_rt_bad_rte_dev_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        rio_rt_state_t              rt;
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
                mock_init_in.rt        = &rt;
                memset(&rt, 0, sizeof(rt));

                assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
                assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

                mock_chg_in.dom_entry = false;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = RIO_DSF_RT_USE_DEVICE_TABLE;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
                assert_int_not_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
            }
        }

        (void)state; // unused
}

void rxs_check_alloc_mc_rt_change(uint32_t mc_idx, rio_rt_alloc_mc_mask_in_t *mock_alloc_in)
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

void rxs_alloc_mc_rt_test_success(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_state_t              rt;
        uint8_t                     port, mc_idx;

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
            mock_init_in.rt        = &rt;
            memset(&rt, 0, sizeof(rt));

            assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            mock_alloc_in.rt = mock_init_in.rt;
	    for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
		assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        	assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
		assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx, mock_alloc_out.mc_mask_rte);

        	rxs_check_alloc_mc_rt_change(mc_idx, &mock_alloc_in);
	    }

            assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
            assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
            assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

            rxs_check_alloc_mc_rt_change(mc_idx - 1, &mock_alloc_in);
        }

        (void)state; // unused
}

void rxs_alloc_mc_rt_null_test(void **state)
{
	rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;

        mock_alloc_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        (void)state; // unused
}

void rxs_alloc_mc_rt_bad_in_use_allocd_test(void **state)
{
	rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_state_t              rt;
	uint32_t idx;

	rxs_init_mock_rt(&rt);
        mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

	for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
	    mock_alloc_in.rt->mc_masks[idx].in_use = true;
        }

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

	rxs_init_mock_rt(&rt);
        mock_alloc_in.rt = &rt;
        memset(&rt, 0, sizeof(rt));

        for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
            mock_alloc_in.rt->mc_masks[idx].allocd = true;
        }

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

	rxs_init_mock_rt(&rt);
        mock_alloc_in.rt = &rt;
        memset(&rt, 0, sizeof(rt));

        for (idx = 0; idx < RIO_DSF_MAX_MC_MASK; idx++) {
	    mock_alloc_in.rt->mc_masks[idx].in_use = true;
            mock_alloc_in.rt->mc_masks[idx].allocd = true;
        }

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
        assert_int_equal(RIO_DSF_BAD_MC_MASK, mock_alloc_out.mc_mask_rte);

        (void)state; // unused
}

void rxs_check_change_mc_rt_change(uint32_t mc_idx, rio_rt_change_mc_mask_in_t  *mock_mc_chg_in)
{
        uint32_t idx, dom_idx, dev_idx;

	for (idx = 0; idx < RXS2448_MAX_MC_MASK; idx++) {
		if (mock_mc_chg_in->mc_info.in_use) {
			if (idx == mc_idx) {
				assert_int_equal(mock_mc_chg_in->mc_info.mc_destID, mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
	                        assert_int_equal(mock_mc_chg_in->mc_info.tt, mock_mc_chg_in->rt->mc_masks[idx].tt);
        	                assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
                	        assert_true(mock_mc_chg_in->rt->mc_masks[idx].in_use);
				assert_true(mock_mc_chg_in->rt->mc_masks[idx].changed);
				assert_false(mock_mc_chg_in->rt->mc_masks[idx].allocd);
				continue;
			}
			if (idx < mc_idx) {
				assert_true(mock_mc_chg_in->rt->mc_masks[idx].changed);
			}
			else {
				assert_false(mock_mc_chg_in->rt->mc_masks[idx].changed);
			}
                } 
		else {
			assert_false(mock_mc_chg_in->rt->mc_masks[idx].changed);
		}
		assert_int_equal(0, mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
                assert_int_equal(tt_dev8, mock_mc_chg_in->rt->mc_masks[idx].tt);
                assert_int_equal(0, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
                assert_false(mock_mc_chg_in->rt->mc_masks[idx].in_use);
		assert_false(mock_mc_chg_in->rt->mc_masks[idx].allocd);
	}

	//Check dom_table
	idx = 0;
	assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_mc_chg_in->rt->dom_table[idx].rte_val);
	assert_false(mock_mc_chg_in->rt->dom_table[idx].changed);

	dom_idx = (mock_mc_chg_in->mc_info.mc_destID & 0xFF00) >> 8;
	for (idx = 1; idx < RIO_DAR_RT_DOM_TABLE_SIZE; idx++) {
                if (idx == dom_idx && mock_mc_chg_in->mc_info.in_use) {
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx, mock_mc_chg_in->rt->dom_table[idx].rte_val);
	                assert_true(mock_mc_chg_in->rt->dom_table[idx].changed);
			continue;
		}
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->dom_table[idx].rte_val);
		assert_false(mock_mc_chg_in->rt->dom_table[idx].changed);
	} 

	//Check dev_table
	idx = 0;
	dev_idx = mock_mc_chg_in->mc_info.mc_destID & 0x00FF;
	if (!mock_mc_chg_in->mc_info.in_use || (dev_idx && mock_mc_chg_in->mc_info.in_use)) {
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                assert_false(mock_mc_chg_in->rt->dev_table[idx].changed);
	}
	else {
		assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
	}

        for (idx = 1; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
                if (idx == dev_idx && mock_mc_chg_in->mc_info.in_use) {
                        assert_int_equal(RIO_DSF_FIRST_MC_MASK + mc_idx, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                        assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
                        continue;
                }
                assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                assert_false(mock_mc_chg_in->rt->dev_table[idx].changed);
        }
}

void rxs_change_mc_rt_test_success(bool dom, bool in_use)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_state_t              rt;
	uint32_t		    port, mc_idx, mc_mask_val;

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
            mock_init_in.rt        = &rt;
            memset(&rt, 0, sizeof(rt));

            assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

	    for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
            	mock_mc_chg_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;
            	mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
            	if (dom) {
			mock_mc_chg_in.mc_info.mc_destID = (port << 8);
            	}
	    	else {
                	mock_mc_chg_in.mc_info.mc_destID = port;
	    	}
            	mock_mc_chg_in.mc_info.tt = tt_dev16;
            	mock_mc_chg_in.mc_info.in_use = in_use;
            	mock_mc_chg_in.mc_info.allocd = false;
            	mock_mc_chg_in.mc_info.changed = false;
            	mock_mc_chg_in.rt = mock_init_in.rt;

            	assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
            	assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

            	rxs_check_change_mc_rt_change(mc_idx, &mock_mc_chg_in);
            }

	    mock_mc_chg_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;
            mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
            if (dom) {
                    mock_mc_chg_in.mc_info.mc_destID = (port << 8);
            }
            else {
                    mock_mc_chg_in.mc_info.mc_destID = port;
            }
            mock_mc_chg_in.mc_info.tt = tt_dev16;
            mock_mc_chg_in.mc_info.in_use = in_use;
            mock_mc_chg_in.mc_info.allocd = false;
            mock_mc_chg_in.mc_info.changed = false;
            mock_mc_chg_in.rt = mock_init_in.rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
            assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);
        }
}

void rxs_change_mc_rt_dom_inuse_hw_test_success(void **state)
{
	rxs_change_mc_rt_test_success(true, true);
	(void)state; // unused
}

void rxs_change_mc_rt_dev_inuse_hw_test_success(void **state)
{
        rxs_change_mc_rt_test_success(false, true);
        (void)state; // unused
}

void rxs_change_mc_rt_dom_hw_test_success(void **state)
{
        rxs_change_mc_rt_test_success(true, false);
        (void)state; // unused
}

void rxs_change_mc_rt_dev_hw_test_success(void **state)
{
        rxs_change_mc_rt_test_success(false, false);
        (void)state; // unused
}

void rxs_check_change_allocd_mc_rt_change(rio_rt_change_mc_mask_in_t  *mock_mc_chg_in)
{
        uint32_t idx, dom_idx, dev_idx, chg_idx;

	dev_idx = mock_mc_chg_in->mc_info.mc_destID & 0x00FF;
	chg_idx = MC_MASK_IDX_FROM_ROUTE(mock_mc_chg_in->mc_mask_rte);
        for (idx = 0; idx < RXS2448_MAX_MC_MASK; idx++) {
                if (mock_mc_chg_in->mc_info.in_use) {
                        if (idx == chg_idx) {
                                assert_int_equal(mock_mc_chg_in->mc_info.mc_destID, mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
                                assert_int_equal(mock_mc_chg_in->mc_info.tt, mock_mc_chg_in->rt->mc_masks[idx].tt);
                                assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
                                assert_true(mock_mc_chg_in->rt->mc_masks[idx].in_use);
                                assert_true(mock_mc_chg_in->rt->mc_masks[idx].changed);
                                assert_true(mock_mc_chg_in->rt->mc_masks[idx].allocd);
                                continue;
                        }
                }
                else {
                        assert_false(mock_mc_chg_in->rt->mc_masks[idx].changed);
                }
		if (dev_idx && idx < dev_idx) {
			assert_int_equal(idx, mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
	                assert_int_equal(tt_dev16, mock_mc_chg_in->rt->mc_masks[idx].tt);
        	        assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
                	assert_true(mock_mc_chg_in->rt->mc_masks[idx].allocd);
			continue;
                }
                assert_int_equal(0, mock_mc_chg_in->rt->mc_masks[idx].mc_destID);
                assert_int_equal(tt_dev8, mock_mc_chg_in->rt->mc_masks[idx].tt);
                assert_int_equal(0, mock_mc_chg_in->rt->mc_masks[idx].mc_mask);
                assert_false(mock_mc_chg_in->rt->mc_masks[idx].in_use);
                assert_false(mock_mc_chg_in->rt->mc_masks[idx].allocd);
        }


        //Check dom_table
        assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_mc_chg_in->rt->dom_table[0].rte_val);
        assert_false(mock_mc_chg_in->rt->dom_table[0].changed);

        dom_idx = (mock_mc_chg_in->mc_info.mc_destID & 0xFF00) >> 8;
        for (idx = 1; idx < RIO_DAR_RT_DOM_TABLE_SIZE; idx++) {
                if (idx == dom_idx && mock_mc_chg_in->mc_info.in_use) {
                        assert_int_equal(RIO_DSF_FIRST_MC_MASK + chg_idx, mock_mc_chg_in->rt->dom_table[idx].rte_val);
                        assert_true(mock_mc_chg_in->rt->dom_table[idx].changed);
                        continue;
                }
		if (idx < dom_idx) {
			assert_int_equal(RIO_DSF_RT_NO_ROUTE, mock_mc_chg_in->rt->dom_table[idx].rte_val);
	                assert_true(mock_mc_chg_in->rt->dom_table[idx].changed);
			continue;
		}
                assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->dom_table[idx].rte_val);
                assert_false(mock_mc_chg_in->rt->dom_table[idx].changed);
        }

        //Check dev_table
	for (idx = 0; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
		if (idx == dev_idx) {
                        assert_int_equal(mock_mc_chg_in->mc_mask_rte, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                        assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
                        continue;
                }
		if (idx < dev_idx) {
			assert_int_equal(RIO_DSF_FIRST_MC_MASK + idx, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                	assert_true(mock_mc_chg_in->rt->dev_table[idx].changed);
			continue;
		}
		assert_int_equal(mock_mc_chg_in->mc_info.mc_mask, mock_mc_chg_in->rt->dev_table[idx].rte_val);
                assert_false(mock_mc_chg_in->rt->dev_table[idx].changed);
	}
}

void rxs_change_allocd_mc_rt_test_success(bool dom)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
	rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_state_t              rt;
        uint32_t                    port, mc_idx, mc_mask_val, chg_rte_val;

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
            mock_init_in.rt        = &rt;
            memset(&rt, 0, sizeof(rt));

            assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

	    mock_alloc_in.rt = mock_init_in.rt;
            for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
		assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        	assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

		mock_chg_in.dom_entry = dom;
                mock_chg_in.idx       = mc_idx;
                mock_chg_in.rte_value = chg_rte_val;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, 
			&mock_chg_in, &mock_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

                mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
                mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
                if (dom) {
			if (!port) {
				break;
			}
                        mock_mc_chg_in.mc_info.mc_destID = (mc_idx << 8);
                }
                else {
                        mock_mc_chg_in.mc_info.mc_destID = mc_idx;
                }
                mock_mc_chg_in.mc_info.tt = tt_dev16;
                mock_mc_chg_in.mc_info.in_use = true;
                mock_mc_chg_in.mc_info.allocd = false;
                mock_mc_chg_in.mc_info.changed = false;
                mock_mc_chg_in.rt = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

                rxs_check_change_allocd_mc_rt_change(&mock_mc_chg_in);
            }

            mock_mc_chg_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;
            mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
            if (dom) {
		    if (!port) continue;
                    mock_mc_chg_in.mc_info.mc_destID = (port << 8);
            }
            else {
                    mock_mc_chg_in.mc_info.mc_destID = port;
            }
            mock_mc_chg_in.mc_info.tt = tt_dev16;
            mock_mc_chg_in.mc_info.in_use = true;
            mock_mc_chg_in.mc_info.allocd = false;
            mock_mc_chg_in.mc_info.changed = false;
            mock_mc_chg_in.rt = mock_init_in.rt;

            assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
            assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);
        }
}

void rxs_change_allocd_mc_rt_dom_hw_test_success(void **state)
{
        rxs_change_allocd_mc_rt_test_success(true);
        (void)state; // unused
}

void rxs_change_allocd_mc_rt_dev_hw_test_success(void **state)
{
        rxs_change_allocd_mc_rt_test_success(false);
        (void)state; // unused
}

void rxs_change_mc_rt_test_rt_null(void **state)
{
	rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
	rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_state_t              rt;

	rxs_init_mock_rt(&rt);

	mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

        assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 0x01;
        mock_mc_chg_in.mc_info.mc_destID = 0x01;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

void rxs_change_mc_rt_test_bad_mc_destID(void **state)
{
        rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_state_t              rt;

        mock_alloc_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

        assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 0x01;
        mock_mc_chg_in.mc_info.mc_destID = 0xFFFFFFFF;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

void rxs_change_mc_rt_test_bad_mc_mask(void **state)
{
	rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_state_t              rt;

        mock_alloc_in.rt = &rt;
        memset(&rt, 0, sizeof(rt));

        assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 0xFF000000;
        mock_mc_chg_in.mc_info.mc_destID = 0x01;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 0x000000FF;
        mock_mc_chg_in.mc_info.mc_destID = RIO_LAST_DEV8_DESTID + 1;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 0x000000FF;
        mock_mc_chg_in.mc_info.mc_destID = RIO_LAST_DEV16_DESTID + 1;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

	(void)state; // unused
}

void rxs_change_mc_rt_test_bad_tdev(void **state)
{
	rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_state_t              rt;

        mock_alloc_in.rt = &rt;
        memset(&rt, 0, sizeof(rt));

        assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 0x01;
        mock_mc_chg_in.mc_info.mc_destID = 0x0100;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_change_mc_rt_test_bad_mc_mask_rte(void **state)
{
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
	rio_rt_state_t              rt;

        mock_mc_chg_in.mc_mask_rte = 0x0FFF;
        mock_mc_chg_in.mc_info.mc_mask = 0x01;
        mock_mc_chg_in.mc_info.mc_destID = 0x01;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = &rt;
	memset(&rt, 0, sizeof(rt));

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_not_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_check_dealloc_mc_rt_change(rio_rt_initialize_in_t *mock_init_in, rio_rt_dealloc_mc_mask_in_t *mock_dealloc_in)
{
        uint32_t idx, dom_rte, dev_rte;

	//Check mc_mask	
        for (idx = 0; idx < RXS2448_MAX_MC_MASK; ++idx) {
 	    assert_true(mock_dealloc_in->rt->mc_masks[idx].changed);
	    assert_false(mock_dealloc_in->rt->mc_masks[idx].allocd);
            assert_false(mock_dealloc_in->rt->mc_masks[idx].in_use);
	    assert_int_equal(tt_dev8, mock_dealloc_in->rt->mc_masks[idx].tt);
	    assert_int_equal(0, mock_dealloc_in->rt->mc_masks[idx].mc_destID);
        }

	//Check dom_table
	for (dom_rte = 0; dom_rte < RIO_DAR_RT_DOM_TABLE_SIZE; dom_rte++) {
		if (dom_rte == 0) {
			assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
 			assert_false(mock_dealloc_in->rt->dom_table[dom_rte].changed);
 			continue;
 		}
		if (mock_init_in->set_on_port == RIO_ALL_PORTS) {
			assert_int_equal(mock_init_in->default_route_table_port, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
		} 
		else {
                	assert_int_equal(mock_dealloc_in->rt->default_route, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
		}
                assert_false(mock_dealloc_in->rt->dom_table[dom_rte].changed);
	}

	//Check dev_table
	for (dev_rte = 0; dev_rte < RIO_DAR_RT_DEV_TABLE_SIZE; dev_rte++) {
		if (mock_init_in->set_on_port == RIO_ALL_PORTS) {
                        assert_int_equal(mock_init_in->default_route_table_port, mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
                }
                else {
			assert_int_equal(mock_dealloc_in->rt->default_route, mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
		}
		assert_false(mock_dealloc_in->rt->dev_table[dev_rte].changed);
	}
}

void rxs_dealloc_mc_rt_test_success(void **state)
{
        rio_rt_initialize_in_t       mock_init_in;
        rio_rt_initialize_out_t      mock_init_out;
        rio_rt_alloc_mc_mask_in_t    mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t   mock_alloc_out;
	rio_rt_dealloc_mc_mask_in_t  mock_dealloc_in;
        rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
        rio_rt_state_t               rt;
        uint8_t                      port, mc_idx;

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
            mock_init_in.rt        = &rt;
            memset(&rt, 0, sizeof(rt));

            assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            mock_alloc_in.rt = mock_init_in.rt;
            for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
                assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
                assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
	    }

	    //One more alloc to be falied 
	    assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
            assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

            mock_dealloc_in.rt          = mock_alloc_in.rt;
	    for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
                mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;

            	assert_int_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
            	assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);
	    }
	    rxs_check_dealloc_mc_rt_change(&mock_init_in, &mock_dealloc_in);

	    //One more dealloc to be succceeded
	    mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK;

            assert_int_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
            assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

	    mock_alloc_in.rt = mock_dealloc_in.rt;
	    for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
		assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
                assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
            }

	    //One more alloc to be falied
	    assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
            assert_int_not_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
        }

        (void)state; // unused
}

void rxs_check_dealloc_change_mc_rt_change(rio_rt_initialize_in_t *mock_init_in, rio_rt_dealloc_mc_mask_in_t *mock_dealloc_in)
{
        uint32_t idx, chg_idx, dom_rte, dev_rte, dev_idx;

        chg_idx = MC_MASK_IDX_FROM_ROUTE(mock_dealloc_in->mc_mask_rte);

	//Check mc_mask
        for (idx = 0; idx < RXS2448_MAX_MC_MASK; ++idx) {
            if (idx <= chg_idx) {
                assert_true(mock_dealloc_in->rt->mc_masks[idx].changed);
            }
            else {
                assert_false(mock_dealloc_in->rt->mc_masks[idx].changed);
            }
            assert_false(mock_dealloc_in->rt->mc_masks[idx].in_use);
            assert_int_equal(tt_dev8, mock_dealloc_in->rt->mc_masks[idx].tt);
            assert_int_equal(0, mock_dealloc_in->rt->mc_masks[idx].mc_destID);
		assert_false(mock_dealloc_in->rt->mc_masks[idx].allocd);
        }

	//Check dom_table
        for (dom_rte = 0; dom_rte < RIO_DAR_RT_DOM_TABLE_SIZE; dom_rte++) {
                if (dom_rte == 0) {
                        assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
                        assert_false(mock_dealloc_in->rt->dom_table[dom_rte].changed);
                        continue;
                }

		if (mock_init_in->set_on_port && dom_rte <= chg_idx) {
			assert_int_equal(RIO_DSF_RT_NO_ROUTE, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
			assert_true(mock_dealloc_in->rt->dom_table[dom_rte].changed);
			continue;
		}

                if (mock_init_in->set_on_port == RIO_ALL_PORTS) {
                        assert_int_equal(mock_init_in->default_route_table_port, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
                }
                else {
                        assert_int_equal(mock_dealloc_in->rt->default_route, mock_dealloc_in->rt->dom_table[dom_rte].rte_val);
                }
                assert_false(mock_dealloc_in->rt->dom_table[dom_rte].changed);
        }

	//Check dev_table
	dev_idx = mock_dealloc_in->rt->mc_masks[chg_idx].mc_destID & 0x00FF;
        for (dev_rte = 0; dev_rte < RIO_DAR_RT_DEV_TABLE_SIZE; dev_rte++) {
		if (mock_init_in->set_on_port && dev_rte == dev_idx) {
			assert_int_equal(RIO_DSF_RT_NO_ROUTE, mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
			assert_true(mock_dealloc_in->rt->dev_table[dev_rte].changed);
			continue;
		}

                if (mock_init_in->set_on_port == RIO_ALL_PORTS) {
                        assert_int_equal(mock_init_in->default_route_table_port, mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
                }
                else {
                        assert_int_equal(mock_dealloc_in->rt->default_route, mock_dealloc_in->rt->dev_table[dev_rte].rte_val);
                }
                assert_false(mock_dealloc_in->rt->dev_table[dev_rte].changed);
        }
}

void rxs_dealloc_change_mc_rt_test_success(void **state)
{
        rio_rt_initialize_in_t       mock_init_in;
        rio_rt_initialize_out_t      mock_init_out;
        rio_rt_alloc_mc_mask_in_t    mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t   mock_alloc_out;
	rio_rt_change_rte_in_t       mock_chg_in;
        rio_rt_change_rte_out_t      mock_chg_out;
	rio_rt_change_mc_mask_in_t   mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t  mock_mc_chg_out;
        rio_rt_dealloc_mc_mask_in_t  mock_dealloc_in;
        rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
        rio_rt_state_t               rt;
        uint32_t                     port, mc_idx, i;
	uint32_t 		     chg_rte_val, mc_mask_val;

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
            mock_init_in.rt        = &rt;
            memset(&rt, 0, sizeof(rt));

            assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            for (i = 0; i < 2; i++) {
		mock_alloc_in.rt = mock_init_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
                	assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
	                assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);
		}

		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
			mock_chg_in.dom_entry = true;
                	mock_chg_in.idx       = mc_idx;
	                mock_chg_in.rte_value = chg_rte_val;
        	        mock_chg_in.rt        = mock_init_in.rt;

                	assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info,
                        	&mock_chg_in, &mock_chg_out));
	                assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        	        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
                	mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
                        if (!port) {
                                break;
                        }
                        mock_mc_chg_in.mc_info.mc_destID = (mc_idx << 8);
	                mock_mc_chg_in.mc_info.tt = tt_dev16;
        	        mock_mc_chg_in.mc_info.in_use = true;
                	mock_mc_chg_in.mc_info.allocd = false;
	                mock_mc_chg_in.mc_info.changed = false;
        	        mock_mc_chg_in.rt = mock_init_in.rt;

                	assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
	                assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);
		}

		mock_dealloc_in.rt          = mock_init_in.rt;
		for (mc_idx = 0; mc_idx < RXS2448_MAX_MC_MASK; mc_idx++) {
        	        mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mc_idx;

	                assert_int_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
        	        assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);
		}
		rxs_check_dealloc_change_mc_rt_change(&mock_init_in, &mock_dealloc_in);

		//One more dealloc to be succceeded
		mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK;

            	assert_int_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
            	assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);
            }
        }

        (void)state; // unused
}

void rxs_dealloc_mc_rt_null_test(void **state)
{
        rio_rt_dealloc_mc_mask_in_t  mock_dealloc_in;
        rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;

	mock_dealloc_in.mc_mask_rte = RIO_DSF_FIRST_MC_MASK;
        mock_dealloc_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

        (void)state; // unused
}

void rxs_dealloc_mc_rt_test_bad_mc_mask_rte(void **state)
{
        rio_rt_dealloc_mc_mask_in_t  mock_dealloc_in;
        rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
	rio_rt_state_t               rt;

        rxs_init_mock_rt(&rt);
	mock_dealloc_in.mc_mask_rte = RIO_DSF_BAD_MC_MASK;
        mock_dealloc_in.rt = &rt;

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

	mock_dealloc_in.mc_mask_rte = RIO_DSF_MAX_MC_MASK;
        mock_dealloc_in.rt = &rt;

        assert_int_not_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
        assert_int_not_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

        (void)state; // unused
}
/*
void rxs_check_set_changed_regs_change(uint32_t port, rio_rt_change_rte_in_t *mock_chg_in)
{
	uint32_t chk_port, rte_num, mc_idx;
	uint32_t dev_out, dom_out, mc_mask_out;

	for (chk_port = 0; chk_port < RXS2448_MAX_PORTS; chk_port++) {
                rxs_reg_dev_dom(port, 0, &dom_out, &dev_out);
		if (port == RIO_ALL_PORTS) {
			assert_int_equal(0, dom_out);
	                assert_int_equal(0, dev_out);
		}
		else {
	                assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, dom_out);
        	        assert_int_equal(mock_chg_in->rte_value, dev_out);
		}

		if (chk_port == port) {
                   	for (rte_num = 1; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                       		rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                       		assert_int_equal(mock_chg_in->rte_value, dom_out);
                       		assert_int_equal(mock_chg_in->rte_value, dev_out);
                   	}
		}
		else {
                   for (rte_num = 1; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                       rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                       assert_int_equal(0, dom_out);
                       assert_int_equal(0, dev_out);
                   }
                }

                for (mc_idx = 0; mc_idx < RIO_DSF_MAX_MC_MASK; mc_idx++) {
                    rxs_reg_mc_mask(chk_port, mc_idx, &mc_mask_out);
                    assert_int_equal(0, mc_mask_out);
                }
	}
}

void rxs_check_set_changed_rt_change(rio_rt_set_changed_in_t *mock_set_chg_in, rio_rt_change_rte_in_t *mock_chg_in)
{
        uint32_t idx;

	//Check dev_table
	for (idx = 0; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
            assert_int_equal(mock_chg_in->rte_value, mock_set_chg_in->rt->dev_table[idx].rte_val);
            assert_false(mock_set_chg_in->rt->dev_table[idx].changed);
        }

	//Check dom_table
        assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_set_chg_in->rt->dom_table[0].rte_val);
        assert_true(!mock_set_chg_in->rt->dom_table[0].changed);

        for (idx = 1; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
            assert_int_equal(mock_chg_in->rte_value, mock_set_chg_in->rt->dom_table[idx].rte_val);
            assert_false(mock_set_chg_in->rt->dom_table[idx].changed);
        }
}

void rxs_set_changed_rt_test_success(bool dom)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
	rio_rt_set_changed_in_t     mock_set_chg_in;
        rio_rt_set_changed_out_t    mock_set_chg_out;
        rio_rt_state_t              rt;
        uint32_t                    port;
        uint32_t                    chg_rte_val, rte_num;
        uint32_t                    rte_num_upb = (dom)?RIO_DAR_RT_DOM_TABLE_SIZE:RIO_DAR_RT_DEV_TABLE_SIZE;

        for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
	    init_mock_rxs_reg();
            for (rte_num = 0; rte_num < rte_num_upb; rte_num++) {
                rxs_init_mock_rt(&rt);

                if (RXS2448_MAX_PORTS == port) {
                        mock_init_in.set_on_port = RIO_ALL_PORTS;
                        mock_init_in.default_route =
                                                RIO_DSF_RT_NO_ROUTE;
                        mock_init_in.default_route_table_port =
                                        RIO_DSF_RT_USE_DEFAULT_ROUTE;
                        chg_rte_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
                } else {
                        mock_init_in.set_on_port = port;
                        mock_init_in.default_route = port;
                        mock_init_in.default_route_table_port = port;
                        chg_rte_val = port;
                }
                mock_init_in.update_hw = true;
                mock_init_in.rt        = &rt;
                memset(&rt, 0, sizeof(rt));

                assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
                assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

                mock_chg_in.dom_entry = dom;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = chg_rte_val;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info,
                        &mock_chg_in, &mock_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

                rxs_check_change_rte_rt_change(rte_num,
                        mock_init_in.default_route_table_port, &mock_chg_in);

		mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
                mock_set_chg_in.rt = mock_chg_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, 
			&mock_set_chg_in, &mock_set_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

		rxs_check_set_changed_rt_change(&mock_set_chg_in, &mock_chg_in);
		rxs_check_set_changed_regs_change(mock_init_in.set_on_port, &mock_chg_in);
            }
        }
}

void rxs_set_changed_rt_dom_hw_test_success(void **state)
{
        rxs_set_changed_rt_test_success(true);
        (void)state; // unused
}

void rxs_set_changed_rt_dev_hw_test_success(void **state)
{
        rxs_set_changed_rt_test_success(false);
        (void)state; // unused
}

void rxs_set_changed_rt_dev_hw_bad_rt_test(void **state)
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

void rxs_set_changed_rt_dev_hw_bad_port_test(void **state)
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

void rxs_set_changed_rt_dev_hw_bad_default_route_test(void **state)
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

void rxs_check_set_all_rt_change(rio_rt_set_all_in_t *mock_set_all_in, rio_rt_change_rte_in_t *mock_chg_in)
{
        uint32_t idx;

        //Check dev_table
        for (idx = 0; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
		assert_int_equal(mock_chg_in->rte_value, mock_set_all_in->rt->dev_table[idx].rte_val);
		assert_false(mock_set_all_in->rt->dev_table[idx].changed);
	}

        //Check dom_table
	assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, mock_set_all_in->rt->dom_table[0].rte_val);
	assert_true(!mock_set_all_in->rt->dom_table[0].changed);

	for (idx = 1; idx < RIO_DAR_RT_DEV_TABLE_SIZE; idx++) {
		assert_int_equal(mock_chg_in->rte_value, mock_set_all_in->rt->dom_table[idx].rte_val);
		assert_false(mock_set_all_in->rt->dom_table[idx].changed);
        }
}

void rxs_set_all_rt_test_success(bool dom)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        rio_rt_set_all_in_t         mock_set_all_in;
        rio_rt_set_all_out_t        mock_set_all_out;
        rio_rt_state_t              rt;
        uint32_t                    port;
        uint32_t                    chg_rte_val, rte_num;
        uint32_t                    rte_num_upb = (dom)?RIO_DAR_RT_DOM_TABLE_SIZE:RIO_DAR_RT_DEV_TABLE_SIZE;

        for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
	    init_mock_rxs_reg();
            for (rte_num = 0; rte_num < rte_num_upb; rte_num++) {
                rxs_init_mock_rt(&rt);

                if (RXS2448_MAX_PORTS == port) {
                        mock_init_in.set_on_port = RIO_ALL_PORTS;
                        mock_init_in.default_route =
                                                RIO_DSF_RT_NO_ROUTE;
                        mock_init_in.default_route_table_port =
                                        RIO_DSF_RT_USE_DEFAULT_ROUTE;
                        chg_rte_val = RIO_DSF_RT_USE_DEFAULT_ROUTE;
                } else {
                        mock_init_in.set_on_port = port;
                        mock_init_in.default_route = port;
                        mock_init_in.default_route_table_port = port;
                        chg_rte_val = port;
                }
                mock_init_in.update_hw = true;
                mock_init_in.rt        = &rt;
                memset(&rt, 0, sizeof(rt));

                assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
                assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

                mock_chg_in.dom_entry = dom;
                mock_chg_in.idx       = rte_num;
                mock_chg_in.rte_value = chg_rte_val;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info,
                        &mock_chg_in, &mock_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

                rxs_check_change_rte_rt_change(rte_num,
                        mock_init_in.default_route_table_port, &mock_chg_in);

                mock_set_all_in.set_on_port = mock_init_in.set_on_port;
                mock_set_all_in.rt = mock_chg_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info,
                        &mock_set_all_in, &mock_set_all_out));
                assert_int_equal(RIO_SUCCESS, mock_set_all_out.imp_rc);

                rxs_check_set_all_rt_change(&mock_set_all_in, &mock_chg_in);
            }
        }
}

void rxs_set_all_rt_dom_hw_test_success(void **state)
{
        rxs_set_all_rt_test_success(true);
        (void)state; // unused
}

void rxs_set_all_rt_dev_hw_test_success(void **state)
{
        rxs_set_all_rt_test_success(false);
        (void)state; // unused
}

void rxs_set_all_rt_null_test(void **state)
{
        rio_rt_set_all_in_t         mock_set_in;
        rio_rt_set_all_out_t        mock_set_out;

        mock_set_in.set_on_port = 0;
        mock_set_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_out.imp_rc);

	mock_set_in.set_on_port = RIO_ALL_PORTS;
        mock_set_in.rt = NULL;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        (void)state; // unused
}

void rxs_set_all_rt_dev_hw_bad_port_test(void **state)
{
	rio_rt_set_all_in_t         mock_set_all_in;
        rio_rt_set_all_out_t        mock_set_all_out;
        rio_rt_state_t              rt;

        rxs_init_mock_rt(&rt);
        mock_set_all_in.set_on_port = RXS2448_MAX_PORTS+1;
        mock_set_all_in.rt = &rt;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_all_in, &mock_set_all_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_all_out.imp_rc);

        (void)state; // unused
}

void rxs_set_all_rt_dev_hw_bad_default_route_test(void **state)
{
	rio_rt_set_all_in_t         mock_set_all_in;
        rio_rt_set_all_out_t        mock_set_all_out;
        rio_rt_state_t              rt;

        rxs_init_mock_rt(&rt);
        mock_set_all_in.set_on_port = 0;
        mock_set_all_in.rt = &rt;
        mock_set_all_in.rt->default_route = RIO_DSF_RT_USE_DEVICE_TABLE;

        assert_int_not_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_all_in, &mock_set_all_out));
        assert_int_not_equal(RIO_SUCCESS, mock_set_all_out.imp_rc);

        (void)state; // unused
}

void rxs_rio_rt_test_success(bool dom)
{
        rio_rt_initialize_in_t       mock_init_in;
        rio_rt_initialize_out_t      mock_init_out;
        rio_rt_set_all_in_t          mock_set_all_in;
        rio_rt_set_all_out_t         mock_set_all_out;
        rio_rt_change_rte_in_t       mock_chg_in;
        rio_rt_change_rte_out_t      mock_chg_out;
        rio_rt_set_changed_in_t      mock_set_chg_in;
        rio_rt_set_changed_out_t     mock_set_chg_out;
        rio_rt_alloc_mc_mask_in_t    mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t   mock_alloc_out;
        rio_rt_dealloc_mc_mask_in_t  mock_dealloc_in;
        rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
        rio_rt_change_mc_mask_in_t   mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t  mock_mc_chg_out;
        rio_rt_state_t               rt;
        uint32_t                     port, mc_mask_val;
        uint32_t                     chg_rte_val, mc_idx;
        uint32_t                     mc_idx_upb = (dom)?RIO_DAR_RT_DOM_TABLE_SIZE:RIO_DAR_RT_DEV_TABLE_SIZE;

        for (port = 0; port <= RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg();
            for (mc_idx = 0; mc_idx < mc_idx_upb; mc_idx++) {
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
                mock_init_in.rt        = &rt;
                memset(&rt, 0, sizeof(rt));

                assert_int_equal(RIO_SUCCESS,
                        rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in,
                                                        &mock_init_out));
                assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

                mock_chg_in.dom_entry = dom;
                mock_chg_in.idx       = mc_idx;
                mock_chg_in.rte_value = chg_rte_val;
                mock_chg_in.rt        = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info,
                        &mock_chg_in, &mock_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);
         
		mock_alloc_in.rt = mock_chg_in.rt;
		assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, 
			&mock_alloc_in, &mock_alloc_out));
                assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

		mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
                mock_mc_chg_in.mc_info.mc_mask = mc_mask_val;
                if (dom) {
                        if (!port) {
                                break;
                        }
                        mock_mc_chg_in.mc_info.mc_destID = (mc_idx << 8);
                }
                else {
                        mock_mc_chg_in.mc_info.mc_destID = mc_idx;
                }
                mock_mc_chg_in.mc_info.tt = tt_dev16;
                mock_mc_chg_in.mc_info.in_use = true;
                mock_mc_chg_in.mc_info.allocd = false;
                mock_mc_chg_in.mc_info.changed = false;
                mock_mc_chg_in.rt = mock_init_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, 
			&mock_mc_chg_in, &mock_mc_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

		mock_dealloc_in.mc_mask_rte = mock_mc_chg_in.mc_mask_rte;
                mock_dealloc_in.rt          = mock_mc_chg_in.rt;

		assert_int_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, 
			&mock_dealloc_in, &mock_dealloc_out));
                assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

		mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
                mock_set_chg_in.rt = mock_chg_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info,
                        &mock_set_chg_in, &mock_set_chg_out));
                assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

                mock_set_all_in.set_on_port = mock_set_chg_in.set_on_port;
                mock_set_all_in.rt = mock_set_chg_in.rt;

                assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info,
                        &mock_set_all_in, &mock_set_all_out));
                assert_int_equal(RIO_SUCCESS, mock_set_all_out.imp_rc);
            }
        }
}

void rxs_routing_table_01_test(void **state)
{
	rxs_rio_rt_test_success(true);
	(void)state; // unused
}

void rxs_routing_table_02_test(void **state)
{
        rxs_rio_rt_test_success(false);
        (void)state; // unused
}

void check_set_chg_regs(uint32_t port, rio_rt_change_rte_in_t mock_chg_in, bool isdom)
{
        uint32_t dev_out, dom_out, mc_mask_out;
        uint32_t chk_port, rte_num, mask_num;

        if (port == RIO_ALL_PORTS) {
           rte_num = 0;
           rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
           assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, dom_out);
           assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);

           rte_num++;
           rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
           assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
           assert_int_equal(mock_chg_in.rte_value, dev_out);

           for (rte_num = 2; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
               rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
               assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
               assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);
           }

           for (chk_port = 0; chk_port < RXS2448_MAX_PORTS; chk_port++) {
               rte_num = 0;
               rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
               assert_int_equal(0x0200, dom_out);
               assert_int_equal(0x0301, dev_out);

               for (rte_num = 1; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                   if (rte_num % 4 == 0) {
                      rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                      assert_int_equal(0x0301, dom_out);
                      if (rte_num == 4) {
                         assert_int_equal(0x01, dev_out);
                      }
                      else {
                         assert_int_equal(0x0301, dev_out);
                      }
                   }
                   else {
                      rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                      assert_int_equal(0, dom_out);
                      assert_int_equal(0, dev_out);
                   }
               }

               for (mask_num = 0; mask_num < RIO_DSF_MAX_MC_MASK; mask_num++) {
                   rxs_reg_mc_mask(chk_port, mask_num, &mc_mask_out);
                   assert_int_equal(0, mc_mask_out);
               }
           }
        }
        else {
            for (chk_port = 0; chk_port < RXS2448_MAX_PORTS; chk_port++) {
                if (chk_port == port) {
                   rte_num = 0;
                   rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                   assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, dom_out);
                   assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);

                   rte_num++;
                   rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                   if (isdom) {
                      assert_int_equal(mock_chg_in.rte_value, dom_out);
                   } 
                   else {
                      assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
                   }
                   assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);

                   for (rte_num = 2; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                       rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                       assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
                       assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);
                   }
                }
                else {
                   for (rte_num = 0; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                       rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                       assert_int_equal(0, dom_out);
                       assert_int_equal(0, dev_out);
                   }
                }
                for (mask_num = 0; mask_num < RIO_DSF_MAX_MC_MASK; mask_num++) {
                    rxs_reg_mc_mask(chk_port, mask_num, &mc_mask_out);
                    assert_int_equal(0, mc_mask_out);
                }
            }
        }
}

void check_set_all_regs(uint32_t port, rio_rt_change_rte_in_t mock_chg_in, bool isdom)
{
        uint32_t dev_out, dom_out, mc_mask_out;
        uint32_t chk_port, rte_num, mask_num;

        if (port == RIO_ALL_PORTS) {
           rte_num = 0;
           rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
           assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, dom_out);
           assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);

           rte_num++;
           rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
           if (isdom) {
              assert_int_equal(mock_chg_in.rte_value, dom_out);
              assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);
           } 
           else {
              assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
              assert_int_equal(mock_chg_in.rte_value, dev_out);
           }

           for (rte_num = 2; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
               rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
               assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
               assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);
           }

           for (chk_port = 0; chk_port < RXS2448_MAX_PORTS; chk_port++) {
               rte_num = 0;
               rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
               assert_int_equal(0x0200, dom_out);
               assert_int_equal(0x0301, dev_out);

               for (rte_num = 1; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                   if (rte_num % 4 == 0) {
                      rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                      if (isdom && rte_num == 4) {
                         assert_int_equal(0x01, dom_out);
                       }
                       else {
                          assert_int_equal(0x0301, dom_out);
                       }

                       if (rte_num == 4 && !isdom) {
                          assert_int_equal(0x01, dev_out);
                       }
                       else {
                          assert_int_equal(0x0301, dev_out);
                       }
                   }
                   else {
                      rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                      assert_int_equal(0, dom_out);
                      assert_int_equal(0, dev_out);
                   }
               }
               for (mask_num = 0; mask_num < RIO_DSF_MAX_MC_MASK; mask_num++) {
                   rxs_reg_mc_mask(chk_port, mask_num, &mc_mask_out);
                   assert_int_equal(0, mc_mask_out);
               }
           }
        }
        else {
            for (chk_port = 0; chk_port < RXS2448_MAX_PORTS; chk_port++) {
                if (chk_port == port) {
                   rte_num = 0;
                   rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                   assert_int_equal(RIO_DSF_RT_USE_DEVICE_TABLE, dom_out);
                   assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);

                   rte_num++;
                   rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                   if (isdom) {
                      assert_int_equal(mock_chg_in.rte_value, dom_out);
                   }
                   else {
                      assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
                   }
                   assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);

                   for (rte_num = 2; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                       rxs_reg_dev_dom(port, rte_num, &dom_out, &dev_out);
                       assert_int_equal(RIO_DSF_RT_NO_ROUTE, dom_out);
                       assert_int_equal(RIO_DSF_RT_NO_ROUTE, dev_out);
                   }
                }
                else {
                   for (rte_num = 0; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
                       rxs_reg_dev_dom(chk_port, rte_num, &dom_out, &dev_out);
                       assert_int_equal(0, dom_out);
                       assert_int_equal(0, dev_out);
                   }
                }
                for (mask_num = 0; mask_num < RIO_DSF_MAX_MC_MASK; mask_num++) {
                    rxs_reg_mc_mask(chk_port, mask_num, &mc_mask_out);
                    assert_int_equal(0, mc_mask_out);
                }
            }
        }
}


void rxs_routing_table_03_hw_test(void **state)
{
        rio_rt_initialize_in_t       mock_init_in;
        rio_rt_initialize_out_t      mock_init_out;
        rio_rt_set_all_in_t          mock_set_in;
        rio_rt_set_all_out_t         mock_set_out;
        rio_rt_change_rte_in_t       mock_chg_in;
        rio_rt_change_rte_out_t      mock_chg_out;
        rio_rt_set_changed_in_t      mock_set_chg_in;
        rio_rt_set_changed_out_t     mock_set_chg_out;
        rio_rt_alloc_mc_mask_in_t    mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t   mock_alloc_out;
        rio_rt_change_mc_mask_in_t   mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t  mock_mc_chg_out;
        rio_rt_dealloc_mc_mask_in_t  mock_dealloc_in;
        rio_rt_dealloc_mc_mask_out_t mock_dealloc_out;
        rio_rt_state_t               rt;
        uint32_t temp;
        uint8_t port;

        for (port = 0; port < RXS2448_MAX_PORTS; port++) {
            init_mock_rxs_reg();
            rxs_init_mock_rt(&rt);
            assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
            mock_init_in.set_on_port = port;
            mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
            mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
            mock_init_in.update_hw = true;
            mock_init_in.rt        = &rt;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
            assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

            mock_chg_in.dom_entry = true;
            mock_chg_in.idx       = 1;
            mock_chg_in.rte_value = 1;
            mock_chg_in.rt        = mock_init_in.rt;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
            assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

            mock_alloc_in.rt = mock_chg_in.rt;

            assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
            assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

            mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
            mock_mc_chg_in.mc_info.mc_mask = 0x02;
            mock_mc_chg_in.mc_info.mc_destID = 0x0202;
            mock_mc_chg_in.mc_info.tt = tt_dev16;
            mock_mc_chg_in.mc_info.in_use = true;
            mock_mc_chg_in.mc_info.allocd = false;
            mock_mc_chg_in.mc_info.changed = false;
            mock_mc_chg_in.rt = mock_alloc_in.rt;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
            assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

            mock_dealloc_in.mc_mask_rte = mock_mc_chg_in.mc_mask_rte;
            mock_dealloc_in.rt          = mock_mc_chg_in.rt;

            assert_int_equal(RIO_SUCCESS, DSF_rio_rt_dealloc_mc_mask(&mock_dev_info, &mock_dealloc_in, &mock_dealloc_out));
            assert_int_equal(RIO_SUCCESS, mock_dealloc_out.imp_rc);

            mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
            mock_set_chg_in.rt = mock_dealloc_in.rt;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
            assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

            mock_set_in.set_on_port = mock_set_chg_in.set_on_port;
            mock_set_in.rt = mock_set_chg_in.rt;

            assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
            assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);
        }

        (void)state; // unused
}

void rxs_probe_rt_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_set_all_in_t         mock_set_in;
        rio_rt_set_all_out_t        mock_set_out;
        rio_rt_probe_in_t           mock_probe_in;
        rio_rt_probe_out_t          mock_probe_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 1;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_probe_in.probe_on_port = port;
	mock_probe_in.tt = tt_dev8;
	mock_probe_in.destID = did;
	mock_probe_in.rt = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        (void)state; // unused
}

void rxs_probe_all_rt_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_set_all_in_t         mock_set_in;
        rio_rt_set_all_out_t        mock_set_out;
        rio_rt_probe_all_in_t       mock_probe_all_in;
	rio_rt_probe_all_out_t      mock_probe_all_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_probes_rt_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_set_all_in_t         mock_set_in;
        rio_rt_set_all_out_t        mock_set_out;
        rio_rt_probe_in_t           mock_probe_in;
        rio_rt_probe_out_t          mock_probe_out;
        rio_rt_probe_all_in_t       mock_probe_all_in;
        rio_rt_probe_all_out_t      mock_probe_all_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 1;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_probe_in.probe_on_port = port;
        mock_probe_in.tt = tt_dev8;
        mock_probe_in.destID = did;
        mock_probe_in.rt = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_probe_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_routing_table_04_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_set_all_in_t         mock_set_in;
        rio_rt_set_all_out_t        mock_set_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        rio_rt_set_changed_in_t     mock_set_chg_in;
        rio_rt_set_changed_out_t    mock_set_chg_out;
        rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_probe_in_t           mock_probe_in;
        rio_rt_probe_out_t          mock_probe_out;
        rio_rt_probe_all_in_t       mock_probe_all_in;
        rio_rt_probe_all_out_t      mock_probe_all_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 2;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = did;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = true;
        mock_mc_chg_in.mc_info.changed = true;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_mc_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_probe_in.probe_on_port = port;
        mock_probe_in.tt = tt_dev8;
        mock_probe_in.destID = did;
        mock_probe_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_probe_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(mock_chg_in.rte_value, mock_set_chg_in.rt->dom_table[mock_chg_in.idx].rte_val);
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_routing_table_05_test(void **state)
{
        rio_rt_initialize_in_t      mock_init_in;
        rio_rt_initialize_out_t     mock_init_out;
        rio_rt_set_all_in_t         mock_set_in;
        rio_rt_set_all_out_t        mock_set_out;
        rio_rt_change_rte_in_t      mock_chg_in;
        rio_rt_change_rte_out_t     mock_chg_out;
        rio_rt_set_changed_in_t     mock_set_chg_in;
        rio_rt_set_changed_out_t    mock_set_chg_out;
        rio_rt_alloc_mc_mask_in_t   mock_alloc_in;
        rio_rt_alloc_mc_mask_out_t  mock_alloc_out;
        rio_rt_change_mc_mask_in_t  mock_mc_chg_in;
        rio_rt_change_mc_mask_out_t mock_mc_chg_out;
        rio_rt_probe_in_t           mock_probe_in;
        rio_rt_probe_out_t          mock_probe_out;
        rio_rt_probe_all_in_t       mock_probe_all_in;
        rio_rt_probe_all_out_t      mock_probe_all_out;
        rio_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 2;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = RIO_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, DSF_rio_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = did;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = true;
        mock_mc_chg_in.mc_info.changed = true;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_mc_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_probe_in.probe_on_port = port;
        mock_probe_in.tt = tt_dev8;
        mock_probe_in.destID = did;
        mock_probe_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_probe_in.rt;

        assert_int_equal(RIO_SUCCESS, rxs_rio_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(mock_chg_in.rte_value, mock_set_chg_in.rt->dom_table[mock_chg_in.idx].rte_val);
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}
*/

int main(int argc, char** argv)
{
	const struct CMUnitTest tests[] = {
                cmocka_unit_test(macros_test),
                cmocka_unit_test_setup_teardown(assumptions_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_bad_ptrs, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_bad_p_ctrs, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_bad_ptl_1, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_bad_ptl_2, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_bad_ptl_3, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test_good_ptl, setup, NULL),

                cmocka_unit_test_setup_teardown(rxs_cfg_dev_ctrs_test_per_port, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_cfg_dev_ctrs_test_all_ports, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_cfg_dev_ctrs_test_default, setup, NULL),

                cmocka_unit_test_setup_teardown(rxs_read_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_read_dev_ctrs_test_bad_parms1, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_read_dev_ctrs_test_bad_parms2, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_read_dev_ctrs_test, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_init_cfg_read_dev_ctrs_test, setup, NULL),

                cmocka_unit_test_setup_teardown(rxs_init_rt_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test_success_hw, setup, NULL),

                cmocka_unit_test_setup_teardown(rxs_init_rt_null_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_null_update_hw_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test_port_rte, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test_all_port_mc_mask, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test_bad_default_route, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test_bad_default_route_table, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test_bad_port, setup, NULL),

		cmocka_unit_test_setup_teardown(rxs_change_rte_rt_test_dom_hw_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_change_rte_rt_test_dev_hw_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_change_rte_rt_null_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_rte_rt_bad_rte_test, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_change_rte_rt_bad_rte_dev_test, setup, NULL),

		cmocka_unit_test_setup_teardown(rxs_alloc_mc_rt_test_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_alloc_mc_rt_null_test, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_alloc_mc_rt_bad_in_use_allocd_test, setup, NULL),

		cmocka_unit_test_setup_teardown(rxs_change_mc_rt_dom_inuse_hw_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_dev_inuse_hw_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_dom_hw_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_dev_hw_test_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_change_mc_rt_test_rt_null, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_test_bad_mc_destID, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_test_bad_mc_mask, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_test_bad_tdev, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_test_bad_mc_mask_rte, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_change_allocd_mc_rt_dom_hw_test_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_change_allocd_mc_rt_dev_hw_test_success, setup, NULL),

		cmocka_unit_test_setup_teardown(rxs_dealloc_mc_rt_test_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_dealloc_change_mc_rt_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_dealloc_mc_rt_null_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_dealloc_mc_rt_test_bad_mc_mask_rte, setup, teardown),

/*		cmocka_unit_test_setup_teardown(rxs_set_changed_rt_dom_hw_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_changed_rt_dev_hw_test_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_set_changed_rt_dev_hw_bad_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_changed_rt_dev_hw_bad_port_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_changed_rt_dev_hw_bad_default_route_test, setup, NULL),

		cmocka_unit_test_setup_teardown(rxs_set_all_rt_dom_hw_test_success, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_all_rt_dev_hw_test_success, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_set_all_rt_null_test, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_set_all_rt_dev_hw_bad_port_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_all_rt_dev_hw_bad_default_route_test, setup, NULL),

                cmocka_unit_test_setup_teardown(rxs_routing_table_01_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_02_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_03_hw_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_04_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_05_test, setup, NULL),

                cmocka_unit_test_setup_teardown(rxs_probe_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_probe_all_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_probes_rt_test, setup, teardown),
*/
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
