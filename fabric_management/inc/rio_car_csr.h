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

#ifndef __RIO_CAR_CSR_H__
#define __RIO_CAR_CSR_H__

#include <stdint.h>
#include "rio_standard.h"
#include "rio_ecosystem.h"
#include "regrw.h"

/* rio_regdefs.h contains definitons for all RapidIO Standard Registers
*/

#ifdef __cplusplus
extern "C" {
#endif

struct rt_regs {
	uint32_t ctl; /* RIO_RT_BC_CTL, RIO_RT_SPX_CTL */
	uint32_t mc_oset; /* RIO_RT_BC_MC, RIO_RT_SPX_MC */
	uint32_t lvl0_oset; /* RIO_RT_BC_LVL0, RIO_RT_SPX_LVL0 */
	uint32_t lvl1_oset; /* RIO_RT_BC_LVL1, RIO_RT_SPX_LVL1 */
	uint32_t lvl2_oset; /* RIO_RT_BC_LVL2, RIO_RT_SPX_LVL2 */
};

struct rt_regs_blk {
	struct rt_regs bc;
	struct rt_regs pt[RIO_MAX_DEV_PORT];
};


/* Offsets, field masks and value definitions for Device CARs and CSRs
*/

typedef enum { 
	tt_uninit = 0,
	tt_dev8 = 1,
	tt_dev16 = 2,
	tt_dev32 = 3,
	tt_last = 4
} tt_t;

#define HC_LOCAL 0x100

struct rio_car_csr {
	/** \brief RapidIO CARs */
	uint32_t dev_ident; /* RIO_DEV_IDENT */
	uint32_t dev_info; /* RIO_DEV_INF */
	uint32_t assy_id; /* RIO_ASSY_ID */
	uint32_t assy_info; /* RIO_ASSY_INF */
	RIO_PE_FEAT_T pe_feat; /* RIO_PE_FEAT */
	uint32_t sw_port_info; /* RIO_SW_PORT_INF */
	uint32_t src_ops; /* RIO_SRC_OPS */
	uint32_t dst_ops; /* RIO_SRC_OPS */
	uint32_t sw_mc_sup; /* RIO_SW_MC_SUP */
	uint32_t sw_rt_lim; /* RIO_SW_RT_TBL_LIM */
	uint32_t sw_mc_info; /* RIO_SW_MC_INFO */

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
	uint32_t sp_oset; /* RO: RIO_EFB_T_SP_EP3, RIO_EFB_T_SP_EP3_SAER, */
			/* RIO_EFB_T_SP_NOEP3, RIO_EFB_T_SP_NOEP3_SAER */
	uint32_t sp3_oset; /* RIO_EFB_T_SP_EP3,RIO_EFB_T_SP_EP3_SAER, 
			* RIO_EFB_T_SP_NOEP3, RIO_EFB_T_SP_NOEP3_SAER */
	uint32_t sp_type;
	uint32_t ctl1[RIO_MAX_DEV_PORT]; /* Values of ctl1_reg */
			/* Values of RIO_SPX_ERR_STAT */
	RIO_SPX_ERR_STAT_T errstat[RIO_MAX_DEV_PORT];
	uint32_t emhs_oset; /* RIO_EFB_T_EMHS */

	uint32_t hs_oset; /* RIO_EFB_T_HS */
	uint32_t em_info; /* RIO_EMHS_INFO */
	uint32_t lane_oset; /* RO: RIO_EFB_T_LANE */
	uint32_t rt_oset; /* RO: RIO_EFB_T_RT */
	struct rt_regs_blk rt;

	/** \brief Device access info */
	uint32_t dest_id; /* DEstination ID used to access the device */
	tt_t tt; /* dev8, dev16, or dev32 */
	uint16_t hc; /* Hopcount used to access the device with mtc transactions
			* hc = HC_LOCAL means local device.
			*/
	/** \brief Register access driver handle */
	struct regrw_driver regrw;
};

/** \brief Macros to extract/check common fields/values
 *   from a * struct rio_car_csr.
 */

#define GET_DEV_VENDOR(x) (x->dev_ident & RIO_DEV_IDENT_VEND)
#define GET_DEV_IDENT(x) ((x->dev_ident & RIO_DEV_IDENT_DEVI) >> 16)
#define GET_ASSY_VENDOR(x) (x->assy_id & RIO_ASSY_ID_VEND)
#define GET_ASSY_IDENT(x) ((x->assy_id & RIO_ASSY_ID_ASSY) >> 16) 
#define GET_ASSY_VERSION(x) ((x->assy_info & RIO_ASSY_INF_ASSY_REV) >> 16)
#define GET_GET_DEV8(x) ((x->devid & RIO_MC_CON_SEL_DEV8) >> 16)
#define GET_CHG_DEV8(x,y) ((x->devid & ~RIO_MC_CON_SEL_DEV8) | \
			((y << 16) & RIO_MC_CON_SEL_DEV8))
#define GET_GET_DEV16(x) (x->devid & RIO_MC_CON_SEL_DEV16)
#define GET_CHG_DEV16(x,y) ((x->devid & ~RIO_MC_CON_SEL_DEV16) | \
			(y & RIO_MC_CON_SEL_DEV16))
#define GET_CT(x) (x->comptag)

#define GET_EXT_FEAT_OSET(x) ( \
	(x->pe_feat & RIO_PE_FEAT_EFB_VALID)? \
	(x->assy_info & RIO_ASSY_INF_EFB_PTR):0)

#define GET_ENUMB(x,p) (((p >= 0) && (p < RIO_PE_PORT_COUNT))? \
	(x->ctl1[p] & RIO_SPX_CTL_ENUM_B):0xFFFFFFFF)

#define RIO_ADDRSZ_SUPP(x) (RIO_PE_ADDR_T)(x->pe_feat & RIO_PE_FEAT_EXT_ADDR)
#define RIO_ADDR34_SUPP(x) (bool)(!!(x->pe_feat & RIO_PE_FEAT_EXT_ADDR34))
#define RIO_ADDR50_SUPP(x) (bool)(!!(x->pe_feat & RIO_PE_FEAT_EXT_ADDR50))
#define RIO_ADDR66_SUPP(x) (bool)(!!(x->pe_feat & RIO_PE_FEAT_EXT_ADDR66))

#define RIO_ADDRSZ_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_34BIT))
#define RIO_ADDR34_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_34BIT))
#define RIO_ADDR50_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_50BIT))
#define RIO_ADDR66_NOW(x) (bool)(!!(x->pe_ll_ctl & RIO_PE_LL_CTL_66BIT))

/* Check for processing element features */
#define RIO_STD_RTE(x) (x->pe_feat & RIO_PE_FEAT_STD_RTE)
#define RIO_EXT_RTE(x) (x->pe_feat & RIO_PE_FEAT_EXTD_RTE)

#define PE_IS_MP(x) (x->pe_feat & RIO_PE_FEAT_MULTIP)
#define PE_IS_SW(x) (x->pe_feat & RIO_PE_FEAT_SW)
#define PE_IS_PROC(x) (x->pe_feat & RIO_PE_FEAT_PROC)
#define PE_IS_MEM(x) (x->pe_feat & RIO_PE_FEAT_MEM)
#define PE_IS_BRIDGE(x) (x->pe_feat & RIO_PE_FEAT_BRDG)
#define PE_PORT_COUNT(x) \
        (rio_port_t)((PE_IS_SW(x))?RIO_AVAIL_PORTS(x->sw_port_info):1)
#define RIO_SW_ACC_PORT(x) \
        (rio_port_t)((PE_IS_SW(x))?RIO_ACCESS_PORT(x->sw_port_info):1)

/** \brief Initialize rio_car_csr structure based on registers read at
 * dest_id/tt/hc coordinates.
 *
 * Performs one register access to determine device type, and if successful,
 * may perform more register accesses to get values of control registers.
 *
 * No register accesses are performed to get CAR or other constant register 
 * values, unless the device is unknown.
 *
 * Sets access information for the device, but does not set device ID register.
 *
 * @params[out] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] dest_id Dev8, dev16, or dev32 destID to use to
 * 		access the device.
 * @params[in] tt_t Size of dest_id to use.  tt_dev_8 and 16, are supported.
 * @params[in] hc Hop count for maintenance transactions used to access the
 *                target device.
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int rcc_init(struct rio_car_csr *rcc, uint32_t dest_id, tt_t tt, uint16_t hc);

/** \brief Change the device ID, tt, and/or hop count used to access the 
 * device.  Note that this does not update the devices destID register.
 *
 * No register accesses are performed by this routine.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] dest_id Dev8, dev16, or dev32 destID to use to
 * 		access the device.
 * @params[in] tt_t Size of dest_id to use.  tt_dev_8 and 16, are supported.
 * @params[in] hc Hop count for maintenance transactions used to access the
 *                target device.
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int rcc_set_access(struct rio_car_csr *rcc, uint32_t dest_id, tt_t tt,
	uint16_t hc);

/** \brief Write the devices deviceID register.
 *
 * Note: use RIO_GET_DEV8, RIO_GET_DEV8 and RIO_GET_DEV16, RIO_GET_DEV16
 *       macros to retrieve and change the dest_ID for the device.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] dest_id Dev8, dev16, or dev32 destID to be updated on the device.
 * @params[in] tt_t Size of dest_id to be updated on the device.
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int rcc_write_destid(struct rio_car_csr *rcc, uint32_t dest_id, tt_t);

/** \brief Read the devices deviceID register(s), and store result in rcc.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int rcc_read_destids(struct rio_car_csr *rcc);

/** \brief Write the devices Component Tag register.
 *
 * Note: Writes the component tag, and reads back the value to confirm that
 *       it was written correctly.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] ct New component tag value.
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int rcc_write_comptag(struct rio_car_csr *rcc, uint32_t ct);

/** \brief Read the devices Component Tag register, and store result in rcc.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int rcc_read_comptag(struct rio_car_csr *rcc);

/** \brief Attempt to lock the device, using the specified lock value.
 *
 * If the lock is successful, rcc is updated with lock_val.
 * If the lock is unsuccessful, rcc is updated with the current lock value.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] lock_val Value to write to RIO_HOST_LOCK to lock the device.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_lock(struct rio_car_csr *rcc, uint32_t lock_val);

/** \brief Unlock the device, using the specified lock value.
 *
 * If the unlock is successful, rcc is updated with lock_val.
 * If the unlock is unsuccessful, rcc is updated with the current lock value.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] unlock_val Value to write to RIO_HOST_LOCK to unlock the device.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_unlock(struct rio_car_csr *rcc, uint32_t unlock_val);

/** \brief Set the device RapidIO memory address size for read/write/atomic
 *         transactions.
 *
 * Write, then read back the addrsz set.
 * Update rcc->pe_ll_ctl if the addrsz was set correctly. 
 * Fail if the addrsz is not set correctly.
 * Note: RIO_ADDRSZ_NOW, RIO_ADDR34_NOW, RIO_ADDR50_NOW, and RIO_ADDR66_NOW
 *	macros determine the address size currently in use.
 * Note: RIO_ADDRSZ_SUPP, RIO_ADDR34_SUPP, RIO_ADDR50_SUPP, and RIO_ADDR66_SUPP
 *	macros determine the address sizes supported by the device.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] addrsz One of RIO_PE_FEAT_EXT_ADDR34, RIO_PE_FEAT_EXT_ADDR50,
 * 			or RIO_PE_FEAT_EXT_ADDR66.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_set_addrsz(struct rio_car_csr *rcc, RIO_PE_ADDR_T addrsz );

/** \brief Set the enumeration boundary bit to the requested value for the 
 *         specified port.
 *
 * Set the enumeration boundary bit, and update appropriate rcc->ctrl1[] values
 * NOTE: Use the RIO_GET_ENUMB(x.p) macro to get the current enumeration 
 *       boundary bit setting.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for enumeration boundary bit.  If RIO_ALL_PORTS,
 *                then all device ports are updated.
 * @params[in] enumb value of enumeration boundary bit to set.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_set_enumb(struct rio_car_csr *rcc, rio_port_t pt, bool enumb);

/** \brief Read port(s) current error status register value.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for error status register.  If RIO_ALL_PORTS,
 *                then all device ports are updated.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_get_errstat(struct rio_car_csr *rcc, rio_port_t pt);

/** \brief Issue link reqest/port-status and retrieve response.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for error status register.  
 * 		Must not be RIO_ALL_PORTS.
 * @params[inout] resp Port status value returned.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_lreq_n_resp(struct rio_car_csr *rcc, rio_port_t pt, 
		RIO_SPX_LM_RESP_STAT_T resp);

/** \brief Update port controls as requested, and update rcc->ctl1[] values.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for error status register.  
 * 		May be RIO_ALL_PORTS.
 * @params[in] port_enable True if port transmitter should be enabled
 * @params[in] port_lkout True if port should not accept any packets
 * @params[in] in_out_en True if port should accept packets other than 
 * 			maintenance packets, false otherwise.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_port_enable(struct rio_car_csr *rcc, rio_port_t pt, 
		bool port_enable, bool port_lkout, bool in_out_en);

/** \brief Set port lockout bit immediately
 *
 * Sets the port lockout bit to prevent packets from being transmitted.
 * On older devices, this is a reaction to the detection of a fatal port error.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for error status register.  
 * 		Must not be RIO_ALL_PORTS.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_emerg_lkout(struct rio_car_csr *rcc, rio_port_t pt);

/** \brief Set routing value for a switch.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for the routing table.
 * 		Use RIO_ALL_PORTS for global routing table.
 * @params[in] tt Destination ID size
 * @params[in] did Set the route for this destination ID.
 * @params[in] rte Standard routing table entry value, one of RIO_RTE_DROP,
 *                 RIO_RTE_DFLT_PORT, RIO_RTV_PORT() or RIO_RTV_MC_MSK()
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_set_rte(struct rio_car_csr *rcc, rio_port_t pt,
		tt_t tt, uint32_t did, pe_rt_val rte);

/** \brief Get the routing value for a switch.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for the routing table.
 * 		Use RIO_ALL_PORTS for global routing table.
 * @params[in] tt Destination ID size
 * @params[in] did Get the route for this destination ID.
 * @params[in] rte Standard routing table entry value, one of RIO_RTE_DROP,
 *                 RIO_RTE_DFLT_PORT, RIO_RTV_PORT() or RIO_RTV_MC_MSK()
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_get_rte(struct rio_car_csr *rcc, rio_port_t pt,
		tt_t tt, uint32_t did, pe_rt_val *rte);

/** \brief Initialize the routing value for a switch.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] pt Port number for the routing table.
 * 		Use RIO_ALL_PORTS for global routing table.
 * @params[in] tt Destination ID size of routing table to initialize.
 * @params[in] rte Standard routing table entry value, one of RIO_RTE_DROP,
 *                 RIO_RTE_DFLT_PORT, RIO_RTV_PORT() or RIO_RTV_MC_MSK()
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_init_rte(struct rio_car_csr *rcc, rio_port_t pt,
		tt_t tt, pe_rt_val *rte);


/** \brief Set the default routing value for a switch.
 *
 * @params[inout] rcc RapidIO CAR/CSR tracking structure.
 * @params[in] rte Standard routing table entry value, one of RIO_RTE_DROP,
 *                 RIO_RTE_DFLT_PORT, RIO_RTV_PORT() or RIO_RTV_MC_MSK()
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int rcc_set_dflt_rte(struct rio_car_csr *rcc, pe_rt_val *rte);


#ifdef __cplusplus
}
#endif

#endif /* __RIO_CAR_CSR_H__ */

