#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <cstdio>

#include <iostream>
#include <memory>

#include "tok_parse.h"
#include "cm_sock.h"

using namespace std;

static uint16_t channel = CM_SOCK_DFLT_SVR_CM_PORT;
static uint32_t mport_id = 0;
static uint32_t destid = 0;
static cm_client *client;
static bool shutting_down = false;

void usage(char * program)
{
	printf("%s - client for demonstration\n", program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("Options are:\n");
	printf("    <channel> channel number on remote RapidIO device\n");
	printf("    <destid> target RapidIO device destination ID\n");
	printf("    <mport> local mport device index\n");
	printf("\n");
	printf("%s -c<channel> -d<destid> -m<mport_id>\n", program);
}

void sig_handler(int sig)
{
	delete client;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}


int main(int argc, char *argv[])
{
	int c;
	char *program = argv[0];

	if (argc < 2) {
		usage(program);
		exit(EXIT_FAILURE);
	}

	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	while (-1 != (c = getopt(argc, argv, "c:d:hm:"))) {
		switch (c) {
		case 'c':
			if (tok_parse_socket(optarg, &channel, 0)) {
				printf(TOK_ERR_SOCKET_MSG_FMT, "Channel number");
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			if (tok_parse_did(optarg, &destid, 0)) {
				printf(TOK_ERR_DID_MSG_FMT);
				exit(EXIT_FAILURE);
			}
			break;
		case 'm':
			if (tok_parse_mport_id(optarg, &mport_id, 0)) {
				printf(TOK_ERR_MPORT_MSG_FMT);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage(program);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			/* Invalid command line option */
			if (isprint(optopt)) {
				printf("Unknown option '-%c\n", optopt);
			}
			usage(program);
			exit(EXIT_FAILURE);
		}
	}

	if (!destid) {
		printf("\nMust specify destid!\n");
		usage(program);
		exit(EXIT_FAILURE);
	}

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
}

