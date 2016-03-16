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

#include <string>
#include <mutex>

#include "unix_sock.h"
#include "cm_sock.h"
#include "tx_engine.h"
#include "daemon_info.h"

using std::string;
using std::mutex;

class unix_msg_t;

/**
 * @brief Each daemon shall maintain a list of all connections to remote memory
 * spaces. Each entry in that list represents a connection between one of
 * the daemon's applications, and a REMOTE memory space. As such a memory
 * space may appear multiple times in this list, each time with a different
 * 'client_msubid'. This list is created during the 'connect-to-ms' phase
 * and therefore each entry starts with 'connected' set to false. The tx_eng
 * data member contains the tx engine that the daemon shall use to communicate
 * with the application that requested the connection to the remote memory
 * space.
 *
 * @param client_msubid	Client memory subspace identifier provided by client
 * 			application when it called rdma_conn_ms_h(). Maybe
 * 			NULL_MSUBID.
 *
 * @param server_msname	Memory space name to connect to on the server daemon
 *
 * @param server_destid	Destination ID of the node hosting the server daemon
 *
 * @param to_lib_tx_eng Tx engine on the daemon that is used to communicate
 * 			with the client application.
 */
struct connected_to_ms_info {
	connected_to_ms_info(uint32_t client_msubid,
			     const char *server_msname,
			     uint32_t server_destid,
			     tx_engine<unix_server,unix_msg_t> *to_lib_tx_eng) :
	connected(false),
	client_msubid(client_msubid),
	server_msname(server_msname),
	server_msid(0),
	server_msubid(0),
	server_destid(server_destid),
	to_lib_tx_eng(to_lib_tx_eng)
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

	bool operator==(tx_engine<unix_server,unix_msg_t> *to_lib_tx_eng)
	{
		return this->to_lib_tx_eng == to_lib_tx_eng;
	}

	bool operator==(connected_to_ms_info& other)
	{
		return (this->client_msubid == other.client_msubid) &&
		       (this->server_msname == other.server_msname) &&
		       (this->server_destid == other.server_destid) &&
		       (this->server_msid == other.server_msid);
	}

	bool	 connected;
	uint32_t client_msubid;
	string	 server_msname;
	uint32_t server_msid;
	uint32_t server_msubid;
	uint32_t server_destid;
	tx_engine<unix_server,unix_msg_t> *to_lib_tx_eng;
};

extern daemon_list<cm_client>		hello_daemon_info_list;
extern vector<connected_to_ms_info>	connected_to_ms_info_list;
extern mutex 				connected_to_ms_info_list_mutex;

/**
 * @brief Sends FORCE_DISCONNECT_MS to RDMA library, and waits (with timeout)
 * 	  for FORCE_DISCONNECT_MS_ACK.
 *
 * @param server_msid	Server memory space identifier
 *
 * @param server_msubid	Server memory subspace identifier
 *
 * @param to_lib_tx_eng	The Tx engine that the daemon uses to communicate with the
 * 			library/app that created the memory space
 *
 * @return 0 if successul, non-zero otherwise
 */
int send_force_disconnect_ms_to_lib(uint32_t server_msid,
				  uint32_t server_msubid,
				  uint64_t client_to_lib_tx_eng_h);

/**
 * @brief The server daemon has died. Client daemon needs to:
 * 	  1. Notify the libraries of apps that have connected to memory spaces
 *           on that 'did' so they self-disconnect and clean their databases
 *           of the server's remove msub entries.
 *        2. Remove entries for that 'did' from the connected_to_ms_info_list.
 *
 * @param did	Destination ID of remote server daemon that has died
 *
 * @return 0 if successful OR there are no connections to the remote daemon.
 * 	   Non-zero otherwise
 */
int send_force_disconnect_ms_to_lib_for_did(uint32_t did);

/**
 * Provision a remote daemon by sending a HELLO message.
 *
 * @param destid	Destination ID of node running remote daemon
 */
int provision_rdaemon(uint32_t destid);


#endif

