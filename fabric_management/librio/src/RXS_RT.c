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
#include <stdbool.h>

#include "RapidIO_Source_Config.h"
#include "RapidIO_Device_Access_Routines_API.h"
#include "RapidIO_Routing_Table_API.h"
#include "RXS_DeviceDriver.h"
#include "DSF_DB_Private.h"
#include "RXS2448.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RXSx_DAR_WANTED

#define MC_MASK_ADDR(b,m) ((b)+(8*m))
#define NUM_RXS_PORTS(x) ((NUM_PORTS(x) > RXS2448_MAX_PORTS) ? \
		RXS2448_MAX_PORTS : NUM_PORTS(x))

#define DEV_RTE_ADDR(b,n) ((b)+(4*n))
#define DOM_RTE_ADDR(b,n) ((b)+(4*n))

//TODO: Maybe it needs to add lane to port mapping for this routine.
static void rxs_check_multicast_routing(DAR_DEV_INFO_t *dev_info,
		rio_rt_probe_in_t *in_parms, rio_rt_probe_out_t *out_parms)
{
	uint8_t mc_idx, bit;
	uint32_t mc_mask;
	bool found = false;

	for (mc_idx = 0; mc_idx < NUM_MC_MASKS(dev_info); mc_idx++) {
		if ((in_parms->tt == in_parms->rt->mc_masks[mc_idx].tt)
				&& (in_parms->rt->mc_masks[mc_idx].in_use)) {
			if (tt_dev8 == in_parms->tt) {
				mc_mask = 0x00FF;
			} else {
				mc_mask = 0xFFFF;
			}

			if ((in_parms->destID & mc_mask)
					== (in_parms->rt->mc_masks[mc_idx].mc_destID
							& mc_mask)) {
				if (found) {
					out_parms->reason_for_discard =
							rio_rt_disc_mc_mult_masks;
					out_parms->valid_route = false;
					break;
				} else {
					found = true;
					out_parms->routing_table_value = mc_idx
							+ RIO_DSF_FIRST_MC_MASK;
					for (bit = 0;
							bit
									< NUM_RXS_PORTS(
											dev_info);
							bit++)
						out_parms->mcast_ports[bit] =
								((uint32_t)(1
										<< bit)
										& in_parms->rt->mc_masks[mc_idx].mc_mask) ?
										true :
										false;
					if (in_parms->rt->mc_masks[mc_idx].mc_mask) {
						if ((uint32_t)((uint32_t)(1)
								<< in_parms->probe_on_port)
								== in_parms->rt->mc_masks[mc_idx].mc_mask) {
							out_parms->reason_for_discard =
									rio_rt_disc_mc_one_bit;
						} else {
							out_parms->reason_for_discard =
									rio_rt_disc_not;
							out_parms->valid_route =
									true;
						}
					} else {
						out_parms->reason_for_discard =
								rio_rt_disc_mc_empty;
					}
				}
			}
		}
	}

	return;
}

static void rxs_check_unicast_routing(DAR_DEV_INFO_t *dev_info,
		rio_rt_probe_in_t *in_parms, rio_rt_probe_out_t *out_parms)
{
	uint8_t idx;
	uint32_t rte = 0;

	if (NULL == dev_info)
		return;

	if (tt_dev16 == in_parms->tt) {
		idx = (uint8_t)((in_parms->destID & (uint16_t)(0xFF00)) >> 8);
		rte = in_parms->rt->dom_table[idx].rte_val;
	}

	if ((tt_dev8 == in_parms->tt) || (RIO_DSF_RT_USE_DEVICE_TABLE == rte)) {
		idx = (uint8_t)(in_parms->destID & 0x00FF);
		rte = in_parms->rt->dev_table[idx].rte_val;
	}

	out_parms->routing_table_value = rte;
	out_parms->valid_route = true;
	out_parms->reason_for_discard = rio_rt_disc_not;

	if (in_parms->rt->default_route >= NUM_RXS_PORTS(dev_info)) {
		out_parms->valid_route = false;
		out_parms->reason_for_discard = rio_rt_disc_dflt_pt_invalid;
	}

	return;
}

static uint32_t rxs_check_port_for_discard(DAR_DEV_INFO_t *dev_info,
		rio_rt_probe_in_t *in_parms, rio_rt_probe_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t ctlData;
	uint8_t port;
	bool dflt_rt = (RIO_DSF_RT_USE_DEFAULT_ROUTE
			== out_parms->routing_table_value) ? true : false;
	rio_pc_get_config_in_t cfg_in;
	rio_pc_get_config_out_t cfg_out;
	rio_pc_get_status_in_t stat_in;
	rio_pc_get_status_out_t stat_out;

	port = (dflt_rt) ?
			in_parms->rt->default_route :
			out_parms->routing_table_value;

	if (NUM_RXS_PORTS(dev_info) <= port) {
		out_parms->reason_for_discard = rio_rt_disc_probe_abort;
		out_parms->imp_rc = RT_PROBE(1);
		goto exit;
	}

	cfg_in.ptl.num_ports = 1;
	cfg_in.ptl.pnums[0] = port;
	rc = rxs_rio_pc_get_config(dev_info, &cfg_in, &cfg_out);
	if (RIO_SUCCESS != rc) {
		out_parms->reason_for_discard = rio_rt_disc_probe_abort;
		out_parms->imp_rc = RT_PROBE(2);
		goto exit;
	}

	stat_in.ptl.num_ports = 1;
	stat_in.ptl.pnums[0] = port;
	rc = rxs_rio_pc_get_status(dev_info, &stat_in, &stat_out);
	if (RIO_SUCCESS != rc) {
		out_parms->reason_for_discard = rio_rt_disc_probe_abort;
		out_parms->imp_rc = RT_PROBE(3);
		goto exit;
	}

	if (!cfg_out.pc[0].port_available) {
		out_parms->reason_for_discard =
				(dflt_rt) ? rio_rt_disc_dflt_pt_unavail : rio_rt_disc_port_unavail;
	} else {
		if (!cfg_out.pc[0].powered_up) {
			out_parms->reason_for_discard =
					(dflt_rt) ? rio_rt_disc_dflt_pt_pwdn : rio_rt_disc_port_pwdn;
		} else {
			if (!stat_out.ps[0].port_ok) {
				if (cfg_out.pc[0].xmitter_disable) {
					out_parms->reason_for_discard =
							(dflt_rt) ? rio_rt_disc_dflt_pt_lkout_or_dis : rio_rt_disc_port_lkout_or_dis;
				} else {
					out_parms->reason_for_discard =
							(dflt_rt) ? rio_rt_disc_dflt_pt_no_lp : rio_rt_disc_port_no_lp;
				}
			} else {
				if (stat_out.ps[0].port_error) {
					out_parms->reason_for_discard =
							(dflt_rt) ? rio_rt_disc_dflt_pt_fail : rio_rt_disc_port_fail;
				} else {
					if (cfg_out.pc[0].port_lockout) {
						out_parms->reason_for_discard =
								(dflt_rt) ? rio_rt_disc_dflt_pt_lkout_or_dis : rio_rt_disc_port_lkout_or_dis;
					} else {
						rc =
								DARRegRead(
										dev_info,
										RXS_RIO_SPX_CTL(
												port),
										&ctlData);
						if (RIO_SUCCESS != rc) {
							out_parms->reason_for_discard =
									rio_rt_disc_probe_abort;
							out_parms->imp_rc =
									RT_PROBE(
											4);
							goto exit;
						}

						if ((RIO_SPX_CTL_INP_EN
								| RIO_SPX_CTL_OTP_EN)
								!= ((RIO_SPX_CTL_INP_EN
										| RIO_SPX_CTL_OTP_EN)
										& ctlData)) {
							out_parms->reason_for_discard =
									(dflt_rt) ? rio_rt_disc_dflt_pt_in_out_dis : rio_rt_disc_port_in_out_dis;
						}
					}
				}
			}
		}
	}

	rc = RIO_SUCCESS;

exit:
	if (rio_rt_disc_not != out_parms->reason_for_discard) {
		out_parms->valid_route = false;
	}
	return rc;
}

static uint32_t rxs_read_mc_masks(DAR_DEV_INFO_t *dev_info, uint8_t pnum,
		rio_rt_state_t *rt, uint32_t *imp_rc)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint8_t mask_idx;
	uint32_t reg_val, port_mask;
	rio_rt_dealloc_mc_mask_in_t d_in_parm;
	rio_rt_dealloc_mc_mask_out_t d_out_parm;

	uint32_t vend_id = dev_info->devID & RXS_RIO_DEV_IDENT_VEND;
	uint32_t dev_id = (dev_info->devID & RXS_RIO_DEV_IDENT_DEVI) >> 16;

	if (RXS_RIO_DEVICE_VENDOR != vend_id)
		goto exit;

	switch (dev_id) {
	case RIO_DEVI_IDT_RXS2448:
		port_mask = RXS2448_RIO_SPX_MC_Y_S_CSR_SET;
		break;
	case RIO_DEVI_IDT_RXS1632:
		port_mask = RXS1632_RIO_SPX_MC_Y_S_CSR_SET;
		break;
	default:
		goto exit;
	}

	d_in_parm.rt = rt;
	for (mask_idx = NUM_MC_MASKS(dev_info); mask_idx < RIO_DSF_MAX_MC_MASK;
			mask_idx++) {
		d_in_parm.mc_mask_rte = RIO_DSF_FIRST_MC_MASK + mask_idx;
		rc = DSF_rio_rt_dealloc_mc_mask(dev_info, &d_in_parm,
				&d_out_parm);
		if (RIO_SUCCESS != rc) {
			*imp_rc = d_out_parm.imp_rc;
			goto exit;
		}
	}

	for (mask_idx = 0; mask_idx < NUM_MC_MASKS(dev_info); mask_idx++) {
		rc = DARRegRead(dev_info,
				RXS_RIO_SPX_MC_Y_S_CSR(pnum, mask_idx),
				&reg_val);
		if (RIO_SUCCESS != rc) {
			*imp_rc = RXS_READ_MC_MASKS(1);
			goto exit;
		}

		rt->mc_masks[mask_idx].allocd = false;
		rt->mc_masks[mask_idx].changed = false;
		rt->mc_masks[mask_idx].tt = tt_dev8;
		rt->mc_masks[mask_idx].in_use = false;
		rt->mc_masks[mask_idx].mc_destID = 0x0;
		rt->mc_masks[mask_idx].mc_mask = reg_val & port_mask;
	}

exit:
	return rc;
}

static uint32_t rxs_program_mc_masks(DAR_DEV_INFO_t *dev_info,
		rio_rt_set_all_in_t *in_parms,
		bool set_all, // true if all entries should be set
		uint32_t *imp_rc)
{
	uint32_t rc = RIO_SUCCESS;
	// Note that the base address for RXS2448 and RXS1632
	// are all the same.
	uint8_t mask_num;
	uint32_t set_base_addr, clr_base_addr, mask_mask;
	rio_port_t port = in_parms->set_on_port;

	switch (DEV_CODE(dev_info)) {
	case RIO_DEVI_IDT_RXS2448:
		mask_mask = RXS2448_RIO_BC_MC_X_S_CSR_SET;
		break;
	case RIO_DEVI_IDT_RXS1632:
		mask_mask = RXS1632_RIO_BC_MC_X_S_CSR_SET;
		break;
	default:
		rc = RIO_ERR_NO_FUNCTION_SUPPORT;
		*imp_rc = RXS_PROGRAM_MC_MASKS(0x01);
		goto exit;
	}

	if (RIO_ALL_PORTS == port) {
		set_base_addr = RXS_RIO_BC_MC_X_S_CSR(0);
		clr_base_addr = RXS_RIO_BC_MC_X_C_CSR(0);
	} else {
		set_base_addr = RXS_RIO_SPX_MC_Y_S_CSR(port, 0);
		clr_base_addr = RXS_RIO_SPX_MC_Y_C_CSR(port, 0);
	}

	for (mask_num = 0; mask_num < NUM_MC_MASKS(dev_info); mask_num++) {
		if (in_parms->rt->mc_masks[mask_num].changed || set_all) {
			uint32_t mc_mask =
				in_parms->rt->mc_masks[mask_num].mc_mask;
			if (mc_mask & ~mask_mask) {
				rc = RIO_ERR_INVALID_PARAMETER;
				*imp_rc = RXS_PROGRAM_MC_MASKS(3);
				goto exit;
			}

			// If there are bits to set, set them.
			if (mc_mask & mask_mask) {
				rc = DARRegWrite(dev_info,
					MC_MASK_ADDR(set_base_addr, mask_num),
					mc_mask & mask_mask);
				if (RIO_SUCCESS != rc) {
					*imp_rc = RXS_PROGRAM_MC_MASKS(4);
					goto exit;
				}
			}
			// If there are bits to clear, clear them
			if (~mc_mask & mask_mask) {
				rc = DARRegWrite(dev_info,
					MC_MASK_ADDR(clr_base_addr, mask_num),
					~mc_mask & mask_mask);
				if (RIO_SUCCESS != rc) {
					*imp_rc = RXS_PROGRAM_MC_MASKS(4);
					goto exit;
				}
			}
			in_parms->rt->mc_masks[mask_num].changed = false;
		}
	}

exit:
	return rc;
}

static uint32_t rxs_read_rte_entries(DAR_DEV_INFO_t *dev_info, uint8_t pnum,
		rio_rt_state_t *rt, uint32_t *imp_rc)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t destID, rte_val, first_mc_destID;
	bool found_one = false;

	// Fill in default route value

	rc = DARRegRead(dev_info, RXS_RIO_ROUTE_DFLT_PORT, &rte_val);
	if (RIO_SUCCESS != rc) {
		*imp_rc = RXS_READ_RTE_ENTRIES(1);
		goto exit;
	}

	rt->default_route = (uint8_t)(rte_val
			& RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
	if (rt->default_route >= NUM_RXS_PORTS(dev_info)) {
		rt->default_route = RIO_DSF_RT_NO_ROUTE;
	}

	// Read all of the domain routing table entries.
	rt->dom_table[0].rte_val = RIO_DSF_RT_USE_DEVICE_TABLE;
	rt->dom_table[0].changed = false;
	first_mc_destID = 0;

	for (destID = 1; destID < RIO_DAR_RT_DOM_TABLE_SIZE; destID++) {
		rt->dom_table[destID].changed = false;

		// Read routing table entry for deviceID
		rc = DARRegRead(dev_info,
				RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(pnum, 0, destID),
				&rte_val);
		if (RIO_SUCCESS != rc) {
			*imp_rc = RXS_READ_RTE_ENTRIES(4);
			goto exit;
		}
		rte_val &= RXS_RIO_BC_L1_GX_ENTRYY_CSR_ROUTING_VALUE;
		rt->dom_table[destID].rte_val = (uint8_t)(rte_val);

		if (RIO_DSF_RT_USE_DEVICE_TABLE == rte_val) {
			if (!found_one) {
				first_mc_destID = (uint16_t)(destID) << 8;
				found_one = true;
			}
		} else {
			if ((RIO_DSF_RT_USE_DEFAULT_ROUTE != rte_val)
					&& (RIO_DSF_RT_NO_ROUTE != rte_val)
					&& (NUM_RXS_PORTS(dev_info) <= rte_val)) {
				rt->dom_table[destID].rte_val =
						RIO_DSF_RT_NO_ROUTE;
			}
		}
	}

	// Read all of the device routing table entries.
	// Update multicast entries as we go...
	for (destID = 0; destID < RIO_DAR_RT_DEV_TABLE_SIZE; destID++) {
		uint32_t mask_idx;

		rt->dev_table[destID].changed = false;
		rc = DARRegRead(dev_info,
				RXS_RIO_BC_L2_GX_ENTRYY_CSR(pnum, destID),
				&rte_val);
		if (RIO_SUCCESS != rc) {
			*imp_rc = RXS_READ_RTE_ENTRIES(8);
			goto exit;
		}

		rte_val &= RXS_RIO_BC_L2_GX_ENTRYY_CSR_ROUTING_VALUE;

		rt->dev_table[destID].rte_val = (uint32_t)(rte_val);

		mask_idx = MC_MASK_IDX_FROM_ROUTE(rte_val);
		if ((RIO_DSF_BAD_MC_MASK != mask_idx)
				&& !(rt->mc_masks[mask_idx].in_use)) {
			rt->mc_masks[mask_idx].tt = tt_dev16;
			rt->mc_masks[mask_idx].in_use = true;
			rt->mc_masks[mask_idx].mc_destID = first_mc_destID
					+ destID;
		}

		if (((rte_val >= NUM_RXS_PORTS(dev_info))
				&& (rte_val < RIO_DSF_FIRST_MC_MASK))
				|| ((rte_val >= RIO_DSF_BAD_MC_MASK)
						&& (RIO_DSF_RT_NO_ROUTE
								!= rte_val)
						&& (RIO_DSF_RT_USE_DEFAULT_ROUTE
								!= rte_val))) {
			rt->dev_table[destID].rte_val = RIO_DSF_RT_NO_ROUTE;
		}
	}

exit:
	return rc;
}

static uint32_t rxs_program_rte_entries(DAR_DEV_INFO_t *dev_info,
		rio_rt_set_all_in_t *in_parms,
		bool set_all, // true if all entries should be set
		uint32_t *imp_rc)
{
	uint32_t rc = RIO_SUCCESS;
	// Note that the base address for RXS2448 and RXS1632
	// are all the same.
	uint16_t rte_num;
	uint32_t dev_rte_base, dom_rte_base;

	rc = DARRegWrite(dev_info, RXS_RIO_ROUTE_DFLT_PORT,
			in_parms->rt->default_route);
	if (RIO_SUCCESS != rc) {
		*imp_rc = RXS_PROGRAM_RTE_ENTRIES(0x10);
		goto exit;
	}

	if (RIO_ALL_PORTS == in_parms->set_on_port) {
		dev_rte_base = RXS_RIO_BC_L2_GX_ENTRYY_CSR(0, 0);
		dom_rte_base = RXS_RIO_BC_L1_GX_ENTRYY_CSR(0, 0);
	} else {
		dev_rte_base = RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(
				in_parms->set_on_port, 0, 0);
		dom_rte_base = RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(
				in_parms->set_on_port, 0, 0);
	}

	for (rte_num = 0; rte_num < RIO_DAR_RT_DOM_TABLE_SIZE; rte_num++) {
		if (in_parms->rt->dom_table[rte_num].changed || set_all) {
			rc = DARRegWrite(dev_info,
				DOM_RTE_ADDR( dom_rte_base, rte_num),
				in_parms->rt->dom_table[rte_num].rte_val);
			if (RIO_SUCCESS != rc) {
				*imp_rc = RXS_PROGRAM_RTE_ENTRIES(2);
				goto exit;
			}
			in_parms->rt->dom_table[rte_num].changed = false;
		}
	}

	for (rte_num = 0; rte_num < RIO_DAR_RT_DEV_TABLE_SIZE; rte_num++) {
		if (in_parms->rt->dev_table[rte_num].changed || set_all) {
			// Validate value to be programmed.
			if (RIO_RTV_IS_LVL_GRP(
				in_parms->rt->dev_table[rte_num].rte_val)) {
				rc = RIO_ERR_INVALID_PARAMETER;
				*imp_rc = RXS_PROGRAM_RTE_ENTRIES(3);
				goto exit;
			}

			rc = DARRegWrite(dev_info,
				DEV_RTE_ADDR( dev_rte_base, rte_num),
				in_parms->rt->dev_table[rte_num].rte_val);
			if (RIO_SUCCESS != rc) {
				*imp_rc = RXS_PROGRAM_RTE_ENTRIES(4);
				goto exit;
			}
			in_parms->rt->dev_table[rte_num].changed = false;
		}
	}

exit:
	return rc;
}

static uint32_t rxs_rt_set_common(DAR_DEV_INFO_t *dev_info,
		rio_rt_set_all_in_t *in_parms, rio_rt_set_all_out_t *out_parms,
		bool set_all) // true if all entries should be set
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;

	out_parms->imp_rc = RIO_SUCCESS;

	if ((RIO_ALL_PORTS != in_parms->set_on_port)
			&& (in_parms->set_on_port >= NUM_RXS_PORTS(dev_info))) {
		out_parms->imp_rc = RXS_RTE_SET_COMMON(1);
		goto exit;
	}

	if (NULL == in_parms->rt) {
		out_parms->imp_rc = RXS_RTE_SET_COMMON(2);
		goto exit;
	}

	if ((NUM_RXS_PORTS(dev_info) <= in_parms->rt->default_route)
			&& !(RIO_DSF_RT_NO_ROUTE == in_parms->rt->default_route)) {
		out_parms->imp_rc = RXS_RTE_SET_COMMON(3);
		goto exit;
	}

	out_parms->imp_rc = RIO_SUCCESS;
	rc = rxs_program_mc_masks(dev_info, in_parms, set_all,
			&out_parms->imp_rc);
	if (RIO_SUCCESS != rc) {
		goto exit;
	}

	rc = rxs_program_rte_entries(dev_info, in_parms, set_all,
			&out_parms->imp_rc);
exit:
	return rc;
}

// Make sure that we're not orphaning a multicast mask...
static uint32_t rxs_tidy_routing_table(DAR_DEV_INFO_t *dev_info, uint8_t idx,
		rio_rt_state_t *rt, uint32_t *fail_pt,
		bool is_dev_table)
{
	uint32_t rc = RIO_SUCCESS;
	uint16_t srch_idx, dev_idx, dom_idx;
	bool found_one = false;

	if (is_dev_table) {
		dev_idx = idx;
		if ((rt->dev_table[dev_idx].rte_val >= RIO_DSF_FIRST_MC_MASK)
				&& (rt->dev_table[dev_idx].rte_val
						< RIO_DSF_BAD_MC_MASK)) {
			for (srch_idx = 0;
					(srch_idx < RIO_DAR_RT_DEV_TABLE_SIZE)
							&& !found_one;
					srch_idx++) {
				if (dev_idx == srch_idx) {
					continue;
				}
				if (rt->dev_table[dev_idx].rte_val
						== rt->dev_table[srch_idx].rte_val)
					found_one = true;
			}

			if (!found_one) {
				rio_rt_dealloc_mc_mask_in_t in_parms;
				rio_rt_dealloc_mc_mask_out_t out_parms;
				in_parms.rt = rt;
				in_parms.mc_mask_rte =
						rt->dev_table[dev_idx].rte_val;
				rc = DSF_rio_rt_dealloc_mc_mask(dev_info,
						&in_parms, &out_parms);
				if (RIO_SUCCESS != rc) {
					*fail_pt = out_parms.imp_rc;
				}
			}
		}
	} else {
		dom_idx = idx;
		if ((rt->dom_table[dom_idx].rte_val >= RIO_DSF_FIRST_MC_MASK)
				&& (rt->dom_table[dom_idx].rte_val
						< RIO_DSF_BAD_MC_MASK)) {
			for (srch_idx = 0;
					(srch_idx < RIO_DAR_RT_DEV_TABLE_SIZE)
							&& !found_one;
					srch_idx++) {
				if (dom_idx == srch_idx) {
					continue;
				}
				if (rt->dom_table[dom_idx].rte_val
						== rt->dom_table[srch_idx].rte_val)
					found_one = true;
			}

			if (!found_one) {
				rio_rt_dealloc_mc_mask_in_t in_parms;
				rio_rt_dealloc_mc_mask_out_t out_parms;
				in_parms.rt = rt;
				in_parms.mc_mask_rte =
						rt->dom_table[dom_idx].rte_val;
				rc = DSF_rio_rt_dealloc_mc_mask(dev_info,
						&in_parms, &out_parms);
				if (RIO_SUCCESS != rc) {
					*fail_pt = out_parms.imp_rc;
				}
			}
		}
	}
	return rc;
}

uint32_t rxs_rio_rt_initialize(DAR_DEV_INFO_t *dev_info,
		rio_rt_initialize_in_t *in_parms,
		rio_rt_initialize_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t destID;
	uint32_t mc_idx;
	rio_rt_set_changed_in_t all_in;
	rio_rt_set_changed_out_t all_out;
	rio_rt_state_t rt_state;
	// Validate parameters

	if ((in_parms->default_route >= NUM_RXS_PORTS(dev_info))
			&& !(RIO_DSF_RT_NO_ROUTE == in_parms->default_route)) {
		out_parms->imp_rc = RT_INITIALIZE(1);
		goto exit;
	}

	if ((in_parms->default_route_table_port >= NUM_RXS_PORTS(dev_info))
			&& !((RIO_DSF_RT_USE_DEFAULT_ROUTE
					== in_parms->default_route_table_port)
					|| (RIO_DSF_RT_NO_ROUTE
							== in_parms->default_route_table_port))) {
		out_parms->imp_rc = RT_INITIALIZE(2);
		goto exit;
	}

	if ((in_parms->set_on_port >= NUM_RXS_PORTS(dev_info))
			&& !(RIO_ALL_PORTS == in_parms->set_on_port)) {
		out_parms->imp_rc = RT_INITIALIZE(3);
		goto exit;
	}

	out_parms->imp_rc = RIO_SUCCESS;
	all_in.set_on_port = in_parms->set_on_port;

	if (!in_parms->rt) {
		all_in.rt = &rt_state;
	} else {
		all_in.rt = in_parms->rt;
	}

	all_in.rt->default_route = in_parms->default_route;

	// Configure initialization of all of the routing table entries
	for (destID = 0; destID < RIO_DAR_RT_DEV_TABLE_SIZE; destID++) {
		all_in.rt->dev_table[destID].changed = true;
		all_in.rt->dev_table[destID].rte_val =
				in_parms->default_route_table_port;
	}

	all_in.rt->dom_table[0].changed = true;
	all_in.rt->dom_table[0].rte_val = RIO_DSF_RT_USE_DEVICE_TABLE;

	for (destID = 1; destID < RIO_DAR_RT_DOM_TABLE_SIZE; destID++) {
		all_in.rt->dom_table[destID].changed = true;
		all_in.rt->dom_table[destID].rte_val =
				in_parms->default_route_table_port;
	}

	// Configure initialization of multicast masks and associations as necessary.
	for (mc_idx = 0; mc_idx < RIO_DSF_MAX_MC_MASK; mc_idx++) {
		all_in.rt->mc_masks[mc_idx].mc_destID = 0;
		all_in.rt->mc_masks[mc_idx].tt = tt_dev8;
		all_in.rt->mc_masks[mc_idx].mc_mask = 0;
		all_in.rt->mc_masks[mc_idx].in_use = false;
		all_in.rt->mc_masks[mc_idx].allocd = false;
		if ((mc_idx < RXS_MAX_MC_MASKS)
				&& (mc_idx < RIO_DSF_MAX_MC_MASK)) {
			all_in.rt->mc_masks[mc_idx].changed = true;
		} else {
			all_in.rt->mc_masks[mc_idx].changed = false;
		}
	}

	if (in_parms->update_hw) {
		rc = rxs_rio_rt_set_changed(dev_info, &all_in, &all_out);
	} else {
		rc = RIO_SUCCESS;
	}

	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = all_out.imp_rc;
	}

exit:
	return rc;
}

uint32_t rxs_rio_rt_probe(DAR_DEV_INFO_t *dev_info, rio_rt_probe_in_t *in_parms,
		rio_rt_probe_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint8_t bit;
	uint32_t regVal;

	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->valid_route = false;
	out_parms->routing_table_value = RIO_ALL_PORTS;
	out_parms->filter_function_active = false; /* not supported on RXS */
	out_parms->trace_function_active = false; /* not supported on RXS */

	for (bit = 0; bit < NUM_RXS_PORTS(dev_info); bit++)
		out_parms->mcast_ports[bit] = false;
	out_parms->reason_for_discard = rio_rt_disc_probe_abort;

	if (((NUM_RXS_PORTS(dev_info) <= in_parms->probe_on_port)
			&& (RIO_ALL_PORTS != in_parms->probe_on_port))
			|| (!in_parms->rt)) {
		out_parms->imp_rc = RT_PROBE(0x11);
		goto exit;
	}

	rc = DARRegRead(dev_info, RXS_RIO_PKT_TIME_LIVE, &regVal);
	if ( RIO_SUCCESS != rc) {
		out_parms->imp_rc = RT_PROBE(0x12);
		goto exit;
	}
	out_parms->time_to_live_active =
			(regVal & RXS_RIO_PKT_TIME_LIVE_PKT_TIME_LIVE) ?
					true : false;

	rc = RIO_SUCCESS;

	// Note, no failure possible...
	rxs_check_multicast_routing(dev_info, in_parms, out_parms);

	/* Done if hit in multicast masks. */
	if (RIO_ALL_PORTS != out_parms->routing_table_value) {
		goto exit;
	}

	/*  Determine routing table value for the specified destination ID.
	 *  If out_parms->valid_route is true
	 *  the valid values for out_parms->routing_table_value are
	 *  - a valid port number, OR
	 *  - RIO_DSF_RT_USE_DEFAULT_ROUTE
	 *  When out_parms->routing_table_value is RIO_DSF_RT_USE_DEFAULT_ROUTE, the
	 *  default route is a valid switch port number.
	 */

	rxs_check_unicast_routing(dev_info, in_parms, out_parms);

	if (out_parms->valid_route) {
		rc = rxs_check_port_for_discard(dev_info, in_parms, out_parms);
	}

exit:
	return rc;
}

/* This function returns the complete hardware state of packet routing
 * in a routing table state structure.
 *
 * The routing table hardware must be initialized using rio_rt_initialize()
 * before calling this routine.
 */
uint32_t rxs_rio_rt_probe_all(DAR_DEV_INFO_t *dev_info,
		rio_rt_probe_all_in_t *in_parms,
		rio_rt_probe_all_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint8_t probe_port;

	out_parms->imp_rc = RIO_SUCCESS;
	if ((((uint8_t)(RIO_ALL_PORTS) != in_parms->probe_on_port)
			&& (in_parms->probe_on_port >= NUM_RXS_PORTS(dev_info)))
			|| (!in_parms->rt)) {
		out_parms->imp_rc = RT_PROBE_ALL(1);
		goto exit;
	}

	probe_port = (RIO_ALL_PORTS == in_parms->probe_on_port) ?
			0 : in_parms->probe_on_port;

	rc = rxs_read_mc_masks(dev_info, probe_port, in_parms->rt,
			&out_parms->imp_rc);
	if (RIO_SUCCESS != rc) {
		goto exit;
	}

	rc = rxs_read_rte_entries(dev_info, probe_port, in_parms->rt,
			&out_parms->imp_rc);

exit:
	return rc;
}

/* This function sets the routing table hardware to match every entry
 * in the routing table state structure.
 * After rio_rt_set_all is called, no entries are marked as changed in
 * the routing table state structure.
 */
uint32_t rxs_rio_rt_set_all(DAR_DEV_INFO_t *dev_info,
		rio_rt_set_all_in_t *in_parms, rio_rt_set_all_out_t *out_parms)
{
	return rxs_rt_set_common(dev_info, in_parms, out_parms, RXS_SET_ALL);
}

/* This function sets the the routing table hardware to match every entry
 * that has been changed in the routing table state structure.
 * Changes must be made using rio_rt_alloc_mc_mask, rio_rt_deallocate_mc_mask,
 * rio_rt_change_rte, and rio_rt_change_mc.
 * After rio_rt_set_changed is called, no entries are marked as changed in
 * the routing table state structure.
 */
uint32_t rxs_rio_rt_set_changed(DAR_DEV_INFO_t *dev_info,
		rio_rt_set_changed_in_t *in_parms,
		rio_rt_set_changed_out_t *out_parms)
{
	return rxs_rt_set_common(dev_info, in_parms, out_parms, RXS_SET_CHANGED);
}

/* This function updates an rio_rt_state_t structure to
 * change a routing table entry, and tracks changes.
 */
uint32_t rxs_rio_rt_change_rte(DAR_DEV_INFO_t *dev_info,
		rio_rt_change_rte_in_t *in_parms,
		rio_rt_change_rte_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;

	out_parms->imp_rc = RIO_SUCCESS;

	if (!in_parms->rt) {
		out_parms->imp_rc = RT_CHANGE_RTE(1);
		goto exit;
	}

	// Validate rte_value
	if ((RIO_DSF_RT_USE_DEVICE_TABLE != in_parms->rte_value)
			&& (RIO_DSF_RT_USE_DEFAULT_ROUTE != in_parms->rte_value)
			&& (RIO_DSF_RT_NO_ROUTE != in_parms->rte_value)
			&& (in_parms->rte_value >= NUM_RXS_PORTS(dev_info))) {
		out_parms->imp_rc = RT_CHANGE_RTE(2);
		goto exit;
	}

	if ((RIO_DSF_RT_USE_DEVICE_TABLE == in_parms->rte_value)
			&& (!in_parms->dom_entry)) {
		out_parms->imp_rc = RT_CHANGE_RTE(3);
		goto exit;
	}

	rc = RIO_SUCCESS;

	// Do not allow any changes to index 0 of the domain table.
	// This must be set to "RXS_DSF_RT_USE_PACKET_ROUTE" at all times,
	// as this is the behavior required by the RXS RIO Domain register.

	if (in_parms->dom_entry && !in_parms->idx) {
		goto exit;
	}

	// If the entry has not already been changed, see if it is being changed
	if (in_parms->dom_entry) {
		if (!in_parms->rt->dom_table[in_parms->idx].changed) {
			if (in_parms->rt->dom_table[in_parms->idx].rte_val
					!= in_parms->rte_value)
				in_parms->rt->dom_table[in_parms->idx].changed =
						true;
		}
		in_parms->rt->dom_table[in_parms->idx].rte_val =
				in_parms->rte_value;
	} else {
		if (!in_parms->rt->dev_table[in_parms->idx].changed) {
			if (in_parms->rt->dev_table[in_parms->idx].rte_val
					!= in_parms->rte_value)
				in_parms->rt->dev_table[in_parms->idx].changed =
						true;
		}
		in_parms->rt->dev_table[in_parms->idx].rte_val =
				in_parms->rte_value;
	}

exit:
	return rc;
}



/* This function updates an rio_rt_state_t structure to
 * change a multicast mask value, and tracks changes.
 */
uint32_t rxs_rio_rt_change_mc_mask(DAR_DEV_INFO_t *dev_info,
		rio_rt_change_mc_mask_in_t *in_parms,
		rio_rt_change_mc_mask_out_t *out_parms)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint8_t chg_idx, dom_idx, dev_idx;
	uint32_t illegal_ports = ~((1 << RXS2448_MAX_PORTS) - 1);
	uint32_t avail_ports = (1 << NUM_RXS_PORTS(dev_info)) - 1;

	out_parms->imp_rc = RIO_SUCCESS;

	if (!in_parms->rt) {
		out_parms->imp_rc = CHANGE_MC_MASK(1);
		goto exit;
	}

	// Check destination ID value against tt, and that the multicast mask
	// does not select ports which do not exist on the RXS device.
	if ((in_parms->mc_info.mc_destID > RIO_LAST_DEV16_DESTID)
			|| ((in_parms->mc_info.mc_destID > RIO_LAST_DEV8_DESTID)
					&& (tt_dev8 == in_parms->mc_info.tt))
			|| (in_parms->mc_info.mc_mask & illegal_ports)) {
		out_parms->imp_rc = CHANGE_MC_MASK(2);
		goto exit;
	}

	if (!in_parms->mc_info.in_use) {
		rio_rt_dealloc_mc_mask_in_t d_in_parm;
		rio_rt_dealloc_mc_mask_out_t d_out_parm;

		d_in_parm.mc_mask_rte = in_parms->mc_mask_rte;
		d_in_parm.rt = in_parms->rt;

		rc = DSF_rio_rt_dealloc_mc_mask(dev_info, &d_in_parm,
				&d_out_parm);
		if (RIO_SUCCESS != rc) {
			out_parms->imp_rc = d_out_parm.imp_rc;
		}
		goto exit;
	}

	// Allow requests to change masks not supported by RXS family
	// but there's nothing to do...

	chg_idx = MC_MASK_IDX_FROM_ROUTE(in_parms->mc_mask_rte);

	if (chg_idx >= NUM_MC_MASKS(dev_info)) {
		rc = RIO_ERR_INVALID_PARAMETER;
		out_parms->imp_rc = CHANGE_MC_MASK(3);
		goto exit;
	}

	// If entry has not already been changed, see if it is being changed
	if (!in_parms->rt->mc_masks[chg_idx].changed) {
		if ((in_parms->rt->mc_masks[chg_idx].mc_mask
				!= in_parms->mc_info.mc_mask)
				|| (in_parms->rt->mc_masks[chg_idx].in_use
						!= in_parms->mc_info.in_use)) {
			in_parms->rt->mc_masks[chg_idx].changed = true;
		}
	}

	// Note: The multicast mask must be in use now.  We must make sure that
	// the routing tables are set appropriately.
	dom_idx = (in_parms->mc_info.mc_destID & 0xFF00) >> 8;
	if ((tt_dev16 == in_parms->mc_info.tt) && (dom_idx)
			&& (in_parms->mc_mask_rte
					!= in_parms->rt->dom_table[dom_idx].rte_val)) {
		rc = rxs_tidy_routing_table(dev_info, dom_idx, in_parms->rt,
				&out_parms->imp_rc, false);
		if (RIO_SUCCESS != rc) {
			goto exit;
		}

		in_parms->rt->dom_table[dom_idx].rte_val =
				in_parms->mc_mask_rte;
		in_parms->rt->dom_table[dom_idx].changed = true;
	}

	dev_idx = (in_parms->mc_info.mc_destID & 0x00FF);
	if (in_parms->mc_mask_rte != in_parms->rt->dev_table[dev_idx].rte_val) {
		rc = rxs_tidy_routing_table(dev_info, dev_idx, in_parms->rt,
				&out_parms->imp_rc, true);
		if (RIO_SUCCESS != rc) {
			goto exit;
		}

		in_parms->rt->dev_table[dev_idx].rte_val =
				in_parms->mc_mask_rte;
		in_parms->rt->dev_table[dev_idx].changed = true;
	}

	in_parms->rt->mc_masks[chg_idx].in_use = true;
	in_parms->rt->mc_masks[chg_idx].mc_destID = in_parms->mc_info.mc_destID;
	in_parms->rt->mc_masks[chg_idx].tt = in_parms->mc_info.tt;
	in_parms->rt->mc_masks[chg_idx].mc_mask = (in_parms->mc_info.mc_mask
			& avail_ports);

	rc = RIO_SUCCESS;

exit:
	return rc;
}

#endif /* RXSx_DAR_WANTED */

#ifdef __cplusplus
}
#endif
