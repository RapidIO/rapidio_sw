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

#include "RapidIO_Source_Config.h"
#include "RapidIO_Device_Access_Routines_API.h"
#include "RapidIO_Port_Config_API.h"
#include "RXS2448.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RXSx_DAR_WANTED

typedef struct spx_ctl2_ls_check_info_t_TAG {
	uint32_t ls_en_val;
	uint32_t ls_sup_val;
	rio_pc_ls_t ls;
	uint32_t prescalar_srv_clk;
} spx_ctl2_ls_check_info_t;

spx_ctl2_ls_check_info_t rxs_ls_check[] = {
	{ RIO_SPX_CTL2_GB_1p25_EN , RIO_SPX_CTL2_GB_1p25 , rio_pc_ls_1p25 , 13 },
	{ RIO_SPX_CTL2_GB_2p5_EN  , RIO_SPX_CTL2_GB_2p5  , rio_pc_ls_2p5  , 13 },
	{ RIO_SPX_CTL2_GB_3p125_EN, RIO_SPX_CTL2_GB_3p125, rio_pc_ls_3p125, 16 },
	{ RIO_SPX_CTL2_GB_5p0_EN  , RIO_SPX_CTL2_GB_5p0  , rio_pc_ls_5p0  , 25 },
	{ RIO_SPX_CTL2_GB_6p25_EN , RIO_SPX_CTL2_GB_6p25 , rio_pc_ls_6p25 , 31 },
	{ RIO_SPX_CTL2_GB_10p3_EN , RIO_SPX_CTL2_GB_10p3 , rio_pc_ls_10p3 ,  0 }, // TODO: prescalar_srv_clk:?
	{ RIO_SPX_CTL2_GB_12p5_EN , RIO_SPX_CTL2_GB_12p5 , rio_pc_ls_12p5 ,  0 }, // TODO: prescalar_srv_clk:?
	{ 0x00000000              , 0x00000000           , rio_pc_ls_last ,  0 },
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

	rc = DARRegRead(dev_info, RXS_RIO_MPM_CFGSIG0, &rck);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, RXS_RIO_PRESCALAR_SRV_CLK, &ps);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	if ((ps & ~RXS_RIO_PRESCALAR_SRV_CLK_PRESCALAR_SRV_CLK) || (!ps)){
		rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
		goto fail;
	}

	cck = rck & RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT;
	rck &= RXS_RIO_MPM_CFGSIG0_REFCLK_SELECT;
	ps &= RXS_RIO_PRESCALAR_SRV_CLK_PRESCALAR_SRV_CLK;

	switch(cck) {
	case RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_LO_LAT:
		if (42 != ps) {
			rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
			goto fail;
		}
		if (RXS_RIO_MPM_CFGSIG0_REFCLK_SELECT_156p25MHz == rck) {
			*srv_pd = 1000;
		} else {
			*srv_pd = 1001;
		}
		break;
	case RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_RSVD:
		rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
		goto fail;
		break;

	case RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_12G:
		if (RXS_RIO_MPM_CFGSIG0_REFCLK_SELECT_156p25MHz == rck) {
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
	case RXS_RIO_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_10G:
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

uint32_t rxs_rio_pc_get_config(DAR_DEV_INFO_t *dev_info,
		rio_pc_get_config_in_t *in_parms,
		rio_pc_get_config_out_t *out_parms)
{
	uint32_t rc;
	uint32_t port_idx, idx;
	bool misconfigured = false;
	uint32_t plmCtl, spxCtl, spxCtl2, plmPol, errStat;
	int32_t lane_num;
	struct DAR_ptl good_ptl;

	out_parms->num_ports = 0;
	out_parms->imp_rc = 0;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SET_CONFIG(0x1);
		goto exit;
	}

	out_parms->num_ports = good_ptl.num_ports;
	for (port_idx = 0; port_idx < good_ptl.num_ports; port_idx++)
		out_parms->pc[port_idx].pnum = good_ptl.pnums[port_idx];

	// Always get LRTO
	{
		uint32_t lrto;
		rc = DARRegRead(dev_info, RXS_RIO_SP_LT_CTL, &lrto);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_SET_CONFIG(0x2);
			goto exit;
		}
		out_parms->lrto = lrto >> 8;
	}

	for (port_idx = 0; port_idx < out_parms->num_ports; port_idx++) {
		out_parms->pc[port_idx].port_available = true;
		out_parms->pc[port_idx].pw = rio_pc_pw_last;
		out_parms->pc[port_idx].ls = rio_pc_ls_last;
		out_parms->pc[port_idx].iseq = rio_pc_is_one;
		out_parms->pc[port_idx].fc = rio_pc_fc_rx;
		out_parms->pc[port_idx].xmitter_disable = false;
		out_parms->pc[port_idx].port_lockout = false;
		out_parms->pc[port_idx].nmtc_xfer_enable = false;
		out_parms->pc[port_idx].rx_lswap = false;
		out_parms->pc[port_idx].tx_lswap = false;
		for (lane_num = 0; lane_num < RIO_MAX_PORT_LANES; lane_num++) {
			out_parms->pc[port_idx].tx_linvert[lane_num] = false;
			out_parms->pc[port_idx].rx_linvert[lane_num] = false;
		}

		// Check that RapidIO transmitter is enabled...
		rc = DARRegRead(dev_info, RXS_RIO_SPX_CTL(port_idx), &spxCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(8);
			goto exit;
		}

		out_parms->pc[port_idx].xmitter_disable =
				(spxCtl & RXS_RIO_SPX_CTL_PORT_DIS) ?
						true : false;

		// OK, port is enabled so it can train.
		// Check for port width overrides...
		rc = DARRegRead(dev_info, RXS_RIO_SPX_CTL(port_idx), &spxCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x10);
			goto exit;
		}

		switch (spxCtl & RIO_SPX_CTL_PTW_OVER) {
		case RIO_SPX_CTL_PTW_OVER_4x_NO_2X:
		case RIO_SPX_CTL_PTW_OVER_NONE_2:
		case RIO_SPX_CTL_PTW_OVER_NONE:
			out_parms->pc[port_idx].pw = rio_pc_pw_4x;
			break;
		case RIO_SPX_CTL_PTW_OVER_1x_L0:
			out_parms->pc[port_idx].pw = rio_pc_pw_1x_l0;
			break;
		case RIO_SPX_CTL_PTW_OVER_1x_LR:
			out_parms->pc[port_idx].pw = rio_pc_pw_1x_l2;
			break;
		case RIO_SPX_CTL_PTW_OVER_2x_NO_4X:
			out_parms->pc[port_idx].pw = rio_pc_pw_2x;
			break;
		default:
			out_parms->pc[port_idx].pw = rio_pc_pw_last;
		}

		// Determine configured port speed...
		rc = DARRegRead(dev_info, RXS_RIO_SPX_CTL2(port_idx), &spxCtl2);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x11);
			goto exit;
		}

		out_parms->pc[port_idx].ls = rio_pc_ls_last;
		misconfigured = false;

		for (idx = 0; (rxs_ls_check[idx].ls_en_val) && !misconfigured;
				idx++) {
			if (rxs_ls_check[idx].ls_en_val & spxCtl2) {
				if (!(rxs_ls_check[idx].ls_sup_val & spxCtl2)) {
					misconfigured = true;
					out_parms->pc[port_idx].ls =
							rio_pc_ls_last;
				} else {
					if (rio_pc_ls_last
							!= out_parms->pc[port_idx].ls) {
						misconfigured = true;
						out_parms->pc[port_idx].ls =
								rio_pc_ls_last;
					} else {
						out_parms->pc[port_idx].ls =
								rxs_ls_check[idx].ls;
					}
				}
			}
		}

		out_parms->pc[port_idx].port_lockout =
				(spxCtl & RXS_RIO_SPX_CTL_PORT_LOCKOUT) ?
						true : false;

		out_parms->pc[port_idx].nmtc_xfer_enable = ((spxCtl
				& (RXS_RIO_SPX_CTL_INP_EN
						| RXS_RIO_SPX_CTL_OTP_EN))
				== (RXS_RIO_SPX_CTL_INP_EN
						| RXS_RIO_SPX_CTL_OTP_EN));

		// Check for lane swapping & inversion
		// LANE SWAPPING AND INVERSION NOT SUPPORTED
		rc = DARRegRead(dev_info,
				RXS_RIO_PLM_SPX_IMP_SPEC_CTL(port_idx),
				&plmCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x20);
			goto exit;
		}

		if (plmCtl & RXS_RIO_PLM_SPX_IMP_SPEC_CTL_SWAP_RX) {
			out_parms->pc[port_idx].rx_lswap = true;
		}

		if (plmCtl & RXS_RIO_PLM_SPX_IMP_SPEC_CTL_SWAP_TX) {
			out_parms->pc[port_idx].tx_lswap = true;
		}

		rc = DARRegRead(dev_info, RXS_RIO_SPX_ERR_STAT(port_idx),
				&errStat);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x30);
			goto exit;
		}

		out_parms->pc[port_idx].port_available = (
				errStat & RXS_RIO_SPX_ERR_STAT_PORT_UNAVL ?
						true : false);
		out_parms->pc[port_idx].port_available = (
				errStat & RXS_RIO_SPX_ERR_STAT_PORT_W_DIS ?
						true : false);

		rc = DARRegRead(dev_info, RXS_PLM_SPX_POL_CTL(port_idx),
				&plmPol);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x40);
			goto exit;
		}

		out_parms->pc[port_idx].tx_linvert[3] = (
				plmPol & RXS_PLM_SPX_POL_CTL_TX3_POL ?
						true : false);
		out_parms->pc[port_idx].rx_linvert[3] = (
				plmPol & RXS_PLM_SPX_POL_CTL_RX3_POL ?
						true : false);

		out_parms->pc[port_idx].tx_linvert[2] = (
				plmPol & RXS_PLM_SPX_POL_CTL_TX2_POL ?
						true : false);
		out_parms->pc[port_idx].rx_linvert[2] = (
				plmPol & RXS_PLM_SPX_POL_CTL_RX2_POL ?
						true : false);

		out_parms->pc[port_idx].tx_linvert[1] = (
				plmPol & RXS_PLM_SPX_POL_CTL_TX1_POL ?
						true : false);
		out_parms->pc[port_idx].rx_linvert[1] = (
				plmPol & RXS_PLM_SPX_POL_CTL_RX1_POL ?
						true : false);

		out_parms->pc[port_idx].tx_linvert[0] = (
				plmPol & RXS_PLM_SPX_POL_CTL_TX0_POL ?
						true : false);
		out_parms->pc[port_idx].rx_linvert[0] = (
				plmPol & RXS_PLM_SPX_POL_CTL_RX0_POL ?
						true : false);
	}

exit:
	return rc;
}

uint32_t rxs_rio_pc_set_config(DAR_DEV_INFO_t *dev_info,
		rio_pc_set_config_in_t *in_parms,
		rio_pc_set_config_out_t *out_parms)
{
	(void)dev_info;

	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->lrto = in_parms->lrto;
	out_parms->log_rto = 0;
	out_parms->num_ports = in_parms->num_ports;
	memcpy(out_parms->pc, in_parms->pc,
	RIO_MAX_PORTS * sizeof(out_parms->pc[0]));

	return RIO_SUCCESS;
}

uint32_t rxs_rio_pc_get_status(DAR_DEV_INFO_t *dev_info,
		rio_pc_get_status_in_t *in_parms,
		rio_pc_get_status_out_t *out_parms)
{
	uint32_t rc;
	uint8_t port_idx;
	uint32_t errStat, spxCtl;
	struct DAR_ptl good_ptl;

	out_parms->num_ports = 0;
	out_parms->imp_rc = RIO_SUCCESS;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_GET_STATUS(1);
		goto exit;
	}

	out_parms->num_ports = good_ptl.num_ports;
	for (port_idx = 0; port_idx < good_ptl.num_ports; port_idx++)
		out_parms->ps[port_idx].pnum = good_ptl.pnums[port_idx];

	for (port_idx = 0; port_idx < out_parms->num_ports; port_idx++) {
		out_parms->ps[port_idx].pw = rio_pc_pw_last;
		out_parms->ps[port_idx].port_error = false;
		out_parms->ps[port_idx].input_stopped = false;
		out_parms->ps[port_idx].output_stopped = false;

		// Port is available and powered up, so let's figure out the status...
		rc = DARRegRead(dev_info, RXS_RIO_SPX_ERR_STAT(port_idx),
				&errStat);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_STATUS(0x30 + port_idx);
			goto exit;
		}

		rc = DARRegRead(dev_info, RXS_RIO_SPX_CTL(port_idx), &spxCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_STATUS(0x40 + port_idx);
			goto exit;
		}

		out_parms->ps[port_idx].port_ok =
				(errStat & RXS_RIO_SPX_ERR_STAT_PORT_OK) ?
						true : false;
		out_parms->ps[port_idx].input_stopped =
				(errStat & RXS_RIO_SPX_ERR_STAT_INPUT_ERR_STOP) ?
						true : false;
		out_parms->ps[port_idx].output_stopped =
				(errStat & RXS_RIO_SPX_ERR_STAT_OUTPUT_ERR_STOP) ?
						true : false;

		// Port Error is true if a PORT_ERR is present, OR
		// if a OUTPUT_FAIL is present when STOP_FAIL_EN is set.
		out_parms->ps[port_idx].port_error =
				((errStat & RXS_RIO_SPX_ERR_STAT_PORT_ERR)
						|| ((spxCtl
								& RXS_RIO_SPX_CTL_STOP_FAIL_EN)
								&& (errStat
										& RXS_RIO_SPX_ERR_STAT_OUTPUT_FAIL)));

		// Baudrate and portwidth status are only defined when
		// PORT_OK is asserted...
		if (out_parms->ps[port_idx].port_ok) {
			switch (spxCtl & RXS_RIO_SPX_CTL_INIT_PWIDTH) {
			case RIO_SPX_CTL_PTW_INIT_1x_L0:
				out_parms->ps[port_idx].pw = rio_pc_pw_1x_l0;
				break;
			case RIO_SPX_CTL_PTW_INIT_1x_LR:
				out_parms->ps[port_idx].pw = rio_pc_pw_1x_l2;
				break;
			case RIO_SPX_CTL_PTW_INIT_2x:
				out_parms->ps[port_idx].pw = rio_pc_pw_2x;
				break;
			case RIO_SPX_CTL_PTW_INIT_4x:
				out_parms->ps[port_idx].pw = rio_pc_pw_4x;
				break;
			default:
				out_parms->ps[port_idx].pw = rio_pc_pw_last;
			}
		}
	}

exit:
	return rc;
}

uint32_t rxs_rio_pc_dev_reset_config(DAR_DEV_INFO_t *dev_info,
		rio_pc_dev_reset_config_in_t *in_parms,
		rio_pc_dev_reset_config_out_t *out_parms)
{
	if (NULL != dev_info) {
		out_parms->rst = in_parms->rst;
	}
	return RIO_SUCCESS;
}

#endif /* RXSx_DAR_WANTED */

#ifdef __cplusplus
}
#endif
