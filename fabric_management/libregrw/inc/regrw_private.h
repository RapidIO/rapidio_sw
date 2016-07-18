/*
 * RapidIO Standard Capability Registers (CARs)
 * and Command and Status Registers
 */
/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
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

#ifndef __REGRW_PRIVATE_H__
#define __REGRW_PRIVATE_H__

#include <stdint.h>
#include "rio_standard.h"
#include "rio_ecosystem.h"
#include "regrw.h"

/* rio_regdefs.h contains definitons for all RapidIO Standard Registers
*/

#ifdef __cplusplus
extern "C" {
#endif

struct const_regs {
        RIO_PE_FEAT_T pe_feat;
        uint32_t sw_port_info;
        uint32_t src_ops;
        uint32_t dst_ops;
        uint32_t sw_mc_sup;
        uint32_t sw_rt_lim;
        uint32_t sw_mc_inf;
	uint32_t sp_oset; /* RO: RIO_EFB_T_SP_EP3, RIO_EFB_T_SP_EP3_SAER, */
			/* RIO_EFB_T_SP_NOEP3, RIO_EFB_T_SP_NOEP3_SAER */
	uint32_t sp3_oset; /* RIO_EFB_T_SP_EP3,RIO_EFB_T_SP_EP3_SAER, 
			* RIO_EFB_T_SP_NOEP3, RIO_EFB_T_SP_NOEP3_SAER */
	uint32_t sp_type;
	uint32_t emhs_oset; /* RIO_EFB_T_EMHS */

	uint32_t hs_oset; /* RIO_EFB_T_HS */
	uint32_t em_info; /* RIO_EMHS_INFO */
	uint32_t lane_oset; /* RO: RIO_EFB_T_LANE */
	uint32_t lane_regs; /* RIO_LNX_ST0_STAT1 + RIO_LNX_ST0_STAT2_7 */
	uint32_t rt_oset; /* RO: RIO_EFB_T_RT */
};

struct rt_regs {
	uint32_t ctl; /* RIO_RT_BC_CTL, RIO_RT_SPX_CTL */
	uint32_t mc_i; /* RIO_RT_BC_MC, RIO_RT_SPX_MC */
	uint32_t lvl0_i; /* RIO_RT_BC_LVL0, RIO_RT_SPX_LVL0 */
	uint32_t lvl1_i; /* RIO_RT_BC_LVL1, RIO_RT_SPX_LVL1 */
	uint32_t lvl2_i; /* RIO_RT_BC_LVL2, RIO_RT_SPX_LVL2 */
	rt_val_t mc[RIO_LVL_GRP_SZ];
	rt_val_t rt0[RIO_MAX_RT_L0 * RIO_LVL_GRP_SZ];
	rt_val_t rt1[RIO_MAX_RT_L1][RIO_LVL_GRP_SZ];
	rt_val_t rt2[RIO_MAX_RT_L2][RIO_LVL_GRP_SZ];
};

struct rt_regs_blk {
	struct rt_regs bc;
	struct rt_regs pt[RIO_MAX_DEV_PORT];
};

/* Offsets, field masks and value definitions for Device CARs and CSRs
*/

#define MAX_DAR_SCRPAD_IDX  30

struct regrw_i {
	/** \brief RapidIO CARs */
	uint32_t dev_ident; /* RIO_DEV_IDENT */
	uint32_t dev_info; /* RIO_DEV_INF */
	uint32_t assy_id; /* RIO_ASSY_ID */
	uint32_t assy_info; /* RIO_ASSY_INF */

	/** Other constant registers */
	struct const_regs cregs;

	/** \brief RapidIO CSRs */
	RIO_PE_ADDR_T pe_ll_ctl; /* RIO_PE_LL_CTL */
	uint32_t lcs_addr0; /* RIO_LCS_ADDR0 */
	uint32_t lcs_addr1; /* RIO_LCS_ADDR1 */
	uint32_t did; /* RIO_DEVID, contains dev8 & dev16 */
	uint32_t host_lock; /* RIO_HOST_LOCK */
	uint32_t comptag; /* RIO_COMPTAG */
	uint32_t dflt_rte; /* RIO_DFLT_RTE */

	/** \brief Global scratch register support for each device. */
	uint32_t global_scratch;

	/** \brief RapidIO block headers */
	struct rt_regs_blk *rt;

	/** \brief Vendor and device type name strings */
	const char *vend_name;
	const char *dev_t_name;

	/* Values of ctl1_reg */
	uint32_t ctl1[RIO_MAX_DEV_PORT];

	/** \brief Device access info */
	uint32_t dest_id; /* DEstination ID used to access the device */
	tt_t tt; /* dev8, dev16, or dev32 */
	rio_hc_t hc; /* Hopcount used to access the device with mtc transactions
			* hc = HC_LOCAL means local device.
			*/
	/** \brief Register access driver handle */
	struct regrw_driver regrw;

	/** Device specific information.
	* SW_INFO is specific to switches
	* scratchpad is infor that may (or may not) be needed for a device.
	**/
	
	uint32_t *scratchpad;
};


/** \brief Macros to extract/check common fields/values
 *   from a * struct rio_car_csr.
 */

#define GET_DEV_VENDOR(x) ((regrw_i *)(x)->dev_ident & RIO_DEV_IDENT_VEND)
#define GET_DEV_IDENT(x) (((regrw_i *)(x)->dev_ident & RIO_DEV_IDENT_DEVI) >> 16)
#define GET_ASSY_VENDOR(x) ((regrw_i *)(x)->assy_id & RIO_ASSY_ID_VEND)
#define GET_ASSY_IDENT(x) (((regrw_i *)(x)->assy_id & RIO_ASSY_ID_ASSY) >> 16) 
#define GET_ASSY_VERSION(x) (((regrw_i *)(x)->assy_info & RIO_ASSY_INF_ASSY_REV) >> 16)
#define GET_GET_DEV8(x) (((regrw_i *)(x)->devid & RIO_MC_CON_SEL_DEV8) >> 16)
#define GET_CHG_DEV8(x,y) (((regrw_i *)(x)->devid & ~RIO_MC_CON_SEL_DEV8) | \
			((y << 16) & RIO_MC_CON_SEL_DEV8))
#define GET_GET_DEV16(x) (((regrw_i *)(x)->devid & RIO_MC_CON_SEL_DEV16)
#define GET_CHG_DEV16(x,y) ((((regrw_i *)(x)->devid & ~RIO_MC_CON_SEL_DEV16)\
			| (y & RIO_MC_CON_SEL_DEV16))
#define GET_CT(x) (((regrw_i *)(x)->comptag)

#define GET_EXT_FEAT_OSET(x) ( \
	((regrw_i *)(x)->pe_feat & RIO_PE_FEAT_EFB_VALID)? \
	((regrw_i *)(x)->assy_info & RIO_ASSY_INF_EFB_PTR):0)

#define GET_ENUMB(x,p) (((p >= 0) && (p < RIO_PE_PORT_COUNT))? \
	((regrw_i *)(x)->ctl1[p] & RIO_SPX_CTL_ENUM_B):0xFFFFFFFF)

#define RIO_ADDRSZ_SUPP(x) (RIO_PE_ADDR_T)(x->pe_feat & RIO_PE_FEAT_EXT_ADDR)
#define RIO_ADDR34_SUPP(x) (bool)(!!(x->pe_feat & RIO_PE_FEAT_EXT_ADDR34))
#define RIO_ADDR50_SUPP(x) (bool)(!!(x->pe_feat & RIO_PE_FEAT_EXT_ADDR50))
#define RIO_ADDR66_SUPP(x) (bool)(!!(x->pe_feat & RIO_PE_FEAT_EXT_ADDR66))

#define RIO_ADDRSZ_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_34BIT))
#define RIO_ADDR34_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_34BIT))
#define RIO_ADDR50_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_50BIT))
#define RIO_ADDR66_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_66BIT))

/* Check for processing element features */
#define RIO_STD_RTE(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_STD_RTE)
#define RIO_EXT_RTE(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_EXTD_RTE)

#define PE_IS_MP(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_MULTIP)
#define PE_IS_SW(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_SW)
#define PE_IS_PROC(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_PROC)
#define PE_IS_MEM(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_MEM)
#define PE_IS_BRIDGE(x) ((regrw_i *)x->pe_feat & RIO_PE_FEAT_BRDG)
#define PE_PORT_COUNT(x) \
	(rio_port_t)((PE_IS_SW(x))? \
		RIO_AVAIL_PORTS((regrw_i *)(x)->sw_port_info):1)
#define RIO_SW_ACC_PORT(x) \
	(rio_port_t)((PE_IS_SW(x))? \
		RIO_ACCESS_PORT((regrw_i *)(x)->sw_port_info):0)
#define RIO_MC_MASK_COUNT(x) ((regrw_i *)x->sw_mc_inf & RIO_SW_MC_INF_MC_MSK)

#define REGRW_RT_BC_CTL(h)     ((regrw_i *)(h)->evb_rt + 0x020)
#define REGRW_RT_BC_MC(h)      ((regrw_i *)(h)->evb_rt + 0x028)
#define REGRW_RT_BC_LVL0(h)    ((regrw_i *)(h)->evb_rt + 0x030)
#define REGRW_RT_BC_LVL1(h)    ((regrw_i *)(h)->evb_rt + 0x034)
#define REGRW_RT_BC_LVL2(h)    ((regrw_i *)(h)->evb_rt + 0x038)
#define REGRW_RT_CTL(h,p)      ((regrw_i *)(h)->evb_rt + 0x040 + 0x020*(p))
#define REGRW_RT_MC(h,p)       ((regrw_i *)(h)->evb_rt + 0x048 + 0x020*(p))
#define REGRW_RT_LVL0(h,p)     ((regrw_i *)(h)->evb_rt + 0x050 + 0x020*(p))
#define REGRW_RT_LVL1(h,p)     ((regrw_i *)(h)->evb_rt + 0x054 + 0x020*(p))
#define REGRW_RT_LVL2(h,p)     ((regrw_i *)(h)->evb_rt + 0x058 + 0x020*(p))

#ifdef __cplusplus
}
#endif

#endif /* __REGRW_PRIVATE_H__ */

