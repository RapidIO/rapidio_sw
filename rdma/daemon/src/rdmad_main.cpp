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

#include <pthread.h>
#include <rapidio_mport_dma.h>
#include <semaphore.h>
#include <signal.h>
#include <execinfo.h>

#include <memory>

#include "cm_sock.h"
#include "liblog.h"

#include "rdma_types.h"
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
#include "rdmad_tx_engine.h"
#include "rdmad_rx_engine.h"
#include "rdmad_msg_processor.h"
#include "rdmad_dispatch.h"

using std::shared_ptr;
using std::make_shared;

typedef vector<unix_rx_engine *>	rx_engines_list;
typedef vector<unix_tx_engine *>	tx_engines_list;

static tx_engines_list	tx_eng_list;
static rx_engines_list	rx_eng_list;

static thread *engine_monitoring_thread;
static sem_t  *engine_cleanup_sem;

struct peer_info peer(16, 0xFFFF, 0, 0, DEFAULT_PROV_CHANNEL, DEFAULT_PROV_MBOX_ID,
			DEFAULT_CONSOLE_SKT, DEFAULT_RUN_CONS);

/* Memory Space Owner data */
ms_owners owners;

/* Inbound space */
inbound *the_inbound;

/* Global flag for shutting down */
bool shutting_down = false;

static 	pthread_t console_thread;
static	pthread_t prov_thread;
static	pthread_t cli_session_thread;

static unix_server *server;

static unix_msg_processor	d2l_msg_proc;

struct lib_connections_ti
{
	lib_connections_ti(int accept_socket) : accept_socket(accept_socket), tid(0)
	{}
	int accept_socket;
	sem_t	started;
	pthread_t tid;
};

int send_disc_ms_cm(uint32_t server_destid,
		    uint32_t server_msid,
		    uint32_t client_msubid,
		    uint64_t client_to_lib_tx_eng_h)
{
	cm_client *the_client;
	int ret = 0;

	/* Do we have an entry for that destid ? */
	sem_wait(&hello_daemon_info_list_sem);
	auto it = find(begin(hello_daemon_info_list),
		       end(hello_daemon_info_list),
		       server_destid);

	/* If the server's destid is not found, just fail */
	if (it == end(hello_daemon_info_list)) {
		ERR("destid(0x%X) was not provisioned\n", server_destid);
		ret = RDMA_REMOTE_UNREACHABLE;
	} else {
		/* Obtain pointer to socket object connected to destid */
		the_client = it->client;
	}
	sem_post(&hello_daemon_info_list_sem);

	if (ret == 0) {
		cm_disconnect_req_msg *disc_msg;

		/* Get and flush send buffer */
		the_client->flush_send_buffer();
		the_client->get_send_buffer((void **)&disc_msg);

		disc_msg->type		    = htobe64(CM_DISCONNECT_MS_REQ);
		disc_msg->client_msubid	    = htobe64(client_msubid);
		disc_msg->client_destid     = htobe64(peer.destid);
		disc_msg->client_destid_len = htobe64(16);
		disc_msg->client_to_lib_tx_eng_h = htobe64(client_to_lib_tx_eng_h);
		disc_msg->server_msid       = htobe64(server_msid);

		/* Send buffer to server */
		if (the_client->send()) {
			ret = -1;
		} else {
			DBG("Sent DISCONNECT_MS for msid(0x%X) @ destid(0x%X)\n",
					server_msid,
					server_destid);
		}
	}

	return ret;
} /* send_disc_ms_cm() */

void engine_monitoring_thread_f(sem_t *engine_cleanup_sem)
{
	while (1) {
		/* Wait until there is a reason to perform cleanup */
		sem_wait(engine_cleanup_sem);

		/* If some engine is being killed then the app using
		 * that engine may own memory spaces. The remote users
		 * of such memory space (if any) need to be sent a
		 * 'force disconnect' for proper cleanup.
		 */

		HIGH("Cleaning up dead engines!\n");
		/* Check the rx_eng_list for dead engines */
		for (auto it = begin(rx_eng_list); it != end(rx_eng_list); it++) {
			if ((*it)->isdead() || shutting_down) {
				/* Delete rx_engine and set pointer to null */
				HIGH("Cleaning up an rx_engine\n");
				delete *it;
				*it = nullptr;
			}
		}

		/* Remove all null Rx engine entries */
		DBG("rx_eng_list.size() = %u\n", rx_eng_list.size());
		rx_eng_list.erase( std::remove(begin(rx_eng_list),
				               end(rx_eng_list),
				               nullptr),
					       end(rx_eng_list));
		DBG("rx_eng_list.size() = %u\n", rx_eng_list.size());

		/* Check the tx_eng_list for dead engines */
		for (auto it = begin(tx_eng_list); it != end(tx_eng_list); it++) {
			if ((*it)->isdead() || shutting_down) {

				/* If the tx_eng is being used by a client app, then
				 * we must clear the connected_to_ms_info_list  */
				DBG("Cleaning up connected_to_ms_info_list\n");
				sem_wait(&connected_to_ms_info_list_sem);
				connected_to_ms_info_list.erase(
					remove(begin(connected_to_ms_info_list),
					       end(connected_to_ms_info_list),
					       *it),
					end(connected_to_ms_info_list));
				sem_post(&connected_to_ms_info_list_sem);

				/* If the tx_eng is being used by a server app,
				 * then we must clear all related memory spaces
				 *  and owners using that tx_eng. */
				DBG("Cleaning up memory spaces & owners...\n");
				the_inbound->close_and_destroy_mspaces_using_tx_eng(*it);
				owners.close_mso(*it);
				owners.destroy_mso(*it);

				HIGH("Destroying Tx engine\n");
				delete *it;
				*it = nullptr;
			}
		}

		/* Remove all null Tx engine entries */
		DBG("tx_eng_list.size() = %u\n", tx_eng_list.size());
		tx_eng_list.erase(remove(begin(tx_eng_list),
				         end(tx_eng_list),
				         nullptr),
				  end(tx_eng_list));
		DBG("tx_eng_list.size() = %u\n", tx_eng_list.size());
	} /* while */
} /* engine_monitoring_thread_f() */

int start_accepting_connections()
{
	/* Create a server */
	DBG("Creating Unix socket server object...\n");
	try {
		server = new unix_server("main_server");
	}
	catch(unix_sock_exception& e) {
		CRIT("Failed to create server: %s. Aborting.\n",  e.what());
		abort();
	}

	while (1) {
		try {
			/* Wait for client to connect */
			HIGH("Waiting for (another) RDMA application to connect..\n");
			if (server->accept()) {
				CRIT("Failed to accept connections.\n");
				throw unix_sock_exception("Failed in server->accept()");
			}
			HIGH("Application connected!\n");

			/* Create other server for handling connection with app */
			auto accept_socket = server->get_accept_socket();
			auto other_server = make_shared<unix_server>("other_server",
								accept_socket);

			/* Engine cleanup semaphore. Posted by engines that die
			 * so we can clean up after them. */
			engine_cleanup_sem = new sem_t();
			sem_init(engine_cleanup_sem, 0, 0);

			/* Create Tx and Rx engine per connection */
			unix_rx_engine *rx_eng;
			unix_tx_engine *tx_eng;	// FIXME: one for all?

			tx_eng = new unix_tx_engine(other_server,
							  engine_cleanup_sem);
			rx_eng = new unix_rx_engine(other_server,
							  d2l_msg_proc,
							  tx_eng,
							  engine_cleanup_sem);

			/* We passed 'other_server' to both engines. Now
			 * relinquish ownership. They own it. Together.	 */
			other_server.reset();

			/* Store engines in list for later cleanup */
			rx_eng_list.push_back(rx_eng);
			tx_eng_list.push_back(tx_eng);

			/* Start engine monitoring thread */
			engine_monitoring_thread =
					new thread(engine_monitoring_thread_f,
						   engine_cleanup_sem);
		}
		catch(exception& e) {
			CRIT("Fatal error: %s. Aborting daemon!\n", e.what());
			delete server;
			abort();
		}
	} /* while */
} /* start_accepting_connections() */

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

	/* Kill Tx/Rx engines */
	sem_post(engine_cleanup_sem);

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

	case SIGSEGV:
	{
		puts("SIGSEGV (Segmentation Fault)");
		constexpr unsigned MAX_BT = 100;
		void *buffer[MAX_BT];
		size_t count = backtrace(buffer, MAX_BT);
		backtrace_symbols_fd(buffer, count, STDERR_FILENO);
	}
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

int main (int argc, char **argv)
{
	int c;
	int rc = 0;
	int cons_ret;

	/* Do no show console if started in background mode (rdmad &) */
	if (!foreground())
		peer.run_cons = 0;

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
			abort(); /* Unexpected error */
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
			CRIT("Failed to start console, rc: %d\n", cons_ret);
			exit(EXIT_FAILURE);
		}
	}

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
	sigaction(SIGSEGV, &sig_action, NULL);

	/* Open mport */
	rc = riomp_mgmt_mport_create_handle(peer.mport_id, 0, &peer.mport_hnd);
	if (rc < 0) {
		CRIT("Failed in riomp_mgmt_mport_create_handle(): %s\n", strerror(-rc));
	        rc = 1;
		goto out;
    	}
	DBG("peer.mport_hnd = 0x%X\n", peer.mport_hnd);

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
		CRIT("%s\n", e.what());
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
	if (sem_init(&connected_to_ms_info_list_sem, 0, 1) == -1) {
		CRIT("Failed to initialize connected_to_ms_info_list_sem: %s\n",
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
		CRIT("Failed to create fm_thread: %s\n", strerror(errno));
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

	/* Start accepting connections from apps linked with LIBRDMA */
	start_accepting_connections();

	/* Never reached */

out_free_inbound:
	delete the_inbound;

out_close_mport:
	riomp_mgmt_mport_destroy_handle(&peer.mport_hnd);
out:
	pthread_join(console_thread, NULL);
	return rc;	
} /* main() */
