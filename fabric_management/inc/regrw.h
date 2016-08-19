/* Register read/write interface
 */
/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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

#ifndef __REGRW_H__
#define __REGRW_H__

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include "rio_standard.h"
#include "rio_ecosystem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief If can't use malloc, then must have a maximum number of handles */
#ifndef REGRW_USE_MALLOC
#define REGRW_NUM_HANDLES 10
#define REGRW_NUM_SWITCHES 2
#endif

#ifdef DEBUG
#define REGRW_LOGGING_ENABLED
#endif

typedef void *regrw_h;

/** All routines return 0 on success and -1 on failure.
 * Init handle may perform several register accesses.
 * Errno is set on failure.
 */
int regrw_get_handle(regrw_h *h);
int regrw_init_handle(regrw_h h, uint32_t did, rio_hc_t hc);
int regrw_destroy_handle(regrw_h *h);

int regrw_set_path(regrw_h h, uint32_t did, rio_hc_t hc);
int regrw_get_path(regrw_h h, uint32_t *did, rio_hc_t *hc);

/** \brief Get the current register value:
 * - from stored value if possible
 * - from * the device if not
 * Correct the value to be compliant with standard register definitions;
 */
int regrw_rd(regrw_h h, uint32_t offset, uint32_t *val);
/** \brief Write the register value to the device.
 * Update the stored value if possible.
 * Correct the value to match any device errata before writing.
 */
int regrw_wr(regrw_h h, uint32_t offset, uint32_t val);
/** \brief Read the register value from the device.
 * Do not update the stored value.
 * Correct the value to be compliant with standard register definitions;
 */
int regrw_rare_rd(regrw_h h, uint32_t did, rio_hc_t hc,
					uint32_t addr, uint32_t *val);
/** \brief Write the register value to the device.
 * Do not update the stored value.
 * Correct the value to match any device errata before writing.
 */
int regrw_rare_wr(regrw_h h, uint32_t did, rio_hc_t hc,
					uint32_t addr, uint32_t val);

/** \brief Read the register value from the device.
 * Do not update the stored value.
 * Do not correct the value for any errata.
 * Use the specified path instead of the default
 */
int regrw_raw_rd(regrw_h h, uint32_t did, rio_hc_t hc,
		uint32_t addr, uint32_t *val);

/** \brief Write the register value to the device.
 * Do not update the stored value.
 * Do not correct the value for any errata.
 * Use the specified path instead of the default
 */
int regrw_raw_wr(regrw_h h, uint32_t did, rio_hc_t hc,
		uint32_t addr, uint32_t val);

typedef enum {
        regrw_tt_uninit = 0,
        regrw_tt_dev8 = 1,
        regrw_tt_dev16 = 2,
        regrw_tt_dev32 = 3,
        regrw_tt_last = 4
} regrw_tt_t;

/** \brief Write the devices deviceID register.
 *
 * @params[inout] h regrw handle
 * @params[in] dest_id Dev8, dev16, or dev32 destID to be updated on the device.
 * @params[in] tt Size of dest_id to be updated on the device.
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int regrw_write_destid(regrw_h h, regrw_tt_t tt, uint32_t dest_id);

/** \brief Read all the devices deviceID register(s), and store result in h
 * Includes support for dev8, dev16, and dev32.
 *
 * @params[inout] h regrw handle
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int regrw_read_destids(regrw_h h);

/** \brief Write the devices Component Tag register.
 *
 * Note: Writes the component tag, and reads back the value to confirm that
 *       it was written correctly.
 *
 * @params[inout] h regrw handle
 * @params[in] ct New component tag value.
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int regrw_write_comptag(regrw_h h, uint32_t ct);

/** \brief Read the devices Component Tag register, and store result in rcc.
 *
 * @params[inout] h regrw handle
 *
 * returns standard error code
 * return 0 - success
 * return <0 - -errno
 */

int regrw_read_comptag(regrw_h h);

/** \brief Attempt to lock the device, using the specified lock value.
 *
 * If the lock is successful, rcc is updated with lock_val.
 * If the lock is unsuccessful, rcc is updated with the current lock value.
 *
 * @params[inout] h regrw handle
 * @params[inout] lck_val Value to write to RIO_HOST_LOCK to lock the device.
 * 		Updated with current lock value on exit.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success - device is locked
 * return <0 - -errno - device is not locked
 */

int regrw_lock(regrw_h h, uint32_t *lck_val);

/** \brief Unlock the device, using the specified lock value.
 *
 * If the unlock is successful, rcc is updated with lock_val.
 * If the unlock is unsuccessful, rcc is updated with the current lock value.
 *
 * @params[inout] h regrw handle
 * @params[inout] unlck_val Value to write to RIO_HOST_LOCK to unlock 
 * 		the device.  Updated with current lock value on exit.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success - device is unlocked
 * return <0 - -errno - device may still be locked
 */

int regrw_unlock(regrw_h h, uint32_t *unlck_val);

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
 * @params[inout] h regrw handle
 * @params[in] addrsz One of RIO_PE_FEAT_EXT_ADDR34, RIO_PE_FEAT_EXT_ADDR50,
 * 			or RIO_PE_FEAT_EXT_ADDR66.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_set_addrsz(regrw_h h, RIO_PE_ADDR_T addrsz );

/** \brief Set the enumeration boundary bit to the requested value for the 
 *         specified port.
 *
 * Set the enumeration boundary bit, and update appropriate rcc->ctrl1[] values
 * NOTE: Use the RIO_GET_ENUMB(x.p) macro to get the current enumeration 
 *       boundary bit setting.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for enumeration boundary bit.  If RIO_ALL_PORTS,
 *                then all device ports are updated.
 * @params[in] enumb value of enumeration boundary bit to set.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_set_enumb(regrw_h h, rio_port_t p, bool enumb);

/** \brief Read port(s) current error status register value.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for error status register.  If RIO_ALL_PORTS,
 *                then all device ports are updated.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_get_errstat(regrw_h h, rio_port_t p, RIO_SPX_ERR_STAT_T *e_stat);

/** \brief Issue link reqest/port-status and retrieve response.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for error status register.  
 * 		Must not be RIO_ALL_PORTS.
 * @params[inout] resp Port status value returned.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_lreq_n_resp(regrw_h h, rio_port_t p, RIO_SPX_LM_RESP_STAT_T *lresp);

/** \brief Update port controls as requested, and update rcc->ctl1[] values.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for error status register.  
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

int regrw_port_enable(regrw_h h, rio_port_t p,
		bool port_enable, bool port_lkout, bool in_out_en);

/** \brief Set port lockout bit immediately
 *
 * Sets the port lockout bit to prevent packets from being transmitted.
 * On older devices, this is a reaction to the detection of a fatal port error.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for error status register.  
 * 		Must not be RIO_ALL_PORTS.
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_emerg_lkout(regrw_h h, rio_port_t p);

/** \brief Set routing value for a switch.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for the routing table.
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

int regrw_wr_rte(regrw_h h, rio_port_t p,
		regrw_tt_t tt, uint32_t did, rio_rtv_t rte);

/** \brief Get the routing value for a switch.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for the routing table.
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

int regrw_rd_rte(regrw_h h, rio_port_t p, regrw_tt_t tt, uint32_t did,
								rio_rtv_t *rte);

/** \brief Initialize the routing value for a switch.
 *
 * @params[inout] h regrw handle
 * @params[in] p Port number for the routing table.
 * 		Use RIO_ALL_PORTS for global routing table.
 * @params[in] tt Destination ID size of routing table to initialize.
 * @params[in] rte Standard routing table entry value, one of RIO_RTE_DROP,
 *                 RIO_RTE_DFLT_PORT, RIO_RTV_PORT() or RIO_RTV_MC_MSK()
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_init_rte(regrw_h h, rio_port_t p, regrw_tt_t tt, rio_rtv_t *rte);


/** \brief Set the default routing value for a switch.
 *
 * @params[inout] h regrw handle
 * @params[in] rte Standard routing table entry value, one of RIO_RTE_DROP,
 *                 RIO_RTE_DFLT_PORT, RIO_RTV_PORT() or RIO_RTV_MC_MSK()
 *
 * returns standard error code indicating success or failure.
 * return 0 - success
 * return <0 - -errno
 */

int regrw_wr_dflt_rte(regrw_h h, rio_rtv_t *rte);

/** Routnes which retrieve information from the initialized handle.
 * No register accesses are performed.
 */

uint32_t regrw_dev_vendor(regrw_h h);
uint32_t regrw_dev_ident(regrw_h h);
uint32_t regrw_assy_vendor(regrw_h h);
uint32_t regrw_assy_ident(regrw_h h);
uint32_t regrw_assy_version(regrw_h h);
uint32_t regrw_dev8(regrw_h h);
uint32_t regrw_new_dev8(regrw_h h,uint8_t dev8);
uint32_t regrw_dev16(regrw_h h);
uint32_t regrw_new_dev16(regrw_h h, uint16_t dev16);
uint32_t regrw_dev32(regrw_h h);
uint32_t regrw_ct(regrw_h h);
uint32_t regrw_scratch(regrw_h h);

uint32_t regrw_ext_feat_oset(regrw_h h);
uint32_t regrw_enumb(regrw_h h, rio_port_t p);

RIO_PE_ADDR_T regrw_addrsz_supp(regrw_h h);
bool regrw_addr34_supp(regrw_h h);
bool regrw_addr50_supp(regrw_h h);
bool regrw_addr66_supp(regrw_h h);

RIO_PE_ADDR_T regrw_addrsz_now(regrw_h h);
bool regrw_addr34_now(regrw_h h);
bool regrw_addr50_now(regrw_h h);
bool regrw_addr66_now(regrw_h h);

/* Check for processing element features */
bool regrw_std_rte(regrw_h h);
bool regrw_ext_rte(regrw_h h);

uint32_t regrw_is_MULTP(regrw_h h);
uint32_t regrw_is_SW(regrw_h h);
uint32_t regrw_is_PROC(regrw_h h);
uint32_t regrw_is_MEM(regrw_h h);
uint32_t regrw_is_BRIDGE(regrw_h h);
rio_port_t regrw_port_count(regrw_h h);
rio_port_t regrw_sw_acc_port(regrw_h h);

/* Names for vendor and device type */
int regrw_vend_name(regrw_h h, const char **name);
int regrw_dev_t_name(regrw_h h, const char **name);

struct regrw_driver {
	int (* rd)(regrw_h *h, uint32_t did, rio_hc_t hc,
                        uint32_t offset, uint32_t *val);
        int (* wr)(regrw_h *h, uint32_t did, rio_hc_t hc,
                        uint32_t offset, uint32_t val);
        int (* dly)(int dly_usecs);
	void *drv_data;
};

/* \brief Override device/handle register read/write functions 
 * Only non-null entries in drv are copied to *h.
 */
int regrw_override_h_drvr(regrw_h *h, struct regrw_driver *drv);

/* \brief Override default register read/write functions 
 * Only non-null entries in drv are copied.
 */
int regrw_override_regrw_drv(struct regrw_driver *drv);

/* \brief Set regrw logging level for capture and display (dlevel) */
int regrw_set_log_level(int level);
int regrw_set_log_dlevel(int level);
/* \brief Get regrw logging level for capture and display (dlevel) */
int regrw_get_log_level(void);
int regrw_get_log_dlevel(void);

/* \brief Bind regrw cli commands */
void regrw_bind_cli_cmds(void);

#ifdef __cplusplus
}
#endif

#endif /* __REGRW_H__ */
