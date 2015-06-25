/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBRIO_MAINT_H_
#define _LIBRIO_MAINT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIBRIO_MAINT_REV_NR	1

struct rio_maint {
        int fd;
};

typedef struct rio_maint *rio_maint_handle;

int rio_maint_init(uint8_t mport_id, rio_maint_handle *handle);
void rio_maint_shutdown(rio_maint_handle *handle);
int rio_maint_read_local(rio_maint_handle handle, uint32_t offset, uint32_t *data);
int rio_maint_write_local(rio_maint_handle handle, uint32_t offset, uint32_t data);
int rio_maint_read_remote(rio_maint_handle handle, uint32_t dest_id, uint8_t hop_count, uint32_t offset, uint32_t *data, uint32_t word_count);
int rio_maint_write_remote(rio_maint_handle handle, uint32_t dest_id, uint8_t hop_count, uint32_t offset, uint32_t *data, uint32_t word_count);

#ifdef __cplusplus
}
#endif

#endif /* _LIBRIO_MAINT_H_ */
