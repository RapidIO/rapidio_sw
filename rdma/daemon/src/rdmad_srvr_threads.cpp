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

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "liblog.h"
#include "cm_sock.h"

#include "rdmad_cm.h"
#include "rdmad_inbound.h"
#include "rdmad_main.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_peer_utils.h"

/* List of destids provisioned via the provisioning thread */
daemon_list	prov_daemon_info_list;

struct wait_conn_disc_thread_info {
	wait_conn_disc_thread_info(cm_server *prov_server) :
		prov_server(prov_server), tid(0), ret_code(0)
	{
		sem_init(&started, 0, 0);
	}

	cm_server *prov_server;
	pthread_t	tid;
	sem_t		started;
	int		ret_code;
};

/**
 * @brief Send indication to remote daemon that the connect request to
 * 	  the specified memory space was declined, most likley since
 * 	  the memory space was no in accept mode in the first place
 *
 * @param conn_msg Pointer to cm_connect_msg sent by remote daemon
 *
 * @param rx_conn_disc_server CM server used to communicate with remote daemon
 *
 * @return 0 if successful, non-zero otherwise
 */
int send_accept_nack(cm_connect_msg *conn_msg, cm_server *rx_conn_disc_server)
{
	int rc;

	/* Prepare CM_ACCEPT_MS message from CONNECT_MS_RESP params */
	cm_accept_msg	*cmnam;
	rx_conn_disc_server->get_send_buffer((void **)&cmnam);
	rx_conn_disc_server->flush_send_buffer();

	cmnam->type = htobe64(CM_ACCEPT_MS);
	cmnam->sub_type = htobe64(CM_ACCEPT_MS_NACK);
	strcpy(cmnam->server_ms_name, conn_msg->server_msname);
	cmnam->client_to_lib_tx_eng_h = conn_msg->client_to_lib_tx_eng_h;

	/* Send the CM_ACCEPT_MS message to remote (client) daemon */
	rc = rx_conn_disc_server->send();
	if (rc) {
		ERR("Failed to send CM_ACCEPT_MS\n");
		throw rc;
	}
	return rc;
} /* send_accept_nack() */

/**
 * @brief Thread for handling requests from remote client daemons
 */
void *wait_conn_disc_thread_f(void *arg)
{
	DBG("ENTER\n");

	if (!arg) {
		CRIT("arg is NULL. Fatal error. Aborting\n");
		abort();
	}

	wait_conn_disc_thread_info	*wcdti =
			(wait_conn_disc_thread_info *)arg;

	if (!wcdti->prov_server) {
		CRIT("wcdti->prov_server is NULL. Fatal error.\n");
		abort();
	}

	cm_server *prov_server = wcdti->prov_server;

	/* Now receive HELLO message */
	int ret = prov_server->receive();
	if (ret) {
		if (ret == EINTR) {
			WARN("pthread_kill() called. Exiting!\n");
			/* Not an error if we intentionally kill the thread */
			wcdti->ret_code = 0;
		} else {
			CRIT("Failed to receive HELLO message: %s. EXITING\n",
							strerror(ret));
			wcdti->ret_code = -2;	/* To be handled by caller */
		}
		sem_post(&wcdti->started);	/* Unblock provisioning thread */
		pthread_exit(0);
	}

	hello_msg_t	*hello_msg;
	prov_server->get_recv_buffer((void **)&hello_msg);
	uint32_t remote_destid = be64toh(hello_msg->destid);
	DBG("Received HELLO message from destid(0x%X)\n", remote_destid);

	/* Send HELLO ACK with our own destid */
	prov_server->get_send_buffer((void **)&hello_msg);
	prov_server->flush_send_buffer();
	hello_msg->destid = htobe64(the_inbound->get_peer().destid);
	if (prov_server->send()) {
		CRIT("Failed to send HELLO_ACK message: %s. EXITING\n",
							strerror(ret));
		wcdti->ret_code = -3;		/* To be handled by caller */
		sem_post(&wcdti->started);	/* Unblock provisioning thread */
		pthread_exit(0);
	}
	DBG("Sent HELLO_ACK message back\n");

	/* Create CM server object based on the accept socket */
	cm_server *rx_conn_disc_server;
	try {
		rx_conn_disc_server = new cm_server("rx_conn_disc_server",
				prov_server->get_accept_socket(),
				&shutting_down);
	}
	catch(exception& e) {
		CRIT("Failed to create rx_conn_disc_server: %s\n", e.what());
		wcdti->ret_code = -4;		/* To be handled by caller */
		sem_post(&wcdti->started);	/* Unblock provisioning thread */
		pthread_exit(0);
	}
	DBG("Created rx_conn_disc_server=0x%X\n", rx_conn_disc_server);


	/* Store info about the remote daemon/destid in list */
	HIGH("Storing info for destid=0x%X\n", remote_destid);
	prov_daemon_info_list.add_daemon(remote_destid,
			   	   	 rx_conn_disc_server,
			   	   	 wcdti->tid);
	/* Notify prov_thread so it can loop back and accept more connections */
	wcdti->ret_code = 0;		/* No errors. HELLO exchange worked. */
	sem_post(&wcdti->started);	/* Unblock provisioning thread */

	while(1) {
		int	ret;

		/* Receive CONNECT_MS, or DISCONNECT_MS */
		DBG("Waiting for CONNECT_MS or DISCONNECT_MS on rx_conn_disc_server(0x%X)\n",
				rx_conn_disc_server);
		ret = rx_conn_disc_server->receive();
		if (ret) {
			delete rx_conn_disc_server;

			if (ret == EINTR) {
				WARN("pthread_kill() called\n");
			} else {
				CRIT("Failed to rx on rx_conn_disc_server: %s\n",
								strerror(ret));

				WARN("Removing entry from prov_daemon_info_list\n");
				if( prov_daemon_info_list.remove_daemon(remote_destid)) {
					ERR("Failed to remove entry for destid(0x%X)\n",
							remote_destid);
				}
			}
			CRIT("Exiting thread on error/shutdown\n");
			pthread_exit(0);
		}

		/* Read all messages as a connect message first, then if the
		 * type is different then cast message buffer accordingly. */
		cm_connect_msg	*conn_msg;
		rx_conn_disc_server->get_recv_buffer((void **)&conn_msg);
		if (be64toh(conn_msg->type) == CM_CONNECT_MS) {
			HIGH("Received CONNECT_MS for '%s'. Contents:\n",
						conn_msg->server_msname);
			conn_msg->dump();
			mspace *ms = the_inbound->get_mspace(
						conn_msg->server_msname);
			if (ms == nullptr) {
				WARN("'%s' not found. Ignore CM_CONNECT_MS\n",
						conn_msg->server_msname);
				if(send_accept_nack(conn_msg, rx_conn_disc_server)) {
					CRIT("Failed to send ACCEPT NACK message\n");
				}
				continue;
			}
			DBG("mspace '%s' found\n", conn_msg->server_msname);
			tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng;
			to_lib_tx_eng = ms->get_accepting_tx_eng();
			if (to_lib_tx_eng == nullptr) {
				WARN("'%s' not accepting by owner or users\n",
							conn_msg->server_msname);
				WARN("Ignoring CM_CONNECT_MS\n");
				if(send_accept_nack(conn_msg, rx_conn_disc_server)) {
					CRIT("Failed to send ACCEPT NACK message\n");
				}
				continue;
			}
			DBG("Found Tx engine in accepting mode\n");

			/* Send 'connect' POSIX message contents to the RDMA library */
			static unix_msg_t in_msg;
			connect_to_ms_req_input *cm = &in_msg.connect_to_ms_req;

			in_msg.type = CONNECT_MS_REQ;
			in_msg.category = RDMA_LIB_DAEMON_CALL;
			cm->client_msid = be64toh(conn_msg->client_msid);
			cm->client_msubid = be64toh(conn_msg->client_msubid);
			cm->client_msub_bytes = be64toh(conn_msg->client_bytes);
			cm->client_rio_addr_len = be64toh(conn_msg->client_rio_addr_len);
			cm->client_rio_addr_lo	= be64toh(conn_msg->client_rio_addr_lo);
			cm->client_rio_addr_hi	= be64toh(conn_msg->client_rio_addr_hi);
			cm->client_destid_len = be64toh(conn_msg->client_destid_len);
			cm->client_destid = be64toh(conn_msg->client_destid);
			cm->seq_num = be64toh(conn_msg->seq_num);
			cm->connh = be64toh(conn_msg->connh);
			cm->client_to_lib_tx_eng_h = be64toh(conn_msg->client_to_lib_tx_eng_h);

			to_lib_tx_eng->send_message(&in_msg);

			DBG("Sent CONNECT_MS_REQ to Server RDMA library. Contents:\n");
			cm->dump();
		} else if (be64toh(conn_msg->type) == CM_DISCONNECT_MS_REQ) {
			cm_disconnect_req_msg	*disc_msg;

			rx_conn_disc_server->get_recv_buffer((void **)&disc_msg);
			HIGH("Received DISCONNECT_MS for msid(0x%X)\n",
						be64toh(disc_msg->server_msid));

			/* Remove client_destid from 'ms' identified by server_msid */
			mspace *ms = the_inbound->get_mspace(
						be64toh(disc_msg->server_msid));
			if (!ms) {
				ERR("Failed to find ms(0x%X). Was it destroyed?\n",
						be64toh(disc_msg->server_msid));
				continue;	/* Can't do much without the ms */
			}

			/* Relay disconnection request to the RDMA library */
			ret = ms->client_disconnect(be64toh(disc_msg->client_msubid),
					     be64toh(disc_msg->client_to_lib_tx_eng_h));
			if (ret) {
				ERR("Failed to relay disconnect ms('%s') to lib\n",
					ms->get_name());
			} else {
				HIGH("'Disconnect' for ms('%s') relayed to 'server'\n",
					ms->get_name());
			}
		} else if (be64toh(conn_msg->type) == CM_FORCE_DISCONNECT_MS_ACK) {
			cm_force_disconnect_ack_msg *force_disconnect_ack_msg;

			rx_conn_disc_server->get_recv_buffer((void **)&force_disconnect_ack_msg);
			HIGH("Received CM_FORCE_DISCONNECT_MS_ACK for msid(0x%X), '%s'\n",
					be64toh(force_disconnect_ack_msg->server_msid),
					force_disconnect_ack_msg->server_msname);
		} else {
			CRIT("Unknown message type 0x%016" PRIx64 "\n",
							be64toh(conn_msg->type));
		}
	} /* while(1) */
	pthread_exit(0);	/* Not reached */
} /* conn_disc_thread_f() */

/**
 * @brief Provisioning thread. Accepts requests from other daemons,
 * 	  then spawns conn_disc_threads for receiving HELLO provisionin
 * 	  messages then handling other daemon-to-daemon communication
 */
void *prov_thread_f(void *arg)
{
	DBG("ENTER\n");
	if (!arg) {
		CRIT("NULL peer_info argument. Failing.\n");
		abort();
	}
	peer_info *peer = (peer_info *)arg;

	cm_server *prov_server;
	try {
		prov_server = new cm_server("prov_server",
					peer->mport_id,
					peer->prov_mbox_id,
					peer->prov_channel,
					&shutting_down);
	}
	catch(exception& e) {
		CRIT("Failed to create prov_server: %s. EXITING\n", e.what());
		abort();
		pthread_exit(0);	/* For g++ warning */
	}
	DBG("prov_server created.\n");
	sem_post(&peer->prov_started);

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
				/* Not much we can do here if we can't accept
				 * connections from remote daemons. This is
				 * a fatal error so we fail in a big way! */
				abort();
			}
		}

		DBG("Creating connect/disconnect thread\n");
		wait_conn_disc_thread_info *wcdti;
		try {
			wcdti = new wait_conn_disc_thread_info(prov_server);
		}
		catch(...) {
			CRIT("Fatal: Failed to allocate wcdti. Aborting\n");
			delete prov_server;
			abort();
		}

		ret = pthread_create(&wcdti->tid, NULL, wait_conn_disc_thread_f,
									wcdti);
		if (ret) {
			CRIT("Failed to create conn_disc thread\n");
			delete wcdti;
			delete prov_server;
			/* If pthread_create() fails the system has serious
			 * problems and we abort instead of trying to run
			 * without such an important thread! */
			abort();
		}

		/* The thread was successfully created but maybe it failed for
		 *  one reason or another. Check the return code. Perhaps
		 *  a remote daemon sent a corrupted HELLO message? */
		sem_wait(&wcdti->started);
		if (wcdti->ret_code < 0) {
			CRIT("Failure in wait_conn_disc_thread_f(), code = %d\n",
								wcdti->ret_code);
		}
		delete wcdti;	/* was just for passing the arguments */

		/* Loop again and try to provision another remote daemon */
	} /* while(1) */
	pthread_exit(0);
} /* prov_thread() */
