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

#include "pe.h"
#include "event.h"
#include "switch.h"
#include "rio_regs.h"
#include "rio_devs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CPS1xxx_DEBUG_INT_STATUS 1

#define CPS1xxx_RTE_PORT_SEL			(0x00010070)

#define CPS1xxx_RIO_PW_DESTID_CSR		0x1028
#define CPS1xxx_RIO_SET_PW_DESTID(destid)	((destid) << 16)

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

#define CPS1xxx_PORT_X_TRACE_PW_CTL(x)			(0xf40058 + 0x100*(x))
#define CPS1xxx_PORT_TRACE_PW_DIS				(0x00000001)

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
#define CPS1xxx_ERR_RATE_RESET		(CPS1xxx_ERR_RATE_RB_DISABLE | CPS1xxx_ERR_RATE_RR_NO_LIMIT)

#define CPS1xxx_ERR_RATE_RB_DISABLE		0x00000000
#define CPS1xxx_ERR_RATE_RB_10SEC		0x10000000
#define CPS1xxx_ERR_RATE_RR_16			0x00020000
#define CPS1xxx_ERR_RATE_RR_NO_LIMIT		0x00030000
#define CPS1xxx_ERR_RATE_RESET		(CPS1xxx_ERR_RATE_RB_DISABLE | CPS1xxx_ERR_RATE_RR_NO_LIMIT)

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
#define CPS1xxx_RIO_LINK_TIMEOUT_DEFAULT	0x00002000	/* approx 10 usecs */
#define CPS1xxx_PKT_TTL_CSR_TTL_MAXTTL		0xffff0000
#define CPS1xxx_DEFAULT_ROUTE			0xde
#define CPS1xxx_NO_ROUTE			0xdf
#define CPS1xxx_QUAD_CFG            0xF20200

/* CPS1616 */
#define CPS1616_PW_TIMER			0x05000 /* 28 us */
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

		{RIO_DID_IDT_CPS1432, 2, 1, 1, 1, 4, 2, 5},
		{RIO_DID_IDT_CPS1432, 2, 1, 1, 5, 20, 4, 5},
		{RIO_DID_IDT_CPS1432, 2, 1, 1, 13, 4, 2, 5},

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

			RIOCP_DEBUG("link req on 0x%x:%u (0x%x) port %u (atttempt %u)\n", sw->destid, sw->hopcount, sw->comptag, port, attempts);

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
 *	- enable the port to issue and receive any packets
 *	- Implement the procedure to prepare the port for hotswapping.
 *		see CPS1848 or CPS1616 manual: chapter 2.8
 */
static int cps1xxx_arm_port(struct riocp_pe *sw, uint8_t port)
{
	uint32_t result;
	int ret;

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

#ifdef CONFIG_PORTWRITE_ENABLE
	/* port write basic configuration */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_TRACE_PW_CTL(port), CPS1xxx_PORT_TRACE_PW_DIS);
	if (ret < 0)
		return ret;
#endif

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

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_OPS(port), result);
	if (ret < 0)
		return ret;

	/* set OUTPUT_EN and INPUT_EN */
	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &result);
	if (ret < 0)
		return ret;

	result |= CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN |
		CPS1xxx_CTL_STOP_ON_PORT_FAIL | CPS1xxx_CTL_DROP_PKT_EN;

	/* The STOP_ON_PORT_FAIL and DROP_PKT_EN are required to enable hot swapping */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), result);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_RPT_EN(port),
		CPS1xxx_IMPL_SPEC_ERR_DET_ERR_RATE | CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT);
	if (ret < 0)
		return ret;

	/* check whether we missed link initialization between lockout and arming now */
	ret = cps1xxx_check_link_init(sw, port);

	return ret;
}

/**
 * The following function will apply global settings to the switch.
 * It configures:
 *	- port-write destination
 *	- link time-out
 *	- enable error reporting
 *
 * Reporting for every error is enabled in ERR_DET is enabled except for UNEXP_ACKID.
 * this error is unreliable see CPS1848 or CPS1616 errata: false unexpected ackid reporting
 * for more information.
 */
static int cps1xxx_init_bdc(struct riocp_pe *sw)
{
	uint32_t result;
	int ret;
	uint32_t val;
	uint16_t did = RIOCP_PE_DID(sw->cap);

	/* use repeated port-write sending for CPS1616.*/
	/* For the CPS1848 a work-around is used. See pw_workaround for more information */
	if (did == RIO_DID_IDT_CPS1616 || did == RIO_DID_IDT_SPS1616) {
		ret = riocp_pe_maint_write(sw, CPS1616_DEVICE_PW_TIMEOUT, CPS1616_PW_TIMER);
		if (ret < 0)
			return ret;
	}

	/* Set the Port-Write Target Device ID CSR to the host */
	val = CPS1xxx_RIO_SET_PW_DESTID(sw->mport->destid);
	ret = riocp_pe_maint_write(sw, CPS1xxx_RIO_PW_DESTID_CSR, val);
	if (ret)
		return ret;

	/* configure link time out */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PORT_LINK_TO_CTL_CSR,
		CPS1xxx_RIO_LINK_TIMEOUT_DEFAULT);
	if (ret < 0)
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

	/* Enable reporting of loss of lane sync errors for hot swapping */
	ret = riocp_pe_maint_write(sw, CPS1xxx_BCAST_LANE_ERR_RATE_EN,
		CPS1xxx_LANE_ERR_RATE_EN_LOSS);
	if (ret < 0)
		return ret;

	/* enable port-write reporting for configuration errors,
		this is needed for the repeated port-write workaround */
	ret = riocp_pe_maint_read(sw, CPS1xxx_CFG_BLK_ERR_RPT, &result);
	if (ret < 0)
		return ret;

	ret = riocp_pe_maint_write(sw, CPS1xxx_CFG_BLK_ERR_RPT,
		result | CPS1xxx_CFG_BLK_ERR_RPT_CFG_PW_EN);

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
	/* enable input/output, clear lockout */
	return riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN);
}

static int cps1xxx_lock_port(struct riocp_pe *sw, uint8_t port)
{
	/* lockout port, simply wait for init notification to prevent race condition */
	return riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), CPS1xxx_CTL_PORT_LOCKOUT |
		CPS1xxx_CTL_OUTPUT_EN | CPS1xxx_CTL_INPUT_EN);
}

static int cps1xxx_disable_port(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t val;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &val);
	if (ret < 0)
		return ret;

	val |= CPS1xxx_CTL_PORT_DIS;

	return riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), val);
}

static int cps1xxx_enable_port(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t val;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &val);
	if (ret < 0)
		return ret;

	val &= ~CPS1xxx_CTL_PORT_DIS;

	return riocp_pe_maint_write(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), val);
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

/*
 * The following function will initialize a port by clearing the
 * error status csr. if there is no link detected on the port, the port will be
 * locked to improve switch performance.
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
		/* no break */
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

int cps1xxx_set_route_entry(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint8_t port)
{
	int ret;
	uint32_t val;

	/*
	 * Change 0xff to CPS1xxx_NO_ROUTE as values higher than 0xdf are not allowed
	 * in the routing table. See CPS1848 or CPS1616 user manual chapter: 2.3
	 * for more information.
	 */
	if (port == 0xff)
		port = CPS1xxx_NO_ROUTE;

	/* Select routing table to update */
	if (lut == RIOCP_PE_ANY_PORT)
		lut = 0;
	else
		lut++;

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

	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, port);
	if (ret < 0)
		return ret;

	/* Wait for entry to be commited, this is mandatory because otherwise
		the host will enumerate the same device multiple times.
		use a read to make sure entry is commited. */
	ret = riocp_pe_maint_read(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, &val);
	if (ret < 0)
		return ret;

	return ret;
}

int cps1xxx_get_route_entry(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint8_t *port)
{
	int ret;
	uint32_t _port;

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

	_port &= CPS1xxx_RTE_PORT_CSR_PORT;

	*port = _port;

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

/**
 * Get lane speed of port
 */
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
		RIOCP_ERROR("Could net get first lane of port %u (ret = %d, %s)\n",
			port, ret, strerror(-ret));
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

/**
 * Get number of lanes per port
 */
int cps1xxx_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width)
{
	int ret;
	uint32_t ctl;
	uint32_t port_cfg;
	uint8_t _width = 0, map;

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
	}
found:
	*width = _width;

	RIOCP_TRACE("ctl(0x%08x): 0x%08x, lane width: %u\n",
		CPS1xxx_PORT_X_CTL_1_CSR(port), ctl, _width);

	return 0;
}

/**
 * Set lane speed of port
 */
int cps1xxx_set_lane_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed speed)
{
	int ret, retr;
	uint32_t ctl, ctl_new;
	enum riocp_pe_speed _speed = RIOCP_SPEED_UNKNOWN;
	uint8_t lane = 0, width = 0, current_lane;

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
			ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &ctl);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading lane x ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}

			ctl &= ~(CPS1xxx_LANE_CTL_PLL_SEL | CPS1xxx_LANE_CTL_RX_RATE | CPS1xxx_LANE_CTL_TX_RATE);
			ctl_new |= ctl;

			ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(lane), ctl_new);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error writing lane x ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}
		}

		ret = cps1xxx_enable_port(sw, port);
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error enable port %u failed\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
			return ret;
		}
		cps1xxx_reset_port(sw, port);
		retr = 20;
		do {
			ret = cps1xxx_clear_port_error(sw, port);
			retr--;
		} while (ret > 0 && retr > 0);

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

		if (speed == RIOCP_SPEED_3_125G || speed == RIOCP_SPEED_6_25G) {
			if (!(ctl & CPS1xxx_PLL_X_CTL_PLL_DIV_SEL))
				pll_chg = 1;
			ctl |= CPS1xxx_PLL_X_CTL_PLL_DIV_SEL;
		} else {
			if (ctl & CPS1xxx_PLL_X_CTL_PLL_DIV_SEL)
				pll_chg = 1;
			ctl |= ~CPS1xxx_PLL_X_CTL_PLL_DIV_SEL;
		}

		ret = riocp_pe_maint_write(sw, CPS1xxx_PLL_X_CTL_1(_pll), ctl);
		if (ret < 0)
			return ret;

		for(current_lane = lane; current_lane < (lane+width); current_lane++) {
			ret = riocp_pe_maint_read(sw, CPS1xxx_LANE_X_CTL(lane), &ctl);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error reading lane x ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}

			ctl &= ~(CPS1xxx_LANE_CTL_RX_RATE | CPS1xxx_LANE_CTL_TX_RATE);
			ctl_new |= ctl;

			ret = riocp_pe_maint_write(sw, CPS1xxx_LANE_X_CTL(lane), ctl_new);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Error writing lane x ctl\n",
						sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount);
				cps1xxx_enable_port(sw, port);
				cps1xxx_clear_port_error(sw, port);
				return ret;
			}
		}

		if (pll_chg)
			cps1xxx_reset_pll(sw, _pll);

		ret = cps1xxx_enable_port(sw, port);
		if (ret < 0) {
			RIOCP_ERROR("[0x%08x:%s:hc %u] Error enable port %u failed\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, port);
			return ret;
		}
		cps1xxx_reset_port(sw, port);
		retr = 20;
		do {
			ret = cps1xxx_clear_port_error(sw, port);
			retr--;
		} while (ret > 0 && retr > 0);

		break;
	}
	default:
		return -ENOSYS;
	}

	RIOCP_TRACE("Port %u lane %u speed set to: %u\n", port, lane, _speed);

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

	port = event->port;

	RIOCP_TRACE("Port %u event\n", port);

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_STAT_CSR(port), &err_status);
	if (ret < 0) {
		RIOCP_ERROR("Switch 0x%08x went down while processing event\n", sw->comptag);
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
		RIOCP_ERROR("switch 0x%08x port %d is in invalid state,"
			"ignoring port-write\n", sw->comptag, port);
		return -EIO;
	}

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_ERR_DET_CSR(port), &err_det);
	if (ret < 0)
		goto out;

	ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_IMPL_SPEC_ERR_DET(port), &impl_err_det);
	if (ret < 0)
		goto out;

	if (err_det & CPS1xxx_ERR_DET_LINK_TIMEOUT)
		RIOCP_TRACE("Link timeout\n");
	if (err_det & CPS1xxx_ERR_DET_CS_ACK_ILL)
		RIOCP_TRACE("Unexpected ack control symbol\n");

	if (err_status & CPS1xxx_ERR_STATUS_PORT_ERR) {
		RIOCP_ERROR("switch 0x%08x fatal error\n");
		/* probably both input and output error are set */
		goto skip_port_errors;
	}

skip_port_errors:
	/* in case both fatal error and link initialized set (slow software)
	 handle fatal error first, then link initialized */
	/* PORT_UNINIT can be set sometimes even when port is still operational */
	if (err_status & (CPS1xxx_ERR_STATUS_PORT_ERR | CPS1xxx_ERR_STATUS_OUTPUT_FAIL)) {
		/* check if we got an extra port-write (e.g. input-err then fatal)
		 see whether we already locked the port */
		ret = riocp_pe_maint_read(sw, CPS1xxx_PORT_X_CTL_1_CSR(port), &ctl);
		if (ret < 0)
			goto out;

		if (!(ctl & CPS1xxx_CTL_PORT_LOCKOUT)) {
			RIOCP_DEBUG("port %d un-initialized\n", port);
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

			cps1xxx_lock_port(sw, port);
			event->event |= RIOCP_PE_EVENT_LINK_DOWN;
		}
	}

	if (err_status & CPS1xxx_ERR_STATUS_OUTPUT_DROP)
		RIOCP_WARN("dropped a packet\n");

	/* IMPL_SPEC_ERR_DET_PORT_INIT indicates that the link was initialized */
	if (impl_err_det & CPS1xxx_IMPL_SPEC_ERR_DET_PORT_INIT) {
		RIOCP_DEBUG("port %d link initialized\n", port);
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
	RIOCP_DEBUG("errdet=%08x impl_err_det=%08x errsts=%08x ctl=%08x\n",
		err_det, impl_err_det, err_status, ctl);
#endif

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
 * The following function handles the CPS1616 repeated port-writes.
 * See CPS1616 user manual figure 26 for repeated port-write format where impl_spec is zero.
 *
 * This function verifies if it comes from CPS1616 and then calls
 * the Port event or CFG event handler.
 */
static int cps1xxx_rpw_event(struct riocp_pe *sw, struct riomp_mgmt_event *revent,
	struct riocp_pe_event *event)
{
	int ret = 0;
	uint32_t cfg_err_det;

	RIOCP_TRACE("Got repeated pw\n");

	/* For Configuration Errors the ERROR_LOG_CODE is incorrect therefore
		read the CFG_ERR_DET register. */
	ret = riocp_pe_maint_read(sw, CPS1xxx_CFG_ERR_DET, &cfg_err_det);
	if (ret < 0)
		return 0;

	if ((RIOCP_PE_DID(sw->cap) == RIO_DID_IDT_CPS1616) || (RIOCP_PE_DID(sw->cap) == RIO_DID_IDT_SPS1616)) {
		if ((*revent).u.portwrite.payload[2] > 0)
			ret = cps1xxx_port_event_handler(sw, event);
		if (cfg_err_det > 0)
			ret = cps1xxx_cfg_event_handler(sw, revent, event);
	}

	return ret;
}

int cps1xxx_event_handler(struct riocp_pe *sw, struct riomp_mgmt_event *revent, struct riocp_pe_event *event)
{
	int ret = 0;

	uint8_t port;
	uint8_t event_code;

	port = RIOCP_PE_EVENT_PW_PORT_ID((*revent).u.portwrite);
	event->port = port;

	event_code = CPS1xxx_PW_GET_EVENT_CODE(revent);
	if (port >= LOG_PORT_ERR_FIRST && port <= LOG_PORT_ERR_LAST) {
		ret = cps1xxx_port_event_handler(sw, event);
		if (ret)
			return ret;
	} else if (event_code >= LOG_CFG_ERR_FIRST && event_code <= LOG_CFG_ERR_LAST) {
		ret = cps1xxx_cfg_event_handler(sw, revent, event);
		if (ret)
			return ret;
	} else {
		/* when no known error is reported. it might be a repeated port-write
			from the CPS1616.
			This is checked after all other possible events have been detected.
			For some events the ERROR_LOG_CODE does not comply with the user manual.
			Therefore the right registers are read in the event handler. */
		ret = cps1xxx_rpw_event(sw, revent, event);
		if (ret)
			return ret;
	}

	return ret;
}

int cps1xxx_init(struct riocp_pe *sw)
{
	int ret;
	uint8_t port;
	uint32_t result;

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

	/* init global settings for all ports */
	cps1xxx_init_bdc(sw);

#ifdef CONFIG_PORTWRITE_ENABLE
	/* Set Port-Write info CSR: PRIO=3 and CRF=1 */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PW_CTL, CPS1xxx_PW_INFO_PRIO3_CRF1 | CPS1xxx_PW_INFO_SRCID(sw->mport->destid));
	if (ret < 0)
		return ret;

	/* Disable Port-Write timeout and send one event per error */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PW_TMO, CPS1xxx_PW_TMO_TIMOUT_DIS);
	if (ret < 0)
		return ret;
#endif

	/* clear lut table */
	ret = cps1xxx_clear_lut(sw, RIOCP_PE_ANY_PORT);
	if (ret)
		return ret;

	/* set default route to drop */
	ret = riocp_pe_maint_write(sw, RIO_STD_RTE_DEFAULT_PORT, CPS1xxx_NO_ROUTE);
	if (ret < 0)
		return ret;

	/* initialize ports */
	for (port = 0; port < RIOCP_PE_PORT_COUNT(sw->cap); port++) {
		ret = cps1xxx_arm_port(sw, port);
		if (ret < 0)
			return ret;

		ret = cps1xxx_init_port(sw, port);
		if (ret < 0)
			return ret;
	}

	/* Set packet time-to-live to prevent final buffer deadlock.
		see CPS1616 errate: maintenance packet buffer mananement for
		more information. Default TTL is disabled.
		use maximum value of approximate 110 ms */
	ret = riocp_pe_maint_write(sw, CPS1xxx_PKT_TTL_CSR, CPS1xxx_PKT_TTL_CSR_TTL_MAXTTL);

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

	return 0;
}

int cps1xxx_init_em(struct riocp_pe *sw)
{
	sw = sw; /* gcc */
	return 0;
}

struct riocp_pe_device_id cps1848_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_CPS1848, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_cps1848 = {
	-1,
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
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	NULL
};

struct riocp_pe_device_id cps1432_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_CPS1432, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_cps1432 = {
	-1,
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
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	NULL
};

struct riocp_pe_device_id cps1616_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_CPS1616, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_cps1616 = {
	-1,
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
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	NULL
};

struct riocp_pe_device_id sps1616_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_IDT_SPS1616, RIO_VID_IDT)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};
struct riocp_pe_switch riocp_pe_switch_sps1616 = {
	-1,
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
	cps1xxx_get_port_state,
	cps1xxx_event_handler,
	NULL,
	cps1xxx_set_lane_speed
};

#ifdef __cplusplus
}
#endif
