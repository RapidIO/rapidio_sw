#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <cstdio>

#include <iostream>
#include <memory>

#include "cm_sock.h"

using namespace std;

static int channel = CM_SOCK_DFLT_SVR_CM_PORT;
static int mport_id = 0;
static uint16_t destid = 0;
static cm_client *client;
static bool shutting_down = false;

void sig_handler(int sig)
{
	delete client;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;

	if (argc < 2) {
		goto print_help;
	}

	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	while ((c = getopt(argc, argv, "c:d:hm:")) != -1) {
		switch (c) {
		case 'c':
			channel = (int)strtol(optarg, NULL, 10);
			break;
		case 'd':
			destid = (uint16_t)strtoul(optarg, NULL, 10);
			break;
		case 'm':
			mport_id = (int)strtol(optarg, NULL, 10);
			break;
		case 'h':
		default:
			goto print_help;
		}
	}

	if (!destid) {
		printf("\nMust specify destid!\n");
		goto print_help;
	};

	rdma_log_init("cm_sock_client", false);

	/* Create a client */
	puts("Creating client object...");
	try {
		client = new cm_client("client", mport_id, channel,
				&shutting_down);
	}
	catch(exception& e) {
		cout << e.what() << endl;
		return 1;
	}

	/* Connect to server */
	puts("Connecting to server...");
	if( client->connect(destid)) {
		puts("Failed to connect");
		goto out;
	}
	puts("Connected to server");

	/* Prep a buffer and data */
	void *send_buf;
	client->get_send_buffer(&send_buf);

	/* Send data to server */
	puts("Sending data...");
	strcpy((char *)send_buf, "Hello");
	if (client->send()) {
		puts("Failed to send to server");
		goto out;
	}

	/* Receive data from server */
	void *recv_buf;
	client->get_recv_buffer(&recv_buf);
	if (client->receive()) {
		puts("Failed to receive from server");
		goto out;
	}

	puts((char *)recv_buf);

	puts("Press ENTER to exit");
	getchar();

out:
	rdma_log_close();
	delete client;
	return 0;

print_help:
	printf("%s -c<channel> -d<destid> -m<mport_id\n", argv[0]);
	return 1;
}
