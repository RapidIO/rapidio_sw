/* Definition of messages exchanged between the RSKT Daemon and the library */
/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/

#include <stdint.h>
#include "librskt.h"
#include "librskt_private.h"

#ifndef __LIBRSKTD_H__
#define __LIBRSKTD_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LIBRSKTD_RESP 0x80000000
#define LIBRSKTD_FAIL 0x40000000

#define LIBRSKTD_BIND 1
#define LIBRSKTD_BIND_RESP (LIBRSKTD_BIND|LIBRSKTD_RESP)
#define LIBRSKTD_LISTEN 2
#define LIBRSKTD_LISTEN_RESP (LIBRSKTD_LISTEN|LIBRSKTD_RESP)
#define LIBRSKTD_ACCEPT 3
#define LIBRSKTD_ACCEPT_RESP (LIBRSKTD_ACCEPT|LIBRSKTD_RESP)
#define LIBRSKTD_CONN 4
#define LIBRSKTD_CONN_RESP (LIBRSKTD_CONN|LIBRSKTD_RESP)
#define LIBRSKTD_CLOSE 5
#define LIBRSKTD_CLOSE_RESP (LIBRSKTD_CLOSE|LIBRSKTD_RESP)
#define LIBRSKTD_HELLO 6
#define LIBRSKTD_HELLO_RESP (LIBRSKTD_HELLO|LIBRSKTD_RESP)
#define LIBRSKTD_LAST_MSG_TYPE 0x10;

#define LIBRSKT_CLOSE_CMD 0x111
#define LIBRSKT_CLOSE_CMD_RESP (LIBRSKT_CLOSE_CMD|LIBRSKTD_RESP)

#define MAX_APP_NAME 48

#define LIBRSKT_APP_MSG_TO_STR(x) ( \
        (LIBRSKTD_BIND == x)?           "BIND   ": \
        (LIBRSKTD_BIND_RESP == x)?      "BINDRSP": \
        (LIBRSKTD_LISTEN == x)?         "LIST   ": \
        (LIBRSKTD_LISTEN_RESP == x)?    "LISTRSP": \
        (LIBRSKTD_ACCEPT == x)?         "ACC    ": \
        (LIBRSKTD_ACCEPT_RESP == x)?    "ACC RSP": \
        (LIBRSKTD_CONN == x)?           "CON    ": \
        (LIBRSKTD_CONN_RESP == x)?      "CON RSP": \
        (LIBRSKTD_CLOSE == x)?          "CLOS   ": \
        (LIBRSKTD_CLOSE_RESP == x)?     "CLOS   ": \
        (LIBRSKTD_HELLO == x)?          "HELO   ": \
        (LIBRSKTD_HELLO_RESP == x)?     "HELORSP": \
        (LIBRSKT_CLOSE_CMD == x)?       "LCLS   ": \
        (LIBRSKT_CLOSE_CMD_RESP == x)?  "LCLSRSP":"UNKNOWN")

#define LIBRSKT_APP_2_DMN_MSG_SEQ_NO(x, y) (( \
        (LIBRSKTD_BIND == y) || \
        (LIBRSKTD_LISTEN == y) || \
        (LIBRSKTD_ACCEPT == y) || \
        (LIBRSKTD_CONN == y) || \
        (LIBRSKTD_CLOSE == y) || \
        (LIBRSKTD_HELLO == y)|| \
        (LIBRSKT_CLOSE_CMD == y))? ntohl(x->a_rq.app_seq_num): \
        ((LIBRSKTD_BIND_RESP == y) || \
        (LIBRSKTD_LISTEN_RESP == y) || \
        (LIBRSKTD_ACCEPT_RESP == y) || \
        (LIBRSKTD_CONN_RESP == y) || \
        (LIBRSKTD_CLOSE_RESP == y) || \
        (LIBRSKTD_HELLO_RESP == y) || \
        (LIBRSKT_CLOSE_CMD_RESP == y))?ntohl(x->rsp_a.req_a.rsktd_seq_num): \
        0xFFFFFFFF)

#define LIBRSKT_DMN_2_APP_MSG_SEQ_NO(x, y) (( \
        (LIBRSKT_CLOSE_CMD == y))? ntohl(x->rq_a.rsktd_seq_num): \
        ((LIBRSKTD_BIND_RESP == y) || \
        (LIBRSKTD_LISTEN_RESP == y) || \
        (LIBRSKTD_ACCEPT_RESP == y) || \
        (LIBRSKTD_CONN_RESP == y) || \
        (LIBRSKTD_CLOSE_RESP == y) || \
        (LIBRSKTD_HELLO_RESP == y) || \
        (LIBRSKT_CLOSE_CMD_RESP == y))?ntohl(x->a_rsp.req.app_seq_num): \
        0xFFFFFFFF)

/* Requests sent from library to RSKTD */

struct librskt_hello_req {
	char app_name[MAX_APP_NAME];
	int32_t proc_num;
};

struct librskt_bind_req {
	uint32_t sn;
	uint32_t ct;
};

struct librskt_listen_req {
	uint32_t sn;
	uint32_t max_bklog;
};

struct librskt_accept_req {
	uint32_t sn;
};

struct librskt_connect_req {
	uint32_t sn;
	uint32_t ct;
};

struct librskt_close_req {
	uint32_t sn;
};

struct librskt_cli_req {
	char cmd_line[MAX_MS_NAME*3];
};

union librskt_req_u {
	struct librskt_hello_req	hello;
	struct librskt_bind_req		bind;
	struct librskt_listen_req	listen;
	struct librskt_accept_req	accept;
	struct librskt_connect_req	conn;
	struct librskt_close_req	close;
	struct librskt_cli_req		cli;
};

struct librskt_req {
	uint32_t app_seq_num;
	union librskt_req_u msg;
};

/* Responses sent from RSKTD to library */

struct librskt_hello_resp {
	int32_t ct; /* Component tag for host port device */
};

struct librskt_accept_resp {
	uint32_t new_sn;
	uint32_t new_ct;
	struct rskt_sockaddr peer_sa;
	/* Local mos/ms/msub to allocate for RDMA connect operation */
	uint32_t ms_size;
	char mso_name[MAX_MS_NAME]; 
	char ms_name[MAX_MS_NAME];
};

struct librskt_connect_resp {
	uint32_t new_sn; /* Local socket number allocated for connection */
	uint32_t new_ct; /* Local Component Tag value */
	char mso[MAX_MS_NAME];  /* Local mso to open() for ms */
	char ms[MAX_MS_NAME]; /* Local ms to open() for msub */
	char rem_ms[MAX_MS_NAME]; /* Remote ms for RDMA connect operation */
	uint32_t rem_sn;
	uint32_t msub_sz;
};

union librskt_resp_u {
	struct librskt_hello_resp	hello;
	struct librskt_accept_resp	accept;
	struct librskt_connect_resp	conn;
};
struct librskt_resp {
	uint32_t err;
	struct librskt_req req;
	union librskt_resp_u msg;
};

/* Requests sent from RSKTD to library */

struct close_req_librskt {
	uint32_t sn;
};

struct cli_req_librskt {
	char cmd_line[MAX_MS_NAME*3];
};

union req_librskt_u {
	struct close_req_librskt clos;
	struct cli_req_librskt cli;
};

struct req_librskt {
	uint32_t rsktd_seq_num;
	union req_librskt_u msg;
};

/* Responses sent from library to RSKTD */

struct resp_librskt {
	uint32_t err;
	struct req_librskt req_a; /* Request message, including rsktd_seq_num */
};

/* Format of messages that can be sent by the library */

struct librskt_app_to_rsktd_msg { /* Messages sent FROM application TO RSKTD */
	uint32_t in_use; /* Used for message pool tracking */
        uint32_t msg_type;
	union {
		struct librskt_req a_rq;
		struct resp_librskt rsp_a;
	};
};

/* Format of messages that can be received by the library */

struct librskt_rsktd_to_app_msg {
	uint32_t in_use; /* Used for message pool tracking */
	uint32_t msg_type;
	union {
		struct librskt_resp a_rsp;
		struct req_librskt rq_a;
	};
};

#define MSG_SZ(x) ((x+7)&0xff8)
#define A2RSKTD_SZ MSG_SZ(sizeof(struct librskt_app_to_rsktd_msg))
#define RSKTD2A_SZ MSG_SZ(sizeof(struct librskt_rsktd_to_app_msg))

#define LIBRSKTD_SKT_FMT "/var/tmp/RSKTD%04d.%1d"

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_H__ */

