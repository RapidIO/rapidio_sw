/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file comptag.h
 * Processing element component tag functions
 */
#ifndef RIOCP_PE_COMPTAG_H__
#define RIOCP_PE_COMPTAG_H__

#include "ct.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_COMPTAG_DESTID(ct) ((ct) & 0xffff)

int riocp_pe_comptag_read(struct riocp_pe *pe, ct_t *comptag);
int riocp_pe_comptag_write(struct riocp_pe *pe, ct_t comptag);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_COMPTAG_H__ */
