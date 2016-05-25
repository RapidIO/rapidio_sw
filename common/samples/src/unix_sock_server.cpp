#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <semaphore.h>

#include <iostream>
#include <memory>

#include <cstdio>

#include "liblog.h"
#include "unix_sock.h"

using namespace std;

static unix_server *server;

struct rpc_ti
{
	rpc_ti(int accept_socket) : accept_socket(accept_socket)
	{}
	int accept_socket;
	sem_t	started;
	pthread_t tid;
};

void *rpc_thread_f(void *arg)
{
	if (!arg) {
		pthread_exit(0);
	}

	rpc_ti *ti = (rpc_ti *)arg;

	puts("Creating other server object...");
	unix_server *other_server;
	try {
		other_server = new unix_server("other_server", nullptr, ti->accept_socket);
	}
	catch(unix_sock_exception e) {
		cout << e.what() << endl;
		sem_post(&ti->started);
		pthread_exit(0);
	}

	sem_post(&ti->started);

	while (1) {
		/* Wait for data from clients */
		puts("Waiting to receive from client...");
		size_t	received_len = 0;
		if (other_server->receive(&received_len)) {
			puts("Failed to receive");
			delete other_server;
			pthread_exit(0);
		}
		if (received_len > 0) {
			printf("received_len = %u\n", (unsigned)received_len);

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
				delete other_server;
				pthread_exit(0);
			}
		} else {
			puts("Remote daemon has closed connection..BYE");
			pthread_exit(0);
		}
	}
	pthread_exit(0);
}

void sig_handler(int sig)
{
	delete server;
	printf("sig_handler got sig(%d)...exiting\n", sig);
	exit(1);
}

int run_rpc_alternative()
{
	/* Create a server */
	puts("Creating server object...");
	try {
		server = new unix_server("main_server");
	}
	catch(unix_sock_exception e) {
		cout << e.what() << endl;
		return 1;
	}

	/* Wait for client to connect */
	puts("Wait for client to connect..");

	while (1) {
		if (server->accept()) {
			puts("Failed to accept");
			delete server;
			return 2;
		}

		int accept_socket = server->get_accept_socket();
		printf("After accept() call, accept_socket = 0x%X\n", accept_socket);

		rpc_ti	*ti;
		try {
			ti = new rpc_ti(accept_socket);
		}
		catch(...) {
			puts("Failed to create rpc_ti");
			delete server;
			return 3;
		}

		int ret = pthread_create(&ti->tid,
					 NULL,
					 rpc_thread_f,
					 ti);
		if (ret) {
			puts("Failed to create request thread\n");
			delete server;
			delete ti;
			return -6;
		}
		sem_wait(&ti->started);
	} /* while */
} /* run_rpc_alternative() */

int main()
{
	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	int ret = run_rpc_alternative();
	if (ret)
		printf("run_rpc_alternative failed (%d)!\n", ret);

	return 0;
}
