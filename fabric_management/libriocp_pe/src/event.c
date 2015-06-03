/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file event.c
 * Processing element event management
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "pe.h"
#include "event.h"
#include "handle.h"
#include "switch.h"

/**
 * Open the mport for a new filedescriptor for the PE
 *  and set the port-write filter on the comptag
 * @param pe Target PE
 */
int riocp_pe_event_init(struct riocp_pe *pe)
{
	int ret = 0;

	ret = riocp_pe_handle_open_mport(pe);
	if (ret) {
		RIOCP_ERROR("Error open mport\n");
		return ret;
	}

	ret = ioctl(pe->fd, RIO_SET_EVENT_MASK, RIO_PORTWRITE);
	if (ret < 0) {
		RIOCP_ERROR("Could not set portwrite event mask with ioctl (err: %s)\n",
			strerror(errno));
		ret = errno;
		goto err;
	}

	memset(&pe->pwfilter, 0, sizeof(pe->pwfilter));
	pe->pwfilter.mask = 0xffffffff;
	pe->pwfilter.low  = pe->comptag;
	pe->pwfilter.high = pe->comptag;

	ret = ioctl(pe->fd, RIO_ENABLE_PORTWRITE_RANGE, &pe->pwfilter);
	if (ret < 0) {
		RIOCP_ERROR("Could not enable port write range with ioctl (err: %s)\n",
			strerror(errno));
		ret = errno;
		goto err;
	}

	RIOCP_DEBUG("Set eventfd for ct %08x, fd %d\n",
		pe->comptag, pe->fd);

	return 0;

err:
	close(pe->fd);

	RIOCP_TRACE("Error \"%s\"(%d) in eventfd for %08x\n", strerror(ret), ret, pe->comptag);

	return ret;
}

/**
 * Check if event is not masked
 * @retval 0 When event is OK
 * @retval -ENOENT Event is masked (not enabled)
 */
static int riocp_pe_event_check(struct riocp_pe *pe, struct riocp_pe_event *e)
{
	if (e->port >= RIOCP_PE_PORT_COUNT(pe->cap))
		return -EINVAL;
	if (e->event & pe->port_event_mask[e->port])
		return 0;

	return -ENOENT;
}

/**
 * Receive event from PE, this rewrites raw-portwrites to the
 *  riocp_pe_event struct and checks if we expect a port-write from
 *  the event mask
 * @param pe Target PE
 * @param e  Event
 */
/**
 * FIXME: Using one file descriptor per device is not a scalable solution.
 * There is no reason to force the kernel driver to perform categorization of port-writes
 * that could be more easily and quickly performed at user level by this library.
 */
int riocp_pe_event_receive(struct riocp_pe *pe, struct riocp_pe_event *e)
{
	int ret;
	struct riocp_pe_event _e;
	struct rio_event revent;
	ssize_t bytes = 0;

	bytes = read(pe->fd, &revent, sizeof(revent));
	if (bytes == -1)
		return -ENOMSG;
	if (bytes != sizeof(revent)) {
		RIOCP_ERROR("Read incorrect number of %zu bytes from fd %d\n",
			bytes, pe->fd);
		return -EIO;
	}
	if (!(revent.header & RIO_PORTWRITE)) {
		RIOCP_ERROR("Event not of type RIO_PORTWRITE\n", bytes, pe->fd);
		return -ENOMSG;
	}

	_e.port  = 0;
	_e.event = 0;

	RIOCP_TRACE("pw[0] = 0x%08x\n", revent.u.portwrite.payload[0]);
	RIOCP_TRACE("pw[1] = 0x%08x\n", revent.u.portwrite.payload[1]);
	RIOCP_TRACE("pw[2] = 0x%08x\n", revent.u.portwrite.payload[2]);
	RIOCP_TRACE("pw[3] = 0x%08x\n", revent.u.portwrite.payload[3]);

	ret = riocp_pe_switch_handle_event(pe, &revent, &_e);
	if (ret) {
		RIOCP_ERROR("Handle event on port %u failed (%s)",
			_e.port, strerror(errno));
		return ret;
	}

	RIOCP_TRACE("Event received from PE 0x%08x from port %u\n",
		pe->comptag, _e.port);

	ret = riocp_pe_event_check(pe, &_e);
	if (ret) {
		RIOCP_DEBUG("Dropped event for 0x%08x on port %u\n",
			pe->comptag, _e.port);
		return -ENOMSG;
	}

	if (_e.event == 0) {
		RIOCP_DEBUG("No event mask set by switch driver for port %u\n",
			_e.port);
		return -ENOMSG;
	}

	e->port  = _e.port;
	e->event = _e.event;

	return 0;
}

/**
 * Get event port mask from handle
 * @param pe   Target PE
 * @param port Port number of PE
 * @param mask Event mask
 */
void riocp_pe_event_get_port_mask(riocp_pe_handle pe, uint8_t port,
	riocp_pe_event_mask_t *mask)
{
	*mask = pe->port_event_mask[port];
}

/**
 * Set event port mask and enable corresponding error-management bits
 * @param pe   Target PE
 * @param port Port number of PE
 * @param mask Event mask
 */
void riocp_pe_event_set_port_mask(riocp_pe_handle pe, uint8_t port,
	riocp_pe_event_mask_t mask)
{
	unsigned int i = 0;

	if (port == RIOCP_PE_ANY_PORT)
		for (i = 0; i < RIOCP_PE_PORT_COUNT(pe->cap); i++)
			pe->port_event_mask[i] = mask;
	else
		pe->port_event_mask[port] = mask;
}
