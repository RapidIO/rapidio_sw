#ifndef _RIODP_MPORT_LIB_H_
#define _RIODP_MPORT_LIB_H_
/*
 * Copyright 2014 Integrated Device Technology, Inc.
 *
 * Header file for RapidIO mport device library.
 *
 * This program uses code fragments from Linux kernel dmaengine
 * framework test driver developed by Atmel and Intel. Please, see
 * drivers/dma/dmatest.c file in Linux kernel source code tree for more
 * details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef __cplusplus
extern "C" {
#endif

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

#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */
#define RIO_MPORT_DEV_PATH "/dev/rio_mport"
#define RIO_CMDEV_PATH "/dev/rio_cm"

enum rio_link_speed {
	RIO_LINK_DOWN = 0, /* SRIO Link not initialized */
	RIO_LINK_125 = 1, /* 1.25 GBaud  */
	RIO_LINK_250 = 2, /* 2.5 GBaud   */
	RIO_LINK_312 = 3, /* 3.125 GBaud */
	RIO_LINK_500 = 4, /* 5.0 GBaud   */
	RIO_LINK_625 = 5  /* 6.25 GBaud  */
};

enum rio_link_width {
	RIO_LINK_1X  = 0,
	RIO_LINK_1XR = 1,
	RIO_LINK_2X  = 3,
	RIO_LINK_4X  = 2,
	RIO_LINK_8X  = 4,
	RIO_LINK_16X = 5
};

enum rio_mport_flags {
	RIO_MPORT_DMA	 = (1 << 0), /* supports DMA data transfers */
	RIO_MPORT_DMA_SG = (1 << 1), /* DMA supports HW SG mode */
	RIO_MPORT_IBSG	 = (1 << 2), /* inbound mapping supports SG */
};

struct riodp_mailbox {
	int fd;
	uint8_t mport_id;
};

struct riodp_socket {
	struct riodp_mailbox *mbox;
	struct rio_cm_channel cdev;
	uint8_t	*rx_buffer;
	uint8_t	*tx_buffer;
};

typedef struct riodp_mailbox  *riodp_mailbox_t;
typedef struct riodp_socket   *riodp_socket_t;




int riodp_mport_open(uint32_t mport_id, int flags);
int riodp_cm_open(void);
int riodp_mport_get_mport_list(uint32_t **dev_ids, uint8_t *number_of_mports);
int riodp_mport_free_mport_list(uint32_t **dev_ids);
int riodp_mport_get_ep_list(uint8_t mport_id, uint32_t **destids, uint32_t *number_of_eps);
int riodp_mport_free_ep_list(uint32_t **destids);
int riodp_dma_write(int fd, uint16_t destid, uint64_t tgt_addr, void *buf,
		    uint32_t size, enum rio_exchange wr_mode,
		    enum rio_transfer_sync sync);
int riodp_dma_write_d(int fd, uint16_t destid, uint64_t tgt_addr,
		      uint64_t handle, uint32_t offset, uint32_t size,
		      enum rio_exchange wr_mode, enum rio_transfer_sync sync);
int riodp_dma_read(int fd, uint16_t destid, uint64_t tgt_addr, void *buf,
		   uint32_t size, enum rio_transfer_sync sync);
int riodp_dma_read_d(int fd, uint16_t destid, uint64_t tgt_addr,
		     uint64_t handle, uint32_t offset, uint32_t size,
		     enum rio_transfer_sync sync);
int riodp_wait_async(int fd, uint32_t cookie, uint32_t tmo);
int riodp_ibwin_map(int fd, uint64_t *rio_base, uint32_t size, uint64_t *handle);
int riodp_ibwin_free(int fd, uint64_t *handle);
int riodp_obwin_map(int fd, uint16_t destid, uint64_t rio_base, uint32_t size,
		    uint64_t *handle);
int riodp_obwin_free(int fd, uint64_t *handle);
int riodp_dbuf_alloc(int fd, uint32_t size, uint64_t *handle);
int riodp_dbuf_free(int fd, uint64_t *handle);
int riodp_query_mport(int fd, struct rio_mport_properties *qresp);
int riodp_lcfg_read(int fd, uint32_t offset, uint32_t size, uint32_t *data);
int riodp_lcfg_write(int fd, uint32_t offset, uint32_t size, uint32_t data);
int riodp_maint_read(int fd, uint32_t destid, uint32_t hc, uint32_t offset,
		     uint32_t size, uint32_t *data);
int riodp_maint_write(int fd, uint32_t destid, uint32_t hc, uint32_t offset,
		      uint32_t size, uint32_t data);

int riodp_dbrange_enable(int fd, uint32_t rioid, uint16_t start, uint16_t end);
int riodp_dbrange_disable(int fd, uint32_t rioid, uint16_t start, uint16_t end);
int riodp_pwrange_enable(int fd, uint32_t mask, uint32_t low, uint32_t high);
int riodp_pwrange_disable(int fd, uint32_t mask, uint32_t low, uint32_t high);
int riodp_set_event_mask(int fd, unsigned int mask);
int riodp_get_event_mask(int fd, unsigned int *mask);
int riodp_destid_set(int fd, uint16_t destid);
int riodp_device_add(int fd, uint16_t destid, uint8_t hc, uint32_t ctag,
		     const char *name);
int riodp_device_del(int fd, uint16_t destid, uint8_t hc, uint32_t ctag);

int riodp_mbox_create_handle(uint8_t mport_id, uint8_t mbox_id,
			     riodp_mailbox_t *mailbox);
int riodp_socket_socket(riodp_mailbox_t mailbox, riodp_socket_t *socket_handle);
int riodp_socket_send(riodp_socket_t socket_handle, void *buf, uint32_t size);
int riodp_socket_receive(riodp_socket_t socket_handle, void **buf,
			 uint32_t size, uint32_t timeout);
int riodp_socket_release_receive_buffer(riodp_socket_t socket_handle, void *buf);
int riodp_socket_close(riodp_socket_t *socket_handle);
int riodp_mbox_destroy_handle(riodp_mailbox_t *mailbox);
int riodp_socket_bind(riodp_socket_t socket_handle, uint16_t local_channel);
int riodp_socket_listen(riodp_socket_t socket_handle);
int riodp_socket_accept(riodp_socket_t socket_handle, riodp_socket_t *conn,
			uint32_t timeout);
int riodp_socket_connect(riodp_socket_t socket_handle, uint32_t remote_destid,
			 uint8_t remote_mbox, uint16_t remote_channel);
int riodp_socket_request_send_buffer(riodp_socket_t socket_handle, void **buf);
int riodp_socket_release_send_buffer(riodp_socket_t socket_handle, void *buf);

void display_mport_info(struct rio_mport_properties *prop);

#ifdef __cplusplus
}
#endif
#endif /* _RIODP_MPORT_LIB_H_ */
