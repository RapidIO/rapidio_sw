#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>

#include <iostream>
#include <memory>

#include <cstdio>

#include "unix_sock.h"

using namespace std;

static unix_client *client;

void sig_handler(int sig)
{
	delete client;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}

int main(int argc, char *argv[])
{
	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	/* Create a client */
	puts("Creating client object...");
	try {
		client = new unix_client();
	}
	catch(unix_sock_exception e) {
		cout << e.err << endl;
		return 1;
	}

	/* Connect to server */
	puts("Connecting to server...");
	if( client->connect()) {
		puts("Failed to connect");
		goto out;
	}
	puts("Connected to server");

	/* Prep a buffer and data */
	void *send_buf;
	client->get_send_buffer(&send_buf);

	/* Send data to server */
	puts("Press ENTER to send data");
	getchar();
	puts("Sending data...");
	strcpy((char *)send_buf, "Hello");
	if (client->send(strlen("Hello" + 1))) {
		puts("Failed to send to server");
		goto out;
	}

	/* Receive data from server */
	void *recv_buf;
	size_t	received_len;
	client->get_recv_buffer(&recv_buf);
	if (client->receive(&received_len)) {
		puts("Failed to receive from server");
		goto out;
	}

	printf("%u characters received: ", received_len);
	puts((char *)recv_buf);

	puts("Press ENTER to exit");
	getchar();

out:
	delete client;

	return 0;
}
