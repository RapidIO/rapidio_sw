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
#include "Tsi721.h"
#include "Tsi721_API.h"
#include "DAR_DB.h"
#include "DAR_DB_Private.h"
#include "RapidIO_Utilities_API.h"
#include "RapidIO_Port_Config_API.h"
#include "RapidIO_Routing_Table_API.h"
#include "RapidIO_Error_Management_API.h"
#include "IDT_DSF_DB_Private.h"

#include "string_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: TSI721_RIO_PW_CTL_PWC_MODE (Reliable port-write reception) is
* always disabled by this routine.
*/
uint32_t idt_tsi721_em_cfg_pw  ( DAR_DEV_INFO_t       *dev_info, 
                               rio_em_cfg_pw_in_t   *in_parms, 
                               rio_em_cfg_pw_out_t  *out_parms ) 
{
  uint32_t rc = RIO_ERR_INVALID_PARAMETER;
  uint32_t regData;
  uint32_t retx;

  out_parms->imp_rc = RIO_SUCCESS;

  if (in_parms->priority > 3) {
     out_parms->imp_rc = EM_CFG_PW(1);
     goto idt_tsi721_em_cfg_pw_exit;
  };
      
  // Configure destination ID for port writes.
  regData = ((uint32_t)(in_parms->port_write_destID)) << 16;
  if (tt_dev16 == in_parms->deviceID_tt) {
     regData |= TSI721_RIO_PW_TGT_ID_LRG_TRANS;
  } else {
     regData &= ~(TSI721_RIO_PW_TGT_ID_MSB_PW_ID | TSI721_RIO_PW_TGT_ID_LRG_TRANS);
  };

  rc = DARRegWrite( dev_info, TSI721_RIO_PW_TGT_ID, regData );
  if (RIO_SUCCESS != rc) {
     out_parms->imp_rc = EM_CFG_PW(2);
     goto idt_tsi721_em_cfg_pw_exit;
  };

  rc = DARRegRead( dev_info, TSI721_RIO_PW_TGT_ID, &regData );
  if (RIO_SUCCESS != rc) {
     out_parms->imp_rc = EM_CFG_PW(3);
     goto idt_tsi721_em_cfg_pw_exit;
  };

  out_parms->deviceID_tt = (regData & TSI721_RIO_PW_TGT_ID_LRG_TRANS)?tt_dev16:tt_dev8;
  out_parms->port_write_destID = (uint16_t)((regData & ( TSI721_RIO_PW_TGT_ID_PW_TGT_ID 
                                                     | TSI721_RIO_PW_TGT_ID_MSB_PW_ID )) >> 16);
  // Source ID for port writes is found in the TSI721_RIO_BASE_ID of the endpoint
  // Source ID for port-writes cannot be set by this routine.
  rc = DARRegRead( dev_info, TSI721_RIO_BASE_ID, &regData );
  if (RIO_SUCCESS != rc) {
     out_parms->imp_rc = EM_CFG_PW(3);
     goto idt_tsi721_em_cfg_pw_exit;
  };

  out_parms->srcID_valid      = true;
  out_parms->port_write_srcID = (tt_dev8 == out_parms->deviceID_tt)?((regData & TSI721_RIO_BASE_ID_BASE_ID)>> 16):
	                                                             (regData & TSI721_RIO_BASE_ID_LAR_BASE_ID);

  // Cannot configure port-write priority or CRF.
  
  out_parms->priority = 3;
  out_parms->CRF      = true;

  // Configure port-write re-transmission rate.
  // Assumption: it is better to choose a longer retransmission time than the value requested.
 
  regData = 0;
     retx = in_parms->port_write_re_tx * PORT_WRITE_RE_TX_NSEC;

  if (retx) {
     if ((retx <= 103000) && retx) {
	regData = TSI721_RIO_PW_CTL_PW_TIMER_103us;
     } else {
        if ((retx <= 205000) && retx) {
	       regData = TSI721_RIO_PW_CTL_PW_TIMER_205us;
        } else {
           if ((retx <= 410000) && retx) {
	          regData = TSI721_RIO_PW_CTL_PW_TIMER_410us;
           } else {
	          regData = TSI721_RIO_PW_CTL_PW_TIMER_820us;
	       };
		};
     };
  };

  rc = DARRegWrite( dev_info, TSI721_RIO_PW_CTL, regData );
  if (RIO_SUCCESS != rc) {
     out_parms->imp_rc = EM_CFG_PW(3);
     goto idt_tsi721_em_cfg_pw_exit;
  };

  rc = DARRegRead( dev_info, TSI721_RIO_PW_CTL, &regData );
  if (RIO_SUCCESS != rc) {
     out_parms->imp_rc = EM_CFG_PW(3);
     goto idt_tsi721_em_cfg_pw_exit;
  };

  switch (regData) {
     case 0:
         out_parms->port_write_re_tx = 0;
         break;
     case TSI721_RIO_PW_CTL_PW_TIMER_103us:
         out_parms->port_write_re_tx = 103000/PORT_WRITE_RE_TX_NSEC;
         break;
     case TSI721_RIO_PW_CTL_PW_TIMER_205us:
         out_parms->port_write_re_tx = 205000/PORT_WRITE_RE_TX_NSEC;
         break;
     case TSI721_RIO_PW_CTL_PW_TIMER_410us:
         out_parms->port_write_re_tx = 410000/PORT_WRITE_RE_TX_NSEC;
         break;
     case TSI721_RIO_PW_CTL_PW_TIMER_820us:
         out_parms->port_write_re_tx = 820000/PORT_WRITE_RE_TX_NSEC;
         break;
     default:
         out_parms->port_write_re_tx = regData;
         rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
         out_parms->imp_rc = EM_CFG_PW(9);
  }

  idt_tsi721_em_cfg_pw_exit:
  return rc; 
};

uint32_t tsi721_em_determine_notfn( DAR_DEV_INFO_t       *dev_info  , 
                                  rio_em_notfn_ctl_t   *notfn      ,
                                  uint8_t                 pnum      ,
                                  uint32_t               *imp_rc    ) 
{
	if (NULL != dev_info)
		*imp_rc = pnum + *(int*)notfn;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
} 

uint32_t idt_tsi721_em_cfg_set  ( DAR_DEV_INFO_t        *dev_info, 
                                rio_em_cfg_set_in_t   *in_parms, 
                                rio_em_cfg_set_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
};

uint32_t idt_tsi721_em_cfg_get  ( DAR_DEV_INFO_t        *dev_info, 
                                rio_em_cfg_get_in_t   *in_parms, 
                                rio_em_cfg_get_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->port_num;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
};

uint32_t idt_tsi721_em_dev_rpt_ctl  ( DAR_DEV_INFO_t            *dev_info, 
                                    rio_em_dev_rpt_ctl_in_t   *in_parms, 
                                    rio_em_dev_rpt_ctl_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

    return RIO_SUCCESS;
}

uint32_t idt_tsi721_em_parse_pw  ( DAR_DEV_INFO_t         *dev_info, 
                                 rio_em_parse_pw_in_t   *in_parms, 
                                 rio_em_parse_pw_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->num_events;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
}

uint32_t idt_tsi721_em_get_int_stat  ( DAR_DEV_INFO_t             *dev_info, 
                                     rio_em_get_int_stat_in_t   *in_parms, 
                                     rio_em_get_int_stat_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->num_events;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
};

uint32_t idt_tsi721_em_get_pw_stat  ( DAR_DEV_INFO_t            *dev_info, 
                                    rio_em_get_pw_stat_in_t   *in_parms, 
                                    rio_em_get_pw_stat_out_t  *out_parms )
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->ptl.num_ports;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
};

uint32_t idt_tsi721_em_clr_events   ( DAR_DEV_INFO_t           *dev_info, 
                                    rio_em_clr_events_in_t   *in_parms, 
                                    rio_em_clr_events_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->num_events;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
};

uint32_t idt_tsi721_em_create_events( DAR_DEV_INFO_t              *dev_info, 
                                    rio_em_create_events_in_t   *in_parms, 
                                    rio_em_create_events_out_t  *out_parms ) 
{
	if (NULL != dev_info)
		out_parms->imp_rc = in_parms->num_events;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
};

#ifdef __cplusplus
}
#endif
