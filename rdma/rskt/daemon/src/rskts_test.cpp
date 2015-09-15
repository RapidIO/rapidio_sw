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

int main(int argc, char *argv[])
{
	rskt_server *server;
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
	puts("Connected with client...now waiting for data...");
	if (server->receive(32)) {
		ERR("Failed to receive. Dying!\n");
		delete server;
		return 3;
	}
	puts("Received data!");

	char *in_msg;

	server->get_recv_buffer((void **)&in_msg);

	cout << in_msg << endl;

	char *out_msg;

	server->get_send_buffer((void **)&out_msg);

	strcpy(out_msg, in_msg);

	if (server->send(strlen(in_msg))) {
		ERR("Failed to send. Dying!");
		delete server;
		return 4;
	}

	cout << "All is good. Goodbye!\n";

	delete server;
}

#ifdef __cplusplus
}
#endif