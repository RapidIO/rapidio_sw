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
#define RIOCP_PE_COMPTAG_DESTID(destid) ((destid) & 0xffff)
#define RIOCP_PE_COMPTAG_NR(nr) (((nr) << 16) & 0x00ff0000)
#define RIOCP_PE_COMPTAG_GET_NR(comptag) ((comptag & 0x00ff0000) >> 16)
#define RIOCP_PE_COMPTAG_MASK (0x00ffffff)

int riocp_pe_comptag_read(struct riocp_pe *pe, uint32_t *comptag);
int riocp_pe_comptag_write(struct riocp_pe *pe, uint32_t comptag);
int riocp_pe_comptag_init(struct riocp_pe *pe);
int riocp_pe_comptag_set_slot(struct riocp_pe *pe, uint32_t comptag_nr);
int riocp_pe_comptag_get_slot(struct riocp_pe *mport, uint32_t comptag_nr, struct riocp_pe **pe);
int riocp_pe_comptag_read_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t *comptag);
int riocp_pe_comptag_write_remote(struct riocp_pe *mport, uint32_t destid, uint8_t hopcount, uint32_t comptag);
int riocp_pe_comptag_clear(struct riocp_pe *pe);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_COMPTAG_H__ */
