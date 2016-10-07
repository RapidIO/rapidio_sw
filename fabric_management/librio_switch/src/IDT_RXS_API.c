/*
 * ****************************************************************************
 * Copyright (c) 2016, Integrated Device Technology Inc.
 * Copyright (c) 2016, RapidIO Trade Association
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
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

#include "IDT_RXS_API.h"

#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


static DSF_Handle_t RXS_driver_handle;
static uint32_t num_RXS_driver_instances;




/* Initials counters on selected ports
 * */
uint32_t idt_sc_init_dev_rxs_ctrs( DAR_DEV_INFO_t          *dev_info,
                                idt_sc_init_dev_ctrs_in_t  *in_parms,
                                idt_sc_init_dev_ctrs_out_t *out_parms )
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint8_t idx, cntr_i;
	idt_sc_ctr_val_t init_val = INIT_IDT_SC_CTR_VAL;
	struct DAR_ptl good_ptl;

	out_parms->imp_rc = RIO_SUCCESS;

	if (NULL == in_parms->dev_ctrs) {
		out_parms->imp_rc = SC_INIT_RXS_CTRS(0x01);
		goto idt_sc_init_rxs_ctr_exit;
	};

	if (NULL == in_parms->dev_ctrs->p_ctrs) {
		out_parms->imp_rc = SC_INIT_RXS_CTRS(0x02);
		goto idt_sc_init_rxs_ctr_exit;
	};

	if (!in_parms->dev_ctrs->num_p_ctrs ||
		(in_parms->dev_ctrs->num_p_ctrs > IDT_MAX_PORTS) ||
		(in_parms->dev_ctrs->num_p_ctrs < in_parms->dev_ctrs->valid_p_ctrs)) {
		out_parms->imp_rc = SC_INIT_RXS_CTRS(0x03);
		goto idt_sc_init_rxs_ctr_exit;
	};

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_INIT_RXS_CTRS(0x10);
		goto idt_sc_init_rxs_ctr_exit;
	};

	in_parms->dev_ctrs->valid_p_ctrs = good_ptl.num_ports;
	for (idx = 0; idx < good_ptl.num_ports; idx++) {
		in_parms->dev_ctrs->p_ctrs[idx].pnum = good_ptl.pnums[idx];
		in_parms->dev_ctrs->p_ctrs[idx].ctrs_cnt = RXS_NUM_PERF_CTRS;
		for (cntr_i = 0; cntr_i < RXS_NUM_PERF_CTRS; cntr_i++) {
			in_parms->dev_ctrs->p_ctrs[idx].ctrs[cntr_i] = init_val;
		};
	};

	rc = RIO_SUCCESS;

idt_sc_init_rxs_ctr_exit:
	return rc;
}

/* Reads enabled counters on selected ports
 * */

uint32_t idt_sc_read_rxs_ctrs(DAR_DEV_INFO_t         *dev_info,
                            idt_sc_read_ctrs_in_t    *in_parms,
                            idt_sc_read_ctrs_out_t   *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER, ctl_reg;
	uint8_t p_to_i[RXS2448_MAX_PORTS], srch_i, srch_p, port_num, cntr;
	bool found;
	struct DAR_ptl good_ptl;

	out_parms->imp_rc = RIO_SUCCESS;

	if (NULL == in_parms->dev_ctrs) {
		out_parms->imp_rc = SC_READ_RXS_CTRS(0x01);
		goto idt_sc_read_rxs_ctr_exit;
	};

	if (NULL == in_parms->dev_ctrs->p_ctrs) {
		out_parms->imp_rc = SC_READ_RXS_CTRS(0x02);
		goto idt_sc_read_rxs_ctr_exit;
	};

	if (!in_parms->dev_ctrs->num_p_ctrs ||
		(in_parms->dev_ctrs->num_p_ctrs > IDT_MAX_PORTS) ||
		(in_parms->dev_ctrs->num_p_ctrs < in_parms->dev_ctrs->valid_p_ctrs)) {
		out_parms->imp_rc = SC_READ_RXS_CTRS(0x03);
		goto idt_sc_read_rxs_ctr_exit;
	};

	if (((RIO_ALL_PORTS == in_parms->ptl.num_ports) && (in_parms->dev_ctrs->num_p_ctrs < NUM_PORTS(dev_info))) ||
		((RIO_ALL_PORTS != in_parms->ptl.num_ports) && (in_parms->dev_ctrs->num_p_ctrs < in_parms->ptl.num_ports))) {
		out_parms->imp_rc = SC_READ_RXS_CTRS(0x04);
		goto idt_sc_read_rxs_ctr_exit;
	};

	rc = DARRegWrite(dev_info, RXS_RIO_PCNTR_CTL, 0x80000000);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x07);
		goto idt_sc_read_rxs_ctr_exit;
	};

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_READ_RXS_CTRS(0x10);
		goto idt_sc_read_rxs_ctr_exit;
	};

	for (srch_i = 0; srch_i < NUM_PORTS(dev_info); srch_i++)
		p_to_i[srch_i] = RXS2448_MAX_PORTS;

	for (srch_p = 0; srch_p < good_ptl.num_ports; srch_p++) {
		port_num = good_ptl.pnums[srch_p];
		found = false;
		for (srch_i = 0; srch_i < in_parms->dev_ctrs->valid_p_ctrs; srch_i++) {
			if (in_parms->dev_ctrs->p_ctrs[srch_i].pnum == port_num) {
				found = true;
				if ((RXS2448_MAX_PORTS == p_to_i[port_num]) &&
					(RXS_NUM_PERF_CTRS == in_parms->dev_ctrs->p_ctrs[srch_i].ctrs_cnt)) {
					p_to_i[port_num] = srch_i;
				}
				else {
					rc = RIO_ERR_INVALID_PARAMETER;
					out_parms->imp_rc = SC_READ_RXS_CTRS(0x50 + port_num);
					goto idt_sc_read_rxs_ctr_exit;
				};

				for (cntr = 0; cntr < RXS_NUM_PERF_CTRS; cntr++) {
					if (idt_sc_disabled != in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc) {

						rc = DARRegRead(dev_info, RXS_RIO_SPX_PCNTR_CTL(port_num, cntr), &ctl_reg);
                                                if (RIO_SUCCESS != rc) {
                                                        rc = RIO_ERR_INVALID_PARAMETER;
                                                        out_parms->imp_rc = SC_READ_RXS_CTRS(0x71 + cntr);
                                                        goto idt_sc_read_rxs_ctr_exit;
                                                };
						if (0x00000001 & (ctl_reg >> 7))
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = true;
						else
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = false;
						switch (ctl_reg & 0x7f) {
						case 0x00:
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pkt;
						break;
                                                case 0x01:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pkt;
                                                break;
                                                case 0x02:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pcntr;
                                                break;
                                                case 0x03:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pcntr;
                                                break;
                                                case 0x07:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_ttl_pcntr;
                                                break;
                                                case 0x08:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_retries;
                                                break;
                                                case 0x09:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pna;
                                                break;
                                                case 0x0a:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pkt_drop;
                                                break;
						case 0xef:
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_disabled;
						break;
						default:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_last;
						break;
						}

						rc = DARRegRead(dev_info, RXS_RIO_SPX_PCNTR_CNT(port_num, cntr), &in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].last_inc);
                                                if (RIO_SUCCESS != rc) {
                                                        rc = RIO_ERR_INVALID_PARAMETER;
                                                        out_parms->imp_rc = SC_READ_RXS_CTRS(0x70 + cntr);
                                                        goto idt_sc_read_rxs_ctr_exit;
                                                };
                                                in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].total += in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].last_inc; 
					};
				};
			};
		};
		if (!found) {
			rc = RIO_ERR_INVALID_PARAMETER;
			out_parms->imp_rc = SC_READ_RXS_CTRS(0x90 + srch_p);
			goto idt_sc_read_rxs_ctr_exit;
		};
	};
	rc = RIO_SUCCESS;

idt_sc_read_rxs_ctr_exit:
	rc = DARRegWrite(dev_info, RXS_RIO_PCNTR_CTL, 0x00);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x08);
	};
	return rc;
}

#define PRIO_MASK (RXS_RIO_SPX_PCNTR_CTL_PRIO0  | \
		   RXS_RIO_SPX_PCNTR_CTL_PRIO0C | \
                   RXS_RIO_SPX_PCNTR_CTL_PRIO1  | \
		   RXS_RIO_SPX_PCNTR_CTL_PRIO1C | \
		   RXS_RIO_SPX_PCNTR_CTL_PRIO2  | \
		   RXS_RIO_SPX_PCNTR_CTL_PRIO2C | \
		   RXS_RIO_SPX_PCNTR_CTL_PRIO3  | \
		   RXS_RIO_SPX_PCNTR_CTL_PRIO3C)

/* Configure counters on selected ports of a
 *  * RXS device.
 *   */
uint32_t idt_sc_cfg_rxs_ctr(DAR_DEV_INFO_t       *dev_info,
                        idt_sc_cfg_rxs_ctr_in_t  *in_parms,
                        idt_sc_cfg_rxs_ctr_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t new_ctl = 0, ctl_reg, new_ctl_reg, reg_mask;
	uint8_t p_to_i[RXS2448_MAX_PORTS], srch_i, srch_p, port_num;
	bool found;
	struct DAR_ptl good_ptl;

	out_parms->imp_rc = RIO_SUCCESS;

	if (NULL == in_parms->dev_ctrs) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x01);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	if (NULL == in_parms->dev_ctrs->p_ctrs) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x02);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	if (!in_parms->dev_ctrs->num_p_ctrs ||
		(in_parms->dev_ctrs->num_p_ctrs > IDT_MAX_PORTS) ||
		(in_parms->dev_ctrs->num_p_ctrs < in_parms->dev_ctrs->valid_p_ctrs)) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x03);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	if (((RIO_ALL_PORTS == in_parms->ptl.num_ports) && (in_parms->dev_ctrs->num_p_ctrs < NUM_PORTS(dev_info))) ||
		((RIO_ALL_PORTS != in_parms->ptl.num_ports) && (in_parms->dev_ctrs->num_p_ctrs < in_parms->ptl.num_ports))) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x04);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x10);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	if ((in_parms->dev_ctrs->num_p_ctrs   < good_ptl.num_ports) ||
		(in_parms->dev_ctrs->valid_p_ctrs < good_ptl.num_ports)) {
		rc = RIO_ERR_INVALID_PARAMETER;
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x05);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	rc = DARRegWrite(dev_info, RXS_RIO_PCNTR_CTL, 0x80000000);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x07);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	reg_mask = 0x0000FF00;
	// Create SC_CTL
	if (idt_sc_disabled != in_parms->ctr_type) {
	new_ctl = ((uint32_t)(in_parms->prio_mask) << 8) & PRIO_MASK;
	// It is a programming error to have an empty priority mask when counting
	// packets or packet data...
		if (!new_ctl) {
			out_parms->imp_rc = SC_CFG_RXS_CTRS(0x20);
			goto idt_sc_cfg_rxs_ctr_exit;
		};
		
		new_ctl |= (in_parms->tx) ? RXS_RIO_SPX_PCNTR_CTL_TX : 0;
		switch (in_parms->ctr_type) {
		case idt_sc_rio_pkt       :                    break;
		case idt_sc_fab_pkt       :  new_ctl |= 0x01;  break;
		case idt_sc_rio_pcntr     :  new_ctl |= 0x02;  break;
		case idt_sc_fab_pcntr     :  new_ctl |= 0x03;  break;
		case idt_sc_rio_ttl_pcntr :  new_ctl |= 0x07;  break;
		case idt_sc_retries       :  new_ctl |= 0x08;  break;
		case idt_sc_pna           :  new_ctl |= 0x09;  break;
		case idt_sc_pkt_drop      :  new_ctl |= 0x0a;  break;
		case idt_sc_disabled      :  new_ctl |= 0xef;  break;
		default: out_parms->imp_rc = SC_CFG_RXS_CTRS(0x30);
			goto idt_sc_cfg_rxs_ctr_exit;
		};
	};	

	for (srch_i = 0; srch_i < NUM_PORTS(dev_info); srch_i++)
		p_to_i[srch_i] = RXS2448_MAX_PORTS;

	for (srch_p = 0; srch_p < good_ptl.num_ports; srch_p++) {
		port_num = good_ptl.pnums[srch_p];
		found = false;

		if (in_parms->ctr_en == 0x80000000) {
			rc = DARRegWrite(dev_info, RXS_RIO_SPX_PCNTR_EN(port_num), in_parms->ctr_en);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = SC_CFG_RXS_CTRS(0x06);
				goto idt_sc_cfg_rxs_ctr_exit;
			};
		}
		else {
			// To config counters, performance counter control should be enabled.
			// The paramater is zero.
			goto idt_sc_cfg_rxs_ctr_exit;
		};

		for (srch_i = 0; srch_i < in_parms->dev_ctrs->valid_p_ctrs; srch_i++) {
			if (in_parms->dev_ctrs->p_ctrs[srch_i].pnum == port_num) {
				found = true;
				// If the port hasn't previously been programmed and the counter structure is
				// correctly initialized, keep going...
				if ((IDT_MAX_PORTS - 1  == p_to_i[port_num]) &&
					(RXS_NUM_PERF_CTRS == in_parms->dev_ctrs->p_ctrs[srch_i].ctrs_cnt)) {
					p_to_i[port_num] = srch_i;
				}
				else {
					// Port number appears multiple times in the list,
					// or number of performance counters is incorrect/uninitialized...
					rc = RIO_ERR_INVALID_PARAMETER;
					out_parms->imp_rc = SC_CFG_RXS_CTRS(0x40 + port_num);
					goto idt_sc_cfg_rxs_ctr_exit;
				};

				// Always program the control value...
				rc = DARRegRead(dev_info, RXS_RIO_SPX_PCNTR_CTL(port_num, in_parms->ctr_idx), &ctl_reg);
				if (RIO_SUCCESS != rc) {
					out_parms->imp_rc = SC_CFG_RXS_CTRS(0x50);
					goto idt_sc_cfg_rxs_ctr_exit;
				};
				new_ctl_reg = ctl_reg & reg_mask;
				new_ctl_reg |= new_ctl;

				// Performing this count, Count of the total number of code-groups/codewords 
				// transmitted on the RapidIO interface per lane, with TX = 0 is invalid.
				if (in_parms->ctr_type == idt_sc_rio_ttl_pcntr && !in_parms->tx) {
					out_parms->imp_rc = SC_CFG_RXS_CTRS(0x53);
					goto idt_sc_cfg_rxs_ctr_exit;
				}
	
				rc = DARRegWrite(dev_info, RXS_RIO_SPX_PCNTR_CTL(port_num, in_parms->ctr_idx), new_ctl_reg);
				if (RIO_SUCCESS != rc) {
					out_parms->imp_rc = SC_CFG_RXS_CTRS(0x51);
					goto idt_sc_cfg_rxs_ctr_exit;
				};

				if ((in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[in_parms->ctr_idx].sc != in_parms->ctr_type) ||
					(in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[in_parms->ctr_idx].tx != in_parms->tx) ||
					(ctl_reg != new_ctl_reg)) {
					// If the counted value has changed in the structure or in hardware, 
					// zero the counters and zero hardware counters
					in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[in_parms->ctr_idx].sc = in_parms->ctr_type;
					in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[in_parms->ctr_idx].tx = in_parms->tx;
					in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[in_parms->ctr_idx].total = 0;
					in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[in_parms->ctr_idx].last_inc = 0;
					rc = DARRegRead(dev_info, RXS_RIO_SPX_PCNTR_CNT(port_num, in_parms->ctr_idx), &ctl_reg);
					if (RIO_SUCCESS != rc) {
						out_parms->imp_rc = SC_CFG_RXS_CTRS(0x52);
						goto idt_sc_cfg_rxs_ctr_exit;
					};
				};
			};
		};
		if (!found) {
			rc = RIO_ERR_INVALID_PARAMETER;
			out_parms->imp_rc = SC_CFG_RXS_CTRS(0x53);
			goto idt_sc_cfg_rxs_ctr_exit;
		};
	};

	rc = RIO_SUCCESS;

idt_sc_cfg_rxs_ctr_exit:

	rc = DARRegWrite(dev_info, RXS_RIO_PCNTR_CTL, 0x00);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x08);
	};

	return rc;
}

uint32_t IDT_rxs2448_rioSetEnumBound( DAR_DEV_INFO_t *dev_info,
                                           struct DAR_ptl *ptl,
			                      int enum_bnd_val)
{
	if (NULL != dev_info || !ptl || enum_bnd_val)
		return RIO_SUCCESS;

	return RIO_SUCCESS;
}

uint32_t IDT_rxs2448_rioSetAddrMode( DAR_DEV_INFO_t *dev_info,
                                      RIO_PE_ADDR_T addr_mode)
{
	if (NULL != dev_info || addr_mode)
		return RIO_SUCCESS;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_pc_get_config(DAR_DEV_INFO_t           *dev_info,
	                       idt_pc_get_config_in_t   *in_parms,
	                       idt_pc_get_config_out_t  *out_parms)
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_pc_get_status(DAR_DEV_INFO_t         *dev_info,
	                     idt_pc_get_status_in_t   *in_parms,
	                     idt_pc_get_status_out_t  *out_parms)
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_pc_dev_reset_config(DAR_DEV_INFO_t                 *dev_info,
	                             idt_pc_dev_reset_config_in_t   *in_parms,
	                             idt_pc_dev_reset_config_out_t  *out_parms)
{
	if (NULL != dev_info)
		out_parms->rst = in_parms->rst;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_em_cfg_pw(DAR_DEV_INFO_t       *dev_info,
	                   idt_em_cfg_pw_in_t   *in_parms,
	                   idt_em_cfg_pw_out_t  *out_parms)
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->imp_rc;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_em_dev_rpt_ctl(DAR_DEV_INFO_t            *dev_info,
	                        idt_em_dev_rpt_ctl_in_t   *in_parms,
	                        idt_em_dev_rpt_ctl_out_t  *out_parms)
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_rt_probe_all(DAR_DEV_INFO_t           *dev_info, 
                             idt_rt_probe_all_in_t     *in_parms, 
                             idt_rt_probe_all_out_t    *out_parms )
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->probe_on_port;

    return RIO_SUCCESS;
}

uint32_t idt_rxs_rt_set_all(DAR_DEV_INFO_t            *dev_info, 
                            idt_rt_set_all_in_t       *in_parms, 
                            idt_rt_set_all_out_t      *out_parms )
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->set_on_port;

	return RIO_SUCCESS;
}

uint32_t IDT_rxs2448DeviceSupported(DAR_DEV_INFO_t *DAR_info)
{
	uint32_t rc = DAR_DB_NO_DRIVER;

	if (RXS_RIO_DEVICE_VENDOR == (DAR_info->devID & RIO_DEV_IDENT_VEND))
	{
		if ((RXS_RIO_DEVICE_ID) == ((DAR_info->devID & RIO_DEV_IDENT_DEVI) >> 16))
		{
			/* Now fill out the DAR_info structure... */
			rc = DARDB_rioDeviceSupportedDefault(DAR_info);

			/* Index and information for DSF is the same as the DAR handle */
			DAR_info->dsf_h = RXS_driver_handle;

			if (rc == RIO_SUCCESS) {
				num_RXS_driver_instances++;
				strncpy(DAR_info->name, "RXS2448", sizeof(DAR_info->name));
			}
		}
	}
	return rc;
}

uint32_t bind_rxs2448_DAR_support(void)
{
	DAR_DB_Driver_t DAR_info;

	DARDB_Init_Driver_Info(IDT_TSI_VENDOR_ID, &DAR_info);

	DAR_info.rioDeviceSupported = IDT_rxs2448DeviceSupported;

	DAR_info.rioSetEnumBound = IDT_rxs2448_rioSetEnumBound;
	DAR_info.rioSetAddrMode = IDT_rxs2448_rioSetAddrMode;

	DARDB_Bind_Driver(&DAR_info);

	return RIO_SUCCESS;
}

/* Routine to bind in all RXS specific Device Specific Function routines.
 * Supports RXS2448
 * */

uint32_t bind_rxs_DSF_support(void)
{
	IDT_DSF_DB_t idt_driver;

	IDT_DSF_init_driver(&idt_driver);

	idt_driver.dev_type = RXS_RIO_DEVICE_ID;

	idt_driver.idt_pc_get_config = idt_rxs_pc_get_config;
	idt_driver.idt_pc_get_status = idt_rxs_pc_get_status;
	idt_driver.idt_pc_dev_reset_config = idt_rxs_pc_dev_reset_config;

	idt_driver.idt_rt_probe_all = idt_rxs_rt_probe_all;
	idt_driver.idt_rt_set_all = idt_rxs_rt_set_all;

        idt_driver.idt_sc_init_dev_ctrs = idt_sc_init_dev_rxs_ctrs;
	idt_driver.idt_sc_read_ctrs = idt_sc_read_rxs_ctrs;

	idt_driver.idt_em_dev_rpt_ctl = idt_rxs_em_dev_rpt_ctl;
	idt_driver.idt_em_cfg_pw = idt_rxs_em_cfg_pw;

	IDT_DSF_bind_driver(&idt_driver, &RXS_driver_handle);

	return RIO_SUCCESS;
}

#ifdef __cplusplus
}
#endif
