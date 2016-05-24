/*
 * Copyright (c) 2014, Prodrive Technologies
 * Copyright (c) 2016, IDT
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file ctdrv.h
 * Component tag driver and component tag management interface
 */
#ifndef CTDRV_H__
#define CTDRV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CTDRV_UNSET 0x00000000
#define CTDRV_NR(nr) (((nr) << 16) & 0xffff0000)
#define CTDRV_GET_NR(comptag) ((comptag & 0xffff0000) >> 16)

int ct_read(struct riocp_pe *pe, uint32_t *ct);
int ct_write(struct riocp_pe *pe, uint32_t ct);
int ct_init(struct riocp_pe *pe);
int ct_set_slot(struct riocp_pe *pe, uint32_t ct_nr);
int ct_get_slot(struct riocp_pe *mport, uint32_t ct_nr, struct riocp_pe **pe);

#ifdef __cplusplus
}
#endif

#endif /* CTDRV_H__ */
