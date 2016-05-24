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
#include <emdrv_types.h>

#ifndef __EMDRV_H__
#define __EMDRV_H__

#ifdef __cplusplus
extern "C" {
#endif

// Implementation specific error return codes
#define EM_CFG_PW_0           (DAR_FIRST_IMP_SPEC_ERROR+0x2000)
#define EM_CFG_SET_0          (DAR_FIRST_IMP_SPEC_ERROR+0x2100)
#define EM_CFG_GET_0          (DAR_FIRST_IMP_SPEC_ERROR+0x2200)
#define EM_DEV_RPT_CTL_0      (DAR_FIRST_IMP_SPEC_ERROR+0x2300)
#define EM_PARSE_PW_0         (DAR_FIRST_IMP_SPEC_ERROR+0x2400)
#define EM_GET_INT_STAT_0     (DAR_FIRST_IMP_SPEC_ERROR+0x2500)
#define EM_GET_PW_STAT_0      (DAR_FIRST_IMP_SPEC_ERROR+0x2600)
#define EM_CLR_EVENTS_0       (DAR_FIRST_IMP_SPEC_ERROR+0x2700)
#define EM_CREATE_EVENTS_0    (DAR_FIRST_IMP_SPEC_ERROR+0x2800)

#define EM_FIRST_SUBROUTINE_0 (DAR_FIRST_IMP_SPEC_ERROR+0x200000)


// Routines to configure port write transmission, 
// and set/query specific event detection/reporting.

#define EM_CFG_PW(x) (EM_CFG_PW_0+x)

uint32_t em_cfg_pw  ( struct dev_info       *info, 
                        em_cfg_pw_in_t   *in_parms, 
                        em_cfg_pw_out_t  *out_parms );

#define EM_CFG_SET(x) (EM_CFG_SET_0+x)

uint32_t em_cfg_set  ( struct dev_info        *info, 
                         em_cfg_set_in_t   *in_parms, 
                         em_cfg_set_out_t  *out_parms );

#define EM_CFG_GET(x) (EM_CFG_GET_0+x)

uint32_t em_cfg_get  ( struct dev_info        *info, 
                         em_cfg_get_in_t   *in_parms, 
                         em_cfg_get_out_t  *out_parms );

// Routines to query and control port-write and interrupt
// reporting configuration for a port/device.

#define EM_DEV_RPT_CTL(x) (EM_DEV_RPT_CTL_0+x)

uint32_t em_dev_rpt_ctl  ( struct dev_info            *info, 
                             em_dev_rpt_ctl_in_t   *in_parms, 
                             em_dev_rpt_ctl_out_t  *out_parms );

// Routines to convert port-write contents into an event list,
// and to query a port/device and return a list of asserted events 
// which are reported via interrupt or port-write, 

#define EM_PARSE_PW(x) (EM_PARSE_PW_0+x)

uint32_t em_parse_pw  ( struct dev_info       *info, 
                        em_parse_pw_in_t   *in_parms, 
                        em_parse_pw_out_t  *out_parms );

#define EM_GET_INT_STAT(x) (EM_GET_INT_STAT_0+x)

uint32_t em_get_int_stat  ( struct dev_info             *info, 
                              em_get_int_stat_in_t   *in_parms, 
                              em_get_int_stat_out_t  *out_parms );

#define EM_GET_PW_STAT(x) (EM_GET_PW_STAT_0+x)

uint32_t em_get_pw_stat  ( struct dev_info       *info, 
                        em_get_pw_stat_in_t   *in_parms, 
                        em_get_pw_stat_out_t  *out_parms );

// Routine to clear events, and a routine to create events
// for software testing purposes.
//
// Note:  Clearing fatal errors assumes that all packets in
//        the affected port should be discarded, and that
//        ackIDs should be returned to 0.  This will be       
//        done without issuing any control symbols or packets.  
//        The state of the link partner must be made consistent
//        with the state of the affected port using separate 
//        routines found in IDT_Port_Configuration_API.h

#define EM_CLR_EVENTS(x) (EM_CLR_EVENTS_0+x)

uint32_t em_clr_events   ( struct dev_info           *info, 
                             em_clr_events_in_t   *in_parms, 
                             em_clr_events_out_t  *out_parms );

#define EM_CREATE_EVENTS(x)    (EM_CREATE_EVENTS_0+x)

uint32_t em_create_events( struct dev_info              *info, 
                             em_create_events_in_t   *in_parms, 
                             em_create_events_out_t  *out_parms );

struct em_driver {
uint32_t (* em_cfg_pw)(struct dev_info *info, 
                             em_cfg_pw_in_t *in_parms, 
                             em_cfg_pw_out_t *out_parms);
uint32_t (* em_cfg_set)(struct dev_info *info, 
                             em_cfg_set_in_t *in_parms, 
                             em_cfg_set_out_t *out_parms);
uint32_t (* em_cfg_get)(struct dev_info *info, 
                             em_cfg_get_in_t *in_parms, 
                             em_cfg_get_out_t *out_parms);
uint32_t (* em_dev_rpt_ctl)(struct dev_info *info, 
                             em_dev_rpt_ctl_in_t *in_parms, 
                             em_dev_rpt_ctl_out_t *out_parms);
uint32_t (* em_parse_pw)(struct dev_info *info, 
                             em_parse_pw_in_t *in_parms, 
                             em_parse_pw_out_t *out_parms);
uint32_t (* em_get_int_stat)(struct dev_info *info, 
                             em_get_int_stat_in_t *in_parms, 
                             em_get_int_stat_out_t *out_parms);
uint32_t (* em_get_pw_stat)(struct dev_info *info, 
                             em_get_pw_stat_in_t *in_parms, 
                             em_get_pw_stat_out_t *out_parms);
uint32_t (* em_get_int_stat)(struct dev_info *info, 
                             em_get_int_stat_in_t *in_parms, 
                             em_get_int_stat_out_t *out_parms);
uint32_t (* em_parse_pw)(struct dev_info *info, 
                             em_parse_pw_in_t *in_parms, 
                             em_parse_pw_out_t *out_parms);
uint32_t (* em_clr_events)(struct dev_info *info, 
                             em_clr_events_in_t *in_parms, 
                             em_clr_events_out_t *out_parms);
uint32_t (* em_create_events)( struct dev_info *info, 
                             em_create_events_in_t *in_parms, 
                             em_create_events_out_t *out_parms);
};

int override_emdrv(struct em_driver *emdrv);

#ifdef __cplusplus
}
#endif

#endif // __EMDRV_H__
