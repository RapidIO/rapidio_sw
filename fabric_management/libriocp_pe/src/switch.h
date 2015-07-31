/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file switch.h
 * RapidIO Node Manager switch driver framework
 */
#ifndef RIOCP_PE_SWITCH_H__
#define RIOCP_PE_SWITCH_H__

#include <stdbool.h>

#include "pe.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_SWITCH_PORT_UNMAPPED 0xff

extern struct riocp_pe_switch riocp_pe_switch_std;
extern struct riocp_pe_switch riocp_pe_switch_tsi57x;
extern struct riocp_pe_switch riocp_pe_switch_cps1848;
extern struct riocp_pe_switch riocp_pe_switch_cps1432;
extern struct riocp_pe_switch riocp_pe_switch_cps1616;
extern struct riocp_pe_switch riocp_pe_switch_sps1616;

int RIOCP_WU riocp_pe_switch_attach_driver(struct riocp_pe *sw, bool initialize);
int RIOCP_WU riocp_pe_switch_port_is_enumerated(struct riocp_pe *sw, uint8_t port);
int RIOCP_WU riocp_pe_switch_port_set_enumerated(struct riocp_pe *sw, uint8_t port);
int RIOCP_WU riocp_pe_switch_port_clear_enumerated(struct riocp_pe *sw, uint8_t port);

/* Callback wrappers */
int RIOCP_WU riocp_pe_switch_init(struct riocp_pe *sw);
int RIOCP_WU riocp_pe_switch_init_em(struct riocp_pe *sw);
int RIOCP_WU riocp_pe_switch_set_route_entry(struct riocp_pe *sw, uint8_t lut,
	uint32_t destid, uint8_t port);
int RIOCP_WU riocp_pe_switch_get_route_entry(struct riocp_pe *sw, uint8_t lut,
	uint32_t destid, uint8_t *port);
int RIOCP_WU riocp_pe_switch_clear_lut(struct riocp_pe *sw, uint8_t lut);
int RIOCP_WU riocp_pe_switch_get_lane_speed(struct riocp_pe *sw, uint8_t port, uint32_t *speed);
int RIOCP_WU riocp_pe_switch_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width);
int RIOCP_WU riocp_pe_switch_get_port_state(struct riocp_pe *sw, uint8_t port, riocp_pe_port_state_t *state);
int RIOCP_WU riocp_pe_switch_handle_event(struct riocp_pe *sw, struct riomp_mgmt_event *revent,
	struct riocp_pe_event *event);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_SWITCH_H__ */
