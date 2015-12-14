#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"
#include "rskts_info.h"

#define RSKT_DEFAULT_SEND_BUF_SIZE	4*1024
#define RSKT_DEFAULT_RECV_BUF_SIZE	4*1024

#define RSKT_DEFAULT_DAEMON_SOCKET	3333

#define RSKT_DEFAULT_SOCKET_NUMBER	1234

#define RSKT_DEFAULT_MAX_BACKLOG	  50

struct slave_thread_params {
	pthread_t slave_thread;
	rskt_h	accept_socket;
};

void *slave_thread_f(void *arg)
{
	struct slave_thread_params	*slave_params;
	pthread_t slave_thread;
	rskt_h accept_socket;
	uint32_t data_size;
	void *send_buf;
	void *recv_buf;
	int rc;

	/* Extract parameters and free the params struct */
	if (arg == NULL) {
		fprintf(stderr, "NULL argument. Exiting\n");
		goto slave_thread_f_exit;
	}
	slave_params = (struct slave_thread_params *)arg;
	accept_socket = slave_params->accept_socket;
	slave_thread  = slave_params->slave_thread;
	free(slave_params);

	printf("*** Started %s with thread id = 0x%" PRIx64 "\n", __func__, slave_thread);

	/* Allocate send and receive buffers */
	send_buf = malloc(RSKT_DEFAULT_SEND_BUF_SIZE);
	if (send_buf == NULL) {
		fprintf(stderr, "Failed to alloc send_buf: %s\n",
							strerror(errno));
		goto slave_thread_f_exit;
	}
	recv_buf = malloc(RSKT_DEFAULT_RECV_BUF_SIZE);
	if (recv_buf == NULL) {
		fprintf(stderr, "Failed to alloc recv_buf: %s\n",
							strerror(errno));
		goto slave_thread_f_exit;
	}
	do  {
		rc = rskt_read(accept_socket,
			       recv_buf,
			       RSKT_DEFAULT_RECV_BUF_SIZE);
		if (rc < 0) {
			if (errno == ETIMEDOUT) {
				fprintf(stderr, "rskt_read() timed out. Retrying!\n");
				continue;
			}
			fprintf(stderr, "Receive failed, rc=%d: %s\n",
						rc, strerror(errno));
			/* Client closed the connection. Die! */
			break;
		} else
			printf("Received %d bytes\n", rc);

		data_size = rc;

		memcpy(send_buf, recv_buf, data_size);

		rc = rskt_write(accept_socket, send_buf, data_size);
		if (rc) {
			fprintf(stderr, "Failed to send data, rc = %d: %s\n",
							rc, strerror(rc));
			break;
		}
	} while(1); /* read/write loop */

slave_thread_f_exit:
	printf("Exiting %s, thread id=0x%X, socket=0x%X\n", __func__,
						slave_thread, accept_socket);

	/* Free send/receive buffers */
	if (send_buf != NULL)
		free(send_buf);
	if (recv_buf == NULL)
		free(recv_buf);

	/* FIXME: Close and destroy the accept socket */
	pthread_exit(0);
} /* slave_thread_f() */


int main(int argc, char *argv[])
{
	rskt_h	listen_socket;
	rskt_h	accept_socket;

	struct rskt_sockaddr sock_addr;

	int data_size;
	int rc;

	rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
	if (rc) {
		fprintf(stderr, "Failed in librskt_init, rc = %d: %s\n", 
							rc, strerror(errno));
		goto exit_main;
	}

	listen_socket = rskt_create_socket();
	if (!listen_socket) {
		fprintf(stderr, "Failed to create listen socket, rc = %d: %s\n",
							rc, strerror(errno));
		goto exit_main;
	}

	sock_addr.ct = 0;
	sock_addr.sn = RSKT_DEFAULT_SOCKET_NUMBER;

	rc = rskt_bind(listen_socket, &sock_addr);
	if (rc) {
		fprintf(stderr, "Failed to bind listen socket, rc = %d: %s\n",
							rc, strerror(errno));
		goto free_listen_socket;
	}

	rc = rskt_listen(listen_socket, RSKT_DEFAULT_MAX_BACKLOG);
	if (rc) {
		fprintf(stderr, "Failed to listen & set max backlog, rc = %d: %s\n",
							rc, strerror(errno));
		goto free_listen_socket;

	}

	while (1) {
		struct slave_thread_params *slave_params;

		/* Create a new accept socket for the next connection */
		accept_socket = rskt_create_socket();
		if (!accept_socket) {
			fprintf(stderr, "Failed to create accept socket, rc = %d: %s\n",
							rc, strerror(errno));
			goto free_listen_socket;
		}		

		/* Await connect requests from RSKT clients */
		rc = rskt_accept(listen_socket, accept_socket, &sock_addr);
		if (rc) {
			fprintf(stderr, "Failed in rskt_accept, rc = %d: %s\n",
							rc, strerror(errno));
			goto destroy_accept_socket;
		}

		/* Create a thread for handling transmit/receive on new socket */
		slave_params = (struct slave_thread_params *)
				malloc(sizeof(struct slave_thread_params));
		slave_params->accept_socket = accept_socket;
		rc = pthread_create(&slave_params->slave_thread,
				    NULL,
				    slave_thread_f,
				    slave_params);
		if (rc) {
			fprintf(stderr, "slave_thread failed, rc %d\n : %s",
						errno, strerror(errno));
			/* We failed. But don't exit so we can maintain the other
			 * successful connections we may already have. */
			continue;
		}
	} /* while */

destroy_accept_socket:
	rskt_destroy_socket(&accept_socket);

free_listen_socket:
	rskt_close(listen_socket);
	rskt_destroy_socket(&listen_socket);

exit_main:
	return rc;
}/* main() */
