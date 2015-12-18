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

#include <memory>

#include "memory_supp.h"

#include "rdma_mq_msg.h"
#include "liblog.h"
#include "cm_sock.h"
#include "rdmad_cm.h"
#include "rdmad_main.h"
#include "rdmad_clnt_threads.h"

using std::unique_ptr;
using std::move;

/* List of destids provisioned via the HELLO command/message */
vector<hello_daemon_info>	hello_daemon_info_list;
sem_t	hello_daemon_info_list_sem;

vector<connected_to_ms_info>	connected_to_ms_info_list;
sem_t 				connected_to_ms_info_list_sem;
static int send_destroy_ms_to_lib(const char *server_ms_name,
			   uint32_t server_msid);

struct wait_accept_destroy_thread_info {
	wait_accept_destroy_thread_info(unique_ptr<cm_client> && hello_client,
					uint32_t destid) : tid(0), destid(destid)
	{
		this->hello_client = move(hello_client);
		sem_init(&started, 0, 0);
	}

	unique_ptr<cm_client>	hello_client;
	pthread_t	tid;
	sem_t		started;
	uint32_t	destid;
};

int send_destroy_ms_for_did(uint32_t did)
{
	int ret = 0;

	sem_wait(&connected_to_ms_info_list_sem);
	for (auto& conn_to_ms : connected_to_ms_info_list) {
		if ((conn_to_ms == did) && conn_to_ms.connected) {
			int ret = send_destroy_ms_to_lib(
					conn_to_ms.server_msname.c_str(),
					conn_to_ms.server_msid);
			if (ret) {
				ERR("Failed to send destroy for '%s'\n",
					conn_to_ms.server_msname.c_str());
			}
		}
	}
	remove(begin(connected_to_ms_info_list),
	       end(connected_to_ms_info_list),
	       did);
	sem_post(&connected_to_ms_info_list_sem);

	return ret;
} /* send_destroy_ms_for_did() */

static int send_destroy_ms_to_lib(
		const char *server_ms_name,
		uint32_t server_msid)
{
	int ret;

	/* Prepare POSIX message queue name */
	std::stringstream	mq_name;
	mq_name << "/dest-" << server_ms_name;

	try {
		/* Open destroy/destroy-ack message queue */
		auto destroy_mq = make_unique<msg_q<mq_destroy_msg>>(
					mq_name.str().c_str(), MQ_OPEN);

		/* Send 'destroy' POSIX message to the RDMA library */
		mq_destroy_msg	*dest_msg;
		destroy_mq->get_send_buffer(&dest_msg);
		dest_msg->server_msid = server_msid;
		if (destroy_mq->send()) {
			ERR("Failed to send 'destroy' message to client.\n");
			throw -2;
		}

		/* Message buffer for receiving destroy ack message */
		mq_destroy_msg *destroy_ack_msg;
		destroy_mq->get_recv_buffer(&destroy_ack_msg);

		/* Wait for 'destroy_ack', but with timeout; we cannot be
		 * stuck here if the library fails to send the 'destroy_ack' */
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += 5;
		if (destroy_mq->timed_receive(&tm)) {
			/* The server daemon will timeout on the destory_ack
			 * reception since it is using a timed receive CM API */
			HIGH("Timed out without receiving ACK to destroy\n");
			throw -3;
		} else {
			HIGH("POSIX destroy_ack rcvd for %s\n", server_ms_name);
			ret = 0;	/* SUCCESS */
		}
	}
	catch(msg_q_exception& e) {
		ERR("Failed to open 'destroy' POSIX queue (%s): %s\n",
					mq_name.str().c_str(), e.msg.c_str());
		ret = -1;
	}
	catch(int e) {
		ret = e;
	}

	return ret;
} /* send_destroy_ms_to_lib() */

/**
 * Functor for matching a memory space on both server_destid and
 * server_msid.
 */
struct match_ms {
	match_ms(uint16_t server_destid, uint32_t server_msid) :
		server_destid(server_destid), server_msid(server_msid)
	{}

	bool operator()(connected_to_ms_info& cmi)
	{
		return (cmi.server_msid == this->server_msid) &&
		       (cmi.server_destid == this->server_destid);
	}

	uint16_t server_destid;
	uint32_t server_msid;
};

/**
 * Request for handling requests such as RDMA connection request, and
 * RDMA disconnection requests.
 */
void *wait_accept_destroy_thread_f(void *arg)
{
	DBG("ENTER\n");
	if (!arg) {
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
	}


	wait_accept_destroy_thread_info *wadti =
			(wait_accept_destroy_thread_info *)arg;
	uint32_t destid = wadti->destid;

	/* Obtain pointer to hello_client */
	if (!wadti->hello_client) {
		CRIT("NULL argument. Exiting\n");
		delete wadti;
		pthread_exit(0);
	}

	/* Create a new cm_client based on the hello client socket */
	riomp_sock_t	client_socket = wadti->hello_client->get_socket();
	cm_client *accept_destroy_client;
	try {
		accept_destroy_client = new cm_client("accept_destroy_client",
						      client_socket,
						      &shutting_down);
	}
	catch(cm_exception& e) {
		CRIT("Failed to create rx_conn_disc_server: %s\n", e.err);
		delete wadti;
		pthread_exit(0);
	}

	/* Send HELLO message containing our destid */
	hello_msg_t	*hm;
	accept_destroy_client->get_send_buffer((void **)&hm);
	hm->destid = htobe64(peer.destid);
	if (accept_destroy_client->send()) {
		ERR("Failed to send HELLO to destid(0x%X)\n", destid);
		delete wadti;
		pthread_exit(0);
	}
	HIGH("HELLO message successfully sent to destid(0x%X)\n", destid);

	/* Receive HELLO (ack) message back with remote destid */
	hello_msg_t 	*ham;	/* HELLO-ACK message */
	accept_destroy_client->get_recv_buffer((void **)&ham);
	if (accept_destroy_client->timed_receive(5000)) {
		ERR("Failed to receive HELLO ACK from destid(0x%X)\n", destid);
		delete wadti;
		pthread_exit(0);
	}
	if (be64toh(ham->destid) != destid) {
		WARN("hello-ack destid(0x%X) != destid(0x%X)\n", be64toh(ham->destid), destid);
	}
	HIGH("HELLO ACK successfully received from destid(0x%X)\n", destid);

	/* Store remote daemon info in the 'hello' daemon list */
	sem_wait(&hello_daemon_info_list_sem);
	hello_daemon_info_list.emplace_back(destid, accept_destroy_client, wadti->tid);
	HIGH("Stored info for destid(0x%X) in hello_daemon_info_list\n", wadti->destid);
	sem_post(&hello_daemon_info_list_sem);

	/* Post semaphore to caller to indicate thread is up */
	sem_post(&wadti->started);

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

			delete wadti;

			/* Free the cm_client object */
			delete accept_destroy_client;

			/* If we just failed to receive() then we should also
			 * clear the entry in hello_daemon_info_list. If we are
			 * shutting down, the shutdown function would be accessing
			 * the list so we should NOT erase an element from it.
			 */
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

			/* Form message queue name from memory space name */
			char mq_name[CM_MS_NAME_MAX_LEN+2];
			mq_name[0] = '/';
			strcpy(&mq_name[1], accept_cm_msg->server_ms_name);
			string mq_str(mq_name);

			/* Is the message queue name in wait_accept_mq_names? */
			/* Not found. Ignore message since no one is waiting for it */
			if (!wait_accept_mq_names.contains(mq_str)) {
				WARN("Ignoring message from ms('%s')!\n",
						accept_cm_msg->server_ms_name);
				continue;
			}

			/* All is good, and we need to relay the message back
			 * to rdma_conn_ms_h() via POSIX messaging */
			/* Open message queue */
			msg_q<mq_accept_msg>	*accept_mq;
			try {
				accept_mq =
				    new msg_q<mq_accept_msg>(mq_name, MQ_OPEN);
			}
			catch(msg_q_exception& e) {
				ERR("Failed to open POSIX queue '%s': %s\n",
						mq_name, e.msg.c_str());
				continue;
			}
			DBG("Opened POSIX queue '%s'\n", mq_name);

			/* POSIX accept message preparation */
			mq_accept_msg	*accept_mq_msg;
			accept_mq->get_send_buffer(&accept_mq_msg);
			accept_mq_msg->server_msubid		= be64toh(accept_cm_msg->server_msubid);
			accept_mq_msg->server_msid		= be64toh(accept_cm_msg->server_msid);
			accept_mq_msg->server_bytes		= be64toh(accept_cm_msg->server_bytes);
			accept_mq_msg->server_rio_addr_len	= be64toh(accept_cm_msg->server_rio_addr_len);
			accept_mq_msg->server_rio_addr_lo	= be64toh(accept_cm_msg->server_rio_addr_lo);
			accept_mq_msg->server_rio_addr_hi	= be64toh(accept_cm_msg->server_rio_addr_hi);
			accept_mq_msg->server_destid_len	= be64toh(accept_cm_msg->server_destid_len);
			accept_mq_msg->server_destid		= be64toh(accept_cm_msg->server_destid);
			DBG("CM Accept: msubid=0x%X msid=0x%X destid=0x%X, destid_len=0x%X, rio=0x%lX\n",
							be64toh(accept_cm_msg->server_msubid),
							be64toh(accept_cm_msg->server_msid),
							be64toh(accept_cm_msg->server_destid),
							be64toh(accept_cm_msg->server_destid_len),
							be64toh(accept_cm_msg->server_rio_addr_lo));
			DBG("CM Accept: bytes = %u, rio_addr_len = %u\n",
							be64toh(accept_cm_msg->server_bytes),
							be64toh(accept_cm_msg->server_rio_addr_len));
			DBG("MQ Accept: msubid=0x%X msid= 0x%X destid=0x%X destid_len=0x%X, rio=0x%lX\n",
								accept_mq_msg->server_msubid,
								accept_mq_msg->server_msid,
								accept_mq_msg->server_destid,
								accept_mq_msg->server_destid_len,
								accept_mq_msg->server_rio_addr_lo);
			DBG("MQ Accept: bytes = %u, rio_addr_len = %u\n",
								accept_mq_msg->server_bytes,
								accept_mq_msg->server_rio_addr_len);
			/* Send POSIX accept back to conn_ms_h() */
			if (accept_mq->send()) {
				WARN("Failed to send accept_msg on '%s': %s\n", strerror(errno));
				delete accept_mq;
				continue;
			}

			/* Delete the accept message queue object */
			delete accept_mq;

			/* All is good. Just remove the processed mq_name from the
			 * wait_accept_mq_names list and go back and wait for the
			 * next accept message */
			wait_accept_mq_names.remove(mq_str);

			/* Update the corresponding element of connected_to_ms_info_list */
			sem_wait(&connected_to_ms_info_list_sem);
			auto it = find(begin(connected_to_ms_info_list),
				       end(connected_to_ms_info_list),
				       accept_cm_msg->server_ms_name);
			if (it == end(connected_to_ms_info_list)) {
				ERR("Cannot find '%s' in connected_to_ms_info_list\n",
						accept_cm_msg->server_ms_name);
			} else {
				it->connected = true;
				it->server_msid = be64toh(accept_cm_msg->server_msid);
				DBG("Setting '%s' to 'connected\n",
						accept_cm_msg->server_ms_name);
			}
			sem_post(&connected_to_ms_info_list_sem);

		} else if (be64toh(accept_cm_msg->type) == CM_DESTROY_MS) {
			cm_destroy_msg	*destroy_msg;
			accept_destroy_client->get_recv_buffer((void **)&destroy_msg);

			HIGH("Received CM destroy  containing '%s'\n",
								destroy_msg->server_msname);

			/* Relay to library and get ACK back */
			if (send_destroy_ms_to_lib(destroy_msg->server_msname, be64toh(destroy_msg->server_msid))) {
				ERR("Failed to send destroy message to library or get ack\n");
				/* Don't exit; there maybe a problem with that memory space
				 * but not with others */
				continue;
			}

			/* Remove the entry relating to the destroy ms. Note that it has to match both
			 * the server_destid and server_msid since multiple daemons can allocate the same
			 * msid to different memory spaces and they are distinct only by the servers DID (destid)
			 */
			match_ms	mms(accept_destroy_client->server_destid,
					be64toh(destroy_msg->server_msid));
			sem_wait(&connected_to_ms_info_list_sem);
			remove_if(begin(connected_to_ms_info_list), end(connected_to_ms_info_list), mms);
			sem_post(&connected_to_ms_info_list_sem);

			cm_destroy_ack_msg *dam;

			/* Flush CM send buffer of previous message */
			accept_destroy_client->get_send_buffer((void **) &dam);
			accept_destroy_client->flush_send_buffer();

			/* Now send back a destroy_ack CM message */
			dam->type	= htobe64(CM_DESTROY_ACK_MS);
			strcpy(dam->server_msname, destroy_msg->server_msname);
			dam->server_msid = destroy_msg->server_msid; /* Both are BE */
			if (accept_destroy_client->send()) {
				WARN("Failed to send destroy_ack to server daemon\n");
			} else {
				HIGH("Sent destroy_ack to server daemon\n");
			}
		} else {
			CRIT("Got an unknown message code (0x%X)\n",
					accept_cm_msg->type);
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
		pthread_kill(it->tid, SIGUSR1);
	}
	sem_post(&hello_daemon_info_list_sem);

	try {
		/* Create provision client to connect to remote
		 * daemon's provisioning thread */
		auto hello_client = make_unique<cm_client>("hello_client",
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

		wadti = new wait_accept_destroy_thread_info(move(hello_client), destid);
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

		sem_wait(&wadti->started);
		DBG("wait_accept_destroy_thread started successully\n");

		/* Free the wadti struct */
		delete wadti;
	}
	catch(cm_exception& e) {
		CRIT("Failed to create hello_client %s\n", e.err);
		rc = -100;
	}
	catch(int e) {
		switch(e) {
		case FAILED_TO_CREATE_THREAD:
			delete wadti;
			/* no break */
		case FAILED_TO_ALLOCATE:
		case FAILED_TO_CONNECT:
			break;
		}
		rc = e;
	}
	return rc;
} /* provision_rdaemon() */

