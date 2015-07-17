#ifndef _RAPIDIO_MPORT_SOCK_H_
#define _RAPIDIO_MPORT_SOCK_H_
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



typedef struct riodp_mailbox  *riodp_mailbox_t;
typedef struct riodp_socket   *riodp_socket_t;


int riodp_cm_open(void);


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
