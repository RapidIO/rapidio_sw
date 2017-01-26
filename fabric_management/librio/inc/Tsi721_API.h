/*
*******************************************************************************
* Copyright (c) 2010 Integrated Device Technology, Inc.
*      All Rights Reserved
*
* Distribution of source code or binaries derived from this file is not
* permitted except as specifically allowed for in the Integrated Device
* Technology Software License agreement.  All copies of this source code
* modified or unmodified must retain this entire copyright notice and
* comment as is.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*
*******************************************************************************
*/

#ifndef __TSI721_API_H__
#define __TSI721_API_H__

#include <stdint.h>

#include "DAR_DB.h"
#include "DAR_DB_Private.h"
#include "DSF_DB_Private.h"
#include "RapidIO_Utilities_API.h"
#include "RapidIO_Port_Config_API.h"
#include "RapidIO_Routing_Table_API.h"
#include "RapidIO_Error_Management_API.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Routine to bind in all Tsi721 specific DAR support routines.
*/
uint32_t bind_tsi721_DAR_support( void );

/* Routine to bind in all Tsi721 specific Device Specific Function routines.
*/
uint32_t bind_tsi721_DSF_support( void );

/* All Tsi721 routines */

// Port configuration routines
uint32_t idt_tsi721_pc_get_config  ( DAR_DEV_INFO_t           *dev_info,
                                   rio_pc_get_config_in_t   *in_parms,
                                   rio_pc_get_config_out_t  *out_parms );
uint32_t idt_tsi721_pc_set_config  ( DAR_DEV_INFO_t           *dev_info,
                                   rio_pc_set_config_in_t   *in_parms,
                                   rio_pc_set_config_out_t  *out_parms );
uint32_t idt_tsi721_pc_get_status  ( DAR_DEV_INFO_t         *dev_info,
                                 rio_pc_get_status_in_t   *in_parms,
                                 rio_pc_get_status_out_t  *out_parms );
uint32_t idt_tsi721_pc_reset_port  ( DAR_DEV_INFO_t          *dev_info,
                                   rio_pc_reset_port_in_t  *in_parms,
                                   rio_pc_reset_port_out_t *out_parms );
uint32_t idt_tsi721_pc_reset_link_partner(
    DAR_DEV_INFO_t                   *dev_info,
    rio_pc_reset_link_partner_in_t   *in_parms,
    rio_pc_reset_link_partner_out_t  *out_parms );
uint32_t idt_tsi721_pc_clr_errs  ( DAR_DEV_INFO_t       *dev_info,
                               rio_pc_clr_errs_in_t   *in_parms,
                               rio_pc_clr_errs_out_t  *out_parms );
uint32_t idt_tsi721_pc_secure_port  ( DAR_DEV_INFO_t          *dev_info,
                                  rio_pc_secure_port_in_t   *in_parms,
                                  rio_pc_secure_port_out_t  *out_parms );
uint32_t idt_tsi721_pc_dev_reset_config(
    DAR_DEV_INFO_t                 *dev_info,
    rio_pc_dev_reset_config_in_t   *in_parms,
    rio_pc_dev_reset_config_out_t  *out_parms );

// Event Management routines
//
uint32_t idt_tsi721_em_cfg_pw  ( DAR_DEV_INFO_t       *dev_info,
                               rio_em_cfg_pw_in_t   *in_parms,
                               rio_em_cfg_pw_out_t  *out_parms );
uint32_t idt_tsi721_em_cfg_set  ( DAR_DEV_INFO_t        *dev_info,
                                rio_em_cfg_set_in_t   *in_parms,
                                rio_em_cfg_set_out_t  *out_parms );
uint32_t idt_tsi721_em_cfg_get  ( DAR_DEV_INFO_t        *dev_info,
                                rio_em_cfg_get_in_t   *in_parms,
                                rio_em_cfg_get_out_t  *out_parms );
uint32_t idt_tsi721_em_dev_rpt_ctl  ( DAR_DEV_INFO_t            *dev_info,
                                    rio_em_dev_rpt_ctl_in_t   *in_parms,
                                    rio_em_dev_rpt_ctl_out_t  *out_parms );
uint32_t idt_tsi721_em_parse_pw  ( DAR_DEV_INFO_t         *dev_info,
                                 rio_em_parse_pw_in_t   *in_parms,
                                 rio_em_parse_pw_out_t  *out_parms );
uint32_t idt_tsi721_em_get_int_stat  ( DAR_DEV_INFO_t             *dev_info,
                                     rio_em_get_int_stat_in_t   *in_parms,
                                     rio_em_get_int_stat_out_t  *out_parms );
uint32_t idt_tsi721_em_get_pw_stat  ( DAR_DEV_INFO_t            *dev_info,
                                    rio_em_get_pw_stat_in_t   *in_parms,
                                    rio_em_get_pw_stat_out_t  *out_parms );
uint32_t idt_tsi721_em_clr_events   ( DAR_DEV_INFO_t           *dev_info,
                                    rio_em_clr_events_in_t   *in_parms,
                                    rio_em_clr_events_out_t  *out_parms );
uint32_t idt_tsi721_em_create_events( DAR_DEV_INFO_t              *dev_info,
                                    rio_em_create_events_in_t   *in_parms,
                                    rio_em_create_events_out_t  *out_parms );

// Statistics Counter routines
uint32_t idt_tsi721_sc_init_dev_ctrs (
         DAR_DEV_INFO_t                          *dev_info,
         rio_sc_init_dev_ctrs_in_t  *in_parms,
         rio_sc_init_dev_ctrs_out_t *out_parms);

uint32_t idt_tsi721_sc_read_ctrs(DAR_DEV_INFO_t  *dev_info,
                            rio_sc_read_ctrs_in_t    *in_parms,
                            rio_sc_read_ctrs_out_t   *out_parms);

#ifdef __cplusplus
}
#endif

#endif /* __TSI721_API_H__ */
