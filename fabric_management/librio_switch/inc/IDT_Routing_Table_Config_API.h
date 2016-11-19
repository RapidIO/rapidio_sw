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

#ifndef __IDT_ROUTING_TABLE_CONFIG_API_H__
#define __IDT_ROUTING_TABLE_CONFIG_API_H__

#include <DAR_DevDriver.h>
#include <IDT_Common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
   This file is structured as:
   - Definitions of constants and types
   - Input and output parameter structures for each routine
   - List of routines for routing table support
*/

/* Legacy definitions for legacy users */
#define IDT_DSF_RT_USE_DEVICE_TABLE                 RIO_RTE_LVL_G0
#define IDT_DSF_RT_USE_DEFAULT_ROUTE                RIO_RTE_DFLT_PORT
#define IDT_DSF_RT_NO_ROUTE                         RIO_RTE_DROP

#define IDT_DSF_FIRST_MC_MASK                       RIO_RTE_MC_0
#define IDT_DSF_MAX_MC_MASK                         RIO_MAX_MC_MASKS
#define IDT_DSF_BAD_MC_MASK                         RIO_RTE_BAD

#define IDT_LAST_DEV8_DESTID  0xFF
#define IDT_LAST_DEV16_DESTID 0xFFFF

#define MC_MASK_IDX_FROM_ROUTE(x) (uint32_t)(RIO_RTV_GET_MC_MSK(x))
#define MC_MASK_ROUTE_FROM_IDX(x) (RIO_RTV_MC_MSK(x))

typedef enum { tt_dev8, tt_dev16, tt_dev32 } tt_t;

typedef enum rio_rt_disc_reason_t_TAG
{
   rio_rt_disc_not                   =  0,  // Packet is not discarded
   rio_rt_disc_rt_invalid            =  1,  // Invalid value in routing table
   rio_rt_disc_deliberately          =  2,  // Routing table selects port discard
   rio_rt_disc_port_unavail          =  3,  // Target port is unavailable
   rio_rt_disc_port_pwdn             =  4,  // Target port is powered down
   rio_rt_disc_port_fail             =  5,  // Target port has PORT_ERR condition
   rio_rt_disc_port_no_lp            =  6,  // Target port does not have PORT_OK status
   rio_rt_disc_port_lkout_or_dis     =  7,  // Target port LOCKOUT or PORT_DISABLE bits are set
   rio_rt_disc_port_in_out_dis       =0x8,  // Target port input/output enable bits are clear
   rio_rt_disc_mc_empty              =0x9,  // DestID is multicast with an empty mask
   rio_rt_disc_mc_one_bit            =0xA,  // DestID is multicast with a mask empty except for this port
   rio_rt_disc_mc_mult_masks         =0xB,  // Some devices allow multiple masks to be mapped to the same device ID
   rio_rt_disc_dflt_pt_invalid       =0xC,  // Default route port is invalid
   rio_rt_disc_dflt_pt_deliberately  =0x0D, // Default route port deliberately discards packets
   rio_rt_disc_dflt_pt_unavail       =0x0E, // Default route port is unavailable
   rio_rt_disc_dflt_pt_pwdn          =0x0F, // Default route port is powered down
   rio_rt_disc_dflt_pt_fail          =0x10, // Default route port has PORT_ERR condition
   rio_rt_disc_dflt_pt_no_lp         =0x11, // Default route port does not have PORT_OK status
   rio_rt_disc_dflt_pt_lkout_or_dis  =0x12, // Default route port LOCKOUT or PORT_DISABLE bits are set
   rio_rt_disc_dflt_pt_in_out_dis    =0x13, // Default route port input/output enable bits are clear
   rio_rt_disc_probe_abort           =0x14, // Probe aborted due to routing error
   rio_rt_disc_last                  =0x15, // Last entry...
   rio_rt_disc_dflt_pt_used          =0x16, // DEPRECATED
} rio_rt_disc_reason_t;

extern char *idt_em_disc_reason_names[ (uint8_t)(rio_rt_disc_last) ];

#define DISC_REASON_STR(x) ((x < (uint8_t)(rio_rt_disc_last))?(idt_em_disc_reason_names[x]):"OORange")

#define MC_MASK_ADD_PORT(m,p) ((rio_mcm_t)(m |  (rio_mcm_t)(1 << (rio_port_t)p)))
#define MC_MASK_REM_PORT(m,p) ((rio_mcm_t)(m & ~(rio_mcm_t)(1 << (rio_port_t)p)))
#define MC_MASK_GOT_PORT(m,p) ((rio_mcm_t)(m &  (rio_mcm_t)(1 << (rio_port_t)p)))

/** \brief Information for a single multicast mask.
 *
 * mc_destID, tt, and mc_mask, can be manipulated by user software.
 *
 * in_use, allocd and changed should not be set by user software.
 */

typedef struct {
rio_dev32_t	mc_destID;	///< First device ID associated with this mask.
	///< There may be others...
tt_t	tt;		///< Size of first device ID associated with 
	///< this mask.  There may be others...
rio_mcm_t	mc_mask;	///< Multicast mask value.
	///< Least significant bit is port 0
	///< Most significant bit is Port 31.
	///< Can be modified with the MC_MASK_ADD/REM
	///< macros, and queried with MC_MASK_GOT_PORT.
bool   in_use;   ///< true if this multicast mask and mc_destID are in use.
bool   allocd;   ///< true if this mask has been allocated by 
	///< rio_rt_alloc_mc_mask.  Otherwise, should be ignored.
bool   changed;  ///< true if the mc_destID or mc_mask value has changed.
} rio_rt_mc_info_t;

typedef struct {
rio_rte_t rte_val; ///< Routing table entry value.
bool	changed;   ///< true if the rte_val value has changed.
} rio_rt_uc_info_t;

typedef struct {
   rio_rt_uc_info_t       l0[RIO_RT_MAX_FLAT_GRPS][RIO_RT_GRP_SIZE];
} rio_rt_flat_t;

typedef struct rio_rt_hier_t_TAG {
rio_rt_uc_info_t l0[RIO_RT_GRP_SIZE]
rio_rt_uc_info_t l1[RIO_RT_MAX_L1_GRP][RIO_RT_GRP_SIZE];
rio_rt_uc_info_t l2[RIO_RT_MAX_L2_GRP][RIO_RT_GRP_SIZE]; 
} rio_rt_hier_t;

typedef struct {
    bool	rt_is_flat;
    rio_rte_t   default_route;             
    ovly {
	rio_rt_flat_t flat;
	rio_rt_hier_t hier;
    } rtv;
    rio_rt_mc_info_t      mc_masks[RIO_MAX_MC_MASKS];
} rio_rt_state_t;

/* For code which used the old style interface definitions */
#define dom_table rtv.hier.l1[0]
#define dev_table rtv.hier.l2[0]

typedef struct {
rio_port_t set_on_port;///< Must be a valid port number, or RIO_ALL_PORTS
	///< Note that when RIO_ALL_PORTS is specified, 
	///< this function also clears all multicast masks and removes all
	///< associations between multicast masks and ports.
rio_rte_t default_route; ///< Routing control for RIO_DFLT_RTE register
	///< Must be a valid port number, or RIO_RTE_DROP/IDT_DSF_RT_NO_ROUTE
rio_rte_t default_route_table_port; ///< Initial value for routing table entries.
	///< Must be one of the following:
	///< - a valid port number
	///< - RIO_RTE_DROP/IDT_DSF_RT_NO_ROUTE
	///< - RIO_RTE_DFLT_PORT/IDT_DSF_RT_USE_DEFAULT_ROUTE
bool update_hw; ///< true : Update hardware state
rio_rt_state_t *rt; ///< Optionally provide a pointer to
	///< an rio_rt_state_t structure. If provided, the 
	///< structure is initialized to match default_route_table_port
} rio_rt_initialize_in_t;

typedef struct {
uint32_t imp_rc;
} rio_rt_initialize_out_t;

typedef struct {
rio_port_t	probe_on_port; // Must be a valid port number, or RIO_ALL_PORTS
	// RIO_ALL_PORTS probes the global routing table.
tt_t		tt; ///< DestID size 
uint16_t	destID; ///< Check routing for specified device ID.
rio_rt_state_t	*rt; ///< A pointer to an rio_rt_state_t structure.
	///<   This structure is used to determine the route.
} rio_rt_probe_in_t;

typedef struct {
uint32_t imp_rc;  
bool	valid_route;  ///< true : packets will exit the port as 
	///<	defined by the routing_table_value.
	///< false: Packets will be discarded as 
	///<	indicated by reason_for_discard.
rio_prt_t routing_table_value; ///< Encoded routing table value read
rio_rte_t default_route;       ///< When routing_table_value is 
	///<    RIO_RTE_DFLT_PORT/IDT_DSF_RT_USE_DEFAULT_ROUTE,
	///<    this field contains the value of 
	///<    the default route register.
bool	filter_function_active;   ///< If true, packets that
	///<   match the FILTER MASK will be
	///<   dropped.
bool	trace_function_active;    ///< If true, packets that
	///<   match the TRACE MASK will be
	///<   copied to the trace port.
bool	time_to_live_active;  ///< If true, packets buffered for longer than
	///<   the time-to-live period may be discarded.
bool	mcast_ports[RIO_MAX_DEV_PORT];
	///< True if packet will be multicast to this
	///<   port, false if not.
	///< For completeness, will return true for
	///<   the port which was queried!
	///< Only valid if the routing_table_value
	///<   indicates a multicast mask.
rio_rt_disc_reason_t reason_for_discard; ///< Encoding for the reason that
	///< packets will be discarded.  Only valid if valid_route is false.
} rio_rt_probe_out_t;


typedef struct {
rio_port_t	probe_on_port; ///> Must be a valid port number, or RIO_ALL_PORTS.
	///< RIO_ALL_PORTS probes the global routing table, which may not 
	///< be consistent with per-port routing tables.
rio_rt_state_t *rt; ///< Pointer to routing table state structure.
} rio_rt_probe_all_in_t;

typedef struct {
uint32_t imp_rc;
} rio_rt_probe_all_out_t;

typedef struct {
uint8_t	set_on_port; ///< A valid port number, or RIO_ALL_PORTS.
	///< RIO_ALL_PORTS selects the global routing table 
rio_rt_state_t *rt;
} rio_rt_set_all_in_t;

typedef struct {
uint32_t imp_rc;
} rio_rt_set_all_out_t;

typedef rio_rt_set_all_in_t  rio_rt_set_changed_in_t  ;
typedef rio_rt_set_all_out_t rio_rt_set_changed_out_t ;

typedef struct {
rio_rt_state_t *rt; 
} rio_rt_alloc_mc_mask_in_t;

typedef struct {
uint32_t imp_rc;
rio_rte_t mc_mask_rte; ///< Multicast mask allocated.  Only valid for success.
	///<   If no free multicast masks exist, set to RIO_RTE_BAD
} rio_rt_alloc_mc_mask_out_t;

typedef struct {
rio_rte_t mc_mask_rte; ///< Multicast mask routing value to be removed from
	///< the routing table state pointed to by "rt".
	///< The multicast mask is also cleared to 0 by this routine.
rio_rt_state_t *rt;
} rio_rt_dealloc_mc_mask_in_t;

typedef struct {
    uint32_t imp_rc;
} rio_rt_dealloc_mc_mask_out_t;

typedef struct {
bool	dom_entry; ///< true  if domain routing table entry is being updated 
	///< false if device routing table entry is being update
uint8_t idx; ///< Index of routing table entry to be updated
	///< 0 to RIO_RT_GRP_SIZE
rio_rte_t rte_value;  ///< Value for the routing table entry
	///< Note that if the requested routing table entry
	///< matches the routing table entry value in *rt,
	///< the rio_rt_uc_info_ti changed field is false.
rio_rt_state_t *rt;
} rio_rt_change_rte_in_t;

typedef struct {
uint32_t imp_rc; 
} rio_rt_change_rte_out_t;

typedef struct {
rio_rte_t mc_mask_rte; ///< The mask to be modified.                          
rio_rt_mc_info_t mc_info; ///< Multicast information to be assigned to the mask
rio_rt_state_t  *rt; 
} rio_rt_change_mc_mask_in_t;

typedef struct {
uint32_t imp_rc;
} rio_rt_change_mc_mask_out_t;


// Implementation specific return code starting numbers for each
// standard routine.  These are the "base numbers" for the "imp_rc"
// fields in the return code structures.  
//
// RT_FIRST_SUBROUTINE_0 is the first "base number" to be used
// for implementation specific subroutines complex enough to
// warrant their own implementation specific return codes.

#define RT_INITIALIZE_0       (DAR_FIRST_IMP_SPEC_ERROR+0x1000)
#define RT_PROBE_0            (DAR_FIRST_IMP_SPEC_ERROR+0x1100)
#define RT_PROBE_ALL_0        (DAR_FIRST_IMP_SPEC_ERROR+0x1200)
#define RT_SET_ALL_0          (DAR_FIRST_IMP_SPEC_ERROR+0x1300)
#define RT_SET_CHANGED_0      (DAR_FIRST_IMP_SPEC_ERROR+0x1400)
#define RT_ALLOC_MC_MASK_0    (DAR_FIRST_IMP_SPEC_ERROR+0x1500)
#define RT_DEALLOC_MC_MASK_0  (DAR_FIRST_IMP_SPEC_ERROR+0x1600)
#define RT_CHANGE_RTE_0       (DAR_FIRST_IMP_SPEC_ERROR+0x1700)
#define RT_CHANGE_MC_MASK_0   (DAR_FIRST_IMP_SPEC_ERROR+0x1800)
#define RT_FIRST_SUBROUTINE_0 (DAR_FIRST_IMP_SPEC_ERROR+0x100000)

/**
 * @brief Initializes the routing table hardware and/or routing table state
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[inout] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
*/ 

uint32_t rio_rt_initialize(
    DAR_DEV_INFO_t           *dev_info,
    rio_rt_initialize_in_t   *in_parms,
    rio_rt_initialize_out_t  *out_parms
);

#define RT_INITIALIZE(x) (RT_INITIALIZE_0+x)

/**
 * @brief Probes the hardware status of a routing table entry for 
 *   the specified port and destination ID
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[in] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
*/ 

uint32_t rio_rt_probe(
    DAR_DEV_INFO_t      *dev_info,
    rio_rt_probe_in_t   *in_parms,
    rio_rt_probe_out_t  *out_parms
);

#define RT_PROBE(x) (RT_PROBE_0+x)

/**
 * @brief Reads all hardware routing table entries and stores them in 
 *    the in_parms->rt structure. 
 * After rio_rt_probe_all is called, no entries are marked as changed in
 *    the in_parms->rt structure.
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 * 
 * This function returns the complete hardware state of packet routing
 * in a routing table state structure.
 *
 * The routing table hardware must be initialized using rio_rt_initialize() 
 *    before calling this routine.
 * 
*/

uint32_t rio_rt_probe_all(
    DAR_DEV_INFO_t          *dev_info,
    rio_rt_probe_all_in_t   *in_parms,
    rio_rt_probe_all_out_t  *out_parms
);

#define RT_PROBE_ALL(x) (RT_PROBE_ALL_0+x)

/**
 * @brief Sets the routing table hardware to match every entry
 *    in the in_parms->rt structure. 
 * After rio_rt_set_all is called, no entries are marked as changed in
 *    the in_parms->rt structure.
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 *
*/

uint32_t rio_rt_set_all (
    DAR_DEV_INFO_t        *dev_info, 
    rio_rt_set_all_in_t   *in_parms, 
    rio_rt_set_all_out_t  *out_parms
);

#define RT_SET_ALL(x) (RT_SET_ALL_0+x)

/**
 * @brief Sets the the routing table hardware to match every entry
 *    that has been changed in the in_parms->rt structure. 
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 *
 * Changes must be made using rio_rt_alloc_mc_mask, rio_rt_deallocate_mc_mask,
 *    rio_rt_change_rte, and rio_rt_change_mc.
 * After rio_rt_set_changed is called, no entries are marked as changed in
 *    the in_parms->rt structure.
*/

uint32_t rio_rt_set_changed (
    DAR_DEV_INFO_t            *dev_info, 
    rio_rt_set_changed_in_t   *in_parms, 
    rio_rt_set_changed_out_t  *out_parms
);

#define RT_SET_CHANGED(x) (RT_SET_CHANGED_0+x)

/**
 * @brief Updates an in_parms->rt structure to
 * find the first previously unused multicast mask.  
 * Can be called consecutively to allocate multiple 
 * multicast masks.
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 *
*/

uint32_t rio_rt_alloc_mc_mask(
    DAR_DEV_INFO_t              *dev_info, 
    rio_rt_alloc_mc_mask_in_t   *in_parms, 
    rio_rt_alloc_mc_mask_out_t  *out_parms
);

#define RT_ALLOC_MC_MASK(x) (RT_ALLOC_MC_MASK_0+x)

/**
 * @brief Updates an in_parms->rt structure to
 * deallocate a specified multicast mask.
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 *
 * Routing tables are updated to remove all references to the multicast mask.
 * After deallocation, the hardware state must be updated by
 * calling rio_rt_set_all() or rio_rt_set_changed().
*/

uint32_t rio_rt_dealloc_mc_mask(
    DAR_DEV_INFO_t                *dev_info, 
    rio_rt_dealloc_mc_mask_in_t   *in_parms, 
    rio_rt_dealloc_mc_mask_out_t  *out_parms
);

#define RT_DEALLOC_MC_MASK(x) (RT_DEALLOC_MC_MASK_0+x)

/**
 * @brief Updates an rio_rt_state_t structure to
 * change a routing table entry, and tracks changes.
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 *
 * NOTE: The Tsi57x family allows only a single domain
 * table entry to have a value of RIO_RTE_LVL_G0.
 * RIO_RTE_LVL_G1 through 255 are not allowed.
 * These limitations are enforced by this routine. 
 *
 * NOTE: The CPS family FORCES domain table entry 0 
 * to have a value of RIO_RTE_LVL_G0.
 * These limitations are enforced by this routine. 
 */

uint32_t rio_rt_change_rte(
    DAR_DEV_INFO_t           *dev_info, 
    rio_rt_change_rte_in_t   *in_parms, 
    rio_rt_change_rte_out_t  *out_parms
);

#define RT_CHANGE_RTE(x) (RT_CHANGE_RTE_0+x)

/**
 * @brief Updates an rio_rt_state_t structure to
 * change a routing table entry, and tracks changes.
 * 
 * @param[in] dev_info Structure identifying the device and driver
 * @param[inout] in_parms Input parameters, as described with typedef
 * @param[out] out_parms Output parameters, as described with typedef
 *
 * @return status of the function call
 * @retval 0 on success
 * @retval non-zero on error, additional information found in out_parms->imp_rc
 *
 * NOTE: The Tsi57x family allows only a single domain
 * table entry to have a value of RIO_RTE_LVL_G0.
 * RIO_RTE_LVL_G1 through 255 are not allowed.
 * These limitations are enforced by this routine. 
 *
 * NOTE: The CPS family FORCES domain table entry 0 
 * to have a value of RIO_RTE_LVL_G0.
 * These limitations are enforced by this routine. 
 */

uint32_t rio_rt_change_mc_mask(
    DAR_DEV_INFO_t               *dev_info, 
    rio_rt_change_mc_mask_in_t   *in_parms, 
    rio_rt_change_mc_mask_out_t  *out_parms
);

#define CHANGE_MC_MASK(x) (RT_CHANGE_MC_MASK_0+x)


#ifdef __cplusplus
}
#endif

#endif /* __IDT_ROUTING_TABLE_CONFIG_API_H__ */
