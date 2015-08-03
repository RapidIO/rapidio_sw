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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <rapidio_mport_dma.h>
#include <semaphore.h>
#include <signal.h>

//#include <rapidio_mport_mgmt.h>
#include "cm_sock.h"
#include "rdma_mq_msg.h"
#include "liblog.h"
#include "ts_map.h"

#include "rdmad_cm.h"
#include "rdmad_inbound.h"
#include "rdmad_ms_owners.h"
#include "rdmad_peer_utils.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_console.h"
#include "rdmad_fm.h"
#include "libcli.h"
#include "unix_sock.h"
#include "rdmad_unix_msg.h"

struct peer_info	peer;

using namespace std;

/* Memory Space Owner data */
ms_owners owners;

/* Inbound space */
inbound *the_inbound;

/* Global flag for shutting down */
bool shutting_down = false;

/* Map of accept messages awaiting connect. Keyed by message queue name */
ts_map<string, cm_accept_msg>	accept_msg_map;

/* List of queue names awaiting accept */
ts_vector<string>	wait_accept_mq_names;

static 	pthread_t console_thread;
static	pthread_t prov_thread;
static	pthread_t cli_session_thread;
static unix_server *server;

static void init_peer()
{
	peer.destid = 0xFFFF;
	peer.destid_len = 16;

	/* MPORT */
	peer.mport_id = 0;	/* Overriden on the command line */
	peer.mport_hnd = 0;

	/* Messaging */
	peer.prov_channel	= DEFAULT_PROV_CHANNEL;
	peer.prov_mbox_id	= DEFAULT_PROV_MBOX_ID;

	/* CLI */
	peer.cons_skt = 4444;
	peer.run_cons = 1;
}

struct rpc_ti
{
	rpc_ti(int accept_socket) : accept_socket(accept_socket)
	{}
	int accept_socket;
	sem_t	started;
	pthread_t tid;
};

void *rpc_thread_f(void *arg)
{
	if (!arg) {
		CRIT("Null argument.\n");
		pthread_exit(0);
	}

	rpc_ti *ti = (rpc_ti *)arg;

	DBG("Creating application-specific server object...\n");
	unix_server *other_server;
	try {
		other_server = new unix_server("other_server", ti->accept_socket);
	}
	catch(unix_sock_exception& e) {
		CRIT("Failed to create unix_server:%:\n", e.err);
		sem_post(&ti->started);
		pthread_exit(0);
	}

	sem_post(&ti->started);

	while (1) {
		/* Wait for data from clients */
		DBG("Waiting to receive API call library...\n");
		size_t	received_len = 0;	/* For build warning */
		if (other_server->receive(&received_len)) {
			CRIT("Failed to receive\n");
			delete other_server;
			pthread_exit(0);
		}

		if (received_len > 0) {
			unix_msg_t	*in_msg;
			unix_msg_t	*out_msg;

			other_server->get_recv_buffer((void **)&in_msg);
			other_server->get_send_buffer((void **)&out_msg);

			switch(in_msg->type) {
				case GET_MPORT_ID:
				{
					DBG("GET_MPORT_ID\n");
					out_msg->get_mport_id_out.mport_id = peer.mport_id;
					out_msg->get_mport_id_out.status = 0;
					out_msg->type = GET_MPORT_ID_ACK;
				}
				break;

				case CREATE_MSO:
				{
					DBG("CREATE_MSO\n");
					create_mso_input *in = &in_msg->create_mso_in;
					create_mso_output *out = &out_msg->create_mso_out;
					out_msg->type = CREATE_MSO_ACK;

					int ret = owners.create_mso(in->owner_name,
								    other_server,
								    &out->msoid);
					out->status = (ret > 0) ? 0 : ret;
				}
				break;

				case OPEN_MSO:
				{
					DBG("OPEN_MSO\n");
					open_mso_input	*in = &in_msg->open_mso_in;
					open_mso_output *out = &out_msg->open_mso_out;
					out_msg->type = OPEN_MSO_ACK;

					int ret = owners.open_mso(in->owner_name,
							&out->msoid,
							&out->mso_conn_id,
							other_server);
					out->status = (ret > 0) ? 0 : ret;
				}
				break;

				case CLOSE_MSO:
				{
					DBG("CLOSE_MSO\n");
					close_mso_input *in = &in_msg->close_mso_in;
					close_mso_output *out = &out_msg->close_mso_out;
					out_msg->type = CLOSE_MSO_ACK;

					int ret = owners.close_mso(in->msoid, in->mso_conn_id);
					out->status = (ret > 0) ? 0 : ret;
				}
				break;

				case DESTROY_MSO:
				{
					DBG("DESTROY_MSO\n");
					destroy_mso_input *in = &in_msg->destroy_mso_in;
					destroy_mso_output *out = &out_msg->destroy_mso_out;
					out_msg->type = DESTROY_MSO_ACK;

					ms_owner *owner;

					try {
						owner = owners[in->msoid];
						if (owner==nullptr) {
							ERR("Invalid msoid(0x%X)\n",
									in->msoid);
							out->status = -1;
						} else if (owner->owns_mspaces()) {
							WARN("msoid(0x%X) owns spaces!\n",
										in->msoid);
							out->status = -1;
						} else {
							/* No memory spaces owned by mso,
							 * just destroy it */
							int ret = owners.destroy_mso(
									in->msoid);
							out->status = (ret > 0) ? 0 : ret;
							DBG("owners.destroy_mso() %s\n",
							out->status ? "FAILED":"PASSED");
						}
						out_msg->type = DESTROY_MSO_ACK;
					}
					catch(...) {
						ERR("Invalid msoid(0x%X) caused segfault\n",
								in->msoid);
						out->status = -1;
					}
				}
				break;

				case CREATE_MS:
				{
					DBG("CREATE_MS\n");
					create_ms_input *in = &in_msg->create_ms_in;
					create_ms_output *out = &out_msg->create_ms_out;
					out_msg->type = CREATE_MS_ACK;

					/* Create memory space in the inbound space */
					mspace *ms;
					int ret = the_inbound->create_mspace(
							in->ms_name,
							in->bytes, in->msoid,
							&out->msid,
							&ms);
					out->status = (ret > 0) ? 0 : ret;
					DBG("the_inbound->create_mspace(%s) %s\n",
						in->ms_name,
						out->status ? "FAILED" : "PASSED");

					/* Add the memory space to the owner */
					if (!out->status)
						owners[in->msoid]->add_ms(ms);
				}
				break;

				case OPEN_MS:
				{
					DBG("OPEN_MS\n");
					open_ms_input  *in = &in_msg->open_ms_in;
					open_ms_output *out = &out_msg->open_ms_out;
					out_msg->type = OPEN_MS_ACK;

					/* Find memory space, return its msid,
					 *  ms_conn_id, and size in bytes */
					int ret = the_inbound->open_mspace(
								in->ms_name,
								in->msoid,
								&out->msid,
								&out->ms_conn_id,
								&out->bytes);
					out->status = (ret > 0) ? 0 : ret;
					DBG("the_inbound->open_mspace(%s) %s\n",
							in->ms_name,
							out->status ? "FAILED":"PASSED");					out_msg->open_ms_out = *out;
				}
				break;

				case CLOSE_MS:
				{
					DBG("CLOSE_MS\n");
					close_ms_input *in = &in_msg->close_ms_in;
					close_ms_output *out = &out_msg->close_ms_out;
					out_msg->type = CLOSE_MS_ACK;

					DBG("ENTER, msid=%u, ms_conn_id=%u\n", in->msid,
									in->ms_conn_id);
					mspace *ms = the_inbound->get_mspace(in->msid);
					if (!ms) {
						ERR("Could not find mspace with msid(0x%X)\n",
										in->msid);
						out->status = -1;
					} else {
						/* Now close the memory space */
						int ret = ms->close(in->ms_conn_id);
						out->status = (ret > 0) ? 0 : ret;
					}
				}
				break;

				case DESTROY_MS:
				{
					DBG("DESTROY_MS\n");
					destroy_ms_input *in = &in_msg->destroy_ms_in;
					destroy_ms_output *out = &out_msg->destroy_ms_out;
					out_msg->type = DESTROY_MS_ACK;

					mspace *ms = the_inbound->get_mspace(in->msoid, in->msid);
					if (!ms) {
						ERR("Could not find mspace with msid(0x%X)\n", in->msid);
						out->status = -1;
					} else {
						/* Now destroy the memory space */
						out->status = ms->destroy();
					}
				}
				break;

				case CREATE_MSUB:
				{
					DBG("CREATE_MSUB\n");
					create_msub_input *in = &in_msg->create_msub_in;
					create_msub_output *out = &out_msg->create_msub_out;
					out_msg->type = CREATE_MSUB_ACK;

					int ret = the_inbound->create_msubspace(in->msid,
									        in->offset,
									        in->req_bytes,
									        &out->bytes,
				                                                &out->msubid,
									        &out->rio_addr,
										&out->phys_addr);
					out->status = (ret > 0) ? 0 : ret;

					DBG("msubid=0x%X, bytes=%d, rio_addr = 0x%lX\n",
								out->msubid,
								out->bytes,
								out->rio_addr);
				}
				break;

				case DESTROY_MSUB:
				{
					DBG("DESTROY_MSUB\n");
					destroy_msub_input  *in  = &in_msg->destroy_msub_in;
					destroy_msub_output *out = &out_msg->destroy_msub_out;
					out_msg->type = DESTROY_MSUB_ACK;

					int ret = the_inbound->destroy_msubspace(in->msid, in->msubid);
					out->status = (ret > 0) ? 0 : ret;
				}
				break;

				case ACCEPT_MS:
				{
					DBG("ACCEPT_MS\n");
					accept_input  *in = &in_msg->accept_in;
					accept_output *out = &out_msg->accept_out;
					out_msg->type = ACCEPT_MS_ACK;

					/* Does it exist? */
					mspace *ms = the_inbound->get_mspace(in->loc_ms_name);
					if (!ms) {
						WARN("%s does not exist\n", in->loc_ms_name);
						out->status = -1;
						break;
					}

					/* Prevent concurrent accept() calls to
					 *  the same ms from different applications.
					 */
					if (ms->is_accepted()) {
						ERR("%s already in accept() or connected.\n",
									in->loc_ms_name);
						out->status = -2;
						break;;
					}
					ms->set_accepted(true);

					/* Get the memory space name, and prepend
					 * '/' to make it a queue */
					string	s(in->loc_ms_name);
					s.insert(0, 1, '/');

					/* Prepare accept message from input parameters */
					struct cm_accept_msg	cmam;
					cmam.type		= htobe64(CM_ACCEPT_MS);
					strcpy(cmam.server_ms_name, in->loc_ms_name);
					cmam.server_msid	= htobe64(ms->get_msid());
					cmam.server_msubid	= htobe64(in->loc_msubid);
					cmam.server_bytes	= htobe64(in->loc_bytes);
					cmam.server_rio_addr_len= htobe64(in->loc_rio_addr_len);
					cmam.server_rio_addr_lo	= htobe64(in->loc_rio_addr_lo);
					cmam.server_rio_addr_hi	= htobe64(in->loc_rio_addr_hi);
					cmam.server_destid_len	= htobe64(16);
					cmam.server_destid	= htobe64(peer.destid);
					DBG("cm_accept_msg has server_destid = 0x%X\n",
							be64toh(cmam.server_destid));
					DBG("cm_accept_msg has server_destid_len = 0x%X\n",
							be64toh(cmam.server_destid_len));

					/* Add accept message content to map indexed by message queue name */
					DBG("Adding entry in accept_msg_map for '%s'\n", s.c_str());
					accept_msg_map.add(s, cmam);

					out->status = 0;
				}
				break;

				case UNDO_ACCEPT:
				{
					DBG("UNDO_ACCEPT\n");
					undo_accept_input *in = &in_msg->undo_accept_in;
					undo_accept_output *out = &out_msg->undo_accept_out;
					out_msg->type = UNDO_ACCEPT_ACK;

					/* Does it exist? */
					mspace *ms = the_inbound->get_mspace(in->server_ms_name);
					if (!ms) {
						WARN("%s does not exist\n", in->server_ms_name);
						out->status = -1;
						break;
					}

					/* An accept() must be in effect to undo it. Double-check */
					if (!ms->is_accepted()) {
						ERR("%s NOT in accept().\n", in->server_ms_name);
						out->status = -2;
						break;
					}

					/* Get the memory space name, and prepend '/' to make it a queue */
					string	s(in->server_ms_name);
					s.insert(0, 1, '/');

					/* Remove accept message content from map indexed by message queue name */
					accept_msg_map.remove(s);

					/* TODO: How about if it is connected, but the server doesn't
					 * get the notification. If it is connected it cannot do undo_accept
					 * so let's think about that. */

					/* Now set it as unaccepted */
					ms->set_accepted(false);
					out->status = 0;
				}
				break;

				case SEND_CONNECT:
				{
					DBG("SEND_CONNECT\n");
					send_connect_input *in = &in_msg->send_connect_in;
					send_connect_output *out = &out_msg->send_connect_out;
					out_msg->type = SEND_CONNECT_ACK;

					/* Do we have an entry for that destid ? */
					sem_wait(&hello_daemon_info_list_sem);
					auto it = find(begin(hello_daemon_info_list),
						       end(hello_daemon_info_list),
						       in->server_destid);

					/* If the server's destid is not found, just fail */
					if (it == end(hello_daemon_info_list)) {
						ERR("destid(0x%X) was not provisioned\n", in->server_destid);
						sem_post(&hello_daemon_info_list_sem);
						out->status = -1;
						break;
					}
					sem_post(&hello_daemon_info_list_sem);

					/* Obtain pointer to socket object already connected to destid */
					cm_client *main_client = it->client;

					/* Obtain and flush send buffer for sending CM_CONNECT_MS message */
					cm_connect_msg *c;
					main_client->get_send_buffer((void **)&c);
					main_client->flush_send_buffer();

					/* Compose CONNECT_MS message */
					c->type			= htobe64(CM_CONNECT_MS);
					strcpy(c->server_msname, in->server_msname);
					c->client_msid		= htobe64(in->client_msid);
					c->client_msubid	= htobe64(in->client_msubid);
					c->client_bytes		= htobe64(in->client_bytes);
					c->client_rio_addr_len	= htobe64(in->client_rio_addr_len);
					c->client_rio_addr_lo	= htobe64(in->client_rio_addr_lo);
					c->client_rio_addr_hi	= htobe64(in->client_rio_addr_hi);
					c->client_destid_len	= htobe64(peer.destid_len);
					c->client_destid	= htobe64(peer.destid);

					/* Send buffer to server */
					if (main_client->send()) {
						ERR("Failed to send CONNECT_MS to destid(0x%X)\n",
											in->server_destid);
						out->status = -3;
						break;
					}
					INFO("cm_connect_msg sent to remote daemon\n");

					/* Add POSIX message queue name to list of queue names */
					string	mq_name(in->server_msname);
					mq_name.insert(0, 1, '/');

					/* Add to list of message queue names awaiting an 'accept' to 'connect' */
					wait_accept_mq_names.push_back(mq_name);

					out->status = 0;
				}
				break;

				case UNDO_CONNECT:
				{
					DBG("UNDO_CONNECT\n");
					undo_connect_input *in = &in_msg->undo_connect_in;
					undo_connect_output *out = &out_msg->undo_connect_out;
					out_msg->type = UNDO_CONNECT_ACK;

					/* Add POSIX message queue name to list of queue names */
					string	mq_name(in->server_ms_name);
					mq_name.insert(0, 1, '/');

					/* Remove from list of mq names awaiting an 'accept' reply to 'connect' */
					wait_accept_mq_names.remove(mq_name);
					out->status = 0;
				}
				break;

				case SEND_DISCONNECT:
				{
					DBG("SEND_DISCONNECT\n");
					send_disconnect_input *in = &in_msg->send_disconnect_in;
					send_disconnect_output *out = &out_msg->send_disconnect_out;
					out_msg->type = SEND_DISCONNECT_ACK;

					DBG("Client to disconnect from destid = 0x%X\n", in->rem_destid);
					/* Do we have an entry for that destid ? */
					sem_wait(&hello_daemon_info_list_sem);
					auto it = find(begin(hello_daemon_info_list),
						       end(hello_daemon_info_list),
						       in->rem_destid);

					/* If the server's destid is not found, just fail */
					if (it == end(hello_daemon_info_list)) {
						ERR("destid(0x%X) was not provisioned\n", in->rem_destid);
						sem_post(&hello_daemon_info_list_sem);
						out->status = -1;
						break;
					}
					sem_post(&hello_daemon_info_list_sem);

					/* Obtain pointer to socket object
					 * already connected to destid */
					cm_client *the_client = it->client;

					cm_disconnect_msg *disc_msg;

					/* Get and flush send buffer */
					the_client->flush_send_buffer();
					the_client->get_send_buffer((void **)&disc_msg);

					disc_msg->type		= htobe64(CM_DISCONNECT_MS);
					disc_msg->client_msubid	= htobe64(in->loc_msubid);	/* For removal from server database */
					disc_msg->server_msid   = htobe64(in->rem_msid);	/* For removing client's destid from server's
												 * info on the daemon */
					disc_msg->client_destid = htobe64(peer.destid);		/* For knowing which destid to remove */
					disc_msg->client_destid_len = htobe64(16);

					/* Send buffer to server */
					if (the_client->send()) {
						out->status = -1;
						break;
					}
					DBG("Sent DISCONNECT_MS for msid = 0x%lX, client_destid = 0x%lX\n",
						be64toh(disc_msg->server_msid),
						be64toh(disc_msg->client_destid));

					out->status = 0;
				}
				break;

				default:
					CRIT("UNKNOWN MESSAGE TYPE: 0x%X\n", in_msg->type);
			} /* switch */

			if (other_server->send(sizeof(unix_msg_t))) {
				CRIT("Failed to send API output parameters back to library\n");
				delete other_server;
				pthread_exit(0);
			}
		} else {
			HIGH("Application has closed connection. Exiting!\n");
			/* First destroy mso corresponding to socket */
			if (owners.destroy_mso(other_server)) {
				WARN("Failed to find owner mso using this sock conn\n");
			}

			/* Now find out if this socket is a user socket for an mso. If the
			 * user app shuts down without closing the mso, we do it
			 * here.
			 */
			owners.close_mso(other_server);

			delete other_server;

			pthread_exit(0);
		}
	} /* while */
	pthread_exit(0);
} /* rpc_thread_f() */

int run_rpc_alternative()
{
	/* Create a server */
	DBG("Creating Unix socket server object...\n");
	try {
		server = new unix_server();
	}
	catch(unix_sock_exception& e) {
		CRIT("Failed to create server: %s \n",  e.err);
		return 1;
	}

	while (1) {
		/* Wait for client to connect */
		HIGH("Waiting for (another) RDMA application to connect..\n");
		if (server->accept()) {
			CRIT("Failed to accept\n");
			delete server;
			return 2;
		}
		HIGH("Application connected!\n");
		int accept_socket = server->get_accept_socket();

		rpc_ti	*ti;
		try {
			ti = new rpc_ti(accept_socket);
		}
		catch(...) {
			CRIT("Failed to create rpc_ti\n");
			delete server;
			return 3;
		}

		/* Create thread that will handle requests from the new application */
		int ret = pthread_create(&ti->tid,
					 NULL,
					 rpc_thread_f,
					 ti);
		if (ret) {
			CRIT("Failed to create request thread\n");
			delete server;
			delete ti;
			return -6;
		}
		/* Wait for RPC processing thread to start */
		sem_wait(&ti->started);
	} /* while */
} /* run_rpc_alternative() */

void shutdown(struct peer_info *peer)
{
	int	ret = 0;

	/* Kill the threads */
	shutting_down = true;

	/* Delete the inbound object */
	INFO("Deleting the_inbound\n");
	delete the_inbound;

	/* Kill threads for remote daemons provisioned via incoming HELLO */
	HIGH("Killing remote daemon threads provisioned via incoming HELLO\n");
	sem_wait(&prov_daemon_info_list_sem);
	for (auto it = begin(prov_daemon_info_list);
	    it != end(prov_daemon_info_list);
	    it++) {
		/* We must post the semaphore so that the thread can access
		 * the list. Otherwise we'll have a deadlock with the thread waiting
		 * on the semaphore while we wait in pthread_join below!
		 */
		sem_post(&prov_daemon_info_list_sem);
		ret = pthread_kill(it->tid, SIGUSR1);
		if (ret == EINVAL) {
			CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
		}
		pthread_join(it->tid, NULL);
		/* Thread has terminated and posted the semaphore. Lock again */
		sem_wait(&prov_daemon_info_list_sem);
	}
	prov_daemon_info_list.clear();	/* Not really needed; we are exiting anyway */
	sem_post(&prov_daemon_info_list_sem);

	/* Kill threads for remote daemons provisioned via outgoing HELLO */
	HIGH("Killing remote daemon threads provisioned via outgoing HELLO\n");
	sem_wait(&hello_daemon_info_list_sem);
	for (auto it = begin(hello_daemon_info_list);
	    it != end(hello_daemon_info_list);
	    it++) {
		/* We must post the semaphore so that the thread can access
		 * the list. Otherwise we'll have a deadlock with the thread waiting
		 * on the semaphore while we wait in pthread_join below!
		 */
		sem_post(&hello_daemon_info_list_sem);
		ret = pthread_kill(it->tid, SIGUSR1);
		if (ret == EINVAL) {
			CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
		}
		pthread_join(it->tid, NULL);
		/* Thread has terminated and posted the semaphore. Lock again */
		sem_wait(&hello_daemon_info_list_sem);
	}
	hello_daemon_info_list.clear();	/* Not really needed; we are exiting anyway */
	sem_post(&hello_daemon_info_list_sem);

	/* Next, kill provisioning thread */
	HIGH("Killing provisioning thread\n");
	ret = pthread_kill(prov_thread, SIGUSR1);
	if (ret == EINVAL) {
		CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
	}
	pthread_join(prov_thread, NULL);
	HIGH("Provisioning thread is dead\n");

	/* Kill the fabric management thread */
	HIGH("Killing fabric management thread\n");
	halt_fm_thread();
	HIGH("Fabric management thread is dead\n");
	/* Close mport device */
	if (peer->mport_hnd != 0) {
		INFO("Closing mport\n");
		riomp_mgmt_mport_destroy_handle(&peer->mport_hnd);
	}
	INFO("Mport %d closed\n", peer->mport_id);

	rdma_log_close();
	exit(1);
} /* shutdown() */

void sig_handler(int sig)
{
	switch (sig) {

	case SIGQUIT:	/* ctrl-\ */
		puts("SIGQUIT - CTRL-\\ signal");
	break;

	case SIGINT:	/* ctrl-c */
		puts("SIGINT - CTRL-C signal");
	break;

	case SIGABRT:	/* abort() */
		puts("SIGABRT - abort() signal");
	break;

	case SIGTERM:	/* kill <pid> */
		puts("SIGTERM - kill <pid> signal");
	break;

	case SIGUSR1:	/* pthread_kill() */
	/* Ignore signal */
	return;

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
	}

	shutdown(&peer);
} /* sig_handler() */

bool foreground(void)
{
	return (tcgetpgrp(STDIN_FILENO) == getpgrp());
}

/**
 * Server for remote debug (remdbg) application.
 */
void *cli_session(void *arg)
{
	int sockfd;
	int portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	int one = 1;
	int session_num = 0;

	/* Check for NULL */
	if (arg == NULL) {
		CRIT("Argument is NULL. Exiting\n");
		pthread_exit(0);
	}

	/* TCP port number */
	portno = *((int *)arg);

	/* Create listen socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		CRIT("ERROR opening socket. Exiting\n");
		pthread_exit(0);
	}

	/* Prepare the family, address, and port */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(portno);

	/* Enable reuse of addresses as long as there is no active accept() */
	setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

	/* For socket to send data in buffer right away */
	setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));

	/* Bind socket to address */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		CRIT("ERROR on binding. Exiting\n");
		close(sockfd);
		pthread_exit(0);
	}

	INFO("RDMAD bound to socket on port number %d\n", portno);

	while (strncmp(buffer, "done", 4)) {
		struct cli_env env;

		/* Initialize the environment */
		env.script = NULL;
		env.fout = NULL;
		bzero(env.output, BUFLEN);
		bzero(env.input, BUFLEN);
		env.DebugLevel = 0;
		env.progressState = 0;
		env.sess_socket = -1;

		/* Set the prompt for the CLI */
		bzero(env.prompt, PROMPTLEN+1);
		strcpy(env.prompt, "RRDMAD> ");

		/* Prepare socket for listening */
		listen(sockfd,5);

		/* Accept connections from remdbg apps */
		clilen = sizeof(cli_addr);
		env.sess_socket = accept(sockfd,
				(struct sockaddr *) &cli_addr,
				&clilen);
		if (env.sess_socket < 0) {
			CRIT("ERROR on accept\n");
			close(sockfd);
			pthread_exit(0);
		}

		/* Start the session */
		INFO("\nStarting session %d\n", session_num);
		cli_terminal(&env);
		INFO("\nFinishing session %d\n", session_num);
		close(env.sess_socket);
		session_num++;
	}

	pthread_exit(0);
} /* cli_session() */

int main (int argc, char **argv)
{
	int c;
	int rc = 0;
	int cons_ret;

	/* Initialize peer_info struct with default values. This must be done
 	 * before parsing command line parameters as command line parameters
 	 * may override some of the default values assigned here */
	init_peer();

	/* Do no show console if started in background mode (rdmad &) */
	if (!foreground())
		peer.run_cons = 0;

	/* Register signal handler */
	struct sigaction sig_action;
	sig_action.sa_handler = sig_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGQUIT, &sig_action, NULL);
	sigaction(SIGABRT, &sig_action, NULL);
	sigaction(SIGUSR1, &sig_action, NULL);

	/* Parse command-line parameters */
	while ((c = getopt(argc, argv, "hnc:m:")) != -1)
		switch (c) {
		case 'c':
			peer.cons_skt = atoi(optarg);
		break;
		case 'h':
			puts("rdmad -h -m<port> -c<socket num>|-n");
			puts("-h		Display this help message");
			puts("-m<mport>		Use specified master port number");
			puts("-c<sock num>	Use specified socket number for console");
			puts("-n		Do not display console (for background operation");
			exit(1);
		break;
		case 'm':
			peer.mport_id = atoi(optarg);
		break;
		case 'n':
			peer.run_cons = 0;
		break;
		case '?':
			/* Invalid command line option */
			exit(1);
		default:
			abort();	
		}

	/* Initialize logger */
	if (rdma_log_init("rdmad.log", 1)) {
		puts("Failed to initialize logging system");
		return 1;
	}

	/* Prepare and start console thread, if applicable */
	if (peer.run_cons) {
		cli_init_base(custom_quit);

		add_commands_to_cmd_db(rdmad_cmds_size()/sizeof(rdmad_cmds[0]),
				rdmad_cmds);
		liblog_bind_cli_cmds();
		splashScreen((char *)"RDMA Daemon Command Line Interface");
		cons_ret = pthread_create( &console_thread, NULL, 
				console, (void *)((char *)"RDMADaemon> "));
		if(cons_ret) {
			CRIT("Error cons_thread rc: %d\n", cons_ret);
			exit(EXIT_FAILURE);
		}
	}

	/* Open mport */
	rc = riomp_mgmt_mport_create_handle(peer.mport_id, 0, &peer.mport_hnd);
	if (rc < 0) {
		CRIT("Failed in riomp_mgmt_mport_create_handle(): %s\n", strerror(-rc));
	        rc = 1;
		goto out;
    	}

	/* Query device information, and store destid */
	struct riomp_mgmt_mport_properties prop;

	rc = riomp_mgmt_query(peer.mport_hnd, &prop);
	if (rc != 0) {
		CRIT("Failed in riodp_query_mport(): %s\n", strerror(errno));
		rc = 2;
		goto out_close_mport;
	}
	peer.destid = prop.hdid;
	INFO("mport(%d), destid = 0x%X\n",
				peer.mport_id, peer.destid);

	/* Create inbound space */
	try {
		the_inbound = new inbound(peer.mport_hnd,
				 2,		/* No. of windows */
				 4*1024*1024);	/* Size in MB */
	}
	catch(std::bad_alloc& e) {
		CRIT("Failed to allocate the_inbound\n");
		goto out_close_mport;
	}
	catch(inbound_exception& e) {
		CRIT("%s\n", e.err);
		goto out_free_inbound;
	}

	/* Initialize message queue attributes */
	init_mq_attribs();

	/* Initialize semaphores */
	if (sem_init(&peer.cm_wait_connect_sem, 0, 0) == -1) {
		CRIT("Failed to initialize cm_wait_connect_sem: %s\n",
							strerror(errno));
		goto out_free_inbound;
	}
	if (sem_init(&hello_daemon_info_list_sem, 0, 1) == -1) {
		CRIT("Failed to initialize hello_daemon_info_list_sem: %s\n",
							strerror(errno));
		goto out_free_inbound;
	}
	if (sem_init(&prov_daemon_info_list_sem, 0, 1) == -1) {
		CRIT("Failed to initialize prov_daemon_info_list_sem: %s\n",
							strerror(errno));
		goto out_free_inbound;
	}

	/* Create provisioning thread */
	rc = pthread_create(&prov_thread, NULL, prov_thread_f, &peer);
	if (rc) {
		CRIT("Failed to create prov_thread: %s\n", strerror(errno));
		rc = 7;
		shutdown(&peer);
		goto out_free_inbound;
	}

	/* Create fabric management thread */
	rc = start_fm_thread();
	if (rc) {
		CRIT("Failed to create prov_thread: %s\n", strerror(errno));
		rc = 8;
		shutdown(&peer);
		goto out_free_inbound;
	}

	/* Create remote CLI terminal thread */
	rc = pthread_create(&cli_session_thread, NULL, cli_session, (void*)(&peer.cons_skt));
	if(rc) {
		CRIT("Failed to create cli_session_thread: %s\n", strerror(errno));
		rc = 9;
		shutdown(&peer);
		goto out_free_inbound;
	}

	run_rpc_alternative();

	/* Never reached */

out_free_inbound:
	pthread_join(console_thread, NULL);
	delete the_inbound;

out_close_mport:
	pthread_join(console_thread, NULL);
	riomp_mgmt_mport_destroy_handle(&peer.mport_hnd);
out:
	pthread_join(console_thread, NULL);
	return rc;	
} /* main() */
