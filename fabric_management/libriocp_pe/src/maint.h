/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file maint.h
 * Processing element maintainance access using librio_maint
 */
#ifndef RIOCP_PE_MAINT_H__
#define RIOCP_PE_MAINT_H__

#include <sys/types.h>
#include "inc/riocp_pe.h"

int RIOCP_WU riocp_pe_maint_write_local(struct riocp_pe *mport, uint32_t offset, uint32_t val);
int RIOCP_WU riocp_pe_maint_read_local(struct riocp_pe *mport, uint32_t offset, uint32_t *val);
int RIOCP_WU riocp_pe_maint_write_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount,
	uint32_t offset, uint32_t val);
int RIOCP_WU riocp_pe_maint_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount,
	uint32_t offset, uint32_t *val);

int RIOCP_WU riocp_pe_maint_set_anyid_route(struct riocp_pe *pe);
int RIOCP_WU riocp_pe_maint_unset_anyid_route(struct riocp_pe *pe);

#endif /* RIOCP_PE_MAINT_H__ */
