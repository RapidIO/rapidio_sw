#include <semaphore.h>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <cstring>

#include "ts_vector.h"
#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librdma.h"
#include "liblog.h"
#include "rskts_info.h"

#include "rskt_sock.h"


#ifdef __cplusplus
extern "C" {
#endif

struct rskt_ti
{
	rskt_ti(rskt_h accept_socket) : accept_socket(accept_socket)
	{}
	rskt_h accept_socket;
	sem_t	started;
	pthread_t tid;
};

static rskt_server *prov_server = nullptr;
static ts_vector<pthread_t> worker_threads;

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
		return;
	}

	/* Kill all worker threads */
	DBG("Killing %u active worker threads\n", worker_threads.size());
	for (unsigned i = 0; i < worker_threads.size(); i++) {
		pthread_kill(worker_threads[i], SIGUSR1);
		pthread_join(worker_threads[i], NULL);
	}
	worker_threads.clear();

	if (prov_server != nullptr)
		delete prov_server;

	librskt_finish();
	puts("Goodbye!");
	exit(0);
} /* sig_handler() */

void *rskt_thread_f(void *arg)
{
	if (!arg) {
		pthread_exit(0);
	}

	rskt_ti *ti = (rskt_ti *)arg;

	INFO("Creating other server object...\n");
	rskt_server *other_server;
	try {
		other_server = new rskt_server("other_server", ti->accept_socket);
	}
	catch(rskt_exception& e) {
		CRIT(":%s\n", e.err);
		sem_post(&ti->started);
		pthread_exit(0);
	}

	sem_post(&ti->started);

	while (1) {
		/* Wait for data from clients */
		INFO("Waiting to receive from client...\n");

		int received_len = other_server->receive(RSKT_DEFAULT_RECV_BUF_SIZE);
		if ( received_len < 0) {
			ERR("Failed to receive, rc = %d\n", received_len);
			delete other_server;
			worker_threads.remove(ti->tid);
			delete ti;
			DBG("Exiting thread\n");
			pthread_exit(0);
		}

		if (received_len > 0) {
			DBG("received_len = %d\n", received_len);

			/* Get & display the data */
			void *recv_buf;
			other_server->get_recv_buffer(&recv_buf);

			/* Echo data back to client */
			void *send_buf;
			other_server->get_send_buffer(&send_buf);
			memcpy(send_buf, recv_buf, received_len);

			if (other_server->send(received_len) < 0) {
				ERR("Failed to send back\n");
				delete other_server;
				delete ti;
				pthread_exit(0);
			}
		}
	}
	/* Not reached */
	pthread_exit(0);
}

int run_server()
{
	int rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		CRIT("failed in librskt_init, rc = %d\n", rc);
		return -1;
	}

	try {
		prov_server = new rskt_server("prov_server", 1234);
	}
	catch(rskt_exception& e) {
		ERR("Failed to create prov_server: %s\n", e.err);
		return 1;
	}

	puts("Provisioning server created...now accepting connections...");
	rskt_h acc_socket;
	while (1) {
		if (prov_server->accept(&acc_socket)) {
			ERR("Failed to accept. Dying!\n");
			delete prov_server;
			return 2;
		}
		puts("Connected with client");

		/* Create struct for passing info to thread */
		rskt_ti *ti;
		try {
			ti = new rskt_ti(acc_socket);
		}
		catch(...) {
			ERR("Failed to create rskt_ti\n");
			delete prov_server;
			return 3;
		}

		/* Create thread for handling further Tx/Rx on the accept socket */
		int ret = pthread_create(&ti->tid,
					 NULL,
					 rskt_thread_f,
					 ti);
		if (ret) {
			puts("Failed to create request thread\n");
			delete prov_server;
			delete ti;
			return -6;
		}
		worker_threads.push_back(ti->tid);
		DBG("Now %u threads in action\n", worker_threads.size());
		sem_wait(&ti->started);
	} /* while */

	/* Not reached! */
	return 0;
} /* run_server */

int main(int argc, char *argv[])
{
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

	return run_server();
}

#ifdef __cplusplus
}
#endif
