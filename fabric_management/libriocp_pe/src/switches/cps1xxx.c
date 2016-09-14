/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * RapidIO Specification Switch Driver
 */
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "pe.h"
#include "event.h"
#include "switch.h"
#include "rio_regs.h"
#include "rio_devs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_IDTGEN2_DIRECT_ROUTING 1
#define CONFIG_PRINT_LANE_STATUS_ON_EVENT 1
#define CONFIG_ERROR_LOG_SUPPORT 1
#define CONFIG_ERROR_LOG_STOP_THRESHOLD 100
#define CONFIG_SETUP_CACHE_ENABLE 1

#define CPS1xxx_DEBUG_INT_STATUS 1

#define CPS1xxx_RTE_PORT_SEL			(0x00010070)
#define CPS1xxx_MCAST_RTE_SEL			(0x00010080)

#define CPS1xxx_RIO_PW_DESTID_CSR		0x1028
#define CPS1xxx_RIO_SET_PW_DESTID(destid)	((destid) << 16)
#define CPS1xxx_RIO_PW_LARGE_DESTID	0x00008000

#define CPS1xxx_LANE_CTL_LANE_DIS			0x00000001
#define CPS1xxx_LANE_CTL_RX_RATE			0x00000006
#define CPS1xxx_LANE_CTL_RX_RATE_SHIFT			1
#define CPS1xxx_LANE_CTL_TX_RATE			0x00000018
#define CPS1xxx_LANE_CTL_TX_RATE_SHIFT			3
#define CPS1xxx_LANE_CTL_PLL_SEL_SHIFT			0x1f
#define CPS1xxx_LANE_CTL_TX_AMP_CTL			0x000007e0
#define CPS1xxx_LANE_CTL_TX_SYMBOL_CTL			0x00001800
#define CPS1xxx_LANE_CTL_LPBK_8BIT_EN			0x00004000
#define CPS1xxx_LANE_CTL_LPBK_10BIT_EN			0x00008000
#define CPS1xxx_LANE_CTL_XMITPRBS			0x00040000
#define CPS1xxx_LANE_CTL_PRBS_EN			0x00080000
#define CPS1xxx_LANE_CTL_PRBS_TRAIN			0x00100000
#define CPS1xxx_LANE_CTL_LANE_PW_EN			0x00200000
#define CPS1xxx_LANE_CTL_LANE_INT_EN			0x00400000
#define CPS1xxx_LANE_CTL_PRBS_UNIDIR_BERT_MODE_EN	0x00800000
#define CPS1xxx_LANE_CTL_PRBS_MODE			0x1e000000
#define CPS1xxx_LANE_CTL_RX_PLL_SEL			0x40000000
#define CPS1xxx_LANE_CTL_TX_PLL_SEL			0x80000000
#define CPS1xxx_LANE_CTL_PLL_SEL			\
	(CPS1xxx_LANE_CTL_TX_PLL_SEL | CPS1xxx_LANE_CTL_RX_PLL_SEL)
#define CPS1xxx_LANE_ERR_RATE_EN_LOSS			0x00000003

#define CPS1xxx_RTX_RATE_0			0x00000000
#define CPS1xxx_RTX_RATE_1			0x00000001
#define CPS1xxx_RTX_RATE_2			0x00000002
#define CPS1xxx_RTX_RATE_3			0x00000003

#define CPS1xxx_PLL_X_CTL_PLL_DIV_SEL		0x00000001
#define CPS1xxx_PLL_X_CTL_PLL_PWR_DOWN		0x00000002

#define CPS1xxx_PLL_X_CTL_1(x)				(0xff0000 + 0x010*(x))
#define CPS1xxx_LANE_X_CTL(x)				(0xff8000 + 0x100*(x))
#define CPS1616_LANE_CTL_1_25G				0x00000000
#define CPS1616_LANE_CTL_2_5G				0x0000000A
#define CPS1616_LANE_CTL_5_0G				0x00000014
#define CPS1616_LANE_CTL_3_125G				0xC000000A
#define CPS1616_LANE_CTL_6_25G				0xC0000014
#define CPS1432_LANE_CTL_1_25G				0x00000000
#define CPS1432_LANE_CTL_2_5G				0x0000000A
#define CPS1432_LANE_CTL_5_0G				0x00000014
#define CPS1432_LANE_CTL_3_125G				0x0000000A
#define CPS1432_LANE_CTL_6_25G				0x00000014
#define CPS1xxx_LANE_CTL_TX_AMP_S			(5)
#define CPS1xxx_LANE_CTL_TX_AMP_M			(0x3f<<CPS1xxx_LANE_CTL_TX_AMP_S)
#define CPS1xxx_LANE_CTL_TX_AMP(x)			(((x)<<CPS1xxx_LANE_CTL_TX_AMP_S)&CPS1xxx_LANE_CTL_TX_AMP_M)
#define CPS1xxx_LANE_X_ERR_RATE_EN(x)		(0xff8010 + 0x100*(x))
#define CPS1xxx_LANE_ERR_SYNC_EN			0x00000001
#define CPS1xxx_LANE_ERR_RDY_EN				0x00000002
#define CPS1xxx_LANE_X_ERR_DET(x)			(0xff800c + 0x100*(x))
#define CPS1xxx_LANE_ERR_SYNC				0x00000001
#define CPS1xxx_LANE_ERR_RDY				0x00000002
#define CPS1xxx_LANE_X_DFE1(x)				(0xff8028 + 0x100*(x))
#define CPS1xxx_LANE_DFE1_RX_DFE_DIS		(0x00040000)
#define CPS1xxx_LANE_DFE1_TAP_OFFS_SEL		(0x00020000)
#define CPS1xxx_LANE_DFE1_TAP4_SEL			(0x00010000)
#define CPS1xxx_LANE_DFE1_TAP3_SEL			(0x00008000)
#define CPS1xxx_LANE_DFE1_TAP2_SEL			(0x00004000)
#define CPS1xxx_LANE_DFE1_TAP1_SEL			(0x00002000)
#define CPS1xxx_LANE_DFE1_TAP0_SEL			(0x00001000)
#define CPS1xxx_LANE_X_DFE2(x)				(0xff802c + 0x100*(x))
#define CPS1xxx_LANE_DFE2_CFG_EN			(0x00000001)
#define CPS1xxx_LANE_DFE2_TAP_OFFS_CFG_S	(23)
#define CPS1xxx_LANE_DFE2_TAP_OFFS_CFG_M	(0x3f<<CPS1xxx_LANE_DFE2_TAP_OFFS_CFG_S)
#define CPS1xxx_LANE_DFE2_TAP_OFFS_CFG(x)	(((x)<<CPS1xxx_LANE_DFE2_TAP_OFFS_CFG_S)&CPS1xxx_LANE_DFE2_TAP_OFFS_CFG_M)
#define CPS1xxx_LANE_DFE2_TAP4_CFG_S		(20)
#define CPS1xxx_LANE_DFE2_TAP4_CFG_M		(0x7<<CPS1xxx_LANE_DFE2_TAP4_CFG_S)
#define CPS1xxx_LANE_DFE2_TAP4_CFG(x)		(((x)<<CPS1xxx_LANE_DFE2_TAP4_CFG_S)&CPS1xxx_LANE_DFE2_TAP4_CFG_M)
#define CPS1xxx_LANE_DFE2_TAP3_CFG_S		(16)
#define CPS1xxx_LANE_DFE2_TAP3_CFG_M		(0xf<<CPS1xxx_LANE_DFE2_TAP3_CFG_S)
#define CPS1xxx_LANE_DFE2_TAP3_CFG(x)		(((x)<<CPS1xxx_LANE_DFE2_TAP3_CFG_S)&CPS1xxx_LANE_DFE2_TAP3_CFG_M)
#define CPS1xxx_LANE_DFE2_TAP2_CFG_S		(11)
#define CPS1xxx_LANE_DFE2_TAP2_CFG_M		(0x1f<<CPS1xxx_LANE_DFE2_TAP2_CFG_S)
#define CPS1xxx_LANE_DFE2_TAP2_CFG(x)		(((x)<<CPS1xxx_LANE_DFE2_TAP2_CFG_S)&CPS1xxx_LANE_DFE2_TAP2_CFG_M)
#define CPS1xxx_LANE_DFE2_TAP1_CFG_S		(5)
#define CPS1xxx_LANE_DFE2_TAP1_CFG_M		(0x3f<<CPS1xxx_LANE_DFE2_TAP1_CFG_S)
#define CPS1xxx_LANE_DFE2_TAP1_CFG(x)		(((x)<<CPS1xxx_LANE_DFE2_TAP1_CFG_S)&CPS1xxx_LANE_DFE2_TAP1_CFG_M)
#define CPS1xxx_LANE_DFE2_TAP0_CFG_S		(1)
#define CPS1xxx_LANE_DFE2_TAP0_CFG_M		(0xf<<CPS1xxx_LANE_DFE2_TAP0_CFG_S)
#define CPS1xxx_LANE_DFE2_TAP0_CFG(x)		(((x)<<CPS1xxx_LANE_DFE2_TAP0_CFG_S)&CPS1xxx_LANE_DFE2_TAP0_CFG_M)

#define CPS1xxx_LANE_STAT_0_CSR(x)			(0x2010 + 0x020*(x))
#define CPS1xxx_LANE_STAT_1_CSR(x)			(0x2014 + 0x020*(x))
#define CPS1xxx_LANE_STAT_2_CSR(x)			(0x2018 + 0x020*(x))
#define CPS1xxx_LANE_STAT_3_CSR(x)			(0x201c + 0x020*(x))
#define CPS1xxx_LANE_STAT_4_CSR(x)			(0x2020 + 0x020*(x))
#define CPS1xxx_LANE_STAT_0_PORT_S			(24)
#define CPS1xxx_LANE_STAT_0_PORT_M			(0xff)
#define CPS1xxx_LANE_STAT_0_PORT(x)			(((x)>>CPS1xxx_LANE_STAT_0_PORT_S)&CPS1xxx_LANE_STAT_0_PORT_M)
#define CPS1xxx_LANE_STAT_0_LANE_S			(24)
#define CPS1xxx_LANE_STAT_0_LANE_M			(0xff)
#define CPS1xxx_LANE_STAT_0_LANE(x)			(((x)>>CPS1xxx_LANE_STAT_0_LANE_S)&CPS1xxx_LANE_STAT_0_LANE_M)
#define CPS1xxx_LANE_STAT_0_ERR_8B10B       (0x00000780)
#define CPS1xxx_LANE_STAT_0_ERR_8B10B_S     (7)
#define CPS1xxx_LANE_STAT_0_STAT_1_IMPL         (0x00000008)
#define CPS1xxx_LANE_STAT_0_STAT_2_7_MASK       (0x00000007)
#define CPS1xxx_LANE_STAT_0_STAT_N_IMPL(st0, n) (((st0) & CPS1xxx_LANE_STAT_0_STAT_2_7_MASK) >= ((n) - 1))
#define CPS1xxx_LANE_STAT_3_AMP_PROG_EN		(0x20000000)
#define CPS1xxx_LANE_STAT_3_GBAUD_BITS		(24)
#define CPS1xxx_LANE_STAT_3_GBAUD_MASK		(0x1f)
#define CPS1xxx_LANE_STAT_3_NEG1_TAP_S		(6)
#define CPS1xxx_LANE_STAT_3_NEG1_TAP_M		(0x3f<<CPS1xxx_LANE_STAT_3_NEG1_TAP_S)
#define CPS1xxx_LANE_STAT_3_NEG1_TAP(x)		(((x)<<CPS1xxx_LANE_STAT_3_NEG1_TAP_S)&CPS1xxx_LANE_STAT_3_NEG1_TAP_M)
#define CPS1xxx_LANE_STAT_3_POS1_TAP_S		(0)
#define CPS1xxx_LANE_STAT_3_POS1_TAP_M		(0x3f<<CPS1xxx_LANE_STAT_3_POS1_TAP_S)
#define CPS1xxx_LANE_STAT_3_POS1_TAP(x)		(((x)<<CPS1xxx_LANE_STAT_3_POS1_TAP_S)&CPS1xxx_LANE_STAT_3_POS1_TAP_M)

#define CPS1xxx_PORT_LINK_TO_CTL_CSR			(0x00000120)
#define CPS1xxx_PORT_X_LINK_MAINT_REQ_CSR(x)		(0x0140 + 0x020*(x))
#define CPS1xxx_PORT_X_LINK_MAINT_RESP_CSR(x)		(0x0144 + 0x020*(x))
#define CPS1xxx_PORT_X_LOCAL_ACKID_CSR(x)		(0x0148 + 0x020*(x))
#define CPS1xxx_PORT_X_CTL_2_CSR(x)			(0x0154 + 0x020*(x))
#define CPS1xxx_PORT_X_ERR_STAT_CSR(x)			(0x0158 + 0x020*(x))
#define CPS1xxx_PORT_X_CTL_1_CSR(x)			(0x015c + 0x020*(x))
#define CPS1xxx_PORT_X_ERR_DET_CSR(x)			(0x1040 + 0x040*(x))
#define CPS1xxx_PORT_X_ERR_RATE_EN_CSR(x)		(0x1044 + 0x040*(x))
#define CPS1xxx_PORT_X_ERR_RATE_CSR(x)			(0x1068 + 0x040*(x))
#define CPS1xxx_PORT_X_ERR_RATE_THRESH_CSR(x)		(0x106c + 0x040*(x))
#define CPS1xxx_PORT_X_OPS(x)				(0xf40004 + 0x100*(x))
#define CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(x)		(0xf40008 + 0x100*(x))
#define CPS1xxx_PORT_X_IMPL_SPEC_ERR_RATE_EN(x)		(0xf4000c + 0x100*(x))
#define CPS1xxx_PORT_X_IMPL_SPEC_ERR_RPT_EN(x)		(0x03104c + 0x040*(x))
#define CPS1xxx_PORT_X_ERR_RPT_EN(x)			(0x031044 + 0x040*(x))
#define CPS1xxx_PORT_X_STAT_CTRL(x)				(0xf400F0 + 0x100*(x))
#define CPS1xxx_PORT_X_RETRY_CNTR(x)			(0xf400CC + 0x100*(x))
#define CPS1xxx_PORT_X_LANE_SYNC(x)				(0xf40060 + 0x100*(x))

#define CPS1xxx_PORT_X_TRACE_PW_CTL(x)			(0xf40058 + 0x100*(x))
#define CPS1xxx_PORT_TRACE_PW_DIS				(0x00000001)

#define CPS1xxx_PORT_X_VC0_PA_TX_CNTR           (0xf40010)
#define CPS1xxx_PORT_X_VC0_NACK_TX_CNTR         (0xf40014)
#define CPS1xxx_PORT_X_VC0_RTRY_TX_CNTR         (0xf40018)
#define CPS1xxx_PORT_X_VC0_PKT_TX_CNTR          (0xf4001c)
#define CPS1xxx_PORT_X_VC0_PA_RX_CNTR           (0xf40040)
#define CPS1xxx_PORT_X_VC0_NACK_RX_CNTR         (0xf40044)
#define CPS1xxx_PORT_X_VC0_RTRY_RX_CNTR         (0xf40048)
#define CPS1xxx_PORT_X_VC0_PKT_RX_CNTR          (0xf40050)
#define CPS1xxx_PORT_X_VC0_CPB_TX_CNTR          (0xf4004c)
#define CPS1xxx_PORT_X_VC0_PKT_DROP_RX_CNTR     (0xf40064)
#define CPS1xxx_PORT_X_VC0_PKT_DROP_TX_CNTR     (0xf40068)
#define CPS1xxx_PORT_X_VC0_TTL_DROP_CNTR        (0xf4006c)
#define CPS1xxx_PORT_X_VC0_CRC_LIMIT_DROP_CNTR  (0xf40070)
#define CPS1xxx_PORT_X_TRC_MATCH_0              (0xf40020)
#define CPS1xxx_PORT_X_TRC_MATCH_1              (0xf40024)
#define CPS1xxx_PORT_X_TRC_MATCH_2              (0xf40028)
#define CPS1xxx_PORT_X_TRC_MATCH_3              (0xf4002c)
#define CPS1xxx_PORT_X_FIL_MATCH_0              (0xf40030)
#define CPS1xxx_PORT_X_FIL_MATCH_1              (0xf40034)
#define CPS1xxx_PORT_X_FIL_MATCH_2              (0xf40038)
#define CPS1xxx_PORT_X_FIL_MATCH_3              (0xf4003c)

#define CPS1xxx_BCAST_PORT_ERR_RPT_EN			(0x0003ff04)
#define CPS1xxx_BCAST_PORT_IMPL_SPEC_ERR_RPT_EN		(0x0003ff0c)
#define CPS1xxx_BCAST_PORT_OPS				(0x00f4ff04)
#define CPS1xxx_BCAST_PORT_IMPL_SPEC_ERR_DET		(0x00f4ff08)
#define CPS1xxx_BCAST_PORT_IMPL_SPEC_ERR_RATE_EN	(0x00f4ff0c)
#define CPS1xxx_BCAST_PORT_ERR_DET			(0x00ffff40)
#define CPS1xxx_BCAST_PORT_ERR_RATE_EN			(0x00ffff44)
#define CPS1xxx_BCAST_LANE_ERR_RPT_EN			(0x0003ff10)
#define CPS1xxx_BCAST_LANE_CTL				(0x00ffff00)
#define CPS1xxx_BCAST_LANE_GEN_SEED			(0x00ffff04)
#define CPS1xxx_BCAST_LANE_ERR_DET			(0x00ffff0c)
#define CPS1xxx_BCAST_LANE_ERR_RATE_EN			(0x00ffff10)
#define CPS1xxx_BCAST_LANE_ATTR_CAPT			(0x00ffff14)
#define CPS1xxx_BCAST_LANE_DFE_1			(0x00ffff18)
#define CPS1xxx_BCAST_LANE_DFE_2			(0x00ffff1c)

#define CPS1xxx_DEVICE_CTL_1				(0x00f2000c)
#define CPS1xxx_DEVICE_CTL_TRACE_PORT_MODE	(0x00010000)
#define CPS1xxx_DEVICE_CTL_TRACE_EN			(0x00008000)
#define CPS1xxx_DEVICE_CTL_TRACE_PORT(p)	((p) << 1)
#define CPS1xxx_DEVICE_CTL_TRACE_PORT_MASK	(0x1f << 1)
#define CPS1xxx_PW_CTL						(0x00f20024)
#define CPS1xxx_PW_TMO						(0x00f20180)
#define CPS1xxx_DEVICE_RESET_CTL			(0x00f20300)
#define CPS1xxx_DEVICE_RESET_DO				(0x80000000)
#define CPS1xxx_DEVICE_RESET_ALL			(0x40000000)
#define CPS1xxx_DEVICE_RESET_PLL_SHIFT		(18)
#define CPS1432_DEVICE_RESET_PLL_MASK		(0xff << CPS1xxx_DEVICE_RESET_PLL_SHIFT)
#define CPS1848_DEVICE_RESET_PLL_MASK		(0xfff << CPS1xxx_DEVICE_RESET_PLL_SHIFT)
#define CPS1xxx_DEVICE_RESET_PLL(p)			(1 << (CPS1xxx_DEVICE_RESET_PLL_SHIFT + (p)))
#define CPS1xxx_DEVICE_RESET_PORT_SHIFT		(0)
#define CPS1xxx_DEVICE_RESET_PORT_MASK		(0xffff << CPS1xxx_DEVICE_RESET_PORT_SHIFT)
#define CPS1xxx_DEVICE_RESET_PORT(p)		(1 << (CPS1xxx_DEVICE_RESET_PORT_SHIFT + (p)))
#define CPS1xxx_I2C_MASTER_STAT_CTL			(0x00f20054)

#define CPS1xxx_PKT_TTL_CSR				(0x0000102C)

#define CPS1xxx_CFG_ERR_CAPT_EN				(0x00020008)
#define CPS1xxx_CFG_BLK_ERR_RPT				(0x00f20014)
#define CPS1xxx_CFG_ERR_DET				(0x00020010)

#define CPS1xxx_LT_ERR_DET_CSR				(0x00001008)
#define CPS1xxx_LT_CTL_CAPT_CSR				(0x0000101c)
#define CPS1xxx_LT_ERR_EN_CSR				(0x0000100c)
#define CPS1xxx_LT_ERR_RPT_EN				(0x0003100c)

/* Port width */
#define CPS1xxx_CTL_PORT_WIDTH(x)		((x) & 0xc0000000)
#define CPS1xxx_CTL_PORT_WIDTH_X1		0x00000000 /* x1 supported */
#define CPS1xxx_CTL_PORT_WIDTH_X1_X4		0x40000000 /* x1 or x4 supported */
#define CPS1xxx_CTL_PORT_WIDTH_X1_X2		0x80000000 /* x1 or x2 supported */
#define CPS1xxx_CTL_PORT_WIDTH_X1_X2_X4		0xc0000000 /* x1, x2 or x4 supported */
/* others reserved */

/* Initialized port width */
#define CPS1xxx_CTL_INIT_PORT_WIDTH(x)		((x) & 0x38000000)
#define CPS1xxx_CTL_INIT_PORT_WIDTH_X1_L0	0x00000000
#define CPS1xxx_CTL_INIT_PORT_WIDTH_X1_L2	0x08000000
#define CPS1xxx_CTL_INIT_PORT_WIDTH_X4		0x10000000
#define CPS1xxx_CTL_INIT_PORT_WIDTH_X2		0x18000000
/* others reserved */

/* Override port width */
#define CPS1xxx_CTL_PORT_WIDTH_OVR(x)		((x) & 0x07000000)
#define CPS1xxx_CTL_PORT_WIDTH_OVR_NONE		0x00000000
#define CPS1xxx_CTL_PORT_WIDTH_OVR_X1_L0	0x02000000 /* x1 over default lane */
#define CPS1xxx_CTL_PORT_WIDTH_OVR_X1_L2	0x03000000 /* x1 over redundant lane */
#define CPS1xxx_CTL_PORT_WIDTH_OVR_X1_X2	0x05000000 /* x1 or x2 supported */
#define CPS1xxx_CTL_PORT_WIDTH_OVR_X1_X4	0x06000000 /* x1 or x4 supported */
#define CPS1xxx_CTL_PORT_WIDTH_OVR_X1_X2_X4	0x07000000 /* x1, x2 or x4 supported */
/* others reserved */

#define CPS1xxx_CTL_PORT_DIS			0x00800000
#define CPS1xxx_CTL_OUTPUT_EN			0x00400000
#define CPS1xxx_CTL_INPUT_EN			0x00200000
#define CPS1xxx_CTL_ERROR_CHK_DIS		0x00100000
#define CPS1xxx_CTL_ENUMERATION_BOUNDARY	0x00040000
#define CPS1xxx_CTL_PORT_LOCKOUT		0x00000002
#define CPS1xxx_CTL_STOP_ON_PORT_FAIL		0x00000008
#define CPS1xxx_CTL_DROP_PKT_EN			0x00000004

#define CPS1xxx_OPS_GEN_INTERRUPT		0x10000000
#define CPS1xxx_OPS_GEN_PORT_WRITE		0x08000000
#define CPS1xxx_OPS_CRC_RETX_LIMIT		0x0000000e
#define CPS1xxx_OPS_LT_LOG_EN			0x00000010
#define CPS1xxx_OPS_LANE_LOG_EN			0x00000020
#define CPS1xxx_OPS_PORT_LOG_EN			0x00000040
#define CPS1xxx_OPS_TRACE_PW_EN			0x00000080
#define CPS1xxx_OPS_PORT_LPBK_EN		0x00000100
#define CPS1xxx_OPS_TRACE_0_EN			0x00000200
#define CPS1xxx_OPS_TRACE_1_EN			0x00000400
#define CPS1xxx_OPS_TRACE_2_EN			0x00000800
#define CPS1xxx_OPS_TRACE_3_EN			0x00001000
#define CPS1xxx_OPS_FILTER_0_EN			0x00002000
#define CPS1xxx_OPS_FILTER_1_EN			0x00004000
#define CPS1xxx_OPS_FILTER_2_EN			0x00008000
#define CPS1xxx_OPS_FILTER_3_EN			0x00010000
#define CPS1xxx_OPS_SELF_MCAST_EN		0x00020000
#define CPS1xxx_OPS_TX_FLOW_CTL_DIS		0x00080000
#define CPS1xxx_OPS_FORCE_REINIT		0x00100000
#define CPS1xxx_OPS_SILENCE_CTL			0x03c00000
#define CPS1xxx_OPS_CNTRS_EN			0x04000000
#define CPS1xxx_OPS_PORT_PW_EN			0x08000000
#define CPS1xxx_OPS_PORT_INT_EN			0x10000000
#define CPS1xxx_OPS_CRC_DIS			0x20000000

#define CPS1xxx_LT_ERR_RPT_EN_DEFAULT		0x08C000FF
#define CPS1xxx_LT_ERR_EN_CSR_DEFAULT		0x08C00001

#define CPS1xxx_ERR_STATUS_IDLE2		0x80000000
#define CPS1xxx_ERR_STATUS_IDLE2_EN		0x40000000
#define CPS1xxx_ERR_STATUS_IDLE_SEQ		0x20000000
#define CPS1xxx_ERR_STATUS_OUTPUT_DROP		0x04000000
#define CPS1xxx_ERR_STATUS_OUTPUT_FAIL		0x02000000
#define CPS1xxx_ERR_STATUS_OUTPUT_RE		0x00100000
#define CPS1xxx_ERR_STATUS_OUTPUT_R		0x00080000
#define CPS1xxx_ERR_STATUS_OUTPUT_RS		0x00040000
#define CPS1xxx_ERR_STATUS_OUTPUT_ERR		0x00020000
#define CPS1xxx_ERR_STATUS_OUTPUT_ERR_STOP	0x00010000
#define CPS1xxx_ERR_STATUS_INPUT_RS		0x00000400
#define CPS1xxx_ERR_STATUS_INPUT_ERR		0x00000200
#define CPS1xxx_ERR_STATUS_INPUT_ERR_STOP	0x00000100
#define CPS1xxx_ERR_STATUS_PORT_W_PEND		0x00000010
#define CPS1xxx_ERR_STATUS_PORT_ERR		0x00000004
#define CPS1xxx_ERR_STATUS_PORT_OK		0x00000002
#define CPS1xxx_ERR_STATUS_PORT_UNINIT		0x00000001
#define CPS1xxx_ERR_STATUS_CLEAR		0x07120204

#define CPS1xxx_ERR_DET_LINK_TIMEOUT		0x00000001
#define CPS1xxx_ERR_DET_CS_ACK_ILL		0x00000002
#define CPS1xxx_ERR_DET_DELIN_ERR		0x00000004
#define CPS1xxx_ERR_DET_PRTCL_ERR		0x00000010
#define CPS1xxx_ERR_DET_LR_ACKID_ILL		0x00000020
#define CPS1xxx_ERR_DET_UNUSED			0x00004000
#define CPS1xxx_ERR_DET_IDLE1_ERR		0x00008000
#define CPS1xxx_ERR_DET_PKT_ILL_SIZE		0x00020000
#define CPS1xxx_ERR_DET_PKT_CRC_ERR		0x00040000
#define CPS1xxx_ERR_DET_PKT_ILL_ACKID		0x00080000
#define CPS1xxx_ERR_DET_CS_NOT_ACC		0x00100000
#define CPS1xxx_ERR_DET_UNEXP_ACKID		0x00200000
#define CPS1xxx_ERR_DET_CS_CRC_ERR		0x00400000
#define CPS1xxx_ERR_DET_IMP_SPEC_ERR		0x80000000

#define CPS1xxx_ERR_DET_IMP_SPEC_SHIFT		0x1f

/* This reset value comes from CPS1xxx User Manual,
	chapter 9.2.3 Register Initialization */
#define CPS1xxx_ERR_RPT_DEFAULT			(0x807e8037 & ~CPS1xxx_ERR_DET_UNEXP_ACKID)

#define CPS1xxx_IMPL_SPEC_ERR_DET_REORDER	0x00000001
#define CPS1xxx_IMPL_SPEC_ERR_DET_BAD_CTL	0x00000002
#define CPS1xxx_IMPL_SPEC_ERR_DET_LOA		0x00000004
#define CPS1xxx_IMPL_SPEC_ERR_DET_IDLE_IN_PKT	0x00000008
#define CPS1xxx_IMPL_SPEC_ERR_DET_PORT_WIDTH	0x00000010
#define CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT	0x00000020
#define CPS1xxx_IMPL_SPEC_ERR_DET_UNEXP_STOMP	0x00000040
#define CPS1xxx_IMPL_SPEC_ERR_DET_UNEXP_EOP	0x00000080
#define CPS1xxx_IMPL_SPEC_ERR_DET_LR_X2		0x00000100
#define CPS1xxx_IMPL_SPEC_ERR_DET_LR_CMD	0x00000200
#define CPS1xxx_IMPL_SPEC_ERR_DET_RX_STOMP	0x00000400
#define CPS1xxx_IMPL_SPEC_ERR_DET_STOMP_TO	0x00000800
#define CPS1xxx_IMPL_SPEC_ERR_DET_RETRY_ACKID	0x00001000
#define CPS1xxx_IMPL_SPEC_ERR_DET_RETRY		0x00002000
#define CPS1xxx_IMPL_SPEC_ERR_DET_FATAL_TO	0x00004000
#define CPS1xxx_IMPL_SPEC_ERR_DET_UNSOL_RFR	0x00008000
#define CPS1xxx_IMPL_SPEC_ERR_DET_SHORT		0x00010000
#define CPS1xxx_IMPL_SPEC_ERR_DET_BAD_TT	0x00020000
#define CPS1xxx_IMPL_SPEC_ERR_DET_RX_DROP	0x00080000
#define CPS1xxx_IMPL_SPEC_ERR_DET_MANY_RETRY	0x00100000
#define CPS1xxx_IMPL_SPEC_ERR_DET_TX_DROP	0x00200000
#define CPS1xxx_IMPL_SPEC_ERR_DET_SET_ACKID	0x00400000
#define CPS1xxx_IMPL_SPEC_ERR_DET_RTE_ISSUE	0x01000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_PNA_RETRY	0x02000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_UNEXP_ACKID	0x04000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_UNSOL_LR	0x08000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_PNA		0x10000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_CRC_EVENT	0x20000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_TTL_EVENT	0x40000000
#define CPS1xxx_IMPL_SPEC_ERR_DET_ERR_RATE	0x80000000

#define CPS1xxx_IMPL_SPEC_ERR_RATE_EN_ERR_RATE_EN	0x00000000

#define CPS1xxx_IMPL_SPEC_ERR_RPT_DEFAULT	(0x80000020 & ~CPS1xxx_IMPL_SPEC_ERR_DET_UNEXP_ACKID)

#define CPS1xxx_CFG_BLK_ERR_RPT_CFG_LOG_EN	0x00000001
#define CPS1xxx_CFG_BLK_ERR_RPT_CFG_PW_EN	0x00000002
#define CPS1xxx_CFG_BLK_ERR_RPT_CFG_INT_EN	0x00000004
#define CPS1xxx_CFG_BLK_ERR_RPT_CFG_PW_PEND	0x00000008

#define CPS1xxx_ERR_RATE_RB_DISABLE		0x00000000
#define CPS1xxx_ERR_RATE_RB_10SEC		0x10000000
#define CPS1xxx_ERR_RATE_RR_16			0x00020000
#define CPS1xxx_ERR_RATE_RR_NO_LIMIT		0x00030000
#define CPS1xxx_ERR_RATE_RESET		(CPS1xxx_ERR_RATE_RB_DISABLE)

#define CPS1xxx_ERR_THRESH_OUTPUT_FAIL		0x01000000
#define CPS1xxx_ERR_THRESH_RFT_DISABLE		0x00000000
#define CPS1xxx_ERR_THRESH_RFT_1		0x01000000
#define CPS1xxx_ERR_THRESH_RDT_DISABLE		0x00000000
#define CPS1xxx_ERR_THRESH_RDT_1		0x00010000

#define CPS1xxx_RTE_PORT_CSR_PORT		0x000000ff
#define CPS1xxx_RTE_PORT_CSR_PORT_1		0x0000ff00
#define CPS1xxx_RTE_PORT_CSR_PORT_2		0x00ff0000
#define CPS1xxx_RTE_PORT_CSR_PORT_3		0xff000000

#define CPS1xxx_I2C_CHKSUM_FAIL				(1 << 23)
#define CPS1xxx_I2C_WORD_ERR_32				(1 << 24)
#define CPS1xxx_I2C_WORD_ERR_22				(1 << 25)
#define CPS1xxx_I2C_NACK				(1 << 26)
#define CPS1xxx_I2C_UNEXP_START_STOP			(1 << 27)
#define CPS1xxx_I2C_CPS10Q_BAD_IMG_VERSION		(1 << 28)

#define CPS1xxx_PORT_STAT_CTL_CLR_MANY_RETRY	0x00000004
#define CPS1xxx_PORT_STAT_CTL_RETRY_LIM_EN		0x00000002

#define CPS1xxx_PORT_RETRY_CNTR_LIM(x)		((x) << 16)

/* Port-write */
#define CPS1xxx_PW_GET_EVENT_CODE(revent) (((*(revent)).u.portwrite.payload[2] >> 8) & 0xFF)

#define LOG_LT_ERR_FIRST			0x31
#define LOG_LT_ERR_LAST				0x36
#define LOG_PORT_ERR_FIRST			0x71
#define LOG_PORT_ERR_LAST			0xaa
#define LOG_LANE_ERR_FIRST			0x60
#define LOG_LANE_ERR_LAST			0x6a
#define LOG_I2C_ERR_FIRST			0x10
#define LOG_I2C_ERR_LAST			0x14
#define LOG_CFG_ERR_FIRST			0x50
#define LOG_CFG_ERR_LAST			0x56

#define LOG_LT_BAD_READ_SIZE			0x31
#define LOG_LT_BAD_WR_SIZE			0x32
#define LOG_LT_READ_REQ_WITH_DATA		0x33
#define LOG_LT_WR_REQ_WITHOUT_DATA		0x34
#define LOG_LT_INVALID_READ_WR_SIZE		0x35
#define LOG_LT_BAD_MTC_TRANS			0x36

#define LOG_CFG_BAD_MASK			0x50
#define LOG_CFG_BAD_PORT			0x53
#define LOG_CFG_RTE_FORCE			0x54
#define LOG_CFG_BAD_RTE				0x55
#define LOG_CFG_BAD_MCAST			0x56

#define I2C_CHKSUM_FAIL				(1 << 23)
#define I2C_WORD_ERR_32				(1 << 24)
#define I2C_WORD_ERR_22				(1 << 25)
#define I2C_NACK				(1 << 26)
#define I2C_UNEXP_START_STOP			(1 << 27)
#define I2C_CPS10Q_BAD_IMG_VERSION		(1 << 28)

/* Misc */
#define CPS1xxx_LOCAL_ACKID_CSR_RESET		0x80000000
#define CPS1xxx_PW_INFO_PRIO3_CRF1			0x0000e000
#define CPS1xxx_PW_INFO_SRCID(id)			(0xffff0000 & ((id) << 16))
#define CPS1xxx_PW_TMO_TIMOUT_DIS			(0x00000001)
#define CPS1xxx_PW_TMO_TIMEOU(tm)			(0xffffff00 & ((tm) << 8))
#define CPS1xxx_DEVICE_RESET_CTL_DO_RESET	0x80000000
#define CPS1xxx_RIO_LINK_TIMEOUT_DEFAULT	0x00000500	/* approx 1.76 usecs */
#define CPS1xxx_PKT_TTL_CSR_TTL_MAXTTL		0xffff0000
#define CPS1xxx_PKT_TTL_CSR_TTL_OFF			0x00000000
#define CPS1xxx_DEFAULT_ROUTE			0xde
#define CPS1xxx_NO_ROUTE			0xdf
#define CPS1xxx_MCAST_MASK_FIRST		0x40
#define CPS1xxx_MCAST_MASK_LAST			0x67
#define CPS1xxx_QUAD_CFG            0xF20200
#define CPS1xxx_VMIN_DEFAULT		4	/* 2^15-1 or 2^14-1 consecutive control symbols for link up required, reduces link flicker during startup */

/* CPS1616 */
//#define CPS1616_PW_TIMER			0x05000 /* 28 us */
#define CPS1616_PW_TIMER			0x820be700 /* 3 s */
#define CPS1616_DEVICE_PW_TIMEOUT		(0x00f20180)

/* CPS10Q */
#define CPS10Q_QUAD_X_CTRL(quad)			(0xff0000 + 0x1000*(quad))
#define CPS10Q_QUAD_X_CTRL_SPEEDSEL			(3 << 0)
#define CPS10Q_QUAD_X_CTRL_TCOEFF			(7 << 2)
#define CPS10Q_QUAD_X_CTRL_STD_ENH_SEL			(1 << 5)
#define CPS10Q_QUAD_X_CTRL_FORCE_REINIT			(1 << 6)
#define CPS10Q_QUAD_X_CTRL_TXDRVSEL			(7 << 7)
#define CPS10Q_QUAD_X_CTRL_PLL_LANE_0_1_RESET		(1 << 10) /* quad 0/5: lane 2/3 */
#define CPS10Q_QUAD_X_CTRL_PLL_LANE_2_3_RESET		(1 << 11) /* quad 0/5: lane 0/1 */
#define CPS10Q_QUAD_X_CTRL_LANE23_CTRL_EN		(1 << 16)
#define CPS10Q_QUAD_X_CTRL_LANE23_SPEEDSEL		(3 << 17)
#define CPS10Q_QUAD_X_CTRL_LANE23_SPEEDSEL_SHIFT	17
#define CPS10Q_QUAD_X_CTRL_LANE23_TCOEFF		(7 << 19)
#define CPS10Q_QUAD_X_CTRL_LANE23_FORCE_REINIT		(1 << 22)
#define CPS10Q_QUAD_X_CTRL_LANE23_TXDRVSEL		(7 << 23)

#define CPS10Q_QUAD_X_ERROR_REPORT_EN(quad)		(0xff0004 + 0x1000*(quad))
#define CPS10Q_QUAD_CTRL_BROADCAST			(0xfff000)

#ifdef CONFIG_IDTGEN2_DIRECT_ROUTING
#define CPS1xxx_RTE_PORT_DM(port, domain)	(0x00e10400+(port)*0x1000+(domain)*0x4)
#define CPS1xxx_RTE_PORT_DEV(port, device)	(0x00e10000+(port)*0x1000+(device)*0x4)
#define CPS1xxx_RTE_BCAST_DM(domain)		(0x00e00400+(domain)*0x4)
#define CPS1xxx_RTE_BCAST_DEV(device)		(0x00e00000+(device)*0x4)
#define CPS1xxx_RTE_DM_TO_DEV				(0x000000DD)
#define CPS1xxx_BCAST_MCAST_MASK(maskid)	(0x00f30000+(maskid)*0x4)
#define CPS1xxx_PORT_MCAST_MASK(port, maskid)	(0x00f38000+(port)*0x100+(maskid)*0x4)
#endif
#define CPS1xxx_RTE_RIO_DOMAIN				(0x00F20020)
#define CPS1xxx_LOG_DATA					(0x00FD0004)

#define CPS1xxx_TRACE_FILTER_UNITS			4
#define CPS1xxx_TRACE_FILTER_WORDS			5
#define CPS1xxx_TRACE_FILTER_VALUE(p, u, w) (0xe40000 + 0x100 * (p) + 0x28 * (u) + 4 * (w))
#define CPS1xxx_TRACE_FILTER_MASK(p, u, w)  (0xe40014 + 0x100 * (p) + 0x28 * (u) + 4 * (w))
#define CPS1xxx_TRACE_FILTER_VALUE_BC(u, w) (0xe4f000 + 0x28 * (u) + 4 * (w))
#define CPS1xxx_TRACE_FILTER_MASK_BC(u, w)  (0xe4f014 + 0x28 * (u) + 4 * (w))

#define CPS1xxx_SUPPORTED_SPEEDS_CACHED			(0x8000)
#define CPS1xxx_SUPPORTED_SPEEDS_MASK			(0x7fff)

struct switch_port_priv_t {
	uint16_t retry_lim;
#ifdef CONFIG_SETUP_CACHE_ENABLE
	uint8_t first_lane;
	uint8_t width;
#endif
};

struct switch_lane_priv_t {
#ifdef CONFIG_SETUP_CACHE_ENABLE
    uint8_t port;
    uint8_t lane_in_port;
    uint8_t supported_speeds;
    enum riocp_pe_speed speed;
#endif
    uint32_t err_8b10b;
};

struct switch_priv_t {
	uint32_t event_counter;
	struct switch_lane_priv_t lanes[48];  /* CPS1848 is the maximum we support here */
	struct switch_port_priv_t ports[18];  /* CPS1848 is the maximum we support here */
};

struct portmap {
	uint16_t did;
	int quad_shift;
	uint8_t quad;
	uint8_t cfg;
	uint8_t port;
	uint8_t lane0;
	uint8_t width;
	uint8_t pll;
};

static const struct portmap gen2_portmaps[] = {
		{RIO_DID_IDT_CPS1432, 2, 0, 0, 0, 0, 4, 0},
		{RIO_DID_IDT_CPS1432, 2, 0, 0, 4, 16, 4, 4},

		{RIO_DID_IDT_CPS1432, 2, 0, 1, 0, 0, 2, 0},
		{RIO_DID_IDT_CPS1432, 2, 0, 1, 4, 16, 4, 4},
		{RIO_DID_IDT_CPS1432, 2, 0, 1, 12, 2, 2, 0},

		{RIO_DID_IDT_CPS1432, 2, 1, 0, 1, 4, 4, 1},
		{RIO_DID_IDT_CPS1432, 2, 1, 0, 5, 20, 4, 5},

		{RIO_DID_IDT_CPS1432, 2, 1, 1, 1, 4, 2, 1},
		{RIO_DID_IDT_CPS1432, 2, 1, 1, 5, 20, 4, 5},
		{RIO_DID_IDT_CPS1432, 2, 1, 1, 13, 6, 2, 1},

		{RIO_DID_IDT_CPS1432, 2, 2, 0, 2, 8, 4, 2},
		{RIO_DID_IDT_CPS1432, 2, 2, 0, 6, 24, 4, 6},

		{RIO_DID_IDT_CPS1432, 2, 2, 1, 2, 8, 2, 2},
		{RIO_DID_IDT_CPS1432, 2, 2, 1, 6, 24, 4, 6},
		{RIO_DID_IDT_CPS1432, 2, 2, 1, 14, 10, 2, 2},

		{RIO_DID_IDT_CPS1432, 2, 2, 2, 2, 8, 2, 2},
		{RIO_DID_IDT_CPS1432, 2, 2, 2, 6, 24, 2, 6},
		{RIO_DID_IDT_CPS1432, 2, 2, 2, 10, 26, 2, 6},
		{RIO_DID_IDT_CPS1432, 2, 2, 2, 14, 10, 2, 2},

		{RIO_DID_IDT_CPS1432, 2, 2, 3, 2, 8, 2, 2},
		{RIO_DID_IDT_CPS1432, 2, 2, 3, 6, 24, 4, 6},
		{RIO_DID_IDT_CPS1432, 2, 2, 3, 10, 11, 1, 2},
		{RIO_DID_IDT_CPS1432, 2, 2, 3, 14, 10, 1, 2},

		{RIO_DID_IDT_CPS1432, 2, 3, 0, 3, 12, 4, 3},
		{RIO_DID_IDT_CPS1432, 2, 3, 0, 7, 28, 4, 7},

		{RIO_DID_IDT_CPS1432, 2, 3, 1, 3, 12, 2, 3},
		{RIO_DID_IDT_CPS1432, 2, 3, 1, 7, 28, 4, 7},
		{RIO_DID_IDT_CPS1432, 2, 3, 1, 15, 14, 2, 3},

		{RIO_DID_IDT_CPS1432, 2, 3, 2, 3, 12, 2, 3},
		{RIO_DID_IDT_CPS1432, 2, 3, 2, 7, 28, 2, 7},
		{RIO_DID_IDT_CPS1432, 2, 3, 2, 11, 30, 2, 7},
		{RIO_DID_IDT_CPS1432, 2, 3, 2, 15, 14, 2, 3},

		{RIO_DID_IDT_CPS1432, 2, 3, 3, 3, 12, 2, 3},
		{RIO_DID_IDT_CPS1432, 2, 3, 3, 7, 28, 4, 7},
		{RIO_DID_IDT_CPS1432, 2, 3, 3, 11, 15, 1, 3},
		{RIO_DID_IDT_CPS1432, 2, 3, 3, 15, 14, 1, 3},


		{RIO_DID_IDT_CPS1848, 2, 0, 0, 0, 0, 4, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 0, 4, 16, 4, 4},
		{RIO_DID_IDT_CPS1848, 2, 0, 0, 8, 32, 4, 8},

		{RIO_DID_IDT_CPS1848, 2, 0, 1, 0, 0, 2, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 1, 12, 2, 2, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 1, 4, 16, 4, 4},
		{RIO_DID_IDT_CPS1848, 2, 0, 1, 8, 32, 4, 8},

		{RIO_DID_IDT_CPS1848, 2, 0, 2, 0, 0, 2, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 2, 12, 2, 2, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 2, 4, 16, 4, 4},
		{RIO_DID_IDT_CPS1848, 2, 0, 2, 8, 32, 2, 8},
		{RIO_DID_IDT_CPS1848, 2, 0, 2, 16, 34, 2, 8},

		{RIO_DID_IDT_CPS1848, 2, 0, 3, 0, 0, 2, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 3, 12, 2, 1, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 3, 16, 3, 1, 0},
		{RIO_DID_IDT_CPS1848, 2, 0, 3, 4, 16, 4, 4},
		{RIO_DID_IDT_CPS1848, 2, 0, 3, 8, 32, 4, 8},

		{RIO_DID_IDT_CPS1848, 2, 1, 0, 1, 4, 4, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 0, 5, 20, 4, 5},
		{RIO_DID_IDT_CPS1848, 2, 1, 0, 9, 36, 4, 9},

		{RIO_DID_IDT_CPS1848, 2, 1, 1, 1, 4, 2, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 1, 13, 6, 2, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 1, 5, 20, 4, 5},
		{RIO_DID_IDT_CPS1848, 2, 1, 1, 9, 36, 4, 9},

		{RIO_DID_IDT_CPS1848, 2, 1, 2, 1, 4, 2, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 2, 13, 6, 2, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 2, 5, 20, 4, 5},
		{RIO_DID_IDT_CPS1848, 2, 1, 2, 9, 36, 2, 9},
		{RIO_DID_IDT_CPS1848, 2, 1, 2, 17, 38, 2, 9},

		{RIO_DID_IDT_CPS1848, 2, 1, 3, 1, 4, 2, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 3, 13, 6, 1, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 3, 17, 7, 1, 1},
		{RIO_DID_IDT_CPS1848, 2, 1, 3, 5, 20, 4, 5},
		{RIO_DID_IDT_CPS1848, 2, 1, 3, 9, 36, 4, 9},

		{RIO_DID_IDT_CPS1848, 2, 2, 0, 2, 8, 4, 2},
		{RIO_DID_IDT_CPS1848, 2, 2, 0, 6, 24, 4, 6},
		{RIO_DID_IDT_CPS1848, 2, 2, 0, 10, 40, 4, 10},

		{RIO_DID_IDT_CPS1848, 2, 2, 1, 2, 8, 2, 2},
		{RIO_DID_IDT_CPS1848, 2, 2, 1, 14, 10, 2, 2},
		{RIO_DID_IDT_CPS1848, 2, 2, 1, 6, 24, 4, 6},
		{RIO_DID_IDT_CPS1848, 2, 2, 1, 10, 40, 4, 10},

		{RIO_DID_IDT_CPS1848, 2, 3, 0, 3, 12, 4, 3},
		{RIO_DID_IDT_CPS1848, 2, 3, 0, 7, 28, 4, 7},
		{RIO_DID_IDT_CPS1848, 2, 3, 0, 11, 44, 4, 11},

		{RIO_DID_IDT_CPS1848, 2, 3, 1, 3, 13, 2, 3},
		{RIO_DID_IDT_CPS1848, 2, 3, 1, 15, 14, 2, 15},
		{RIO_DID_IDT_CPS1848, 2, 3, 1, 7, 28, 4, 7},
		{RIO_DID_IDT_CPS1848, 2, 3, 1, 11, 44, 4, 11},
};
static const int gen2_portmaps_len = (sizeof(gen2_portmaps)/sizeof(gen2_portmaps[0]));

static int cps1xxx_port_get_first_lane(struct riocp_pe *sw,	uint8_t port, uint8_t *lane);
int cps1xxx_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width);
static int cps1xxx_read_lane_stat_0_csr(struct riocp_pe *sw, uint8_t lane, uint32_t *value);
static int cps1xxx_reset_port(struct riocp_pe *sw, uint8_t port);

#ifdef CONFIG_ERROR_LOG_SUPPORT

struct event_pair {
	uint8_t id;
	int index;
	const char *txt;
};

static const struct event_pair event_source_set[] = {
{0x40,0,"Lane 0"},
{0x41,1,"Lane 1"},
{0x42,2,"Lane 2"},
{0x43,3,"Lane 3"},
{0x44,4,"Lane 4"},
{0x45,5,"Lane 5"},
{0x46,6,"Lane 6"},
{0x47,7,"Lane 7"},
{0x48,8,"Lane 8"},
{0x49,9,"Lane 9"},
{0x4a,10,"Lane 10"},
{0x4b,11,"Lane 11"},
{0x4c,12,"Lane 12"},
{0x4d,13,"Lane 13"},
{0x4e,14,"Lane 14"},
{0x4f,15,"Lane 15"},
{0x50,16,"Lane 16"},
{0x51,17,"Lane 17"},
{0x52,18,"Lane 18"},
{0x53,19,"Lane 19"},
{0x54,20,"Lane 20"},
{0x55,21,"Lane 21"},
{0x56,22,"Lane 22"},
{0x57,23,"Lane 23"},
{0x58,24,"Lane 24"},
{0x59,25,"Lane 25"},
{0x5a,26,"Lane 26"},
{0x5b,27,"Lane 27"},
{0x5c,28,"Lane 28"},
{0x5d,29,"Lane 29"},
{0x5e,30,"Lane 30"},
{0x5f,31,"Lane 31"},
{0x60,32,"Lane 32"},
{0x61,33,"Lane 33"},
{0x62,34,"Lane 34"},
{0x63,35,"Lane 35"},
{0x64,36,"Lane 36"},
{0x65,37,"Lane 37"},
{0x66,38,"Lane 38"},
{0x67,39,"Lane 39"},
{0x68,40,"Lane 40"},
{0x69,41,"Lane 41"},
{0x6a,42,"Lane 42"},
{0x6b,43,"Lane 43"},
{0x6c,44,"Lane 44"},
{0x6d,45,"Lane 45"},
{0x6e,46,"Lane 46"},
{0x6f,47,"Lane 47"},
{0x2a,0,"Port 0"},
{0x29,1,"Port 1"},
{0x34,2,"Port 2"},
{0x33,3,"Port 3"},
{0x32,4,"Port 4"},
{0x31,5,"Port 5"},
{0x3c,6,"Port 6"},
{0x3b,7,"Port 7"},
{0x3a,8,"Port 8"},
{0x39,9,"Port 9"},
{0x3d,10,"Port 10"},
{0x3e,11,"Port 11"},
{0x1c,12,"Port 12"},
{0x1d,13,"Port 13"},
{0x27,14,"Port 14"},
{0x26,15,"Port 15"},
{0x25,16,"Port 16"},
{0x24,17,"Port 17"},
{0x1e,-1,"LT Layer"},
{0x00,-1,"Config,JTAG,I2C"}
};
#define EVENT_SOURCE_COUNT (sizeof(event_source_set)/sizeof(event_source_set[0]))

struct event_pair event_name_set[] = {
{0x30,0,"Maintenance_Handler_route_error"},
{0x31,0,"BAD_READ_SIZE"},
{0x32,0,"BAD_WRITE_SIZE"},
{0x33,0,"READ_REQ_WITH_DATA"},
{0x34,0,"WRITE_REQ_WITHOUT_DATA"},
{0x35,0,"INVALID_READ_WRITE_SIZE"},
{0x36,0,"BAD_MTC_TRANS"},
{0x71,0,"DELINEATION_ERROR"},
{0x78,0,"PROTOCOL_ERROR"},
{0x79,0,"PROTOCOL_ERROR"},
{0x7E,0,"UNSOLICITED_ACKNOWLEDGEMENT_CONTROL_SYMBOL"},
{0x80,0,"RECEIVED_CORRUPT_CONTROL_SYMBOL"},
{0x81,0,"RECEIVED_PACKET_WITH_BAD_CRC"},
{0x82,0,"RECEIVED_PACKET_WITH_BAD_ACKID"},
{0x83,0,"PROTOCOL_ERROR"},
{0x84,0,"PROTOCOL_ERROR"},
{0x87,0,"RECEIVED_ACKNOWLEDGE_CONTROL_SYMBOL_WITH_UNEXPECTED_ACKID"},
{0x88,0,"RECEIVED_RETRY_CONTROL_SYMBOL_WITH_UNEXPECTED_ACKID"},
{0x8A,0,"RECEIVED_PACKET_NOT_ACCEPTED_CONTROL_SYMBOL"},
{0x8B,0,"NON_OUTSTANDING_ACKID"},
{0x8D,0,"LINK_TIME_OUT"},
{0x8E,0,"PROTOCOL_ERROR"},
{0x8F,0,"PROTOCOL_ERROR"},
{0x90,0,"RECEIVED_PACKET_EXCEEDS_276_BYTES"},
{0xA0,0,"RECEIVED_DATA_CHARACTER_IN_IDLE1"},
{0x72,0,"SET_OUTSTANDING_ACKID_INVALID"},
{0x73,0,"DISCARDED_A_NON-MAINTENANCE_PACKET_TO_BE_TRANSMITTED"},
{0x74,0,"IDLE_CHARACTER_IN_PACKET"},
{0x75,0,"PORT_WIDTH_DOWNGRADE"},
{0x76,0,"LANES_REORDERED"},
{0x77,0,"LOSS_OF_ALIGNMENT"},
{0x78,0,"DOUBLE_LINK_REQUEST"},
{0x79,0,"LINK_REQUEST_WITH_RESERVED_COMMAND_FIELD_ENCODING"},
{0x7A,0,"STOMP_TIMEOUT"},
{0x7B,0,"STOMP_RECEIVED"},
{0x7C,0,"CONTINUOUS_MODE_PACKET_WAS_NACKED_AND_DISCARDED"},
{0x7D,0,"RECEIVED_PACKET_TOO_SHORT"},
{0x7F,0,"BAD_CONTROL_CHARACTER_SEQUENCE"},
{0x83,0,"RECEIVE_STOMP_OUTSIDE_OF_PACKET"},
{0x84,0,"RECEIVE_EOP_OUTSIDE_OF_PACKET"},
{0x85,0,"PORT_INIT_TX_ACQUIRED"},
{0x86,0,"DISCARDED_A_RECEIVED_NON-MAINTENANCE_PACKET"},
{0x87,0,"RECEIVED_ACCEPT_CONTROL_SYMBOL_WITH_UNEXPECTED_ACKID"},
{0x88,0,"RECEIVED__RETRY_CONTROL_SYMBOL_WITH_UNEXPECTED_ACKID"},
{0x89,0,"RECEIVED_RETRY_CONTROL_SYMBOL_WITH_VALID_ACKID"},
{0x8C,0,"LINK_RESPONSE_TIMEOUT"},
{0x8E,0,"UNSOLICITED_RESTART_FROM_RETRY"},
{0x8F,0,"RECEIVED_UNSOLICITED_LINK_RESPONSE"},
{0x91,0,"RECEIVED_PACKET_HAS_INVALID_TT"},
{0x92,0,"RECEIVED_NACK_OTHER_THAN_LACK_OF_RESOURCES"},
{0x97,0,"RECEIVED_PACKET_NOT_ACCEPTED_RX_BUFFER_UNAVAILABLE"},
{0x99,0,"TRANSMITTED_PACKET_DROPPED_VIA_CRC_RETRANSMIT_LIMIT"},
{0xA1,0,"PACKET_RECEIVED_THAT_REFERENCES_NO_ROUTE_AND_DROPPED"},
{0xA2,0,"PACKET_RECEIVED_THAT_REFERENCES_A_DISABLED_PORT_AND_DROPPED"},
{0xA3,0,"PACKET_RECEIVED_THAT_REFERENCES_A_PORT_IN_ERROR_STATE_AND_DROPPED"},
{0xA4,0,"PACKET_DROPPED_DUE_TO_TIME_TO_LIVE_EVENT"},
{0xA6,0,"A_PACKET_WAS_RECEIVED_WITH_A_CRC_ERROR_WITH_CRC_SUPPRESSION_WAS_ENABLED"},
{0xA7,0,"A_PACKET_WAS_RECEIVED_WHEN_AN_ERROR_RATE_THRESHOLD_EVENT_HAS_OCURRED_ANDDROP_PACKET_MODE_IS_ENABLED"},
{0xA9,0,"PACKET_RECEIVED_THAT_REFERENCES_A_PORT_CONFIGURED_IN_PORT_LOCKOUT_AND_DROPPED"},
{0xAA,0,"RX_RETRY_COUNT_TRIGGERED_CONGESTION_EVENT"},
{0x60,0,"LOSS_OF_LANE_SYNC"},
{0x61,0,"LOSS_OF_LANE_READY"},
{0x62,0,"RECEIVED_ILLEGAL_OR_INVALID_CHARACTER"},
{0x63,0,"LOSS_OF_DESCRAMBLER_SYNCHRONIZATION"},
{0x64,0,"RECEIVER_TRANSMITTER_TYPE_MISMATCH"},
{0x65,0,"TRAINING_ERROR"},
{0x66,0,"RECEIVER_TRANSMITTER_SCRAMBLING_MISMATCH"},
{0x67,0,"IDLE2_FRAMING_ERROR"},
{0x68,0,"LANE_INVERSION_DETECTED"},
{0x69,0,"REQUEST_LINK_SPEED_NOT_SUPPORTED"},
{0x6A,0,"RECEIVED_NACK_IN_IDLE2_CS_FIELD"},
{0x6B,0,"RECEIVED_TAP_MINUS_1_UPDATE_REQUEST_WHEN_TAP_IS_SATURATED"},
{0x6C,0,"RECEIVED_TAP_PLUS_1_UPDATE_REQUEST_WHEN_TAP_IS_SATURATED"},
{0x10,0,"I2C_LENGTH_ERROR"},
{0x11,0,"I2C_ACK_ERROR"},
{0x12,0,"I2C_22_BIT_MEMORY_ADDRESS_INCOMPLETE_ERROR"},
{0x13,0,"I2C_UNEXPECTED_START_STOP"},
{0x14,0,"I2C_EPROM_CHECKSUM_ERROR"},
{0x20,0,"JTAG_INCOMPLETE_WRITE"},
{0x50,0,"MULTICAST_MASK_CONFIG_ERROR"},
{0x53,0,"PORT_CONFIG_ERROR"},
{0x54,0,"FORCE_LOCAL_CONFIG_ERROR"},
{0x55,0,"ROUTE_TABLE_CONFIG_ERROR"},
{0x56,0,"MULTICAST_TRANSLATION_ERROR"},
{0x9E,0,"TRACE_MATCH_OCCURRED"},
{0x9F,0,"FILTER_MATCH_OCCURRED"},
{0xA8,0,"PGC_COMPLETE"}
};
#define EVENT_NAME_COUNT (sizeof(event_name_set)/sizeof(event_name_set[0]))

static void cps1xxx_dump_event_log(struct riocp_pe *sw)
{
	int ret;
	unsigned i;
	uint32_t log_data;
	const char *evt_src_str = "unkown";
	const char *evt_name_str = "unknown";
	uint8_t evt_src, evt_name;
#ifdef CONFIG_ERROR_LOG_STOP_THRESHOLD
	unsigned same_log_count = 0;
	uint32_t log_data_old = 0;
#endif

	do {
		ret = riocp_pe_maint_read(sw, CPS1xxx_LOG_DATA, &log_data);
		if(ret == 0 && log_data != 0) {
			evt_src = (log_data >> 8) & 0x7f;
			evt_name = log_data & 0xff;
			for(i=0;i<EVENT_SOURCE_COUNT;i++)
				if(event_source_set[i].id == evt_src)
					evt_src_str = event_source_set[i].txt;
			for(i=0;i<EVENT_NAME_COUNT;i++)
				if(event_name_set[i].id == evt_name)
					evt_name_str = event_name_set[i].txt;
			RIOCP_INFO("[0x%08x:%s:hc %u] %s %s\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
					evt_src_str, evt_name_str);
#ifdef CONFIG_ERROR_LOG_STOP_THRESHOLD
			if(log_data == log_data_old)
				same_log_count++;
			else
				same_log_count = 0;
			log_data_old = log_data;
			if(same_log_count >= CONFIG_ERROR_LOG_STOP_THRESHOLD) {
				/* switch off that dedicated log which is flooding us */
				for(i=0;i<EVENT_SOURCE_COUNT;i++)
					if(event_source_set[i].id == evt_src)
						break;
				if(evt_src >= 0x40 || evt_src <= 0x6f) {
					/* lane event */
					uint32_t lane_status_0, port_ops, port;

					ret = cps1xxx_read_lane_stat_0_csr(sw, (uint8_t)event_source_set[i].index, &lane_status_0);
					if (ret < 0) {
						RIOCP_WARN("[0x%08x:%s:hc %u] Failed to read lane status 0 for lane &d\n",
								sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
								event_source_set[i].index);
						continue;
					}
					port = (lane_status_0 >> 24) & 0xff;

					ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &port_ops);
					if (ret < 0) {
						RIOCP_WARN("[0x%08x:%s:hc %u] Failed to read port options for port %d\n",
								sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
								port);
						continue;
					}

					port_ops &= ~CPS1xxx_OPS_LANE_LOG_EN;

					ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), port_ops);
					if (ret < 0) {
						RIOCP_WARN("[0x%08x:%s:hc %u] Failed to write port options for port %d\n",
								sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
								port);
						continue;
					}
					RIOCP_WARN("[0x%08x:%s:hc %u] Disabled event logging for lane %d\n",
							sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
							event_source_set[i].index);
				} else if (evt_src == 0x1e) {
					/* LT event, do nothing here since the port number is unknown. */
				} else if (evt_src == 0) {
					/* config or JTAG or I2C, do nothing here since the port number is unknown. */
				} else {
					/* port event */
					uint32_t port_ops, port = (uint32_t)event_source_set[i].index;

					ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &port_ops);
					if (ret < 0) {
						RIOCP_WARN("[0x%08x:%s:hc %u] Failed to read port options for port %d\n",
								sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
								port);
						continue;
					}

					port_ops &= ~CPS1xxx_OPS_PORT_LOG_EN;

					ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), port_ops);
					if (ret < 0) {
						RIOCP_WARN("[0x%08x:%s:hc %u] Failed to write port options for port %d\n",
								sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
								port);
						continue;
					}
					RIOCP_WARN("[0x%08x:%s:hc %u] Disabled event logging for port %d\n",
							sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
							event_source_set[i].index);
				}
				same_log_count = 0;
			}
#endif
		}
	} while(ret == 0 && log_data != 0);
}
#endif

/* Reading the 8b10b error counter */
static int cps1xxx_get_switch_lane_8b10b(struct riocp_pe *sw, uint8_t lane, uint32_t *value)
{
    int ret;
    uint32_t reg_val;
    struct switch_priv_t *priv = (struct switch_priv_t *)sw->private_driver_data;

    ret = cps1xxx_read_lane_stat_0_csr(sw, lane, &reg_val);
    if (ret < 0)
        return ret;

    // return cached value
    *value = priv->lanes[lane].err_8b10b;
    //clear cached value after read
    priv->lanes[lane].err_8b10b = 0;

    return 0;
}

/* Reading the 8b10b error counter of a port specific lane */
static int cps1xxx_get_port_lane_8b10b(struct riocp_pe *sw, uint8_t port,
        uint8_t lane_in_port, uint32_t *lane_8b10b)
{
    int ret;
    uint8_t first_lane;

    ret = cps1xxx_port_get_first_lane(sw, port, &first_lane);
    if (ret < 0) {
        RIOCP_ERROR("Could net get first lane of port %u (ret = %d, %s)\n",
            port, ret, strerror(-ret));
        return ret;
    }
    cps1xxx_get_switch_lane_8b10b(sw, first_lane+lane_in_port, lane_8b10b);

    return 0;
}

/*
 * This function is for reading the lane status 0 CSR. It is caching the 8b10b error counter
 * which is a clear on read value.
 */
static int cps1xxx_read_lane_stat_0_csr(struct riocp_pe *sw, uint8_t lane, uint32_t *value)
{
	int ret;
	uint32_t status_0_csr, _err8b10b;
	struct switch_priv_t *priv = (struct switch_priv_t *)sw->private_driver_data;

	ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_STAT_0_CSR(lane), &status_0_csr);
	if (ret < 0)
		return ret;

	/* 8b10b decoding error caching */
	_err8b10b = status_0_csr & CPS1xxx_LANE_STAT_0_ERR_8B10B;
	_err8b10b = _err8b10b >> CPS1xxx_LANE_STAT_0_ERR_8B10B_S;
	priv->lanes[lane].err_8b10b += _err8b10b;

	*value = status_0_csr;
	return 0;
}

#define LANE_READ(LANE_IN_PORT) \
    int cps1xxx_get_port_err8b10b_lane_##LANE_IN_PORT(struct riocp_pe *sw, uint8_t port, \
            uint32_t *lane_8b10b) \
    { \
        return cps1xxx_get_port_lane_8b10b(sw, port, LANE_IN_PORT, lane_8b10b); \
    } \

LANE_READ(0)
LANE_READ(1)
LANE_READ(2)
LANE_READ(3)

/*
 * The following function checks if a port went to PORT_OK during arming of the port.
 */
static int cps1xxx_check_link_init(struct riocp_pe *sw, uint8_t port)
{
	uint32_t status, control, result, lm_resp;
	int ret, attempts, i;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &status);
	if (ret < 0)
		return ret;

	/* did irq run yet? */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &control);
	if (ret < 0)
		return ret;

	if (status & CPS1xxx_ERR_STATUS_PORT_OK) {
		for (attempts=0; attempts<5; attempts++) {
			/* trigger link request */
			ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_LINK_MAINT_REQ_CSR(port), RIO_MNT_REQ_CMD_IS);
			if (ret)
				return ret;

			RIOCP_DEBUG("link req on 0x%x:%u (0x%x) port %u (attempt %u)\n", sw->destid, sw->hopcount, sw->comptag, port, attempts);

			for(i=0;i<3;i++) {
				/* read link response */
				ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_LINK_MAINT_RESP_CSR(port), &lm_resp);
				if (ret)
					return ret;

				RIOCP_DEBUG("link resp on 0x%x:%u (0x%x) port %u: 0x%x\n", sw->destid, sw->hopcount, sw->comptag, port, lm_resp);
				/* check link response */
				if (lm_resp & RIO_PORT_N_MNT_RSP_RVAL) {
					break;
				}
			}
			if ((lm_resp & RIO_PORT_N_MNT_RSP_LSTAT) == 0x10) {
				/* clear port error status */
				ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &status);
				if (ret < 0)
					return ret;
				ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), status);
				if (ret < 0)
					return ret;

				break;
				/* TODO: Add possibility to update the ackid status for that port */
			}
		}
	}

	/* If the port did not achieve PORT_OK during initialization of the port
		it got locked. If it achieved PORT_OK in the meantime the link
		partner is not added to the network. this will toggle the port_init
		error to force a port-write with the PORT_INIT as implementation specific error */
	if ((status & CPS1xxx_ERR_STATUS_PORT_OK) && (control & CPS1xxx_CTL_PORT_LOCKOUT)) {
		/* Toggle link init if missed */

		RIOCP_DEBUG("toggle link init on 0x%x:%u (0x%x) port %u\n", sw->destid, sw->hopcount, sw->comptag, port);

		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &result);
		if (ret < 0)
			return ret;

		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port),
			result & ~CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT);
		if (ret < 0)
			return ret;

		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port),
			result | CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT);
		if (ret < 0)
			return ret;
	}

	return ret;
}

/**
 * The following function will prepare a port on initialization of the driver.
 * this function will:
 *	- enable interrupts for the port.
 *	- enable port-writes for the port.
 *	- Implement the procedure to prepare the port for hotswapping.
 *		see CPS1848 or CPS1616 manual: chapter 2.8
 */
static int cps1xxx_arm_port(struct riocp_pe *sw, uint8_t port)
{
	uint32_t result;
	int ret, lane_err_detected = 0;
	uint8_t first_lane = 0, lane_count = 0, lane;
    uint32_t err8b10b;

	ret = cps1xxx_port_get_first_lane(sw, port, &first_lane);
	if (ret < 0)
		return (ret == -ENOTSUP)?(0):(ret); /* return success if port not supported */

	ret = cps1xxx_get_lane_width(sw, port, &lane_count);
	if (ret < 0)
		return ret;

	if (lane_count == 0)
		return ret;

	/* Disable error rate bias, clear error rate counter. */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_RATE_CSR(port),
		CPS1xxx_ERR_RATE_RESET);
	if (ret < 0)
		return ret;

	/* Set error rate threshold csr to detect an output fail condition */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_RATE_THRESH_CSR(port),
		CPS1xxx_ERR_THRESH_OUTPUT_FAIL);
	if (ret < 0)
		return ret;

	/* enable los of lane trigger for all lanes on the current port */
	for (lane = 0; lane < lane_count; lane++) {

		/* Before the link is up and settles down, there are naturally 8b10b errors.
		 * We read them before the link is up and discard them.
		 */
        ret = cps1xxx_get_switch_lane_8b10b(sw, lane + first_lane, &err8b10b);
        if (ret)
            return ret;
        RIOCP_INFO("Read and discard 8b10b error counter for lane %d: %d\n",
        		lane + first_lane, err8b10b);
        if(err8b10b) {
        	lane_err_detected++;
        }

		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_ERR_DET(lane + first_lane), 0);
		if (ret < 0)
			return ret;
		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_ERR_RATE_EN(lane + first_lane),
				CPS1xxx_LANE_ERR_SYNC_EN | CPS1xxx_LANE_ERR_RDY_EN);
		if (ret < 0)
			return ret;
	}
	if(lane_err_detected) {
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &result);
		if (ret < 0)
			return ret;
		RIOCP_INFO("PORT%d_ERR_STAT: 0x%08x\n", port, result);
		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), result);
		if (ret < 0)
			return ret;

		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &result);
		if (ret < 0)
			return ret;
		RIOCP_INFO("PORT%d_IMP_SPEC_ERR_DET: 0x%08x\n", port, result);
		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port),
			result & ~CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT);
		if (ret < 0)
			return ret;
	}

	/* enable port-writes and interrupts for port events */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &result);
	if (ret < 0)
		return ret;

	/** FIXME temporary disable port interrupts to IPMC
		because probably the carrier gets in M7 communication lost state
	*/
#if 0
	result |= CPS1xxx_OPS_PORT_PW_EN | CPS1xxx_OPS_PORT_INT_EN;
#endif
#ifdef CONFIG_PORTWRITE_ENABLE
	result |= CPS1xxx_OPS_PORT_PW_EN;
#endif
#ifdef CONFIG_ERROR_LOG_SUPPORT
	result |= (CPS1xxx_OPS_LT_LOG_EN | CPS1xxx_OPS_LANE_LOG_EN | CPS1xxx_OPS_PORT_LOG_EN);
#endif

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), result);
	if (ret < 0)
		return ret;

	/*
	 * set STOP_ON_PORT_FAIL and DROP_PKT_EN to allow packets
	 * getting dropped when the port is in error condition
	 */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &result);
	if (ret < 0)
		return ret;

	result |= CPS1xxx_CTL_STOP_ON_PORT_FAIL | CPS1xxx_CTL_DROP_PKT_EN;

	/* The STOP_ON_PORT_FAIL and DROP_PKT_EN are required to enable hot swapping */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), result);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_RPT_EN(port),
		CPS1xxx_ERR_DET_IMP_SPEC_ERR);
	if (ret < 0)
		return ret;

	/*
	 * port write trigger events setup:
	 * - port fatal timeout is used to handle "PORT_OK may indicate incorrect status" errata.
	 */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_RPT_EN(port),
		CPS1xxx_IMPL_SPEC_ERR_DET_ERR_RATE |
		CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT |
		CPS1xxx_IMPL_SPEC_ERR_DET_MANY_RETRY |
		CPS1xxx_IMPL_SPEC_ERR_DET_FATAL_TO);
	if (ret < 0)
		return ret;

	/* check whether we missed link initialization between lockout and arming now */
	ret = cps1xxx_check_link_init(sw, port);

#ifdef CONFIG_ERROR_LOG_SUPPORT
	cps1xxx_dump_event_log(sw);
#endif

	return ret;
}

/*
 * The following function will enable a port to respond to all input packets.
 * and enable to port to issue any packets.
 *
 * See CPS1848 or CPS1616 manual: chapter 10.5 port control 1 csr, OUTPUT_PORT_EN
 * and INPUT_PORT_EN for more information.
 */
static int cps1xxx_unlock_port(struct riocp_pe *sw, uint8_t port)
{
	uint32_t val;
	int ret;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &val);
	if (ret < 0)
		return ret;

	val &= ~CPS1xxx_CTL_PORT_LOCKOUT;
	val |= (CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN);

	/* enable input/output, clear lockout */
	return riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), val);
}

static int cps1xxx_lock_port(struct riocp_pe *sw, uint8_t port)
{
	uint32_t val;
	int ret;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &val);
	if (ret < 0)
		return ret;

	val |= CPS1xxx_CTL_PORT_LOCKOUT | CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN;

	/* lockout port, simply wait for init notification to prevent race condition */
	return riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), val);
}

/*
 * Disable port logic and lanes that are assigned to that port.
 */
static int cps1xxx_disable_port(struct riocp_pe *sw, uint8_t port)
{
	int ret, i;
	uint32_t val;
	uint8_t lane = 0, width = 0, current_lane;

	/* obtain port width and lane configuration */
	ret = cps1xxx_get_lane_width(sw, port, &width);
	if (ret < 0)
		return ret;

	/* this port has no lanes assigned */
	if (width == 0)
		return 0;

	ret = cps1xxx_port_get_first_lane(sw, port, &lane);
	if (ret < 0)
		return ret;

	/* trigger los */
	for(current_lane = lane; current_lane < (lane+width); current_lane++) {
		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_ERR_DET(current_lane), &val);
		if (ret < 0)
			return ret;

		val |= (CPS1xxx_LANE_ERR_SYNC | CPS1xxx_LANE_ERR_RDY);

		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_ERR_DET(current_lane), val);
		if (ret < 0)
			return ret;
	}

	/* wait some time for port write pending */
	for(i=0;i<50;i++) {
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &val);
		if (ret < 0)
			return ret;
		if(val & CPS1xxx_ERR_STATUS_PORT_W_PEND)
			break;
	}

	/* disable lanes */
	for(current_lane = lane; current_lane < (lane+width); current_lane++) {
		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(current_lane), &val);
		if (ret < 0)
			return ret;

		val |= CPS1xxx_LANE_CTL_LANE_DIS;

		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(current_lane), val);
		if (ret < 0)
			return ret;
	}

	/* disable port logic */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &val);
	if (ret < 0)
		return ret;

	val |= CPS1xxx_CTL_PORT_DIS;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), val);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * enable port logic and lanes that are assigned to that port.
 * Finally a reset of the port is done to get it reinitialized entirely.
 */
static int cps1xxx_enable_port(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t val;
	uint8_t lane = 0, width = 0, current_lane;

	/* obtain port width and lane configuration */
	ret = cps1xxx_get_lane_width(sw, port, &width);
	if (ret < 0)
		return ret;

	ret = cps1xxx_port_get_first_lane(sw, port, &lane);
	if (ret < 0)
		return ret;

	for(current_lane = lane; current_lane < (lane+width); current_lane++) {
		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(current_lane), &val);
		if (ret < 0)
			return ret;

		/* enable lanes */
		val &= ~CPS1xxx_LANE_CTL_LANE_DIS;

		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(current_lane), val);
		if (ret < 0)
			return ret;
	}

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &val);
	if (ret < 0)
		return ret;

	/* enable port logic */
	val &= ~CPS1xxx_CTL_PORT_DIS;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), val);
	if (ret < 0)
		return ret;

	return cps1xxx_reset_port(sw, port);
}

/*
 * Test if a port is disabled.
 */
static int cps1xxx_is_port_disabled(struct riocp_pe *sw, uint8_t port, uint32_t *status)
{
	int ret;
	uint32_t port_ctl;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &port_ctl);
	if (ret < 0)
		return ret;

	*status = port_ctl & CPS1xxx_CTL_PORT_DIS;

	return 0;
}

/**
 * The function clears all port errors by rewriting the port error
 * and status register. It returns > 1 when there are error bits
 * remaining after that action or < 0 on access failures.
 */
static int cps1xxx_clear_port_error(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t val;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &val);
	if (ret < 0)
		return ret;
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), val | CPS1xxx_ERR_STATUS_CLEAR);
	if (ret < 0)
		return ret;
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &val);
	if (ret < 0)
		return ret;
	if (val & CPS1xxx_ERR_STATUS_CLEAR) {
		RIOCP_DEBUG("[0x%08x:%s:hc %u] Error status on port %u is 0x%08x after clear\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port, val);
		return 1;
	} else
		return ret;
}

static int cps1xxx_reset_port(struct riocp_pe *sw, uint8_t port)
{
	return riocp_pe_maint_write(sw, CPS1xxx_DEVICE_RESET_CTL, CPS1xxx_DEVICE_RESET_PORT(port) | CPS1xxx_DEVICE_RESET_DO);
}

static int cps1xxx_reset_pll(struct riocp_pe *sw, uint8_t pll)
{
	return riocp_pe_maint_write(sw, CPS1xxx_DEVICE_RESET_CTL, CPS1xxx_DEVICE_RESET_PLL(pll) | CPS1xxx_DEVICE_RESET_DO);
}

/**
 * The following functions gets called to force a port out of an input error state.
 * It clears input, output and port error from the error status csr.
 */
int cps1xxx_recover_port(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t val;

	/* preserve IDLE2 bit. IDT recommends disabling IDLE 2 for non 6.25 Gbaud
		connections. If it is disabled it must be preserved otherwise it will
		use IDLE 2 again */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &val);
	if (ret < 0)
		return ret;

	/* If the port is in input-error-stopped state it must be resetted to clear this error.
		a port will be in this state if a link toggles at reset of the switches. */
	if (val & CPS1xxx_ERR_STATUS_INPUT_ERR_STOP) {
		val = CPS1xxx_DEVICE_RESET_CTL_DO_RESET | (1 << port);
		ret = riocp_pe_maint_write(sw, CPS1xxx_DEVICE_RESET_CTL, val);
		if (ret < 0)
			return ret;
	}

	/* Read the errors again because they might be cleared already. */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &val);
	if (ret < 0)
		return ret;

	/* clear port errors */
	val |= CPS1xxx_ERR_STATUS_OUTPUT_ERR | CPS1xxx_ERR_STATUS_INPUT_ERR | CPS1xxx_ERR_STATUS_PORT_ERR;
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), val);

	return ret;
}

static int cps1xxx_enable_counters(struct riocp_pe *sw, uint8_t port)
{
    uint32_t result;
    int ret;

    /* enable port-writes and interrupts for port events */
    ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &result);
    if (ret < 0)
        return ret;

    result |= CPS1xxx_OPS_CNTRS_EN;

    ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), result);
    if (ret < 0)
        return ret;

    return 0;
}

/*
 * The following function will initialize a port by clearing the
 * error status csr. if there is no link detected on the port, the port will be
 * locked to improve switch performance.
 * 	- enable the port to issue and receive any packets
 *
 */
static int cps1xxx_init_port(struct riocp_pe *sw, uint8_t port)
{
	uint32_t status, result;
	int ret;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &status);
	if (ret < 0)
		return ret;

	/* If the port is in input-error-stopped state it must be resetted to clear this error.
		a port will be in this state if a link toggles at reset of the switches. */
	if (status & CPS1xxx_ERR_STATUS_INPUT_ERR_STOP) {
		ret = riocp_pe_maint_write(sw, CPS1xxx_DEVICE_RESET_CTL, CPS1xxx_DEVICE_RESET_CTL_DO_RESET | (1 << port));
		if (ret < 0)
			return ret;
	}

	/* program default Vmin */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_LANE_SYNC(port), CPS1xxx_VMIN_DEFAULT);
	if (ret < 0)
		return ret;

	/* Read the errors again because they might be cleared already. */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &status);
	if (ret < 0)
		return ret;

	/* Clear port errors */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port),
		status | CPS1xxx_ERR_STATUS_CLEAR);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &result);
	if (ret < 0)
		return ret;

    ret = cps1xxx_enable_counters(sw, port);
    if (ret) {
        RIOCP_ERROR("Failed to enable counters on port %d\n", port);
        return ret;
    }

	/* lock uninitialized ports */
	if (!(status & CPS1xxx_ERR_STATUS_PORT_OK)) {
		result |= CPS1xxx_CTL_PORT_LOCKOUT | CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN;
		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), result);
	} else {
		result |= CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN;
		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), result);
	}

	return ret;
}

/**
 * Returns first lane with given port number and port mode
 * @param port: RIO port as referred to in IDT switch specification
 * @param mode: any of CTL_INIT_PORT_WIDTH_*
 */
#ifdef CONFIG_SETUP_CACHE_ENABLE
static int cps1xxx_port_get_first_lane(struct riocp_pe *sw,
		uint8_t port, uint8_t *lane)
{
	struct switch_priv_t *priv = (struct switch_priv_t*)sw->private_driver_data;
	*lane = priv->ports[port].first_lane;
	return 0;
}
#else
static int cps1xxx_port_get_first_lane(struct riocp_pe *sw,
		uint8_t port, uint8_t *lane)
{
	uint8_t _lane = 0;
	uint32_t ctl;
	int ret, map;

	switch (RIOCP_PE_DID(sw->cap)) {
	case RIO_DID_IDT_SPS1616:
	case RIO_DID_IDT_CPS1616: /* fall through */
		if (port <= 15)
			_lane = port;
		else
			return -EINVAL;
	break;
	case RIO_DID_IDT_CPS1848:
	case RIO_DID_IDT_CPS1432:

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &ctl);
		if (ret < 0)
			return ret;

		RIOCP_DEBUG("[0x%08x:%s:hc %u] Quad cfg 0x%08x for port %u\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, ctl, port);

		for (map=0;map<gen2_portmaps_len;map++) {
			if (gen2_portmaps[map].did == RIOCP_PE_DID(sw->cap) && gen2_portmaps[map].port == port) {
				if (gen2_portmaps[map].cfg == ((ctl >> (gen2_portmaps[map].quad * gen2_portmaps[map].quad_shift)) & 3)) {
					_lane = gen2_portmaps[map].lane0;
					goto found;
				}
			}
		}
		/* no valid lane found for that given port according to bootstrap configuration */
		return -ENOTSUP;
		break;
	default:
		RIOCP_ERROR("Unable to get first lane for DID 0x%04x\n",
			RIOCP_PE_DID(sw->cap));
		return -ENOSYS;
	}

found:
	RIOCP_DEBUG("[0x%08x:%s:hc %u] Port %u, lane %u\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port, _lane);
	*lane = _lane;

	return 0;
}
#endif

#if 0
static int cps10q_get_lane_speed(struct riocp_pe *sw, uint8_t port, uint8_t lane)
{
	int ret;
	uint32_t ctl;

	ret = riocp_pe_maint_read(sw, CPS10Q_QUAD_X_CTRL(lane / 4), &ctl);
	if (ret < 0)
		return ret;

	/* consider redundancy lane (2/3) settings */
	if (lane & 2 && ctl & CPS10Q_QUAD_X_CTRL_LANE23_CTRL_EN)
		ctl = (ctl & CPS10Q_QUAD_X_CTRL_LANE23_SPEEDSEL)
			>> CPS10Q_QUAD_X_CTRL_LANE23_SPEEDSEL_SHIFT;
	else
		ctl &= CPS10Q_QUAD_X_CTRL_SPEEDSEL;

	if (ctl == CPS1xxx_RTX_RATE_0)
		return CPS1xxx_LANE_SPEED_1_25;
	else if (ctl == CPS1xxx_RTX_RATE_1)
		return CPS1xxx_LANE_SPEED_2_5;
	else if (ctl == CPS1xxx_RTX_RATE_2)
		return CPS1xxx_LANE_SPEED_3_125;
	else
		return -EXDEV;
}
#endif

int cps1xxx_set_route_entry(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint16_t value)
{
	int ret;
#ifdef CONFIG_IDTGEN2_DIRECT_ROUTING
	uint32_t off_dev, off_dm;
#else
	uint32_t val;
#endif
	if (value == RIOCP_PE_NO_ROUTE)
		value = CPS1xxx_NO_ROUTE;
	else if (value == RIOCP_PE_DEFAULT_ROUTE)
		value = CPS1xxx_DEFAULT_ROUTE;

#ifdef CONFIG_IDTGEN2_DIRECT_ROUTING
	if (RIOCP_PE_IS_MULTICAST_MASK(value))
		value = CPS1xxx_MCAST_MASK_FIRST + RIOCP_PE_GET_MULTICAST_MASK(value);

	if (lut == RIOCP_PE_ANY_PORT) {
		off_dm = CPS1xxx_RTE_BCAST_DM(0x0ff&(destid>>8));
		off_dev = CPS1xxx_RTE_BCAST_DEV(0x0ff&destid);
	} else if (lut < RIOCP_PE_PORT_COUNT(sw->cap)) {
		off_dm = CPS1xxx_RTE_PORT_DM(lut, 0x0ff&(destid>>8));
		off_dev = CPS1xxx_RTE_PORT_DEV(lut, 0x0ff&destid);
	} else {
		return -EINVAL;
	}

	if ((destid & 0xff00) == 0 || (destid & 0xff00) == (sw->destid & 0xff00)) {
		ret = riocp_pe_maint_write(sw, off_dev, value);
		if (ret < 0)
			return ret;
	} else if ((destid & 0x00ff) == 0) {
		ret = riocp_pe_maint_write(sw, off_dm, value);
		if (ret < 0)
			return ret;
	} else {
		ret = riocp_pe_maint_write(sw, off_dm, (value == CPS1xxx_NO_ROUTE)?(CPS1xxx_NO_ROUTE):(CPS1xxx_RTE_DM_TO_DEV));
		if (ret < 0)
			return ret;

		ret = riocp_pe_maint_write(sw, off_dev, value);
		if (ret < 0)
			return ret;
	}
#else
	/* Select routing table to update */
	if (lut == RIOCP_PE_ANY_PORT)
		lut = 0;
	else
		lut++;

	/* Multicast route */
	if (RIOCP_PE_IS_MULTICAST_MASK(value)) {
		value = RIOCP_PE_GET_MULTICAST_MASK(value);

		ret = riocp_pe_maint_write(sw, CPS1xxx_MCAST_RTE_SEL, lut);
		if (ret < 0)
			return ret;

		/* Associate destid with the mcast mask index */
		ret = riocp_pe_maint_write(sw, RIO_STD_MC_ASSOCIATE_SEL_CSR,
				(destid << 16) | value);
		if (ret < 0)
			return ret;

		/* Add association */
		return riocp_pe_maint_write(sw, RIO_STD_MC_ASSOCIATE_OPS_CSR,
				RIO_STD_MC_DESTID_ASSOC_CMD_ADD_ASSOC);
	}

	ret = riocp_pe_maint_write(sw, CPS1xxx_RTE_PORT_SEL, lut);
	if (ret < 0)
		return ret;

	/*
	 * Program destination port for the specified destID
	 */
	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR,
		destid);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, value);
	if (ret < 0)
		return ret;

	/* Wait for entry to be commited, this is mandatory because otherwise
		the host will enumerate the same device multiple times.
		use a read to make sure entry is commited. */
	ret = riocp_pe_maint_read(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, &val);
	if (ret < 0)
		return ret;
#endif
	return ret;
}

int cps1xxx_get_route_entry(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint16_t *value)
{
	int ret;
	uint32_t _port;
#ifdef CONFIG_IDTGEN2_DIRECT_ROUTING
	uint32_t off_dev, off_dm;

	if (lut == RIOCP_PE_ANY_PORT) {
		off_dm = CPS1xxx_RTE_BCAST_DM(0x0ff&(destid>>8));
		off_dev = CPS1xxx_RTE_BCAST_DEV(0x0ff&destid);
	} else if (lut < RIOCP_PE_PORT_COUNT(sw->cap)) {
		off_dm = CPS1xxx_RTE_PORT_DM(lut, 0x0ff&(destid>>8));
		off_dev = CPS1xxx_RTE_PORT_DEV(lut, 0x0ff&destid);
	} else {
		return -EINVAL;
	}

	if ((destid && 0xff00) != 0 && (destid & 0xff00) != (sw->destid & 0xff00)) {
		ret = riocp_pe_maint_read(sw, off_dm, &_port);
		if (ret < 0)
			return ret;
	} else {
		_port = CPS1xxx_RTE_DM_TO_DEV;
	}

	if (_port == CPS1xxx_RTE_DM_TO_DEV) {
		ret = riocp_pe_maint_read(sw, off_dev, &_port);
		if (ret < 0)
			return ret;
	}

#else

	/* Select routing table to read */
	if (lut == RIOCP_PE_ANY_PORT)
		lut = 0;
	else
		lut++;

	ret = riocp_pe_maint_write(sw, CPS1xxx_RTE_PORT_SEL, lut);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, destid);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_read(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, &_port);
	if (ret < 0)
		return ret;

#endif
	_port &= CPS1xxx_RTE_PORT_CSR_PORT;

	if (_port >= CPS1xxx_MCAST_MASK_FIRST && _port <= CPS1xxx_MCAST_MASK_LAST)
		_port = RIOCP_PE_MULTICAST_MASK(_port - CPS1xxx_MCAST_MASK_FIRST);
	else if (_port == CPS1xxx_DEFAULT_ROUTE)
		_port = RIOCP_PE_DEFAULT_ROUTE;
	else if (_port == CPS1xxx_NO_ROUTE)
		_port = RIOCP_PE_NO_ROUTE;
	else if (!RIOCP_PE_IS_EGRESS_PORT(_port))
		return -EINVAL;

	*value = _port;

	return ret;
}

int cps1xxx_get_port_route(struct riocp_pe *sw, uint8_t lut, uint8_t port, uint32_t *destid)
{
	int ret;
	uint32_t _destid;

	/* Select routing table to read */
	if (lut == RIOCP_PE_ANY_PORT)
		lut = 0;
	else
		lut++;

	ret = riocp_pe_maint_write(sw, CPS1xxx_RTE_PORT_SEL, lut);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, port);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_read(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, &_destid);
	if (ret < 0)
		return ret;

	*destid = _destid;

	return ret;
}

int cps1xxx_clear_lut(struct riocp_pe *sw, uint8_t lut)
{
	uint32_t i;
	uint32_t val;
	int ret;

	/* Select routing table */
	if (lut == RIOCP_PE_ANY_PORT)
		lut = 0;
	else
		lut++;

	RIOCP_TRACE("[%s] Clear lut %u\n", RIOCP_SW_DRV_NAME(sw), lut);

	ret = riocp_pe_maint_write(sw, CPS1xxx_RTE_PORT_SEL, lut);
	if (ret < 0)
		return ret;

	/* clear lut table */
	for (i = RIO_STD_RTE_CONF_EXTCFGEN;
		i <= (RIO_STD_RTE_CONF_EXTCFGEN | 0xff); i += 4) {

		ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, i);
		if (ret < 0)
			return ret;

		val = (CPS1xxx_NO_ROUTE << 24) | (CPS1xxx_NO_ROUTE << 16) |
			(CPS1xxx_NO_ROUTE << 8) | CPS1xxx_NO_ROUTE;

		ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, val);
		if (ret < 0)
			return ret;
	}

	RIOCP_TRACE("[%s] Clear lut %u done", sw->sw->name, lut);

	return 0;
}

int cps1xxx_set_domain(struct riocp_pe *sw, uint8_t domain)
{
	return riocp_pe_maint_write(sw, CPS1xxx_RTE_RIO_DOMAIN, domain);
}

/* split capabilities into
    - those that are fixed by specification (known at build time)
    - those that are configured (known at run time)
*/

#define READ_CNTR(CNTR_REG) \
    int cps1xxx_read_##CNTR_REG(struct riocp_pe *sw, uint8_t port, uint32_t *counter) \
    { \
        uint32_t reg_addr = CNTR_REG+0x100*port; \
        return riocp_pe_maint_read(sw, reg_addr, counter); \
    } \

READ_CNTR(CPS1xxx_PORT_X_VC0_PA_TX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_NACK_TX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_RTRY_TX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_PKT_TX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_PA_RX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_NACK_RX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_RTRY_RX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_PKT_RX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_CPB_TX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_PKT_DROP_RX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_PKT_DROP_TX_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_TTL_DROP_CNTR)
READ_CNTR(CPS1xxx_PORT_X_VC0_CRC_LIMIT_DROP_CNTR)
READ_CNTR(CPS1xxx_PORT_X_TRC_MATCH_0)
READ_CNTR(CPS1xxx_PORT_X_TRC_MATCH_1)
READ_CNTR(CPS1xxx_PORT_X_TRC_MATCH_2)
READ_CNTR(CPS1xxx_PORT_X_TRC_MATCH_3)
READ_CNTR(CPS1xxx_PORT_X_FIL_MATCH_0)
READ_CNTR(CPS1xxx_PORT_X_FIL_MATCH_1)
READ_CNTR(CPS1xxx_PORT_X_FIL_MATCH_2)
READ_CNTR(CPS1xxx_PORT_X_FIL_MATCH_3)


/* This function needs to know the switch's counter capabilities.
 * Therefore it has to assure that all capabilities which are defined in the
 * general riocp_pe API and that do not apply for this switch are set to NULL.
 */
static int cps1xxx_get_capabilities(struct riocp_pe *sw, uint8_t port,
		counter_caps_t *counter_caps)
{
    int ret, i;
    uint8_t width;

    counter_caps->read_reg[PORT_X_VC0_PA_TX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_PA_TX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_NACK_TX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_NACK_TX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_RTRY_TX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_RTRY_TX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_PKT_TX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_PKT_TX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_PA_RX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_PA_RX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_NACK_RX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_NACK_RX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_RTRY_RX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_RTRY_RX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_PKT_RX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_PKT_RX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_CPB_TX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_CPB_TX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_PKT_DROP_RX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_PKT_DROP_RX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_PKT_DROP_TX_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_PKT_DROP_TX_CNTR;
    counter_caps->read_reg[PORT_X_VC0_TTL_DROP_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_TTL_DROP_CNTR;
    counter_caps->read_reg[PORT_X_VC0_CRC_LIMIT_DROP_CNTR] = cps1xxx_read_CPS1xxx_PORT_X_VC0_CRC_LIMIT_DROP_CNTR;
    counter_caps->read_reg[PORT_X_TRC_MATCH_0] = cps1xxx_read_CPS1xxx_PORT_X_TRC_MATCH_0;
    counter_caps->read_reg[PORT_X_TRC_MATCH_1] = cps1xxx_read_CPS1xxx_PORT_X_TRC_MATCH_1;
    counter_caps->read_reg[PORT_X_TRC_MATCH_2] = cps1xxx_read_CPS1xxx_PORT_X_TRC_MATCH_2;
    counter_caps->read_reg[PORT_X_TRC_MATCH_3] = cps1xxx_read_CPS1xxx_PORT_X_TRC_MATCH_3;
    counter_caps->read_reg[PORT_X_FIL_MATCH_0] = cps1xxx_read_CPS1xxx_PORT_X_FIL_MATCH_0;
    counter_caps->read_reg[PORT_X_FIL_MATCH_1] = cps1xxx_read_CPS1xxx_PORT_X_FIL_MATCH_1;
    counter_caps->read_reg[PORT_X_FIL_MATCH_2] = cps1xxx_read_CPS1xxx_PORT_X_FIL_MATCH_2;
    counter_caps->read_reg[PORT_X_FIL_MATCH_3] = cps1xxx_read_CPS1xxx_PORT_X_FIL_MATCH_3;

    // preinitialize with dedicated functions to read counters
    counter_caps->read_reg[PORT_X_LANE_0_ERR_8B10B] = cps1xxx_get_port_err8b10b_lane_0;
    counter_caps->read_reg[PORT_X_LANE_1_ERR_8B10B] = cps1xxx_get_port_err8b10b_lane_1;
    counter_caps->read_reg[PORT_X_LANE_2_ERR_8B10B] = cps1xxx_get_port_err8b10b_lane_2;
    counter_caps->read_reg[PORT_X_LANE_3_ERR_8B10B] = cps1xxx_get_port_err8b10b_lane_3;

    // clear function pointers for unassigned lanes
    ret = cps1xxx_get_lane_width(sw, port, &width);
    for (i=width; i<4; ++i) {
        counter_caps->read_reg[PORT_X_LANE_0_ERR_8B10B+i] = NULL;
    }

    return ret;
}

int cps1xxx_get_counters(struct riocp_pe *sw, uint8_t port, counter_regs_t *counter_regs,
		counter_caps_t *counter_caps)
{
    uint32_t cap, ret;
    uint32_t reg_val;

    for (cap=0; cap<LAST_CAPABILITY; ++cap) {
    	// check the counter capability: is there a read function for this counter register available?
        if (counter_caps.read_reg[cap] == NULL) {
        	counter_regs->val[cap] = 0;
            continue;
        }

        // read the counter value from its register
        if ((ret = counter_caps->read_reg[cap](sw, port, &reg_val)))
        	return ret;
        counter_regs->val[cap] = reg_val;
    }

    return 0;
}

int cps1xxx_set_multicast_mask(struct riocp_pe *sw, uint8_t lut, uint8_t maskid, uint16_t port_mask, bool clear)
{
#ifdef CONFIG_IDTGEN2_DIRECT_ROUTING
	uint32_t off, val;
	int ret;

	if (maskid > CPS1xxx_MCAST_MASK_LAST - CPS1xxx_MCAST_MASK_FIRST)
		return -EINVAL;

	if (lut == RIOCP_PE_ANY_PORT)
		off = CPS1xxx_BCAST_MCAST_MASK(maskid);
	else if (lut < RIOCP_PE_PORT_COUNT(sw->cap))
		off = CPS1xxx_PORT_MCAST_MASK(lut, maskid);
	else
		return -EINVAL;

	ret = riocp_pe_maint_read(sw, off, &val);
	if (ret < 0)
		return ret;

	if (clear)
		val = val & ~port_mask;
	else
		val = val | port_mask;

	ret = riocp_pe_maint_write(sw, off, val);
	if (ret < 0)
		return ret;
#else
	uint32_t val;
	int ret, i;

	if (maskid > CPS1xxx_MCAST_MASK_LAST - CPS1xxx_MCAST_MASK_FIRST)
		return -EINVAL;

	/* Add/delete ports as indicated by port_mask */
	for (i = 0; i < sw->sw->port_count; i++) {
		val = (maskid << 16) | (i << 8);

		if (port_mask & (1 << i)) {
			if (clear)
				val |= RIO_STD_MC_MASK_CFG_MASK_CMD_DEL_PORT;
			else
				val |= RIO_STD_MC_MASK_CFG_MASK_CMD_ADD_PORT;
		}

		ret = riocp_pe_main_write(sw, RIO_STD_MC_MASK_PORT_CSR, val);
		if (ret < 0)
			return ret;
	}

#endif
	return 0;
}

static int cps1xxx_set_self_mcast(struct riocp_pe *sw, uint8_t port, bool state)
{
    uint32_t result;
    int ret;

    /* enable port-writes and interrupts for port events */
    ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &result);
    if (ret < 0)
        return ret;

    if (state)
	    result |= CPS1xxx_OPS_SELF_MCAST_EN;
    else
	    result &= ~CPS1xxx_OPS_SELF_MCAST_EN;

    ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), result);
    if (ret < 0)
        return ret;

    return 0;
}

/*
 * set the retry limit per port
 */
int cps1xxx_set_retry_limit(struct riocp_pe *sw, uint8_t port, uint16_t limit)
{
	int ret;
	uint32_t port_stat_ctrl, port_impl_err_det;
	struct switch_priv_t *priv = (struct switch_priv_t *)sw->private_driver_data;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &port_impl_err_det);
	if (ret < 0)
		return ret;

	port_impl_err_det &= ~CPS1xxx_IMPL_SPEC_ERR_DET_MANY_RETRY;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), port_impl_err_det);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_STAT_CTRL(port), &port_stat_ctrl);
	if (ret < 0) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading port %d status and control register\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
		return ret;
	}

	port_stat_ctrl |= CPS1xxx_PORT_STAT_CTL_CLR_MANY_RETRY;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_STAT_CTRL(port), port_stat_ctrl);
	if (ret < 0) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] Disable port %d retry limit failed.\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
		return ret;
	}

	if(!limit) {
		/* disable the retry limit for that port */

		port_stat_ctrl &= ~CPS1xxx_PORT_STAT_CTL_RETRY_LIM_EN;
	} else {
		/* enable the retry limit for that port */

		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_RETRY_CNTR(port), CPS1xxx_PORT_RETRY_CNTR_LIM(limit));
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error write port %d retry limit value %u failed.\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port, limit);
			return ret;
		}

		priv->ports[port].retry_lim = limit;

		port_stat_ctrl |= CPS1xxx_PORT_STAT_CTL_RETRY_LIM_EN;
	}

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_STAT_CTRL(port), port_stat_ctrl);
	if (ret < 0) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] Enable port %d retry limit failed.\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
		return ret;
	}

	return 0;
}

/*
 * Get the trace and filter capabilities
 */
int cps1xxx_get_trace_filter_capabilities(struct riocp_pe *sw, struct riocp_pe_trace_filter_caps *caps)
{
	caps->match_unit_count = CPS1xxx_TRACE_FILTER_UNITS;
	caps->match_unit_words = CPS1xxx_TRACE_FILTER_WORDS;
	caps->filter_caps = RIOCP_PE_TRACE_FILTER_FORWARD | RIOCP_PE_TRACE_FILTER_DROP;
	caps->port_caps = RIOCP_PE_TRACE_PORT_EXCLUSIVE;

	(void)sw;
	return 0;
}

/*
 * set a trace filter per port
 */
int cps1xxx_set_trace_filter(struct riocp_pe *sw, uint8_t port, uint8_t filter, uint32_t flags, uint32_t *val, uint32_t *mask)
{
	int ret, word;
	uint32_t port_ops;

	if(flags & RIOCP_PE_TRACE_FILTER_NOTIFY) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] port %d RIOCP_PE_TRACE_FILTER_NOTIFY not supported.\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
		return -ENOSYS;
	}

	if(filter >= CPS1xxx_TRACE_FILTER_UNITS) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] port %d filter number %d exceeds limit of %d.\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port, filter, CPS1xxx_TRACE_FILTER_UNITS);
		return -EINVAL;
	}

	if(flags) {
		if (port == RIOCP_PE_ANY_PORT) {
			for(word=0;word<CPS1xxx_TRACE_FILTER_WORDS;word++) {
				if (val) {
					ret = riocp_pe_maint_write(sw, CPS1xxx_TRACE_FILTER_VALUE_BC(filter, word), val[word]);
					if (ret < 0)
						return ret;
				}
				if (mask) {
					ret = riocp_pe_maint_write(sw, CPS1xxx_TRACE_FILTER_MASK_BC(filter, word), mask[word]);
					if (ret < 0)
						return ret;
				}
			}
		} else {
			for(word=0;word<CPS1xxx_TRACE_FILTER_WORDS;word++) {
				if (val) {
					ret = riocp_pe_maint_write(sw, CPS1xxx_TRACE_FILTER_VALUE(port, filter, word), val[word]);
					if (ret < 0)
						return ret;
				}
				if (mask) {
					ret = riocp_pe_maint_write(sw, CPS1xxx_TRACE_FILTER_MASK(port, filter, word), mask[word]);
					if (ret < 0)
						return ret;
				}
			}
		}
	}

	if (port == RIOCP_PE_ANY_PORT)
		goto outhere;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &port_ops);
	if (ret < 0)
		return ret;

	if(flags & RIOCP_PE_TRACE_FILTER_FORWARD)
		port_ops |= (CPS1xxx_OPS_TRACE_0_EN << filter);
	else
		port_ops &= ~(CPS1xxx_OPS_TRACE_0_EN << filter);

	if(flags & RIOCP_PE_TRACE_FILTER_DROP)
		port_ops |= (CPS1xxx_OPS_FILTER_0_EN << filter);
	else
		port_ops &= ~(CPS1xxx_OPS_FILTER_0_EN << filter);

	port_ops &= ~CPS1xxx_OPS_TRACE_PW_EN;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), port_ops);
	if (ret < 0)
		return ret;
outhere:
	return 0;
}

/*
 * set the trace port
 */
int cps1xxx_set_trace_port(struct riocp_pe *sw, uint8_t port, uint32_t flags)
{
	int ret;
	uint32_t dev_ctrl_1;

	ret = riocp_pe_maint_read(sw, CPS1xxx_DEVICE_CTL_1, &dev_ctrl_1);
	if (ret < 0)
		return ret;

	/* disable trace */
	dev_ctrl_1 &= ~CPS1xxx_DEVICE_CTL_TRACE_EN;

	ret = riocp_pe_maint_write(sw, CPS1xxx_DEVICE_CTL_1, dev_ctrl_1);
	if (ret < 0)
		return ret;

	/* change port */
	dev_ctrl_1 &= ~CPS1xxx_DEVICE_CTL_TRACE_PORT_MASK;
	dev_ctrl_1 |= CPS1xxx_DEVICE_CTL_TRACE_PORT(port);

	/* update flags */
	if (flags & RIOCP_PE_TRACE_PORT_EXCLUSIVE)
		dev_ctrl_1 |= CPS1xxx_DEVICE_CTL_TRACE_PORT_MODE;
	else
		dev_ctrl_1 &= ~CPS1xxx_DEVICE_CTL_TRACE_PORT_MODE;

	/* enable trace */
	dev_ctrl_1 |= CPS1xxx_DEVICE_CTL_TRACE_EN;

	ret = riocp_pe_maint_write(sw, CPS1xxx_DEVICE_CTL_1, dev_ctrl_1);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * Get lane speed of port
 */
#ifdef CONFIG_SETUP_CACHE_ENABLE
int cps1xxx_get_lane_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed *speed)
{
	int ret;
	uint8_t lane = 0;
	struct switch_priv_t *priv = (struct switch_priv_t*)sw->private_driver_data;

	RIOCP_TRACE("[0x%08x:%s:hc %u] Read port %u\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);

	ret = cps1xxx_port_get_first_lane(sw, port, &lane);
	if (ret < 0) {
		if (ret != -ENOTSUP) {
			RIOCP_ERROR("Could net get first lane of port %u (ret = %d, %s)\n",
					port, ret, strerror(-ret));
		}
		return ret;
	}

	*speed = priv->lanes[lane].speed;
	RIOCP_TRACE("Port %u lane %u speed: %u\n", port, lane, priv->lanes[lane].speed);
	return ret;
}

int __cps1xxx_get_lane_speed(struct riocp_pe *sw, uint8_t lane, enum riocp_pe_speed *speed)
{
	int ret;
	uint32_t ctl;
	uint32_t tx_rate, rx_rate;
	uint32_t pll_div;
	enum riocp_pe_speed _speed;

	RIOCP_TRACE("[0x%08x:%s:hc %u] Read lane %u\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, lane);

	switch (RIOCP_PE_DID(sw->cap)) {
	case RIO_DID_IDT_SPS1616:
	case RIO_DID_IDT_CPS1616:
		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &ctl);
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading lane x ctl\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
			return ret;
		}
		pll_div = (ctl & CPS1xxx_LANE_CTL_PLL_SEL) >> CPS1xxx_LANE_CTL_PLL_SEL_SHIFT;
		break;
	case RIO_DID_IDT_CPS1848:
	case RIO_DID_IDT_CPS1432: {
		uint8_t _pll = lane / 4;

		ret = riocp_pe_maint_read(sw, CPS1xxx_PLL_X_CTL_1(_pll), &ctl);
		if (ret < 0)
			return ret;

		RIOCP_DEBUG("[0x%08x:%s:hc %u] PLL_CTL[%u]:0x%08x\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
			_pll, ctl);

		pll_div = ctl & CPS1xxx_PLL_X_CTL_PLL_DIV_SEL;

		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &ctl);
		if (ret < 0)
			return ret;

		break;
	}
	default:
		return -ENOSYS;
	}

	RIOCP_DEBUG("[0x%08x:%s:hc %u] LANE_CTL[%u]:0x%08x\n",
		sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
		lane, ctl);

	/* sanity check: tx/rx rate must be equal */
	rx_rate = (ctl & CPS1xxx_LANE_CTL_RX_RATE) >> CPS1xxx_LANE_CTL_RX_RATE_SHIFT;
	tx_rate = (ctl & CPS1xxx_LANE_CTL_TX_RATE) >> CPS1xxx_LANE_CTL_TX_RATE_SHIFT;
	if (rx_rate != tx_rate) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] rx and tx rate are not equal\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
		return -EIO;
	}

	if (rx_rate == CPS1xxx_RTX_RATE_0)
		_speed = pll_div ? RIOCP_SPEED_UNKNOWN : RIOCP_SPEED_1_25G;
	else if (rx_rate == CPS1xxx_RTX_RATE_1)
		_speed = pll_div ? RIOCP_SPEED_3_125G : RIOCP_SPEED_2_5G;
	else if (rx_rate == CPS1xxx_RTX_RATE_2 || rx_rate == CPS1xxx_RTX_RATE_3)
		_speed = pll_div ? RIOCP_SPEED_6_25G : RIOCP_SPEED_5_0G;
	else
		return -EIO;

	*speed = _speed;
	RIOCP_TRACE("Lane %u speed: %u\n", lane, _speed);
	return ret;
}
#else
int cps1xxx_get_lane_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed *speed)
{
	int ret;
	uint32_t ctl;
	uint32_t tx_rate, rx_rate;
	uint32_t pll_div;
	enum riocp_pe_speed _speed;
	uint8_t lane = 0;

	RIOCP_TRACE("[0x%08x:%s:hc %u] Read port %u\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);

	ret = cps1xxx_port_get_first_lane(sw, port, &lane);
	if (ret < 0) {
		if (ret != -ENOTSUP) {
			RIOCP_ERROR("Could net get first lane of port %u (ret = %d, %s)\n",
					port, ret, strerror(-ret));
		}
		return ret;
	}

	switch (RIOCP_PE_DID(sw->cap)) {
	case RIO_DID_IDT_SPS1616:
	case RIO_DID_IDT_CPS1616:
		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &ctl);
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading lane x ctl\n",
				sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
			return ret;
		}
		pll_div = (ctl & CPS1xxx_LANE_CTL_PLL_SEL) >> CPS1xxx_LANE_CTL_PLL_SEL_SHIFT;
		break;
	case RIO_DID_IDT_CPS1848:
	case RIO_DID_IDT_CPS1432: {
		int map;
		uint8_t _pll = 255;

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &ctl);
		if (ret < 0)
			return ret;

		for (map=0;map<gen2_portmaps_len;map++) {
			if (gen2_portmaps[map].did == RIOCP_PE_DID(sw->cap) && gen2_portmaps[map].port == port) {
				if (gen2_portmaps[map].cfg == ((ctl >> (gen2_portmaps[map].quad * gen2_portmaps[map].quad_shift)) & 3)) {
					_pll = gen2_portmaps[map].pll;
					RIOCP_DEBUG("[0x%08x:%s:hc %u] DID:0x%04x Q:%u C:%u P:%u L0:%u W:%u PLL:%u\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
						gen2_portmaps[map].did, gen2_portmaps[map].quad, gen2_portmaps[map].cfg,
						gen2_portmaps[map].port, gen2_portmaps[map].lane0,
						gen2_portmaps[map].width, gen2_portmaps[map].pll);
					break;
				}
			}
		}

		if (_pll == 255)
			return -ENOSYS;

		ret = riocp_pe_maint_read(sw, CPS1xxx_PLL_X_CTL_1(_pll), &ctl);
		if (ret < 0)
			return ret;

		RIOCP_DEBUG("[0x%08x:%s:hc %u] PLL_CTL[%u]:0x%08x\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
			_pll, ctl);

		pll_div = ctl & CPS1xxx_PLL_X_CTL_PLL_DIV_SEL;

		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &ctl);
		if (ret < 0)
			return ret;

		RIOCP_DEBUG("[0x%08x:%s:hc %u] LANE_CTL[%u]:0x%08x\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
			lane, ctl);

		break;
	}
	default:
		return -ENOSYS;
	}

	/* sanity check: tx/rx rate must be equal */
	rx_rate = (ctl & CPS1xxx_LANE_CTL_RX_RATE) >> CPS1xxx_LANE_CTL_RX_RATE_SHIFT;
	tx_rate = (ctl & CPS1xxx_LANE_CTL_TX_RATE) >> CPS1xxx_LANE_CTL_TX_RATE_SHIFT;
	if (rx_rate != tx_rate) {
		RIOCP_ERROR("[0x%08x:%s:hc %u] rx and tx rate are not equal\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
		return -EIO;
	}

	if (rx_rate == CPS1xxx_RTX_RATE_0)
		_speed = pll_div ? RIOCP_SPEED_UNKNOWN : RIOCP_SPEED_1_25G;
	else if (rx_rate == CPS1xxx_RTX_RATE_1)
		_speed = pll_div ? RIOCP_SPEED_3_125G : RIOCP_SPEED_2_5G;
	else if (rx_rate == CPS1xxx_RTX_RATE_2 || rx_rate == CPS1xxx_RTX_RATE_3)
		_speed = pll_div ? RIOCP_SPEED_6_25G : RIOCP_SPEED_5_0G;
	else
		return -EIO;

	*speed = _speed;
	RIOCP_TRACE("Port %u lane %u speed: %u\n", port, lane, _speed);
	return ret;
}
#endif

/**
 * Get number of lanes per port
 */
#ifdef CONFIG_SETUP_CACHE_ENABLE
int cps1xxx_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width)
{
	struct switch_priv_t *priv = (struct switch_priv_t*)sw->private_driver_data;
	*width = priv->ports[port].width;
	return 0;
}

static int __cps1xxx_get_lane_width_and_lane0(struct riocp_pe *sw, uint8_t port, uint8_t *width, uint8_t *lane0)
{
	int ret;
	uint32_t ctl;
	uint8_t _width = 0, map, _lane0 = 0;

	switch (RIOCP_PE_DID(sw->cap)) {
	case RIO_DID_IDT_SPS1616:
	case RIO_DID_IDT_CPS1616: {
		int quad = port / 4;
		int port_in_quad = port - (quad*4);

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &ctl);
		if (ret < 0)
			return ret;

		ctl >>= (quad * 4);
		ctl &= 3;

		switch (ctl) {
		case 0:
			/* 1 4x */
			if (port_in_quad == 0)
				_width = 4;
			break;
		case 1:
			/* 2 2x */
			if (port_in_quad == 0 || port_in_quad == 2)
				_width = 2;
			break;
		case 2:
			/* 1 2x, 2 1x */
			if (port_in_quad == 0 || port_in_quad == 1)
				_width = 1;
			else if (port_in_quad == 2)
				_width = 2;
			break;
		case 3:
			/* 4 1x */
			_width = 1;
			break;
		default:
			break;
		}
		_lane0 = port;
		break;
	}
	case RIO_DID_IDT_CPS1432:
	case RIO_DID_IDT_CPS1848:

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &ctl);
		if (ret < 0)
			return ret;

		for (map=0;map<gen2_portmaps_len;map++) {
			if (gen2_portmaps[map].did == RIOCP_PE_DID(sw->cap) && gen2_portmaps[map].port == port) {
				if (gen2_portmaps[map].cfg == ((ctl >> (gen2_portmaps[map].quad * gen2_portmaps[map].quad_shift)) & 3)) {
					_width = gen2_portmaps[map].width;
					_lane0 = gen2_portmaps[map].lane0;
					goto found;
				}
			}
		}

		break;
	default:
#if 0
		/*
		 * FIXME: This regsiter contains only valid port width values when a port_ok is detected
		 * but needs also supported by ports that have no link.
		 */
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &ctl);
		if (ret < 0)
			return ret;

		port_cfg = CPS1xxx_CTL_INIT_PORT_WIDTH(ctl);

		if (port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X4)
			_width = 4;
		else if (port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X2)
			_width = 2;
		else if (port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X1_L0 ||
				port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X1_L2)
			_width = 1;
		break;
#else
		return -ENOSYS;
#endif
	}
found:
	*width = _width;
	*lane0 = _lane0;

	RIOCP_TRACE("ctl(0x%08x): 0x%08x, lane width: %u\n",
		CPS1xxx_PORT_X_CTL_1_CSR(port), ctl, _width);

	return 0;
}
#else
int cps1xxx_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width)
{
	int ret;
	uint32_t quad_cfg;
	uint32_t port_cfg;
	uint8_t _width = 0, map;

	switch (RIOCP_PE_DID(sw->cap)) {
	case RIO_DID_IDT_SPS1616:
	case RIO_DID_IDT_CPS1616: {
		int quad = port / 4;
		int port_in_quad = port - (quad*4);

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &quad_cfg);
		if (ret < 0)
			return ret;

		quad_cfg >>= (quad * 4);
		quad_cfg &= 3;

		switch (quad_cfg) {
		case 0:
			/* 1 4x */
			if (port_in_quad == 0)
				_width = 4;
			break;
		case 1:
			/* 2 2x */
			if (port_in_quad == 0 || port_in_quad == 2)
				_width = 2;
			break;
		case 2:
			/* 1 2x, 2 1x */
			if (port_in_quad == 0 || port_in_quad == 1)
				_width = 1;
			else if (port_in_quad == 2)
				_width = 2;
			break;
		case 3:
			/* 4 1x */
			_width = 1;
			break;
		default:
			break;
		}
		break;
	}
	case RIO_DID_IDT_CPS1432:
	case RIO_DID_IDT_CPS1848:

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &quad_cfg);
		if (ret < 0)
			return ret;

		for (map=0;map<gen2_portmaps_len;map++) {
			if (gen2_portmaps[map].did == RIOCP_PE_DID(sw->cap) && gen2_portmaps[map].port == port) {
				if (gen2_portmaps[map].cfg == ((quad_cfg >> (gen2_portmaps[map].quad * gen2_portmaps[map].quad_shift)) & 3)) {
					_width = gen2_portmaps[map].width;
					goto found;
				}
			}
		}

		break;
	default:
		/*
		 * FIXME: This regsiter contains only valid port width values when a port_ok is detected
		 * but needs also supported by ports that have no link.
		 */
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &quad_cfg);
		if (ret < 0)
			return ret;

		port_cfg = CPS1xxx_CTL_INIT_PORT_WIDTH(quad_cfg);

		if (port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X4)
			_width = 4;
		else if (port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X2)
			_width = 2;
		else if (port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X1_L0 ||
				port_cfg == CPS1xxx_CTL_INIT_PORT_WIDTH_X1_L2)
			_width = 1;
		break;
	}
found:
	*width = _width;

	RIOCP_TRACE("ctl(0x%08x): 0x%08x, lane width: %u\n",
		CPS1xxx_PORT_X_CTL_1_CSR(port), quad_cfg, _width);

	return 0;
}
#endif


static int cps1xxx_get_port_supported_speeds(struct riocp_pe *sw, uint8_t port, uint8_t *speeds)
{
#ifdef CONFIG_SETUP_CACHE_ENABLE
	struct switch_priv_t *priv = (struct switch_priv_t *)sw->private_driver_data;
#endif
	uint32_t val;
	uint8_t lane;
	int ret;

	ret = cps1xxx_port_get_first_lane(sw, port, &lane);
	if (ret < 0) {
		RIOCP_ERROR("Could not get first lane of port %u (ret = %d, %s)\n",
				port, ret, strerror(-ret));
		return ret;
	}

#ifdef CONFIG_SETUP_CACHE_ENABLE
	if (priv->lanes[lane].supported_speeds & CPS1xxx_SUPPORTED_SPEEDS_CACHED) {
		*speeds = priv->lanes[lane].supported_speeds & CPS1xxx_SUPPORTED_SPEEDS_MASK;
		return 0;
	}
#endif

	ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_STAT_0_CSR(lane), &val);
	if (ret < 0)
		return ret;

	/* Check if LANE_STATUS_3 register is implemented or not */
	if (!CPS1xxx_LANE_STAT_0_STAT_N_IMPL(val, 3)) {
		*speeds = RIOCP_SUPPORTED_SPEED_UNKNOWN;
		goto out_update_cache;
	}

	ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_STAT_3_CSR(lane), &val);
	if (ret < 0)
		return ret;


	*speeds = (val >> CPS1xxx_LANE_STAT_3_GBAUD_BITS) & CPS1xxx_LANE_STAT_3_GBAUD_MASK;

out_update_cache:
#ifdef CONFIG_SETUP_CACHE_ENABLE
	priv->lanes[lane].supported_speeds = *speeds | CPS1xxx_SUPPORTED_SPEEDS_CACHED;
#endif

	return 0;
}

/*
 * program RX serdes data
 */
static int cps1xxx_set_rx_serdes(struct riocp_pe *sw, int lane, const struct riocp_pe_serdes_idtgen2_rx *serdes)
{
	uint32_t reg_dfe1 = 0, reg_dfe2 = 0;
	int ret;

	if(!serdes)
		return 0;

	if(serdes->dfe_tap0 == RIOCP_SERDES_NOVAL &&
	   serdes->dfe_tap1 == RIOCP_SERDES_NOVAL &&
	   serdes->dfe_tap2 == RIOCP_SERDES_NOVAL &&
	   serdes->dfe_tap3 == RIOCP_SERDES_NOVAL &&
	   serdes->dfe_tap4 == RIOCP_SERDES_NOVAL &&
	   serdes->dfe_offs == RIOCP_SERDES_NOVAL)
		return 0;

	ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_DFE2(lane), &reg_dfe2);
	if(ret < 0)
		return ret;

	reg_dfe1 |= CPS1xxx_LANE_DFE1_TAP_OFFS_SEL;
	if(serdes->dfe_offs != RIOCP_SERDES_NOVAL) {
		reg_dfe2 &= ~CPS1xxx_LANE_DFE2_TAP_OFFS_CFG_M;
		reg_dfe2 |= CPS1xxx_LANE_DFE2_TAP_OFFS_CFG(serdes->dfe_offs);
	}
	reg_dfe1 |= CPS1xxx_LANE_DFE1_TAP4_SEL;
	if(serdes->dfe_tap4 != RIOCP_SERDES_NOVAL) {
		reg_dfe2 &= ~CPS1xxx_LANE_DFE2_TAP4_CFG_M;
		reg_dfe2 |= CPS1xxx_LANE_DFE2_TAP4_CFG(serdes->dfe_tap4);
	}
	reg_dfe1 |= CPS1xxx_LANE_DFE1_TAP3_SEL;
	if(serdes->dfe_tap3 != RIOCP_SERDES_NOVAL) {
		reg_dfe2 &= ~CPS1xxx_LANE_DFE2_TAP3_CFG_M;
		reg_dfe2 |= CPS1xxx_LANE_DFE2_TAP3_CFG(serdes->dfe_tap3);
	}
	reg_dfe1 |= CPS1xxx_LANE_DFE1_TAP2_SEL;
	if(serdes->dfe_tap2 != RIOCP_SERDES_NOVAL) {
		reg_dfe2 &= ~CPS1xxx_LANE_DFE2_TAP2_CFG_M;
		reg_dfe2 |= CPS1xxx_LANE_DFE2_TAP2_CFG(serdes->dfe_tap2);
	}
	reg_dfe1 |= CPS1xxx_LANE_DFE1_TAP1_SEL;
	if(serdes->dfe_tap1 != RIOCP_SERDES_NOVAL) {
		reg_dfe2 &= ~CPS1xxx_LANE_DFE2_TAP1_CFG_M;
		reg_dfe2 |= CPS1xxx_LANE_DFE2_TAP1_CFG(serdes->dfe_tap1);
	}
	reg_dfe1 |= CPS1xxx_LANE_DFE1_TAP0_SEL;
	if(serdes->dfe_tap0 != RIOCP_SERDES_NOVAL) {
		reg_dfe2 &= ~CPS1xxx_LANE_DFE2_TAP0_CFG_M;
		reg_dfe2 |= CPS1xxx_LANE_DFE2_TAP0_CFG(serdes->dfe_tap0);
	}

	ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_DFE1(lane), reg_dfe1);
	if(ret < 0)
		return ret;
	ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_DFE2(lane), reg_dfe2);
	if(ret < 0)
		return ret;

	reg_dfe2 |= CPS1xxx_LANE_DFE2_CFG_EN;

	ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_DFE2(lane), reg_dfe2);
	if(ret < 0)
		return ret;

	return 0;
}

/*
 * program TX serdes data
 */
static int cps1xxx_set_tx_serdes(struct riocp_pe *sw, int lane, const struct riocp_pe_serdes_idtgen2_tx *serdes)
{
	uint32_t lane_status3_csr, lane_ctl;
	int ret;

	if(!serdes)
		return 0;

	ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_STAT_3_CSR(lane), &lane_status3_csr);
	if (ret < 0)
		return ret;

	if(serdes->amplitude != RIOCP_SERDES_NOVAL) {
		lane_status3_csr |= CPS1xxx_LANE_STAT_3_AMP_PROG_EN;

		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &lane_ctl);
		if (ret < 0)
			return ret;
	}
	if(serdes->pos1_tap != RIOCP_SERDES_NOVAL) {
		lane_status3_csr &= ~CPS1xxx_LANE_STAT_3_POS1_TAP_M;
		lane_status3_csr |= CPS1xxx_LANE_STAT_3_POS1_TAP(serdes->pos1_tap);
	}
	if(serdes->neg1_tap != RIOCP_SERDES_NOVAL) {
		lane_status3_csr &= ~CPS1xxx_LANE_STAT_3_NEG1_TAP_M;
		lane_status3_csr |= CPS1xxx_LANE_STAT_3_NEG1_TAP(serdes->neg1_tap);
	}

	ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_STAT_3_CSR(lane), lane_status3_csr);
	if (ret < 0)
		return ret;

	if(serdes->amplitude != RIOCP_SERDES_NOVAL) {
		lane_ctl &= ~CPS1xxx_LANE_CTL_TX_AMP_M;
		lane_ctl |= CPS1xxx_LANE_CTL_TX_AMP(serdes->amplitude);

		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(lane), lane_ctl);
		if (ret < 0)
			return ret;

		lane_status3_csr &= ~CPS1xxx_LANE_STAT_3_AMP_PROG_EN;

		ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_STAT_3_CSR(lane), lane_status3_csr);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static char *serdes_val_2_str(char *str, size_t len, int val)
{
	if(val == RIOCP_SERDES_NOVAL)
		strncpy(str, "x", len);
	else
		snprintf(str, len, "%d", val);
	return str;
}

/**
 * Set lane speed of port
 */
int cps1xxx_set_lane_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed speed, struct riocp_pe_serdes *serdes)
{
	int ret, retr;
	uint32_t ctl, ctl_new, port_disabled;
	enum riocp_pe_speed _speed = RIOCP_SPEED_UNKNOWN;
	uint8_t lane = 0, width = 0, current_lane;
	char serdes_str[9][4];
	struct switch_priv_t *priv = (struct switch_priv_t*)sw->private_driver_data;

	RIOCP_TRACE("[0x%08x:%s:hc %u] Set port %u speed\n",
			sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);

	/* obtain port width and lane configuration */
	ret = cps1xxx_get_lane_width(sw, port, &width);
	if (ret < 0) {
		RIOCP_ERROR("Could net get lane count of port %u (ret = %d, %s)\n",
			port, ret, strerror(-ret));
		return 0;
	}

	ret = cps1xxx_port_get_first_lane(sw, port, &lane);
	if (ret < 0) {
		RIOCP_ERROR("Could net get first lane of port %u (ret = %d, %s)\n",
			port, ret, strerror(-ret));
		return ret;
	}

	ret = cps1xxx_is_port_disabled(sw, port, &port_disabled);
	if (ret < 0) {
		RIOCP_ERROR("Could net get disabled state of port %u (ret = %d, %s)\n",
			port, ret, strerror(-ret));
		return ret;
	}

	switch (RIOCP_PE_DID(sw->cap)) {
	case RIO_DID_IDT_SPS1616:
	case RIO_DID_IDT_CPS1616:
		switch(speed) {
		case RIOCP_SPEED_1_25G:
			ctl_new = CPS1616_LANE_CTL_1_25G;
			_speed = RIOCP_SPEED_1_25G;
			break;
		case RIOCP_SPEED_2_5G:
			ctl_new = CPS1616_LANE_CTL_2_5G;
			_speed = RIOCP_SPEED_2_5G;
			break;
		default:
		case RIOCP_SPEED_3_125G:
			ctl_new = CPS1616_LANE_CTL_3_125G;
			_speed = RIOCP_SPEED_3_125G;
			break;
		case RIOCP_SPEED_5_0G:
			ctl_new = CPS1616_LANE_CTL_5_0G;
			_speed = RIOCP_SPEED_5_0G;
			break;
		case RIOCP_SPEED_6_25G:
			ctl_new = CPS1616_LANE_CTL_6_25G;
			_speed = RIOCP_SPEED_6_25G;
			break;
		}

		ret = cps1xxx_disable_port(sw, port);
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error disable port %u failed\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
			return ret;
		}

		for(current_lane = lane; current_lane < (lane+width); current_lane++) {
			ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(current_lane), &ctl);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading lane %d ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}

			ctl &= ~(CPS1xxx_LANE_CTL_PLL_SEL | CPS1xxx_LANE_CTL_RX_RATE | CPS1xxx_LANE_CTL_TX_RATE);
			ctl_new |= ctl;

			ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(current_lane), ctl_new);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error writing lane %d ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}

			priv->lanes[current_lane].speed = speed;

			if(serdes) {
				ret = cps1xxx_set_tx_serdes(sw, current_lane, &serdes->val[current_lane-lane].idtgen2.tx);
				if (ret < 0) {
					RIOCP_WARN("[0x%08x:%s:hc %u] failed write lane %d TX SERDES\n",
							sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				}
				ret = cps1xxx_set_rx_serdes(sw, current_lane, &serdes->val[current_lane-lane].idtgen2.rx);
				if (ret < 0) {
					RIOCP_WARN("[0x%08x:%s:hc %u] failed write lane %d RX SERDES\n",
							sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				}
				RIOCP_DEBUG("[0x%08x:%s:hc %u] lane %u %dMBaud SERDES tx:(%s,%s,%s) rx:(%s,%s,%s,%s,%s,%s)\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
						current_lane, (int)speed,
						serdes_val_2_str(serdes_str[0], sizeof(serdes_str[0]), serdes->val[current_lane-lane].idtgen2.tx.amplitude),
						serdes_val_2_str(serdes_str[1], sizeof(serdes_str[1]), serdes->val[current_lane-lane].idtgen2.tx.pos1_tap),
						serdes_val_2_str(serdes_str[2], sizeof(serdes_str[2]), serdes->val[current_lane-lane].idtgen2.tx.neg1_tap),
						serdes_val_2_str(serdes_str[3], sizeof(serdes_str[3]), serdes->val[current_lane-lane].idtgen2.rx.dfe_offs),
						serdes_val_2_str(serdes_str[4], sizeof(serdes_str[4]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap4),
						serdes_val_2_str(serdes_str[5], sizeof(serdes_str[5]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap3),
						serdes_val_2_str(serdes_str[6], sizeof(serdes_str[6]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap2),
						serdes_val_2_str(serdes_str[7], sizeof(serdes_str[7]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap1),
						serdes_val_2_str(serdes_str[8], sizeof(serdes_str[8]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap0));
			} else {
				RIOCP_WARN("[0x%08x:%s:hc %u] lane %u %dMBaud SERDES missing.\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
						current_lane, (int)speed);
			}
		}

		if(!port_disabled) {
			ret = cps1xxx_enable_port(sw, port);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error enable port %u failed\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
				return ret;
			}
			retr = 20;
			do {
				ret = cps1xxx_clear_port_error(sw, port);
				retr--;
			} while (ret > 0 && retr > 0);
		}

		break;
	case RIO_DID_IDT_CPS1848:
	case RIO_DID_IDT_CPS1432: {
		int map, pll_chg = 0;
		uint8_t _pll = 255;

		switch(speed) {
		case RIOCP_SPEED_1_25G:
			ctl_new = CPS1432_LANE_CTL_1_25G;
			_speed = RIOCP_SPEED_1_25G;
			break;
		case RIOCP_SPEED_2_5G:
			ctl_new = CPS1432_LANE_CTL_2_5G;
			_speed = RIOCP_SPEED_2_5G;
			break;
		default:
		case RIOCP_SPEED_3_125G:
			/* 2.5G and 3.125G same rate value */
			ctl_new = CPS1432_LANE_CTL_3_125G;
			_speed = RIOCP_SPEED_3_125G;
			break;
		case RIOCP_SPEED_5_0G:
			ctl_new = CPS1432_LANE_CTL_5_0G;
			_speed = RIOCP_SPEED_5_0G;
			break;
		case RIOCP_SPEED_6_25G:
			/* 5.0G and 6.25G same rate value */
			ctl_new = CPS1432_LANE_CTL_6_25G;
			_speed = RIOCP_SPEED_6_25G;
			break;
		}

		ret = riocp_pe_maint_read(sw, CPS1xxx_QUAD_CFG, &ctl);
		if (ret < 0)
			return ret;

		for (map=0;map<gen2_portmaps_len;map++) {
			if (gen2_portmaps[map].did == RIOCP_PE_DID(sw->cap) && gen2_portmaps[map].port == port) {
				if (gen2_portmaps[map].cfg == ((ctl >> (gen2_portmaps[map].quad * gen2_portmaps[map].quad_shift)) & 3)) {
					_pll = gen2_portmaps[map].pll;
					break;
				}
			}
		}

		if (_pll == 255)
			return -ENOSYS;

		ret = cps1xxx_disable_port(sw, port);
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error disable port %u failed\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
			return ret;
		}

		ret = riocp_pe_maint_read(sw, CPS1xxx_PLL_X_CTL_1(_pll), &ctl);
		if (ret < 0)
			return ret;

		if (ctl & CPS1xxx_PLL_X_CTL_PLL_PWR_DOWN) {
			ctl &= ~CPS1xxx_PLL_X_CTL_PLL_PWR_DOWN;
			pll_chg = 1;
		}
		if (speed == RIOCP_SPEED_3_125G || speed == RIOCP_SPEED_6_25G) {
			if (!(ctl & CPS1xxx_PLL_X_CTL_PLL_DIV_SEL))
				pll_chg = 1;
			ctl |= CPS1xxx_PLL_X_CTL_PLL_DIV_SEL;
		} else {
			if (ctl & CPS1xxx_PLL_X_CTL_PLL_DIV_SEL)
				pll_chg = 1;
			ctl &= ~CPS1xxx_PLL_X_CTL_PLL_DIV_SEL;
		}

		ret = riocp_pe_maint_write(sw, CPS1xxx_PLL_X_CTL_1(_pll), ctl);
		if (ret < 0)
			return ret;

		for(current_lane = lane; current_lane < (lane+width); current_lane++) {
			ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(current_lane), &ctl);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading lane %d ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}

			ctl &= ~(CPS1xxx_LANE_CTL_RX_RATE | CPS1xxx_LANE_CTL_TX_RATE);
			ctl_new |= ctl;

			ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(current_lane), ctl_new);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error writing lane %d ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}

			priv->lanes[current_lane].speed = speed;

			if(serdes) {
				ret = cps1xxx_set_tx_serdes(sw, current_lane, &serdes->val[current_lane-lane].idtgen2.tx);
				if (ret < 0) {
					RIOCP_WARN("[0x%08x:%s:hc %u] failed write lane %d TX SERDES\n",
							sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				}
				ret = cps1xxx_set_rx_serdes(sw, current_lane, &serdes->val[current_lane-lane].idtgen2.rx);
				if (ret < 0) {
					RIOCP_WARN("[0x%08x:%s:hc %u] failed write lane %d RX SERDES\n",
							sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, current_lane);
				}
				RIOCP_DEBUG("[0x%08x:%s:hc %u] lane %u %dMBaud SERDES tx:(%s,%s,%s) rx:(%s,%s,%s,%s,%s,%s)\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
						current_lane, (int)speed,
						serdes_val_2_str(serdes_str[0], sizeof(serdes_str[0]), serdes->val[current_lane-lane].idtgen2.tx.amplitude),
						serdes_val_2_str(serdes_str[1], sizeof(serdes_str[1]), serdes->val[current_lane-lane].idtgen2.tx.pos1_tap),
						serdes_val_2_str(serdes_str[2], sizeof(serdes_str[2]), serdes->val[current_lane-lane].idtgen2.tx.neg1_tap),
						serdes_val_2_str(serdes_str[3], sizeof(serdes_str[3]), serdes->val[current_lane-lane].idtgen2.rx.dfe_offs),
						serdes_val_2_str(serdes_str[4], sizeof(serdes_str[4]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap4),
						serdes_val_2_str(serdes_str[5], sizeof(serdes_str[5]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap3),
						serdes_val_2_str(serdes_str[6], sizeof(serdes_str[6]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap2),
						serdes_val_2_str(serdes_str[7], sizeof(serdes_str[7]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap1),
						serdes_val_2_str(serdes_str[8], sizeof(serdes_str[8]), serdes->val[current_lane-lane].idtgen2.rx.dfe_tap0));
			} else {
				RIOCP_WARN("[0x%08x:%s:hc %u] lane %u %dMBaud SERDES missing.\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount,
						current_lane, (int)speed);
			}
		}

		if (pll_chg)
			cps1xxx_reset_pll(sw, _pll);

		if(!port_disabled) {
			ret = cps1xxx_enable_port(sw, port);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error enable port %u failed\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
				return ret;
			}
			retr = 20;
			do {
				ret = cps1xxx_clear_port_error(sw, port);
				retr--;
			} while (ret > 0 && retr > 0);
		}

		break;
	}
	default:
		return -ENOSYS;
	}

	RIOCP_TRACE("Port %u speed set to: %u\n", port, _speed);

	return ret;
}

int cps1xxx_get_port_state(struct riocp_pe *sw, uint8_t port, riocp_pe_port_state_t *state)
{
	int ret;
	uint32_t err_stat;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &err_stat);
	if (ret) {
		RIOCP_ERROR("could not read port state: %s\n", strerror(-ret));
		return ret;
	}

	*state = 0;

	if (err_stat & RIO_PORT_N_ERR_STATUS_PORT_ERR)
		*state |= RIOCP_PE_PORT_STATE_ERROR;
	if (err_stat & RIO_PORT_N_ERR_STATUS_PORT_OK)
		*state |= RIOCP_PE_PORT_STATE_OK;
	if (err_stat & RIO_PORT_N_ERR_STATUS_PORT_UNINIT)
		*state |= RIOCP_PE_PORT_STATE_UNINITIALIZED;

	return 0;
}

static int cps1xxx_port_event_handler(struct riocp_pe *sw, struct riocp_pe_event *event)
{
	int ret;
	uint32_t val;
	uint32_t ctl;
	uint32_t err_status;
	uint32_t err_det, err_det_after;
	uint32_t impl_err_det, impl_err_det_after;
	uint8_t port;
	struct switch_priv_t *priv = (struct switch_priv_t *)sw->private_driver_data;

	port = event->port;

	RIOCP_TRACE("Port %u event\n", port);

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &err_status);
	if (ret < 0) {
		RIOCP_ERROR("Switch 0x%04x (0x%08x) port %d read error while processing event\n", sw->destid, sw->comptag, port);
		return -EIO;
	}

	/* If PORT_OK is not set PORT_UNINIT must be set together with PORT_ERR.
		to notify the link has been lost.
		if not the link probably toggled generating a port-write and then
		going to this state for a very short amount of time.
		If errors are handled when the port is in this state somehow the next error
		will not be detected and no port-writes are sent anymore. This causes
		the port to stop detecting any events. Therefore this check is added. */
	if (!(err_status & (CPS1xxx_ERR_STATUS_PORT_OK | CPS1xxx_ERR_STATUS_PORT_UNINIT | CPS1xxx_ERR_STATUS_PORT_ERR))) {
		RIOCP_ERROR("switch 0x%04x (0x%08x) port %d is in invalid state, ignoring port-write\n",
				sw->destid, sw->comptag, port);
		return -EIO;
	}

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_DET_CSR(port), &err_det);
	if (ret < 0)
		goto out;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &impl_err_det);
	if (ret < 0)
		goto out;

	if (err_det & CPS1xxx_ERR_DET_LINK_TIMEOUT)
		RIOCP_TRACE("switch 0x%04x (0x%08x) port %d Link timeout\n", sw->destid, sw->comptag, port);
	if (err_det & CPS1xxx_ERR_DET_CS_ACK_ILL)
		RIOCP_TRACE("switch 0x%04x (0x%08x) port %d Unexpected ack control symbol\n", sw->destid, sw->comptag, port);

	if (err_status & CPS1xxx_ERR_STATUS_PORT_ERR) {
		RIOCP_WARN("switch 0x%04x (0x%08x) port %d error detected (0x%08x)\n", sw->destid, sw->comptag, port, err_status);
		/* probably both input and output error are set */

		if(!(riocp_pe_maint_read(sw, 0x148 + 0x20 * port, &val) < 0))
			RIOCP_WARN("ACKID_STAT   : 0x%08x\n", val);
		RIOCP_WARN("PORT_ERR_DET : 0x%08x\n", err_det);
		if(!(riocp_pe_maint_read(sw, 0x1068 + 0x40 * port, &val) < 0))
			RIOCP_WARN("PORT_ERR_RATE: 0x%08x\n", val);
		RIOCP_WARN("PORT_IMP_ERR : 0x%08x\n", impl_err_det);

		goto skip_port_errors;
	}
	RIOCP_INFO("PORT:%u ERR_STAT_CSR:0x%08x ERR_DET_CSR:0x%08x IMP_ERR_DET_CSR:0x%08x\n", port, err_status, err_det, impl_err_det);
#ifdef CONFIG_PRINT_LANE_STATUS_ON_EVENT
	{
		uint8_t first_lane, lane_count, lane;

		ret = cps1xxx_port_get_first_lane(sw, port, &first_lane);
		if (ret < 0)
			goto out;

		ret = cps1xxx_get_lane_width(sw, port, &lane_count);
		if (ret < 0)
			goto out;

		for(lane=first_lane; lane<(lane_count+first_lane);lane++) {
			if(!(riocp_pe_maint_read(sw, 0xff800c + 0x100 * lane, &val) < 0))
				RIOCP_WARN("LANE_%u_ERR_DET: 0x%08x\n", lane, val);
		}
	}
#endif

skip_port_errors:
	/* in case both fatal error and link initialized set (slow software)
	 handle fatal error first, then link initialized */
	/* PORT_UNINIT can be set sometimes even when port is still operational */
	if ((err_status & (CPS1xxx_ERR_STATUS_PORT_ERR | CPS1xxx_ERR_STATUS_OUTPUT_FAIL)) ||
	   (impl_err_det & (CPS1xxx_IMPL_SPEC_ERR_DET_ERR_RATE | CPS1xxx_IMPL_SPEC_ERR_DET_LOA))) {
		/* check if we got an extra port-write (e.g. input-err then fatal)
		 see whether we already locked the port */
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &ctl);
		if (ret < 0)
			goto out;

		if (!(ctl & CPS1xxx_CTL_PORT_LOCKOUT)) {
			RIOCP_DEBUG("switch 0x%04x (0x%08x) port %d un-initialized\n", sw->destid, sw->comptag, port);
			/* force link reinitialization as suggested in the CPS1616 errata
				see PORT_OK may indicate incorrect status for more information */
			ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &val);
			if (ret < 0)
				goto out;

			/* This will force the switch to try to retrain a link which will fail.
				this ensures that PORT_OK bit indicates the right status. */
			ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port),
				val | CPS1xxx_OPS_FORCE_REINIT);
			if (ret < 0)
				goto out;

			/* reset our expected and next ackid
			 * (assume resetted device at other side) */
			ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_LOCAL_ACKID_CSR(port), CPS1xxx_LOCAL_ACKID_CSR_RESET);
			if (ret < 0)
				goto out;

			ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &err_status);
			if (ret < 0) {
				RIOCP_ERROR("Switch 0x%04x (0x%08x) went down while processing event\n", sw->destid, sw->comptag);
				return -EIO;
			}
			if (!(err_status & CPS1xxx_ERR_STATUS_PORT_OK)) {
				RIOCP_DEBUG("switch 0x%04x (0x%08x) port %d link down detected\n", sw->destid, sw->comptag, port);
				cps1xxx_lock_port(sw, port);
				event->event |= RIOCP_PE_EVENT_LINK_DOWN;
			}
		}
	}

	if (err_status & CPS1xxx_ERR_STATUS_OUTPUT_DROP)
		RIOCP_WARN("switch 0x%04x (0x%08x) port %d dropped a packet\n", sw->destid, sw->comptag, port);

	/* IMPL_SPEC_ERR_DET_PORT_INIT indicates that the link was initialized */
	if ((impl_err_det & CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT) &&
			(err_status & CPS1xxx_ERR_STATUS_PORT_OK)) {
		RIOCP_DEBUG("switch 0x%04x (0x%08x) port %d link initialized\n", sw->destid, sw->comptag, port);
		/* unlock port */
		cps1xxx_unlock_port(sw, port);
		/* do not clear link init notification when trigger new port failed */
		if (ret < 0) {
			impl_err_det &= ~CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT;
			err_status &= ~CPS1xxx_ERR_STATUS_PORT_W_PEND;
		} else {
			/* clear errors due to port resynchronization */
			err_status |= CPS1xxx_ERR_STATUS_OUTPUT_FAIL | CPS1xxx_ERR_STATUS_OUTPUT_ERR;
		}
		event->event |= RIOCP_PE_EVENT_LINK_UP;
	}

	if (impl_err_det & CPS1xxx_IMPL_SPEC_ERR_DET_MANY_RETRY) {
		RIOCP_DEBUG("switch 0x%04x (0x%08x) port %d retry limit (%u) triggered\n", sw->destid, sw->comptag, port, priv->ports[port].retry_lim);
		event->event |= RIOCP_PE_EVENT_RETRY_LIMIT;
	}

	/* clear error rate register to see next error */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_RATE_CSR(port), CPS1xxx_ERR_RATE_RESET);
	if (ret < 0)
		goto out;

	/* Read the ERR_DET and IMPL_ERR_DET register to verify if new errors have
		been introduced during the routine. */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_DET_CSR(port), &err_det_after);
	if (ret < 0)
		goto out;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &impl_err_det_after);
	if (ret < 0)
		goto out;

	/* only clear the errors that are seen.
		new errors may be generated during the port-event routine. this makes
		sure they are not lost when clearing the errrors.
		If there are errors left this will make the switch generate new port-writes again
		(In combination with the PW_PNDG bit). */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port),
		impl_err_det_after & ~impl_err_det);
	if (ret < 0)
		goto out;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_DET_CSR(port),
		err_det_after & ~err_det);
	if (ret < 0)
		goto out;

	/* if new errors are introduced do not clear PW_PNDG bit
		this bit gets cleared by writing 1 to it. If it gets cleared port-write
		generation is stopped even if there are errors left.
		So if errors are left do not clear this bit. */
	if ((impl_err_det_after != impl_err_det) || (err_det_after != err_det))
		err_status &= ~CPS1xxx_ERR_STATUS_PORT_W_PEND;

	/* clear error status as last one, this will set the PW_PNDG bit to
		generate port-writes or stop generating port-writes */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), err_status);
	if (ret < 0)
		goto out;

#if CPS1xxx_DEBUG_INT_STATUS == 1
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_DET_CSR(port), &err_det);
	if (ret < 0)
		goto out;
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &impl_err_det);
	if (ret < 0)
		goto out;
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &err_status);
	if (ret < 0)
		goto out;
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_OPS(port), &ctl);
	if (ret < 0)
		goto out;
	RIOCP_DEBUG("switch 0x%04x (0x%08x) port %d errdet=%08x impl_err_det=%08x errsts=%08x ctl=%08x\n",
			sw->destid, sw->comptag, port,
			err_det, impl_err_det, err_status, ctl);
#endif

	if (event->event | RIOCP_PE_EVENT_LINK_UP) {
		/* prepare port again for hot unplug */
		ret = cps1xxx_arm_port(sw, port);
		if (ret < 0)
			goto out;
	}
out:
	return ret;
}

/*
 * the following function implements the workaround for missing repeated port-write
 * functionality on the CPS1848.
 * The workaround will look through every port for errors. If an error is found the
 * port error is handled.
 * This function will handle only one missed port error.
 */
static int riocp_pe_cps1848_pw_workaround(struct riocp_pe *sw, struct riocp_pe_event *event)
{
	int ret;
	uint8_t port;
	uint32_t err_det;
	uint32_t impl_err_det;

	for (port = 0; port < RIOCP_PE_PORT_COUNT(sw->cap); port++) {
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_DET_CSR(port), &err_det);
		if (ret < 0)
			return ret;

		if (err_det != 0)
			goto found_port_error;

		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port),
			&impl_err_det);
		if (ret < 0)
			return ret;

		if (err_det != 0)
			goto found_port_error;
	}

	RIOCP_TRACE("No port errors found\n");

	return 0;

found_port_error:
	RIOCP_TRACE("Found error on port %u\n", port);
	event->port = port;

	ret = cps1xxx_port_event_handler(sw, event);

	return ret;
}

static int cps1xxx_cfg_event_handler(struct riocp_pe *sw, struct riomp_mgmt_event *revent, struct riocp_pe_event *event)
{
	int ret;
	uint8_t event_code;
	uint32_t val;

	RIOCP_TRACE("CFG event\n");
	event_code = CPS1xxx_PW_GET_EVENT_CODE(revent);

	/* Check if the switch is from a Prodrive PRIOCG2 */
	if (sw->cap.dev_info & (RIO_ASSY_IDENT_CAR_ASSY_PRIOCG2 | RIO_ASSY_IDENT_CAR_VENDOR_PRODRIVE)) {
		if (RIOCP_PE_DID(sw->cap) == RIO_DID_IDT_CPS1848 && event_code == LOG_CFG_RTE_FORCE) {
			RIOCP_DEBUG("Got generated repeated pw for CPS1848\n");
			ret = riocp_pe_cps1848_pw_workaround(sw, event);
			if (ret)
				return ret;
		}
	}

	switch (event_code) {
	case LOG_CFG_BAD_MASK:
		RIOCP_DEBUG("has a multi mask configuration error\n");
		break;
	case LOG_CFG_BAD_PORT:
		RIOCP_DEBUG("has a port configuration error\n");
		break;
	case LOG_CFG_BAD_RTE:
		RIOCP_DEBUG("has a Routing table configuration error\n");
		break;
	case LOG_CFG_BAD_MCAST:
		RIOCP_DEBUG("has a Multicast translation error\n");
		break;
	default:
		RIOCP_WARN("unknown configuration event: %d\n", event_code);
		break;
	}

	/* Clear configuration block errors */
	ret = riocp_pe_maint_write(sw, CPS1xxx_CFG_ERR_DET, 0);
	if (ret < 0)
		return ret;

	/* Clear pending port-write bit. bit remains set until written with a 1 to clear */
	ret = riocp_pe_maint_read(sw, CPS1xxx_CFG_BLK_ERR_RPT, &val);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_CFG_BLK_ERR_RPT, val |
		CPS1xxx_CFG_BLK_ERR_RPT_CFG_PW_PEND);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The following function handles the CPS1616 and SPS1616 repeated port-writes.
 * See CPS1616 user manual figure 26 for repeated port-write format where impl_spec is zero.
 *
 * This function verifies if it comes from CPS1616 or SPS1616 and then calls
 * the Port event or CFG event handler.
 */
static int cps1xxx_rpw_event(struct riocp_pe *sw, struct riomp_mgmt_event *revent,
	struct riocp_pe_event *event)
{
	int ret = 0;

	if ((RIOCP_PE_DID(sw->cap) == RIO_DID_IDT_CPS1616) || (RIOCP_PE_DID(sw->cap) == RIO_DID_IDT_SPS1616)) {
		if ((*revent).u.portwrite.payload[1])
			ret = cps1xxx_port_event_handler(sw, event);
		else {
			uint32_t cfg_err_det;

			/* For Configuration Errors the ERROR_LOG_CODE is incorrect therefore
				read the CFG_ERR_DET register. */
			ret = riocp_pe_maint_read(sw, CPS1xxx_CFG_ERR_DET, &cfg_err_det);
			if (ret < 0)
				return 0;

			RIOCP_INFO("CFG_ERR_DET:0x%08x\n", cfg_err_det);
			if (cfg_err_det > 0)
				ret = cps1xxx_cfg_event_handler(sw, revent, event);
			else
				RIOCP_TRACE("Got repeated pw\n");
		}
	} else {
		RIOCP_TRACE("Got repeated pw\n");
	}

	return ret;
}

int cps1xxx_event_handler(struct riocp_pe *sw, struct riomp_mgmt_event *revent, struct riocp_pe_event *event)
{
	int ret = 0;

	uint8_t port;
	uint8_t event_code;

	struct switch_priv_t *priv = (struct switch_priv_t *)sw->private_driver_data;

	int i;
	for (i=0;i<4;i++) {
		RIOCP_INFO("PW[%d]: 0x%08x\n", i, revent->u.portwrite.payload[i]);
	}

	port = RIOCP_PE_EVENT_PW_PORT_ID((*revent).u.portwrite);
	event_code = CPS1xxx_PW_GET_EVENT_CODE(revent);

	event->port = port;
	RIOCP_INFO("PW: event_code:0x%x port:%d\n", event_code, port);

	if (event_code >= LOG_PORT_ERR_FIRST && event_code <= LOG_PORT_ERR_LAST) {
		ret = cps1xxx_port_event_handler(sw, event);
		if (ret)
			goto errout;
	} else if (event_code >= LOG_CFG_ERR_FIRST && event_code <= LOG_CFG_ERR_LAST) {
		ret = cps1xxx_cfg_event_handler(sw, revent, event);
		if (ret)
			goto errout;
	} else {
		/* when no known error is reported. it might be a repeated port-write
			from the CPS1616 and SPS1616.
			This is checked after all other possible events have been detected.
			For some events the ERROR_LOG_CODE does not comply with the user manual.
			Therefore the right registers are read in the event handler. */
		ret = cps1xxx_rpw_event(sw, revent, event);
		if (ret)
			goto errout;
	}

errout:
#ifdef CONFIG_ERROR_LOG_SUPPORT
	cps1xxx_dump_event_log(sw);
#endif
	if(priv) {
		event->counter = priv->event_counter;
		priv->event_counter++;
	}

	return ret;
}

int cps1xxx_init(struct riocp_pe *sw)
{
	int ret;
	uint8_t port, lane;
	uint32_t result;
	struct switch_priv_t *switch_priv;

	/* Check if the switch read the EEPROM successfully */
	ret = riocp_pe_maint_read(sw, CPS1xxx_I2C_MASTER_STAT_CTL, &result);
	if (ret < 0)
		return ret;

	/* Print warnings because the switches will still work but a configuration
		stored in EEPROM is not loaded. */
	if (result & CPS1xxx_I2C_UNEXP_START_STOP)
		RIOCP_WARN("EEPROM: unexpected I2C start "
			"or stop detected\n");
	if (result & CPS1xxx_I2C_NACK)
		RIOCP_WARN("switch %s: EEPROM: An expected ack was not "
			"received\n");
	if (result & CPS1xxx_I2C_WORD_ERR_22)
		RIOCP_WARN("switch %s: EEPROM: 22 bit read got terminated "
			"prematurely\n");
	if (result & CPS1xxx_I2C_WORD_ERR_32)
		RIOCP_WARN("switch %s: EEPROM: 32 bit read got terminated prematurely\n");
	if (result & CPS1xxx_I2C_CHKSUM_FAIL)
		RIOCP_WARN("switch %s: EEPROM: checksum verifcation failed\n");

	if (RIOCP_PE_DID(sw->cap) == RIO_DID_IDT_CPS10Q)
		if (result & CPS1xxx_I2C_CPS10Q_BAD_IMG_VERSION)
			RIOCP_WARN("switch %s: EEPROM: bad image version\n");

	switch_priv = (struct switch_priv_t *)calloc(1,	sizeof(struct switch_priv_t));
	if (!switch_priv)
		return -ENOMEM;

	sw->private_driver_data = switch_priv;

	/* configure link time out */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_LINK_TO_CTL_CSR,
		CPS1xxx_RIO_LINK_TIMEOUT_DEFAULT);
	if (ret < 0)
		return ret;

	/* clear lut table */
	ret = cps1xxx_clear_lut(sw, RIOCP_PE_ANY_PORT);
	if (ret)
		return ret;

	/* set default route to drop */
	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_DEFAULT_PORT, CPS1xxx_NO_ROUTE);
	if (ret < 0)
		return ret;

	/* Enable reporting of loss of lane sync errors for hot swapping */
	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_LANE_ERR_RATE_EN,
		CPS1xxx_LANE_ERR_RATE_EN_LOSS);
	if (ret < 0)
		return ret;

	/* initialize lanes */
	for (lane = 0; lane < sw->sw->lane_count; lane++) {
#ifdef CONFIG_SETUP_CACHE_ENABLE
		ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_STAT_0_CSR(lane), &result);
		if (ret < 0)
			return ret;

		switch_priv->lanes[lane].lane_in_port = CPS1xxx_LANE_STAT_0_LANE(result);
		switch_priv->lanes[lane].port = CPS1xxx_LANE_STAT_0_PORT(result);
		ret = __cps1xxx_get_lane_speed(sw, lane, &switch_priv->lanes[lane].speed);
		if (ret < 0)
			return ret;
#endif
	}

	/* initialize ports */
	for (port = 0; port < RIOCP_PE_PORT_COUNT(sw->cap); port++) {

		/* port basic initialization */
		ret = cps1xxx_init_port(sw, port);
		if (ret < 0)
			return ret;

#ifdef CONFIG_SETUP_CACHE_ENABLE
		/* init driver private port data */
		ret = __cps1xxx_get_lane_width_and_lane0(sw, port,
				&switch_priv->ports[port].width,
				&switch_priv->ports[port].first_lane);
		if (ret < 0)
			return ret;
#endif
	}

	/* Set packet time-to-live to prevent final buffer deadlock.
		see CPS1616 errata: maintenance packet buffer management for
		more information. Default TTL is disabled.
		use maximum value of approximate 110 ms */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PKT_TTL_CSR, CPS1xxx_PKT_TTL_CSR_TTL_OFF);

	return ret;
}

int cps1xxx_init_em(struct riocp_pe *sw)
{
	int ret;
	uint8_t port;
#ifdef CONFIG_PORTWRITE_ENABLE
	uint16_t did = RIOCP_PE_DID(sw->cap);
	uint32_t val, result;
#endif

	/*
	 * Set a route towards the enumeration host for the input port
	 */
	{
		uint32_t port_info = 0;
		ret = riocp_pe_maint_read(sw, 0x14, &port_info);
		if (ret)
			return ret;

		RIOCP_WARN("%s: set workaround route 0x%04x-->%d\n", __func__, sw->mport->destid, 0x0ff & port_info);

		ret = riocp_pe_maint_write(sw, 0x70, sw->mport->destid);
		if (ret)
			return ret;
		ret = riocp_pe_maint_write(sw, 0x74, 0x0ff & port_info);
		if (ret)
			return ret;
//		ret = cps1xxx_set_route_entry(sw, RIOCP_PE_ANY_PORT, sw->mport->destid, (uint8_t)port_info);
//		if (ret)
//			return ret;
	}

	/* Clear configuration errors */
	ret = riocp_pe_maint_write(sw, CPS1xxx_CFG_ERR_DET, 0);
	if (ret)
		return ret;

	/* Clear lane/port errors using broadcast registers */
	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_LANE_ERR_DET, 0);
	if (ret)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_PORT_IMPL_SPEC_ERR_DET, 0);
	if (ret)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_PORT_ERR_DET, 0);
	if (ret)
		return ret;

	/* implementation specific errors are only enabled for ERR_RATE errors
		and PORT_INIT errors. The preparing a port for hot swapping procedure
		lists that other errors might lead to unexpected events. */
	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_PORT_IMPL_SPEC_ERR_RPT_EN,
		CPS1xxx_IMPL_SPEC_ERR_RPT_DEFAULT);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_PORT_ERR_RPT_EN,
		CPS1xxx_ERR_RPT_DEFAULT);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_LANE_ERR_RPT_EN,
		0xFFFFFFFF);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_CFG_ERR_CAPT_EN, 0xFFFFFFFF);
	if (ret < 0)
		return ret;

#ifdef CONFIG_PORTWRITE_ENABLE
	/* use repeated port-write sending for CPS1616.*/
	/* For the CPS1848 a work-around is used. See pw_workaround for more information */
	if (did == RIO_DID_IDT_CPS1616 || did == RIO_DID_IDT_SPS1616) {
		ret = riocp_pe_maint_write(sw, CPS1616_DEVICE_PW_TIMEOUT, CPS1616_PW_TIMER);
		if (ret < 0)
			return ret;
	}

	/* Set the Port-Write Target Device ID CSR to the host */
	val = CPS1xxx_RIO_SET_PW_DESTID(sw->mport->destid);
	if (sw->mport->minfo->prop.sys_size == RIO_SYS_SIZE_16)
		val |= CPS1xxx_RIO_PW_LARGE_DESTID;
	ret = riocp_pe_maint_write(sw, CPS1xxx_RIO_PW_DESTID_CSR, val);
	if (ret)
		return ret;

	/* Set Port-Write info CSR: PRIO=3 and CRF=1 */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PW_CTL, CPS1xxx_PW_INFO_PRIO3_CRF1 | CPS1xxx_PW_INFO_SRCID(sw->destid));
	if (ret < 0)
		return ret;

	if (!(did == RIO_DID_IDT_CPS1616 || did == RIO_DID_IDT_SPS1616)) {
		/* enable port-write reporting for configuration errors,
			this is needed for the repeated port-write workaround */
		ret = riocp_pe_maint_read(sw, CPS1xxx_CFG_BLK_ERR_RPT, &result);
		if (ret < 0)
			return ret;

		ret = riocp_pe_maint_write(sw, CPS1xxx_CFG_BLK_ERR_RPT,
			result | CPS1xxx_CFG_BLK_ERR_RPT_CFG_PW_EN);
	}
#endif


	for (port = 0; port < RIOCP_PE_PORT_COUNT(sw->cap); port++) {

#ifdef CONFIG_PORTWRITE_ENABLE
		/* port write basic configuration */
		ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_TRACE_PW_CTL(port), CPS1xxx_PORT_TRACE_PW_DIS);
		if (ret < 0)
			return ret;
#endif

		/* enable event handling for that port */
		ret = cps1xxx_arm_port(sw, port);
		if (ret < 0)
			return ret;
	}

	return 0;
}

struct riocp_pe_device_id cps1848_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_CPS1848, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_cps1848 = {
	48,
	"cps1848",
	NULL,
	cps1848_id_table,
	cps1xxx_init,
	cps1xxx_init_em,
	cps1xxx_set_route_entry,
	cps1xxx_get_route_entry,
	cps1xxx_clear_lut,
	cps1xxx_get_lane_speed,
	cps1xxx_get_lane_width,
	cps1xxx_get_port_supported_speeds,
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	NULL,
	cps1xxx_set_domain,
	cps1xxx_enable_port,
	cps1xxx_disable_port,
	cps1xxx_set_multicast_mask,
	cps1xxx_set_self_mcast,
	cps1xxx_set_retry_limit,
    cps1xxx_get_capabilities,
    cps1xxx_get_counters,
	NULL,
	NULL,
	NULL
};

struct riocp_pe_device_id cps1432_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_CPS1432, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_cps1432 = {
	32,
	"cps1432",
	NULL,
	cps1432_id_table,
	cps1xxx_init,
	cps1xxx_init_em,
	cps1xxx_set_route_entry,
	cps1xxx_get_route_entry,
	cps1xxx_clear_lut,
	cps1xxx_get_lane_speed,
	cps1xxx_get_lane_width,
	cps1xxx_get_port_supported_speeds,
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	cps1xxx_set_lane_speed,
	cps1xxx_set_domain,
	cps1xxx_enable_port,
	cps1xxx_disable_port,
	cps1xxx_set_multicast_mask,
	cps1xxx_set_self_mcast,
	cps1xxx_set_retry_limit,
    cps1xxx_get_capabilities,
    cps1xxx_get_counters,
	cps1xxx_get_trace_filter_capabilities,
	cps1xxx_set_trace_filter,
	cps1xxx_set_trace_port
};

struct riocp_pe_device_id cps1616_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_CPS1616, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_cps1616 = {
	16,
	"cps1616",
	NULL,
	cps1616_id_table,
	cps1xxx_init,
	cps1xxx_init_em,
	cps1xxx_set_route_entry,
	cps1xxx_get_route_entry,
	cps1xxx_clear_lut,
	cps1xxx_get_lane_speed,
	cps1xxx_get_lane_width,
	cps1xxx_get_port_supported_speeds,
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	NULL,
	cps1xxx_set_domain,
	cps1xxx_enable_port,
	cps1xxx_disable_port,
	cps1xxx_set_multicast_mask,
	cps1xxx_set_self_mcast,
	cps1xxx_set_retry_limit,
    cps1xxx_get_capabilities,
    cps1xxx_get_counters,
	NULL,
	NULL,
	NULL
};

struct riocp_pe_device_id sps1616_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_SPS1616, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_sps1616 = {
	16,
	"sps1616",
	NULL,
	sps1616_id_table,
	cps1xxx_init,
	cps1xxx_init_em,
	cps1xxx_set_route_entry,
	cps1xxx_get_route_entry,
	cps1xxx_clear_lut,
	cps1xxx_get_lane_speed,
	cps1xxx_get_lane_width,
	cps1xxx_get_port_supported_speeds,
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	cps1xxx_set_lane_speed,
	cps1xxx_set_domain,
	cps1xxx_enable_port,
	cps1xxx_disable_port,
	cps1xxx_set_multicast_mask,
	cps1xxx_set_self_mcast,
	cps1xxx_set_retry_limit,
    cps1xxx_get_capabilities,
    cps1xxx_get_counters,
	cps1xxx_get_trace_filter_capabilities,
	cps1xxx_set_trace_filter,
	cps1xxx_set_trace_port
};

#ifdef __cplusplus
}
#endif
