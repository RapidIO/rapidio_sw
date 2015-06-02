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
#include "rdma_types.h"

#define	CM_MS_NAME_MAX_LEN	31

struct cm_connect_msg {
	char		server_msname[CM_MS_NAME_MAX_LEN+1];
	ms_h		client_msid;	  /* Client msid */
	uint64_t	client_msubid;	  /* Client msub ID */
	uint64_t	client_bytes;	  /* Client msub length in bytes */
	uint64_t	client_rio_addr_len; /* Client length of RIO address */
	uint64_t	client_rio_addr_lo;  /* Client RIO address lo 64-bits */
	uint64_t	client_rio_addr_hi;  /* Client lpper RIO address */
	/* Populated by daemon */
	uint64_t	client_destid_len;  /* Client length of destid */
	uint64_t	client_destid;	  /* Client node destination ID */
};

struct cm_accept_msg {
	char		server_ms_name[CM_MS_NAME_MAX_LEN+1];
	uint64_t	server_msid;	/* Server msid */
	uint64_t	server_msubid;
	uint64_t	server_bytes;	/* Length of msubh local to server */
	uint64_t	server_rio_addr_len;
	uint64_t	server_rio_addr_lo;
	uint64_t	server_rio_addr_hi;
	uint64_t	server_destid_len;
	uint64_t	server_destid;
};

struct cm_disconnect_msg {
	uint64_t client_msubid;
	uint64_t client_destid;
	uint64_t client_destid_len;
	uint64_t server_msid;
};

struct cm_destroy_msg {
	char server_msname[CM_MS_NAME_MAX_LEN+1];
	uint64_t server_msid;
};

struct cm_destroy_ack_msg {
	char server_msname[CM_MS_NAME_MAX_LEN+1];
	uint64_t server_msid;
};

#endif
