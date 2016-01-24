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
#ifndef MQ_MSG_H
#define MQ_MSG_H

#include <stdint.h>
#include <mqueue.h>

#include "rdma_types.h"

#define MQ_MSG_SIZE	128
#define MQ_RCV_BUF_SIZE	256

#define MQ_CONNECT_MS	 0x0401
#define MQ_ACCEPT_MS 	 0x0402
#define MQ_CLOSE_MSO	 0x0403
#define MQ_DISCONNECT_MS 0x0404
#define MQ_DESTROY	 0x0405
#define MQ_DESTROY_ACK	 0x0406

struct mq_connect_msg {
	/* rem refers to client. Those are the client msub's
	 * parameters.*/
	uint32_t rem_bytes;
	uint8_t	 rem_rio_addr_len;
	uint64_t rem_rio_addr_lo;
	uint8_t	 rem_rio_addr_hi;
	uint32_t rem_destid_len;
	uint32_t rem_destid;
	uint32_t rem_msubid;
	uint32_t rem_msid;	/* Client msid, used during disconnection */
	uint32_t seq_num;
};

struct mq_accept_msg {
	uint32_t server_msubid;
	uint32_t server_msid;	/* To be used during disconnection to locate the
				 * memory space and remove destids of client(s)
				 * from it */
	uint32_t server_bytes;
	uint8_t	 server_rio_addr_len;
	uint64_t server_rio_addr_lo;
	uint8_t  server_rio_addr_hi;
	uint8_t	 server_destid_len;
	uint32_t server_destid;
};

struct mq_disconnect_msg {
	uint32_t client_msubid;
};

/* Used for 'destroy' AND 'destroy_ack' */
struct mq_destroy_msg {
	uint32_t server_msid;
};

struct mq_destroy_ack_msg {
	uint32_t server_msid;
};

struct mq_rdma_msg {
	uint32_t	type;
	union {
		mq_connect_msg		connect_msg;
		mq_accept_msg		accept_msg;
		mq_disconnect_msg 	disconnect_msg;
		mq_destroy_msg		destroy_msg;
		mq_destroy_ack_msg	destroy_ack_msg;
	};
};

/* Message queue attributes */
extern struct mq_attr	attr;

void init_mq_attribs();

#endif


