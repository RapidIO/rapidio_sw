/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file riocp_pe.c
 * Processing element manager (public API functions)
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <fcntl.h>
#include <dirent.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "llist.h"
#include "maint.h"
#include "event.h"
#include "switch.h"
#include "handle.h"
#include "comptag.h"
#include "rio_regs.h"
#include "rio_devs.h"
#include "liblog.h"

#include <rapidio_mport_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *riocp_switch_cap_desc[] = {
    ALL_CAPABILITIES(CREATE_CAP_STRING)
};

static struct riocp_pe_llist_item _riocp_mport_list_head; /**< List of created mport lists */

static riocp_lock_func_t riocp_lock_func = NULL;

/**
 * Register a callback function to be called by riocp_pe when it enters
 *  a critical region.
 * @param  callback Pointer to the callback function
 */
int riocp_lock_register_callback(riocp_lock_func_t callback)
{
	riocp_lock_func = callback;
	return 0;
}

/**
 * Enter critical region using externally provided callback, if any.
 */
static inline int riocp_lock(void)
{
	return riocp_lock_func ? riocp_lock_func(1) : 0;
}

/**
 * Exit critical region using externally provided callback, if any.
 */
static inline int riocp_unlock(void)
{
	return riocp_lock_func ? riocp_lock_func(0) : 0;
}

/**
 * Search if mport device is available
 * @param  mport Mport number to test availability
 * @retval true  When mport is available
 * @retval false When mport is not available
 */
static bool riocp_pe_mport_available(uint8_t mport)
{
	int ret = riomp_mgmt_mport_available(mport);
	if(ret > 0)
		return true;
	else
		return false;
}

/**
 * Create a list of available mports
 * @note The structure is allocated and needs to be freed by the callee
 * @param[out] count The amount of entries in the list
 * @param[out] list  The list of mport ids
 * @retval 0 When list is successfull created
 * @retval < 0 When there was an error creating the list
 */
static int riocp_pe_mport_list(size_t *count, uint8_t **list)
{
	int ret;
	size_t _count = 0;
	uint8_t *_list;

	ret = riomp_mgmt_mport_list(&_count, NULL);
	if(ret < 0) {
		RIOCP_ERROR("Could not get mport list\n");
		return false;
	}

	_list = (uint8_t *)malloc(_count * sizeof(*_list));
	if(!_list) {
		RIOCP_ERROR("Memory allocation failed\n");
		return false;
	}

	ret = riomp_mgmt_mport_list(&_count, _list);
	if(ret < 0) {
		RIOCP_ERROR("Could not get mport list\n");
		free(_list);
		return false;
	}

	*count = _count;
	*list  = _list;

	return 0;
}

/**
 * Determine available master ports in execution platform. Values returned
 *  through ports can be used as input parameter for riocp_create_host_handle
 *  and riocp_create_agent_handle.
 * @param count Number of available master ports
 * @param ports Pointer to allocated array of available master port numbers
 * @retval -EINVAL ports is NULL
 * @retval -ENOMEM out of memory
 * @retval -EFAULT ports could not be added to _riocp_pe_handles_head
 */
int RIOCP_SO_ATTR riocp_mport_get_port_list(size_t *count,
	uint8_t *ports[])
{
	uint8_t *list = NULL;
	size_t list_size = 0;

	if (count == NULL)
		return -EINVAL;
	if (ports == NULL)
		return -EINVAL;
	if (riocp_pe_mport_list(&list_size, &list) != 0)
		return -EIO;
	if (riocp_pe_llist_add(&_riocp_mport_list_head, list))
		return -EFAULT;

	*count = list_size;
	*ports = list;

	return 0;
}

/**
 * Free array of master ports previously created using riocp_get_mport_list.
 * @param ports Pointer to array of uint8_t
 * @retval -EINVAL Invalid argument
 * @retval -EFAULT Invalid linked list
 */
int RIOCP_SO_ATTR riocp_mport_free_port_list(uint8_t *ports[])
{
	if (ports == NULL || *ports == NULL)
		return -EINVAL;
	if (riocp_pe_llist_del(&_riocp_mport_list_head, *ports))
		return -EFAULT;

	free(*ports);
	*ports = NULL;

	return 0;
}

/**
 * Get list of handles from current internal administration
 * @param      mport Master port PE handle
 * @param[out] pe_list  List of PE handles including mport as first entry
 * @param[out] pe_list_size The amount of PE handles in list
 */
int RIOCP_SO_ATTR riocp_mport_get_pe_list(riocp_pe_handle mport, size_t *count, riocp_pe_handle *pes[])
{
	int ret = 0;
	size_t handle_counter = 0;
	riocp_pe_handle *_pe_list;
	size_t n;
	struct riocp_pe *p;
	struct riocp_pe_llist_item *item;

	if (count == NULL) {
		RIOCP_ERROR("Invalid count argument\n");
		return -EINVAL;
	}

	if (pes == NULL) {
		RIOCP_ERROR("Invalid pes argument\n");
		return -EINVAL;
	}

	ret = riocp_pe_handle_check(mport);
	if (ret) {
		RIOCP_ERROR("Invalid mport handle\n");
		return -EINVAL;
	}

	if (!RIOCP_PE_IS_MPORT(mport)) {
		RIOCP_ERROR("Handle is not a mport\n");
		return -ENOSYS;
	}

	RIOCP_TRACE("Get list of handles behind mport %u\n",
		mport->minfo->id);

	if (*pes != NULL) {
		RIOCP_ERROR("Pointer to be alloced is not NULL\n");
		return -EINVAL;
	}

	/* Include mport handle as first PE in list */
	handle_counter = 1;

	/* Count amount of handles in mport handles list */
	riocp_pe_llist_foreach(item, &mport->minfo->handles) {
		p = (struct riocp_pe *)item->data;
		if (p)
			handle_counter++;
	}

	_pe_list = (struct riocp_pe **)calloc(handle_counter, sizeof(riocp_pe_handle));
	if (_pe_list == NULL) {
		RIOCP_TRACE("Could not allocate handle list\n");
		return -ENOMEM;
	}

	/* Mport handle is first handle in the list */
	_pe_list[0] = mport;

	/* Copy mport handles list pointers to newlist */
	n = 1;
	riocp_pe_llist_foreach(item, &mport->minfo->handles) {
		if (n >= handle_counter)
			break;

		p = (struct riocp_pe *)item->data;
		if (!p)
			continue;

		_pe_list[n] = p;
		n++;
	}

	*pes = _pe_list;
	*count = handle_counter;

	return ret;
}

/**
 * Free pe handle list
 * @param list List to free
 */
int RIOCP_SO_ATTR riocp_mport_free_pe_list(riocp_pe_handle *pes[])
{
	if (pes == NULL) {
		RIOCP_TRACE("Pes argument is NULL\n");
		return -EINVAL;
	}

	if (*pes == NULL) {
		RIOCP_TRACE("Pointer to be freed is NULL\n");
		return -EINVAL;
	}

	free(*pes);

	*pes = NULL;

	return 0;
}

/**
 * Create a mport host/agent handle for further use for the given port on this host. Multiple
 *  subsequent calls to this function with the same parameters result in the
 *  same handle.
 * @param handle  Pointer to riocp_pe_handle
 * @param mport   Master port
 * @param rev     Version number of this library (RIOCP_PE_LIB_REV)
 * @param is_host Create host or agent handle
 * @retval -EINVAL  handle is NULL
 * @retval -ENOTSUP invalid library version
 * @retval -ENOMEM  error in allocating mport handle
 */
static int riocp_pe_create_mport_handle(riocp_pe_handle *handle,
	uint8_t mport,
	unsigned int rev,
	bool is_host)
{
	struct riocp_pe *pe;

	if (handle == NULL)
		return -EINVAL;
	if (rev != RIOCP_PE_LIB_REV)
		return -ENOTSUP;
	if (!riocp_pe_mport_available(mport))
		return -ENODEV;
	if (riocp_pe_handle_mport_exists(mport, is_host, handle) == 0)
		return 0;
	if (riocp_pe_handle_create_mport(mport, is_host, &pe))
		return -ENOMEM;

	*handle = pe;

	return 0;
}

/**
 * Create a handle for further use for the given port on this host. Multiple
 *  subsequent calls to this function with the same parameters result in the
 *  same handle.
 * @param handle Pointer to riocp_pe_handle
 * @param mport  Master port
 * @param rev    Version number of this library (RIOCP_PE_LIB_REV)
 * @retval -EINVAL  handle is NULL
 * @retval -ENOTSUP invalid library version
 * @retval -ENOMEM  error in allocating mport handle
 */
int RIOCP_SO_ATTR riocp_pe_create_host_handle(riocp_pe_handle *handle,
	uint8_t mport,
	unsigned int rev)
{
	return riocp_pe_create_mport_handle(handle, mport, rev, true);
}

/**
 * Create a handle for further use for the given port on this agent. Multiple
 *  subsequent calls to this function with the same parameters result in the
 *  same handle.
 * @param handle Pointer to riocp_pe_handle
 * @param mport  Master port
 * @param rev    Version number of this library (RIOCP_PE_LIB_REV)
 */
int RIOCP_SO_ATTR riocp_pe_create_agent_handle(riocp_pe_handle *handle,
	uint8_t mport,
	unsigned int rev)
{
	return riocp_pe_create_mport_handle(handle, mport, rev, false);
}

/**
 * Discover peer at given port number of given PE and get a handle for further
 *  use. Use riocp_pe_destroy_handle to clean up after usage. This function is
 *  typically used by agents to determine the network topology.
 * @param pe   Target PE
 * @param port Port number of target PE to traverse and discover
 * @param peer Pointer to riocp_pe_handle
 */
int RIOCP_SO_ATTR riocp_pe_discover(riocp_pe_handle pe,
	uint8_t port,
	riocp_pe_handle *peer)
{
	struct riocp_pe *p;
	uint8_t hopcount = 0;
	uint32_t comptag = 0;
	uint32_t destid;
	uint32_t any_id;
	unsigned int i;
	uint16_t _port = 0;
	int ret;

	RIOCP_TRACE("Discover behind port %d on handle %p\n", port, pe);

	if (peer == NULL) {
		RIOCP_ERROR("invalid handle\n");
		return -EINVAL;
	}
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap)) {
		RIOCP_ERROR("invalid port count\n");
		return -EINVAL;
	}
	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		ret = riocp_pe_is_port_active(pe, port);
		if (ret < 0) {
			RIOCP_ERROR("unable to read if port is active\n");
			return -EIO;
		}
		if (ret == 0) {
			RIOCP_ERROR("port inactive\n");
			return -ENODEV;
		}
	}
	if (!RIOCP_PE_IS_MPORT(pe))
		hopcount = pe->hopcount + 1;

	any_id = RIOCP_PE_ANY_ID(pe);
	destid = any_id;

	if (RIOCP_PE_IS_MPORT(pe))
		goto found;

	/* Check if the handle is already present */
	if (pe->peers != NULL && pe->peers[port].peer != NULL) {
		p = pe->peers[port].peer;
		*peer = p;

		RIOCP_DEBUG("Found handle already in peers: %p\n", *peer);

		return 0;
	}

	if (!RIOCP_PE_IS_SWITCH(pe->cap))
		goto found;

	/* Lock */
	ret = riocp_pe_lock(pe, 0);
	if (ret) {
		RIOCP_ERROR("Unable to lock pe 0x%08x (hc %u, addr: %s)\n",
			pe->comptag, pe->hopcount, riocp_pe_handle_addr_ntoa(pe->address, pe->hopcount));
		return -EAGAIN;
	}

	/* Search for route behind port in switch LUT */
	/** @TODO: Find something more performant instead of checking all possible IDs, might be a long runner in big sys_size systems. */
	for (i = 0; i < (any_id - 1); i++) {
		ret = riocp_pe_switch_get_route_entry(pe, 0xff, i, &_port);
		if (ret) {
			RIOCP_ERROR("Unable to get switch route for destid %u\n", i);
			goto err;
		}

		if (RIOCP_PE_IS_EGRESS_PORT(_port)) {
			if (port == _port) {
				destid = i;
				goto found_unlock_pe;
			}
		}
	}

	RIOCP_WARN("No route on port %d of PE with comptag 0x%08x\n", port, pe->comptag);

	return -ENODEV;

found_unlock_pe:

	ret = riocp_pe_unlock(pe);
	if (ret) {
		RIOCP_ERROR("Unable to unlock pe 0x%08x\n", pe->comptag);
		return -EIO;
	}

found:
	RIOCP_TRACE("Found peer d: %u (0x%08x) -> Port %d\n", destid, destid, _port);

	/* Read comptag */
	ret = riocp_pe_comptag_read_remote(pe->mport, destid, hopcount, &comptag);
	if (ret) {
		RIOCP_ERROR("Found not working route d: %u, h: %u\n", destid, hopcount);
		return -EIO;
	}

	RIOCP_DEBUG("Working route on port %u to peer(d: %u (0x%08x), hc: %u, ct 0x%08x\n",
		port, destid, destid, hopcount, comptag);

	if (comptag == RIOCP_PE_COMPTAG_UNSET) {
		RIOCP_TRACE("PE found, comptag is not set\n");
		return -ENODEV;
	}

	/* Read/test existing component tag */
	ret = riocp_pe_handle_pe_exists(pe->mport, comptag, &p);
	if (ret == 0) {
		/* Create new handle */
		ret = riocp_pe_handle_create_pe(pe, &p, hopcount, destid, port);
		if (ret) {
			RIOCP_ERROR("Could not create handle on port %d of ct 0x%08x:%s\n",
				port, pe->comptag, strerror(-ret));
			return -ENOMEM;
		}
		RIOCP_DEBUG("Created new agent handle %p for PE with comptag"
				" 0x%08x, destid 0x%08x\n", p,
				p->comptag, p->destid);
	} else if (ret == 1) {
		RIOCP_DEBUG("FOUND existing peer hop %d, port %d, didvid 0x%08x, devinfo 0x%08x, comptag 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info, p->comptag);
	} else {
		RIOCP_ERROR("Error in checking if handle exists ret = %d (%s)", ret, strerror(-ret));
		return -EIO;
	}

	*peer = p;
	return 0;

err:
	RIOCP_ERROR("Error in discover, unlocking pe 0x%08x\n", pe->comptag);

	ret = riocp_pe_unlock(pe);
	if (ret) {
		RIOCP_ERROR("Unable to unlock pe 0x%08x\n", pe->comptag);
		return -EIO;
	}

	*peer = NULL;
	return -EIO;
}

/**
 * Probe for next peer
 * @param pe   Point from where to probe
 * @param port Port to probe
 * @param peer New probed peer
 * @note Keep in mind this function always initialises found switches (clearing LUTs etc) even
 *  when they are previously found and no handle exists yet!
 * @retval -EPERM Handle has no host capabilities
 */
int RIOCP_SO_ATTR riocp_pe_probe(riocp_pe_handle pe,
	uint8_t port,
	riocp_pe_handle *peer)
{
	uint32_t val;
	struct riocp_pe *p;
	uint8_t hopcount = 0;
	uint32_t comptag = 0;
	uint32_t any_id;
	uint8_t sw_port = 0;
	int ret;

	if (peer == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(pe))
		return -EPERM;
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap))
		return -EINVAL;
	if (!RIOCP_PE_IS_MPORT(pe))
		hopcount = pe->hopcount + 1;

	RIOCP_TRACE("Probe on PE 0x%08x (hopcount %u, port %u)\n",
		pe->comptag, hopcount, port);

	any_id = RIOCP_PE_ANY_ID(pe);

	/* Prepare probe (setup route, test if port is active on PE) */
	ret = riocp_pe_probe_prepare(pe, port);
	if (ret)
		return -EIO;

	/* Read component tag on peer */
	ret = riocp_pe_comptag_read_remote(pe->mport, any_id, hopcount, &comptag);
	if (ret) {
		/* TODO try second time when failed, the ANY_ID route seems to be programmed correctly
			at this point but the route was not working previous read */
		ret = riocp_pe_comptag_read_remote(pe->mport, any_id, hopcount, &comptag);
		if (ret) {
			RIOCP_ERROR("Retry read comptag failed on h: %u\n", hopcount);
			goto err_clear_any_id_route;
		}
		RIOCP_WARN("Retry read successfull: 0x%08x\n", comptag);
	}

	RIOCP_DEBUG("Probe peer(hc: %u, address: %s,%u) comptag 0x%08x\n",
		hopcount, riocp_pe_handle_addr_ntoa(pe->address, pe->hopcount), port, comptag);

	/* Read/test existing handle, based on component tag */
	ret = riocp_pe_handle_pe_exists(pe->mport, comptag, &p);
	if (ret == 0) {
		RIOCP_DEBUG("Peer not found on mport %u with comptag 0x%08x\n",
			pe->mport->minfo->id, comptag);

		ret = riocp_pe_lock_set(pe->mport, any_id, hopcount);
		if (ret) {
			RIOCP_ERROR("Could not lock peer on port %d of ct 0x%08x: %s\n",
				port, pe->comptag, strerror(-ret));
			goto err_clear_any_id_route;
		}

create_pe:
		/* Create peer handle */
		ret = riocp_pe_handle_create_pe(pe, &p, hopcount, any_id, port);
		if (ret) {
			RIOCP_ERROR("Could not create handle for peer on port %d of ct 0x%08x: %s\n",
				port, pe->comptag, strerror(-ret));
			goto err_peer_unlock;
		}

		/* Initialize peer */
		ret = riocp_pe_probe_initialize_peer(p);
		if (ret)
			goto err_destroy_hnd;

		RIOCP_DEBUG("Created PE hop %d, port %d, didvid 0x%08x, devinfo 0x%08x, comptag 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info, p->comptag);

	} else if (ret == 1) {

		ret = riocp_pe_lock_set(pe->mport, any_id, hopcount);
		if (ret) {
			RIOCP_ERROR("Could not lock peer on port %d of ct 0x%08x: %s\n",
				port, pe->comptag, strerror(-ret));
			goto err_clear_any_id_route;
		}

		/* Verify existing handle only when found handle is not a mport */
		if (!RIOCP_PE_IS_MPORT(pe)) {
			ret = riocp_pe_probe_verify_found(pe, port, p);
			if (ret == 0)
				goto create_pe;
			else if (ret < 0)
				goto err_peer_unlock;
		}

		RIOCP_DEBUG("Peer found, h: %d, port %d, didvid 0x%08x, devinfo 0x%08x, comptag 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info, p->comptag);

		/* Peer handle already in list, add PE to peer for network graph */
		if (RIOCP_PE_IS_SWITCH(p->cap)) {
			ret = riocp_pe_maint_read_remote(pe->mport, any_id,
				hopcount, RIO_SWP_INFO_CAR, &val);
			if (ret) {
				RIOCP_ERROR("Could not read switch port info CAR at hc %u\n", hopcount);
				goto err_peer_unlock;
			}
			sw_port = RIO_GET_PORT_NUM(val);

			ret = riocp_pe_add_peer(pe, p, port, sw_port);
			if (ret) {
				RIOCP_ERROR("Could not add peer(p) to pe\n");
				goto err_peer_unlock;
			}
		}

	} else {
		RIOCP_ERROR("Error in checking if handle exists ret = %d (%s)\n",
			ret, strerror(-ret));
		goto err_clear_any_id_route;
	}

	ret = riocp_pe_lock_clear(pe->mport, any_id, hopcount);
	if (ret) {
		RIOCP_ERROR("Could not clear lock on peer\n");
		goto err_clear_any_id_route;
	}

	ret = riocp_pe_maint_unset_anyid_route(pe);
	if (ret) {
		RIOCP_ERROR("Error in unset_anyid_route for peer\n");
		goto err_out;
	}

	*peer = p;
	return 0;


err_destroy_hnd:
	ret = riocp_pe_destroy_handle(&p);
	if (ret) {
		RIOCP_ERROR("Could not destroy peer handle\n");
	}
err_peer_unlock:
	ret = riocp_pe_lock_clear(pe->mport, any_id, hopcount);
	if (ret) {
		RIOCP_ERROR("Could not clear lock on peer\n");
	}
err_clear_any_id_route:
	ret = riocp_pe_maint_unset_anyid_route(pe);
	if (ret) {
		RIOCP_ERROR("Error in unset_anyid_route for peer\n");
	}
err_out:
	return -EIO;
}

/**
 * Get peer on port of PE from internal handle administration. This will
 *  not perform any RapidIO maintenance transactions.
 * @param pe   Target PE
 * @param port Port on PE to peek
 * @retval NULL Invalid argument or no connected peer
 */
riocp_pe_handle RIOCP_SO_ATTR riocp_pe_peek(riocp_pe_handle pe, uint8_t port)
{
	RIOCP_TRACE("Peek on PE %p\n", pe);

	if (riocp_pe_handle_check(pe)) {
		RIOCP_ERROR("Invalid handle\n");
		return NULL;
	}

	RIOCP_TRACE("Peek on PE 0x%08x (dev_id 0x%08x), port %u\n",
		pe->comptag, pe->cap.dev_id, port);

	if (port >= RIOCP_PE_PORT_COUNT(pe->cap)) {
		RIOCP_ERROR("Invalid port\n");
		return NULL;
	}

	RIOCP_TRACE("Peer dev_id 0x%08x\n", pe->peers[port].peer);

	return pe->peers[port].peer;
}

/**
 * Get PE connected peers list
 * @param      pe Target PE
 * @param[out] peer_list  List of handles including mport as first entry
 * @param[out] peer_list_size Amount of handles in list
 */
int RIOCP_SO_ATTR riocp_pe_get_peer_list(riocp_pe_handle pe,
	riocp_pe_handle **peer_list, size_t *peer_list_size)
{
	int ret = 0;
	unsigned int i;
	riocp_pe_handle *_peer_list;

	ret = riocp_pe_handle_check(pe);
	if (ret) {
		RIOCP_TRACE("Invalid pe handle\n");
		return -EINVAL;
	}

	_peer_list = (struct riocp_pe **)
		calloc(RIOCP_PE_PORT_COUNT(pe->cap), sizeof(riocp_pe_handle));
	if (_peer_list == NULL) {
		RIOCP_TRACE("Could not allocate peer handle list\n");
		return -ENOMEM;
	}

	for (i = 0; i < RIOCP_PE_PORT_COUNT(pe->cap); i++)
		_peer_list[i] = pe->peers[i].peer;

	*peer_list = _peer_list;
	*peer_list_size = RIOCP_PE_PORT_COUNT(pe->cap);

	return ret;
}

/**
 * Free peer handle list
 * @param list List to free
 */
int RIOCP_SO_ATTR riocp_pe_free_peer_list(riocp_pe_handle *pes[])
{
	if (*pes == NULL) {
		RIOCP_TRACE("Pointer to be freed is NULL\n");
		return -EINVAL;
	}

	free(*pes);

	*pes = NULL;

	return 0;
}

/**
 * Get peer-PE connected at a PE's port
 * @param[in]    pe: the port's PE
 * @param[in]  port: the port of the PE 
 * @param[out] peer: peer-PE at port of PE
 * @retval        0: Valid peer-PE was found
 * @retval  -EINVAL: PE- or Peer-PE-handle is NULL or invalid port
 * @retval  -ENOENT: No peer-PE found
 * @retval  -EFAULT: Invalid peer-PE
 */
int RIOCP_SO_ATTR 
riocp_pe_get_peer_pe(riocp_pe_handle pe, uint8_t port, riocp_pe_handle *peer){
	
	if(!peer || riocp_pe_handle_check(pe)){
		RIOCP_TRACE("Invalid pe handle\n");
		return -EINVAL;
	}
	
	if(port >= RIOCP_PE_PORT_COUNT(pe->cap)){
		RIOCP_TRACE("Invalid port (%u)\n", port);
		return -EINVAL;
	}

	if(NULL == pe->peers[port].peer)
		return -ENOENT;
		
	if(riocp_pe_handle_check(pe->peers[port].peer)){
		return -EFAULT;
	}
	else{
		*peer = pe->peers[port].peer;
		return 0;	
	}
}


/**
 * Destroy a handle to previously probed device. Device must have been probed
 * riocp_create_host_handle, riocp_create_agent_handle, riocp_discover or
 * riocp_probe
 * @param pe Pointer to handle of previously probed PE
 */
int RIOCP_SO_ATTR riocp_pe_destroy_handle(riocp_pe_handle *pe)
{
	if (pe == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(*pe))
		return -EINVAL;

	RIOCP_TRACE("Destroying handle %p\n", *pe);

	if (RIOCP_PE_IS_MPORT(*pe)){
		riocp_pe_handle_mport_put(pe);
	}
	else{
		riocp_pe_handle_destroy(pe);
		*pe = NULL;
	}

	return 0;
}

/**
 * Verify the device is responsive and still matches that was previously probed
 * or discovered with this handle
 * @param pe Target PE
 * @retval -EINVAL Handle invalid or not known
 * @retval -EIO Unable to read from remote device
 * @retval -EBADF Remote component tag doesn't match handle component tag
 */
int RIOCP_SO_ATTR riocp_pe_verify(riocp_pe_handle pe)
{
	uint32_t comptag;

	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (riocp_pe_comptag_read(pe, &comptag))
		return -EIO;
	if (pe->comptag != comptag)
		return -EBADF;

	return 0;
}

/**
 * Restore match between handle and previously probed/discovered device. Note
 * restoring any additional information, e.g destination ID and/or route LUT contents
 * is the responsibilty of the user
 * @param pe Target PE
 */
int RIOCP_SO_ATTR riocp_pe_restore(riocp_pe_handle pe)
{
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (riocp_pe_comptag_write(pe, pe->comptag))
		return -EIO;

	RIOCP_TRACE("Handle %p restored dev_id: 0x%08x, ct 0x%08x\n", pe, pe->comptag);

	return 0;
}

/**
 * Obtain capabilities of the given PE
 * @param pe Target PE
 * @param capabilities Pointer to struct riocp_pe_capabilities
 */
int RIOCP_SO_ATTR riocp_pe_get_capabilities(riocp_pe_handle pe,
	struct riocp_pe_capabilities *capabilities)
{
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (capabilities == NULL)
		return -EINVAL;

	memcpy(capabilities, &pe->cap, sizeof(struct riocp_pe_capabilities));

	return 0;
}

/**
 * Allocate and add peer to ports[port]
 * @param pe Target PE
 * @param port Port index for ports argument
 * @param ports Ports info list
 */
static int riocp_pe_get_ports_add_peer(struct riocp_pe *pe, uint8_t port, struct riocp_pe_port ports[])
{
	struct riocp_pe *peer;

	peer = pe->peers[port].peer;

	ports[port].id = port;
	ports[port].pe = pe;

	if (peer) {
		ports[port].peer        = peer->port;
		ports[port].peer->pe    = peer;
		ports[port].peer->id    = pe->peers[port].remote_port;
		ports[port].peer->width = ports[port].width;
		ports[port].peer->speed = ports[port].speed;
	} else {
		ports[port].peer = NULL;
	}

	return 0;
}

/**
 * Get list of ports and their current status of the given PE
 * @param pe Target PE
 * @param ports User allocated array of struct riocp_pe_port of sufficient size.
 *   Use riocp_get_capabilities to determine the size needed.
 * @retval -EINVAL Invalid handle
 */
int RIOCP_SO_ATTR riocp_pe_get_ports(riocp_pe_handle pe, struct riocp_pe_port ports[])
{
	int ret;
	unsigned int i;
	struct riocp_pe *sw;


	if (ports == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	/** @todo the local mport speed/port state/lane width is read from the first switch currently
		the MCD driver should extend struct rio_properties with:
			- Port state (active etc.)
			- Lane width
			- Port speed
	*/
	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		/* Fetch speed and width of all available ports */
		for (i = 0; i < RIOCP_PE_PORT_COUNT(pe->cap); i++) {


			/* Check if port is active */
			ret = riocp_pe_is_port_active(pe, i);
			if (ret < 0) {
				RIOCP_ERROR("[0x%08x:%s:hc %u] Unable to read if port %u is active\n",
					pe->comptag, RIOCP_SW_DRV_NAME(pe), pe->hopcount, i);
				return -EIO;
			}
			if (ret == 1) {
				RIOCP_DEBUG("[0x%08x:%s:hc %u] Port %u is active\n",
					pe->comptag, RIOCP_SW_DRV_NAME(pe), pe->hopcount, i);
			}

			ret = riocp_pe_switch_get_lane_speed(pe, i, &ports[i].speed);
			if (ret == -ENOTSUP) {
				ports[i].speed = RIOCP_SPEED_UNKNOWN;
			} else if (ret) {
				RIOCP_ERROR("Could not get port speed for port %u\n", i);
				return ret;
			}

			ret = riocp_pe_switch_get_port_state(pe, i, &ports[i].state);
			if (ret) {
				RIOCP_ERROR("Could not get port %u state\n", i);
				return ret;
			}

			ret = riocp_pe_switch_get_lane_width(pe, i, &ports[i].width);
			if (ret) {
				RIOCP_ERROR("Could not get port %u width\n", i);
				return ret;
			}

			ret = riocp_pe_get_ports_add_peer(pe, i, ports);
			if (ret)
				return ret;
		}
	} else {
		for (i = 0; i < RIOCP_PE_PORT_COUNT(pe->cap); i++) {
			sw = pe->peers[i].peer;

			if (riocp_pe_handle_check(sw))
				return -EFAULT;

			ret = riocp_pe_is_port_active(sw, pe->peers[i].remote_port);
			if (ret < 0) {
				RIOCP_ERROR("Unable to read if port %u is active\n", i);
				return -EIO;
			}
			if (ret == 1) {
				RIOCP_DEBUG("%08x port %d is active, speed %d\n",
					sw->comptag, pe->peers[i].remote_port, ports[i].speed);

				ret = riocp_pe_switch_get_lane_speed(sw,
					pe->peers[i].remote_port, &ports[i].speed);
				if (ret && ret != -ENOSYS) {
					RIOCP_ERROR("Could not get port speed\n");
					return ret;
				}
			} else {
				RIOCP_DEBUG("[0x%08x:%s:hc %u] Port %u is inactive\n",
					sw->comptag, RIOCP_SW_DRV_NAME(sw), sw->hopcount, pe->peers[i].remote_port);

				ports[i].speed = RIOCP_SPEED_UNKNOWN;
			}

			ret = riocp_pe_switch_get_lane_width(sw,
				pe->peers[i].remote_port, &ports[i].width);
			if (ret) {
				RIOCP_ERROR("Could not get port %u width\n", i);
				return ret;
			}

			ret = riocp_pe_get_ports_add_peer(pe, i, ports);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/**
 * Lock the given PE. Locking is achieved by writing the destination ID of this
 *  device into its Host Base Device ID Lock CSR.
 * @param pe Target PE
 * @param flags Additional lock flags
 * @returns
 *    - -EINVAL Invalid handle
 *    - -EPERM Handle is not allowed to lock this device (lock already set)
 *    - -EIO Unable to read from remote device
 */
int RIOCP_SO_ATTR riocp_pe_lock(riocp_pe_handle pe, int flags)
{
	uint32_t lock;

	/* Handle checks */
	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	/* Check if already locked by mport destid, this also
		makes sure the anyid route is programmed */
	if (riocp_pe_maint_read(pe, RIO_HOST_DID_LOCK_CSR, &lock))
		return -EIO;

	lock = lock & RIO_HOST_LOCK_BASE_ID_MASK;

	RIOCP_DEBUG("[ct: 0x%08x] current lock: 0x%08x (pe->mport->destid: 0x%08x)\n",
		pe->comptag, lock, pe->mport->destid);

	if (lock == pe->mport->destid)
		return 0;

	/* Check lock flags */
	if (flags == 0) {
		if (lock != RIO_HOST_LOCK_BASE_ID_MASK)
			return -EAGAIN;
		if (riocp_pe_lock_write(pe, pe->destid, pe->hopcount, pe->mport->destid))
			return -EIO;
	} else if (flags == RIOCP_PE_FLAG_FORCE) {
		/* Force unlock old lock, the lock with our own destid, when old lock is same
			as mask, dont unlock old lock. See TSI578 errata 0xffff may lock the PE. */
		if (lock != RIO_HOST_LOCK_BASE_ID_MASK) {
			if (riocp_pe_lock_write(pe, pe->destid, pe->hopcount, lock))
				return -EIO;
		}
		if (riocp_pe_lock_write(pe, pe->destid, pe->hopcount, pe->mport->destid))
			return -EIO;
	}

	/* Verify the lock is set */
	if (riocp_pe_lock_read(pe, pe->destid, pe->hopcount, &lock))
		return -EIO;
	if (lock != pe->mport->destid)
		return -EAGAIN;

	return 0;
}

/**
 * Unlock the given PE. Unlocking is achieved by writing the destination ID of
 *  this host into its Host Base Device ID Lock CSR. If a different host has
 *  already locked the given PE, unlocking is not allowed. If the given PE
 *  already had been unlocked upon calling this function, it remains unlocked.
 * @returns
 *    - -EINVAL Invalid handle
 *    - -EPERM Handle is not allowed to lock this device
 *    - -EIO Unable to read/write remote device
 */
int RIOCP_SO_ATTR riocp_pe_unlock(riocp_pe_handle pe)
{
	uint32_t lock;

	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	/* Read lock, to program the anyid route for host handles */
	if (riocp_pe_maint_read(pe, RIO_HOST_DID_LOCK_CSR, &lock))
		return -EIO;

	RIOCP_DEBUG("[ct: 0x%08x] current lock: 0x%08x\n", pe->comptag, lock);

	if (riocp_pe_lock_clear(pe->mport, pe->destid, pe->hopcount))
		return -EIO;

	return 0;
}

/**
 * Read destination ID of the given PE. The destination is stored in its Base
 *  Device ID CSR. Note that a different register is used for 8 bit, 16 bit and
 *  32 bit IDs; the correct register is chosen based on the common transport
 *  size of the host.
 * @param pe     Target PE
 * @param destid Destination ID
 */
int RIOCP_SO_ATTR riocp_pe_get_destid(riocp_pe_handle pe,
	uint32_t *destid)
{
	uint32_t _destid;

	if (destid == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (RIOCP_PE_IS_SWITCH(pe->cap) && !RIOCP_PE_IS_MPORT(pe))
		return -ENOSYS;
	if (riocp_pe_maint_read(pe, RIO_DID_CSR, &_destid))
		return -EIO;
	switch (pe->mport->minfo->prop.sys_size) {
	case RIO_SYS_SIZE_8:
		*destid = RIO_DID_GET_BASE_DEVICE_ID(_destid);
		break;
	case RIO_SYS_SIZE_16:
		*destid = RIO_DID_GET_LARGE_DEVICE_ID(_destid);
		break;
	default:
		return -ENOTSUP;
		break;
	}

	RIOCP_DEBUG("PE 0x%08x has destid %u (0x%08x)\n",
		pe->comptag, *destid, *destid);

	return 0;
}

/**
 * Write destination ID of the given PE. The destination ID is written into its
 *  Base Device ID CSR. Note that a different register is used for 8 bit,
 *  16 bit and 32 bit IDs; the correct register is chosen based on the common
 *  transport size of the host. Note that setting or updating the destination
 *  ID of a PE does not imply that routes toward this PE are updated.
 * @param pe     Target PE
 * @param destid Destination ID; ANY_ID is not allowed
 * @returns
 *    - -EPERM Handle is not allowed to lock this device
 *    - -EACCESS destid ANY_ID is not allowed
 *    - -ENOSYS Handle is not of switch type
 */
int RIOCP_SO_ATTR riocp_pe_set_destid(riocp_pe_handle pe,
	uint32_t destid)
{
	int ret;

	ret = riocp_pe_handle_check(pe);
	if (ret)
		return ret;

	if (!RIOCP_PE_IS_HOST(pe)) {
		RIOCP_ERROR("Pe is not a host\n");
		return -EPERM;
	}
	if (destid == RIOCP_PE_ANY_ID(pe)) {
		RIOCP_ERROR("Cannot program ANYID destid\n");
		return -EACCES;
	}
	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		RIOCP_DEBUG("program dummy destid for switch\n");
		pe->destid = destid;
		goto outhere;
	}

	if (RIOCP_PE_IS_MPORT(pe)) {
		ret = riomp_mgmt_destid_set(pe->minfo->maint, destid);
		if ((0x80ab0038 == pe->cap.dev_id) && !ret)
			ret = riocp_pe_maint_write_local(pe, 0x60020, destid);
		
	} else {
		switch (pe->mport->minfo->prop.sys_size) {
		case RIO_SYS_SIZE_8:
			ret = riocp_pe_maint_write(pe, RIO_DID_CSR,
							(destid << 16) & 0x00ff0000);
			break;
		case RIO_SYS_SIZE_16:
			ret = riocp_pe_maint_write(pe, RIO_DID_CSR,
							destid & 0x0000ffff);
			break;
		default:
			ret = -ENOTSUP;
			break;
		}
		if ((0x80ab0038 == pe->cap.dev_id) && !ret)
			ret = riocp_pe_maint_write(pe, 0x60020, destid);
	};
	if (ret)
		return ret;

outhere:
	pe->destid = destid;

	RIOCP_DEBUG("PE 0x%08x destid set to %u (0x%08x)\n",
		pe->comptag, pe->destid, pe->destid);

	return 0;
}

/**
 * Obtain the default routing action for the given switch. This action is
 *  performed when the switch encounters a packet that is not matched in its
 *  route LUT. This function only works on switch PEs.
 * @param sw     Target switch
 * @param action Desired action to take
 * @param port   Default port; ignore when the desired action is to drop the packet
 * @returns
 *    - -EINVAL Invalid handle
 *    - -ENOSYS Handle is not of switch type
 *    - -EIO IO error during communication with device
 */
int RIOCP_SO_ATTR riocp_sw_get_default_route_action(riocp_pe_handle sw,
	enum riocp_sw_default_route_action *action,
	uint8_t *port)
{
	uint32_t _port;

	if (action == NULL || port == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;
	if (riocp_pe_maint_read(sw, RIO_STD_RTE_DEFAULT_PORT, &_port))
		return -EIO;

	if (_port == RIOCP_PE_SWITCH_PORT_UNMAPPED) {
		*action = RIOCP_SW_DEFAULT_ROUTE_DROP;
	} else {
		*action = RIOCP_SW_DEFAULT_ROUTE_UNICAST;
		*port = _port & 0xff;
	}

	return 0;
}

/**
 * Configure the default routing action for the given switch. This action is
 *  performed when the switch encounters a packet that is not matched in its route LUT.
 *  This function only works on switch PEs.
 * @param sw     Target switch
 * @param action Desired action to take
 * @param port   Default port; ignore when the desired action is to drop the packet
 * @retval -EINVAL Invalid handle or action
 * @retval -EPERM Handle is not allowed to set default route
 * @retval -ENOSYS Handle is not of switch type
 * @retval -EIO IO error during communication with device
 */
int RIOCP_SO_ATTR riocp_sw_set_default_route_action(riocp_pe_handle sw,
	enum riocp_sw_default_route_action action,
	uint8_t port)
{
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;
/*
 * FIXME; Validate only "good" port numbers, or link this with
 * the RapidIO Switch API driver.
	if (port >= RIOCP_PE_PORT_COUNT(sw->cap))
		return -EINVAL;
 */

	switch (action) {
	case RIOCP_SW_DEFAULT_ROUTE_DROP:
		if (riocp_pe_maint_write(sw, RIO_STD_RTE_DEFAULT_PORT,
			RIOCP_PE_SWITCH_PORT_UNMAPPED))
			return -EIO;
		break;
	case RIOCP_SW_DEFAULT_ROUTE_UNICAST:
		if (riocp_pe_maint_write(sw, RIO_STD_RTE_DEFAULT_PORT, port))
			return -EIO;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

/**
 * Obtain the mapping between a given destination ID and egress port of the
 *  given switch and LUT. The number of route LUTs supported by a RapidIO
 *  switch is implementation specific; at least the global route LUT is
 *  supported. This function only works on switch PEs.
 * @param sw     Target switch
 * @param lut    Target route LUT, specify 0xff to address the global route LUT
 * @param destid Destination ID to route
 * @param value  Routing table entry value
 * @retval -EINVAL Invalid handle
 * @retval -ENOSYS Handle is not of switch type
 * @retval -EIO When the lut entry is invalid
 */
int RIOCP_SO_ATTR riocp_sw_get_route_entry(riocp_pe_handle sw,
	uint8_t lut,
	uint32_t destid,
	uint16_t *value)
{
	int ret;
	int err = 0;

	if (value == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;

	/* Lock PE, because get route entry performs a write/read action */
	ret = riocp_pe_lock(sw, 0);
	if (ret)
		return -EAGAIN;

	ret = riocp_pe_switch_get_route_entry(sw, lut, destid, value);
	if (ret)
		err = -EIO;

	ret = riocp_pe_unlock(sw);
	if (ret)
		return -EIO;

	return err;
}

/**
 * Modify the mapping between a given destination ID and egress port of the given
 *  switch and LUT. The number of route LUTs supported by a RapidIO switch is
 *  implementation specific; at least the global route LUT is supported. This
 *  function only works on switch PEs.
 * @param sw     Target switch
 * @param lut    Target route LUT, specify 0xff to address the global route LUT
 * @param destid Destination ID to route
 * @param value  Routing table entry value
 */
int RIOCP_SO_ATTR riocp_sw_set_route_entry(riocp_pe_handle sw,
	uint8_t lut,
	uint32_t destid,
	uint16_t value)
{
	int ret;

	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;
/*
 * FIXME: Change this to validate the correct range of devices, or better
 * yet, to use the RapidIO switch APIs...
	if (port > RIOCP_PE_PORT_COUNT(sw->cap) && port != 0xff)
		return -EINVAL;
*/
	if (destid == RIOCP_PE_ANY_ID(sw))
		return -EACCES;

	ret = riocp_pe_switch_set_route_entry(sw, lut, destid, value);

	return ret;
}

/**
 * Clear the given switch’s route LUT. This erases all previously made mappings.
 *  The number of route LUTs supported by a RapidIO switch is implementation
 *  specific; at least the global route LUT is supported. This function only
 *  works on switch PEs.
 * @param sw  Target switch
 * @param lut Target route LUT, specify 0xff to address the global route LUT
 */
int RIOCP_SO_ATTR riocp_sw_clear_lut(riocp_pe_handle sw,
	uint8_t lut)
{
	int ret;

	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;

	ret = riocp_pe_switch_clear_lut(sw, lut);
	return ret;
}

/**
 * Obtain event mask for a given port of the given PE
 * @param pe   This host or any switch
 * @param port Port of PE to set event mask of
 * @param mask Mask that was set for the given PE and port
 */
int RIOCP_SO_ATTR riocp_pe_get_event_mask(riocp_pe_handle pe,
	uint8_t port,
	riocp_pe_event_mask_t *mask)
{
	if (mask == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (RIOCP_PE_IS_MPORT(pe) || !RIOCP_PE_IS_HOST(pe) || !RIOCP_PE_IS_SWITCH(pe->cap))
		return -ENOSYS;
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap))
		return -EINVAL;

	riocp_pe_event_get_port_mask(pe, port, mask);

	return 0;
}

/**
 * Set event mask for a given port of the given PE
 * @param pe This host or any switch
 * @param port Port of PE to set event mask of; use RIOCP_PE_ANY_PORT to set event mask
 *             for all ports
 * @param mask Event mask
 */
int RIOCP_SO_ATTR riocp_pe_set_event_mask(riocp_pe_handle pe,
	uint8_t port,
	riocp_pe_event_mask_t mask)
{
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (RIOCP_PE_IS_MPORT(pe) || !RIOCP_PE_IS_HOST(pe) || !RIOCP_PE_IS_SWITCH(pe->cap))
		return -ENOSYS;
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap) && port != RIOCP_PE_ANY_PORT)
		return -EINVAL;

	riocp_pe_event_set_port_mask(pe, port, mask);

	return 0;
}

/**
 * Read an event from a dedicated mport
 * @param mport mport handle
 * @param pe PE pointer upated by the function with the PE the event comes from
 * @param ev pointer to event data
 * @param timout in milliseconds
 */
int RIOCP_SO_ATTR riocp_pe_event_mport(riocp_pe_handle mport, riocp_pe_handle *pe,
		struct riocp_pe_event *ev, int timeout)
{
	int ret, lock;
	struct riocp_pe_event _e;
	struct riomp_mgmt_event revent;
	uint32_t comptag_nr;
	struct riocp_pe *_pe;

	if (!pe || !ev)
		return -EINVAL;
	if (riocp_pe_handle_check(mport))
		return -EINVAL;
	if (!RIOCP_PE_IS_MPORT(mport))
		return -EINVAL;

	ret = riomp_mgmt_get_event(mport->mp_hnd, &revent, timeout);
	if (ret < 0)
		return ret;
	if (revent.header != RIO_EVENT_PORTWRITE) {
		RIOCP_ERROR("Event not of type RIO_EVENT_PORTWRITE\n");
		return -ENOMSG;
	}

	_e.port  = 0;
	_e.event = 0;

	RIOCP_TRACE("pw[0] = 0x%08x\n", revent.u.portwrite.payload[0]);
	RIOCP_TRACE("pw[1] = 0x%08x\n", revent.u.portwrite.payload[1]);
	RIOCP_TRACE("pw[2] = 0x%08x\n", revent.u.portwrite.payload[2]);
	RIOCP_TRACE("pw[3] = 0x%08x\n", revent.u.portwrite.payload[3]);

	if ((lock = riocp_lock())) {
		RIOCP_ERROR("Failed to acquire external lock (%s)",
			strerror(lock));
		return -EBUSY;
	}

	comptag_nr = RIOCP_PE_COMPTAG_GET_NR(revent.u.portwrite.payload[0]);
	ret = riocp_pe_comptag_get_slot(mport, comptag_nr, &_pe);
	if (ret) {
		RIOCP_ERROR("Failed to retrieve pe for comptag %d\n", comptag_nr);
		goto out;
	}

	ret = riocp_pe_switch_handle_event(_pe, &revent, &_e);
	if (ret) {
		RIOCP_ERROR("Handle event on port %u failed (%s)",
			_e.port, strerror(errno));
		goto out;
	}

	*pe = _pe;

	ev->port  = _e.port;
	ev->event = _e.event;
	ev->counter = _e.counter;

	RIOCP_DEBUG("New event 0x%08"PRIx32" with number %"PRIu32" from (%s:0x%04x:0x%08x.%u) port %"PRIu8" received",
			_e.event, _e.counter, riocp_pe_get_device_name(*pe), _pe->destid, _pe->comptag, _pe->hopcount, _e.port);

out:
	if ((lock = riocp_unlock())) {
		RIOCP_ERROR("Failed to release external lock (%s)",
			strerror(lock));
		return -EBUSY;
	}

	return ret;
}

/**
 * Read an event off the event queue for the given PE.
 * @param pe This host or any switch
 * @param e  Event received from this device
 */
int RIOCP_SO_ATTR riocp_pe_receive_event(riocp_pe_handle pe,
	struct riocp_pe_event *e)
{
	int ret;

	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (RIOCP_PE_IS_MPORT(pe) || !RIOCP_PE_IS_HOST(pe) || !RIOCP_PE_IS_SWITCH(pe->cap))
		return -ENOSYS;

	e->port  = 0;
	e->event = 0;

	ret = riocp_pe_event_receive(pe, e);
	return ret;
}

/**
 * Get RapidIO device name string based on device id (did)
 * @param pe Target PE
 */
const char RIOCP_SO_ATTR *riocp_pe_get_device_name(riocp_pe_handle pe)
{
	unsigned int i;
	uint16_t vid;
	uint16_t did;

	if (riocp_pe_handle_check(pe))
		goto out;

	vid = RIOCP_PE_VID(pe->cap);
	did = RIOCP_PE_DID(pe->cap);

	for (i = 0; i < (sizeof(riocp_pe_device_ids)/sizeof(struct riocp_pe_device_id)); i++)
		if (riocp_pe_device_ids[i].vid == vid && riocp_pe_device_ids[i].did == did)
			return riocp_pe_device_ids[i].name;

out:
	return "unknown device";
}

/**
 * Get rapidio vendor name string based on vendor id (vid)
 * @param pe Target PE
 */
const char RIOCP_SO_ATTR *riocp_pe_get_vendor_name(riocp_pe_handle pe)
{
	unsigned int i;
	uint16_t vid;

	if (riocp_pe_handle_check(pe))
		goto out;

	vid = RIOCP_PE_VID(pe->cap);

	for (i = 0; i < (sizeof(riocp_pe_vendors)/sizeof(struct riocp_pe_vendor)); i++)
		if (riocp_pe_vendors[i].vid == vid)
			return riocp_pe_vendors[i].vendor;

out:
	return "unknown vendor";
}

/**
 * Change the component tag of the given PE.
 * The compent tag is stored in its Component Tag CSR.
 * @param pe      Target PE
 * @param comptag Component tag
 * @param destid  Destination ID for the PE
 * @retval -EINVAL Invalid argument
 * @retval -EBADF  Component tag in device doesn't match with handle
 */
int RIOCP_SO_ATTR riocp_pe_update_comptag(riocp_pe_handle pe,
        uint32_t *comptag, uint32_t did, uint32_t wr_did)
{
        int ret = 0;
        uint32_t ct = 0, new_ct;

        if (riocp_pe_handle_check(pe))
                return -EINVAL;

	if (pe->comptag != *comptag)
                return -EINVAL;

	RIOCP_TRACE("Updating PE handle %p CompTag %x *ct %x\n",
		pe, pe->comptag, *comptag);
	ret = riocp_pe_comptag_read(pe, &ct);
	if (ret) {
		RIOCP_ERROR("Unable to read PE %p component tag", pe);
		return ret;
	}

	if (pe->comptag != ct) {
		RIOCP_ERROR("pe->comptag(0x%08x) != ct(0x%08x)\n", 
			pe->comptag, ct);
		*comptag = ct;
		return -EBADF;
	}

	new_ct = RIOCP_PE_COMPTAG_DESTID(did) |
		(RIOCP_PE_COMPTAG_NR(RIOCP_PE_COMPTAG_GET_NR((*comptag))));

	RIOCP_TRACE("Changing ct %x to %x\n", pe->comptag, new_ct);
	
	ret = riocp_pe_comptag_write(pe, new_ct);
	if (ret) {
		RIOCP_ERROR("Unable to write PE %p component tag\n", pe);
		return ret;
	}

	pe->comptag = new_ct;
	*comptag = new_ct;

	RIOCP_TRACE("Changed pe ct to %x\n", pe->comptag);

	if (wr_did) {
		RIOCP_TRACE("Writing device ID to  %x\n", pe->destid);
		ret = riocp_pe_set_destid(pe, did);
		if (ret) 
			RIOCP_ERROR("Unable to update device ID\n");
	};
	pe->destid = did;

        return ret;
}

/**
 * Read component tag of the given PE. The compent tag is stored in its Component Tag CSR.
 * @param pe      Target PE
 * @param comptag Component tag
 * @retval -EINVAL Invalid argument
 * @retval -EBADF  Component tag in device doesn't match with handle
 */
int RIOCP_SO_ATTR riocp_pe_get_comptag(riocp_pe_handle pe,
        uint32_t *comptag)
{
        int ret = 0;
        uint32_t ct = 0;

        if (comptag == NULL)
                return -EINVAL;
        if (riocp_pe_handle_check(pe))
                return -EINVAL;

        /* Maint read component tag only when we are host, because
                reading from agent handle will return invalid comptag for switches
                (the ANY_ID route is not set to this pe). We return for agent
                handles always the cached PE handle value */
        if (RIOCP_PE_IS_HOST(pe)) {
        		ret = riocp_pe_comptag_read(pe, &ct);
                if (ret) {
                        RIOCP_ERROR("Unable to read component tag");
                        return ret;
                }

                if (pe->comptag != ct) {
                        RIOCP_ERROR("pe->comptag(0x%08x) != ct(0x%08x)\n", pe->comptag, ct);
                        *comptag = ct;
                        return -EBADF;
                }

                *comptag = ct;
        } else {
                *comptag = pe->comptag;
        }

        return ret;
}

/**
 * Read hopcount of the given PE.
 * @param pe      Target PE
 * @param hopcount Hop count
 * @retval -EINVAL Invalid argument
 */
int RIOCP_SO_ATTR riocp_pe_get_hopcount(riocp_pe_handle pe,
		uint8_t *hopcount)
{
        if (hopcount == NULL)
                return -EINVAL;
        if (riocp_pe_handle_check(pe))
                return -EINVAL;

        *hopcount = pe->hopcount;

        return 0;
}

/**
 * Set the speed configuration of a port
 *
 * The functions changes the speed only when the
 * new speed is not already programmed. If the autobaud
 * speed is selected the baud detection is not triggered
 * if there is already a port_ok state. This handling
 * avoids link interruptions due to the low level driver
 * speed setup procedures on already up and running
 * connections.
 *
 * The serdes parameter can be a NULL pointer then no serdes parameters are
 * programmed. For the serdes parameter array it is expected that it ends with
 * a final entry that has the speed property to RIOCP_SPEED_UNKNOWN. If some of
 * the serdes data should not be changed they need to have assigned
 * RIOCP_SERDES_NOVAL as value.
 *
 * @param pe Target PE
 * @param port port number
 * @param speed speed value
 * @param serdes pointer to serdes data array
 * @retval -EINVAL Invalid argument
 */
int RIOCP_WU riocp_pe_set_port_speed(riocp_pe_handle pe, uint8_t port, enum riocp_pe_speed speed, struct riocp_pe_serdes *serdes)
{
	int ret = 0, retr;
	unsigned int i;
	riocp_pe_port_state_t port_state;
	enum riocp_pe_speed current_speed;
	enum riocp_pe_speed supported_speeds[] = {RIOCP_SPEED_1_25G, RIOCP_SPEED_2_5G, RIOCP_SPEED_5_0G, RIOCP_SPEED_6_25G, RIOCP_SPEED_3_125G};
	struct riocp_pe_serdes *serdes_selected;

	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	if (serdes && serdes->speed == RIOCP_SPEED_AUTO)
		return -EINVAL;

	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		if (port >= RIOCP_PE_PORT_COUNT(pe->cap)) {
			ret = -EINVAL;
			RIOCP_ERROR("Port parameter %u exceeds number of available ports %u\n", port, RIOCP_PE_PORT_COUNT(pe->cap));
			goto outhere;
		}

		if (speed == RIOCP_SPEED_AUTO) {
			ret = riocp_pe_switch_get_port_state(pe, port, &port_state);
			if (ret) {
				RIOCP_ERROR("Could not get port %u status\n", port);
				goto outhere;
			}

			RIOCP_INFO("[0x%08x:%s:hc %u] Port %u auto speed port state is %d\n",
				pe->comptag, RIOCP_SW_DRV_NAME(pe), pe->hopcount, port, port_state);

			if (port_state & RIOCP_PE_PORT_STATE_OK)
				goto outhere;

			for (i=0;i<sizeof(supported_speeds)/sizeof(supported_speeds[0]);i++) {
				speed = supported_speeds[i];

				RIOCP_INFO("[0x%08x:%s:hc %u] Port %u test speed %u\n",
					pe->comptag, RIOCP_SW_DRV_NAME(pe), pe->hopcount, port, speed);

				for (serdes_selected = serdes; serdes_selected; serdes_selected++) {
					if (serdes_selected->speed == speed)
						break;
					if (serdes_selected->speed == RIOCP_SPEED_AUTO || serdes_selected->speed == RIOCP_SPEED_UNKNOWN) {
						serdes_selected = NULL;
						break;
					}
				}
				ret = riocp_pe_switch_set_port_speed(pe, port, speed, serdes_selected);
				if (ret) {
					RIOCP_ERROR("Could not set port %u speed\n", port);
					goto outhere;
				}

				retr = 20;
				while(retr > 0) {
					ret = riocp_pe_switch_get_port_state(pe, port, &port_state);
					if (ret) {
						RIOCP_ERROR("Could not get port %u status\n", port);
						goto outhere;
					}
					if (port_state == RIOCP_PE_PORT_STATE_OK)
						goto speedchg;
					retr--;
					usleep(10000);
				}
			}
			goto outhere;
		} else {
			ret = riocp_pe_switch_get_lane_speed(pe, port, &current_speed);
			if (ret) {
				RIOCP_ERROR("Could not get port %u speed\n", port);
				goto outhere;
			}

			if (current_speed == speed)
				goto outhere;

			for (serdes_selected = serdes; serdes_selected; serdes_selected++) {
				if (serdes_selected->speed == speed)
					break;
				if (serdes_selected->speed == RIOCP_SPEED_AUTO || serdes_selected->speed == RIOCP_SPEED_UNKNOWN) {
					serdes_selected = NULL;
					break;
				}
			}
			ret = riocp_pe_switch_set_port_speed(pe, port, speed, serdes_selected);
			if (ret) {
				RIOCP_ERROR("Could not set port %u speed\n", port);
				goto outhere;
			}
		}
speedchg:
		RIOCP_INFO("[0x%08x:%s:hc %u] Port %u speed changed to %u\n",
			pe->comptag, RIOCP_SW_DRV_NAME(pe), pe->hopcount, port, speed);

	} else {
		ret = -ENOTSUP;
		RIOCP_ERROR("Port speed setup not supported for non switch PEs\n");
	}
outhere:
	return ret;
}

/**
 * Announce a PE to the underlaying software stack
 *
 * @param pe Target PE
 * @retval -EINVAL Invalid argument
 */
int RIOCP_WU riocp_pe_announce(riocp_pe_handle pe)
{
	int ret;

	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	if (RIOCP_PE_IS_MPORT(pe)) {
		if (RIOCP_PE_IS_HOST(pe))
			ret = riocp_pe_set_discovered(pe);
		return ret ? ret : -EINVAL;
	}

	ret = riocp_pe_maint_device_add(pe);
	if (ret)
		return ret;

	return riocp_pe_set_discovered(pe);
}

/**
 * Revoke a PE from the underlaying software stack
 *
 * @param pe Target PE
 * @retval -EINVAL Invalid argument
 */
int RIOCP_WU riocp_pe_revoke(riocp_pe_handle pe)
{
	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	if (RIOCP_PE_IS_MPORT(pe))
		return -EINVAL;

	return riocp_pe_maint_device_del(pe);
}

/**
 * Probe for next peer and do link sync if needed
 * @param pe   Point from where to probe
 * @param port Port to probe
 * @param peer New probed peer
 * @param peer_port input port of the peer
 * @note Keep in mind this function always initialises found switches (clearing LUTs etc) even
 *  when they are previously found and no handle exists yet!
 * @retval -EPERM Handle has no host capabilities
 */
int RIOCP_SO_ATTR riocp_pe_probe_sync(riocp_pe_handle pe,
	uint8_t port, uint8_t peer_port,
	riocp_pe_handle *peer)
{
	uint32_t val;
	struct riocp_pe *p;
	uint8_t hopcount = 0;
	uint32_t comptag = 0;
	uint32_t any_id;
	uint8_t sw_port = 0;
	int ret;

	if (peer == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(pe))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(pe))
		return -EPERM;
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap))
		return -EINVAL;
	if (!RIOCP_PE_IS_MPORT(pe))
		hopcount = pe->hopcount + 1;

	RIOCP_TRACE("Probe on PE 0x%08x (hopcount %u, port %u)\n",
		pe->comptag, hopcount, port);

	any_id = RIOCP_PE_ANY_ID(pe);

	/* Prepare probe (setup route, test if port is active on PE) */
	ret = riocp_pe_probe_prepare(pe, port);
	if (ret)
		return -EIO;

	/* Read component tag on peer */
	ret = riocp_pe_comptag_test_read_remote(pe->mport, any_id, hopcount, &comptag);
	if (ret) {
		/* TODO try second time when failed, the ANY_ID route seems to be programmed correctly
			at this point but the route was not working previous read */
		RIOCP_INFO("Trying link sync and reading again component tag on h: %u\n", hopcount);
		ret = riocp_pe_link_sync_peer(pe, port, peer_port);
		if (ret) {
			RIOCP_ERROR("Link sync failed on h: %u\n", hopcount);
		}
		ret = riocp_pe_comptag_read_remote(pe->mport, any_id, hopcount, &comptag);
		if (ret) {
			RIOCP_ERROR("Retry read comptag failed on h: %u\n", hopcount);
			goto err_clear_any_id_route;
		}
		RIOCP_INFO("Retry read successfull: 0x%08x\n", comptag);
	}

	RIOCP_DEBUG("Probe peer(hc: %u, address: %s,%u) comptag 0x%08x\n",
		hopcount, riocp_pe_handle_addr_ntoa(pe->address, pe->hopcount), port, comptag);

	/* Read/test existing handle, based on component tag */
	ret = riocp_pe_handle_pe_exists(pe->mport, comptag, &p);
	if (ret == 0) {
		RIOCP_DEBUG("Peer not found on mport %u with comptag 0x%08x\n",
			pe->mport->minfo->id, comptag);

		ret = riocp_pe_lock_set(pe->mport, any_id, hopcount);
		if (ret) {
			RIOCP_ERROR("Could not lock peer on port %d of ct 0x%08x: %s\n",
				port, pe->comptag, strerror(-ret));
			goto err_clear_any_id_route;
		}

create_pe:
		/* Create peer handle */
		ret = riocp_pe_handle_create_pe(pe, &p, hopcount, any_id, port);
		if (ret) {
			RIOCP_ERROR("Could not create handle for peer on port %d of ct 0x%08x: %s\n",
				port, pe->comptag, strerror(-ret));
			goto err_peer_unlock;
		}

		/* Initialize peer */
		ret = riocp_pe_probe_initialize_peer(p);
		if (ret)
			goto err_destroy_hnd;

		RIOCP_DEBUG("Created PE hop %d, port %d, didvid 0x%08x, devinfo 0x%08x, comptag 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info, p->comptag);

	} else if (ret == 1) {

		ret = riocp_pe_lock_set(pe->mport, any_id, hopcount);
		if (ret) {
			RIOCP_ERROR("Could not lock peer on port %d of ct 0x%08x: %s\n",
				port, pe->comptag, strerror(-ret));
			goto err_clear_any_id_route;
		}

		/* Verify existing handle only when found handle is not a mport */
		if (!RIOCP_PE_IS_MPORT(pe)) {
			ret = riocp_pe_probe_verify_found(pe, port, p);
			if (ret == 0)
				goto create_pe;
			else if (ret < 0)
				goto err_peer_unlock;
		}

		RIOCP_DEBUG("Peer found, h: %d, port %d, didvid 0x%08x, devinfo 0x%08x, comptag 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info, p->comptag);

		/* Peer handle already in list, add PE to peer for network graph */
		if (RIOCP_PE_IS_SWITCH(p->cap)) {
			ret = riocp_pe_maint_read_remote(pe->mport, any_id,
				hopcount, RIO_SWP_INFO_CAR, &val);
			if (ret) {
				RIOCP_ERROR("Could not read switch port info CAR at hc %u\n", hopcount);
				goto err_peer_unlock;
			}
			sw_port = RIO_GET_PORT_NUM(val);

			ret = riocp_pe_add_peer(pe, p, port, sw_port);
			if (ret) {
				RIOCP_ERROR("Could not add peer(p) to pe\n");
				goto err_peer_unlock;
			}
		}

	} else {
		RIOCP_ERROR("Error in checking if handle exists ret = %d (%s)\n",
			ret, strerror(-ret));
		goto err_clear_any_id_route;
	}

	ret = riocp_pe_lock_clear(pe->mport, any_id, hopcount);
	if (ret) {
		RIOCP_ERROR("Could not clear lock on peer\n");
		goto err_clear_any_id_route;
	}

	ret = riocp_pe_maint_unset_anyid_route(pe);
	if (ret) {
		RIOCP_ERROR("Error in unset_anyid_route for peer\n");
		goto err_out;
	}

	*peer = p;
	return 0;


err_destroy_hnd:
	ret = riocp_pe_destroy_handle(&p);
	if (ret) {
		RIOCP_ERROR("Could not destroy peer handle\n");
	}
err_peer_unlock:
	ret = riocp_pe_lock_clear(pe->mport, any_id, hopcount);
	if (ret) {
		RIOCP_ERROR("Could not clear lock on peer\n");
	}
err_clear_any_id_route:
	ret = riocp_pe_maint_unset_anyid_route(pe);
	if (ret) {
		RIOCP_ERROR("Error in unset_anyid_route for peer\n");
	}
err_out:
	return -EIO;
}

/**
 * Set switch domain ID
 * @param sw     switch handle
 * @param domain domain ID
 * @retval -EPERM Handle has no host capabilities
 * @retval -ENOSYS Device does not support domain routing
 */
int RIOCP_SO_ATTR riocp_pe_set_sw_domain(riocp_pe_handle sw, uint8_t domain)
{
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;

	return riocp_pe_switch_set_domain(sw, domain);
}

/**
 * Enable or disable a port on a switch
 *
 * This function handles the port logic and the assigned
 * SERDES. So ti can be used for EMI reduction.
 *
 * @param sw        Target switch
 * @param port      Port to enable or disable
 * @param enable    True for enable the port, false for disable the port
 */
int RIOCP_SO_ATTR riocp_sw_set_port_enable(riocp_pe_handle sw, uint8_t port, bool enable)
{
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;

	if(enable)
		return riocp_pe_switch_port_enable(sw, port);
	else
		return riocp_pe_switch_port_disable(sw, port);
}

/**
 * Add or remove ports from a multicast mask
 * @param sw        Target switch
 * @param maskid    Index of the mask being modified
 * @param port_mask Bitmask with the ports that are being added/removed
 * @param clear     If true the bits in port_mask are being cleared,
 *                  otherwise the bits in port_mask are OR'ed to the
 *                  current mask value
 */
int RIOCP_SO_ATTR riocp_sw_set_multicast_mask(riocp_pe_handle sw,
		uint8_t lut,
		uint8_t maskid,
		uint16_t port_mask,
		bool clear)

{
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;

	return riocp_pe_switch_set_multicast_mask(sw, lut, maskid, port_mask, clear);
}

/**
 * Set the congestion protection for a switch port.
 *
 * The function resets a currently pending congestion state and
 * reprogams the threshold with the given limit value.
 *
 * A limit value of 0 disables the congestion limit function.
 *
 * @param sw        Target switch
 * @param port      Switch port number
 * @param limit     Threshold limit for that port
 */
int RIOCP_SO_ATTR riocp_sw_set_congestion_limit(riocp_pe_handle sw,
		uint8_t port,
		uint16_t limit)
{
	if (riocp_pe_handle_check(sw))
		return -EINVAL;
	if (!RIOCP_PE_IS_HOST(sw))
		return -EPERM;
	if (!RIOCP_PE_IS_SWITCH(sw->cap))
		return -ENOSYS;

	return riocp_pe_switch_set_congestion_limit(sw, port, limit);
}

/**
 * Read switch capabilities
 * @param sw                Target switch
 * @param reg_cap           Capability bit field that specifies the counter
 *                          registers that this switch offers
 */
int RIOCP_SO_ATTR riocp_pe_get_sw_counter_capabilites(riocp_pe_handle sw,
        riocp_sw_cap_t *reg_cap)
{
    if (riocp_pe_handle_check(sw))
        return -EINVAL;
    if (!RIOCP_PE_IS_HOST(sw))
        return -EPERM;
    if (!RIOCP_PE_IS_SWITCH(sw->cap))
        return -ENOSYS;
    return riocp_pe_switch_get_counter_capabilites(sw, reg_cap);
}

/**
 * Read port counter values from switch.
 * @param sw                Target switch
 * @param port              Port ID
 * @param reg_cap           Capability bit field that specifies the counter
 *                          registers that this switch offers
 * @param counter_val       Container to hold the counter register values
 * @param counter_val_size  Size of container for counter values
 */
int RIOCP_SO_ATTR riocp_pe_get_sw_counters(riocp_pe_handle sw, uint8_t port,
        riocp_sw_cap_t reg_cap, uint32_t *reg_values, uint32_t reg_cnt)
{
    if (riocp_pe_handle_check(sw))
        return -EINVAL;
    if (!RIOCP_PE_IS_HOST(sw))
        return -EPERM;
    if (!RIOCP_PE_IS_SWITCH(sw->cap))
        return -ENOSYS;

    return riocp_pe_switch_get_counters(sw, port, reg_cap, reg_values, reg_cnt);
}

/**
 * Read switch trace and filter capabilities
 * @param sw   Target switch
 * @param caps pointer to trace filter capability structure
 */
int RIOCP_SO_ATTR riocp_pe_get_sw_trace_filter_caps(struct riocp_pe *sw, struct riocp_pe_trace_filter_caps *caps)
{
    if (riocp_pe_handle_check(sw))
        return -EINVAL;
    if (!RIOCP_PE_IS_HOST(sw))
        return -EPERM;
    if (!RIOCP_PE_IS_SWITCH(sw->cap))
        return -ENOSYS;

    return riocp_pe_switch_get_trace_filter_caps(sw, caps);
}

/**
 * Set a port trace filter
 * @param sw          Target switch
 * @param port        Port number to put the trace filter on
 * @param filter      Trace filter number
 * @param flags       Trace filter flags
 * @param match_val   pointer to trace match values array
 * @param match_mask  pointer to trace match mask array
 *
 * Based on the flags bits set or cleared the corresponding
 * functionality is switched off or on.
 * If match_val and/or match_mask is NULL the match resgisters
 * are not updated. This can be used to update the flags only
 * for enable/disable the match/trace function for a dedicated
 * filter.
 * If the port is set to RIOCP_PE_ANY_PORT all match_val and
 * match_mask, if not NULL, registers will be udpated for all
 * ports to the same values, but the flags will not be udpated.
 * The flags need to be modified dedicated per port.
 */
int RIOCP_SO_ATTR riocp_pe_set_sw_trace_filter(struct riocp_pe *sw, uint8_t port, uint8_t filter, uint32_t flags, uint32_t *match_val, uint32_t *match_mask)
{
    if (riocp_pe_handle_check(sw))
        return -EINVAL;
    if (!RIOCP_PE_IS_HOST(sw))
        return -EPERM;
    if (!RIOCP_PE_IS_SWITCH(sw->cap))
        return -ENOSYS;

    return riocp_pe_switch_set_trace_filter(sw, port, filter, flags, match_val, match_mask);
}

/**
 * Set the trace port
 * @param sw     Target switch
 * @param port   Trace destination port
 * @param flags  Trace port configuration flags
 */
int RIOCP_SO_ATTR riocp_pe_set_sw_trace_port(struct riocp_pe *sw, uint8_t port, uint32_t flags)
{
    if (riocp_pe_handle_check(sw))
        return -EINVAL;
    if (!RIOCP_PE_IS_HOST(sw))
        return -EPERM;
    if (!RIOCP_PE_IS_SWITCH(sw->cap))
        return -ENOSYS;

    return riocp_pe_switch_set_trace_port(sw, port, flags);
}

#ifdef __cplusplus
}
#endif
