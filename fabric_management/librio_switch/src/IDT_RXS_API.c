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

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef UNIT_TESTING
#include <stdarg.h>
#include <setjmp.h>
#include "cmocka.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


static DSF_Handle_t RXS_driver_handle;
static uint32_t num_RXS_driver_instances;




/* Initials counters on selected ports
 */
uint32_t idt_rxs_sc_init_dev_ctrs( DAR_DEV_INFO_t             *dev_info,
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
 */

uint32_t idt_rxs_sc_read_ctrs( DAR_DEV_INFO_t           *dev_info,
                               idt_sc_read_ctrs_in_t    *in_parms,
                               idt_sc_read_ctrs_out_t   *out_parms )
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
                                                        out_parms->imp_rc = SC_READ_RXS_CTRS(0x70 + cntr);
                                                        goto idt_sc_read_rxs_ctr_exit;
                                                };
						if (0x00000001 & (ctl_reg >> 7))
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = true;
						else
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = false;
						switch (ctl_reg & RXS_RIO_SPX_PCNTR_CTL_SEL) {
						case RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PKT:
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pkt;
						break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKT:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pkt;
                                                break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PKTCNTR:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pcntr;
                                                break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKTCNTR:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pcntr;
                                                break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_TTL_PKTCNTR:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_ttl_pcntr;
                                                break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_RETRIES:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_retries;
                                                break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_PNA:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pna;
                                                break;
                                                case RXS_RIO_SPX_PCNTR_CTL_SEL_PKT_DROP:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pkt_drop;
                                                break;
						case RXS_RIO_SPX_PCNTR_CTL_SEL_DISABLED:
							in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_disabled;
						break;
						default:
                                                        in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_last;
						break;
						}

						rc = DARRegRead(dev_info, RXS_RIO_SPX_PCNTR_CNT(port_num, cntr), &in_parms->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].last_inc);
                                                if (RIO_SUCCESS != rc) {
                                                        rc = RIO_ERR_INVALID_PARAMETER;
                                                        out_parms->imp_rc = SC_READ_RXS_CTRS(0x71 + cntr);
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
 * RXS device.
 */
uint32_t idt_rxs_sc_cfg_ctr( DAR_DEV_INFO_t           *dev_info,
                             idt_sc_cfg_rxs_ctr_in_t  *in_parms,
                             idt_sc_cfg_rxs_ctr_out_t *out_parms )
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

	rc = DARRegWrite(dev_info, RXS_RIO_PCNTR_CTL, RXS_RIO_PCNTR_CTL_CNTR_FRZ);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = SC_CFG_RXS_CTRS(0x07);
		goto idt_sc_cfg_rxs_ctr_exit;
	};

	reg_mask = SECOND_BYTE_MASK;
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
		case idt_sc_rio_pkt       :                                                         break;
		case idt_sc_fab_pkt       :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKT;          break;
		case idt_sc_rio_pcntr     :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_PKTCNTR;      break;
		case idt_sc_fab_pcntr     :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_FAB_PKTCNTR;      break;
		case idt_sc_rio_ttl_pcntr :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_RIO_TTL_PKTCNTR;  break;
		case idt_sc_retries       :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_RETRIES;          break;
		case idt_sc_pna           :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_PNA;              break;
		case idt_sc_pkt_drop      :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_PKT_DROP;         break;
		case idt_sc_disabled      :  new_ctl |= RXS_RIO_SPX_PCNTR_CTL_SEL_DISABLED;         break;
		default: out_parms->imp_rc = SC_CFG_RXS_CTRS(0x30);
			goto idt_sc_cfg_rxs_ctr_exit;
		};
	};	

	for (srch_i = 0; srch_i < NUM_PORTS(dev_info); srch_i++)
		p_to_i[srch_i] = RXS2448_MAX_PORTS;

	for (srch_p = 0; srch_p < good_ptl.num_ports; srch_p++) {
		port_num = good_ptl.pnums[srch_p];
		found = false;

		if (in_parms->ctr_en == RXS_RIO_PCNTR_CTL_CNTR_FRZ) {
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
					out_parms->imp_rc = SC_CFG_RXS_CTRS(0x51);
					goto idt_sc_cfg_rxs_ctr_exit;
				}
	
				rc = DARRegWrite(dev_info, RXS_RIO_SPX_PCNTR_CTL(port_num, in_parms->ctr_idx), new_ctl_reg);
				if (RIO_SUCCESS != rc) {
					out_parms->imp_rc = SC_CFG_RXS_CTRS(0x52);
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
						out_parms->imp_rc = SC_CFG_RXS_CTRS(0x53);
						goto idt_sc_cfg_rxs_ctr_exit;
					};
				};
			};
		};
		if (!found) {
			rc = RIO_ERR_INVALID_PARAMETER;
			out_parms->imp_rc = SC_CFG_RXS_CTRS(0x59);
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

#define RXS_MAX_MC_MASKS      0xff

#define RTE_SET_COMMON_0      (RT_FIRST_SUBROUTINE_0+0x0100)
#define PROGRAM_RTE_ENTRIES_0 (RT_FIRST_SUBROUTINE_0+0x1900)
#define PROGRAM_MC_MASKS_0    (RT_FIRST_SUBROUTINE_0+0x1A00)
#define READ_MC_MASKS_0       (RT_FIRST_SUBROUTINE_0+0x1B00)
#define READ_RTE_ENTRIES_0    (RT_FIRST_SUBROUTINE_0+0x1C00)

#define MC_MASK_ADDR(b,m)     ((b)+(8*m))
#define PROGRAM_MC_MASKS(x)   (PROGRAM_MC_MASKS_0+x)

#define SET_ALL               true
#define SET_CHANGED           false

uint32_t idt_rxs_program_mc_masks ( DAR_DEV_INFO_t        *dev_info, 
                                    idt_rt_set_all_in_t   *in_parms,
                                    bool                   set_all, // true if all entries should be set
                                    uint32_t              *imp_rc )
{
    uint32_t rc = RIO_SUCCESS;
    // Note that the base address for RXS2448 and RXS1632
    // are all the same.
    uint8_t  mask_num;
    uint32_t base_addr, mask_mask;
   

    if (IDT_RXS2448_RIO_DEVICE_ID == DEV_CODE(dev_info)) {
       mask_mask = RXS2448_RIO_BC_MC_X_S_CSR_SET;
    } else {
       mask_mask = RXS1632_RIO_BC_MC_X_S_CSR_SET;
    };

    if (RIO_ALL_PORTS == in_parms->set_on_port) {
       base_addr = RXS_RIO_SPX_MC_Y_S_CSR(0, 0);
    } else {
       base_addr = RXS_RIO_SPX_MC_Y_S_CSR(in_parms->set_on_port, 0);
    };

    for (mask_num = 0; mask_num < NUM_MC_MASKS(dev_info); mask_num++) {
       if (in_parms->rt->mc_masks[mask_num].changed || set_all) {
	  if ( in_parms->rt->mc_masks[mask_num].mc_mask & ~mask_mask ) {
             rc = RIO_ERR_INVALID_PARAMETER;
             *imp_rc = PROGRAM_MC_MASKS(3);
             goto idt_rxs_program_mc_masks_exit; 
          };
          rc = DARRegWrite( dev_info, MC_MASK_ADDR(base_addr, mask_num), 
                            in_parms->rt->mc_masks[mask_num].mc_mask & mask_mask );
          if (RIO_SUCCESS != rc) {
             *imp_rc = PROGRAM_MC_MASKS(4);
             goto idt_rxs_program_mc_masks_exit; 
          };
	  in_parms->rt->mc_masks[mask_num].changed = false;
       };
    };
        
idt_rxs_program_mc_masks_exit: 
    return rc;
};

#define DEV_RTE_ADDR(b,n) ((b)+(4*n))
#define DOM_RTE_ADDR(b,n) ((b)+(4*n))
#define PROGRAM_RTE_ENTRIES(x) (PROGRAM_RTE_ENTRIES_0+x)

uint32_t idt_rxs_program_rte_entries ( DAR_DEV_INFO_t        *dev_info, 
                                       idt_rt_set_all_in_t   *in_parms, 
                                       bool                   set_all, // true if all entries should be set
                                       uint32_t              *imp_rc ) 
{
    uint32_t rc = RIO_SUCCESS;
    // Note that the base address for RXS2448 and RXS1632
    // are all the same.
    uint16_t rte_num;
    uint32_t dev_rte_base, dom_rte_base;

    rc = DARRegWrite( dev_info, RXS_RIO_ROUTE_DFLT_PORT, in_parms->rt->default_route );
    if (RIO_SUCCESS != rc) {
       *imp_rc = PROGRAM_RTE_ENTRIES(0x10);
       goto idt_rxs_program_rte_entries_exit; 
    };

    if (RIO_ALL_PORTS == in_parms->set_on_port) {
       dev_rte_base = RXS_RIO_BC_L2_GX_ENTRYY_CSR(0, 0);
       dom_rte_base = RXS_RIO_BC_L1_GX_ENTRYY_CSR(0, 0);
    } else {
       dev_rte_base = RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(in_parms->set_on_port, 0, 0);
       dom_rte_base = RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(in_parms->set_on_port, 0, 0);
    };
    
    for (rte_num = 0; rte_num < IDT_DAR_RT_DOM_TABLE_SIZE; rte_num++) {
       if (in_parms->rt->dom_table[rte_num].changed || set_all) {
          // Validate value to be programmed.
          if (in_parms->rt->dom_table[rte_num].rte_val >= NUM_PORTS(dev_info)) {
	     // Domain table can be a port number, use device table, use default route, or drop.
	     if ((in_parms->rt->dom_table[rte_num].rte_val != IDT_DSF_RT_USE_DEVICE_TABLE    ) && //TODO: we have to chack out rte_val with RXS values, so we need to change init_rt 
	         (in_parms->rt->dom_table[rte_num].rte_val != IDT_DSF_RT_USE_DEFAULT_ROUTE   ) && // to support RXS routing values, like
	         (in_parms->rt->dom_table[rte_num].rte_val != IDT_DSF_RT_NO_ROUTE            ) &&
                 (in_parms->rt->dom_table[rte_num].rte_val != IDT_RXS_DSF_RT_NO_ROUTE        ) &&
                 (in_parms->rt->dom_table[rte_num].rte_val != IDT_RXS_DSF_RT_USE_PACKET_ROUTE) ) { //IDT_RXS_DSF_RT_USE_PACKET_ROUTE or IDT_RXS_DSF_RT_NO_ROUTEIDT_RXS_DSF_RT_NO_ROUTE
                rc = RIO_ERR_INVALID_PARAMETER;
                *imp_rc = PROGRAM_RTE_ENTRIES(1);
                goto idt_rxs_program_rte_entries_exit; 
             };
          };

          rc = DARRegWrite( dev_info, DOM_RTE_ADDR(dom_rte_base, rte_num), 
                            (uint32_t)(in_parms->rt->dom_table[rte_num].rte_val) );
          if (RIO_SUCCESS != rc) {
             *imp_rc = PROGRAM_RTE_ENTRIES(2);
             goto idt_rxs_program_rte_entries_exit; 
          };
	  in_parms->rt->dom_table[rte_num].changed = false;
       };
    };
        
    for (rte_num = 0; rte_num < IDT_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
       if (in_parms->rt->dev_table[rte_num].changed || set_all) {
	  // Validate value to be programmed.
          if (in_parms->rt->dev_table[rte_num].rte_val >= NUM_PORTS(dev_info)) {
	     // Device table can be a port number, a multicast mask, use default route, or drop.
	     if ((IDT_RXS_MC_MASK_IDX_FROM_ROUTE(in_parms->rt->dev_table[rte_num].rte_val)     //TODO: same as above
				                           == IDT_DSF_BAD_MC_MASK         ) &&
	         (in_parms->rt->dev_table[rte_num].rte_val != IDT_DSF_RT_USE_DEFAULT_ROUTE) &&
	         (in_parms->rt->dev_table[rte_num].rte_val != IDT_DSF_RT_NO_ROUTE         ) &&
                 (in_parms->rt->dom_table[rte_num].rte_val != IDT_RXS_DSF_RT_NO_ROUTE     ) ) {
                rc = RIO_ERR_INVALID_PARAMETER;
                *imp_rc = PROGRAM_RTE_ENTRIES(3);
                goto idt_rxs_program_rte_entries_exit; 
             };
          };

          rc = DARRegWrite( dev_info, DEV_RTE_ADDR(dev_rte_base, rte_num),
                            (uint32_t)(in_parms->rt->dev_table[rte_num].rte_val) );
          if (RIO_SUCCESS != rc) {
             *imp_rc = PROGRAM_RTE_ENTRIES(4);
             goto idt_rxs_program_rte_entries_exit; 
          };
          in_parms->rt->dev_table[rte_num].changed = false;
       };
    };
        
idt_rxs_program_rte_entries_exit: 
    return rc;
};

#define RTE_SET_COMMON(x) (RTE_SET_COMMON_0+x)

uint32_t idt_rxs_rt_set_common( DAR_DEV_INFO_t        *dev_info, 
                                idt_rt_set_all_in_t   *in_parms, 
                                idt_rt_set_all_out_t  *out_parms,
                                bool                   set_all  ) // true if all entries should be set
{
    uint32_t rc = RIO_ERR_INVALID_PARAMETER;

    out_parms->imp_rc = RIO_SUCCESS;

    if ( ( ( (uint8_t)(RIO_ALL_PORTS) != in_parms->set_on_port ) && 
           ( in_parms->set_on_port >= NUM_PORTS(dev_info)    ) ) ||
         ( !in_parms->rt) ) 
    {
        out_parms->imp_rc = RTE_SET_COMMON(1);
        goto idt_CPS_rt_set_common_exit;
    }

    if ((NUM_PORTS(dev_info) <= in_parms->rt->default_route) &&
        !(IDT_DSF_RT_NO_ROUTE == in_parms->rt->default_route))   {
        out_parms->imp_rc = RTE_SET_COMMON(2);
        goto idt_CPS_rt_set_common_exit;
    }

    out_parms->imp_rc = RIO_SUCCESS;
    rc = idt_rxs_program_mc_masks( dev_info, in_parms, set_all, &out_parms->imp_rc );
    if (RIO_SUCCESS != rc) {
       goto idt_CPS_rt_set_common_exit;
    }

    rc = idt_rxs_program_rte_entries( dev_info, in_parms, set_all, &out_parms->imp_rc );
    if (RIO_SUCCESS != rc) 
       goto idt_CPS_rt_set_common_exit;

idt_CPS_rt_set_common_exit:

    return rc;
};

/* This function sets the routing table hardware to match every entry
 * in the routing table state structure. 
 * After idt_rt_set_all is called, no entries are marked as changed in
 * the routing table state structure.
 */
uint32_t idt_rxs_rt_set_all( DAR_DEV_INFO_t        *dev_info, 
                             idt_rt_set_all_in_t   *in_parms, 
                             idt_rt_set_all_out_t  *out_parms )
{
    return idt_rxs_rt_set_common(dev_info, in_parms, out_parms, SET_ALL);
}

/* This function sets the the routing table hardware to match every entry
 * that has been changed in the routing table state structure. 
 * Changes must be made using idt_rt_alloc_mc_mask, idt_rt_deallocate_mc_mask,
 * idt_rt_change_rte, and idt_rt_change_mc.
 * After idt_rt_set_changed is called, no entries are marked as changed in
 * the routing table state structure.
 */
uint32_t idt_rxs_rt_set_changed( DAR_DEV_INFO_t            *dev_info, 
                                 idt_rt_set_changed_in_t   *in_parms, 
                                 idt_rt_set_changed_out_t  *out_parms ) 
{
    return idt_rxs_rt_set_common(dev_info, in_parms, out_parms, SET_CHANGED);
}

/* This function updates an idt_rt_state_t structure to
 * change a routing table entry, and tracks changes.
 */
uint32_t idt_rxs_rt_change_rte( DAR_DEV_INFO_t           *dev_info, 
                                idt_rt_change_rte_in_t   *in_parms, 
                                idt_rt_change_rte_out_t  *out_parms ) 
{
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;

   out_parms->imp_rc = RIO_SUCCESS;

   if (!in_parms->rt) {
      out_parms->imp_rc = RT_CHANGE_RTE(1);
      goto idt_rxs_rt_change_rte_exit;
   };

   // Validate rte_value 
   if ( (IDT_RXS_DSF_RT_NO_ROUTE         != in_parms->rte_value) &&
        (in_parms->rte_value             >= NUM_PORTS(dev_info))) {
      out_parms->imp_rc = RT_CHANGE_RTE(2);
      goto idt_rxs_rt_change_rte_exit;
   }

   //if ( (IDT_DSF_RT_USE_DEVICE_TABLE  == in_parms->rte_value) && (!in_parms->dom_entry)) {
   if ( (IDT_RXS_DSF_RT_USE_PACKET_ROUTE  == in_parms->rte_value) && (!in_parms->dom_entry)) {
      out_parms->imp_rc = RT_CHANGE_RTE(3);
      goto idt_rxs_rt_change_rte_exit;
   };

   rc = RIO_SUCCESS;

   // Do not allow any changes to index 0 of the domain table.
   // This must be set to "IDT_RXS_DSF_RT_USE_PACKET_ROUTE" at all times,
   // as this is the behavior required by the RXS RIO Domain register.

   if (in_parms->dom_entry && !in_parms->idx)
      goto idt_rxs_rt_change_rte_exit;

   // If the entry has not already been changed, see if it is being changed
   if (in_parms->dom_entry) {
      if (  !in_parms->rt->dom_table[in_parms->idx].changed ) {
         if (in_parms->rt->dom_table[in_parms->idx].rte_val  != in_parms->rte_value)
             in_parms->rt->dom_table[in_parms->idx].changed = true;
      };
      in_parms->rt->dom_table[in_parms->idx].rte_val = in_parms->rte_value;
   } else {
      if (  !in_parms->rt->dev_table[in_parms->idx].changed ) {
         if (in_parms->rt->dev_table[in_parms->idx].rte_val  != in_parms->rte_value)
             in_parms->rt->dev_table[in_parms->idx].changed = true;
      };
      in_parms->rt->dev_table[in_parms->idx].rte_val = in_parms->rte_value;
   };

idt_rxs_rt_change_rte_exit:
   return rc;
}

uint32_t idt_rxs_rt_initialize( DAR_DEV_INFO_t           *dev_info,
                                idt_rt_initialize_in_t   *in_parms,
                                idt_rt_initialize_out_t  *out_parms ) 
{
    uint32_t rc = RIO_ERR_INVALID_PARAMETER;
    uint32_t destID;
    uint32_t mc_idx;
    idt_rt_set_changed_in_t  all_in;
    idt_rt_set_changed_out_t all_out;
    idt_rt_state_t          *rt_state;

    // Validate parameters

   
    if (  (in_parms->default_route      >= NUM_PORTS(dev_info))  &&
        !(IDT_DSF_RT_NO_ROUTE == in_parms->default_route)  )
    {
        out_parms->imp_rc = RT_INITIALIZE(1);
        goto idt_rxs_rt_initialize_exit;
    }

    if ( (in_parms->default_route_table_port >= NUM_PORTS(dev_info)) &&
         !((IDT_RXS_DSF_RT_NO_ROUTE          == in_parms->default_route_table_port)) )
    {
        out_parms->imp_rc = RT_INITIALIZE(2);
        goto idt_rxs_rt_initialize_exit;
    }

    if ( (in_parms->set_on_port >= NUM_PORTS(dev_info)  )  &&
        !(RIO_ALL_PORTS         == in_parms->set_on_port))
    {
        out_parms->imp_rc = RT_INITIALIZE(3);
        goto idt_rxs_rt_initialize_exit;
    }

    out_parms->imp_rc = RIO_SUCCESS;
    all_in.set_on_port = in_parms->set_on_port;

    if (!in_parms->rt)
    {
       rt_state = (idt_rt_state_t *)malloc(sizeof(idt_rt_state_t));
       all_in.rt = rt_state;
    }
    else
       all_in.rt = in_parms->rt;

    all_in.rt->default_route = in_parms->default_route;

    // Configure initialization of all of the routing table entries
    for (destID = 0; destID < IDT_DAR_RT_DEV_TABLE_SIZE; destID++)
    {
        all_in.rt->dev_table[destID].changed = true ;
        all_in.rt->dev_table[destID].rte_val = in_parms->default_route_table_port;
    };
    
    all_in.rt->dom_table[0].changed = true ;
    all_in.rt->dom_table[0].rte_val = IDT_RXS_DSF_RT_USE_PACKET_ROUTE;

    for (destID = 1; destID < IDT_DAR_RT_DOM_TABLE_SIZE; destID++)
    {
        all_in.rt->dom_table[destID].changed = true ;
        all_in.rt->dom_table[destID].rte_val = in_parms->default_route_table_port;
    };
    
    // Configure initialization of multicast masks and associations as necessary. 
    for (mc_idx = 0; mc_idx < IDT_RXS_DSF_MAX_MC_MASK; mc_idx++) 
    {
       all_in.rt->mc_masks[mc_idx].mc_destID = 0;
       all_in.rt->mc_masks[mc_idx].tt        = tt_dev8;
       all_in.rt->mc_masks[mc_idx].mc_mask   = 0;
       all_in.rt->mc_masks[mc_idx].in_use    = false;
       all_in.rt->mc_masks[mc_idx].allocd    = false;
       if ((mc_idx < RXS_MAX_MC_MASKS) && (mc_idx < IDT_RXS_DSF_MAX_MC_MASK)) {
          all_in.rt->mc_masks[mc_idx].changed   = true ;
       } else {
          all_in.rt->mc_masks[mc_idx].changed   = false ;
       };
    };

    if (in_parms->update_hw) {
       rc = idt_rxs_rt_set_changed(dev_info, &all_in, &all_out );
    } else {
       rc = RIO_SUCCESS;
    }
     
    if (RIO_SUCCESS != rc) {
        out_parms->imp_rc = all_out.imp_rc;
    }

idt_rxs_rt_initialize_exit:

    return rc;
}

typedef struct spx_ctl2_ls_check_info_t_TAG {
	uint32_t      ls_en_val;
	uint32_t      ls_sup_val;
	idt_pc_ls_t   ls;
	uint32_t      prescalar_srv_clk;
} spx_ctl2_ls_check_info_t;

spx_ctl2_ls_check_info_t rxs_ls_check[] = {
	{ RIO_SPX_CTL2_GB_1p25_EN , RIO_SPX_CTL2_GB_1p25 , idt_pc_ls_1p25 , 13 },
	{ RIO_SPX_CTL2_GB_2p5_EN  , RIO_SPX_CTL2_GB_2p5  , idt_pc_ls_2p5  , 13 },
	{ RIO_SPX_CTL2_GB_3p125_EN, RIO_SPX_CTL2_GB_3p125, idt_pc_ls_3p125, 16 },
	{ RIO_SPX_CTL2_GB_5p0_EN  , RIO_SPX_CTL2_GB_5p0  , idt_pc_ls_5p0  , 25 },
	{ RIO_SPX_CTL2_GB_6p25_EN , RIO_SPX_CTL2_GB_6p25 , idt_pc_ls_6p25 , 31 },
	{ RIO_SPX_CTL2_GB_10p3_EN , RIO_SPX_CTL2_GB_10p3 , idt_pc_ls_10p3 ,  0 },/*TODO: prescalar_srv_clk:?*/
	{ RIO_SPX_CTL2_GB_12p5_EN , RIO_SPX_CTL2_GB_12p5 , idt_pc_ls_12p5 ,  0 },/*TODO: prescalar_srv_clk:?*/
	{ 0x00000000              , 0x00000000           , idt_pc_ls_last ,  0 }
};
//TODO: Maybe it needs to add lane to port mapping for this routine.
uint32_t idt_rxs_pc_get_config( DAR_DEV_INFO_t           *dev_info,
                                idt_pc_get_config_in_t   *in_parms,
                                idt_pc_get_config_out_t  *out_parms )
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t port_idx, idx;
	bool   misconfigured = false;
	uint32_t plmCtl, spxCtl, spxCtl2;
	int32_t  lane_num;
	struct DAR_ptl good_ptl;

	out_parms->num_ports = 0;
	out_parms->imp_rc = 0;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SET_CONFIG(0x1);
		goto idt_rxs_pc_get_config_exit;
	};

	out_parms->num_ports = good_ptl.num_ports;
	for (port_idx = 0; port_idx < good_ptl.num_ports; port_idx++)
		out_parms->pc[port_idx].pnum = good_ptl.pnums[port_idx];

	// Always get LRTO
	{ uint32_t lrto;
	rc = DARRegRead(dev_info, RXS_RIO_SP_LT_CTL, &lrto);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SET_CONFIG(0x2);
		goto idt_rxs_pc_get_config_exit;
	}
	out_parms->lrto = lrto >> 8;
	};

	// Always get LOG_RTO
	{ uint32_t log_rto;
	rc = DARRegRead(dev_info, RXS_RIO_SR_RSP_TO, &log_rto);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_SET_CONFIG(0x3);
		goto idt_rxs_pc_get_config_exit;
	}
	out_parms->log_rto = ((log_rto >> 8) * 188) / 100;
	}

	for (port_idx = 0; port_idx < out_parms->num_ports; port_idx++)
	{
		out_parms->pc[port_idx].port_available = true;
		out_parms->pc[port_idx].pw = idt_pc_pw_last;
		out_parms->pc[port_idx].ls = idt_pc_ls_last;
		out_parms->pc[port_idx].iseq = idt_pc_is_one;
		out_parms->pc[port_idx].fc = idt_pc_fc_rx;
		out_parms->pc[port_idx].xmitter_disable = false;
		out_parms->pc[port_idx].port_lockout = false;
		out_parms->pc[port_idx].nmtc_xfer_enable = false;
		out_parms->pc[port_idx].rx_lswap = false;
		out_parms->pc[port_idx].tx_lswap = false;
		for (lane_num = 0; lane_num < IDT_PC_MAX_LANES; lane_num++) {
			out_parms->pc[port_idx].tx_linvert[lane_num] = false;
			out_parms->pc[port_idx].rx_linvert[lane_num] = false;
		};

		// Check that RapidIO transmitter is enabled...
		rc = DARRegRead(dev_info, RXS_RIO_SP0_CTL(port_idx), &spxCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(8);
			goto idt_rxs_pc_get_config_exit;
		};

		out_parms->pc[port_idx].xmitter_disable =
			(spxCtl & RXS_RIO_SP0_CTL_PORT_DIS) ? true : false;

		// OK, port is enabled so it can train.
		// Check for port width overrides...
		rc = DARRegRead(dev_info, RXS_RIO_SP0_CTL(port_idx), &spxCtl);
		if (RIO_SUCCESS != rc)
		{
			out_parms->imp_rc = PC_GET_CONFIG(0x10);
			goto idt_rxs_pc_get_config_exit;
		};
		switch (spxCtl & RIO_SPX_CTL_PTW_OVER) {
		case RIO_SPX_CTL_PTW_OVER_4x_NO_2X:
		case RIO_SPX_CTL_PTW_OVER_NONE_2:
		case RIO_SPX_CTL_PTW_OVER_NONE: out_parms->pc[port_idx].pw = idt_pc_pw_4x;
			break;
		case RIO_SPX_CTL_PTW_OVER_1x_L0: out_parms->pc[port_idx].pw = idt_pc_pw_1x_l0;
			break;
		case RIO_SPX_CTL_PTW_OVER_1x_LR: out_parms->pc[port_idx].pw = idt_pc_pw_1x_l2;
			break;
		case RIO_SPX_CTL_PTW_OVER_2x_NO_4X: out_parms->pc[port_idx].pw = idt_pc_pw_2x;
			break;
		default: out_parms->pc[port_idx].pw = idt_pc_pw_last;
		};

		// Determine configured port speed...
		rc = DARRegRead(dev_info, RXS_RIO_SP0_CTL2, &spxCtl2);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x11);
			goto idt_rxs_pc_get_config_exit;
		};

		out_parms->pc[port_idx].ls = idt_pc_ls_last;
		misconfigured = false;

		for (idx = 0; (rxs_ls_check[idx].ls_en_val) && !misconfigured; idx++) {
			if (rxs_ls_check[idx].ls_en_val & spxCtl2) {
				if (!(rxs_ls_check[idx].ls_sup_val & spxCtl2)) {
					misconfigured = true;
					out_parms->pc[port_idx].ls = idt_pc_ls_last;
				}
				else {
					if (idt_pc_ls_last != out_parms->pc[port_idx].ls) {
						misconfigured = true;
						out_parms->pc[port_idx].ls = idt_pc_ls_last;
					}
					else {
						out_parms->pc[port_idx].ls = rxs_ls_check[idx].ls;
					};
				};
			};
		};

		out_parms->pc[port_idx].port_lockout =
			(spxCtl & RXS_RIO_SP0_CTL_PORT_LOCKOUT) ? true : false;

		out_parms->pc[port_idx].nmtc_xfer_enable =
			((spxCtl & (RXS_RIO_SP0_CTL_INP_EN | RXS_RIO_SP0_CTL_OTP_EN))
				== (RXS_RIO_SP0_CTL_INP_EN | RXS_RIO_SP0_CTL_OTP_EN));

		// Check for lane swapping & inversion
		// LANE SWAPPING AND INVERSION NOT SUPPORTED
		rc = DARRegRead(dev_info, RXS_RIO_PLM_SP0_IMP_SPEC_CTL, &plmCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_CONFIG(0x20);
			goto idt_rxs_pc_get_config_exit;
		};

		if (plmCtl & RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_RX) {
			out_parms->pc[port_idx].rx_lswap = true;
		};

		if (plmCtl & RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_TX) {
			out_parms->pc[port_idx].tx_lswap = true;
		};
	};

idt_rxs_pc_get_config_exit:
	return rc;
}

uint32_t idt_rxs_pc_get_status( DAR_DEV_INFO_t           *dev_info,
                                idt_pc_get_status_in_t   *in_parms,
                                idt_pc_get_status_out_t  *out_parms )
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint8_t  port_idx;
	uint32_t errStat, spxCtl;
	struct DAR_ptl good_ptl;

	out_parms->num_ports = 0;
	out_parms->imp_rc = RIO_SUCCESS;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = PC_GET_uint32_t(1);
		goto idt_rxs_pc_get_status_exit;
	};

	out_parms->num_ports = good_ptl.num_ports;
	for (port_idx = 0; port_idx < good_ptl.num_ports; port_idx++)
		out_parms->ps[port_idx].pnum = good_ptl.pnums[port_idx];

	for (port_idx = 0; port_idx < out_parms->num_ports; port_idx++)
	{
		out_parms->ps[port_idx].pw = idt_pc_pw_last;
		out_parms->ps[port_idx].port_error = false;
		out_parms->ps[port_idx].input_stopped = false;
		out_parms->ps[port_idx].output_stopped = false;

		// Port is available and powered up, so let's figure out the status...
		rc = DARRegRead(dev_info, RXS_RIO_SP0_ERR_STAT, &errStat);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_uint32_t(0x30 + port_idx);
			goto idt_rxs_pc_get_status_exit;
		};

		rc = DARRegRead(dev_info, RXS_RIO_SP0_CTL(port_idx), &spxCtl);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = PC_GET_uint32_t(0x40 + port_idx);
			goto idt_rxs_pc_get_status_exit;
		};

		out_parms->ps[port_idx].port_ok =
			(errStat & RXS_RIO_SP0_ERR_STAT_PORT_OK) ? true : false;
		out_parms->ps[port_idx].input_stopped =
			(errStat & RXS_RIO_SP0_ERR_STAT_INPUT_ERR_STOP) ? true : false;
		out_parms->ps[port_idx].output_stopped =
			(errStat & RXS_RIO_SP0_ERR_STAT_OUTPUT_ERR_STOP) ? true : false;

		// Port Error is true if a PORT_ERR is present, OR
		// if a OUTPUT_FAIL is present when STOP_FAIL_EN is set.
		out_parms->ps[port_idx].port_error =
			((errStat & RXS_RIO_SP0_ERR_STAT_PORT_ERR) ||
				((spxCtl  & RXS_RIO_SP0_CTL_STOP_FAIL_EN) &&
					(errStat & RXS_RIO_SP0_ERR_STAT_OUTPUT_FAIL)));

		// Baudrate and portwidth status are only defined when
		// PORT_OK is asserted... 
		if (out_parms->ps[port_idx].port_ok) {
			switch (spxCtl & RXS_RIO_SP0_CTL_INIT_PWIDTH) {
			case RIO_SPX_CTL_PTW_INIT_1x_L0: out_parms->ps[port_idx].pw = idt_pc_pw_1x_l0;
				break;
			case RIO_SPX_CTL_PTW_INIT_1x_LR: out_parms->ps[port_idx].pw = idt_pc_pw_1x_l2;
				break;
			case RIO_SPX_CTL_PTW_INIT_2x: out_parms->ps[port_idx].pw = idt_pc_pw_2x;
				break;
			case RIO_SPX_CTL_PTW_INIT_4x: out_parms->ps[port_idx].pw = idt_pc_pw_4x;
				break;
			default:  out_parms->ps[port_idx].pw = idt_pc_pw_last;
			};
		};
	};

idt_rxs_pc_get_status_exit:
	return rc;
}

uint32_t idt_rxs_check_port_for_discard( DAR_DEV_INFO_t     *dev_info, 
                                         idt_rt_probe_in_t  *in_parms, 
                                         idt_rt_probe_out_t *out_parms ) 
{
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;
   uint32_t ctlData;
   uint8_t  port;
   bool  dflt_rt = (IDT_DSF_RT_USE_DEFAULT_ROUTE == out_parms->routing_table_value)?true:false;
   idt_pc_get_config_in_t  cfg_in;
   idt_pc_get_config_out_t cfg_out;
   idt_pc_get_status_in_t  stat_in;
   idt_pc_get_status_out_t stat_out;

   port = (dflt_rt)?in_parms->rt->default_route:out_parms->routing_table_value;

   if (NUM_PORTS(dev_info) <= port) {
      out_parms->reason_for_discard = idt_rt_disc_probe_abort;
      out_parms->imp_rc = RT_PROBE(1);
      goto idt_rxs_check_port_for_discard_exit;
   };

   cfg_in.ptl.num_ports = 1;
   cfg_in.ptl.pnums[0] = port;
   rc = idt_rxs_pc_get_config( dev_info, &cfg_in, &cfg_out );
   if (RIO_SUCCESS != rc) {
      out_parms->reason_for_discard = idt_rt_disc_probe_abort;
      out_parms->imp_rc = RT_PROBE(2);
      goto idt_rxs_check_port_for_discard_exit;
   };

   stat_in.ptl.num_ports = 1;
   stat_in.ptl.pnums[0] = port;
   rc = idt_rxs_pc_get_status( dev_info, &stat_in, &stat_out );
   if (RIO_SUCCESS != rc) {
      out_parms->reason_for_discard = idt_rt_disc_probe_abort;
      out_parms->imp_rc = RT_PROBE(3);
      goto idt_rxs_check_port_for_discard_exit;
   };

   if (!cfg_out.pc[0].port_available) {
      out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_unavail:idt_rt_disc_port_unavail;
   } else { 
      if (!cfg_out.pc[0].powered_up) {
         out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_pwdn:idt_rt_disc_port_pwdn;
      } else {
          if (!stat_out.ps[0].port_ok) {
             if (cfg_out.pc[0].xmitter_disable) {
                out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_lkout_or_dis:
		                                      idt_rt_disc_port_lkout_or_dis;
             } else {
                out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_no_lp:idt_rt_disc_port_no_lp;
	     };
	  } else {
             if (stat_out.ps[0].port_error) {
                out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_fail:idt_rt_disc_port_fail;
             } else {
                if (cfg_out.pc[0].port_lockout) {
                   out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_lkout_or_dis:
			                                 idt_rt_disc_port_lkout_or_dis;
		} else {
                   rc = DARRegRead( dev_info, RXS_RIO_SP0_CTL(port), &ctlData );
                   if (RIO_SUCCESS != rc) {
                      out_parms->reason_for_discard = idt_rt_disc_probe_abort;
                      out_parms->imp_rc = RT_PROBE(4);
                      goto idt_rxs_check_port_for_discard_exit;
                   };
            
                  if ( (RIO_SPX_CTL_INP_EN | RIO_SPX_CTL_OTP_EN) != 
                      ((RIO_SPX_CTL_INP_EN | RIO_SPX_CTL_OTP_EN) & ctlData)) {
                     out_parms->reason_for_discard = (dflt_rt)?idt_rt_disc_dflt_pt_in_out_dis:idt_rt_disc_port_in_out_dis;
                  };
               }
            }
         }
      }
   }

   rc = RIO_SUCCESS;

idt_rxs_check_port_for_discard_exit:

    if (idt_rt_disc_not != out_parms->reason_for_discard)
       out_parms->valid_route = false;

    return rc;
}

uint32_t idt_rxs_rt_probe( DAR_DEV_INFO_t      *dev_info,
                           idt_rt_probe_in_t   *in_parms,
                           idt_rt_probe_out_t  *out_parms )
{
    uint32_t rc = RIO_ERR_INVALID_PARAMETER;
    uint8_t bit;
    uint32_t regVal;

    out_parms->imp_rc                 = RIO_SUCCESS;
    out_parms->valid_route            = false;
    out_parms->routing_table_value    = RIO_ALL_PORTS;
    out_parms->filter_function_active = false; /* not supported on RXS */
    out_parms->trace_function_active  = false; /* not supported on RXS */

    for (bit = 0; bit < NUM_PORTS(dev_info); bit++)
        out_parms->mcast_ports[bit] = false;
    out_parms->reason_for_discard     = idt_rt_disc_probe_abort;

    if (   ((NUM_PORTS(dev_info) <= in_parms->probe_on_port) &&
            (RIO_ALL_PORTS       != in_parms->probe_on_port))  ||
           ( !in_parms->rt           ) ) {
       out_parms->imp_rc = RT_PROBE(0x11);
       goto idt_RXS_rt_probe_exit;
    }
        
    rc = DARRegRead( dev_info, RXS_RIO_PKT_TIME_LIVE, &regVal ) ;
    if ( RIO_SUCCESS != rc ) {
       out_parms->imp_rc = RT_PROBE(0x12);
       goto idt_RXS_rt_probe_exit;
    };
    out_parms->time_to_live_active = (regVal & RXS_RIO_PKT_TIME_LIVE_PKT_TIME_LIVE)?true:false;

    rc = RIO_SUCCESS;

    // Note, no failure possible...
    check_multicast_routing( dev_info, in_parms, out_parms );

    /* Done if hit in multicast masks. */
    if (RIO_ALL_PORTS != out_parms->routing_table_value) 
       goto idt_RXS_rt_probe_exit; 

    /*  Determine routing table value for the specified destination ID.
     *  If out_parms->valid_route is true 
     *  the valid values for out_parms->routing_table_value are
     *  - a valid port number, OR
     *  - IDT_DSF_RT_USE_DEFAULT_ROUTE
     *  When out_parms->routing_table_value is IDT_DSF_RT_USE_DEFAULT_ROUTE, the
     *  default route is a valid switch port number.
     */

    check_unicast_routing( dev_info, in_parms, out_parms );

    if (out_parms->valid_route) {
       rc = idt_rxs_check_port_for_discard( dev_info, in_parms, out_parms );
    }
    
idt_RXS_rt_probe_exit:
    return rc;
}   

uint32_t idt_rxs_rt_dealloc_mc_mask( DAR_DEV_INFO_t                *dev_info,
                                   idt_rt_dealloc_mc_mask_in_t   *in_parms,
                                   idt_rt_dealloc_mc_mask_out_t  *out_parms )
{
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;
   uint8_t  mc_idx;
   uint16_t dev_rte, dom_rte;

   out_parms->imp_rc = RIO_SUCCESS;

   NULL_CHECK;

   if (!in_parms->rt) {
      out_parms->imp_rc = RT_DEALLOC_MC_MASK(1);
      goto idt_rxs_rt_dealloc_mc_mask_exit;
   };

   mc_idx = in_parms->mc_mask_rte - IDT_RXS_DSF_FIRST_MC_MASK;

   if (mc_idx >= IDT_RXS_DSF_MAX_MC_MASK) {
      out_parms->imp_rc = RT_DEALLOC_MC_MASK(2);
      goto idt_rxs_rt_dealloc_mc_mask_exit;
   };

   rc = RIO_SUCCESS;

   for (dev_rte = 0; dev_rte < IDT_DAR_RT_DOM_TABLE_SIZE; dev_rte++) {
      if (in_parms->rt->dev_table[dev_rte].rte_val == in_parms->mc_mask_rte) {
         in_parms->rt->dev_table[dev_rte].changed = true;
         in_parms->rt->dev_table[dev_rte].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
      };
   };

   for (dom_rte = 0; dom_rte < IDT_DAR_RT_DOM_TABLE_SIZE; dom_rte++) {
      if (in_parms->rt->dom_table[dom_rte].rte_val == in_parms->mc_mask_rte) {
         in_parms->rt->dom_table[dom_rte].changed = true;
         in_parms->rt->dom_table[dom_rte].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
      };
   };

   if (in_parms->rt->mc_masks[mc_idx].in_use) {
      dev_rte = in_parms->rt->mc_masks[mc_idx].mc_destID & 0x00FF;
      in_parms->rt->dev_table[dev_rte].changed = true;
      in_parms->rt->dev_table[dev_rte].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
   };

   if ((in_parms->rt->mc_masks[mc_idx].in_use) || (in_parms->rt->mc_masks[mc_idx].allocd)) {
      in_parms->rt->mc_masks[mc_idx].mc_destID = 0;
      in_parms->rt->mc_masks[mc_idx].tt        = tt_dev8;
      in_parms->rt->mc_masks[mc_idx].mc_mask   = 0;
      in_parms->rt->mc_masks[mc_idx].in_use    = false;
      in_parms->rt->mc_masks[mc_idx].allocd    = false;
      in_parms->rt->mc_masks[mc_idx].changed   = true;
   };

idt_rxs_rt_dealloc_mc_mask_exit:
   return rc;
}

#define READ_MC_MASKS(x) (READ_MC_MASKS_0+x)

uint32_t idt_rxs_read_mc_masks( DAR_DEV_INFO_t            *dev_info,
                                uint8_t                    pnum,
                                idt_rt_state_t            *rt,
                                uint32_t                  *imp_rc )  
{
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;
   uint8_t  mask_idx;
   uint32_t reg_val, port_mask;
   idt_rt_dealloc_mc_mask_in_t  d_in_parm;
   idt_rt_dealloc_mc_mask_out_t d_out_parm;

   uint32_t vend_id = dev_info->devID & RXS_RIO_DEV_IDENT_VEND;
   uint32_t dev_id = (dev_info->devID & RXS_RIO_DEV_IDENT_DEVI) >> 16;

   if (RXS_RIO_DEVICE_VENDOR != vend_id)
	   goto idt_rxs_read_mc_masks_exit;

   switch (dev_id) {
   case IDT_RXS2448_RIO_DEVICE_ID:
	   port_mask = RXS2448_RIO_SPX_MC_Y_S_CSR_SET;
	   break;
   case IDT_RXS1632_RIO_DEVICE_ID:
	   port_mask = RXS1632_RIO_SPX_MC_Y_S_CSR_SET;
	   break;
   default:
	   goto idt_rxs_read_mc_masks_exit;
   };

   d_in_parm.rt = rt;
   for (mask_idx = NUM_MC_MASKS(dev_info); mask_idx < IDT_RXS_DSF_MAX_MC_MASK; mask_idx++ )
   {
      d_in_parm.mc_mask_rte = IDT_RXS_DSF_FIRST_MC_MASK + mask_idx;
      rc = idt_rxs_rt_dealloc_mc_mask( dev_info, &d_in_parm, &d_out_parm );
      if (RIO_SUCCESS != rc) 
      {
         *imp_rc = d_out_parm.imp_rc;
         goto idt_rxs_read_mc_masks_exit;
      };
   };

   for (mask_idx = 0; mask_idx < NUM_MC_MASKS(dev_info); mask_idx++)  {
      rc = DARRegRead(dev_info, RXS_RIO_SPX_MC_Y_S_CSR(pnum, mask_idx), &reg_val);
      if (RIO_SUCCESS != rc) {
         *imp_rc = READ_MC_MASKS(1);
         goto idt_rxs_read_mc_masks_exit;
      };

      rt->mc_masks[mask_idx].allocd    = false;
      rt->mc_masks[mask_idx].changed   = false;
      rt->mc_masks[mask_idx].tt        = tt_dev8;
      rt->mc_masks[mask_idx].in_use    = false;
      rt->mc_masks[mask_idx].mc_destID = 0x0;
      rt->mc_masks[mask_idx].mc_mask   = reg_val & port_mask;
   };

idt_rxs_read_mc_masks_exit:
   return rc;
}

// Make sure that we're not orphaning a multicast mask...

uint32_t idt_rxs_tidy_routing_table( DAR_DEV_INFO_t  *dev_info, 
                                     uint8_t          dev_idx, 
                                     idt_rt_state_t  *rt,
                                     uint32_t        *fail_pt )
{
   uint32_t rc = RIO_SUCCESS;
   uint16_t srch_idx;
   bool found_one = false;

   if ((rt->dev_table[dev_idx].rte_val >= IDT_RXS_DSF_FIRST_MC_MASK) && 
       (rt->dev_table[dev_idx].rte_val <  IDT_RXS_DSF_BAD_MC_MASK  )) {
      for (srch_idx = 0; (srch_idx < IDT_DAR_RT_DEV_TABLE_SIZE) && !found_one; srch_idx++) {
	 if (dev_idx == srch_idx)
            continue;
         if (rt->dev_table[dev_idx].rte_val == rt->dev_table[srch_idx].rte_val)
            found_one = true;
      };

      if (!found_one) {
         idt_rt_dealloc_mc_mask_in_t  in_parms;
         idt_rt_dealloc_mc_mask_out_t out_parms;
	 in_parms.rt = rt;
	 in_parms.mc_mask_rte = rt->dev_table[dev_idx].rte_val;
         rc = idt_rxs_rt_dealloc_mc_mask( dev_info, &in_parms, &out_parms );
	 if (RIO_SUCCESS != rc) {
	    *fail_pt = out_parms.imp_rc;
	 };
      };
   };
   return rc;
};

/* This function updates an idt_rt_state_t structure to
 * change a multicast mask value, and tracks changes.
 */
uint32_t idt_rxs_rt_change_mc_mask( DAR_DEV_INFO_t               *dev_info,
                                    idt_rt_change_mc_mask_in_t   *in_parms, 
                                    idt_rt_change_mc_mask_out_t  *out_parms )
{
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;
   uint8_t  chg_idx, dom_idx, dev_idx;
   uint32_t illegal_ports   = ~((1 << IDT_MAX_PORTS      ) - 1);
   uint32_t avail_ports     =   (1 << NUM_PORTS(dev_info)) - 1;

   out_parms->imp_rc = RIO_SUCCESS;

   if (!in_parms->rt) {
      out_parms->imp_rc = CHANGE_MC_MASK(1);
      goto idt_rxs_rt_change_mc_mask_exit;
   }

   // Check destination ID value against tt, and that the multicast mask
   // does not select ports which do not exist on the RXS device.
   if ( (in_parms->mc_info.mc_destID > IDT_LAST_DEV16_DESTID   ) ||
        ((in_parms->mc_info.mc_destID > IDT_LAST_DEV8_DESTID) &&
         (tt_dev8 == in_parms->mc_info.tt                     )) ||
        (in_parms->mc_info.mc_mask & illegal_ports             )) {
      out_parms->imp_rc = CHANGE_MC_MASK(2);
      goto idt_rxs_rt_change_mc_mask_exit;
   }
 
   if (!in_parms->mc_info.in_use) {
      idt_rt_dealloc_mc_mask_in_t  d_in_parm;
      idt_rt_dealloc_mc_mask_out_t d_out_parm;

      d_in_parm.mc_mask_rte = in_parms->mc_mask_rte;
      d_in_parm.rt          = in_parms->rt;

      rc = idt_rxs_rt_dealloc_mc_mask( dev_info, &d_in_parm, &d_out_parm );
      if (RIO_SUCCESS != rc) {
	 out_parms->imp_rc = d_out_parm.imp_rc;
      };
      goto idt_rxs_rt_change_mc_mask_exit;
   };

   // Allow requests to change masks not supported by CPS family
   // but there's nothing to do...

   chg_idx = IDT_RXS_MC_MASK_IDX_FROM_ROUTE(in_parms->mc_mask_rte);
   
   if (chg_idx >= NUM_MC_MASKS(dev_info)) {
      rc = RIO_ERR_INVALID_PARAMETER;
      out_parms->imp_rc = CHANGE_MC_MASK(3);
      goto idt_rxs_rt_change_mc_mask_exit;
   }

   // If entry has not already been changed, see if it is being changed
   if ( !in_parms->rt->mc_masks[chg_idx].changed ) {
      if ((in_parms->rt->mc_masks[chg_idx].mc_mask != in_parms->mc_info.mc_mask) ||
          (in_parms->rt->mc_masks[chg_idx].in_use  != in_parms->mc_info.in_use ))  {
         in_parms->rt->mc_masks[chg_idx].changed = true;
      };
   };

   // Note: The multicast mask must be in use now.  We must make sure that
   // the routing tables are set appropriately.
   dom_idx = (in_parms->mc_info.mc_destID & 0xFF00) >> 8;
   if ((tt_dev16 == in_parms->mc_info.tt) && (dom_idx) 
       && (IDT_RXS_DSF_RT_NO_ROUTE != in_parms->rt->dom_table[dom_idx].rte_val)) {
      in_parms->rt->dom_table[dom_idx].rte_val = IDT_RXS_DSF_RT_USE_PACKET_ROUTE;
      in_parms->rt->dom_table[dom_idx].changed = true;
   };

   dev_idx = (in_parms->mc_info.mc_destID & 0x00FF);
   if (in_parms->mc_mask_rte != in_parms->rt->dev_table[dev_idx].rte_val) {
      rc = idt_rxs_tidy_routing_table(dev_info, dev_idx, in_parms->rt, &out_parms->imp_rc);
      if (RIO_SUCCESS != rc) 
         goto idt_rxs_rt_change_mc_mask_exit;

      in_parms->rt->dev_table[dev_idx].rte_val = in_parms->mc_mask_rte;
      in_parms->rt->dev_table[dev_idx].changed = true;
   };
	 
   in_parms->rt->mc_masks[chg_idx].in_use    = true                       ;
   in_parms->rt->mc_masks[chg_idx].mc_destID = in_parms->mc_info.mc_destID;
   in_parms->rt->mc_masks[chg_idx].tt        = in_parms->mc_info.tt       ;
   in_parms->rt->mc_masks[chg_idx].mc_mask   = (in_parms->mc_info.mc_mask & avail_ports);

   rc = RIO_SUCCESS;
idt_rxs_rt_change_mc_mask_exit:
   return rc;
}

#define READ_RTE_ENTRIES(x) (READ_RTE_ENTRIES_0+x)

uint32_t idt_rxs_read_rte_entries( DAR_DEV_INFO_t            *dev_info,
                                   uint8_t                    pnum,
                                   idt_rt_state_t            *rt,
                                   uint32_t                  *imp_rc )  
{
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;
   uint32_t destID, rte_val, first_mc_destID;
   bool  found_one = false;

   // Fill in default route value
   
   rc = DARRegRead(dev_info, RXS_RIO_ROUTE_DFLT_PORT, &rte_val);
   if (RIO_SUCCESS != rc) {
      *imp_rc = READ_RTE_ENTRIES(1);
      goto idt_rxs_read_rte_entries_exit;
   };

   rt->default_route = (uint8_t)(rte_val & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
   if ( rt->default_route >= NUM_PORTS(dev_info)) {
      rt->default_route = IDT_RXS_DSF_RT_NO_ROUTE;
   }

   // Read all of the domain routing table entries.
   rt->dom_table[0].rte_val = IDT_RXS_DSF_RT_USE_PACKET_ROUTE;
   rt->dom_table[0].changed = false;
   first_mc_destID = 0;

   for (destID = 1; destID < IDT_DAR_RT_DOM_TABLE_SIZE; destID++)
   {
      rt->dom_table[destID].changed = false;

      // Read routing table entry for deviceID
      rc = DARRegRead(dev_info, RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(pnum, 0, destID), &rte_val);
      if (RIO_SUCCESS != rc) {
         *imp_rc = READ_RTE_ENTRIES(4);
         goto idt_rxs_read_rte_entries_exit;
      }
      rte_val &= RXS_RIO_BC_L1_GX_ENTRYY_CSR_ROUTING_VALUE;
      rt->dom_table[destID].rte_val = (uint8_t)(rte_val);

      if (IDT_RXS_DSF_RT_USE_PACKET_ROUTE == rte_val) {
         if (!found_one) {
            first_mc_destID = (uint16_t)(destID) << 8;
            found_one = true;
         };
      } else {
        if ((IDT_RXS_DSF_RT_NO_ROUTE          != rte_val) &&
            (NUM_PORTS(dev_info)          <= rte_val) ) { 
            rt->dom_table[destID].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
        };
      };
   };
   
   // Read all of the device routing table entries.
   // Update multicast entries as we go...
   for (destID = 0; destID < IDT_DAR_RT_DEV_TABLE_SIZE; destID++)
   {
      uint8_t mask_idx;

      rt->dev_table[destID].changed = false;
      rc = DARRegRead(dev_info, RXS_RIO_BC_L2_GX_ENTRYY_CSR(pnum, destID), &rte_val);
      if (RIO_SUCCESS != rc) {
         *imp_rc = READ_RTE_ENTRIES(8);
         goto idt_rxs_read_rte_entries_exit;
      }

      rte_val &= RXS_RIO_BC_L2_GX_ENTRYY_CSR_ROUTING_VALUE;

      rt->dev_table[destID].rte_val = (uint8_t)(rte_val);

      mask_idx = IDT_RXS_MC_MASK_IDX_FROM_ROUTE(rte_val);
      if ((IDT_DSF_BAD_MC_MASK != mask_idx) && !(rt->mc_masks[mask_idx].in_use)) {
         rt->mc_masks[mask_idx].tt        = tt_dev16;
         rt->mc_masks[mask_idx].in_use    = true;
         rt->mc_masks[mask_idx].mc_destID = first_mc_destID + destID;
      };
         
      if (  ((rte_val >= NUM_PORTS(dev_info)) && (rte_val < IDT_RXS_DSF_FIRST_MC_MASK))        ||
            ((rte_val >= IDT_RXS_DSF_BAD_MC_MASK) && (IDT_RXS_DSF_RT_NO_ROUTE          != rte_val) 
                                             /*&& (IDT_DSF_RT_USE_DEFAULT_ROUTE != rte_val)*/) ) {
         rt->dev_table[destID].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
      };
   };
   
idt_rxs_read_rte_entries_exit:
   return rc;
}

/* This function returns the complete hardware state of packet routing
 * in a routing table state structure.
 *
 * The routing table hardware must be initialized using idt_rt_initialize() 
 * before calling this routine.
 */
uint32_t idt_rxs_rt_probe_all( DAR_DEV_INFO_t          *dev_info,
                               idt_rt_probe_all_in_t   *in_parms,
                               idt_rt_probe_all_out_t  *out_parms ) 
{
    uint32_t rc = RIO_ERR_INVALID_PARAMETER;
    uint8_t  probe_port;

    out_parms->imp_rc = RIO_SUCCESS;
    if ( ( ( (uint8_t)(RIO_ALL_PORTS) != in_parms->probe_on_port ) && 
           ( in_parms->probe_on_port >= NUM_PORTS(dev_info)    ) ) ||
         ( !in_parms->rt) ) 
    {
        out_parms->imp_rc = RT_PROBE_ALL(1);
        goto idt_rxs_rt_probe_all_exit;
    }

    probe_port = (RIO_ALL_PORTS == in_parms->probe_on_port)?0:in_parms->probe_on_port;

    rc = idt_rxs_read_mc_masks( dev_info, probe_port, in_parms->rt, &out_parms->imp_rc );
    if (RIO_SUCCESS != rc)
       goto idt_rxs_rt_probe_all_exit;
    
    rc = idt_rxs_read_rte_entries( dev_info, probe_port, in_parms->rt, &out_parms->imp_rc );

idt_rxs_rt_probe_all_exit:
    return rc;
}

uint32_t idt_rxs_rioSetEnumBound( DAR_DEV_INFO_t *dev_info,
                                  struct DAR_ptl *ptl,
			          int             enum_bnd_val )
{
	if (NULL != dev_info || !ptl || enum_bnd_val)
		return RIO_SUCCESS;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_rioSetAddrMode( DAR_DEV_INFO_t *dev_info,
                                 RIO_PE_ADDR_T   addr_mode )
{
	if (NULL != dev_info || addr_mode)
		return RIO_SUCCESS;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_pc_dev_reset_config( DAR_DEV_INFO_t                 *dev_info,
	                              idt_pc_dev_reset_config_in_t   *in_parms,
	                              idt_pc_dev_reset_config_out_t  *out_parms )
{
	if (NULL != dev_info)
		out_parms->rst = in_parms->rst;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_em_cfg_pw( DAR_DEV_INFO_t       *dev_info,
	                    idt_em_cfg_pw_in_t   *in_parms,
	                    idt_em_cfg_pw_out_t  *out_parms )
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->imp_rc;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_em_dev_rpt_ctl( DAR_DEV_INFO_t            *dev_info,
	                         idt_em_dev_rpt_ctl_in_t   *in_parms,
	                         idt_em_dev_rpt_ctl_out_t  *out_parms )
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

	return RIO_SUCCESS;
}

uint32_t idt_rxs_DeviceSupported( DAR_DEV_INFO_t *DAR_info )
{
	uint32_t rc = DAR_DB_NO_DRIVER;

	if (RXS_RIO_DEVICE_VENDOR == (DAR_info->devID & RIO_DEV_IDENT_VEND))
	{
		if ((IDT_RXS2448_RIO_DEVICE_ID) == ((DAR_info->devID & RIO_DEV_IDENT_DEVI) >> 16))
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
		else if ((IDT_RXS1632_RIO_DEVICE_ID) == ((DAR_info->devID & RIO_DEV_IDENT_DEVI) >> 16))
                {
                        /* Now fill out the DAR_info structure... */
                        rc = DARDB_rioDeviceSupportedDefault(DAR_info);

                        /* Index and information for DSF is the same as the DAR handle */
                        DAR_info->dsf_h = RXS_driver_handle;

                        if (rc == RIO_SUCCESS) {
                                num_RXS_driver_instances++;
                                strncpy(DAR_info->name, "RXS1632", sizeof(DAR_info->name));
                        }
                }
	}
	return rc;
}

uint32_t bind_rxs_DAR_support(void)
{
	DAR_DB_Driver_t DAR_info;

	DARDB_Init_Driver_Info(IDT_TSI_VENDOR_ID, &DAR_info);

	DAR_info.rioDeviceSupported = idt_rxs_DeviceSupported;

	DAR_info.rioSetEnumBound = idt_rxs_rioSetEnumBound;
	DAR_info.rioSetAddrMode = idt_rxs_rioSetAddrMode;

	DARDB_Bind_Driver(&DAR_info);

	return RIO_SUCCESS;
}

/* Routine to bind in all RXS specific Device Specific Function routines.
 * Supports RXS2448
 */

uint32_t bind_rxs2448_DSF_support(void)
{
	IDT_DSF_DB_t idt_driver;

	IDT_DSF_init_driver(&idt_driver);

	idt_driver.dev_type = IDT_RXS2448_RIO_DEVICE_ID;

	idt_driver.idt_pc_get_config = idt_rxs_pc_get_config;
	idt_driver.idt_pc_get_status = idt_rxs_pc_get_status;
	idt_driver.idt_pc_dev_reset_config = idt_rxs_pc_dev_reset_config;

	idt_driver.idt_rt_initialize = idt_rxs_rt_initialize;
	idt_driver.idt_rt_probe = idt_rxs_rt_probe;
	idt_driver.idt_rt_probe_all = idt_rxs_rt_probe_all;
	idt_driver.idt_rt_set_all = idt_rxs_rt_set_all;
        idt_driver.idt_rt_set_changed = idt_rxs_rt_set_changed;
        idt_driver.idt_rt_change_rte = idt_rxs_rt_change_rte;
        idt_driver.idt_rt_alloc_mc_mask = IDT_DSF_rt_alloc_mc_mask;
        idt_driver.idt_rt_dealloc_mc_mask = idt_rxs_rt_dealloc_mc_mask;
        idt_driver.idt_rt_change_mc_mask = idt_rxs_rt_change_mc_mask;

        idt_driver.idt_sc_init_dev_ctrs = idt_rxs_sc_init_dev_ctrs;
	idt_driver.idt_sc_read_ctrs = idt_rxs_sc_read_ctrs;

	idt_driver.idt_em_dev_rpt_ctl = idt_rxs_em_dev_rpt_ctl;
	idt_driver.idt_em_cfg_pw = idt_rxs_em_cfg_pw;

	IDT_DSF_bind_driver(&idt_driver, &RXS_driver_handle);

	return RIO_SUCCESS;
}

/* Routine to bind in all RXS specific Device Specific Function routines.
 * Supports RXS1632
 */

uint32_t bind_rxs1632_DSF_support(void)
{
        IDT_DSF_DB_t idt_driver;

        IDT_DSF_init_driver(&idt_driver);

        idt_driver.dev_type = IDT_RXS1632_RIO_DEVICE_ID;

        idt_driver.idt_pc_get_config = idt_rxs_pc_get_config;
        idt_driver.idt_pc_get_status = idt_rxs_pc_get_status;
        idt_driver.idt_pc_dev_reset_config = idt_rxs_pc_dev_reset_config;

	idt_driver.idt_rt_initialize = idt_rxs_rt_initialize;
	idt_driver.idt_rt_probe = idt_rxs_rt_probe;
        idt_driver.idt_rt_probe_all = idt_rxs_rt_probe_all;
        idt_driver.idt_rt_set_all = idt_rxs_rt_set_all;
        idt_driver.idt_rt_set_changed = idt_rxs_rt_set_changed;
        idt_driver.idt_rt_change_rte = idt_rxs_rt_change_rte;
        idt_driver.idt_rt_alloc_mc_mask = IDT_DSF_rt_alloc_mc_mask;
        idt_driver.idt_rt_dealloc_mc_mask = idt_rxs_rt_dealloc_mc_mask;
        idt_driver.idt_rt_change_mc_mask = idt_rxs_rt_change_mc_mask;

        idt_driver.idt_sc_init_dev_ctrs = idt_rxs_sc_init_dev_ctrs;
        idt_driver.idt_sc_read_ctrs = idt_rxs_sc_read_ctrs;

        idt_driver.idt_em_dev_rpt_ctl = idt_rxs_em_dev_rpt_ctl;
        idt_driver.idt_em_cfg_pw = idt_rxs_em_cfg_pw;

        IDT_DSF_bind_driver(&idt_driver, &RXS_driver_handle);

        return RIO_SUCCESS;
}

#ifdef __cplusplus
}
#endif
