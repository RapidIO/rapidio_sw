#ifndef _RAPIDIO_MPORT_LIB_H_
#define _RAPIDIO_MPORT_LIB_H_
/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * Header file for RapidIO mport device library.
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



#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */
#define RIO_MAP_ANY_ADDR	(uint64_t)(~((uint64_t) 0))

#define RIO_EVENT_DOORBELL	(1 << 0)
#define RIO_EVENT_PORTWRITE	(1 << 1)

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

struct rio_channel {
	uint16_t id;
	uint32_t remote_destid;
	uint32_t remote_mbox;
	uint16_t remote_channel;
	uint8_t mport_id;
};

struct riodp_socket {
	struct riodp_mailbox *mbox;
	struct rio_channel ch;
	uint8_t	*rx_buffer;
	uint8_t	*tx_buffer;
};

struct riodp_mport_properties {
	uint16_t hdid;
	uint8_t id;			/* Physical port ID */
	uint8_t  index;
	uint32_t flags;
	uint32_t sys_size;		/* Default addressing size */
	uint8_t  port_ok;
	uint8_t  link_speed;
	uint8_t  link_width;
	uint32_t dma_max_sge;
	uint32_t dma_max_size;
	uint32_t dma_align;
	uint32_t transfer_mode;		/* Default transfer mode */
	uint32_t cap_sys_size;		/* Capable system sizes */
	uint32_t cap_addr_size;		/* Capable addressing sizes */
	uint32_t cap_transfer_mode;	/* Capable transfer modes */
	uint32_t cap_mport;		/* Mport capabilities */
};

enum riodp_directio_type {
	//RIO_DIRECTIO_TYPE_DEFAULT,	/* Default method */
	RIO_DIRECTIO_TYPE_NWRITE,		/* All packets using NWRITE */
	RIO_DIRECTIO_TYPE_SWRITE,		/* All packets using SWRITE */
	RIO_DIRECTIO_TYPE_NWRITE_R,		/* Last packet NWRITE_R, others NWRITE */
	RIO_DIRECTIO_TYPE_SWRITE_R,		/* Last packet NWRITE_R, others SWRITE */
	RIO_DIRECTIO_TYPE_NWRITE_R_ALL,	/* All packets using NWRITE_R */
};

enum riodp_directio_transfer_sync {
	RIO_DIRECTIO_TRANSFER_SYNC,		/* synchronous transfer */
	RIO_DIRECTIO_TRANSFER_ASYNC,	/* asynchronous transfer */
	RIO_DIRECTIO_TRANSFER_FAF,		/* fire-and-forget transfer only for write transactions */
};

struct riodp_doorbell {
	uint32_t rioid;
	uint16_t payload;
};

struct riodp_portwrite {
	uint32_t payload[16];
};

struct riodp_event {
	unsigned int header;	/* event kind, e.g. RIO_EVENT_DOORBELL or RIO_EVENT_PORTWRITE */
	union {
		struct riodp_doorbell doorbell;	/* header is RIO_EVENT_DOORBELL */
		struct riodp_portwrite portwrite; /* header is RIO_EVENT_PORTWRITE */
	} u;
};


typedef struct riodp_mailbox  *riodp_mailbox_t;
typedef struct riodp_socket   *riodp_socket_t;


int riodp_mport_get_mport_list(uint32_t **dev_ids, uint8_t *number_of_mports);
int riodp_mport_free_mport_list(uint32_t **dev_ids);
int riodp_mport_get_ep_list(uint8_t mport_id, uint32_t **destids, uint32_t *number_of_eps);
int riodp_mport_free_ep_list(uint32_t **destids);


int riodp_mport_open(uint32_t mport_id, int flags);
int riodp_mport_close(int fd);

int riodp_mport_query(int fd, struct riodp_mport_properties *qresp);
void riodp_mport_display_info(struct riodp_mport_properties *prop);


int riodp_cm_open(void);

int riodp_dma_write(int fd, uint16_t destid, uint64_t tgt_addr, void *buf,
		    uint32_t size, enum riodp_directio_type wr_mode,
		    enum riodp_directio_transfer_sync sync);
int riodp_dma_write_d(int fd, uint16_t destid, uint64_t tgt_addr,
		      uint64_t handle, uint32_t offset, uint32_t size,
		      enum riodp_directio_type wr_mode, enum riodp_directio_transfer_sync sync);
int riodp_dma_read(int fd, uint16_t destid, uint64_t tgt_addr, void *buf,
		   uint32_t size, enum riodp_directio_transfer_sync sync);
int riodp_dma_read_d(int fd, uint16_t destid, uint64_t tgt_addr,
		     uint64_t handle, uint32_t offset, uint32_t size,
		     enum riodp_directio_transfer_sync sync);
int riodp_dma_wait_async(int fd, uint32_t cookie, uint32_t tmo);
int riodp_ibwin_map(int fd, uint64_t *rio_base, uint32_t size, uint64_t *handle);
int riodp_ibwin_free(int fd, uint64_t *handle);
int riodp_obwin_map(int fd, uint16_t destid, uint64_t rio_base, uint32_t size,
		    uint64_t *handle);
int riodp_obwin_free(int fd, uint64_t *handle);
int riodp_dbuf_alloc(int fd, uint32_t size, uint64_t *handle);
int riodp_dbuf_free(int fd, uint64_t *handle);
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
int riodp_get_event(int fd, struct riodp_event *evt);

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


#ifdef __cplusplus
}
#endif
#endif /* _RIODP_MPORT_LIB_H_ */
