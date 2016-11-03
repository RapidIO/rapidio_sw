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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>

#include <mutex>
#include <memory>
#include "memory_supp.h"

#include <cassert>

#include "rdma_types.h"
#include "liblog.h"
#include "cm_sock.h"
#include "rdmad_main.h"
#include "rdmad_inbound.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_tx_engine.h"
#include "rdmad_rx_engine.h"
#include "rdmad_msg_processor.h"
#include "rdmad_peer_utils.h"
#include "daemon_info.h"

using std::unique_ptr;
using std::move;
using std::make_shared;
using std::shared_ptr;
using std::mutex;
using std::lock_guard;

/* List of destids provisioned via the HELLO command/message */
daemon_list<cm_client>	hello_daemon_info_list;

vector<connected_to_ms_info>	connected_to_ms_info_list;
mutex 				connected_to_ms_info_list_mutex;

static cm_client_msg_processor d2d_msg_proc;

int send_force_disconnect_ms_to_lib(uint32_t server_msid,
				  uint32_t server_msubid,
				  uint64_t client_to_lib_tx_eng_h)
{
	int rc;

	/* Verify that there is an actual connection between one of the client
	 * apps of this daemon to the specified msid. */
	lock_guard<mutex> conn_lock(connected_to_ms_info_list_mutex);
	auto it = find_if(
		begin(connected_to_ms_info_list),
		end(connected_to_ms_info_list),
		[server_msid, server_msubid, client_to_lib_tx_eng_h]
		(connected_to_ms_info& info)
		{
			uint64_t to_lib_tx_eng = (uint64_t)info.to_lib_tx_eng;
			return (info.server_msid == server_msid) &&
			       (info.server_msubid == server_msubid) &&
			       (to_lib_tx_eng == client_to_lib_tx_eng_h) &&
			       info.connected;
		});
	if (it == end(connected_to_ms_info_list)) {
		WARN("No clients connected to memory space!\n");
		return 0;
	}

	/* Prepare for force disconnect ack notification BEFORE
	 * sending the force disconnect */
	rx_engine<unix_server, unix_msg_t> *rx_eng
					= it->to_lib_tx_eng->get_rx_eng();
	auto reply_sem = make_shared<sem_t>();
	sem_init(reply_sem.get(), 0, 0);
	rx_eng->set_notify(FORCE_DISCONNECT_MS_ACK, RDMA_CALL, 0, reply_sem);

	/* Send the FORCE_DISCONNECT_MS message */
	auto in_msg = make_unique<unix_msg_t>();
	in_msg->type 	= FORCE_DISCONNECT_MS;
	in_msg->category = RDMA_REQ_RESP;
	in_msg->seq_no 	= 0;
	in_msg->force_disconnect_ms_in.server_msid = server_msid;
	in_msg->force_disconnect_ms_in.server_msubid = server_msubid;
	it->to_lib_tx_eng->send_message(move(in_msg));

	/* Wait for FORCE_DISCONNECT_MS_ACK */
	unix_msg_t	out_msg;
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 1;	/* 1 second timeout */
	rc = sem_timedwait(reply_sem.get(), &timeout);
	if (rc) {
		ERR("reply_sem failed: %s\n", strerror(errno));
		if (errno == ETIMEDOUT) {
			ERR("Timeout occurred\n");
			rc = ETIMEDOUT;
		}
		return rc;
	}
	INFO("Got FORCE_DISCONNECT_MS_ACK\n");

	/* Read the FORCE_DISCONNECT_MS_ACK message contents */
	rc = rx_eng->get_message(FORCE_DISCONNECT_MS_ACK, RDMA_CALL, 0, &out_msg);
	if (rc) {
		ERR("Failed to obtain reply message, rc = %d\n", rc);
		return rc;
	}

	/* Verify that the FORCE_DISCONNECT_MS_ACK message contents match
	 * what we were expecting */
	bool ok = out_msg.force_disconnect_ms_ack_in.server_msid == server_msid;
	ok &= out_msg.force_disconnect_ms_ack_in.server_msubid == server_msubid;
	if (!ok) {
		ERR("Mismatched FORCE_DISCONNECT_MS_ACK\n");
		ERR("Expecting server_msid(0x%X), but got server_msid(0x%X)\n",
			server_msid,
			out_msg.force_disconnect_ms_ack_in.server_msid );
		ERR("Expecting server_msubid(0x%X), but got server_msubid(0x%X)\n",
			server_msubid,
			out_msg.force_disconnect_ms_ack_in.server_msubid);
		return -1;
	}

	return 0;
} /* send_force_disconnect_ms_to_lib() */


int send_force_disconnect_ms_to_lib_for_did(uint32_t did)
{
	int ret = 0;	/* The list could be empty */

	lock_guard<mutex> conn_lock(connected_to_ms_info_list_mutex);
	for (auto& conn_to_ms : connected_to_ms_info_list) {
		if ((conn_to_ms == did) && conn_to_ms.connected) {
			ret = send_force_disconnect_ms_to_lib(
					conn_to_ms.server_msid,
					conn_to_ms.server_msubid,
					(uint64_t)conn_to_ms.to_lib_tx_eng);
			if (ret) {
				ERR("Failed in send_force_disconnect_ms_to_lib '%s'\n",
					conn_to_ms.server_msname.c_str());
			}
		}
	}

	/* Now remove all entries delonging to 'did' from the
	 * connected_to_ms_info_list */
	connected_to_ms_info_list.erase(
			remove(begin(connected_to_ms_info_list),
			       end(connected_to_ms_info_list),
			       did),
			end(connected_to_ms_info_list));
	return ret;
} /* send_force_disconnect_ms_to_lib_for_did() */

int provision_rdaemon(uint32_t destid)
{
	int rc;

	DBG("ENTER\n");
	try {
		/* Create Client to connect to remote daemon */
		peer_info &peer = the_inbound->get_peer();
		shared_ptr<cm_client> the_client = make_shared<cm_client>
					   ("the_client",
					    peer.mport_id,
					    peer.prov_channel,
					    &shutting_down);

		/* Connect to remote daemon */
		rc = the_client->connect(destid);
		if (rc < 0) {
			CRIT("Failed to connect to destid(0x%X)\n", destid);
			throw RDMA_DAEMON_UNREACHABLE;
		}
		DBG("Connected to remote daemon at destid(0x%X)\n", destid);

		/* Now create a Tx and Rx engines for communicating
		 * with remote client. */
		auto cm_tx_eng = make_unique<cm_client_tx_engine>(
				"hello_tx_eng",
				the_client,
				cm_engine_cleanup_sem);

		auto cm_rx_eng = make_unique<cm_client_rx_engine>(
				"hello_rx_eng",
				the_client,
				d2d_msg_proc,
				cm_tx_eng.get(),
				cm_engine_cleanup_sem);

		/* Send HELLO message containing our destid */
		auto in_msg = make_unique<cm_msg_t>();
		in_msg->type = htobe64(CM_HELLO);
		in_msg->category = htobe64(RDMA_REQ_RESP);
		in_msg->seq_no = htobe64(0);
		in_msg->cm_hello.destid = htobe64(the_inbound->get_peer().destid);
		cm_tx_eng->send_message(move(in_msg));
		HIGH("HELLO message sent to destid(0x%X)\n", destid);

		/* Create entry for remote daemon */
		hello_daemon_info_list.add_daemon(move(cm_tx_eng),
						  move(cm_rx_eng),
						  destid);
		DBG("Created daemon entry in hello_damon_info_list\n");
	}
	catch(exception& e) {
		CRIT("Failed to create hello_client %s\n", e.what());
		rc = RDMA_MALLOC_FAIL;
	}
	catch(int& e) {
		rc = e;
	}
	catch(...) {
		CRIT("Other exception\n");
		rc = -1;
	}
	DBG("EXIT\n");
	return rc;
} /* provision_rdaemon() */

