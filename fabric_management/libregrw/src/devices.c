/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, Prodrive Technologies
Copyright (c) 2014, RapidIO Trade Association

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

THIS SOFTWARE IS PROVENDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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
/* Initialization of regrw handle based on known devices and regsiters. 
 */

#include <stdint.h>
#include "rio_standard.h"
#include "rio_ecosystem.h"
#include "regrw_private.h"
#include "CPS1616.h"
#include "CPS1848.h"
#include "Tsi578.h"
#include "Tsi721.h"
#include "Tsi575_SRSF.h"

#ifdef __cplusplus
extern "C" {
#endif

struct regrw_vendor {
        uint16_t vid;
        const char *vendor;
};

struct const_regs Tsi57x_const = {
        (RIO_PE_FEAT_T)Tsi578_PE_FEAT_VAL,
        Tsi578_SRC_OP_VAL,
	0,
        Tsi578_MC_FEAT_VAL,
        Tsi578_RIO_LUT_SIZE_VAL,
        Tsi578_SW_MC_INFO_VAL,
        Tsi578_RIO_SW_MB_HEAD,
        0, 
        RIO_EFB_T_SP_NOEP_SAER,
        Tsi578_RIO_ERR_RPT_BH,
        0,
        0,
        0,
        0
};

struct const_regs CPS1848_const = {
        (RIO_PE_FEAT_T)CPS1848_PE_FEAT_VAL,
        CPS1848_SRC_OP_VAL,
	0,
        CPS1848_MC_FEAT_VAL,
        CPS1848_LUT_SIZE_VAL,
        CPS1848_SW_MC_INFO_VAL,
        CPS1848_PORT_MAINT_BLK_HEAD,
        0, 
        RIO_EFB_T_SP_NOEP_SAER,
        CPS1848_ERR_MGT_EXTENSION_BLK_HEAD,
        0,
        0,
        CPS1848_LANE_STATUS_BLK_HEAD,
	0x0000000B,
        0
};

struct const_regs CPS1432_const = {
        (RIO_PE_FEAT_T)CPS1848_PE_FEAT_VAL,
        CPS1848_SRC_OP_VAL,
	0,
        CPS1848_MC_FEAT_VAL,
        CPS1848_LUT_SIZE_VAL,
        CPS1848_SW_MC_INFO_VAL,
        CPS1848_PORT_MAINT_BLK_HEAD,
        0, 
        RIO_EFB_T_SP_NOEP_SAER,
        CPS1848_ERR_MGT_EXTENSION_BLK_HEAD,
        0,
        0,
        CPS1848_LANE_STATUS_BLK_HEAD,
	0x0000000B,
        0
};

struct const_regs Tsi721_const = {
        (RIO_PE_FEAT_T)TSI721_PE_FEAT_VAL,
        TSI721_SRC_OP_VAL,
	721_DST_OP_VAL0,
        0,
        0,
        0,
        TSI721_RIO_SP_MB_HEAD,
        0, 
        RIO_EFB_T_SP_NOEP_SAER,
        TSI721_RIO_ERR_RPT_BH,
        0,
        0,
        TSI721_RIO_PER_LANE_BH,
	0x00000008,
        0
};

struct regrw_dev_id {
        uint16_t vid;
        uint16_t did;
        const char *name;
	const struct const_regs *regs;
};

static const struct regrw_vendor regrw_vendors[] = {
        {0x0000,                "Reserved"},
        {0x0001,                "Mercury Computer Systems"},
        {RIO_VEND_FREESCALE,     "Freescale"},
        {0x0003,                "Alcatel Corporation"},
        {0x0005,                "EMC Corporation"},
        {0x0006,                "Ericsson"},
        {0x0007,                "Alcatel-Lucent Technologies"},
        {0x0008,                "Nortel Networks"},
        {0x0009,                "Altera"},
        {0x000a,                "LSA Corporation"},
        {0x000b,                "Rydal Research"},
        {RIO_VEND_TUNDRA,        "Tundra Semiconductor"},
        {0x000e,                "Xilinx"},
        {0x0019,                "Curtiss-Wright Controls Embedded Computing"},
        {0x001f,                "Raytheon Company"},
        {0x0028,                "VMetro"},
        {RIO_VEND_TI,            "Texas Instruments"},
        {0x0035,                "Cypress Semiconductor"},
        {0x0037,                "Cadence Design Systems"},
        {RIO_VEND_IDT,           "Integrated Device Technology"},
        {0x003d,                "Thales Computer"},
        {0x003f,                "Praesum Communications"},
        {0x0040,                "Lattice Semiconductor"},
        {0x0041,                "Honeywell Inc."},
        {0x005a,                "Jennic, Inc."},
        {0x0064,                "AMCC"},
        {0x0066,                "GDA Technologies"},
        {0x006a,                "Fabric Embedded Tools Corporation"},
        {0x006c,                "Silicon Turnkey Express"},
        {0x006e,                "Micro Memory"},
        {0x0072,                "PA Semi, Inc."},
        {0x0074,                "SRISA - Scientific Research Inst for System Analysis"},
        {0x0076,                "Nokia Siemens Networks"},
        {0x0079,                "Nokia Siemens Networks"},
        {0x007c,                "Hisilicon Technologies Co."},
        {0x007e,                "Creatuve Electronix Systems"},
        {0x0080,                "ELVEES"},
        {0x0082,                "GE Fanuc Embedded Systems"},
        {0x0084,                "Wintegra"},
        {0x0088,                "HDL Design House"},
        {0x008a,                "Motorola"},
        {0x008c,                "Cavium Networks"},
        {0x008e,                "Mindspeed Technologies"},
        {0x0094,                "Eclipse Electronic Systems, Inc."},
        {0x009a,                "Sandia National Laboratories"},
        {0x009e,                "HCL Technologies, Ltd."},
        {0x00a2,                "ASML"},
        {RIO_VEND_PRODRIVE,      "Prodrive Technologies"},
        {0x00a6,                "BAE Systems"},
        {0x00a8,                "Broadcom"},
        {0x00aa,                "Mobiveil, Inc."},
        {0xffff,                "Reserved"},
};

static const struct regrw_dev_id regrw_device_ids[] = {
        {RIO_VEND_TUNDRA, RIO_DEVI_TSI572,        "Tsi572", &Tsi57x_const},
        {RIO_VEND_TUNDRA, RIO_DEVI_TSI574,        "Tsi574", &Tsi57x_const},
        {RIO_VEND_TUNDRA, RIO_DEVI_TSI577,        "Tsi577", &Tsi57x_const},
        {RIO_VEND_TUNDRA, RIO_DEVI_TSI576,        "Tsi576", &Tsi57x_const},
        {RIO_VEND_TUNDRA, RIO_DEVI_TSI578,        "Tsi578", &Tsi57x_const},

        /* IDT */
        {RIO_VEND_IDT, RIO_DEVI_IDT_CPS1432,      "CPS1432", &CPS1432_const},
        {RIO_VEND_IDT, RIO_DEVI_IDT_CPS1848,      "CPS1848", &CPS1848_const},
        {RIO_VEND_IDT, RIO_DEVI_IDT_CPS1616,      "CPS1616", &CPS1616_const},
        {RIO_VEND_IDT, RIO_DEVI_IDT_SPS1616,      "SPS1616", &CPS1616_const},
        {RIO_VEND_IDT, RIO_DEVI_IDT_TSI721,       "Tsi721", &Tsi721_const},

        {RIO_VEND_IDT, RIO_DEVI_IDT_RXS2448,      "RXS2448", NULL},
        {RIO_VEND_IDT, RIO_DEVI_IDT_RXS1632,      "RXS1632", NULL},


        /* End of list */
        {RIO_VEND_RESERVED, RIO_DEVI_RESERVED, "Unknown", NULL},
};

struct const_regs const_regs_offsets {
        RIO_PROC_ELEM_FEAT;
        RIO_SW_PORT_INF;
        RIO_SRC_OPS;
        RIO_DST_OPS;
        RIO_SW_MC_SUP;
        RIO_SW_RT_TBL_LIM;
        RIO_SW_MC_INF;
        RIO_BAD_OFFSET, 
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET,
        RIO_BAD_OFFSET
};

int read_rt_regs(regrw_h hnd)
{
	rio_port_t port;
	regrw_i *h = (regrw_i *)hnd;
	uint32_t rtp = h->evb_rt;

	errno = 0;
	if (!efb_p) {
		errno = EINVAL;
		goto failed;
	};

	h->rt = (struct rt_regs_blk *)calloc(1, sizeof(struct rt_regs_blk));
	if (NULL == h->rt) {
		errno = ENOMEM;
		goto failed;
	};
	if (regrw_raw_rd(hnd, RIO_RT_BC_CTL(rtp), &h->rt->bc.ctl))
		goto failed;
	if (regrw_raw_rd(hnd, RIO_RT_BC_MC(rtp), &h->rt->bc.mc_oset))
		goto failed;
	if (regrw_raw_rd(hnd, RIO_RT_BC_LVL0(rtp), &h->rt->bc.lvl0_i))
		goto failed;
	if (regrw_raw_rd(hnd, RIO_RT_BC_LVL1(rtp), &h->rt->bc.lvl1_i))
		goto failed;
	if (regrw_raw_rd(hnd, RIO_RT_BC_LVL2(rtp), &h->rt->bc.lvl2_i))
		goto failed;
	for (i = 0; i < PE_PORT_COUNT(h); i++) {
		struct rt_regs *pt = &h->rt->pt[i];
		if (regrw_raw_rd(hnd, RIO_RT_CTL(rtp), &pt->ctl))
			oto failed;
		if (regrw_raw_rd(hnd, RIO_RT_MC(rtp), &pt->mc_oset))
			oto failed;
		if (regrw_raw_rd(hnd, RIO_RT_LVL0(rtp), &pt->lvl0_i))
			oto failed;
		if (regrw_raw_rd(hnd, RIO_RT_LVL1(rtp), &pt->lvl1_i))
			oto failed;
		if (regrw_raw_rd(hnd, RIO_RT_LVL2(rtp), &pt->lvl2_i))
			goto failed;
	};

	return 0;
failed:
	return -1;
};

int read_efb_headers(regrw_h h)
{
        uint32_t efb;
        uint32_t efb_p = GET_EXT_FEAT_OSET(h);

	while (efb_p) {
		if (regrw_raw_rd(h, efb_p, &efb))
			goto failed;

            	switch(RIO_EFB_GET_EFB_T(efb)) {
                	case RIO_EFB_T_SP_EP:
                	case RIO_EFB_T_SP_EP_SAER:
                	case RIO_EFB_T_SP_NOEP:
                	case RIO_EFB_T_SP_NOEP_SAER:
	            		h->sp_oset = efb_p;
                    		h->sp_type = RIO_EFB_GET_EFB_T(efb);
                    		break;
                	case RIO_EFB_T_SP_EP3:
                	case RIO_EFB_T_SP_EP3_SAER:
                	case RIO_EFB_T_SP_NOEP3:
                	case RIO_EFB_T_SP_NOEP3_SAER:
	            		h->sp_oset3 = prev_addr;
                    		h->sp_type = RIO_EFB_GET_EFB_T(efb);
                    		break;
			case RIO_EFB_T_EMHS:
				h->emhs_oset = efb_p;
				if (regrw_raw_rd(h, RIO_EMHS_INFO(efb_p),
								&h->em_info))
					goto failed;
				break;
			case RIO_EFB_T_HS:
				h->hs_oset = efb_p;
				if (regrw_raw_rd(h, RIO_EMHS_INFO(efb_p),
								&h->em_info))
					goto failed;
				break;

			case RIO_EFB_T_LANE:
				h->lane_oset = efb_p;
				if (regrw_raw_rd(h, RIO_LNX_ST0(efb_p),
								&h->lane_regs))
					goto failed;
				h->lane_regs &= RIO_LNX_ST0_STAT1 |
					RIO_LNX_ST0_STAT2_7;
                    		break;

			case RIO_EFB_T_RT:
                    		h->rt_oset = efb_p;
				if (read_rt_regs(h))
					goto failed;
                    		break;
			default: /* Don't care about other ext feat blocks */
		}
		prev_addr = RIO_EFB_GET_NEXT(curr_ext_feat);
        }
	return 0;
failed:
	return -1;
};

void read_variable_regs(regrw_h h, bool read_const_regs)
{
	struct regrw_i *hnd = (struct regrw_i *)h;

	if (read_const_regs) {
    		if (regrw_raw_rd(h, RIO_PROC_ELEM_FEAT, &h->cregs.pe_feat))
        		goto failed;
    		if (regrw_raw_rd(h, RIO_SW_PORT_INF, &h->cregs.sw_port_inf))
        		goto failed;
    		if (regrw_raw_rd(h, RIO_SRC_OPS, &h->cregs.src_ops))
        		goto failed;
    		if (regrw_raw_rd(h, RIO_DST_OPS, &h->cregs.dst_ops))
        		goto failed;
		if (PE_IS_SW(h)) {
    			if (regrw_raw_rd(h, RIO_SW_MC_SUP, &h->cregs.sw_mc_sup))
        			goto failed;
    			if (regrw_raw_rd(h, RIO_SW_RT_TBL_LIM,
						&h->cregs.sw_rt_tbl_lim))
        			goto failed;
    			if (regrw_raw_rd(h, RIO_SW_MC_INF, &h->cregs.sw_mc_inf))
        			goto failed;
		};
	};

 	if (regrw_raw_rd(h, RIO_DEV_INF, &h->dev_info))
		goto failed;
 	if (regrw_raw_rd(h, RIO_ASSY_ID, &h->assy_id))
		goto failed;
 	if (regrw_raw_rd(h, RIO_ASSY_INF, &h->assy_info))
		goto failed;
	/* Make sure every device has at least one port. */
 	if (regrw_raw_rd(h, RIO_SW_PORT_INF, &h->cregs.sw_port_info))
		goto failed;
	if (!h->sw_port_info)
		h->sw_port_info = 0x00000100;
 	if (regrw_raw_rd(h, RIO_PE_LL_CTL, &h->pe_ll_ctl))
		goto failed;
 	if (regrw_raw_rd(h, RIO_LCS_ADDR0, &h->lcs_addr0))
		goto failed;
 	if (regrw_raw_rd(h, RIO_LCS_ADDR1, &h->lcs_addr1))
		goto failed;
 	if (regrw_raw_rd(h, RIO_DEVID, &h->did))
		goto failed;
 	if (regrw_raw_rd(h, RIO_HOST_LOCK, &h->host_lock))
		goto failed;
 	if (regrw_raw_rd(h, RIO_COMPTAG, &h->comptag))
		goto failed;
	if (PE_IS_SW(h)) {
 		if (regrw_raw_rd(h, RIO_DFLT_RTE, &h->dflt_rte))
			goto failed;
	};

	if (!(h->pe_feat & RIO_PE_FEAT_EFB_VALID) || !read_const_regs)
		return 0;

	return read_efb_headers(h);
failed:
	return -1;
};

#define SPX_CTL1(h,p) RIO_SPX_CTL(h->sp_oset, h->sp_type, p)

int regrw_fill_in_handle(regrw_i *h, RIO_DEV_IDENT_T vend_devi)
{
	uint32_t vendor = vend_devi & RIO_DEV_IDENT_VEND;
	uint32_t devi = (vend_devi & IO_DEV_IDENT_DEVID) >> 16;
	bool found = false;
	int i;

	errno = 0;

	for (i = 0; RIO_VEND_RESERVED != regrw_vendors[i]; i++) {
		if (regrw_vendors[i].vid == vendor) {
			found = true;
			h->vend_name = regrw_vendors[i].vendor;
			break;
		};
	};

	if (!found)
		goto fail;
	found = false;

	for (i = 0; RIO_VEND_RESERVED != regrw_device_ids[i]; i++) {
		if ((regrw_device_ids[i].vid == vendor) &&
				(regrw_device_ids[i].did == devi)) {
			found = true;
			break;
		};
	};

	if (!found)
		goto fail;

	h->dev_t_name = regrw_device_ids[i].name;
	if (NULL != regrw_device_ids[i].regs) {
		memcpy(&h->cregs, &regrw_device_ids[i].regs, sizeof(h->cregs));
	};

	if (read_variable_regs((regrw_h)h, NULL == regrw_device_ids[i].regs))
		goto failed;

	if (PE_IS_SW(h)) {
		h->rt = (struct rt_regs_blk *)
					calloc(sizeof(struct rt_regs_blk),1);
		if (NULL == h->rt) {
			errno = ENOMEM;
			goto failed;
		};
	};

	switch (GET_DEV_VENDOR(h)) {
	case RIO_VEND_TUNDRA: tundra_fill_in_handle(h);
		break;
	case RIO_VEND_IDT: idt_fill_in_handle(h);
		break;
	default: goto fail;
		break;
	};

	if  (h->sp_oset) {
		rio_pt_t pt;
		for (pt = 0; pt < PE_PORT_COUNT(h); pt++) {
 			if (regrw_raw_rd(h, SPX_CTL1(h,pt), &h->ctl1[pt])) {
				goto failed;
			};
		};
	};
	return 0;
fail:
	errno = ENXIO;
failed:
	return -1;
};

int regrw_update_h_info_on_write(regrw_i *h, uint32_t offset, uint32_t data)
{
	int rc = 0;

	switch (GET_DEV_VENDOR(h)) {
	case RIO_VEND_TUNDRA: tundra_update_h_info_on_write(h, offseet, data);
			break;
	case RIO_VEND_IDT: idt_update_h_info_on_write(h, offseet, data);
			break;
	default: rc = -1;
		errno = -RIO_VEND_IDT;
		break;
	};
	return rc;
};

int regrw_get_info_from_h_on_read(regrw_i *h, uint32_t offset, uint32_t *data)
{
	int rc = 0;

	switch (GET_DEV_VENDOR(h)) {
	case RIO_VEND_TUNDRA: tundra_update_h_info_on_write(h, offseet, data);
			break;
	case RIO_VEND_IDT: idt_update_h_info_on_write(h, offseet, data);
			break;
	default: rc = -1;
		errno = -RIO_VEND_IDT;
		break;
	};
	return rc;
};

/** \brief Device specific corrections to data read from device
 *  */
int regrw_read_fixups(regrw_i *h, uint32_t offset, uint32_t *data)
{
	int rc = 0;

	switch (GET_DEV_VENDOR(h)) {
	case RIO_VEND_TUNDRA: tundra_update_h_info_on_write(h, offseet, data);
			break;
	case RIO_VEND_IDT: idt_update_h_info_on_write(h, offseet, data);
			break;
	default: rc = -1;
		errno = -RIO_VEND_IDT;
		break;
	};
	return rc;
};

/** \brief Device specific corrections to data to be written to device
 *  */
int regrw_write_fixups(regrw_i *h, uint32_t offset, uint32_t *data);
{
	int rc = 0;

	switch (GET_DEV_VENDOR(h)) {
	case RIO_VEND_TUNDRA: tundra_update_h_info_on_write(h, offseet, data);
			break;
	case RIO_VEND_IDT: idt_update_h_info_on_write(h, offseet, data);
			break;
	default: rc = -1;
		errno = -RIO_VEND_IDT;
		break;
	};
	return rc;
};

#ifdef __cplusplus
}
#endif


