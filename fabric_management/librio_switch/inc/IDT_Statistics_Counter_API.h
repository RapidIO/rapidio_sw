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

#ifndef __IDT_STATISTICS_COUNTER_API_H__
#define __IDT_STATISTICS_COUNTER_API_H__

#include <IDT_Common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions of parameter structures for IDT Port Configuration routines.

   Generic structure which contains the parameters which describe the
   configuration of a port.
*/
#define IDT_MAX_SC 32

typedef enum idt_sc_ctr_t_TAG
{
    idt_sc_disabled,        // The counter is disabled.
    idt_sc_enabled,         // The counter is enabled.
    idt_sc_uc_req_pkts,     // Tsi57x Unicast request packets
                            //   Excludes response packets, maintenance packets,
			    //   maintenance packets with hop count of 0,
			    //   and packets which are multicast.
    idt_sc_uc_pkts,         // Tsi57x Unicast packets.
                            //   Excludes all packets which are multicast.
    idt_sc_retries,         // Count retry control symbols              
    idt_sc_all_cs,          // Excludes retries, and Status+NOP control symbols
    idt_sc_uc_4b_data,      // Count of multiple of words (4 bytes) of packet data
                            //    for unicast packets.  Excludes multicast packets.
    idt_sc_mc_pkts,         // Count of multicast packets.
                            //   Excludes all packets which are unicast.  
    idt_sc_mecs,            // Count of Multicast Event Control Symbols         
    idt_sc_mc_4b_data,      // Count of multiple of words (4 bytes) of packet data
                            //    for multicast packets.  Excludes unicast packets.
    idt_sc_pa,              // CPS Packet acknowledgements count (TX or RX)
    idt_sc_pkt,             // CPS Packets count (TX or RX)
    idt_sc_pna,             // CPS Packet negative acknowledgements count (RX only)
    idt_sc_cpb,             // CPS Packets from CrossPoint Buffer count (!srio, RX)
    idt_sc_pkt_drop,        // CPS Packets dropped count (TX or RX)
    idt_sc_pkt_drop_ttl,    // CPS RX Packets dropped count due to TTL (TX only)
    idt_sc_last             // Last index for enumerated type
} idt_sc_ctr_t;

extern char *sc_names[(UINT8)(idt_sc_last)+2];
#define SC_NAME(x) ((x<=idt_sc_last)?sc_names[x]:sc_names[(UINT8)(idt_sc_last)+1])

#define DIR_TX TRUE
#define DIR_RX !DIR_TX
#define DIR_SRIO TRUE
#define DIR_FAB  !DIR_SRIO

typedef struct idt_sc_ctr_val_t_TAG
{
    long long    total;    // Accumulated counter value since counter was 
                           //   enabled/configured
    UINT32       last_inc; // Value the counter increased since previous read.
    idt_sc_ctr_t sc;       // What is being counted
                           //    May be modified by device specific configuration routines,
			   //    as counters are configured/enabled/disabled
			   //    The fields "total" and "last_inc" are 0 when sc == idt_sc_dir
    BOOL         tx;       // TRUE : transmitted "sc" are being counted.
                           // FALSE: received    "sc" are being counted.
    BOOL         srio;     // TRUE : Counter type reflects information on the RapidIO interface
                           // FALSE: Counter type reflects information on the internal fabric interface
} idt_sc_ctr_val_t;

#define INIT_IDT_SC_CTR_VAL {0, 0, idt_sc_disabled, FALSE, TRUE} 

typedef struct idt_sc_p_ctrs_val_t_TAG
{
    UINT8        pnum;      // Port number for these counters
    UINT8        ctrs_cnt;  // Number of valid entries in ctrs
                            //    Device specific.
    idt_sc_ctr_val_t ctrs[IDT_MAX_SC];  // Counter values for the device
} idt_sc_p_ctrs_val_t;

typedef struct idt_sc_dev_ctrs_t_TAG
{
    UINT8                num_p_ctrs;    // Number of allocated entries in p_ctrs[], 
                                        //    Maximum value is IDT_MAX_PORTS
	UINT8                valid_p_ctrs;  // Number of valid entries in p_ctrs[],
	                                    //    Maximum value is num_p_ctrs;
	                                    // Initialized by idt_sc_init_dev_ctrs()...
    idt_sc_p_ctrs_val_t *p_ctrs;        // Location of performance counters structure array
} idt_sc_dev_ctrs_t;

typedef struct idt_sc_init_dev_ctrs_in_t_TAG
{
   struct DAR_ptl       ptl;       // Port list
   idt_sc_dev_ctrs_t   *dev_ctrs;  // Device performance counters state
} idt_sc_init_dev_ctrs_in_t;

typedef struct idt_sc_init_dev_ctrs_out_t_TAG 
{
   UINT32      imp_rc;     // Implementation specific return code information.
} idt_sc_init_dev_ctrs_out_t;

typedef struct idt_sc_read_ctrs_in_t_TAG
{
  struct DAR_ptl        ptl;       // Port list
   idt_sc_dev_ctrs_t   *dev_ctrs;  // Device performance counters.
} idt_sc_read_ctrs_in_t;

typedef struct idt_sc_read_ctrs_out_t_TAG
{
   UINT32      imp_rc;     // Implementation specific return code information.
} idt_sc_read_ctrs_out_t;

typedef struct idt_sc_cfg_tsi57x_ctr_in_t_TAG 
{
   struct DAR_ptl         ptl;        // Port list
   UINT8                  ctr_idx;    // Index of the Tsi57x counter to be configured.  Range 0-5.
   UINT8                  prio_mask;  // Priority of packets to be counted.  Not used for control symbol counters.
                                      //    Uses IDT_SC_TSI57X_PRIO_MASK_x constant definitions.
   BOOL                   tx;         // Determines direction for the counter.  !tx = rx.
   idt_sc_ctr_t           ctr_type;   // Valid counter type, valid range from idt_sc_disabled to idt_sc_uc_4b_data
   idt_sc_dev_ctrs_t     *dev_ctrs;   // Device counters data type, initialized by idt_sc_init_dev_ctrs
} idt_sc_cfg_tsi57x_ctr_in_t;

typedef struct idt_sc_cfg_tsi57x_ctr_out_t_TAG
{
   UINT32      imp_rc;     // Implementation specific return code information.
} idt_sc_cfg_tsi57x_ctr_out_t;

typedef struct idt_sc_cfg_cps_ctrs_in_t_TAG
{
   struct DAR_ptl         ptl;       // Port list
   BOOL                   enable_ctrs; // TRUE - enable all counters, FALSE - disable all counters
   idt_sc_dev_ctrs_t     *dev_ctrs;    // Device counters data type, initialized by idt_sc_init_dev_ctrs
} idt_sc_cfg_cps_ctrs_in_t;

typedef struct idt_sc_cfg_cps_ctrs_out_t_TAG
{
   UINT32      imp_rc;     // Implementation specific return code information.
} idt_sc_cfg_cps_ctrs_out_t;

typedef struct idt_sc_cfg_cps_trace_in_t_TAG
{
  struct DAR_ptl          ptl;       // Port list
   UINT8                  trace_idx;   // Index of the CPS trace/filter counter to be configured.  Range 0-3.
   UINT32                 pkt_mask[5]; // Mask of packet fields to be checked.
   UINT32                 pkt_val[5];  // Packet field values to match.
   BOOL                   count;       // Count packets which are traced or dropped.      
                                       //    If FALSE, no action occurs. 
				       //    If TRUE , all counters will be enabled on the port.
   BOOL                   trace;       // Send a copy of this packet to the trace port.   
   BOOL                   drop;        // Drop this packet.  This is independent of trace behavior.
   idt_sc_dev_ctrs_t     *dev_ctrs;    // Device counters data type, initialized by idt_sc_init_dev_ctrs
                                       //    May be set to NULL if counters are not of interest.
} idt_sc_cfg_cps_trace_in_t;

typedef struct idt_sc_cfg_cps_trace_out_t_TAG
{
   UINT32      imp_rc;     // Implementation specific return code information.
} idt_sc_cfg_cps_trace_out_t;



// Implementation specific return codes for Statistics Counter routines

#define SC_INIT_DEV_CTRS_0    (DAR_FIRST_IMP_SPEC_ERROR+0x0100)
#define SC_READ_CTRS_0        (DAR_FIRST_IMP_SPEC_ERROR+0x0200)
#define SC_CFG_TSI57X_CTR_0   (DAR_FIRST_IMP_SPEC_ERROR+0x0300)
#define SC_CFG_CPS_CTRS_0     (DAR_FIRST_IMP_SPEC_ERROR+0x0400)
#define SC_CFG_CPS_TRACE_0    (DAR_FIRST_IMP_SPEC_ERROR+0x0500)

/* The following functions are implemented to support the above structures
   Refer to the above structures for the implementation detail 
*/
/* This function initializes an idt_sc_dev_ctrs structure based
 * on input parameters and the current hardware state.
*/
#define SC_INIT_DEV_CTRS(x) (SC_INIT_DEV_CTRS_0+x)

STATUS idt_sc_init_dev_ctrs (
    DAR_DEV_INFO_t             *dev_info,
    idt_sc_init_dev_ctrs_in_t  *in_parms,
    idt_sc_init_dev_ctrs_out_t *out_parms
);

/* Reads enabled/configured counters on selected ports   
*/
#define SC_READ_CTRS(x) (SC_READ_CTRS_0+x)

STATUS idt_sc_read_ctrs(
    DAR_DEV_INFO_t           *dev_info,
    idt_sc_read_ctrs_in_t    *in_parms,
    idt_sc_read_ctrs_out_t   *out_parms
);

/* Configure counters on selected ports of a 
 * Tsi device.
 */

#define SC_CFG_TSI57X_CTR(x) (SC_CFG_TSI57X_CTR_0+x)
extern STATUS idt_sc_cfg_tsi57x_ctr (
    DAR_DEV_INFO_t              *dev_info,
    idt_sc_cfg_tsi57x_ctr_in_t  *in_parms,
    idt_sc_cfg_tsi57x_ctr_out_t *out_parms
);

/* Configure counters on selected ports   
 *    of a CPS Gen2 device.
*/
#define SC_CFG_CPS_CTRS(x) (SC_CFG_CPS_CTRS_0+x)

extern STATUS idt_sc_cfg_cps_ctrs(
    DAR_DEV_INFO_t            *dev_info,
    idt_sc_cfg_cps_ctrs_in_t  *in_parms,
    idt_sc_cfg_cps_ctrs_out_t *out_parms
);

#ifdef __cplusplus
}
#endif

#endif /* __IDT_STATISTICS_COUNTER_API_H__ */
