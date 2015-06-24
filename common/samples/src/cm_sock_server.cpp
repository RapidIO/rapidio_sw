#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <memory>

#include "liblog.h"
#include "cm_sock.h"

using namespace std;

static int channel;
static int mbox_id;
static int mport_id;
static cm_server *server;
static cm_server *other_server;

void sig_handler(int sig)
{
	delete server;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}

int main(int argc, char *argv[])
{
	char c;

	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	if (argc < 4) {
		printf("%s -b<mbox_id> -c<channel> -m<mport_id\n", argv[0]);
		return 1;
	}

	while ((c = getopt(argc, argv, "hb:c:m:")) != -1)
		switch (c) {

		case 'b':
			mbox_id = atoi(optarg);
			break;
		case 'c':
			channel = atoi(optarg);
			break;
		case 'h':
			printf("%s -b<mbox_id> -c<channel> -m<mport_id\n", argv[0]);
			return 1;
		case 'm':
			mport_id = atoi(optarg);
			break;
		case '?':
			/* Invalid command line option */
			exit(1);
			break;
		default:
			abort();
		}

	/* Create a server */
	puts("Creating server object...");
	try {
		server = new cm_server("server", mport_id, mbox_id, channel);
	}

	catch(cm_exception e) {
		cout << e.err << endl;
		return 1;
	}

	/* Wait for client to connect */
	puts("Wait for client to connect..");
	riodp_socket_t	accept_socket;

	if (server->accept(&accept_socket)) {
		puts("Failed to accept");
		goto out;
	}
	printf("After accept() call, accept_socket = 0x%X\n", accept_socket);
	
	puts("Creating other server object...");
	try {
		other_server = new cm_server("other_server", accept_socket);
	}
	catch(cm_exception e) {
		cout << e.err << endl;
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
	delete server;
	delete other_server;

	return 0;
}
