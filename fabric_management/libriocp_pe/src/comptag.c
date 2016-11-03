/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file comptag.c
 * Processing element component tag functions
 */
#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdint.h>

#include "inc/riocp_pe_internal.h"

#include "maint.h"
#include "comptag.h"
#include "rio_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the component tag
 * @param pe Target PE
 * @param comptag Component tag to read
 * @retval -EIO Could not read to the device
 */
int riocp_pe_comptag_read(struct riocp_pe *pe, uint32_t *comptag)
{
  int ret;

  ret = riocp_pe_maint_read(pe, RIO_COMPONENT_TAG_CSR, comptag);
  if (ret < 0)
    return ret;

  *comptag &= pe->mport->minfo->ct_mask;

  return 0;
}

/**
 * Write the component tag
 * @param pe Target PE
 * @param comptag Component tag to write
 * @retval -EIO Could not write to the device
 *
 * @warning the caller fo this function should hold the Base ID lock
 */
int riocp_pe_comptag_write(struct riocp_pe *pe, uint32_t comptag)
{
  uint32_t _ct = 0;

  if (pe->mport->minfo->ct_mask != UINT32_MAX) {
    int ret = riocp_pe_maint_read(pe, RIO_COMPONENT_TAG_CSR, &_ct);
    if (ret < 0)
      return ret;
  }

  comptag &= pe->mport->minfo->ct_mask;
  _ct &= ~pe->mport->minfo->ct_mask;

  return riocp_pe_maint_write(pe, RIO_COMPONENT_TAG_CSR, comptag | _ct);
}

/**
 * Read the component tag remote via destid/hop
 * @param mport Target PE
 * @param destid Destination ID
 * @param hopcount Destination hopcount
 * @param comptag Component tag to read
 * @retval -EIO Could not read to the device
 */
int riocp_pe_comptag_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t *comptag)
{
  int ret;

  ret = riocp_pe_maint_read_remote(mport, destid, hopcount, RIO_COMPONENT_TAG_CSR, comptag);
  if (ret < 0)
    return ret;

  *comptag &= mport->minfo->ct_mask;

  return 0;
}

/**
 * Read the component tag remote via destid/hop
 *
 * This is a test read which does not log if it fails.
 *
 * @param mport Target PE
 * @param destid Destination ID
 * @param hopcount Destination hopcount
 * @param comptag Component tag to read
 * @retval -EIO Could not read to the device
 */
int riocp_pe_comptag_test_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t *comptag)
{
  int ret;

  ret = riocp_pe_maint_read_remote_silent(mport, destid, hopcount, RIO_COMPONENT_TAG_CSR, comptag);
  if (ret < 0)
    return ret;

  *comptag &= mport->minfo->ct_mask;

  return 0;
}

/**
 * Write the component tag remote via destid/hop
 * @param pe Target PE
 * @param destid Destination ID
 * @param hopcount Destination hopcount
 * @param comptag Component tag to write
 * @retval -EIO Could not write to the device
 *
 * @warning the caller fo this function should hold the Base ID lock
 */
int riocp_pe_comptag_write_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t comptag)
{
  uint32_t _ct;

  if (mport->minfo->ct_mask != UINT32_MAX) {
    int ret = riocp_pe_maint_read_remote(mport, destid, hopcount, RIO_COMPONENT_TAG_CSR, &_ct);
    if (ret < 0)
      return ret;
  }

  comptag &= mport->minfo->ct_mask;
  _ct &= ~mport->minfo->ct_mask;

  return riocp_pe_maint_write_remote(mport, destid, hopcount, RIO_COMPONENT_TAG_CSR, comptag | _ct);
}

/**
 * Search a PE by component tag
 * @param mport The mport that enumerated the PE
 * @param comptag Component tag to search for
 * @param pe Pointer to PE handle
 * @retval 0 The PE was found
 * @retval -ENOENT The PE was not found
 */
int riocp_pe_comptag_get_pe(struct riocp_pe *mport, uint32_t comptag, struct riocp_pe **pe)
{
  struct riocp_pe_llist_item *item;
  uint32_t ct_mask = mport->minfo->ct_mask;

  if (!pe)
    return -EINVAL;

  comptag &= ct_mask;
  riocp_pe_llist_foreach(item, &mport->minfo->handles) {
    struct riocp_pe *p = (struct riocp_pe *)item->data;
    if (p && (p->comptag & ct_mask) == comptag) {
      *pe = p;
      return 0;
    }
  }

  return -ENOENT;
}

/**
 * Write the component tag register and update the cached component tag value
 * @param pe Target PE
 * @param comptag Component tag
 * @retval 0 on success
 * @retval negative error code on error
 */
int riocp_pe_comptag_set(struct riocp_pe *pe, uint32_t comptag)
{
  int ret = 0;

  ret = riocp_pe_comptag_write(pe, comptag);
  if (ret) {
    RIOCP_ERROR("Could not write comptag\n");
    return ret;
  }

  pe->comptag = comptag;

  return 0;
}

#ifdef __cplusplus
}
#endif

