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

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "maint.h"
#include "switch.h"
#include "rio_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Clear the enumerated bits on all the switch ports
 * @param sw Target switch PE
 */
int RIOCP_WU riocp_pe_switch_clear_enumerated(struct riocp_pe *sw)
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

#ifdef __cplusplus
}
#endif
