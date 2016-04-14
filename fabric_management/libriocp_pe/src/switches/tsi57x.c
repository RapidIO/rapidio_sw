/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file tsi57x.c
 * Switch driver for Tundra TSI57x
 */
#include <errno.h>
#include <string.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "pe.h"
#include "maint.h"
#include "event.h"
#include "switch.h"
#include "rio_regs.h"
#include "rio_devs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TSI5xx_ROUTE_UNMAPPED			0xff
#define TSI5xx_RIO_PW_DESTID			0x1028
#define TSI5xx_RIO_SET_PW_DESTID(destid)	((destid) << 16)

#define TSI5xx_SPx_ERR_STATUS(x)		(0x158+0x020*(x))
#define TSI5xx_SPx_CTL(x)			(0x0015C+0x020*(x))
#define TSI5xx_SPx_ERR_DET(x)			(0x01040+0x040*(x))
#define TSI57X_SPBC_ROUTE_CFG_DESTID		0x10070
#define TSI57X_SPBC_ROUTE_CFG_PORT		0x10074
#define TSI5xx_SPx_MODE(x)			(0x11004+0x100*(x))
#define TSI57X_SPP_ROUTE_CFG_DESTID(n)		(0x11070 + 0x100 * (n))
#define TSI57X_SPP_ROUTE_CFG_PORT(n)		(0x11074 + 0x100 * (n))
#define TSI5xx_SPx_CTL_INDEP(x)			(0x13004+0x100*(x))
#define TSI578_SMACx_DLOOP_CLK_SEL(x)		(0x130C8 + 0x200 * (x))
#define TSI5xx_SPx_CS_TX(x)			(0x13014+0x100*(x))
#define TSI5xx_SPx_INT_STATUS(x)		(0x13018+0x100*(x))
#define TSI5xx_SPx_ACKID_STAT(x)		(0x00148+0x020*(x))

#define TSI5xx_CTL_PORT_WIDTH(x)		((x) & 0xC0000000)
#define TSI5xx_CTL_INIT_PORT_WIDTH(x)		((x) & 0x38000000)
#define TSI5xx_CTL_PORT_WIDTH_X4		0x40000000
#define TSI5xx_CTL_INIT_PORT_WIDTH_X4		0x10000000

#define TSI5xx_CTL_OUTPUT_EN			0x00400000
#define TSI5xx_CTL_INPUT_EN			0x00200000
#define TSI5xx_CTL_PORT_ERR_EN			0x00080000
#define TSI5xx_CTL_LINK_INIT_NOTIFICATION_EN	0x00020000
#define TSI5xx_CTL_IRQ_EN			0x00000040
#define TSI5xx_CTL_PORT_LOCKOUT			0x00000002

#define INT_STATUS_LINK_INIT_NOTIFICATION	0x00020000

#define TSI5xx_ERR_STATUS_OUTPUT_FAIL		0x02000000
#define TSI5xx_ERR_STATUS_OUTPUT_ERR		0x00020000
#define TSI5xx_ERR_STATUS_INPUT_ERR		0x00000200
#define TSI5xx_ERR_STATUS_PORT_ERR		0x00000004
#define TSI5xx_ERR_STATUS_PORT_OK		0x00000002
#define TSI5xx_ERR_STATUS_PORT_UNINIT		0x00000001

#define ERR_DET_DELIN_ERR			0x00000004
#define ERR_DET_LR_ACKID_ILL			0x00000020
#define ERR_DET_LINK_TO				0x00000001

#define ERR_DET_BOGUS_ERR_MASK (ERR_DET_LINK_TO | ERR_DET_DELIN_ERR \
	| ERR_DET_LR_ACKID_ILL)

#define TSI5xx_SMAC_CLK_SEL_PWDN_X1		0x00000008
#define TSI5xx_SMAC_CLK_SEL_SCLK_SEL_MASK	0x00000003

#define TSI5xx_SCLK_SEL_1250			0x00000000
#define TSI5xx_SCLK_SEL_2500			0x00000001
#define TSI5xx_SCLK_SEL_3125			0x00000002

#define TSI5xx_MODE_PW_DIS			0x08000000

#define CS_LINK_REQ				0x40FC8000
#define CTL_DEBUG_MODE				0x00800000

#define TSI5xx_SW_LT_CTL			0x120

#define TSI578_LINK_TIMEOUT_DEFAULT		0x00002000      /* approx 10 usecs */
#define TSI578_PW_TIMEOUT			0x1AC14
#define TSI578_PW_TIMER_MASK			0xF0000000
#define TSI578_PW_TIMER_167MS			0x10000000
#define TSI578_PW_TIMER				TSI578_PW_TIMER_167MS

#define TSI57X_MAX_PORTS			16

static int tsi57x_lock_port(struct riocp_pe *sw, uint8_t port)
{
	int ret = 0;
	uint32_t ctl;
	uint32_t status;

	if (riocp_pe_maint_read(sw, TSI5xx_SPx_ERR_STATUS(port), &status))
		return -EIO;
	if (!(status & TSI5xx_ERR_STATUS_PORT_OK)) {
		ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL(port), &ctl);
		if (ret < 0)
			return ret;
		ctl |= TSI5xx_CTL_PORT_LOCKOUT;
		ret = riocp_pe_maint_write(sw, TSI5xx_SPx_CTL(port), ctl);
	}

	return ret;
}

static int tsi57x_unlock_port(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t ctl;
	uint32_t ctl_indep;

	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL(port),
		&ctl);
	if (ret)
		return ret;

	ctl &= ~TSI5xx_CTL_PORT_LOCKOUT;
	ctl |= TSI5xx_CTL_OUTPUT_EN | TSI5xx_CTL_INPUT_EN;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_CTL(port), ctl);
	if (ret)
		return ret;

	/* debug mode unlocks control symbol generation which we need
	   to recover from input status error */
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL_INDEP(port),
		&ctl_indep);
	if (ret)
		return ret;

	ctl_indep |= CTL_DEBUG_MODE;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_CTL_INDEP(port),
		ctl_indep);
	if (ret)
		return ret;

	/* send a packet-not-accepted/link-request symbol
	 * this will eventually clear our input-err-stopped state */
	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_CS_TX(port), CS_LINK_REQ);
	if (ret)
		return ret;

	/* disable debug mode, so error capture works */
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL_INDEP(port),
		&ctl_indep);
	if (ret)
		return ret;

	ctl_indep &= ~CTL_DEBUG_MODE;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_CTL_INDEP(port), ctl_indep);
	if (ret)
		return ret;

	return 0;
}

static int tsi57x_set_route_entry(struct riocp_pe *sw, uint8_t lut,
	uint32_t destid, uint16_t value)
{
	uint32_t val;
	uint32_t cfg_destid;
	uint32_t cfg_port;

	RIOCP_TRACE("Write LUT 0x%02x for switch 0x%08x, destid %u (0x%08x), port %04x\n",
		lut, sw->comptag, destid, destid, value);

	if (lut == RIOCP_PE_ANY_PORT) {
		cfg_destid = RIO_STD_RTE_CONF_DESTID_SEL_CSR;
		cfg_port   = RIO_STD_RTE_CONF_PORT_SEL_CSR;
	} else if (lut < TSI57X_MAX_PORTS) {
		cfg_destid = TSI57X_SPP_ROUTE_CFG_DESTID(lut & 0x0f);
		cfg_port   = TSI57X_SPP_ROUTE_CFG_PORT(lut & 0x0f);
	} else {
		RIOCP_ERROR("Invalid LUT: %u\n", lut);
		return -EINVAL;
	}

	if (riocp_pe_maint_write(sw, cfg_destid, destid))
		return -EIO;
	if (riocp_pe_maint_write(sw, cfg_port, value))
		return -EIO;

	/* Wait for entry to be committed */
	if (riocp_pe_maint_read(sw, cfg_port, &val))
		return -EIO;

	RIOCP_TRACE("Write LUT successful\n");

	return 0;
}

static int tsi57x_get_route_entry(struct riocp_pe *sw, uint8_t lut,
	uint32_t destid, uint16_t *value)
{
	uint32_t _port;

	if (lut == RIOCP_PE_ANY_PORT) {
		if (riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, destid))
			return -EIO;
		if (riocp_pe_maint_read(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, &_port))
			return -EIO;
	} else if (lut < TSI57X_MAX_PORTS) {
		if (riocp_pe_maint_write(sw, TSI57X_SPP_ROUTE_CFG_DESTID(lut), destid))
			return -EIO;
		if (riocp_pe_maint_read(sw, TSI57X_SPP_ROUTE_CFG_PORT(lut), &_port))
				return -EIO;
	} else {
		RIOCP_ERROR("Invalid LUT: %u\n", lut);
		return -EINVAL;
	}

	*value = _port;

	return 0;
}

static int tsi57x_clear_lut(struct riocp_pe *sw, uint8_t lut)
{
	uint32_t destid = 0;
	int ret;

	/** FIXME: This routine only supports dev8, it needs enhancement to support dev16 */
	if (lut == RIOCP_PE_ANY_PORT) {
		for (destid = 0; destid < sw->cap.lut_size; destid++) {
			ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, destid);
			if (ret)
				return ret;
			ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR,
				TSI5xx_ROUTE_UNMAPPED);
			if (ret)
				return ret;
		}
	} else if (lut < TSI57X_MAX_PORTS) {
		ret = riocp_pe_maint_write(sw, TSI57X_SPP_ROUTE_CFG_DESTID(lut), 0x80000000);
		if (ret)
			return ret;
		for (destid = 0; destid < 256; destid++) {
			ret = riocp_pe_maint_write(sw, TSI57X_SPP_ROUTE_CFG_PORT(lut),
				TSI5xx_ROUTE_UNMAPPED);
			if (ret)
				return ret;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int tsi57x_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width)
{
	uint32_t val;
	int ret;

	/** FIXME: Note that lane width value is invalid unless PORT_OK is asserted. */
	/* This has implications for fault handling below... */
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL(port), &val);
	if (ret) {
		RIOCP_ERROR("could not read lane width reg: %s\n", strerror(-ret));
		return ret;
	}

	if (TSI5xx_CTL_INIT_PORT_WIDTH(val) == TSI5xx_CTL_INIT_PORT_WIDTH_X4)
		*width = 4;
	else
		*width = 1;

	return 0;
}

static void tsi5xx_init_port(struct riocp_pe *sw, uint8_t port)
{
	riocp_pe_maint_write(sw, TSI5xx_SPx_ERR_DET(port), 0);
	riocp_pe_maint_write(sw, TSI5xx_SPx_ERR_STATUS(port), 0xffffffff);
	riocp_pe_maint_write(sw, TSI5xx_SPx_INT_STATUS(port), 0xffffffff);
}

static int tsi57x_init(struct riocp_pe *sw)
{
	uint8_t port;

	tsi57x_clear_lut(sw, RIOCP_PE_ANY_PORT);

	/* Clear error detect, error status and interrupt status registers */
	for (port = 0; port < RIOCP_PE_PORT_COUNT(sw->cap); port++) {
		tsi5xx_init_port(sw, port);
		tsi57x_lock_port(sw, port);
	}

	/* Use repeated port-write sending */
	riocp_pe_maint_write(sw, TSI578_PW_TIMEOUT, TSI578_PW_TIMER);
	/* Configure link time out */
	riocp_pe_maint_write(sw, TSI5xx_SW_LT_CTL, TSI578_LINK_TIMEOUT_DEFAULT);

	return 0;
}

static int tsi5xx_init_em_port(struct riocp_pe *sw, uint8_t port)
{
	int ret;
	uint32_t mode;
	uint32_t control;

	RIOCP_TRACE("Init error management for port %u\n", port);

	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_MODE(port), &mode);
	if (ret < 0)
		return ret;
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL_INDEP(port), &control);
	if (ret < 0)
		return ret;

	mode &= ~TSI5xx_MODE_PW_DIS;
	control |= TSI5xx_CTL_LINK_INIT_NOTIFICATION_EN;
	control |= TSI5xx_CTL_PORT_ERR_EN;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_MODE(port), mode);
	if (ret < 0)
		return ret;
	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_CTL_INDEP(port), control);
	if (ret < 0)
		return ret;

	return 0;
}

static int tsi57x_init_em(struct riocp_pe *sw)
{
	int i;
	int ret;
	uint32_t reg;
	uint32_t val;
	uint8_t width;

	/* Set the Port-Write Target Device ID CSR to the host */
	val = TSI5xx_RIO_SET_PW_DESTID(sw->mport->destid);
	riocp_pe_maint_write(sw, TSI5xx_RIO_PW_DESTID, val);

	/* Enable port-writes for link-up-down */
	for (i = 0; i < (TSI57X_MAX_PORTS/2); i++) {
		ret = tsi57x_get_lane_width(sw, 2*i, &width);
		if (ret < 0)
			return ret;
		ret = tsi5xx_init_em_port(sw, 2*i);
		if (width == 4) {
			RIOCP_TRACE("Powerdown odd port %d\n", 2*i+1);
			/* power-down odd x1 port */
			reg = TSI578_SMACx_DLOOP_CLK_SEL(i);
			ret = riocp_pe_maint_read(sw, reg, &val);
			if (ret < 0)
				return ret;
			val |= TSI5xx_SMAC_CLK_SEL_PWDN_X1;
			riocp_pe_maint_write(sw, reg, val);
		} else {
			ret = tsi5xx_init_em_port(sw, 2*i+1);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tsi57x_get_lane_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed *speed)
{
	uint32_t reg;
	int ret;
	uint32_t val;

	reg = TSI578_SMACx_DLOOP_CLK_SEL(port >> 1);
	ret = riocp_pe_maint_read(sw, reg, &val);
	if (ret) {
		RIOCP_ERROR("Could not read lane speed (reg 0x%08x): %s\n", reg, strerror(-ret));
		return ret;
	}

	switch (val & TSI5xx_SMAC_CLK_SEL_SCLK_SEL_MASK) {
	case TSI5xx_SCLK_SEL_1250:
		*speed = RIOCP_SPEED_1_25G;
		break;
	case TSI5xx_SCLK_SEL_2500:
		*speed = RIOCP_SPEED_2_5G;
		break;
	case TSI5xx_SCLK_SEL_3125:
		*speed = RIOCP_SPEED_3_125G;
		break;
	default:
		*speed = RIOCP_SPEED_UNKNOWN;
		RIOCP_ERROR("Invalid read lane speed: 0x%08x\n", val);
		return -EIO;
	}

	return 0;
}

static int tsi57x_get_port_state(struct riocp_pe *sw, uint8_t port, riocp_pe_port_state_t *state)
{
	int ret;
	uint32_t err_stat;

	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_ERR_STATUS(port), &err_stat);
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

static int tsi57x_event_handler(struct riocp_pe *sw, struct riomp_mgmt_event *revent, struct riocp_pe_event *event)
{
	int ret;
	uint8_t port;
	uint32_t err_status;
	uint32_t int_status;
	uint32_t err_det;
	uint32_t control;
	uint8_t width;

	port = RIOCP_PE_EVENT_PW_PORT_ID((*revent).u.portwrite);
	RIOCP_TRACE("Received event on port %u\n", port);
	event->port = port;

	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_ERR_STATUS(port), &err_status);
	if (ret) {
		RIOCP_ERROR("Register read error for comptag 0x%08x on port %u\n",
			sw->comptag,  port);
		return ret;
	}
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_INT_STATUS(port), &int_status);
	if (ret) {
		RIOCP_ERROR("Register read error for comptag 0x%08x on port %u\n",
			sw->comptag,  port);
		return ret;
	}
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_ERR_DET(port), &err_det);
	if (ret) {
		RIOCP_ERROR("Register read error for comptag 0x%08x on port %u\n",
			sw->comptag,  port);
		return ret;
	}
	ret = riocp_pe_maint_read(sw, TSI5xx_SPx_CTL(port), &control);
	if (ret) {
		RIOCP_ERROR("Register read error for comptag 0x%08x on port %u\n",
			sw->comptag,  port);
		return ret;
	}

	RIOCP_DEBUG("err_status[0x%08x]: 0x%08x\n", TSI5xx_SPx_ERR_STATUS(port), err_status);
	RIOCP_DEBUG("int_status[0x%08x]: 0x%08x\n", TSI5xx_SPx_INT_STATUS(port), int_status);
	RIOCP_DEBUG("err_det[0x%08x]: 0x%08x\n", TSI5xx_SPx_ERR_DET(port), err_det);
	RIOCP_DEBUG("control[0x%08x]: 0x%08x\n", TSI5xx_SPx_CTL(port), control);

	if (err_status & TSI5xx_ERR_STATUS_PORT_OK)
		RIOCP_DEBUG("Port %d OK in err_status\n", port);
	if (err_status & TSI5xx_ERR_STATUS_PORT_ERR)
		RIOCP_DEBUG("Port %d error in err_status\n", port);
	if (err_status & TSI5xx_ERR_STATUS_PORT_UNINIT)
		RIOCP_DEBUG("Port %d port uninitialized\n", port);

	/* Check for bogus port errors that occure when:
	 * - MAC is conigured as a double 1x link
	 * - port has an even number
	 * - an odd port on the same MAC has a PORT_ERR
	 * If PORT_OK is still set and LR_ACKID_ILL, DELIN_ERR,
	 * LINK_TO are not, then treat this error as bogus */
	if (err_status & TSI5xx_ERR_STATUS_PORT_ERR) {
		if (((err_det & ERR_DET_BOGUS_ERR_MASK) == 0) &&
			(err_status & TSI5xx_ERR_STATUS_PORT_OK)) {
			/**
			 * FIXME: What is required here is the configured lane width, not the trained
			 * lane width.  Check SMACx_DLOOP_CLK_SEL.MAC_MODE
			 */
			ret = tsi57x_get_lane_width(sw, port, &width);
			if (ret) {
				RIOCP_ERROR("Could not get lane width for port %d\n", port);
				return ret;
			}
			if ((width != 4) && ((port & 1) == 0)) {
				RIOCP_ERROR("bogus fatal error on port %d\n", port);
				goto skip;
			}
		}
	}

	if (int_status & INT_STATUS_LINK_INIT_NOTIFICATION) {
		RIOCP_DEBUG("port %d link initialized\n", port);
		ret = tsi57x_unlock_port(sw, port);
		if (ret)
			return ret;
		/* clear errors due to port resynchronization */
		err_status |= TSI5xx_ERR_STATUS_OUTPUT_FAIL | TSI5xx_ERR_STATUS_OUTPUT_ERR;
		event->event |= RIOCP_PE_EVENT_LINK_UP;
	}

	if (err_status & TSI5xx_ERR_STATUS_PORT_ERR) {
		if (!(control & TSI5xx_CTL_PORT_LOCKOUT)) {
			RIOCP_DEBUG("port %d un-initialized\n", port);
			tsi57x_lock_port(sw, port);
			/* tsi578 does not need power-down, reset our expected and
			 * next ackid (assume resetted device at other side), we cannot
			 * really use the synchronize ackid method, since we don't know
			 * via what port we are connected at remote device */
			riocp_pe_maint_write(sw, TSI5xx_SPx_ACKID_STAT(port), 0);
			event->event |= RIOCP_PE_EVENT_LINK_DOWN;
		}
	}

skip:
	err_status |= RIO_PORT_N_ERR_STATUS_PORT_W_PEND;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_ERR_DET(port), 0);
	if (ret)
		return ret;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_INT_STATUS(port), int_status);
	if (ret)
		return ret;

	ret = riocp_pe_maint_write(sw, TSI5xx_SPx_ERR_STATUS(port), err_status);
	if (ret)
		return ret;

	RIOCP_TRACE("Cleared PORT_W_PEND for %d, %08x\n", port, sw->comptag);

	return ret;
}

struct riocp_pe_device_id tsi57x_id_table[] = {
	{RIOCP_PE_PE_DEVICE(RIO_DID_TSI578, RIO_VID_TUNDRA)},
	{RIOCP_PE_PE_DEVICE(0xffff, 0xffff)}
};

struct riocp_pe_switch riocp_pe_switch_tsi57x = {
	-1,
	0,
	-1,
	"tsi57x",
	NULL,
	tsi57x_id_table,
	tsi57x_init,
	tsi57x_init_em,
	tsi57x_set_route_entry,
	tsi57x_get_route_entry,
	tsi57x_clear_lut,
	tsi57x_get_lane_speed,
	tsi57x_get_lane_width,
	tsi57x_get_port_state,
	tsi57x_event_handler,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

#ifdef __cplusplus
}
#endif
