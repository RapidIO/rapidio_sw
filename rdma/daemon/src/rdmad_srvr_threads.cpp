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

#include <vector>
#include <algorithm>

#include "liblog.h"
#include "rdmad_cm.h"
#include "rdma_mq_msg.h"
#include "cm_sock.h"
#include "ts_vector.h"

#include "rdmad_main.h"
#include "rdmad_svc.h"
#include "rdmad_peer_utils.h"

struct prov_daemon_info {
	uint32_t 	destid;
	cm_server	*conn_disc_server;
	pthread_t	tid;
	bool operator==(uint32_t destid) { return this->destid == destid; }
};

/* List of destids provisioned via the provisioning thread */
vector<prov_daemon_info>	prov_daemon_info_list;


using namespace std;

struct conn_disc_thread_info {
	cm_server *prov_server;
	pthread_t	tid;
};

/**
 * New thread for handing RDMA connections to memory spaces and for
 * handling RDMA disconnections from memory spaces.
 */
void *conn_disc_thread_f(void *arg)
{
	DBG("ENTER\n");

	if (!arg) {
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
	}

	conn_disc_thread_info	*cdti = (conn_disc_thread_info *)arg;

	if (!cdti->prov_server) {
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
	}

	cm_server *prov_server = cdti->prov_server;

	/* Now receive HELLO message */
	int ret = prov_server->receive();
	if (ret) {
		if (ret == EINTR) {
			WARN("pthread_kill() called. Exiting!\n");
		} else {
			CRIT("Failed to receive HELLO message: %s. EXITING\n",
							strerror(ret));
		}
		pthread_exit(0);
	}

	hello_msg_t	*hello_msg;
	prov_server->get_recv_buffer((void **)&hello_msg);
	DBG("Received HELLO message from destid(0x%X)\n", hello_msg->destid);

	/* If destid already in our list, just exit thread */
	auto it = find(begin(prov_daemon_info_list),
		       end(prov_daemon_info_list), hello_msg->destid);
	if (it != end(prov_daemon_info_list)) {
		WARN("Received HELLO msg for known destid(0x%X. EXITING\n",
						hello_msg->destid);
		pthread_exit(0);
	}

	/* Create CM server object based on the accept socket */
	cm_server *conn_disc_server;
	try {
		conn_disc_server = new cm_server("conn_disc_server",
				prov_server->get_accept_socket());
	}
	catch(cm_exception e) {
		CRIT("Failed to create conn_disc_server: %s\n", e.err);
		pthread_exit(0);
	}

	/* Create new entry for this destid */
	prov_daemon_info	*pdi;
	pdi = (prov_daemon_info *)malloc(sizeof(prov_daemon_info));
	pdi->destid = hello_msg->destid;
	pdi->tid = cdti->tid;
	pdi->conn_disc_server = conn_disc_server;

	/* Store info about the remote daemon/destid in list */
	DBG("Storing info for destid=0x%X\n", pdi->destid);
	prov_daemon_info_list.push_back(*pdi);

	free(cdti);	/* was just for passing the arguments */
	free(pdi);	/* We have a copy in prov_daemon_info_list */

	while(1) {
		int	ret;
		/* Receive CONNECT_MS or DISCONNECT_MS message */
		ret = conn_disc_server->receive();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called. Exiting!\n");
			} else {
				CRIT("Failed to receive conn_disc_server: %s\n",
								strerror(ret));
			}
			delete conn_disc_server;
			pthread_exit(0);
		}
		cm_connect_msg	*cm;
		cm_disconnect_msg *dm;
		conn_disc_server->get_recv_buffer((void **)&cm);
		/* Determine message type */
		if (cm->type == CONNECT_MS) {
			INFO("Received CONNECT_MS '%s'\n", cm->server_msname);

			/* Form message queue name from memory space name */
			char mq_name[CM_MS_NAME_MAX_LEN+2];
			memset(mq_name, '\0', CM_MS_NAME_MAX_LEN+2);
			mq_name[0] = '/';
			strcpy(&mq_name[1], cm->server_msname);
			string mq_str(mq_name);
			DBG("mq_name = %s\n", mq_name);

			/* Compare message queue name with list. If no match ignore! */
			if (!accept_msg_map.contains(mq_str)) {
				WARN("cm_connect_msg to %s ignored!\n", cm->server_msname);
				continue;
			}

			/* Send 'connect' POSIX message contents to the RDMA library */
			struct mq_connect_msg	connect_msg;
			memset(&connect_msg, 0, sizeof(connect_msg));
			connect_msg.rem_msid		= cm->client_msid;
			connect_msg.rem_msubid		= cm->client_msubid;
			connect_msg.rem_bytes		= cm->client_bytes;
			connect_msg.rem_rio_addr_len	= cm->client_rio_addr_len;
			connect_msg.rem_rio_addr_lo	= cm->client_rio_addr_lo;
			connect_msg.rem_rio_addr_hi	= cm->client_rio_addr_hi;
			connect_msg.rem_destid_len	= cm->client_destid_len;
			connect_msg.rem_destid		= cm->client_destid;

			/* Open message queue */
			mqd_t cm_mq = mq_open(mq_name, O_RDWR, 0644, &attr);
			if (cm_mq == (mqd_t)-1) {
				ERR("mq_open() failed: %s\n", strerror(errno));
				/* Don't remove MS from accept_msg_map; the
				 * client may retry connecting. However, don't also
				 * send an ACCEPT_MS since the server didn't get
				 * the message. */
				continue;
			}

			/* Send connect message to RDMA library/app */
			ret = mq_send(cm_mq,
				      (const char *)&connect_msg,
				      sizeof(struct mq_connect_msg),
				      1);
			if (ret < 0) {
				ERR("mq_send failed: %s\n", strerror(errno));
				mq_close(cm_mq);
				/* Don't remove MS from accept_msg_map; the
				 * client may retry connecting. However, don't also
				 * send an ACCEPT_MS since the server didn't get
				 * the message. */
				mq_close(cm_mq);
				continue;
			}
			mq_close(cm_mq);

			/* Request a send buffer */
			void *cm_send_buf;
			conn_disc_server->get_send_buffer(&cm_send_buf);
			conn_disc_server->flush_send_buffer();

			/* Copy the corresponding accept message from map */
			cm_accept_msg	accept_message = accept_msg_map.get_item(mq_str);
			memcpy( cm_send_buf,
				(void *)&accept_message,
				sizeof(cm_accept_msg));

			/* Send 'accept' message to remote daemon */
			if (conn_disc_server->send()) {
				/* The server was already notified of the CONNECT_MS
				 * via the POSIX message. Now that we are failing
				 * to notify the client's daemon then we should remove
				 * the memory space from the map because there is
				 * no rdma_accept_ms_h() to receive a another CONNECT
				 * notification.
				 */
				accept_msg_map.remove(mq_str);
				continue;
			}

			INFO("cm_accept_msg sent back to remote daemon!\n");

			/* Now the destination ID must be added to the memory space.
			 * This is for the case where the memory space is destroyed
			 * and the remote users of that space need to be notified. */
			mspace	*ms;
			ms = the_inbound->get_mspace(cm->server_msname);
			if (ms)
				ms->add_destid(cm->client_destid);

			/* Erase cm_accept_msg from map */
			accept_msg_map.remove(mq_str);
		} else if (cm->type == DISCONNECT_MS) {
			conn_disc_server->get_recv_buffer((void **)&dm);
			INFO("Received DISCONNECT_MS msid(0x%X)\n",
							dm->server_msid);

			/* Remove client_destid from 'ms' identified by server_msid */
			mspace *ms = the_inbound->get_mspace(dm->server_msid);
			if (!ms) {
				ERR("Failed to find ms(0x%X)\n", dm->server_msid);
				continue;	/* Not much else to do without the ms */
			} else {
				if (ms->remove_destid(dm->client_destid) < 0)
					ERR("Failed to remove destid(0x%X)\n",
							dm->client_destid);
					/* Although this should not happen, we can still
					 * proceed with the disconnection at least. This
					 * may in the future result in spurious destroy
					 * messages, but those can be discarded.
					 */
				else {
					INFO("Removed desitd(0x%X) from msid(0x%X)\n",
								dm->client_destid, dm->server_msid);
				}
			}

			/* Consider this memory space disconnected. Allow accepting */
			ms->set_accepted(false);

			/* Prepare POSIX disconnect message from CM disconnect message */
			struct mq_disconnect_msg	disconnect_msg;
			disconnect_msg.client_msubid = dm->client_msubid;

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
			INFO("'Disconnect' message relayed to 'server'\n");
		} /* DISCONNECT_MS message */
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
					peer->prov_channel);
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
		conn_disc_thread_info	*cdti =
			(conn_disc_thread_info *)malloc(sizeof(conn_disc_thread_info));
		if (!cdti) {
			CRIT("Failed to allocate cdti\n");
			continue;
		}
		cdti->prov_server = prov_server;
		ret = pthread_create(&cdti->tid, NULL, conn_disc_thread_f, prov_server);
		if (ret) {
			CRIT("Failed to create conn_disc thread\n");
			free(cdti);
			continue;	/* Better luck next time? */
		}
	} /* while(1) */
	pthread_exit(0);
} /* prov_thread() */



/**
 * In this thread we wait for a connect request from client and we send
 * an accept reply.
 */
void *accept_thread_f(void *arg)
{
	(void)arg;
	cm_accept_msg	accept_message;
	mqd_t	cm_mq;
	int	ret;
	void *cm_recv_buf;

	main_server->get_recv_buffer(&cm_recv_buf);

	while (1) {
		/* Block until there is a 'connect' pending reception */
		HIGH("Waiting for cm_wait_connect_sem\n");
		if (sem_wait(&peer.cm_wait_connect_sem) == -1) {
			WARN("sem_wait(&peer.cm_wait_connect_sem): %s\n",
							strerror(errno));
			goto exit;
		}

		/* Kill the thread? */
		if (shutting_down) {
			CRIT("Terminating thread via global flag\n");
			pthread_exit(0);
		}

		/* Wait for connect request from client */
		HIGH("Calling main_server->accept()\n");
		ret = main_server->accept();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() was called. Exiting thread\n");
			} else {
				ERR("%s\n", strerror(ret));
			}
			pthread_exit(0);
		}
		HIGH("Connection from other RDMA daemon!!!\n");

		/* Wait for cm_connect_msg from client daemon */
		HIGH("Waiting for cm_connect_msg from client daemon\n");
		ret = main_server->receive();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() was called. Exiting thread\n");
			} else {
				ERR("%s\n", strerror(ret));
			}
			pthread_exit(0);
		}
		HIGH("Got cm_connect_msg from client daemon\n");
		/* Obtain memory space name */
		struct cm_connect_msg *c = (struct cm_connect_msg *)cm_recv_buf;

		/* Form message queue name from memory space name */
		char mq_name[CM_MS_NAME_MAX_LEN+2];
		memset(mq_name, '\0', CM_MS_NAME_MAX_LEN+2);
		mq_name[0] = '/';
		strcpy(&mq_name[1], c->server_msname);
		string mq_str(mq_name);
		DBG("mq_name = %s\n", mq_name);

		/* Compare message queue name with list. If no match ignore! */
		if (!accept_msg_map.contains(mq_str)) {
			WARN("cm_connect_msg to %s ignored!\n", c->server_msname);
			continue;
		}

		/* Send 'connect' POSIX message contents to the RDMA library */
		struct mq_connect_msg	connect_msg;
		memset(&connect_msg, 0, sizeof(connect_msg));
		connect_msg.rem_msid		= c->client_msid;
		connect_msg.rem_msubid		= c->client_msubid;
		connect_msg.rem_bytes		= c->client_bytes;
		connect_msg.rem_rio_addr_len	= c->client_rio_addr_len;
		connect_msg.rem_rio_addr_lo	= c->client_rio_addr_lo;
		connect_msg.rem_rio_addr_hi	= c->client_rio_addr_hi;
		connect_msg.rem_destid_len	= c->client_destid_len;
		connect_msg.rem_destid		= c->client_destid;

		/* Open message queue */
		cm_mq = mq_open(mq_name, O_RDWR, 0644, &attr);
		if (cm_mq == (mqd_t)-1) {
			ERR("mq_open(): %s\n", strerror(errno));
			goto exit;
		}

		/* Send connect message to RDMA library/app */
		ret = mq_send(cm_mq,
			      (const char *)&connect_msg,
			      sizeof(struct mq_connect_msg),
			      1);
		if (ret < 0) {
			WARN("Failed to send message: %s\n", strerror(errno));
			/* Failed to send the POSIX message. Discard the
			 * corresponding entry from the map, and wait for
			 * next CM message */
			goto cm_mq_close;
		}

		/* Request a send buffer */
		void *cm_send_buf;
		main_server->get_send_buffer(&cm_send_buf);
		main_server->flush_send_buffer();

		/* Copy the corresponding accept message from map */
		accept_message = accept_msg_map.get_item(mq_str);
		memcpy( cm_send_buf,
			(void *)&accept_message,
			sizeof(cm_accept_msg));

		/* Send 'accept' message to remote daemon */
		if (main_server->send()) {
			/* Failed to send a reply to remote daemon. Discard
			 * corresponding entry from the map, and wait for
			 * next CM message */
			goto cm_mq_close;
		}
		INFO("cm_accept_msg sent back to remote daemon!\n");

		/* Now the destination ID must be added to the memory space.
		 * This is for the case where the memory space is destroyed
		 * and the remote users of that space need to be notified. */
		mspace	*ms;
		ms = the_inbound->get_mspace(c->server_msname);
		if (ms)
			ms->add_destid(c->client_destid);
cm_mq_close:
		mq_close(cm_mq);

		/* Erase cm_accept_msg from map */
		accept_msg_map.remove(mq_str);
	} /* while */
exit:
	CRIT("died due to above error(s)\n");
	pthread_exit(0);

} /* accept_thread_f() */

/* For a server daemon to be notified of 'disconnect's from client daemons */
void *server_wait_disc_thread_f(void *arg)
{
	(void)arg;
	void	*cm_recv_buf;

	/* Get pointer to the recieve buffer */
	aux_server->get_recv_buffer(&cm_recv_buf);

	while (1) {
		int	ret;
		/* Wait for the CM disconnect message containing rem_msh */
		HIGH("Calling aux_server->accept()\n");
		ret = aux_server->accept();
		if (ret) {
			if (ret == EINTR) {
				HIGH("pthread_kill() was called. Exiting thread\n");
				pthread_exit(0);
			} else {
				ERR("aux_server->accept() failed: %s\n", strerror(ret));
			}
			continue;
		}
		HIGH("Connection from RDMA daemon on AUX!\n");

		/* Flush receive buffer of previous message */
		aux_server->flush_recv_buffer();

		/* Wait for 'disconnect' CM from client's RDMA daemon */
		HIGH("Waiting for 'disconnect' CM from client daemon\n");
		ret = aux_server->receive();
		if (ret) {
			if (ret == EINTR) {
				HIGH("pthread_kill() was called. Exiting thread\n");
				pthread_exit(0);
			} else {
				ERR("aux_server->receive() failed: %s\n", strerror(ret));
			}
			continue;
		}
		HIGH("Received cm_disconnect_msg from client daemon\n");

		/* Extract CM message */
		cm_disconnect_msg *disc_msg = (cm_disconnect_msg *)cm_recv_buf;

		/* Remove client_destid from 'ms' identified by server_msid */
		mspace *ms = the_inbound->get_mspace(disc_msg->server_msid);
		if (!ms) {
			ERR("Failed to find ms(0x%X)\n", disc_msg->server_msid);
		} else {
			if (ms->remove_destid(disc_msg->client_destid) < 0)
				ERR("Failed to remove destid(0x%X)\n",
							disc_msg->client_destid);
			else {
				INFO("Removed desitd(0x%X) from msid(0x%X)\n",
				disc_msg->client_destid, disc_msg->server_msid);
			}
		}

		/* Consider this memory space disconnected. Allow other connections */
		ms->set_accepted(false);

		/* Prepare POSIX disconnect message from CM disconnect message */
		struct mq_disconnect_msg	disconnect_msg;
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
		INFO("'Disconnect' message relayed to 'server'..back to accepting\n");
	} /* while */

	pthread_exit(0);
} /* server_wait_disc_thread() */


