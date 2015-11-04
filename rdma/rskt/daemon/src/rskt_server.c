#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

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

static uint8_t send_buf[RSKT_DEFAULT_SEND_BUF_SIZE];
static uint8_t recv_buf[RSKT_DEFAULT_RECV_BUF_SIZE];

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
		accept_socket = rskt_create_socket();
		if (!accept_socket) {
			fprintf(stderr, "Failed to create accept socket, rc = %d: %s\n",
							rc, strerror(errno));
			goto free_listen_socket;
		}		

		rc = rskt_accept(listen_socket, accept_socket, &sock_addr);
		if (rc) {
			fprintf(stderr, "Failed in rskt_accept, rc = %d: %s\n",
							rc, strerror(errno));
			goto destroy_accept_socket;
		}

		do  {
			rc = rskt_read(accept_socket,
				       recv_buf,
				       RSKT_DEFAULT_RECV_BUF_SIZE);
			if (rc < 0) {
				fprintf(stderr, "Receive failed, rc=%d: %s\n",
							rc, strerror(errno));
				/* Client closed the connection. Back to accepting */
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
		} while(0); /* read/write loop */

		/* FIXME: No calls to rskt_close()/rskt_destroy() at this point. */
	} /* while */

close_accept_socket:
	rskt_close(accept_socket);

destroy_accept_socket:
	rskt_destroy_socket(&accept_socket);

free_listen_socket:
	rskt_close(listen_socket);
	rskt_destroy_socket(&listen_socket);

exit_main:
	return rc;
}/* main() */
