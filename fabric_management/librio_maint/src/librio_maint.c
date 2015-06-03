/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "../inc/librio_maint.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <linux/rio_mport_cdev.h>

#define MAINT_SO_ATTR __attribute__((visibility("default")))

struct rio_maint {
	int fd;
};

int MAINT_SO_ATTR rio_maint_init(uint8_t mport_id, rio_maint_handle *handle)
{
	struct rio_maint *h;
	char dev[256];

	if (handle == NULL)
		return -EINVAL;

	sprintf(dev, "/dev/rio_mport%u", mport_id);
	h = malloc(sizeof(*h));
	if (!h)
		return -ENOMEM;

	h->fd = open(dev, O_RDWR);
	if (h->fd < 0) {
		free(h);
		return -ENODEV;
	}

	*handle = h;
	return 0;
}

void MAINT_SO_ATTR rio_maint_shutdown(rio_maint_handle *handle)
{
	if (!handle || !*handle)
		return;

	free(*handle);
	*handle = NULL;
}

int MAINT_SO_ATTR rio_maint_read_local(rio_maint_handle handle, uint32_t offset, uint32_t *data)
{
	uint32_t loc_data;
	struct rio_mport_maint_io io = {
		.rioid = 0,
		.hopcount = 0,
		.offset = offset,
		.length = 4,
		.u.buffer = &loc_data,
	};

	if (!handle)
		return -EINVAL;

	if (ioctl(handle->fd, RIO_MPORT_MAINT_READ_LOCAL, &io) < 0)
		return -EIO;

	*data = loc_data;
	return 0;
}

int MAINT_SO_ATTR rio_maint_write_local(rio_maint_handle handle, uint32_t offset, uint32_t data)
{
	uint32_t loc_data = data;
	struct rio_mport_maint_io io = {
		.rioid = 0,
		.hopcount = 0,
		.offset = offset,
		.length = 4,
		.u.buffer = &loc_data,
	};

	if (!handle)
		return -EINVAL;

	if (ioctl(handle->fd, RIO_MPORT_MAINT_WRITE_LOCAL, &io) < 0)
		return -EIO;

	return 0;
}

int MAINT_SO_ATTR rio_maint_read_remote(rio_maint_handle handle, uint32_t dest_id,
		uint8_t hop_count, uint32_t offset,
		uint32_t *data, uint32_t word_count)
{
	uint32_t loc_data;
	struct rio_mport_maint_io io = {
		.rioid = dest_id,
		.hopcount = hop_count,
		.offset = offset,
		.length = 4,
		.u.buffer = &loc_data,
	};

	if (!handle || (word_count != 1))
		return -EINVAL;

	if (ioctl(handle->fd, RIO_MPORT_MAINT_READ_REMOTE, &io) < 0)
		return -EIO;

	*data = loc_data;
	return 0;
}

int MAINT_SO_ATTR rio_maint_write_remote(rio_maint_handle handle, uint32_t dest_id, uint8_t hop_count,
		uint32_t offset, uint32_t *data, uint32_t word_count)
{
	uint32_t loc_data = *data;
	struct rio_mport_maint_io io = {
		.rioid = dest_id,
		.hopcount = hop_count,
		.offset = offset,
		.length = 4,
		.u.buffer = &loc_data,
	};

	if (!handle || (word_count != 1))
		return -EINVAL;

	if (ioctl(handle->fd, RIO_MPORT_MAINT_WRITE_REMOTE, &io) < 0)
		return -EIO;

	return 0;
}
