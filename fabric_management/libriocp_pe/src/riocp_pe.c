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

#include <fcntl.h>
#include <dirent.h>

#include "rio_route.h"
#include "rio_ecosystem.h"
#include "rio_standard.h"
#include "lock.h"
#include "riocp_pe.h"
#include "riocp_pe_internal.h"

#include "llist.h"
#include "maint.h"
#include "handle.h"
#include "comptag.h"
#include "did.h"
#include "driver.h"
#include "liblog.h"
#include "ct.h"

#include "rapidio_mport_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct riocp_pe_llist_item _riocp_mport_list_head; /**< List of created mport lists */
did_sz_t riocp_pe_did_sz = dev08_sz;

/**
 * Set the device ID size for network management.  This controls the
 * size of destination IDs used in the entire system.
 * @param[in] did_sz Size of the destination ID, either 8 or 16.
 * @retval 0 When successfully assigned
 * @retval < 0 if anything other than dev08_sz or dev16_sz are passed in.
 */
int riocp_set_did_sz(did_sz_t did_sz)
{
	RIOCP_TRACE("ENTRY: Size is %d\n", did_sz);
	if ((dev08_sz != did_sz) && (dev16_sz != did_sz)) {
		return -EINVAL;
	}
	riocp_pe_did_sz = did_sz;
	RIOCP_TRACE("EXIT\n");
	return 0;
}

did_sz_t RIOCP_WU riocp_get_did_sz(void)
{
	return riocp_pe_did_sz;
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
	DIR *dev_dir = NULL;
	struct dirent *dev_ent = NULL;
	unsigned int _mport;
	unsigned int i = 0;
	size_t _count = 0;
	uint8_t *_list = NULL;

	dev_dir = opendir(RIOCP_PE_DEV_DIR);
	if (dev_dir == NULL) {
		RIOCP_ERROR("Could not open %s\n", RIOCP_PE_DEV_DIR);
		return false;
	}

	/* Iteration 1, find out how many devices */
	while ((dev_ent = readdir(dev_dir)) != NULL) {
		if (dev_ent->d_name[0] == '.' || strstr(dev_ent->d_name, RIOCP_PE_DEV_NAME) == NULL)
			continue;
		ret = sscanf(dev_ent->d_name, RIOCP_PE_DEV_NAME "%u", &_mport);
		if (ret != 1)
			goto err;
		_count++;
	}

	rewinddir(dev_dir);

	_list = (uint8_t *)calloc(_count, sizeof(*_list));
	if (NULL == _list) {
		goto err;
	}

	/* Iteration 2, get the devices */
	while ((dev_ent = readdir(dev_dir)) != NULL) {
		if (dev_ent->d_name[0] == '.' || strstr(dev_ent->d_name, RIOCP_PE_DEV_NAME) == NULL)
			continue;
		ret = sscanf(dev_ent->d_name, RIOCP_PE_DEV_NAME "%u", &_mport);
		if (ret != 1)
			goto err_pass2;
		_list[i] = _mport;
		i++;
	}

	closedir(dev_dir);

	*count = _count;
	*list  = _list;

	return 0;

err_pass2:
	free(_list);
err:
	closedir(dev_dir);
	return -1;
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
	riocp_pe_handle *_pe_list = NULL;
	size_t n;
	struct riocp_pe *p = NULL;
	struct riocp_pe_llist_item *item = NULL;

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
	RIOCP_PE_LLIST_FOREACH(item, &mport->minfo->handles) {
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
	RIOCP_PE_LLIST_FOREACH(item, &mport->minfo->handles) {
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
	bool is_host,
	ct_t *comptag,
	char *name)
{
	struct riocp_pe *pe = NULL;

	if (handle == NULL) {
		return -EINVAL;
	}

	if (rev != RIOCP_PE_LIB_REV) {
		return -ENOTSUP;
	}

	if (riocp_pe_handle_mport_exists(mport, is_host, handle) == 0) {
		return 0;
	}

	if (riocp_pe_handle_create_mport(mport, is_host, &pe, comptag, name)) {
		return -ENOMEM;
	}

	if (is_host) {
		if (riocp_pe_lock_clear(pe, DID_ANY_ID(riocp_pe_did_sz), 0)) {
			return -EAGAIN;
		}

		if (riocp_enable_pe(pe, RIOCP_PE_ANY_PORT)) {
			return -ENOTSUP;
		}
	}

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
	unsigned int rev,
	ct_t *comptag,
	char *name)
{
	return riocp_pe_create_mport_handle(handle, mport, rev, true, comptag,
			name);
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
	unsigned int rev,
	ct_t *comptag,
	char *name)
{
	return riocp_pe_create_mport_handle(handle, mport, rev, false, comptag,
			name);
}

/**
 * Discover peer at given port number of given PE and get a handle for further
 *  use. Use riocp_pe_destroy_handle to clean up after usage. This function is
 *  typically used by agents to determine the network topology.
 * @param pe   Target PE
 * @param port Port number of target PE to traverse and discover
 * @param peer Pointer to riocp_pe_handle
 */
int RIOCP_SO_ATTR riocp_pe_discover(riocp_pe_handle pe, uint8_t port,
		riocp_pe_handle *peer, char *name)
{
	struct riocp_pe *p = NULL;
	hc_t hopcount;
	ct_t comptag = 0;
	did_t did;
	did_t tmp_did;
	did_val_t did_val;
	did_val_t upb;
	pe_rt_val _port = 0;
	int ret;
	ct_t comptag_in = 0;

	RIOCP_TRACE("Discover behind port %d on handle %p\n", port, pe);

	if (peer == NULL) {
		RIOCP_ERROR("invalid handle\n");
		return -EINVAL;
	}

	if (riocp_pe_handle_check(pe)) {
		return -EINVAL;
	}

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

	did = DID_ANY_ID(riocp_pe_did_sz);
	if (RIOCP_PE_IS_MPORT(pe)) {
		goto found;
	}

	/* Check if the handle is already present */
	if (pe->peers != NULL && pe->peers[port].peer != NULL) {
		p = pe->peers[port].peer;
		*peer = p;

		RIOCP_DEBUG("Found handle already in peers: %p\n", *peer);
		return 0;
	}

	if (!RIOCP_PE_IS_SWITCH(pe->cap)) {
		goto found;
	}

	/* Lock */
	ret = riocp_pe_lock(pe, 0);
	if (ret) {
		RIOCP_ERROR("Unable to lock pe 0x%08x (hc %u, addr: %s)\n",
				pe->comptag, pe->hopcount,
				riocp_pe_handle_addr_ntoa(pe->address,
						pe->hopcount));
		return -EAGAIN;
	}

	/* Search for route behind port in switch LUT */
	upb = did_get_value(did) - 1;
	for (did_val = 0; did_val < upb; did_val++) {
		if (did_get(&tmp_did, did_val)) {
			continue;
		}

		ret = riocp_drv_get_route_entry(pe, 0xff, tmp_did, &_port);
		if (ret) {
			RIOCP_ERROR(
					"Unable to get switch route for destid %u\n",
					did_val);
			goto err;
		}

		if ((0xff != _port) && (port == _port)) {
			did = tmp_did;
			goto found_unlock_pe;
		}
	}

	RIOCP_WARN("No route on port %d of PE with comptag 0x%08x\n", port,
			pe->comptag);
	return -ENODEV;

found_unlock_pe:
	ret = riocp_pe_unlock(pe);
	if (ret) {
		RIOCP_ERROR("Unable to unlock pe 0x%08x\n", pe->comptag);
		return -EIO;
	}

found:
	did_val = did_get_value(did);
	RIOCP_TRACE("Found peer d: %u (0x%08x) -> Port %d\n", did_val, did_val,
			_port);

	// initialize the hopcount
	HC_INCR(hopcount, pe->hopcount);

	/* Read comptag */
	ret = riocp_drv_raw_reg_rd(pe->mport, did_val, hopcount, RIO_COMPTAG,
			&comptag);
	if (ret) {
		RIOCP_ERROR("Found not working route d: %u, h: %u\n", did_val,
				hopcount);
		return -EIO;
	}

	RIOCP_DEBUG(
			"Working route on port %u to peer(d: %u (0x%08x), hc: %u, ct 0x%08x\n",
			port, did_val, did_val, hopcount, comptag);

	if (COMPTAG_UNSET == comptag) {
		RIOCP_TRACE("PE found, comptag is not set\n");
		return -ENODEV;
	}

	/* Read/test existing component tag */
	ret = riocp_pe_handle_pe_exists(pe->mport, comptag, &p);
	if (ret == 0) {
		/* Create new handle */
		ret = riocp_pe_handle_create_pe(pe, &p, hopcount, did, port,
				&comptag_in, name);
		if (ret) {
			RIOCP_ERROR(
					"Could not create handle on port %d of ct 0x%08x:%s\n",
					port, pe->comptag, strerror(-ret));
			return -ENOMEM;
		}
		RIOCP_DEBUG("Created new agent handle %p for PE with comptag"
				" 0x%08x, destid 0x%08x\n", p, p->comptag,
				did_val);
	} else if (ret == 1) {
		RIOCP_DEBUG(
				"FOUND existing peer hop %d, port %d, didvid 0x%08x, devinfo 0x%08x, comptag 0x%08x\n",
				p->hopcount, port, p->cap.dev_id,
				p->cap.dev_info, p->comptag);
	} else {
		RIOCP_ERROR("Error in checking if handle exists ret = %d (%s)",
				ret, strerror(-ret));
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
 * @param[in] pe   Point from where to probe
 * @param[in] port Port to probe on pe
 * @param[out] peer New probed peer.  Handle is invalid if the peer was
 *              previously probed, or if this is a new link to a previously
 *              enumerated peer.
 * @param[inout] comptag Points to comptag value configured for connected
 *                device. On exit, when a newpeer is found, contains the
 *                component tag of the connected device.
 * @param name sysfs_name of the peer, if found and configured
 * @param force_ct True if the the peer node is in the configuration file,
 *              so the component tag of the connected peer is known.
 *              False if the peer node is not in the configuration file, so the
 *              peer may or may not be a known node.
 *
 * @note Keep in mind this function always initialises found switches
 * 	(clearing LUTs etc) even
 *  when they are previously found and no handle exists yet!
 * @retval -EINVAL invalid parameters
 * @retval -EPERM Handle has no host capabilities
 * @retval -EIO Error in maintenance access
 * @retval -ENODEV Supplied port is inactive
 * @retvak -ENOMEM Out of memory
 */
int RIOCP_SO_ATTR riocp_pe_probe(riocp_pe_handle pe,
	uint8_t port,
	riocp_pe_handle *peer,
	ct_t *comptag_in,
	char *name,
	bool force_ct)
{
	uint32_t val;
	struct riocp_pe *p = NULL; // Temporary handle pointer
	struct riocp_pe *temp_p = NULL; // Temporary handle pointer
	hc_t hopcount;
	ct_t comptag = 0;
	did_t did;
	uint8_t sw_port = 0;
	int ret, find_ret, verif_ret = 0;

	if (peer == NULL) {
		return -EINVAL;
	}
	if (riocp_pe_handle_check(pe)) {
		return -EINVAL;
	}
	if (!RIOCP_PE_IS_HOST(pe)) {
		return -EPERM;
	}
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap)) {
		return -EINVAL;
	}
	if (NULL == comptag_in) {
		return -EINVAL;
	}

	// initialize the hopcount
	HC_INCR(hopcount, pe->hopcount);

	RIOCP_TRACE("Probe on PE 0x%08x (hopcount %u, port %u)\n",
		pe->comptag, hopcount, port);

	if ((NULL != pe->peers) && (NULL != pe->peers[port].peer)) {
		RIOCP_TRACE("Probe PE 0x%08x (hopcount %u, port %u)"
				" Peer Exists! Comptag 0x%08x\n", pe->comptag,
				hopcount, port, pe->peers[port].peer->comptag);
		*peer = NULL;
		return 0;
	}

	/* Prepare probe (setup route, test if port is active on PE) */
	ret = riocp_pe_probe_prepare(pe, port);
	if (ret) {
		return -EIO;
	}

	temp_p = (struct riocp_pe *)calloc(1, sizeof(struct riocp_pe));
	if (NULL == temp_p) {
		return -ENOMEM;
	}

	*temp_p = *pe;
	temp_p->hopcount = hopcount;
	temp_p->did_reg_val = DID_ANY_VAL(riocp_pe_did_sz);
	temp_p->address = NULL;
	temp_p->mport = pe->mport;
	temp_p->minfo = NULL;
	temp_p->peers = NULL;
	temp_p->port = NULL;
	temp_p->private_data = NULL;

	/* Read component tag on peer */
	ret = riocp_drv_raw_reg_rd(temp_p, ANY_DID_VAL, hopcount,
					RIO_COMPTAG, &comptag);
	if (ret) {
		RIOCP_WARN("Trying reading again component tag on h: %u\n", hopcount);
		ret = riocp_drv_raw_reg_rd(temp_p, ANY_ID, hopcount,
					RIO_COMPTAG, &comptag);
		if (ret) {
			RIOCP_ERROR("Retry read comptag failed on h: %u\n", hopcount);
			free(temp_p);
			goto err_out;
		}
		RIOCP_WARN("Retry read successfull: 0x%08x\n", comptag);
	}

	RIOCP_DEBUG("Probe peer(hc: %u, address: %s,%u) comptag 0x%08x\n",
		hopcount,
		riocp_pe_handle_addr_ntoa(temp_p->address, temp_p->hopcount),
		port, comptag);

	// If the device is accessible, and the component tag value has been
	// dictated in a configuration file, set the component tag value.
	// This ensures that future checks of the device do not accidentally
	// see an old component tag value.
	if (force_ct && (*comptag_in != comptag)) {
		comptag = *comptag_in;
		ret = riocp_drv_raw_reg_wr(temp_p, ANY_DID_VAL, hopcount,
				RIO_COMPTAG, comptag);
		if (ret) {
			RIOCP_ERROR("Update comptag failed on h: %u\n",
					 hopcount);
			free(temp_p);
			goto err_out;
		}
	}
	free(temp_p);

	// Comptag contains the current component tag value of the device.
	// Check if the component tag already exists in the device database.
	find_ret = riocp_pe_find_comptag(pe->mport, comptag, &p);
	if (find_ret < 0) {
		RIOCP_ERROR(
			"Error in checking if handle exists ret = %d (%s)\n",
			-errno, strerror(-errno));
		goto err;
	}

	// The component tag exists in the device database.  However,
	// this could be a stale component tag value on a different
	// device than what is in the database.
	// Check that the device we've found is in fact the
	// device in the database.  If it is not, then it's actually
	// a new device.
	if ((!find_ret) && (!RIOCP_PE_IS_MPORT(p))) {
		verif_ret = riocp_pe_probe_verify_found(pe, port, p);
		if (verif_ret < 0) {
			goto err;
		}
	}

	if (find_ret || (!find_ret && !verif_ret)) {
		// The device is not known to libriocp_pe.
		// Create a new handle with the requested component tag value.
		// Note that this updates the component tag of the device
		RIOCP_DEBUG("Peer not found on mport %u with comptag 0x%08x\n",
			pe->mport->minfo->id, comptag);
		ct_get_destid(&did, *comptag_in);
		ret = riocp_pe_maint_set_route(pe, did, port);
		if (ret) {
			RIOCP_ERROR(
				"Error setting mtx route for comptag 0x%08x\n",
				comptag);
			goto err;
		}
		
		ret = riocp_pe_lock_clear(pe->mport, ANY_DID, hopcount);
		if (ret) {
			RIOCP_ERROR(
			"Failed lock clear, new PE CT 0x%08x on port %d, %s\n",
				pe->comptag, port, strerror(-ret));
			goto err;
		}

		// Create peer handle using new component tag
		ret = riocp_pe_handle_create_pe(pe, &p, hopcount,
				did, port, comptag_in, name);
		if (ret) {
			RIOCP_ERROR(
			"Create peer failed for ct 0x%08x on port %d, %s\n",
				pe->comptag, port, strerror(-ret));
			goto err_out;
		}

		RIOCP_DEBUG("Created PE hop %d p %d vid 0x%08x"
				" devinfo 0x%08x, ct 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info,
			p->comptag);
	} else {
		// Found a device that already exists in the database.
		// Add peer connection to switch device.
		// INFW: This will need to be improved in future to support
		//       endpoints with multiple ports.
		RIOCP_DEBUG("Peer found h: %d p %d vid 0x%08x devinfo 0x%08x"
				" ct 0x%08x\n",
			p->hopcount, port, p->cap.dev_id, p->cap.dev_info,
			p->comptag);

		if (RIOCP_PE_IS_SWITCH(p->cap)) {
			RIOCP_TRACE("Determine connected port, hc %d\n",
						hopcount);
			ret = riocp_drv_raw_reg_rd(p, ANY_ID, hopcount,
					RIO_SW_PORT_INF, &val);
			if (ret) {
				RIOCP_ERROR("Could not read switch port info CAR at hc %u\n", hopcount);
				goto err;
			}
			sw_port = RIO_ACCESS_PORT(val);
			RIOCP_DEBUG("Peer connected to port %d\n", sw_port);
			ret = riocp_pe_add_peer(pe, p, port, sw_port);
			if (ret) {
				RIOCP_ERROR("Could not add peer(p) to pe\n");
				goto err;
			}
		} else {
			RIOCP_ERROR("CONNECTED DEVICE IS NOT A SWITCH???\n");
		}
	}
	*peer = p;

	ret = riocp_pe_maint_unset_anyid_route(p);
	if (ret) {
		RIOCP_ERROR("Error in unset_anyid_route for peer\n");
		goto err_out;
	}
	return 0;
err:
	ret = riocp_pe_maint_unset_anyid_route(pe);
	if (ret) {
		RIOCP_ERROR("Error in unset_anyid_route for peer\n");
	}

err_out:
	did_get(&did, pe->did_reg_val);
	ret = riocp_pe_lock_clear(pe->mport, did, pe->hopcount);
	if (ret) {
		RIOCP_ERROR("Could not clear lock on PE\n");
		return -EIO;
	}
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
	riocp_pe_handle *_peer_list = NULL;

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
 * Destroy a handle to previously probed device. Device must have been probed
 * riocp_create_host_handle, riocp_create_agent_handle, riocp_discover or
 * riocp_probe
 * @param pe Pointer to handle of previously probed PE
 */
int RIOCP_SO_ATTR riocp_pe_destroy_handle(riocp_pe_handle *pe)
{
	if (NULL == pe) {
		return -EINVAL;
	}
	if (riocp_pe_handle_check(*pe)) {
		return -EINVAL;
	}

	RIOCP_TRACE("Destroying handle %p\n", *pe);

	if (RIOCP_PE_IS_MPORT(*pe)) {
		riocp_pe_handle_mport_put(pe);
	} else {
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
	ct_t comptag;

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
	ports[port].id = port;
	ports[port].pe = pe;
	ports[port].peer = pe->peers[port].peer;
	ports[port].peer_port = pe->peers[port].remote_port;

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
	struct riocp_pe *sw = NULL;

	RIOCP_TRACE("ENTRY\n");

	if (ports == NULL)
		return -EINVAL;
	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	if (RIOCP_PE_IS_SWITCH(pe->cap) || RIOCP_PE_IS_MPORT(pe)) {
		RIOCP_DEBUG("Getting state for %d ports\n",
			RIOCP_PE_PORT_COUNT(pe->cap));
		/* Fetch speed and width of all available ports */
		for (i = 0; i < RIOCP_PE_PORT_COUNT(pe->cap); i++) {
			ret = riocp_drv_get_port_state(pe, i, &ports[i].state);
			if (ret) {
				RIOCP_ERROR("Could not get port %u state\n", i);
				return ret;
			}

			ret = riocp_pe_get_ports_add_peer(pe, i, ports);
			if (ret) {
				RIOCP_DEBUG("FAILED: %d\n", ret);
				return ret;
			}
		}
		goto exit;
	} else {
		for (i = 0; i < RIOCP_PE_PORT_COUNT(pe->cap); i++) {
			struct riocp_pe_port_state_t state;

			sw = pe->peers[i].peer;

			if (riocp_pe_handle_check(sw))
				return -EFAULT;

			ret = riocp_drv_get_port_state(sw,
					pe->peers[i].remote_port, &state);
			if (ret < 0) {
				RIOCP_ERROR("Unable to get port %u state\n", i);
				return -EIO;
			}
			if (state.port_ok) {
				RIOCP_DEBUG("%08x port %d is active, speed %d\n",
					sw->comptag, pe->peers[i].remote_port,
					state.port_lane_speed);

			} else {
				RIOCP_DEBUG("[0x%08x:%s:hc %u] Port %u is inactive\n",
					sw->comptag, pe->sysfs_name,  sw->hopcount, pe->peers[i].remote_port);
			}

			ret = riocp_pe_get_ports_add_peer(pe, i, ports);
			if (ret)
				RIOCP_TRACE("EXIT\n");
				return ret;
		}
	}

exit:
	RIOCP_TRACE("EXIT\n");
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
	did_t did;

	/* Handle checks */
	if (riocp_pe_handle_check(pe))
		return -EINVAL;

	/* Check if already locked by mport destid, this also
		makes sure the anyid route is programmed */
	if (riocp_pe_maint_read(pe, RIO_HOST_LOCK, &lock))
		return -EIO;

	lock = lock & RIO_HOST_LOCK_BASE_ID_MASK;

	RIOCP_DEBUG("[ct: 0x%08x] current lock: 0x%08x (pe->mport->did_reg_val: 0x%08x)\n",
		pe->comptag, lock, pe->mport->did_reg_val);

	if (lock == pe->mport->did_reg_val)
		return 0;

	/* Check lock flags */
	did_get(&did, pe->did_reg_val);
	if (flags == 0) {
		if (lock != RIO_HOST_LOCK_BASE_ID_MASK) {
			return -EAGAIN;
		}
		if (riocp_pe_lock_write(pe, did, pe->hopcount,
				pe->mport->did_reg_val)) {
			return -EIO;
		}
	} else if (flags == RIOCP_PE_FLAG_FORCE) {
		/* Force unlock old lock, the lock with our own destid, when old lock is same
		 as mask, dont unlock old lock. See TSI578 errata 0xffff may lock the PE. */
		if ((lock != RIO_HOST_LOCK_BASE_ID_MASK)
				&& (riocp_pe_lock_write(pe, did, pe->hopcount,
						lock))) {
			return -EIO;
		}
		if (riocp_pe_lock_write(pe, did, pe->hopcount,
				pe->mport->did_reg_val)) {
			return -EIO;
		}
	}

	/* Verify the lock is set */
	if (riocp_pe_lock_read(pe, did, pe->hopcount, &lock)) {
		return -EIO;
	}
	if (lock != pe->mport->did_reg_val) {
		return -EAGAIN;
	}

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
	did_t did;
	uint32_t lock;

	if (riocp_pe_handle_check(pe)) {
		return -EINVAL;
	}

	/* Read lock, to program the anyid route for host handles */
	if (riocp_pe_maint_read(pe, RIO_HOST_LOCK, &lock)) {
		return -EIO;
	}

	RIOCP_DEBUG("[ct: 0x%08x] current lock: 0x%08x\n", pe->comptag, lock);

	did_get(&did, pe->did_reg_val);
	if (riocp_pe_lock_clear(pe->mport, did, pe->hopcount)) {
		return -EIO;
	}
	return 0;
}

/**
 * Read destination ID of the given PE. The destination is stored in its Base
 *  Device ID CSR. Note that a different register is used for 8 bit, 16 bit and
 *  32 bit IDs; the correct register is chosen based on the common transport
 *  size of the host.
 * @param pe   Target PE
 * @param did  Destination ID
 */
int RIOCP_SO_ATTR riocp_pe_get_destid(riocp_pe_handle pe, did_t *did)
{
	uint32_t reg_val;

	if (did == NULL) {
		return -EINVAL;
	}

	if (riocp_pe_handle_check(pe)) {
		return -EINVAL;
	}

	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		// No DestID register for a switch, so extract the
		// destID from the component tag value.
		int rc = ct_get_destid(did, pe->comptag);
		RIOCP_TRACE("SWITCH COMPTAG rc %d 0x%x Did 0x%04x Sz %d\n",
			rc, pe->comptag, did->value, did->size);
		return rc;
	}

	if (riocp_pe_maint_read(pe, RIO_DEVID, &reg_val)) {
		return -EIO;
	}
	switch (riocp_pe_did_sz) {
		case dev08_sz: reg_val = GET_DEV8_FROM_HW(reg_val);
				break;
		case dev16_sz: reg_val = GET_DEV16_FROM_HW(reg_val);
				break;
		default:
			RIOCP_ERROR("riocp_pe_did_sz invalid %d", riocp_pe_did_sz);
			return -EINVAL;
	}

	if (did_get(did, reg_val)) {
		return -EINVAL;
	};

	RIOCP_DEBUG("PE 0x%08x has destid %u (0x%08x sz %d exp %d)\n",
			pe->comptag, did_get_value(*did), did_get_value(*did),
			did_get_size(*did), riocp_pe_did_sz);

	return 0;
}

/**
 * Write destination ID of the given PE. The destination ID is written into its
 *  Base Device ID CSR. Note that a different register is used for 8 bit,
 *  16 bit and 32 bit IDs; the correct register is chosen based on the common
 *  transport size of the host. Note that setting or updating the destination
 *  ID of a PE does not imply that routes toward this PE are updated.
 * @param pe  Target PE
 * @param did Destination ID; DID_ANY_DEV8_ID/DID_ANY_DEV16_ID is not allowed
 * @returns
 *    - -EPERM Handle is not allowed to lock this device
 *    - -EACCESS did DID_ANY_DEV8_ID/DID_ANY_DEV16_ID is not allowed
 *    - -ENOSYS Handle is not of switch type
 */
int RIOCP_SO_ATTR riocp_pe_set_destid(riocp_pe_handle pe, did_t did)
{
	did_val_t did_val;
	did_sz_t did_sz;
	did_val_t wr_val = 0xFFFFFFFF;
	int ret;

	ret = riocp_pe_handle_check(pe);
	if (ret) {
		return ret;
	}

	if (!RIOCP_PE_IS_HOST(pe) && !RIOCP_PE_IS_MPORT(pe)) {
		RIOCP_ERROR("Pe is not a host\n");
		return -EPERM;
	}

	if (did_equal(DID_ANY_ID(riocp_pe_did_sz), did)) {
		RIOCP_ERROR("Cannot program DID_ANY_DEV8_ID or DID_ANY_DEV16_ID destid\n");
		return -EACCES;
	}

	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		RIOCP_ERROR("Cannot program destid for switch\n");
		return -ENOSYS;
	}

	did_val = did_get_value(did);
	did_sz = did_get_size(did);
	switch (did_sz) {
		case dev08_sz:
			wr_val = MAKE_HW_FROM_DEV8(did_val);
			break;
		case dev16_sz:
			wr_val = MAKE_HW_FROM_DEV16(did_val);
			break;
		default:
			RIOCP_ERROR("Invalid did size %d", did_sz);
			return -EINVAL;
	}
	ret = riocp_pe_maint_write(pe, RIO_DEVID, wr_val);
	if (ret) {
		return ret;
	}

	pe->did_reg_val = did_val;

	RIOCP_DEBUG("PE 0x%08x destid set to %u (0x%08x)\n", pe->comptag,
			did_val, did_val);

	return 0;
}

int RIOCP_SO_ATTR riocp_pe_reset_port(riocp_pe_handle pe, pe_port_t port,
        bool reset_lp)
{
	int ret;

	ret = riocp_pe_handle_check(pe);
	if (ret)
		return ret;

	if (!RIOCP_PE_IS_HOST(pe) && !RIOCP_PE_IS_MPORT(pe)) {
		RIOCP_ERROR("Pe is not a host\n");
		return -EPERM;
	}

	if (port >= RIOCP_PE_PORT_COUNT(pe->cap)) {
		RIOCP_ERROR("Invalid port number, max is %d",
			RIOCP_PE_PORT_COUNT(pe->cap));
		return -EINVAL;
	}

	ret = riocp_drv_reset_port(pe, port, reset_lp);
	if (ret)
		return ret;

	return 0;
}

/**
 * Obtain the mapping between a given destination ID and egress port of the
 *  given switch and LUT. The number of route LUTs supported by a RapidIO
 *  switch is implementation specific; at least the global route LUT is
 *  supported. This function only works on switch PEs.
 * @param sw   Target switch
 * @param lut  Target route LUT, specify 0xff to address the global route LUT
 * @param did  Destination ID to route
 * @param port Egress port; 0xff indicates unmapped route
 * @retval -EINVAL Invalid handle
 * @retval -ENOSYS Handle is not of switch type
 * @retval -EIO When the lut entry is invalid
 */
int RIOCP_SO_ATTR riocp_sw_get_route_entry(riocp_pe_handle sw, uint8_t lut,
		did_t did, pe_rt_val *port)
{
	int ret;

	if (port == NULL) {
		return -EINVAL;
	}

	if (riocp_pe_handle_check(sw)) {
		return -EINVAL;
	}

	if (!RIOCP_PE_IS_SWITCH(sw->cap)) {
		return -ENOSYS;
	}

	/* Lock PE, because get route entry performs a write/read action */
	ret = riocp_pe_lock(sw, 0);
	if (ret) {
		return -EAGAIN;
	}

	ret = riocp_drv_get_route_entry(sw, lut, did, port);
	if (ret) {
		return -EIO;
	}

	ret = riocp_pe_unlock(sw);
	if (ret){
		return -EIO;
	}

	return 0;
}

/**
 * Modify the mapping between a given destination ID and egress port of the given
 *  switch and LUT. The number of route LUTs supported by a RapidIO switch is
 *  implementation specific; at least the global route LUT is supported. This
 *  function only works on switch PEs.
 * @param sw   Target switch
 * @param lut  Target route LUT, specify 0xff to address the global route LUT
 * @param did  Destination ID to route
 * @param port Egress port; 0xff indicates unmapped route
 */
int RIOCP_SO_ATTR riocp_sw_set_route_entry(riocp_pe_handle sw, uint8_t lut,
		did_t did, pe_rt_val port)
{
	if (riocp_pe_handle_check(sw)) {
		return -EINVAL;
	}

	if (!RIOCP_PE_IS_HOST(sw)) {
		return -EPERM;
	}

	if (!RIOCP_PE_IS_SWITCH(sw->cap)) {
		return -ENOSYS;
	}

	if (did_equal(DID_ANY_ID(riocp_pe_did_sz), did)) {
		return -EACCES;
	}

	return riocp_drv_set_route_entry(sw, lut, did, port);
}

/**
 * Get RapidIO sysfs name string
 * @param pe Target PE
 */
const char *bad_sysfs_name = "INVALID";

const char RIOCP_SO_ATTR *riocp_pe_get_sysfs_name(riocp_pe_handle pe)
{
	if (riocp_pe_handle_check(pe)) {
		return bad_sysfs_name;
	}

	return pe->sysfs_name;
}

/**
 * Get RapidIO device name string based on device id (did)
 * @param pe Target PE
 */
const char *bad_device_name = "NoDevName";

const char RIOCP_SO_ATTR *riocp_pe_get_device_name(riocp_pe_handle pe)
{
	if (riocp_pe_handle_check(pe)) {
		return bad_device_name;
	}

	return pe->dev_name;
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
 * @retval -EINVAL Invalid argument
 * @retval -EBADF  Component tag in device doesn't match with handle
 */
int RIOCP_SO_ATTR riocp_pe_update_comptag(riocp_pe_handle pe, uint32_t wr_did)
{
	int ret;
	ct_t ct;
	did_t did;
	did_val_t did_val;

	RIOCP_INFO("ENTRY\n");
	if (riocp_pe_handle_check(pe)) {
		return -EINVAL;
	}

	ret = riocp_pe_maint_write(pe, RIO_COMPTAG, pe->comptag);
	if (ret) {
		RIOCP_ERROR("Unable to write PE %p component tag\n", pe);
		return ret;
	}

	ret = riocp_pe_maint_read(pe, RIO_COMPTAG, &ct);
	if (ret) {
		RIOCP_ERROR("Unable to read PE %p component tag", pe);
		return ret;
	}

	if (pe->comptag != ct) {
		RIOCP_ERROR("pe->comptag(0x%08x) != ct(0x%08x)\n", pe->comptag,
				ct);
		pe->comptag = ct;
		return -EBADF;
	}

	did_val =  RIOCP_PE_COMPTAG_DESTID(pe->comptag);
	ret = did_get(&did, did_val);
	if (ret) {
		RIOCP_ERROR("Cannot get did from 0x%x rc 0x%x\n",
				did_val, ret);
		return -EBADF;
	}

	if (wr_did) {
		RIOCP_TRACE("Writing device ID to  %x\n", pe->did_reg_val);
		ret = riocp_pe_set_destid(pe, did);
		if (ret)  {
			RIOCP_ERROR("Unable to update device ID\n");
		}
	}
	pe->did_reg_val = did_val;

	RIOCP_INFO("EXIT ret 0x%x\n", ret);
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
        ct_t *comptag)
{
        int ret = 0;
        ct_t ct = 0;

        if (comptag == NULL)
                return -EINVAL;
        if (riocp_pe_handle_check(pe))
                return -EINVAL;

        /* Maint read component tag only when we are host, because
                reading from agent handle will return invalid comptag for switches
                (the ANY_ID route is not set to this pe). We return for agent
                handles always the cached PE handle value */
        if (RIOCP_PE_IS_HOST(pe)) {
                ret = riocp_pe_maint_read(pe, RIO_COMPTAG, &ct);
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

int RIOCP_WU riocp_pe_alloc_ct_did(riocp_pe_handle pe,
								ct_t *ct, did_t *did, did_sz_t did_sz)
{
	if (dev08_sz == did_sz || !pe->did_grp) {
		return ct_create_all(ct, did, did_sz);
	}

	// dev16_sz and pe->did_grp is not NULL.
	if (did_grp_resrv_did(pe->did_grp, did)) {
		RIOCP_ERROR("pe->comptag(0x%08x) could not allocate did & ct\n");
	}
	return ct_create_from_did(ct, *did);
}

#ifdef __cplusplus
}
#endif
