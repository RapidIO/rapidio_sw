/*
****************************************************************************
opyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

#ifndef __IDT_RXS_AP_H__
#define __IDT_RXS_API_H__

#include <DAR_DB.h>
#include <DAR_DB_Private.h>
#include <DAR_DevDriver.h>
#include <IDT_Statistics_Counter_API.h>
#include <IDT_DSF_DB_Private.h>
#include "IDT_Port_Config_API.h"
#include "DAR_Utilities.h"
#include "IDT_Error_Management_API.h"
#include "IDT_RXS2448.h"

#define RXS_MAX_PORTS			 23

#define RXS_NUM_PERF_CTRS        8


#ifdef __cplusplus
extern "C" {
#endif

	uint32_t idt_sc_init_xrs_ctrs(DAR_DEV_INFO_t      *dev_info,
		idt_sc_cfg_rxs_ctr_in_t  *in_parms,
		idt_sc_cfg_rxs_ctr_out_t *out_parms);

	uint32_t idt_sc_read_rxs_ctrs(DAR_DEV_INFO_t      *dev_info,
		idt_sc_read_ctrs_in_t    *in_parms,
		idt_sc_read_ctrs_out_t   *out_parms);

	uint32_t idt_sc_cfg_rxs_ctr(DAR_DEV_INFO_t        *dev_info,
		idt_sc_cfg_rxs_ctr_in_t  *in_parms,
		idt_sc_cfg_rxs_ctr_out_t *out_parms);

#ifdef __cplusplus
}
#endif

#endif /* __IDT_RXS_API_H__ */

