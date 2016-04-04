#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "liblog.h"
#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"

#define SINGLE_CONNECTION	1

#define RSKT_DEFAULT_SEND_BUF_SIZE	4*1024
#define RSKT_DEFAULT_RECV_BUF_SIZE	4*1024

#define RSKT_DEFAULT_DAEMON_SOCKET	3333

#define RSKT_DEFAULT_SOCKET_NUMBER	1234

static uint8_t send_buf[RSKT_DEFAULT_SEND_BUF_SIZE];
static uint8_t recv_buf[RSKT_DEFAULT_RECV_BUF_SIZE];

void show_help()
{
	printf("rskt_client -d<destid> -s<socket_number> [-h] ");
	printf("[-l<data_length>] [-r<repetitions>] [-t] \n");
	printf("-d<destid>: Specify destination ID of machine running rskts_test.");

	printf("-s<socket_number>: Specify socket number used by rskts_test ");
	printf("for listening for connections\n");
	printf("Default is 1234\n");

	puts("-h:  This help message.");

	printf("-l<data_length>: Specify length of data to send (0 to 8192). ");
	printf("If omitted default is 512 bytes\n");

	puts("-r<repetitions>: Specify number of repetitions to run for. Default is 1");

	printf("-t:  Use varying data length data. Overrides -l ");
} /* show_help() */

unsigned generate_data(unsigned data_length, unsigned tx_test)
{
	static unsigned index = 0;
	static unsigned data_lengths[] = { 128, 2048, 1024, 512, 4096, 256, 32, 1536 };
	unsigned j;
	unsigned length;

	if (tx_test) {
		length = data_lengths[index];
		index = (index + 1) & (sizeof(data_lengths)/sizeof(unsigned) - 1);
	} else {
		length = data_length;
	}

	/* Data to be sent */
	for (j = 0; j < length; j++)
		send_buf[j] = j;

	/* Fill receive buffer with 0xAA */
	memset(recv_buf, 0xAA, length);

	printf("Generated %u bytes\n", length);
	return length;
} /* generate_data() */

int main(int argc, char *argv[])
{
	char c;

	uint16_t destid = 0xFFFF;
	int socket_number = RSKT_DEFAULT_SOCKET_NUMBER;
	unsigned repetitions = 1;
	unsigned data_length = 512;
	unsigned tx_test = 0;

	rskt_h	client_socket;
	struct rskt_sockaddr sock_addr;
	unsigned i;
	int rc = 0;

	/* Must specify at least 1 argument (the destid) */
	if (argc < 2) {
		puts("Insufficient arguments. Must specify <destid>");
		show_help();
		goto exit_main;
	}

	while ((c = getopt(argc, argv, "htd:l:r:s:")) != -1)
		switch (c) {

		case 'd':
			destid = atoi(&optarg[5]);
			break;
		case 'h':
			show_help();
			exit(1);
			break;
		case 'l':
			data_length = atoi(optarg);
			break;
		case 'r':
			repetitions = atoi(optarg);
			break;
		case 's':
			socket_number = atoi(optarg);
			break;
		case 't':
			tx_test = 1;
			break;
		case '?':
			/* Invalid command line option */
			show_help();
			exit(1);
			break;
		default:
			abort();
		}

	/* Must specify destid */
	if (destid == 0xFFFF) {
		puts("Error. Must specify <destid>");
		show_help();
		exit(1);
	}

	rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
	if (rc) {
		CRIT("librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
		goto exit_main;
	}

	sock_addr.ct = destid;
	sock_addr.sn = socket_number;

#if SINGLE_CONNECTION == 1
	/* Create a client socket */
	client_socket = rskt_create_socket();

	if (!client_socket) {
		CRIT("rskt_create_socket fail, rc=%d: %s\n", rc, strerror(errno));
		goto cleanup_rskt;
	}

	/* Connect to server */
	rc = rskt_connect(client_socket, &sock_addr);
	if (rc) {
		CRIT("rskt_connect to %u on %u failed\n",destid, socket_number);
		goto close_client_socket;
	}
#endif
	for (i = 0; i < repetitions; i++) {
#if SINGLE_CONNECTION == 0
		/* Create a client socket */
		client_socket = rskt_create_socket();

		if (!client_socket) {
			CRIT("Create client socket failed, rc = %d: %s\n",
							rc, strerror(errno));
			goto cleanup_rskt;
		}

		/* Connect to server */
		rc = rskt_connect(client_socket, &sock_addr);
		if (rc) {
			CRIT("Connect to %u on %u failed\n",
							destid, socket_number);
			goto close_client_socket;
		}
#endif
		/* Generate data to send to server */
		data_length = generate_data(data_length, tx_test);

		/* Send the data */
		rc = rskt_write(client_socket, send_buf, data_length);
		if (rc) {
			CRIT("rskt_write failed, rc = %d: %s\n",
							rc, strerror(errno));
			goto close_client_socket;
		}

		/* Read data echoed back from the server */
retry_read:
		rc = rskt_read(client_socket, recv_buf, RSKT_DEFAULT_RECV_BUF_SIZE);
		if (rc < 0) {
			if (rc == -ETIMEDOUT) {
				ERR("rskt_read() timedout. Retrying!\n");
				goto retry_read;
			}
			CRIT("rskt_read failed, rc=%d: %s\n", rc, strerror(errno));
			goto close_client_socket;
		}
		if (rc != data_length) {
			ERR("Sent %u bytes but received %u bytes\n",
							data_length, rc);
		}

		/* Compare with the original data that we'd sent */
		if (memcmp(send_buf, recv_buf, data_length))
			puts("!!! Data did not compare. FAILED. !!!\n");
		else
			printf("*** Iteration %u, DATA COMPARED OK ***\n", i);
#if SINGLE_CONNECTION == 0
		/* Close the socket and destroy it */
		puts("Closing socket and destroying it.");
		rc = rskt_close(client_socket);
		if (rc) {
			CRIT("Failed to close client socket, rc=%d: %s\n",
							rc, strerror(errno));
			goto destroy_client_socket;
		}

		rskt_destroy_socket(&client_socket);
#endif
	} /* for() */

#if SINGLE_CONNECTION == 1
	/* Close the socket and destroy it */
	HIGH("Closing socket and destroying it.");
	rc = rskt_close(client_socket);
	if (rc) {
		ERR("Failed to close client socket, rc=%d: %s\n",
							rc, strerror(errno));
		goto destroy_client_socket;
	}

	rskt_destroy_socket(&client_socket);
#endif
	librskt_finish();
	puts("@@@ Graceful Goodbye! @@@");
	return rc;

	/* Code below is only for handling errors */
close_client_socket:
	rskt_close(client_socket);

destroy_client_socket:
	rskt_destroy_socket(&client_socket);

cleanup_rskt:

	librskt_finish();

exit_main:
	puts("#### Errorish Goodbye! ###");
	return rc;
} /* main() */
