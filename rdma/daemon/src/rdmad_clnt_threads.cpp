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

#include "rdma_mq_msg.h"
#include "liblog.h"
#include "cm_sock.h"
#include "rdmad_svc.h"
#include "rdmad_main.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_rdaemon.h"

using namespace std;

/* List of destids provisioned via the HELLO command/message */
vector<hello_daemon_info>	hello_daemon_info_list;
sem_t	hello_daemon_info_list_sem;

struct wait_accept_destroy_thread_info {
	wait_accept_destroy_thread_info(hello_daemon_info *hdi) : hdi(hdi)
	{
		sem_init(&started, 0, 0);
	}
	hello_daemon_info *hdi;
	sem_t		started;
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

	/* Post semaphore to caller to indicate thread is up */
	sem_post(&wadti->started);

	if (!wadti->hdi) {
		CRIT("NULL argument. Exiting\n");
		pthread_exit(0);
	}

	/* Store remote daemon info in the 'hello' daemon list */
	hello_daemon_info *hdi = wadti->hdi;
	sem_wait(&hello_daemon_info_list_sem);
	hello_daemon_info_list.push_back(*hdi);
	sem_post(&hello_daemon_info_list_sem);

	cm_client	*rx_accept_dest_client = hdi->client;

	while(1) {
		int	ret;
		/* Receive ACCEPT_MS, or DESTROY_MS message */
		ret = rx_accept_dest_client->receive();
		if (ret) {
			if (ret == EINTR) {
				WARN("pthread_kill() called. Exiting!\n");
			} else {
				CRIT("Failed to receive on rx_accept_dest_client: %s\n",
								strerror(ret));
			}
			pthread_exit(0);
		}
		/* Read all messages as ACCEPT_MS first, then if the
		 * type is different then cast message buffer accordingly. */
		cm_accept_msg	*accept_msg;
		rx_accept_dest_client->get_recv_buffer((void **)&accept_msg);
		if (accept_msg->type == ACCEPT_MS) {
			HIGH("Received ACCEPT_MS from %s\n",
						accept_msg->server_ms_name);

			/* Form message queue name from memory space name */
			char mq_name[CM_MS_NAME_MAX_LEN+2];
			mq_name[0] = '/';
			strcpy(&mq_name[1], accept_msg->server_ms_name);
			string mq_str(mq_name);

			/* Is the message queue name in wait_accept_mq_names? */
			/* Not found. Ignore message since no one is waiting for it */
			if (!wait_accept_mq_names.contains(mq_str)) {
				WARN("Ignoring message from ms('%s')!\n",
						accept_msg->server_ms_name);
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
			catch(msg_q_exception e) {
				e.print();
				WARN("Failed to open POSIX queue '%s': %s\n",
							mq_name, strerror(errno));
				continue;
			}
			DBG("Opened POSIX queue '%s'\n", mq_name);

			/* POSIX accept message preparation */
			mq_accept_msg	*accept_cm_msg;
			accept_mq->get_send_buffer(&accept_cm_msg);
			accept_msg->server_msid		= accept_cm_msg->server_msid;
			accept_msg->server_bytes	= accept_cm_msg->server_bytes;
			accept_msg->server_rio_addr_len	= accept_cm_msg->server_rio_addr_len;
			accept_msg->server_rio_addr_lo	= accept_cm_msg->server_rio_addr_lo;
			accept_msg->server_rio_addr_hi	= accept_cm_msg->server_rio_addr_hi;
			accept_msg->server_destid_len	= accept_cm_msg->server_destid_len;
			accept_msg->server_destid	= accept_cm_msg->server_destid;
			DBG("CM Accept: msid= 0x%X, destid=0x%X, destid_len = 0x%X, rio=0x%lX\n",
							accept_cm_msg->server_msid,
							accept_cm_msg->server_destid,
							accept_cm_msg->server_destid_len,
							accept_cm_msg->server_rio_addr_lo);
			DBG("CM Accept: bytes = %u, rio_addr_len = %u\n",
							accept_cm_msg->server_bytes,
							accept_cm_msg->server_rio_addr_len);
			DBG("MQ Accept: msid= 0x%X, destid=0x%X, destid_len = 0x%X, rio=0x%lX\n",
								accept_msg->server_msid,
								accept_msg->server_destid,
								accept_msg->server_destid_len,
								accept_msg->server_rio_addr_lo);
			DBG("MQ Accept: bytes = %u, rio_addr_len = %u\n",
								accept_msg->server_bytes,
								accept_msg->server_rio_addr_len);
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
		} else if (accept_msg->type == DESTROY_MS) {
			cm_destroy_msg	*destroy_msg;
			rx_accept_dest_client->get_recv_buffer((void **)&destroy_msg);

			HIGH("Received CM destroy  containing '%s'\n",
								destroy_msg->server_msname);

			/* Prep POSIX message queue name */
			char	mq_name[CM_MS_NAME_MAX_LEN+2];
			mq_name[0] = '/';
			strcpy(&mq_name[1], destroy_msg->server_msname);

			/* Open destroy/destroy-ack message queue */
			msg_q<mq_destroy_msg>	*destroy_mq;
			try {
				destroy_mq = new msg_q<mq_destroy_msg>(mq_name, MQ_OPEN);
			}
			catch(msg_q_exception e) {
				e.print();
				ERR("Failed to open 'destroy' POSIX queue (%s): %s\n",
								mq_name, strerror(errno));
				continue;
			}

			/* Send 'destroy' POSIX message to the RDMA library */
			mq_destroy_msg	*dest_msg;
			destroy_mq->get_send_buffer(&dest_msg);
			dest_msg->server_msid = destroy_msg->server_msid;
			if (destroy_mq->send()) {
				ERR("Failed to send 'destroy' message to client.\n");
				/* Don't exit; there maybe a problem with that memory space
				 * but not with others */
				delete destroy_mq;
				continue;
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
				/* The server daemon will timeout on the destory-ack
				 * reception since it is now using a timed receive CM call.
				 */
				HIGH("Timed out without receiving ACK to destroy\n");
			} else {
				HIGH("POSIX destroy_ack received for %s\n",
							destroy_msg->server_msname);
				cm_destroy_ack_msg *dam;

				/* Flush CM send buffer of previous message */
				rx_accept_dest_client->get_send_buffer((void **) &dam);
				rx_accept_dest_client->flush_send_buffer();

				/* Now send back a destroy_ack CM message */
				dam->type	= DESTROY_ACK_MS;
				strcpy(dam->server_msname, destroy_msg->server_msname);
				dam->server_msid = destroy_msg->server_msid;
				if (rx_accept_dest_client->send()) {
					WARN("Failed to send destroy_ack to server daemon\n");
				} else {
					HIGH("Sent destroy_ack to server daemon\n");
				}
			}

			/* Done with the destroy POSIX message queue */
			delete destroy_mq;
		} /* if DESTROY_MS */
	} /* while(1) */
	pthread_exit(0);
} /* wait_accept_destroy_thread_f() */

/**
 * Provision a remote daemon by sending a HELLO message.
 */
int provision_rdaemon(uint32_t destid)
{
	/* Create provision client to connect to remote daemon's provisioning thread */
	cm_client	*hello_client;

	/* FOR NOW, fail if the 'destid' already has an entry/thread */
	sem_wait(&hello_daemon_info_list_sem);
	auto it = find(begin(hello_daemon_info_list), end(hello_daemon_info_list),
			destid);
	if (it != end(hello_daemon_info_list)) {
		WARN("destid(0x%X) is already known\n", destid);
		sem_post(&hello_daemon_info_list_sem);
		return -7;	/* TODO: should be #define'd in a header */
	}
	sem_post(&hello_daemon_info_list_sem);

	try {
		hello_client = new cm_client("prov_client", peer.mport_id, 0, peer.prov_channel);
	}
	catch(cm_exception e) {
		CRIT("%s\n", e.err);
		return -1;
	}

	/* Connect to remote daemon */
	int ret = hello_client->connect(destid);
	if (ret) {
		CRIT("Failed to connect to destid(0x%X)\n", destid);
		delete hello_client;
		return -2;
	}
	INFO("Connected to remote daemon on destid(0x%X\n", destid);

	/* Send HELLO message containing our destid */
	hello_msg_t	*hm;
	hello_client->get_send_buffer((void **)&hm);
	hm->destid = peer.destid;
	if (hello_client->send()) {
		ERR("Failed to send HELLO to destid(0x%X)\n", destid);
		delete hello_client;
		return -3;
	}
	DBG("HELLO message successfully sent to destid(0x%X\n", destid);

	/* Receive HELLO (ack) message back with remote destid */
	hello_msg_t 	*ham;	/* HELLO-ACK message */
	hello_client->get_recv_buffer((void **)&ham);
	if (hello_client->timed_receive(5000)) {
		ERR("Failed to receive HELLO ACK from destid(0x%X)\n", destid);
		delete hello_client;
		return -4;
	}
	if (ham->destid != destid) {
		WARN("hello-ack destid(0x%X) != destid(0x%X)\n", ham->destid, destid);
	}
	DBG("HELLO ACK successfully received from destid(0x%X\n", destid);

	/* Create and initialize hello_daemon_info struct */
	hello_daemon_info *hdi = new hello_daemon_info(destid, hello_client);
	if (!hdi) {
		CRIT("Failed to allocate hello_daemon_info\n");
		delete hello_client;
		return -5;
	}

	/* Create thread for sending requests from client to server */
	wait_accept_destroy_thread_info	*wadti =
			new wait_accept_destroy_thread_info(hdi);
	if (!wadti) {
		CRIT("Failed to allocate hello_daemon_info\n");
		delete hello_client;
		delete hdi;
		return -5;
	}
	DBG("Creating request thread\n");
	ret = pthread_create(&wadti->hdi->tid,
			     NULL,
			     wait_accept_destroy_thread_f,
			     wadti);
	if (ret) {
		CRIT("Failed to create request thread\n");
		delete hello_client;
		delete hdi;
		delete wadti;
		return -6;
	}
	sem_wait(&wadti->started);
	DBG("Request thread started successully\n");

	return 0;
} /* provision_rdaemon() */

#if 0
/**
 * This thread waits for an accept reply from a server that we
 * have sent a connect request to.
 */
void *wait_accept_thread_f(void *arg)
{
	uint32_t destid = *((uint32_t *)arg);

	/* Wait until main thread says it is OK to access the list */
	HIGH("BEFORE sem_wait(&client_rdaemon_list_sem)\n");
	sem_wait(&client_rdaemon_list_sem);	/* Lock client_rdaemon_list */
	HIGH("AFTER sem_wait(&client_rdaemon_list_sem)\n");

	/* Find the rdaemon entry in client_rdaemon_list (by destid) */
	rdaemon_has_destid	rdhd(destid);
	auto rdit = find_if(begin(client_rdaemon_list), end(client_rdaemon_list), rdhd);
	if (rdit != end(client_rdaemon_list)) {
		INFO("FOUND daemon for destid (0x%X) in the list\n", destid);
	} else {
		CRIT("Daemon for destid(0x%X) not found. EXITING\n", destid);
		sem_post(&client_rdaemon_list_sem); /* Unlock */
		HIGH("AFTER sem_post(&client_rdaemon_list_sem)\n");
		pthread_exit(0);
	}
	rdaemon_t *rdaemon = (*rdit);
	sem_post(&client_rdaemon_list_sem); /* Unlock client_rdaemon_list */
	HIGH("AFTER sem_post(&client_rdaemon_list_sem)\n");

	/* Initialize CM receive buffer */
	cm_client *main_client = rdaemon->main_client;
	cm_accept_msg	*accept_cm_msg;
	main_client->get_recv_buffer((void **)&accept_cm_msg);

	while (1) {

		/* Block until there is an 'accept' pending reception */
		HIGH("Waiting for cm_wait_accept_sem (0x%X) to post\n",
						rdaemon->cm_wait_accept_sem);
		if (sem_wait(&rdaemon->cm_wait_accept_sem) == -1) {
			WARN("sem_wait(&cm_wait_accept_sem), %s\n",
							strerror(errno));
			goto exit;
		}
		HIGH("rdaemon->cm_wait_accept_sem has posted\n");

		/* Kill the thread? */
		if (shutting_down) {
			CRIT("Terminating since daemon is shutting down\n");
			pthread_exit(0);
		}

		/* Flush the CM receive buffer */
		main_client->flush_recv_buffer();

		/* Wait for incoming accept CM from server's RDMA daemon */
		HIGH("main_client->receive() waiting for accept from server daemon\n");
		if (main_client->receive())
			continue;
		HIGH("Received accept from %s\n", accept_cm_msg->server_ms_name);

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

		/* All is good, and we need to relay the message back to the
		 * rdma_conn_ms_h() API via POSIX messaging */
		/* Open message queue */
		msg_q<mq_accept_msg>	*accept_mq;
		try {
			accept_mq = new msg_q<mq_accept_msg>(mq_name, MQ_OPEN);
		}
		catch(msg_q_exception e) {
			e.print();
			WARN("Failed to open POSIX queue '%s': %s\n",
						mq_name, strerror(errno));
			continue;
		}
		INFO("Opened POSIX queue '%s'\n", mq_name);

		/* POSIX accept message preparation */
		mq_accept_msg	*accept_msg;
		accept_mq->get_send_buffer(&accept_msg);
		accept_msg->server_msid		= accept_cm_msg->server_msid;
		accept_msg->server_bytes	= accept_cm_msg->server_bytes;
		accept_msg->server_rio_addr_len	= accept_cm_msg->server_rio_addr_len;
		accept_msg->server_rio_addr_lo	= accept_cm_msg->server_rio_addr_lo;
		accept_msg->server_rio_addr_hi	= accept_cm_msg->server_rio_addr_hi;
		accept_msg->server_destid_len	= accept_cm_msg->server_destid_len;
		accept_msg->server_destid	= accept_cm_msg->server_destid;
		DBG("CM Accept: msid= 0x%X, destid=0x%X, destid_len = 0x%X, rio=0x%lX\n",
							accept_cm_msg->server_msid,
							accept_cm_msg->server_destid,
							accept_cm_msg->server_destid_len,
							accept_cm_msg->server_rio_addr_lo);
		DBG("CM Accept: bytes = %u, rio_addr_len = %u\n",
							accept_cm_msg->server_bytes,
							accept_cm_msg->server_rio_addr_len);
		DBG("MQ Accept: msid= 0x%X, destid=0x%X, destid_len = 0x%X, rio=0x%lX\n",
							accept_msg->server_msid,
							accept_msg->server_destid,
							accept_msg->server_destid_len,
							accept_msg->server_rio_addr_lo);
		DBG("MQ Accept: bytes = %u, rio_addr_len = %u\n",
							accept_msg->server_bytes,
							accept_msg->server_rio_addr_len);
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

		/* Clean up rdaemon and remove from list */
		/* FIXME: Temporary solution. The real solution is to have a reference
		 * count every time we use the rdaemon entry and only remove when the
		 * count reaches 0.
		 */
		delete rdaemon->main_client;
		client_rdaemon_list.erase(rdit);
		pthread_exit(0);
	} /* while */

exit:
	pthread_exit(0);
} /* wait_accept_thread() */

/* Thread that waits for a destroy message from a remote daemon */
void *client_wait_destroy_thread_f(void *arg)
{
	(void)arg;
	cm_server *destroy_server;
	void	*cm_recv_buf;
	void	*cm_send_buf;
	int	ret;

	/* Create server object for 'destroy' messages */
	try {
		destroy_server = new cm_server("destroy_server",
					       peer.mport_id,
					       peer.destroy_mbox_id,
					       peer.destroy_channel);
	}
	catch(cm_exception e) {
		CRIT("destroy_server: %s. EXITING\n", e.err);
		pthread_exit(0);
	}

	/* Get pointer to the recieve buffer */
	destroy_server->get_recv_buffer(&cm_recv_buf);

	/* Get pointer to the send buffer */
	destroy_server->get_send_buffer(&cm_send_buf);

	while (1) {
		/* Wait for the CM destroy message containing msid, and ms_name */
		HIGH("Waiting for 'destroy' message from remote daemon\n");
		ret = destroy_server->accept();
		if (ret) {
			if (ret == EINTR) {
				INFO("pthread_kill was called. Exiting thread\n");
				delete destroy_server;
				pthread_exit(0);
			} else {
				ERR("Failed to accept(). Retrying...\n");
			}
			continue;
		}
		HIGH("Connection from RDMA daemon on DESTROY!\n");

		/* Flush receive buffer of previous message */
		destroy_server->flush_recv_buffer();

		/* Wait for incoming destroy CM from server's RDMA daemon */
		HIGH("Waiting for destroy message from server daemon\n");
		if (destroy_server->receive()) {
			ERR("Failed to receive(). Retrying...\n");
			continue;
		}

		/* Extract message */
		cm_destroy_msg	*destroy_msg = (cm_destroy_msg *)cm_recv_buf;
		HIGH("Received CM destroy  containing '%s'\n",
						destroy_msg->server_msname);

		/* Prep POSIX message queue name */
		char		mq_name[CM_MS_NAME_MAX_LEN+2];
		mq_name[0] = '/';
		strcpy(&mq_name[1], destroy_msg->server_msname);

		/* Open destroy/destroy-ack message queue */
		msg_q<mq_destroy_msg>	*destroy_mq;
		try {
			destroy_mq = new msg_q<mq_destroy_msg>(mq_name, MQ_OPEN);
		}
		catch(msg_q_exception e) {
			e.print();
			ERR("Failed to open 'destroy' POSIX queue (%s): %s\n",
							mq_name, strerror(errno));
			/* Don't exit; there maybe a problem with that memory space
			 * but not with others */
			continue;
		}

		/* Send 'destroy' POSIX message to the RDMA library */
		mq_destroy_msg	*dest_msg;
		destroy_mq->get_send_buffer(&dest_msg);
		dest_msg->server_msid = destroy_msg->server_msid;
		if (destroy_mq->send()) {
			ERR("Failed to send 'destroy' message to client.\n");
			/* Don't exit; there maybe a problem with that memory space
			 * but not with others */
			delete destroy_mq;
			continue;
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
			/* The server daemon will timeout on the destory-ack
			 * reception since it is now using a timed receive CM call.
			 */
			HIGH("Timed out without receiving ACK to destroy\n");
		} else {
			HIGH("destroy_ack received for %s\n",
						destroy_msg->server_msname);
			/* Flush CM send buffer of previous message */
			destroy_server->flush_send_buffer();

			/* Now send back a destroy_ack CM message */
			cm_destroy_ack_msg *dam = (cm_destroy_ack_msg *)cm_send_buf;
			dam->type	= DESTROY_ACK_MS;
			strcpy(dam->server_msname, destroy_msg->server_msname);
			dam->server_msid = destroy_msg->server_msid;
			if (destroy_server->send()) {
				WARN("Failed to send destroy_ack to server daemon\n");
			} else {
				HIGH("Sent destroy_ack to server daemon\n");
			}
		}

		/* Done with the destroy POSIX message queue */
		delete destroy_mq;
	} /* while */

	pthread_exit(0);
} /* client_wait_destroy_thread_f() */


#endif
