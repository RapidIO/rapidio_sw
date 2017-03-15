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
#include "src/RXS_DeviceDriver.h"
#include "rio_standard.h"
#include "rio_ecosystem.h"
#include "tok_parse.h"
#include "libcli.h"
#include "rapidio_mport_mgmt.h"

#include "RXS2448.h"
#include "src/RXS_RT.c"
#include "src/RXS_EM.c"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RXSx_DAR_WANTED

static void rsx_not_supported_test(void **state)
{
	(void)state; // not used
}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++;// not used

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(rsx_not_supported_test)};
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
	hc_t hc;
	did_reg_t did_reg_val;
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
	bool got_did_reg_val = false;

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
			if (tok_parse_did(parm, &st.did_reg_val, 0)) {
				printf("\nFailed tok_parse_did\n");
				goto fail;
			}
			st.real_hw = true;
			got_did_reg_val = true;
			break;
		default:
			printf("\nUnknown option, options are -m -h -d.\n");
			goto fail;
			break;
		}
	}

	if ((st.real_hw) && !(got_mport && got_hc && got_did_reg_val)) {
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

// The behavior of the performance optimization register access
// must be overridden when registers are mocked.
//
// The elegant way to do this, while retaining transparency in the code,
// is to add 1 to the defined register offset to compute the mocked register
// offset.  This prevents the standard performance optimization register
// support from recognizing the register and updating the poregs array.

#define MOCK_REG_ADDR(x) (x | 1)

static uint32_t rxs_get_poreg_idx(DAR_DEV_INFO_t *dev_info, uint32_t offset)
{
        return DAR_get_poreg_idx(dev_info, MOCK_REG_ADDR(offset));
}

static uint32_t rxs_expect_poreg_idx(DAR_DEV_INFO_t *dev_info, uint32_t offset)
{
        uint32_t idx =  DAR_get_poreg_idx(dev_info, MOCK_REG_ADDR(offset));

	if (DAR_POREG_BAD_IDX == idx) {
		if (DEBUG_PRINTF) {
			printf("\nMissing offset is 0x%x\n", offset);
		}
		assert_true(false);
	}
	assert_int_not_equal(idx, DAR_POREG_BAD_IDX);
	return idx;
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
		if (DAR_POREG_BAD_IDX == idx) {
			if (DEBUG_PRINTF) {
				printf("\nMissing offset is 0x%x\n", offset);
			}
			assert_true(st.real_hw);
			idx = 0;
			goto exit;
		}
		rc = RIO_SUCCESS;
		*readdata = dev_info->poregs[idx].data;
		goto exit;
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
		rc = riomp_mgmt_rcfg_read(st.mp_h, st.did_reg_val, st.hc,
				offset, 4, readdata);
	}
exit:
	return rc;
}

static void update_mc_masks(DAR_DEV_INFO_t *dev_info,
			rio_port_t port, uint32_t idx, uint32_t new_mask)
{
	uint32_t reg_idx;

	// Update register values for both the set and
	// clear mask.

	reg_idx = rxs_expect_poreg_idx(dev_info,
				RXS_SPX_MC_Y_S_CSR(port, idx));
	dev_info->poregs[reg_idx].data = new_mask;

	reg_idx = rxs_expect_poreg_idx(dev_info,
					RXS_SPX_MC_Y_C_CSR(port, idx));
	dev_info->poregs[reg_idx].data = new_mask;
}

static void update_plm_status(DAR_DEV_INFO_t *dev_info,
				uint32_t plm_mask,
				uint32_t plm_events,
				rio_port_t port)
{
	unsigned int st_idx;
	unsigned int en_idx;
	const uint32_t pp_mask = 1 << port;
	uint32_t pp_stat = 0;
	uint32_t rst_stat;
	uint32_t rst_mask;

	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_PLM_SPX_STAT(port));

	dev_info->poregs[st_idx].data &= ~plm_mask;
	dev_info->poregs[st_idx].data |= plm_events;
	plm_events = dev_info->poregs[st_idx].data;

	// Update per-port interrupt status
	en_idx  = rxs_expect_poreg_idx(dev_info, RXS_PLM_SPX_ALL_INT_EN(port));
	if (plm_events && dev_info->poregs[en_idx].data) {
		pp_stat = pp_mask;
	}

	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_INT_PORT_STAT);
	dev_info->poregs[st_idx].data &= ~pp_mask;
	dev_info->poregs[st_idx].data |= pp_stat;

	pp_stat = dev_info->poregs[st_idx].data;

	// Update global interrupt status for PORT
	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_INT_STAT);
	if (pp_stat) {
		dev_info->poregs[st_idx].data &= ~RXS_EM_INT_STAT_PORT;
	} else {
		dev_info->poregs[st_idx].data |= RXS_EM_INT_STAT_PORT;
	}

	// Update per-port port-write status
	pp_stat = 0;
	en_idx  = rxs_expect_poreg_idx(dev_info, RXS_SPX_CTL(port));
	if ((RXS_SPX_ERR_STAT_PORT_W_DIS & dev_info->poregs[en_idx].data) &&
								plm_events ) {
		pp_stat = pp_mask;
	}

	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_PW_PORT_STAT);
	dev_info->poregs[st_idx].data &= ~pp_mask;
	dev_info->poregs[st_idx].data |= pp_stat;

	pp_stat = dev_info->poregs[st_idx].data;

	// Update global port-write status for PORT
	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_PW_STAT);
	if (pp_stat) {
		dev_info->poregs[st_idx].data &= ~RXS_EM_PW_STAT_PORT;
	} else {
		dev_info->poregs[st_idx].data |= RXS_EM_PW_STAT_PORT;
	}

	// Update per-port reset request status...
	pp_stat = 0;
	if (plm_events & (RXS_PLM_SPX_STAT_RST_REQ| RXS_PLM_SPX_STAT_PRST_REQ)){
		pp_stat = pp_mask;
	}

	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_RST_PORT_STAT);
	dev_info->poregs[st_idx].data &= ~pp_mask;
	dev_info->poregs[st_idx].data |= pp_stat;

	rst_stat = dev_info->poregs[st_idx].data;

	// Update top-level reset request interrupt status...
	en_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_RST_INT_EN);
	rst_mask = dev_info->poregs[en_idx].data;

	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_INT_STAT);
	if (rst_stat & rst_mask) {
		dev_info->poregs[st_idx].data |= RXS_EM_INT_STAT_RCS;
	} else {
		dev_info->poregs[st_idx].data &= ~ RXS_EM_INT_STAT_RCS;
	}

	// Update top-level reset request port-write status...
	en_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_RST_PW_EN);
	rst_mask = dev_info->poregs[en_idx].data;

	st_idx  = rxs_expect_poreg_idx(dev_info, RXS_EM_PW_STAT);
	if (rst_stat & rst_mask) {
		dev_info->poregs[st_idx].data |= RXS_EM_PW_STAT_RCS;
	} else {
		dev_info->poregs[st_idx].data &= ~ RXS_EM_PW_STAT_RCS;
	}
}

static void emulate_emhs_pp_reg(DAR_DEV_INFO_t *dev_info,
				uint32_t offset,
				uint32_t writedata,
				uint32_t idx)
{
	rio_port_t port = (offset - RXS_SPX_ERR_DET(0)) / RXS_SPX_PP_OSET;
	uint32_t plm_mask = RXS_PLM_SPX_STAT_DLT |
			RXS_PLM_SPX_STAT_OK_TO_UNINIT |
			RXS_PLM_SPX_STAT_LINK_INIT;
	uint32_t plm_events = 0;

	// Might need to enhance this if, as shown in the docs,
	// PLM status bits are gated by RXS_SPX_RATE_EN(port) settings
	if (RXS_SPX_ERR_DET(port) == offset) {
		dev_info->poregs[idx].data = writedata;

		if (writedata & RXS_SPX_ERR_DET_DLT) {
			plm_events |= RXS_PLM_SPX_STAT_DLT;
		}
		if (writedata & RXS_SPX_ERR_DET_OK_TO_UNINIT) {
			plm_events |= RXS_PLM_SPX_STAT_OK_TO_UNINIT;
		}
		if (writedata & RXS_SPX_ERR_DET_LINK_INIT) {
			plm_events |= RXS_PLM_SPX_STAT_LINK_INIT;
		}

		update_plm_status(dev_info, plm_mask, plm_events, port);
		return;
	}

	// No additional behavior required, just update the data...
	dev_info->poregs[idx].data = writedata;
}

static void emulate_plm_reg(DAR_DEV_INFO_t *dev_info,
				uint32_t offset,
				uint32_t writedata,
				uint32_t idx)
{
	rio_port_t port = (offset - RXS_PLM_SPX_IMP_SPEC_CTL(0))
			/ RXS_IMP_SPEC_PP_OSET;
	unsigned int t_idx;

	if (RXS_PLM_SPX_STAT(port) == offset) {
		dev_info->poregs[idx].data &= ~writedata;
		update_plm_status(dev_info, 0xFFFFFFFF,
				dev_info->poregs[idx].data, port);
		return;
	}

	if (RXS_PLM_SPX_EVENT_GEN(port) == offset) {
		// Event generation register clears itself...
		dev_info->poregs[idx].data = 0;
		t_idx = rxs_expect_poreg_idx(dev_info, RXS_PLM_SPX_STAT(port));
		dev_info->poregs[t_idx].data |= writedata;

		update_plm_status(dev_info, 0xFFFFFFFF,
				dev_info->poregs[t_idx].data, port);
		return;
	}

	// No additional behavior required, just update the data...
	dev_info->poregs[idx].data = writedata;
}

static void tlm_update_plm_status(DAR_DEV_INFO_t *dev_info,
				rio_port_t port)
{
	uint32_t plm_mask = RXS_PLM_SPX_STAT_TLM_INT | RXS_PLM_SPX_STAT_TLM_PW;
	uint32_t plm_events = 0;
	uint32_t stat;
	unsigned int t_idx;

	// Determine if an interrupt or port-write event has been
	// triggered.
	t_idx = rxs_expect_poreg_idx(dev_info, RXS_TLM_SPX_STAT(port));
	stat = dev_info->poregs[t_idx].data;

	t_idx = rxs_expect_poreg_idx(dev_info, RXS_TLM_SPX_PW_EN(port));
	if (stat & dev_info->poregs[t_idx].data) {
		plm_events |= RXS_PLM_SPX_STAT_TLM_PW;
	}

	t_idx = rxs_expect_poreg_idx(dev_info, RXS_TLM_SPX_INT_EN(port));
	if (stat & dev_info->poregs[t_idx].data) {
		plm_events |= RXS_PLM_SPX_STAT_TLM_INT;
	}

	update_plm_status(dev_info, plm_mask, plm_events, port);
}

static void emulate_tlm_reg(DAR_DEV_INFO_t *dev_info,
				uint32_t offset,
				uint32_t writedata,
				uint32_t idx)
{
	rio_port_t port = (offset - RXS_TLM_SPX_CONTROL(0))
			/ RXS_IMP_SPEC_PP_OSET;
	unsigned int t_idx;

	if (RXS_TLM_SPX_STAT(port) == offset) {
		dev_info->poregs[idx].data &= ~writedata;

		tlm_update_plm_status(dev_info, port);
		return;
	}

	if (RXS_TLM_SPX_EVENT_GEN(port) == offset) {
		// Event generation register clears itself...
		dev_info->poregs[idx].data = 0;
		t_idx = rxs_expect_poreg_idx(dev_info, RXS_TLM_SPX_STAT(port));
		dev_info->poregs[t_idx].data |= writedata;

		tlm_update_plm_status(dev_info, port);
		return;
	}

	// No additional behavior required, just update the data...
	dev_info->poregs[idx].data = writedata;
}

static void pbm_update_plm_status(DAR_DEV_INFO_t *dev_info,
				rio_port_t port)
{
	uint32_t plm_mask = RXS_PLM_SPX_STAT_PBM_INT |
				RXS_PLM_SPX_STAT_PBM_PW |
				RXS_PLM_SPX_STAT_PBM_FATAL;
	uint32_t plm_events = 0;
	uint32_t stat;
	unsigned int t_idx;

	// Determine if an interrupt or port-write event has been
	// triggered.
	t_idx = rxs_expect_poreg_idx(dev_info, RXS_PBM_SPX_STAT(port));
	stat = dev_info->poregs[t_idx].data;

	t_idx = rxs_expect_poreg_idx(dev_info, RXS_PBM_SPX_PW_EN(port));
	if (stat & dev_info->poregs[t_idx].data) {
		plm_events |= RXS_PLM_SPX_STAT_PBM_PW;
	}

	t_idx = rxs_expect_poreg_idx(dev_info, RXS_PBM_SPX_INT_EN(port));
	if (stat & dev_info->poregs[t_idx].data) {
		plm_events |= RXS_PLM_SPX_STAT_PBM_INT;
	}

	if (stat & (RXS_PBM_SPX_STAT_EG_DNFL_FATAL |
			RXS_PBM_SPX_STAT_EG_DOH_FATAL |
			RXS_PBM_SPX_STAT_EG_DATA_UNCOR)) {
		plm_events |= RXS_PLM_SPX_STAT_PBM_FATAL;
	}

	update_plm_status(dev_info, plm_mask, plm_events, port);
}

static void emulate_pbm_reg(DAR_DEV_INFO_t *dev_info,
				uint32_t offset,
				uint32_t writedata,
				uint32_t idx)
{
	rio_port_t port = (offset - RXS_PBM_SPX_CONTROL(0))
			/ RXS_IMP_SPEC_PP_OSET;
	unsigned int t_idx;

	if (RXS_PBM_SPX_STAT(port) == offset) {
		dev_info->poregs[idx].data &= ~writedata;
		pbm_update_plm_status(dev_info, port);
		return;
	}

	if (RXS_PBM_SPX_EVENT_GEN(port) == offset) {
		// Event generation register clears itself...
		dev_info->poregs[idx].data = 0;
		t_idx = rxs_expect_poreg_idx(dev_info, RXS_PBM_SPX_STAT(port));
		dev_info->poregs[t_idx].data |= writedata;

		pbm_update_plm_status(dev_info, port);
		return;
	}

	// No additional behavior required, just update the data...
	dev_info->poregs[idx].data = writedata;
}

static void update_dev_int_pw_stat(DAR_DEV_INFO_t *dev_info,
					uint32_t int_mask,
					uint32_t pw_mask,
					uint32_t events)
{
	unsigned int t_idx;

	// Update device port-write status
	t_idx = rxs_expect_poreg_idx(dev_info, RXS_EM_PW_STAT);
	if (events) {
		dev_info->poregs[t_idx].data |= pw_mask;
	} else {
		dev_info->poregs[t_idx].data &= ~pw_mask;
	}

	// Update device interrupt status
	t_idx = rxs_expect_poreg_idx(dev_info, RXS_EM_INT_STAT);
	if (events) {
		dev_info->poregs[t_idx].data |= int_mask;
	} else {
		dev_info->poregs[t_idx].data &= ~int_mask;
	}
	events = dev_info->poregs[t_idx].data;

	// Update interrupt pin status
	t_idx = rxs_expect_poreg_idx(dev_info, RXS_EM_INT_EN);
	events &= dev_info->poregs[t_idx].data;

	t_idx = rxs_expect_poreg_idx(dev_info, RXS_EM_DEV_INT_EN);
	if (events) {
		dev_info->poregs[t_idx].data |= RXS_EM_DEV_INT_EN_INT;
	} else {
		dev_info->poregs[t_idx].data |= RXS_EM_DEV_INT_EN_INT;
	}
}

#define IN_RANGE(a,b,c) (((a) <= (b)) && ((b) <= (c)))

static void rxs_emulate_reg_write(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t writedata)
{
	uint32_t idx = rxs_get_poreg_idx(dev_info, MOCK_REG_ADDR(offset));
	uint32_t t_idx;
	uint32_t events;

	if ((DAR_POREG_BAD_IDX == idx) && DEBUG_PRINTF) {
		printf("\nMissing offset is 0x%x\n", offset);
		assert_int_equal(0xFFFFFFFF, offset);
		assert_true(st.real_hw);
		return;
	}

	// Note: RXS_ERR_DET (logical layer errors) are emulated later in
	// this routine.
	if (IN_RANGE(RXS_SPX_ERR_DET(0), offset,
			RXS_SPX_DLT_CSR(NUM_RXS_PORTS(dev_info) - 1))) {
		emulate_emhs_pp_reg(dev_info, offset, writedata, idx);
		return;
	}

	if (IN_RANGE(RXS_PLM_BH, offset,
			RXS_PLM_SPX_SCRATCHY(NUM_RXS_PORTS(dev_info) - 1,
						RXS_PLM_SPX_MAX_SCRATCHY))) {
		emulate_plm_reg(dev_info, offset, writedata, idx);
		return;
	}

	if (IN_RANGE(RXS_TLM_BH, offset,
			RXS_TLM_SPX_ROUTE_EN(NUM_RXS_PORTS(dev_info) - 1))) {
		emulate_tlm_reg(dev_info, offset, writedata, idx);
		return;
	}

	if (IN_RANGE(RXS_PBM_BH, offset,
			RXS_PBM_SPX_SCRATCH2(NUM_RXS_PORTS(dev_info) - 1))) {
		emulate_pbm_reg(dev_info, offset, writedata, idx);
		return;
	}

	switch (offset) {
	case I2C_INT_STAT:
		dev_info->poregs[idx].data &= ~writedata;
		events = dev_info->poregs[idx].data;

		t_idx = rxs_expect_poreg_idx(dev_info, I2C_INT_ENABLE);
		events &= dev_info->poregs[t_idx].data;

		update_dev_int_pw_stat(dev_info,
					RXS_EM_INT_STAT_EXTERNAL_I2C,
					RXS_EM_PW_STAT_EXTERNAL_I2C,
					events);
		break;

	case I2C_INT_SET:
		dev_info->poregs[idx].data = 0;

		t_idx = rxs_expect_poreg_idx(dev_info, I2C_INT_STAT);
		dev_info->poregs[t_idx].data |= writedata;
		events = dev_info->poregs[t_idx].data;

		t_idx = rxs_expect_poreg_idx(dev_info, I2C_INT_ENABLE);
		events &= dev_info->poregs[t_idx].data;

		update_dev_int_pw_stat(dev_info,
					RXS_EM_INT_STAT_LOG,
					RXS_EM_PW_STAT_LOG,
					events);
		break;

	case RXS_ERR_DET:
		writedata &= (RXS_ERR_DET_ILL_TYPE |
				RXS_ERR_DET_UNS_RSP |
				RXS_ERR_DET_ILL_ID);

		dev_info->poregs[idx].data = writedata;
		events = dev_info->poregs[idx].data;

		t_idx = rxs_expect_poreg_idx(dev_info, RXS_ERR_EN);
		events &= dev_info->poregs[t_idx].data;

		update_dev_int_pw_stat(dev_info,
					RXS_EM_INT_STAT_LOG,
					RXS_EM_PW_STAT_LOG,
					events);
		break;

	default:
		dev_info->poregs[idx].data = writedata;
		break;
	}
}

static void check_write_bc(DAR_DEV_INFO_t *dev_info,
			uint32_t offset, uint32_t writedata)
{
	uint32_t did, mask_idx, mask;
	rio_port_t port;

	if (st.real_hw) {
		return;
	}

	// Handle writes to broadcast set/clear multicast mask registers
	if ((offset >= RXS_BC_MC_X_S_CSR(0)) &&
		(offset < RXS_BC_MC_X_C_CSR(RXS2448_MC_MASK_CNT))) {
		uint32_t new_mask;
		bool do_clear = (offset & 4) ? true : false;

		mask_idx = (offset - RXS_BC_MC_X_S_CSR(0)) / 8;
		assert_in_range(mask_idx, 0, RXS2448_MAX_MC_MASK);

		for (port = 0; port < NUM_RXS_PORTS(dev_info);  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(dev_info,
				RXS_SPX_MC_Y_S_CSR(port, mask_idx),
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
	for (port = 0; port < NUM_RXS_PORTS(dev_info);  port++) {
		if ((offset >= RXS_SPX_MC_Y_S_CSR(port, 0)) &&
		(offset < RXS_SPX_MC_Y_C_CSR(port, RXS2448_MC_MASK_CNT))) {
			uint32_t new_mask;
			bool do_clear = (offset & 4) ? true : false;

			mask_idx = offset - RXS_SPX_MC_Y_S_CSR(port, 0);
			mask_idx /= 8;
			assert_in_range(mask_idx, 0, RXS2448_MAX_MC_MASK);

			assert_int_equal(RIO_SUCCESS,
				DARRegRead(dev_info,
				RXS_SPX_MC_Y_S_CSR(port, mask_idx),
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

	if ((offset >= RXS_BC_L2_GX_ENTRYY_CSR(0,0)) &&
		(offset <= RXS_BC_L2_GX_ENTRYY_CSR(0, RIO_RT_GRP_SZ-1))) {
		did = (offset - RXS_BC_L2_GX_ENTRYY_CSR(0,0)) / 4;

		for (port = 0; port < NUM_RXS_PORTS(dev_info);  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(dev_info,
				RXS_SPX_L2_GY_ENTRYZ_CSR(port, 0, did),
				writedata));
		}
		return;
	}

	if ((offset >= RXS_BC_L1_GX_ENTRYY_CSR(0,0)) &&
		(offset <= RXS_BC_L1_GX_ENTRYY_CSR(0, RIO_RT_GRP_SZ-1))) {
		did = (offset - RXS_BC_L1_GX_ENTRYY_CSR(0,0)) / 4;

		for (port = 0; port < NUM_RXS_PORTS(dev_info);  port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(dev_info,
				RXS_SPX_L1_GY_ENTRYZ_CSR(port, 0, did),
				writedata));
		}
	}
	rxs_emulate_reg_write(dev_info, offset, writedata);
}

// TODO: The register support leverages RXS routing table register test support
// code verbatim, extending it to support emulation of Error Management
// registers.  Would be nice if the register emulation code could be a common
// superset for all tests.

static uint32_t RXSWriteReg(DAR_DEV_INFO_t *dev_info,
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
		rc = riomp_mgmt_rcfg_write(st.mp_h, st.did_reg_val, st.hc, offset, 4, writedata);
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

uint32_t rxs_mock_reg_oset[] = {
	RXS_PRESCALAR_SRV_CLK,
	RXS_MPM_CFGSIG0,
	RXS_SP_LT_CTL,
	RXS_SP_GEN_CTL,
	RXS_ROUTE_DFLT_PORT,
	RXS_PKT_TIME_LIVE,
	RXS_ERR_DET,
	RXS_ERR_EN,
        RXS_PW_TGT_ID,
        RXS_PW_CTL,
        RXS_PW_ROUTE,

        RXS_EM_RST_PW_EN,
        RXS_EM_RST_INT_EN,
        RXS_EM_INT_EN,
        RXS_EM_DEV_INT_EN,
        RXS_EM_PW_EN,
        RXS_PW_TRAN_CTL,
        RXS_EM_INT_STAT,
        RXS_EM_INT_PORT_STAT,
        RXS_EM_PW_STAT,
        RXS_EM_PW_PORT_STAT,
        RXS_EM_RST_PORT_STAT,

        I2C_INT_ENABLE,
        I2C_INT_SET,
        I2C_INT_STAT
};

typedef struct rxs_mock_pp_reg_t_TAG {
        uint32_t base;
        uint32_t pp_oset;
        uint32_t val;
} rxs_mock_pp_reg_t;

#define RXS_SPX_LM_RESP_DFLT 0
#define RXS_SPX_IN_ACKID_CSR_DFLT 0
#define RXS_SPX_OUT_ACKID_CSR_DFLT 0
#define RXS_PLM_SPX_PW_EN_DFLT 0
#define RXS_PLM_SPX_INT_EN_DFLT 0
#define RXS_PLM_SPX_ALL_INT_EN_DFLT 0
#define RXS_PLM_SPX_DENIAL_CTL_DFLT 0
#define RXS_SPX_CTL2_DFLT (RXS_SPX_CTL2_GB_6p25_EN | \
                                RXS_SPX_CTL2_GB_6p25 | \
                                RIO_SPX_CTL2_BAUD_SEL_6P25_BR)
#define RXS_SPX_ERR_STAT_DFLT (RXS_SPX_ERR_STAT_PORT_UNAVL)
#define RXS_SPX_CTL_DFLT (RXS_SPX_CTL_PORT_DIS | \
                                RIO_SPX_CTL_PTW_INIT_4x | \
                                RXS_SPX_CTL_PORT_WIDTH)
#define RXS_SPX_ERR_DET_DFLT 0
#define RXS_SPX_RATE_EN_DFLT 0
#define RXS_SPX_DLT_DFLT 0

#define RXS_PLM_SPX_IMP_SPEC_CTL_DFLT (RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST)
#define RXS_PLM_SPX_PWDN_CTL_DFLT (RXS_PLM_SPX_PWDN_CTL_PWDN_PORT)
#define RXS_PLM_SPX_POL_CTL_DFLT 0
#define RXS_PLM_SPX_PNA_CAP_DFLT (RXS_PLM_SPX_PNA_CAP_VALID)
#define RXS_PLM_SPX_STAT_DFLT 0
#define RXS_PLM_SPX_INT_EN_DFLT 0
#define RXS_PLM_SPX_EVENT_GEN_DFLT 0

#define RXS_TLM_SPX_STAT_DFLT 0
#define RXS_TLM_SPX_PW_EN_DFLT 0
#define RXS_TLM_SPX_INT_EN_DFLT 0
#define RXS_TLM_SPX_FTYPE_FILT_DFLT 0
#define RXS_TLM_SPX_EVENT_GEN_DFLT 0

#define RXS_PBM_SPX_STAT_DFLT 0
#define RXS_PBM_SPX_PW_EN_DFLT 0
#define RXS_PBM_SPX_INT_EN_DFLT 0
#define RXS_PBM_SPX_EVENT_GEN_DFLT 0

rxs_mock_pp_reg_t rxs_mock_pp_reg[] = {
        {RXS_SPX_LM_RESP(0), 0x40, RXS_SPX_LM_RESP_DFLT},
        {RXS_SPX_IN_ACKID_CSR(0), 0x40, RXS_SPX_IN_ACKID_CSR_DFLT},
        {RXS_SPX_OUT_ACKID_CSR(0), 0x40, RXS_SPX_OUT_ACKID_CSR_DFLT},
        {RXS_SPX_CTL2(0), 0x40, RXS_SPX_CTL2_DFLT},
        {RXS_SPX_ERR_STAT(0), 0x40, RXS_SPX_ERR_STAT_DFLT},
        {RXS_SPX_CTL(0), 0x40, RXS_SPX_CTL_DFLT},
        {RXS_SPX_ERR_DET(0), 0x100, RXS_SPX_ERR_DET_DFLT},
        {RXS_SPX_RATE_EN(0), 0x40, RXS_SPX_RATE_EN_DFLT},
        {RXS_SPX_DLT_CSR(0), 0x40, RXS_SPX_DLT_DFLT},
        {RXS_PLM_SPX_IMP_SPEC_CTL(0), 0x100,
                                RXS_PLM_SPX_IMP_SPEC_CTL_DFLT},
        {RXS_PLM_SPX_STAT(0), 0x100, RXS_PLM_SPX_STAT_DFLT},
        {RXS_PLM_SPX_PW_EN(0), 0x100, RXS_PLM_SPX_PW_EN_DFLT},
        {RXS_PLM_SPX_INT_EN(0), 0x100, RXS_PLM_SPX_INT_EN_DFLT},
        {RXS_PLM_SPX_ALL_INT_EN(0), 0x100, RXS_PLM_SPX_ALL_INT_EN_DFLT},
        {RXS_PLM_SPX_PWDN_CTL(0), 0x100, RXS_PLM_SPX_PWDN_CTL_DFLT},
        {RXS_PLM_SPX_POL_CTL(0), 0x100, RXS_PLM_SPX_POL_CTL_DFLT},
        {RXS_PLM_SPX_PNA_CAP(0), 0x100, RXS_PLM_SPX_PNA_CAP_DFLT},
        {RXS_PLM_SPX_DENIAL_CTL(0), 0x100, RXS_PLM_SPX_DENIAL_CTL_DFLT},
        {RXS_PLM_SPX_EVENT_GEN(0), 0x100, RXS_PLM_SPX_EVENT_GEN_DFLT},

        {RXS_TLM_SPX_STAT(0), 0x100, RXS_TLM_SPX_STAT_DFLT},
        {RXS_TLM_SPX_PW_EN(0), 0x100, RXS_TLM_SPX_PW_EN_DFLT},
        {RXS_TLM_SPX_INT_EN(0), 0x100, RXS_TLM_SPX_INT_EN_DFLT},
        {RXS_TLM_SPX_FTYPE_FILT(0), 0x100, RXS_TLM_SPX_FTYPE_FILT_DFLT},
        {RXS_TLM_SPX_EVENT_GEN(0), 0x100, RXS_TLM_SPX_EVENT_GEN_DFLT},

        {RXS_PBM_SPX_STAT(0), 0x100, RXS_PBM_SPX_STAT_DFLT},
        {RXS_PBM_SPX_PW_EN(0), 0x100, RXS_PBM_SPX_PW_EN_DFLT},
        {RXS_PBM_SPX_INT_EN(0), 0x100, RXS_PBM_SPX_INT_EN_DFLT},
        {RXS_PBM_SPX_EVENT_GEN(0), 0x100, RXS_PBM_SPX_EVENT_GEN_DFLT},
};

// Count up maximum registers saved.
// Note: only the first level 1 and level 2 routing table groups are supported
// Use *_MAX_PORTS + 1 to account for device broadcast registers
// There are 2 multicast mask registers for each multicast mask:
//    one to set, and one to clear

#define MOCK_DEV_REG (sizeof(rxs_mock_reg_oset)/sizeof(rxs_mock_reg_oset[0]))
#define MOCK_PP_REG (sizeof(rxs_mock_pp_reg)/sizeof(rxs_mock_pp_reg[0]))
#define MOCK_MC_REG ((RXS2448_MAX_PORTS + 1) * RXS2448_MC_MASK_CNT * 2)
#define MOCK_RT_REG ((RXS2448_MAX_PORTS + 1) * RIO_RT_GRP_SZ * 2)

#define TOT_MOCK_REG ((MOCK_DEV_REG + MOCK_MC_REG + MOCK_RT_REG) + \
			(MOCK_PP_REG * RXS2448_MAX_PORTS))
#define UPB_MOCK_REG (TOT_MOCK_REG + 1)

static DAR_DEV_INFO_t mock_dev_info;

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
	mock_dev_info.swMcastInfo = 0; // RXS does not support MC INFO reg!!!

	for (idx = 0; idx < RIO_MAX_PORTS; idx++) {
		mock_dev_info.ctl1_reg[idx] = 0;
	}

	for (idx = 0; idx < MAX_DAR_SCRPAD_IDX; idx++) {
		mock_dev_info.scratchpad[idx] = 0;
	}
}

/* Initialize the mock register structure for all mocked registers.
 */

static rio_perf_opt_reg_t mock_dar_reg[UPB_MOCK_REG];

static void init_mock_rxs_reg(void **state)
{
	// idx is always should be less than UPB_DAR_REG.
	uint32_t port, idev, i, val;
	RXS_test_state_t *l_st = *(RXS_test_state_t **)state;

	DAR_proc_ptr_init(RXSReadReg, RXSWriteReg, RXSWaitSec);
	if (l_st->real_hw) {
		mock_dev_info.poregs_max = 0;
		mock_dev_info.poreg_cnt = 0;
		mock_dev_info.poregs = NULL;
		return;
	}

	mock_dev_info.poregs_max = UPB_MOCK_REG;
	mock_dev_info.poreg_cnt = 0;
	mock_dev_info.poregs = mock_dar_reg;

	// Initialize RXS device regs
	for (i = 0; i < MOCK_DEV_REG; i++) {
		assert_int_equal(RIO_SUCCESS,
			rxs_add_poreg(&mock_dev_info, rxs_mock_reg_oset[i], 0));
	}

	// Initialize RXS per port regs
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (i = 0; i < MOCK_PP_REG; i++) {
			// Odd ports cannot support 4x, and they train at 2x
			val = rxs_mock_pp_reg[i].val;
			if (RXS_SPX_CTL(0) == rxs_mock_pp_reg[i].base) {
				if (port & 1) {
					val &= ~RIO_SPX_CTL_PTW_MAX_4X;
					val &= ~RIO_SPX_CTL_PTW_INIT_4x;
					val |= RIO_SPX_CTL_PTW_INIT_2x;
				}
			}
			assert_int_equal(RIO_SUCCESS,
				rxs_add_poreg(&mock_dev_info,
					rxs_mock_pp_reg[i].base +
					(rxs_mock_pp_reg[i].pp_oset * port),
					val));
		}
	}

	// Initialize RXS_BC_MC_Y_S_CSR and RXS_BC_MC_Y_C_CSR
	for (idev = 0; idev < RXS2448_MC_MASK_CNT; idev++) {
		assert_int_equal(RIO_SUCCESS,
			rxs_add_poreg(&mock_dev_info,
				RXS_BC_MC_X_S_CSR(idev),
				0x00));
		assert_int_equal(RIO_SUCCESS,
			rxs_add_poreg(&mock_dev_info,
				RXS_BC_MC_X_C_CSR(idev),
				0x00));
	}

	// Initialize RXS_SPX_MC_Y_S_CSR and RXS_SPX_MC_Y_C_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RXS2448_MC_MASK_CNT; idev++) {
			assert_int_equal(RIO_SUCCESS,
				rxs_add_poreg(&mock_dev_info,
					RXS_SPX_MC_Y_S_CSR(port, idev),
					0x00));
			assert_int_equal(RIO_SUCCESS,
				rxs_add_poreg(&mock_dev_info,
					RXS_SPX_MC_Y_C_CSR(port, idev),
					0x00));
		}
	}

	// Initialize RXS_BC_L2_GX_ENTRYY_CSR
	for (idev = 0; idev < RIO_RT_GRP_SZ; idev++) {
		assert_int_equal(RIO_SUCCESS,
			rxs_add_poreg(&mock_dev_info,
				RXS_BC_L2_GX_ENTRYY_CSR(0, idev),
				0x00));
	}

	// Initialize RXS_BC_L1_GX_ENTRYY_CSR
	for (idev = 0; idev < RIO_RT_GRP_SZ; idev++) {
		assert_int_equal(RIO_SUCCESS,
			rxs_add_poreg(&mock_dev_info,
				RXS_BC_L1_GX_ENTRYY_CSR(0, idev),
				0x00));
	}

	// Initialize RXS_SPX_L2_GY_ENTRYZ_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RIO_RT_GRP_SZ; idev++) {
			assert_int_equal(RIO_SUCCESS,
					rxs_add_poreg(&mock_dev_info,
				RXS_SPX_L2_GY_ENTRYZ_CSR(port, 0, idev),
					0x00));
		}
	}

	// Initialize RXS_SPX_L1_GY_ENTRYZ_CSR
	for (port = 0; port < RXS2448_MAX_PORTS; port++) {
		for (idev = 0; idev < RIO_RT_GRP_SZ; idev++) {
			assert_int_equal(RIO_SUCCESS,
					rxs_add_poreg(&mock_dev_info,
				RXS_SPX_L1_GY_ENTRYZ_CSR(port, 0, idev),
					(!idev)? RIO_RTE_LVL_G0 : 0x00));
		}
	}
}

// The setup function which should be called before any unit tests that
// need to be executed.

static int rxs_em_setup(void **state)
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

// Routine to set virtual register status for
// rxs_pc_get_config and rxs_pc_get_status

typedef enum config_hw_t_TAG {
	cfg_unavl,
	cfg_avl_pwdn,
	cfg_pwup_txdis,
	cfg_txen_no_lp,
	cfg_txen_lp_perr,
	cfg_lp_lkout,
	cfg_lp_nmtc_dis,
	cfg_lp_lpbk,
	cfg_lp_ecc,
	cfg_perfect
} config_hw_t;

#define NO_TTL false
#define YES_TTL true
#define NO_FILT false
#define YES_FILT true

static void set_all_port_config(config_hw_t cfg,
					bool ttl,
					bool filter,
					rio_port_t port)
{
	uint32_t ctl2, err_stat, ctl, plm_ctl, plm_stat, pwdn;
	uint32_t err_stat_avail = RXS_SPX_ERR_STAT_DFLT &
				~RXS_SPX_ERR_STAT_PORT_UNAVL;
	uint32_t err_stat_nolp = err_stat_avail |
			RXS_SPX_ERR_STAT_PORT_UNINIT;
	uint32_t err_stat_lp_ok = err_stat_avail |
			RXS_SPX_ERR_STAT_PORT_OK;
	uint32_t err_stat_lp_perr = err_stat_lp_ok |
			RXS_SPX_ERR_STAT_PORT_ERR;
	uint32_t lpbk_mask = RXS_PLM_SPX_IMP_SPEC_CTL_DLB_EN |
				RXS_PLM_SPX_IMP_SPEC_CTL_LLB_EN;

	uint32_t ttl_reg = (ttl) ? 0xFFFFFFFF : 0;
	uint32_t filt = (filter) ? 0xFFFFFFFF : 0;
	rio_port_t st_port = port, end_port = port;

	if (RIO_ALL_PORTS == port) {
		st_port = 0;
		end_port = NUM_RXS_PORTS(&mock_dev_info) - 1;
	}

	assert_int_equal(RIO_SUCCESS,
		RXSWriteReg(&mock_dev_info, RXS_PKT_TIME_LIVE, ttl_reg));

	for (port = st_port; port <= end_port; port++) {
		assert_int_equal(RIO_SUCCESS,
			RXSReadReg(&mock_dev_info,
				RXS_SPX_CTL2(port), &ctl2));
		assert_int_equal(RIO_SUCCESS,
			RXSReadReg(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		assert_int_equal(RIO_SUCCESS,
			RXSReadReg(&mock_dev_info,
				RXS_SPX_CTL(port), &ctl));
		assert_int_equal(RIO_SUCCESS,
			RXSReadReg(&mock_dev_info,
				RXS_PLM_SPX_IMP_SPEC_CTL(port), &plm_ctl));
		assert_int_equal(RIO_SUCCESS,
			RXSReadReg(&mock_dev_info,
				RXS_PLM_SPX_STAT(port),
								&plm_stat));
		assert_int_equal(RIO_SUCCESS,
			RXSReadReg(&mock_dev_info,
				RXS_PLM_SPX_PWDN_CTL(port), &pwdn));
		switch(cfg) {
		case cfg_unavl:
			err_stat = RXS_SPX_ERR_STAT_DFLT;
			break;;
		case cfg_avl_pwdn:
			err_stat = err_stat_avail;
			pwdn = RXS_PLM_SPX_PWDN_CTL_DFLT;
			break;
		case cfg_pwup_txdis:
			err_stat = err_stat_avail;
			pwdn = 0;
			ctl |= RXS_SPX_CTL_PORT_DIS;
			break;
		case cfg_txen_no_lp:
			err_stat = err_stat_nolp;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			break;
		case cfg_txen_lp_perr:
			err_stat = err_stat_lp_perr;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			break;
		case cfg_lp_lkout:
			err_stat = err_stat_lp_ok;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			ctl |= RXS_SPX_CTL_PORT_LOCKOUT;
			break;
		case cfg_lp_nmtc_dis:
			err_stat = err_stat_lp_ok;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			ctl &= ~RXS_SPX_CTL_PORT_LOCKOUT;
			ctl &= ~(RXS_SPX_CTL_INP_EN |
				RXS_SPX_CTL_OTP_EN);
			break;
		case cfg_lp_lpbk:
			err_stat = err_stat_lp_ok;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			ctl &= ~RXS_SPX_CTL_PORT_LOCKOUT;
			ctl |= RXS_SPX_CTL_INP_EN | RXS_SPX_CTL_OTP_EN;
			plm_ctl |= lpbk_mask;
			break;
		case cfg_lp_ecc:
			err_stat = err_stat_lp_ok;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			ctl &= ~RXS_SPX_CTL_PORT_LOCKOUT;
			ctl |= RXS_SPX_CTL_INP_EN | RXS_SPX_CTL_OTP_EN;
			plm_ctl &= ~lpbk_mask;
			plm_stat = RXS_PLM_SPX_STAT_PBM_FATAL;
			break;
		case cfg_perfect:
			err_stat = err_stat_lp_ok;
			pwdn = 0;
			ctl &= ~RXS_SPX_CTL_PORT_DIS;
			ctl &= ~RXS_SPX_CTL_PORT_LOCKOUT;
			ctl |= RXS_SPX_CTL_INP_EN | RXS_SPX_CTL_OTP_EN;
			plm_ctl &= ~RXS_PLM_SPX_IMP_SPEC_CTL_LLB_EN;
			plm_stat = 0;
			break;
		default:
			assert_true(false);
		}
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info,
				RXS_SPX_CTL2(port), ctl2));
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info, RXS_SPX_CTL(port), ctl));
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info,
				RXS_PLM_SPX_IMP_SPEC_CTL(port), plm_ctl));
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info,
				RXS_PLM_SPX_STAT(port), plm_stat));
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info,
				RXS_PLM_SPX_PWDN_CTL(port), pwdn));
		assert_int_equal(RIO_SUCCESS,
			RXSWriteReg(&mock_dev_info,
				RXS_TLM_SPX_FTYPE_FILT(port), filt));
	}
}

#define COMPUTE_RXS_PW_RETX(x) \
	((((x * PORT_WRITE_RE_TX_NSEC) + RXS_PW_CTL_PW_TMR_NSEC - 1) / \
			RXS_PW_CTL_PW_TMR_NSEC) << 8)

static void rxs_em_cfg_pw_success_test(void **state)
{
	rio_em_cfg_pw_in_t in_parms;
	rio_em_cfg_pw_out_t out_parms;
	did_reg_t targ_id = 0x1234;
	uint32_t retx = 1000000; // 352 msec
	uint32_t retx_reg = COMPUTE_RXS_PW_RETX(retx);
	uint32_t chkdata;
	rio_port_t dflt_port = 5;
        rio_rt_initialize_in_t init_in;
        rio_rt_initialize_out_t init_out;
	rio_rt_state_t rt;

	// RXS routes port-writes according to port bit-vector.
	// RXS EM support computes this bit vector based on current routing
	// table values, so set up the routing table...

	init_in.set_on_port = RIO_ALL_PORTS;
	init_in.default_route = RIO_RTV_PORT(dflt_port);
	init_in.default_route_table_port = RIO_RTV_PORT(dflt_port);
	init_in.update_hw = true;
	init_in.rt = &rt;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_rt_initialize(&mock_dev_info, &init_in, &init_out));

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	// Test for dev16 destIDs...
	in_parms.imp_rc = 0xFFFFFFFF;
	in_parms.deviceID_tt = tt_dev16;
	in_parms.port_write_destID = targ_id;
	in_parms.srcID_valid = false;
	in_parms.port_write_srcID = 0;
	in_parms.priority = 1;
	in_parms.CRF = false;
	in_parms.port_write_re_tx = retx; // 1 msec
	memset(&out_parms, 0, sizeof(out_parms));
	in_parms.imp_rc = 0xFFFFFFFF;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_em_cfg_pw(&mock_dev_info, &in_parms, &out_parms));
	assert_int_equal(0, out_parms.imp_rc);
	assert_int_equal(tt_dev16, out_parms.deviceID_tt);
	assert_int_equal(targ_id, out_parms.port_write_destID);
	assert_false(out_parms.srcID_valid);
	assert_int_equal(0, out_parms.port_write_srcID);
	assert_int_equal(3, out_parms.priority);
	assert_true(out_parms.CRF);
	assert_int_equal(retx, in_parms.port_write_re_tx);

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_TGT_ID, &chkdata));
	assert_int_equal(chkdata, (targ_id << 16) | RXS_PW_TGT_ID_DEV16);

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_CTL, &chkdata));
	assert_int_equal(chkdata, retx_reg);

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_ROUTE, &chkdata));
	assert_int_equal(chkdata, 1 << dflt_port);

	// Test for dev8 destIDs...
	in_parms.imp_rc = 0xFFFFFFFF;
	in_parms.deviceID_tt = tt_dev8;
	in_parms.port_write_destID = targ_id & 0xFF;
	in_parms.srcID_valid = false;
	in_parms.port_write_srcID = 0;
	in_parms.priority = 1;
	in_parms.CRF = false;
	in_parms.port_write_re_tx = retx;
	memset(&out_parms, 0, sizeof(out_parms));
	in_parms.imp_rc = 0xFFFFFFFF;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_em_cfg_pw(&mock_dev_info, &in_parms, &out_parms));
	assert_int_equal(0, out_parms.imp_rc);
	assert_int_equal(tt_dev8, out_parms.deviceID_tt);
	assert_int_equal((did_reg_t)(targ_id & 0xFF), out_parms.port_write_destID);
	assert_false(out_parms.srcID_valid);
	assert_int_equal((did_reg_t)0, out_parms.port_write_srcID);
	assert_int_equal(3, out_parms.priority);
	assert_true(out_parms.CRF);
	assert_int_equal(retx, out_parms.port_write_re_tx);

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_TGT_ID, &chkdata));
	assert_int_equal(chkdata, (targ_id << 16) & RXS_PW_TGT_ID_PW_TGT_ID);

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_CTL, &chkdata));
	assert_int_equal(chkdata, retx_reg);

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_ROUTE, &chkdata));
	assert_int_equal(chkdata, 1 << dflt_port);
	(void)state; // unused
}

static void rxs_em_cfg_pw_bad_parms_test(void **state)
{
	rio_em_cfg_pw_in_t in_parms;
	rio_em_cfg_pw_out_t out_parms;
	did_reg_t targ_id = 0x12345678;

	// Test for dev16 destIDs...
	in_parms.imp_rc = 0xFFFFFFFF;
	in_parms.deviceID_tt = tt_dev16;
	in_parms.port_write_destID = targ_id;
	in_parms.srcID_valid = false;
	in_parms.port_write_srcID = 0;
	in_parms.priority = 1;
	in_parms.CRF = false;
	in_parms.port_write_re_tx = 5555;
	memset(&out_parms, 0, sizeof(out_parms));

	in_parms.deviceID_tt = tt_dev16;
	in_parms.priority = 4;
	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_pw(&mock_dev_info, &in_parms,
					&out_parms));
	assert_int_not_equal(0, out_parms.imp_rc);

	(void)state; // unused
}

typedef struct rxs_pw_retx_info_t_TAG {
	uint32_t timer_val_in;
	uint32_t timer_val_out;
	uint32_t reg_val_out;
} rxs_pw_retx_info_t;

// Verify the RXS exact values for port-write retransmission
static void rxs_rio_em_cfg_pw_retx_compute_test(void **state)
{
	rio_em_cfg_pw_in_t in_p;
	rio_em_cfg_pw_out_t out_p;
	did_reg_t targ_id = 0x1234;
	uint32_t chkdata;
	const rxs_pw_retx_info_t tests[] = {
		{ 1000, 1000, COMPUTE_RXS_PW_RETX(1000)},
		{ 9999, 9999, COMPUTE_RXS_PW_RETX(9999)},
		{ 1234, 1234, COMPUTE_RXS_PW_RETX(1234)},
		{ 1, 1, COMPUTE_RXS_PW_RETX(1)}
	};
	const int num_tests = sizeof(tests) / sizeof(tests[0]);
	int i;
	rio_port_t dflt_port = 5;
        rio_rt_initialize_in_t init_in;
        rio_rt_initialize_out_t init_out;
	rio_rt_state_t rt;

	// RXS routes port-writes according to port bit-vector.
	// RXS EM support computes this bit vector based on current routing
	// table values, so set up the routing table...

	init_in.set_on_port = RIO_ALL_PORTS;
	init_in.default_route = RIO_RTV_PORT(dflt_port);
	init_in.default_route_table_port = RIO_RTV_PORT(dflt_port);
	init_in.update_hw = true;
	init_in.rt = &rt;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_rt_initialize(&mock_dev_info, &init_in, &init_out));

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	// Test for dev16 destIDs...
	in_p.imp_rc = 0xFFFFFFFF;
	in_p.deviceID_tt = tt_dev16;
	in_p.port_write_destID = targ_id;
	in_p.srcID_valid = false;
	in_p.port_write_srcID = 0;
	in_p.priority = 1;
	in_p.CRF = false;
	memset(&out_p, 0, sizeof(out_p));
	in_p.imp_rc = 0xFFFFFFFF;

	for (i = 0; i < num_tests; i++) {
		in_p.port_write_re_tx = tests[i].timer_val_in;
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_pw(&mock_dev_info, &in_p, &out_p));

		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(tt_dev16, out_p.deviceID_tt);
		assert_int_equal(targ_id, out_p.port_write_destID);
		assert_false(out_p.srcID_valid);
		assert_int_equal((did_reg_t)0, out_p.port_write_srcID);
		assert_int_equal(3, out_p.priority);
		assert_true(out_p.CRF);
		assert_int_equal(tests[i].timer_val_out,
				out_p.port_write_re_tx);

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_PW_CTL, &chkdata));
		assert_int_equal(chkdata, tests[i].reg_val_out);
	}

	(void)state; // unused
}

static void chk_plm_event_enables(rio_em_notfn_ctl_t ntfn,
				uint32_t event_mask,
				rio_port_t port)
{
	uint32_t int_en;
	uint32_t pw_en;
	bool ints;
	bool pws;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_INT_EN(port), &int_en));
	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_PW_EN(port), &pw_en));

	pws = pw_en & event_mask;
	ints = int_en & event_mask;

	switch (ntfn) {
	case rio_em_notfn_none:
		assert_false(ints);
		assert_false(pws);
		break;
	case rio_em_notfn_int:
		assert_true(ints);
		assert_false(pws);
		break;
	case rio_em_notfn_pw:
		assert_false(ints);
		assert_true(pws);
		break;
	case rio_em_notfn_both:
		assert_true(ints);
		assert_true(pws);
		break;
	case rio_em_notfn_0delta:
	case rio_em_notfn_last:
	default:
		assert_true(false);
		break;
	}
}

static void chk_regs_los(rio_em_cfg_t *event, rio_port_t port)
{
	uint32_t ctl;
	uint32_t plm_ctl;
	uint32_t dlt_to;
	uint32_t ctl_mask = RXS_SPX_RATE_EN_OK_TO_UNINIT |
				RXS_SPX_RATE_EN_DLT;
	uint32_t plm_ctl_mask = RXS_PLM_SPX_IMP_SPEC_CTL_OK2U_FATAL |
				RXS_PLM_SPX_IMP_SPEC_CTL_DLT_FATAL |
				RXS_PLM_SPX_IMP_SPEC_CTL_DWNGD_FATAL;
	uint64_t time;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_SPX_RATE_EN(port), &ctl));

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_SPX_DLT_CSR(port), &dlt_to));

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port),
								&plm_ctl));

	ctl &= ctl_mask;
	time = (dlt_to >> 8);
	time *= RXS_SPX_DLT_CSR_TIMEOUT_NSEC;
	if (time > UINT32_MAX) {
		time = UINT32_MAX;
	}
	dlt_to = time;
	plm_ctl &= plm_ctl_mask;

	switch (event->em_detect) {
	case rio_em_detect_off:
		assert_int_equal(0, ctl);
		assert_int_equal(0, dlt_to);
		assert_int_equal(0, plm_ctl);
		break;

	case rio_em_detect_on:
		assert_int_equal(plm_ctl, plm_ctl_mask);
		assert_int_equal(dlt_to, event->em_info);
		if (event->em_info) {
			assert_int_equal(ctl, RXS_SPX_RATE_EN_DLT);
		} else {
			assert_int_equal(ctl, RXS_SPX_RATE_EN_OK_TO_UNINIT);
		}
		break;
	case rio_em_detect_0delta:
	case rio_em_detect_last:
	default:
		assert_true(false);
	}
}

static void chk_regs_2many_retx(rio_em_cfg_t *event, rio_port_t port)
{
	uint32_t plm_denial_ctl;
	uint32_t mask = RXS_PLM_SPX_DENIAL_CTL_CNT_RTY;
	uint32_t thresh;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_DENIAL_CTL(port),
							&plm_denial_ctl));
	thresh = plm_denial_ctl & RXS_PLM_SPX_DENIAL_CTL_DENIAL_THRESH;

	switch (event->em_detect) {
	case rio_em_detect_off:
		assert_int_equal(0, plm_denial_ctl & mask);
		break;
	case rio_em_detect_on:
		assert_int_equal(thresh, event->em_info);
		assert_int_equal(mask, plm_denial_ctl & mask);
		break;
	case rio_em_detect_0delta:
	case rio_em_detect_last:
	default:
		assert_true(false);
	}
}

static void chk_regs_2many_pna(rio_em_cfg_t *event, rio_port_t port)
{
	uint32_t ctl;
	uint32_t mask = RXS_PLM_SPX_DENIAL_CTL_CNT_PNA;
	uint32_t thresh;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_DENIAL_CTL(port), &ctl));
	thresh = ctl & RXS_PLM_SPX_DENIAL_CTL_DENIAL_THRESH;

	switch (event->em_detect) {
	case rio_em_detect_off:
		assert_int_equal(0, ctl & mask);
		break;
	case rio_em_detect_on:
		assert_int_equal(thresh, event->em_info);
		assert_int_equal(mask, ctl & mask);
		break;
	case rio_em_detect_0delta:
	case rio_em_detect_last:
	default:
		assert_true(false);
	}
}

static void chk_regs_err_rate(rio_em_cfg_t *event, rio_port_t port)
{
	uint32_t int_en;
	uint32_t pw_en;
	uint32_t p_mask = RXS_PBM_SPX_PW_EN_EG_DNFL_FATAL |
			RXS_PBM_SPX_PW_EN_EG_DOH_FATAL |
			RXS_PBM_SPX_PW_EN_EG_DATA_UNCOR;
	uint32_t i_mask = RXS_PBM_SPX_INT_EN_EG_DNFL_FATAL |
			RXS_PBM_SPX_INT_EN_EG_DOH_FATAL |
			RXS_PBM_SPX_INT_EN_EG_DATA_UNCOR;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PBM_SPX_PW_EN(port), &pw_en));
	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PBM_SPX_INT_EN(port), &int_en));

	int_en &= i_mask;
	pw_en &= p_mask;

	if (rio_em_detect_on == event->em_detect) {
		assert_int_equal(p_mask, pw_en);
		assert_int_equal(i_mask, int_en);
	} else {
		assert_int_equal(0, pw_en);
		assert_int_equal(0, int_en);
	}
}

static void chk_regs_log(rio_em_cfg_t *event)
{
	uint32_t err_en;
	uint32_t mask = RXS_ERR_EN_ILL_TYPE_EN |
			RXS_ERR_EN_UNS_RSP_EN |
			RXS_ERR_EN_ILL_ID_EN;

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_ERR_EN, &err_en));

	if (rio_em_detect_on == event->em_detect) {
		assert_int_equal(event->em_info, err_en & mask);
	} else {
		assert_int_equal(0, err_en & mask);
	}
}

static void chk_regs_sig_det(rio_em_cfg_t *event, rio_port_t port)
{
	uint32_t rate_en;
	uint32_t mask = RXS_SPX_ERR_DET_LINK_INIT;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_SPX_RATE_EN(port), &rate_en));

	rate_en &= mask;
	if (rio_em_detect_on == event->em_detect) {
		assert_int_equal(mask, rate_en);
	} else {
		assert_int_equal(0, rate_en);
	}
}

static void chk_regs_rst_req(rio_em_cfg_t *event, rio_port_t port)
{
	uint32_t plm_ctl;
	uint32_t mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
			RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port),
								&plm_ctl));

	// An event can be detected as long as at least one of the self
	// reset bits is cleared.
	if (rio_em_detect_on == event->em_detect) {
		assert_int_not_equal(mask, plm_ctl & mask);
	} else {
		assert_int_equal(mask, plm_ctl & mask);
	}
}

static void chk_regs_init_fail(rio_em_cfg_t *event)
{
	uint32_t i2c_i_en;
	uint32_t mask = I2C_INT_ENABLE_BL_FAIL;

	assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, I2C_INT_ENABLE, &i2c_i_en));

	i2c_i_en &= mask;
	if (rio_em_detect_on == event->em_detect) {
		assert_int_equal(mask, i2c_i_en);
	} else {
		assert_int_equal(0, i2c_i_en);
	}
}

static void chk_regs_ttl(rio_em_cfg_t *event,
			rio_port_t port,
			rio_em_notfn_ctl_t ntfn)
{
	uint32_t ttl_tmr;
	uint32_t int_en;
	uint32_t pw_en;
	uint32_t i_mask = RXS_PBM_SPX_INT_EN_EG_TTL_EXPIRED;
	uint32_t p_mask = RXS_PBM_SPX_PW_EN_EG_TTL_EXPIRED;
	uint32_t tval = (((event->em_info + 5999) / 6000) -1 ) * 4;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PKT_TIME_LIVE, &ttl_tmr));
	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PBM_SPX_INT_EN(port), &int_en));
	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PBM_SPX_PW_EN(port), &pw_en));

	if (!tval) {
		tval = 1;
	}
	if (tval > 0xFFFC) {
		tval = 0xFFFC;
	}

	if (rio_em_detect_on == event->em_detect) {
		assert_int_equal(tval << 16, ttl_tmr);
	} else {
		assert_int_equal(0, ttl_tmr);
	}

	int_en &= i_mask;
	pw_en &= p_mask;

	switch (ntfn) {
	case rio_em_notfn_none:
		assert_false(int_en);
		assert_false(pw_en);
		break;
	case rio_em_notfn_int:
		assert_true(int_en);
		assert_false(pw_en);
		break;
	case rio_em_notfn_pw:
		assert_false(int_en);
		assert_true(pw_en);
		break;
	case rio_em_notfn_both:
		assert_true(int_en);
		assert_true(pw_en);
		break;
	case rio_em_notfn_0delta:
	case rio_em_notfn_last:
	default:
		assert_true(false);
		break;
	}
}

static void chk_regs_rte(rio_port_t port,
			rio_em_notfn_ctl_t ntfn)
{
	uint32_t int_en;
	uint32_t pw_en;
	uint32_t i_mask = RXS_TLM_SPX_INT_EN_LUT_DISCARD;
	uint32_t p_mask = RXS_TLM_SPX_PW_EN_LUT_DISCARD;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_TLM_SPX_INT_EN(port), &int_en));
	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_TLM_SPX_PW_EN(port), &pw_en));

	int_en &= i_mask;
	pw_en &= p_mask;

	switch (ntfn) {
	case rio_em_notfn_none:
		assert_false(int_en);
		assert_false(pw_en);
		break;
	case rio_em_notfn_int:
		assert_true(int_en);
		assert_false(pw_en);
		break;
	case rio_em_notfn_pw:
		assert_false(int_en);
		assert_true(pw_en);
		break;
	case rio_em_notfn_both:
		assert_true(int_en);
		assert_true(pw_en);
		break;
	case rio_em_notfn_0delta:
	case rio_em_notfn_last:
	default:
		assert_true(false);
		break;
	}
}

static void rxs_rio_em_cfg_set_get_chk_regs(rio_em_cfg_t *event,
						rio_em_notfn_ctl_t ntfn,
						rio_port_t port)
{
	bool chk_plm_en = false;
	uint32_t plm_mask = 0;

	assert_in_range(event->em_event, rio_em_f_los, rio_em_a_no_event);

	switch (event->em_detect) {
	case rio_em_detect_off:
		ntfn = rio_em_notfn_none;
		break;
	case rio_em_detect_on:
		break;
	case rio_em_detect_0delta:
	case rio_em_detect_last:
	default:
		assert_true(false);
	}

	switch (event->em_event) {
	case rio_em_f_los:
		chk_regs_los(event, port);
		chk_plm_en = true;
		plm_mask = RXS_PLM_SPX_STAT_DWNGD |
			RXS_PLM_SPX_STAT_OK_TO_UNINIT |
			RXS_PLM_SPX_STAT_DLT;
		break;
	case rio_em_f_port_err:
		chk_plm_en = true;
		plm_mask = RXS_PLM_SPX_STAT_PORT_ERR;
		break;
	case rio_em_f_2many_retx:
		chk_regs_2many_retx(event, port);
		chk_plm_en = true;
		plm_mask = RXS_PLM_SPX_STAT_MAX_DENIAL;
		break;
	case rio_em_f_2many_pna:
		chk_regs_2many_pna(event, port);
		chk_plm_en = true;
		plm_mask = RXS_PLM_SPX_STAT_MAX_DENIAL;
		break;
	case rio_em_f_err_rate:
		chk_regs_err_rate(event, port);
		break;
	case rio_em_d_ttl:
		chk_regs_ttl(event, port, ntfn);
		break;
	case rio_em_d_rte:
		chk_regs_rte(port, ntfn);
		break;
	case rio_em_d_log:
		chk_regs_log(event);
		break;
	case rio_em_i_sig_det:
		chk_regs_sig_det(event, port);
		chk_plm_en = true;
		plm_mask = RXS_PLM_SPX_STAT_LINK_INIT;
		break;
	case rio_em_i_rst_req:
		chk_regs_rst_req(event, port);
		break;
	case rio_em_i_init_fail:
		chk_regs_init_fail(event);
		break;
	case rio_em_a_clr_pwpnd:
		break;
	case rio_em_a_no_event:
		break;
	default:
		assert_true(false);
	}

	if (chk_plm_en) {
		chk_plm_event_enables(ntfn, plm_mask, port);
	}
}

typedef struct port_fail_checks_t_TAG {
	uint32_t sp_rate_en;
	uint32_t sp_rate;
	uint32_t sp_thresh;
	rio_em_cfg_t event;
} port_fail_checks_t;

// Verify various values for successfully configuring a Tsi721 event.
// Note that this test will not configure interrupt notofication if
// it is not supported.

static void rxs_rio_em_cfg_set_success_em_info_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_cfg_get_in_t get_cfg_in;
	rio_em_cfg_get_out_t get_cfg_out;
	rio_em_notfn_ctl_t set_chk_notfn = rio_em_notfn_both;
	uint32_t chk_em_info;
	rio_em_events_t get_cfg_event_req;
	rio_em_cfg_t get_cfg_event_info;
	rio_port_t port;

	rio_em_cfg_t events[] = {
		{rio_em_f_los, rio_em_detect_on, 0 * 0x253 },  // 0
		{rio_em_f_los, rio_em_detect_on, 1 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 2 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 3 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 4 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 5 * 0x253 }, // 5
		{rio_em_f_los, rio_em_detect_on, 6 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 7 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 9 * 0x253 },
		{rio_em_f_los, rio_em_detect_on, 100 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_on, 200 * 0x253 * 1000}, // 10
		{rio_em_f_los, rio_em_detect_on, 300 * 0x253 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on, 0xffff},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0001},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x8888}, // 15
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0001},
		{rio_em_f_2many_pna, rio_em_detect_on, 0xFFFE},
		{rio_em_f_2many_pna, rio_em_detect_on, 0xFFFF},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x809E},
		{rio_em_d_log, rio_em_detect_on,
					RXS_ERR_EN_ILL_TYPE_EN | // 20
					RXS_ERR_EN_UNS_RSP_EN |
					RXS_ERR_EN_ILL_ID_EN},
		{rio_em_d_log, rio_em_detect_on, RXS_ERR_EN_ILL_TYPE_EN},
		{rio_em_d_log, rio_em_detect_on, RXS_ERR_EN_UNS_RSP_EN},
		{rio_em_d_log, rio_em_detect_on, RXS_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0}, // 25
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{rio_em_f_los, rio_em_detect_off,              0},
		{rio_em_f_los, rio_em_detect_off, 1 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 2 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 3 * 0x253 * 1000}, // 30
		{rio_em_f_los, rio_em_detect_off, 4 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 5 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 6 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 7 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 9 * 0x253 * 1000}, // 35
		{rio_em_f_los, rio_em_detect_off, 100 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 200 * 0x253 * 1000},
		{rio_em_f_los, rio_em_detect_off, 300 * 0x253 * 1000},
		{rio_em_f_port_err, rio_em_detect_off, 0},
		{rio_em_f_2many_retx, rio_em_detect_off, 0x0000}, // 40
		{rio_em_f_2many_retx, rio_em_detect_off, 0x0000},
		{rio_em_f_2many_retx, rio_em_detect_off, 0x0000},
		{rio_em_f_2many_pna, rio_em_detect_off, 0x0000},
		{rio_em_f_2many_pna, rio_em_detect_off, 0x0000},
		{rio_em_f_2many_pna, rio_em_detect_off, 0x0000}, // 45
		{rio_em_f_2many_pna, rio_em_detect_off, 0x0000},
		{rio_em_d_log, rio_em_detect_off, 0},
		{rio_em_i_sig_det, rio_em_detect_off, 0},
		{rio_em_i_rst_req, rio_em_detect_off, 0},
		{rio_em_a_clr_pwpnd, rio_em_detect_off, 0}, // 50
		{rio_em_d_ttl, rio_em_detect_on, 1 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 2 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 3 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 4 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 5 * 6000}, // 55
		{rio_em_d_ttl, rio_em_detect_on, 0x0800 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 0x1000 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 0x2000 * 6000},
		{rio_em_d_ttl, rio_em_detect_on, 0x4000 * 6000},
		{rio_em_d_ttl, rio_em_detect_off, 0}, // 60
		{rio_em_d_rte, rio_em_detect_on, 0},
		{rio_em_d_rte, rio_em_detect_off, 0},
		{rio_em_a_no_event, rio_em_detect_off, 0},
		{rio_em_i_init_fail, rio_em_detect_off, 0},
	};

	unsigned int num_events = sizeof(events) / sizeof(events[0]);
	unsigned int i;
	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST;

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	// For each event in the prodigous list above
	for (i = 0; i < num_events; i++) {
		port = i % NUM_RXS_PORTS(&mock_dev_info);
		if (DEBUG_PRINTF) {
			printf(
			"\nevent idx %d event %d port %d onoff %d info 0x%x\n",
			i, events[i].em_event, port,
			events[i].em_detect,
			events[i].em_info);
		}
		if (rio_em_i_rst_req == events[i].em_event) {
			// If we're testing disabling the Reset Request
			// event, do the real disable since this events
			// detection is actually controlled by Port Config
			// functionality.

			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
					RXS_PLM_SPX_IMP_SPEC_CTL(port),
					&plm_imp_spec_ctl));
			if (rio_em_detect_off == events[i].em_detect) {
				plm_imp_spec_ctl |= t_mask;
			} else {
				plm_imp_spec_ctl &= ~t_mask;
			}
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
					RXS_PLM_SPX_IMP_SPEC_CTL(port),
					plm_imp_spec_ctl));
		}

		// Set the event configuration specified
		set_chk_notfn = rio_em_notfn_both;
		set_chk_notfn = (events[i].em_detect == rio_em_detect_on) ?
				set_chk_notfn : rio_em_notfn_none;

		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = port;
		set_cfg_in.notfn = (events[i].em_detect == rio_em_detect_on) ?
						rio_em_notfn_both :
						rio_em_notfn_none;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &events[i];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		// Check the returned configuration
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(set_chk_notfn, set_cfg_out.notfn);

		// Check the register values for the event
		rxs_rio_em_cfg_set_get_chk_regs(&events[i], set_chk_notfn, port);

		// Get the event configuration
		get_cfg_in.port_num = port;
		get_cfg_in.num_events = 1;
		get_cfg_in.event_list = &get_cfg_event_req;
		get_cfg_event_req = events[i].em_event;
		get_cfg_in.events = &get_cfg_event_info;

		get_cfg_out.imp_rc = 0xFFFFFFFF;
		get_cfg_out.fail_idx = 0xFF;
		get_cfg_out.notfn = (rio_em_notfn_ctl_t)0xFF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_get(&mock_dev_info,
						&get_cfg_in, &get_cfg_out));
		assert_int_equal(0, get_cfg_out.imp_rc);
		assert_int_equal(rio_em_last, get_cfg_out.fail_idx);
		assert_int_equal(set_chk_notfn, get_cfg_out.notfn);

		// Check the returned configuration
		if (rio_em_detect_on == events[i].em_detect) {
			chk_em_info = events[i].em_info;
		} else {
			chk_em_info = 0;
		}
		assert_int_equal(events[i].em_event,
			get_cfg_in.events[0].em_event);
		assert_int_equal(events[i].em_detect,
			get_cfg_in.events[0].em_detect);
		assert_int_equal(chk_em_info, get_cfg_in.events[0].em_info);
	}
	(void)state;
}
// Check that some events that should be ignored can be successfully
// configured.

static void rxs_rio_em_cfg_set_ignore_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_cfg_get_in_t get_cfg_in;
	rio_em_cfg_get_out_t get_cfg_out;
	rio_em_notfn_ctl_t set_chk_notfn = rio_em_notfn_both;
	rio_em_events_t get_cfg_event_req;
	rio_em_cfg_t get_cfg_event_info;

	rio_em_cfg_t events[] = {
		{rio_em_a_no_event, rio_em_detect_on, 0},
		{rio_em_a_no_event, rio_em_detect_off, 0},
	};

	unsigned int num_events = sizeof(events) / sizeof(events[0]);
	unsigned int i;
	rio_port_t port = 0;

	assert_in_range(num_events, 0, NUM_RXS_PORTS(&mock_dev_info) - 1);

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	for (i = 0; i < num_events; i++) {
		if (DEBUG_PRINTF) {
			printf("\nevent idx %d\n",  i);
		}
		port = i;
		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = port;
		set_cfg_in.notfn = rio_em_notfn_both;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &events[i];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(set_chk_notfn, set_cfg_out.notfn);

		get_cfg_in.port_num = port;
		get_cfg_in.num_events = 1;
		get_cfg_event_req = events[i].em_event;
		get_cfg_in.event_list = &get_cfg_event_req;
		get_cfg_in.events = &get_cfg_event_info;

		get_cfg_out.imp_rc = 0xFFFFFFFF;
		get_cfg_out.fail_idx = 0xFF;
		get_cfg_out.notfn = (rio_em_notfn_ctl_t)0xFF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_get(&mock_dev_info,
						&get_cfg_in, &get_cfg_out));
		assert_int_equal(0, get_cfg_out.imp_rc);
		assert_int_equal(rio_em_last, get_cfg_out.fail_idx);
		assert_int_equal(set_chk_notfn, get_cfg_out.notfn);

		assert_int_equal(events[i].em_event,
				get_cfg_in.events[0].em_event);
		assert_int_equal(rio_em_detect_off,
				get_cfg_in.events[0].em_detect);
		assert_int_equal(events[i].em_info,
				get_cfg_in.events[0].em_info);
	}
	(void)state;
}

typedef struct err_rate_checks_t_TAG {
	uint32_t sp_rate_en;
	uint32_t sp_rate;
	uint32_t sp_thresh;
	rio_em_cfg_t event;
} err_rate_checks_t;

// Test various illegal (generally 0) em_info parameters are detected
// and reported.

static void rxs_rio_em_cfg_set_fail_em_info_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;

	rio_em_cfg_t events[] = {
		{rio_em_f_2many_retx, rio_em_detect_on, 0},
		{rio_em_f_2many_pna, rio_em_detect_on, 0},
		{rio_em_d_log, rio_em_detect_on, 0},
		{rio_em_d_ttl, rio_em_detect_on, 0},
		{rio_em_last, rio_em_detect_on, 0},
	};
	rio_em_cfg_t pass_events[2];
	rio_port_t port;
	rio_port_t chk_port;

	unsigned int num_events = sizeof(events) / sizeof(events[0]);
	unsigned int i;

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	pass_events[0].em_event = rio_em_a_no_event;
	pass_events[0].em_detect = rio_em_detect_0delta;
	pass_events[0].em_info = 0;

	for (i = 0; i < num_events; i++) {
		if (DEBUG_PRINTF) {
			printf("\nevent idx %d\n",  i);
		}
		port = i;
		if ((rio_em_d_log == events[i].em_event) ||
		    (rio_em_d_ttl == events[i].em_event)) {
			chk_port = RIO_ALL_PORTS;
		} else {
			chk_port = port;
		}
		memcpy(&pass_events[1], &events[i], sizeof(pass_events[0]));
		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = port;
		set_cfg_in.notfn = rio_em_notfn_both;
		set_cfg_in.num_events = 2;
		set_cfg_in.events = &pass_events[0];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xFF;

		assert_int_not_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_not_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(chk_port, set_cfg_out.fail_port_num);
		assert_int_equal(1, set_cfg_out.fail_idx);
	}
	(void)state;
}

typedef struct em_info_diff_checks_t_TAG {
	uint32_t chk_info;
	rio_em_cfg_t event;
} em_info_diff_checks_t;

// Test various illegal (generally 0) em_info parameters are detected
// and reported.

static void rxs_rio_em_cfg_set_roundup_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_cfg_get_in_t get_cfg_in;
	rio_em_cfg_get_out_t get_cfg_out;
	rio_em_notfn_ctl_t set_chk_notfn = rio_em_notfn_both;
	rio_em_events_t get_cfg_event_req;
	rio_em_cfg_t get_cfg_event_info;

	em_info_diff_checks_t events[] = {
		{0x0000FFFF, {rio_em_f_2many_retx, rio_em_detect_on, 0x10000}},
		{0x0000FFFF, {rio_em_f_2many_retx, rio_em_detect_on, 0xFFFFF}},
		{0x0000FFFF, {rio_em_f_2many_pna, rio_em_detect_on, 0x10000}},
		{0x0000FFFF, {rio_em_f_2many_pna, rio_em_detect_on, 0xFFFFF}},
		{0x253, {rio_em_f_los, rio_em_detect_on, 1}},
		{0x253, {rio_em_f_los, rio_em_detect_on, 0x252}},
		{0x253, {rio_em_f_los, rio_em_detect_on, 0x253}},
		{0x4A6, {rio_em_f_los, rio_em_detect_on, 0x254}},
		{0x4A6, {rio_em_f_los, rio_em_detect_on, 0x4a5}},
		{0x4A6, {rio_em_f_los, rio_em_detect_on, 0x4a6}},
		{0x6F9, {rio_em_f_los, rio_em_detect_on, 0x4a7}}, // 10
		{0xFFFFFF00, {rio_em_f_los, rio_em_detect_on, 0xFFFFFF00}},
		{0xFFFFFFFF, {rio_em_f_los, rio_em_detect_on, 0xFFFFFF01}},
		{0xFFFFFFFF, {rio_em_f_los, rio_em_detect_on, 0xFFFFFFFF}},
		{6000, {rio_em_d_ttl, rio_em_detect_on, 1}},
		{6000, {rio_em_d_ttl, rio_em_detect_on, 5999}},
		{6000, {rio_em_d_ttl, rio_em_detect_on, 6000}},
		{12000, {rio_em_d_ttl, rio_em_detect_on, 6001}},
		{0x4000 * 6000, {rio_em_d_ttl, rio_em_detect_on, 0x20000000}},
		{0x4000 * 6000, {rio_em_d_ttl, rio_em_detect_on, 0xFFFFFFFF}},
	};

	unsigned int num_events = sizeof(events) / sizeof(events[0]);
	unsigned int i;
	rio_port_t port;

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	for (i = 0; i < num_events; i++) {
		port = i & NUM_RXS_PORTS(&mock_dev_info);;
		if (DEBUG_PRINTF) {
			printf("\nevent idx %d port %d\n",  i, port);
		}
		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = port;
		set_cfg_in.notfn = rio_em_notfn_both;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &events[i].event;

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xFF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(set_chk_notfn, set_cfg_out.notfn);

		events[i].event.em_info = events[i].chk_info;
		rxs_rio_em_cfg_set_get_chk_regs(&events[i].event,
				set_chk_notfn, port);

		get_cfg_in.port_num = port;
		get_cfg_in.num_events = 1;
		get_cfg_in.event_list = &get_cfg_event_req;
		get_cfg_event_req = events[i].event.em_event;
		get_cfg_in.events = &get_cfg_event_info;
		get_cfg_out.imp_rc = 0xFFFFFFFF;
		get_cfg_out.fail_idx = 0xFF;
		get_cfg_out.notfn = (rio_em_notfn_ctl_t)0xFF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_get(&mock_dev_info,
						&get_cfg_in, &get_cfg_out));
		assert_int_equal(0, get_cfg_out.imp_rc);
		assert_int_equal(rio_em_last, get_cfg_out.fail_idx);
		assert_int_equal(set_chk_notfn, get_cfg_out.notfn);

		assert_int_equal(events[i].event.em_event,
				get_cfg_in.events[0].em_event);
		assert_int_equal(events[i].event.em_detect,
				get_cfg_in.events[0].em_detect);
		assert_int_equal(events[i].event.em_info,
				get_cfg_in.events[0].em_info);
	}
	(void)state;
}

// Test bad parameter values are detected and reported.

static void rxs_rio_em_cfg_get_bad_parms_test(void **state)
{
	rio_em_cfg_get_in_t p_in;
	rio_em_cfg_get_out_t p_out;
	rio_em_events_t get_cfg_event_req;
	rio_em_cfg_t get_cfg_event_info;

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	// Bad port number
	p_in.port_num = NUM_RXS_PORTS(&mock_dev_info) + 1;
	p_in.num_events = 1;
	p_in.event_list = &get_cfg_event_req;
	get_cfg_event_req = rio_em_f_2many_pna;
	p_in.events = &get_cfg_event_info;

	p_out.imp_rc = 0;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_get(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// Bad number of events
	p_in.port_num = 0;
	p_in.num_events = 0;

	p_out.imp_rc = 0;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_get(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// NULL event list pointer
	p_in.num_events = 1;
	p_in.event_list = NULL;

	p_out.imp_rc = 0;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_get(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// NULL events pointer
	p_in.num_events = 1;
	p_in.event_list = &get_cfg_event_req;
	p_in.events = NULL;

	p_out.imp_rc = 0;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_get(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// Bad event in list
	p_in.num_events = 1;
	p_in.event_list = &get_cfg_event_req;
	get_cfg_event_req = rio_em_last;
	p_in.events = &get_cfg_event_info;

	p_out.imp_rc = 0;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_get(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(0, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	(void)state;
}

// Test bad parameter values are detected and reported.

static void rxs_rio_em_cfg_set_bad_parms_test(void **state)
{
	rio_em_cfg_set_in_t p_in;
	rio_em_cfg_set_out_t p_out;
	rio_em_cfg_t events = {rio_em_f_2many_pna, rio_em_detect_on, 0x0100};

	// Bad number of ports
	p_in.ptl.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;
	p_in.ptl.pnums[0] = 0;
	p_in.ptl.pnums[1] = 0;
	p_in.num_events = 1;
	p_in.events = &events;

	p_out.imp_rc = 0;
	p_out.fail_port_num = 0x99;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_set(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(RIO_ALL_PORTS, p_out.fail_port_num);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// Bad pnum
	p_in.ptl.num_ports = 1;
	p_in.ptl.pnums[0] = NUM_RXS_PORTS(&mock_dev_info);
	p_in.num_events = 1;
	p_in.events = &events;

	p_out.imp_rc = 0;
	p_out.fail_port_num = 0x99;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_set(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(RIO_ALL_PORTS, p_out.fail_port_num);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// Bad notification value
	p_in.ptl.num_ports = 1;
	p_in.ptl.pnums[0] = 0;
	p_in.notfn = rio_em_notfn_last;
	p_in.num_events = 1;
	p_in.events = &events;

	p_out.imp_rc = 0;
	p_out.fail_port_num = 0x99;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_set(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(RIO_ALL_PORTS, p_out.imp_rc);
	assert_int_equal(RIO_ALL_PORTS, p_out.fail_port_num);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// Bad number of events
	p_in.ptl.num_ports = 1;
	p_in.ptl.pnums[0] = 0;
	p_in.notfn = rio_em_notfn_none;
	p_in.num_events = 0;
	p_in.events = &events;

	p_out.imp_rc = 0;
	p_out.fail_port_num = 0x99;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_set(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(RIO_ALL_PORTS, p_out.fail_port_num);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	// Bad event pointer
	p_in.ptl.num_ports = 1;
	p_in.ptl.pnums[0] = 0;
	p_in.notfn = rio_em_notfn_last;
	p_in.num_events = 1;
	p_in.events = NULL;

	p_out.imp_rc = 0;
	p_out.fail_port_num = 0x99;
	p_out.fail_idx = 0xFF;
	p_out.notfn = (rio_em_notfn_ctl_t)0xFF;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_cfg_set(&mock_dev_info, &p_in, &p_out));
	assert_int_not_equal(0, p_out.imp_rc);
	assert_int_equal(RIO_ALL_PORTS, p_out.fail_port_num);
	assert_int_equal(rio_em_last, p_out.fail_idx);
	assert_int_equal(rio_em_notfn_0delta, p_out.notfn);

	(void)state;
}

static void rxs_rio_em_dev_rpt_ctl_chk_regs_port(rio_em_notfn_ctl_t chk_notfn,
					rio_port_t port)
{
	const uint32_t p_pw_mask = RXS_SPX_ERR_STAT_PORT_W_DIS;
	const uint32_t p_int_mask = RXS_PLM_SPX_ALL_INT_EN_IRQ_EN;
	uint32_t err_st;
	uint32_t int_en;

	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_SPX_ERR_STAT(port), &err_st));
	assert_int_equal(RIO_SUCCESS,
		DARRegRead(&mock_dev_info, RXS_PLM_SPX_ALL_INT_EN(port),
								&int_en));
	err_st &= p_pw_mask;
	int_en &= p_int_mask;
	switch (chk_notfn) {
	case rio_em_notfn_none:
		assert_int_equal(0, int_en);
		assert_int_equal(p_pw_mask, err_st);
		break;

	case rio_em_notfn_int:
		assert_int_equal(p_int_mask, int_en);
		assert_int_equal(p_pw_mask, err_st);
		break;

	case rio_em_notfn_pw:
		assert_int_equal(0, int_en);
		assert_int_equal(0, err_st);
		break;

	case rio_em_notfn_both:
		assert_int_equal(p_int_mask, int_en);
		assert_int_equal(0, err_st);
		break;

	default:
		assert_true(false);
		break;
	}
}

static void rxs_rio_em_dev_rpt_ctl_chk_regs(rio_em_notfn_ctl_t chk_notfn,
					DAR_ptl *ptl)
{
	const uint32_t pw_mask = RXS_PW_TRAN_CTL_PW_DIS;
	const uint32_t int_mask = RXS_EM_DEV_INT_EN_INT_EN;
	DAR_ptl good_ptl;
	unsigned int port_idx;
	rxs_rpt_ctl_regs_t regs;

	assert_int_equal(RIO_SUCCESS,
		DARrioGetPortList(&mock_dev_info, ptl, &good_ptl));
	assert_int_equal(RIO_SUCCESS,
		rxs_rio_em_dev_rpt_ctl_reg_read(&mock_dev_info, &regs));

	switch (chk_notfn) {
	case rio_em_notfn_none:
		assert_int_equal(0, regs.dev_int_en);
		assert_int_equal(pw_mask, pw_mask & regs.pw_trans_dis);
		break;

	case rio_em_notfn_int:
		assert_int_equal(int_mask, int_mask & regs.dev_int_en);
		assert_int_equal(pw_mask, pw_mask & regs.pw_trans_dis);
		break;

	case rio_em_notfn_pw:
		assert_int_equal(0, regs.dev_int_en);
		assert_int_equal(0, regs.pw_trans_dis);
		break;

	case rio_em_notfn_both:
		assert_int_equal(int_mask, int_mask & regs.dev_int_en);
		assert_int_equal(0, regs.pw_trans_dis);
		break;

	default:
		assert_true(false);
		break;
	}

	for (port_idx = 0; port_idx < good_ptl.num_ports; port_idx++) {
		if (DEBUG_PRINTF) {
			printf("\nNotfn %d Port %d\n", chk_notfn,
						good_ptl.pnums[port_idx]);
		}
		rxs_rio_em_dev_rpt_ctl_chk_regs_port(chk_notfn,
						good_ptl.pnums[port_idx]);
	}
}

// Test that port-write/interrupt reporting control is working.
// Updates status for all ports.

static void rxs_rio_em_dev_rpt_ctl_success_test(void **state)
{
	rio_em_dev_rpt_ctl_in_t in_p;
	rio_em_dev_rpt_ctl_out_t out_p;
	rio_em_notfn_ctl_t notfn, chk_notfn;

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	in_p.ptl.num_ports = RIO_ALL_PORTS;
	for (notfn = rio_em_notfn_none; notfn < rio_em_notfn_0delta; notfn =
			(rio_em_notfn_ctl_t)(notfn + 1)) {
		if (DEBUG_PRINTF) {
			printf("\nnotfn %d\n", notfn);
		}
		in_p.notfn = notfn;
		chk_notfn = notfn;
		out_p.imp_rc = 0xFFFFFFFF;
		out_p.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p, &out_p));

		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(chk_notfn, out_p.notfn);

		rxs_rio_em_dev_rpt_ctl_chk_regs(chk_notfn, &in_p.ptl);

		// Repeat check with 0delta
		if (DEBUG_PRINTF) {
			printf("\nnotfn %d 0delta\n", notfn);
		}
		in_p.notfn = rio_em_notfn_0delta;
		out_p.imp_rc = 0xFFFFFFFF;
		out_p.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p,
						&out_p));
		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(chk_notfn, out_p.notfn);

		rxs_rio_em_dev_rpt_ctl_chk_regs(chk_notfn, &in_p.ptl);
	}
	(void)state;
}

// Test that port-write/interrupt reporting control is working.
// Updates status for half the ports.
// Checks that other half is not changed.

static void rxs_rio_em_dev_rpt_ctl_oddport_test(void **state)
{
	rio_em_dev_rpt_ctl_in_t in_p;
	rio_em_dev_rpt_ctl_out_t out_p;
	rio_em_notfn_ctl_t notfn, chk_notfn;
	rio_port_t port;
	unsigned int port_idx;
	DAR_ptl other_ptl;

	// Power up and enable all ports...
	set_all_port_config(cfg_perfect, false, false, RIO_ALL_PORTS);

	// Set all ports to "both" int and pw notification
	in_p.ptl.num_ports = RIO_ALL_PORTS;
	in_p.notfn = rio_em_notfn_both;
	out_p.imp_rc = 0xFFFFFFFF;
	out_p.notfn = (rio_em_notfn_ctl_t)0xF;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p, &out_p));

	assert_int_equal(0, out_p.imp_rc);
	assert_int_equal(rio_em_notfn_both, out_p.notfn);

	// Divide ports in half, some are updated, others not so much...
	in_p.ptl.num_ports = 0;
	other_ptl.num_ports = 0;
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		if (port & 1) {
			in_p.ptl.pnums[in_p.ptl.num_ports++] = port;
		} else {
			other_ptl.pnums[other_ptl.num_ports++] = port;
		}
	}

	for (notfn = rio_em_notfn_none; notfn < rio_em_notfn_0delta; notfn =
			(rio_em_notfn_ctl_t)(notfn + 1)) {
		in_p.notfn = notfn;
		chk_notfn = notfn;
		out_p.imp_rc = 0xFFFFFFFF;
		out_p.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p,
						&out_p));

		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(chk_notfn, out_p.notfn);

		rxs_rio_em_dev_rpt_ctl_chk_regs(chk_notfn, &in_p.ptl);

		for (port_idx = 0; port_idx < other_ptl.num_ports; port_idx++) {
			rxs_rio_em_dev_rpt_ctl_chk_regs_port(rio_em_notfn_both,
					other_ptl.pnums[port_idx]);
		}
	}
	(void)state;
}

// Test bad parameter values are detected and reported.

static void rxs_rio_em_dev_rpt_ctl_bad_parms_test(void **state)
{
	rio_em_dev_rpt_ctl_in_t in_p;
	rio_em_dev_rpt_ctl_out_t out_p;

	// Bad number of ports
	in_p.ptl.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;
	in_p.notfn = rio_em_notfn_last;

	out_p.imp_rc = 0;
	out_p.notfn = rio_em_notfn_last;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p, &out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(rio_em_notfn_last, out_p.notfn);

	// Bad port number
	in_p.ptl.num_ports = 1;
	in_p.ptl.pnums[0] = NUM_RXS_PORTS(&mock_dev_info);
	in_p.notfn = rio_em_notfn_none;

	out_p.imp_rc = 0;
	out_p.notfn = rio_em_notfn_last;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(rio_em_notfn_last, out_p.notfn);

	// Bad notification type
	in_p.ptl.num_ports = RIO_ALL_PORTS;
	in_p.notfn = rio_em_notfn_last;

	out_p.imp_rc = 0;
	out_p.notfn = rio_em_notfn_last;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(rio_em_notfn_last, out_p.notfn);

	// Another bad notification type
	in_p.ptl.num_ports = RIO_ALL_PORTS;
	in_p.notfn = (rio_em_notfn_ctl_t)((uint8_t)rio_em_notfn_last + 1);

	out_p.imp_rc = 0;
	out_p.notfn = rio_em_notfn_last;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_dev_rpt_ctl(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(rio_em_notfn_last, out_p.notfn);

	(void)state;
}

// Test that a port-write with no events is parsed correctly.

static void rxs_rio_em_parse_pw_no_events_test(void **state)
{
	rio_em_parse_pw_in_t in_p;
	rio_em_parse_pw_out_t out_p;
	rio_em_event_n_loc_t events[(int)rio_em_last];

	memset(in_p.pw, 0, sizeof(in_p.pw));
	in_p.num_events = (int)rio_em_last;
	memset(events, 0, sizeof(events));
	in_p.events = events;

	out_p.imp_rc = 0xFFFFFFFF;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_equal(RIO_SUCCESS,
			rxs_rio_em_parse_pw(&mock_dev_info, &in_p, &out_p));

	assert_int_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	(void)state;
}

typedef struct parse_pw_info_t_TAG {
	uint32_t pw[4];
	rio_em_event_n_loc_t event;
} parse_pw_info_t;

// Test that a port-write with one event is parsed correctly.

static void rxs_rio_em_parse_pw_all_events_test(void **state)
{
	unsigned int i;
	rio_em_parse_pw_in_t in_p;
	rio_em_parse_pw_out_t out_p;
	const unsigned int max_events = 50;
	rio_em_event_n_loc_t events[max_events];
	parse_pw_info_t pws[] = {
		{{	0,
			RXS_SPX_ERR_DET_DLT | RXS_SPX_ERR_DET_OK_TO_UNINIT,
			1 | RXS_PW_OK_TO_UNINIT | RXS_PW_DLT | RXS_PW_DWNGD,
			0},
			{1, rio_em_f_los}},
		{{	0,
			0,
			2 | RXS_PW_PORT_ERR,
			0},
			{2, rio_em_f_port_err}},
		{{	0,
			0,
			3 | RXS_PW_MAX_DENIAL,
			0},
			{3, rio_em_f_2many_retx}},
		{{	0,
			0,
			4 | RXS_PW_PBM_FATAL,
			0},
			{4, rio_em_f_err_rate}},
		{{	0,
			0,
			5 | RXS_PW_PBM_PW,
			0},
			{5, rio_em_d_ttl}},
		{{	0,
			0,
			6 | RXS_PW_TLM_PW,
			0},
			{6, rio_em_d_rte}},
		{{	0,
			0,
			7,
			RXS_ERR_DET_ILL_TYPE |
			RXS_ERR_DET_UNS_RSP |
			RXS_ERR_DET_ILL_ID},
			{RIO_ALL_PORTS, rio_em_d_log}},
		{{	0,
			RXS_SPX_ERR_DET_LINK_INIT,
			8 | RXS_PW_LINK_INIT,
			0},
			{8, rio_em_i_sig_det}},
		{{	0,
			0,
			9 | RXS_PW_RST_REQ | RXS_PW_PRST_REQ,
			0},
			{9, rio_em_i_rst_req}},
		{{	0,
			0,
			10 | RXS_PW_DEV_RCS,
			0},
			{RIO_ALL_PORTS, rio_em_i_rst_req}},
		{{	0,
			0,
			23 | RXS_PW_INIT_FAIL,
			0},
			{RIO_ALL_PORTS, rio_em_i_init_fail}},
	};
	unsigned int num_pws = sizeof(pws) / sizeof(pws[0]);

	// Checking values for "all events" test
	unsigned int num_chk_ev = 0;
	rio_em_event_n_loc_t chk_ev[max_events];
	bool found_it;
	unsigned int j;
	const uint32_t IMP_SPEC_IDX = RIO_EMHS_PW_IMP_SPEC_IDX;
	const uint32_t IMP_SPEC_0_PORT = ~RIO_EMHS_PW_IMP_SPEC_PORT;

	// Make sure the random value chosen for event size is correct.
	assert_in_range(num_pws + 2, 0, max_events);

	// Try each event individually...
	for (i = 0; i < num_pws; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni %d\n", i);
		}
		memcpy(in_p.pw, pws[i].pw, sizeof(in_p.pw));
		in_p.num_events = max_events;
		memset(events, 0, sizeof(events));
		in_p.events = events;

		out_p.imp_rc = 0xFFFFFFFF;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_em_parse_pw(&mock_dev_info, &in_p, &out_p));

		assert_int_equal(0, out_p.imp_rc);
		if (rio_em_f_2many_retx == pws[i].event.event) {
			// Creation of a 2many_pna event also creates an
			// 2many_retx event.
			assert_int_equal(2, out_p.num_events);
			assert_int_equal(pws[i].event.event,
							in_p.events[0].event);
			assert_int_equal(pws[i].event.port_num,
						in_p.events[0].port_num);
			assert_int_equal(rio_em_f_2many_pna,
						in_p.events[1].event);
			// Yes, index for events port_num should be 'i', not 1
			assert_int_equal(pws[i].event.port_num,
						in_p.events[1].port_num);
			assert_false(out_p.too_many);
			assert_false(out_p.other_events);
		} else {
			assert_int_equal(1, out_p.num_events);
			assert_int_equal(pws[i].event.event, in_p.events[0].event);
			assert_int_equal(pws[i].event.port_num,
					in_p.events[0].port_num);
			assert_false(out_p.too_many);
			assert_false(out_p.other_events);
			continue;
		}
	}

	// Formulate port-write with all possible events...
	memcpy(in_p.pw, pws[0].pw, sizeof(in_p.pw));
	memcpy(&chk_ev[num_chk_ev], &pws[0].event, sizeof(chk_ev[0]));
	num_chk_ev++;
	for (i = 1; i < num_pws; i++) {
		for (j = 0; j < RIO_EMHS_PW_WORDS; j++) {
			if (IMP_SPEC_IDX == j) {
				in_p.pw[j] |= pws[i].pw[j] & IMP_SPEC_0_PORT;
			} else {
				in_p.pw[j] |= pws[i].pw[j];
			}
		}
		memcpy(&chk_ev[num_chk_ev], &pws[i].event, sizeof(chk_ev[i]));
		if (pws[i].event.port_num != RIO_ALL_PORTS) {
			chk_ev[num_chk_ev].port_num = chk_ev[0].port_num;
		}
		num_chk_ev++;
		if (rio_em_f_2many_retx == pws[i].event.event) {
			chk_ev[num_chk_ev].port_num = pws[0].event.port_num;
			chk_ev[num_chk_ev].event = rio_em_f_2many_pna;
			num_chk_ev++;
		}
	}

	// Parse Murphy's own port write
	in_p.num_events = max_events;
	memset(events, 0, sizeof(events));
	in_p.events = events;

	out_p.imp_rc = 0xFFFFFFFF;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_em_parse_pw(&mock_dev_info, &in_p, &out_p));

	assert_int_equal(0, out_p.imp_rc);

	assert_int_equal(out_p.num_events, num_chk_ev);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Check that every chk_ev event exists in the events list
	for (i = 0; i < num_chk_ev; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni %d event %d port %d\n",
				i, chk_ev[i].event,chk_ev[i].port_num);
		}
		found_it = false;
		for (j = 0; !found_it && (j < out_p.num_events); j++) {
			found_it = ((chk_ev[i].port_num == events[j].port_num)
				&& (chk_ev[i].event == events[j].event));
			if (DEBUG_PRINTF) {
				if (found_it) {
					printf("\n	j %d\n", j);
				}
			}
		}
		assert_true(found_it);
	}

	(void)state;
}

// Test that a port-write with other, unsupported events is parsed correctly

static void rxs_rio_em_parse_pw_oth_events_test(void **state)
{
	rio_em_parse_pw_in_t in_p;
	rio_em_parse_pw_out_t out_p;
	rio_em_event_n_loc_t events[(int)rio_em_last];
	parse_pw_info_t pws[] = {
	{{0, 0, RXS_PW_ZERO, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_MULTIPORT, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_FAB_OR_DEL, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_DEV_ECC, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_EL_INTA, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_EL_INTB, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_II_CHG_0, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_II_CHG_1, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_II_CHG_2, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_II_CHG_3, 0}, {0, rio_em_f_los}},
	{{0, 0, RXS_PW_PCAP, 0}, {0, rio_em_f_los}},
	};
	unsigned int num_pws = sizeof(pws) / sizeof(pws[0]);
	unsigned int i;

	// Should not have any events, but should have "other" events
	for (i = 0; i < num_pws; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni %d\n", i);
		}
		memcpy(in_p.pw, pws[i].pw, sizeof(in_p.pw));
		in_p.num_events = (int)rio_em_last;
		memset(events, 0, sizeof(events));
		in_p.events = events;

		out_p.imp_rc = 0xFFFFFFFF;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_parse_pw(&mock_dev_info, &in_p,
						&out_p));

		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(0, out_p.num_events);
		assert_int_equal(pws[i].event.event, in_p.events[0].event);
		assert_int_equal(pws[i].event.port_num,
				in_p.events[0].port_num);
		assert_false(out_p.too_many);
		assert_true(out_p.other_events);
	}

	(void)state;
}

// Test bad parameter values are detected and reported.

static void rxs_rio_em_parse_pw_bad_parms_test(void **state)
{
	rio_em_parse_pw_in_t in_p;
	rio_em_parse_pw_out_t out_p;
	rio_em_event_n_loc_t events[(int)rio_em_last];

	// Null "events" pointer
	memset(in_p.pw, 0, sizeof(in_p.pw));
	in_p.num_events = (uint8_t)rio_em_last;
	memset(events, 0, sizeof(events));
	in_p.events = NULL;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_parse_pw(&mock_dev_info, &in_p, &out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// No events allowed
	in_p.num_events = 0;
	in_p.events = events;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_parse_pw(&mock_dev_info, &in_p, &out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Illegal port value in port write
	in_p.num_events = (uint8_t)rio_em_last;
	in_p.pw[RIO_EM_PW_IMP_SPEC_IDX] = RIO_EM_PW_IMP_SPEC_PORT_MASK;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_parse_pw(&mock_dev_info, &in_p, &out_p));
	assert_int_not_equal(RIO_SUCCESS, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	(void)state;
}

// Test that bad parameters are detected and reported
// Also tests that "no_event" is ignored when creating events.

static void rxs_rio_em_create_events_bad_parms_test(void **state)
{
	rio_em_create_events_in_t in_p;
	rio_em_create_events_out_t out_p;
	rio_em_event_n_loc_t events[(uint8_t)rio_em_last];

	// Bad number of events
	in_p.num_events = 0;
	in_p.events = events;

	out_p.imp_rc = 0;
	out_p.failure_idx = 0xff;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_create_events(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0xFF, out_p.failure_idx);

	// NULL event pointer
	in_p.num_events = 1;
	in_p.events = NULL;

	out_p.imp_rc = 0;
	out_p.failure_idx = 0xff;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_create_events(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0xFF, out_p.failure_idx);

	// Illegal port in event
	in_p.num_events = 2;
	in_p.events = events;
	events[0].port_num = 0;
	events[0].event = rio_em_a_no_event;
	events[1].port_num = NUM_RXS_PORTS(&mock_dev_info);
	events[1].event = rio_em_f_los;

	out_p.imp_rc = 0;
	out_p.failure_idx = 0xff;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_create_events(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(1, out_p.failure_idx);

	// Illegal event type in event
	in_p.num_events = 2;
	in_p.events = events;
	events[0].port_num = 0;
	events[0].event = rio_em_a_no_event;
	events[1].port_num = 1;
	events[1].event = rio_em_last;

	out_p.imp_rc = 0;
	out_p.failure_idx = 0xff;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_create_events(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(1, out_p.failure_idx);

	(void)state;
}

// Test that each individual event can be created.

typedef struct addr_and_value_t_TAG {
	uint32_t addr;
	uint32_t val;
} addr_and_value_t;

typedef struct offset_value_event_t_TAG {
	rio_em_event_n_loc_t event;
	addr_and_value_t chk[2];
} offset_value_event_t;

static void rxs_rio_em_create_events_success_test(void **state)
{
	rio_em_create_events_in_t in_p;
	rio_em_create_events_out_t out_p;
	rio_em_event_n_loc_t events[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	offset_value_event_t tests[] = {
		{{0, rio_em_f_los},
			{{RXS_PLM_SPX_STAT(0),
				RXS_PLM_SPX_STAT_OK_TO_UNINIT |
				RXS_PLM_SPX_STAT_DLT |
				RXS_PLM_SPX_STAT_DWNGD},
			{RXS_SPX_ERR_DET(0),
				RXS_PLM_SPX_STAT_DLT |
				RXS_SPX_ERR_DET_OK_TO_UNINIT}}},
		{{1, rio_em_f_port_err},
			{{RXS_PLM_SPX_STAT(1),
				RXS_PLM_SPX_STAT_PORT_ERR},
			{0, 0}}},
		{{2, rio_em_f_2many_pna},
			{{RXS_PLM_SPX_STAT(2),
				RXS_PLM_SPX_STAT_MAX_DENIAL},
			{RXS_PLM_SPX_PNA_CAP(2),
				RXS_PLM_SPX_PNA_CAP_VALID}}},
		{{3, rio_em_f_2many_retx},
			{{RXS_PLM_SPX_STAT(3),
				RXS_PLM_SPX_STAT_MAX_DENIAL},
			{0, 0}}},
		{{4, rio_em_f_err_rate},
			{{RXS_PBM_SPX_STAT(4),
				RXS_PBM_SPX_STAT_EG_DNFL_FATAL |
				RXS_PBM_SPX_STAT_EG_DOH_FATAL |
				RXS_PBM_SPX_STAT_EG_DATA_UNCOR},
			{RXS_PLM_SPX_STAT(4),
				RXS_PLM_SPX_STAT_PBM_FATAL}}},
		{{5, rio_em_d_ttl},
			{{RXS_PBM_SPX_STAT(5),
				RXS_PBM_SPX_STAT_EG_TTL_EXPIRED},
			{0, 0}}},
		{{6, rio_em_d_rte},
			{{RXS_TLM_SPX_STAT(6),
				RXS_TLM_SPX_STAT_LUT_DISCARD},
			{0, 0}}},
		{{RIO_ALL_PORTS, rio_em_d_log},
			{{RXS_ERR_DET,
				RXS_ERR_DET_ILL_TYPE |
				RXS_ERR_DET_UNS_RSP |
				RXS_ERR_DET_ILL_ID},
			{0, 0}}},
		{{8, rio_em_i_sig_det},
			{{RXS_PLM_SPX_STAT(8),
				RXS_PLM_SPX_STAT_LINK_INIT},
			{RXS_SPX_ERR_DET(8),
				RXS_PLM_SPX_STAT_LINK_INIT}}},
		{{9, rio_em_i_rst_req},
			{{RXS_PLM_SPX_STAT(9),
				RXS_PLM_SPX_STAT_RST_REQ |
				RXS_PLM_SPX_STAT_PRST_REQ},
			{0, 0}}},
		{{RIO_ALL_PORTS, rio_em_i_init_fail},
			{{I2C_INT_STAT, I2C_INT_STAT_BL_FAIL},
			{0, 0}}},
	};
	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i;
	unsigned int j;
	uint32_t chk_val;

	for (i = 0; i < test_cnt; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni %d event %d port %d\n", i,
				tests[i].event.event, tests[i].event.port_num);
		}
		in_p.num_events = 1;
		in_p.events = events;
		events[0].port_num = tests[i].event.port_num;
		events[0].event = tests[i].event.event;

		out_p.imp_rc = 0xFFFFFF;
		out_p.failure_idx = 0xff;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_create_events(dev_info, &in_p,
						&out_p));

		assert_int_equal(RIO_SUCCESS, out_p.imp_rc);
		assert_int_equal(0, out_p.failure_idx);
		for (j = 0; j < 2; j++) {
			if (!tests[i].chk[j].addr) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\nj %d addr 0x%x port 0x%x\n", j,
					tests[i].chk[j].addr,
					tests[i].chk[j].val);
			}
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(dev_info, tests[i].chk[j].addr,
						&chk_val));
			assert_int_equal(chk_val & tests[i].chk[j].val,
					tests[i].chk[j].val);
		}
	}

	(void)state;
}

// Test that events which should be ignored are successfull.

static void rxs_rio_em_create_ignored_events_test(void **state)
{
	rio_em_create_events_in_t in_p;
	rio_em_create_events_out_t out_p;
	rio_em_event_n_loc_t events[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	rio_em_events_t tests[] = {rio_em_a_clr_pwpnd, rio_em_a_no_event};
	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i;

	for (i = 0; i < test_cnt; i++) {
		in_p.num_events = 1;
		in_p.events = events;
		events[0].port_num = 0;
		events[0].event = tests[i];

		out_p.imp_rc = 0xFFFFFF;
		out_p.failure_idx = 0xff;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_create_events(dev_info, &in_p,
						&out_p));

		assert_int_equal(RIO_SUCCESS, out_p.imp_rc);
		assert_int_equal(0, out_p.failure_idx);
	}

	(void)state;
}

/*
// Test bad parameter detection.

static void rxs_rio_em_get_int_stat_bad_parms_test(void **state)
{
	rio_em_get_int_stat_in_t in_p;
	rio_em_get_int_stat_out_t out_p;
	rio_em_event_n_loc_t events[(uint8_t)rio_em_last];

	// Illegal number of ports
	in_p.ptl.num_ports = 2;
	in_p.ptl.pnums[0] = 0;
	in_p.ptl.pnums[1] = 0;
	in_p.num_events = (uint8_t)rio_em_last;
	in_p.events = events;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_int_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Illegal port number
	in_p.ptl.num_ports = 1;
	in_p.ptl.pnums[0] = 1;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_int_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Illegal number of events
	in_p.ptl.num_ports = 1;
	in_p.ptl.pnums[0] = 0;
	in_p.num_events = 0;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_int_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Null events pointer
	in_p.num_events = (uint8_t)rio_em_last;
	in_p.events = NULL;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_int_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	(void)state;
}

// Test the interrupt status is correctly determined for all events
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_get_int_stat_success_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_int_stat_in_t in_p;
	rio_em_get_int_stat_out_t out_p;
	rio_em_event_n_loc_t c_e[1];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes a 2many_retx event.
	// For this reason, "2many_pna" must be the last test,
	// and 2many_retx must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{ rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x100000FF},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_d_log, rio_em_detect_on,
				RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
				RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{ rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, chk_i, srch_i;

	for (i = 0; i < test_cnt; i++) {
		// Enable detection of each event.
		if (rio_em_i_rst_req == tests[i].em_event) {
			// If we're testing disabling the Reset Request
			// event, do the real disable since this events
			// detection is actually controlled by Port Config
			// functionality.

			assert_int_equal(RIO_SUCCESS,
					DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
			plm_imp_spec_ctl &= ~t_mask;
			assert_int_equal(RIO_SUCCESS,
					DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
		}

		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = 0;
		set_cfg_in.notfn = rio_em_notfn_int;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &tests[i];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(rio_em_notfn_int, set_cfg_out.notfn);

		// Create the event
		c_in.num_events = 1;
		c_in.events = c_e;
		c_e[0].port_num = 0;
		c_e[0].event = tests[i].em_event;

		c_out.imp_rc = 0xFFFFFF;
		c_out.failure_idx = 0xff;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_create_events(dev_info, &c_in,
						&c_out));

		assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
		assert_int_equal(0, c_out.failure_idx);

		// Query the event interrupt status
		in_p.ptl.num_ports = 1;
		in_p.ptl.pnums[0] = 0;
		in_p.num_events = (uint8_t)rio_em_last;
		in_p.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_p.imp_rc = 0;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_int_stat(&mock_dev_info,
						&in_p, &out_p));
		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(i + 1, out_p.num_events);
		assert_false(out_p.too_many);
		assert_false(out_p.other_events);

		// Check that all events created to date are all found...
		for (chk_i = 0; chk_i <= i; chk_i++) {
			bool found = false;
			for (srch_i = 0; !found && (srch_i <= i); srch_i++) {
				if (tests[chk_i].em_event
						== stat_e[srch_i].event) {
					found = true;
				}
			}
			if (!found && DEBUG_PRINTF) {
				printf("i %d event_cnt %d chk_i %d event %d", i,
						out_p.num_events, chk_i,
						tests[chk_i].em_event);
			}
			assert_true(found);
		}

		// Query the event interrupt status again, and trigger the
		// "too many events" flag.
		if (!i) {
			continue;
		}
		in_p.ptl.num_ports = 1;
		in_p.ptl.pnums[0] = 0;
		in_p.num_events = i;
		in_p.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_p.imp_rc = 0;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_int_stat(&mock_dev_info,
						&in_p, &out_p));
		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(i, out_p.num_events);
		assert_true(out_p.too_many);
		assert_false(out_p.other_events);
	}

	(void)state;
}

// Test that if one event is configured with interrupt
// notification and all other events are disabled, that
// the "other events" fields behave correctly.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_get_int_stat_other_events_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_int_stat_in_t in_p;
	rio_em_get_int_stat_out_t out_p;
	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an 2many_retx event.
	// This test skips "2many_pna" events.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{ rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, t;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	// Use test_cnt - 1 here to avoid trying for rio_em_f_2many_pna
	// without also setting rio_em_f_2many_retx.
	for (i = 0; i < test_cnt - 1; i++) {
		for (t = 0; t < test_cnt; t++) {
			// Must have two different events for this test.
			if (i == t) {
				continue;
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing disabling the Reset Request
				// event, do the real disable since this events
				// detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Enable the i'th test
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = rio_em_notfn_int;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[i];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(rio_em_notfn_int, set_cfg_out.notfn);

			// Create the i'th and t'th event
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[t].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event interrupt status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = true;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			assert_int_equal(1, out_p.num_events);
			assert_false(out_p.too_many);
			assert_true(out_p.other_events);

			// Check that the event created was found
			assert_int_equal(tests[i].em_event, stat_e[0].event);
		}
	}

	(void)state;
}

// Test bad parameter values are correctly detected and reported.

static void rxs_rio_em_get_pw_stat_bad_parms_test(void **state)
{
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_event_n_loc_t events[(uint8_t)rio_em_last];

	// Illegal number of ports
	in_p.ptl.num_ports = 2;
	in_p.ptl.pnums[0] = 0;
	in_p.ptl.pnums[1] = 0;
	in_p.num_events = (uint8_t)rio_em_last;
	in_p.events = events;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Illegal port number
	in_p.ptl.num_ports = 1;
	in_p.ptl.pnums[0] = 1;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Illegal number of events
	in_p.ptl.num_ports = 1;
	in_p.ptl.pnums[0] = 0;
	in_p.num_events = 0;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	// Null events pointer
	in_p.num_events = (uint8_t)rio_em_last;
	in_p.events = NULL;

	out_p.imp_rc = 0;
	out_p.num_events = 0xFF;
	out_p.too_many = true;
	out_p.other_events = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
					&out_p));
	assert_int_not_equal(0, out_p.imp_rc);
	assert_int_equal(0, out_p.num_events);
	assert_false(out_p.too_many);
	assert_false(out_p.other_events);

	(void)state;
}

// Test port-write status is correctly determined

static void rxs_rio_em_get_pw_stat_success_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_event_n_loc_t c_e[1];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x100000FF},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, chk_i, srch_i;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	for (i = 0; i < test_cnt; i++) {
		// Enable detection of each event.
		if (rio_em_i_rst_req == tests[i].em_event) {
			// If we're testing disabling the Reset Request
			// event, do the real disable since this events
			// detection is actually controlled by Port Config
			// functionality.

			assert_int_equal(RIO_SUCCESS,
					DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
			plm_imp_spec_ctl &= ~t_mask;
			assert_int_equal(RIO_SUCCESS,
					DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
		}

		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = 0;
		set_cfg_in.notfn = rio_em_notfn_pw;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &tests[i];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(rio_em_notfn_pw, set_cfg_out.notfn);

		// Create the event
		c_in.num_events = 1;
		c_in.events = c_e;
		c_e[0].port_num = 0;
		c_e[0].event = tests[i].em_event;

		c_out.imp_rc = 0xFFFFFF;
		c_out.failure_idx = 0xff;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_create_events(dev_info, &c_in,
						&c_out));

		assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
		assert_int_equal(0, c_out.failure_idx);

		// Query the event port-write status
		in_p.ptl.num_ports = 1;
		in_p.ptl.pnums[0] = 0;
		in_p.num_events = (uint8_t)rio_em_last;
		in_p.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_p.imp_rc = 0;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
						&out_p));
		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(i + 1, out_p.num_events);
		assert_false(out_p.too_many);
		assert_false(out_p.other_events);

		// Check that all events created to date are all found...
		for (chk_i = 0; chk_i <= i; chk_i++) {
			bool found = false;
			for (srch_i = 0; !found && (srch_i <= i); srch_i++) {
				if (tests[chk_i].em_event
						== stat_e[srch_i].event) {
					found = true;
				}
			}
			if (!found && DEBUG_PRINTF) {
				printf("i %d event_cnt %d chk_i %d event %d", i,
						out_p.num_events, chk_i,
						tests[chk_i].em_event);
			}
			assert_true(found);
		}

		// Query the event interrupt status again, and trigger the
		// "too many events" flag.
		if (!i) {
			continue;
		}
		in_p.ptl.num_ports = 1;
		in_p.ptl.pnums[0] = 0;
		in_p.num_events = i;
		in_p.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_p.imp_rc = 0;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
						&out_p));
		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(i, out_p.num_events);
		assert_true(out_p.too_many);
		assert_false(out_p.other_events);
	}

	(void)state;
}

// Test that if one event is configured with port-write
// notification and all other events are disabled, that
// the "other events" fields behave correctly.
//
// This test is skipped on real hardware.

static void rxs_rio_em_get_pw_stat_other_events_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an 2many_retx event.
	// For this reason, "2many_pna" must be the last test,
	// and 2many_retx must occur before 2many_pna.
	//
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{ rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, t;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	// Use test_cnt - 1 here to avoid trying for rio_em_f_2many_pna
	// without also setting rio_em_f_2many_retx.
	for (i = 0; i < test_cnt - 1; i++) {
		for (t = 0; t < test_cnt; t++) {
			// Must have two different events for this test.
			if (i == t) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\ni %d t %d\n", i, t);
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing disabling the Reset Request
				// event, do the real disable since this events
				// detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Enable the i'th test
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = rio_em_notfn_pw;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[i];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(rio_em_notfn_pw, set_cfg_out.notfn);

			// Create the i'th and t'th event
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[t].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = true;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			assert_int_equal(1, out_p.num_events);
			assert_false(out_p.too_many);
			assert_int_equal(tests[i].em_event, stat_e[0].event);

			// Check that the other events were found, EXCEPT
			// when the two events are 2many_retx & 2many_pna
			if ((rio_em_f_2many_retx == tests[i].em_event) &&
				(rio_em_f_2many_pna == tests[t].em_event)) {
				continue;
			}
			assert_true(out_p.other_events);
		}
	}

	(void)state;
}

// Test that if one event is configured with interrupt notification, and another
// is configured with port-write notification, that the "other events" fields
// for interrupt and port-write status indicate that another event is present.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_get_int_pw_stat_other_events_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_get_int_stat_in_t in_i;
	rio_em_get_int_stat_out_t out_i;
	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on,0x0010},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, p;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;
	rio_em_notfn_ctl_t i_notfn = rio_em_notfn_int;
	rio_em_notfn_ctl_t p_notfn = rio_em_notfn_pw;

	if (l_st->real_hw) {
		return;
	}

	for (i = 0; i < test_cnt; i++) {
		for (p = 0; p < test_cnt; p++) {
			// Must have two different events for this test.
			if (i == p) {
				continue;
			}
			// init_fail events can only receive interrupt notifn..
			if (rio_em_i_init_fail == tests[p].em_event) {
				continue;
			}
			// 2many_retx and 2many_pna cause the same event,
			// so skip ahead if they're both selected...
			if ((rio_em_f_2many_retx == tests[i].em_event) &&
				(rio_em_f_2many_pna == tests[p].em_event)) {
				continue;
			}
			if ((rio_em_f_2many_retx == tests[p].em_event) &&
				(rio_em_f_2many_pna == tests[i].em_event)) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\ni = %d p = %d event = %d %d\n", i, p,
						tests[i].em_event,
						tests[p].em_event);
			}
			if (DEBUG_PRINTF) {
				printf("\ni %d p %d\n", i, p);
			}

			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			// NOTE: Only one of tests[i].em_event and
			// tests[p].em_event can be true at any one time.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}
			if (rio_em_i_rst_req == tests[p].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[p].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Enable event with interrupt notification
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = i_notfn;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[i];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(i_notfn, set_cfg_out.notfn);

			// Enable event with port-write notification
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = p_notfn;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[p];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(p_notfn, set_cfg_out.notfn);

			// Create the i'th and p'th events
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[p].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event interrupt status
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = false;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			if (rio_em_notfn_int == i_notfn) {
				assert_int_equal(1, out_i.num_events);
				assert_false(out_i.too_many);
				assert_true(out_i.other_events);
				assert_int_equal(stat_e[0].port_num, 0);
				assert_int_equal(stat_e[0].event,
						tests[i].em_event);
			} else {
				bool found;

				assert_int_equal(2, out_i.num_events);
				assert_false(out_i.too_many);
				assert_false(out_i.other_events);
				assert_int_equal(stat_e[0].port_num, 0);
				assert_int_equal(stat_e[1].port_num, 0);

				found =
						((stat_e[0].event
								== tests[p].em_event)
								&& (stat_e[1].event
										== tests[i].em_event))
								|| ((stat_e[0].event
										== tests[i].em_event)
										&& (stat_e[1].event
												== tests[p].em_event));
				assert_true(found);
			}

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = false;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			if (rio_em_notfn_pw == p_notfn) {
				assert_int_equal(1, out_p.num_events);
				assert_false(out_p.too_many);
				// init_fail is an interrupt-only event,
				// so it cannot trigger "other events" for
				// a port-write
				if (rio_em_i_init_fail == tests[i].em_event) {
					assert_false(out_p.other_events);
				} else {
					assert_true(out_p.other_events);
				}
				assert_int_equal(stat_e[0].port_num, 0);
				assert_int_equal(stat_e[0].event,
						tests[p].em_event);
			} else {
				bool found;

				assert_int_equal(2, out_p.num_events);
				assert_false(out_p.too_many);
				assert_false(out_p.other_events);
				assert_int_equal(stat_e[0].port_num, 0);
				assert_int_equal(stat_e[1].port_num, 0);
				found =
						((stat_e[0].event
								== tests[p].em_event)
								&& (stat_e[1].event
										== tests[i].em_event))
								|| ((stat_e[0].event
										== tests[i].em_event)
										&& (stat_e[1].event
												== tests[p].em_event));
				assert_true(found);
			}
		}
	}

	(void)state;
}

// Test that if two events are configured with both interrupt and
// port-write notification, that the interrupt and port-write status is
// correct.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_get_int_pw_stat_both_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_get_int_stat_in_t in_i;
	rio_em_get_int_stat_out_t out_i;
	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;
	rio_em_cfg_t tests_in[2];

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{rio_em_d_log, rio_em_detect_on,
				RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
				RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_f_2many_pna, rio_em_detect_on,0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, p, srch_i, chk_i;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	// Use test_cnt - 1 here to avoid trying for rio_em_f_2many_pna
	// without also setting rio_em_f_2many_retx.
	for (i = 0; i < test_cnt - 1; i++) {
		for (p = 0; p < test_cnt; p++) {
			// Must have two different events for this test.
			if (i == p) {
				continue;
			}
			// Cannot cause an init_fail event to generate a
			// port-write
			if (rio_em_i_init_fail == tests[p].em_event) {
				continue;
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			// NOTE: Only one of tests[i].em_event and
			// tests[p].em_event can be true at any one time.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}
			if (rio_em_i_rst_req == tests[p].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[p].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Configure the i'th and p'th event
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = rio_em_notfn_both;
			set_cfg_in.num_events = 2;
			memcpy(&tests_in[0], &tests[i], sizeof(tests_in[0]));
			memcpy(&tests_in[1], &tests[p], sizeof(tests_in[1]));
			set_cfg_in.events = tests_in;

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(rio_em_notfn_both, set_cfg_out.notfn);

			// Create the i'th and p'th events
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[p].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event interrupt status
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = true;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			assert_int_equal(2, out_i.num_events);
			assert_false(out_i.too_many);
			assert_false(out_i.other_events);
			assert_int_equal(stat_e[0].port_num, 0);
			assert_int_equal(stat_e[1].port_num, 0);

			// Check that events created are found...
			for (chk_i = 0; chk_i <= i; chk_i++) {
				bool found = false;
				if ((chk_i != i) && (chk_i != p)) {
					continue;
				}
				for (srch_i = 0;
						!found
								&& (srch_i
										<= out_i.num_events);
						srch_i++) {
					if (tests[chk_i].em_event
							== stat_e[srch_i].event) {
						found = true;
					}
				}
				if (!found && DEBUG_PRINTF) {
					printf(
							"i %d event_cnt %d chk_i %d event %d",
							i, out_p.num_events,
							chk_i,
							tests[chk_i].em_event);
				}
				assert_true(found);
			}

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = true;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			// It is not possible to detect a port-write init_fail
			// event so reduce the event count by 1.
			if (rio_em_i_init_fail == tests[i].em_event) {
				assert_int_equal(1, out_p.num_events);
				assert_int_equal(stat_e[0].port_num, 0);
			} else {
				assert_int_equal(2, out_p.num_events);
				assert_int_equal(stat_e[0].port_num, 0);
				assert_int_equal(stat_e[1].port_num, 0);
			}
			assert_false(out_p.too_many);
			assert_false(out_p.other_events);

			// Check that events created are found...
			for (chk_i = 0; chk_i <= i; chk_i++) {
				bool found = false;
				if ((chk_i != i) && (chk_i != p)) {
					continue;
				}
				if ((chk_i == i)
						&& (rio_em_i_init_fail
								== tests[i].em_event)) {
					continue;
				}
				for (srch_i = 0;
						!found
								&& (srch_i
										<= out_i.num_events);
						srch_i++) {
					if (tests[chk_i].em_event
							== stat_e[srch_i].event) {
						found = true;
					}
				}
				if (!found && DEBUG_PRINTF) {
					printf(
							"i %d event_cnt %d chk_i %d event %d",
							i, out_p.num_events,
							chk_i,
							tests[chk_i].em_event);
				}
				assert_true(found);
			}
		}
	}

	(void)state;
}

// Test that bad parameter values are detected and reported.

static void rxs_rio_em_clr_events_bad_parms_test(void **state)
{
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;
	rio_em_event_n_loc_t events[(uint8_t)rio_em_last];

	// Illegal number of events
	in_c.num_events = 0;
	in_c.events = events;

	out_c.imp_rc = 0;
	out_c.failure_idx = 0xFF;
	out_c.pw_events_remain = true;
	out_c.int_events_remain = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_clr_events(&mock_dev_info, &in_c,
					&out_c));
	assert_int_not_equal(0, out_c.imp_rc);
	assert_int_equal(0xFF, out_c.failure_idx);
	assert_true(out_c.pw_events_remain);
	assert_true(out_c.int_events_remain);

	// Null events pointer
	in_c.num_events = 1;
	in_c.events = NULL;

	out_c.imp_rc = 0;
	out_c.failure_idx = 0xFF;
	out_c.pw_events_remain = true;
	out_c.int_events_remain = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_clr_events(&mock_dev_info, &in_c,
					&out_c));
	assert_int_not_equal(0, out_c.imp_rc);
	assert_int_equal(0xFF, out_c.failure_idx);
	assert_true(out_c.pw_events_remain);
	assert_true(out_c.int_events_remain);

	// Illegal port in event
	in_c.num_events = 1;
	in_c.events = events;
	events[0].port_num = 1;
	events[0].event = rio_em_a_no_event;

	out_c.imp_rc = 0;
	out_c.failure_idx = 0xFF;
	out_c.pw_events_remain = true;
	out_c.int_events_remain = true;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_em_clr_events(&mock_dev_info, &in_c,
					&out_c));
	assert_int_not_equal(0, out_c.imp_rc);
	assert_int_equal(0, out_c.failure_idx);
	assert_false(out_c.pw_events_remain);
	assert_false(out_c.int_events_remain);

	(void)state;
}

// Verify that each interrupt event can be cleared.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_clr_int_events_success_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_int_stat_in_t in_i;
	rio_em_get_int_stat_out_t out_i;
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;

	rio_em_event_n_loc_t c_e[(uint8_t)rio_em_last];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an 2many_retx event.
	// For this reason, "2many_pna" must be the last test,
	// and 2many_retx must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x100000FF},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, chk_i, srch_i;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	for (i = 0; i < test_cnt; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni = %d event = %d\n", i, tests[i].em_event);
		}
		// Enable detection of each event.
		if (rio_em_i_rst_req == tests[i].em_event) {
			// If we're testing disabling the Reset Request
			// event, do the real disable since this events
			// detection is actually controlled by Port Config
			// functionality.

			assert_int_equal(RIO_SUCCESS,
					DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
			plm_imp_spec_ctl &= ~t_mask;
			assert_int_equal(RIO_SUCCESS,
					DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
		}

		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = 0;
		set_cfg_in.notfn = rio_em_notfn_both;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &tests[i];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(rio_em_notfn_both, set_cfg_out.notfn);

		// Create all events
		c_in.num_events = i + 1;
		c_in.events = c_e;
		for (srch_i = 0; srch_i <= i; srch_i++) {
			c_e[srch_i].port_num = 0;
			c_e[srch_i].event = tests[srch_i].em_event;
		}

		c_out.imp_rc = 0xFFFFFF;
		c_out.failure_idx = 0xff;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_create_events(dev_info, &c_in,
						&c_out));

		assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
		assert_int_equal(0, c_out.failure_idx);

		// Query the event interrupt status
		in_i.ptl.num_ports = 1;
		in_i.ptl.pnums[0] = 0;
		in_i.num_events = (uint8_t)rio_em_last;
		in_i.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_i.imp_rc = 0;
		out_i.num_events = 0xFF;
		out_i.too_many = true;
		out_i.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_int_stat(&mock_dev_info,
						&in_i, &out_i));
		assert_int_equal(0, out_i.imp_rc);
		assert_int_equal(i + 1, out_i.num_events);
		assert_false(out_i.too_many);
		assert_false(out_i.other_events);

		// Check that all events created to date are all found...
		for (chk_i = 0; chk_i <= i; chk_i++) {
			bool found = false;
			for (srch_i = 0; !found && (srch_i <= i); srch_i++) {
				if (tests[chk_i].em_event
						== stat_e[srch_i].event) {
					found = true;
				}
			}
			if (!found && DEBUG_PRINTF) {
				printf("i %d event_cnt %d chk_i %d event %d", i,
						out_i.num_events, chk_i,
						tests[chk_i].em_event);
			}
			assert_true(found);
		}

		// Clear all interrupt events...
		in_c.num_events = out_i.num_events;
		in_c.events = in_i.events;

		out_c.imp_rc = 0xFFFF;
		out_c.failure_idx = 0xFF;
		out_c.pw_events_remain = true;
		out_c.int_events_remain = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_clr_events(&mock_dev_info, &in_c,
						&out_c));
		assert_int_equal(0, out_c.imp_rc);
		assert_int_equal(0, out_c.failure_idx);
		assert_false(out_c.pw_events_remain);
		assert_false(out_c.int_events_remain);

		// Query the event interrupt status, ensure all interrupts
		// are gone...
		in_i.ptl.num_ports = 1;
		in_i.ptl.pnums[0] = 0;
		in_i.num_events = (uint8_t)rio_em_last;
		in_i.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_i.imp_rc = 0;
		out_i.num_events = 0xFF;
		out_i.too_many = true;
		out_i.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_int_stat(&mock_dev_info,
						&in_i, &out_i));
		assert_int_equal(0, out_i.imp_rc);
		if (out_i.num_events && DEBUG_PRINTF) {
			printf("\n%d events, first is %d\n", out_i.num_events,
					stat_e[0].event);
		}
		assert_int_equal(0, out_i.num_events);
		assert_false(out_i.too_many);
		assert_false(out_i.other_events);
	}

	(void)state;
}

// Verify that each port-write event can be cleared.

static void rxs_rio_em_clr_pw_events_success_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;

	rio_em_event_n_loc_t c_e[(uint8_t)rio_em_last];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
			{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000}, {
					rio_em_f_port_err, rio_em_detect_on, 0},
			{rio_em_f_err_rate, rio_em_detect_on, 0x100000FF}, {
					rio_em_f_2many_retx, rio_em_detect_on,
					0x0010}, {rio_em_d_log,
					rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN}, {
					rio_em_i_sig_det, rio_em_detect_on, 0},
			{rio_em_i_rst_req, rio_em_detect_on, 0}, {
					rio_em_f_2many_pna, rio_em_detect_on,
					0x0010}};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, chk_i, srch_i;

	// Before beginning, clear all events in hardware
	// Fail if any events remain.

	for (i = 0; i < test_cnt; i++) {
		c_e[i].event = tests[i].em_event;
		c_e[i].port_num = 0;
	}
	in_c.num_events = test_cnt;
	in_c.events = c_e;

	out_c.imp_rc = 0xFFFF;
	out_c.failure_idx = 0xFF;
	out_c.pw_events_remain = true;
	out_c.int_events_remain = true;

	assert_int_equal(RIO_SUCCESS,
			rxs_rio_em_clr_events(&mock_dev_info, &in_c,
					&out_c));

	assert_int_equal(0, out_c.imp_rc);
	assert_int_equal(0, out_c.failure_idx);
	assert_false(out_c.pw_events_remain);
	assert_false(out_c.int_events_remain);

	// Now grind through creating and clearing all events.

	for (i = 0; i < test_cnt; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni = %d event = %d\n", i, tests[i].em_event);
		}
		// Enable detection of each event.
		if (rio_em_i_rst_req == tests[i].em_event) {
			// If we're testing disabling the Reset Request
			// event, do the real disable since this events
			// detection is actually controlled by Port Config
			// functionality.

			assert_int_equal(RIO_SUCCESS,
					DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
			plm_imp_spec_ctl &= ~t_mask;
			assert_int_equal(RIO_SUCCESS,
					DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
		}

		set_cfg_in.ptl.num_ports = 1;
		set_cfg_in.ptl.pnums[0] = 0;
		set_cfg_in.notfn = rio_em_notfn_pw;
		set_cfg_in.num_events = 1;
		set_cfg_in.events = &tests[i];

		set_cfg_out.imp_rc = 0xFFFFFFFF;
		set_cfg_out.fail_port_num = 0x99;
		set_cfg_out.fail_idx = 0xFF;
		set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_cfg_set(&mock_dev_info,
						&set_cfg_in, &set_cfg_out));
		assert_int_equal(0, set_cfg_out.imp_rc);
		assert_int_equal(RIO_ALL_PORTS, set_cfg_out.fail_port_num);
		assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
		assert_int_equal(rio_em_notfn_pw, set_cfg_out.notfn);

		// Create the event
		c_in.num_events = i + 1;
		c_in.events = c_e;
		for (srch_i = 0; srch_i <= i; srch_i++) {
			c_e[srch_i].port_num = 0;
			c_e[srch_i].event = tests[srch_i].em_event;
		}

		c_out.imp_rc = 0xFFFFFF;
		c_out.failure_idx = 0xff;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_create_events(dev_info, &c_in,
						&c_out));

		assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
		assert_int_equal(0, c_out.failure_idx);

		// Query the event port-write status
		in_p.ptl.num_ports = 1;
		in_p.ptl.pnums[0] = 0;
		in_p.num_events = (uint8_t)rio_em_last;
		in_p.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_p.imp_rc = 0;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
						&out_p));
		assert_int_equal(0, out_p.imp_rc);
		assert_int_equal(i + 1, out_p.num_events);
		assert_false(out_p.too_many);
		assert_false(out_p.other_events);

		// Check that all events created to date are all found...
		for (chk_i = 0; chk_i <= i; chk_i++) {
			bool found = false;
			for (srch_i = 0; !found && (srch_i <= i); srch_i++) {
				if (tests[chk_i].em_event
						== stat_e[srch_i].event) {
					found = true;
				}
			}
			if (!found && DEBUG_PRINTF) {
				printf("i %d event_cnt %d chk_i %d event %d", i,
						out_p.num_events, chk_i,
						tests[chk_i].em_event);
			}
			assert_true(found);
		}

		// Clear all port-write events...
		in_c.num_events = out_p.num_events;
		in_c.events = in_p.events;

		out_c.imp_rc = 0xFFFF;
		out_c.failure_idx = 0xFF;
		out_c.pw_events_remain = true;
		out_c.int_events_remain = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_clr_events(&mock_dev_info, &in_c,
						&out_c));
		assert_int_equal(0, out_c.imp_rc);
		assert_int_equal(0, out_c.failure_idx);
		assert_false(out_c.pw_events_remain);
		assert_false(out_c.int_events_remain);

		// Query the event port-write status, check all events are gone
		in_p.ptl.num_ports = 1;
		in_p.ptl.pnums[0] = 0;
		in_p.num_events = (uint8_t)rio_em_last;
		in_p.events = stat_e;
		memset(stat_e, 0xFF, sizeof(stat_e));

		out_p.imp_rc = 0;
		out_p.num_events = 0xFF;
		out_p.too_many = true;
		out_p.other_events = true;

		assert_int_equal(RIO_SUCCESS,
				rxs_rio_em_get_pw_stat(&mock_dev_info, &in_p,
						&out_p));
		assert_int_equal(0, out_p.imp_rc);
		if (out_p.num_events && DEBUG_PRINTF) {
			printf("\n%d events, first is %d\n", out_p.num_events,
					stat_e[0].event);
		}
		assert_int_equal(0, out_p.num_events);
		assert_false(out_p.too_many);
		assert_false(out_p.other_events);
	}

	(void)state;
}

// Test that if one event is configured with interrupt
// notification and all other events are disabled, that when the port-write
// event is cleared the "other events" fields behave correctly.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_clr_int_events_other_events_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_int_stat_in_t in_i;
	rio_em_get_int_stat_out_t out_i;
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;

	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an 2many_retx event.
	// For this reason, "2many_pna" must be the last test,
	// and 2many_retx must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, t;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	// Use test_cnt - 1 here to avoid trying for rio_em_f_2many_pna
	// without also setting rio_em_f_err_rate.
	for (i = 0; i < test_cnt - 1; i++) {
		for (t = 0; t < test_cnt; t++) {
			// Must have two different events for this test.
			if (i == t) {
				continue;
			}
			// init_fail events can only cause interrupts on the
			// Tsi721, so if they are the test event, don't bother
			if (rio_em_i_init_fail == tests[t].em_event) {
				continue;
			}
			// 2many_retx and 2many_pna cause the same event,
			// so skip ahead if they're both selected...
			if ((rio_em_f_2many_retx == tests[i].em_event) &&
				(rio_em_f_2many_pna == tests[t].em_event)) {
				continue;
			}
			if ((rio_em_f_2many_retx == tests[t].em_event) &&
				(rio_em_f_2many_pna == tests[i].em_event)) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\ni = %d t = %d event = %d %d\n", i, t,
						tests[i].em_event,
						tests[t].em_event);
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing disabling the Reset Request
				// event, do the real disable since this events
				// detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Enable the i'th test
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = rio_em_notfn_int;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[i];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(rio_em_notfn_int, set_cfg_out.notfn);

			// Create the i'th and t'th event
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[t].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event interrupt status
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = true;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			assert_int_equal(1, out_i.num_events);
			assert_false(out_i.too_many);
			assert_true(out_i.other_events);

			// Check that the event created was found
			assert_int_equal(tests[i].em_event, stat_e[0].event);

			// Clear all interrupt events...
			in_c.num_events = out_i.num_events;
			in_c.events = in_i.events;

			out_c.imp_rc = 0xFFFF;
			out_c.failure_idx = 0xFF;
			out_c.pw_events_remain = true;
			out_c.int_events_remain = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_clr_events(&mock_dev_info,
							&in_c, &out_c));
			assert_int_equal(0, out_c.imp_rc);
			assert_int_equal(0, out_c.failure_idx);

			assert_true(out_c.pw_events_remain);
			assert_true(out_c.int_events_remain);

			// Query the event interrupt status, confirm that
			// port-write events remain...
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = true;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			assert_int_equal(0, out_i.num_events);
			assert_false(out_i.too_many);
			assert_true(out_i.other_events);
		}
	}

	(void)state;
}

// Test that if one event is configured with port-write
// notification and all other events are disabled, that when the port-write
// event is cleared the "other events" fields behave correctly.
//
// This test is skipped on real hardware.

static void rxs_rio_em_clr_pw_events_other_events_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;

	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_e[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, t;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	// Use test_cnt - 1 here to avoid trying for rio_em_f_2many_pna
	// without also setting rio_em_f_err_rate.
	for (i = 0; i < test_cnt - 1; i++) {
		for (t = 0; t < test_cnt; t++) {
			// Must have two different events for this test.
			if (i == t) {
				continue;
			}
			// 2many_retx and 2many_pna cause the same event,
			// so skip ahead if they're both selected...
			if ((rio_em_f_2many_retx == tests[i].em_event) &&
				(rio_em_f_2many_pna == tests[t].em_event)) {
				continue;
			}
			if ((rio_em_f_2many_retx == tests[t].em_event) &&
				(rio_em_f_2many_pna == tests[i].em_event)) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\ni = %d t = %d event = %d %d\n", i, t,
						tests[i].em_event,
						tests[t].em_event);
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing disabling the Reset Request
				// event, do the real disable since this events
				// detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Enable the i'th test
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = rio_em_notfn_pw;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[i];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(rio_em_notfn_pw, set_cfg_out.notfn);

			// Create the i'th and t'th event
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[t].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = true;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			assert_int_equal(1, out_p.num_events);
			assert_false(out_p.too_many);
			assert_true(out_p.other_events);

			// Check that the event created was found
			assert_int_equal(tests[i].em_event, stat_e[0].event);

			// Clear all port-write events...
			in_c.num_events = out_p.num_events;
			in_c.events = in_p.events;

			out_c.imp_rc = 0xFFFF;
			out_c.failure_idx = 0xFF;
			out_c.pw_events_remain = true;
			out_c.int_events_remain = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_clr_events(&mock_dev_info,
							&in_c, &out_c));
			assert_int_equal(0, out_c.imp_rc);
			assert_int_equal(0, out_c.failure_idx);
			assert_true(out_c.pw_events_remain);
			assert_true(out_c.int_events_remain);

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_e;
			memset(stat_e, 0xFF, sizeof(stat_e));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = true;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			assert_int_equal(0, out_p.num_events);
			assert_false(out_p.too_many);
			assert_true(out_p.other_events);
		}
	}

	(void)state;
}

// Test that if one event is configured with port-write
// notification and another is configured with interrupt notification,
// that when the events are created and cleared the
// "other events" fields behave correctly.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_clr_int_pw_events_other_events_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_get_int_stat_in_t in_i;
	rio_em_get_int_stat_out_t out_i;
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;

	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_i[(uint8_t)rio_em_last];
	rio_em_event_n_loc_t stat_p[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	//
	// Note that the rio_em_f_err_rate.em_info value excludes
	// RIO_EM_REC_ERR_SET_CS_NOT_ACC, so the events
	// for rio_em_f_err_rate and rio_em_f_2many_pna are exclusive of
	// each other.
	rio_em_cfg_t tests[] = {
		{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000},
		{rio_em_f_port_err, rio_em_detect_on, 0},
		{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F},
		{rio_em_d_log, rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN},
		{rio_em_i_sig_det, rio_em_detect_on, 0},
		{rio_em_i_rst_req, rio_em_detect_on, 0},
		{rio_em_i_init_fail, rio_em_detect_on, 0},
		{rio_em_f_2many_retx, rio_em_detect_on, 0x0010},
		{rio_em_f_2many_pna, rio_em_detect_on, 0x0010}
	};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, p;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;
	rio_em_notfn_ctl_t i_notfn, p_notfn;

	if (l_st->real_hw) {
		return;
	}

	for (i = 0; i < test_cnt; i++) {
		for (p = 0; p < test_cnt; p++) {
			// Must have two different events for this test.
			if (i == p) {
				continue;
			}
			// init_fail events can only use pw notification
			if (rio_em_i_init_fail == tests[p].em_event) {
				continue;
			}
			// 2many_retx & 2many_pna both create the same event,
			// so skip this test configuration.
			if ((rio_em_f_2many_retx == tests[i].em_event) &&
				(rio_em_f_2many_pna == tests[p].em_event)) {
				continue;
			}
			if ((rio_em_f_2many_retx == tests[p].em_event) &&
				(rio_em_f_2many_pna == tests[i].em_event)) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\ni = %d p = %d event = %d %d\n", i, p,
						tests[i].em_event,
						tests[p].em_event);
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			// NOTE: Only one of tests[i].em_event and
			// tests[p].em_event can be true at any one time.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}
			if (rio_em_i_rst_req == tests[p].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[p].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Enable the i'th and p'th event
			// Special notification case for f_err_rate and
			// 2many_pna: Both are port_fail events, so notification
			// must be "both".
			i_notfn = rio_em_notfn_int;
			p_notfn = rio_em_notfn_pw;
			if (((rio_em_f_err_rate == tests[i].em_event)
					|| (rio_em_f_2many_pna
							== tests[i].em_event))
					&& ((rio_em_f_err_rate
							== tests[p].em_event)
							|| (rio_em_f_2many_pna
									== tests[p].em_event))) {
				i_notfn = rio_em_notfn_both;
				p_notfn = rio_em_notfn_both;
			}
			// Enable event with interrupt notification
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = i_notfn;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[i];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(i_notfn, set_cfg_out.notfn);

			// Enable event with port-write notification
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = p_notfn;
			set_cfg_in.num_events = 1;
			set_cfg_in.events = &tests[p];

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(p_notfn, set_cfg_out.notfn);

			// Create the i'th and p'th events
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[p].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event interrupt status
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_i;
			memset(stat_i, 0xFF, sizeof(stat_i));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = false;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			if (rio_em_notfn_int == i_notfn) {
				assert_int_equal(1, out_i.num_events);
				assert_false(out_i.too_many);
				assert_true(out_i.other_events);
				assert_int_equal(stat_i[0].port_num, 0);
				assert_int_equal(stat_i[0].event,
						tests[i].em_event);
			} else {
				bool found;

				assert_int_equal(2, out_i.num_events);
				assert_false(out_i.too_many);
				assert_false(out_i.other_events);
				assert_int_equal(stat_i[0].port_num, 0);
				assert_int_equal(stat_i[1].port_num, 0);

				found =
						((stat_i[0].event
								== tests[p].em_event)
								&& (stat_i[1].event
										== tests[i].em_event))
								|| ((stat_i[0].event
										== tests[i].em_event)
										&& (stat_i[1].event
												== tests[p].em_event));
				assert_true(found);
			}

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_p;
			memset(stat_p, 0xFF, sizeof(stat_p));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = false;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			if (rio_em_notfn_pw == p_notfn) {
				assert_int_equal(1, out_p.num_events);
				assert_false(out_p.too_many);
				// init_fail is an interrupt-only event,
				// so it cannot trigger "other events" for
				// a port-write
				if (rio_em_i_init_fail == tests[i].em_event) {
					assert_false(out_p.other_events);
				} else {
					assert_true(out_p.other_events);
				}
				assert_int_equal(stat_p[0].port_num, 0);
				assert_int_equal(stat_p[0].event,
						tests[p].em_event);
			} else {
				bool found;

				assert_int_equal(2, out_p.num_events);
				assert_false(out_p.too_many);
				assert_false(out_p.other_events);
				assert_int_equal(stat_p[0].port_num, 0);
				assert_int_equal(stat_p[1].port_num, 0);
				found =
						((stat_p[0].event
								== tests[p].em_event)
								&& (stat_p[1].event
										== tests[i].em_event))
								|| ((stat_p[0].event
										== tests[i].em_event)
										&& (stat_p[1].event
												== tests[p].em_event));
				assert_true(found);
			}

			// Clear all interrupt events...
			in_c.num_events = out_i.num_events;
			in_c.events = in_i.events;

			out_c.imp_rc = 0xFFFF;
			out_c.failure_idx = 0xFF;
			out_c.pw_events_remain = true;
			out_c.int_events_remain = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_clr_events(&mock_dev_info,
							&in_c, &out_c));
			assert_int_equal(0, out_c.imp_rc);
			assert_int_equal(0, out_c.failure_idx);
			// if notfn_pw != p_notfn, notfn_both == p_notfn
			// which implies that the events are 2many_pna
			// and err_rate, which are generally cleared together.
			if (rio_em_notfn_pw == p_notfn) {
				assert_true(out_c.pw_events_remain);
				assert_true(out_c.int_events_remain);
			} else {
				assert_false(out_c.pw_events_remain);
				assert_false(out_c.int_events_remain);
			}

			// Clear all port-write events...
			in_c.num_events = out_p.num_events;
			in_c.events = in_p.events;

			out_c.imp_rc = 0xFFFF;
			out_c.failure_idx = 0xFF;
			out_c.pw_events_remain = true;
			out_c.int_events_remain = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_clr_events(&mock_dev_info,
							&in_c, &out_c));
			assert_int_equal(0, out_c.imp_rc);
			assert_int_equal(0, out_c.failure_idx);
			assert_false(out_c.pw_events_remain);
			assert_false(out_c.int_events_remain);

			// Query the event interrupt status, confirm they're
			// gone.
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_i;
			memset(stat_i, 0xFF, sizeof(stat_i));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = false;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			assert_int_equal(0, out_i.num_events);
			assert_false(out_i.too_many);
			assert_false(out_i.other_events);

			// Query port-write events, confirm they're gone
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_p;
			memset(stat_p, 0xFF, sizeof(stat_p));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = false;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			assert_int_equal(0, out_p.num_events);
			assert_false(out_p.too_many);
			assert_false(out_p.other_events);
		}
	}

	(void)state;
}

// Test that when two events are configured with both port-write
// and interrupt notification, that when the events are created and
// cleared the "other events" fields behave correctly.
//
// This test is skipped if interrupts are not supported.

static void rxs_rio_em_clr_int_pw_events_both_test(void **state)
{
	rio_em_cfg_set_in_t set_cfg_in;
	rio_em_cfg_set_out_t set_cfg_out;
	rio_em_create_events_in_t c_in;
	rio_em_create_events_out_t c_out;
	rio_em_get_pw_stat_in_t in_p;
	rio_em_get_pw_stat_out_t out_p;
	rio_em_get_int_stat_in_t in_i;
	rio_em_get_int_stat_out_t out_i;
	rio_em_clr_events_in_t in_c;
	rio_em_clr_events_out_t out_c;

	rio_em_event_n_loc_t c_e[2];
	rio_em_event_n_loc_t stat_i[(uint8_t)rio_em_last];
	rio_em_event_n_loc_t stat_p[(uint8_t)rio_em_last];
	DAR_DEV_INFO_t *dev_info = &mock_dev_info;
	rio_em_cfg_t tests_in[2];

	// NOTE: A 2many_pna event also causes an err_rate event.
	// For this reason, "2many_pna" must be the last test,
	// and err_rate must occur before 2many_pna.
	//
	// Note that the rio_em_f_err_rate.em_info value excludes
	// RIO_EM_REC_ERR_SET_CS_NOT_ACC, so the events
	// for rio_em_f_err_rate and rio_em_f_2many_pna are exclusive of
	// each other.
	rio_em_cfg_t tests[] = {
			{rio_em_f_los, rio_em_detect_on, 1 * 256 * 1000}, {
					rio_em_f_port_err, rio_em_detect_on, 0},
			{rio_em_f_err_rate, rio_em_detect_on, 0x1000000F}, {
					rio_em_f_2many_retx, rio_em_detect_on,
					0x0010}, {rio_em_d_log,
					rio_em_detect_on,
					RXS_LOCAL_ERR_EN_ILL_TYPE_EN |
					RXS_LOCAL_ERR_EN_ILL_ID_EN}, {
					rio_em_i_sig_det, rio_em_detect_on, 0},
			{rio_em_i_rst_req, rio_em_detect_on, 0},
			{rio_em_i_init_fail, rio_em_detect_on, 0}, {
					rio_em_f_2many_pna, rio_em_detect_on,
					0x0010}};

	uint32_t plm_imp_spec_ctl;
	uint32_t t_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
	RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;

	const unsigned int test_cnt = sizeof(tests) / sizeof(tests[0]);
	unsigned int i, p, srch_i, chk_i;
	Tsi721_test_state_t *l_st = (Tsi721_test_state_t *)*state;

	if (l_st->real_hw) {
		return;
	}

	// Use test_cnt - 1 here to avoid trying for rio_em_f_2many_pna
	// without also setting rio_em_f_err_rate.
	for (i = 0; i < test_cnt - 1; i++) {
		for (p = 0; p < test_cnt; p++) {
			// Must have two different events for this test.
			if (i == p) {
				continue;
			}
			// Cannot cause an init_fail event to generate a
			// port-write
			if (rio_em_i_init_fail == tests[p].em_event) {
				continue;
			}
			if (DEBUG_PRINTF) {
				printf("\ni = %d p = %d event = %d %d\n", i, p,
						tests[i].em_event,
						tests[p].em_event);
			}
			// This test requires a clean slate at the beginning
			// of each attempt
			rxs_em_setup(state);

			// Enable detection of the current event.
			// NOTE: Only one of tests[i].em_event and
			// tests[p].em_event can be true at any one time.
			if (rio_em_i_rst_req == tests[i].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[i].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}
			if (rio_em_i_rst_req == tests[p].em_event) {
				// If we're testing the Reset Request
				// event, do the disable/enable since this
				// events detection is actually controlled by
				// Port Config functionality.

				assert_int_equal(RIO_SUCCESS,
						DARRegRead(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, &plm_imp_spec_ctl));
				if (rio_em_detect_off == tests[p].em_detect) {
					plm_imp_spec_ctl |= t_mask;
				} else {
					plm_imp_spec_ctl &= ~t_mask;
				}
				assert_int_equal(RIO_SUCCESS,
						DARRegWrite(&mock_dev_info, RXS_PLM_SPX_IMP_SPEC_CTL, plm_imp_spec_ctl));
			}

			// Configure the i'th and p'th event
			set_cfg_in.ptl.num_ports = 1;
			set_cfg_in.ptl.pnums[0] = 0;
			set_cfg_in.notfn = rio_em_notfn_both;
			set_cfg_in.num_events = 2;
			memcpy(&tests_in[0], &tests[i], sizeof(tests_in[0]));
			memcpy(&tests_in[1], &tests[p], sizeof(tests_in[1]));
			set_cfg_in.events = tests_in;

			set_cfg_out.imp_rc = 0xFFFFFFFF;
			set_cfg_out.fail_port_num = 0x99;
			set_cfg_out.fail_idx = 0xFF;
			set_cfg_out.notfn = (rio_em_notfn_ctl_t)0xF;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_cfg_set(&mock_dev_info,
							&set_cfg_in,
							&set_cfg_out));
			assert_int_equal(0, set_cfg_out.imp_rc);
			assert_int_equal(RIO_ALL_PORTS,
					set_cfg_out.fail_port_num);
			assert_int_equal(rio_em_last, set_cfg_out.fail_idx);
			assert_int_equal(rio_em_notfn_both, set_cfg_out.notfn);

			// Create the i'th and p'th events
			c_in.num_events = 2;
			c_in.events = c_e;
			c_e[0].port_num = 0;
			c_e[0].event = tests[i].em_event;
			c_e[1].port_num = 0;
			c_e[1].event = tests[p].em_event;

			c_out.imp_rc = 0xFFFFFF;
			c_out.failure_idx = 0xff;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_create_events(dev_info,
							&c_in, &c_out));

			assert_int_equal(RIO_SUCCESS, c_out.imp_rc);
			assert_int_equal(0, c_out.failure_idx);

			// Query the event interrupt status
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_i;
			memset(stat_i, 0xFF, sizeof(stat_i));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = true;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			assert_int_equal(2, out_i.num_events);
			assert_false(out_i.too_many);
			assert_false(out_i.other_events);
			assert_int_equal(stat_i[0].port_num, 0);
			assert_int_equal(stat_i[1].port_num, 0);

			// Check that events created are found...
			for (chk_i = 0; chk_i <= i; chk_i++) {
				bool found = false;
				if ((chk_i != i) && (chk_i != p)) {
					continue;
				}
				for (srch_i = 0;
						!found
								&& (srch_i
										<= out_i.num_events);
						srch_i++) {
					if (tests[chk_i].em_event
							== stat_i[srch_i].event) {
						found = true;
					}
				}
				if (!found && DEBUG_PRINTF) {
					printf(
							"i %d event_cnt %d chk_i %d event %d",
							i, out_p.num_events,
							chk_i,
							tests[chk_i].em_event);
				}
				assert_true(found);
			}

			// Query the event port-write status
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_p;
			memset(stat_p, 0xFF, sizeof(stat_p));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = true;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			// It is not possible to detect a pw init_fail event
			// so reduce the event count by 1.
			if (rio_em_i_init_fail == tests[i].em_event) {
				assert_int_equal(1, out_p.num_events);
				assert_int_equal(stat_p[0].port_num, 0);
			} else {
				assert_int_equal(2, out_p.num_events);
				assert_int_equal(stat_p[0].port_num, 0);
				assert_int_equal(stat_p[1].port_num, 0);
			}
			assert_false(out_p.too_many);
			assert_false(out_p.other_events);

			// Check that events created are found...
			for (chk_i = 0; chk_i <= i; chk_i++) {
				bool found = false;
				if ((chk_i != i) && (chk_i != p)) {
					continue;
				}
				if ((chk_i == i)
						&& (rio_em_i_init_fail
								== tests[i].em_event)) {
					continue;
				}
				for (srch_i = 0;
						!found
								&& (srch_i
										<= out_i.num_events);
						srch_i++) {
					if (tests[chk_i].em_event
							== stat_p[srch_i].event) {
						found = true;
					}
				}
				if (!found && DEBUG_PRINTF) {
					printf(
							"i %d event_cnt %d chk_i %d event %d",
							i, out_p.num_events,
							chk_i,
							tests[chk_i].em_event);
				}
				assert_true(found);
			}

			// init_fail is an interrupt only event, clear it
			// first...
			if (rio_em_i_init_fail == tests[i].em_event) {
				in_c.num_events = 1;
				in_c.events = c_e;
				c_e[0].port_num = 0;
				c_e[0].event = rio_em_i_init_fail;

				out_c.imp_rc = 0xFFFF;
				out_c.failure_idx = 0xFF;
				out_c.pw_events_remain = true;
				out_c.int_events_remain = true;

				assert_int_equal(RIO_SUCCESS,
						rxs_rio_em_clr_events(
								&mock_dev_info,
								&in_c, &out_c));
				assert_int_equal(0, out_c.imp_rc);
				assert_int_equal(0, out_c.failure_idx);
				assert_true(out_c.pw_events_remain);
				assert_true(out_c.int_events_remain);
			}

			// Clear all port-write events...
			in_c.num_events = out_p.num_events;
			in_c.events = in_p.events;

			out_c.imp_rc = 0xFFFF;
			out_c.failure_idx = 0xFF;
			out_c.pw_events_remain = true;
			out_c.int_events_remain = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_clr_events(&mock_dev_info,
							&in_c, &out_c));
			assert_int_equal(0, out_c.imp_rc);
			assert_int_equal(0, out_c.failure_idx);
			assert_false(out_c.pw_events_remain);
			assert_false(out_c.int_events_remain);

			// Query the event interrupt status, confirm they're
			// gone.
			in_i.ptl.num_ports = 1;
			in_i.ptl.pnums[0] = 0;
			in_i.num_events = (uint8_t)rio_em_last;
			in_i.events = stat_i;
			memset(stat_i, 0xFF, sizeof(stat_i));

			out_i.imp_rc = 0;
			out_i.num_events = 0xFF;
			out_i.too_many = false;
			out_i.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_int_stat(
							&mock_dev_info, &in_i,
							&out_i));
			assert_int_equal(0, out_i.imp_rc);
			assert_int_equal(0, out_i.num_events);
			assert_false(out_i.too_many);
			assert_false(out_i.other_events);

			// Query port-write events, confirm they're gone
			in_p.ptl.num_ports = 1;
			in_p.ptl.pnums[0] = 0;
			in_p.num_events = (uint8_t)rio_em_last;
			in_p.events = stat_p;
			memset(stat_p, 0xFF, sizeof(stat_p));

			out_p.imp_rc = 0;
			out_p.num_events = 0xFF;
			out_p.too_many = false;
			out_p.other_events = true;

			assert_int_equal(RIO_SUCCESS,
					rxs_rio_em_get_pw_stat(
							&mock_dev_info, &in_p,
							&out_p));
			assert_int_equal(0, out_p.imp_rc);
			assert_int_equal(0, out_p.num_events);
			assert_false(out_p.too_many);
			assert_false(out_p.other_events);
		}
	}

	(void)state;
}
*/
int main(int argc, char** argv)
{
	memset(&st, 0, sizeof(st));
	st.argc = argc;
	st.argv = argv;

	const struct CMUnitTest tests[] = {
	cmocka_unit_test_setup(
			rxs_rio_em_dev_rpt_ctl_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_dev_rpt_ctl_oddport_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_dev_rpt_ctl_bad_parms_test,
			rxs_em_setup),

	cmocka_unit_test_setup(
			rxs_em_cfg_pw_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_em_cfg_pw_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_cfg_pw_retx_compute_test,
			rxs_em_setup),

	cmocka_unit_test_setup(
			rxs_rio_em_cfg_set_success_em_info_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_cfg_set_roundup_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_cfg_set_ignore_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_cfg_set_fail_em_info_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_cfg_set_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_cfg_get_bad_parms_test,
			rxs_em_setup),

	cmocka_unit_test_setup(
			rxs_rio_em_parse_pw_no_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_parse_pw_all_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_parse_pw_oth_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_parse_pw_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_create_events_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_create_events_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_create_ignored_events_test,
			rxs_em_setup),
/*
	cmocka_unit_test_setup(
			rxs_rio_em_get_int_stat_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_int_stat_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_int_stat_other_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_pw_stat_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_pw_stat_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_pw_stat_other_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_int_pw_stat_other_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_get_int_pw_stat_both_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_events_bad_parms_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_int_events_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_pw_events_success_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_int_events_other_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_pw_events_other_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_int_pw_events_other_events_test,
			rxs_em_setup),
	cmocka_unit_test_setup(
			rxs_rio_em_clr_int_pw_events_both_test,
			rxs_em_setup),
*/
	};

	return cmocka_run_group_tests(tests, grp_setup, grp_teardown);
}

#endif /* RXSx_DAR_WANTED */

#ifdef __cplusplus
}
#endif
