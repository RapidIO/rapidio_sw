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

void sig_handler(int sig)
{
	delete server;
	printf("sig_handler got sig(%d)...exiting", sig);
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
		return 0;
	}

	/* Listen for clients */
	puts("Listening for clients...");
	if (server->listen()) {
		puts("Failed to listen");
		goto out;
	}

	/* Wait for client to connect */
	if (server->accept()) {
		puts("Failed to accept");
		goto out;
	}
	
	/* Wait for data from clients */
	if (server->receive()) {
		puts("Failed to receive");
		goto out;
	}

	/* Get & display the data */
	void *recv_buf;
	server->get_recv_buffer(&recv_buf);
	puts((char *)recv_buf);

	/* Echo data back to client */
	void *send_buf;
	server->get_send_buffer(&send_buf);
	strcpy((char *)send_buf, (char *)recv_buf);

	if (server->send()) {
		puts("Failed to send back");
		goto out;
	}
	puts("Press ENTER to exit");
	getchar();
out:	
	delete server;

	return 0;
}
