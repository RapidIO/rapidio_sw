/*
 * ****************************************************************************
 * Copyright (c) 2016, Integrated Device Technology Inc.
 * Copyright (c) 2016, RapidIO Trade Association
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *************************************************************************
 * */

#ifndef _IDT_RXS2448_H_
#define _IDT_RXS2448_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RXS2448_MAX_PORTS                                     23
#define RXS2448_MAX_LANES                                     47
#define RXS2448_RIO_DEVICE_ID                                 (0x000080e6)
#define RXS1632_RIO_DEVICE_ID                                 (0x000080e5)

/* ************************************************ */
/* RXS2448 : Register address offset definitions    */
/* ************************************************ */
#define RXS_RIO_DEV_ID                                        (0x00000000)
#define RXS_RIO_SP0_LM_REQ                                    (0x00000140)
#define RXS_RIO_SP0_LM_RESP                                   (0x00000144)
#define RXS_RIO_SP0_CTL2                                      (0x00000154)
#define RXS_RIO_SP0_ERR_STAT                                  (0x00000158)
#define RXS_RIO_SP0_CTL                                       (0x0000015c)
#define RXS_RIO_SP_LT_CTL                                     (0x00000120)
#define RXS_RIO_SR_RSP_TO                                     (0x00000124)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL                          (0x00010100)
#define RXS_RIO_PW_TGT_ID                                     (0x00001028)

#define RXS_RIO_PCNTR_CTL                                     (0x1c004)
#define RXS_RIO_SPX_PCNTR_EN(X)                               (0x1c100 +(0x100*(X)))
#define RXS_RIO_SPX_PCNTR_CTL(X,Y)                            (0x1c110 +(0x100*(X))+(4*Y))
#define RXS_RIO_SPX_PCNTR_CNT(X,Y)                            (0x1c130 +(0x100*(X))+(4*Y))

#define RXS_RIO_EM_RST_INT_EN                                 (0x00040068)
#define RXS_RIO_EM_RST_PW_EN                                  (0x00040070)
#define RXS_RIO_PW_CTL                                        (0x00040204)

/* RXS_RIO_DEV_ID : Register Bits Masks Definitions */
#define RXS_RIO_DEV_IDENT_VEND                                (0x0000ffff)
#define RXS_RIO_DEV_IDENT_DEVI                                (0xffff0000)
#define RXS_RIO_DEVICE_VENDOR                                 (0x00000038)

/* RXS_RIO_SP0_LM_RESP : Register Bits Masks Definitions */
#define RXS_RIO_SP0_LM_RESP_LINK_STAT                         (0x0000001f)
#define RXS_RIO_SP0_LM_RESP_ACK_ID_STAT                       (0x0001ffe0)
#define RXS_RIO_SP0_LM_RESP_STAT_CS64                         (0x01ffe000)
#define RXS_RIO_SP0_LM_RESP_RESP_VLD                          (0x80000000)

/* RXS_RIO_SP0_CTL2 : Register Bits Masks Definitions */
#define RXS_RIO_SP0_CTL2_RTEC_EN                              (0x00000001)
#define RXS_RIO_SP0_CTL2_RTEC                                 (0x00000002)
#define RXS_RIO_SP0_CTL2_D_SCRM_DIS                           (0x00000004)
#define RXS_RIO_SP0_CTL2_INACT_EN                             (0x00000008)
#define RXS_RIO_SP0_CTL2_D_RETRAIN_EN                         (0x00000010)
#define RXS_RIO_SP0_CTL2_GB_12p5_EN                           (0x00001000)
#define RXS_RIO_SP0_CTL2_GB_12p5                              (0x00002000)
#define RXS_RIO_SP0_CTL2_GB_10p3_EN                           (0x00004000)
#define RXS_RIO_SP0_CTL2_GB_10p3                              (0x00008000)
#define RXS_RIO_SP0_CTL2_GB_6p25_EN                           (0x00010000)
#define RXS_RIO_SP0_CTL2_GB_6p25                              (0x00020000)
#define RXS_RIO_SP0_CTL2_GB_5p0_EN                            (0x00040000)
#define RXS_RIO_SP0_CTL2_GB_5p0                               (0x00080000)
#define RXS_RIO_SP0_CTL2_GB_3p125_EN                          (0x00100000)
#define RXS_RIO_SP0_CTL2_GB_3p125                             (0x00200000)
#define RXS_RIO_SP0_CTL2_GB_2p5_EN                            (0x00400000)
#define RXS_RIO_SP0_CTL2_GB_2p5                               (0x00800000)
#define RXS_RIO_SP0_CTL2_GB_1p25_EN                           (0x01000000)
#define RXS_RIO_SP0_CTL2_GB_1p25                              (0x02000000)
#define RXS_RIO_SP0_CTL2_BAUD_DISC                            (0x08000000)
#define RXS_RIO_SP0_CTL2_BAUD_SEL                             (0xf0000000)

/* RXS_RIO_SP0_ERR_STAT : Register Bits Masks Definitions */
#define RXS_RIO_SP0_ERR_STAT_PORT_UNIT                        (0x00000001)
#define RXS_RIO_SP0_ERR_STAT_PORT_OK                          (0x00000002)
#define RXS_RIO_SP0_ERR_STAT_PORT_ERR                         (0x00000004)
#define RXS_RIO_SP0_ERR_STAT_PORT_UNAVL                       (0x00000008)
#define RXS_RIO_SP0_ERR_STAT_PORT_W_P                         (0x00000010)
#define RXS_RIO_SP0_ERR_STAT_INPUT_ERR_STOP                   (0x00000100)
#define RXS_RIO_SP0_ERR_STAT_INPUT_ERR_ENCTR                  (0x00000200)
#define RXS_RIO_SP0_ERR_STAT_INPUT_RS                         (0x00000400)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_ERR_STOP                  (0x00010000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_ERR_ENCTR                 (0x00020000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_RS                        (0x00040000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_R                         (0x00080000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_RE                        (0x00100000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_DEGR                      (0x01000000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_FAIL                      (0x02000000)
#define RXS_RIO_SP0_ERR_STAT_OUTPUT_DROP                      (0x04000000)
#define RXS_RIO_SP0_ERR_STAT_TXFC                             (0x08000000)
#define RXS_RIO_SP0_ERR_STAT_IDLE_SEQ                         (0x20000000)
#define RXS_RIO_SP0_ERR_STAT_IDLE2_EN                         (0x40000000)
#define RXS_RIO_SP0_ERR_STAT_IDLE2                            (0x80000000)

/* RXS_RIO_SP0_CTL : Register Bits Masks Definitions */
#define RXS_RIO_SP0_CTL_PTYP                                  (0x00000001)
#define RXS_RIO_SP0_CTL_PORT_LOCKOUT                          (0x00000002)
#define RXS_RIO_SP0_CTL_DROP_EN                               (0x00000004)
#define RXS_RIO_SP0_CTL_STOP_FAIL_EN                          (0x00000008)
#define RXS_RIO_SP0_CTL_PORT_WIDTH2                           (0x00003000)
#define RXS_RIO_SP0_CTL_OVER_PWIDTH2                          (0x0000c000)
#define RXS_RIO_SP0_CTL_FLOW_ARB                              (0x00010000)
#define RXS_RIO_SP0_CTL_ENUM_B                                (0x00020000)
#define RXS_RIO_SP0_CTL_FLOW_CTRL                             (0x00040000)
#define RXS_RIO_SP0_CTL_MULT_CS                               (0x00080000)
#define RXS_RIO_SP0_CTL_ERR_DIS                               (0x00100000)
#define RXS_RIO_SP0_CTL_INP_EN                                (0x00200000)
#define RXS_RIO_SP0_CTL_OTP_EN                                (0x00400000)
#define RXS_RIO_SP0_CTL_PORT_DIS                              (0x00800000)
#define RXS_RIO_SP0_CTL_OVER_PWIDTH                           (0x07000000)
#define RXS_RIO_SP0_CTL_INIT_PWIDTH                           (0x38000000)
#define RXS_RIO_SP0_CTL_PORT_WIDTH                            (0xc0000000)

/* RXS_RIO_PLM_SP0_IMP_SPEC_CTL : Register Bits Masks Definitions */
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_PA_BKLOG_THRESH          (0x0000003f)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_CONT_PNA                 (0x00000040)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_CONT_LR                  (0x00000080)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_BLIP_CS                  (0x00000100)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_LOST_CS_DIS              (0x00000200)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_INFER_SELF_RST           (0x00000400)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_DLT_FATAL                (0x00000800)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_PRE_SILENCE              (0x00001000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_OK2U_FATAL               (0x00002000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_MAXD_FATAL               (0x00004000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_DWNGD_FATAL              (0x00008000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_RX                  (0x00030000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_TX                  (0x000c0000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SELF_RST                 (0x00100000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_PORT_SELF_RST            (0x00200000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_RESET_REG                (0x00400000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_LLB_EN                   (0x00800000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_CS_FIELD1                (0x01000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SOFT_RST_PORT            (0x02000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_FORCE_REINIT             (0x04000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_DME_TRAINING             (0x08000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_DLB_EN                   (0x10000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_USE_IDLE1                (0x20000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_USE_IDLE2                (0x40000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_USE_IDLE3                (0x80000000)

#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_RX_NONE             (0x00000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_RX_1032             (0x00010000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_RX_3210             (0x00020000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_RX_2301             (0x00030000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_TX_NONE             (0x00000000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_TX_1032             (0x00040000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_TX_3210             (0x00080000)
#define RXS_RIO_PLM_SP0_IMP_SPEC_CTL_SWAP_TX_2301             (0x000C0000)

/* RXS_RIO_PW_TGT_ID : Register Bits Masks Definitions */
#define RXS_RIO_PW_TGT_ID_DEV32                               (0x00004000)
#define RXS_RIO_PW_TGT_ID_DEV16                               (0x00008000)
#define RXS_RIO_PW_TGT_ID_PW_TGT_ID                           (0x00ff0000)
#define RXS_RIO_PW_TGT_ID_MSB_PW_ID                           (0xff000000)

/* RXS_RIO_SPX_PCNTR_EN : Register Bits Masks Definitions */
#define RXS_RIO_SPX_PCNTR_EN_ENABLE                           (0x80000000)

/* RXS_RIO_SPX_PCNTR_CTL : Register Bits Masks Definitions */
#define RXS_RIO_SPX_PCNTR_CTL_TX                              (0x00000080)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO0                           (0x00000100)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO0C                          (0x00000200)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO1                           (0x00000400)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO1C                          (0x00000800)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO2                           (0x00001000)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO2C                          (0x00002000)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO3                           (0x00004000)
#define RXS_RIO_SPX_PCNTR_CTL_PRIO3C                          (0x00008000)

/* RXS_RIO_EM_RST_INT_EN : Register Bits Masks Definitions */
#define RXS_RIO_EM_RST_INT_EN_RST_INT_EN                      (0x00ffffff)

/* RXS_RIO_EM_RST_PW_EN : Register Bits Masks Definitions */
#define RXS_RIO_EM_RST_PW_EN_RST_PW_EN                        (0x00ffffff)

/* RXS_RIO_PW_CTL : Register Bits Masks Definitions */
#define RXS_RIO_PW_CTL_PW_TMR                                 (0xffffff00)

#ifdef __cplusplus
}
#endif

#endif /* _IDT_RXS2448_H_  */

