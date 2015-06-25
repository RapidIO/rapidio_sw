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
#include "rdmad.h"

struct peer_info	peer;

using namespace std;

cm_server *main_server;
cm_server *aux_server;

static	pthread_t cm_accept_thread;
static 	pthread_t console_thread;
static	pthread_t server_wait_disc_thread;
static	pthread_t client_wait_destroy_thread;
static	pthread_t prov_thread;

static void init_peer()
{
	peer.destid = 0xFFFF;
	peer.destid_len = 16;

	/* MPORT */
	peer.mport_id = 0;	/* Overriden on the command line */
	peer.mport_fd = -1;

	/* Messaging */
	peer.loc_channel	= DEFAULT_LOC_CHANNEL;
	peer.aux_channel	= DEFAULT_AUX_CHANNEL;
	peer.destroy_channel	= DEFAULT_DESTROY_CHANNEL;
	peer.prov_channel	= DEFAULT_PROV_CHANNEL;

	peer.mbox_id		= DEFAULT_MAILBOX_ID;
	peer.aux_mbox_id	= DEFAULT_AUX_MAILBOX_ID;
	peer.destroy_mbox_id	= DEFAULT_DESTROY_MAILBOX_ID;
	peer.prov_mbox_id	= DEFAULT_PROV_MBOX_ID;

	/* CLI */
	peer.cons_skt = 4444;
	peer.run_cons = 1;
}

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

void shutdown(struct peer_info *peer)
{
	/* Kill the threads */
	shutting_down = true;

	/* Wake up the accept_thread_f hread if necessary */
	sem_post(&peer->cm_wait_connect_sem);

	/* If the thread is still alive (because it is in a CM accept,
	 * then kill it. */
	int ret = pthread_kill(cm_accept_thread, SIGUSR1);
	if (ret == EINVAL) {
		CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
	} else if (ret == ESRCH) {
		WARN("It is possible that cm_accept_thread already killed\n");
	}
	pthread_join(cm_accept_thread, NULL);

	/* Next, kill server_wait_disc_thread */
	ret = pthread_kill(server_wait_disc_thread, SIGUSR1);
	if (ret == EINVAL) {
		CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
	}
	pthread_join(server_wait_disc_thread, NULL);

	/* Next, kill client_wait_destroy_thread */
	ret = pthread_kill(client_wait_destroy_thread, SIGUSR1);
	if (ret == EINVAL) {
		CRIT("Invalid signal specified 'SIGUSR1' for pthread_kill\n");
	}
	pthread_join(client_wait_destroy_thread, NULL);

	/* Next, kill provisioning thread */
	ret = pthread_kill(prov_thread, SIGUSR1);
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

	/* Delete messaging objects */
	INFO("Deleting main_server\n");
	delete main_server;

	INFO("Deleting aux_server\n");
	delete aux_server;

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

	/* Configure RPC as a listener */
	configure_rpc();

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

	/* Initialize messaging */
	try {
		INFO("Create main_server\n");
		main_server = new cm_server("main_server",
					    peer.mport_id,
					    peer.mbox_id,
					    peer.loc_channel);
	}
	catch(cm_exception e) {
		CRIT("main_server: %s\n", e.err);
		goto out_free_inbound;
	}
	try {
		INFO("Create aux_server\n");
		aux_server = new cm_server("aux_server",
					   peer.mport_id,
					   peer.aux_mbox_id,
					   peer.aux_channel);
	}
	catch(cm_exception e) {
		CRIT("aux_server: %s\n", e.err);
		goto out_free_main_server;
	}

	/* Create threads */
	rc = pthread_create(&cm_accept_thread, NULL, accept_thread_f, NULL);
	if (rc) {
		CRIT("Failed to create cm_accept_thread: %s\n", strerror(errno));
		rc = 5;
		shutdown(&peer);
		goto out_free_aux_server;
	}
	rc = pthread_create(&server_wait_disc_thread,
					NULL, server_wait_disc_thread_f, NULL);
	if (rc) {
		CRIT("Failed to create server_wait_disc_thread: %s\n",
								strerror(errno));
		rc = 7;
		shutdown(&peer);
		goto out_free_aux_server;
	}

	rc = pthread_create(&client_wait_destroy_thread,
				NULL, client_wait_destroy_thread_f, NULL);
	if (rc) {
		CRIT("Failed to create client_wait_destroy_thread: %s\n",
								strerror(errno));
		rc = 7;
		shutdown(&peer);
		goto out_free_aux_server;
	}

	/* Create provisioning thread */
	rc = pthread_create(&prov_thread, NULL, prov_thread_f, &peer);
	if (rc) {
		CRIT("Failed to create prov_thread: %s\n", strerror(errno));
		rc = 7;
		shutdown(&peer);
		goto out_free_aux_server;
	}

	run_rpc();

	/* Never reached */

out_free_aux_server:
	pthread_join(console_thread, NULL);
	delete aux_server;

out_free_main_server:
	pthread_join(console_thread, NULL);
	delete main_server;

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
