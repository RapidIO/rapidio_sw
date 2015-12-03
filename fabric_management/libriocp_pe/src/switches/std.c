/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * RapidIO Specification Switch Driver
 */
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "inc/riocp_pe_internal.h"

#include "pe.h"
#include "maint.h"
#include "rio_regs.h"
#include "switch.h"

#ifdef __cplusplus
extern "C" {
#endif

static int std_set_route_entry(struct riocp_pe *sw, uint8_t __attribute__((unused))lut,
	uint32_t destid, uint8_t port)
{
	if (riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, destid))
		return -EIO;
	if (riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, port))
		return -EIO;
	return 0;
}

static int std_get_route_entry(struct riocp_pe *sw, uint8_t __attribute__((unused))lut,
	uint32_t destid, uint8_t *port)
{
	uint32_t _port;

	if (riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, destid))
		return -EIO;
	if (riocp_pe_maint_read(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, &_port))
		return -EIO;

	*port = _port;

	return 0;
}

static int std_clear_lut(struct riocp_pe *sw, uint8_t __attribute__((unused))lut)
{
	uint32_t destid = 0;
	int ret = 0;

	for (destid = 0; destid < 256; destid++) {
		ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_DESTID_SEL_CSR, destid);
		if (ret) {
			RIOCP_ERROR("Could not clear lut: %s\n", strerror(-ret));
			return ret;
		}
		ret = riocp_pe_maint_write(sw, RIO_STD_RTE_CONF_PORT_SEL_CSR, RIOCP_PE_SWITCH_PORT_UNMAPPED);
		if (ret) {
			RIOCP_ERROR("Could not clear lut: %s\n", strerror(-ret));
			return ret;
		}
	}

	return 0;
}

struct efb {
	uint16_t ef_ptr;
	uint16_t ef_id;
};

/* TODO(steos): move to generic code
 merge with one of the following functions:
  - riocp_pe_get_efb
  - riocp_pe_get_efptr_phys
  - riocp_pe_get_ef
*/
static int std_get_efptr(struct riocp_pe *sw, uint16_t ef_id, uint16_t *ef_ptr)
{
	int ret = 0;
	uint32_t val;
	uint32_t ptr;

	ret = riocp_pe_maint_read(sw, RIO_ASM_INFO_CAR, &val);
	if (ret) {
		RIOCP_ERROR("Could not parse extended features pointer: %s\n", strerror(-ret));
		return ret;
	}
	ptr = (val & RIO_EXT_FTR_PTR_MASK);

	while (ptr) {
		ret = riocp_pe_maint_read(sw, ptr, &val);
		ptr = (val >> 16);
		RIOCP_TRACE("Found efptr with id 0x%04x\n", (val & RIO_EFB_ID_MASK));
		if ((val & RIO_EFB_ID_MASK) == ef_id) {
			*ef_ptr = ptr;
			return 0;
		}
	}

	return -ENOSYS;
}

/* TODO: move to generic code */
#define RIO_EFID_PORT_MAINT 0x0009

static int std_init(struct riocp_pe *sw)
{
	int ret = 0;

	ret = std_clear_lut(sw, RIOCP_PE_ANY_PORT);
	if (ret) {
		RIOCP_ERROR("Could not clear lut: %s\n", strerror(-ret));
		return ret;
	}

	return ret;
}

static int std_init_em(struct riocp_pe __attribute__((unused))*sw)
{
	return 0;
}

static int std_get_lane_width(struct riocp_pe *sw, uint8_t port, uint8_t *width)
{
	uint32_t val;
	uint16_t efptr;
	int ret;

	ret = std_get_efptr(sw, RIO_EFID_PORT_MAINT, &efptr);
	if (ret) {
		RIOCP_ERROR("Could not get ef_ptr: %s\n", strerror(-ret));
		return ret;
	}

	ret = riocp_pe_maint_read(sw, efptr + RIO_PORT_N_CTL_CSR(port), &val);
	if (ret) {
		RIOCP_ERROR("could not read lane width reg: %s\n", strerror(-ret));
		return ret;
	}

	if (RIO_PORT_WIDTH(val) == RIO_PORT_N_CTL_PWIDTH_4)
		*width = 4;
	else
		*width = 1;

	return 0;
}

static int std_get_port_state(struct riocp_pe *sw, uint8_t port, riocp_pe_port_state_t *state)
{
	int ret;
	uint32_t err_stat;

	ret = riocp_pe_maint_read(sw, RIO_PORT_N_ERR_STATUS(port), &err_stat);
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

struct riocp_pe_switch riocp_pe_switch_std = {
	0,
	-1,
	"std",
	NULL,
	NULL,
	std_init,
	std_init_em,
	std_set_route_entry,
	std_get_route_entry,
	std_clear_lut,
	NULL,
	std_get_lane_width,
	std_get_port_state,
	NULL,
	NULL,
	NULL,
	NULL
};

#ifdef __cplusplus
}
#endif
