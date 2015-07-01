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
#include <rpc/pmap_clnt.h>
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
#include "libcli.h"
#if 0
#include "rdmad.h"
#endif
#include "unix_sock.h"
#include "rdmad_unix_msg.h"

extern  get_mport_id_output * get_mport_id_1_svc(get_mport_id_input *in);
extern  create_mso_output * create_mso_1_svc(create_mso_input *in);
extern  open_mso_output * open_mso_1_svc(open_mso_input *in);
extern  close_mso_output * close_mso_1_svc(close_mso_input *in);
extern  destroy_mso_output * destroy_mso_1_svc(destroy_mso_input *in);
extern  create_ms_output * create_ms_1_svc(create_ms_input *in);
extern  open_ms_output * open_ms_1_svc(open_ms_input *in);
extern  close_ms_output * close_ms_1_svc(close_ms_input *in);
extern  destroy_ms_output * destroy_ms_1_svc(destroy_ms_input *in);
extern  create_msub_output * create_msub_1_svc(create_msub_input *in);
extern  destroy_msub_output * destroy_msub_1_svc(destroy_msub_input *in);
extern  accept_output * accept_1_svc(accept_input *in);
extern  undo_accept_output * undo_accept_1_svc(undo_accept_input *in);
extern  send_connect_output * send_connect_1_svc(send_connect_input *in);
extern  undo_connect_output * undo_connect_1_svc(undo_connect_input *in);
extern  send_disconnect_output * send_disconnect_1_svc(send_disconnect_input *in);

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
#if 0
void
rdmad_1(struct svc_req *rqstp, register SVCXPRT *transp);

void configure_rpc()
{
	register SVCXPRT *transp;

	pmap_unset (RDMAD, RDMAD_1);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		CRIT("Cannot create UDP RPC service.");
		exit(1);
	}
	if (!svc_register(transp, RDMAD, RDMAD_1, rdmad_1, IPPROTO_UDP)) {
		CRIT("Unable to register (RDMAD, RDMAD_1, UDP).\n");
		CRIT("Make sure you have run 'sudo rpcbind'.\n");
		CRIT("Also make sure you are running this application as 'sudo'.\n");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		CRIT("Cannot create TCP RPC service.");
		exit(1);
	}
	if (!svc_register(transp, RDMAD, RDMAD_1, rdmad_1, IPPROTO_TCP)) {
		CRIT("Unable to register (RDMAD, RDMAD_1, TCP).");
		exit(1);
	}
} /* configure_rpc() */

void run_rpc()
{
	INFO("Running svc_run...\n");

	svc_run ();

	/* NOTREACHED */

	CRIT("svc_run returned\n");
	exit (1);
} /* run_rpc() */
#endif
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
		pthread_exit(0);
	}

	rpc_ti *ti = (rpc_ti *)arg;

	puts("Creating other server object...");
	unix_server *other_server;
	try {
		other_server = new unix_server("other_server", ti->accept_socket);
	}
	catch(unix_sock_exception e) {
		cout << e.err << endl;
		sem_post(&ti->started);
		pthread_exit(0);
	}

	sem_post(&ti->started);

	while (1) {
		/* Wait for data from clients */
		puts("Waiting to receive from client...");
		size_t	received_len = 0;	/* For build warning */
		if (other_server->receive(&received_len)) {
			puts("Failed to receive");
			delete other_server;
			pthread_exit(0);
		}
		if (received_len > 0) {
			printf("receveived_len = %d\n", (int)received_len);

			unix_msg_t	*in_msg;
			unix_msg_t	*out_msg;

			other_server->get_recv_buffer((void **)&in_msg);
			other_server->get_send_buffer((void **)&out_msg);

			switch(in_msg->type) {
				case GET_MPORT_ID:
				{
					get_mport_id_input in = in_msg->get_mport_id_in;
					get_mport_id_output *out;
					out = get_mport_id_1_svc(&in);
					out_msg->get_mport_id_out = *out;
					out_msg->type = GET_MPORT_ID_ACK;
					delete out;
				}
				break;

				case CREATE_MSO:
				{
					create_mso_input in = in_msg->create_mso_in;
					create_mso_output *out;
					out = create_mso_1_svc(&in);
					out_msg->create_mso_out = *out;
					out_msg->type = CREATE_MSO_ACK;
					delete out;
				}
				break;

				case OPEN_MSO:
				{
					open_mso_input	in = in_msg->open_mso_in;
					open_mso_output *out;
					out = open_mso_1_svc(&in);
					out_msg->open_mso_out = *out;
					out_msg->type = OPEN_MSO_ACK;
					delete out;
				}
				break;

				case CLOSE_MSO:
				{
					close_mso_input in = in_msg->close_mso_in;
					close_mso_output *out;
					out = close_mso_1_svc(&in);
					out_msg->close_mso_out = *out;
					out_msg->type = CLOSE_MSO_ACK;
					delete out;
				}
				break;

				case DESTROY_MSO:
				{
					destroy_mso_input in = in_msg->destroy_mso_in;
					destroy_mso_output *out;
					out = destroy_mso_1_svc(&in);
					out_msg->destroy_mso_out = *out;
					out_msg->type = DESTROY_MSO_ACK;
					delete out;
				}
				break;

				case CREATE_MS:
				{
					create_ms_input in = in_msg->create_ms_in;
					create_ms_output *out;
					out = create_ms_1_svc(&in);
					out_msg->create_ms_out = *out;
					out_msg->type = CREATE_MS_ACK;
					delete out;
				}
				break;

				case OPEN_MS:
				{
					open_ms_input in = in_msg->open_ms_in;
					open_ms_output *out;
					out = open_ms_1_svc(&in);
					out_msg->open_ms_out = *out;
					out_msg->type = OPEN_MS_ACK;
					delete out;
				}
				break;

				case CLOSE_MS:
				{
					close_ms_input in = in_msg->close_ms_in;
					close_ms_output *out;
					out = close_ms_1_svc(&in);
					out_msg->close_ms_out = *out;
					out_msg->type = CLOSE_MS_ACK;
					delete out;
				}
				break;

				case DESTROY_MS:
				{
					destroy_ms_input in = in_msg->destroy_ms_in;
					destroy_ms_output *out;
					out = destroy_ms_1_svc(&in);
					out_msg->destroy_ms_out = *out;
					out_msg->type = DESTROY_MS_ACK;
					delete out;
				}
				break;

				case CREATE_MSUB:
				{
					create_msub_input in = in_msg->create_msub_in;
					create_msub_output *out;
					out = create_msub_1_svc(&in);
					out_msg->create_msub_out = *out;
					out_msg->type = CREATE_MSUB_ACK;
					delete out;
				}
				break;

				case DESTROY_MSUB:
				{
					destroy_msub_input in = in_msg->destroy_msub_in;
					destroy_msub_output *out;
					out = destroy_msub_1_svc(&in);
					out_msg->destroy_msub_out = *out;
					out_msg->type = DESTROY_MSUB_ACK;
					delete out;
				}
				break;

				case ACCEPT_MS:
				{
					accept_input in = in_msg->accept_in;
					accept_output *out;
					out = accept_1_svc(&in);
					out_msg->accept_out = *out;
					out_msg->type = ACCEPT_MS_ACK;
					delete out;
				}
				break;

				case UNDO_ACCEPT:
				{
					undo_accept_input in = in_msg->undo_accept_in;
					undo_accept_output *out;
					out = undo_accept_1_svc(&in);
					out_msg->undo_accept_out = *out;
					out_msg->type = UNDO_ACCEPT_ACK;
					delete out;
				}
				break;

				case SEND_CONNECT:
				{
					send_connect_input in = in_msg->send_connect_in;
					send_connect_output *out;
					out = send_connect_1_svc(&in);
					out_msg->send_connect_out = *out;
					out_msg->type = SEND_CONNECT_ACK;
					delete out;
				}
				break;

				case UNDO_CONNECT:
				{
					undo_connect_input in = in_msg->undo_connect_in;
					undo_connect_output *out;
					out = undo_connect_1_svc(&in);
					out_msg->undo_connect_out = *out;
					out_msg->type = UNDO_CONNECT_ACK;
					delete out;
				}
				break;

				case SEND_DISCONNECT:
				{
					send_disconnect_input in = in_msg->send_connect_in;
					send_disconnect_output *out;
					out = send_disconnect_1_svc(&in);
					out_msg->send_connect_out = *out;
					out_msg->type = SEND_DISCONNECT_ACK;
					delete out;
				}
				break;
			} /* switch */

			if (other_server->send(sizeof(unix_msg_t))) {
				puts("Failed to send back");
				delete other_server;
				pthread_exit(0);
			}
		} else {
			puts("Remote daemon has closed connection..BYTE");
			pthread_exit(0);
		}
	} /* while */
	pthread_exit(0);
}

int run_rpc_alternative()
{
	/* Create a server */
	puts("Creating server object...");
	try {
		server = new unix_server();
	}
	catch(unix_sock_exception e) {
		cout << e.err << endl;
		return 1;
	}

	/* Wait for client to connect */
	puts("Wait for client to connect..");

	while (1) {
		if (server->accept()) {
			puts("Failed to accept");
			delete server;
			return 2;
		}

		int accept_socket = server->get_accept_socket();
		printf("After accept() call, accept_socket = 0x%X\n", accept_socket);

		rpc_ti	*ti;
		try {
			ti = new rpc_ti(accept_socket);
		}
		catch(...) {
			puts("Failed to create rpc_ti");
			delete server;
			return 3;
		}

		int ret = pthread_create(&ti->tid,
					 NULL,
					 rpc_thread_f,
					 ti);
		if (ret) {
			puts("Failed to create request thread\n");
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

	/* Next, kill provisioning thread */
	int ret = pthread_kill(prov_thread, SIGUSR1);
	if (ret == EINVAL) {
		CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
	}
	pthread_join(prov_thread, NULL);

	/* Post the semaphore of each of the client remote daemon threads
	 * if any. This causes the threads to see 'shutting_down' has
	 * been set and they self-exit */
	rdaemon_sem_post	rsp;
	for_each(begin(client_rdaemon_list), end(client_rdaemon_list), rsp);

	/* Delete the inbound object */
	INFO("Deleting the_inbound\n");
	delete the_inbound;

	/* Close mport device */
	if (peer->mport_fd > 0) {
		INFO("Closing mport fd\n");
		close(peer->mport_fd);
	}
	INFO("Mport %d closed\n", peer->mport_id);

	rdma_log_close();
	exit(1);
} /* shutdown() */

void end_handler(int sig)
{
	switch (sig) {
	case SIGQUIT:
		puts("SIGQUIT");
	break;
	case SIGINT:
		puts("SIGINT");
	break;
	case SIGABRT:
		puts("SIGABRT");
	break;
	case SIGUSR1:
		puts("SIGUSR1");
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

int main (int argc, char **argv)
{
	int c;
	int rc = 0;
	int cons_ret;

	/* Initialize peer_info struct with defautl values. This must be done
 	 * before parsing command line parameters as command line parameters
 	 * may override some of the default values assigned here */
	init_peer();

	/* Register end handler */
	signal(SIGQUIT, end_handler);
	signal(SIGINT, end_handler);
	signal(SIGABRT, end_handler);
	signal(SIGUSR1, end_handler);

	/* Parse command-line parameters */
	while ((c = getopt(argc, argv, "hnc:m:")) != -1)
		switch (c) {
		case 'c':
			peer.cons_skt = atoi(optarg);
		break;
		case 'h':
			puts("rdmad -h -m<port> -c<socket num>");
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
#if 0
	/* Configure RPC as a listener */
	configure_rpc();
#endif
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
	if (sem_init(&client_rdaemon_list_sem, 0, 1) == -1) {
		CRIT("Failed to initialize client_rdaemon_list_sem: %s\n",
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

	run_rpc_alternative();
#if 0
	run_rpc();
#endif
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
