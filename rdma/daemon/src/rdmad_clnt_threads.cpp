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

#include <mqueue.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#include <memory>

#include "memory_supp.h"

#include "liblog.h"
#include "cm_sock.h"
#include "rdmad_cm.h"
#include "rdmad_main.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_rx_engine.h"

using std::unique_ptr;
using std::move;
using std::make_shared;

/* List of destids provisioned via the HELLO command/message */
vector<hello_daemon_info>	hello_daemon_info_list;
sem_t				hello_daemon_info_list_sem;

vector<connected_to_ms_info>	connected_to_ms_info_list;
sem_t 				connected_to_ms_info_list_sem;

static int await_message(rx_engine<unix_server, unix_msg_t> *rx_eng,
			rdma_msg_cat category, rdma_msg_type type,
			rdma_msg_seq_no seq_no, unsigned timeout_in_secs,
			unix_msg_t *out_msg)
{
	auto rc = 0;

	/* Prepare for reply */
	auto reply_sem = make_shared<sem_t>();
	sem_init(reply_sem.get(), 0, 0);

	rc = rx_eng->set_notify(type, category, seq_no, reply_sem);

	/* Wait for reply */
	DBG("Notify configured...WAITING...\n");
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += timeout_in_secs;
	rc = sem_timedwait(reply_sem.get(), &timeout);
	if (rc) {
		ERR("reply_sem failed: %s\n", strerror(errno));
		if (timeout_in_secs && errno == ETIMEDOUT) {
			ERR("Timeout occurred\n");
			rc = ETIMEDOUT;
		}
	} else {
		DBG("Got reply!\n");
		rc = rx_eng->get_message(type, category, seq_no, out_msg);
		if (rc) {
			ERR("Failed to obtain reply message, rc = %d\n", rc);
		}
	}
	return rc;
} /* await_message() */

/**
 * Sends FORCE_DISCONNECT_MS to RDMA library, and waits (with timeout)
 * for FORCE_DISCONNECT_MS_ACK.
 */
static int send_force_disconnect_ms_to_lib(uint32_t server_msid,
				  uint32_t server_msubid,
				  uint64_t client_to_lib_tx_eng_h)
{
	int rc;

	/* Verify that there is an actual connection to the specified msid. */
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
		ERR("No clients connected to memory space!\n");
		rc = -1;
	} else {
		unix_msg_t	in_msg;
		in_msg.type = FORCE_DISCONNECT_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.force_disconnect_ms_in.server_msid = server_msid;
		in_msg.force_disconnect_ms_in.server_msubid = server_msubid;
		it->to_lib_tx_eng->send_message(&in_msg);

		unix_msg_t	out_msg;
		rx_engine<unix_server, unix_msg_t> *rx_eng
					= it->to_lib_tx_eng->get_rx_eng();
		rc = await_message(
			rx_eng, RDMA_LIB_DAEMON_CALL, FORCE_DISCONNECT_MS_ACK,
							0, 1, &out_msg);
		bool ok = out_msg.force_disconnect_ms_ack_in.server_msid ==
					in_msg.force_disconnect_ms_in.server_msid;

		ok &= out_msg.force_disconnect_ms_ack_in.server_msubid ==
					in_msg.force_disconnect_ms_in.server_msubid;
		if (rc) {
			ERR("Timeout waiting for FORCE_DISCONNECT_MS_ACK\n");
		} else if (!ok) {
			ERR("Mismatched FORCE_DISCONNECT_MS_ACK\n");
		}
	}
	return rc;
} /* send_force_disconnect_ms_to_lib() */

/**
 * The server daemon has died. Client daemon needs to:
 * 1. Notify the libraries of apps that have connected to memory spaces
 *    on that 'did' so they self-disconnect and clean their databases
 *    of the server's remove msub entries.
 * 2. Remove entries for that 'did' from the connected_to_ms_info_list.
 */
int send_force_disconnect_ms_to_lib_for_did(uint32_t did)
{
	int ret = 0;

	sem_wait(&connected_to_ms_info_list_sem);
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
	auto it = remove(begin(connected_to_ms_info_list),
			 end(connected_to_ms_info_list),
			 did);
	connected_to_ms_info_list.erase(it, end(connected_to_ms_info_list));

	sem_post(&connected_to_ms_info_list_sem);

	return ret;
} /* send_force_disconnect_ms_to_lib_for_did() */

struct wait_accept_destroy_thread_info {
	wait_accept_destroy_thread_info(cm_client *hello_client,
					uint32_t destid) :
					hello_client(hello_client),
					tid(0), destid(destid), rc(0)
	{
		sem_init(&started, 0, 0);
	}

	cm_client	*hello_client;
	pthread_t	tid;
	sem_t		started;
	uint32_t	destid;
	int		rc;	/* Whether thread working or failed */
};

/**
 * Thread for handling requests such as RDMA connection request, and
 * RDMA disconnection requests.
 */
void *wait_accept_destroy_thread_f(void *arg)
{
	assert(arg != NULL);
	wait_accept_destroy_thread_info *wadti =
			(wait_accept_destroy_thread_info *)arg;
	uint32_t destid = wadti->destid;
	cm_client *accept_destroy_client;

	try {
		/* Obtain pointer to hello_client */
		if (!wadti->hello_client) {
			CRIT("NULL argument. Exiting\n");
			throw -1;
		}

		/* Create a new cm_client based on the hello client socket */
		riomp_sock_t	client_socket = wadti->hello_client->get_socket();

		accept_destroy_client = new cm_client("accept_destroy_client",
						client_socket, &shutting_down);

		/* Send HELLO message containing our destid */
		hello_msg_t	*hm;
		accept_destroy_client->get_send_buffer((void **)&hm);
		hm->destid = htobe64(peer.destid);
		if (accept_destroy_client->send()) {
			ERR("Failed to send HELLO to destid(0x%X)\n", destid);
			throw -3;
		}
		HIGH("HELLO message sent to destid(0x%X)\n", destid);

		/* Receive HELLO (ack) message back with remote destid */
		hello_msg_t 	*ham;	/* HELLO-ACK message */
		accept_destroy_client->get_recv_buffer((void **)&ham);
		if (accept_destroy_client->timed_receive(5000)) {
			ERR("NO HELLO ACK from destid(0x%X)\n", destid);
			throw -4;
		}
		if (be64toh(ham->destid) != destid) {
			WARN("hello-ack destid(0x%X) != destid(0x%X)\n",
						be64toh(ham->destid), destid);
		}
		HIGH("HELLO ACK received from destid(0x%X)\n", destid);

		/* Store remote daemon info in the 'hello' daemon list */
		sem_wait(&hello_daemon_info_list_sem);
		hello_daemon_info_list.emplace_back(destid,
					accept_destroy_client, wadti->tid);
		HIGH("Stored info for destid(0x%X) in hello_daemon_info_list\n",
								wadti->destid);
		sem_post(&hello_daemon_info_list_sem);

		/* Post semaphore to caller to indicate thread is up */
		wadti->rc = 0;
		sem_post(&wadti->started);
	}
	catch(exception& e) {
		CRIT("Failed to create rx_conn_disc_server: %s\n", e.what());
		wadti->rc = -2;
		sem_post(&wadti->started);
		pthread_exit(0);
	}
	catch(int e) {
		switch(e) {

		case -3:
		case -4:
			delete accept_destroy_client;
			delete wadti->hello_client;	/* FIXME: Is this OK?? */
			/* no break */
		default:
			wadti->rc = e;
			sem_post(&wadti->started);
			pthread_exit(0);
		}
	}

	while(1) {
		int	ret;
		/* Receive ACCEPT_MS, or DESTROY_MS message */
		DBG("Waiting for ACCEPT_MS or DESTROY_MS\n");
		ret = accept_destroy_client->receive();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called\n");
			} else {
				CRIT("Failed to receive on hello_client: %s\n",
								strerror(ret));
			}

			/* If we just failed to receive() then we should also
			 * clear the entry in hello_daemon_info_list. If we are
			 * shutting down, the shutdown function would be accessing
			 * the list so we should NOT erase an element from it. */
			if (!shutting_down) {
				/* Remove entry from hello_daemon_info_list */
				WARN("Removing entry from hello_daemon_info_list\n");
				sem_wait(&hello_daemon_info_list_sem);
				auto it = find(begin(hello_daemon_info_list),
					       end(hello_daemon_info_list),
					       destid);
				if (it != end(hello_daemon_info_list))
					hello_daemon_info_list.erase(it);
				sem_post(&hello_daemon_info_list_sem);
			}

			delete accept_destroy_client;
			delete wadti->hello_client;	/* FIXME: Is this OK?? */

			CRIT("Exiting thread\n");
			pthread_exit(0);
		}

		/* Read all messages as ACCEPT_MS first, then if the
		 * type is different then cast message buffer accordingly. */
		cm_accept_msg	*accept_cm_msg;
		accept_destroy_client->get_recv_buffer((void **)&accept_cm_msg);
		if (be64toh(accept_cm_msg->type) == CM_ACCEPT_MS) {
			HIGH("Received ACCEPT_MS from %s\n",
						accept_cm_msg->server_ms_name);

			/* Find the entry matching the memory space and tx_eng
			 * and also make sure it is not already marked as 'connected'.
			 * If not found, there is nothing to do and the ACCEPT_MS
			 * is ignored. */
			auto it = find_if
				(begin(connected_to_ms_info_list),
				end(connected_to_ms_info_list),
				[accept_cm_msg](connected_to_ms_info& info)
				{
					bool match = (info.server_msname ==
						accept_cm_msg->server_ms_name);
					match &= !info.connected;
					match &=
					(be64toh((uint64_t)info.to_lib_tx_eng)
							==
					accept_cm_msg->client_to_lib_tx_eng_h);
					return match;
				});
			if (it == end(connected_to_ms_info_list)) {
				WARN("Ignoring CM_ACCEPT_MS from ms('%s')\n",
						accept_cm_msg->server_ms_name);
				continue;
			}

			/* Compose the ACCEPT_FROM_MS_REQ that is to be sent
			 * over to the BLOCKED rdma_conn_ms_h(). */
			static unix_msg_t in_msg;
			accept_from_ms_req_input *accept_msg = &in_msg.accept_from_ms_req_in;

			in_msg.category = RDMA_LIB_DAEMON_CALL;
			in_msg.type	= ACCEPT_FROM_MS_REQ;
			accept_msg->server_msid
					= be64toh(accept_cm_msg->server_msid);
			accept_msg->server_msubid
					= be64toh(accept_cm_msg->server_msubid);
			accept_msg->server_msub_bytes
				= be64toh(accept_cm_msg->server_msub_bytes);
			accept_msg->server_rio_addr_len
				= be64toh(accept_cm_msg->server_rio_addr_len);
			accept_msg->server_rio_addr_lo
				= be64toh(accept_cm_msg->server_rio_addr_lo);
			accept_msg->server_rio_addr_hi
				= be64toh(accept_cm_msg->server_rio_addr_hi);
			accept_msg->server_destid_len
				= be64toh(accept_cm_msg->server_destid_len);
			accept_msg->server_destid
				= be64toh(accept_cm_msg->server_destid);
			DBG("Accept: msubid=0x%X msid= 0x%X destid=0x%X destid_len=0x%X, rio=0x%"
										PRIx64 "\n",
						accept_msg->server_msubid,
						accept_msg->server_msid,
						accept_msg->server_destid,
						accept_msg->server_destid_len,
						accept_msg->server_rio_addr_lo);
			DBG("Accept: msub_bytes = %u, rio_addr_len = %u\n",
						accept_msg->server_msub_bytes,
						accept_msg->server_rio_addr_len);

			/* Send the ACCEPT_FROM_MS_REQ message to the blocked
			 * rdma_conn_ms_h() via the tx engine */
			it->to_lib_tx_eng->send_message(&in_msg);

			sem_wait(&connected_to_ms_info_list_sem);
			/* Update the corresponding element of connected_to_ms_info_list */
			/* By setting this entry to 'connected' it is ignored if there
			 * is an ACCEPT_MS destined for another client. */
			DBG("Setting '%s' to 'connected\n", accept_cm_msg->server_ms_name);
			it->connected = true;
			it->server_msid = be64toh(accept_cm_msg->server_msid);
			it->server_msubid = be64toh(accept_cm_msg->server_msubid);
			sem_post(&connected_to_ms_info_list_sem);

		} else if (be64toh(accept_cm_msg->type) == CM_FORCE_DISCONNECT_MS) {
			cm_force_disconnect_msg	*force_disconnect_msg;
			/* Receive CM_FORCE_DISCONNECT_MS */
			accept_destroy_client->get_recv_buffer(
							(void **)&force_disconnect_msg);

			HIGH("Received CM_FORCE_DISCONNECT_MS containing '%s'\n",
					force_disconnect_msg->server_msname);

			/* Relay to library and get ACK back */
			int rc = send_force_disconnect_ms_to_lib(
					be64toh(force_disconnect_msg->server_msid),
					be64toh(force_disconnect_msg->server_msubid),
					be64toh(force_disconnect_msg->client_to_lib_tx_eng_h)
					);
			if (rc) {
				ERR("Failed to send destroy message to library or get ack\n");
				/* Don't exit; there maybe a problem with that memory space
				 * but not with others */
				continue;
			}

			/* Remove the entry relating to the destroyed ms. The entry fields must
			 * match the 'CM_FORCE_DISCONNECT_MS */
			sem_wait(&connected_to_ms_info_list_sem);
			connected_to_ms_info_list.erase (
				remove_if(begin(connected_to_ms_info_list),
				  end(connected_to_ms_info_list),
				  [&](connected_to_ms_info& info) {
					return (info.server_msid == be64toh(force_disconnect_msg->server_msid))
					&&     (info.server_msubid == be64toh(force_disconnect_msg->server_msubid))
					&&     ((uint64_t)info.to_lib_tx_eng == be64toh(force_disconnect_msg->client_to_lib_tx_eng_h));
   				})
   				, end(connected_to_ms_info_list));
			sem_post(&connected_to_ms_info_list_sem);

			/**
			 * Send back CM_FORCE_DISCONNECT_MS_ACK to the remote
			 * daemon on which the memory space was either
			 * destroyed or self-disconnected by its server. */
			cm_destroy_ack_msg *dam;

			/* Flush CM send buffer of previous message */
			accept_destroy_client->get_send_buffer((void **) &dam);
			accept_destroy_client->flush_send_buffer();

			/* Now send back a destroy_ack CM message */
			dam->type	= htobe64(CM_FORCE_DISCONNECT_MS_ACK);
			strcpy(dam->server_msname, force_disconnect_msg->server_msname);
			dam->server_msid = force_disconnect_msg->server_msid; /* Both are BE */
			if (accept_destroy_client->send()) {
				WARN("Failed to send CM_FORCE_DISCONNECT_MS_ACK to server daemon\n");
			} else {
				HIGH("Sent CM_FORCE_DISCONNECT_MS_ACK to server daemon\n");
			}
		} else {
			CRIT("Got an unknown message code (0x%X)\n", accept_cm_msg->type);
			assert(false);
		}
	} /* while(1) */
	pthread_exit(0);
} /* wait_accept_destroy_thread_f() */

/**
 * Provision a remote daemon by sending a HELLO message.
 */
int provision_rdaemon(uint32_t destid)
{
	int rc;

	const auto FAILED_TO_CONNECT  = -1;
	const auto FAILED_TO_ALLOCATE = -2;
	const auto FAILED_TO_CREATE_THREAD = -3;
	wait_accept_destroy_thread_info *wadti = NULL;

	/* If the 'destid' is already known, kill its thread */
	sem_wait(&hello_daemon_info_list_sem);
	auto it = find(begin(hello_daemon_info_list), end(hello_daemon_info_list),
			destid);
	if (it != end(hello_daemon_info_list)) {
		WARN("destid(0x%X) is already known\n", destid);
		/* FIXME: 1. When does this happen?
		 * 	  2. Don't we also need to remove the entry from
		 * 	  hello_daemon_info_list so we don't have multiple entries
		 * 	  for the same destid?
		 */
		pthread_kill(it->tid, SIGUSR1);
	}
	sem_post(&hello_daemon_info_list_sem);

	cm_client *hello_client = nullptr;

	try {
		/* Create provision client to connect to remote
		 * daemon's provisioning thread */
		hello_client = new cm_client("hello_client",
						peer.mport_id,
						peer.prov_mbox_id,
						peer.prov_channel,
						&shutting_down);
		/* Connect to remote daemon */
		rc = hello_client->connect(destid);
		if (rc) {
			CRIT("Failed to connect to destid(0x%X)\n", destid);
			throw FAILED_TO_CONNECT;
		}
		HIGH("Connected to remote daemon on destid(0x%X)\n", destid);

		wadti = new wait_accept_destroy_thread_info(hello_client, destid);
		if (!wadti) {
			CRIT("Failed to allocate wadti: %s\n", strerror(errno));
			throw FAILED_TO_ALLOCATE;
		}

		DBG("Creating wait_accept_destroy_thread\n");
		rc = pthread_create(&wadti->tid, NULL,
				    wait_accept_destroy_thread_f, wadti);
		if (rc) {
			CRIT("Failed to create wait_accept_destroy_thread: rc = %d, '%s'\n",
					rc, strerror(errno));
			throw FAILED_TO_CREATE_THREAD;
		}

		/* Determine whether thread worked or failed from 'rc' */
		sem_wait(&wadti->started);
		rc = wadti->rc;
		if (rc) {
			ERR("wait_accept_destroy_thread failed\n");
		} else {
			DBG("wait_accept_destroy_thread started successfully\n");
		}

		/* Free the wadti struct */
		delete wadti;
	}
	catch(exception& e) {
		CRIT("Failed to create hello_client %s\n", e.what());
		rc = -100;
	}
	catch(int e) {
		switch(e) {
		case FAILED_TO_CREATE_THREAD:
			delete wadti;
			/* no break */
		case FAILED_TO_ALLOCATE:
		case FAILED_TO_CONNECT:
			if (hello_client != nullptr)
				delete hello_client;
			break;
		}
		rc = e;
	}
	return rc;
} /* provision_rdaemon() */

