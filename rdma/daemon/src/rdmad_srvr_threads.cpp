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
};

/**
 * Handles incoming 'connect', 'disconnect', and 'destroy'
 * Sends back 'accept', and 'destroy_ack'
 */
void *wait_conn_disc_thread_f(void *arg)
{
	DBG("ENTER\n");

	if (!arg) {
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
	}

	wait_conn_disc_thread_info	*wcdti =
			(wait_conn_disc_thread_info *)arg;

	if (!wcdti->prov_server) {
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
	}

	cm_server *prov_server = wcdti->prov_server;

	/* Now receive HELLO message */
	int ret = prov_server->receive();
	if (ret) {
		if (ret == EINTR) {
			WARN("pthread_kill() called. Exiting!\n");
		} else {
			CRIT("Failed to receive HELLO message: %s. EXITING\n",
							strerror(ret));
		}
		free(wcdti);
		pthread_exit(0);
	}

	hello_msg_t	*hello_msg;
	prov_server->get_recv_buffer((void **)&hello_msg);
	DBG("Received HELLO message from destid(0x%X)\n", hello_msg->destid);

	uint32_t remote_destid = hello_msg->destid;

	/* Send HELLO ACK withour own destid */
	prov_server->get_send_buffer((void **)&hello_msg);
	prov_server->flush_send_buffer();
	hello_msg->destid = peer.destid;
	if (prov_server->send()) {
		CRIT("Failed to send HELLO_ACK message: %s. EXITING\n",
							strerror(ret));
		free(wcdti);
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
	catch(cm_exception e) {
		CRIT("Failed to create rx_conn_disc_server: %s\n", e.err);
		free(wcdti);
		pthread_exit(0);
	}
	DBG("Created rx_conn_disc_server cm_sock\n", rx_conn_disc_server);

	/* Create new entry for this destid */
	prov_daemon_info	*pdi;
	pdi = (prov_daemon_info *)malloc(sizeof(prov_daemon_info));
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
	 * from other sockets without waiting for this one to get a HELLO.
	 */
	sem_post(&wcdti->started);


	while(1) {
		int	ret;
		/* Receive CONNECT_MS, or DISCONNECT_MS */
		DBG("Waiting for CONNECT_MS or DISCONNECT_MS\n");
		ret = rx_conn_disc_server->receive();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called\n");
			} else {
				CRIT("Failed to receive on rx_conn_disc_server: %s\n",
								strerror(ret));
			}

			/* Delete the cm_server object */
			delete rx_conn_disc_server;

			/* If we just failed to receive() then we should also
			 * clear the entry in prov_daemon_info_list. If we are
			 * shutting down, the shutdown function would be accessing
			 * the list so we should NOT erase an element from it.
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
			}
			CRIT("Exiting thread\n");
			pthread_exit(0);
		}

		/* Read all messages as a connect message first, then if the
		 * type is different then cast message buffer accordingly. */
		cm_connect_msg	*conn_msg;
		rx_conn_disc_server->get_recv_buffer((void **)&conn_msg);
		if (conn_msg->type == CM_CONNECT_MS) {
			HIGH("Received CONNECT_MS '%s'\n", conn_msg->server_msname);

			/* Form message queue name from memory space name */
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
			msg_q<mq_connect_msg>	*connect_mq;
			try {
				connect_mq = new msg_q<mq_connect_msg>(mq_name, MQ_OPEN);

			}
			catch(msg_q_exception e) {
				ERR("Failed to open connect_mq: %s\n", e.msg.c_str());
				/* Don't remove MS from accept_msg_map; the
				 * client may retry connecting. However, don't also
				 * send an ACCEPT_MS since the server didn't get
				 * the message. */
				continue;
			}
			DBG("Opened POSIX message queue: '%s'\n", mq_name);

			/* Send 'connect' POSIX message contents to the RDMA library */
			mq_connect_msg	*connect_msg;
			connect_mq->get_send_buffer(&connect_msg);
			connect_msg->rem_msid		= conn_msg->client_msid;
			connect_msg->rem_msubid		= conn_msg->client_msubid;
			connect_msg->rem_bytes		= conn_msg->client_bytes;
			connect_msg->rem_rio_addr_len	= conn_msg->client_rio_addr_len;
			connect_msg->rem_rio_addr_lo	= conn_msg->client_rio_addr_lo;
			connect_msg->rem_rio_addr_hi	= conn_msg->client_rio_addr_hi;
			connect_msg->rem_destid_len	= conn_msg->client_destid_len;
			connect_msg->rem_destid		= conn_msg->client_destid;

			/* Send connect message to RDMA library/app */
			if (connect_mq->send()) {
				ERR("connect_mq->send() failed: %s\n", strerror(errno));
				delete connect_mq;
				/* Don't remove MS from accept_msg_map; the
				 * client may retry connecting. However, don't also
				 * send an ACCEPT_MS since the server didn't get
				 * the message. */
				continue;
			}
			DBG("Relayed CONNECT_MS to RDMA library to unblock rdma_accept_ms_h()n");
			delete connect_mq;

			/* Request a send buffer */
			void *cm_send_buf;
			rx_conn_disc_server->get_send_buffer(&cm_send_buf);
			rx_conn_disc_server->flush_send_buffer();	/* For Valgrind */

			/* Copy the corresponding accept message from map into the send buffer */
			cm_accept_msg	accept_msg = accept_msg_map.get_item(mq_str);
			memcpy( cm_send_buf,
				(void *)&accept_msg,
				sizeof(cm_accept_msg));
			DBG("cm_accept_msg has server_destid = 0x%X\n", accept_msg.server_destid);
			DBG("cm_accept_msg has server_destid_len = 0x%X\n", accept_msg.server_destid_len);

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
				ERR("Failed to send back ACCEPT_MS\n");
				accept_msg_map.remove(mq_str);
				continue;
			}
			HIGH("ACCEPT_MS sent back to remote daemon!\n");

			/* Now the destination ID must be added to the memory space.
			 * This is for the case where the memory space is destroyed
			 * and the remote users of that space need to be notified. */
			mspace	*ms = the_inbound->get_mspace(conn_msg->server_msname);
			if (ms) {
				ms->add_destid(conn_msg->client_destid);
				DBG("Added destid(0x%X) as one that is connected to '%s'\n",
						conn_msg->client_destid, conn_msg->server_msname);
			} else {
				WARN("memory space '%s' NOT found!\n",
						conn_msg->server_msname);
			}

			/* Erase cm_accept_msg from map */
			accept_msg_map.remove(mq_str);
			DBG("%s now removed from the accept message map\n",
							mq_str.c_str());
		} else if (conn_msg->type == CM_DISCONNECT_MS) {
			cm_disconnect_msg	*disc_msg;

			rx_conn_disc_server->get_recv_buffer((void **)&disc_msg);
			HIGH("Received DISCONNECT_MS for msid(0x%X)\n",
							disc_msg->server_msid);

			/* Remove client_destid from 'ms' identified by server_msid */
			mspace *ms = the_inbound->get_mspace(disc_msg->server_msid);
			if (!ms) {
				CRIT("Failed to find ms(0x%X)\n", disc_msg->server_msid);
				continue;	/* Not much else to do without the ms */
			} else {
				if (ms->remove_destid(disc_msg->client_destid) < 0)
					ERR("Failed to remove destid(0x%X)\n",
							disc_msg->client_destid);
					/* Although this should not happen, we can still
					 * proceed with the disconnection at least. This
					 * may in the future result in spurious destroy
					 * messages, but those can be discarded.
					 */
				else {
					INFO("Removed desitd(0x%X) from msid(0x%X)\n",
					disc_msg->client_destid, disc_msg->server_msid);
				}
			}

			/* Consider this memory space disconnected. Allow accepting */
			ms->set_accepted(false);

			/* Prepare POSIX disconnect message from CM disconnect message */
			mq_disconnect_msg	disconnect_msg;
			disconnect_msg.client_msubid = disc_msg->client_msubid;

			/* Form message queue name from memory space name */
			char mq_name[CM_MS_NAME_MAX_LEN+2];
			mq_name[0] = '/';
			strcpy(&mq_name[1], ms->get_name());

			/* Open POSIX message queue */
			mqd_t	disc_mq = mq_open(mq_name,O_RDWR, 0644, &attr);
			if (disc_mq == (mqd_t)-1) {
				ERR("Failed to open %s\n", mq_name);
				continue;
			}

			/* Send 'disconnect' POSIX message contents to the RDMA library */
			ret = mq_send(disc_mq,
				      (const char *)&disconnect_msg,
				      sizeof(struct mq_disconnect_msg),
				      1);
			if (ret < 0) {
				ERR("Failed to send message: %s\n", strerror(errno));
				mq_close(disc_mq);
				continue;
			}

			/* Now close POSIX queue */
			if (mq_close(disc_mq)) {
				ERR("Failed to close '%s': %s\n", mq_name, strerror(errno));
				continue;
			}
			HIGH("'Disconnect' message relayed to 'server'\n");
		}

	} /* while(1) */
	pthread_exit(0);
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
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
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
	catch(cm_exception e) {
		CRIT("Failed to create prov_server: %s. EXITING\n", e.err);
		pthread_exit(0);
	}
	DBG("prov_server created.\n");

	while(1) {
		/* Accept connections from other daemons */
		DBG("Accepting connections from other daemons...\n");
		int ret = prov_server->accept();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called. Exiting!\n");
			} else {
				CRIT("Failed to accept on prov_server: %s\n",
								strerror(ret));
			}
			delete prov_server;
			pthread_exit(0);
		}

		DBG("Creating connect/disconnect thread\n");
		wait_conn_disc_thread_info	*wcdti =
				(wait_conn_disc_thread_info *)malloc(sizeof(wait_conn_disc_thread_info));
		if (!wcdti) {
			CRIT("Failed to allocate wcdti\n");
			continue;
		}
		wcdti->prov_server = prov_server;
		sem_init(&wcdti->started, 0, 0);
		ret = pthread_create(&wcdti->tid, NULL, wait_conn_disc_thread_f, wcdti);
		if (ret) {
			CRIT("Failed to create conn_disc thread\n");
			delete prov_server;
			free(wcdti);
			continue;	/* Better luck next time? */
		}
		sem_wait(&wcdti->started);
		free(wcdti);	/* was just for passing the arguments */
	} /* while(1) */
	pthread_exit(0);
} /* prov_thread() */
