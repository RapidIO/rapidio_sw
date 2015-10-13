/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file lock.c
 * Processing element lock functions
 */
#include <stdint.h>

#include "inc/riocp_pe_internal.h"

#include "maint.h"
#include "rio_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write lock at hopcount, destid. Make sure the route is programmed!
 */
int riocp_pe_lock_read(struct riocp_pe *pe, uint32_t destid, uint8_t hopcount, uint32_t *lock)
{
	int ret;
	uint32_t _lock;

	if (RIOCP_PE_IS_MPORT(pe)) {
		ret = riocp_pe_maint_read_local(pe, RIO_HOST_DID_LOCK_CSR, &_lock);
		if (ret)
			return -EIO;
	} else {
		ret = riocp_pe_maint_read_remote(pe->mport, destid, hopcount, RIO_HOST_DID_LOCK_CSR, &_lock);
		if (ret)
			return -EIO;
	}

	*lock = _lock & RIO_HOST_LOCK_BASE_ID_MASK;

	return 0;
}

/**
 * Read lock at hopcount, destid. Make sure the route is programmed!
 */
int riocp_pe_lock_write(struct riocp_pe *pe, uint32_t destid, uint8_t hopcount, uint32_t lock)
{
	int ret;

	if (RIOCP_PE_IS_MPORT(pe)) {
		ret = riocp_pe_maint_write_local(pe, RIO_HOST_DID_LOCK_CSR, lock);
		if (ret)
			return -EIO;
	} else {
		ret = riocp_pe_maint_write_remote(pe->mport, destid, hopcount, RIO_HOST_DID_LOCK_CSR, lock);
		if (ret)
			return -EIO;
	}

	return 0;
}

/**
 * Set the lock: read, verify, write, read and verify
 */
int riocp_pe_lock_set(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount)
{
	int ret;
	uint32_t lock;

	RIOCP_TRACE("Set lock d: %u, h: %u\n", destid, hopcount);

	ret = riocp_pe_lock_read(mport, destid, hopcount, &lock);
	if (ret) {
		RIOCP_ERROR("Unable to read lock d: %u, h: %u\n",
			destid, hopcount);
		return -EIO;
	}

	if (lock == mport->destid) {
		RIOCP_DEBUG("Lock already set by mport (d: %u, h: %u, lock: 0x%08x)\n",
			destid, hopcount, lock);
		return 0;
	}

	ret = riocp_pe_lock_write(mport, destid, hopcount, mport->destid);
	if (ret) {
		RIOCP_ERROR("Unable to write lock d: %u, h: %u\n",
			destid, hopcount);
		return -EIO;
	}

	ret = riocp_pe_lock_read(mport, destid, hopcount, &lock);
	if (ret) {
		RIOCP_ERROR("Unable to read lock d: %u, h: %u\n",
			destid, hopcount);
		return -EIO;
	}

	if (lock == mport->destid) {
		RIOCP_DEBUG("Lock set d: %u, h: %u, lock: 0x%08x\n",
			destid, hopcount, lock);
		return 0;
	}

	return -EAGAIN;
}

/**
 * Clear the lock: write, read and verify
 */
int riocp_pe_lock_clear(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount)
{
	int ret;
	uint32_t lock;

	RIOCP_TRACE("Clear lock d: %u, h: %u\n", destid, hopcount);

	ret = riocp_pe_lock_read(mport, destid, hopcount, &lock);
	if (ret) {
		RIOCP_ERROR("Unable to read lock d: %u, h: %u\n",
			destid, hopcount);
		return -EIO;
	}

	RIOCP_TRACE("Lock set to 0x%08x (d: %u, h: %u)\n",
		lock, destid, hopcount);

	if (lock == RIO_HOST_LOCK_BASE_ID_MASK)
		return 0;

	ret = riocp_pe_lock_write(mport, destid, hopcount, mport->destid);
	if (ret) {
		RIOCP_ERROR("Unable to write lock d: %u, h: %u\n",
			destid, hopcount);
		return -EIO;
	}

	ret = riocp_pe_lock_read(mport, destid, hopcount, &lock);
	if (ret) {
		RIOCP_ERROR("Unable to read lock d: %u, h: %u\n",
			destid, hopcount);
		return -EIO;
	}

	RIOCP_TRACE("New lock value 0x%08x (d: %u, h: %u)\n",
		lock, destid, hopcount);

	return 0;
}

#ifdef __cplusplus
}
#endif
