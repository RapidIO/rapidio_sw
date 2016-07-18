/***************************************************************************
**
** RXS2448/1632 Header File
** 
** March ??, 2016
** 
** DISCLAIMER Integrated Device Technology, Inc. (IDT) reserves the right to 
** modify the products and/or specifications described herein at any time, 
** without notice, at IDT's sole discretion. Performance specifications and
** operating parameters of the described products are determined in an 
** independent state and are not guaranteed to perform the same way when 
** installed in customer products. The information contained herein is provided
** without representation or warranty of any kind, whether express or implied,
** including, but not limited to, the suitability of IDT's products for any 
** particular purpose, an implied warranty of merchantability, or 
** non-infringement of the intellectual property rights of others. This 
** document is presented only as a guide and does not convey any license under
** intellectual property rights of IDT or any third parties. IDT's products
** are not intended for use in applications involving extreme environmental
** conditions or in life support systems or similar devices where the failure
** or malfunction of an IDT product can be reasonably expected to significantly
** affect the health or safety of users. Anyone using an IDT product in such a
** manner does so at their own risk, absent an express, written agreement by IDT.
** 
** Copyright ©2016 Integrated Device Technology, Inc. All rights reserved. 
** 
***************************************************************************/

/*****************************************************/
/* RapidIO Register Address Offset Definitions       */
/*****************************************************/

#define DEV_ID                                         (0x00000000)
#define DEV_INFO   	                               (0x00000004)
#define ASBLY_ID                                       (0x00000008)
#define ASBLY_INFO                                     (0x0000000c)
#define PE_FEAT                                        (0x00000010)
#define SW_PORT                                        (0x00000014)
#define SRC_OP                                         (0x00000018)
#define SW_RT_DEST_ID                                  (0x00000034)
#define PE_LL_CTL                                      (0x0000004c)
#define HOST_BASE_ID_LOCK                              (0x00000068)
#define COMP_TAG                                       (0x0000006c)
#define ROUTE_CFG_DESTID                               (0x00000070)
#define ROUTE_CFG_PORT                                 (0x00000074)
#define ROUTE_DFLT_PORT                                (0x00000078)
#define SP_MB_HEAD                                     (0x00000100)
#define SP_LT_CTL                                      (0x00000120)
#define SP_GEN_CTL                                     (0x0000013c)
#define SPX_LM_REQ(X)                          (0x0140 + 0x040*(X))
#define SPX_LM_RESP(X)                         (0x0144 + 0x040*(X))
#define SPX_CTL2(X)                            (0x0154 + 0x040*(X))
#define SPX_ERR_STAT(X)                        (0x0158 + 0x040*(X))
#define SPX_CTL(X)                             (0x015c + 0x040*(X))
#define SPX_OUT_ACKID_CSR(X)                   (0x0160 + 0x040*(X))
#define SPX_IN_ACKID_CSR(X)                    (0x0164 + 0x040*(X))
#define SPX_POWER_MNGT_CSR(X)                  (0x0168 + 0x040*(X))
#define SPX_LAT_OPT_CSR(X)                     (0x016c + 0x040*(X))
#define SPX_TMR_CTL(X)                         (0x0170 + 0x040*(X))
#define SPX_TMR_CTL2(X)                        (0x0174 + 0x040*(X))
#define SPX_TMR_CTL3(X)                        (0x0178 + 0x040*(X))
#define ERR_RPT_BH                                     (0x00001000)
#define ERR_MGMT_HS                                    (0x00001004)
#define ERR_DET                                        (0x00001008)
#define ERR_EN                                         (0x0000100c)
#define H_ADDR_CAPT                                    (0x00001010)
#define ADDR_CAPT                                      (0x00001014)
#define ID_CAPT                                        (0x00001018)
#define CTRL_CAPT                                      (0x0000101c)
#define DEV32_DESTID_CAPT                              (0x00001020)
#define DEV32_SRCID_CAPT                               (0x00001024)
#define PW_TGT_ID                                      (0x00001028)
#define PKT_TIME_LIVE                                  (0x0000102c)
#define DEV32_PW_TGT_ID                                (0x00001030)
#define PW_TRAN_CTL                                    (0x00001034)
#define SPX_ERR_DET(X)                         (0x1040 + 0x040*(X))
#define SPX_RATE_EN(X)                         (0x1044 + 0x040*(X))
#define SPX_DLT_CSR(X)                         (0x1070 + 0x040*(X))
#define PER_LANE_BH                                    (0x00003000)
#define LANEX_STAT0(X)                         (0x3010 + 0x020*(X))
#define LANEX_STAT1(X)                         (0x3014 + 0x020*(X))
#define LANEX_STAT2(X)                         (0x3018 + 0x020*(X))
#define LANEX_STAT3(X)                         (0x301c + 0x020*(X))
#define LANEX_STAT4(X)                         (0x3020 + 0x020*(X))
#define LANEX_STAT5(X)                         (0x3024 + 0x020*(X))
#define LANEX_STAT6(X)                         (0x3028 + 0x020*(X))
#define SWITCH_RT_BH                                   (0x00005000)
#define BC_RT_CTL                                      (0x00005020)
#define BC_MC_INFO                                     (0x00005028)
#define BC_RT_LVL0_INFO                                (0x00005030)
#define BC_RT_LVL1_INFO                                (0x00005034)
#define BC_RT_LVL2_INFO                                (0x00005038)
#define SPX_RT_CTL(X)                          (0x5040 + 0x020*(X))
#define SPX_MC_INFO(X)                         (0x5048 + 0x020*(X))
#define SPX_RT_LVL0_INFO(X)                    (0x5050 + 0x020*(X))
#define SPX_RT_LVL1_INFO(X)                    (0x5054 + 0x020*(X))
#define SPX_RT_LVL2_INFO(X)                    (0x5058 + 0x020*(X))
#define PLM_BH                                         (0x00010000)
#define PLM_SPX_IMP_SPEC_CTL(X)               (0x10100 + 0x100*(X))
#define PLM_SPX_PWDN_CTL(X)                   (0x10104 + 0x100*(X))
#define PLM_SPX_1WR(X)                        (0x10108 + 0x100*(X))
#define PLM_SPX_MULT_ACK_CTL(X)               (0x1010c + 0x100*(X))
#define PLM_SPX_STAT(X)                       (0x10110 + 0x100*(X))
#define PLM_SPX_INT_EN(X)                     (0x10114 + 0x100*(X))
#define PLM_SPX_PW_EN(X)                      (0x10118 + 0x100*(X))
#define PLM_SPX_EVENT_GEN(X)                  (0x1011c + 0x100*(X))
#define PLM_SPX_ALL_INT_EN(X)                 (0x10120 + 0x100*(X))
#define PLM_SPX_PATH_CTL(X)                   (0x1012c + 0x100*(X))
#define PLM_SPX_SILENCE_TMR(X)                (0x10138 + 0x100*(X))
#define PLM_SPX_VMIN_EXP(X)                   (0x1013c + 0x100*(X))
#define PLM_SPX_POL_CTL(X)                        (0x10140 + 0x100*(X))
#define PLM_SPX_CLKCOMP_CTL(X)                (0x10144 + 0x100*(X))
#define PLM_SPX_DENIAL_CTL(X)                 (0x10148 + 0x100*(X))
#define PLM_SPX_ERR_REC_CTL(X)                (0x1014c + 0x100*(X))
#define PLM_SPX_CS_TX1(X)                     (0x10160 + 0x100*(X))
#define PLM_SPX_CS_TX2(X)                     (0x10164 + 0x100*(X))
#define PLM_SPX_PNA_CAP(X)                    (0x10168 + 0x100*(X))
#define PLM_SPX_ACKID_CAP(X)                  (0x1016c + 0x100*(X))
#define PLM_SPX_SCRATCHY(X,Y)       (0x10190 + 0x100*(X) + 0x4*(Y))
#define TLM_BH                                         (0x00014000)
#define TLM_SPX_CONTROL(X)                    (0x14100 + 0x100*(X))
#define TLM_SPX_STAT(X)                       (0x14110 + 0x100*(X))
#define TLM_SPX_INT_EN(X)                     (0x14114 + 0x100*(X))
#define TLM_SPX_PW_EN(X)                      (0x14118 + 0x100*(X))
#define TLM_SPX_EVENT_GEN(X)                  (0x1411c + 0x100*(X))
#define TLM_SPX_FTYPE_FILT(X)                 (0x14160 + 0x100*(X))
#define TLM_SPX_FTYPE_CAPT(X)                 (0x14164 + 0x100*(X))
#define TLM_SPX_MTC_ROUTE_EN(X)               (0x14170 + 0x100*(X))
#define TLM_SPX_ROUTE_EN(X)                   (0x14174 + 0x100*(X))
#define PBM_BH                                         (0x00018000)
#define PBM_SPX_CONTROL(X)                    (0x18100 + 0x100*(X))
#define PBM_SPX_STAT(X)                       (0x18110 + 0x100*(X))
#define PBM_SPX_INT_EN(X)                     (0x18114 + 0x100*(X))
#define PBM_SPX_PW_EN(X)                      (0x18118 + 0x100*(X))
#define PBM_SPX_EVENT_GEN(X)                  (0x1811c + 0x100*(X))
#define PBM_SPX_EG_RESOURCES(X)               (0x18124 + 0x100*(X))
#define PBM_SPX_BUFF_uint32_t(X)                (0x18170 + 0x100*(X))
#define PBM_SPX_SCRATCH1(X)                   (0x18180 + 0x100*(X))
#define PBM_SPX_SCRATCH2(X)                   (0x18184 + 0x100*(X))
#define PCNTR_BH                                       (0x0001c000)
#define PCNTR_CTL                                      (0x0001c004)
#define SPX_PCNTR_EN(X)                       (0x1c100 + 0x100*(X))
#define SPX_PCNTR_CTLY(X,Y)         (0x1c110 + 0x100*(X) + 0x4*(Y))
#define SPX_PCNTR_CNTY(X,Y)         (0x1c130 + 0x100*(X) + 0x4*(Y))
#define PCAP_BH                                        (0x00020000)
#define SPX_PCAP_ACC(X)                       (0x20100 + 0x100*(X))
#define SPX_PCAP_CTL(X)                       (0x20104 + 0x100*(X))
#define SPX_PCAP_STAT(X)                      (0x20110 + 0x100*(X))
#define SPX_PCAP_GEN(X)                       (0x2011c + 0x100*(X))
#define SPX_PCAP_SB_DATA(X)                   (0x20120 + 0x100*(X))
#define SPX_PCAP_MEM_DEPTH(X)                 (0x20124 + 0x100*(X))
#define SPX_PCAP_DATAY(X,Y)         (0x20130 + 0x100*(X) + 0x4*(Y))
#define DBG_EL_BH                                      (0x00024000)
#define SPX_DBG_EL_ACC(X)                     (0x24100 + 0x100*(X))
#define SPX_DBG_EL_INFO(X)                    (0x24104 + 0x100*(X))
#define SPX_DBG_EL_CTL(X)                     (0x24108 + 0x100*(X))
#define SPX_DBG_EL_INT_EN(X)                  (0x2410c + 0x100*(X))
#define SPX_DBG_EL_SRC_LOG_EN(X)              (0x24110 + 0x100*(X))
#define SPX_DBG_EL_TRIG0_MASK(X)              (0x24114 + 0x100*(X))
#define SPX_DBG_EL_TRIG0_VAL(X)               (0x24118 + 0x100*(X))
#define SPX_DBG_EL_TRIG1_MASK(X)              (0x2411c + 0x100*(X))
#define SPX_DBG_EL_TRIG1_VAL(X)               (0x24120 + 0x100*(X))
#define SPX_DBG_EL_TRIG_STAT(X)               (0x24124 + 0x100*(X))
#define SPX_DBG_EL_WR_TRIG_IDX(X)             (0x24128 + 0x100*(X))
#define SPX_DBG_EL_RD_IDX(X)                  (0x2412c + 0x100*(X))
#define SPX_DBG_EL_DATA(X)                    (0x24130 + 0x100*(X))
#define SPX_DBG_EL_SRC_STATY(X,Y)   (0x24140 + 0x100*(X) + 0x4*(Y))
#define SPX_DBG_EL_SRC_STAT4(X)               (0x24150 + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT5(X)               (0x24154 + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT6(X)               (0x24158 + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT7(X)               (0x2415c + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT8(X)               (0x24160 + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT9(X)               (0x24164 + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT10(X)              (0x24168 + 0x100*(X))
#define SPX_DBG_EL_SRC_STAT11(X)              (0x2416c + 0x100*(X))
#define SPX_DBG_EL_SRC_STATY(X,Y)  (0x24170 + 0xffffffe9*(X) + 0x000*(Y))
#define LANE_TEST_BH                                   (0x00028000)
#define LANEX_PRBS_CTRL(X)                    (0x28104 + 0x100*(X))
#define LANEX_PRBS_uint32_t(X)                  (0x28108 + 0x100*(X))
#define LANEX_PRBS_ERR_CNT(X)                 (0x28120 + 0x100*(X))
#define LANEX_PRBS_SEED_0U(X)                 (0x28124 + 0x100*(X))
#define LANEX_PRBS_SEED_0M(X)                 (0x28128 + 0x100*(X))
#define LANEX_PRBS_SEED_0L(X)                 (0x2812c + 0x100*(X))
#define LANEX_PRBS_SEED_1U(X)                 (0x28134 + 0x100*(X))
#define LANEX_PRBS_SEED_1M(X)                 (0x28138 + 0x100*(X))
#define LANEX_PRBS_SEED_1L(X)                 (0x2813c + 0x100*(X))
#define LANEX_BER_CTL(X)                      (0x28140 + 0x100*(X))
#define LANEX_BER_DATA_0(X)                   (0x28144 + 0x100*(X))
#define LANEX_BER_DATA_1(X)                   (0x28148 + 0x100*(X))
#define LANEX_PCS_DBG(X)                      (0x28150 + 0x100*(X))
#define LANEX_BERM_CTL(X)                     (0x28160 + 0x100*(X))
#define LANEX_BERM_CNTR(X)                    (0x28164 + 0x100*(X))
#define LANEX_BERM_PD(X)                      (0x28168 + 0x100*(X))
#define LANEX_BERM_BITS(X)                    (0x2816c + 0x100*(X))
#define LANEX_BERM_ERRORS(X)                  (0x28170 + 0x100*(X))
#define LANEX_BERM_PERIODS(X)                 (0x28174 + 0x100*(X))
#define LANEX_DME_TEST(X)                     (0x281fc + 0x100*(X))
#define FAB_PORT_BH                                    (0x0002c000)
#define FP_X_IB_BUFF_WM_01(X)                 (0x2c100 + 0x100*(X))
#define FP_X_IB_BUFF_WM_23(X)                 (0x2c104 + 0x100*(X))
#define FP_X_PLW_SCRATCH(X)                   (0x2c108 + 0x100*(X))
#define BC_L0_G0_ENTRYX_CSR(X)                (0x30000 + 0x004*(X))
#define BC_L1_GX_ENTRYY_CSR(X,Y)    (0x30400 + 0x400*(X) + 0x4*(Y))
#define BC_L2_GX_ENTRYY_CSR(X,Y)    (0x31000 + 0x400*(X) + 0x4*(Y))
#define BC_MC_X_S_CSR(X)                      (0x32000 + 0x008*(X))
#define BC_MC_X_C_CSR(X)                      (0x32004 + 0x008*(X))
#define FAB_INGR_CTL_BH                                (0x00034000)
#define FAB_IG_X_2X4X_4X2X_WM(X)              (0x34104 + 0x100*(X))
#define FAB_IG_X_2X2X_WM(X)                   (0x34108 + 0x100*(X))
#define FAB_IG_X_4X4X_WM(X)                   (0x34110 + 0x100*(X))
#define FAB_IG_X_MTC_VOQ_ACT(X)               (0x34120 + 0x100*(X))
#define FAB_IG_X_VOQ_ACT(X)                   (0x34130 + 0x100*(X))
#define FAB_IG_X_CTL(X)                       (0x34140 + 0x100*(X))
#define FAB_IG_X_SCRATCH(X)                   (0x341f8 + 0x100*(X))
#define EM_BH                                          (0x00040000)
#define EM_INT_STAT                                    (0x00040010)
#define EM_INT_EN                                      (0x00040014)
#define EM_INT_PORT_STAT                               (0x00040018)
#define EM_PW_STAT                                     (0x00040020)
#define EM_PW_EN                                       (0x00040024)
#define EM_PW_PORT_STAT                                (0x00040028)
#define EM_DEV_INT_EN                                  (0x00040030)
#define EM_EVENT_GEN                                   (0x00040034)
#define EM_MECS_CTL                                    (0x00040038)
#define EM_MECS_INT_EN                                 (0x00040040)
#define EM_MECS_PORT_STAT                              (0x00040050)
#define EM_RST_PORT_STAT                               (0x00040060)
#define EM_RST_INT_EN                                  (0x00040068)
#define EM_RST_PW_EN                                   (0x00040070)
#define EM_FAB_INT_STAT                                (0x00040080)
#define EM_FAB_PW_STAT                                 (0x00040088)
#define PW_BH                                          (0x00040200)
#define PW_CTL                                         (0x00040204)
#define PW_ROUTE                                       (0x00040208)
#define MPM_BH                                         (0x00040400)
#define RB_RESTRICT                                    (0x00040404)
#define MTC_WR_RESTRICT                                (0x00040408)
#define MTC_RD_RESTRICT                                (0x00040418)
#define MPM_SCRATCH1                                   (0x00040424)
#define PORT_NUMBER                                    (0x00040428)
#define PRESCALAR_SRV_CLK                              (0x00040430)
#define REG_RST_CTL                                    (0x00040434)
#define MPM_SCRATCH2                                   (0x00040438)
#define ASBLY_ID_OVERRIDE                              (0x00040470)
#define ASBLY_INFO_OVERRIDE                            (0x00040474)
#define MPM_MTC_RESP_PRIO                              (0x00040478)
#define MPM_MTC_ACTIVE                                 (0x0004047c)
#define MPM_CFGSIG0                                    (0x000404f0)
#define MPM_CFGSIG1                                    (0x000404f4)
#define FAB_GEN_BH                                     (0x00040600)
#define FAB_GLOBAL_MEM_PWR_MODE                        (0x00040700)
#define FAB_GLOBAL_CLK_GATE                            (0x00040708)
#define FAB_4X_MODE                                    (0x00040720)
#define FAB_MBCOL_ACT                                  (0x00040760)
#define FAB_MIG_ACT                                    (0x00040770)
#define PHY_SERIAL_IF_EN                               (0x00040780)
#define PHY_TX_DISABLE_CTRL                            (0x00040784)
#define PHY_LOOPBACK_CTRL                              (0x00040788)
#define PHY_PORT_OK_CTRL                               (0x0004078c)
#define MCM_ROUTE_EN                                   (0x000407a0)
#define BOOT_CTL                                       (0x000407c0)
#define FAB_GLOBAL_PWR_GATE_CLR                        (0x000407e8)
#define FAB_GLOBAL_PWR_GATE                            (0x000407ec)
#define SYS_PLL_CFG_1                                  (0x000407f0)
#define SYS_PLL_CFG_2                                  (0x000407f4)
#define SYS_PLL_CFG_3                                  (0x000407f8)
#define SYS_PLL_uint32_t                                 (0x000407fc)
#define DEL_BH                                         (0x00040800)
#define DEL_ACC                                        (0x00040804)
#define DEL_INFO                                       (0x00040984)
#define DEL_CTL                                        (0x00040988)
#define DEL_INT_EN                                     (0x0004098c)
#define DEL_SRC_LOG_EN                                 (0x00040990)
#define DEL_TRIG0_MASK                                 (0x00040994)
#define DEL_TRIG0_VAL                                  (0x00040998)
#define DEL_TRIG1_MASK                                 (0x0004099c)
#define DEL_TRIG1_VAL                                  (0x000409a0)
#define DEL_TRIG_STAT                                  (0x000409a4)
#define DEL_WR_TRIG_IDX                                (0x000409a8)
#define DEL_RD_IDX                                     (0x000409ac)
#define DEL_DATA                                       (0x000409b0)
#define DEL_SRC_STAT0                                  (0x000409c0)
#define DEL_SRC_STAT1                                  (0x000409c4)
#define DEL_SRC_STAT2                                  (0x000409c8)
#define DEL_SRC_STAT3                                  (0x000409cc)
#define DEL_SRC_STAT4                                  (0x000409d0)
#define DEL_SRC_STATX(X)                      (0x409d4 + 0x004*(X))
#define FAB_CBCOL_BH                                   (0x00048000)
#define FAB_CBCOL_X_CTL(X)                    (0x48100 + 0x100*(X))
#define FAB_CBCOL_X_SAT_CTL(X)                (0x48120 + 0x100*(X))
#define FAB_CBCOL_X_OS_INP_EN(X)              (0x48130 + 0x100*(X))
#define FAB_CBCOL_X_ACT(X)                    (0x48140 + 0x100*(X))
#define FAB_CBCOL_X_ACT_SUMM(X)               (0x48150 + 0x100*(X))
#define FAB_CBCOL_X_PG_WM(X)                  (0x48160 + 0x100*(X))
#define FAB_CBCOL_X_ND_WM_01(X)               (0x48164 + 0x100*(X))
#define FAB_CBCOL_X_ND_WM_23(X)               (0x48168 + 0x100*(X))
#define FAB_CBCOL_X_CLK_GATE(X)               (0x48180 + 0x100*(X))
#define FAB_CBCOL_X_SCRATCH(X)                (0x481f8 + 0x100*(X))
#define PKT_GEN_BH                                     (0x0004c000)
#define FAB_PGEN_X_ACC(X)                     (0x4c100 + 0x100*(X))
#define FAB_PGEN_X_STAT(X)                    (0x4c110 + 0x100*(X))
#define FAB_PGEN_X_INT_EN(X)                  (0x4c114 + 0x100*(X))
#define FAB_PGEN_X_PW_EN(X)                   (0x4c118 + 0x100*(X))
#define FAB_PGEN_X_GEN(X)                     (0x4c11c + 0x100*(X))
#define FAB_PGEN_X_CTL(X)                     (0x4c120 + 0x100*(X))
#define FAB_PGEN_X_DATA_CTL(X)                (0x4c124 + 0x100*(X))
#define FAB_PGEN_X_DATA_Y(X,Y)      (0x4c130 + 0x100*(X) + 0x4*(Y))
#define SPX_L0_G0_ENTRYY_CSR(X,Y)  (0x50000 + 0x2000*(X) + 0x4*(Y))
#define SPX_L1_GY_ENTRYZ_CSR(X,Y,Z)  (0x50400 + 0x2000*(X) + 0x400*(Y) + 0x4*(Z))
#define SPX_L2_GY_ENTRYZ_CSR(X,Y,Z)  (0x51000 + 0x2000*(X) + 0x400*(Y) + 0x4*(Z))
#define SPX_MC_Y_S_CSR(X,Y)        (0x80000 + 0x1000*(X) + 0x8*(Y))
#define SPX_MC_Y_C_CSR(X,Y)        (0x80004 + 0x1000*(X) + 0x8*(Y))


/***************************************************************/
/* RapidIO Register Bit Masks and Reset Values Definitions     */
/***************************************************************/

/* DEV_ID : Register Bits Masks Definitions */
#define DEV_ID_DEV_VEN_ID                                (0x0000ffff)
#define DEV_ID_DEV_ID                                    (0xffff0000)

/* DEV_INFO : Register Bits Masks Definitions */
#define DEV_INFO_DEV_REV                                 (0xffffffff)

/* ASBLY_ID : Register Bits Masks Definitions */
#define ASBLY_ID_ASBLY_VEN_ID                            (0x0000ffff)
#define ASBLY_ID_ASBLY_ID                                (0xffff0000)

/* ASBLY_INFO : Register Bits Masks Definitions */
#define ASBLY_INFO_EXT_FEAT_PTR                          (0x0000ffff)
#define ASBLY_INFO_ASBLY_REV                             (0xffff0000)

/* PE_FEAT : Register Bits Masks Definitions */
#define PE_FEAT_EXT_AS                                   (0x00000007)
#define PE_FEAT_EXT_FEA                                  (0x00000008)
#define PE_FEAT_CTLS                                     (0x00000010)
#define PE_FEAT_CRF                                      (0x00000020)
#define PE_FEAT_FLOW_CTRL                                (0x00000080)
#define PE_FEAT_SRTC                                     (0x00000100)
#define PE_FEAT_ERTC                                     (0x00000200)
#define PE_FEAT_MC                                       (0x00000400)
#define PE_FEAT_FLOW_ARB                                 (0x00000800)
#define PE_FEAT_DEV32                                    (0x00001000)
#define PE_FEAT_MULT_P                                   (0x08000000)
#define PE_FEAT_SW                                       (0x10000000)
#define PE_FEAT_PROC                                     (0x20000000)
#define PE_FEAT_MEM                                      (0x40000000)
#define PE_FEAT_BRDG                                     (0x80000000)

/* SW_PORT : Register Bits Masks Definitions */
#define SW_PORT_PORT_NUM                                 (0x000000ff)
#define SW_PORT_PORT_TOTAL                               (0x0000ff00)

/* SRC_OP : Register Bits Masks Definitions */
#define SRC_OP_IMP_DEF2                                  (0x00000003)
#define SRC_OP_PORT_WR                                   (0x00000004)
#define SRC_OP_A_SWAP                                    (0x00000008)
#define SRC_OP_A_CLEAR                                   (0x00000010)
#define SRC_OP_A_SET                                     (0x00000020)
#define SRC_OP_A_DEC                                     (0x00000040)
#define SRC_OP_A_INC                                     (0x00000080)
#define SRC_OP_ATSWAP                                    (0x00000100)
#define SRC_OP_ACSWAP                                    (0x00000200)
#define SRC_OP_DBELL                                     (0x00000400)
#define SRC_OP_D_MSG                                     (0x00000800)
#define SRC_OP_WR_RES                                    (0x00001000)
#define SRC_OP_STRM_WR                                   (0x00002000)
#define SRC_OP_WRITE                                     (0x00004000)
#define SRC_OP_READ                                      (0x00008000)
#define SRC_OP_IMP_DEF                                   (0x00030000)
#define SRC_OP_DS                                        (0x00040000)
#define SRC_OP_DS_TM                                     (0x00080000)
#define SRC_OP_RSVD_11                        	         (0x00100000)
#define SRC_OP_RSVD_10                           	 (0x00200000)
#define SRC_OP_G_TLB_SYNC                                (0x00400000)
#define SRC_OP_G_TLB_INVALIDATE                          (0x00800000)
#define SRC_OP_G_IC_INVALIDATE                           (0x01000000)
#define SRC_OP_G_IO_READ                                 (0x02000000)
#define SRC_OP_G_DC_FLUSH                                (0x04000000)
#define SRC_OP_G_CASTOUT                                 (0x08000000)
#define SRC_OP_G_DC_INVALIDATE                           (0x10000000)
#define SRC_OP_G_READ_OWN                                (0x20000000)
#define SRC_OP_G_IREAD                                   (0x40000000)
#define SRC_OP_G_READ                                    (0x80000000)

/* SW_RT_DEST_ID : Register Bits Masks Definitions */
#define SW_RT_DEST_ID_MAX_DEST_ID                        (0x0000ffff)

/* PE_LL_CTL : Register Bits Masks Definitions */
#define PE_LL_CTL_EACTRL                                 (0x00000007)

/* HOST_BASE_ID_LOCK : Register Bits Masks Definitions */
#define HOST_BASE_ID_LOCK_HOST_BASE_ID                   (0x0000ffff)
#define HOST_BASE_ID_LOCK_HOST_BASE_DEV32ID              (0xffff0000)

/* COMP_TAG : Register Bits Masks Definitions */
#define COMP_TAG_CTAG                                    (0xffffffff)

/* ROUTE_CFG_DESTID : Register Bits Masks Definitions */
#define ROUTE_CFG_DESTID_DEV8_DEST_ID                    (0x000000ff)
#define ROUTE_CFG_DESTID_DEV16_MSB_DEST_ID               (0x0000ff00)
#define ROUTE_CFG_DESTID_ERTC_EN                         (0x80000000)

/* ROUTE_CFG_PORT : Register Bits Masks Definitions */
#define ROUTE_CFG_PORT_CFG_OUT_PORT                      (0x000000ff)
#define ROUTE_CFG_PORT_ROUTE_TYPE                        (0x00000300)
#define ROUTE_CFG_PORT_CAPTURE                           (0x80000000)

/* ROUTE_DFLT_PORT : Register Bits Masks Definitions */
#define ROUTE_DFLT_PORT_DEFAULT_OUT_PORT                 (0x000000ff)
#define ROUTE_DFLT_PORT_ROUTE_TYPE                       (0x00000300)
#define ROUTE_DFLT_PORT_CAPTURE                          (0x80000000)

/* SP_MB_HEAD : Register Bits Masks Definitions */
#define SP_MB_HEAD_EF_ID                                 (0x0000ffff)
#define SP_MB_HEAD_EF_PTR                                (0xffff0000)

/* SP_LT_CTL : Register Bits Masks Definitions */
#define SP_LT_CTL_TVAL                                   (0xffffff00)

/* SP_GEN_CTL : Register Bits Masks Definitions */
#define SP_GEN_CTL_DISC                                  (0x20000000)

/* SPX_LM_REQ : Register Bits Masks Definitions */
#define SPX_LM_REQ_CMD                                   (0x00000007)

/* SPX_LM_RESP : Register Bits Masks Definitions */
#define SPX_LM_RESP_LINK_STAT                            (0x0000001f)
#define SPX_LM_RESP_ACK_ID_STAT                          (0x0001ffe0)
#define SPX_LM_RESP_STAT_CS64                            (0x1ffe0000)
#define SPX_LM_RESP_RESP_VLD                             (0x80000000)

/* SPX_CTL2 : Register Bits Masks Definitions */
#define SPX_CTL2_RTEC_EN                                 (0x00000001)
#define SPX_CTL2_RTEC                                    (0x00000002)
#define SPX_CTL2_D_SCRM_DIS                              (0x00000004)
#define SPX_CTL2_INACT_LN_EN                             (0x00000008)
#define SPX_CTL2_RETRAIN_EN                              (0x00000010)
#define SPX_CTL2_GB_12p5_EN                              (0x00001000)
#define SPX_CTL2_GB_12p5                                 (0x00002000)
#define SPX_CTL2_GB_10p3_EN                              (0x00004000)
#define SPX_CTL2_GB_10p3                                 (0x00008000)
#define SPX_CTL2_GB_6p25_EN                              (0x00010000)
#define SPX_CTL2_GB_6p25                                 (0x00020000)
#define SPX_CTL2_GB_5p0_EN                               (0x00040000)
#define SPX_CTL2_GB_5p0                                  (0x00080000)
#define SPX_CTL2_GB_3p125_EN                             (0x00100000)
#define SPX_CTL2_GB_3p125                                (0x00200000)
#define SPX_CTL2_GB_2p5_EN                               (0x00400000)
#define SPX_CTL2_GB_2p5                                  (0x00800000)
#define SPX_CTL2_GB_1p25_EN                              (0x01000000)
#define SPX_CTL2_GB_1p25                                 (0x02000000)
#define SPX_CTL2_BAUD_DISC                               (0x08000000)
#define SPX_CTL2_BAUD_SEL                                (0xf0000000)

/* SPX_ERR_STAT : Register Bits Masks Definitions */
#define SPX_ERR_STAT_PORT_UNINIT                         (0x00000001)
#define SPX_ERR_STAT_PORT_OK                             (0x00000002)
#define SPX_ERR_STAT_PORT_ERR                            (0x00000004)
#define SPX_ERR_STAT_PORT_UNAVL                          (0x00000008)
#define SPX_ERR_STAT_PORT_W_P                            (0x00000010)
#define SPX_ERR_STAT_PORT_W_DIS                          (0x00000020)
#define SPX_ERR_STAT_INPUT_ERR_STOP                      (0x00000100)
#define SPX_ERR_STAT_INPUT_ERR_ENCTR                     (0x00000200)
#define SPX_ERR_STAT_INPUT_RS                            (0x00000400)
#define SPX_ERR_STAT_OUTPUT_ERR_STOP                     (0x00010000)
#define SPX_ERR_STAT_OUTPUT_ERR_ENCTR                    (0x00020000)
#define SPX_ERR_STAT_OUTPUT_RS                           (0x00040000)
#define SPX_ERR_STAT_OUTPUT_R                            (0x00080000)
#define SPX_ERR_STAT_OUTPUT_RE                           (0x00100000)
#define SPX_ERR_STAT_OUTPUT_FAIL                         (0x02000000)
#define SPX_ERR_STAT_OUTPUT_DROP                         (0x04000000)
#define SPX_ERR_STAT_TXFC                                (0x08000000)
#define SPX_ERR_STAT_IDLE_SEQ                            (0x30000000)
#define SPX_ERR_STAT_IDLE2_EN                            (0x40000000)
#define SPX_ERR_STAT_IDLE2                               (0x80000000)

/* SPX_CTL : Register Bits Masks Definitions */
#define SPX_CTL_PTYP                                     (0x00000001)
#define SPX_CTL_PORT_LOCKOUT                             (0x00000002)
#define SPX_CTL_DROP_EN                                  (0x00000004)
#define SPX_CTL_STOP_FAIL_EN                             (0x00000008)
#define SPX_CTL_PORT_WIDTH2                              (0x00003000)
#define SPX_CTL_OVER_PWIDTH2                             (0x0000c000)
#define SPX_CTL_FLOW_ARB                                 (0x00010000)
#define SPX_CTL_ENUM_B                                   (0x00020000)
#define SPX_CTL_FLOW_CTRL                                (0x00040000)
#define SPX_CTL_MULT_CS                                  (0x00080000)
#define SPX_CTL_ERR_DIS                                  (0x00100000)
#define SPX_CTL_INP_EN                                   (0x00200000)
#define SPX_CTL_OTP_EN                                   (0x00400000)
#define SPX_CTL_PORT_DIS                                 (0x00800000)
#define SPX_CTL_OVER_PWIDTH                              (0x07000000)
#define SPX_CTL_INIT_PWIDTH                              (0x38000000)
#define SPX_CTL_PORT_WIDTH                               (0xc0000000)

/* SPX_OUT_ACKID_CSR : Register Bits Masks Definitions */
#define SPX_OUT_ACKID_CSR_OUTB_ACKID                     (0x00000fff)
#define SPX_OUT_ACKID_CSR_OUTSTD_ACKID                   (0x00fff000)
#define SPX_OUT_ACKID_CSR_CLR_OUTSTD_ACKID               (0x80000000)

/* SPX_IN_ACKID_CSR : Register Bits Masks Definitions */
#define SPX_IN_ACKID_CSR_INB_ACKID                       (0x00000fff)

/* SPX_POWER_MNGT_CSR : Register Bits Masks Definitions */
#define SPX_POWER_MNGT_CSR_LP_TX_uint32_t                  (0x000000c0)
#define SPX_POWER_MNGT_CSR_CHG_LP_TX_WIDTH               (0x00000700)
#define SPX_POWER_MNGT_CSR_MY_TX_uint32_t                  (0x00001800)
#define SPX_POWER_MNGT_CSR_CHG_MY_TX_WIDTH               (0x0000e000)
#define SPX_POWER_MNGT_CSR_RX_WIDTH_uint32_t               (0x00070000)
#define SPX_POWER_MNGT_CSR_TX_WIDTH_uint32_t               (0x00380000)
#define SPX_POWER_MNGT_CSR_ASYM_MODE_EN                  (0x07c00000)
#define SPX_POWER_MNGT_CSR_ASYM_MODE_SUP                 (0xf8000000)

/* SPX_LAT_OPT_CSR : Register Bits Masks Definitions */
#define SPX_LAT_OPT_CSR_PNA_ERR_REC_EN                   (0x00400000)
#define SPX_LAT_OPT_CSR_MULT_ACK_EN                      (0x00800000)
#define SPX_LAT_OPT_CSR_PNA_ACKID                        (0x20000000)
#define SPX_LAT_OPT_CSR_PNA_ERR_REC                      (0x40000000)
#define SPX_LAT_OPT_CSR_MULT_ACK                         (0x80000000)

/* SPX_TMR_CTL : Register Bits Masks Definitions */
#define SPX_TMR_CTL_EMPHASIS_CMD_TIMEOUT                 (0x000000ff)
#define SPX_TMR_CTL_CW_CMPLT_TMR                         (0x0000ff00)
#define SPX_TMR_CTL_DME_WAIT_FRAMES                      (0x00ff0000)
#define SPX_TMR_CTL_DME_CMPLT_TMR                        (0xff000000)

/* SPX_TMR_CTL2 : Register Bits Masks Definitions */
#define SPX_TMR_CTL2_RECOVERY_TMR                        (0x0000ff00)
#define SPX_TMR_CTL2_DISCOVERY_CMPLT_TMR                 (0x00ff0000)
#define SPX_TMR_CTL2_RETRAIN_CMPLT_TMR                   (0xff000000)

/* SPX_TMR_CTL3 : Register Bits Masks Definitions */
#define SPX_TMR_CTL3_KEEP_ALIVE_INTERVAL                 (0x000003ff)
#define SPX_TMR_CTL3_KEEP_ALIVE_PERIOD                   (0x0000fc00)
#define SPX_TMR_CTL3_RX_WIDTH_CMD_TIMEOUT                (0x00ff0000)
#define SPX_TMR_CTL3_TX_WIDTH_CMD_TIMEOUT                (0xff000000)

/* ERR_RPT_BH : Register Bits Masks Definitions */
#define ERR_RPT_BH_EF_ID                                 (0x0000ffff)
#define ERR_RPT_BH_EF_PTR                                (0xffff0000)

/* ERR_MGMT_HS : Register Bits Masks Definitions */
#define ERR_MGMT_HS_HOT_SWAP                             (0x40000000)
#define ERR_MGMT_HS_NO_ERR_MGMT                          (0x80000000)

/* ERR_DET : Register Bits Masks Definitions */
#define ERR_DET_ILL_TYPE                                 (0x00400000)
#define ERR_DET_UNS_RSP                                  (0x00800000)
#define ERR_DET_ILL_ID                                   (0x04000000)

/* ERR_EN : Register Bits Masks Definitions */
#define ERR_EN_ILL_TYPE_EN                               (0x00400000)
#define ERR_EN_UNS_RSP_EN                                (0x00800000)
#define ERR_EN_ILL_ID_EN                                 (0x04000000)

/* H_ADDR_CAPT : Register Bits Masks Definitions */
#define H_ADDR_CAPT_ADDR                                 (0xffffffff)

/* ADDR_CAPT : Register Bits Masks Definitions */
#define ADDR_CAPT_XAMSBS                                 (0x00000003)
#define ADDR_CAPT_ADDR                                   (0xfffffff8)

/* ID_CAPT : Register Bits Masks Definitions */
#define ID_CAPT_SRC_ID                                   (0x000000ff)
#define ID_CAPT_MSB_SRC_ID                               (0x0000ff00)
#define ID_CAPT_DEST_ID                                  (0x00ff0000)
#define ID_CAPT_MSB_DEST_ID                              (0xff000000)

/* CTRL_CAPT : Register Bits Masks Definitions */
#define CTRL_CAPT_TT                                     (0x00000003)
#define CTRL_CAPT_IMPL_SPECIFIC                          (0x0000fff0)
#define CTRL_CAPT_MSG_INFO                               (0x00ff0000)
#define CTRL_CAPT_TTYPE                                  (0x0f000000)
#define CTRL_CAPT_FTYPE                                  (0xf0000000)

/* DEV32_DESTID_CAPT : Register Bits Masks Definitions */
#define DEV32_DESTID_CAPT_DEV32_DESTID                   (0xffffffff)

/* DEV32_SRCID_CAPT : Register Bits Masks Definitions */
#define DEV32_SRCID_CAPT_DEV32_SRCID                     (0xffffffff)

/* PW_TGT_ID : Register Bits Masks Definitions */
#define PW_TGT_ID_DEV32                                  (0x00004000)
#define PW_TGT_ID_DEV16                                  (0x00008000)
#define PW_TGT_ID_PW_TGT_ID                              (0x00ff0000)
#define PW_TGT_ID_MSB_PW_ID                              (0xff000000)

/* PKT_TIME_LIVE : Register Bits Masks Definitions */
#define PKT_TIME_LIVE_PKT_TIME_LIVE                      (0xffff0000)

/* DEV32_PW_TGT_ID : Register Bits Masks Definitions */
#define DEV32_PW_TGT_ID_DEV32                            (0xffffffff)

/* PW_TRAN_CTL : Register Bits Masks Definitions */
#define PW_TRAN_CTL_PW_DIS                               (0x00000001)

/* SPX_ERR_DET : Register Bits Masks Definitions */
#define SPX_ERR_DET_LINK_INIT                            (0x10000000)
#define SPX_ERR_DET_DLT                                  (0x20000000)
#define SPX_ERR_DET_OK_TO_UNINIT                         (0x40000000)

/* SPX_RATE_EN : Register Bits Masks Definitions */
#define SPX_RATE_EN_LINK_INIT                            (0x10000000)
#define SPX_RATE_EN_DLT                                  (0x20000000)
#define SPX_RATE_EN_OK_TO_UNINIT                         (0x40000000)

/* SPX_DLT_CSR : Register Bits Masks Definitions */
#define SPX_DLT_CSR_TIMEOUT                              (0xffffff00)

/* PER_LANE_BH : Register Bits Masks Definitions */
#define PER_LANE_BH_EF_ID                                (0x0000ffff)
#define PER_LANE_BH_EF_PTR                               (0xffff0000)

/* LANEX_STAT0 : Register Bits Masks Definitions */
#define LANEX_STAT0_STAT2_7                              (0x00000007)
#define LANEX_STAT0_STAT1                                (0x00000008)
#define LANEX_STAT0_CHG_TRN                              (0x00000040)
#define LANEX_STAT0_CHG_SYNC                             (0x00000080)
#define LANEX_STAT0_ERR_CNT                              (0x00000f00)
#define LANEX_STAT0_RX_RDY                               (0x00001000)
#define LANEX_STAT0_RX_SYNC                              (0x00002000)
#define LANEX_STAT0_RX_TRN                               (0x00004000)
#define LANEX_STAT0_RX_INV                               (0x00008000)
#define LANEX_STAT0_RX_TYPE                              (0x00030000)
#define LANEX_STAT0_TX_MODE                              (0x00040000)
#define LANEX_STAT0_TX_TYPE                              (0x00080000)
#define LANEX_STAT0_LANE_NUM                             (0x00f00000)
#define LANEX_STAT0_PORT_NUM                             (0xff000000)

/* LANEX_STAT1 : Register Bits Masks Definitions */
#define LANEX_STAT1_CWR_CMPLT                            (0x00000020)
#define LANEX_STAT1_CWR_FAIL                             (0x00000040)
#define LANEX_STAT1_CW_CMPLT                             (0x00000080)
#define LANEX_STAT1_CW_FAIL                              (0x00000100)
#define LANEX_STAT1_DME_CMPLT                            (0x00000200)
#define LANEX_STAT1_DME_FAIL                             (0x00000400)
#define LANEX_STAT1_TRAIN_TYPE                           (0x00003800)
#define LANEX_STAT1_SIG_LOST                             (0x00004000)
#define LANEX_STAT1_LP_SCRM                              (0x00008000)
#define LANEX_STAT1_LP_TAP_P1                            (0x00030000)
#define LANEX_STAT1_LP_TAP_M1                            (0x000c0000)
#define LANEX_STAT1_LP_LANE_NUM                          (0x00f00000)
#define LANEX_STAT1_LP_WIDTH                             (0x07000000)
#define LANEX_STAT1_LP_RX_TRN                            (0x08000000)
#define LANEX_STAT1_IMPL_SPEC                            (0x10000000)
#define LANEX_STAT1_CHG                                  (0x20000000)
#define LANEX_STAT1_INFO_OK                              (0x40000000)
#define LANEX_STAT1_IDLE_RX                              (0x80000000)

/* LANEX_STAT2 : Register Bits Masks Definitions */
#define LANEX_STAT2_LP_RX_W_NACK                         (0x00000004)
#define LANEX_STAT2_LP_RX_W_ACK                          (0x00000008)
#define LANEX_STAT2_LP_RX_W_CMD                          (0x00000070)
#define LANEX_STAT2_LP_TRAINED                           (0x00000080)
#define LANEX_STAT2_LP_RX_LANE_RDY                       (0x00000100)
#define LANEX_STAT2_LP_RX_LANES_RDY                      (0x00000e00)
#define LANEX_STAT2_LP_RX_W                              (0x00007000)
#define LANEX_STAT2_LP_TX_1X                             (0x00008000)
#define LANEX_STAT2_LP_PORT_INIT                         (0x00010000)
#define LANEX_STAT2_LP_ASYM_EN                           (0x00020000)
#define LANEX_STAT2_LP_RETRN_EN                          (0x00040000)
#define LANEX_STAT2_LP_TX_ADJ_SUP                        (0x00080000)
#define LANEX_STAT2_LP_LANE                              (0x00f00000)
#define LANEX_STAT2_LP_PORT                              (0xff000000)

/* LANEX_STAT3 : Register Bits Masks Definitions */
#define LANEX_STAT3_SC_RSVD                              (0x00000ff0)
#define LANEX_STAT3_LP_L_SILENT                          (0x00001000)
#define LANEX_STAT3_LP_P_SILENT                          (0x00002000)
#define LANEX_STAT3_LP_RETRN                             (0x00004000)
#define LANEX_STAT3_LP_RETRN_RDY                         (0x00008000)
#define LANEX_STAT3_LP_RETRN_GNT                         (0x00010000)
#define LANEX_STAT3_LP_TX_EMPH_STAT                      (0x000e0000)
#define LANEX_STAT3_LP_TX_EMPH_CMD                       (0x00700000)
#define LANEX_STAT3_LP_TAP                               (0x07800000)
#define LANEX_STAT3_LP_TX_SC_REQ                         (0x08000000)
#define LANEX_STAT3_LP_TX_W_PEND                         (0x10000000)
#define LANEX_STAT3_LP_TX_W_REQ                          (0xe0000000)

/* LANEX_STAT4 : Register Bits Masks Definitions */
#define LANEX_STAT4_GEN_STAT6_CHG                        (0x00000001)
#define LANEX_STAT4_GEN_STAT5_CHG                        (0x00000002)
#define LANEX_STAT4_GEN_STAT1_CHG                        (0x00000004)
#define LANEX_STAT4_GEN_STAT0_CHG_TRN                    (0x00000008)
#define LANEX_STAT4_GEN_STAT0_CHG_SYNC                   (0x00000010)
#define LANEX_STAT4_STAT6_CHG                            (0x02000000)
#define LANEX_STAT4_STAT5_CHG                            (0x04000000)
#define LANEX_STAT4_STAT1_CHG                            (0x40000000)
#define LANEX_STAT4_STAT0_CHG                            (0x80000000)

/* LANEX_STAT5 : Register Bits Masks Definitions */
#define LANEX_STAT5_CM1_STAT                             (0x00000003)
#define LANEX_STAT5_CP1_STAT                             (0x0000000c)
#define LANEX_STAT5_C0_STAT                              (0x00000070)
#define LANEX_STAT5_LOC_REMOTE_EN                        (0x00000080)
#define LANEX_STAT5_SENT                                 (0x00000100)
#define LANEX_STAT5_RETRAIN_LANE                         (0x00000400)
#define LANEX_STAT5_TRAIN_LANE                           (0x00000800)
#define LANEX_STAT5_CW_SELECT                            (0x00001000)
#define LANEX_STAT5_DME_MODE                             (0x00002000)
#define LANEX_STAT5_UNTRAINED                            (0x00004000)
#define LANEX_STAT5_SILENCE_ENTERED                      (0x00008000)
#define LANEX_STAT5_CM1_CMD                              (0x00030000)
#define LANEX_STAT5_CP1_CMD                              (0x000c0000)
#define LANEX_STAT5_C0_CMD                               (0x00700000)
#define LANEX_STAT5_SILENT_NOW                           (0x00800000)
#define LANEX_STAT5_TAP                                  (0x0f000000)
#define LANEX_STAT5_RX_RDY                               (0x10000000)
#define LANEX_STAT5_CHG                                  (0x20000000)
#define LANEX_STAT5_VALID                                (0x40000000)
#define LANEX_STAT5_HW_ACK                               (0x80000000)

/* LANEX_STAT6 : Register Bits Masks Definitions */
#define LANEX_STAT6_CM1_STAT                             (0x00000003)
#define LANEX_STAT6_CP1_STAT                             (0x0000000c)
#define LANEX_STAT6_C0_STAT                              (0x00000070)
#define LANEX_STAT6_LP_REMOTE_EN                         (0x00000080)
#define LANEX_STAT6_SENT                                 (0x00000100)
#define LANEX_STAT6_TRAINING_FAILED                      (0x00000200)
#define LANEX_STAT6_RETRAIN_LANE                         (0x00000400)
#define LANEX_STAT6_TRAIN_LANE                           (0x00000800)
#define LANEX_STAT6_CW_SELECT                            (0x00001000)
#define LANEX_STAT6_DME_MODE                             (0x00002000)
#define LANEX_STAT6_UNTRAINED                            (0x00004000)
#define LANEX_STAT6_SILENCE_ENTERED                      (0x00008000)
#define LANEX_STAT6_CM1_CMD                              (0x00030000)
#define LANEX_STAT6_CP1_CMD                              (0x000c0000)
#define LANEX_STAT6_C0_CMD                               (0x00700000)
#define LANEX_STAT6_SILENT_NOW                           (0x00800000)
#define LANEX_STAT6_TAP                                  (0x0f000000)
#define LANEX_STAT6_RX_RDY                               (0x10000000)
#define LANEX_STAT6_CHG                                  (0x20000000)
#define LANEX_STAT6_VALID                                (0x40000000)
#define LANEX_STAT6_HW_CMD                               (0x80000000)

/* SWITCH_RT_BH : Register Bits Masks Definitions */
#define SWITCH_RT_BH_EF_ID                               (0x0000ffff)
#define SWITCH_RT_BH_EF_PTR                              (0xffff0000)

/* BC_RT_CTL : Register Bits Masks Definitions */
#define BC_RT_CTL_MC_MASK_SZ                             (0x03000000)
#define BC_RT_CTL_DEV32_RT_CTRL                          (0x40000000)
#define BC_RT_CTL_THREE_LEVELS                           (0x80000000)

/* BC_MC_INFO : Register Bits Masks Definitions */
#define BC_MC_INFO_MASK_PTR                              (0x00fffc00)
#define BC_MC_INFO_NUM_MASKS                             (0xff000000)

/* BC_RT_LVL0_INFO : Register Bits Masks Definitions */
#define BC_RT_LVL0_INFO_L0_GROUP_PTR                     (0x00fffc00)
#define BC_RT_LVL0_INFO_NUM_L0_GROUPS                    (0xff000000)

/* BC_RT_LVL1_INFO : Register Bits Masks Definitions */
#define BC_RT_LVL1_INFO_L1_GROUP_PTR                     (0x00fffc00)
#define BC_RT_LVL1_INFO_NUM_L1_GROUPS                    (0xff000000)

/* BC_RT_LVL2_INFO : Register Bits Masks Definitions */
#define BC_RT_LVL2_INFO_L2_GROUP_PTR                     (0x00fffc00)
#define BC_RT_LVL2_INFO_NUM_L2_GROUPS                    (0xff000000)

/* SPX_RT_CTL : Register Bits Masks Definitions */
#define SPX_RT_CTL_MC_MASK_SZ                            (0x03000000)
#define SPX_RT_CTL_DEV32_RT_CTRL                         (0x40000000)
#define SPX_RT_CTL_THREE_LEVELS                          (0x80000000)

/* SPX_MC_INFO : Register Bits Masks Definitions */
#define SPX_MC_INFO_MASK_PTR                             (0x00fffc00)
#define SPX_MC_INFO_NUM_MASKS                            (0xff000000)

/* SPX_RT_LVL0_INFO : Register Bits Masks Definitions */
#define SPX_RT_LVL0_INFO_L0_GROUP_PTR                    (0x00fffc00)
#define SPX_RT_LVL0_INFO_NUM_L0_GROUPS                   (0xff000000)

/* SPX_RT_LVL1_INFO : Register Bits Masks Definitions */
#define SPX_RT_LVL1_INFO_L1_GROUP_PTR                    (0x00fffc00)
#define SPX_RT_LVL1_INFO_NUM_L1_GROUPS                   (0xff000000)

/* SPX_RT_LVL2_INFO : Register Bits Masks Definitions */
#define SPX_RT_LVL2_INFO_L2_GROUP_PTR                    (0x00fffc00)
#define SPX_RT_LVL2_INFO_NUM_L2_GROUPS                   (0xff000000)

/* PLM_BH : Register Bits Masks Definitions */
#define PLM_BH_BLK_TYPE                                  (0x00000fff)
#define PLM_BH_BLK_REV                                   (0x0000f000)
#define PLM_BH_NEXT_BLK_PTR                              (0xffff0000)

/* PLM_SPX_IMP_SPEC_CTL : Register Bits Masks Definitions */
#define PLM_SPX_IMP_SPEC_CTL_PA_BKLOG_THRESH             (0x0000003f)
#define PLM_SPX_IMP_SPEC_CTL_CONT_PNA                    (0x00000040)
#define PLM_SPX_IMP_SPEC_CTL_CONT_LR                     (0x00000080)
#define PLM_SPX_IMP_SPEC_CTL_BLIP_CS                     (0x00000100)
#define PLM_SPX_IMP_SPEC_CTL_LOST_CS_DIS                 (0x00000200)
#define PLM_SPX_IMP_SPEC_CTL_INFER_SELF_RST              (0x00000400)
#define PLM_SPX_IMP_SPEC_CTL_DLT_FATAL                   (0x00000800)
#define PLM_SPX_IMP_SPEC_CTL_PRE_SILENCE                 (0x00001000)
#define PLM_SPX_IMP_SPEC_CTL_OK2U_FATAL                  (0x00002000)
#define PLM_SPX_IMP_SPEC_CTL_MAXD_FATAL                  (0x00004000)
#define PLM_SPX_IMP_SPEC_CTL_DWNGD_FATAL                 (0x00008000)
#define PLM_SPX_IMP_SPEC_CTL_SWAP_RX                     (0x00030000)
#define PLM_SPX_IMP_SPEC_CTL_SWAP_TX                     (0x000c0000)
#define PLM_SPX_IMP_SPEC_CTL_SELF_RST                    (0x00100000)
#define PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST               (0x00200000)
#define PLM_SPX_IMP_SPEC_CTL_RESET_REG                   (0x00400000)
#define PLM_SPX_IMP_SPEC_CTL_LLB_EN                      (0x00800000)
#define PLM_SPX_IMP_SPEC_CTL_CS_FIELD1                   (0x01000000)
#define PLM_SPX_IMP_SPEC_CTL_SOFT_RST_PORT               (0x02000000)
#define PLM_SPX_IMP_SPEC_CTL_FORCE_REINIT                (0x04000000)
#define PLM_SPX_IMP_SPEC_CTL_DME_TRAINING                (0x08000000)
#define PLM_SPX_IMP_SPEC_CTL_DLB_EN                      (0x10000000)
#define PLM_SPX_IMP_SPEC_CTL_USE_IDLE1                   (0x20000000)
#define PLM_SPX_IMP_SPEC_CTL_USE_IDLE2                   (0x40000000)
#define PLM_SPX_IMP_SPEC_CTL_USE_IDLE3                   (0x80000000)

/* PLM_SPX_PWDN_CTL : Register Bits Masks Definitions */
#define PLM_SPX_PWDN_CTL_PWDN_PORT                       (0x00000001)

/* PLM_SPX_1WR : Register Bits Masks Definitions */
#define PLM_SPX_1WR_TIMEOUT                              (0x0000000f)
#define PLM_SPX_1WR_PORT_SELECT                          (0x00000f00)
#define PLM_SPX_1WR_PATH_MODE                            (0x00070000)
#define PLM_SPX_1WR_TX_MODE                              (0x00800000)
#define PLM_SPX_1WR_IDLE_SEQ                             (0x03000000)
#define PLM_SPX_1WR_BAUD_EN                              (0xf0000000)

/* PLM_SPX_MULT_ACK_CTL : Register Bits Masks Definitions */
#define PLM_SPX_MULT_ACK_CTL_MULT_ACK_DLY                (0x000000ff)

/* PLM_SPX_STAT : Register Bits Masks Definitions */
#define PLM_SPX_STAT_BERM_0                              (0x00000001)
#define PLM_SPX_STAT_BERM_1                              (0x00000002)
#define PLM_SPX_STAT_BERM_2                              (0x00000004)
#define PLM_SPX_STAT_BERM_3                              (0x00000008)
#define PLM_SPX_STAT_TLM_INT                             (0x00000400)
#define PLM_SPX_STAT_PBM_INT                             (0x00000800)
#define PLM_SPX_STAT_MECS                                (0x00001000)
#define PLM_SPX_STAT_TLM_PW                              (0x00004000)
#define PLM_SPX_STAT_PBM_PW                              (0x00008000)
#define PLM_SPX_STAT_RST_REQ                             (0x00010000)
#define PLM_SPX_STAT_PRST_REQ                            (0x00020000)
#define PLM_SPX_STAT_EL_INTA                             (0x00040000)
#define PLM_SPX_STAT_EL_INTB                             (0x00080000)
#define PLM_SPX_STAT_II_CHG_0                            (0x00100000)
#define PLM_SPX_STAT_II_CHG_1                            (0x00200000)
#define PLM_SPX_STAT_II_CHG_2                            (0x00400000)
#define PLM_SPX_STAT_II_CHG_3                            (0x00800000)
#define PLM_SPX_STAT_PCAP                                (0x01000000)
#define PLM_SPX_STAT_DWNGD                               (0x02000000)
#define PLM_SPX_STAT_PBM_FATAL                           (0x04000000)
#define PLM_SPX_STAT_PORT_ERR                            (0x08000000)
#define PLM_SPX_STAT_LINK_INIT                           (0x10000000)
#define PLM_SPX_STAT_DLT                                 (0x20000000)
#define PLM_SPX_STAT_OK_TO_UNINIT                        (0x40000000)
#define PLM_SPX_STAT_MAX_DENIAL                          (0x80000000)

/* PLM_SPX_INT_EN : Register Bits Masks Definitions */
#define PLM_SPX_INT_EN_BERM_0                            (0x00000001)
#define PLM_SPX_INT_EN_BERM_1                            (0x00000002)
#define PLM_SPX_INT_EN_BERM_2                            (0x00000004)
#define PLM_SPX_INT_EN_BERM_3                            (0x00000008)
#define PLM_SPX_INT_EN_EL_INTA                           (0x00040000)
#define PLM_SPX_INT_EN_EL_INTB                           (0x00080000)
#define PLM_SPX_INT_EN_II_CHG_0                          (0x00100000)
#define PLM_SPX_INT_EN_II_CHG_1                          (0x00200000)
#define PLM_SPX_INT_EN_II_CHG_2                          (0x00400000)
#define PLM_SPX_INT_EN_II_CHG_3                          (0x00800000)
#define PLM_SPX_INT_EN_PCAP                              (0x01000000)
#define PLM_SPX_INT_EN_DWNGD                             (0x02000000)
#define PLM_SPX_INT_EN_PBM_FATAL                         (0x04000000)
#define PLM_SPX_INT_EN_PORT_ERR                          (0x08000000)
#define PLM_SPX_INT_EN_LINK_INIT                         (0x10000000)
#define PLM_SPX_INT_EN_DLT                               (0x20000000)
#define PLM_SPX_INT_EN_OK_TO_UNINIT                      (0x40000000)
#define PLM_SPX_INT_EN_MAX_DENIAL                        (0x80000000)

/* PLM_SPX_PW_EN : Register Bits Masks Definitions */
#define PLM_SPX_PW_EN_BERM_0                             (0x00000001)
#define PLM_SPX_PW_EN_BERM_1                             (0x00000002)
#define PLM_SPX_PW_EN_BERM_2                             (0x00000004)
#define PLM_SPX_PW_EN_BERM_3                             (0x00000008)
#define PLM_SPX_PW_EN_EL_INTA                            (0x00040000)
#define PLM_SPX_PW_EN_EL_INTB                            (0x00080000)
#define PLM_SPX_PW_EN_II_CHG_0                           (0x00100000)
#define PLM_SPX_PW_EN_II_CHG_1                           (0x00200000)
#define PLM_SPX_PW_EN_II_CHG_2                           (0x00400000)
#define PLM_SPX_PW_EN_II_CHG_3                           (0x00800000)
#define PLM_SPX_PW_EN_PCAP                               (0x01000000)
#define PLM_SPX_PW_EN_DWNGD                              (0x02000000)
#define PLM_SPX_PW_EN_PBM_FATAL                          (0x04000000)
#define PLM_SPX_PW_EN_PORT_ERR                           (0x08000000)
#define PLM_SPX_PW_EN_LINK_INIT                          (0x10000000)
#define PLM_SPX_PW_EN_DLT                                (0x20000000)
#define PLM_SPX_PW_EN_OK_TO_UNINIT                       (0x40000000)
#define PLM_SPX_PW_EN_MAX_DENIAL                         (0x80000000)

/* PLM_SPX_EVENT_GEN : Register Bits Masks Definitions */
#define PLM_SPX_EVENT_GEN_MECS                           (0x00001000)
#define PLM_SPX_EVENT_GEN_RST_REQ                        (0x00010000)
#define PLM_SPX_EVENT_GEN_PRST_REQ                       (0x00020000)
#define PLM_SPX_EVENT_GEN_DWNGD                          (0x02000000)
#define PLM_SPX_EVENT_GEN_PBM_FATAL                      (0x04000000)
#define PLM_SPX_EVENT_GEN_PORT_ERR                       (0x08000000)
#define PLM_SPX_EVENT_GEN_MAX_DENIAL                     (0x80000000)

/* PLM_SPX_ALL_INT_EN : Register Bits Masks Definitions */
#define PLM_SPX_ALL_INT_EN_IRQ_EN                        (0x00000001)

/* PLM_SPX_PATH_CTL : Register Bits Masks Definitions */
#define PLM_SPX_PATH_CTL_PATH_MODE                       (0x00000007)
#define PLM_SPX_PATH_CTL_PATH_CONFIGURATION              (0x00000700)
#define PLM_SPX_PATH_CTL_PATH_ID                         (0x001f0000)

/* PLM_SPX_SILENCE_TMR : Register Bits Masks Definitions */
#define PLM_SPX_SILENCE_TMR_TENB_PATTERN                 (0x000fffff)
#define PLM_SPX_SILENCE_TMR_SILENCE_TMR                  (0xfff00000)

/* PLM_SPX_VMIN_EXP : Register Bits Masks Definitions */
#define PLM_SPX_VMIN_EXP_DS_MIN                          (0x000000ff)
#define PLM_SPX_VMIN_EXP_MMAX                            (0x00000f00)
#define PLM_SPX_VMIN_EXP_IMAX                            (0x000f0000)
#define PLM_SPX_VMIN_EXP_VMIN_EXP                        (0x1f000000)
#define PLM_SPX_VMIN_EXP_DISC_IGNORE_LOLS                (0x40000000)
#define PLM_SPX_VMIN_EXP_LOLS_RECOV_DIS                  (0x80000000)

/* PLM_SPX_POL_CTL : Register Bits Masks Definitions */
#define PLM_SPX_POL_CTL_RX0_POL                          (0x00000001)
#define PLM_SPX_POL_CTL_RX1_POL                          (0x00000002)
#define PLM_SPX_POL_CTL_RX2_POL                          (0x00000004)
#define PLM_SPX_POL_CTL_RX3_POL                          (0x00000008)
#define PLM_SPX_POL_CTL_TX0_POL                          (0x00010000)
#define PLM_SPX_POL_CTL_TX1_POL                          (0x00020000)
#define PLM_SPX_POL_CTL_TX2_POL                          (0x00040000)
#define PLM_SPX_POL_CTL_TX3_POL                          (0x00080000)

/* PLM_SPX_CLKCOMP_CTL : Register Bits Masks Definitions */
#define PLM_SPX_CLKCOMP_CTL_CLK_COMP_CNT                 (0x00001fff)

/* PLM_SPX_DENIAL_CTL : Register Bits Masks Definitions */
#define PLM_SPX_DENIAL_CTL_DENIAL_THRESH                 (0x0000ffff)
#define PLM_SPX_DENIAL_CTL_CNT_RTY                       (0x10000000)
#define PLM_SPX_DENIAL_CTL_CNT_PNA                       (0x20000000)

/* PLM_SPX_ERR_REC_CTL : Register Bits Masks Definitions */
#define PLM_SPX_ERR_REC_CTL_LREQ_LIMIT                   (0x003fffff)

/* PLM_SPX_CS_TX1 : Register Bits Masks Definitions */
#define PLM_SPX_CS_TX1_PAR_1                             (0x0000fff0)
#define PLM_SPX_CS_TX1_PAR_0                             (0x0fff0000)
#define PLM_SPX_CS_TX1_STYPE_0                           (0xf0000000)

/* PLM_SPX_CS_TX2 : Register Bits Masks Definitions */
#define PLM_SPX_CS_TX2_CMD                               (0x00000700)
#define PLM_SPX_CS_TX2_STYPE_1                           (0x00003800)
#define PLM_SPX_CS_TX2_STYPE1_CS64                       (0x0000c000)
#define PLM_SPX_CS_TX2_PARM                              (0x1fff0000)
#define PLM_SPX_CS_TX2_STYPE2                            (0x20000000)

/* PLM_SPX_PNA_CAP : Register Bits Masks Definitions */
#define PLM_SPX_PNA_CAP_PARM_1                           (0x00000fff)
#define PLM_SPX_PNA_CAP_PARM_0                           (0x0fff0000)
#define PLM_SPX_PNA_CAP_VALID                            (0x80000000)

/* PLM_SPX_ACKID_CAP : Register Bits Masks Definitions */
#define PLM_SPX_ACKID_CAP_ACKID                          (0x00000fff)
#define PLM_SPX_ACKID_CAP_VALID                          (0x80000000)

/* PLM_SPX_SCRATCHY : Register Bits Masks Definitions */
#define PLM_SPX_SCRATCHY_SCRATCH                         (0xffffffff)

/* TLM_BH : Register Bits Masks Definitions */
#define TLM_BH_BLK_TYPE                                  (0x00000fff)
#define TLM_BH_BLK_REV                                   (0x0000f000)
#define TLM_BH_NEXT_BLK_PTR                              (0xffff0000)

/* TLM_SPX_CONTROL : Register Bits Masks Definitions */
#define TLM_SPX_CONTROL_LENGTH                           (0x00001f00)
#define TLM_SPX_CONTROL_BLIP_CRC32                       (0x01000000)
#define TLM_SPX_CONTROL_BLIP_CRC16                       (0x02000000)
#define TLM_SPX_CONTROL_VOQ_SELECT                       (0x30000000)
#define TLM_SPX_CONTROL_PORTGROUP_SELECT                 (0x40000000)

/* TLM_SPX_STAT : Register Bits Masks Definitions */
#define TLM_SPX_STAT_IG_LUT_UNCOR                        (0x00010000)
#define TLM_SPX_STAT_IG_LUT_COR                          (0x00020000)
#define TLM_SPX_STAT_EG_BAD_CRC                          (0x00040000)
#define TLM_SPX_STAT_LUT_DISCARD                         (0x00400000)
#define TLM_SPX_STAT_IG_FTYPE_FILTER                     (0x00800000)
#define TLM_SPX_STAT_IG_BAD_VC                           (0x80000000)

/* TLM_SPX_INT_EN : Register Bits Masks Definitions */
#define TLM_SPX_INT_EN_IG_LUT_UNCOR                      (0x00010000)
#define TLM_SPX_INT_EN_IG_LUT_COR                        (0x00020000)
#define TLM_SPX_INT_EN_EG_BAD_CRC                        (0x00040000)
#define TLM_SPX_INT_EN_LUT_DISCARD                       (0x00400000)
#define TLM_SPX_INT_EN_IG_FTYPE_FILTER                   (0x00800000)
#define TLM_SPX_INT_EN_IG_BAD_VC                         (0x80000000)

/* TLM_SPX_PW_EN : Register Bits Masks Definitions */
#define TLM_SPX_PW_EN_IG_LUT_UNCOR                       (0x00010000)
#define TLM_SPX_PW_EN_IG_LUT_COR                         (0x00020000)
#define TLM_SPX_PW_EN_EG_BAD_CRC                         (0x00040000)
#define TLM_SPX_PW_EN_LUT_DISCARD                        (0x00400000)
#define TLM_SPX_PW_EN_IG_FTYPE_FILTER                    (0x00800000)
#define TLM_SPX_PW_EN_IG_BAD_VC                          (0x80000000)

/* TLM_SPX_EVENT_GEN : Register Bits Masks Definitions */
#define TLM_SPX_EVENT_GEN_IG_LUT_UNCOR                   (0x00010000)
#define TLM_SPX_EVENT_GEN_IG_LUT_COR                     (0x00020000)
#define TLM_SPX_EVENT_GEN_EG_BAD_CRC                     (0x00040000)
#define TLM_SPX_EVENT_GEN_LUT_DISCARD                    (0x00400000)
#define TLM_SPX_EVENT_GEN_IG_FTYPE_FILTER                (0x00800000)
#define TLM_SPX_EVENT_GEN_IG_BAD_VC                      (0x80000000)

/* TLM_SPX_FTYPE_FILT : Register Bits Masks Definitions */
#define TLM_SPX_FTYPE_FILT_F15_IMP                       (0x00000002)
#define TLM_SPX_FTYPE_FILT_F14_RSVD                      (0x00000004)
#define TLM_SPX_FTYPE_FILT_F13_OTHER                     (0x00000008)
#define TLM_SPX_FTYPE_FILT_F13_RESPONSE_DATA             (0x00000010)
#define TLM_SPX_FTYPE_FILT_F13_RESPONSE                  (0x00000020)
#define TLM_SPX_FTYPE_FILT_F12_RSVD                      (0x00000040)
#define TLM_SPX_FTYPE_FILT_F11_MSG                       (0x00000080)
#define TLM_SPX_FTYPE_FILT_F10_DOORBELL                  (0x00000100)
#define TLM_SPX_FTYPE_FILT_F9_DATA_STREAMING             (0x00000200)
#define TLM_SPX_FTYPE_FILT_F8_OTHER                      (0x00000400)
#define TLM_SPX_FTYPE_FILT_F8_PWR                        (0x00000800)
#define TLM_SPX_FTYPE_FILT_F8_MWR                        (0x00001000)
#define TLM_SPX_FTYPE_FILT_F8_MRR                        (0x00002000)
#define TLM_SPX_FTYPE_FILT_F8_MW                         (0x00004000)
#define TLM_SPX_FTYPE_FILT_F8_MR                         (0x00008000)
#define TLM_SPX_FTYPE_FILT_F7_FLOW                       (0x00010000)
#define TLM_SPX_FTYPE_FILT_F6_STREAMING_WRITE            (0x00020000)
#define TLM_SPX_FTYPE_FILT_F5_OTHER                      (0x00040000)
#define TLM_SPX_FTYPE_FILT_F5_ATOMIC                     (0x00080000)
#define TLM_SPX_FTYPE_FILT_F5_NWRITE_R                   (0x00100000)
#define TLM_SPX_FTYPE_FILT_F5_NWRITE                     (0x00200000)
#define TLM_SPX_FTYPE_FILT_F5_GSM                        (0x00400000)
#define TLM_SPX_FTYPE_FILT_F4_RSVD                       (0x00800000)
#define TLM_SPX_FTYPE_FILT_F3_RSVD                       (0x01000000)
#define TLM_SPX_FTYPE_FILT_F2_ATOMIC                     (0x02000000)
#define TLM_SPX_FTYPE_FILT_F2_NREAD                      (0x04000000)
#define TLM_SPX_FTYPE_FILT_F2_GSM                        (0x08000000)
#define TLM_SPX_FTYPE_FILT_F1_INTERVENTION               (0x10000000)
#define TLM_SPX_FTYPE_FILT_F0_IMP                        (0x40000000)

/* TLM_SPX_FTYPE_CAPT : Register Bits Masks Definitions */
#define TLM_SPX_FTYPE_CAPT_F15_IMP                       (0x00000002)
#define TLM_SPX_FTYPE_CAPT_F14_RSVD                      (0x00000004)
#define TLM_SPX_FTYPE_CAPT_F13_OTHER                     (0x00000008)
#define TLM_SPX_FTYPE_CAPT_F13_RESPONSE_DATA             (0x00000010)
#define TLM_SPX_FTYPE_CAPT_F13_RESPONSE                  (0x00000020)
#define TLM_SPX_FTYPE_CAPT_F12_RSVD                      (0x00000040)
#define TLM_SPX_FTYPE_CAPT_F11_MSG                       (0x00000080)
#define TLM_SPX_FTYPE_CAPT_F10_DOORBELL                  (0x00000100)
#define TLM_SPX_FTYPE_CAPT_F9_DATA_STREAMING             (0x00000200)
#define TLM_SPX_FTYPE_CAPT_F8_OTHER                      (0x00000400)
#define TLM_SPX_FTYPE_CAPT_F8_PWR                        (0x00000800)
#define TLM_SPX_FTYPE_CAPT_F8_MWR                        (0x00001000)
#define TLM_SPX_FTYPE_CAPT_F8_MRR                        (0x00002000)
#define TLM_SPX_FTYPE_CAPT_F8_MW                         (0x00004000)
#define TLM_SPX_FTYPE_CAPT_F8_MR                         (0x00008000)
#define TLM_SPX_FTYPE_CAPT_F7_FLOW                       (0x00010000)
#define TLM_SPX_FTYPE_CAPT_F6_STREAMING_WRITE            (0x00020000)
#define TLM_SPX_FTYPE_CAPT_F5_OTHER                      (0x00040000)
#define TLM_SPX_FTYPE_CAPT_F5_ATOMIC                     (0x00080000)
#define TLM_SPX_FTYPE_CAPT_F5_NWRITE_R                   (0x00100000)
#define TLM_SPX_FTYPE_CAPT_F5_NWRITE                     (0x00200000)
#define TLM_SPX_FTYPE_CAPT_F5_GSM                        (0x00400000)
#define TLM_SPX_FTYPE_CAPT_F4_RSVD                       (0x00800000)
#define TLM_SPX_FTYPE_CAPT_F3_RSVD                       (0x01000000)
#define TLM_SPX_FTYPE_CAPT_F2_ATOMIC                     (0x02000000)
#define TLM_SPX_FTYPE_CAPT_F2_NREAD                      (0x04000000)
#define TLM_SPX_FTYPE_CAPT_F2_GSM                        (0x08000000)
#define TLM_SPX_FTYPE_CAPT_F1_INTERVENTION               (0x10000000)
#define TLM_SPX_FTYPE_CAPT_F0_IMP                        (0x40000000)

/* TLM_SPX_MTC_ROUTE_EN : Register Bits Masks Definitions */
#define TLM_SPX_MTC_ROUTE_EN_MTC_EN                      (0x00000001)

/* TLM_SPX_ROUTE_EN : Register Bits Masks Definitions */
#define TLM_SPX_ROUTE_EN_RT_EN                           (0x00ffffff)

/* PBM_BH : Register Bits Masks Definitions */
#define PBM_BH_BLK_TYPE                                  (0x00000fff)
#define PBM_BH_BLK_REV                                   (0x0000f000)
#define PBM_BH_NEXT_BLK_PTR                              (0xffff0000)

/* PBM_SPX_CONTROL : Register Bits Masks Definitions */
#define PBM_SPX_CONTROL_EG_REORDER_STICK                 (0x00000007)
#define PBM_SPX_CONTROL_EG_REORDER_MODE                  (0x00000030)
#define PBM_SPX_CONTROL_EG_STORE_MODE                    (0x00001000)

/* PBM_SPX_STAT : Register Bits Masks Definitions */
#define PBM_SPX_STAT_EG_BABBLE_PACKET                    (0x00000001)
#define PBM_SPX_STAT_EG_BAD_CHANNEL                      (0x00000002)
#define PBM_SPX_STAT_EG_CRQ_OVERFLOW                     (0x00000008)
#define PBM_SPX_STAT_EG_DATA_OVERFLOW                    (0x00000010)
#define PBM_SPX_STAT_EG_DNFL_FATAL                       (0x00000020)
#define PBM_SPX_STAT_EG_DNFL_COR                         (0x00000040)
#define PBM_SPX_STAT_EG_DOH_FATAL                        (0x00000080)
#define PBM_SPX_STAT_EG_DOH_COR                          (0x00000100)
#define PBM_SPX_STAT_EG_TTL_EXPIRED                      (0x00000200)
#define PBM_SPX_STAT_EG_DATA_UNCOR                       (0x00000800)
#define PBM_SPX_STAT_EG_DATA_COR                         (0x00001000)
#define PBM_SPX_STAT_EG_EMPTY                            (0x00008000)

/* PBM_SPX_INT_EN : Register Bits Masks Definitions */
#define PBM_SPX_INT_EN_EG_BABBLE_PACKET                  (0x00000001)
#define PBM_SPX_INT_EN_EG_BAD_CHANNEL                    (0x00000002)
#define PBM_SPX_INT_EN_EG_CRQ_OVERFLOW                   (0x00000008)
#define PBM_SPX_INT_EN_EG_DATA_OVERFLOW                  (0x00000010)
#define PBM_SPX_INT_EN_EG_DNFL_FATAL                     (0x00000020)
#define PBM_SPX_INT_EN_EG_DNFL_COR                       (0x00000040)
#define PBM_SPX_INT_EN_EG_DOH_FATAL                      (0x00000080)
#define PBM_SPX_INT_EN_EG_DOH_COR                        (0x00000100)
#define PBM_SPX_INT_EN_EG_TTL_EXPIRED                    (0x00000200)
#define PBM_SPX_INT_EN_EG_DATA_UNCOR                     (0x00000800)
#define PBM_SPX_INT_EN_EG_DATA_COR                       (0x00001000)

/* PBM_SPX_PW_EN : Register Bits Masks Definitions */
#define PBM_SPX_PW_EN_EG_BABBLE_PACKET                   (0x00000001)
#define PBM_SPX_PW_EN_EG_BAD_CHANNEL                     (0x00000002)
#define PBM_SPX_PW_EN_EG_CRQ_OVERFLOW                    (0x00000008)
#define PBM_SPX_PW_EN_EG_DATA_OVERFLOW                   (0x00000010)
#define PBM_SPX_PW_EN_EG_DNFL_FATAL                      (0x00000020)
#define PBM_SPX_PW_EN_EG_DNFL_COR                        (0x00000040)
#define PBM_SPX_PW_EN_EG_DOH_FATAL                       (0x00000080)
#define PBM_SPX_PW_EN_EG_DOH_COR                         (0x00000100)
#define PBM_SPX_PW_EN_EG_TTL_EXPIRED                     (0x00000200)
#define PBM_SPX_PW_EN_EG_DATA_UNCOR                      (0x00000800)
#define PBM_SPX_PW_EN_EG_DATA_COR                        (0x00001000)

/* PBM_SPX_EVENT_GEN : Register Bits Masks Definitions */
#define PBM_SPX_EVENT_GEN_EG_BABBLE_PACKET               (0x00000001)
#define PBM_SPX_EVENT_GEN_EG_BAD_CHANNEL                 (0x00000002)
#define PBM_SPX_EVENT_GEN_EG_CRQ_OVERFLOW                (0x00000008)
#define PBM_SPX_EVENT_GEN_EG_DATA_OVERFLOW               (0x00000010)
#define PBM_SPX_EVENT_GEN_EG_DNFL_FATAL                  (0x00000020)
#define PBM_SPX_EVENT_GEN_EG_DNFL_COR                    (0x00000040)
#define PBM_SPX_EVENT_GEN_EG_DOH_FATAL                   (0x00000080)
#define PBM_SPX_EVENT_GEN_EG_DOH_COR                     (0x00000100)
#define PBM_SPX_EVENT_GEN_EG_TTL_EXPIRED                 (0x00000200)
#define PBM_SPX_EVENT_GEN_EG_DATA_UNCOR                  (0x00000800)
#define PBM_SPX_EVENT_GEN_EG_DATA_COR                    (0x00001000)

/* PBM_SPX_EG_RESOURCES : Register Bits Masks Definitions */
#define PBM_SPX_EG_RESOURCES_CRQ_ENTRIES                 (0x000000ff)
#define PBM_SPX_EG_RESOURCES_DATANODES                   (0x03ff0000)

/* PBM_SPX_BUFF_uint32_t : Register Bits Masks Definitions */
#define PBM_SPX_BUFF_uint32_t_EG_DATA_F                    (0x00200000)
#define PBM_SPX_BUFF_uint32_t_EG_CRQ_F                     (0x00400000)
#define PBM_SPX_BUFF_uint32_t_EG_MT                        (0x00800000)

/* PBM_SPX_SCRATCH1 : Register Bits Masks Definitions */
#define PBM_SPX_SCRATCH1_SCRATCH                         (0xffffffff)

/* PBM_SPX_SCRATCH2 : Register Bits Masks Definitions */
#define PBM_SPX_SCRATCH2_SCRATCH                         (0xffffffff)

/* PCNTR_BH : Register Bits Masks Definitions */
#define PCNTR_BH_BLK_TYPE                                (0x00000fff)
#define PCNTR_BH_BLK_REV                                 (0x0000f000)
#define PCNTR_BH_NEXT_BLK_PTR                            (0xffff0000)

/* PCNTR_CTL : Register Bits Masks Definitions */
#define PCNTR_CTL_CNTR_CLR                               (0x40000000)
#define PCNTR_CTL_CNTR_FRZ                               (0x80000000)

/* SPX_PCNTR_EN : Register Bits Masks Definitions */
#define SPX_PCNTR_EN_ENABLE                              (0x80000000)

/* SPX_PCNTR_CTLY : Register Bits Masks Definitions */
#define SPX_PCNTR_CTLY_SEL                               (0x0000007f)
#define SPX_PCNTR_CTLY_TX                                (0x00000080)
#define SPX_PCNTR_CTLY_PRIO0                             (0x00000100)
#define SPX_PCNTR_CTLY_PRIO0C                            (0x00000200)
#define SPX_PCNTR_CTLY_PRIO1                             (0x00000400)
#define SPX_PCNTR_CTLY_PRIO1C                            (0x00000800)
#define SPX_PCNTR_CTLY_PRIO2                             (0x00001000)
#define SPX_PCNTR_CTLY_PRIO2C                            (0x00002000)
#define SPX_PCNTR_CTLY_PRIO3                             (0x00004000)
#define SPX_PCNTR_CTLY_PRIO3C                            (0x00008000)

/* SPX_PCNTR_CNTY : Register Bits Masks Definitions */
#define SPX_PCNTR_CNTY_COUNT                             (0xffffffff)

/* PCAP_BH : Register Bits Masks Definitions */
#define PCAP_BH_BLK_TYPE                                 (0x00000fff)
#define PCAP_BH_BLK_REV                                  (0x0000f000)
#define PCAP_BH_NEXT_BLK_PTR                             (0xffff0000)

/* SPX_PCAP_ACC : Register Bits Masks Definitions */
#define SPX_PCAP_ACC_DCM_ON                              (0x80000000)

/* SPX_PCAP_CTL : Register Bits Masks Definitions */
#define SPX_PCAP_CTL_PORT                                (0x00000003)
#define SPX_PCAP_CTL_FRM_DME                             (0x00000004)
#define SPX_PCAP_CTL_RAW                                 (0x00000008)
#define SPX_PCAP_CTL_ADDR                                (0x00000ff0)
#define SPX_PCAP_CTL_ADD_WR_COUNT                        (0x00fff000)
#define SPX_PCAP_CTL_STOP_4_EL_INTB                      (0x01000000)
#define SPX_PCAP_CTL_STOP_4_EL_INTA                      (0x02000000)
#define SPX_PCAP_CTL_MODE                                (0x0c000000)
#define SPX_PCAP_CTL_STOP                                (0x10000000)
#define SPX_PCAP_CTL_DONE                                (0x20000000)
#define SPX_PCAP_CTL_START                               (0x40000000)
#define SPX_PCAP_CTL_PC_INIT                             (0x80000000)

/* SPX_PCAP_STAT : Register Bits Masks Definitions */
#define SPX_PCAP_STAT_TRIG_ADDR                          (0x00000ff0)
#define SPX_PCAP_STAT_FULL                               (0x01000000)
#define SPX_PCAP_STAT_TRIG                               (0x02000000)
#define SPX_PCAP_STAT_WRAP                               (0x04000000)
#define SPX_PCAP_STAT_EVNT                               (0x80000000)

/* SPX_PCAP_GEN : Register Bits Masks Definitions */
#define SPX_PCAP_GEN_GEN                                 (0x80000000)

/* SPX_PCAP_SB_DATA : Register Bits Masks Definitions */
#define SPX_PCAP_SB_DATA_DME_M3                          (0x00000010)
#define SPX_PCAP_SB_DATA_DME_M2                          (0x00000020)
#define SPX_PCAP_SB_DATA_DME_M1                          (0x00000040)
#define SPX_PCAP_SB_DATA_DME_M0                          (0x00000080)
#define SPX_PCAP_SB_DATA_CG_V3                           (0x00001000)
#define SPX_PCAP_SB_DATA_CG_V2                           (0x00002000)
#define SPX_PCAP_SB_DATA_CG_V1                           (0x00004000)
#define SPX_PCAP_SB_DATA_CG_V0                           (0x00008000)
#define SPX_PCAP_SB_DATA_CW_E1                           (0x00040000)
#define SPX_PCAP_SB_DATA_CW_T1                           (0x00080000)
#define SPX_PCAP_SB_DATA_CW_V1                           (0x00100000)
#define SPX_PCAP_SB_DATA_CW_E0                           (0x00200000)
#define SPX_PCAP_SB_DATA_CW_T0                           (0x00400000)
#define SPX_PCAP_SB_DATA_CW_V0                           (0x00800000)
#define SPX_PCAP_SB_DATA_LEN                             (0x1c000000)
#define SPX_PCAP_SB_DATA_TRUNCATED                       (0x20000000)
#define SPX_PCAP_SB_DATA_DISCARD                         (0x40000000)
#define SPX_PCAP_SB_DATA_EOP                             (0x80000000)

/* SPX_PCAP_MEM_DEPTH : Register Bits Masks Definitions */
#define SPX_PCAP_MEM_DEPTH_MEM_DEPTH                     (0xffffffff)

/* SPX_PCAP_DATAY : Register Bits Masks Definitions */
#define SPX_PCAP_DATAY_DATA                              (0xffffffff)

/* DBG_EL_BH : Register Bits Masks Definitions */
#define DBG_EL_BH_BLK_TYPE                               (0x00000fff)
#define DBG_EL_BH_BLK_REV                                (0x0000f000)
#define DBG_EL_BH_NEXT_BLK_PTR                           (0xffff0000)

/* SPX_DBG_EL_ACC : Register Bits Masks Definitions */
#define SPX_DBG_EL_ACC_EL_ON                             (0x80000000)

/* SPX_DBG_EL_INFO : Register Bits Masks Definitions */
#define SPX_DBG_EL_INFO_MAX_STAT_INDEX                   (0x0000000f)
#define SPX_DBG_EL_INFO_MEM_SIZE                         (0x00000700)
#define SPX_DBG_EL_INFO_VERSION                          (0xff000000)

/* SPX_DBG_EL_CTL : Register Bits Masks Definitions */
#define SPX_DBG_EL_CTL_T0_EN                             (0x00000001)
#define SPX_DBG_EL_CTL_T0_INTA                           (0x00000002)
#define SPX_DBG_EL_CTL_T0_INTB                           (0x00000004)
#define SPX_DBG_EL_CTL_T1_EN                             (0x00000010)
#define SPX_DBG_EL_CTL_T1_INTA                           (0x00000020)
#define SPX_DBG_EL_CTL_T1_INTB                           (0x00000040)
#define SPX_DBG_EL_CTL_TR_COND                           (0x00000700)
#define SPX_DBG_EL_CTL_TR_INTA                           (0x00001000)
#define SPX_DBG_EL_CTL_TR_INTB                           (0x00002000)
#define SPX_DBG_EL_CTL_RST                               (0x00008000)
#define SPX_DBG_EL_CTL_TRIG_POS                          (0x00ff0000)

/* SPX_DBG_EL_INT_EN : Register Bits Masks Definitions */
#define SPX_DBG_EL_INT_EN_EN_A                           (0x00000001)
#define SPX_DBG_EL_INT_EN_EN_B                           (0x00000002)

/* SPX_DBG_EL_SRC_LOG_EN : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_LOG_EN_EN_0                       (0x00000001)
#define SPX_DBG_EL_SRC_LOG_EN_EN_1                       (0x00000002)
#define SPX_DBG_EL_SRC_LOG_EN_EN_2                       (0x00000004)
#define SPX_DBG_EL_SRC_LOG_EN_EN_3                       (0x00000008)
#define SPX_DBG_EL_SRC_LOG_EN_EN_4                       (0x00000010)
#define SPX_DBG_EL_SRC_LOG_EN_EN_5                       (0x00000020)
#define SPX_DBG_EL_SRC_LOG_EN_EN_6                       (0x00000040)
#define SPX_DBG_EL_SRC_LOG_EN_EN_7                       (0x00000080)
#define SPX_DBG_EL_SRC_LOG_EN_EN_8                       (0x00000100)
#define SPX_DBG_EL_SRC_LOG_EN_EN_9                       (0x00000200)
#define SPX_DBG_EL_SRC_LOG_EN_EN_10                      (0x00000400)
#define SPX_DBG_EL_SRC_LOG_EN_EN_11                      (0x00000800)
#define SPX_DBG_EL_SRC_LOG_EN_EN_12                      (0x00001000)
#define SPX_DBG_EL_SRC_LOG_EN_EN_13                      (0x00002000)
#define SPX_DBG_EL_SRC_LOG_EN_EN_14                      (0x00004000)
#define SPX_DBG_EL_SRC_LOG_EN_EN_15                      (0x00008000)
#define SPX_DBG_EL_SRC_LOG_EN_CLR                        (0x80000000)

/* SPX_DBG_EL_TRIG0_MASK : Register Bits Masks Definitions */
#define SPX_DBG_EL_TRIG0_MASK_MASK                       (0x07ffffff)
#define SPX_DBG_EL_TRIG0_MASK_SAME_TIME_MASK             (0x08000000)
#define SPX_DBG_EL_TRIG0_MASK_SRC_MASK                   (0xf0000000)

/* SPX_DBG_EL_TRIG0_VAL : Register Bits Masks Definitions */
#define SPX_DBG_EL_TRIG0_VAL_EVENTS                      (0x07ffffff)
#define SPX_DBG_EL_TRIG0_VAL_SAME_TIME                   (0x08000000)
#define SPX_DBG_EL_TRIG0_VAL_SRC_VAL                     (0xf0000000)

/* SPX_DBG_EL_TRIG1_MASK : Register Bits Masks Definitions */
#define SPX_DBG_EL_TRIG1_MASK_MASK                       (0x07ffffff)
#define SPX_DBG_EL_TRIG1_MASK_SAME_TIME_MASK             (0x08000000)
#define SPX_DBG_EL_TRIG1_MASK_SRC_MASK                   (0xf0000000)

/* SPX_DBG_EL_TRIG1_VAL : Register Bits Masks Definitions */
#define SPX_DBG_EL_TRIG1_VAL_EVENTS                      (0x07ffffff)
#define SPX_DBG_EL_TRIG1_VAL_SAME_TIME                   (0x08000000)
#define SPX_DBG_EL_TRIG1_VAL_SRC_VAL                     (0xf0000000)

/* SPX_DBG_EL_TRIG_STAT : Register Bits Masks Definitions */
#define SPX_DBG_EL_TRIG_STAT_TRIG_0                      (0x00000001)
#define SPX_DBG_EL_TRIG_STAT_TRIG_1                      (0x00000002)
#define SPX_DBG_EL_TRIG_STAT_TRIG                        (0x00000004)
#define SPX_DBG_EL_TRIG_STAT_FULL                        (0x00000008)
#define SPX_DBG_EL_TRIG_STAT_INT_A                       (0x00000010)
#define SPX_DBG_EL_TRIG_STAT_INT_B                       (0x00000020)
#define SPX_DBG_EL_TRIG_STAT_ALL_VALID                   (0x00000080)

/* SPX_DBG_EL_WR_TRIG_IDX : Register Bits Masks Definitions */
#define SPX_DBG_EL_WR_TRIG_IDX_WR_IDX                    (0x000000ff)
#define SPX_DBG_EL_WR_TRIG_IDX_TRIG_IDX                  (0x00ff0000)

/* SPX_DBG_EL_RD_IDX : Register Bits Masks Definitions */
#define SPX_DBG_EL_RD_IDX_RD_IDX                         (0x000000ff)

/* SPX_DBG_EL_DATA : Register Bits Masks Definitions */
#define SPX_DBG_EL_DATA_EVENTS                           (0x07ffffff)
#define SPX_DBG_EL_DATA_SAME_TIME                        (0x08000000)
#define SPX_DBG_EL_DATA_SOURCE                           (0xf0000000)

/* SPX_DBG_EL_SRC_STATY : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STATY_CW_LOCK                     (0x00000001)
#define SPX_DBG_EL_SRC_STATY_CW_LOCK_LOST                (0x00000002)
#define SPX_DBG_EL_SRC_STATY_CW_BAD_TYPE                 (0x00000004)
#define SPX_DBG_EL_SRC_STATY_IG_CDC_OV                   (0x00000008)
#define SPX_DBG_EL_SRC_STATY_SEED_ERR                    (0x00000010)
#define SPX_DBG_EL_SRC_STATY_SKIPM_ERR                   (0x00000020)
#define SPX_DBG_EL_SRC_STATY_SKIPC_ERR                   (0x00000040)
#define SPX_DBG_EL_SRC_STATY_BAD_DME_FM                  (0x00000080)
#define SPX_DBG_EL_SRC_STATY_EG_CDC_OV                   (0x00000100)
#define SPX_DBG_EL_SRC_STATY_AEQ3                        (0x00000200)
#define SPX_DBG_EL_SRC_STATY_AEQ2                        (0x00000400)
#define SPX_DBG_EL_SRC_STATY_AEQ1                        (0x00000800)
#define SPX_DBG_EL_SRC_STATY_AEQ0                        (0x00001000)
#define SPX_DBG_EL_SRC_STATY_RE_FAIL                     (0x00002000)
#define SPX_DBG_EL_SRC_STATY_RE_TRN2                     (0x00004000)
#define SPX_DBG_EL_SRC_STATY_RE_TRN1                     (0x00008000)
#define SPX_DBG_EL_SRC_STATY_RE_TRN0                     (0x00010000)
#define SPX_DBG_EL_SRC_STATY_KEEP_ALIVE                  (0x00020000)
#define SPX_DBG_EL_SRC_STATY_TRAINED                     (0x00040000)
#define SPX_DBG_EL_SRC_STATY_CW_FAIL                     (0x00080000)
#define SPX_DBG_EL_SRC_STATY_CW_TRN1                     (0x00100000)
#define SPX_DBG_EL_SRC_STATY_CW_TRN0                     (0x00200000)
#define SPX_DBG_EL_SRC_STATY_DME_FAIL                    (0x00400000)
#define SPX_DBG_EL_SRC_STATY_DME_TRN2                    (0x00800000)
#define SPX_DBG_EL_SRC_STATY_DME_TRN1                    (0x01000000)
#define SPX_DBG_EL_SRC_STATY_DME_TRN0                    (0x02000000)
#define SPX_DBG_EL_SRC_STATY_UNTRAINED                   (0x04000000)

/* SPX_DBG_EL_SRC_STAT4 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT4_IDLE                        (0x00000001)
#define SPX_DBG_EL_SRC_STAT4_XMT_WIDTH                   (0x00000002)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN0                     (0x00000004)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN1                     (0x00000008)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN2                     (0x00000010)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN3                     (0x00000020)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN4                     (0x00000040)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN5                     (0x00000080)
#define SPX_DBG_EL_SRC_STAT4_RE_TRN_TO                   (0x00000100)
#define SPX_DBG_EL_SRC_STAT4_SOFTWARE                    (0x00000600)
#define SPX_DBG_EL_SRC_STAT4_LOST_CS                     (0x00000800)
#define SPX_DBG_EL_SRC_STAT4_RETRAIN_1X                  (0x00001000)
#define SPX_DBG_EL_SRC_STAT4_RETRAIN_2X                  (0x00002000)
#define SPX_DBG_EL_SRC_STAT4_RETRAIN_4X                  (0x00004000)
#define SPX_DBG_EL_SRC_STAT4_MODE_2X                     (0x00008000)
#define SPX_DBG_EL_SRC_STAT4_RECOV_2X                    (0x00010000)
#define SPX_DBG_EL_SRC_STAT4_MODE_4X                     (0x00020000)
#define SPX_DBG_EL_SRC_STAT4_RECOV_4X                    (0x00040000)
#define SPX_DBG_EL_SRC_STAT4_LANE2_1XR                   (0x00080000)
#define SPX_DBG_EL_SRC_STAT4_LANE1_1XR                   (0x00100000)
#define SPX_DBG_EL_SRC_STAT4_LANE0_1XR                   (0x00200000)
#define SPX_DBG_EL_SRC_STAT4_RECOV_1X                    (0x00400000)
#define SPX_DBG_EL_SRC_STAT4_DISCOVERY                   (0x00800000)
#define SPX_DBG_EL_SRC_STAT4_SEEK                        (0x01000000)
#define SPX_DBG_EL_SRC_STAT4_ASYM_MODE                   (0x02000000)
#define SPX_DBG_EL_SRC_STAT4_SILENT                      (0x04000000)

/* SPX_DBG_EL_SRC_STAT5 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT5_ASYM_TX_EXIT                (0x00000001)
#define SPX_DBG_EL_SRC_STAT5_ASYM_TX_IDLE                (0x00000002)
#define SPX_DBG_EL_SRC_STAT5_TX_NACK                     (0x00000004)
#define SPX_DBG_EL_SRC_STAT5_SK_TX_1X                    (0x00000008)
#define SPX_DBG_EL_SRC_STAT5_SK_TX1_1X                   (0x00000010)
#define SPX_DBG_EL_SRC_STAT5_SK_TX2_1X                   (0x00000020)
#define SPX_DBG_EL_SRC_STAT5_SK_TX3_1X                   (0x00000040)
#define SPX_DBG_EL_SRC_STAT5_TX_ACK_1X                   (0x00000080)
#define SPX_DBG_EL_SRC_STAT5_TX_1X                       (0x00000100)
#define SPX_DBG_EL_SRC_STAT5_SK_TX_2X                    (0x00000200)
#define SPX_DBG_EL_SRC_STAT5_SK_TX1_2X                   (0x00000400)
#define SPX_DBG_EL_SRC_STAT5_SK_TX2_2X                   (0x00000800)
#define SPX_DBG_EL_SRC_STAT5_SK_TX3_2X                   (0x00001000)
#define SPX_DBG_EL_SRC_STAT5_TX_ACK_2X                   (0x00002000)
#define SPX_DBG_EL_SRC_STAT5_TX_2X                       (0x00004000)
#define SPX_DBG_EL_SRC_STAT5_SK_TX_4X                    (0x00008000)
#define SPX_DBG_EL_SRC_STAT5_SK_TX1_4X                   (0x00010000)
#define SPX_DBG_EL_SRC_STAT5_SK_TX2_4X                   (0x00020000)
#define SPX_DBG_EL_SRC_STAT5_SK_TX3_4X                   (0x00040000)
#define SPX_DBG_EL_SRC_STAT5_TX_ACK_4X                   (0x00080000)
#define SPX_DBG_EL_SRC_STAT5_TX_4X                       (0x00100000)
#define SPX_DBG_EL_SRC_STAT5_SOFTWARE                    (0x00600000)
#define SPX_DBG_EL_SRC_STAT5_TXW_IDLE                    (0x00800000)
#define SPX_DBG_EL_SRC_STAT5_TXW_CMD3                    (0x01000000)
#define SPX_DBG_EL_SRC_STAT5_TXW_CMD2                    (0x02000000)
#define SPX_DBG_EL_SRC_STAT5_TXW_CMD1                    (0x04000000)

/* SPX_DBG_EL_SRC_STAT6 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT6_ASYM_RX_EXIT                (0x00000001)
#define SPX_DBG_EL_SRC_STAT6_ASYM_RX_IDLE                (0x00000002)
#define SPX_DBG_EL_SRC_STAT6_RX_NACK                     (0x00000004)
#define SPX_DBG_EL_SRC_STAT6_SK_RX_1X                    (0x00000008)
#define SPX_DBG_EL_SRC_STAT6_ACK_RX_1X                   (0x00000010)
#define SPX_DBG_EL_SRC_STAT6_RECOV_RX_1X                 (0x00000020)
#define SPX_DBG_EL_SRC_STAT6_RX_1X                       (0x00000040)
#define SPX_DBG_EL_SRC_STAT6_SK_RX_2X                    (0x00000080)
#define SPX_DBG_EL_SRC_STAT6_ACK_RX_2X                   (0x00000100)
#define SPX_DBG_EL_SRC_STAT6_RECOV_RX_2X                 (0x00000200)
#define SPX_DBG_EL_SRC_STAT6_RX_2X                       (0x00000400)
#define SPX_DBG_EL_SRC_STAT6_SK_RX_4X                    (0x00000800)
#define SPX_DBG_EL_SRC_STAT6_ACK_RX_4X                   (0x00001000)
#define SPX_DBG_EL_SRC_STAT6_RECOV_RX_4X                 (0x00002000)
#define SPX_DBG_EL_SRC_STAT6_RX_4X                       (0x00004000)
#define SPX_DBG_EL_SRC_STAT6_SOFTWARE                    (0x007f8000)
#define SPX_DBG_EL_SRC_STAT6_RXW_IDLE                    (0x00800000)
#define SPX_DBG_EL_SRC_STAT6_RXW_CMD3                    (0x01000000)
#define SPX_DBG_EL_SRC_STAT6_RXW_CMD2                    (0x02000000)
#define SPX_DBG_EL_SRC_STAT6_RXW_CMD1                    (0x04000000)

/* SPX_DBG_EL_SRC_STAT7 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT7_PORT_UNINIT                 (0x00000001)
#define SPX_DBG_EL_SRC_STAT7_PORT_OK                     (0x00000002)
#define SPX_DBG_EL_SRC_STAT7_PORT_ERR_SET                (0x00000004)
#define SPX_DBG_EL_SRC_STAT7_PORT_ERR_CLR                (0x00000008)
#define SPX_DBG_EL_SRC_STAT7_PW_PEND_SET                 (0x00000010)
#define SPX_DBG_EL_SRC_STAT7_PW_PEND_CLR                 (0x00000020)
#define SPX_DBG_EL_SRC_STAT7_INP_ERR_SET                 (0x00000040)
#define SPX_DBG_EL_SRC_STAT7_INP_ERR_CLR                 (0x00000080)
#define SPX_DBG_EL_SRC_STAT7_OUT_ERR_SET                 (0x00000100)
#define SPX_DBG_EL_SRC_STAT7_OUT_ERR_CLR                 (0x00000200)
#define SPX_DBG_EL_SRC_STAT7_CS_CRC_ERR                  (0x00000400)
#define SPX_DBG_EL_SRC_STAT7_PKT_CRC_ERR                 (0x00000800)
#define SPX_DBG_EL_SRC_STAT7_PKT_ILL_ACKID               (0x00001000)
#define SPX_DBG_EL_SRC_STAT7_CS_ILL_ACKID                (0x00002000)
#define SPX_DBG_EL_SRC_STAT7_PROT_ERR                    (0x00004000)
#define SPX_DBG_EL_SRC_STAT7_DESCRAM_LOS                 (0x00008000)
#define SPX_DBG_EL_SRC_STAT7_DESCRAM_RESYNC              (0x00010000)
#define SPX_DBG_EL_SRC_STAT7_LINK_TO                     (0x00020000)
#define SPX_DBG_EL_SRC_STAT7_STYP0_RSVD                  (0x00040000)
#define SPX_DBG_EL_SRC_STAT7_STYP1_RSVD                  (0x00080000)
#define SPX_DBG_EL_SRC_STAT7_DELIN_ERR                   (0x00100000)
#define SPX_DBG_EL_SRC_STAT7_PRBS_ERROR                  (0x00200000)
#define SPX_DBG_EL_SRC_STAT7_PKT_ILL_SIZE                (0x00400000)
#define SPX_DBG_EL_SRC_STAT7_LR_ACKID_ILL                (0x00800000)
#define SPX_DBG_EL_SRC_STAT7_CS_ACK_ILL                  (0x01000000)
#define SPX_DBG_EL_SRC_STAT7_OUT_FAIL_SET                (0x02000000)
#define SPX_DBG_EL_SRC_STAT7_OUT_FAIL_CLR                (0x04000000)

/* SPX_DBG_EL_SRC_STAT8 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT8_RX_SOP                      (0x00000001)
#define SPX_DBG_EL_SRC_STAT8_RX_EOP                      (0x00000002)
#define SPX_DBG_EL_SRC_STAT8_RX_STOMP                    (0x00000004)
#define SPX_DBG_EL_SRC_STAT8_RX_RFR                      (0x00000008)
#define SPX_DBG_EL_SRC_STAT8_RX_LREQ                     (0x00000010)
#define SPX_DBG_EL_SRC_STAT8_RX_LRESP                    (0x00000020)
#define SPX_DBG_EL_SRC_STAT8_RX_PACK                     (0x00000040)
#define SPX_DBG_EL_SRC_STAT8_RX_RETRY                    (0x00000080)
#define SPX_DBG_EL_SRC_STAT8_RX_PNA                      (0x00000100)
#define SPX_DBG_EL_SRC_STAT8_SOFTWARE3                   (0x00000600)
#define SPX_DBG_EL_SRC_STAT8_ALIGN6                      (0x00000800)
#define SPX_DBG_EL_SRC_STAT8_ALIGN5_7                    (0x00001000)
#define SPX_DBG_EL_SRC_STAT8_ALIGN4                      (0x00002000)
#define SPX_DBG_EL_SRC_STAT8_ALIGN3                      (0x00004000)
#define SPX_DBG_EL_SRC_STAT8_ALIGN2                      (0x00008000)
#define SPX_DBG_EL_SRC_STAT8_SOFTWARE5                   (0x00010000)
#define SPX_DBG_EL_SRC_STAT8_ALIGNED                     (0x00020000)
#define SPX_DBG_EL_SRC_STAT8_SOFTWARE2                   (0x00040000)
#define SPX_DBG_EL_SRC_STAT8_NALIGN2_3                   (0x00080000)
#define SPX_DBG_EL_SRC_STAT8_NALIGN1                     (0x00100000)
#define SPX_DBG_EL_SRC_STAT8_NALIGN                      (0x00200000)
#define SPX_DBG_EL_SRC_STAT8_SOFTWARE                    (0x00400000)
#define SPX_DBG_EL_SRC_STAT8_LP_RX_TRAIND                (0x00800000)
#define SPX_DBG_EL_SRC_STAT8_RX_TRAIND                   (0x01000000)
#define SPX_DBG_EL_SRC_STAT8_SOFTWARE4                   (0x06000000)

/* SPX_DBG_EL_SRC_STAT9 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT9_SOFTWARE                    (0x07ffffff)

/* SPX_DBG_EL_SRC_STAT10 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT10_TX_SOP                     (0x00000001)
#define SPX_DBG_EL_SRC_STAT10_TX_EOP                     (0x00000002)
#define SPX_DBG_EL_SRC_STAT10_TX_STOMP                   (0x00000004)
#define SPX_DBG_EL_SRC_STAT10_TX_RFR                     (0x00000008)
#define SPX_DBG_EL_SRC_STAT10_TX_LREQ                    (0x00000010)
#define SPX_DBG_EL_SRC_STAT10_TX_LRESP                   (0x00000020)
#define SPX_DBG_EL_SRC_STAT10_TX_PACK                    (0x00000040)
#define SPX_DBG_EL_SRC_STAT10_TX_RETRY                   (0x00000080)
#define SPX_DBG_EL_SRC_STAT10_TX_PNA                     (0x00000100)
#define SPX_DBG_EL_SRC_STAT10_SOFTWARE                   (0x07fffe00)

/* SPX_DBG_EL_SRC_STAT11 : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STAT11_SOFTWARE5                  (0x00000001)
#define SPX_DBG_EL_SRC_STAT11_PNA_IN_OES                 (0x00000002)
#define SPX_DBG_EL_SRC_STAT11_TWO_RETRY                  (0x00000004)
#define SPX_DBG_EL_SRC_STAT11_UNEXP_PR_PA                (0x00000008)
#define SPX_DBG_EL_SRC_STAT11_SOP_EOP                    (0x00000010)
#define SPX_DBG_EL_SRC_STAT11_TWO_SOP                    (0x00000020)
#define SPX_DBG_EL_SRC_STAT11_XTRA_LR                    (0x00000040)
#define SPX_DBG_EL_SRC_STAT11_UNEXP_EOP                  (0x00000080)
#define SPX_DBG_EL_SRC_STAT11_UNEXP_RFR                  (0x00000100)
#define SPX_DBG_EL_SRC_STAT11_UNEXP_LR                   (0x00000200)
#define SPX_DBG_EL_SRC_STAT11_BAD_CSEB_END               (0x00000400)
#define SPX_DBG_EL_SRC_STAT11_BAD_CS_CW_SEQ              (0x00000800)
#define SPX_DBG_EL_SRC_STAT11_BAD_SC_OS                  (0x00001000)
#define SPX_DBG_EL_SRC_STAT11_SOFTWARE4                  (0x00002000)
#define SPX_DBG_EL_SRC_STAT11_CTL_CW_RSVD                (0x00004000)
#define SPX_DBG_EL_SRC_STAT11_BAD_OS                     (0x00008000)
#define SPX_DBG_EL_SRC_STAT11_IDLE3_BAD_COL              (0x00010000)
#define SPX_DBG_EL_SRC_STAT11_SOFTWARE2                  (0x00020000)
#define SPX_DBG_EL_SRC_STAT11_IDLE3_DATA_NON_ZERO        (0x00040000)
#define SPX_DBG_EL_SRC_STAT11_LR_NO_SEED                 (0x00080000)
#define SPX_DBG_EL_SRC_STAT11_NON_ZERO                   (0x00100000)
#define SPX_DBG_EL_SRC_STAT11_OS_IN_PKT                  (0x00200000)
#define SPX_DBG_EL_SRC_STAT11_SOFTWARE3                  (0x00400000)
#define SPX_DBG_EL_SRC_STAT11_BAD_PAD                    (0x00800000)
#define SPX_DBG_EL_SRC_STAT11_SOFTWARE                   (0x07000000)

/* SPX_DBG_EL_SRC_STATY : Register Bits Masks Definitions */
#define SPX_DBG_EL_SRC_STATY_AEQ17                       (0x00000001)
#define SPX_DBG_EL_SRC_STATY_AEQ16                       (0x00000002)
#define SPX_DBG_EL_SRC_STATY_BAD_LANE_CHECK              (0x00000004)
#define SPX_DBG_EL_SRC_STATY_BIP_IBIP_MISMATCH           (0x00000008)
#define SPX_DBG_EL_SRC_STATY_BAD_BIP                     (0x00000010)
#define SPX_DBG_EL_SRC_STATY_AEQ15                       (0x00000020)
#define SPX_DBG_EL_SRC_STATY_AEQ14                       (0x00000040)
#define SPX_DBG_EL_SRC_STATY_AEQ13                       (0x00000080)
#define SPX_DBG_EL_SRC_STATY_AEQ12                       (0x00000100)
#define SPX_DBG_EL_SRC_STATY_FL_TST_MK                   (0x00000200)
#define SPX_DBG_EL_SRC_STATY_FL_NEW_MK                   (0x00000400)
#define SPX_DBG_EL_SRC_STATY_AEQ11                       (0x00000800)
#define SPX_DBG_EL_SRC_STATY_FL_OOFM                     (0x00001000)
#define SPX_DBG_EL_SRC_STATY_AEQ9                        (0x00002000)
#define SPX_DBG_EL_SRC_STATY_AEQ8                        (0x00004000)
#define SPX_DBG_EL_SRC_STATY_AEQ7                        (0x00008000)
#define SPX_DBG_EL_SRC_STATY_AEQ6                        (0x00010000)
#define SPX_DBG_EL_SRC_STATY_AEQ5                        (0x00020000)
#define SPX_DBG_EL_SRC_STATY_AEQ4                        (0x00040000)
#define SPX_DBG_EL_SRC_STATY_NO_SYNC_1                   (0x00080000)
#define SPX_DBG_EL_SRC_STATY_NO_SYNC2_3_4                (0x00100000)
#define SPX_DBG_EL_SRC_STATY_SYNC                        (0x00200000)
#define SPX_DBG_EL_SRC_STATY_SYNC1_2                     (0x00400000)
#define SPX_DBG_EL_SRC_STATY_NO_LOCK                     (0x00800000)
#define SPX_DBG_EL_SRC_STATY_AEQ10                       (0x01000000)
#define SPX_DBG_EL_SRC_STATY_NO_LOCK1_2_3_4              (0x02000000)
#define SPX_DBG_EL_SRC_STATY_LOCK_1_2_3                  (0x04000000)

/* LANE_TEST_BH : Register Bits Masks Definitions */
#define LANE_TEST_BH_BLK_TYPE                            (0x00000fff)
#define LANE_TEST_BH_BLK_REV                             (0x0000f000)
#define LANE_TEST_BH_NEXT_BLK_PTR                        (0xffff0000)

/* LANEX_PRBS_CTRL : Register Bits Masks Definitions */
#define LANEX_PRBS_CTRL_TRANSMIT                         (0x01000000)
#define LANEX_PRBS_CTRL_ENABLE                           (0x02000000)
#define LANEX_PRBS_CTRL_TRAIN                            (0x04000000)
#define LANEX_PRBS_CTRL_INVERT                           (0x08000000)
#define LANEX_PRBS_CTRL_PATTERN                          (0xf0000000)

/* LANEX_PRBS_uint32_t : Register Bits Masks Definitions */
#define LANEX_PRBS_uint32_t_FIXED_UNLOCK                   (0x00000001)
#define LANEX_PRBS_uint32_t_PRBS_LOS                       (0x00000002)

/* LANEX_PRBS_ERR_CNT : Register Bits Masks Definitions */
#define LANEX_PRBS_ERR_CNT_COUNT                         (0xffffffff)

/* LANEX_PRBS_SEED_0U : Register Bits Masks Definitions */
#define LANEX_PRBS_SEED_0U_SEED                          (0x0000ffff)

/* LANEX_PRBS_SEED_0M : Register Bits Masks Definitions */
#define LANEX_PRBS_SEED_0M_SEED                          (0xffffffff)

/* LANEX_PRBS_SEED_0L : Register Bits Masks Definitions */
#define LANEX_PRBS_SEED_0L_SEED                          (0xffffffff)

/* LANEX_PRBS_SEED_1U : Register Bits Masks Definitions */
#define LANEX_PRBS_SEED_1U_SEED                          (0x0000ffff)

/* LANEX_PRBS_SEED_1M : Register Bits Masks Definitions */
#define LANEX_PRBS_SEED_1M_SEED                          (0xffffffff)

/* LANEX_PRBS_SEED_1L : Register Bits Masks Definitions */
#define LANEX_PRBS_SEED_1L_SEED                          (0xffffffff)

/* LANEX_BER_CTL : Register Bits Masks Definitions */
#define LANEX_BER_CTL_COUNT                              (0x000fffff)
#define LANEX_BER_CTL_BLIP_CNT                           (0x01000000)
#define LANEX_BER_CTL_BLIP_SCRAM                         (0x02000000)
#define LANEX_BER_CTL_BLIP_BIP                           (0x04000000)
#define LANEX_BER_CTL_WIDTH                              (0xf0000000)

/* LANEX_BER_DATA_0 : Register Bits Masks Definitions */
#define LANEX_BER_DATA_0_DATA_LO                         (0xffffffff)

/* LANEX_BER_DATA_1 : Register Bits Masks Definitions */
#define LANEX_BER_DATA_1_DATA_HI                         (0xffffffff)

/* LANEX_PCS_DBG : Register Bits Masks Definitions */
#define LANEX_PCS_DBG_LOCK_DIS                           (0x00000001)
#define LANEX_PCS_DBG_KEEP_SKIP                          (0x00000002)
#define LANEX_PCS_DBG_FRC_RX_EN                          (0x00000004)
#define LANEX_PCS_DBG_LSYNC_DIS                          (0x80000000)

/* LANEX_BERM_CTL : Register Bits Masks Definitions */
#define LANEX_BERM_CTL_DEGRADED_THRESHOLD                (0x00000007)
#define LANEX_BERM_CTL_TEST_PERIODS                      (0x00000070)
#define LANEX_BERM_CTL_COUNTING_MODE                     (0x00040000)
#define LANEX_BERM_CTL_INVALID_DEGRADES                  (0x00080000)
#define LANEX_BERM_CTL_RETRAIN_REQ                       (0x00100000)
#define LANEX_BERM_CTL_DEGRADED                          (0x00200000)
#define LANEX_BERM_CTL_VALID                             (0x00400000)
#define LANEX_BERM_CTL_MAX_COUNT                         (0x00800000)
#define LANEX_BERM_CTL_STOP_ON_ERR                       (0x01000000)
#define LANEX_BERM_CTL_TRAINING_CHECK                    (0x02000000)
#define LANEX_BERM_CTL_HW_EVAL                           (0x04000000)
#define LANEX_BERM_CTL_RETRAIN                           (0x08000000)
#define LANEX_BERM_CTL_HALT                              (0x10000000)
#define LANEX_BERM_CTL_MODE                              (0x20000000)
#define LANEX_BERM_CTL_MONITOR                           (0x40000000)
#define LANEX_BERM_CTL_HW_BERM                           (0x80000000)

/* LANEX_BERM_CNTR : Register Bits Masks Definitions */
#define LANEX_BERM_CNTR_CUR_ERROR_COUNT                  (0x0000ffff)
#define LANEX_BERM_CNTR_LAST_ERROR_COUNT                 (0xffff0000)

/* LANEX_BERM_PD : Register Bits Masks Definitions */
#define LANEX_BERM_PD_TEST_PERIOD                        (0xffffffff)

/* LANEX_BERM_BITS : Register Bits Masks Definitions */
#define LANEX_BERM_BITS_TESTED_BITS                      (0xffffffff)

/* LANEX_BERM_ERRORS : Register Bits Masks Definitions */
#define LANEX_BERM_ERRORS_ERROR_THRESHOLD                (0x0000ffff)

/* LANEX_BERM_PERIODS : Register Bits Masks Definitions */
#define LANEX_BERM_PERIODS_DEGRADED_PERIODS              (0x0000ff00)
#define LANEX_BERM_PERIODS_VALID_PERIODS                 (0x00ff0000)
#define LANEX_BERM_PERIODS_COMPLETED_PERIODS             (0xff000000)

/* LANEX_DME_TEST : Register Bits Masks Definitions */
#define LANEX_DME_TEST_LOCK                              (0x00000001)
#define LANEX_DME_TEST_BAD_CELL                          (0x00000002)
#define LANEX_DME_TEST_BAD_RES                           (0x00000004)
#define LANEX_DME_TEST_BAD_FM                            (0x00000008)
#define LANEX_DME_TEST_BAD_FM_MAX                        (0x00070000)
#define LANEX_DME_TEST_CC_STRICT                         (0x03000000)
#define LANEX_DME_TEST_FM_STRICT                         (0x30000000)

/* FAB_PORT_BH : Register Bits Masks Definitions */
#define FAB_PORT_BH_BLK_TYPE                             (0x00000fff)
#define FAB_PORT_BH_BLK_REV                              (0x0000f000)
#define FAB_PORT_BH_NEXT_BLK_PTR                         (0xffff0000)

/* FP_X_IB_BUFF_WM_01 : Register Bits Masks Definitions */
#define FP_X_IB_BUFF_WM_01_IB_PRIO0_CRF1_RSVD_PAGES      (0x0000ff00)
#define FP_X_IB_BUFF_WM_01_IB_PRIO1_CRF0_RSVD_PAGES      (0x00ff0000)
#define FP_X_IB_BUFF_WM_01_IB_PRIO1_CRF1_RSVD_PAGES      (0xff000000)

/* FP_X_IB_BUFF_WM_23 : Register Bits Masks Definitions */
#define FP_X_IB_BUFF_WM_23_IB_PRIO2_CRF0_RSVD_PAGES      (0x000000ff)
#define FP_X_IB_BUFF_WM_23_IB_PRIO2_CRF1_RSVD_PAGES      (0x0000ff00)
#define FP_X_IB_BUFF_WM_23_IB_PRIO3_CRF0_RSVD_PAGES      (0x00ff0000)
#define FP_X_IB_BUFF_WM_23_IB_PRIO3_CRF1_RSVD_PAGES      (0xff000000)

/* FP_X_PLW_SCRATCH : Register Bits Masks Definitions */
#define FP_X_PLW_SCRATCH_SCRATCH                         (0xffffffff)

/* BC_L0_G0_ENTRYX_CSR : Register Bits Masks Definitions */
#define BC_L0_G0_ENTRYX_CSR_ROUTING_VALUE                (0x000003ff)
#define BC_L0_G0_ENTRYX_CSR_CAPTURE                      (0x80000000)

/* BC_L1_GX_ENTRYY_CSR : Register Bits Masks Definitions */
#define BC_L1_GX_ENTRYY_CSR_ROUTING_VALUE                (0x000003ff)
#define BC_L1_GX_ENTRYY_CSR_CAPTURE                      (0x80000000)

/* BC_L2_GX_ENTRYY_CSR : Register Bits Masks Definitions */
#define BC_L2_GX_ENTRYY_CSR_ROUTING_VALUE                (0x000003ff)
#define BC_L2_GX_ENTRYY_CSR_CAPTURE                      (0x80000000)

/* BC_MC_X_S_CSR : Register Bits Masks Definitions */
#define BC_MC_X_S_CSR_SET                                (0x00ffffff)

/* BC_MC_X_C_CSR : Register Bits Masks Definitions */
#define BC_MC_X_C_CSR_CLR                                (0x00ffffff)

/* FAB_INGR_CTL_BH : Register Bits Masks Definitions */
#define FAB_INGR_CTL_BH_BLK_TYPE                         (0x00000fff)
#define FAB_INGR_CTL_BH_BLK_REV                          (0x0000f000)
#define FAB_INGR_CTL_BH_NEXT_BLK_PTR                     (0xffff0000)

/* FAB_IG_X_2X4X_4X2X_WM : Register Bits Masks Definitions */
#define FAB_IG_X_2X4X_4X2X_WM_CB_PRIO1_RSVD_PAGES        (0x00001f00)
#define FAB_IG_X_2X4X_4X2X_WM_CB_PRIO2_RSVD_PAGES        (0x001f0000)
#define FAB_IG_X_2X4X_4X2X_WM_CB_PRIO3_RSVD_PAGES        (0x1f000000)

/* FAB_IG_X_2X2X_WM : Register Bits Masks Definitions */
#define FAB_IG_X_2X2X_WM_CB_PRIO1_RSVD_PAGES             (0x00000f00)
#define FAB_IG_X_2X2X_WM_CB_PRIO2_RSVD_PAGES             (0x000f0000)
#define FAB_IG_X_2X2X_WM_CB_PRIO3_RSVD_PAGES             (0x0f000000)

/* FAB_IG_X_4X4X_WM : Register Bits Masks Definitions */
#define FAB_IG_X_4X4X_WM_CB_PRIO1_RSVD_PAGES             (0x00003f00)
#define FAB_IG_X_4X4X_WM_CB_PRIO2_RSVD_PAGES             (0x003f0000)
#define FAB_IG_X_4X4X_WM_CB_PRIO3_RSVD_PAGES             (0x3f000000)

/* FAB_IG_X_MTC_VOQ_ACT : Register Bits Masks Definitions */
#define FAB_IG_X_MTC_VOQ_ACT_ACTIVE                      (0x00000001)

/* FAB_IG_X_VOQ_ACT : Register Bits Masks Definitions */
#define FAB_IG_X_VOQ_ACT_ACTIVE                          (0x00ffffff)

/* FAB_IG_X_CTL : Register Bits Masks Definitions */
#define FAB_IG_X_CTL_GRAD_THRESHOLD                      (0x001f0000)
#define FAB_IG_X_CTL_GRAD_AGE                            (0x01000000)
#define FAB_IG_X_CTL_INST_AGE                            (0x10000000)

/* FAB_IG_X_SCRATCH : Register Bits Masks Definitions */
#define FAB_IG_X_SCRATCH_SCRATCH                         (0xffffffff)

/* EM_BH : Register Bits Masks Definitions */
#define EM_BH_BLK_TYPE                                   (0x00000fff)
#define EM_BH_BLK_REV                                    (0x0000f000)
#define EM_BH_NEXT_BLK_PTR                               (0xffff0000)

/* EM_INT_STAT : Register Bits Masks Definitions */
#define EM_INT_STAT_IG_DATA_UNCOR                        (0x00000400)
#define EM_INT_STAT_IG_DATA_COR                          (0x00000800)
#define EM_INT_STAT_DEL_A                                (0x00001000)
#define EM_INT_STAT_DEL_B                                (0x00002000)
#define EM_INT_STAT_BOOT_MEM_UNCOR                       (0x00004000)
#define EM_INT_STAT_BOOT_MEM_COR                         (0x00008000)
#define EM_INT_STAT_EXTERNAL                             (0x00fe0000)
#define EM_INT_STAT_MECS                                 (0x04000000)
#define EM_INT_STAT_RCS                                  (0x08000000)
#define EM_INT_STAT_LOG                                  (0x10000000)
#define EM_INT_STAT_PORT                                 (0x20000000)
#define EM_INT_STAT_FAB                                  (0x40000000)

/* EM_INT_EN : Register Bits Masks Definitions */
#define EM_INT_EN_IG_DATA_UNCOR                          (0x00000400)
#define EM_INT_EN_IG_DATA_COR                            (0x00000800)
#define EM_INT_EN_DEL_A                                  (0x00001000)
#define EM_INT_EN_DEL_B                                  (0x00002000)
#define EM_INT_EN_BOOT_MEM_UNCOR                         (0x00004000)
#define EM_INT_EN_BOOT_MEM_COR                           (0x00008000)
#define EM_INT_EN_EXTERNAL                               (0x00fe0000)
#define EM_INT_EN_MECS                                   (0x04000000)
#define EM_INT_EN_LOG                                    (0x10000000)
#define EM_INT_EN_FAB                                    (0x40000000)

/* EM_INT_PORT_STAT : Register Bits Masks Definitions */
#define EM_INT_PORT_STAT_IRQ_PENDING                     (0x00ffffff)

/* EM_PW_STAT : Register Bits Masks Definitions */
#define EM_PW_STAT_MULTIPORT_ERR                         (0x00000200)
#define EM_PW_STAT_IG_DATA_UNCOR                         (0x00000400)
#define EM_PW_STAT_IG_DATA_COR                           (0x00000800)
#define EM_PW_STAT_DEL_A                                 (0x00001000)
#define EM_PW_STAT_DEL_B                                 (0x00002000)
#define EM_PW_STAT_BOOT_MEM_UNCOR                        (0x00004000)
#define EM_PW_STAT_BOOT_MEM_COR                          (0x00008000)
#define EM_PW_STAT_EXTERNAL                              (0x00fe0000)
#define EM_PW_STAT_RCS                                   (0x08000000)
#define EM_PW_STAT_LOG                                   (0x10000000)
#define EM_PW_STAT_PORT                                  (0x20000000)
#define EM_PW_STAT_FAB                                   (0x40000000)

/* EM_PW_EN : Register Bits Masks Definitions */
#define EM_PW_EN_IG_DATA_UNCOR                           (0x00000400)
#define EM_PW_EN_IG_DATA_COR                             (0x00000800)
#define EM_PW_EN_DEL_A                                   (0x00001000)
#define EM_PW_EN_DEL_B                                   (0x00002000)
#define EM_PW_EN_BOOT_MEM_UNCOR                          (0x00004000)
#define EM_PW_EN_BOOT_MEM_COR                            (0x00008000)
#define EM_PW_EN_EXTERNAL                                (0x00fe0000)
#define EM_PW_EN_LOG                                     (0x10000000)
#define EM_PW_EN_FAB                                     (0x40000000)

/* EM_PW_PORT_STAT : Register Bits Masks Definitions */
#define EM_PW_PORT_STAT_PW_PENDING                       (0x00ffffff)

/* EM_DEV_INT_EN : Register Bits Masks Definitions */
#define EM_DEV_INT_EN_INT_EN                             (0x00000001)
#define EM_DEV_INT_EN_INT                                (0x00010000)

/* EM_EVENT_GEN : Register Bits Masks Definitions */
#define EM_EVENT_GEN_IG_DATA_UNCOR                       (0x00000400)
#define EM_EVENT_GEN_IG_DATA_COR                         (0x00000800)
#define EM_EVENT_GEN_BOOT_MEM_UNCOR                      (0x00004000)
#define EM_EVENT_GEN_BOOT_MEM_COR                        (0x00008000)

/* EM_MECS_CTL : Register Bits Masks Definitions */
#define EM_MECS_CTL_OUT_EN                               (0x00000001)
#define EM_MECS_CTL_IN_EN                                (0x00000002)
#define EM_MECS_CTL_SEND                                 (0x00000010)
#define EM_MECS_CTL_IN_EDGE                              (0x00000100)

/* EM_MECS_INT_EN : Register Bits Masks Definitions */
#define EM_MECS_INT_EN_MECS_INT_EN                       (0x00ffffff)

/* EM_MECS_PORT_STAT : Register Bits Masks Definitions */
#define EM_MECS_PORT_STAT_PORT                           (0x00ffffff)

/* EM_RST_PORT_STAT : Register Bits Masks Definitions */
#define EM_RST_PORT_STAT_RST_REQ                         (0x00ffffff)

/* EM_RST_INT_EN : Register Bits Masks Definitions */
#define EM_RST_INT_EN_RST_INT_EN                         (0x00ffffff)

/* EM_RST_PW_EN : Register Bits Masks Definitions */
#define EM_RST_PW_EN_RST_PW_EN                           (0x00ffffff)

/* EM_FAB_INT_STAT : Register Bits Masks Definitions */
#define EM_FAB_INT_STAT_PORT                             (0x00ffffff)

/* EM_FAB_PW_STAT : Register Bits Masks Definitions */
#define EM_FAB_PW_STAT_PORT                              (0x00ffffff)

/* PW_BH : Register Bits Masks Definitions */
#define PW_BH_BLK_TYPE                                   (0x00000fff)
#define PW_BH_BLK_REV                                    (0x0000f000)
#define PW_BH_NEXT_BLK_PTR                               (0xffff0000)

/* PW_CTL : Register Bits Masks Definitions */
#define PW_CTL_PW_TMR                                    (0xffffff00)

/* PW_ROUTE : Register Bits Masks Definitions */
#define PW_ROUTE_PORT                                    (0x00ffffff)

/* MPM_BH : Register Bits Masks Definitions */
#define MPM_BH_BLK_TYPE                                  (0x00000fff)
#define MPM_BH_BLK_REV                                   (0x0000f000)
#define MPM_BH_NEXT_BLK_PTR                              (0xffff0000)

/* RB_RESTRICT : Register Bits Masks Definitions */
#define RB_RESTRICT_I2C_WR                               (0x00000001)
#define RB_RESTRICT_JTAG_WR                              (0x00000002)
#define RB_RESTRICT_SWR                             	 (0x00000004)
#define RB_RESTRICT_I2C_RD                               (0x00010000)
#define RB_RESTRICT_JTAG_RD                              (0x00020000)
#define RB_RESTRICT_SRD                             	 (0x00040000)

/* MTC_WR_RESTRICT : Register Bits Masks Definitions */
#define MTC_WR_RESTRICT_WR_DIS                           (0x00ffffff)

/* MTC_RD_RESTRICT : Register Bits Masks Definitions */
#define MTC_RD_RESTRICT_RD_DIS                           (0x00ffffff)

/* MPM_SCRATCH1 : Register Bits Masks Definitions */
#define MPM_SCRATCH1_SCRATCH                             (0xffffffff)

/* PORT_NUMBER : Register Bits Masks Definitions */
#define PORT_NUMBER_PORT_NUM                             (0x000000ff)
#define PORT_NUMBER_PORT_TOTAL                           (0x0000ff00)

/* PRESCALAR_SRV_CLK : Register Bits Masks Definitions */
#define PRESCALAR_SRV_CLK_PRESCALAR_SRV_CLK              (0x000000ff)

/* REG_RST_CTL : Register Bits Masks Definitions */
#define REG_RST_CTL_CLEAR_STICKY                         (0x00000001)
#define REG_RST_CTL_SOFT_RST_DEV                         (0x00000002)

/* MPM_SCRATCH2 : Register Bits Masks Definitions */
#define MPM_SCRATCH2_SCRATCH                             (0xffffffff)

/* ASBLY_ID_OVERRIDE : Register Bits Masks Definitions */
#define ASBLY_ID_OVERRIDE_ASBLY_VEN_ID                   (0x0000ffff)
#define ASBLY_ID_OVERRIDE_ASBLY_ID                       (0xffff0000)

/* ASBLY_INFO_OVERRIDE : Register Bits Masks Definitions */
#define ASBLY_INFO_OVERRIDE_EXT_FEAT_PTR                 (0x0000ffff)
#define ASBLY_INFO_OVERRIDE_ASBLY_REV                    (0xffff0000)

/* MPM_MTC_RESP_PRIO : Register Bits Masks Definitions */
#define MPM_MTC_RESP_PCRF                    	         (0x00000001)
#define MPM_MTC_RESP_PPRIO                       	 (0x00000006)
#define MPM_MTC_RESP_PPLUS_1                         	 (0x00000010)

/* MPM_MTC_ACTIVE : Register Bits Masks Definitions */
#define MPM_MTC_ACTIVE_MTC_ACTIVE                        (0x00ffffff)

/* MPM_CFGSIG0 : Register Bits Masks Definitions */
#define MPM_CFGSIG0_SWAP_RX                              (0x00000003)
#define MPM_CFGSIG0_RX_POLARITY                          (0x00000004)
#define MPM_CFGSIG0_SWAP_TX                              (0x00000030)
#define MPM_CFGSIG0_TX_POLARITY                          (0x00000040)
#define MPM_CFGSIG0_CORECLK_SELECT                       (0x00000600)
#define MPM_CFGSIG0_REFCLK_SELECT                        (0x00000800)
#define MPM_CFGSIG0_BAUD_SELECT                          (0x0000f000)
#define MPM_CFGSIG0_HW_CMD                               (0x01000000)
#define MPM_CFGSIG0_HW_ACK                               (0x02000000)
#define MPM_CFGSIG0_TX_MODE                              (0x10000000)

/* MPM_CFGSIG1 : Register Bits Masks Definitions */
#define MPM_CFGSIG1_PORT_SELECT_STRAP                    (0x00ffffff)

/* FAB_GEN_BH : Register Bits Masks Definitions */
#define FAB_GEN_BH_BLK_TYPE                              (0x00000fff)
#define FAB_GEN_BH_BLK_REV                               (0x0000f000)
#define FAB_GEN_BH_NEXT_BLK_PTR                          (0xffff0000)

/* FAB_GLOBAL_MEM_PWR_MODE : Register Bits Masks Definitions */
#define FAB_GLOBAL_MEM_PWR_MODE_PWR_MODE                 (0x00000003)

/* FAB_GLOBAL_CLK_GATE : Register Bits Masks Definitions */
#define FAB_GLOBAL_CLK_GATE_GLBL_PORT_CGATE              (0x00ffffff)

/* FAB_4X_MODE : Register Bits Masks Definitions */
#define FAB_4X_MODE_FAB_4X_MODE                          (0x00000fff)

/* FAB_MBCOL_ACT : Register Bits Masks Definitions */
#define FAB_MBCOL_ACT_ACTIVE                             (0x00ffffff)

/* FAB_MIG_ACT : Register Bits Masks Definitions */
#define FAB_MIG_ACT_ACTIVE                               (0x00ffffff)

/* PHY_SERIAL_IF_EN : Register Bits Masks Definitions */
#define PHY_SERIAL_IF_EN_SERIAL_PORT_OK_EN               (0x20000000)
#define PHY_SERIAL_IF_EN_SERIAL_LOOPBACK_EN              (0x40000000)
#define PHY_SERIAL_IF_EN_SERIAL_TX_DISABLE_EN            (0x80000000)

/* PHY_TX_DISABLE_CTRL : Register Bits Masks Definitions */
#define PHY_TX_DISABLE_CTRL_PHY_TX_DISABLE               (0x00ffffff)
#define PHY_TX_DISABLE_CTRL_DONE                         (0x20000000)
#define PHY_TX_DISABLE_CTRL_POLARITY                     (0x40000000)
#define PHY_TX_DISABLE_CTRL_START                        (0x80000000)

/* PHY_LOOPBACK_CTRL : Register Bits Masks Definitions */
#define PHY_LOOPBACK_CTRL_PHY_LOOPBACK                   (0x00ffffff)
#define PHY_LOOPBACK_CTRL_DONE                           (0x20000000)
#define PHY_LOOPBACK_CTRL_POLARITY                       (0x40000000)
#define PHY_LOOPBACK_CTRL_START                          (0x80000000)

/* PHY_PORT_OK_CTRL : Register Bits Masks Definitions */
#define PHY_PORT_OK_CTRL_CFG_TMR                         (0x00ffffff)
#define PHY_PORT_OK_CTRL_POLARITY                        (0x80000000)

/* MCM_ROUTE_EN : Register Bits Masks Definitions */
#define MCM_ROUTE_EN_RT_EN                               (0x00ffffff)

/* BOOT_CTL : Register Bits Masks Definitions */
#define BOOT_CTL_BOOT_FAIL                               (0x20000000)
#define BOOT_CTL_BOOT_OK                                 (0x40000000)
#define BOOT_CTL_BOOT_CMPLT                              (0x80000000)

/* FAB_GLOBAL_PWR_GATE_CLR : Register Bits Masks Definitions */
#define FAB_GLOBAL_PWR_GATE_CLR_PGATE_CLR                (0x80000000)

/* FAB_GLOBAL_PWR_GATE : Register Bits Masks Definitions */
#define FAB_GLOBAL_PWR_GATE_CB_COL_PGATE                 (0x00000fff)

/* SYS_PLL_CFG_1 : Register Bits Masks Definitions */
#define SYS_PLL_CFG_1_TEST_DRV                           (0x00000001)
#define SYS_PLL_CFG_1_TEST_OUT                           (0x00000002)
#define SYS_PLL_CFG_1_TEST_REP                           (0x00000004)
#define SYS_PLL_CFG_1_TEST_SEL                           (0x00000008)
#define SYS_PLL_CFG_1_SYS_PLL_CLKOD                      (0x000000f0)
#define SYS_PLL_CFG_1_BG_PWRDN                           (0x00000300)
#define SYS_PLL_CFG_1_SYS_PLL_CLKR                       (0x0000fc00)
#define SYS_PLL_CFG_1_EXT_SEL                            (0x00030000)
#define SYS_PLL_CFG_1_SYS_PLL_CLKF                       (0x7ffc0000)

/* SYS_PLL_CFG_2 : Register Bits Masks Definitions */
#define SYS_PLL_CFG_2_SYS_PLL_PG_PWRDWN_TMR              (0x0000ffff)
#define SYS_PLL_CFG_2_SYS_PLL_ENSAT                      (0x00040000)
#define SYS_PLL_CFG_2_SYS_PLL_FASTEN                     (0x00080000)
#define SYS_PLL_CFG_2_SYS_PLL_BWADJ                      (0xfff00000)

/* SYS_PLL_CFG_3 : Register Bits Masks Definitions */
#define SYS_PLL_CFG_3_SYS_PLL_LOCK_TMR                   (0x0000ffff)
#define SYS_PLL_CFG_3_SYS_PLL_BG_RST_TMR                 (0xffff0000)

/* SYS_PLL_uint32_t : Register Bits Masks Definitions */
#define SYS_PLL_uint32_t_uint32_t_2                          (0x20000000)
#define SYS_PLL_uint32_t_uint32_t_1                          (0x40000000)
#define SYS_PLL_uint32_t_uint32_t_0                          (0x80000000)

/* DEL_BH : Register Bits Masks Definitions */
#define DEL_BH_BLK_TYPE                                  (0x00000fff)
#define DEL_BH_BLK_REV                                   (0x0000f000)
#define DEL_BH_NEXT_BLK_PTR                              (0xffff0000)

/* DEL_ACC : Register Bits Masks Definitions */
#define DEL_ACC_EL_ON                                    (0x80000000)

/* DEL_INFO : Register Bits Masks Definitions */
#define DEL_INFO_MAX_STAT_INDEX                          (0x0000000f)
#define DEL_INFO_MEM_SIZE                                (0x00000700)
#define DEL_INFO_VERSION                                 (0xff000000)

/* DEL_CTL : Register Bits Masks Definitions */
#define DEL_CTL_T0_EN                                    (0x00000001)
#define DEL_CTL_T0_INTA                                  (0x00000002)
#define DEL_CTL_T0_INTB                                  (0x00000004)
#define DEL_CTL_T1_EN                                    (0x00000010)
#define DEL_CTL_T1_INTA                                  (0x00000020)
#define DEL_CTL_T1_INTB                                  (0x00000040)
#define DEL_CTL_TR_COND                                  (0x00000700)
#define DEL_CTL_TR_INTA                                  (0x00001000)
#define DEL_CTL_TR_INTB                                  (0x00002000)
#define DEL_CTL_RST                                      (0x00008000)
#define DEL_CTL_TRIG_POS                                 (0x00ff0000)

/* DEL_INT_EN : Register Bits Masks Definitions */
#define DEL_INT_EN_EN_A                                  (0x00000001)
#define DEL_INT_EN_EN_B                                  (0x00000002)

/* DEL_SRC_LOG_EN : Register Bits Masks Definitions */
#define DEL_SRC_LOG_EN_EN_0                              (0x00000001)
#define DEL_SRC_LOG_EN_EN_1                              (0x00000002)
#define DEL_SRC_LOG_EN_EN_2                              (0x00000004)
#define DEL_SRC_LOG_EN_EN_3                              (0x00000008)
#define DEL_SRC_LOG_EN_EN_4                              (0x00000010)
#define DEL_SRC_LOG_EN_EN_5                              (0x00000020)
#define DEL_SRC_LOG_EN_EN_6                              (0x00000040)
#define DEL_SRC_LOG_EN_EN_7                              (0x00000080)
#define DEL_SRC_LOG_EN_CLR                               (0x80000000)

/* DEL_TRIG0_MASK : Register Bits Masks Definitions */
#define DEL_TRIG0_MASK_MASK                              (0x07ffffff)
#define DEL_TRIG0_MASK_SAME_TIME_MASK                    (0x08000000)
#define DEL_TRIG0_MASK_SRC_MASK                          (0xf0000000)

/* DEL_TRIG0_VAL : Register Bits Masks Definitions */
#define DEL_TRIG0_VAL_EVENTS                             (0x07ffffff)
#define DEL_TRIG0_VAL_SAME_TIME                          (0x08000000)
#define DEL_TRIG0_VAL_SRC_VAL                            (0xf0000000)

/* DEL_TRIG1_MASK : Register Bits Masks Definitions */
#define DEL_TRIG1_MASK_MASK                              (0x07ffffff)
#define DEL_TRIG1_MASK_SAME_TIME_MASK                    (0x08000000)
#define DEL_TRIG1_MASK_SRC_MASK                          (0xf0000000)

/* DEL_TRIG1_VAL : Register Bits Masks Definitions */
#define DEL_TRIG1_VAL_EVENTS                             (0x07ffffff)
#define DEL_TRIG1_VAL_SAME_TIME                          (0x08000000)
#define DEL_TRIG1_VAL_SRC_VAL                            (0xf0000000)

/* DEL_TRIG_STAT : Register Bits Masks Definitions */
#define DEL_TRIG_STAT_TRIG_0                             (0x00000001)
#define DEL_TRIG_STAT_TRIG_1                             (0x00000002)
#define DEL_TRIG_STAT_TRIG                               (0x00000004)
#define DEL_TRIG_STAT_FULL                               (0x00000008)
#define DEL_TRIG_STAT_INT_A                              (0x00000010)
#define DEL_TRIG_STAT_INT_B                              (0x00000020)
#define DEL_TRIG_STAT_ALL_VALID                          (0x00000080)

/* DEL_WR_TRIG_IDX : Register Bits Masks Definitions */
#define DEL_WR_TRIG_IDX_WR_IDX                           (0x000000ff)
#define DEL_WR_TRIG_IDX_TRIG_IDX                         (0x00ff0000)

/* DEL_RD_IDX : Register Bits Masks Definitions */
#define DEL_RD_IDX_RD_IDX                                (0x000000ff)

/* DEL_DATA : Register Bits Masks Definitions */
#define DEL_DATA_EVENTS                                  (0x07ffffff)
#define DEL_DATA_SAME_TIME                               (0x08000000)
#define DEL_DATA_SOURCE                                  (0xf0000000)

/* DEL_SRC_STAT0 : Register Bits Masks Definitions */
#define DEL_SRC_STAT0_INT_ASSERT                         (0x00ffffff)
#define DEL_SRC_STAT0_SOFTWARE                           (0x07000000)

/* DEL_SRC_STAT1 : Register Bits Masks Definitions */
#define DEL_SRC_STAT1_INT_DASSERT                        (0x00ffffff)
#define DEL_SRC_STAT1_SOFTWARE                           (0x07000000)

/* DEL_SRC_STAT2 : Register Bits Masks Definitions */
#define DEL_SRC_STAT2_PW_ASSERT                          (0x00ffffff)
#define DEL_SRC_STAT2_SOFTWARE                           (0x07000000)

/* DEL_SRC_STAT3 : Register Bits Masks Definitions */
#define DEL_SRC_STAT3_PW_DASSERT                         (0x00ffffff)
#define DEL_SRC_STAT3_SOFTWARE                           (0x07000000)

/* DEL_SRC_STAT4 : Register Bits Masks Definitions */
#define DEL_SRC_STAT4_RESET                              (0x00ffffff)
#define DEL_SRC_STAT4_SOFTWARE                           (0x07000000)

/* DEL_SRC_STATX : Register Bits Masks Definitions */
#define DEL_SRC_STATX_SOFTWARE                           (0x07ffffff)

/* FAB_CBCOL_BH : Register Bits Masks Definitions */
#define FAB_CBCOL_BH_BLK_TYPE                            (0x00000fff)
#define FAB_CBCOL_BH_BLK_REV                             (0x0000f000)
#define FAB_CBCOL_BH_NEXT_BLK_PTR                        (0xffff0000)

/* FAB_CBCOL_X_CTL : Register Bits Masks Definitions */
#define FAB_CBCOL_X_CTL_OS_SAT                           (0x00000100)
#define FAB_CBCOL_X_CTL_OS_CRED                          (0x01ff0000)
#define FAB_CBCOL_X_CTL_OS_ARB_MODE                      (0x10000000)

/* FAB_CBCOL_X_SAT_CTL : Register Bits Masks Definitions */
#define FAB_CBCOL_X_SAT_CTL_MIN_SAT                      (0x0000ffff)
#define FAB_CBCOL_X_SAT_CTL_MAX_SAT                      (0xffff0000)

/* FAB_CBCOL_X_OS_INP_EN : Register Bits Masks Definitions */
#define FAB_CBCOL_X_OS_INP_EN_INP_EN                     (0x00ffffff)
#define FAB_CBCOL_X_OS_INP_EN_DPG_EN                     (0x01000000)

/* FAB_CBCOL_X_ACT : Register Bits Masks Definitions */
#define FAB_CBCOL_X_ACT_ACTIVE                           (0x00ffffff)

/* FAB_CBCOL_X_ACT_SUMM : Register Bits Masks Definitions */
#define FAB_CBCOL_X_ACT_SUMM_MB_ACT                      (0x00000001)
#define FAB_CBCOL_X_ACT_SUMM_COL_ACT                     (0x00000002)

/* FAB_CBCOL_X_PG_WM : Register Bits Masks Definitions */
#define FAB_CBCOL_X_PG_WM_PBME_PRIO1_RSVD_PAGES          (0x0000ff00)
#define FAB_CBCOL_X_PG_WM_PBME_PRIO2_RSVD_PAGES          (0x00ff0000)
#define FAB_CBCOL_X_PG_WM_PBME_PRIO3_RSVD_PAGES          (0xff000000)

/* FAB_CBCOL_X_ND_WM_01 : Register Bits Masks Definitions */
#define FAB_CBCOL_X_ND_WM_01_PBME_PRIO1_RSVD_NODES       (0x03ff0000)

/* FAB_CBCOL_X_ND_WM_23 : Register Bits Masks Definitions */
#define FAB_CBCOL_X_ND_WM_23_PBME_PRIO2_RSVD_NODES       (0x000003ff)
#define FAB_CBCOL_X_ND_WM_23_PBME_PRIO3_RSVD_NODES       (0x03ff0000)

/* FAB_CBCOL_X_CLK_GATE : Register Bits Masks Definitions */
#define FAB_CBCOL_X_CLK_GATE_COL_PORT_CGATE              (0x00ffffff)

/* FAB_CBCOL_X_SCRATCH : Register Bits Masks Definitions */
#define FAB_CBCOL_X_SCRATCH_SCRATCH                      (0xffffffff)

/* PKT_GEN_BH : Register Bits Masks Definitions */
#define PKT_GEN_BH_BLK_TYPE                              (0x00000fff)
#define PKT_GEN_BH_BLK_REV                               (0x0000f000)
#define PKT_GEN_BH_NEXT_BLK_PTR                          (0xffff0000)

/* FAB_PGEN_X_ACC : Register Bits Masks Definitions */
#define FAB_PGEN_X_ACC_REG_ACC                           (0x80000000)

/* FAB_PGEN_X_STAT : Register Bits Masks Definitions */
#define FAB_PGEN_X_STAT_EVNT                             (0x80000000)

/* FAB_PGEN_X_INT_EN : Register Bits Masks Definitions */
#define FAB_PGEN_X_INT_EN_INT_EN                         (0x80000000)

/* FAB_PGEN_X_PW_EN : Register Bits Masks Definitions */
#define FAB_PGEN_X_PW_EN_PW_EN                           (0x80000000)

/* FAB_PGEN_X_GEN : Register Bits Masks Definitions */
#define FAB_PGEN_X_GEN_GEN                               (0x80000000)

/* FAB_PGEN_X_CTL : Register Bits Masks Definitions */
#define FAB_PGEN_X_CTL_REPEAT                            (0x0000ffff)
#define FAB_PGEN_X_CTL_PACE                              (0x0fff0000)
#define FAB_PGEN_X_CTL_DONE                              (0x20000000)
#define FAB_PGEN_X_CTL_START                             (0x40000000)
#define FAB_PGEN_X_CTL_PG_INIT                           (0x80000000)

/* FAB_PGEN_X_DATA_CTL : Register Bits Masks Definitions */
#define FAB_PGEN_X_DATA_CTL_ADDR                         (0x000001f0)
#define FAB_PGEN_X_DATA_CTL_PKT_TTL                      (0x00070000)
#define FAB_PGEN_X_DATA_CTL_PORT                         (0x00100000)
#define FAB_PGEN_X_DATA_CTL_LEN                          (0x07000000)
#define FAB_PGEN_X_DATA_CTL_PRIO                         (0x70000000)

/* FAB_PGEN_X_DATA_Y : Register Bits Masks Definitions */
#define FAB_PGEN_X_DATA_Y_DATA                           (0xffffffff)

/* SPX_L0_G0_ENTRYY_CSR : Register Bits Masks Definitions */
#define SPX_L0_G0_ENTRYY_CSR_ROUTING_VALUE               (0x000003ff)
#define SPX_L0_G0_ENTRYY_CSR_CAPTURE                     (0x80000000)

/* SPX_L1_GY_ENTRYZ_CSR : Register Bits Masks Definitions */
#define SPX_L1_GY_ENTRYZ_CSR_ROUTING_VALUE               (0x000003ff)
#define SPX_L1_GY_ENTRYZ_CSR_CAPTURE                     (0x80000000)

/* SPX_L2_GY_ENTRYZ_CSR : Register Bits Masks Definitions */
#define SPX_L2_GY_ENTRYZ_CSR_ROUTING_VALUE               (0x000003ff)
#define SPX_L2_GY_ENTRYZ_CSR_CAPTURE                     (0x80000000)

/* SPX_MC_Y_S_CSR : Register Bits Masks Definitions */
#define SPX_MC_Y_S_CSR_SET                               (0x00ffffff)

/* SPX_MC_Y_C_CSR : Register Bits Masks Definitions */
#define SPX_MC_Y_C_CSR_CLR                               (0x00ffffff)


/*****************************************************/
/* SerDes Register Address Offset Definitions        */
/*****************************************************/

#define SERDES_BH                               (0x00098000)
#define AEC_X_STAT_CSR(X)                    	(0x98004 + 0x3900*(X))
#define AEC_X_INT_EN_CSR(X)                  	(0x98008 + 0x3900*(X))
#define AEC_X_EVENT_GEN_CSR(X)               	(0x9800c + 0x3900*(X))
#define AEC_X_PHY_IND_WR_CSR(X)              	(0x98010 + 0x3900*(X))
#define AEC_X_PHY_IND_RD_CSR(X)              	(0x98014 + 0x3900*(X))
#define AEC_X_SCRATCH(X)                     	(0x98018 + 0x3900*(X))
#define AEC_X_PHYMEM_CTRL_CSR(X)             	(0x98020 + 0x3900*(X))
#define AEC_X_RESET_CTRL_CSR(X)              	(0x98024 + 0x3900*(X))
#define AEC_X_PHY_PLL_CTL_CSR(X)             	(0x98028 + 0x3900*(X))
#define AEC_X_1P25G_XN_100M_VCO_VAL(X)       	(0x9802c + 0x3900*(X))
#define AEC_X_3P12G_XN_100M_VCO_VAL(X)       	(0x98030 + 0x3900*(X))
#define AEC_X_10P3G_100M_VCO_VAL(X)          	(0x98034 + 0x3900*(X))
#define AEC_X_8G_100M_VCO_VAL(X)             	(0x98038 + 0x3900*(X))
#define AEC_X_14G_100M_VCO_VAL(X)            	(0x9803c + 0x3900*(X))
#define AEC_X_15G_100M_VCO_VAL(X)            	(0x98040 + 0x3900*(X))
#define AEC_X_1P25G_XN_156M_VCO_VAL(X)       	(0x98044 + 0x3900*(X))
#define AEC_X_3P12G_XN_156M_VCO_VAL(X)       	(0x98048 + 0x3900*(X))
#define AEC_X_10P31G_156M_VCO_VAL(X)         	(0x9804c + 0x3900*(X))
#define AEC_X_7P96G_156M_VCO_VAL(X)         	(0x98050 + 0x3900*(X))
#define AEC_X_14G_156M_VCO_VAL(X)            	(0x98054 + 0x3900*(X))
#define AEC_X_15G_156M_VCO_VAL(X)            	(0x98058 + 0x3900*(X))
#define AEC_X_MPLL_BW_CTL_1(X)              	(0x9805c + 0x3900*(X))
#define AEC_X_MPLL_BW_CTL_2(X)              	(0x98060 + 0x3900*(X))
#define AEC_X_MPLL_BW_CTL_3(X)              	(0x98064 + 0x3900*(X))
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT1_CSR(X,Y)	(0x98100 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_CM1_LIMIT1_CSR(X,Y)  	(0x98104 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_C0_LIMIT1_CSR(X,Y)  	(0x98108 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_CP1_LIMIT1_CSR(X,Y)  	(0x9810c + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT2_CSR(X,Y)  (0x98110 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_CM1_LIMIT2_CSR(X,Y)  	(0x98114 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_C0_LIMIT2_CSR(X,Y)  	(0x98118 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_CP1_LIMIT2_CSR(X,Y)  	(0x9811c + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_CM1_VALUE_CSR(X,Y)  	(0x98120 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_C0_VALUE_CSR(X,Y)  	(0x98124 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TAP_CP1_VALUE_CSR(X,Y)  	(0x98128 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_EMPH_CTL1_CSR(X,Y)  	(0x9812c + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_EMPH_CTL2_CSR(X,Y)  	(0x98130 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_EMPH_CTL3_CSR(X,Y)  	(0x98134 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_RXADAPT_uint32_t_CSR(X,Y)  	(0x9813c + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_STAT_CSR(X,Y)  		(0x98140 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_INT_EN_CSR(X,Y)  		(0x98144 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_EVENT_GEN_CSR(X,Y)  	(0x98148 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR(X,Y)  	(0x98150 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR(X,Y)  	(0x98154 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_RX_LOS_TMR_CSR(X,Y)  	(0x98158 + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR(X,Y)  	(0x9815c + 0x3900*(X) + 0x100*(Y))
#define AEC_X_LANE_Y_TXFIFO_BYPASS_CSR(X,Y)  	(0x9899c + 0x3900*(X) + 0x400*(Y))

/***************************************************************/
/* SerDes Register Bit Masks and Reset Values Definitions      */
/***************************************************************/

/* SERDES_BH : Register Bits Masks Definitions */
#define SERDES_BH_BLK_TYPE                               (0x00000fff)
#define SERDES_BH_BLK_REV                                (0x0000f000)
#define SERDES_BH_NEXT_BLK_PTR                           (0xffff0000)

/* AEC_X_STAT_CSR : Register Bits Masks Definitions */
#define AEC_X_STAT_CSR_PHY_MEM_UNCOR                     (0x00400000)
#define AEC_X_STAT_CSR_PHY_MEM_COR                       (0x00800000)
#define AEC_X_STAT_CSR_UNUSED                            (0x60000000)
#define AEC_X_STAT_CSR_TXEQ_PROT_ERR                     (0x80000000)

/* AEC_X_INT_EN_CSR : Register Bits Masks Definitions */
#define AEC_X_INT_EN_CSR_PHY_MEM_UNCOR                   (0x00400000)
#define AEC_X_INT_EN_CSR_PHY_MEM_COR                     (0x00800000)
#define AEC_X_INT_EN_CSR_UNUSED                          (0x60000000)
#define AEC_X_INT_EN_CSR_TXEQ_PROT_ERR                   (0x80000000)

/* AEC_X_EVENT_GEN_CSR : Register Bits Masks Definitions */
#define AEC_X_EVENT_GEN_CSR_PHY_MEM_UNCOR                (0x00400000)
#define AEC_X_EVENT_GEN_CSR_PHY_MEM_COR                  (0x00800000)

/* AEC_X_PHY_IND_WR_CSR : Register Bits Masks Definitions */
#define AEC_X_PHY_IND_WR_CSR_WR_DATA                     (0x0000ffff)
#define AEC_X_PHY_IND_WR_CSR_WR_ADDR                     (0xffff0000)

/* AEC_X_PHY_IND_RD_CSR : Register Bits Masks Definitions */
#define AEC_X_PHY_IND_RD_CSR_RD_DATA                     (0x0000ffff)
#define AEC_X_PHY_IND_RD_CSR_RD_ADDR                     (0xffff0000)

/* AEC_X_SCRATCH : Register Bits Masks Definitions */
#define AEC_X_SCRATCH_SCRATCH                            (0xffffffff)

/* AEC_X_PHYMEM_CTRL_CSR : Register Bits Masks Definitions */
#define AEC_X_PHYMEM_CTRL_CSR_BOOT_MEM_CGATE             (0x00800000)

/* AEC_X_RESET_CTRL_CSR : Register Bits Masks Definitions */
#define AEC_X_RESET_CTRL_CSR_UNUSED                      (0x00000080)
#define AEC_X_RESET_CTRL_CSR_AEC_CFG21                   (0x00008000)
#define AEC_X_RESET_CTRL_CSR_EVENT_LOG_LANE_SEL          (0x00c00000)
#define AEC_X_RESET_CTRL_CSR_QUAD_DEBUG_0                (0x20000000)
#define AEC_X_RESET_CTRL_CSR_PHY_REG_UPD                 (0x40000000)
#define AEC_X_RESET_CTRL_CSR_PHY_PLL_CFG_UPD             (0x80000000)

/* AEC_X_PHY_PLL_CTL_CSR : Register Bits Masks Definitions */
#define AEC_X_PHY_PLL_CTL_CSR_TX_VBOOST_LVL              (0x00000007)
#define AEC_X_PHY_PLL_CTL_CSR_AEC_CFG2                   (0x40000000)
#define AEC_X_PHY_PLL_CTL_CSR_AEC_CFG1                   (0x80000000)

/* AEC_X_1P25G_XN_100M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_1P25G_XN_100M_VCO_VAL_RXX_REF_LD_VAL       (0x0000003f)
#define AEC_X_1P25G_XN_100M_VCO_VAL_RXX_VCO_LD_VAL       (0x1fff0000)

/* AEC_X_3P12G_XN_100M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_3P12G_XN_100M_VCO_VAL_RXX_REF_LD_VAL       (0x0000003f)
#define AEC_X_3P12G_XN_100M_VCO_VAL_RXX_VCO_LD_VAL       (0x1fff0000)

/* AEC_X_10P3G_100M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_10P3G_100M_VCO_VAL_RXX_REF_LD_VAL          (0x0000003f)
#define AEC_X_10P3G_100M_VCO_VAL_RXX_VCO_LD_VAL          (0x1fff0000)

/* AEC_X_8G_100M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_8G_100M_VCO_VAL_RXX_REF_LD_VAL             (0x0000003f)
#define AEC_X_8G_100M_VCO_VAL_RXX_VCO_LD_VAL             (0x1fff0000)

/* AEC_X_14G_100M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_14G_100M_VCO_VAL_RXX_REF_LD_VAL            (0x0000003f)
#define AEC_X_14G_100M_VCO_VAL_RXX_VCO_LD_VAL            (0x1fff0000)

/* AEC_X_15G_100M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_15G_100M_VCO_VAL_RXX_REF_LD_VAL            (0x0000003f)
#define AEC_X_15G_100M_VCO_VAL_RXX_VCO_LD_VAL            (0x1fff0000)

/* AEC_X_1P25G_XN_156M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_1P25G_XN_156M_VCO_VAL_RXX_REF_LD_VAL       (0x0000003f)
#define AEC_X_1P25G_XN_156M_VCO_VAL_RXX_VCO_LD_VAL       (0x1fff0000)

/* AEC_X_3P12G_XN_156M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_3P12G_XN_156M_VCO_VAL_RXX_REF_LD_VAL       (0x0000003f)
#define AEC_X_3P12G_XN_156M_VCO_VAL_RXX_VCO_LD_VAL       (0x1fff0000)

/* AEC_X_10P31G_156M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_10P31G_156M_VCO_VAL_RXX_REF_LD_VAL         (0x0000003f)
#define AEC_X_10P31G_156M_VCO_VAL_RXX_VCO_LD_VAL         (0x1fff0000)

/* AEC_X_7P96G_156M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_7P96G_156M_VCO_VAL_RXX_REF_LD_VAL          (0x0000003f)
#define AEC_X_7P96G_156M_VCO_VAL_RXX_VCO_LD_VAL          (0x1fff0000)

/* AEC_X_14G_156M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_14G_156M_VCO_VAL_RXX_REF_LD_VAL            (0x0000003f)
#define AEC_X_14G_156M_VCO_VAL_RXX_VCO_LD_VAL            (0x1fff0000)

/* AEC_X_15G_156M_VCO_VAL : Register Bits Masks Definitions */
#define AEC_X_15G_156M_VCO_VAL_RXX_REF_LD_VAL            (0x0000003f)
#define AEC_X_15G_156M_VCO_VAL_RXX_VCO_LD_VAL            (0x1fff0000)

/* AEC_X_MPLL_BW_CTL_1 : Register Bits Masks Definitions */
#define AEC_X_MPLL_BW_CTL_1_MPLL_B_BW_CTL_3P12G_XN_156M  (0x0000007f)
#define AEC_X_MPLL_BW_CTL_1_MPLL_B_BW_CTL_3P12G_XN_100M  (0x00007f00)
#define AEC_X_MPLL_BW_CTL_1_MPLL_A_BW_CTL_1P25G_XN_156M  (0x007f0000)
#define AEC_X_MPLL_BW_CTL_1_MPLL_A_BW_CTL_1P25G_XN_100M  (0x7f000000)

/* AEC_X_MPLL_BW_CTL_2 : Register Bits Masks Definitions */
#define AEC_X_MPLL_BW_CTL_2_MPLL_A_BW_CTL_10P31G_156M    (0x0000007f)
#define AEC_X_MPLL_BW_CTL_2_MPLL_A_BW_CTL_10P30G_100M    (0x00007f00)
#define AEC_X_MPLL_BW_CTL_2_MPLL_B_BW_CTL_7P96G_156M     (0x007f0000)
#define AEC_X_MPLL_BW_CTL_2_MPLL_B_BW_CTL_8G_100M        (0x7f000000)

/* AEC_X_MPLL_BW_CTL_3 : Register Bits Masks Definitions */
#define AEC_X_MPLL_BW_CTL_3_MPLL_B_BW_CTL_15G_156M       (0x0000007f)
#define AEC_X_MPLL_BW_CTL_3_MPLL_B_BW_CTL_15G_100M       (0x00007f00)
#define AEC_X_MPLL_BW_CTL_3_MPLL_B_BW_CTL_14P06G_156M    (0x007f0000)
#define AEC_X_MPLL_BW_CTL_3_MPLL_B_BW_CTL_14G_100M       (0x7f000000)

/* AEC_X_LANE_Y_TAP_SPACE_LIMIT1_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT1_CSR_LOWER_BND1     (0x000000ff)
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT1_CSR_UPPER_BND0     (0x0000ff00)
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT1_CSR_LOWER_BND0     (0x00ff0000)

/* AEC_X_LANE_Y_TAP_CM1_LIMIT1_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_CM1_LIMIT1_CSR_CM1_INITIALIZE   (0x0000003f)
#define AEC_X_LANE_Y_TAP_CM1_LIMIT1_CSR_CM1_PRESET       (0x00003f00)
#define AEC_X_LANE_Y_TAP_CM1_LIMIT1_CSR_CM1_MIN          (0x003f0000)
#define AEC_X_LANE_Y_TAP_CM1_LIMIT1_CSR_CM1_MAX          (0x3f000000)

/* AEC_X_LANE_Y_TAP_C0_LIMIT1_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_C0_LIMIT1_CSR_C0_INITIALIZE     (0x0000003f)
#define AEC_X_LANE_Y_TAP_C0_LIMIT1_CSR_C0_PRESET         (0x00003f00)
#define AEC_X_LANE_Y_TAP_C0_LIMIT1_CSR_C0_MIN            (0x003f0000)
#define AEC_X_LANE_Y_TAP_C0_LIMIT1_CSR_C0_MAX            (0x3f000000)

/* AEC_X_LANE_Y_TAP_CP1_LIMIT1_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_CP1_LIMIT1_CSR_CP1_INITIALIZE   (0x0000003f)
#define AEC_X_LANE_Y_TAP_CP1_LIMIT1_CSR_CP1_PRESET       (0x00003f00)
#define AEC_X_LANE_Y_TAP_CP1_LIMIT1_CSR_CP1_MIN          (0x003f0000)
#define AEC_X_LANE_Y_TAP_CP1_LIMIT1_CSR_CP1_MAX          (0x3f000000)

/* AEC_X_LANE_Y_TAP_SPACE_LIMIT2_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT2_CSR_LOWER_BND1     (0x000000ff)
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT2_CSR_UPPER_BND0     (0x0000ff00)
#define AEC_X_LANE_Y_TAP_SPACE_LIMIT2_CSR_LOWER_BND0     (0x00ff0000)

/* AEC_X_LANE_Y_TAP_CM1_LIMIT2_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_CM1_LIMIT2_CSR_CM1_INITIALIZE   (0x0000003f)
#define AEC_X_LANE_Y_TAP_CM1_LIMIT2_CSR_CM1_PRESET       (0x00003f00)
#define AEC_X_LANE_Y_TAP_CM1_LIMIT2_CSR_CM1_MIN          (0x003f0000)
#define AEC_X_LANE_Y_TAP_CM1_LIMIT2_CSR_CM1_MAX          (0x3f000000)

/* AEC_X_LANE_Y_TAP_C0_LIMIT2_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_C0_LIMIT2_CSR_C0_INITIALIZE     (0x0000003f)
#define AEC_X_LANE_Y_TAP_C0_LIMIT2_CSR_C0_PRESET         (0x00003f00)
#define AEC_X_LANE_Y_TAP_C0_LIMIT2_CSR_C0_MIN            (0x003f0000)
#define AEC_X_LANE_Y_TAP_C0_LIMIT2_CSR_C0_MAX            (0x3f000000)

/* AEC_X_LANE_Y_TAP_CP1_LIMIT2_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_CP1_LIMIT2_CSR_CP1_INITIALIZE   (0x0000003f)
#define AEC_X_LANE_Y_TAP_CP1_LIMIT2_CSR_CP1_PRESET       (0x00003f00)
#define AEC_X_LANE_Y_TAP_CP1_LIMIT2_CSR_CP1_MIN          (0x003f0000)
#define AEC_X_LANE_Y_TAP_CP1_LIMIT2_CSR_CP1_MAX          (0x3f000000)

/* AEC_X_LANE_Y_TAP_CM1_VALUE_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_CM1_VALUE_CSR_CM1_CURRENT       (0x00003f00)
#define AEC_X_LANE_Y_TAP_CM1_VALUE_CSR_USE_MANUAL_TAP    (0x00010000)
#define AEC_X_LANE_Y_TAP_CM1_VALUE_CSR_CM1_MANUAL        (0x3f000000)

/* AEC_X_LANE_Y_TAP_C0_VALUE_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_C0_VALUE_CSR_C0_CURRENT         (0x00003fc0)
#define AEC_X_LANE_Y_TAP_C0_VALUE_CSR_USE_MANUAL_TAP     (0x00010000)
#define AEC_X_LANE_Y_TAP_C0_VALUE_CSR_C0_MANUAL          (0x3fc00000)

/* AEC_X_LANE_Y_TAP_CP1_VALUE_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TAP_CP1_VALUE_CSR_CP1_CURRENT       (0x00003f00)
#define AEC_X_LANE_Y_TAP_CP1_VALUE_CSR_USE_MANUAL_TAP    (0x00010000)
#define AEC_X_LANE_Y_TAP_CP1_VALUE_CSR_CP1_MANUAL        (0x3f000000)

/* AEC_X_LANE_Y_EMPH_CTL1_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_RST_RX_BEFORE_ADAPT   (0x00000001)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_RX_FINE_TUNE_MODE     (0x00000006)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_TXEQ_MODE             (0x00000018)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CVRG_TST_LIMIT        (0x000003e0)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CVRG_TST              (0x00000400)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CVRG_TST_EN           (0x00001000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_TX_TWO_STEP_MODE_EN   (0x00002000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_RX_LAST_ADAPT_EN      (0x00004000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_TXEQ_1ST_CMD_TIMOUT_EN  (0x00008000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_LOS_INFERENCE         (0x00030000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_MODIFY_EMPHASIS       (0x000c0000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CP1_INC_STEP          (0x00300000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CP1_DEC_STEP          (0x00c00000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_C0_INC_STEP           (0x03000000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_C0_DEC_STEP           (0x0c000000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CM1_INC_STEP          (0x30000000)
#define AEC_X_LANE_Y_EMPH_CTL1_CSR_CM1_DEC_STEP          (0xc0000000)

/* AEC_X_LANE_Y_EMPH_CTL2_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_EMPH_CTL2_CSR_IDLE1_2_x1_TRAIN_CMPLT_TMR  (0x7fffffff)
#define AEC_X_LANE_Y_EMPH_CTL2_CSR_TMR_MODE              (0x80000000)

/* AEC_X_LANE_Y_EMPH_CTL3_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_CVRG_WIN              (0x000000ff)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_INVRT_CMD             (0x00000100)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_LOCAL_TXEQ_INIT       (0x00000600)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_AEC_CFG4              (0x0007f800)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_RX_ADAPT_DFE_EN       (0x00180000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_REMOTE_TXEQ_INIT_CMD  (0x00600000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_RX_ADAPT_AFE_EN       (0x01800000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_RX_OFFCAN_CONT        (0x02000000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_RX_CONT_ADAPT         (0x04000000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_AEC_CFG3              (0x08000000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_TXEQ_VBOOST           (0x30000000)
#define AEC_X_LANE_Y_EMPH_CTL3_CSR_TX_VBOOST_EN          (0xc0000000)

/* AEC_X_LANE_Y_RXADAPT_uint32_t_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_RXADAPT_uint32_t_CSR_FOM_MIN          (0x000000ff)
#define AEC_X_LANE_Y_RXADAPT_uint32_t_CSR_FOM_MAX          (0x0000ff00)
#define AEC_X_LANE_Y_RXADAPT_uint32_t_CSR_DIR_ITERATIONS   (0xffff0000)

/* AEC_X_LANE_Y_STAT_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_STAT_CSR_UNUSED2                    (0x00800000)
#define AEC_X_LANE_Y_STAT_CSR_UNUSED1                    (0x10000000)
#define AEC_X_LANE_Y_STAT_CSR_TXEQ_CMD_TIMEOUT_ERR       (0x20000000)
#define AEC_X_LANE_Y_STAT_CSR_TXEQ_CMD_SEQ_ERR           (0x40000000)
#define AEC_X_LANE_Y_STAT_CSR_TXEQ_MCMD_ERR              (0x80000000)

/* AEC_X_LANE_Y_INT_EN_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_INT_EN_CSR_UNUSED2                  (0x00800000)
#define AEC_X_LANE_Y_INT_EN_CSR_UNUSED1                  (0x10000000)
#define AEC_X_LANE_Y_INT_EN_CSR_TXEQ_CMD_TIMEOUT_ERR     (0x20000000)
#define AEC_X_LANE_Y_INT_EN_CSR_TXEQ_CMD_SEQ_ERR         (0x40000000)
#define AEC_X_LANE_Y_INT_EN_CSR_TXEQ_MCMD_ERR            (0x80000000)

/* AEC_X_LANE_Y_EVENT_GEN_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_EVENT_GEN_CSR_UNUSED2               (0x00800000)
#define AEC_X_LANE_Y_EVENT_GEN_CSR_UNUSED1               (0x10000000)
#define AEC_X_LANE_Y_EVENT_GEN_CSR_TXEQ_CMD_TIMEOUT_ERR  (0x20000000)
#define AEC_X_LANE_Y_EVENT_GEN_CSR_TXEQ_CMD_SEQ_ERR      (0x40000000)
#define AEC_X_LANE_Y_EVENT_GEN_CSR_TXEQ_MCMD_ERR         (0x80000000)

/* AEC_X_LANE_Y_TXEQ_DEBUG1_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG12           (0x0000003f)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_PHY_TX2RX_SLPB_EN   (0x00000040)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_PHY_RX2TX_PLPB_EN   (0x00000080)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG11           (0x00003f00)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG10           (0x001f8000)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG9            (0x00200000)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG8            (0x00c00000)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG7            (0x03000000)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG6            (0x0c000000)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_STAT0           (0x70000000)
#define AEC_X_LANE_Y_TXEQ_DEBUG1_CSR_AEC_CFG5            (0x80000000)

/* AEC_X_LANE_Y_TXEQ_DEBUG2_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_ADAPT_FOM           (0x000000ff)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_TXPOST_DIR          (0x00000c00)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_TXMAIN_DIR          (0x00003000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_TXPRE_DIR           (0x0000c000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_AEC_CFG15           (0x00010000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_UNUSED              (0x00400000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_ADPT_ACK            (0x00800000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_EVENT_LOG_PAGE_SEL  (0x03000000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_ENABLE_EVENT_LOG    (0x30000000)
#define AEC_X_LANE_Y_TXEQ_DEBUG2_CSR_REQ_ADPT            (0xc0000000)

/* AEC_X_LANE_Y_RX_LOS_TMR_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_RX_LOS_TMR_CSR_REQ_ADAPT_AFTER_RXRST  (0x00000001)
#define AEC_X_LANE_Y_RX_LOS_TMR_CSR_AEC_CFG20            (0x000000c0)
#define AEC_X_LANE_Y_RX_LOS_TMR_CSR_LOS_TMR_DME          (0x0000ff00)
#define AEC_X_LANE_Y_RX_LOS_TMR_CSR_LOS_TMR_IDLE3        (0x00ff0000)
#define AEC_X_LANE_Y_RX_LOS_TMR_CSR_LOS_TMR_IDLE1_2      (0xff000000)

/* AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_LANE_DEBUG_2    (0x00002000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_LANE_DEBUG_1    (0x00004000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_LANE_DEBUG_0    (0x00008000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_CP1_ADJ_STPS    (0x00070000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_CP1_ADJ_CMD     (0x00080000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_CM1_ADJ_STPS    (0x00700000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_CM1_ADJ_CMD     (0x00800000)
#define AEC_X_LANE_Y_ADAPT_STAT_CTRL_CSR_CVRG_TST_STAT   (0x80000000)

/* AEC_X_LANE_Y_TXFIFO_BYPASS_CSR : Register Bits Masks Definitions */
#define AEC_X_LANE_Y_TXFIFO_BYPASS_CSR_PHY_CFG0          (0x0000ffff)


/*****************************************************/
/* I2C Register Address Offset Definitions           */
/*****************************************************/

#define I2C_DEVID                                            (0x000c9100)
#define I2C_RESET                                            (0x000c9104)
#define I2C_MST_CFG                                          (0x000c9108)
#define I2C_MST_CNTRL                                        (0x000c910c)
#define I2C_MST_RDATA                                        (0x000c9110)
#define I2C_MST_TDATA                                        (0x000c9114)
#define I2C_ACC_STAT                                         (0x000c9118)
#define I2C_INT_STAT                                         (0x000c911c)
#define I2C_INT_ENABLE                                       (0x000c9120)
#define I2C_INT_SET                                          (0x000c9124)
#define I2C_SLV_CFG                                          (0x000c912c)
#define I2C_BOOT_CNTRL                                       (0x000c9140)
#define EXI2C_REG_WADDR                                      (0x000c9200)
#define EXI2C_REG_WDATA                                      (0x000c9204)
#define EXI2C_REG_RADDR                                      (0x000c9210)
#define EXI2C_REG_RDATA                                      (0x000c9214)
#define EXI2C_ACC_STAT                                       (0x000c9220)
#define EXI2C_ACC_CNTRL                                      (0x000c9224)
#define EXI2C_STAT                                           (0x000c9280)
#define EXI2C_STAT_ENABLE                                    (0x000c9284)
#define EXI2C_MBOX_OUT                                       (0x000c9290)
#define EXI2C_MBOX_IN                                        (0x000c9294)
#define I2C_EVENT                                            (0x000c9300) 
#define I2C_SNAP_EVENT                                       (0x000c9304) 
#define I2C_NEW_EVENT                                        (0x000c9308)
#define I2C_EVENT_ENB                                        (0x000c930c)
#define I2C_DIVIDER                                          (0x000c9320)
#define I2C_FILTER_SCL_CFG                                   (0x000c9328)
#define I2C_FILTER_SDA_CFG                                   (0x000c932c)
#define I2C_START_SETUP_HOLD                                 (0x000c9340)
#define I2C_STOP_IDLE                                        (0x000c9344)
#define I2C_SDA_SETUP_HOLD                                   (0x000c9348)
#define I2C_SCL_PERIOD                                       (0x000c934c)
#define I2C_SCL_MIN_PERIOD                                   (0x000c9350)
#define I2C_SCL_ARB_TIMEOUT                                  (0x000c9354)
#define I2C_BYTE_TRAN_TIMEOUT                                (0x000c9358)
#define I2C_BOOT_DIAG_TIMER                                  (0x000c935c)
#define I2C_BOOT_DIAG_PROGRESS                               (0x000c93b8)
#define I2C_BOOT_DIAG_CFG                                    (0x000c93bc)

/************************************************************/
/* I2C Register Bit Masks and Reset Values Definitions      */
/************************************************************/

/* I2C_DEVID : Register Bits Masks Definitions */
#define I2C_DEVID_REV                                        (0x0000000f)

/* I2C_RESET : Register Bits Masks Definitions */
#define I2C_RESET_SRESET                                     (0x80000000)

/* I2C_MST_CFG : Register Bits Masks Definitions */
#define I2C_MST_CFG_DEV_ADDR                                 (0x0000007f)
#define I2C_MST_CFG_PA_SIZE                                  (0x00030000)
#define I2C_MST_CFG_DORDER                                   (0x00800000)

/* I2C_MST_CNTRL : Register Bits Masks Definitions */
#define I2C_MST_CNTRL_PADDR                                  (0x0000ffff)
#define I2C_MST_CNTRL_SIZE                                   (0x07000000)
#define I2C_MST_CNTRL_WRITE                                  (0x40000000)
#define I2C_MST_CNTRL_START                                  (0x80000000)

/* I2C_MST_RDATA : Register Bits Masks Definitions */
#define I2C_MST_RDATA_RBYTE0                                 (0x000000ff)
#define I2C_MST_RDATA_RBYTE1                                 (0x0000ff00)
#define I2C_MST_RDATA_RBYTE2                                 (0x00ff0000)
#define I2C_MST_RDATA_RBYTE3                                 (0xff000000)

/* I2C_MST_TDATA : Register Bits Masks Definitions */
#define I2C_MST_TDATA_TBYTE0                                 (0x000000ff)
#define I2C_MST_TDATA_TBYTE1                                 (0x0000ff00)
#define I2C_MST_TDATA_TBYTE2                                 (0x00ff0000)
#define I2C_MST_TDATA_TBYTE3                                 (0xff000000)

/* I2C_ACC_STAT : Register Bits Masks Definitions */
#define I2C_ACC_STAT_MST_NBYTES                              (0x0000000f)
#define I2C_ACC_STAT_MST_AN                                  (0x00000100)
#define I2C_ACC_STAT_MST_PHASE                               (0x00000e00)
#define I2C_ACC_STAT_MST_ACTIVE                              (0x00008000)
#define I2C_ACC_STAT_SLV_PA                                  (0x00ff0000)
#define I2C_ACC_STAT_SLV_AN                                  (0x01000000)
#define I2C_ACC_STAT_SLV_PHASE                               (0x06000000)
#define I2C_ACC_STAT_SLV_WAIT                                (0x08000000)
#define I2C_ACC_STAT_BUS_ACTIVE                              (0x40000000)
#define I2C_ACC_STAT_SLV_ACTIVE                              (0x80000000)

/* I2C_INT_STAT : Register Bits Masks Definitions */
#define I2C_INT_STAT_MA_OK                                   (0x00000001)
#define I2C_INT_STAT_MA_ATMO                                 (0x00000002)
#define I2C_INT_STAT_MA_NACK                                 (0x00000004)
#define I2C_INT_STAT_MA_TMO                                  (0x00000008)
#define I2C_INT_STAT_MA_COL                                  (0x00000010)
#define I2C_INT_STAT_MA_DIAG                                 (0x00000080)
#define I2C_INT_STAT_SA_OK                                   (0x00000100)
#define I2C_INT_STAT_SA_READ                                 (0x00000200)
#define I2C_INT_STAT_SA_WRITE                                (0x00000400)
#define I2C_INT_STAT_SA_FAIL                                 (0x00000800)
#define I2C_INT_STAT_BL_OK                                   (0x00010000)
#define I2C_INT_STAT_BL_FAIL                                 (0x00020000)
#define I2C_INT_STAT_IMB_FULL                                (0x01000000)
#define I2C_INT_STAT_OMB_EMPTY                               (0x02000000)

/* I2C_INT_ENABLE : Register Bits Masks Definitions */
#define I2C_INT_ENABLE_MA_OK                                 (0x00000001)
#define I2C_INT_ENABLE_MA_ATMO                               (0x00000002)
#define I2C_INT_ENABLE_MA_NACK                               (0x00000004)
#define I2C_INT_ENABLE_MA_TMO                                (0x00000008)
#define I2C_INT_ENABLE_MA_COL                                (0x00000010)
#define I2C_INT_ENABLE_MA_DIAG                               (0x00000080)
#define I2C_INT_ENABLE_SA_OK                                 (0x00000100)
#define I2C_INT_ENABLE_SA_READ                               (0x00000200)
#define I2C_INT_ENABLE_SA_WRITE                              (0x00000400)
#define I2C_INT_ENABLE_SA_FAIL                               (0x00000800)
#define I2C_INT_ENABLE_BL_OK                                 (0x00010000)
#define I2C_INT_ENABLE_BL_FAIL                               (0x00020000)
#define I2C_INT_ENABLE_IMB_FULL                              (0x01000000)
#define I2C_INT_ENABLE_OMB_EMPTY                             (0x02000000)

/* I2C_INT_SET : Register Bits Masks Definitions */
#define I2C_INT_SET_MA_OK                                    (0x00000001)
#define I2C_INT_SET_MA_ATMO                                  (0x00000002)
#define I2C_INT_SET_MA_NACK                                  (0x00000004)
#define I2C_INT_SET_MA_TMO                                   (0x00000008)
#define I2C_INT_SET_MA_COL                                   (0x00000010)
#define I2C_INT_SET_MA_DIAG                                  (0x00000080)
#define I2C_INT_SET_SA_OK                                    (0x00000100)
#define I2C_INT_SET_SA_READ                                  (0x00000200)
#define I2C_INT_SET_SA_WRITE                                 (0x00000400)
#define I2C_INT_SET_SA_FAIL                                  (0x00000800)
#define I2C_INT_SET_BL_OK                                    (0x00010000)
#define I2C_INT_SET_BL_FAIL                                  (0x00020000)
#define I2C_INT_SET_IMB_FULL                                 (0x01000000)
#define I2C_INT_SET_OMB_EMPTY                                (0x02000000)

/* I2C_SLV_CFG : Register Bits Masks Definitions */
#define I2C_SLV_CFG_SLV_ADDR                                 (0x0000007f)
#define I2C_SLV_CFG_SLV_UNLK                                 (0x01000000)
#define I2C_SLV_CFG_SLV_EN                                   (0x10000000)
#define I2C_SLV_CFG_ALRT_EN                                  (0x20000000)
#define I2C_SLV_CFG_WR_EN                                    (0x40000000)
#define I2C_SLV_CFG_RD_EN                                    (0x80000000)

/* I2C_BOOT_CNTRL : Register Bits Masks Definitions */
#define I2C_BOOT_CNTRL_PADDR                                 (0x00001fff)
#define I2C_BOOT_CNTRL_PAGE_MODE                             (0x0000e000)
#define I2C_BOOT_CNTRL_BOOT_ADDR                             (0x007f0000)
#define I2C_BOOT_CNTRL_BUNLK                                 (0x10000000)
#define I2C_BOOT_CNTRL_BINC                                  (0x20000000)
#define I2C_BOOT_CNTRL_PSIZE                                 (0x40000000)
#define I2C_BOOT_CNTRL_CHAIN                                 (0x80000000)

/* EXI2C_REG_WADDR : Register Bits Masks Definitions */
#define EXI2C_REG_WADDR_ADDR                                 (0xfffffffc)

/* EXI2C_REG_WDATA : Register Bits Masks Definitions */
#define EXI2C_REG_WDATA_WDATA                                (0xffffffff)

/* EXI2C_REG_RADDR : Register Bits Masks Definitions */
#define EXI2C_REG_RADDR_ADDR                                 (0xfffffffc)

/* EXI2C_REG_RDATA : Register Bits Masks Definitions */
#define EXI2C_REG_RDATA_RDATA                                (0xffffffff)

/* EXI2C_ACC_STAT : Register Bits Masks Definitions */
#define EXI2C_ACC_STAT_ALERT_FLAG                            (0x00000001)
#define EXI2C_ACC_STAT_IMB_FLAG                              (0x00000004)
#define EXI2C_ACC_STAT_OMB_FLAG                              (0x00000008)
#define EXI2C_ACC_STAT_ACC_OK                                (0x00000080)

/* EXI2C_ACC_CNTRL : Register Bits Masks Definitions */
#define EXI2C_ACC_CNTRL_WINC                                 (0x00000004)
#define EXI2C_ACC_CNTRL_RINC                                 (0x00000008)
#define EXI2C_ACC_CNTRL_WSIZE                                (0x00000030)
#define EXI2C_ACC_CNTRL_RSIZE                                (0x000000c0)

/* EXI2C_STAT : Register Bits Masks Definitions */
#define EXI2C_STAT_PORT                                      (0x00000001)
#define EXI2C_STAT_DEL                                       (0x00100000)
#define EXI2C_STAT_FAB                                       (0x00200000)
#define EXI2C_STAT_LOG                                       (0x00400000)
#define EXI2C_STAT_RCS                                       (0x00800000)
#define EXI2C_STAT_MECS                                      (0x01000000)
#define EXI2C_STAT_I2C                                       (0x02000000)
#define EXI2C_STAT_IMBR                                      (0x04000000)
#define EXI2C_STAT_OMBW                                      (0x08000000)
#define EXI2C_STAT_SW_STAT0                                  (0x10000000)
#define EXI2C_STAT_SW_STAT1                                  (0x20000000)
#define EXI2C_STAT_SW_STAT2                                  (0x40000000)
#define EXI2C_STAT_RESET                                     (0x80000000)

/* EXI2C_STAT_ENABLE : Register Bits Masks Definitions */
#define EXI2C_STAT_ENABLE_PORT                               (0x00000001)
#define EXI2C_STAT_ENABLE_DEL                                (0x00100000)
#define EXI2C_STAT_ENABLE_FAB                                (0x00200000)
#define EXI2C_STAT_ENABLE_LOG                                (0x00400000)
#define EXI2C_STAT_ENABLE_RCS                                (0x00800000)
#define EXI2C_STAT_ENABLE_MECS                               (0x01000000)
#define EXI2C_STAT_ENABLE_I2C                                (0x02000000)
#define EXI2C_STAT_ENABLE_IMBR                               (0x04000000)
#define EXI2C_STAT_ENABLE_OMBW                               (0x08000000)
#define EXI2C_STAT_ENABLE_SW_STAT0                           (0x10000000)
#define EXI2C_STAT_ENABLE_SW_STAT1                           (0x20000000)
#define EXI2C_STAT_ENABLE_SW_STAT2                           (0x40000000)
#define EXI2C_STAT_ENABLE_RESET                              (0x80000000)

/* EXI2C_MBOX_OUT : Register Bits Masks Definitions */
#define EXI2C_MBOX_OUT_DATA                                  (0xffffffff)

/* EXI2C_MBOX_IN : Register Bits Masks Definitions */
#define EXI2C_MBOX_IN_DATA                                   (0xffffffff)

/* I2C_X : Register Bits Masks Definitions */
#define I2C_X_MARBTO                                         (0x00000001)
#define I2C_X_MSCLTO                                         (0x00000002)
#define I2C_X_MBTTO                                          (0x00000004)
#define I2C_X_MTRTO                                          (0x00000008)
#define I2C_X_MCOL                                           (0x00000010)
#define I2C_X_MNACK                                          (0x00000020)
#define I2C_X_BLOK                                           (0x00000100)
#define I2C_X_BLNOD                                          (0x00000200)
#define I2C_X_BLSZ                                           (0x00000400)
#define I2C_X_BLERR                                          (0x00000800)
#define I2C_X_BLTO                                           (0x00001000)
#define I2C_X_MTD                                            (0x00004000)
#define I2C_X_SSCLTO                                         (0x00020000)
#define I2C_X_SBTTO                                          (0x00040000)
#define I2C_X_STRTO                                          (0x00080000)
#define I2C_X_SCOL                                           (0x00100000)
#define I2C_X_OMBR                                           (0x00400000)
#define I2C_X_IMBW                                           (0x00800000)
#define I2C_X_DCMDD                                          (0x01000000)
#define I2C_X_DHIST                                          (0x02000000)
#define I2C_X_DTIMER                                         (0x04000000)
#define I2C_X_SD                                             (0x10000000)
#define I2C_X_SDR                                            (0x20000000)
#define I2C_X_SDW                                            (0x40000000)

/* I2C_NEW_EVENT : Register Bits Masks Definitions */
#define I2C_NEW_EVENT_MARBTO                                 (0x00000001)
#define I2C_NEW_EVENT_MSCLTO                                 (0x00000002)
#define I2C_NEW_EVENT_MBTTO                                  (0x00000004)
#define I2C_NEW_EVENT_MTRTO                                  (0x00000008)
#define I2C_NEW_EVENT_MCOL                                   (0x00000010)
#define I2C_NEW_EVENT_MNACK                                  (0x00000020)
#define I2C_NEW_EVENT_BLOK                                   (0x00000100)
#define I2C_NEW_EVENT_BLNOD                                  (0x00000200)
#define I2C_NEW_EVENT_BLSZ                                   (0x00000400)
#define I2C_NEW_EVENT_BLERR                                  (0x00000800)
#define I2C_NEW_EVENT_BLTO                                   (0x00001000)
#define I2C_NEW_EVENT_MTD                                    (0x00004000)
#define I2C_NEW_EVENT_SSCLTO                                 (0x00020000)
#define I2C_NEW_EVENT_SBTTO                                  (0x00040000)
#define I2C_NEW_EVENT_STRTO                                  (0x00080000)
#define I2C_NEW_EVENT_SCOL                                   (0x00100000)
#define I2C_NEW_EVENT_OMBR                                   (0x00400000)
#define I2C_NEW_EVENT_IMBW                                   (0x00800000)
#define I2C_NEW_EVENT_DCMDD                                  (0x01000000)
#define I2C_NEW_EVENT_DHIST                                  (0x02000000)
#define I2C_NEW_EVENT_DTIMER                                 (0x04000000)
#define I2C_NEW_EVENT_SD                                     (0x10000000)
#define I2C_NEW_EVENT_SDR                                    (0x20000000)
#define I2C_NEW_EVENT_SDW                                    (0x40000000)

/* I2C_EVENT_ENB : Register Bits Masks Definitions */
#define I2C_EVENT_ENB_MARBTO                                 (0x00000001)
#define I2C_EVENT_ENB_MSCLTO                                 (0x00000002)
#define I2C_EVENT_ENB_MBTTO                                  (0x00000004)
#define I2C_EVENT_ENB_MTRTO                                  (0x00000008)
#define I2C_EVENT_ENB_MCOL                                   (0x00000010)
#define I2C_EVENT_ENB_MNACK                                  (0x00000020)
#define I2C_EVENT_ENB_BLOK                                   (0x00000100)
#define I2C_EVENT_ENB_BLNOD                                  (0x00000200)
#define I2C_EVENT_ENB_BLSZ                                   (0x00000400)
#define I2C_EVENT_ENB_BLERR                                  (0x00000800)
#define I2C_EVENT_ENB_BLTO                                   (0x00001000)
#define I2C_EVENT_ENB_MTD                                    (0x00004000)
#define I2C_EVENT_ENB_SSCLTO                                 (0x00020000)
#define I2C_EVENT_ENB_SBTTO                                  (0x00040000)
#define I2C_EVENT_ENB_STRTO                                  (0x00080000)
#define I2C_EVENT_ENB_SCOL                                   (0x00100000)
#define I2C_EVENT_ENB_OMBR                                   (0x00400000)
#define I2C_EVENT_ENB_IMBW                                   (0x00800000)
#define I2C_EVENT_ENB_DCMDD                                  (0x01000000)
#define I2C_EVENT_ENB_DHIST                                  (0x02000000)
#define I2C_EVENT_ENB_DTIMER                                 (0x04000000)
#define I2C_EVENT_ENB_SD                                     (0x10000000)
#define I2C_EVENT_ENB_SDR                                    (0x20000000)
#define I2C_EVENT_ENB_SDW                                    (0x40000000)

/* I2C_DIVIDER : Register Bits Masks Definitions */
#define I2C_DIVIDER_MSDIV                                    (0x00000fff)
#define I2C_DIVIDER_USDIV                                    (0x0fff0000)

/* I2C_FILTER_SCL_CFG : Register Bits Masks Definitions */
#define I2C_FILTER_SCL_CFG_END_TAP                           (0x00001f00)
#define I2C_FILTER_SCL_CFG_THRES0                            (0x001f0000)
#define I2C_FILTER_SCL_CFG_THRES1                            (0x1f000000)

/* I2C_FILTER_SDA_CFG : Register Bits Masks Definitions */
#define I2C_FILTER_SDA_CFG_END_TAP                           (0x00001f00)
#define I2C_FILTER_SDA_CFG_THRES0                            (0x001f0000)
#define I2C_FILTER_SDA_CFG_THRES1                            (0x1f000000)

/* I2C_START_SETUP_HOLD : Register Bits Masks Definitions */
#define I2C_START_SETUP_HOLD_START_HOLD                      (0x0000ffff)
#define I2C_START_SETUP_HOLD_START_SETUP                     (0xffff0000)

/* I2C_STOP_IDLE : Register Bits Masks Definitions */
#define I2C_STOP_IDLE_IDLE_DET                               (0x0000ffff)
#define I2C_STOP_IDLE_STOP_SETUP                             (0xffff0000)

/* I2C_SDA_SETUP_HOLD : Register Bits Masks Definitions */
#define I2C_SDA_SETUP_HOLD_SDA_HOLD                          (0x0000ffff)
#define I2C_SDA_SETUP_HOLD_SDA_SETUP                         (0xffff0000)

/* I2C_SCL_PERIOD : Register Bits Masks Definitions */
#define I2C_SCL_PERIOD_SCL_LOW                               (0x0000ffff)
#define I2C_SCL_PERIOD_SCL_HIGH                              (0xffff0000)

/* I2C_SCL_MIN_PERIOD : Register Bits Masks Definitions */
#define I2C_SCL_MIN_PERIOD_SCL_MINL                          (0x0000ffff)
#define I2C_SCL_MIN_PERIOD_SCL_MINH                          (0xffff0000)

/* I2C_SCL_ARB_TIMEOUT : Register Bits Masks Definitions */
#define I2C_SCL_ARB_TIMEOUT_ARB_TO                           (0x0000ffff)
#define I2C_SCL_ARB_TIMEOUT_SCL_TO                           (0xffff0000)

/* I2C_BYTE_TRAN_TIMEOUT : Register Bits Masks Definitions */
#define I2C_BYTE_TRAN_TIMEOUT_TRAN_TO                        (0x0000ffff)
#define I2C_BYTE_TRAN_TIMEOUT_BYTE_TO                        (0xffff0000)

/* I2C_BOOT_DIAG_TIMER : Register Bits Masks Definitions */
#define I2C_BOOT_DIAG_TIMER_COUNT                            (0x0000ffff)
#define I2C_BOOT_DIAG_TIMER_FREERUN                          (0x80000000)

/* I2C_BOOT_DIAG_PROGRESS : Register Bits Masks Definitions */
#define I2C_BOOT_DIAG_PROGRESS_PADDR                         (0x0000ffff)
#define I2C_BOOT_DIAG_PROGRESS_REGCNT                        (0xffff0000)

/* I2C_BOOT_DIAG_CFG : Register Bits Masks Definitions */
#define I2C_BOOT_DIAG_CFG_BOOT_ADDR                          (0x0000007f)
#define I2C_BOOT_DIAG_CFG_PINC                               (0x10000000)
#define I2C_BOOT_DIAG_CFG_PASIZE                             (0x20000000)
#define I2C_BOOT_DIAG_CFG_BDIS                               (0x40000000)
#define I2C_BOOT_DIAG_CFG_BOOTING                            (0x80000000)
