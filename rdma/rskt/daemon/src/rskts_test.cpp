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

static rskt_server *prov_server = nullptr;
static rskt_server *server1;

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

	if (server1 != nullptr)
		delete server1;
	if (prov_server != nullptr)
		delete prov_server;

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
		prov_server = new rskt_server("prov_server", 1234);
	}
	catch(rskt_exception& e) {
		ERR("Failed to create prov_server: %s\n", e.err);
		return 1;
	}
	puts("Provisioning server created...now accepting connections...");
	rskt_h acc_socket;
	if (prov_server->accept(&acc_socket)) {
		ERR("Failed to accept. Dying!\n");
		delete prov_server;
		return 2;
	}
	puts("Connected with client");

	/* Now create server1 which is used for exchanging data */
	try {
		server1 = new rskt_server("server1", acc_socket);
	}
	catch(rskt_exception& e) {
		ERR("Failed to create server1: %s\n", e.err);
	}

	if (server1->receive(32) < 0) {
		ERR("Failed to receive. Dying!\n");
		delete server1;
		return 3;
	}
	puts("Received data!");
	char *in_msg;

	server1->get_recv_buffer((void **)&in_msg);

	puts(in_msg);
	char *out_msg;

	server1->get_send_buffer((void **)&out_msg);

	strcpy(out_msg, in_msg);

	if (server1->send((unsigned)strlen(in_msg))) {
		ERR("Failed to send. Dying!");
		delete server1;
		return 4;
	}

	puts("All is good. Press any key to end!");
	getchar();

	delete server1;
}

#ifdef __cplusplus
}
#endif
