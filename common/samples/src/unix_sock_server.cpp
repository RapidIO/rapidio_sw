#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>

#include <iostream>
#include <memory>

#include <cstdio>

#include "liblog.h"
#include "unix_sock.h"

using namespace std;

static unix_server *server;
static unix_server *other_server;

void sig_handler(int sig)
{
	delete server;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}

int main(int argc, char *argv[])
{
	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	/* Create a server */
	puts("Creating server object...");
	try {
		server = new unix_server();
	}

	catch(unix_sock_exception e) {
		cout << e.err << endl;
		return 1;
	}

	/* Wait for client to connect */
	puts("Wait for client to connect..");

	if (server->accept()) {
		puts("Failed to accept");
		delete server;
		return 2;
	}

	int accept_socket = server->get_accept_socket();
	printf("After accept() call, accept_socket = 0x%X\n", accept_socket);
	
	puts("Creating other server object...");
	try {
		other_server = new unix_server("other_server", accept_socket);
	}
	catch(unix_sock_exception e) {
		cout << e.err << endl;
		delete server;
		return 3;
	}

	/* Wait for data from clients */
	size_t	received_len;
	if (other_server->receive(&received_len)) {
		puts("Failed to receive");
		delete server;
		delete other_server;
		return 4;
	}

	/* Get & display the data */
	void *recv_buf;
	other_server->get_recv_buffer(&recv_buf);
	puts((char *)recv_buf);

	/* Echo data back to client */
	void *send_buf;
	other_server->get_send_buffer(&send_buf);
	strcpy((char *)send_buf, (char *)recv_buf);

	if (other_server->send(received_len)) {
		puts("Failed to send back");
		delete server;
		delete other_server;
		return 5;
	}
	puts("Press ENTER to exit");
	getchar();

	delete server;
	delete other_server;

	return 0;
}
