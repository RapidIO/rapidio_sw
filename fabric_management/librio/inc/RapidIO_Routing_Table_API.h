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

#ifndef __RAPIDIO_ROUTING_TABLE_API_H__
#define __RAPIDIO_ROUTING_TABLE_API_H__

#include <stdbool.h>

#include "rio_ecosystem.h"
#include "RapidIO_Device_Access_Routines_API.h"
#include "RXS_Routing_Table_API.h"

#ifdef __cplusplus
extern "C" {
#endif

/* All IDT specific routines for configuration of routing tables.
   The routines support the 'hierarchical' routing table programming model: 
   - 256 entry 'domain' routing table (most significant 8 bits of 16 bit destID)
   - 256 entry 'device' routing table (8 bit destIDs, least significant 8 bits of 16 bit destID)
  
   Domain routing table entry values can be one of the following:
      0x00-0x11 - Output port
      RIO_DSF_RT_USE_DEVICE_TABLE  - Use the device routing table
      RIO_DSF_RT_USE_DEFAULT_ROUTE - Use the default route
      RIO_DSF_RT_NO_ROUTE          - Discard packets with this destID
   * NOTE: For Tsi57x family, a maximum of one domain routing table entry can have
   *       a value of RIO_DSF_RT_USE_DEVICE_TABLE at any one time.

   Device routing table entry values can be one of the following:
      0x00-0x11 - Output port
      RIO_DSF_FIRST_MC_MASK up to RIO_DSF_MAX_MC_MASK - Use multicast mask
      RIO_DSF_RT_USE_DEFAULT_ROUTE - Use the default route
      RIO_DSF_RT_NO_ROUTE          - Discard packets with this destID
   
   There is also a 'default route' entry, as per the RapidIO standard routing
   control registers defined in Part 3 of the RapidIO standard.

   The routines in this file are based on the rio_rt_state_t structure, which
   contains the complete state of the routing table for a device/port.

   rio_rt_initialize() initializes an rio_rt_state_t structure.
   rio_rt_probe_all() updates an rio_rt_state_t structure based on current hardware state.
   rio_rt_set_all() updates the current hardware state to match the rio_rt_state_t structure.
   rio_rt_set_changed() updates the current hardware state of rio_rt_state_t structure entries 
                    "changed" by the activities of rio_rt_change_rte(), rio_rt_change_mc() 
                    and rio_rt_dealloc_mc_mask().
   Idt_rt_alloc_mc_mask() returns the first unallocated multicast mask in an rio_rt_state_t structure.
   rio_rt_dealloc_mc_mask() removes all references to a multicast mask in an rio_rt_state_t structure,
                            and deactivates/reinitializes the mask.
   rio_rt_change_rte() changes a routing table entry  in an rio_rt_state_t structure.  See NOTE below
                       regarding multicast interoperability.
   rio_rt_change_mc()  changes a multicast mask/destID entry in an rio_rt_state_t structure.

   NOTE: Multicast operates differently between the Tsi57x family and the CPS family.
         The Tsi57x family allows a single destID to be mapped to a multicast mask.  
         The CPS family allows multiple destIDs to be mappedd to a multicast mask.
         Code that supports both families must use rio_rt_alloc_mc_mask/rio_rt_dealloc_mc_mask/rio_rt_change_mc.
         For CPS switches, use rio_rt_change_rte to map more than one destID to a multicast mask.

   rio_rt_probe() indicates how hardware is currently routing packets with a designated deviceID. 

   This file is structured as:
   - Constant definitions
   - Input and output parameter structures for each routine
   - List of routines for routing table support
*/

#define RIO_DAR_RT_DEV_TABLE_SIZE                   256
#define RIO_DAR_RT_DOM_TABLE_SIZE                   256

/* For CPS and SPS devices */
/*#define RIO_DSF_RT_USE_DEVICE_TABLE                 0xDD
#define RIO_DSF_RT_USE_DEFAULT_ROUTE                0xDE
#define RIO_DSF_RT_NO_ROUTE                         0xDF

#define RIO_DSF_FIRST_MC_MASK                       0x40
#define RIO_DSF_MAX_MC_MASK                         0x28*/

/* For RXSs */
#define RIO_DSF_RT_USE_DEVICE_TABLE                 0x0200
#define RIO_DSF_RT_USE_DEFAULT_ROUTE                0x0300
#define RIO_DSF_RT_NO_ROUTE                         0x0301

#define RIO_DSF_FIRST_MC_MASK                       0x0100
#define RIO_DSF_MAX_MC_MASK                         0x00FF
#define RIO_DSF_BAD_MC_MASK                         (RIO_DSF_FIRST_MC_MASK+RIO_DSF_MAX_MC_MASK)

#define RIO_LAST_DEV8_DESTID  0xFF
#define RIO_LAST_DEV16_DESTID 0xFFFF

#define MC_MASK_IDX_FROM_ROUTE(x) (uint32_t)(((x >= RIO_DSF_FIRST_MC_MASK) && (x < RIO_DSF_BAD_MC_MASK))?(x - RIO_DSF_FIRST_MC_MASK):RIO_DSF_BAD_MC_MASK)
#define MC_MASK_ROUTE_FROM_IDX(x) (uint32_t)((x < RIO_DSF_MAX_MC_MASK)?(RIO_DSF_FIRST_MC_MASK + x):RIO_DSF_BAD_MC_MASK)

typedef enum { tt_dev8, tt_dev16 } tt_t;

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

extern char *rio_em_disc_reason_names[ (uint8_t)(rio_rt_disc_last) ];

#define DISC_REASON_STR(x) ((x < (uint8_t)(rio_rt_disc_last))?(rio_em_disc_reason_names[x]):"OORange")

typedef struct rio_rt_mc_info_t_TAG
{
    uint32_t mc_destID;// Destination ID of packets to be multicast
    tt_t   tt;       // Size of mc_destID, either 8 or 16 bit.
    uint32_t mc_mask;  // Bit vector of ports.
                     // Least significant bit is port 0, 
                     // Most significant bit is port 31.
    bool   in_use;   // true if this multicast mask and mc_destID are in use.
    bool   allocd;   // true if this mask has been allocated by 
                     // rio_rt_alloc_mc_mask.  Otherwise, should be ignored.
    bool   changed;  // true if the mc_destID or mc_mask value has changed.
} rio_rt_mc_info_t;

typedef struct rio_rt_uc_info_t_TAG
{
    uint32_t  rte_val;  // Routing table entry value.
    bool   changed;  // true if the rte_val value has changed.
} rio_rt_uc_info_t;

#define MC_MASK_ADD_PORT(m,p) (m |  (uint32_t)(1 << p))
#define MC_MASK_REM_PORT(m,p) (m & ~(uint32_t)(1 << p))
#define MC_MASK_GOT_PORT(m,p) (m &  (uint32_t)(1 << p))

typedef struct rio_rt_state_t_TAG
{
    uint32_t                default_route;             // The 'default route' for ports routed using RIO_DAR_RT_USE_DEFAULT_ROUTE
    rio_rt_uc_info_t      dev_table[RIO_DAR_RT_DEV_TABLE_SIZE]; // Encoded routing table value, should never be RIO_DAR_RT_USE_DEVICE_TABLE
    rio_rt_uc_info_t      dom_table[RIO_DAR_RT_DOM_TABLE_SIZE]; // Encoded routing table value read, should never be DAR_RT_FIRST_MC_MASK
    rio_rt_mc_info_t      mc_masks[RIO_DSF_MAX_MC_MASK];
} rio_rt_state_t;

typedef struct rio_rt_initialize_in_t_TAG
{
    uint8_t     set_on_port; // Must be a valid port number, or RIO_ALL_PORTS
                           // Note that when RIO_ALL_PORTS is specified, 
                           //   this function also clears all multicast masks and removes all
                           //   associations between multicast masks and ports.

    uint32_t    default_route; // Routing control for RIO_DSF_RT_DEFAULT_ROUTE routing table value.
                             //    Must be a valid port number, or RIO_DSF_RT_NO_ROUTE

    uint32_t    default_route_table_port; // Select the default routing for every destination ID in the routing table
                                        // Can be one of: a valid port number, RIO_DSF_RT_NO_ROUTE, or
                                        //   RIO_DSF_RT_USE_DEFAULT_ROUTE
    bool      update_hw;  // true : Update hardware state
                          // false: Do not update hardware state
    rio_rt_state_t *rt;   // Optionally provide a pointer to an rio_rt_state_t structure.
                          //   If provided, the structure is initialized to match the requested initial routing table.
} rio_rt_initialize_in_t;

typedef struct rio_rt_initialize_out_t_TAG
{
    uint32_t imp_rc;  /* Implementation specific failure information */

} rio_rt_initialize_out_t;

typedef struct rio_rt_probe_in_t_TAG
{
    uint8_t  probe_on_port; // Must be a valid port number, or RIO_ALL_PORTS
                          // RIO_ALL_PORTS probes the global routing table.
    tt_t   tt;            // DestID size (8 bit or 16 bit)
    uint16_t destID;        // Check routing for specified device ID.
    rio_rt_state_t *rt;   // A pointer to an rio_rt_state_t structure.
                          //   This structure is used to determine the route.
} rio_rt_probe_in_t;

typedef struct rio_rt_probe_out_t_TAG
{
    /* Output fields necessary for RIO_DAR_RT_PROBE function
    */
    uint32_t imp_rc;  /* Implementation specific failure information */
    bool   valid_route;  // true : packets will exit the port as 
                         //        defined by the routing_table_value.
                         // false: Packets will be discarded as 
                         //        indicated by reason_for_discard.
    uint32_t  routing_table_value; // Encoded routing table value read
    uint8_t  default_route;       // When routing_table_value is 
                                //    RIO_DSF_RT_USE_DEFAULT_ROUTE,
                                //    this field contains the value of 
				//    the default route register.
    bool  filter_function_active;   // If true, packets that
                                    //   match the FILTER MASK will be
                                    //   dropped.
    bool  trace_function_active;    // If true, packets that
                                    //   match the TRACE MASK will be
                                    //   copied to the trace port.
    bool  time_to_live_active;  // If true, packets buffered for longer than
                                //   the time-to-live period may be discarded.
    bool mcast_ports[RIO_MAX_PORTS];
                                // True if packet will be multicast to this
                                //   port, false if not.
                                // For completeness, will return true for
                                //   the port which was queried!
                                // Only valid if the routing_table_value
                                //   indicates a multicast mask.
    rio_rt_disc_reason_t
          reason_for_discard;   // Encoding for the reason that packets will be
                                //   discarded.
                                // Only valid if valid_route is false.
} rio_rt_probe_out_t;


typedef struct rio_rt_probe_all_in_t_TAG
{
    uint8_t           probe_on_port; // Must be a valid port number, or RIO_ALL_PORTS.
                                   // RIO_ALL_PORTS probes the global routing table, which may not be consistent
                                   // with per-port routing tables.  No warning is given of inconsistency.
    rio_rt_state_t *rt; // Pointer to routing table state structure.
} rio_rt_probe_all_in_t;

typedef struct rio_rt_probe_all_out_t_TAG
{
    uint32_t imp_rc;  // Implementation specific failure information 
} rio_rt_probe_all_out_t;

typedef struct rio_rt_set_all_in_t_TAG
{
    uint8_t  set_on_port; // A valid port number, or RIO_ALL_PORTS.
						// RIO_ALL_PORTS selects the global routing table
    rio_rt_state_t *rt; // Pointer to routing table state structure.
                        //   The state structure could be initialized using rio_rt_probe_all,
                        //   modified using rio_rt_change_rte, rio_rt_alloc_mc_mask, 
						//   rio_rt_dealloc_mc_mask, or rio_rt_change_mc_mask, and then applied 
						//   to hardware using rio_rt_set_all.
} rio_rt_set_all_in_t;

typedef struct rio_rt_set_all_out_t_TAG
{
    uint32_t imp_rc;  // Implementation specific failure information 
} rio_rt_set_all_out_t;

typedef rio_rt_set_all_in_t  rio_rt_set_changed_in_t  ;
typedef rio_rt_set_all_out_t rio_rt_set_changed_out_t ;

typedef struct rio_rt_alloc_mc_mask_in_t_TAG
{
    rio_rt_state_t *rt      ; // Pointer to routing table state structure 
} rio_rt_alloc_mc_mask_in_t;

typedef struct rio_rt_alloc_mc_mask_out_t_TAG
{
    uint32_t imp_rc     ; // Implementation specific failure information 
    uint32_t mc_mask_rte; // Routing table value which selects the allocated multicast mask.
                        //   If no free multicast masks exist, set to RIO_DSF_BAD_MC_MASK.
} rio_rt_alloc_mc_mask_out_t;

typedef struct rio_rt_dealloc_mc_mask_in_t_TAG
{
    uint32_t        mc_mask_rte; // Multicast mask routing value to be removed from
                                 //    the routing table state pointed to by "rt".
                                 // The multicast mask is also cleared to 0 by this 
                                 //    routine.
    rio_rt_state_t *rt;          // Pointer to routing table state structure to be updated
} rio_rt_dealloc_mc_mask_in_t;

typedef struct rio_rt_dealloc_mc_mask_out_t_TAG 
{
    uint32_t imp_rc     ; // Implementation specific failure information 
} rio_rt_dealloc_mc_mask_out_t;

typedef struct rio_rt_change_rte_in_t_TAG
{
    bool            dom_entry;  // true  if domain routing table entry is being updated 
                                // false if device routing table entry is being update
    uint8_t           idx;        // Index of routing table entry to be updated
    uint32_t          rte_value;  // Value for the routing table entry
                                //  - Note that if the requested routing table entry
                                //    matches the routing table entry value in *rt,
                                //    the routing table entry status is "no change"
    rio_rt_state_t *rt;         // Pointer to routing table state structure to be updated
} rio_rt_change_rte_in_t;

typedef struct rio_rt_change_rte_out_t_TAG 
{
    uint32_t imp_rc     ; // Implementation specific failure information 
} rio_rt_change_rte_out_t;

typedef struct rio_rt_change_mc_mask_in_t_TAG
{
    uint32_t            mc_mask_rte; // Multicast mask routing value which identifies the
                                  //    mask to be modified.                          
    rio_rt_mc_info_t mc_info;     // Multicast information to be assigned to associated multicast entry
    rio_rt_state_t  *rt;          // Pointer to routing table state structure to be updated
} rio_rt_change_mc_mask_in_t;

typedef struct rio_rt_change_mc_mask_out_t_TAG 
{
    uint32_t imp_rc     ; // Implementation specific failure information 
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

/* initializes the routing table hardware and/or routing table state structure.
*/

#define RT_INITIALIZE(x) (RT_INITIALIZE_0+x)

uint32_t rio_rt_initialize(
    DAR_DEV_INFO_t           *dev_info,
    rio_rt_initialize_in_t   *in_parms,
    rio_rt_initialize_out_t  *out_parms
);

/* This function probes the hardware status of a routing table entry for 
 *   the specified port and destination ID
*/

#define RT_PROBE(x) (RT_PROBE_0+x)

uint32_t rio_rt_probe(
    DAR_DEV_INFO_t      *dev_info,
    rio_rt_probe_in_t   *in_parms,
    rio_rt_probe_out_t  *out_parms
);

/* This function returns the complete hardware state of packet routing
 * in a routing table state structure.
 *
 * The routing table hardware must be initialized using rio_rt_initialize() 
 *    before calling this routine.
*/

#define RT_PROBE_ALL(x) (RT_PROBE_ALL_0+x)

uint32_t rio_rt_probe_all(
    DAR_DEV_INFO_t          *dev_info,
    rio_rt_probe_all_in_t   *in_parms,
    rio_rt_probe_all_out_t  *out_parms
);

/* This function sets the routing table hardware to match every entry
 *    in the routing table state structure. 
 * After rio_rt_set_all is called, no entries are marked as changed in
 *    the routing table state structure.
*/

#define RT_SET_ALL(x) (RT_SET_ALL_0+x)

uint32_t rio_rt_set_all (
    DAR_DEV_INFO_t        *dev_info, 
    rio_rt_set_all_in_t   *in_parms, 
    rio_rt_set_all_out_t  *out_parms
);

/* This function sets the the routing table hardware to match every entry
 *    that has been changed in the routing table state structure. 
 * Changes must be made using rio_rt_alloc_mc_mask, rio_rt_deallocate_mc_mask,
 *    rio_rt_change_rte, and rio_rt_change_mc.
 * After rio_rt_set_changed is called, no entries are marked as changed in
 *    the routing table state structure.
*/

#define RT_SET_CHANGED(x) (RT_SET_CHANGED_0+x)

uint32_t rio_rt_set_changed (
    DAR_DEV_INFO_t            *dev_info, 
    rio_rt_set_changed_in_t   *in_parms, 
    rio_rt_set_changed_out_t  *out_parms
);

/* This function updates an rio_rt_state_t structure to
 * find the first previously unused multicast mask.  
 * Can be called consecutively to allocate multiple 
 * multicast masks.
 */

#define RT_ALLOC_MC_MASK(x) (RT_ALLOC_MC_MASK_0+x)

uint32_t rio_rt_alloc_mc_mask(
    DAR_DEV_INFO_t              *dev_info, 
    rio_rt_alloc_mc_mask_in_t   *in_parms, 
    rio_rt_alloc_mc_mask_out_t  *out_parms
);

/* This function updates an rio_rt_state_t structure to
 * deallocate a specified multicast mask.  Routing tables
 * are updated to remove all references to the multicast mask.
 * After deallocation, the hardware state must be updated by
 * calling rio_rt_set_all() or rio_rt_set_changed().
 */

#define RT_DEALLOC_MC_MASK(x) (RT_DEALLOC_MC_MASK_0+x)

uint32_t rio_rt_dealloc_mc_mask(
    DAR_DEV_INFO_t                *dev_info, 
    rio_rt_dealloc_mc_mask_in_t   *in_parms, 
    rio_rt_dealloc_mc_mask_out_t  *out_parms
);

/* This function updates an rio_rt_state_t structure to
 * change a routing table entry, and tracks changes.
 *
 * NOTE: The Tsi57x family allows only a single domain
 * table entry to have a value of RIO_DSF_RT_USE_DEVICE_TABLE.
 * This limitation is supported by this routine. 
 *
 * NOTE: The CPS family FORCES domain table entry 0 
 * to have a value of RIO_DSF_RT_USE_DEVICE_TABLE.
 * This limitation is supported by this routine. 
 */

#define RT_CHANGE_RTE(x) (RT_CHANGE_RTE_0+x)

uint32_t rio_rt_change_rte(
    DAR_DEV_INFO_t           *dev_info, 
    rio_rt_change_rte_in_t   *in_parms, 
    rio_rt_change_rte_out_t  *out_parms
);

/* This function updates an rio_rt_state_t structure to
 * change a multicast mask value, and tracks changes.
 */

#define CHANGE_MC_MASK(x) (RT_CHANGE_MC_MASK_0+x)

uint32_t rio_rt_change_mc_mask(
    DAR_DEV_INFO_t               *dev_info, 
    rio_rt_change_mc_mask_in_t   *in_parms, 
    rio_rt_change_mc_mask_out_t  *out_parms
);

#ifdef __cplusplus
}
#endif

#endif /* __RAPIDIO_ROUTING_TABLE_API_H__ */
