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

#include "DAR_DB_Private.h"
#include "DSF_DB_Private.h"
#include "CPS_API.h"
#include "RapidIO_Routing_Table_API.h"
#include "RapidIO_Port_Config_API.h"
#include "RapidIO_Error_Management_API.h"
#include "IDT_CPS_Common_Test.h"
#include "CPS1848.h"
#include "CPS1616.h"
#include "CPS_DeviceDriver.h"

#include "string_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EM_SET_EVENT_PW_0     (DAR_FIRST_IMP_SPEC_ERROR+0x12900)
#define EM_SET_EVENT_INT_0    (DAR_FIRST_IMP_SPEC_ERROR+0x12A00)
#define EM_EN_ERR_CTR_0       (DAR_FIRST_IMP_SPEC_ERROR+0x12B00)
#define EM_SET_EVENT_EN_0     (DAR_FIRST_IMP_SPEC_ERROR+0x12C00)
#define EM_UPDATE_RESET_0     (DAR_FIRST_IMP_SPEC_ERROR+0x12D00)
#define EM_DET_NOTFN_0        (DAR_FIRST_IMP_SPEC_ERROR+0x12E00)
#define EM_CREATE_RATE_0      (DAR_FIRST_IMP_SPEC_ERROR+0x12F00)

DSF_Handle_t cpsgen2_driver_handle;

uint32_t bind_CPS_DSF_support( void )
{
    IDT_DSF_DB_t idt_driver;
    
	IDT_DSF_init_driver( &idt_driver );
    idt_driver.dev_type = 0x0380;

    idt_driver.rio_pc_clr_errs           = IDT_CPS_pc_clr_errs;
    idt_driver.rio_pc_dev_reset_config   = IDT_CPS_pc_dev_reset_config;
    idt_driver.rio_pc_get_config         = IDT_CPS_pc_get_config;
    idt_driver.rio_pc_get_status         = IDT_CPS_pc_get_status;
    idt_driver.rio_pc_reset_link_partner = IDT_CPS_pc_reset_link_partner;
    idt_driver.rio_pc_reset_port         = IDT_CPS_pc_reset_port;
    idt_driver.rio_pc_secure_port        = IDT_CPS_pc_secure_port;
    idt_driver.rio_pc_set_config         = IDT_CPS_pc_set_config;
    idt_driver.rio_pc_probe              = default_rio_pc_probe;

    idt_driver.rio_rt_initialize         = IDT_CPS_rt_initialize;
    idt_driver.rio_rt_probe              = IDT_CPS_rt_probe;
    idt_driver.rio_rt_probe_all          = IDT_CPS_rt_probe_all;
    idt_driver.rio_rt_set_all            = IDT_CPS_rt_set_all;
    idt_driver.rio_rt_set_changed     = IDT_CPS_rt_set_changed;
    idt_driver.rio_rt_alloc_mc_mask   = RIO_DSF_rt_alloc_mc_mask;
    idt_driver.rio_rt_dealloc_mc_mask = RIO_DSF_rt_dealloc_mc_mask;
    idt_driver.rio_rt_change_rte      = IDT_CPS_rt_change_rte;
    idt_driver.rio_rt_change_mc_mask  = IDT_CPS_rt_change_mc_mask;

    idt_driver.rio_em_cfg_pw       = IDT_CPS_em_cfg_pw       ;
    idt_driver.rio_em_cfg_set      = IDT_CPS_em_cfg_set      ;
    idt_driver.rio_em_cfg_get      = IDT_CPS_em_cfg_get      ;
    idt_driver.rio_em_dev_rpt_ctl  = IDT_CPS_em_dev_rpt_ctl  ;
    idt_driver.rio_em_parse_pw     = IDT_CPS_em_parse_pw     ;
    idt_driver.rio_em_get_int_stat = IDT_CPS_em_get_int_stat ;
    idt_driver.rio_em_get_pw_stat  = IDT_CPS_em_get_pw_stat  ;
    idt_driver.rio_em_clr_events   = IDT_CPS_em_clr_events   ;
    idt_driver.rio_em_create_events= IDT_CPS_em_create_events;

	idt_driver.rio_sc_init_dev_ctrs= idt_cps_sc_init_dev_ctrs;
    idt_driver.rio_sc_read_ctrs    = idt_cps_sc_read_ctrs    ;

    IDT_DSF_bind_driver( &idt_driver, &cpsgen2_driver_handle );

    return RIO_SUCCESS;
}

#ifdef __cplusplus
}
#endif
