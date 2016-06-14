#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <cstdio>

#include <iostream>
#include <memory>

#include "cm_sock.h"

using namespace std;

static int channel;
static int mbox_id;
static int mport_id;
static uint16_t destid;
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
	char c;

	if (argc < 5) {
		printf("%s -b<mbox_id> -c<channel> -d<destid> -m<mport_id\n",
								argv[0]);
		return 1;
	}

	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	while ((c = getopt(argc, argv, "b:c:d:hm:")) != -1)
		switch (c) {

		case 'b':
			mbox_id = atoi(optarg);
			break;
		case 'c':
			channel = atoi(optarg);
			break;
		case 'd':
			destid = atoi(optarg);
			break;
		case 'h':
			printf("%s -b<mbox_id> -c<channel> -d<destid> -m<mport_id\n",
									argv[0]);
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

	/* Create a client */
	puts("Creating client object...");
	try {
		client = new cm_client("client", mport_id, mbox_id, channel,
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
	delete client;

	return 0;
}
