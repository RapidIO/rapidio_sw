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

#ifndef CM_RDMA_H
#define CM_RDMA_H

#include <stdint.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "liblog.h"
#include "rdma_msg.h"

constexpr auto CM_MS_NAME_MAX_LEN 	= 31;

/* Message types */
constexpr uint32_t CM_HELLO 	 		= 0x01;
constexpr uint32_t CM_HELLO_ACK	 		= 0x02;
constexpr uint32_t CM_CONNECT_MS 		= 0x03;
constexpr uint32_t CM_ACCEPT_MS	 		= 0x04;
constexpr uint32_t CM_DISCONNECT_MS_REQ		= 0x05;
constexpr uint32_t CM_FORCE_DISCONNECT_MS 	= 0x06;
constexpr uint32_t CM_FORCE_DISCONNECT_MS_ACK 	= 0x07;

/**
 * @brief HELLO message exchanged between daemons during provisioning
 */
struct hello_msg_t {
	uint64_t destid;
};

/**
 * @brief Sent from client daemon to server daemon requesting connection
 * 	  to a server memory space
 */
struct cm_connect_msg {
	uint64_t	client_msid;	  /* Client msid */
	uint64_t	client_msubid;	  /* Client msub ID */
	uint64_t	client_bytes;	  /* Client msub length in bytes */
	uint64_t	client_rio_addr_len; /* Client length of RIO address */
	uint64_t	client_rio_addr_lo;  /* Client RIO address lo 64-bits */
	uint64_t	client_rio_addr_hi;  /* Client upper RIO address */
	uint64_t	client_to_lib_tx_eng_h;
	uint64_t	seq_num;
	uint64_t	connh;
	char		server_msname[CM_MS_NAME_MAX_LEN+1];
	/* Populated by daemon */
	uint64_t	client_destid_len;  /* Client length of destid */
	uint64_t	client_destid;	  /* Client node destination ID */

	void dump()
	{
		DBG("client_msid = 0x%" PRIx64 "\n", be64toh(client_msid));
		DBG("client_msubsid = 0x%" PRIx64 "\n",be64toh(client_msubid));
		DBG("client_bytes = 0x%" PRIx64 "\n", be64toh(client_bytes));
		DBG("client_rio_addr_len = 0x%" PRIx64 "\n",
						be64toh(client_rio_addr_len));
		DBG("client_rio_addr_lo = 0x%016" PRIx64 "\n",
						be64toh(client_rio_addr_lo));
		DBG("client_rio_addr_hi = 0x%016" PRIx64 "\n",
						be64toh(client_rio_addr_hi));
		DBG("client_destid_len = 0x%" PRIx64 "\n",
						be64toh(client_destid_len));
		DBG("client_destid = 0x%" PRIx64 "\n", be64toh(client_destid));
		DBG("seq_num = 0x%016" PRIx64 "\n", be64toh(seq_num));
		DBG("connh = 0x%016" PRIx64 "\n", be64toh(connh));
		DBG("client_to_lib_tx_eng_h = 0x%" PRIx64 "\n",
					be64toh(client_to_lib_tx_eng_h));
	}
};

constexpr uint32_t CM_ACCEPT_MS_ACK  = 0x01;
constexpr uint32_t CM_ACCEPT_MS_NACK = 0x02;

/**
 * @brief Sent from server daemon to client daemon indicating connection
 * 	  request to memory space was accepted.
 */
struct cm_accept_msg {
	uint64_t	sub_type;
	char		server_ms_name[CM_MS_NAME_MAX_LEN+1];
	uint64_t	server_msid;
	uint64_t	server_msubid;
	uint64_t	server_msub_bytes;
	uint64_t	server_rio_addr_len;
	uint64_t	server_rio_addr_lo;
	uint64_t	server_rio_addr_hi;
	uint64_t	server_destid_len;
	uint64_t	server_destid;
	uint64_t	client_msid;
	uint64_t	client_msubid;
	uint64_t	client_to_lib_tx_eng_h;
};

/**
 * @brief Sent from client daemon to server daemon requesting disconnection
 * 	  from specified memory space
 */
struct cm_disconnect_req_msg {
	uint64_t	type;
	uint64_t 	client_msubid;
	uint64_t 	client_destid;
	uint64_t 	client_destid_len;
	uint64_t	client_to_lib_tx_eng_h;
	uint64_t 	server_msid;
};

/**
 * @brief Sent from server daemon to client daemon to force disconnection
 * 	  from specified memory space either:
 * 	  - because the server has self-disconnected the memory space from
 * 	    the client; or
 * 	  - because the server has closed/destroyed the memory space.
 */
struct cm_force_disconnect_msg {
//	uint64_t	type;
	char 		server_msname[CM_MS_NAME_MAX_LEN+1];
	uint64_t	server_msid;
	uint64_t	server_msubid;
	uint64_t 	client_to_lib_tx_eng_h;
};

/**
 * @brief Acknowledge forced disconnection
 */
struct cm_force_disconnect_ack_msg {
	uint64_t	type;
	char server_msname[CM_MS_NAME_MAX_LEN+1];
	uint64_t server_msid;
};


/* Currently only used when tx_engine and rx_engine templates
 * are instantiated in terms of cm sockets and messages.
 */
struct cm_msg_t {
	rdma_msg_type	type;
	rdma_msg_cat	category;
	rdma_msg_seq_no	seq_no;
	union {
		hello_msg_t		hello;
		cm_connect_msg		cm_connect;
		cm_accept_msg		cm_accept;
		cm_disconnect_req_msg 	cm_disconnect_req;
		cm_force_disconnect_msg		cm_force_disconnect;
		cm_force_disconnect_ack_msg	cm_force_disconnect_ack;
	};
};
#endif
