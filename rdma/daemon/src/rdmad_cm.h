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

#include <cstring>

#include "liblog.h"
#include "rdma_msg.h"

constexpr auto CM_MS_NAME_MAX_LEN 		= 31;

/* Message types */
constexpr uint32_t CM_HELLO 	 		= 0x101;
constexpr uint32_t CM_HELLO_ACK	 		= 0x102;
constexpr uint32_t CM_CONNECT_MS 		= 0x103;
constexpr uint32_t CM_ACCEPT_MS	 		= 0x104;
constexpr uint32_t CM_DISCONNECT_MS_REQ		= 0x105;
constexpr uint32_t CM_FORCE_DISCONNECT_MS 	= 0x106;
constexpr uint32_t CM_FORCE_DISCONNECT_MS_ACK 	= 0x107;

/**
 * @brief HELLO message exchanged between daemons during provisioning
 */
struct cm_hello_msg_t {
	uint64_t destid;
};
struct cm_hello_ack_msg_t {
	uint64_t destid;
};
/**
 * @brief Sent from client daemon to server daemon requesting connection
 * 	  to a server memory space
 */
struct cm_connect_ms_msg {
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

/* CM_ACCEPT_MS subtypes */
constexpr uint32_t CM_ACCEPT_MS_ACK  = 0x01;
constexpr uint32_t CM_ACCEPT_MS_NACK = 0x02;

/**
 * @brief Sent from server daemon to client daemon indicating connection
 * 	  request to memory space was accepted.
 */
struct cm_accept_ms_msg {
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
struct cm_disconnect_ms_req_msg {
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
struct cm_force_disconnect_ms_msg {
	char 		server_msname[CM_MS_NAME_MAX_LEN+1];
	uint64_t	server_msid;
	uint64_t	server_msubid;
	uint64_t 	client_to_lib_tx_eng_h;
};

/**
 * @brief Acknowledge forced disconnection
 */
struct cm_force_disconnect_ms_ack_msg {
	char server_msname[CM_MS_NAME_MAX_LEN+1];
	uint64_t server_msid;
	cm_force_disconnect_ms_ack_msg(cm_force_disconnect_ms_ack_msg& other) {
		strcpy(server_msname, other.server_msname);
		server_msid = other.server_msid;
	}

};


/* Currently only used when tx_engine and rx_engine templates
 * are instantiated in terms of cm sockets and messages.
 */
struct cm_msg_t {
	rdma_msg_type	type;
	rdma_msg_cat	category;
	rdma_msg_seq_no	seq_no;
	union {
		cm_hello_msg_t			cm_hello;
		cm_hello_ack_msg_t		cm_hello_ack;
		cm_connect_ms_msg		cm_connect_ms;
		cm_accept_ms_msg		cm_accept_ms;
		cm_disconnect_ms_req_msg 	cm_disconnect_ms_req;
		cm_force_disconnect_ms_msg		cm_force_disconnect_ms;
		cm_force_disconnect_ms_ack_msg	cm_force_disconnect_ms_ack;
	};

	cm_msg_t() {}

	cm_msg_t(const cm_msg_t& other) :
		type(other.type),
		category(other.category),
		seq_no(other.seq_no)
	{
		DBG("######### COPY CTOR CALLED #########");
		switch(type) {
		case CM_HELLO:
			cm_hello = other.cm_hello;
		break;
		case CM_HELLO_ACK:
			cm_hello_ack = other.cm_hello_ack;
		break;

		case CM_CONNECT_MS:
			cm_connect_ms = other.cm_connect_ms;
		break;

		case CM_DISCONNECT_MS_REQ:
			cm_disconnect_ms_req = other.cm_disconnect_ms_req;
		break;

		case CM_FORCE_DISCONNECT_MS:
			cm_force_disconnect_ms = other.cm_force_disconnect_ms;
		break;

		case CM_FORCE_DISCONNECT_MS_ACK:
			cm_force_disconnect_ms_ack =
					other.cm_force_disconnect_ms_ack;
		break;
		default:
			abort();
		}
	}
};
#endif
