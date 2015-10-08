#include <stdint.h>

#include <cstdio>

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

static rskt_server *server = nullptr;

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

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
		return;
	}

	if (server != nullptr)
		delete server;

	exit(0);
} /* sig_handler() */

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

	int rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		puts("failed in librskt_init");
		return 1;
	}

	try {
		server = new rskt_server("server1", 1234);
	}
	catch(rskt_exception& e) {
		ERR("Failed to create server: %s\n", e.err);
		return 1;
	}
	puts("Server created...now accepting connections...");
	if (server->accept()) {
		ERR("Failed to accept. Dying!\n");
		delete server;
		return 2;
	}
	puts("Connected with client");
	if (server->receive(32) < 0) {
		ERR("Failed to receive. Dying!\n");
		delete server;
		return 3;
	}
	puts("Received data!");
	char *in_msg;

	server->get_recv_buffer((void **)&in_msg);

	puts(in_msg);
	char *out_msg;

	server->get_send_buffer((void **)&out_msg);

	strcpy(out_msg, in_msg);

	if (server->send((unsigned)strlen(in_msg))) {
		ERR("Failed to send. Dying!");
		delete server;
		return 4;
	}

	puts("All is good. Press any key to end!");
	getchar();

	delete server;
}

#ifdef __cplusplus
}
#endif
