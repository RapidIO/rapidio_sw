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
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <execinfo.h>

#include <memory>
#include <thread>
#include "memory_supp.h"

#include "liblog.h"
#include "rapidio_mport_dma.h"
#include "unix_sock.h"

#include "rdmad_inbound.h"
#include "rdmad_ms_owners.h"
#include "rdmad_peer_utils.h"
#include "daemon_info.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_console.h"
#include "rdmad_fm.h"
#include "rdmad_unix_msg.h"
#include "rdmad_tx_engine.h"
#include "rdmad_rx_engine.h"
#include "rdmad_msg_processor.h"

using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::thread;

using rx_engines_list = vector<unique_ptr<unix_rx_engine>>;
using tx_engines_list = vector<unique_ptr<unix_tx_engine>>;

/* Inbound space */
inbound *the_inbound;

/* Global flag for shutting down */
bool shutting_down = false;

static tx_engines_list	unix_tx_eng_list;
static rx_engines_list	unix_rx_eng_list;

static unique_ptr<thread> unix_engine_monitoring_thread;
static sem_t  *unix_engine_cleanup_sem = nullptr;

static peer_info peer(16, 0xFFFF, 0, 0, DEFAULT_PROV_CHANNEL,
		DEFAULT_PROV_MBOX_ID, DEFAULT_CONSOLE_SKT, DEFAULT_RUN_CONS);

/* Memory Space Owner data */
static ms_owners owners;

/* POSIX Threads */
static 	pthread_t console_thread;
static	pthread_t prov_thread;
static	pthread_t cli_session_thread;

static unique_ptr<unix_server> app_conn_server;

static unix_msg_processor	d2l_msg_proc;

static unique_ptr<thread> cm_engine_monitoring_thread;
sem_t  *cm_engine_cleanup_sem = nullptr;

void cm_engine_monitoring_thread_f(sem_t *cm_engine_cleanup_sem)
{
	while(1) {
		assert(cm_engine_cleanup_sem != nullptr);
		/* Wait for notification to check for dead engines */
		sem_wait(cm_engine_cleanup_sem);

		HIGH("Cleaning up Unix dead engines, or shutting down all!\n");



		/* If shutting down then self-terminate the thread */
		if (shutting_down) {
			prov_daemon_info_list.clear();
			hello_daemon_info_list.clear();
			INFO("Shutting down. Exiting '%s'\n", __func__);
			return;
		} else {
			/* Check both the prov and Hello daemon lists for dead
			 * CM engines. Remove if necessary. */
			prov_daemon_info_list.remove_daemon_with_dead_eng();
			hello_daemon_info_list.remove_daemon_with_dead_eng();
		}
	}
} /* cm_engine_monitoring_thread_f() */

/**
 * @brief Thread for monitoring and destroying Tx and Rx engines
 * 	  when they die.
 *
 * @param engine_cleanup_sem	Semaphore posted by an engine when
 * 				it self-dies due to an unbroken
 * 				connection.
 */
static void unix_engine_monitoring_thread_f(sem_t *engine_cleanup_sem)
{
	while (1) {
		/* Wait until there is a reason to perform cleanup */
		sem_wait(engine_cleanup_sem);

		HIGH("Cleaning up Unix dead engines, or shutting down all!\n");
		unix_rx_eng_list.erase(
				 	 remove_if(begin(unix_rx_eng_list),
				 		   end(unix_rx_eng_list),
				 		   [](unique_ptr<unix_rx_engine> &e)
				 		   {return e->isdead() || shutting_down;}),
				 	 end(unix_rx_eng_list)
				      );

		/* Check the tx_eng_list for dead engines. This is more complicated
		 * as we need to close and destroy entities...etc. */
		for(unique_ptr<unix_tx_engine>& tx_eng : unix_tx_eng_list) {
			if (tx_eng->isdead() || shutting_down) {
				/* If the tx_eng is being used by a client app, then
				 * we must clear the connected_to_ms_info_list  */
				DBG("Cleaning up connected_to_ms_info_list\n");
				lock_guard<mutex> conn_lock(connected_to_ms_info_list_mutex);
				connected_to_ms_info_list.erase(
					remove(begin(connected_to_ms_info_list),
					       end(connected_to_ms_info_list),
					       tx_eng.get()),
					end(connected_to_ms_info_list));

				/* If the tx_eng is being used by a server app,
				 * then we must clear all related memory spaces
				 *  and owners using that tx_eng. */
				DBG("Cleaning up memory spaces & owners...\n");
				the_inbound->close_and_destroy_mspaces_using_tx_eng(tx_eng.get());
				owners.close_mso(tx_eng.get());
				owners.destroy_mso(tx_eng.get());
			}
		}

		/* Remove all dead Tx engine entries, or all if shutting down */
		DBG("tx_eng_list.size() = %u\n", unix_tx_eng_list.size());
		unix_tx_eng_list.erase(remove_if(begin(unix_tx_eng_list),
				         end(unix_tx_eng_list),
			 		   [](unique_ptr<unix_tx_engine> &e)
			 		   {return e->isdead() || shutting_down;}),				  end(unix_tx_eng_list));
		if (shutting_down) {
			INFO("Shutting down. Exiting '%s'\n", __func__);
			return;
		}
	} /* while */
} /* unix_engine_monitoring_thread_f() */

/**
 * @brief Begins accepting connections from RDMA applications via Unix sockets
 *
 * @return Aborts on any serious error.
 */
static void start_accepting_app_connections()
{
	try {
		/* Create a server */
		DBG("Creating Unix socket server object...\n");
		app_conn_server = make_unique<unix_server>("main_server", &shutting_down);

		while (1) {
			/* Wait for client to connect */
			HIGH("Waiting for RDMA app to connect..\n");
			if (app_conn_server->accept()) {
				CRIT("Failed to accept connections.\n");
				throw unix_sock_exception(
						"Failed in server->accept()");
			}
			HIGH("Application connected!\n");

			/* Create other server for handling connection with app */
			auto accept_socket = app_conn_server->get_accept_socket();
			auto other_server = make_shared<unix_server>(
								"other_server",
								&shutting_down,
								accept_socket);

			/* Create Tx and Rx engine per connection */
			unique_ptr<unix_tx_engine> unix_tx_eng =
				make_unique<unix_tx_engine>(other_server,
							    unix_engine_cleanup_sem);

			unique_ptr<unix_rx_engine> unix_rx_eng =
				make_unique<unix_rx_engine> (other_server,
							     d2l_msg_proc,
							     unix_tx_eng.get(),
							     unix_engine_cleanup_sem);

			/* We passed 'other_server' to both engines. Now
			 * relinquish ownership. They own it. Together.	 */
			other_server.reset();

			/* Store engines in list for later cleanup */
			unix_rx_eng_list.push_back(move(unix_rx_eng));
			unix_tx_eng_list.push_back(move(unix_tx_eng));

		} /* while */
	} /* try */
	catch(exception& e) {
		CRIT("Fatal error: %s. Aborting daemon!\n", e.what());
		abort();
	}
} /* start_accepting_app_connections() */

/**
 * @brief Shutdown the RDMA daemeon
 *
 */
void shutdown()
{
	/* Kill the threads */
	shutting_down = true;

	/* Delete the inbound object */
	INFO("Deleting the_inbound\n");
	delete the_inbound;

	INFO("Clear the prov and hello daemon lists\n");
	sem_post(cm_engine_cleanup_sem);
	cm_engine_monitoring_thread->join();

	/* Kill Tx/Rx engines that connect with libraries */
	sem_post(unix_engine_cleanup_sem);
	unix_engine_monitoring_thread->join();

	/* Next, kill provisioning thread */
	HIGH("Killing provisioning thread\n");
	if (pthread_kill(prov_thread, SIGUSR1) == 0)
		if (pthread_join(prov_thread, NULL) == 0) {
			HIGH("Provisioning thread is dead\n");
		}

	/* Kill the fabric management thread */
	halt_fm_thread();

	/* Close mport device */
	if (peer.mport_hnd != 0) {
		INFO("Closing mport\n");
		riomp_mgmt_mport_destroy_handle(&peer.mport_hnd);
	}
	INFO("Mport %d closed\n", peer.mport_id);
	//rdma_log_close();
	exit(1);
} /* shutdown() */

/**
 * @brief Signal handler
 *
 * @param sig  Signal code
 */
static void sig_handler(int sig)
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
		printf("Backtrace can hold %u entries\n", (unsigned)count);
		backtrace_symbols_fd(buffer, count, STDERR_FILENO);
	}
	break;

	case SIGUSR1:	/* pthread_kill() */
	/* Ignore signal */
	return;

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
	}

	/* Don't call shutdown again if we are shutting down */
	if (!shutting_down)
		shutdown();
} /* sig_handler() */

/**
 * @brief Determines whether daemon was started in the foreground or background
 * 	  (i.e. "rdmad &")
 *
 * @return true if foreground, false if background
 */
static bool foreground(void)
{
	return (tcgetpgrp(STDIN_FILENO) == getpgrp());
}

int main (int argc, char **argv)
{
	int c;
	int rc;
	int cons_ret;
	constexpr auto LOGGER_FAILURE = 1;
	constexpr auto CONSOLE_FAILURE = 2;
	constexpr auto OUT_KILL_CONSOLE_THREAD = 4;
	constexpr auto OUT_CLOSE_PORT = 5;
	constexpr auto OUT_DELETE_INBOUND = 6;
	constexpr auto OUT_KILL_PROV_THREAD = 7;
	constexpr auto OUT_KILL_FM_THREAD = 8;

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
			puts("-n		Do not run console (background operation");
			exit(1);
		break;
		case 'm':
			peer.mport_id = atoi(optarg);
		break;
		case 'n':	/* Same as doing "rdmad &" */
			peer.run_cons = 0;
		break;
		case '?':
			/* Invalid command line option */
			exit(1);
		default:
			abort(); /* Unexpected error */
		}

	try {
		/* Initialize logger */
		if (rdma_log_init("rdmad.log", 1)) {
			puts("Failed to initialize logging system");
			throw LOGGER_FAILURE;
		}

		/* Prepare and start console thread, if applicable */
		if (peer.run_cons) {
			cli_init_base(custom_quit);

			add_commands_to_cmd_db(
					rdmad_cmds_size()/sizeof(rdmad_cmds[0]),
					rdmad_cmds);
			liblog_bind_cli_cmds();
			splashScreen((char *)"RDMA Daemon Command Line Interface");
			cons_ret = pthread_create( &console_thread, NULL,
					console, (void *)((char *)"RDMADaemon> "));
			if(cons_ret) {
				CRIT("Failed to start console, rc: %d\n", cons_ret);
				throw CONSOLE_FAILURE;
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
			CRIT("Failed in riomp_mgmt_mport_create_handle(): %s\n",
								strerror(-rc));
			throw OUT_KILL_CONSOLE_THREAD;
		}
		DBG("peer.mport_hnd = 0x%X\n", peer.mport_hnd);

		/* Query device information, and store destid */
		struct riomp_mgmt_mport_properties prop;

		rc = riomp_mgmt_query(peer.mport_hnd, &prop);
		if (rc != 0) {
			CRIT("Failed in riodp_query_mport(): %s\n", strerror(errno));
			throw OUT_CLOSE_PORT;
		}
		peer.destid = prop.hdid;
		INFO("mport(%d), destid = 0x%X\n",
					peer.mport_id, peer.destid);

		/* Create inbound space */
		try {
			the_inbound = new inbound(
					peer,
					owners,
					peer.mport_hnd,
					2,		/* No. of windows */
					4*1024*1024);	/* Size in MB */
		}
		catch(std::bad_alloc& e) {
			CRIT("Failed to allocate the_inbound\n");
			throw OUT_CLOSE_PORT;
		}
		catch(inbound_exception& e) {
			CRIT("%s\n", e.what());
			throw OUT_CLOSE_PORT;
		}

		/* Create provisioning thread */
		rc = pthread_create(&prov_thread, NULL, prov_thread_f, (void *)&peer);
		if (rc) {
			CRIT("Failed to create prov_thread: %s\n", strerror(errno));
			throw OUT_DELETE_INBOUND;
		}

		/* CM Engine cleanup semaphore. Posted by engines that die
		 * so we can clean up after them. */
		cm_engine_cleanup_sem = new sem_t();
		sem_init(cm_engine_cleanup_sem, 0, 0);

		/* Create and detach engine monitoring thread */
		cm_engine_monitoring_thread = make_unique<thread>(
						cm_engine_monitoring_thread_f,
						cm_engine_cleanup_sem);

		/* Create fabric management thread */
		rc = start_fm_thread();
		if (rc) {
			CRIT("Failed to create fm_thread: %s\n", strerror(errno));
			throw OUT_KILL_PROV_THREAD;
		}

		/* Create remote CLI terminal thread */
		rc = pthread_create(&cli_session_thread, NULL, cli_session,
						(void*)(&peer.cons_skt));
		if(rc) {
			CRIT("Failed to create cli_session_thread: %s\n", strerror(errno));
			throw OUT_KILL_FM_THREAD;
		}

		/* Engine cleanup semaphore. Posted by engines that die
		 * so we can clean up after them. */
		unix_engine_cleanup_sem = new sem_t();
		sem_init(unix_engine_cleanup_sem, 0, 0);

		/* Create, start, and detach Unix engine monitoring thread */
		unix_engine_monitoring_thread = make_unique<thread>(
					unix_engine_monitoring_thread_f,
					unix_engine_cleanup_sem);

		/* Start accepting connections from apps linked with LIBRDMA */
		start_accepting_app_connections();
	} /* try */

	/* Never reached */

	catch(int& e) {
		switch(e) {

		case OUT_KILL_FM_THREAD:
			/* Kill the fabric management thread */
			halt_fm_thread();

		case OUT_KILL_PROV_THREAD:
			pthread_kill(prov_thread, SIGUSR1);
			pthread_join(prov_thread, NULL);

		case OUT_DELETE_INBOUND:
			delete the_inbound;

		case OUT_CLOSE_PORT:
			riomp_mgmt_mport_destroy_handle(&peer.mport_hnd);

		case OUT_KILL_CONSOLE_THREAD:
			pthread_kill(console_thread, SIGUSR1);
			pthread_join(console_thread, NULL);

		case CONSOLE_FAILURE:
			rdma_log_close();

		case LOGGER_FAILURE:
			fprintf(stderr, "Exiting due to logger failure\n");
			/* No break */

		default:
			rc = e;
		}
	}

	return rc;	
} /* main() */
