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

/* RDMA interface file */

/* RDMA types -- see RDMA_types.h */
typedef uint64_t ms_h;

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
	string owner_name<>;
};
struct create_mso_output {
	uint32_t msoid;
	int	status;
};

/* open_mso() arguments */
struct open_mso_input {
	string owner_name<>;
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
	string	ms_name<>;
	uint32_t msoid;
	uint32_t bytes;
	uint32_t flags;
};
struct create_ms_output {
	uint32_t msid;
	int	status;
};

/* open_ms() arguments */
struct open_ms_input {
	string	ms_name<>;
	uint32_t msoid;
	uint32_t flags;
};
struct open_ms_output {
	uint32_t msid;
	uint32_t ms_conn_id;
	uint32_t bytes;
	int	status;
};

/* close_ms() arguments */
struct close_ms_input {
	uint32_t msid;
	uint32_t ms_conn_id;
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
	string loc_ms_name<>;
	uint32_t loc_msid;
	uint32_t loc_msubid;
	uint32_t loc_bytes;
	uint8_t	 loc_rio_addr_len;
	uint64_t loc_rio_addr_lo;
	uint8_t  loc_rio_addr_hi;
};

struct accept_output {
	int	status;
};

/* undo_accept() arguments */
struct undo_accept_input {
	string server_ms_name<>;
};
struct undo_accept_output {
	int	status;
};

/* send_connect() arguments */
struct send_connect_input {
	string 	 server_msname<>;
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
};
struct send_connect_output {
	int	status;
};

/* undo_connect() arguments */
struct undo_connect_input {
	string server_ms_name<>;
};
struct undo_connect_output {
	int	status;
};

/* send_disconnect() arguments */
struct send_disconnect_input {
	uint32_t loc_msubid;
	uint32_t rem_msid;
	uint32_t rem_destid_len;
	uint32_t rem_destid;
};
struct send_disconnect_output {
	int	status;
};

program RDMAD {
	version RDMAD_1 {
		create_mso_output CREATE_MSO(create_mso_input) = 1;
		destroy_mso_output DESTROY_MSO(destroy_mso_input) = 2;
		create_ms_output CREATE_MS(create_ms_input) = 3;
		destroy_ms_output DESTROY_MS(destroy_ms_input) = 4;
		create_msub_output CREATE_MSUB(create_msub_input) = 5;
		destroy_msub_output DESTROY_MSUB(destroy_msub_input) = 6;
		accept_output ACCEPT(accept_input) = 7;
		send_connect_output SEND_CONNECT(send_connect_input) = 8;
		get_mport_id_output GET_MPORT_ID(get_mport_id_input) = 9;
		send_disconnect_output SEND_DISCONNECT(send_disconnect_input) = 10;
		open_mso_output OPEN_MSO(open_mso_input) = 11;
		close_mso_output CLOSE_MSO(close_mso_input) = 12;
		open_ms_output OPEN_MS(open_ms_input) = 13;
		close_ms_output CLOSE_MS(close_ms_input) = 14;
		undo_accept_output UNDO_ACCEPT(undo_accept_input) = 15;
		undo_connect_output UNDO_CONNECT(undo_connect_input) = 16;
	} = 1;
} = 0x51502112;
