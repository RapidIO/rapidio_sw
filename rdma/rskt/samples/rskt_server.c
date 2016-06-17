/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/

#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "liblog.h"
#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"
#include "liblog.h"

#define RSKT_DEFAULT_SEND_BUF_SIZE	4*1024
#define RSKT_DEFAULT_RECV_BUF_SIZE	4*1024

#define RSKT_DEFAULT_DAEMON_SOCKET	3333

#define RSKT_DEFAULT_SOCKET_NUMBER	1234

#define RSKT_DEFAULT_MAX_BACKLOG	  50

/** 
 * \file
 * \brief Example server application for RMA Sockets library
 *
 * The program starts accepting connection requests an RMA socket,
 * which by default is 1234.
 * For every successful accept, a separate thread
 * is created which receives data and echos it back to the sender. Once the
 * sender closes the connection, the thread terminates.
 *
 * Usage:  
 * rskt_server {-s (socket_number)} {-l (loglevel)} {-h}\
 * - socket_number : Socket number for clients to connect on, 1 to 65535.
 *  Default is 1234\n");
 * - loglevel : Log severity to display and capture. Values are:
 *   - 1 - No logs
 *   - 2 - critical
 *   - 3 - Errors and above
 *   - 4 - Warnings and above
 *   - 5 - High priority info and above
 *   - 6 - Information logs and above
 *   - 7 - Debug information and above
 * - -h Display usage inforamtion and exit.
 */

/**
 * \brief Display usage inforamtion for rskt_server
 */
void usage()
{
	printf("rskt_server [-s<socket_number>] [-l<loglevel>] [-h]\n");
	printf("-s<socket_number>: Socket number for clients to connect on\n");
	printf("                   Default is 1234\n");
	printf("-l<log level>    : Log severity to display and capture\n");
 	printf("                   1 - No logs\n");
 	printf("                   2 - critical\n");
 	printf("                   3 - Errors and above\n");
 	printf("                   4 - Warnings and above\n");
 	printf("                   5 - High priority info and above\n");
 	printf("                   6 - Information logs and above\n");
 	printf("                   7 - Debug information and above\n");
	printf("-h  Display this help message and exit.\n");
};

/**
 * \brief Structure passed to each thread managing a connection to a client.
 *
 * Allocated by main program, deallocated on process exit.
 */

struct slave_thread_params {
	pthread_t slave_thread; /** Thread structure */
	rskt_h	accept_socket;  /** Accepted socket connection */
};
/** Number of threads operating in parallell */
static unsigned num_threads = 0; 

/**
 * \brief This is the function which echos data back to the sending client.
 *
 * \param[in] arg Untyped void pointer to allocated variable 
 * of type slave_thread_parms.
 *
 * \return None.
 *
 * Performs the following steps:
 */
void *slave_thread_f(void *arg)
{
	struct slave_thread_params *slave_params = NULL;
	pthread_t slave_thread;
	rskt_h accept_socket;
	uint32_t data_size;
	void *send_buf = NULL;
	void *recv_buf = NULL;
	int rc;
        char my_name[16] = {0};

	memset(&slave_thread, 0, sizeof(slave_thread));

	/** Copy parameters to local variables */
	if (arg == NULL) {
		CRIT("NULL argument. Exiting\n");
		goto slave_thread_f_exit;
	}
	slave_params = (struct slave_thread_params *)arg;
	accept_socket = slave_params->accept_socket;
	slave_thread  = slave_params->slave_thread;

	/** Set thread name based on unique socket number */
        snprintf(my_name, 15, "ACC_L%5d", accept_socket->sa.sn);
        pthread_setname_np(slave_params->slave_thread, my_name);

	/** Detach the thread to allow a clean process finish */
	pthread_detach(slave_params->slave_thread);

	/** Do accounting for thread start */
	num_threads++;
	INFO("*** Started %s with thread id = 0x%" PRIx64 "\n",
						__func__, slave_thread);

	/* Allocate send and receive buffers */
	send_buf = calloc(1, RSKT_DEFAULT_SEND_BUF_SIZE);
	if (send_buf == NULL) {
		CRIT("Failed to alloc send_buf: %s\n", strerror(errno));
		goto slave_thread_f_exit;
	}
	recv_buf = calloc(1, RSKT_DEFAULT_RECV_BUF_SIZE);
	if (recv_buf == NULL) {
		CRIT("Failed to alloc recv_buf: %s\n", strerror(errno));
		goto slave_thread_f_exit;
	}

	/** Until the socket is closed: */
	while(1)  {
		/** - Read data sent by client */
		rc = rskt_read(accept_socket,
			       recv_buf,
			       RSKT_DEFAULT_RECV_BUF_SIZE);
		if (rc < 0) {
			if (errno == ETIMEDOUT) {
				ERR("rskt_read() timed out. Retry\n");
				continue;
			}
			ERR("Receive failed, rc=%d:%s\n", rc, strerror(errno));
			/* Client closed the connection. Die! */
			break;
		} else
			INFO("Received %d bytes\n", rc);

		data_size = rc;

		/** - Copy data to transmit buffer */
		memcpy(send_buf, recv_buf, data_size);

		/** - Send data back to client */
		rc = rskt_write(accept_socket, send_buf, data_size);
		if (rc != 0) {
			if (errno == ETIMEDOUT) {
				ERR("rskt-write() timed out. Retrying!\n");
				continue;
			}
			ERR("Failed to send data, rc = %d: %s\n",
							rc, strerror(rc));
			break;
		}
	} /* read/write loop */

slave_thread_f_exit:
	/** Do accounting for thread exit */
	num_threads--;	/* For tracking state upon crash */

	INFO("Exiting %s, thread id=0x%X, socket=0x%X, %u threads active\n",
			__func__, slave_thread, accept_socket, num_threads);

	/** Free send/receive buffers and parameters structure */
	if (send_buf != NULL)
		free(send_buf);
	if (recv_buf == NULL)
		free(recv_buf);
	if (slave_params != NULL)
		free(slave_params);

	/** Ensure socket is closed and destroyed */
	rskt_close(accept_socket);
	rskt_destroy_socket(&accept_socket);

	pthread_exit(0);
}

/**
 * \brief Signal handler for the process.  Display received signal and continue.
 *
 * \param[in] sig signal number received
 * 
 * \return None.
 */
void sig_handler(int sig)
{
	switch (sig) {

	case SIGQUIT:	/* ctrl-\ */
		puts("SIGQUIT - CTRL-\\ signal");
	break;

	case SIGINT:	/* ctrl-c */
		puts("SIGINT - CTRL-C signal");
	break;

	case SIGABRT:	/* abort() */
		puts("SIGABRT - abort() signal");
	break;

	case SIGTERM:	/* kill <pid> */
		puts("SIGTERM - kill <pid> signal");
	break;

	case SIGSEGV:	/* Segmentation fault */
		puts("SIGSEGV (Segmentation Fault)");
	break;

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
	}
	fprintf(stderr, "There are %u threads still active\n", num_threads);
	exit(1);
} 

/**
 * \brief This is the entry point for the RSKT server example.
 *
 * \param[in] argc count of number of entries in argv
 * \param[in] argv Pointers to parameter strings
 *
 * \return 0.   
 *
 * Performs the following steps:
 */

int main(int argc, char *argv[])
{
	rskt_h	listen_socket;
	rskt_h	accept_socket;
	int	socket_number = RSKT_DEFAULT_SOCKET_NUMBER;
	struct rskt_sockaddr sock_addr;
	int	c;
	int	rc;

	/** Parse command line parameters */
	while ((c = getopt(argc, argv, "hs:l:")) != -1)
		switch (c) {

		default:
		case 'h':
			usage();
			exit(1);
			break;
		case 's':
			socket_number = atoi(optarg);
			break;
		case 'l':
			g_level = atoi(optarg);
			g_disp_level = g_level;
			break;
		}

	/** Register signal handler */
	struct sigaction sig_action;
	sig_action.sa_handler = sig_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGQUIT, &sig_action, NULL);
	sigaction(SIGABRT, &sig_action, NULL);
	sigaction(SIGUSR1, &sig_action, NULL);
	sigaction(SIGSEGV, &sig_action, NULL);

	/** Initialize RSKT library */
	rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
	if (rc) {
		CRIT("Failed in librskt_init, rc = %d: %s\n",
							rc, strerror(errno));
		goto exit_main;
	}

	/** Allocate socket handle */
	listen_socket = rskt_create_socket();
	if (!listen_socket) {
		CRIT("Failed to create listen socket, rc = %d: %s\n",
							rc, strerror(errno));
		goto exit_main;
	}

	/** Bind socket handle to specified socket number */
	sock_addr.ct = 0;
	sock_addr.sn = socket_number;

	rc = rskt_bind(listen_socket, &sock_addr);
	if (rc) {
		CRIT("Failed to bind listen socket, rc = %d: %s\n",
							rc, strerror(errno));
		goto free_listen_socket;
	}

	/** Set socket to listen for connection requests */
	rc = rskt_listen(listen_socket, RSKT_DEFAULT_MAX_BACKLOG);
	if (rc) {
		CRIT("Failed to listen & set max backlog, rc = %d: %s\n",
							rc, strerror(errno));
		goto free_listen_socket;

	}

	/** Loop untill interrupted, doing the following */
	while (1) {
		struct slave_thread_params *slave_params;

		/** - Create a new accept socket for the next connection */
		accept_socket = rskt_create_socket();
		if (!accept_socket) {
			CRIT("Cannot create accept socket, rc = %d: %s\n",
							rc, strerror(errno));
			goto free_listen_socket;
		}		

		/** - Await connect requests from RSKT clients */
		INFO("%u threads active, accepting connections\n", num_threads);
		rc = rskt_accept(listen_socket, accept_socket, &sock_addr);
		if (rc) {
			CRIT("Failed in rskt_accept, rc = 0x%X, errno=%d: %s\n",
					rc, errno, strerror(errno));
			goto destroy_accept_socket;
		}

		/** -  Create a thread for handling transmit/receive on 
		*      new socket
 		*/
		slave_params = (struct slave_thread_params *)
				calloc(1, sizeof(struct slave_thread_params));
		slave_params->accept_socket = accept_socket;
		rc = pthread_create(&slave_params->slave_thread,
				    NULL,
				    slave_thread_f,
				    slave_params);
		if (rc) {
			CRIT("slave_thread failed, rc %d\n : %s",
							errno, strerror(errno));
			/* We failed. But don't exit so we can maintain 
			 * the other successful connections we may already have.
			 */
			continue;
		}
	} /* while */

	/** On exit, cleanup listen socket and accept socket if necessary. */
destroy_accept_socket:
	rskt_destroy_socket(&accept_socket);

free_listen_socket:
	rskt_close(listen_socket);
	rskt_destroy_socket(&listen_socket);

exit_main:
	/* The server exits on signal or failure */
	CRIT("** EXITING DUE TO FAILURE **, num_threads = %u\n", num_threads);
	return rc;
}
