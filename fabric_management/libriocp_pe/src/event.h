/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file event.h
 * Processing element event management
 */
#ifndef RIOCP_PE_EVENT_H__
#define RIOCP_PE_EVENT_H__

#include "pe.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_EVENT_PW_PORT_ID(portwrite) (portwrite.payload[2] & 0xff)

int RIOCP_WU riocp_pe_event_init(struct riocp_pe *pe);
int RIOCP_WU riocp_pe_event_receive(struct riocp_pe *pe, struct riocp_pe_event *e);

void riocp_pe_event_get_port_mask(riocp_pe_handle pe, uint8_t port,
	riocp_pe_event_mask_t *mask);
void riocp_pe_event_set_port_mask(riocp_pe_handle pe,
	uint8_t port,
	riocp_pe_event_mask_t mask);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_EVENT_H__ */
