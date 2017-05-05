/***************************************************************************
** (C) Copyright 2011; Integrated Device Technology
** July 5, 2011 All Rights Reserved.
**
** This file contains the generic device register definitions and bitfield masks. 
**  
** Disclaimer
** Integrated Device Technology, Inc. ("IDT") reserves the right to make changes
** to its products or specifications at any time, without notice, in order to
** improve design or performance. IDT does not assume responsibility for use of
** any circuitry described herein other than the circuitry embodied in an IDT
** product. Disclosure of the information herein does not convey a license or
** any other right, by implication or otherwise, in any patent, trademark, or
** other intellectual property right of IDT. IDT products may contain errata
** which can affect product performance to a minor or immaterial degree. Current
** characterized errata will be made available upon request. Items identified
** herein as "reserved" or "undefined" are reserved for future definition. IDT
** does not assume responsibility for conflicts or incompatibilities arising
** from the future definition of such items. IDT products have not been
** designed, tested, or manufactured for use in, and thus are not warranted for,
** applications where the failure, malfunction, or any inaccuracy in the
** application carries a risk of death, serious bodily injury, or damage to
** tangible property. Code examples provided herein by IDT are for illustrative
** purposes only and should not be relied upon for developing applications. Any
** use of such code examples shall be at the user's sole risk.
***************************************************************************/

#ifndef __GENDEV_H__
#define __GENDEV_H__

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************/
/* GENDEV : S-RIO Register address offset definitions */
/******************************************************/

#define GENDEV_MAX_PORTS 1
#define GENDEV_MAX_LANES 4

#define GENDEV_DEV_ID                                    ((uint32_t)0x00000000)
#define GENDEV_DEV_INFO                                  ((uint32_t)0x00000004)
#define GENDEV_ASBLY_ID                                  ((uint32_t)0x00000008)
#define GENDEV_ASBLY_INFO                                ((uint32_t)0x0000000c)
#define GENDEV_PE_FEAT                                   ((uint32_t)0x00000010)
#define GENDEV_SRC_OP                                    ((uint32_t)0x00000018)
#define GENDEV_DEST_OP                                   ((uint32_t)0x0000001c)
#define GENDEV_SR_XADDR                                  ((uint32_t)0x0000004c)
#define GENDEV_BASE_ID                                   ((uint32_t)0x00000060)
#define GENDEV_HOST_BASE_ID_LOCK                         ((uint32_t)0x00000068)
#define GENDEV_COMP_TAG                                  ((uint32_t)0x0000006c)
#define GENDEV_SP_MB_HEAD                                ((uint32_t)0x00000100)
#define GENDEV_SP_LT_CTL                                 ((uint32_t)0x00000120)
#define GENDEV_SR_RSP_TO                                 ((uint32_t)0x00000124)
#define GENDEV_SP_GEN_CTL                                ((uint32_t)0x0000013c)
#define GENDEV_SP_LM_REQ                                 ((uint32_t)0x00000140)
#define GENDEV_SP_LM_RESP                                ((uint32_t)0x00000144)
#define GENDEV_SP_ACKID_STAT                             ((uint32_t)0x00000148)
#define GENDEV_SP_CTL2                                   ((uint32_t)0x00000154)
#define GENDEV_SP_ERR_STAT                               ((uint32_t)0x00000158)
#define GENDEV_SP_CTL                                    ((uint32_t)0x0000015c)
//#define GENDEV_DEVICE_ID     (0x000080AB)

/* GENDEV_DEV_INFO : Register Bits Masks Definitions */
#define GENDEV_DEV_INFO_DEV_REV                          ((uint32_t)0xffffffff)

/* GENDEV_ASBLY_ID : Register Bits Masks Definitions */
#define GENDEV_ASBLY_ID_ASBLY_VEN_ID                     ((uint32_t)0x0000ffff)
#define GENDEV_ASBLY_ID_ASBLY_ID                         ((uint32_t)0xffff0000)

/* GENDEV_ASBLY_INFO : Register Bits Masks Definitions */
#define GENDEV_ASSY_INF_EFB_PTR                          ((uint32_t)0x0000ffff)
#define GENDEV_ASSY_INF_ASSY_REV                         ((uint32_t)0xffff0000)

/* GENDEV_PE_FEAT : Register Bits Masks Definitions */
#define GENDEV_PE_FEAT_EXT_AS                            ((uint32_t)0x00000007)
#define GENDEV_PE_FEAT_EXT_FEA                           ((uint32_t)0x00000008)
#define GENDEV_PE_FEAT_CTLS                              ((uint32_t)0x00000010)
#define GENDEV_PE_FEAT_CRF                               ((uint32_t)0x00000020)
#define GENDEV_PE_FEAT_FLOW_CTRL                         ((uint32_t)0x00000080)
#define GENDEV_PE_FEAT_SRTC                              ((uint32_t)0x00000100)
#define GENDEV_PE_FEAT_ERTC                              ((uint32_t)0x00000200)
#define GENDEV_PE_FEAT_MC                                ((uint32_t)0x00000400)
#define GENDEV_PE_FEAT_FLOW_ARB                          ((uint32_t)0x00000800)
#define GENDEV_PE_FEAT_MULT_P                            ((uint32_t)0x08000000)
#define GENDEV_PE_FEAT_SW                                ((uint32_t)0x10000000)
#define GENDEV_PE_FEAT_PROC                              ((uint32_t)0x20000000)
#define GENDEV_PE_FEAT_MEM                               ((uint32_t)0x40000000)
#define GENDEV_PE_FEAT_BRDG                              ((uint32_t)0x80000000)

/* GENDEV_SRC_OP : Register Bits Masks Definitions */
#define GENDEV_SRC_OP_IMPLEMENT_DEF2                     ((uint32_t)0x00000003)
#define GENDEV_SRC_OP_PORT_WR                            ((uint32_t)0x00000004)
#define GENDEV_SRC_OP_A_SWAP                             ((uint32_t)0x00000008)
#define GENDEV_SRC_OP_A_CLEAR                            ((uint32_t)0x00000010)
#define GENDEV_SRC_OP_A_SET                              ((uint32_t)0x00000020)
#define GENDEV_SRC_OP_A_DEC                              ((uint32_t)0x00000040)
#define GENDEV_SRC_OP_A_INC                              ((uint32_t)0x00000080)
#define GENDEV_SRC_OP_ATSWAP                             ((uint32_t)0x00000100)
#define GENDEV_SRC_OP_ACSWAP                             ((uint32_t)0x00000200)
#define GENDEV_SRC_OP_DBELL                              ((uint32_t)0x00000400)
#define GENDEV_SRC_OP_D_MSG                              ((uint32_t)0x00000800)
#define GENDEV_SRC_OP_WR_RES                             ((uint32_t)0x00001000)
#define GENDEV_SRC_OP_STRM_WR                            ((uint32_t)0x00002000)
#define GENDEV_SRC_OP_WRITE                              ((uint32_t)0x00004000)
#define GENDEV_SRC_OP_READ                               ((uint32_t)0x00008000)
#define GENDEV_SRC_OP_IMPLEMENT_DEF                      ((uint32_t)0x00030000)
#define GENDEV_SRC_OP_DS                                 ((uint32_t)0x00040000)
#define GENDEV_SRC_OP_DS_TM                              ((uint32_t)0x00080000)
#define GENDEV_SRC_OP_RIO_RSVD_11                        ((uint32_t)0x00100000)
#define GENDEV_SRC_OP_RIO_RSVD_10                        ((uint32_t)0x00200000)
#define GENDEV_SRC_OP_G_TLB_SYNC                         ((uint32_t)0x00400000)
#define GENDEV_SRC_OP_G_TLB_INVALIDATE                   ((uint32_t)0x00800000)
#define GENDEV_SRC_OP_G_IC_INVALIDATE                    ((uint32_t)0x01000000)
#define GENDEV_SRC_OP_G_IO_READ                          ((uint32_t)0x02000000)
#define GENDEV_SRC_OP_G_DC_FLUSH                         ((uint32_t)0x04000000)
#define GENDEV_SRC_OP_G_CASTOUT                          ((uint32_t)0x08000000)
#define GENDEV_SRC_OP_G_DC_INVALIDATE                    ((uint32_t)0x10000000)
#define GENDEV_SRC_OP_G_READ_OWN                         ((uint32_t)0x20000000)
#define GENDEV_SRC_OP_G_IREAD                            ((uint32_t)0x40000000)
#define GENDEV_SRC_OP_G_READ                             ((uint32_t)0x80000000)

/* GENDEV_DEST_OP : Register Bits Masks Definitions */
#define GENDEV_DEST_OP_IMPLEMENT_DEF2                    ((uint32_t)0x00000003)
#define GENDEV_DEST_OP_PORT_WR                           ((uint32_t)0x00000004)
#define GENDEV_DEST_OP_A_SWAP                            ((uint32_t)0x00000008)
#define GENDEV_DEST_OP_A_CLEAR                           ((uint32_t)0x00000010)
#define GENDEV_DEST_OP_A_SET                             ((uint32_t)0x00000020)
#define GENDEV_DEST_OP_A_DEC                             ((uint32_t)0x00000040)
#define GENDEV_DEST_OP_A_INC                             ((uint32_t)0x00000080)
#define GENDEV_DEST_OP_ATSWAP                            ((uint32_t)0x00000100)
#define GENDEV_DEST_OP_ACSWAP                            ((uint32_t)0x00000200)
#define GENDEV_DEST_OP_DBELL                             ((uint32_t)0x00000400)
#define GENDEV_DEST_OP_D_MSG                             ((uint32_t)0x00000800)
#define GENDEV_DEST_OP_WR_RES                            ((uint32_t)0x00001000)
#define GENDEV_DEST_OP_STRM_WR                           ((uint32_t)0x00002000)
#define GENDEV_DEST_OP_WRITE                             ((uint32_t)0x00004000)
#define GENDEV_DEST_OP_READ                              ((uint32_t)0x00008000)
#define GENDEV_DEST_OP_IMPLEMENT_DEF                     ((uint32_t)0x00030000)
#define GENDEV_DEST_OP_DS                                ((uint32_t)0x00040000)
#define GENDEV_DEST_OP_DS_TM                             ((uint32_t)0x00080000)
#define GENDEV_DEST_OP_RIO_RSVD_11                       ((uint32_t)0x00100000)
#define GENDEV_DEST_OP_RIO_RSVD_10                       ((uint32_t)0x00200000)
#define GENDEV_DEST_OP_G_TLB_SYNC                        ((uint32_t)0x00400000)
#define GENDEV_DEST_OP_G_TLB_INVALIDATE                  ((uint32_t)0x00800000)
#define GENDEV_DEST_OP_G_IC_INVALIDATE                   ((uint32_t)0x01000000)
#define GENDEV_DEST_OP_G_IO_READ                         ((uint32_t)0x02000000)
#define GENDEV_DEST_OP_G_DC_FLUSH                        ((uint32_t)0x04000000)
#define GENDEV_DEST_OP_G_CASTOUT                         ((uint32_t)0x08000000)
#define GENDEV_DEST_OP_G_DC_INVALIDATE                   ((uint32_t)0x10000000)
#define GENDEV_DEST_OP_G_READ_OWN                        ((uint32_t)0x20000000)
#define GENDEV_DEST_OP_G_IREAD                           ((uint32_t)0x40000000)
#define GENDEV_DEST_OP_G_READ                            ((uint32_t)0x80000000)

/* GENDEV_SR_XADDR : Register Bits Masks Definitions */
#define GENDEV_SR_XADDR_EA_CTL                           ((uint32_t)0x00000007)

/* GENDEV_BASE_ID : Register Bits Masks Definitions */
#define GENDEV_BASE_ID_LAR_BASE_ID                       ((uint32_t)0x0000ffff)
#define GENDEV_BASE_ID_BASE_ID                           ((uint32_t)0x00ff0000)

/* GENDEV_HOST_BASE_ID_LOCK : Register Bits Masks Definitions */
#define GENDEV_HOST_BASE_ID_LOCK_HOST_BASE_ID            ((uint32_t)0x0000ffff)

/* GENDEV_COMP_TAG : Register Bits Masks Definitions */
#define GENDEV_COMP_TAG_CTAG                             ((uint32_t)0xffffffff)

/* GENDEV_SP_MB_HEAD : Register Bits Masks Definitions */
#define GENDEV_SP_MB_HEAD_EF_ID                          ((uint32_t)0x0000ffff)
#define GENDEV_SP_MB_HEAD_EF_PTR                         ((uint32_t)0xffff0000)

/* GENDEV_SP_LT_CTL : Register Bits Masks Definitions */
#define GENDEV_SP_LT_CTL_TVAL                            ((uint32_t)0xffffff00)

/* GENDEV_SR_RSP_TO : Register Bits Masks Definitions */
#define GENDEV_SR_RSP_TO_RSP_TO                          ((uint32_t)0x00ffffff)

/* GENDEV_SP_GEN_CTL : Register Bits Masks Definitions */
#define GENDEV_SP_GEN_CTL_DISC                           ((uint32_t)0x20000000)
#define GENDEV_SP_GEN_CTL_MAST_EN                        ((uint32_t)0x40000000)
#define GENDEV_SP_GEN_CTL_HOST                           ((uint32_t)0x80000000)

/* GENDEV_SP_LM_REQ : Register Bits Masks Definitions */
#define GENDEV_SP_LM_REQ_CMD                             ((uint32_t)0x00000007)

/* GENDEV_SP_LM_RESP : Register Bits Masks Definitions */
#define GENDEV_SP_LM_RESP_LINK_STAT                      ((uint32_t)0x0000001f)
#define GENDEV_SP_LM_RESP_ACK_ID_STAT                    ((uint32_t)0x000007e0)
#define GENDEV_SP_LM_RESP_RESP_VLD                       ((uint32_t)0x80000000)

/* GENDEV_SP_ACKID_STAT : Register Bits Masks Definitions */
#define GENDEV_SP_ACKID_STAT_OUTB_ACKID                  ((uint32_t)0x0000003f)
#define GENDEV_SP_ACKID_STAT_OUTSTD_ACKID                ((uint32_t)0x00003f00)
#define GENDEV_SP_ACKID_STAT_INB_ACKID                   ((uint32_t)0x3f000000)
#define GENDEV_SP_ACKID_STAT_CLR_OUTSTD_ACKID            ((uint32_t)0x80000000)

/* GENDEV_SP_CTL2 : Register Bits Masks Definitions */
#define GENDEV_SP_CTL2_RTEC_EN                           ((uint32_t)0x00000001)
#define GENDEV_SP_CTL2_RTEC                              ((uint32_t)0x00000002)
#define GENDEV_SP_CTL2_D_SCRM_DIS                        ((uint32_t)0x00000004)
#define GENDEV_SP_CTL2_INACT_EN                          ((uint32_t)0x00000008)
#define GENDEV_SP_CTL2_GB_6P25_EN                        ((uint32_t)0x00010000)
#define GENDEV_SP_CTL2_GB_6P25                           ((uint32_t)0x00020000)
#define GENDEV_SP_CTL2_GB_5P0_EN                         ((uint32_t)0x00040000)
#define GENDEV_SP_CTL2_GB_5P0                            ((uint32_t)0x00080000)
#define GENDEV_SP_CTL2_GB_3P125_EN                       ((uint32_t)0x00100000)
#define GENDEV_SP_CTL2_GB_3P125                          ((uint32_t)0x00200000)
#define GENDEV_SP_CTL2_GB_2P5_EN                         ((uint32_t)0x00400000)
#define GENDEV_SP_CTL2_GB_2P5                            ((uint32_t)0x00800000)
#define GENDEV_SP_CTL2_GB_1P25_EN                        ((uint32_t)0x01000000)
#define GENDEV_SP_CTL2_GB_1P25                           ((uint32_t)0x02000000)
#define GENDEV_SP_CTL2_BAUD_DISC                         ((uint32_t)0x08000000)
#define GENDEV_SP_CTL2_BAUD_SEL                          ((uint32_t)0xf0000000)

/* GENDEV_SP_ERR_STAT : Register Bits Masks Definitions */
#define GENDEV_SP_ERR_STAT_PORT_UNIT                     ((uint32_t)0x00000001)
#define GENDEV_SP_ERR_STAT_PORT_OK                       ((uint32_t)0x00000002)
#define GENDEV_SP_ERR_STAT_PORT_ERR                      ((uint32_t)0x00000004)
#define GENDEV_SP_ERR_STAT_PORT_UNAVL                    ((uint32_t)0x00000008)
#define GENDEV_SP_ERR_STAT_PORT_W_P                      ((uint32_t)0x00000010)
#define GENDEV_SP_ERR_STAT_INPUT_ERR_STOP                ((uint32_t)0x00000100)
#define GENDEV_SP_ERR_STAT_INPUT_ERR_ENCTR               ((uint32_t)0x00000200)
#define GENDEV_SP_ERR_STAT_INPUT_RS                      ((uint32_t)0x00000400)
#define GENDEV_SP_ERR_STAT_OUTPUT_ERR_STOP               ((uint32_t)0x00010000)
#define GENDEV_SP_ERR_STAT_OUTPUT_ERR_ENCTR              ((uint32_t)0x00020000)
#define GENDEV_SP_ERR_STAT_OUTPUT_RS                     ((uint32_t)0x00040000)
#define GENDEV_SP_ERR_STAT_OUTPUT_R                      ((uint32_t)0x00080000)
#define GENDEV_SP_ERR_STAT_OUTPUT_RE                     ((uint32_t)0x00100000)
#define GENDEV_SP_ERR_STAT_OUTPUT_DEGR                   ((uint32_t)0x01000000)
#define GENDEV_SP_ERR_STAT_OUTPUT_FAIL                   ((uint32_t)0x02000000)
#define GENDEV_SP_ERR_STAT_OUTPUT_DROP                   ((uint32_t)0x04000000)
#define GENDEV_SP_ERR_STAT_TXFC                          ((uint32_t)0x08000000)
#define GENDEV_SP_ERR_STAT_IDLE_SEQ                      ((uint32_t)0x20000000)
#define GENDEV_SP_ERR_STAT_IDLE2_EN                      ((uint32_t)0x40000000)
#define GENDEV_SP_ERR_STAT_IDLE2                         ((uint32_t)0x80000000)

/* GENDEV_SP_CTL : Register Bits Masks Definitions */
#define GENDEV_SP_CTL_PTYP                               ((uint32_t)0x00000001)
#define GENDEV_SP_CTL_PORT_LOCKOUT                       ((uint32_t)0x00000002)
#define GENDEV_SP_CTL_DROP_EN                            ((uint32_t)0x00000004)
#define GENDEV_SP_CTL_STOP_FAIL_EN                       ((uint32_t)0x00000008)
#define GENDEV_SP_CTL_PORT_WIDTH2                        ((uint32_t)0x00003000)
#define GENDEV_SP_CTL_OVER_PWIDTH2                       ((uint32_t)0x0000c000)
#define GENDEV_SP_CTL_FLOW_ARB                           ((uint32_t)0x00010000)
#define GENDEV_SP_CTL_ENUM_B                             ((uint32_t)0x00020000)
#define GENDEV_SP_CTL_FLOW_CTRL                          ((uint32_t)0x00040000)
#define GENDEV_SP_CTL_MULT_CS                            ((uint32_t)0x00080000)
#define GENDEV_SP_CTL_ERR_DIS                            ((uint32_t)0x00100000)
#define GENDEV_SP_CTL_INP_EN                             ((uint32_t)0x00200000)
#define GENDEV_SP_CTL_OTP_EN                             ((uint32_t)0x00400000)
#define GENDEV_SP_CTL_PORT_DIS                           ((uint32_t)0x00800000)
#define GENDEV_SP_CTL_OVER_PWIDTH                        ((uint32_t)0x07000000)
#define GENDEV_SP_CTL_INIT_PWIDTH                        ((uint32_t)0x38000000)
#define GENDEV_SP_CTL_PORT_WIDTH                         ((uint32_t)0xc0000000)

#ifdef __cplusplus
}
#endif

#endif /* __GENDEV_H__ */
