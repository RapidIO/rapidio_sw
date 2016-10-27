#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <cstdio>

#include <iostream>
#include <memory>

#include "liblog.h"
#include "cm_sock.h"

using namespace std;

static int channel = CM_SOCK_DFLT_SVR_CM_PORT;
static int mport_id = 0;
static cm_server *server;
static cm_server *other_server;
static bool shutting_down = false;

void sig_handler(int sig)
{
	delete server;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;

	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	while ((c = getopt(argc, argv, "hc:m:")) != -1) {
		switch (c) {

		case 'c':
			channel = atoi(optarg);
			break;
		case 'm':
			mport_id = atoi(optarg);
			break;
		case 'h':
		default:
			goto print_help;
		}
	}

	rdma_log_init("cm_sock_server", false);

	/* Create a server */
	puts("Creating server object...");
	try {
		server = new cm_server("server", mport_id, channel,
				&shutting_down);
	}

	catch(exception& e) {
		cout << e.what() << endl;
		return 1;
	}

	/* Wait for client to connect */
	puts("Wait for client to connect..");
	riomp_sock_t	accept_socket;

	if (server->accept(&accept_socket)) {
		puts("Failed to accept");
		goto out;
	}
	
	puts("Creating other server object...");
	try {
		other_server = new cm_server("other_server", accept_socket,
				&shutting_down);
	}
	catch(exception& e) {
		cout << e.what() << endl;
		delete server;
		return 2;
	}

	/* Wait for data from clients */
	if (other_server->receive()) {
		puts("Failed to receive");
		goto out;
	}

	/* Get & display the data */
	void *recv_buf;
	other_server->get_recv_buffer(&recv_buf);
	puts((char *)recv_buf);

	/* Echo data back to client */
	void *send_buf;
	other_server->get_send_buffer(&send_buf);
	strcpy((char *)send_buf, (char *)recv_buf);

	if (other_server->send()) {
		puts("Failed to send back");
		goto out;
	}
	puts("Press ENTER to exit");
	getchar();
out:	
	rdma_log_close();
	delete server;
	delete other_server;

	return 0;

print_help:
	printf("%s -c<channel> -m<mport_id\n", argv[0]);
	return 1;
}
