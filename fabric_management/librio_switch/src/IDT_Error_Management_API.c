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
#include <IDT_DSF_DB_Private.h>

char *idt_em_events_names[ idt_em_last ] = {
    "FLossOfSig"  ,
    "FPortErr"    ,
    "F2ManyReTx"  , 
    "F2ManyPNA"   ,
    "FErrRate"    ,
    "DropTTL"     , 
    "DropOnPurp"  ,
    "DropLogErr"  ,
    "ISigDet"     ,
    "IRstReq"     ,
    "IInitFail"   ,
    "AClrPwPnd"   , 
    "ANoEvent"  
};

char *idt_em_notfn_names[ idt_em_notfn_last ] = {
     "NtfnNone"   ,
     "NtfnInt"    ,
     "NtfnPW"     ,
     "NtfnBoth"   ,
     "NtfnNoChg"  
};

char *idt_em_detect_names[ idt_em_detect_last] = {
     "DetOff"   ,
     "DetOn"    ,
     "DetNoChg" 
};

#define IDT_EM_REC_ERR_SET_LINK_TO       ((UINT32)(0x0001))
#define IDT_EM_REC_ERR_SET_CS_ACK_ILL    ((UINT32)(0x0002))
#define IDT_EM_REC_ERR_SET_DELIN_ERR     ((UINT32)(0x0004))
#define IDT_EM_REC_ERR_SET_PROT_ERR      ((UINT32)(0x0008))
#define IDT_EM_REC_ERR_SET_LR_ACKID_ILL  ((UINT32)(0x0010))
#define IDT_EM_REC_ERR_SET_PKT_ILL_SIZE  ((UINT32)(0x0020))
#define IDT_EM_REC_ERR_SET_PKT_CRC_ERR   ((UINT32)(0x0040))
#define IDT_EM_REC_ERR_SET_PKT_ILL_ACKID ((UINT32)(0x0080))
#define IDT_EM_REC_ERR_SET_CS_NOT_ACC    ((UINT32)(0x0100))
#define IDT_EM_REC_ERR_SET_CS_ILL_ID     ((UINT32)(0x0200))
#define IDT_EM_REC_ERR_SET_CS_CRC_ERR    ((UINT32)(0x0400))

typedef struct rec_err_n_spx_err_t_TAG
{
    UINT32 rec_err;
    UINT32 spx_err;
} rec_err_n_spx_err_t;

// Convert from standard bit mask encodings to consecutive bit encodings.

rec_err_n_spx_err_t rec_err_spx_err_table[] = {
   { IDT_EM_REC_ERR_SET_LINK_TO      , RIO_SPX_ERR_DET_LINK_TO      },
   { IDT_EM_REC_ERR_SET_CS_ACK_ILL   , RIO_SPX_ERR_DET_CS_ACK_ILL   },
   { IDT_EM_REC_ERR_SET_DELIN_ERR    , RIO_SPX_ERR_DET_DELIN_ERR    },
   { IDT_EM_REC_ERR_SET_PROT_ERR     , RIO_SPX_ERR_DET_PROT_ERR     },
   { IDT_EM_REC_ERR_SET_LR_ACKID_ILL , RIO_SPX_ERR_DET_LR_ACKID_ILL },
   { IDT_EM_REC_ERR_SET_PKT_ILL_SIZE , RIO_SPX_ERR_DET_PKT_ILL_SIZE },
   { IDT_EM_REC_ERR_SET_PKT_CRC_ERR  , RIO_SPX_ERR_DET_PKT_CRC_ERR  },
   { IDT_EM_REC_ERR_SET_PKT_ILL_ACKID, RIO_SPX_ERR_DET_PKT_ILL_ACKID},
   { IDT_EM_REC_ERR_SET_CS_NOT_ACC   , RIO_SPX_ERR_DET_CS_NOT_ACC   },
   { IDT_EM_REC_ERR_SET_CS_ILL_ID    , RIO_SPX_ERR_DET_CS_ILL_ID    },
   { IDT_EM_REC_ERR_SET_CS_CRC_ERR   , RIO_SPX_ERR_DET_CS_CRC_ERR   } 
};

// Encoding for RB, index = RB value
UINT32 rio_spx_err_rate_err_rb_vals[] = {
   RIO_SPX_ERR_RATE_ERR_RB_NONE     ,
   RIO_SPX_ERR_RATE_ERR_RB_1_MS     ,
   RIO_SPX_ERR_RATE_ERR_RB_10_MS    ,
   RIO_SPX_ERR_RATE_ERR_RB_100_MS   ,
   RIO_SPX_ERR_RATE_ERR_RB_1_SEC    ,
   RIO_SPX_ERR_RATE_ERR_RB_10_SEC   ,
   RIO_SPX_ERR_RATE_ERR_RB_100_SEC  ,
   RIO_SPX_ERR_RATE_ERR_RB_1000_SEC ,
   RIO_SPX_ERR_RATE_ERR_RB_10000_SEC 
};

#define RB_SHIFT_AMT 20

STATUS idt_em_compute_f_err_rate_info( UINT32 spx_rate_en,
		                       UINT32 spx_err_rate,
				       UINT32 spx_err_thresh,
				       UINT32 *info          )
{
   STATUS rc = RIO_ERR_INVALID_PARAMETER;
   UINT32 all_errs = 0, idx;
   BOOL   found_one = FALSE;

   *info = 0;

   for (idx = 0; idx < sizeof(rec_err_spx_err_table)/sizeof(rec_err_n_spx_err_t); idx++ ) {
      all_errs |= rec_err_spx_err_table[idx].spx_err;
      if (rec_err_spx_err_table[idx].spx_err & spx_rate_en) {
	 *info |= rec_err_spx_err_table[idx].rec_err;
      };
   };

   if (spx_rate_en & ~all_errs) {
      goto idt_em_compute_f_err_rate_info_exit;
   };

   *info |= (RIO_SPX_ERR_RATE_ERR_RR    & spx_err_rate  );
   *info |= (RIO_SPX_ERR_THRESH_ERR_RFT & spx_err_thresh);
   spx_err_rate &= RIO_SPX_ERR_RATE_ERR_RB;

   for (idx = 0; idx < sizeof(rio_spx_err_rate_err_rb_vals)/sizeof(UINT32); idx++ ) {
      if (rio_spx_err_rate_err_rb_vals[idx] == spx_err_rate) {
	 found_one = TRUE;
	 *info |= (idx << RB_SHIFT_AMT);
	 break;
      };
   };

   if (!found_one)
      goto idt_em_compute_f_err_rate_info_exit;

   rc = RIO_SUCCESS;

idt_em_compute_f_err_rate_info_exit:
   return rc;
};


STATUS idt_em_get_f_err_rate_info( UINT32  info          ,
		                   UINT32 *spx_rate_en   ,
		                   UINT32 *spx_err_rate  ,
				   UINT32 *spx_err_thresh )
{
   STATUS rc = RIO_ERR_INVALID_PARAMETER;
   UINT32 idx;

   *spx_rate_en    = 0;
   *spx_err_rate   = info & RIO_SPX_ERR_RATE_ERR_RR;
   *spx_err_thresh = info & RIO_SPX_ERR_THRESH_ERR_RFT;

   for (idx = 0; idx < sizeof(rec_err_spx_err_table)/sizeof(rec_err_n_spx_err_t); idx++ ) {
      if (rec_err_spx_err_table[idx].rec_err & info) {
	 *spx_rate_en |= rec_err_spx_err_table[idx].spx_err;
      };
   };

   info = (info & 0x00F00000) >> RB_SHIFT_AMT;
   if (info >= sizeof(rio_spx_err_rate_err_rb_vals)/sizeof(rio_spx_err_rate_err_rb_vals[0]))
      goto idt_em_compute_f_err_rate_info_exit;

   *spx_err_rate |= rio_spx_err_rate_err_rb_vals[info];

   rc = RIO_SUCCESS;

idt_em_compute_f_err_rate_info_exit:
   return rc;
};

// Routines to configure port write transmission, 
// and set/query specific event detection/reporting.

STATUS idt_em_cfg_pw  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_cfg_pw_in_t   *in_parms, 
                        idt_em_cfg_pw_out_t  *out_parms )
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_cfg_pw(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

STATUS idt_em_cfg_set  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_cfg_set_in_t   *in_parms, 
                        idt_em_cfg_set_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_cfg_set(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};


STATUS idt_em_cfg_get  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_cfg_get_in_t   *in_parms, 
                        idt_em_cfg_get_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_cfg_get(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};


// Routines to query and control port-write and interrupt
// reporting configuration for a port/device.

STATUS idt_em_dev_rpt_ctl  ( DAR_DEV_INFO_t      *dev_info, 
                       idt_em_dev_rpt_ctl_in_t   *in_parms, 
                       idt_em_dev_rpt_ctl_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_dev_rpt_ctl(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

// Routines to convert port-write contents into an event list,
// and to query a port/device and return a list of asserted events 
// which are reported via interrupt or port-write, 

STATUS idt_em_parse_pw  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_parse_pw_in_t   *in_parms, 
                        idt_em_parse_pw_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_parse_pw(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

STATUS idt_em_get_int_stat  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_get_int_stat_in_t   *in_parms, 
                        idt_em_get_int_stat_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_get_int_stat(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

STATUS idt_em_get_pw_stat  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_get_pw_stat_in_t   *in_parms, 
                        idt_em_get_pw_stat_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_get_pw_stat(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

// Routine to clear events, and a routine to create events
// for software testing purposes.

STATUS idt_em_clr_events  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_clr_events_in_t   *in_parms, 
                        idt_em_clr_events_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_clr_events(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

STATUS idt_em_create_events  ( DAR_DEV_INFO_t       *dev_info, 
                        idt_em_create_events_in_t   *in_parms, 
                        idt_em_create_events_out_t  *out_parms ) 
{ 
    STATUS rc =  DAR_DB_INVALID_HANDLE;

    NULL_CHECK

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_em_create_events(
                    dev_info, in_parms, out_parms
                 );
    };

    return rc;
};

