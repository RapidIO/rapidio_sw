/*
 ****************************************************************************
 Copyright (c) 2017, Integrated Device Technology Inc.
 Copyright (c) 2017, RapidIO Trade Association
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "RapidIO_Device_Access_Routines_API.h"
#include "RapidIO_Port_Config_API.h"
#include "RXS_DeviceDriver.h"
#include "RXS2448.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RXS_DAR_WANTED

#define RXS_FIRST_PORT_LANE(p) (((p / 2) * 4) + ((p & 1) * 2))

typedef struct spx_ctl2_ls_check_info_t_TAG {
	uint32_t ls_en;
	uint32_t ls_sup;
	rio_pc_ls_t ls;
} spx_ctl2_ls_check_info_t;

spx_ctl2_ls_check_info_t rxs_ls_check[] = {
	{ RIO_SPX_CTL2_GB_1P25_EN , RIO_SPX_CTL2_GB_1P25 , rio_pc_ls_1p25},
	{ RIO_SPX_CTL2_GB_2P5_EN  , RIO_SPX_CTL2_GB_2P5  , rio_pc_ls_2p5 },
	{ RIO_SPX_CTL2_GB_3P125_EN, RIO_SPX_CTL2_GB_3P125, rio_pc_ls_3p125},
	{ RIO_SPX_CTL2_GB_5P0_EN  , RIO_SPX_CTL2_GB_5P0  , rio_pc_ls_5p0  },
	{ RIO_SPX_CTL2_GB_6P25_EN , RIO_SPX_CTL2_GB_6P25 , rio_pc_ls_6p25 },
	{ RIO_SPX_CTL2_GB_10P3_EN , RIO_SPX_CTL2_GB_10P3 , rio_pc_ls_10p3 },
	{ RIO_SPX_CTL2_GB_12P5_EN , RIO_SPX_CTL2_GB_12P5 , rio_pc_ls_12p5 },
	{ 0x00000000		, 0x00000000		, rio_pc_ls_last },
};

// Returns the base clock period (SRV_CLK) for many timers.
// Usually around 1000 (1 usec), but can vary by +/- 0.5%
//
// Inputs:
// dev_info - device to query
//
// Updates:
// srv      - SRV_CLK in nanoseconds
//
// Returns: error code, 0 means all's good, <> 0 means failure

uint32_t rxs_rio_pc_clk_pd(DAR_DEV_INFO_t *dev_info,
			uint32_t *srv_pd)
{
	uint32_t rck, cck;
	uint32_t ps; // Prescaler
	uint32_t rc;

	*srv_pd = 0;

	rc = DARRegRead(dev_info, RXS_MPM_CFGSIG0, &rck);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, RXS_PRESCALAR_SRV_CLK, &ps);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	if ((ps & ~RXS_PRESCALAR_SRV_CLK_PRESCALAR_SRV_CLK) || (!ps)){
		rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
		goto fail;
	}

	cck = rck & RXS_MPM_CFGSIG0_CORECLK_SELECT;
	rck &= RXS_MPM_CFGSIG0_REFCLK_SELECT;
	ps &= RXS_PRESCALAR_SRV_CLK_PRESCALAR_SRV_CLK;

	switch(cck) {
	case RXS_MPM_CFGSIG0_CORECLK_SELECT_LO_LAT:
		if (42 != ps) {
			rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
			goto fail;
		}
		if (RXS_MPM_CFGSIG0_REFCLK_SELECT_156P25MHZ == rck) {
			*srv_pd = 1000;
		} else {
			*srv_pd = 1001;
		}
		break;
	case RXS_MPM_CFGSIG0_CORECLK_SELECT_RSVD:
		rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
		goto fail;
		break;

	case RXS_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_12G:
		if (RXS_MPM_CFGSIG0_REFCLK_SELECT_156P25MHZ == rck) {
			if (38 != ps) {
				rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
				goto fail;
			}
			*srv_pd = 998;
		} else {
			if (37 != ps) {
				rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
				goto fail;
			}
			*srv_pd = 992;
		}
		break;
	case RXS_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_10G:
		if (31 != ps) {
			rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
			goto fail;
		}
		*srv_pd = 992;
		break;

	default:
		rc = RIO_ERR_SW_FAILURE;
		goto fail;
		break;
	}

	rc = RIO_SUCCESS;
fail:
	return rc;
}

static uint32_t rxs_get_lrto(DAR_DEV_INFO_t *dev_info, uint32_t *lrto)
{
	uint32_t rc;
	uint32_t lrto_reg;
	uint32_t srv_pd;
	uint64_t time;

	rc = DARRegRead(dev_info, RXS_SP_LT_CTL, &lrto_reg);
	if (RIO_SUCCESS != rc) {
		goto exit;
	}

	rc = rxs_rio_pc_clk_pd(dev_info, &srv_pd);
	if (RIO_SUCCESS != rc) {
		goto exit;
	}

	time = (lrto_reg >> 8) * 3 * srv_pd;
	time += RIO_LRTO_NSEC - 1;
	time /= RIO_LRTO_NSEC;
	if (time > RIO_LRTO_MAX_100NS) {
		time = RIO_LRTO_MAX_100NS;
	}
	time /= RIO_LRTO_NSEC;
	if (!time && lrto_reg) {
		time = 1;
	}
	*lrto = time;
exit:
	return rc;
}

static uint32_t rxs_set_lrto(DAR_DEV_INFO_t *dev_info, uint32_t lrto)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t lrto_reg;
	uint32_t srv_pd;
	uint64_t time;

	if (lrto > RIO_LRTO_MAX_100NS) {
		goto exit;
	}

	rc = rxs_rio_pc_clk_pd(dev_info, &srv_pd);
	if (RIO_SUCCESS != rc) {
		goto exit;
	}

	time = lrto * RIO_LRTO_NSEC;
	time += (3 * srv_pd) - 1;
	time /= (3 * srv_pd);
	if (!time && lrto) {
		time = 1;
	}
	if (time > RXS_SP_LT_CTL_MAX) {
		time = RXS_SP_LT_CTL_MAX;
	}

	lrto_reg = time << 8;

	rc = DARRegWrite(dev_info, RXS_SP_LT_CTL, lrto_reg);
exit:
	return rc;
}

uint32_t reg_lswap(enum rio_lane_swap_t swap)
{
	uint32_t reg_val = 0;

	switch(swap) {
	default:
	case rio_lswap_none:
		reg_val = (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_NONE >> 16);
		break;
	case rio_lswap_ABCD_BADC:
		reg_val = (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_1032 >> 16);
		break;
	case rio_lswap_ABCD_DCBA:
		reg_val = (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_3210 >> 16);
		break;
	case rio_lswap_ABCD_CDAB:
		reg_val = (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_2301 >> 16);
		break;
	}
	return reg_val;
}

static enum rio_lane_swap_t lswap(uint32_t reg_val)
{
	enum rio_lane_swap_t swap_val = rio_lswap_none;

	switch(reg_val) {
	default:
	case  (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_NONE >> 16):
		swap_val = rio_lswap_none;
		break;
	case  (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_1032 >> 16):
		swap_val = rio_lswap_ABCD_BADC;
		break;
	case  (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_3210 >> 16):
		swap_val = rio_lswap_ABCD_DCBA;
		break;
	case  (RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX_2301 >> 16):
		swap_val = rio_lswap_ABCD_CDAB;
		break;
	}
	return swap_val;
}

void determine_ls(rio_pc_ls_t *ls, uint32_t ctl2)
{
	uint32_t idx;

	*ls = rio_pc_ls_last;

	for (idx = 0; rxs_ls_check[idx].ls_en; idx++) {
		// If speed is not supported, continue
		if (!(rxs_ls_check[idx].ls_sup & ctl2)) {
			continue;
		}
		// If speed is not enabled, programming error!
		if (!(rxs_ls_check[idx].ls_en & ctl2)) {
			*ls = rio_pc_ls_last;
			break;
		}
		// More than one speed supported & enabled,
		// programming error!
		if (rio_pc_ls_last != *ls) {
			*ls = rio_pc_ls_last;
			break;
		}
		*ls = rxs_ls_check[idx].ls;
	}
}

uint32_t rxs_rio_pc_get_config(DAR_DEV_INFO_t *dev_info,
		rio_pc_get_config_in_t *in_parms,
		rio_pc_get_config_out_t *out_parms)
{
	uint32_t rc;
	uint32_t port_idx;
	uint32_t p_ctl, ctl, ctl2, pol, err_stat, pwr_dn;
	int32_t lane_num;
	struct DAR_ptl good_ptl;
	rio_pc_one_port_config_t *pc;
	uint32_t nmtc_en_mask = RXS_SPX_CTL_INP_EN | RXS_SPX_CTL_OTP_EN;
	uint32_t temp;

	out_parms->num_ports = 0;
	out_parms->imp_rc = 0;
	out_parms->lrto = 0;
	out_parms->log_rto = 0;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_GET_CONFIG(0x1);
		goto exit;
	}

	out_parms->num_ports = good_ptl.num_ports;

	// Always get LRTO
	rc = rxs_get_lrto(dev_info, &out_parms->lrto);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_GET_CONFIG(0x2);
		goto exit;
	}

	for (port_idx = 0; port_idx < out_parms->num_ports; port_idx++) {
		pc = &out_parms->pc[port_idx];
		pc->pnum = good_ptl.pnums[port_idx];

		pc->powered_up = false;
		pc->port_available = false;
		pc->pw = rio_pc_pw_last;
		pc->ls = rio_pc_ls_last;
		pc->iseq = rio_pc_is_last;
		pc->fc = rio_pc_fc_last;
		pc->xmitter_disable = false;
		pc->port_lockout = false;
		pc->nmtc_xfer_enable = false;
		pc->rx_lswap = rio_lswap_none;
		pc->tx_lswap = rio_lswap_none;
		for (lane_num = 0; lane_num < RIO_MAX_PORT_LANES; lane_num++) {
			pc->tx_linvert[lane_num] = false;
			pc->rx_linvert[lane_num] = false;
		}

		// Check that port is available, if not, bail.
		rc = DARRegRead(dev_info, RXS_SPX_ERR_STAT(pc->pnum), &err_stat);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x10);
			goto exit;
		}

		pc->port_available = !(err_stat & RXS_SPX_ERR_STAT_PORT_UNAVL);
		if (!pc->port_available) {
			continue;
		}

		// Check that port is powered up, if not, bail.
		rc = DARRegRead(dev_info, RXS_PLM_SPX_PWDN_CTL(pc->pnum), &pwr_dn);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x20);
			goto exit;
		}

		pc->powered_up = !(pwr_dn & RXS_PLM_SPX_PWDN_CTL_PWDN_PORT);
		if (!pc->powered_up) {
			continue;
		}

		// Check that RapidIO transmitter is enabled...
		rc = DARRegRead(dev_info, RXS_SPX_CTL(pc->pnum), &ctl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x30);
			goto exit;
		}

		pc->xmitter_disable = (ctl & RXS_SPX_CTL_PORT_DIS);

		switch (ctl & RIO_SPX_CTL_PTW_OVER) {
		case RIO_SPX_CTL_PTW_OVER_4X_NO_2X:
		case RIO_SPX_CTL_PTW_OVER_NONE_2:
		case RIO_SPX_CTL_PTW_OVER_NONE:
			if (ctl & RIO_SPX_CTL_PTW_MAX_4X) {
				pc->pw = rio_pc_pw_4x;
			} else if (ctl & RIO_SPX_CTL_PTW_MAX_2X) {
				pc->pw = rio_pc_pw_2x;
			} else {
				pc->pw = rio_pc_pw_1x_l0;
			}
			break;
		case RIO_SPX_CTL_PTW_OVER_1X_L0:
			pc->pw = rio_pc_pw_1x_l0;
			break;
		case RIO_SPX_CTL_PTW_OVER_1X_LR:
			if (ctl & RIO_SPX_CTL_PTW_MAX_4X) {
				pc->pw = rio_pc_pw_1x_l2;
			} else if (ctl & RIO_SPX_CTL_PTW_MAX_2X) {
				pc->pw = rio_pc_pw_1x_l1;
			} else {
				pc->pw = rio_pc_pw_1x_l0;
			}
			break;
		case RIO_SPX_CTL_PTW_OVER_2X_NO_4X:
			pc->pw = rio_pc_pw_2x;
			break;
		default:
			pc->pw = rio_pc_pw_last;
		}

		// Determine configured port speed...
		rc = DARRegRead(dev_info, RXS_SPX_CTL2(pc->pnum), &ctl2);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x50);
			goto exit;
		}

		determine_ls(&pc->ls, ctl2);
		pc->fc = rio_pc_fc_rx;
		pc->port_lockout = (ctl & RXS_SPX_CTL_PORT_LOCKOUT);
		pc->nmtc_xfer_enable = ((ctl & nmtc_en_mask) == nmtc_en_mask);

		// Check for lane swapping & lane inversion
		rc = DARRegRead(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(pc->pnum), &p_ctl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x60);
			goto exit;
		}

		temp = (p_ctl & RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX) >> 16;
		pc->rx_lswap = lswap(temp);
		temp = (p_ctl & RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_TX) >> 18;
		pc->tx_lswap = lswap(temp);

		rc = DARRegRead(dev_info, RXS_PLM_SPX_POL_CTL(pc->pnum), &pol);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x70);
			goto exit;
		}

		pc->tx_linvert[3] = (pol & RXS_PLM_SPX_POL_CTL_TX3_POL);
		pc->tx_linvert[2] = (pol & RXS_PLM_SPX_POL_CTL_TX2_POL);
		pc->tx_linvert[1] = (pol & RXS_PLM_SPX_POL_CTL_TX1_POL);
		pc->tx_linvert[0] = (pol & RXS_PLM_SPX_POL_CTL_TX0_POL);

		pc->rx_linvert[3] = (pol & RXS_PLM_SPX_POL_CTL_RX3_POL);
		pc->rx_linvert[2] = (pol & RXS_PLM_SPX_POL_CTL_RX2_POL);
		pc->rx_linvert[1] = (pol & RXS_PLM_SPX_POL_CTL_RX1_POL);
		pc->rx_linvert[0] = (pol & RXS_PLM_SPX_POL_CTL_RX0_POL);
	}
exit:
	return rc;
}

uint32_t rxs_rio_pc_set_config(DAR_DEV_INFO_t *dev_info,
		rio_pc_set_config_in_t *in_parms,
		rio_pc_set_config_out_t *out_parms)
{
	uint32_t rc = RIO_SUCCESS;

	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->num_ports = in_parms->num_ports;

	rc = rxs_set_lrto(dev_info, in_parms->lrto);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SET_CONFIG(0x10);
		goto fail;
	}

	rc = rxs_get_lrto(dev_info, &out_parms->lrto);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SET_CONFIG(0x20);
	}
fail:
	return rc;
}

bool determine_iseq(enum rio_pc_idle_seq *iseq, rio_pc_ls_t ls, uint32_t plm_ctl)
{
	bool idle_err = false;
	uint32_t idle_overrides = RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE1 |
				RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE2 |
				RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE3;

	// Note: programming error if more than one of
	// RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE1,
	// RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE2, and
	// RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE3 set set.
	*iseq = rio_pc_is_last;
	switch (plm_ctl & idle_overrides) {
	case 0:
		break;
	case RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE1:
		*iseq = rio_pc_is_one;
		break;
	case RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE2:
		*iseq = rio_pc_is_two;
		break;
	case RXS_PLM_SPX_IMP_SPEC_CTL_USE_IDLE3:
		*iseq = rio_pc_is_three;
		break;
	default:
		idle_err = true;
		goto fail;
	}

	switch (ls) {
	case rio_pc_ls_1p25:
	case rio_pc_ls_2p5:
	case rio_pc_ls_3p125:
	case rio_pc_ls_5p0:
		if (rio_pc_is_last == *iseq) {
			*iseq = rio_pc_is_one;
		}
		break;
	case rio_pc_ls_6p25:
		if (rio_pc_is_last == *iseq) {
			*iseq = rio_pc_is_two;
		}
		break;
	case rio_pc_ls_10p3:
	case rio_pc_ls_12p5:
	switch (*iseq) {
		// Programming error to use IDLE1 or IDLE2
		// at more than 6.25 Gbaud.
		case rio_pc_is_one:
		case rio_pc_is_two:
			*iseq = rio_pc_is_last;
			idle_err = true;
			break;
		case rio_pc_is_three:
			break;
		case rio_pc_is_last:
			*iseq = rio_pc_is_three;
			}
		break;
	default:
		idle_err = true;
	}
fail:
	return idle_err;
}

uint32_t rxs_rio_pc_get_status(DAR_DEV_INFO_t *dev_info,
		rio_pc_get_status_in_t *in_parms,
		rio_pc_get_status_out_t *out_parms)
{
	uint32_t rc;
	uint32_t port_idx;
	uint32_t err_stat, ctl, ctl2, p_ctl;
	struct DAR_ptl good_ptl;
	rio_pc_one_port_status_t *ps;
	rio_pc_ls_t ls;
	bool idle_err = false;

	out_parms->num_ports = 0;
	out_parms->imp_rc = RIO_SUCCESS;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_GET_STATUS(1);
		goto exit;
	}

	out_parms->num_ports = good_ptl.num_ports;

	for (port_idx = 0; port_idx < out_parms->num_ports; port_idx++) {
		ps = &out_parms->ps[port_idx];

		ps->pnum = good_ptl.pnums[port_idx];
		ps->pw = rio_pc_pw_last;
		ps->fc = rio_pc_fc_last;
		ps->iseq = rio_pc_is_last;
		ps->port_error = false;
		ps->input_stopped = false;
		ps->output_stopped = false;
		ps->num_lanes = 0;
		ps->first_lane = 0;

		// Port is available and powered up,
		// so let's figure out the status...
		rc = DARRegRead(dev_info, RXS_SPX_ERR_STAT(ps->pnum),
								&err_stat);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_STATUS(0x30);
			goto exit;
		}

		rc = DARRegRead(dev_info, RXS_SPX_CTL(ps->pnum), &ctl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_STATUS(0x40);
			goto exit;
		}

		ps->port_ok = (err_stat & RXS_SPX_ERR_STAT_PORT_OK);
		ps->input_stopped = (err_stat &
					RXS_SPX_ERR_STAT_INPUT_ERR_STOP);
		ps->output_stopped = (err_stat &
					RXS_SPX_ERR_STAT_OUTPUT_ERR_STOP);

		// Port Error is true if a PORT_ERR is present, OR
		// if a OUTPUT_FAIL is present when STOP_FAIL_EN is set.
		ps->port_error = ((err_stat & RXS_SPX_ERR_STAT_PORT_ERR) ||
			((ctl & RXS_SPX_CTL_STOP_FAIL_EN) &&
				(err_stat & RXS_SPX_ERR_STAT_OUTPUT_FAIL)));

		// Idle sequence and port width status are only defined when
		// PORT_OK is asserted...
		if (!ps->port_ok) {
			continue;
		}

		rc = DARRegRead(dev_info, RXS_SPX_CTL2(ps->pnum), &ctl2);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_STATUS(0x50);
			goto exit;
		}

		// Only support RX flow control
		ps->fc = rio_pc_fc_rx;

		// Determine lane speed...
		determine_ls(&ls, ctl2);

		// Determine idle sequence
		rc = DARRegRead(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(ps->pnum),
									&p_ctl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_STATUS(0x60);
			goto exit;
		}

		idle_err = determine_iseq(&ps->iseq, ls, p_ctl);

		if (idle_err) {
			continue;
		}

		ps->first_lane = RXS_FIRST_PORT_LANE(ps->pnum);
		ps->num_lanes = RIO_SPX_CTL_PTW_MAX_LANES(ctl);
		switch (ctl & RXS_SPX_CTL_INIT_PWIDTH) {
		case RIO_SPX_CTL_PTW_INIT_1X_L0:
			ps->pw = rio_pc_pw_1x_l0;
			ps->num_lanes = 1;
			break;
		case RIO_SPX_CTL_PTW_INIT_1X_LR:
			// Using redundant lane.
			// on 4x port, that's lane 2.
			// On 2x port, that's lane 1.
			switch (ps->num_lanes) {
			case 4:
				ps->pw = rio_pc_pw_1x_l2;
				ps->first_lane += 2;
				ps->num_lanes = 1;
				break;
			case 2:
				ps->pw = rio_pc_pw_1x_l1;
				ps->first_lane += 2;
				ps->num_lanes = 1;
				break;
			default:
				// Programming error?
				ps->pw = rio_pc_pw_last;
				ps->first_lane = 0;
				ps->num_lanes = 0;
			}
			break;
		case RIO_SPX_CTL_PTW_INIT_2X:
			ps->pw = rio_pc_pw_2x;
			switch (ps->num_lanes) {
			case 4:
			case 2:
				ps->num_lanes = 2;
				break;
			default:
				// Programming error?
				ps->pw = rio_pc_pw_last;
				ps->first_lane = 0;
				ps->num_lanes = 0;
			}
			break;
		case RIO_SPX_CTL_PTW_INIT_4X:
			ps->pw = rio_pc_pw_4x;
			if (4 != ps->num_lanes) {
				ps->pw = rio_pc_pw_last;
				ps->first_lane = 0;
				ps->num_lanes = 0;
			}
			break;
		default:
			ps->pw = rio_pc_pw_last;
		}
	}

exit:
	return rc;
}

uint32_t rxs_pc_reset_port_util(DAR_DEV_INFO_t *dev_info, rio_port_t port)
{
	uint32_t rc;
	uint32_t orig_p_ctl;
	uint32_t p_ctl;

	rc = DARRegRead(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), &orig_p_ctl);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	// Do not infer a self reset, or reset the register configuration.
	p_ctl = orig_p_ctl & ~(RXS_PLM_SPX_IMP_SPEC_CTL_INFER_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_RESET_REG);

	rc = DARRegWrite(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), p_ctl);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	p_ctl |= RXS_PLM_SPX_IMP_SPEC_CTL_SOFT_RST_PORT;

	rc = DARRegWrite(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), p_ctl);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}
	p_ctl &= ~RXS_PLM_SPX_IMP_SPEC_CTL_SOFT_RST_PORT;

	rc = DARRegWrite(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), p_ctl);
fail:
	return rc;
}

uint32_t rxs_rio_pc_reset_port(DAR_DEV_INFO_t *dev_info,
		rio_pc_reset_port_in_t *in_parms,
		rio_pc_reset_port_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t idx;
	struct DAR_ptl ptl_in;
	struct DAR_ptl good_ptl;
	rio_port_t port;

	out_parms->imp_rc = RIO_SUCCESS;

	if (RIO_ALL_PORTS == in_parms->port_num) {
		ptl_in.num_ports = RIO_ALL_PORTS;
	} else {
		ptl_in.num_ports = 1;
		ptl_in.pnums[0] = in_parms->port_num;
	}

	rc = DARrioGetPortList(dev_info, &ptl_in, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_RESET_PORT(1);
		goto exit;
	}

	for (idx = 0; idx < good_ptl.num_ports; idx++) {
		port = good_ptl.pnums[idx];
		// Do not reset the port being used to access the device
		if (!in_parms->oob_reg_acc && (port == in_parms->reg_acc_port))
		{
			continue;
		}

		if (in_parms->reset_lp) {
			rc = DARRegWrite(dev_info, RXS_SPX_LM_REQ(port),
						RIO_SPX_LM_REQ_CMD_RST_PT);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = PC_RESET_PORT(0x10);
				goto exit;
			}
		}

		rc = rxs_pc_reset_port_util(dev_info, port);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_RESET_PORT(0x30);
			goto exit;
		}

	}
	rc = RIO_SUCCESS;
exit:
	return rc;
}

uint32_t rxs_rio_pc_reset_link_partner(DAR_DEV_INFO_t *dev_info,
		rio_pc_reset_link_partner_in_t *in_parms,
		rio_pc_reset_link_partner_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t p_ctl, orig_p_ctl;
	rio_port_t port;

	out_parms->imp_rc = RIO_SUCCESS;

	if (in_parms->port_num >= NUM_RXS_PORTS(dev_info)) {
		out_parms->imp_rc = PC_RESET_LP(0x1);
		goto exit;
	};

	port = in_parms->port_num;

	rc = DARRegRead(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), &orig_p_ctl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_RESET_LP(0x10);
		goto exit;
	}

	// Do not want to infer a self reset, or to reset the register
	// configuration on this port if/when we reset it...
	p_ctl = orig_p_ctl & ~(RXS_PLM_SPX_IMP_SPEC_CTL_INFER_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_RESET_REG);

	rc = DARRegWrite(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), p_ctl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_RESET_LP(0x20);
		goto exit;
	}

	// Reset the link partner with a device reset...
	rc = DARRegWrite(dev_info, RXS_SPX_LM_REQ(port),
						RIO_SPX_LM_REQ_CMD_RST_DEV);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_RESET_LP(0x30);
		goto exit;
	}

	// More deterministic to just reset this port too...
	if (in_parms->resync_ackids) {
		rc = rxs_pc_reset_port_util(dev_info, port);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_RESET_LP(0x40);
			goto exit;
		}
	}

	rc = RIO_SUCCESS;
exit:
	return rc;
}

uint32_t update_outb_ackid(DAR_DEV_INFO_t *dev_info,
			rio_port_t port,
			RIO_SPX_LM_RESP_STAT_T lresp_stat,
			enum rio_pc_idle_seq iseq)
{
	uint32_t ackid, outb_ackid;
	uint32_t rc = RIO_ERR_SW_FAILURE;

	// Update outbound ackid based on link response data
	// Write link response ackID into outbound ackID register
	switch (iseq) {
	case rio_pc_is_one:
		ackid = (lresp_stat & RIO_SPX_LM_RESP_ACK_ID1) >> 5;
		break;
        case rio_pc_is_two:
		ackid = (lresp_stat & RIO_SPX_LM_RESP_ACK_ID2) >> 5;
		break;
        case rio_pc_is_three:
		ackid = (lresp_stat & RIO_SPX_LM_RESP_ACK_ID3) >> 5;
		break;
	default:
		goto fail;
	}

	outb_ackid = (ackid & RXS_SPX_OUT_ACKID_CSR_OUTB_ACKID) |
			((ackid << 12) & RXS_SPX_OUT_ACKID_CSR_OUTSTD_ACKID) |
			RXS_SPX_OUT_ACKID_CSR_CLR_OUTSTD_ACKID;
	rc = DARRegWrite(dev_info, RXS_SPX_OUT_ACKID_CSR(port), outb_ackid);
fail:
	return rc;
}

uint32_t resync_ackids(DAR_DEV_INFO_t *dev_info,
		rio_pc_clr_errs_in_t *in_parms,
		rio_pc_clr_errs_out_t *out_parms)
{
	uint32_t rc;
	uint32_t err_stat, ctl2, p_ctl, plm_ack_cap;
	uint32_t lp_sw_pt_inf;
	RIO_SPX_LM_RESP_STAT_T lr_is_stat;
	rio_pc_ls_t ls;
	enum rio_pc_idle_seq iseq;
	rio_port_t port = in_parms->port_num;

	rc = DARRegRead(dev_info, RXS_SPX_ERR_STAT(port), &err_stat);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x50);
		goto fail;
	}

	if (RXS_SPX_ERR_STAT_INPUT_ERR_STOP & err_stat) {
		rio_pc_reset_port_in_t rst_in;
		rio_pc_reset_port_out_t rst_out;

		// Don't call rxs_pc_reset_port_util directly,
		// must check that we're not attempting to reset
		// our own local port.
		rst_in.port_num = in_parms->port_num;
		rst_in.oob_reg_acc = false;
		rst_in.reg_acc_port = dev_info->swPortInfo &
							RIO_SW_PORT_INF_PORT;
		rst_in.reset_lp = false;
		rst_in.preserve_config = true;

		rc = rxs_rio_pc_reset_port(dev_info, &rst_in, &rst_out);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = rst_out.imp_rc;
			goto fail;
		}
	}

	rc = DARRegRead(dev_info, RXS_SPX_CTL2(port), &ctl2);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x51);
		goto fail;
	}

	rc = DARRegRead(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), &p_ctl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x52);
		goto fail;
	}

	// Determine idle sequence
	determine_ls(&ls, ctl2);
	if (determine_iseq(&iseq, ls, p_ctl)) {
		out_parms->imp_rc = PC_CLR_ERRS(0x53);
		goto fail;
	}

	// Send link-request port-status control symbol
	rc = DARrioLinkReqNResp(dev_info, port, &lr_is_stat);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x54);
		goto fail;
	}

	// Fail on invalid response
	if (!(RXS_SPX_LM_RESP_RESP_VLD & lr_is_stat)) {
		out_parms->imp_rc = PC_CLR_ERRS(0x55);
		goto fail;
	}

	// Update the outbound ackid value
	rc = update_outb_ackid(dev_info, port, lr_is_stat, iseq);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x56);
		goto fail;
	}

	// Read ackID capture register to clear previous results
	rc = DARRegRead(dev_info, RXS_PLM_SPX_ACKID_CAP(port), &plm_ack_cap);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x57);
		goto fail;
	}

	// Attempt to read link partner Switch Port Information CAR
	// This is likely to fail!
	rc = DARRegRead(in_parms->lp_dev_info, RIO_SW_PORT_INF, &lp_sw_pt_inf);
	if (RIO_SUCCESS == rc) {
		goto fail;
	}

	// Read ackID capture register again to get the ackID
	rc = DARRegRead(dev_info, RXS_PLM_SPX_ACKID_CAP(port), &plm_ack_cap);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x58);
		goto fail;
	}

	plm_ack_cap &= RXS_SPX_IN_ACKID_CSR_INB_ACKID;
	// Write inbound ackID to inbound ackID register
	rc = DARRegWrite(dev_info, RXS_SPX_IN_ACKID_CSR(port), plm_ack_cap);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x59);
		goto fail;
	}

	// Attempt to read link partner Switch Port Information CAR
	// to prove that ackIDs are in sync
	rc = DARRegRead(in_parms->lp_dev_info, RIO_SW_PORT_INF, &lp_sw_pt_inf);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x5A);
	}
fail:
	return rc;
}

uint32_t rxs_rio_pc_clr_errs(DAR_DEV_INFO_t *dev_info,
		rio_pc_clr_errs_in_t *in_parms,
		rio_pc_clr_errs_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	rio_port_t port;

	out_parms->imp_rc = RIO_SUCCESS;

	if (in_parms->port_num >= NUM_RXS_PORTS(dev_info)) {
		out_parms->imp_rc = PC_CLR_ERRS(0x1);
		goto fail;
	}

	if (in_parms->clr_lp_port_err) {
		if  (NULL == in_parms->lp_dev_info) {
			out_parms->imp_rc = PC_CLR_ERRS(0x2);
			goto fail;
		}
		if (!in_parms->num_lp_ports) {
			out_parms->imp_rc = PC_CLR_ERRS(0x3);
			goto fail;
		}
	}

	port = in_parms->port_num;

	rc = rxs_pc_reset_port_util(dev_info, port);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_CLR_ERRS(0x10);
		goto fail;
	}

	// Primary goal is to resynchronize ackIDs without resetting the link
	// partner.  RXS has capabilities which go beyond the standard to
	// support this function.
	if (in_parms->clr_lp_port_err) {
		rc = resync_ackids(dev_info, in_parms, out_parms);
	}
fail:
	return rc;
}

uint32_t rxs_rio_pc_set_reset_config_port(DAR_DEV_INFO_t *dev_info,
					rio_port_t port,
					rio_pc_rst_handling rst)
{
	uint32_t rc;
	uint32_t plm_ctl;
	uint32_t em_int_en;
	uint32_t em_pw_en;

	rc = DARRegRead(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), &plm_ctl);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}
	rc = DARRegRead(dev_info, RXS_EM_RST_INT_EN, &em_int_en);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}
	rc = DARRegRead(dev_info, RXS_EM_RST_PW_EN, &em_pw_en);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	switch (rst) {
	case rio_pc_rst_device:
		plm_ctl |= RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
			RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
		em_int_en &= ~(1 << port);
		em_pw_en &= ~(1 << port);
		break;

        case rio_pc_rst_port:
		plm_ctl &= ~RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST;
		plm_ctl |= RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
		em_int_en &= ~(1 << port);
		em_pw_en &= ~(1 << port);
		break;

        case rio_pc_rst_int:
		plm_ctl &= ~(RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST);
		em_int_en |= 1 << port;
		em_pw_en &= ~(1 << port);
		break;

        case rio_pc_rst_pw:
		plm_ctl &= ~(RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST);
		em_int_en &= ~(1 << port);
		em_pw_en |= 1 << port;
		break;

        case rio_pc_rst_ignore:
		plm_ctl &= ~(RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST);
		em_int_en &= ~(1 << port);
		em_pw_en &= ~(1 << port);
		break;

	default:
		rc = RIO_ERR_INVALID_PARAMETER;
		goto fail;
	}

	rc = DARRegWrite(dev_info, RXS_PLM_SPX_IMP_SPEC_CTL(port), plm_ctl);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}
	rc = DARRegWrite(dev_info, RXS_EM_RST_INT_EN, em_int_en);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}
	rc = DARRegWrite(dev_info, RXS_EM_RST_PW_EN, em_pw_en);
fail:
	return rc;
}

#define RXS_TLM_SPX_FTYPE_FILT_MTC (RXS_TLM_SPX_FTYPE_FILT_F8_OTHER | \
					RXS_TLM_SPX_FTYPE_FILT_F8_PWR | \
					RXS_TLM_SPX_FTYPE_FILT_F8_MWR | \
					RXS_TLM_SPX_FTYPE_FILT_F8_MRR | \
					RXS_TLM_SPX_FTYPE_FILT_F8_MW | \
					RXS_TLM_SPX_FTYPE_FILT_F8_MR)

uint32_t rxs_rio_pc_secure_port(DAR_DEV_INFO_t *dev_info,
		rio_pc_secure_port_in_t *in_parms,
		rio_pc_secure_port_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	struct DAR_ptl good_ptl;
	unsigned int port_idx;
	rio_port_t port;
	uint32_t filter;
	uint32_t spx_ctl;

	out_parms->imp_rc = RIO_SUCCESS;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SECURE_PORT(0x1);
		goto fail;
	}

	if (in_parms->rst >= rio_pc_rst_last) {
		out_parms->imp_rc = PC_SECURE_PORT(0x2);
		goto fail;
	}

	out_parms->bc_mtc_pkts_allowed = false;
	out_parms->MECS_participant = false;
	out_parms->MECS_acceptance = true;
	out_parms->rst = rio_pc_rst_last;

	for (port_idx = 0; port_idx < good_ptl.num_ports; port_idx++) {
		port = good_ptl.pnums[port_idx];
		rc = DARRegRead(dev_info, RXS_SPX_CTL(port), &spx_ctl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SECURE_PORT(0x10);
			goto fail;
		}
		rc = DARRegRead(dev_info, RXS_TLM_SPX_FTYPE_FILT(port), &filter);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SECURE_PORT(0x12);
			goto fail;
		}

		if (in_parms->mtc_pkts_allowed) {
			filter &= ~RXS_TLM_SPX_FTYPE_FILT_MTC;
		} else {
			filter |= RXS_TLM_SPX_FTYPE_FILT_MTC;
		}

		if (in_parms->MECS_participant) {
			spx_ctl |= RXS_SPX_CTL_MULT_CS;
		} else {
			spx_ctl &= ~RXS_SPX_CTL_MULT_CS;
		}

		rc = DARRegWrite(dev_info, RXS_SPX_CTL(port), spx_ctl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SECURE_PORT(0x20);
			goto fail;
		}
		rc = DARRegWrite(dev_info, RXS_TLM_SPX_FTYPE_FILT(port), filter);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SECURE_PORT(0x22);
			goto fail;
		}

		rc = rxs_rio_pc_set_reset_config_port(dev_info, port, in_parms->rst);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SECURE_PORT(0x32);
			goto fail;
		}
	}

	out_parms->bc_mtc_pkts_allowed = in_parms->mtc_pkts_allowed;
	out_parms->MECS_participant = in_parms->MECS_participant;
	out_parms->MECS_acceptance = true;
	out_parms->rst = in_parms->rst;

	rc = RIO_SUCCESS;
fail:
	return rc;
}

uint32_t rxs_rio_pc_dev_reset_config(DAR_DEV_INFO_t *dev_info,
		rio_pc_dev_reset_config_in_t *in_parms,
		rio_pc_dev_reset_config_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	rio_port_t port;

	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->rst = rio_pc_rst_last;

	if (in_parms->rst >= rio_pc_rst_last) {
		out_parms->imp_rc = PC_DEV_RESET_CONFIG(0x01);
		goto fail;
	}

	for (port = 0; port < NUM_RXS_PORTS(dev_info); port++) {
		rc = rxs_rio_pc_set_reset_config_port(dev_info, port, in_parms->rst);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SECURE_PORT(0x10 + port);
			break;
		}
	};

	out_parms->rst = in_parms->rst;
fail:
	return rc;
}

#endif /* RXS_DAR_WANTED */

#ifdef __cplusplus
}
#endif
