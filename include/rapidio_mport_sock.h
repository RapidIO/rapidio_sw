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

typedef struct rapidio_mport_mailbox  *riomp_mailbox_t;
typedef struct rapidio_mport_socket   *riomp_sock_t;

int riomp_sock_mbox_init(void);
int riomp_sock_mbox_exit(void);

int riomp_sock_mbox_create_handle(uint8_t mport_id, uint8_t mbox_id, riomp_mailbox_t *mailbox);
int riomp_sock_mbox_destroy_handle(riomp_mailbox_t *mailbox);

int riomp_sock_socket(riomp_mailbox_t mailbox, riomp_sock_t *socket_handle);
int riomp_sock_send(riomp_sock_t socket_handle, void *buf, uint32_t size);
int riomp_sock_receive(riomp_sock_t socket_handle, void **buf, uint32_t size, uint32_t timeout);
int riomp_sock_release_receive_buffer(riomp_sock_t socket_handle, void *buf);
int riomp_sock_close(riomp_sock_t *socket_handle);
int riomp_sock_bind(riomp_sock_t socket_handle, uint16_t local_channel);
int riomp_sock_listen(riomp_sock_t socket_handle);
int riomp_sock_accept(riomp_sock_t socket_handle, riomp_sock_t *conn, uint32_t timeout);
int riomp_sock_connect(riomp_sock_t socket_handle, uint32_t remote_destid, uint8_t remote_mbox, uint16_t remote_channel);
int riomp_sock_request_send_buffer(riomp_sock_t socket_handle, void **buf);
int riomp_sock_release_send_buffer(riomp_sock_t socket_handle, void *buf);

#ifdef __cplusplus
}
#endif
#endif /* _RIODP_MPORT_LIB_H_ */
