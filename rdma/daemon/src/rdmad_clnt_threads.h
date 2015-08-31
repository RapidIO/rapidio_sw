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
#ifndef RDMAD_CLNT_THREADS_H
#define RDMAD_CLNT_THREADS_H

#include <stdint.h>
#include <pthread.h>

#include "cm_sock.h"
#include "unix_sock.h"
#include "rdmad_cm.h"
#include "ts_vector.h"

struct hello_daemon_info {
	hello_daemon_info(uint32_t destid, cm_client *client, pthread_t tid) :
		client(client), destid(destid), tid(tid)
	{
	}

	cm_client	*client;
	uint32_t	destid;
	pthread_t	tid;
	bool operator==(uint32_t destid)
	{
		return this->destid == destid;
	}
};
extern vector<hello_daemon_info>	hello_daemon_info_list;
extern sem_t 				hello_daemon_info_list_sem;

struct connected_to_ms_info {
	connected_to_ms_info(uint32_t client_msubid,
			     const char *server_msname,
			     uint32_t server_destid,
			     unix_server *other_server) :
	connected(false),
	client_msubid(client_msubid),
	server_msname(server_msname),
	server_msid(0xFFFF),
	server_destid(server_destid),
	other_server(other_server)
	{
	}

	bool operator==(uint32_t server_destid)
	{
		return this->server_destid == server_destid;
	}

	bool operator==(const char *server_msname)
	{
		return this->server_msname.compare(server_msname) == 0;
	}

	bool operator==(unix_server *other_server)
	{
		return this->other_server == other_server;
	}

	bool	 connected;
	uint32_t client_msubid;
	string	 server_msname;
	uint32_t server_msid;
	uint32_t server_destid;
	unix_server *other_server;
};
extern vector<connected_to_ms_info>	connected_to_ms_info_list;
extern sem_t 				connected_to_ms_info_list_sem;
int send_destroy_ms_for_did(uint32_t did);

void *wait_accept_thread_f(void *arg);
void *client_wait_destroy_thread_f(void *arg);
int provision_rdaemon(uint32_t destid);

#endif

