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

#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#include <vector>
#include <algorithm>

#include "liblog.h"
#include "rdmad_cm.h"
#include "rdma_mq_msg.h"
#include "msg_q.h"
#include "cm_sock.h"
#include "ts_vector.h"

#include "rdmad_main.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_peer_utils.h"

using std::vector;

/* List of destids provisioned via the provisioning thread */
vector<prov_daemon_info>	prov_daemon_info_list;
sem_t prov_daemon_info_list_sem;


struct wait_conn_disc_thread_info {
	cm_server *prov_server;
	pthread_t	tid;
	sem_t		started;
	int		ret_code;
};

/**
 * Handles incoming 'connect', 'disconnect', and 'destroy'
 * Sends back 'accept', and 'destroy_ack'
 */
void *wait_conn_disc_thread_f(void *arg)
{
	DBG("ENTER\n");

	if (!arg) {
		CRIT("arg is NULL. Fatal error. Aborting\n");
		raise(SIGABRT);
	}

	wait_conn_disc_thread_info	*wcdti =
			(wait_conn_disc_thread_info *)arg;

	if (!wcdti->prov_server) {
		CRIT("wcdti->prov_server is NULL. Fatal error.\n");
		raise(SIGABRT);
	}

	cm_server *prov_server = wcdti->prov_server;

	/* Now receive HELLO message */
	int ret = prov_server->receive();
	if (ret) {
		if (ret == EINTR) {
			WARN("pthread_kill() called. Exiting!\n");
			/* It is not an error if we intentionally kill the thread */
			wcdti->ret_code = 0;
		} else {
			CRIT("Failed to receive HELLO message: %s. EXITING\n",
							strerror(ret));
			wcdti->ret_code = -2;		/* Error. To be handled by caller */
		}
		sem_post(&wcdti->started);	/* Allow main provisioning thread to run */
		pthread_exit(0);
	}

	hello_msg_t	*hello_msg;
	prov_server->get_recv_buffer((void **)&hello_msg);
	uint32_t remote_destid = be64toh(hello_msg->destid);
	DBG("Received HELLO message from destid(0x%X)\n", remote_destid);

	/* Send HELLO ACK with our own destid */
	prov_server->get_send_buffer((void **)&hello_msg);
	prov_server->flush_send_buffer();
	hello_msg->destid = htobe64(peer.destid);
	if (prov_server->send()) {
		CRIT("Failed to send HELLO_ACK message: %s. EXITING\n",
							strerror(ret));
		wcdti->ret_code = -3;		/* Error. To be handled by caller */
		sem_post(&wcdti->started);	/* Allow main provisioning thread to run */
		pthread_exit(0);
	}
	DBG("Sent HELLO_ACK message back\n");

	/* If destid already in our list, kill its thread; we are replacing it */
	sem_wait(&prov_daemon_info_list_sem);
	auto it = find(begin(prov_daemon_info_list),
		       end(prov_daemon_info_list), remote_destid);
	if (it != end(prov_daemon_info_list)) {
		WARN("Killing thread for known destid(0x%X).\n",
						remote_destid);
		pthread_kill(it->tid, SIGUSR1);
	}
	sem_post(&prov_daemon_info_list_sem);

	/* Create CM server object based on the accept socket */
	cm_server *rx_conn_disc_server;
	try {
		rx_conn_disc_server = new cm_server("rx_conn_disc_server",
				prov_server->get_accept_socket(),
				&shutting_down);
	}
	catch(cm_exception& e) {
		CRIT("Failed to create rx_conn_disc_server: %s\n", e.err);
		wcdti->ret_code = -4;		/* Error. To be handled by caller */
		sem_post(&wcdti->started);	/* Allow main provisioning thread to run */
		pthread_exit(0);
	}
	DBG("Created rx_conn_disc_server cm_sock\n");

	/* Create new entry for this destid */
	prov_daemon_info	*pdi;
	pdi = (prov_daemon_info *)malloc(sizeof(prov_daemon_info));
	if (!pdi) {
		CRIT("Failed to allocate prov_daemon_info: %s. Aborting\n",
				strerror(errno));
		raise(SIGABRT);
	}
	pdi->destid = remote_destid;
	pdi->tid = wcdti->tid;
	pdi->conn_disc_server = rx_conn_disc_server;

	/* Store info about the remote daemon/destid in list */
	HIGH("Storing info for destid=0x%X\n", pdi->destid);
	sem_wait(&prov_daemon_info_list_sem);
	prov_daemon_info_list.push_back(*pdi);
	sem_post(&prov_daemon_info_list_sem);
	DBG("prov_daemon_info_list now has %u destids\n", prov_daemon_info_list.size());

	free(pdi);

	/* Tell prov_thread that we started so it can start accepting
	 * from other sockets.
	 */
	wcdti->ret_code = 0;		/* No errors. HELLO exchanged worked fine. */
	sem_post(&wcdti->started);	/* Allow main provisioning thread to run */

	while(1) {
		int	ret;

		/* Receive CONNECT_MS, or DISCONNECT_MS */
		DBG("Waiting for CONNECT_MS or DISCONNECT_MS\n");
		ret = rx_conn_disc_server->receive();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called. Exiting thread.\n");
			} else {
				CRIT("Failed to receive on rx_conn_disc_server: %s\n",
								strerror(ret));
			}

			/* Delete the cm_server object */
			delete rx_conn_disc_server;

			/* If we just failed to receive() then we should also
			 * clear the entry in prov_daemon_info_list. If we are
			 * shutting down, the shutdown function would be accessing
			 * the list so we should NOT erase an element from it here.
			 */
			if (!shutting_down) {
				/* Remove the corresponding entry from the
				 * prov_daemon_info_list */
				WARN("Removing entry from prov_daemon_info_list\n");
				sem_wait(&prov_daemon_info_list_sem);
				auto it = find(begin(prov_daemon_info_list),
					       end(prov_daemon_info_list),
					       remote_destid);
				if (it != end(prov_daemon_info_list))
					prov_daemon_info_list.erase(it);
				sem_post(&prov_daemon_info_list_sem);
				CRIT("Exiting thread on error\n");
			}
			pthread_exit(0);
		}

		/* Read all messages as a connect message first, then if the
		 * type is different then cast message buffer accordingly. */
		cm_connect_msg	*conn_msg;
		rx_conn_disc_server->get_recv_buffer((void **)&conn_msg);
		if (be64toh(conn_msg->type) == CM_CONNECT_MS) {
			HIGH("Received CONNECT_MS '%s'\n", conn_msg->server_msname);
			rx_conn_disc_server->dump_recv_buffer();
			DBG("conn_msg->client_msid = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_msid));
			DBG("conn_msg->client_msubsid = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_msubid));
			DBG("conn_msg->client_bytes = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_bytes));
			DBG("conn_msg->client_rio_addr_len = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_rio_addr_len));
			DBG("conn_msg->client_rio_addr_lo = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_rio_addr_lo));
			DBG("conn_msg->client_rio_addr_hi = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_rio_addr_hi));
			DBG("conn_msg->client_destid_len = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_destid_len));
			DBG("conn_msg->client_destid = 0x%016" PRIx64 "\n", be64toh(conn_msg->client_destid));
			DBG("conn_msg->seq_num = 0x%016" PRIx64 "\n", be64toh(conn_msg->seq_num));

			/* Form message queue name from memory space name */
			/* TODO: Use a stream? */
			char mq_name[CM_MS_NAME_MAX_LEN+2];
			memset(mq_name, '\0', CM_MS_NAME_MAX_LEN+2);	/* For Valgrind */
			mq_name[0] = '/';
			strcpy(&mq_name[1], conn_msg->server_msname);
			string mq_str(mq_name);
			DBG("mq_name = %s\n", mq_name);

			/* If queue name not in map ignore message */
			if (!accept_msg_map.contains(mq_str)) {
				WARN("cm_connect_msg to %s ignored!\n",
						conn_msg->server_msname);
				continue;
			}

			/* Open message queue */
			msg_q<mq_rdma_msg>	*connect_mq;
			try {
				connect_mq = new msg_q<mq_rdma_msg>(mq_name, MQ_OPEN);

			}
			catch(msg_q_exception& e) {
				ERR("Failed to open connect_mq: %s\n", e.msg.c_str());
				/* Don't remove MS from accept_msg_map; the
				 * client may retry connecting. However, don't also
				 * send an ACCEPT_MS since the server didn't get
				 * the message. */
				/* FIXME: For now I think we would like to know if we try to
				 * open a non-existing queue right away. This way we catch the
				 * bug before we have loads of debugging messages!
				 */
#ifdef BE_STRICT
				raise(SIGABRT);	/* TODO: Remove eventually? */
#endif
				continue;
			}
			DBG("Opened POSIX message queue: '%s'\n", mq_name);

			/* Send 'connect' POSIX message contents to the RDMA library */
			mq_rdma_msg	*rdma_mq_msg;
			connect_mq->get_send_buffer(&rdma_mq_msg);
			mq_connect_msg	*connect_msg = &rdma_mq_msg->connect_msg;
			rdma_mq_msg->type = MQ_CONNECT_MS;
			connect_msg->rem_msid		= be64toh(conn_msg->client_msid);
			connect_msg->rem_msubid		= be64toh(conn_msg->client_msubid);
			connect_msg->rem_bytes		= be64toh(conn_msg->client_bytes);
			connect_msg->rem_rio_addr_len	= be64toh(conn_msg->client_rio_addr_len);
			connect_msg->rem_rio_addr_lo	= be64toh(conn_msg->client_rio_addr_lo);
			connect_msg->rem_rio_addr_hi	= be64toh(conn_msg->client_rio_addr_hi);
			connect_msg->rem_destid_len	= be64toh(conn_msg->client_destid_len);
			connect_msg->rem_destid		= be64toh(conn_msg->client_destid);
			connect_msg->seq_num		= be64toh(conn_msg->seq_num);

			/* Send connect message to RDMA library/app */
			if (connect_mq->send()) {
				ERR("connect_mq->send() failed: %s\n", strerror(errno));
				delete connect_mq;
				/* Don't remove MS from accept_msg_map; the
				 * client may retry connecting. However, don't also
				 * send an ACCEPT_MS since the server didn't get
				 * the message. */
				/* FIXME: For now we need to know if we fail to notify an app.
				 * Therefore we will raise a SIGABRT if the send fails.
				 */
#ifdef BE_STRICT
				raise(SIGABRT);	/* TODO: Remove eventually? */
#endif
				continue;
			}
			DBG("connect_msg->rem_msid = 0x%X\n", connect_msg->rem_msid);
			DBG("connect_msg->rem_msubsid = 0x%X\n", connect_msg->rem_msubid);
			DBG("connect_msg->rem_bytes = 0x%X\n", connect_msg->rem_bytes);
			DBG("connect_msg->rem_rio_addr_len = 0x%X\n", connect_msg->rem_rio_addr_len);
			DBG("connect_msg->rem_rio_addr_lo = 0x%016" PRIx64 "\n", connect_msg->rem_rio_addr_lo);
			DBG("connect_msg->rem_rio_addr_hi = 0x%X\n", connect_msg->rem_rio_addr_hi);
			DBG("connect_msg->rem_destid_len = 0x%X\n", connect_msg->rem_destid_len);
			DBG("connect_msg->rem_destid = 0x%X\n", connect_msg->rem_destid);
			DBG("connect_msg->seq_num = 0x%X\n", connect_msg->seq_num);

			DBG("Relayed CONNECT_MS to RDMA library to unblock rdma_accept_ms_h()\n");
			connect_mq->dump_send_buffer();


			/* Request a send buffer */
			void *cm_send_buf;
			rx_conn_disc_server->get_send_buffer(&cm_send_buf);
			rx_conn_disc_server->flush_send_buffer();	/* For Valgrind */

			/* Copy the corresponding accept message from map into the send buffer */
			cm_accept_msg	accept_msg = accept_msg_map.get_item(mq_str);
			memcpy( cm_send_buf,
				(void *)&accept_msg,
				sizeof(cm_accept_msg));

			DBG("cm_accept_msg has server_msid = 0x%X\n", be64toh(accept_msg.server_msid));
			DBG("cm_accept_msg has server_msubid = 0x%X\n", be64toh(accept_msg.server_msubid));
			DBG("cm_accept_msg has server_destid = 0x%X\n", be64toh(accept_msg.server_destid));
			DBG("cm_accept_msg has server_destid_len = 0x%X\n", be64toh(accept_msg.server_destid_len));
			DBG("cm_accept_msg has rio_addr_len = %d\n", be64toh(accept_msg.server_rio_addr_len));
			DBG("cm_accept_msg has rio_addr_lo = 0x%016" PRIx64 "\n", be64toh(accept_msg.server_rio_addr_lo));
			DBG("cm_accept_msg has rio_addr_hi = 0x%X\n", be64toh(accept_msg.server_rio_addr_hi));

			/* Send 'accept' message to remote daemon */
			DBG("Sending back ACCEPT_MS for '%s'\n", mq_str.c_str());
			if (rx_conn_disc_server->send()) {
				/* The server was already notified of the CONNECT_MS
				 * via the POSIX message. Now that we are failing
				 * to notify the client's daemon then we should remove
				 * the memory space from the map because there is
				 * no rdma_accept_ms_h() to receive a another CONNECT
				 * notification.
				 */
				/* FIXME: TODO: We should go back and notify the server app
				 * that the client could not be notified. The server may have
				 * to be modified in rdma_accept_ms_h() to block on 2 messages.
				 * The first being the 'connect' and the second being a confirmation
				 * that the client daemon (at least) was sent the accept. Otherwise
				 * we end up with the server thinking it has a connection from the
				 * client when that is not the case.
				 * If the accept cannot be sent because the remote daemon has died,
				 * then potentially our fault tolerance code takes care of cleaning
				 * up the client connection information? Verify.
				 */
				ERR("Failed to send back ACCEPT_MS to remote daemon\n");
				accept_msg_map.remove(mq_str);
				delete connect_mq;
				/* FIXME: Given the above complications, I'm causing an abort here
				 * if this fails, just for now so we can catch it if it happens.
				 * To be removed if we are doing fault-tolerance.
				 */
#ifdef BE_STRICT
				raise(SIGABRT);
#endif
				continue;
			}
			HIGH("ACCEPT_MS sent back to remote daemon!\n");

			/* Now the destination ID must be added to the memory space.
			 * This is for the case where the memory space is destroyed
			 * and the remote users of that space need to be notified. */
			mspace	*ms = the_inbound->get_mspace(conn_msg->server_msname);
			if (ms) {
				ms->add_rem_connection(be64toh(conn_msg->client_destid),
						       be64toh(conn_msg->client_msubid));
				DBG("Added destid(0x%X),msubid(0x%X)  as one that is connected to '%s'\n",
						be64toh(conn_msg->client_destid),
						be64toh(conn_msg->client_msubid),
						conn_msg->server_msname);
			} else {
				WARN("memory space '%s' NOT found!\n",
						conn_msg->server_msname);
			}

			/* Erase cm_accept_msg from map */
			accept_msg_map.remove(mq_str);
			DBG("%s now removed from the accept message map\n",
							mq_str.c_str());
			/* At this point the CM message is now copied into a
			 * POSIX message, so flush the CM receive buffer.
			 */
			rx_conn_disc_server->flush_recv_buffer();

			delete connect_mq;
		} else if (be64toh(conn_msg->type) == CM_DISCONNECT_MS) {
			cm_disconnect_msg	*disc_msg;

			rx_conn_disc_server->get_recv_buffer((void **)&disc_msg);
			HIGH("Received DISCONNECT_MS for msid(0x%X)\n",
						be64toh(disc_msg->server_msid));

			/* Remove client_destid from 'ms' identified by server_msid */
			mspace *ms =
				the_inbound->get_mspace(be64toh(disc_msg->server_msid));
			if (!ms) {
				ERR("Failed to find ms(0x%X). Was it destroyed?\n",
						be64toh(disc_msg->server_msid));
				continue;	/* Not much else to do without the ms */
			}

			/* Remove the connection to client destid and msubid */
			/* TODO: Write a remove_rem_connection() method that
			 * takes an 'msid' instead of the two-step process we
			 * have here.
			 */
			ret = ms->remove_rem_connection(be64toh(disc_msg->client_destid),
						  be64toh(disc_msg->client_msubid));

			/* Relay disconnection request to the RDMA library */
			ret = ms->disconnect(be64toh(disc_msg->client_msubid));
			if (ret) {
				ERR("Failed to relay disconnect ms('%s') to RDMA library\n",
						ms->get_name());
#ifdef BE_STRICT
				raise(SIGABRT);
#endif
			} else {
				HIGH("'Disconnect' for ms('%s') relayed to 'server'\n",
						ms->get_name());
			}

			/* Consider this memory space disconnected. Allow accepting */
			ms->set_accepted(false);
		} else if (be64toh(conn_msg->type) == CM_DESTROY_ACK_MS) {
			cm_destroy_ack_msg *dest_ack_msg;

			rx_conn_disc_server->get_recv_buffer((void **)&dest_ack_msg);
			HIGH("Received CM_DESTROY_ACK_MS for msid(0x%X), '%s'\n",
			    be64toh(dest_ack_msg->server_msid), dest_ack_msg->server_msname);
		} else {
			CRIT("Message of unknown type 0x%016" PRIx64 "\n",
						be64toh(conn_msg->type));
#ifdef BE_STRICT
			raise(SIGABRT);
#endif
		}

	} /* while(1) */
	pthread_exit(0);	/* Not reached */
} /* conn_disc_thread_f() */

/**
 * Provisioning thread.
 * For waiting for HELLO messages from other daemons and updating the
 * provisioned daemon info list.
 */
void *prov_thread_f(void *arg)
{
	DBG("ENTER\n");
	if (!arg) {
		CRIT("NULL peer_info argument. Failing.\n");
		raise(SIGABRT);
	}
	struct peer_info *peer = (peer_info *)arg;

	cm_server *prov_server;
	try {
		prov_server = new cm_server("prov_server",
					peer->mport_id,
					peer->prov_mbox_id,
					peer->prov_channel,
					&shutting_down);
	}
	catch(cm_exception& e) {
		CRIT("Failed to create prov_server: %s. EXITING\n", e.err);
		raise(SIGABRT);
		pthread_exit(0);	/* For g++ warning */
	}
	DBG("prov_server created.\n");

	while(1) {
		/* Accept connections from other daemons */
		DBG("Accepting connections from other daemons...\n");
		int ret = prov_server->accept();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called. Exiting!\n");
				delete prov_server;
				pthread_exit(0);
			} else {
				CRIT("Failed to accept on prov_server: %s\n",
								strerror(ret));
				/* Not much we can do here if we can't accept connections
				 * from remote daemons. This is a fatal error so we should
				 * just fail in a big way.
				 */
				raise(SIGABRT);
			}
		}

		DBG("Creating connect/disconnect thread\n");
		wait_conn_disc_thread_info	*wcdti =
				(wait_conn_disc_thread_info *)malloc(sizeof(wait_conn_disc_thread_info));
		if (!wcdti) {
			CRIT("Failed to allocate wcdti. Serious failure. Aborting\n");
			delete prov_server;
			raise(SIGABRT);
		}
		wcdti->prov_server = prov_server;
		sem_init(&wcdti->started, 0, 0);
		ret = pthread_create(&wcdti->tid, NULL, wait_conn_disc_thread_f, wcdti);
		if (ret) {
			CRIT("Failed to create conn_disc thread\n");
			free(wcdti);
			delete prov_server;
			/* If pthread_create() fails the system has serious problems and we
			 * should simply just abort instead of trying to run without such
			 * an important thread!
			 */
			raise(SIGABRT);
		}

		/* The thread was successfully created but maybe it failed for one reason or another.
		 * Check the return code. Perhaps a remote daemon sent a corrupted HELLO message?
		 */
		sem_wait(&wcdti->started);
		if (wcdti->ret_code < 0) {
			CRIT("Failure in wait_conn_disc_thread_f(), code = %d\n", wcdti->ret_code);
#ifdef BE_STRICT
			raise(SIGABRT);
#endif
		}
		free(wcdti);	/* was just for passing the arguments */

		/* Loop again and try to provision another remote daemon */
	} /* while(1) */
	pthread_exit(0);
} /* prov_thread() */
