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

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_COMPTAG_UNSET 0x00000000

int riocp_pe_comptag_read(struct riocp_pe *pe, uint32_t *comptag);
int riocp_pe_comptag_write(struct riocp_pe *pe, uint32_t comptag);
int riocp_pe_comptag_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t *comptag);
int riocp_pe_comptag_test_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t *comptag);
int riocp_pe_comptag_write_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t comptag);
int riocp_pe_comptag_get_pe(struct riocp_pe *mport, uint32_t comptag, struct riocp_pe **pe);
int riocp_pe_comptag_set(struct riocp_pe *pe, uint32_t comptag);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_COMPTAG_H__ */
