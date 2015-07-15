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
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include "riodp_mport_lib.h"
#include "cm_sock.h"
#include "rdma_mq_msg.h"
#include "liblog.h"

#include "rdmad_peer_utils.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_svc.h"
#include "rdmad_console.h"
#include "rdmad_fm.h"
#include "libcli.h"
#include "unix_sock.h"
#include "rdmad_unix_msg.h"

struct peer_info	peer;

using namespace std;

static 	pthread_t console_thread;
static	pthread_t prov_thread;

static unix_server *server;

static void init_peer()
{
	peer.destid = 0xFFFF;
	peer.destid_len = 16;

	/* MPORT */
	peer.mport_id = 0;	/* Overriden on the command line */
	peer.mport_fd = -1;

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

	INFO("Creating other server object...\n");
	unix_server *other_server;
	try {
		other_server = new unix_server("other_server", ti->accept_socket);
	}
	catch(unix_sock_exception e) {
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
					DBG("GET_MPORT_ID done\n");
				}
				break;

				case CREATE_MSO:
				{
					DBG("CREATE_MSO\n");
					create_mso_input *in = &in_msg->create_mso_in;
					create_mso_output *out = &out_msg->create_mso_out;
					out_msg->type = CREATE_MSO_ACK;

					int ret = owners.create_mso(in->owner_name, &out->msoid);
					out->status = (ret > 0) ? 0 : ret;
					DBG("CREATE_MSO done\n");
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
							&out->mso_conn_id);
					out->status = (ret > 0) ? 0 : ret;
					DBG("OPEN_MSO done\n");
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
					DBG("CLOSE_MSO done\n");
				}
				break;

				case DESTROY_MSO:
				{
					DBG("DESTROY_MSO\n");
					destroy_mso_input *in = &in_msg->destroy_mso_in;
					destroy_mso_output *out = &out_msg->destroy_mso_out;
					out_msg->type = DESTROY_MSO_ACK;

					DBG("in->msoid = 0x%X\n", in->msoid);
					ms_owner *owner;

					try {
						owner = owners[in->msoid];
						/* Check if the memory space owner
						 * still owns memory spaces */
						if (owner->owns_mspaces()) {
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
					DBG("DESTROY_MSO done\n");
				}
				break;

				case CREATE_MS:
				{
					DBG("CREATE_MS\n");
					create_ms_input *in = &in_msg->create_ms_in;
					create_ms_output *out = &out_msg->create_ms_out;
					out_msg->type = CREATE_MS_ACK;

					/* Create memory space in the inbound space */
					int ret = the_inbound->create_mspace(
							in->ms_name,
							in->bytes, in->msoid,
							&out->msid);
					out->status = (ret > 0) ? 0 : ret;
					DBG("the_inbound->create_mspace(%s) %s\n",
						in->ms_name,
						out->status ? "FAILED" : "PASSED");


					if (!out->status)
						/* Add the memory space handle to owner */
						owners[in->msoid]->add_msid(out->msid);
					DBG("CREATE_MS done\n");
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
					DBG("OPEN_MS done\n");
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
						break;
					}

					/* Now close the memory space */
					int ret = ms->close(in->ms_conn_id);
					out->status = (ret > 0) ? 0 : ret;
					DBG("CLOSE_MS done\n");
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
						break;
					}

					/* Now destroy the memory space */
					out->status = ms->destroy();
					DBG("DESTROY_MS done\n");
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
					DBG("CREATE_MSUB done\n");
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

					DBG("DESTROY_MSUB done\n");
				}
				break;

				case ACCEPT_MS:
				{
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
					cmam.type		= CM_ACCEPT_MS;
					strcpy(cmam.server_ms_name, in->loc_ms_name);
					cmam.server_msid	= ms->get_msid();
					cmam.server_msubid	= in->loc_msubid;
					cmam.server_bytes	= in->loc_bytes;
					cmam.server_rio_addr_len= in->loc_rio_addr_len;
					cmam.server_rio_addr_lo	= in->loc_rio_addr_lo;
					cmam.server_rio_addr_hi	= in->loc_rio_addr_hi;
					cmam.server_destid_len	= 16;
					cmam.server_destid	= peer.destid;
					DBG("cm_accept_msg has server_destid = 0x%X\n",
							cmam.server_destid);
					DBG("cm_accept_msg has server_destid_len = 0x%X\n",
							cmam.server_destid_len);

					/* Add accept message content to map indexed by message queue name */
					DBG("Adding entry in accept_msg_map for '%s'\n", s.c_str());
					accept_msg_map.add(s, cmam);

					out->status = 0;
				}
				break;

				case UNDO_ACCEPT:
				{
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
					c->type			= CM_CONNECT_MS;
					strcpy(c->server_msname, in->server_msname);
					c->client_msid		= in->client_msid;
					c->client_msubid	= in->client_msubid;
					c->client_bytes		= in->client_bytes;
					c->client_rio_addr_len	= in->client_rio_addr_len;
					c->client_rio_addr_lo	= in->client_rio_addr_lo;
					c->client_rio_addr_hi	= in->client_rio_addr_hi;
					c->client_destid_len	= peer.destid_len;
					c->client_destid	= peer.destid;

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

					/* Obtain pointer to socket object already connected to destid */
					cm_client *the_client = it->client;

					cm_disconnect_msg *disc_msg;

					/* Get and flush send buffer */
					the_client->flush_send_buffer();
					the_client->get_send_buffer((void **)&disc_msg);

					disc_msg->type		= CM_DISCONNECT_MS;
					disc_msg->client_msubid	= in->loc_msubid;	/* For removal from server database */
					disc_msg->server_msid    = in->rem_msid;	/* For removing client's destid from server's
												 * info on the daemon */
					disc_msg->client_destid = peer.destid;		/* For knowing which destid to remove */
					disc_msg->client_destid_len = 16;

					/* Send buffer to server */
					if (the_client->send()) {
						out->status = -1;
						break;
					}
					DBG("Sent DISCONNECT_MS for msid = 0x%lX, client_destid = 0x%lX\n",
						disc_msg->server_msid, disc_msg->client_destid);

					out->status = 0;
				}
				break;

				default:
					ERR("UNKNOWN MESSAGE TYPE: 0x%X\n", in_msg->type);
			} /* switch */

			if (other_server->send(sizeof(unix_msg_t))) {
				CRIT("Failed to send API output parameters back to library\n");
				delete other_server;
				pthread_exit(0);
			} else {
				DBG("API processing completed!\n");
			}
		} else {
			HIGH("RDMA library has closed connection!\n");
			pthread_exit(0);
		}
	} /* while */
	pthread_exit(0);
}

int run_rpc_alternative()
{
	/* Create a server */
	DBG("Creating server object...\n");
	try {
		server = new unix_server();
	}
	catch(unix_sock_exception e) {
		cout << e.err << endl;
		return 1;
	}

	/* Wait for client to connect */
	DBG("Wait for client to connect..\n");

	while (1) {
		if (server->accept()) {
			CRIT("Failed to accept\n");
			delete server;
			return 2;
		}

		int accept_socket = server->get_accept_socket();
		DBG("After accept() call, accept_socket = 0x%X\n", accept_socket);

		rpc_ti	*ti;
		try {
			ti = new rpc_ti(accept_socket);
		}
		catch(...) {
			CRIT("Failed to create rpc_ti\n");
			delete server;
			return 3;
		}

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
		sem_wait(&ti->started);
	} /* while */
} /* run_rpc_alternative() */

void shutdown(struct peer_info *peer)
{
	/* Kill the threads */
	shutting_down = true;

	/* Delete the inbound object */
	INFO("Deleting the_inbound\n");
	delete the_inbound;

	/* Next, kill provisioning thread */
	int ret = pthread_kill(prov_thread, SIGUSR1);
	if (ret == EINVAL) {
		CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
	}
	pthread_join(prov_thread, NULL);

	/* Kill the fabric management thread */
	halt_fm_thread();

	/* Kill threads for remote daemons provisioned via incoming HELLO */
	for (auto it = begin(prov_daemon_info_list);
	    it != end(prov_daemon_info_list);
	    it++) {
		pthread_kill(it->tid, SIGUSR1);
		if (ret == EINVAL) {
			CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
		}
		pthread_join(it->tid, NULL);
	}

	/* Kill threads for remote daemons provisioned via outgoing HELLO */
	for (auto it = begin(hello_daemon_info_list);
	    it != end(hello_daemon_info_list);
	    it++) {
		pthread_kill(it->tid, SIGUSR1);
		if (ret == EINVAL) {
			CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
		}
		pthread_join(it->tid, NULL);
	}


	/* Close mport device */
	if (peer->mport_fd > 0) {
		INFO("Closing mport fd\n");
		close(peer->mport_fd);
	}
	INFO("Mport %d closed\n", peer->mport_id);

	rdma_log_close();
	exit(1);
} /* shutdown() */

void sig_handler(int sig)
{
	switch (sig) {
	case SIGQUIT:	/* ctrl-\ */
	case SIGINT:	/* ctrl-c */
	case SIGABRT:	/* abort() */
	case SIGTERM:	/* kill <pid> */
	break;

	case SIGUSR1:	/* pthread_kill() */
		/* Ignore signal */
		return;
	break;

	default:
		puts("UNKNOWN SIGNAL");
	}

	owners.dump_info();
	the_inbound->dump_info();
	the_inbound->dump_all_mspace_with_msubs_info();
	shutdown(&peer);
} /* end_handler() */

bool foreground(void)
{
	return (tcgetpgrp(STDIN_FILENO) == getpgrp());
}

int main (int argc, char **argv)
{
	int c;
	int rc = 0;
	int cons_ret;

	/* Initialize peer_info struct with defautl values. This must be done
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
	peer.mport_fd = riodp_mport_open(peer.mport_id, 0);
	if (peer.mport_fd <= 0) {
		CRIT("Failed in riodp_mport_open(): %s\n", strerror(errno));
	        rc = 1;
		goto out;
    	}

	/* Query device information, and store destid */
	struct rio_mport_properties prop;

	rc = riodp_query_mport(peer.mport_fd, &prop);
	if (rc != 0) {
		CRIT("Failed in riodp_query_mport(): %s\n", strerror(errno));
		rc = 2;
		goto out_close_mport;
	}
	peer.destid = prop.hdid;
	INFO("mport(%d), destid = 0x%X, fd = %d\n",
				peer.mport_id, peer.destid, peer.mport_fd);

	/* Create inbound space */
	try {
		the_inbound = new inbound(peer.mport_fd,
				 2,		/* No. of windows */
				 4*1024*1024);	/* Size in MB */
	}
	catch(std::bad_alloc e) {
		CRIT("Failed to allocate the_inbound\n");
		goto out_close_mport;
	}
	catch(inbound_exception e) {
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

	run_rpc_alternative();

	/* Never reached */

out_free_inbound:
	pthread_join(console_thread, NULL);
	delete the_inbound;

out_close_mport:
	pthread_join(console_thread, NULL);
	close(peer.mport_fd);
out:
	pthread_join(console_thread, NULL);
	return rc;	
} /* main() */
