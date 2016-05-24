/*
 * Copyright (c) 2014, Prodrive Technologies
 * Copyright (c) 2016, IDT
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file diddrv.h
 * Device ID driver and Device ID management interface
 */
#ifndef DIDDRV_H__
#define DIDDRV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DID_UNSET 0x00000000

int did_read(struct riocp_pe *pe, uint32_t *did);
int did_write(struct riocp_pe *pe, uint32_t did);
int did_init(struct riocp_pe *pe);
int did_set_slot(struct riocp_pe *pe, uint32_t did_nr);
int did_get_slot(struct riocp_pe *mport, uint32_t did_nr, struct riocp_pe **pe);

#ifdef __cplusplus
}
#endif

#endif /* DIDDRV_H__ */
