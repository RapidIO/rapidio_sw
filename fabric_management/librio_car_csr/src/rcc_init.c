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
#include <string.h>
#include "rio_car_csr.h"
#include "regrw.h"
#include "CPS1848_registers.h"
#include "IDT_Tsi721.h"
#include "tsi578.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Reads stadard CAR and CSR values which are independent of the
 * Vendor and Device ID CAR.
 *
 * @param[inout] rcc Initialized to 0, with dev_ident/RIO_DEV_IDENT field set
 * returns int <> 0 if failure, 0 on success.
 */

uint32_t rcc_read_cars_csrs(struct rio_car_csr *rcc)
{
	int rc;

	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_DEV_INF, &rcc->dev_info);
	if (rc)
		goto fail;
	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_ASSY_ID, &rcc->assy_id);
	if (rc)
		goto fail;
	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_ASSY_INF, &rcc->assy_info);
	if (rc)
		goto fail;

	if (PE_IS_SW(rcc) || PE_IS_MP(rcc)) {
		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_SW_PORT_INF, &rcc->sw_port_info);
		if (rc)
			goto fail;
	} else {
		rcc->sw_port_info = 0x100; /* One port, accessed from port 0 */
	}
	if (PE_IS_PROC(rcc) || PE_IS_MEM(rcc) || PE_IS_BRIDGE(rcc)) {
		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_PE_LL_CTL, &rcc->pe_ll_ctl);
		if (rc)
			goto fail;
		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_LCS_ADDR0, &rcc->lcs_addr0);
		if (rc)
			goto fail;
		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_LCS_ADDR1, &rcc->lcs_addr1);
		if (rc)
			goto fail;
		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_DEVID, &rcc->did);
		if (rc)
			goto fail;
	};

	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_HOST_LOCK, &rcc->host_lock);
	if (rc)
		goto fail;
	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_COMPTAG, &rcc->comptag);
	if (rc)
		goto fail;
	if (PE_IS_SW(rcc)) {
		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
					RIO_DFLT_RTE, &rcc->dflt_rte);
	};
fail:
	return rc;
};

uint32_t rcc_init_idt_cps_gen2(struct rio_car_csr *rcc)
{
	int rc = 0;

	rcc->pe_feat = 0x18000779 ;
	rcc->src_ops = 0x4;
	rcc->sw_mc_sup = 0x4;
	rcc->sw_rt_lim = 0x00FF;
	rcc->sw_mc_info = 0x00FF0028 ;

	rcc->sp_oset = CPS1848_PORT_MAINT_BLK_HEAD;
	rcc->sp_type = RIO_EFB_T_SP_NOEP_SAER;
	rcc->emhs_oset = CPS1848_PORT_MAINT_BLK_HEAD;
	rcc->lane_oset = CPS1848_LANE_STATUS_BLK_HEAD;
	rcc->rt_oset = 0xFFFF;

	rc = rcc_read_cars_csrs(rcc);
	if (rc)
		goto fail;

	rcc->rt.bc.ctl = RIO_RT_BC_CTL_THREE_LEVELS;
	rcc->rt.bc.lvl1_oset = 0x01000000 | CPS_BROADCAST_DOMAIN_ROUTE_TABLE;
	rcc->rt.bc.lvl2_oset = 0x01000000 | CPS_BROADCAST_DEVICE_ROUTE_TABLE;

	for (rio_port_t p = 0; p < PE_PORT_COUNT(rcc); p++) {
		rcc->rt.pt[p].ctl = 0;
		rcc->rt.pt[p].mc_oset = 0;
		rcc->rt.pt[p].lvl1_oset = 0x01000000 |
					CPS1848_PORT_X_DOM_RTE_TABLE_Y(p,0);
		rcc->rt.pt[p].lvl2_oset = 0x01000000 |
					CPS1848_PORT_X_DEV_RTE_TABLE_Y(p,0);
	};
fail:
	return rc;
}

uint32_t rcc_init_tundra_tsi57x(struct rio_car_csr *rcc)
{
	rcc->pe_feat = 0x1000051F;
	rcc->src_ops = 0x4;
	rcc->sw_mc_sup = 0x4;
	rcc->sw_rt_lim = 0x00FF;
	rcc->sw_mc_info = 0x00000008;

	rcc->sp_oset = Tsi578_RIO_SW_MB_HEAD;
	rcc->sp_type = RIO_EFB_T_SP_NOEP_SAER;
	rcc->emhs_oset = Tsi578_RIO_ERR_RPT_BH;

	return rcc_read_cars_csrs(rcc);
}

uint32_t rcc_init_idt_tsi721(struct rio_car_csr *rcc)
{
	rcc->pe_feat = 0xC000003F;
	rcc->src_ops = 0x0000FC04;
	rcc->dst_ops = 0x0000FC04;

	rcc->sp_oset = TSI721_RIO_SP_MB_HEAD;
	rcc->sp_type = RIO_EFB_T_SP_NOEP_SAER;
	rcc->emhs_oset = TSI721_RIO_ERR_RPT_BH;
	rcc->lane_oset = TSI721_RIO_PER_LANE_BH;

	return rcc_read_cars_csrs(rcc);
}

uint32_t rcc_init_default(struct rio_car_csr *rcc)
{
    	uint32_t rc;

	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_PE_FEAT, &rcc->pe_feat);
	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_SRC_OPS, &rcc->src_ops);
	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_DST_OPS, &rcc->dst_ops);

	rc = rcc_read_cars_csrs(rcc);
	if (rc || !(rcc->pe_feat & RIO_PE_FEAT_EFB_VALID))
		return rc;
	
	uint32_t efp = rcc->assy_info & RIO_ASSY_INF_EFB_PTR;

	while (efp != RIO_EFB_NO_NEXT) {
		uint32_t bh;
		uint32_t bh_type;

		rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc, efp, &bh);
		if (rc)
			goto fail;

		bh_type = efp & RIO_EFB_T;
		switch (bh_type) {
		case RIO_EFB_T_SP_EP:
		case RIO_EFB_T_SP_EP_SAER:
		case RIO_EFB_T_SP_NOEP:
		case RIO_EFB_T_SP_NOEP_SAER:
			rcc->sp_oset = efp;
			rcc->sp_type = bh_type;
			break;

		case RIO_EFB_T_SP_EP3:
		case RIO_EFB_T_SP_EP3_SAER:
		case RIO_EFB_T_SP_NOEP3:
		case RIO_EFB_T_SP_NOEP3_SAER:
			rcc->sp3_oset = efp;
			rcc->sp_type = bh_type;
			break;

		case RIO_EFB_T_EMHS:
			rcc->emhs_oset = efp;
			break;

		case RIO_EFB_T_HS:
			rcc->hs_oset = efp;
			break;

		case RIO_EFB_T_LANE:
			rcc->lane_oset = efp;
			break;

		case RIO_EFB_T_RT:
			rcc->rt_oset = efp;
			break;
		default: break;
		}
		efp = (bh & RIO_EFB_NEXT) >> 16;
	};

	/// \todo Add subroutines to read ctl1, errstat, and rt_regs_blk
fail:
	return rc;
}

int rcc_init_idt(struct rio_car_csr *rcc)
{
	uint32_t dev_id = GET_DEV_IDENT(rcc);
	int rc = EINVAL;

	switch (dev_id) {
	case RIO_DEVI_IDT_CPS1848:
	case RIO_DEVI_IDT_CPS1432:
	case RIO_DEVI_IDT_CPS1616:
	case RIO_DEVI_IDT_VPS1616:
	case RIO_DEVI_IDT_SPS1616:
			rc = rcc_init_idt_cps_gen2(rcc);
			break;
	case RIO_DEVI_IDT_TSI721:
			rc = rcc_init_idt_tsi721(rcc);
			break;
/*
	case RIO_DEVI_IDT_RXS2448:
	case RIO_DEVI_IDT_RXS1632:
			rc = rcc_init_idt_rxs(rcc);
			break;
*/
	default: rc = rcc_init_default(rcc);
			break;
	};

	return rc;
};

int rcc_init_tundra(struct rio_car_csr *rcc)
{
	uint32_t dev_id = GET_DEV_IDENT(rcc);
	int rc = EINVAL;

	switch (dev_id) {
	case RIO_DEVI_TSI568:
	case RIO_DEVI_TSI572:
	case RIO_DEVI_TSI574:
	case RIO_DEVI_TSI577:
	case RIO_DEVI_TSI578: // Also covers RIO_DEVI_TSI576
			rc = rcc_init_tundra_tsi57x(rcc);
			break;
	default: rc = rcc_init_default(rcc);
			break;
	};

	return rc;
};

int rcc_init(struct rio_car_csr *rcc, uint32_t dest_id, tt_t tt, uint16_t hc)
{
	int rc;
	uint32_t dev_ident;

	memset(rcc, 0, sizeof(*rcc));
	rcc->dest_id = dest_id;
	rcc->tt = tt;
	rcc->hc = hc;

	rc = raw_reg_rd(rcc, tt, dest_id, hc, RIO_DEV_IDENT, &dev_ident);
	if (rc)
		goto fail;

	rcc->dev_ident = dev_ident;

	switch (GET_DEV_VENDOR(rcc)) {
	case RIO_VEND_IDT: rc = rcc_init_idt(rcc);
			break;
	case RIO_VEND_TUNDRA: rc = rcc_init_tundra(rcc);
			break;
	default: rc = rcc_init_default(rcc);
			break;
	}

fail:
	return rc;
};

#ifdef __cplusplus
}
#endif
