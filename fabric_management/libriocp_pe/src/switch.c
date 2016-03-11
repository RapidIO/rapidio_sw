/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file switch.c
 * Switch driver wrapper functions
 */
#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "maint.h"
#include "switch.h"
#include "rio_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct riocp_pe_switch *riocp_pe_switch_drv_list[] = {
	&riocp_pe_switch_tsi57x,
	&riocp_pe_switch_cps1616,
	&riocp_pe_switch_cps1432,
	&riocp_pe_switch_cps1848,
	&riocp_pe_switch_sps1616
};

/**
 * Clear the enumerated bits on all the switch ports
 * @param sw Target switch PE
 */
static int riocp_pe_switch_clear_enumerated(struct riocp_pe *sw)
{
	unsigned int i = 0;
	int ret = 0;

	for (i = 0; i < RIOCP_PE_PORT_COUNT(sw->cap); i++) {
		ret = riocp_pe_switch_port_clear_enumerated(sw, i);
		if (ret) {
			RIOCP_ERROR("Could not clear enumerate state of sw 0x%08x\n", sw->comptag);
			return ret;
		}
	}

	return 0;
}

/**
 * Attach driver to switch based on DID, VID, ASMDID, ASMVID
 * @param sw Target switch PE
 * @param initialize Initialize all switch registers to default settings
 */
int riocp_pe_switch_attach_driver(struct riocp_pe *sw, bool initialize)
{
	unsigned int i;
	unsigned int j = 0;
	uint16_t drv_vid = 0;
	uint16_t drv_did = 0;
	uint16_t did = RIOCP_PE_DID(sw->cap);
	uint16_t vid = RIOCP_PE_VID(sw->cap);
	int ret = 0;

	RIOCP_TRACE("did %04X, vid %04X\n", did, vid);
	for (i = 0; i < RIOCP_PE_ARRAY_SIZE(riocp_pe_switch_drv_list); i++) {
		j = 0;
		drv_vid = 0;
		drv_did = 0;
		while ((drv_did != 0xffff) && (drv_vid != 0xffff)) {
			drv_did = riocp_pe_switch_drv_list[i]->id_table[j].did;
			drv_vid = riocp_pe_switch_drv_list[i]->id_table[j].vid;

			RIOCP_TRACE("Driver \"%s\", did: 0x%04x, vid: 0x%04x\n",
				riocp_pe_switch_drv_list[i]->name,
				drv_did, drv_vid);

			if (did == drv_did && vid == drv_vid) {
				sw->sw = riocp_pe_switch_drv_list[i];
				RIOCP_TRACE("Attached driver %s to PE 0x%08x\n", sw->sw->name, sw->comptag);
				goto found;
			}

			j++;
		}
	}

	RIOCP_WARN("Unknown switch, trying standard driver\n");
	sw->sw = &riocp_pe_switch_std;

found:
	if (RIOCP_PE_IS_HOST(sw) && initialize == true) {
		RIOCP_TRACE("Initializing of %08x\n", sw->comptag);

		ret = riocp_pe_switch_clear_enumerated(sw);
		if (ret)
			return ret;

		ret = riocp_pe_switch_init(sw);
		if (ret) {
			RIOCP_ERROR("Could not initialize switch\n");
			return ret;
		}

		ret = riocp_pe_switch_init_em(sw);
		if (ret) {
			RIOCP_ERROR("Could not initialize switch error management\n");
			return ret;
		}
	}
	return 0;
}

/**
 * Check if switch port has the enumerate boundary bit set
 * @param sw   Target switch PE
 * @param port Port to be set
 * @retval 0 When port is not enumerated
 * @retval 1 When port is enumerated
 */
int riocp_pe_switch_port_is_enumerated(struct riocp_pe *sw, uint8_t port)
{
	uint32_t offset;
	uint32_t val;
	int ret = 0;

	offset = sw->efptr_phys + RIO_PORT_N_CTL_CSR(port);
	ret = riocp_pe_maint_read(sw, offset, &val);
	if (ret)
		return ret;

	return (val & RIO_PORT_N_CTL_ENUM_B) ? 1 : 0;
}

/**
 * Set enumerate boundary bit in switch port
 * @param sw   Target switch PE
 * @param port Port to be set
 */
int riocp_pe_switch_port_set_enumerated(struct riocp_pe *sw, uint8_t port)
{
	uint32_t offset;
	uint32_t val;
	int ret = 0;

	offset = sw->efptr_phys + RIO_PORT_N_CTL_CSR(port);
	ret = riocp_pe_maint_read(sw, offset, &val);
	if (ret)
		return ret;

	val |= RIO_PORT_N_CTL_ENUM_B;
	ret = riocp_pe_maint_write(sw, offset, val);

	return ret;
}

/**
 * Clear enumerate boundary bit in switch port
 * @param sw   Target switch PE
 * @param port Port to be cleared
 */
int riocp_pe_switch_port_clear_enumerated(struct riocp_pe *sw, uint8_t port)
{
	uint32_t offset;
	uint32_t val;
	int ret = 0;

	offset = sw->efptr_phys + RIO_PORT_N_CTL_CSR(port);
	ret = riocp_pe_maint_read(sw, offset, &val);
	if (ret)
		return ret;

	val &= ~RIO_PORT_N_CTL_ENUM_B;
	ret = riocp_pe_maint_write(sw, offset, val);

	return ret;
}

/**
 * Handle incomming event on switch port and set occured events
 * @param sw    Target switch PE
 * @param[out] event Events occured on this port
 */
int riocp_pe_switch_handle_event(struct riocp_pe *sw, struct riomp_mgmt_event *revent,
	struct riocp_pe_event *event)
{
	int timeout = 20;
	while (!sw->sw) {
		usleep(100000);
		timeout--;
		if(timeout <= 0)
			return -ETIMEDOUT;
		RIOCP_ERROR("Waiting for switch 0x%04x:%d (0x%08x) to be set.\n", sw->destid, sw->hopcount, sw->comptag);
	}
	if (sw->sw->event_handler)
		return sw->sw->event_handler(sw, revent, event);
	else
		return -ENOSYS;
}

/**
 * Initialise switch registers
 */
int riocp_pe_switch_init(struct riocp_pe *sw)
{
	if (sw->sw->init)
		return sw->sw->init(sw);
	else
		return -ENOSYS;
}

/**
 * Initialise switch error management
 */
int riocp_pe_switch_init_em(struct riocp_pe *sw)
{
	if (sw->sw->init_em)
		return sw->sw->init_em(sw);
	else
		return -ENOSYS;
}

/**
 * Set switch route
 * @param sw     Target switch PE
 * @param lut    Route lookup table
 * @param destid Destination ID
 * @param port   Output port for destid
 */
int riocp_pe_switch_set_route_entry(struct riocp_pe *sw, uint8_t lut,
	uint32_t destid, uint8_t port)
{
	if(sw->sw->set_route_entry)
		return sw->sw->set_route_entry(sw, lut, destid, port);
	else
		return -ENOSYS;
}

/**
 * Get route for destid to port
 * @param sw     Target switch PE
 * @param lut    Route lookup table
 * @param destid Destination ID
 * @param port   Output port for destid
 */
int riocp_pe_switch_get_route_entry(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint8_t *port)
{
	if (sw->sw->get_route_entry)
		return sw->sw->get_route_entry(sw, lut, destid, port);
	else
		return -ENOSYS;
}

/**
 * Clear switch routing LUT
 * @param sw  Target switch PE
 * @param lut LUT to clear
 */
int riocp_pe_switch_clear_lut(struct riocp_pe *sw, uint8_t lut)
{
	if (sw->sw->clear_lut)
		return sw->sw->clear_lut(sw, lut);
	else
		return -ENOSYS;
}

int riocp_pe_switch_get_lane_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed *speed)
{
	if (sw->sw->get_lane_speed)
		return sw->sw->get_lane_speed(sw, port, speed);
	else
		return -ENOSYS;
}

int riocp_pe_switch_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width)
{
	if (sw->sw->get_lane_width)
		return sw->sw->get_lane_width(sw, port, width);
	else
		return -ENOSYS;
}

int riocp_pe_switch_get_port_state(struct riocp_pe *sw, uint8_t port, riocp_pe_port_state_t *state)
{
	if (sw->sw->get_port_state)
		return sw->sw->get_port_state(sw, port, state);
	else
		return -ENOSYS;
}

int riocp_pe_switch_set_port_speed(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed speed)
{
	if (sw->sw->set_port_speed)
		return sw->sw->set_port_speed(sw, port, speed);
	else
		return -ENOSYS;
}

int riocp_pe_switch_set_domain(struct riocp_pe *sw, uint8_t domain)
{
	if (sw->sw->set_domain)
		return sw->sw->set_domain(sw, domain);
	else
		return -ENOSYS;
}

int riocp_pe_switch_port_enable(struct riocp_pe *sw, uint8_t port)
{
	if (sw->sw->port_enable)
		return sw->sw->port_enable(sw, port);
	else
		return -ENOSYS;
}

int riocp_pe_switch_port_disable(struct riocp_pe *sw, uint8_t port)
{
	if (sw->sw->port_disable)
		return sw->sw->port_disable(sw, port);
	else
		return -ENOSYS;
}

#ifdef __cplusplus
}
#endif
