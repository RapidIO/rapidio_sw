/* Definitions for CM Messaging between RSKT Daemons */
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

#include "librsktd_dmn_info.h"
#include "librsktd_lib_info.h"

#ifndef __LIBRSKTD_DMN_H__
#define __LIBRSKTD_DMN_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CM_MSG_HDR_SIZE 20
#define RSKTD_RESP_FLAG 0x1000
#define RSKTD_ERR_FLAG  0x2000

#define RSKTD_HELLO_REQ  0x21
#define RSKTD_HELLO_RESP (RSKTD_HELLO_REQ | RSKTD_RESP_FLAG)
#define RSKTD_CONNECT_REQ  0x42
#define RSKTD_CONNECT_RESP (RSKTD_CONNECT_REQ | RSKTD_RESP_FLAG)
#define RSKTD_CLOSE_REQ  0x84
#define RSKTD_CLOSE_RESP (RSKTD_CLOSE_REQ | RSKTD_RESP_FLAG)
#define RSKTD_CLI_CMD_REQ  0x108
#define RSKTD_CLI_CMD_RESP (RSKTD_CLOSE_REQ | RSKTD_RESP_FLAG)

#define RSKTD_REQ_STR(x) (x==RSKTD_HELLO_REQ)?"HELLO":  \
			(x==RSKTD_CONNECT_REQ)?"CONN ": \
			(x==RSKTD_CLOSE_REQ)?"CLOSE":   \
			(x==RSKTD_CLI_CMD_REQ)?" CLI ":">BAD<"

#define RSKTD_RESP_STR(x) (x==RSKTD_HELLO_RESP)?"HELLO_RSP":  \
			(x==RSKTD_CONNECT_RESP)?"CONN_RSP ": \
			(x==RSKTD_CLOSE_RESP)?"CLOSE_RSP":   \
			(x==RSKTD_CLI_CMD_RESP)?" CLI_RSP ":">ERROR<"

struct librsktd_hello_req {
	uint32_t ct; /* Peer component tag */
	uint32_t cm_skt; /* Peer cm socket number for connecting to */
	uint32_t cm_mp; /* Peer cm MPORT number */
};

struct librsktd_hello_resp {
	uint32_t peer_pid; /* Process ID of speer */
};

struct librsktd_connect_req {
	uint32_t dst_sn; /* Socket should be listening/accepting */
	uint32_t dst_ct; /* Thought we're sending here */
	uint32_t src_sn; /* Connecting socket number */
	/* Local mso/ms/msub to allocate for RDMA connect operation */
	char src_mso[MAX_MS_NAME];  /* Client mso name */
	char src_ms[MAX_MS_NAME]; /* Client ms name */
	uint32_t src_msub_o; /* Client msub offset within ms */
	uint32_t src_msub_s; /* Client msub size in bytes */
};

struct librsktd_connect_resp {
	uint32_t acc_sn; /* New socket number for connection, 0 on err */
	uint32_t dst_sn; /* Socket that was listening/accepting */
	uint32_t dst_ct; /* Request component tag */
	uint32_t dst_dmn_cm_skt; /* Request cm skt number*/
	/* Local mso/ms/msub to allocate for RDMA connect operation */
	char dst_ms[MAX_MS_NAME]; /* Server ms name */
	uint32_t msub_sz; /* Size of msub allocated */
};

struct librsktd_close_req {
	uint32_t rem_sn; /* remote socket that closed */
	uint32_t loc_sn; /* local socket to close */
	uint32_t force; /* 1 means close it, 0 means shutdown */
};

struct librsktd_close_resp {
	uint32_t status; /* Status after closure, guaranteed to be closed */
};

struct librsktd_cli_cmd_req {
	char cmd_line[2*MAX_MS_NAME]; /* remote socket that closed */
};

/* Note: src_msg_seq must match between request and response */

union librsktd_req {
	struct librsktd_hello_req hello;
	struct librsktd_connect_req con;
	struct librsktd_close_req clos;
	struct librsktd_cli_cmd_req cli;
};

union librsktd_resp {
	struct librsktd_hello_resp hello;
	struct librsktd_connect_resp con;
	struct librsktd_close_resp clos;
};

struct rsktd_req_msg {
	uint8_t unusable[CM_MSG_HDR_SIZE];
	/* Info about message sender */
	uint32_t msg_type; /* Message type */
	uint32_t msg_seq; /* Message sequence number */
	union librsktd_req msg;
};

struct rsktd_resp_msg {
	uint8_t unusable[CM_MSG_HDR_SIZE];
	uint32_t msg_type; /* Message type */
	uint32_t msg_seq; /* Request message sequence number */
	uint32_t err;  /* 0 means alls good */
	union librsktd_req req;
	union librsktd_resp msg;
};

void enqueue_wpeer_msg(struct librsktd_unified_msg *msg);
void enqueue_speer_msg(struct librsktd_unified_msg *msg);

/* CM Messages must be a multiple of 8 bytes */
#define DMN_MSG_SZ(x) ((x+7) & 0xFF8)
#define DMN_REQ_SZ (DMN_MSG_SZ(sizeof(struct rsktd_req_msg)))
#define DMN_RESP_SZ (DMN_MSG_SZ(sizeof(struct rsktd_resp_msg)))

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_DMN_H__ */

