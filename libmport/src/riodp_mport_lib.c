/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * RapidIO mport device API library
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License(GPL) Version 2, or the BSD-3 Clause license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>

#include <linux/rio_cm_cdev.h>
#define CONFIG_RAPIDIO_DMA_ENGINE
#include <linux/rio_mport_cdev.h>

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_rdma.h>
#include <rapidio_mport_sock.h>

#define RIO_MPORT_DEV_PATH "/dev/rio_mport"
#define RIO_CMDEV_PATH "/dev/rio_cm"

struct rapidio_mport_mailbox {
	int fd;
	uint8_t mport_id;
};

struct rio_channel {
	uint16_t id;
	uint32_t remote_destid;
	uint32_t remote_mbox;
	uint16_t remote_channel;
	uint8_t mport_id;
};

struct rapidio_mport_socket {
	struct rapidio_mport_mailbox *mbox;
	struct rio_channel ch;
	uint8_t	*rx_buffer;
	uint8_t	*tx_buffer;
};

int riodp_mport_open(uint32_t mport_id, int flags)
{
	char path[32];

	snprintf(path, sizeof(path), RIO_MPORT_DEV_PATH "%d", mport_id);
	return open(path, O_RDWR | flags);
}

int riodp_mport_close(int fd)
{
	return close(fd);
}

int riomp_sock_mbox_init(void)
{
	return open(RIO_CMDEV_PATH, O_RDWR);
}


int riodp_mport_get_mport_list(uint32_t **dev_ids, uint8_t *number_of_mports)
{
	int fd;
	uint32_t entries = *number_of_mports;
	uint32_t *list;
	int ret = -1;


	/* Open RapidIO Channel Manager */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	list = (uint32_t *)calloc((entries + 1), sizeof(*list));
	if (list == NULL)
		goto outfd;

	/* Request MPORT list from the driver (first entry is list size) */
	list[0] = entries;
	if (ioctl(fd, RIO_CM_MPORT_GET_LIST, list)) {
		ret = errno;
		goto outfd;
	}

	/* Return list information */
	*dev_ids = &list[1]; /* pointer to the list */
	*number_of_mports = *list; /* return real number of mports */
	ret = 0;
outfd:
	close(fd);
	return ret;
}

int riodp_mport_free_mport_list(uint32_t **dev_ids)
{
	/* Get head of the list, because we did hide the list size and mport ID
	 * parameters
	 */
	uint32_t *list;

	if(dev_ids == NULL)
		return -1;
	list = (*dev_ids) - 1;
	free(list);
	return 0;
}

int riodp_mport_get_ep_list(uint8_t mport_id, uint32_t **destids, uint32_t *number_of_eps)
{
	int fd;
	int ret = 0;
	uint32_t entries;
	uint32_t *list;

	/* Open mport */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	/* Get list size */
	entries = mport_id;
	if (ioctl(fd, RIO_CM_EP_GET_LIST_SIZE, &entries)) {
#ifdef DEBUG
		printf("%s ep_get_list_size ioctl failed: %s\n", __func__, strerror(errno));
#endif
		ret = errno;
		goto outfd;
	}
#ifdef DEBUG
	printf("RIODP: %s() has %d entries\n", __func__,  entries);
#endif
	/* Get list */
	list = (uint32_t *)calloc((entries + 2), sizeof(*list));
	if (list == NULL) {
		ret = -1;
		goto outfd;
	}

	/* Get list (first entry is list size) */
	list[0] = entries;
	list[1] = mport_id;
	if (ioctl(fd, RIO_CM_EP_GET_LIST, list)) {
		ret = errno;
		goto outfd;
	}

	/* Pass to callee, first entry of list is entries in list */
	*destids = &list[2];
	*number_of_eps = entries;

outfd:
	close(fd);
	return ret;
}

int riodp_mport_free_ep_list(uint32_t **destids)
{
	/* Get head of the list, because we did hide the list size and mport ID
	 * parameters
	 */
	uint32_t *list;

	if(destids == NULL)
		return -1;
	list = (*destids) - 2;
	free(list);
	return 0;
}

static inline enum rio_exchange convert_directio_type(enum riomp_rdma_directio_type type)
{
	switch(type) {
	case RIO_DIRECTIO_TYPE_NWRITE: return RIO_EXCHANGE_NWRITE;
	case RIO_DIRECTIO_TYPE_NWRITE_R: return RIO_EXCHANGE_NWRITE_R;
	case RIO_DIRECTIO_TYPE_NWRITE_R_ALL: return RIO_EXCHANGE_NWRITE_R_ALL;
	case RIO_DIRECTIO_TYPE_SWRITE: return RIO_EXCHANGE_SWRITE;
	case RIO_DIRECTIO_TYPE_SWRITE_R: return RIO_EXCHANGE_SWRITE_R;
	default: return RIO_EXCHANGE_DEFAULT;
	}
}

static inline enum rio_transfer_sync convert_directio_sync(enum riomp_rdma_directio_transfer_sync sync)
{
	switch(sync) {
	default: /* sync as default is the smallest pitfall */
	case RIO_DIRECTIO_TRANSFER_SYNC: return RIO_TRANSFER_SYNC;
	case RIO_DIRECTIO_TRANSFER_ASYNC: return RIO_TRANSFER_ASYNC;
	case RIO_DIRECTIO_TRANSFER_FAF: return RIO_TRANSFER_FAF;
	}
}

/*
 * Perform DMA data write to target transfer using user space source buffer
 */
int riomp_rdma_write(int fd, uint16_t destid, uint64_t tgt_addr, void *buf,
		uint32_t size, enum riomp_rdma_directio_type wr_mode,
		enum riomp_rdma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = buf;
	xfer.length = size;
	xfer.handle = 0;
	xfer.offset = 0;
	xfer.method = convert_directio_type(wr_mode);

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_WRITE;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(fd, RIO_TRANSFER, &tran);
	return (ret < 0)?errno:ret;
}

/*
 * Perform DMA data write to target transfer using kernel space source buffer
 */
int riomp_rdma_write_d(int fd, uint16_t destid, uint64_t tgt_addr,
		      uint64_t handle, uint32_t offset, uint32_t size,
		      enum riomp_rdma_directio_type wr_mode,
		      enum riomp_rdma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = NULL;
	xfer.length = size;
	xfer.handle = handle;
	xfer.offset = offset;
	xfer.method = convert_directio_type(wr_mode);

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_WRITE;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(fd, RIO_TRANSFER, &tran);
	return (ret < 0)?errno:ret;
}

/*
 * Perform DMA data read from target transfer using user space destination buffer
 */
int riomp_rdma_read(int fd, uint16_t destid, uint64_t tgt_addr, void *buf,
		   uint32_t size, enum riomp_rdma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = buf;
	xfer.length = size;
	xfer.handle = 0;
	xfer.offset = 0;

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_READ;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(fd, RIO_TRANSFER, &tran);
	return (ret < 0)?errno:ret;
}

/*
 * Perform DMA data read from target transfer using kernel space destination buffer
 */
int riomp_rdma_read_d(int fd, uint16_t destid, uint64_t tgt_addr,
		     uint64_t handle, uint32_t offset, uint32_t size,
		     enum riomp_rdma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = NULL;
	xfer.length = size;
	xfer.handle = handle;
	xfer.offset = offset;

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_READ;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(fd, RIO_TRANSFER, &tran);
	return (ret < 0)?errno:ret;
}

/*
 * Wait for DMA transfer completion
 */
int riomp_rdma_wait_async(int fd, uint32_t cookie, uint32_t tmo)
{
	struct rio_async_tx_wait wparam;

	wparam.token = cookie;
	wparam.timeout = tmo;

	if (ioctl(fd, RIO_WAIT_FOR_ASYNC, &wparam))
		return errno;
	return 0;
}


/*
 * Allocate and map into RapidIO space a local kernel space data buffer
 * (for inbound RapidIO data read/write requests)
 */
int riomp_rdma_ibwin_map(int fd, uint64_t *rio_base, uint32_t size, uint64_t *handle)
{
	struct rio_mmap ib;

	ib.rio_addr = *rio_base;
	ib.length = size;

	if (ioctl(fd, RIO_MAP_INBOUND, &ib))
		return errno;
	*handle = ib.handle;
	*rio_base = ib.rio_addr;
	return 0;
}

/*
 * Free and unmap from RapidIO space a local kernel space data buffer
 */
int riomp_rdma_ibwin_free(int fd, uint64_t *handle)
{
	if (ioctl(fd, RIO_UNMAP_INBOUND, handle))
		return errno;
	return 0;
}

int riomp_rdma_obwin_map(int fd, uint16_t destid, uint64_t rio_base, uint32_t size,
		    uint64_t *handle)
{
	struct rio_mmap ob;

	ob.rioid = destid;
	ob.rio_addr = rio_base;
	ob.length = size;

	if (ioctl(fd, RIO_MAP_OUTBOUND, &ob))
		return errno;
	*handle = ob.handle;
	return 0;
}

int riomp_rdma_obwin_free(int fd, uint64_t *handle)
{
	if (ioctl(fd, RIO_UNMAP_OUTBOUND, handle))
		return errno;
	return 0;
}

/*
 * Allocate a local kernel space data buffer for DMA data transfers
 */
int riomp_rdma_dbuf_alloc(int fd, uint32_t size, uint64_t *handle)
{
	struct rio_dma_mem db;

	db.length = size;

	if (ioctl(fd, RIO_ALLOC_DMA, &db))
		return errno;
	*handle = db.dma_handle;
	return 0;
}

/*
 * Free previously allocated local kernel space data buffer
 */
int riomp_rdma_dbuf_free(int fd, uint64_t *handle)
{
	if (ioctl(fd, RIO_FREE_DMA, handle))
		return errno;

	return 0;
}

/*
 * Query mport status/capabilities
 */
int riodp_mport_query(int fd, struct riodp_mport_properties *qresp)
{
	struct rio_mport_properties prop;
	if (!qresp)
		return -EINVAL;

	memset(&prop, 0, sizeof(prop));
	if (ioctl(fd, RIO_MPORT_GET_PROPERTIES, &prop))
		return errno;

	qresp->hdid               = prop.hdid;
	qresp->id                 = prop.id;
	qresp->index              = prop.index;
	qresp->flags              = prop.flags;
	qresp->sys_size           = prop.sys_size;
	qresp->port_ok            = prop.port_ok;
	qresp->link_speed         = prop.link_speed;
	qresp->link_width         = prop.link_width;
	qresp->dma_max_sge        = prop.dma_max_sge;
	qresp->dma_max_size       = prop.dma_max_size;
	qresp->dma_align          = prop.dma_align;
	qresp->transfer_mode      = prop.transfer_mode;
	qresp->cap_sys_size       = prop.cap_sys_size;
	qresp->cap_addr_size      = prop.cap_addr_size;
	qresp->cap_transfer_mode  = prop.cap_transfer_mode;
	qresp->cap_mport          = prop.cap_mport;

	return 0;
}

/*
 * Read from local (mport) device register
 */
int riodp_lcfg_read(int fd, uint32_t offset, uint32_t size, uint32_t *data)
{
	struct rio_mport_maint_io mt;

	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = data;

	if (ioctl(fd, RIO_MPORT_MAINT_READ_LOCAL, &mt))
		return errno;
	return 0;
}

/*
 * Write to local (mport) device register
 */
int riodp_lcfg_write(int fd, uint32_t offset, uint32_t size, uint32_t data)
{
	struct rio_mport_maint_io mt;

	mt.offset = offset;
	mt.length = size;
//		uint32_t __user value; /* when length == 0 */
	mt.u.buffer = &data;   /* when length != 0 */

	if (ioctl(fd, RIO_MPORT_MAINT_WRITE_LOCAL, &mt))
		return errno;
	return 0;
}

/*
 * Maintenance read from target RapidIO device register
 */
int riodp_maint_read(int fd, uint32_t destid, uint32_t hc, uint32_t offset,
		     uint32_t size, uint32_t *data)
{
	struct rio_mport_maint_io mt;

	mt.rioid = destid;
	mt.hopcount = hc;
	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = data;   /* when length != 0 */

	if (ioctl(fd, RIO_MPORT_MAINT_READ_REMOTE, &mt))
		return errno;
	return 0;
}


/*
 * Maintenance write to target RapidIO device register
 */
int riodp_maint_write(int fd, uint32_t destid, uint32_t hc, uint32_t offset,
		      uint32_t size, uint32_t data)
{
	struct rio_mport_maint_io mt;

	mt.rioid = destid;
	mt.hopcount = hc;
	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = &data;   /* when length != 0 */

	if (ioctl(fd, RIO_MPORT_MAINT_WRITE_REMOTE, &mt))
		return errno;
	return 0;
}

/*
 * Enable (register) receiving range of RapidIO doorbell events
 */
int riodp_dbrange_enable(int fd, uint32_t rioid, uint16_t start, uint16_t end)
{
	struct rio_doorbell_filter dbf;

	dbf.rioid = rioid;
	dbf.low = start;
	dbf.high = end;

	if (ioctl(fd, RIO_ENABLE_DOORBELL_RANGE, &dbf))
		return errno;
	return 0;
}

/*
 * Disable (unregister) range of inbound RapidIO doorbell events
 */
int riodp_dbrange_disable(int fd, uint32_t rioid, uint16_t start, uint16_t end)
{
	struct rio_doorbell_filter dbf;

	dbf.rioid = rioid;
	dbf.low = start;
	dbf.high = end;

	if (ioctl(fd, RIO_DISABLE_DOORBELL_RANGE, &dbf))
		return errno;
	return 0;
}

/*
 * Enable (register) filter for RapidIO port-write events
 */
int riodp_pwrange_enable(int fd, uint32_t mask, uint32_t low, uint32_t high)
{
	struct rio_pw_filter pwf;

	pwf.mask = mask;
	pwf.low = low;
	pwf.high = high;

	if (ioctl(fd, RIO_ENABLE_PORTWRITE_RANGE, &pwf))
		return errno;
	return 0;
}

/*
 * Disable (unregister) filter for RapidIO port-write events
 */
int riodp_pwrange_disable(int fd, uint32_t mask, uint32_t low, uint32_t high)
{
	struct rio_pw_filter pwf;

	pwf.mask = mask;
	pwf.low = low;
	pwf.high = high;

	if (ioctl(fd, RIO_DISABLE_PORTWRITE_RANGE, &pwf))
		return errno;
	return 0;
}

/*
 * Set event notification mask
 */
int riodp_set_event_mask(int fd, unsigned int mask)
{
	unsigned int evt_mask = 0;
	if (mask & RIO_EVENT_DOORBELL) evt_mask |= RIO_DOORBELL;
	if (mask & RIO_EVENT_PORTWRITE) evt_mask |= RIO_PORTWRITE;
	if (ioctl(fd, RIO_SET_EVENT_MASK, evt_mask))
		return errno;
	return 0;
}

/*
 * Get current value of event mask
 */
int riodp_get_event_mask(int fd, unsigned int *mask)
{
	int evt_mask = 0;
	if (!mask) return -EINVAL;
	if (ioctl(fd, RIO_GET_EVENT_MASK, &evt_mask))
		return errno;
	*mask = 0;
	if (evt_mask & RIO_DOORBELL) *mask |= RIO_EVENT_DOORBELL;
	if (evt_mask & RIO_PORTWRITE) *mask |= RIO_EVENT_PORTWRITE;
	return 0;
}

/*
 * Get current event data
 */
int riodp_get_event(int fd, struct riodp_event *evt)
{
	struct rio_event revent;
	ssize_t bytes = 0;

	if (!evt) return -EINVAL;

	bytes = read(fd, &revent, sizeof(revent));
	if (bytes == -1)
		return -ENOMSG;
	if (bytes != sizeof(revent)) {
		return -EIO;
	}

	if (revent.header == RIO_EVENT_DOORBELL) {
		evt->u.doorbell.payload = revent.u.doorbell.payload;
		evt->u.doorbell.rioid = revent.u.doorbell.rioid;
	} else if (revent.header == RIO_EVENT_PORTWRITE) {
		memcpy(&evt->u.portwrite.payload, &revent.u.portwrite.payload, sizeof(evt->u.portwrite.payload));
	} else {
		return -EIO;
	}
	evt->header = revent.header;

	return 0;
}

/*
 * Set destination ID of local mport device
 */
int riodp_destid_set(int fd, uint16_t destid)
{
	if (ioctl(fd, RIO_MPORT_MAINT_HDID_SET, &destid))
		return errno;
	return 0;
}

/*
 * Create a new kernel device object
 */
int riodp_device_add(int fd, uint16_t destid, uint8_t hc, uint32_t ctag,
		    const char *name)
{
	struct rio_rdev_info dev;

	dev.destid = destid;
	dev.hopcount = hc;
	dev.comptag = ctag;
	if (name)
		strncpy(dev.name, name, RIO_MAX_DEVNAME_SZ);
	else
		*dev.name = '\0';

	if (ioctl(fd, RIO_DEV_ADD, &dev))
		return errno;
	return 0;
}

/*
 * Delete existing kernel device object
 */
int riodp_device_del(int fd, uint16_t destid, uint8_t hc, uint32_t ctag)
{
	struct rio_rdev_info dev;

	dev.destid = destid;
	dev.hopcount = hc;
	dev.comptag = ctag;

	if (ioctl(fd, RIO_DEV_DEL, &dev))
		return errno;
	return 0;
}

/* Mailbox functions */
int riomp_sock_mbox_create_handle(uint8_t mport_id, uint8_t mbox_id,
			     riomp_mailbox_t *mailbox)
{
	int fd;
	struct rapidio_mport_mailbox *lhandle = NULL;

	/* Open mport */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	/* TODO claim mbox_id */

	/* Create handle */
	lhandle = (struct rapidio_mport_mailbox *)malloc(sizeof(struct rapidio_mport_mailbox));
	if(!(lhandle)) {
		close(fd);
		return -2;
	}

	lhandle->fd = fd;
	lhandle->mport_id = mport_id;
	*mailbox = lhandle;
	return 0;
}

int riomp_sock_socket(riomp_mailbox_t mailbox, riomp_sock_t *socket_handle)
{
	struct rapidio_mport_socket *handle = NULL;

	/* Create handle */
	handle = (struct rapidio_mport_socket *)calloc(1, sizeof(struct rapidio_mport_socket));
	if(!handle) {
		printf("error in calloc\n");
		return -1;
	}

	handle->mbox = mailbox;
	handle->ch.id = 0;
	*socket_handle = handle;
	return 0;
}

int riomp_sock_send(riomp_sock_t socket_handle, void *buf, uint32_t size)
{
	int ret;
	struct rapidio_mport_socket *handle = socket_handle;
	struct rio_cm_msg msg;

	msg.ch_num = handle->ch.id;
	msg.size = size;
	msg.msg = buf;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_SEND, &msg);
	if (ret) {
		printf("SEND IOCTL: returned %d for ch_num=%d (errno=%d)\n", ret, msg.ch_num, errno);
		return errno;
	}

	return 0;
}

int riomp_sock_receive(riomp_sock_t socket_handle, void **buf,
			 uint32_t size, uint32_t timeout)
{
	int ret;
	struct rapidio_mport_socket *handle = socket_handle;
	struct rio_cm_msg msg;

	msg.ch_num = handle->ch.id;
	msg.size = size;
	msg.msg = *buf;
	msg.rxto = timeout;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_RECEIVE, &msg);
	if (ret)
		return errno;

	return 0;
}

int riomp_sock_release_receive_buffer(riomp_sock_t socket_handle,
					void *buf) /* always 4k aligned buffers */
{
	free(buf);
	return 0;
}

int riomp_sock_close(riomp_sock_t *socket_handle)
{
	int ret;
	struct rapidio_mport_socket *handle = *socket_handle;
	uint16_t ch_num;

	if(!handle)
		return -1;

	ch_num = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CLOSE, &ch_num);
	if (ret < 0) {
		printf("CLOSE IOCTL: returned %d for ch_num=%d (errno=%d)\n", ret, (*socket_handle)->ch.id, errno);
		ret = errno;
	}

	free(handle);
	return ret;
}

int riomp_sock_mbox_destroy_handle(riomp_mailbox_t *mailbox)
{
	struct rapidio_mport_mailbox *mbox = *mailbox;

	if(mbox != NULL) {
		close(mbox->fd);
		free(mbox);
		return 0;
	}

	return -1;

}

int riomp_sock_bind(riomp_sock_t socket_handle, uint16_t local_channel)
{
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num;
	int ret;
	struct rio_cm_channel cdev;

	ch_num = local_channel;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CREATE, &ch_num);
	if (ret < 0)
		return errno;

	cdev.id = ch_num;
	cdev.mport_id = handle->mbox->mport_id;
	handle->ch.id = cdev.id;
	handle->ch.mport_id = cdev.mport_id;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_BIND, &cdev);
	if (ret < 0)
		return errno;

	return 0;
}

int riomp_sock_listen(riomp_sock_t socket_handle)
{
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num;
	int ret;

	ch_num = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_LISTEN, &ch_num);
	if (ret)
		return errno;

	return 0;
}

int riomp_sock_accept(riomp_sock_t socket_handle, riomp_sock_t *conn,
			uint32_t timeout)
{
	struct rapidio_mport_socket *handle = socket_handle;
	struct rapidio_mport_socket *new_handle = *conn;
	struct rio_cm_accept param;
//	uint16_t ch_num;
	int ret;

	if(!handle || !conn)
		return -1;

	param.ch_num = handle->ch.id;
	param.wait_to = timeout;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_ACCEPT, &param);//&ch_num);
	if (ret)
		return errno;

#ifdef DEBUG
	printf("%s: new ch_num=%d\n", __func__, ch_num);
#endif

	if (new_handle)
		new_handle->ch.id = param.ch_num;

	return 0;
}

int riomp_sock_connect(riomp_sock_t socket_handle, uint32_t remote_destid,
			 uint8_t remote_mbox, uint16_t remote_channel)
{
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num = 0;
	struct rio_cm_channel cdev;

	if (handle->ch.id == 0) {
		if (ioctl(handle->mbox->fd, RIO_CM_CHAN_CREATE, &ch_num))
			return errno;
		handle->ch.id = ch_num;
	}

	/* Configure and Send Connect IOCTL */
	handle->ch.remote_destid  = remote_destid;
	handle->ch.remote_mbox    = remote_mbox;
	handle->ch.remote_channel = remote_channel;
	handle->ch.mport_id = handle->mbox->mport_id;
	cdev.remote_destid  = remote_destid;
	cdev.remote_mbox    = remote_mbox;
	cdev.remote_channel = remote_channel;
	cdev.mport_id = handle->mbox->mport_id;
	cdev.id = handle->ch.id;
	if (ioctl(handle->mbox->fd, RIO_CM_CHAN_CONNECT, &cdev)) {
#ifdef DEBUG
		printf("ioctl rc(%d): %s\n", ret, strerror(errno));
#endif
		return errno;
	}

	return 0;
}

int riomp_sock_request_send_buffer(riomp_sock_t socket_handle,
				     void **buf) //always 4k aligned buffers
{
	/* socket_handle won't be used for now */

	*buf = malloc(0x1000); /* Always allocate maximum size buffers */
	if (*buf == NULL)
		return -1;

	return 0;
}

int riomp_sock_release_send_buffer(riomp_sock_t socket_handle,
				     void *buf) /* always 4k aligned buffers */
{
	free(buf);
	return 0;
}


const char *speed_to_string(int speed)
{
	switch(speed){
		case RIO_LINK_DOWN:
			return "LINK DOWN";
		case RIO_LINK_125:
			return "1.25Gb";
		case RIO_LINK_250:
			return "2.5Gb";
		case RIO_LINK_312:
			return "3.125Gb";
		case RIO_LINK_500:
			return "5.0Gb";
		case RIO_LINK_625:
			return "6.25Gb";
		default:
			return "ERROR";
	}
}

const char *width_to_string(int width)
{
	switch(width){
		case RIO_LINK_1X:
			return "1x";
		case RIO_LINK_1XR:
			return "1xR";
		case RIO_LINK_2X:
			return "2x";
		case RIO_LINK_4X:
			return "4x";
		case RIO_LINK_8X:
			return "8x";
		case RIO_LINK_16X:
			return "16x";
		default:
			return "ERROR";
	}
}

void riodp_mport_display_info(struct riodp_mport_properties *attr)
{
	printf("\n+++ SRIO mport configuration +++\n");
	printf("mport: hdid=%d, id=%d, idx=%d, flags=0x%x, sys_size=%s\n",
		attr->hdid, attr->id, attr->index, attr->flags,
		attr->sys_size?"large":"small");

	printf("link: speed=%s width=%s\n", speed_to_string(attr->link_speed),
		width_to_string(attr->link_width));

	if (attr->flags & RIO_MPORT_DMA) {
		printf("DMA: max_sge=%d max_size=%d alignment=%d (%s)\n",
			attr->dma_max_sge, attr->dma_max_size, attr->dma_align,
			(attr->flags & RIO_MPORT_DMA_SG)?"HW SG":"no HW SG");
	} else
		printf("No DMA support\n");
	printf("\n");
}


