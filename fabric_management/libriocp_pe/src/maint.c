/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file maint.c
 * Processing element maintainance access using librio_maint
 */
#define _XOPEN_SOURCE 500

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include <rapidio_mport_mgmt.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "lock.h"
#include "maint.h"
// #include "switch.h"
#include "handle.h"
#include "rio_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Program the ANY_ID route from hopcount 0 to pe->hopcount in the global switch LUT
 *  it will program according to route in variable pe->address.
 * @note Keep in mind that this function will set the locks of the path!
 * @param pe Target PE
 * @retval 0 When read/write was successfull or skipped
 * @retval -EIO When read/write was unsuccessfull
 */
int riocp_pe_maint_set_anyid_route(struct riocp_pe *pe)
{
	int32_t i;
	int ret = 0;
	uint32_t val;

	if (!RIOCP_PE_IS_HOST(pe))
		return 0;

	/* If the ANY_ID is already programmed for this pe, skip it */
	if (pe == pe->mport->minfo->any_id_target)
		return 0;

	RIOCP_TRACE("Programming ANY_ID route to PE 0x%08x\n", pe->comptag);

	/* Write ANY_ID route until pe */
	for (i = 0; i < pe->hopcount; i++) {

		ret = riocp_pe_lock_set(pe->mport, ANY_ID, i);
		if (ret) {
			RIOCP_TRACE("Could not set lock at hopcount %u\n",
				i);
			ret = -EIO;
			goto err;
		}

		/* Program forward route from host */
		ret = riocp_pe_maint_write_remote(pe->mport, ANY_ID, i,
			RIO_STD_RTE_CONF_DESTID_SEL_CSR, ANY_ID);
		if (ret) {
			ret = -EIO;
			goto err;
		}

		ret = riocp_pe_maint_write_remote(pe->mport, ANY_ID, i,
			RIO_STD_RTE_CONF_PORT_SEL_CSR, pe->address[i]);
		if (ret) {
			ret = -EIO;
			goto err;
		}

		/* Wait for entry to be committed */
		ret = riocp_pe_maint_read_remote(pe->mport, ANY_ID, i,
			RIO_STD_RTE_CONF_PORT_SEL_CSR, &val);
		if (ret) {
			ret = -EIO;
			goto err;
		}

		RIOCP_TRACE("switch[hop: %d] ANY_ID -> port %d programmed\n",
			i, pe->address[i]);
	}

	pe->mport->minfo->any_id_target = pe;

	RIOCP_TRACE("Programming ANY_ID route to PE 0x%08x successfull\n", pe->comptag);

	return ret;

err:
	/* Write ANY_ID route until pe */
	for (; i >= 0; i--) {

		ret = riocp_pe_lock_clear(pe->mport, ANY_ID, i);
		if (ret) {
			RIOCP_TRACE("Could not clear lock at hopcount %u\n",
				i);
			ret = -EIO;
			goto err;
		}
	}

	pe->mport->minfo->any_id_target = NULL;
	RIOCP_TRACE("Error in programming ANY_ID route\n");
	return ret;
}

/**
 * Clear the ANY_ID route locks from pe->hopcount - 1 to 0
 * @note Keep in mind that this function will clear the locks of the path in reverse order!
 * @param pe Target PE
 * @retval 0 When read/write was successfull or skipped
 * @retval -EIO When read/write was unsuccessfull
 */
int riocp_pe_maint_unset_anyid_route(struct riocp_pe *pe)
{
	int32_t i;
	int ret = 0;

	if (!RIOCP_PE_IS_HOST(pe))
		return 0;

	if (RIOCP_PE_IS_MPORT(pe))
		return 0;

	/* If the ANY_ID is already programmed for this pe, skip it */
	if (pe->mport->minfo->any_id_target == NULL)
		return 0;

	RIOCP_TRACE("Unset ANY_ID route locks to PE 0x%08x\n", pe->comptag);

	/* Write ANY_ID route until pe */
	for (i = pe->hopcount; i >= 0; i--) {

		ret = riocp_pe_lock_clear(pe->mport, ANY_ID, i);
		if (ret) {
			RIOCP_TRACE("Could not clear lock at hopcount %u\n",
				i);
			ret = -EIO;
			goto err;
		}
	}

	pe->mport->minfo->any_id_target = NULL;

	RIOCP_TRACE("Unset ANY_ID route to PE 0x%08x successfull\n", pe->comptag);

	return ret;

err:
	pe->mport->minfo->any_id_target = NULL;
	RIOCP_TRACE("Error in unset ANY_ID route\n");
	return ret;
}

/**
 * Maintenance read from local (when mport) or remote device
 * @note  When writing to the remote PE the ANY_ID rioid is always used and not the pe->destid
 * @param pe     Target PE
 * @param offset Offset in the RapidIO maintenance address space
 * @param val    Value of register
 */
int RIOCP_SO_ATTR riocp_pe_maint_read(struct riocp_pe *pe, uint32_t offset, uint32_t *val)
{
	int ret;
	uint32_t destid;

	ret = riocp_pe_handle_check(pe);
	if (ret) {
		RIOCP_ERROR("Handle invalid\n");
		return ret;
	}

	if (RIOCP_PE_IS_HOST(pe))
		destid = ANY_ID;
	else
		destid = pe->destid;

	if (RIOCP_PE_IS_MPORT(pe)) {
		ret = riocp_pe_maint_read_local(pe->mport, offset, val);
		if (ret)
			return -EIO;
	} else {
		/* Program and lock ANY_ID route */
		ret = riocp_pe_maint_set_anyid_route(pe);
		if (ret) {
			RIOCP_ERROR("Could not program ANY_ID to pe: %s\n", strerror(-ret));
			return -EIO;
		}

		ret = riomp_mgmt_rcfg_read(pe->mport->minfo->maint, destid, pe->hopcount, offset,
				     sizeof(*val), val);
		if (ret) {
			RIOCP_ERROR("Read remote error, h: %u, d: %u (0x%08x), o: 0x%08x\n",
				pe->hopcount, destid, destid, offset);
			return -EIO;
		}

		RIOCP_TRACE("Read remote ok, h: %u, d: %u (0x%08x), o: 0x%08x, v: 0x%08x\n",
			pe->hopcount, destid, destid, offset, *val);

		/* Unlock ANY_ID route */
		ret = riocp_pe_maint_unset_anyid_route(pe);
		if (ret) {
			RIOCP_ERROR("Could unset ANY_ID route to pe: %s\n", strerror(-ret));
			return -EIO;
		}
	}

	return ret;
}

/**
 * Maintenance write to local (when mport) or remote device
 * @note  When writing to the remote PE the ANY_ID rioid is always used and not the pe->destid
 * @param pe     Target PE
 * @param offset Offset in the RapidIO maintenance address space
 * @param val    Value to write register
 */
int RIOCP_SO_ATTR riocp_pe_maint_write(struct riocp_pe *pe, uint32_t offset, uint32_t val)
{
	int ret;
	uint32_t destid;

	ret = riocp_pe_handle_check(pe);
	if (ret) {
		RIOCP_ERROR("Handle invalid\n");
		return ret;
	}

	if (RIOCP_PE_IS_HOST(pe))
		destid = ANY_ID;
	else
		destid = pe->destid;

	if (RIOCP_PE_IS_MPORT(pe)) {
		ret = riocp_pe_maint_write_local(pe->mport, offset, val);
		if (ret)
			return -EIO;
	} else {
		/* Program and lock ANY_ID route */
		ret = riocp_pe_maint_set_anyid_route(pe);
		if (ret) {
			RIOCP_ERROR("Could not program ANY_ID to pe: %s\n", strerror(-ret));
			return -EIO;
		}

		RIOCP_TRACE("Write h: %u, d: %u (0x%08x), o: 0x%08x, v: 0x%08x\n",
			pe->hopcount, destid, destid, offset, val);

		ret = riomp_mgmt_rcfg_write(pe->mport->minfo->maint, destid, pe->hopcount, offset, sizeof(val), val);
		if (ret) {
			RIOCP_ERROR("Remote maint write returned error: %s\n", strerror(-ret));
			return -EIO;
		}

		/* Unlock ANY_ID route */
		ret = riocp_pe_maint_unset_anyid_route(pe);
		if (ret) {
			RIOCP_ERROR("Could unset ANY_ID route to pe: %s\n", strerror(-ret));
			return -EIO;
		}
	}

	return ret;
}

/**
 * Maintenance write to local device
 * @param mport    Target mport PE handle
 * @param offset   Offset in the RapidIO address space
 * @param val      Value to write at offset
 * @retval < 0 Error in local maintenance write
 */
int riocp_pe_maint_write_local(struct riocp_pe *mport, uint32_t offset, uint32_t val)
{
	int ret;

	ret = riomp_mgmt_lcfg_write(mport->minfo->maint, offset, sizeof(val), val);
	if (ret) {
		RIOCP_ERROR("Error in local write (o: 0x%08x, v: 0x%08x), %s\n",
			offset, val, strerror(-ret));
		return ret;
	}

	RIOCP_TRACE("o: %04x, v: %08x\n", offset, val);

	return ret;
}

/**
 * Maintenance read from remote device
 * @param mport    Target mport PE handle
 * @param offset   Offset in the RapidIO address space
 * @param val      Value read from offset
 * @retval < 0 Error in local maintenance read
 */
int riocp_pe_maint_read_local(struct riocp_pe *mport, uint32_t offset, uint32_t *val)
{
	int ret;

	ret = riomp_mgmt_lcfg_read(mport->minfo->maint, offset, sizeof(*val), val);
	if (ret) {
		RIOCP_ERROR("Error in local read (o: 0x%08x), %s\n",
			offset, strerror(-ret));
		return ret;
	}

	RIOCP_TRACE("o: %04x, v: %08x\n", offset, *val);

	return ret;
}

/**
 * Maintenance write to remote device
 * @param mport    Target mport PE handle
 * @param hopcount Hopcount to remote
 * @param destid   Destination id of remote
 * @param offset   Offset in the RapidIO address space
 * @param val      Value to write at offset
 * @param < 0 Error in remote maintenance write
 */
int riocp_pe_maint_write_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount,
	uint32_t offset, uint32_t val)
{
	int ret;

	ret = riomp_mgmt_rcfg_write(mport->minfo->maint, destid, hopcount, offset, sizeof(val), val);
	if (ret) {
		RIOCP_ERROR("Error in remote write (d: %u (0x%08x), h: %u, o: 0x%08x, v: 0x%08x), %s\n",
			destid, destid, hopcount, offset, val, strerror(-ret));
		return ret;
	}

	RIOCP_TRACE("d: %u (0x%08x), h: %u, o: 0x%08x, v: 0x%08x\n",
		destid, destid, hopcount, offset, val);

	return ret;
}

/**
 * Maintenance read from remote device
 * @param mport    Target mport PE handle
 * @param hopcount Hopcount to remote
 * @param destid   Destination id of remote
 * @param offset   Offset in the RapidIO address space
 * @param val      Value read from offset
 * @retval < 0 Error in remote maintenance read
 */
int riocp_pe_maint_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount,
	uint32_t offset, uint32_t *val)
{
	int ret;

	ret = riomp_mgmt_rcfg_read(mport->minfo->maint, destid, hopcount, offset, sizeof(*val), val);
	if (ret) {
		RIOCP_ERROR("Error in remote read (d: %u (0x%08x), h: %u, o: 0x%08x), %s\n",
			destid, destid, hopcount, offset, strerror(-ret));
		return ret;
	}

	RIOCP_TRACE("d: %u (0x%08x), h: %u, o: 0x%08x, v: 0x%08x\n",
		destid, destid, hopcount, offset, *val);

	return ret;
}

#ifdef __cplusplus
}
#endif
