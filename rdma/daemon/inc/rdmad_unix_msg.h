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
#ifndef RDMAD_UNIX_MSG_H
#define RDMAD_UNIX_MSG_H

#include <algorithm>
#include <iterator>

using std::begin;
using std::end;
using std::find_if;

#include "rdma_msg.h"

/* Max memory space name length */
#define	UNIX_MS_NAME_MAX_LEN	31

/* Type codes for RDMA_LIB_DAEMON_CALL messages */
#define CREATE_MSO		0x0001
#define CREATE_MSO_ACK		0x8001
#define CREATE_MS		0x0002
#define CREATE_MS_ACK		0x8002
#define CREATE_MSUB		0x0003
#define CREATE_MSUB_ACK		0x8003
#define ACCEPT_MS		0x0004
#define ACCEPT_MS_ACK		0x8004
#define DESTROY_MSUB		0x0005
#define DESTROY_MSUB_ACK 	0x8005
#define DESTROY_MS		0x0006
#define DESTROY_MS_ACK		0x8006
#define DESTROY_MSO		0x0007
#define DESTROY_MSO_ACK		0x8007
#define OPEN_MSO		0x0008
#define OPEN_MSO_ACK		0x8008
#define CLOSE_MSO		0x0009
#define CLOSE_MSO_ACK		0x8009
#define OPEN_MS			0x000A
#define OPEN_MS_ACK		0x800A
#define CLOSE_MS		0x000B
#define CLOSE_MS_ACK		0x800B
#define UNDO_ACCEPT		0x000C
#define UNDO_ACCEPT_ACK		0x800C
#define SEND_CONNECT		0x000D
#define SEND_CONNECT_ACK	0x800D
#define UNDO_CONNECT		0x000E
#define UNDO_CONNECT_ACK	0x800E
#define GET_MPORT_ID		0x000F
#define GET_MPORT_ID_ACK	0x800F
#define SEND_DISCONNECT		0x0010
#define SEND_DISCONNECT_ACK	0x8010
#define RDMAD_IS_ALIVE		0x001A
#define RDMAD_IS_ALIVE_ACK	0x801A
#define GET_IBWIN_PROPERTIES 	0x001B
#define GET_IBWIN_PROPERTIES_ACK 	0x801B
#define CONNECT_MS_REQ		0x001C
#define CONNECT_MS_RESP		0x001D
#define CONNECT_MS_RESP_ACK	0x801D
#define ACCEPT_FROM_MS_REQ	0x001E
#define SERVER_DISCONNECT_MS	0x001F
#define SERVER_DISCONNECT_MS_ACK	0x801F
#define RDMAD_KILL_DAEMON	0x0666

/* Type codes for RDMA_REQ_RESP messages */
#define FORCE_CLOSE_MSO		0x1001
#define FORCE_CLOSE_MS  	0x1002
#define DISCONNECT_MS   	0x1003
#define FORCE_DISCONNECT_MS	0x1004
#define FORCE_DISCONNECT_MS_ACK 0x9004

/* SUB_TYPEs */
#define ACCEPT_FROM_MS_REQ_NACK	0x02

struct type_name_entry {
	uint32_t type;
	const char *name;
};

#define STRINGIFY(s) (#s)

constexpr uint32_t NULL_MSUBID = 0xFFFFFFFF;
constexpr uint32_t NULL_MSID   = 0xFFFFFFFF;

constexpr type_name_entry type_name_table[] =
{
	{CREATE_MSO, STRINGIFY(CREATE_MSO)},
	{CREATE_MSO_ACK, STRINGIFY(CREATE_MSO_ACK)},
	{CREATE_MS, STRINGIFY(CREATE_MS)},
	{CREATE_MS_ACK, STRINGIFY(CREATE_MS_ACK)},
	{CREATE_MSUB, STRINGIFY(CREATE_MSUB)},
	{CREATE_MSUB_ACK, STRINGIFY(CREATE_MSUB_ACK)},
	{ACCEPT_MS, STRINGIFY(ACCEPT_MS)},
	{ACCEPT_MS_ACK, STRINGIFY(ACCEPT_MS_ACK)},
	{DESTROY_MSUB, STRINGIFY(DESTROY_MSUB)},
	{DESTROY_MSUB_ACK, STRINGIFY(DESTROY_MSUB_ACK)},
	{DESTROY_MS, STRINGIFY(DESTROY_MS)},
	{DESTROY_MS_ACK, STRINGIFY(DESTROY_MS_ACK)},
	{DESTROY_MSO, STRINGIFY(DESTROY_MSO)},
	{DESTROY_MSO_ACK, STRINGIFY(DESTROY_MSO_ACK)},
	{OPEN_MS, STRINGIFY(OPEN_MS)},
	{OPEN_MS_ACK, STRINGIFY(OPEN_MS_ACK)},
	{OPEN_MSO, STRINGIFY(OPEN_MSO)},
	{OPEN_MSO_ACK, STRINGIFY(OPEN_MSO_ACK)},
	{CLOSE_MS, STRINGIFY(CLOSE_MS)},
	{CLOSE_MS_ACK, STRINGIFY(CLOSE_MS_ACK)},
	{CLOSE_MSO, STRINGIFY(CLOSE_MSO)},
	{CLOSE_MSO_ACK, STRINGIFY(CLOSE_MSO_ACK)},
	{UNDO_ACCEPT, STRINGIFY(UNDO_ACCEPT)},
	{UNDO_ACCEPT_ACK, STRINGIFY(UNDO_ACCEPT_ACK)},
	{SEND_CONNECT, STRINGIFY(SEND_CONNECT)},
	{SEND_CONNECT_ACK, STRINGIFY(SEND_CONNECT_ACK)},
	{UNDO_CONNECT, STRINGIFY(UNDO_CONNECT)},
	{UNDO_CONNECT_ACK, STRINGIFY(UNDO_CONNECT_ACK)},
	{GET_MPORT_ID, STRINGIFY(GET_MPORT_ID)},
	{GET_MPORT_ID_ACK, STRINGIFY(GET_MPORT_ID_ACK)},
	{SEND_DISCONNECT, STRINGIFY(SEND_DISCONNECT)},
	{SEND_DISCONNECT_ACK, STRINGIFY(SEND_DISCONNECT_ACK)},
	{RDMAD_IS_ALIVE, STRINGIFY(RDMAD_IS_ALIVE)},
	{RDMAD_IS_ALIVE_ACK, STRINGIFY(RDMAD_IS_ALIVE_ACK)},
	{GET_IBWIN_PROPERTIES, STRINGIFY(GET_IBWIN_PROPERTIES)},
	{GET_IBWIN_PROPERTIES_ACK, STRINGIFY(GET_IBWIN_PROPERTIES_ACK)},
	{CONNECT_MS_REQ, STRINGIFY(CONNECT_MS_REQ)},
	{CONNECT_MS_RESP, STRINGIFY(CONNECT_MS_RESP)},
	{CONNECT_MS_RESP_ACK, STRINGIFY(CONNECT_MS_RESP_ACK)},
	{ACCEPT_FROM_MS_REQ, STRINGIFY(ACCEPT_FROM_MS_REQ)},
	{SERVER_DISCONNECT_MS, STRINGIFY(SERVER_DISCONNECT_MS)},
	{SERVER_DISCONNECT_MS_ACK, STRINGIFY(SERVER_DISCONNECT_MS_ACK)},
	{RDMAD_KILL_DAEMON, STRINGIFY(RDMAD_KILL_DAEMON)},
	{FORCE_CLOSE_MSO, STRINGIFY(FORCE_CLOSE_MSO)},
	{FORCE_CLOSE_MS, STRINGIFY(FORCE_CLOSE_MS)},
	{DISCONNECT_MS, STRINGIFY(DISCONNECT_MS)},
	{FORCE_DISCONNECT_MS, STRINGIFY(FORCE_DISCONNECT_MS)},
	{FORCE_DISCONNECT_MS_ACK, STRINGIFY(FORCE_DISCONNECT_MS_ACK)}
};

inline const char *type_name(const uint32_t type)
{
	auto it = find_if(begin(type_name_table),
			  end(type_name_table),
			  [type](const type_name_entry& entry)
			  {
				return entry.type == type;
			  });
	return (it != end(type_name_table)) ? it->name : "shit";
} /* type_name() */

/* rdmad_is_alive() arguments */
struct rdmad_is_alive_input {
	int dummy;
};

struct rdmad_kill_daemon_input {
	int dummy;
};

/* rdmad_is_alive() arguments */
struct rdmad_is_alive_output {
	int dummy;
};

/* get_mport_id() arguments */
struct get_mport_id_input {
	int dummy;
};

struct get_mport_id_output {
	int mport_id;
	int status;
};

/* create_mso() arguments */
struct create_mso_input {
	char owner_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct create_mso_output {
	uint32_t msoid;
	int	status;
};

/* open_mso() arguments */
struct open_mso_input {
	char owner_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct open_mso_output {
	uint32_t msoid;
	uint32_t mso_conn_id;
	int	status;
};

/* close_mso() arguments */
struct close_mso_input {
	uint32_t msoid;
	uint32_t mso_conn_id;
};
struct close_mso_output {
	int	status;
};

/* destroy_mso() arguments */
struct destroy_mso_input {
	uint32_t msoid;
};
struct destroy_mso_output {
	int	status;
};

/* create_ms() arguments */
struct create_ms_input {
	char ms_name[UNIX_MS_NAME_MAX_LEN+1];
	uint32_t msoid;
	uint32_t bytes;
	uint32_t flags;
};
struct create_ms_output {
	uint32_t msid;
	uint64_t phys_addr;
	uint64_t rio_addr;
	int	status;
};

/* open_ms() arguments */
struct open_ms_input {
	char ms_name[UNIX_MS_NAME_MAX_LEN+1];
	uint32_t msoid;
	uint32_t flags;
};
struct open_ms_output {
	uint32_t msid;
	uint64_t phys_addr;
	uint64_t rio_addr;
	uint32_t bytes;
	int	status;
};

/* close_ms() arguments */
struct close_ms_input {
	uint32_t msid;
};
struct close_ms_output {
	int	status;
};

/* destroy_ms() arguments */
struct destroy_ms_input {
	uint32_t msoid;
	uint32_t msid;
};
struct destroy_ms_output {
	int	status;
};

/* create_msub_h() arguments */
struct create_msub_input {
	uint32_t msid;
	uint32_t offset;
	uint32_t req_bytes;
};
struct create_msub_output {
	int	status;
	uint32_t msubid;
	uint32_t bytes;
	uint64_t rio_addr;
	uint64_t phys_addr;
};

/* destroy_msub() arguments */
struct destroy_msub_input {
	uint32_t msid;
	uint32_t msubid;
};
struct destroy_msub_output {
	int	status;
};

/* accept() arguments */
struct accept_input {
	uint32_t server_msid;
	uint32_t server_msubid;	/* Uniquely identifies which app called accept */
};

struct accept_output {
	int	status;
};

/* undo_accept() arguments */
struct undo_accept_input {
	uint32_t server_msid;
};
struct undo_accept_output {
	int	status;
};

/* send_connect() arguments */
struct send_connect_input {
	char server_msname[UNIX_MS_NAME_MAX_LEN+1];
	uint8_t  server_destid_len;
	uint32_t server_destid;
	uint8_t  client_destid_len;
	uint32_t client_destid;
	uint32_t client_msid;
	uint32_t client_msubid;
	uint32_t client_bytes;
	uint8_t	 client_rio_addr_len;
	uint64_t client_rio_addr_lo;
	uint8_t  client_rio_addr_hi;
	uint32_t seq_num;
	uint64_t connh;
};
struct send_connect_output {
	int	status;
};

/* undo_connect() arguments */
struct undo_connect_input {
	char server_ms_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct undo_connect_output {
	int	status;
};

/* send_disconnect() arguments */
struct send_disconnect_input {
	uint32_t client_msubid;
	uint32_t server_msid;
	uint32_t server_destid_len;
	uint32_t server_destid;
};
struct send_disconnect_output {
	int	status;
};

/* get_ibwin_properties arguments */
struct get_ibwin_properties_input {
	int	dummy;
};
struct get_ibwin_properties_output {
	unsigned	num_ibwins;
	uint32_t	ibwin_size;
	int		status;
};

struct force_close_mso_req_input {
	uint32_t msoid;
};

struct force_close_ms_req_input {
	uint32_t msid;
};

/* Server daemon to server library/app */
struct connect_to_ms_req_input {
	uint32_t client_msid;
	uint32_t client_msubid;
	uint32_t client_msub_bytes;
	uint8_t	 client_rio_addr_len;
	uint64_t client_rio_addr_lo;
	uint8_t	 client_rio_addr_hi;
	uint32_t client_destid_len;
	uint32_t client_destid;
	uint32_t seq_num;
	uint64_t client_to_lib_tx_eng_h;
	uint64_t connh;
};

/* Server daemon to server library/app */
struct disconnect_from_ms_req_input {
	uint32_t client_msubid;
	uint32_t server_msubid;
	uint64_t client_to_lib_tx_eng_h;
};

/* Server library/app to server daemon */
struct connect_to_ms_resp_input {
	uint32_t server_msid;
	uint32_t server_msubid;
	uint32_t server_msub_bytes;
	uint8_t	 server_rio_addr_len;
	uint64_t server_rio_addr_lo;
	uint8_t  server_rio_addr_hi;
	uint8_t	 server_destid_len;
	uint32_t server_destid;
	uint32_t client_msid;
	uint32_t client_msubid;
	uint32_t client_destid_len;
	uint32_t client_destid;
	uint64_t client_to_lib_tx_eng_h;
};
struct connect_to_ms_resp_output {
	int status;
};

/* Server disconnects MS connection with a particular client */
struct server_disconnect_ms_input {
	uint32_t client_msubid;
	uint32_t server_msid;
	uint32_t server_msubid;
	uint64_t client_to_lib_tx_eng_h;
};
struct server_disconnect_ms_output {
	int status;
};

/* Client daemon to client app */
/* Basically this is the 'connect_to_ms_resp' (ABOVE)
 * which is sent by the server,
 * and by the time it arrives at the client,
 * some of the fields are no  longer needed
 * (they are consumed by the client daemon).  */
struct accept_from_ms_req_input {
	uint32_t server_msid;
	uint32_t server_msubid;
	uint32_t server_msub_bytes;
	uint8_t	 server_rio_addr_len;
	uint64_t server_rio_addr_lo;
	uint8_t  server_rio_addr_hi;
	uint8_t	 server_destid_len;
	uint32_t server_destid;
};

/* Client daemon to client app */
/* The server destroyed the MS and the client is being
 * notified so it cleans up its database off the server's
 * ms, and msub. */
struct force_disconnect_ms_input {
	uint32_t server_msid;
	uint32_t server_msubid;
};

struct force_disconnect_ms_ack_input {
	uint32_t server_msid;
	uint32_t server_msubid;
};

/* Unix message structure */
struct unix_msg_t {
	rdma_msg_type	type;
	rdma_msg_sub_type sub_type;
	rdma_msg_cat	category;
	rdma_msg_seq_no	seq_no;
	union {
		struct get_mport_id_input	get_mport_id_in;
		struct get_mport_id_output	get_mport_id_out;
		struct create_mso_input		create_mso_in;
		struct create_mso_output	create_mso_out;
		struct open_mso_input		open_mso_in;
		struct open_mso_output		open_mso_out;
		struct close_mso_input		close_mso_in;
		struct close_mso_output 	close_mso_out;
		struct destroy_mso_input	destroy_mso_in;
		struct destroy_mso_output	destroy_mso_out;
		struct create_ms_input		create_ms_in;
		struct create_ms_output		create_ms_out;
		struct open_ms_input		open_ms_in;
		struct open_ms_output		open_ms_out;
		struct close_ms_input		close_ms_in;
		struct close_ms_output		close_ms_out;
		struct destroy_ms_input		destroy_ms_in;
		struct destroy_ms_output	destroy_ms_out;
		struct create_msub_input	create_msub_in;
		struct create_msub_output	create_msub_out;
		struct destroy_msub_input	destroy_msub_in;
		struct destroy_msub_output	destroy_msub_out;
		struct accept_input		accept_in;
		struct accept_output		accept_out;
		struct undo_accept_input	undo_accept_in;
		struct undo_accept_output	undo_accept_out;
		struct send_connect_input	send_connect_in;
		struct send_connect_output	send_connect_out;
		struct undo_connect_input	undo_connect_in;
		struct undo_connect_output	undo_connect_out;
		struct send_disconnect_input	send_disconnect_in;
		struct send_disconnect_output	send_disconnect_out;
		struct rdmad_is_alive_input	rdmad_is_alive_in;
		struct rdmad_is_alive_output	rdmad_is_alive_out;
		struct rdmad_kill_daemon_input	rdmad_kill_daemon_in;
		struct get_ibwin_properties_input  get_ibwin_properties_in;
		struct get_ibwin_properties_output get_ibwin_properties_out;
		struct force_close_mso_req_input   force_close_mso_req;
		struct force_close_ms_req_input    force_close_ms_req;
		struct connect_to_ms_req_input	connect_to_ms_req;
		struct connect_to_ms_resp_input	connect_to_ms_resp_in;
		struct connect_to_ms_resp_output connect_to_ms_resp_out;
		struct accept_from_ms_req_input	accept_from_ms_req_in;
		struct disconnect_from_ms_req_input disconnect_from_ms_req_in;
		struct force_disconnect_ms_input force_disconnect_ms_in;
		struct force_disconnect_ms_ack_input	force_disconnect_ms_ack_in;
		struct server_disconnect_ms_input server_disconnect_ms_in;
		struct server_disconnect_ms_output server_disconnect_ms_out;
	};

};

#endif

