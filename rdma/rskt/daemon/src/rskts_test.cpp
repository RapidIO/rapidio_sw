#include <semaphore.h>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <cstring>

#include "ts_vector.h"
#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
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
		other_server = new rskt_server("other_server",
				ti->accept_socket,
				RSKT_MAX_SEND_BUF_SIZE,
				RSKT_MAX_RECV_BUF_SIZE);
	}
	catch(rskt_exception& e) {
		CRIT(":%s\n", e.err);
		pthread_exit(0);
	}

	while (1) {
		/* Wait for data from clients */
		INFO("Waiting to receive from client...\n");

		int received_len = other_server->receive(RSKT_MAX_RECV_BUF_SIZE);
		if ( received_len < 0) {
			if (errno == ETIMEDOUT) {
				/* It is not an error since the client may not
				 * be sending anymore data. Just go back and
				 * try again.
				 */
				continue;
			} else {
				ERR("Failed to receive, rc = %d\n", received_len);
				goto exit_rskt_thread_f;
			}
		}

		if (received_len > 0) {
			DBG("received_len = %d\n", received_len);

			void *recv_buf;
			other_server->get_recv_buffer(&recv_buf);

			/* Check for the 'disconnect' message */
			if (*(uint8_t *)recv_buf == 0xFD) {
				puts("Disconnect message receive. DISCONNECTING");
				goto exit_rskt_thread_f;
			}
			/* Echo data back to client */
			void *send_buf;
			other_server->get_send_buffer(&send_buf);
			memcpy(send_buf, recv_buf, received_len);

			if (other_server->send(received_len) < 0) {
				ERR("Failed to send back\n");
				goto exit_rskt_thread_f;
			}
		}
	}

exit_rskt_thread_f:
	delete other_server;
	worker_threads.remove(ti->tid);
	delete ti;
	pthread_exit(0);
}

int run_server(int socket_number)
{
	int rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		CRIT("failed in librskt_init, rc = %d\n", rc);
		return -1;
	}

	try {
		prov_server = new rskt_server("prov_server", socket_number);
	}
	catch(rskt_exception& e) {
		ERR("Failed to create prov_server: %s\n", e.err);
		return 1;
	}

	puts("Provisioning server created..");
	rskt_h acc_socket;
	do {
		puts("Accepting connections...");
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
	} while(0);

	/* Wait until all worker threads have exit. Currently
	 * THERE CAN BE ONLY ONE */
	for (auto i = 0; i , worker_threads.size(); i++) {
		pthread_join(worker_threads[i], NULL);
	}

	delete prov_server;
	return 0;
} /* run_server */

void show_help()
{
	puts("Usage: rskts_test -s<socket_number>\n");
}

int main(int argc, char *argv[])
{
	char c;
	int socket_number = -1;

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

	/* Must specify at least 1 argument (the socket number) */
	if (argc < 2) {
		puts("Insufficient arguments. Must specify -s<socket_number>");
		show_help();
		exit(1);
	}

	while ((c = getopt(argc, argv, "hs:")) != -1)
		switch (c) {

		case 'h':
			show_help();
			exit(1);
			break;
		case 's':
			socket_number = atoi(optarg);
			break;
		case '?':
			/* Invalid command line option */
			show_help();
			exit(1);
			break;
		default:
			abort();
		}

	if (socket_number == -1) {
		puts("Error. Must specify -s<socket_number>");
		show_help();
		exit(1);
	}
	return run_server(socket_number);
}

#ifdef __cplusplus
}
#endif
