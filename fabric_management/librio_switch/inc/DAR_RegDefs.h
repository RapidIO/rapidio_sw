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

#ifndef __DAR_REGDEFS_H__
#define __DAR_REGDEFS_H__

/* DAR_RegDefs.h contains definitons for all RapidIO Standard Registers
   WORK IN PROGRESS - not complete yet!
*/
#include <DAR_Basic_Defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Offsets, field masks and value definitions for Device CARs and CSRs
*/
#define RIO_DEV_ID_CAR            (0x000000)     /* Device Identity CAR
                                                 */
#define RIO_DEV_INFO_CAR          (0x000004)     /* Device Information CAR
                                                 */
#define RIO_ASBLY_ID_CAR          (0x000008)     /* Assembly Identify CAR
                                                 */
#define RIO_ASBLY_INFO_CAR        (0x00000c)     /* Assembly Information CAR
                                                 */
#define RIO_PROC_ELEM_FEAT_CAR    (0x000010)     /* Processing Element Features 
                                                        CAR
                                                 */
#define RIO_SWITCH_PORT_INF_CAR   (0x000014)     /* Switch Port Information CAR 
                                                        (switch and multi-port 
                                                         endpoints)
                                                 */
#define RIO_SRC_OPS_CAR           (0x000018)     /* Source Operations CAR
                                                 */
#define RIO_DST_OPS_CAR           (0x00001C)     /* Destination Operations CAR 
                                                        (endpoints only)
                                                 */
#define RIO_MC_FEAT_CAR           (0x000030)     /* Switch Multicast Support
                                                        CAR
                                                        (switch only)
                                                 */
#define RIO_RT_INFO_CAR           (0x000034)     /* Switch Route Table 
                                                        Destination ID Size 
                                                        Limit CAR 
                                                        (switch only)
                                                 */
#define RIO_STD_SW_MC_INFO_CSR    (0x000038)     /* Switch Multicast 
                                                        Information CAR
                                                        (switch only)
                                                 */
#define RIO_PE_LLAYER_CONTROL_CSR (0x00004C)     /* Processing Element Logical 
                                                        Layer Control CSR
                                                 */
#define RIO_LCS_BASE_ADDR_0_CSR   (0x000058)     /* Local Configuration Space 
                                                        Base Address 0 CSR
                                                 */
#define RIO_LCS_BASE_ADDR_1_CSR   (0x00005C)     /* Local Configuration Space 
                                                        Base Address 1 CSR
                                                 */
#define RIO_BASE_DEVICE_ID_CSR    (0x000060)     /* Base Device ID CSR
                                                 */
#define RIO_HOST_BASE_DEVICE_ID_LOCK_CSR  (0x000068)
                                                 /* HostBase Device Lock ID CSR
                                                 */
#define RIO_COMPONENT_TAG_CSR     (0x00006C)     /* Component Tag CSR
                                                 */
#define RIO_STD_RTE_CONF_DESTID_SEL_CSR (0x000070)
                                                 /* Std Route Configuration
                                                        Destination ID Select
                                                        CSR
                                                 */
#define RIO_STD_RTE_CONF_PORT_SEL_CSR (0x000074) /* Standard Route Configuratio 
                                                        Port Select CSR
                                                 */
#define RIO_STD_RTE_DEF_PORT_CSR  (0x000078)     /* Standard Route Default Port
                                                        CSR
                                                 */
#define RIO_STD_MC_MASK_PORT_CSR  (0x000080)     /* Switch Multicast Mask Port                                                   //     CSR
                                                 */
#define RIO_STD_MC_ASSOCIATE_SEL_CSR  (0x000084) /* Switch Multicast Associate 
                                                        Select CSR
                                                 */
#define RIO_STD_MC_ASSOCIATE_OPS_CSR  (0x000088) /* Switch Multicast Associate 
                                                        Operation CSR
                                                 */
#define RIO_STD_FIRST_EXT_FEATS_BLK    (0x00000100)
                                                 /* First extended features 
                                                        block header pointer.
                                                 */
#define RIO_STD_EXT_FEATS_PTR     (0xFFFF0000)   /* Pointer to next field in 
                                                        extended features block
                                                        header
                                                 */
#define RIO_STD_EXT_FEATS_TYPE    (0x0000FFFF)   /* Type for extended features 
                                                        block
                                                 */

/* RIO_DEV_ID_CAR : Register Bits Masks Definitions
*/
#define RIO_DEV_ID_DEV_VEN_ID       (0x0000ffff)
#define RIO_DEV_ID_DEV_ID           (0xffff0000)

/* RIO_DEV_INFO_CAR : Register Bits Masks Definitions
*/
#define RIO_DEV_INFO_MASK           (0xffffffff)

/* RIO_ASBLY_ID_CAR : Register Bits Masks Definitions
*/
#define RIO_ASBLY_ID_ASBLY_VEN_ID   (0x0000ffff)
#define RIO_ASBLY_ID_ASBLY_ID       (0xffff0000)

/* RIO_SBLY_INFO_CAR : Register Bits Masks Definitions
*/
#define RIO_ASBLY_INFO_EXT_FEAT_PTR (0x0000ffff)
#define RIO_ASBLY_INFO_ASBLY_REV    (0xffff0000)

#define RIO_FIRST_FEATURE_PTR( assyinfo ) ( (UINT16)assyinfo )

/* RIO_PROC_ELEM_FEAT_CAR : Register Bits Masks Definitions
*/

typedef UINT32 RIO_FEATURES;
#define RIO_PROC_ELEM_FEAT_EXT_AS   (0x00000007)

typedef UINT32 RIO_ADDR_MODE;
#define RIO_PROC_ELEM_FEAT_AS_34BIT_SUPPORT        ((RIO_ADDR_MODE) 0x00000001)
#define RIO_PROC_ELEM_FEAT_AS_50_34BIT_SUPPORT     ((RIO_ADDR_MODE) 0x00000003)
#define RIO_PROC_ELEM_FEAT_AS_66_34BIT_SUPPORT     ((RIO_ADDR_MODE) 0x00000005)
#define RIO_PROC_ELEM_FEAT_AS_66_50_34BIT_SUPPORT  ((RIO_ADDR_MODE) 0x00000007)

#define RIO_PROC_ELEM_FEAT_EXT_FEA  (0x00000008)
#define RIO_PROC_ELEM_FEAT_CTLS     (0x00000010)
#define RIO_PROC_ELEM_FEAT_CRF      (0x00000020)
#define RIO_PROC_ELEM_FEAT_IMP_SPEC (0x00000040)
#define RIO_PROC_ELEM_FEAT_FCTL     (0x00000080)
#define RIO_PROC_ELEM_FEAT_RT       (0x00000100)
#define RIO_PROC_ELEM_FEAT_EXT_RT   (0x00000200)
#define RIO_PROC_ELEM_FEAT_MC       (0x00000400)
#define RIO_PROC_ELEM_FEAT_FARB     (0x00000800)
#define RIO_PROC_ELEM_FEAT_MULTIP   (0x08000000)
#define RIO_PROC_ELEM_FEAT_SW       (0x10000000)
#define RIO_PROC_ELEM_FEAT_PROC     (0x20000000)
#define RIO_PROC_ELEM_FEAT_MEM      (0x40000000)
#define RIO_PROC_ELEM_FEAT_BRDG     (0x80000000)

/* RIO_SWITCH_PORT_CAR : Register Bits Masks Definitions
*/
typedef UINT32 RIO_SWITCH_PORT_INFO;
#define RIO_SWITCH_PORT_INF_PORT_NUM   (0x000000ff)
#define RIO_SWITCH_PORT_INF_PORT_TOTAL (0x0000ff00)

#define RIO_ACCESS_PORT( swportinfo ) ((UINT8)((UINT32)swportinfo & \
                                       (UINT32)RIO_SWITCH_PORT_INF_PORT_NUM))
#define RIO_AVAIL_PORTS( swportinfo ) ((UINT8)(((UINT32)(swportinfo) & \
                               (UINT32)(RIO_SWITCH_PORT_INF_PORT_TOTAL)) >> 8))

/* Get a valid port number: 
   Range 0 - MAX_AVAIL_PORT_NO and 0xFF for broadcast
*/
#define RIO_IS_VALID_PORT_NO(portval, swportinfo) \
                              (((UINT8)(portval) < \
                                (RIO_AVAIL_PORTS(swportinfo))) || \
                               ((UINT8)(portval) == (UINT8)RIO_ALL_PORTS))

/* Get a valid port: Range 0 - MAX_AVAIL_PORT_NO
*/
#define RIO_IS_VALID_PORT(portval, swportinfo) \
                           ((UINT8)portval < RIO_AVAIL_PORTS(swportinfo))

/* RIO_SRC_OPS_CAR : Register Bits Masks Definitions
*/
typedef UINT32 RIO_SOURCE_OPS;
#define RIO_SRC_OPS_PORT_WR       (0x00000004)
#define RIO_SRC_OPS_A_CLEAR       (0x00000010)
#define RIO_SRC_OPS_A_SET         (0x00000020)
#define RIO_SRC_OPS_A_DEC         (0x00000040)
#define RIO_SRC_OPS_A_INC         (0x00000080)
#define RIO_SRC_OPS_A_TSWAP       (0x00000100)
#define RIO_SRC_OPS_DBELL         (0x00000400)
#define RIO_SRC_OPS_D_MSG         (0x00000800)
#define RIO_SRC_OPS_WR_RES        (0x00001000)
#define RIO_SRC_OPS_STRM_WR       (0x00002000)
#define RIO_SRC_OPS_WRITE         (0x00004000)
#define RIO_SRC_OPS_READ          (0x00008000)
#define RIO_SRC_OPS_IMPLEMENT_DEF (0x00030000)
#define RIO_SRC_OPS_DSTM          (0x00040000)
#define RIO_SRC_OPS_GSM_TLBIS     (0x00400000)
#define RIO_SRC_OPS_GSM_TLBI      (0x00800000)
#define RIO_SRC_OPS_GSM_IINV      (0x01000000)
#define RIO_SRC_OPS_GSM_RD        (0x02000000)
#define RIO_SRC_OPS_GSM_FLUSH     (0x04000000)
#define RIO_SRC_OPS_GSM_CAST      (0x08000000)
#define RIO_SRC_OPS_GSM_DINV      (0x10000000)
#define RIO_SRC_OPS_GSM_R4O       (0x20000000)
#define RIO_SRC_OPS_GSM_IR        (0x40000000)
#define RIO_SRC_OPS_GSM_R         (0x80000000)

/* RIO_DST_OPS_CAR : Register Bits Masks Definitions
*/
typedef UINT32 RIO_DEST_OPS;
#define RIO_DST_OPS_IMP_DEF30 (0x00000003)
#define RIO_DST_OPS_PORT_WR   (0x00000004)
#define RIO_DST_OPS_A_CLEAR   (0x00000010)
#define RIO_DST_OPS_A_SET     (0x00000020)
#define RIO_DST_OPS_A_DEC     (0x00000040)
#define RIO_DST_OPS_A_INC     (0x00000080)
#define RIO_DST_OPS_A_TSWAP   (0x00000100)
#define RIO_DST_OPS_A_CSWAP   (0x00000200)
#define RIO_DST_OPS_DBELL     (0x00000400)
#define RIO_DST_OPS_D_MSG     (0x00000800)
#define RIO_DST_OPS_WR_RES    (0x00001000)
#define RIO_DST_OPS_STRM_WR   (0x00002000)
#define RIO_DST_OPS_WRITE     (0x00004000)
#define RIO_DST_OPS_READ      (0x00008000)
#define RIO_DST_OPS_IMP_DEF14 (0x00030000)
#define RIO_DST_OPS_DSTM      (0x00040000)
#define RIO_DST_OPS_GSM_TLBIS (0x00400000)
#define RIO_DST_OPS_GSM_TLBI  (0x00800000)
#define RIO_DST_OPS_GSM_IINV  (0x01000000)
#define RIO_DST_OPS_GSM_RD    (0x02000000)
#define RIO_DST_OPS_GSM_FLUSH (0x04000000)
#define RIO_DST_OPS_GSM_CAST  (0x08000000)
#define RIO_DST_OPS_GSM_DINV  (0x10000000)
#define RIO_DST_OPS_GSM_R4O   (0x20000000)
#define RIO_DST_OPS_GSM_IR    (0x40000000)
#define RIO_DST_OPS_GSM_R     (0x80000000)

/* RIO_MC_FEAT_CAR : Register Bits Masks Definitions
*/
#define RIO_MC_FEAT_SIMP      (0x80000000)

/* RIO_RT_INFO_CAR : Register Bits Masks Definitions
*/
#define RIO_RT_INFO_SIZE      (0x0000ffff)

/* RIO_STD_SW_MC_INFO_CSR : Register Bits Masks Definitions
*/
#define RIO_STD_SW_MC_INFO_MAX_MASKS         (0x0000ffff)
#define RIO_STD_SW_MC_INFO_MAX_DESTID_ASSOC  (0x3fff0000)
#define RIO_STD_SW_MC_INFO_ASSOC_SCOPE       (0x40000000)
#define RIO_STD_SW_MC_INFO_ASSOC_MODE        (0x80000000)

/* RIO_BASE_DEVICE_ID_CSR : Register Bits Masks Definitions
*/
#define RIO_BASE_DEVICE_ID_DEV16             (0x0000ffff)
#define RIO_BASE_DEVICE_ID_DEV8              (0x00ff0000)

/* RIO_STD_HOST_BASE_ID_LOCK : Register Bits Masks Definitions
*/
#define RIO_STD_HOST_BASE_ID_LOCK_HOST_BASE_ID  (0x0000ffff)

/* RIO_STD_COMP_TAG : Register Bits Masks Definitions
*/
#define RIO_STD_COMP_TAG_CTAG  (0xffffffff)

/* RIO_STD_ROUTE_CFG_DESTID : Register Bits Masks Definitions
*/
#define RIO_STD_ROUTE_CFG_DESTID_CFG_DEST_ID     (0x000000ff)
#define RIO_STD_ROUTE_CFG_DESTID_LRG_CFG_DEST_ID (0x0000ff00)

/* RIO_STD_ROUTE_CFG_PORT : Register Bits Masks Definitions
   Can use PORT1-3 masks if RIO_PROC_ELEM_FEAT_EXT_RT is set
*/
#define RIO_STD_ROUTE_CFG_PORT_PORT0 (0x000000ff)
#define RIO_STD_ROUTE_CFG_PORT_PORT1 (0x0000ff00)
#define RIO_STD_ROUTE_CFG_PORT_PORT2 (0x00ff0000)
#define RIO_STD_ROUTE_CFG_PORT_PORT3 (0xff000000)

/* RIO_STD_LUT_ATTR : Register Bits Masks Definitions
*/
#define RIO_STD_LUT_ATTR_DEFAULT_PORT (0x000000ff)

/* RIO_STD_MC_MASK_CFG : Register Bits Masks Definitions
*/
#define RIO_STD_MC_MASK_CFG_PORT_PRESENT  (0x00000001)

#define RIO_STD_MC_MASK_CFG_MASK_CMD                           (0x00000070)
#define RIO_STD_MC_MASK_CFG_EG_PORT_NUM                        (0x0000ff00)
#define RIO_STD_MC_MASK_CFG_MC_MASK_NUM                        (0xffff0000)

#define RIO_STD_MC_MASK_CFG_MASK_CMD_WRITE_TO_VERIFY           (0x00000000)
#define RIO_STD_MC_MASK_CFG_MASK_CMD_ADD_PORT                  (0x00000010)
#define RIO_STD_MC_MASK_CFG_MASK_CMD_DEL_PORT                  (0x00000020)
#define RIO_STD_MC_MASK_CFG_MASK_CMD_DEL_ALL_PORTS             (0x00000040)
#define RIO_STD_MC_MASK_CFG_MASK_CMD_ADD_ALL_PORTS             (0x00000050)

/* RIO_STD_MC_DESTID_CFG : Register Bits Masks Definitions
*/
#define RIO_STD_MC_DESTID_CFG_MASK_NUM_BASE                    (0x0000ffff)
#define RIO_STD_MC_DESTID_CFG_DESTID_BASE                      (0x00ff0000)
#define RIO_STD_MC_DESTID_CFG_DESTID_BASE_LT                   (0xff000000)

/* RIO_STD_MC_DESTID_ASSOC : Register Bits Masks Definitions
*/
#define RIO_STD_MC_DESTID_ASSOC_ASSOC_PRESENT                  (0x00000001)
#define RIO_STD_MC_DESTID_ASSOC_CMD                            (0x00000060)
#define RIO_STD_MC_DESTID_ASSOC_LARGE                          (0x00000080)
#define RIO_STD_MC_DESTID_ASSOC_INGRESS_PORT                   (0x0000ff00)
#define RIO_STD_MC_DESTID_ASSOC_ASSOC_BLK_SIZE                 (0xffff0000)

#define RIO_STD_MC_DESTID_ASSOC_CMD_WRITE_ASSOC_TO_VERIFY      (0x00000000)
#define RIO_STD_MC_DESTID_ASSOC_CMD_DEL_ASSOC                  (0x00000040)
#define RIO_STD_MC_DESTID_ASSOC_CMD_ADD_ASSOC                  (0x00000060)

/* Get a maximum multicast mask number
*/
#define RIO_MAX_MC_NO(swMcastInfo, maxPortNo) ((UINT16)swMcastInfo / \
                                                (UINT8)maxPortNo)

/* Will need these register definitions for the default implementation of 
*  DARrioGetPortErrorStatus.
*
*  Definitions for LP-Serial Extended Features Blocks
*  Defined in terms of b - base address of the extended features block,
*  and p - the port number for per-port registers.
*  Note: Depending on the block type encoded in the header register,
*       some registers may not appear in the block.
*/
#define RIO_EXT_FEAT_PHYS_EP           0x0001
#define RIO_EXT_FEAT_PHYS_EP_SAER      0x0002
#define RIO_EXT_FEAT_PHYS_EP_FREE      0x0003
#define RIO_EXT_FEAT_PHYS_EP_FREE_SAER 0x0009

/* Header for LP-Serial Extended Features Blocks
*/
#define RIO_STD_SW_MB_HEAD(b)                   (b+0x000000) 
/* Link Control CSR (Timeout Value)
*/
#define RIO_PORT_LINK_TO_CTRL_CSR(b)            (b+0x000020) 
/* Response Control CSR (Timeout Value)
*/
#define RIO_PORT_RESP_TO_CTRL_CSR(b)            (b+0x000024) 
/* General Control CSR (Host, Master Enable, Discovered)
*/
#define RIO_PORT_GEN_CTRL_CSR(b)                (b+0x00003C) 

/* Port N Link Request CSR
*/
#define RIO_PORT_N_LINK_MAINT_REQ_CSR(b,p)      (b+0x000040+(p<<5)) 
/* Port N Link Response CSR
*/
#define RIO_PORT_N_LINK_MAINT_RESP_CSR(b,p)     (b+0x000044+(p<<5)) 
/* Port N Local AckID CSR
*/
#define RIO_PORT_N_LOCAL_ACKID_CSR(b,p)         (b+0x000048+(p<<5)) 
/* Port N Control 2 CSR
*/
#define RIO_PORT_N_CONTROL_2_CSR(b,p)           (b+0x000054+(p<<5)) 
/* Port N Error Status CSR
*/
#define RIO_PORT_N_ERR_STAT_CSR(b,p)            (b+0x000058+(p<<5)) 
/* Port N Control CSR
*/
#define RIO_PORT_N_CONTROL_CSR(b,p)             (b+0x00005C+(p<<5)) 

#define RIO_PORT_N_ERR_STAT_MASK                (0x07120214)
#define RIO_PORT_N_ERR_STAT_ALL_CLEAR           (0x07120214)

#define RIO_PORT_N_LINK_RESP_VALID              (0x80000000)
#define RIO_PORT_N_LINK_RESP_IS_VALID(r) ((UINT32)r & \
                                          RIO_PORT_N_LINK_RESP_VALID)
#define RIO_PORT_N_ACKID_STAT_MASK              (0x000007E0)
#define RIO_PORT_N_ACKID_STAT(r)         (((UINT32)r &\
                                          RIO_PORT_N_ACKID_STAT_MASK) >> 5)
#define RIO_PORT_N_LINK_STAT_MASK               (0x0000001F)
#define RIO_PORT_N_LINK_STAT(r)          ((UINT32)r &\
                                          RIO_PORT_N_LINK_STAT_MASK)
#define RIO_PORT_N_LOCAL_ACKID_CLR_OUTSTANDING_ACKID  (0x80000000)
#define RIO_PORT_N_INBOUND_ACKID_MASK           (0x3F000000)
#define RIO_PORT_N_INBOUND_ACKID(r)      (((UINT32)r &\
                                          RIO_PORT_N_INBOUND_ACKID_MASK) >> 24)
#define RIO_PORT_N_PUT_INBOUND_ACKID(r)  (((UINT32)r << 24) &\
                                          RIO_PORT_N_INBOUND_ACKID_MASK)
#define RIO_PORT_N_OUTSTANDING_ACKID_MASK       (0x00003F00)
#define RIO_PORT_N_OUTSTANDING_ACKID(r)  (((UINT32)r &\
                                          RIO_PORT_N_OUTSTANDING_ACKID_MASK) \
                                             >> 8)
#define RIO_PORT_N_PUT_OUTSTANDING_ACKID(r) (((UINT32)r << 8) & \
                                             RIO_PORT_N_OUTSTANDING_ACKID_MASK)
#define RIO_PORT_N_OUTBOUND_ACKID_MASK          (0x0000003F)
#define RIO_PORT_N_OUTBOUND_ACKID(r)        ((UINT32)r &\
                                             RIO_PORT_N_OUTBOUND_ACKID_MASK)
#define RIO_PORT_N_PUT_OUTBOUND_ACKID(r)    ((UINT32)r &\
                                             RIO_PORT_N_OUTBOUND_ACKID_MASK)

#define IS_RIO_PORT_LOCKOUT(r)           ((UINT32)r & RIO_SPX_CTL_PORT_LOCKOUT)
#define IS_RIO_DROP_PKT_ENABLE(r)        ((UINT32)r & RIO_SPX_CTL_DROP_EN)
#define IS_RIO_STOP_ON_PORT_FAILED_ENC_ENABLE(r) \
                                         ((UINT32)r & RIO_SPX_CTL_STOP_FAIL_EN)

/* RIO_STD_SW_MB_HEAD : Register Bits Masks Definitions
*/
#define RIO_STD_SW_MB_HEAD_EF_ID                       (0x0000ffff)
#define RIO_STD_SW_MB_HEAD_EF_PTR                      (0xffff0000)

#define RIO_LP_SERIAL_EXT_BLK_HDR_EF_PTR               (0xffff0000)
#define RIO_LP_SERIAL_EXT_BLK_HDR_BLK_TYPE             (0x0000ffff)
#define RIO_LP_SERIAL_EXT_BLK_HDR_BLK_TYPE_EP          (0x00000001)                   
#define RIO_LP_SERIAL_EXT_BLK_HDR_BLK_TYPE_EP_SAER     (0x00000002)
#define RIO_LP_SERIAL_EXT_BLK_HDR_BLK_TYPE_SW          (0x00000003)
#define RIO_LP_SERIAL_EXT_BLK_HDR_BLK_TYPE_SW_SAER     (0x00000009)

/* RIO_STD_SW_LT_CTL : Register Bits Masks Definitions
*/
#define RIO_STD_SW_LT_CTL_TVAL       (0xffffff00)

/* RIO_STD_SW_LT_CTL : Register Bits Masks Definitions
*/
#define RIO_STD_SW_RT_CTL_TVAL       (0xffffff00)

/* RIO_STD_SW_GEN_CTL : Register Bits Masks Definitions
*/
#define RIO_STD_SW_GEN_CTL_HOST      (0x80000000)
#define RIO_STD_SW_GEN_CTL_MAST_EN   (0x40000000)
#define RIO_STD_SW_GEN_CTL_DISC      (0x20000000)

/* RIO_SPX_LM_REQ : Register Bits Masks Definitions
*/
#define RIO_SPX_LM_REQ_CMD           (0x00000007)
#define RIO_SPX_LM_REQ_CMD_RESET     (0x00000003)
#define RIO_SPX_LM_REQ_CMD_LR_IS     (0x00000004)

/* RIO_SPX_LM_RESP : Register Bits Masks Definitions
*/
typedef UINT32 RIO_SPX_LM_LINK_STAT;
#define RIO_SPX_LM_RESP_LINK_STAT       (0x0000001f)
#define RIO_SPX_LM_RESP_LINK_STAT_FATAL (0x00000002)
#define RIO_SPX_LM_RESP_LINK_STAT_R_STP (0x00000004)
#define RIO_SPX_LM_RESP_LINK_STAT_E_STP (0x00000005)
#define RIO_SPX_LM_RESP_LINK_STAT_OK    (0x00000010)
#define RIO_SPX_LM_RESP_ACK_ID_STAT     (0x000003e0)
#define RIO_SPX_LM_RESP_RESP_VLD        (0x80000000)

/* RIO_SPX_ACKID_STAT : Register Bits Masks Definitions
*/
#define RIO_SPX_ACKID_STAT_OUTBOUND    (0x0000001f)
#define RIO_SPX_ACKID_STAT_OUTSTANDING (0x00001f00)
#define RIO_SPX_ACKID_STAT_INBOUND     (0x1f000000)
#define RIO_SPX_ACKID_STAT_CLR_PKTS    (0x80000000)

/* RIO_SPX_ERR_STATUS : Register Bits Masks Definitions
*/
typedef UINT32 RIO_PORT_ERROR_STATUS;
#define RIO_SPX_ERR_STATUS_PORT_UNINIT     (0x00000001)
#define RIO_SPX_ERR_STATUS_PORT_OK         (0x00000002)
#define RIO_SPX_ERR_STATUS_PORT_ERR        (0x00000004)
#define RIO_SPX_ERR_STATUS_PORT_UNAVAIL    (0x00000008)
#define RIO_SPX_ERR_STATUS_PORT_W_PEND     (0x00000010)
#define RIO_SPX_ERR_STATUS_INPUT_ERR_STOP  (0x00000100)
#define RIO_SPX_ERR_STATUS_INPUT_ERR       (0x00000200)
#define RIO_SPX_ERR_STATUS_INPUT_RS        (0x00000400)
#define RIO_SPX_ERR_STATUS_OUTPUT_ERR_STOP (0x00010000)
#define RIO_SPX_ERR_STATUS_OUTPUT_ERR      (0x00020000)
#define RIO_SPX_ERR_STATUS_OUTPUT_RS       (0x00040000)
#define RIO_SPX_ERR_STATUS_OUTPUT_R        (0x00080000)
#define RIO_SPX_ERR_STATUS_OUTPUT_RE       (0x00100000)
#define RIO_SPX_ERR_STATUS_OUTPUT_DEG      (0x01000000)
#define RIO_SPX_ERR_STATUS_OUTPUT_FAIL     (0x02000000)
#define RIO_SPX_ERR_STATUS_OUTPUT_DROP     (0x04000000)
#define RIO_SPX_ERR_STATUS_TXFC_ACTIVE     (0x08000000)
#define RIO_SPX_ERR_STATUS_IDLE2_ACTIVE    (0x20000000)
#define RIO_SPX_ERR_STATUS_IDLE2_EN        (0x40000000)
#define RIO_SPX_ERR_STATUS_IDLE2_SUP       (0x80000000)

#define IS_RIO_PORT_N_ERR_STAT_PORT_UNINIT(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_PORT_UNINIT)

#define IS_RIO_PORT_N_ERR_STAT_PORT_OK(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_PORT_OK)

#define IS_RIO_PORT_N_ERR_STAT_PORT_ERR(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_PORT_ERR)

#define IS_RIO_PORT_N_ERR_STAT_PORT_UNAVAIL(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_PORT_UNAVAIL)

#define IS_RIO_PORT_N_ERR_STAT_IN_PORT_ERR_STOP(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_INPUT_ERR_STOP)

#define IS_RIO_PORT_N_ERR_STAT_OUT_PORT_ERR_STOP(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_OUTPUT_ERR_STOP)

#define IS_RIO_PORT_N_ERR_STAT_OUT_ERR_ENC(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_OUTPUT_ERR)

#define IS_RIO_PORT_N_ERR_STAT_OUT_FAILED_ENC(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_OUTPUT_FAIL)

#define IS_RIO_PORT_N_ERR_STAT_OUT_PACKET_DROP(r) \
                      ((UINT32)r & RIO_SPX_ERR_STATUS_OUTPUT_DROP)

/* RIO_SPX_CTL : Register Bits Masks Definitions
*/
#define RIO_SPX_CTL_PORT_TYPE     (0x00000001)
#define RIO_SPX_CTL_PORT_LOCKOUT  (0x00000002)
#define RIO_SPX_CTL_DROP_EN       (0x00000004)
#define RIO_SPX_CTL_STOP_FAIL_EN  (0x00000008)
#define RIO_SPX_CTL_IMP_DEF20     (0x00000FF0)
#define RIO_SPX_CTL_ENUM_B        (0x00020000)
#define RIO_SPX_CTL_MCS_EN        (0x00080000)
#define RIO_SPX_CTL_ERR_DIS       (0x00100000)
#define RIO_SPX_CTL_INPUT_EN      (0x00200000)
#define RIO_SPX_CTL_OUTPUT_EN     (0x00400000)
#define RIO_SPX_CTL_PORT_DIS      (0x00800000)
#define RIO_SPX_CTL_OVER_PWIDTH   (0x07000000)
#define RIO_SPX_CTL_INIT_PWIDTH   (0x38000000)
#define RIO_SPX_CTL_PORT_WIDTH    (0xc0000000)

// Definitions for initialize port width values
#define RIO_SPX_CTL_INIT_PWIDTH_1x_L0 (0x00000000)
#define RIO_SPX_CTL_INIT_PWIDTH_1x_LR (0x08000000)
#define RIO_SPX_CTL_INIT_PWIDTH_4x    (0x10000000)
#define RIO_SPX_CTL_INIT_PWIDTH_2x    (0x18000000)

// Definitions for port width override settings
#define RIO_SPX_CTL_OVER_PWIDTH_NONE     (0x00000000)
#define RIO_SPX_CTL_OVER_PWIDTH_1x_L0    (0x02000000)
#define RIO_SPX_CTL_OVER_PWIDTH_1x_LR    (0x03000000)
#define RIO_SPX_CTL_OVER_PWIDTH_RSVD     (0x04000000)
#define RIO_SPX_CTL_OVER_PWIDTH_2x_NO_4X (0x05000000)
#define RIO_SPX_CTL_OVER_PWIDTH_4x_NO_2X (0x06000000)
#define RIO_SPX_CTL_OVER_PWIDTH_NONE_2   (0x07000000)

#define IS_RIO_PORT_N_CTRL1_MC_EVENT_PARTICIPAT(r) \
                                  ((UINT32)r & RIO_SPX_CTL_MCS_EN)

#define IS_RIO_PORT_N_CTRL1_IN_PORT_ENABLE(r) \
                                  ((UINT32)r & RIO_SPX_CTL_INPUT_EN)

#define IS_RIO_PORT_N_CTRL1_OUT_PORT_ENABLE(r) \
                                  ((UINT32)r & RIO_SPX_CTL_OUTPUT_EN)

#define IS_RIO_PORT_N_CTRL1_PORT_DISABLE(r) \
                                  ((UINT32)r & RIO_SPX_CTL_PORT_DIS)

/* RIO_SPC_CTL2 : Register Bits Masks Definitions
*/
#define RIO_SPX_CTL2_BAUDRATE     (0xF0000000)
#define RIO_SPX_CTL2_NO_BR        (0x00000000)
#define RIO_SPX_CTL2_1P25_BR      (0x10000000)
#define RIO_SPX_CTL2_2P5_BR       (0x20000000)
#define RIO_SPX_CTL2_3P125_BR     (0x30000000)
#define RIO_SPX_CTL2_5P0_BR       (0x40000000)
#define RIO_SPX_CTL2_6P25_BR      (0x50000000)

#define RIO_SPX_CTL2_DISC_SUP     (0x08000000)
#define RIO_SPX_CTL2_DISC_EN      (0x04000000)
#define RIO_SPX_CTL_1P25_SUP      (0x02000000)
#define RIO_SPX_CTL_1P25_EN       (0x01000000)
#define RIO_SPX_CTL_2P5_SUP       (0x00800000)
#define RIO_SPX_CTL_2P5_EN        (0x00400000)
#define RIO_SPX_CTL_3P125_SUP     (0x00200000)
#define RIO_SPX_CTL_3P125_EN      (0x00100000)
#define RIO_SPX_CTL_5P0_SUP       (0x00080000)
#define RIO_SPX_CTL_5P0_EN        (0x00040000)
#define RIO_SPX_CTL_6P25_SUP      (0x00020000)
#define RIO_SPX_CTL_6P25_EN       (0x00010000)
#define RIO_SPX_CTL_EN_INACT      (0x00000008)
#define RIO_SPX_CTL_DS_DIS        (0x00000004)
#define RIO_SPX_CTL_TX_EMPH_SUP   (0x00000002)
#define RIO_SPX_CTL_TX_EMPH_EN    (0x00000001)

/* Error Management Extensions Block Register Bit
   RIO_STD_ERR_RPT_BH : Register Bits Masks Definitions
*/
#define RIO_EXT_FEAT_ERR_RPT      (0x0007)
#define RIO_STD_ERR_RPT_BH_EF_ID  (0x0000ffff)
#define RIO_STD_ERR_RPT_BH_EF_PTR (0xffff0000)

#define RIO_STD_LOG_ERR_DET(b)       (b+0x08)
#define RIO_STD_LOG_ERR_DET_EN(b)    (b+0x0C)
#define RIO_STD_LOG_ERR_ADDR_H(b)    (b+0x10)
#define RIO_STD_LOG_ERR_ADDR(b)      (b+0x14)
#define RIO_STD_LOG_ERR_DEVID(b)     (b+0x18)
#define RIO_STD_LOG_ERR_CTRL_INFO(b) (b+0x1C)
#define RIO_STD_PW_DESTID(b)         (b+0x28)

/* RIO_STD_LOG_ERR_DET : Register Bits Masks Definitions
*/
#define RIO_STD_LOG_ERR_DET_L_UNSUP_TRANS  (0x00400000)
#define RIO_STD_LOG_ERR_DET_L_ILL_RESP     (0x00800000)
#define RIO_STD_LOG_ERR_DET_L_ILL_TRANS    (0x08000000)

/* RIO_STD_LOG_ERR_DET_EN : Register Bits Masks Definitions
*/
#define RIO_STD_LOG_ERR_DET_EN_UNSUP_TRANS_EN (0x00400000)
#define RIO_STD_LOG_ERR_DET_EN_ILL_RESP_EN    (0x00800000)
#define RIO_STD_LOG_ERR_DET_EN_ILL_TRANS_EN   (0x08000000)

/* RIO_STD_LOG_ERR_ADDR : Register Bits Masks Definitions
*/
#define RIO_STD_LOG_ERR_ADDR_WDPTR   (0x00000002)
#define RIO_STD_LOG_ERR_ADDR_ADDRESS (0x00fffff8)

/* RIO_STD_LOG_ERR_DEVID : Register Bits Masks Definitions
*/
#define RIO_STD_LOG_ERR_DEVID_SRCID     (0x000000ff)
#define RIO_STD_LOG_ERR_DEVID_SRCID_MSB (0x0000ff00)

/* RIO_STD_LOG_ERR_CTRL_INFO : Register Bits Masks Definitions
*/
#define RIO_STD_LOG_ERR_CTRL_INFO_TTYPE (0x0f000000)
#define RIO_STD_LOG_ERR_CTRL_INFO_FTYPE (0xf0000000)

/* RIO_STD_PW_DESTID : Register Bits Masks Definitions
*/
#define RIO_STD_PW_DESTID_LARGE_DESTID  (0x00008000)
#define RIO_STD_PW_DESTID_DESTID_LSB    (0x00ff0000)
#define RIO_STD_PW_DESTID_DESTID_MSB    (0xff000000)

/* RIO_STD_PKT_TTL : Register Bits Masks Definitions
*/
#define RIO_STD_PKT_TTL_TTL (0xffff0000)

/* RIO_SPX_ERR_DET : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_DET_LINK_TO       (0x00000001)
#define RIO_SPX_ERR_DET_CS_ACK_ILL    (0x00000002)
#define RIO_SPX_ERR_DET_DELIN_ERR     (0x00000004)
#define RIO_SPX_ERR_DET_PROT_ERR      (0x00000010)
#define RIO_SPX_ERR_DET_LR_ACKID_ILL  (0x00000020)
#define RIO_SPX_ERR_DET_PKT_ILL_SIZE  (0x00020000)
#define RIO_SPX_ERR_DET_PKT_CRC_ERR   (0x00040000)
#define RIO_SPX_ERR_DET_PKT_ILL_ACKID (0x00080000)
#define RIO_SPX_ERR_DET_CS_NOT_ACC    (0x00100000)
#define RIO_SPX_ERR_DET_CS_ILL_ID     (0x00200000)
#define RIO_SPX_ERR_DET_CS_CRC_ERR    (0x00400000)
#define RIO_SPX_ERR_DET_IMP_SPEC_ERR  (0x80000000)

/* RIO_SPX_RATE_EN : Register Bits Masks Definitions
*/
#define RIO_SPX_RATE_EN_LINK_TO_EN         (0x00000001)
#define RIO_SPX_RATE_EN_CS_ACK_ILL_EN      (0x00000002)
#define RIO_SPX_RATE_EN_DELIN_ERR_EN       (0x00000004)
#define RIO_SPX_RATE_EN_PROT_ERR_EN        (0x00000010)
#define RIO_SPX_RATE_EN_LR_ACKID_ILL_EN    (0x00000020)
#define RIO_SPX_RATE_EN_PKT_ILL_SIZE_EN    (0x00020000)
#define RIO_SPX_RATE_EN_PKT_CRC_ERR_EN     (0x00040000)
#define RIO_SPX_RATE_EN_PKT_ILL_ACKID_EN   (0x00080000)
#define RIO_SPX_RATE_EN_CS_NOT_ACC_EN      (0x00100000)
#define RIO_SPX_RATE_EN_CS_ILL_ID_EN       (0x00200000)
#define RIO_SPX_RATE_EN_CS_CRC_ERR_EN      (0x00400000)
#define RIO_SPX_RATE_EN_IMP_SPEC_ERR       (0x80000000)

/* Defintions for Error Management Extensions Register Block
   Logical/Transport Layer Error Detection CSR
*/
#define    RIO_LT_LAYER_ERROR_DETECT_CSR(b)   (b+0x000008)    
/* Logical/Transport Layer Error Enable CSR
*/
#define    RIO_LT_LAYER_ERROR_ENABLE_CSR(b)   (b+0x00000C)    
/* Logical/Transport Layer High Address Capture CSR
*/
#define    RIO_LT_LAYER_HIGH_ADDR_CAP_CSR(b)  (b+0x000010)    
/* Logical/Transport Layer Address Capture CSR
*/
#define    RIO_LT_LAYER_ADDR_CAP_CSR(b)       (b+0x000014)    
/* Logical/Transport Layer Device ID Capture CSR
*/
#define    RIO_LT_LAYER_DEVIC_ID_CAP_CSR(b)   (b+0x000018)    
/* Logical/Transport Layer Control Capture CSR
*/
#define    RIO_LT_LAYER_CONTROL_CAP_CSR(b)    (b+0x00001C)    
/* Port Write Target Device ID CSR
*/
#define RIO_PORT_WRITE_TARGET_DEVICE_ID_CSR(b) (b+0x000028)    
/* Packet Time to Live CSR
*/
#define RIO_PACKET_TIME_TO_LIVE_CSR(b)        (b+0x00002C)    
/* Port N Error Detect CSR
*/
#define RIO_PORT_N_ERROR_DETECT_CSR(b,p)      (b+0x000040+(p<<6))    
/* Port N Error Rate Enable CSR
*/
#define RIO_PORT_N_ERROR_RATE_ENABLE_CSR(b,p) (b+0x000044+(p<<6))    
/* Port N Attribute Capture CSR
*/
#define RIO_PORT_N_ATTRIB_CAP_CSR(b,p)        (b+0x000048+(p<<6))    
/* Port N Packet/Control Symbol Capture 0 CSR
*/
#define RIO_PORT_N_PACKET_CONTROLSYM_CAP_0_CSR(b,p) (b+0x00004C+(p<<6))    
/* Port N Packet/Control Symbol Capture 1 CSR
*/
#define RIO_PORT_N_PACKET_CONTROLSYM_CAP_1_CSR(b,p) (b+0x000050+(p<<6))    
/* Port N Packet/Control Symbol Capture 2 CSR
*/
#define RIO_PORT_N_PACKET_CONTROLSYM_CAP_2_CSR(b,p) (b+0x000054+(p<<6))    
/* Port N Packet/Control Symbol Capture 3 CSR
*/
#define RIO_PORT_N_PACKET_CONTROLSYM_CAP_3_CSR(b,p) (b+0x000058+(p<<6))    
/* Port N Error Rate CSR
*/
#define RIO_PORT_N_ERROR_RATE_CSR(b,p)       (b+0x000068+(p<<6))    
/* Port N Error Rate Threshold CSR
*/
#define RIO_PORT_N_ERROR_RATE_TH_CSR(b,p)      (b+0x00006C+(p<<6))    
#define RIO_PORT_N_ERROR_RATE_COUNT_MASK       (0x000000FF)
#define RIO_PORT_N_ATTRIB_CAP_VALID_INFO_MASK  (0x00000001)

/* RIO_SPX_ERR_ATTR_CAPT_DBG0 : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_ATTR_CAPT_DBG0_VAL_CAPT    (0x00000001)
#define RIO_SPX_ERR_ATTR_CAPT_DBG0_ERR_TYPE    (0x1f000000)
#define RIO_SPX_ERR_ATTR_CAPT_DBG0_INFO_TYPE   (0xc0000000)

/* RIO_SPX_ERR_CAPT_0_DBG1 : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_CAPT_0_DBG1_CAPT_0         (0xffffffff)

/* RIO_SPX_ERR_CAPT_1_DBG2 : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_CAPT_1_DBG2_CAPT_1         (0xffffffff)

/* RIO_SPX_ERR_CAPT_2_DBG3 : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_CAPT_2_DBG3_CAPT_2         (0xffffffff)

/* RIO_SPX_ERR_CAPT_3_DBG4 : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_CAPT_3_DBG4_CAPT_3         (0xffffffff)

/* RIO_SPX_ERR_RATE : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_RATE_ERR_RATE_CNT          (0x000000ff)
#define RIO_SPX_ERR_RATE_PEAK                  (0x0000ff00)
#define RIO_SPX_ERR_RATE_ERR_RR                (0x00030000)
#define RIO_SPX_ERR_RATE_ERR_RB                (0xff000000)

#define RIO_SPX_ERR_RATE_ERR_RB_NONE           (0x00000000)
#define RIO_SPX_ERR_RATE_ERR_RB_1_MS           (0x01000000)
#define RIO_SPX_ERR_RATE_ERR_RB_10_MS          (0x02000000)
#define RIO_SPX_ERR_RATE_ERR_RB_100_MS         (0x04000000)
#define RIO_SPX_ERR_RATE_ERR_RB_1_SEC          (0x08000000)
#define RIO_SPX_ERR_RATE_ERR_RB_10_SEC         (0x10000000)
#define RIO_SPX_ERR_RATE_ERR_RB_100_SEC        (0x20000000)
#define RIO_SPX_ERR_RATE_ERR_RB_1000_SEC       (0x40000000)
#define RIO_SPX_ERR_RATE_ERR_RB_10000_SEC      (0x80000000)

#define RIO_SPX_ERR_RATE_ERR_RR_LIM_2          (0x00000000)
#define RIO_SPX_ERR_RATE_ERR_RR_LIM_4          (0x00010000)
#define RIO_SPX_ERR_RATE_ERR_RR_LIM_16         (0x00020000)
#define RIO_SPX_ERR_RATE_ERR_RR_LIM_NONE       (0x00030000)

/* RIO_SPX_ERR_THRESH : Register Bits Masks Definitions
*/
#define RIO_SPX_ERR_THRESH_ERR_RDT             (0x00ff0000)
#define RIO_SPX_ERR_THRESH_ERR_RFT             (0xff000000)

/* Definitions for Per Lane registers
   No registers/bit field definitions, as these are not necessary for
       system bring up.
*/
#define RIO_EXT_FEAT_PER_LANE                  0x000D

/* Definitions for VC registers
   No registers/bit field definitions, as these are not necessary for
       system bring up.
*/
#define RIO_EXT_FEAT_VC                        0x000A

/* Definitions for VC VoQ Backpressure registers
   No registers/bit field definitions, as these are not necessary for
       system bring up.
*/

#define RIO_EXT_FEAT_VC_VOQ                    0x000B

#ifdef __cplusplus
}
#endif

#endif /* __DAR_REGDEFS_H__ */

