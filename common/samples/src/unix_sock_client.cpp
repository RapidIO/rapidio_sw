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

int main()
{
	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	/* Create a client */
	puts("Creating client object...");
	try {
		client = new unix_client("sample", nullptr);
	}
	catch(unix_sock_exception e) {
		cout << endl;
		return 1;
	}

	/* Connect to server */
	puts("Connecting to server...");
	if( client->connect()) {
		puts("Failed to connect");
		delete client;
		return 2;
	}
	puts("Connected to server");

	/* Prep a buffer and data */
	char *send_buf;
	client->get_send_buffer((void **)&send_buf);
	char ch = '0';
	const char *test_msg = "hello";
	int len = strlen(test_msg);

	puts("Press ENTER to send data");
	getchar();

	while(1) {
		/* Send data to server */
		puts("Sending data...");
		client->flush_send_buffer();
		strcpy(send_buf, test_msg);
		send_buf[len] = ch;
		ch++;
		if (client->send(strlen("Hello") + 1)) {
			puts("Failed to send to server");
			break;
		}

		/* Receive data from server */
		void *recv_buf;
		size_t	received_len = 0;
		client->get_recv_buffer(&recv_buf);
		if (client->receive(&received_len)) {
			puts("Failed to receive from server");
			break;
		}

		printf("%u characters received: ", (unsigned)received_len);
		puts((char *)recv_buf);
	}
	delete client;

	return 0;
}
