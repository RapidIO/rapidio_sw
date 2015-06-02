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

#include "rdma_logger.h"
#include "rdmad_cm.h"
#include "rdma_mq_msg.h"
#include "cm_sock.h"

#include "rdmad_main.h"
#include "rdmad_svc.h"


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
		INFO("Waiting for cm_wait_connect_sem\n");
		if (sem_wait(&peer.cm_wait_connect_sem) == -1) {
			WARN("sem_wait(&peer.cm_wait_connect_sem): %s\n",
							strerror(errno));
			goto exit;
		}

		/* Kill the thread? */
		if (kill_the_threads) {
			CRIT("Terminating\n");
			pthread_exit(0);
		}

		/* Wait for connect request from client */
		INFO("Calling main_server->accept()\n");
		if (main_server->accept())
			goto exit;
		INFO("Connection from other RDMA daemon!!!\n");

		/* Wait for cm_connect_msg from client */
		if (main_server->receive())
			goto exit;

		/* Obtain memory space name */
		struct cm_connect_msg *c = (struct cm_connect_msg *)cm_recv_buf;

		/* Form message queue name from memory space name */
		char mq_name[CM_MS_NAME_MAX_LEN+2];
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
		/* Wait for the CM disconnect message containing rem_msh */
		DBG("Calling aux_server->accept()\n");
		if (aux_server->accept())
			goto thread_exit;
		INFO("Connection from RDMA daemon on AUX!\n");

		/* Flush receive buffer of previous message */
		aux_server->flush_recv_buffer();

		/* Wait for 'disconnect' CM from client's RDMA daemon */
		if (aux_server->receive()) {
			CRIT("Failed to receive CM 'disconnect'..\n");
			CRIT("..back to accepting connections..\n");
			continue;	/* Maybe from another daemon? */
		}

		/* Extract CM message */
		cm_disconnect_msg *disc_msg = (cm_disconnect_msg *)cm_recv_buf;

		/* Remove client_destid from 'ms' identified by server_msid */
		mspace *ms = the_inbound->get_mspace(disc_msg->server_msid);
		if (!ms) {
			WARN("Failed to find ms(0x%X)\n", disc_msg->server_msid);
		} else {
			if (ms->remove_destid(disc_msg->client_destid) < 0)
				WARN("Failed to remove destid(0x%X)\n",
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
			WARN("Failed to open %s\n", mq_name);
			goto thread_exit;
		}

		/* Send 'disconnect' POSIX message contents to the RDMA library */
		int ret = mq_send(disc_mq,
				  (const char *)&disconnect_msg,
				  sizeof(struct mq_disconnect_msg),
				  1);
		if (ret < 0) {
			WARN("Failed to send message: %s\n", strerror(errno));
			mq_close(disc_mq);
			goto thread_exit;
		}

		/* Now close POSIX queue */
		/* FIXME: Can we close the queue or should we have a delay,
		 * to allow the message to arrive first? */
		mq_close(disc_mq);
	} /* while */

thread_exit:
	CRIT("Exit\n");
	pthread_exit(0);
} /* server_wait_disc_thread() */


