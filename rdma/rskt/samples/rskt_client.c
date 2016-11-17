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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <semaphore.h>
#include <pthread.h>

#include "liblog.h"
#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"

#define RSKT_DEFAULT_SEND_BUF_SIZE	4*1024
#define RSKT_DEFAULT_RECV_BUF_SIZE	4*1024

#define RSKT_DEFAULT_DAEMON_SOCKET	3333

#define RSKT_DEFAULT_SOCKET_NUMBER	1234


/**
 * \file
 *\brief Example multi threaded client application for RMA Sockets library
 *
 * The program starts a specified number of threads, assigning a unique 
 * identifying integer to each one. Each thread then repeatedly connects
 * to the server, performws write and read data transfer operations,
 * closes the socket,  and reconnects.
 * 
 * Usage:
 *
 * rskt_client -d(did) -s(socknum) -h -l (loglevel) -L(len) -p (skts)
 *
 * - did : Destination ID of node running rskt_server.
 * - socknum : Socket number the server is listening to, 1 to 65535.
 * Default is 1234.
 * - loglevel : Log severity to display and capture. Values are:
 * - 1 - No logs
 * - 2 - critical
 * - 3 - Errors and above
 * - 4 - Warnings and above
 * - 5 - High priority info and above
 * - 6 - Information logs and above
 * - 7 - Debug information and above
 * - len - Specify length of data to send (0 to 8192).
 *       Default is 512 bytes\
 * - -t : Use varying data length data. Overrides option -L (len)
 * - rpt : Repeat test this many times. Default is 1
 * - rpt : Repeat test this many times. Default is 1
 * - skts : Numbeer of sockets to execute in parallel
 * - -h   Display usage information and exit.
 */

/** 
 * \brief display usage information for the RSKT client
 */
void usage()
{
	printf("rskt_client -d<did> -s<socknum> -h -l <log level> -L<len> -p <threads>"
							" -t -r<rpt> \n");
	printf("-d<did>    : Destination ID of node running rskt_server.\n");
	printf("-s<socknum>: Socket number used by rskt_server\n");
	printf("             Default is 1234\n");
	printf("-h         : Display this help message and exit.\n");
        printf("-l<log level>    : Log severity to display and capture\n");
        printf("                   1 - No logs\n");
        printf("                   2 - critical\n");
        printf("                   3 - Errors and above\n");
        printf("                   4 - Warnings and above\n");
        printf("                   5 - High priority info and above\n");
        printf("                   6 - Information logs and above\n");
        printf("                   7 - Debug information and above\n");
	printf("-L<len>    : Specify length of data to send (0 to 8192).\n");
	printf("             Default is 512 bytes\n");
	printf("-t         : Use varying data length data. Overrides -l\n");
	printf("-r<rpt>    : Repeat test this many times. Default is 1\n");
	printf("-p<skts>   : Number of sockets to execute in parallel.\n");
	printf("             Default is 1.\n");
} /* usage() */

/**
 * \brief Generate random data for each send/receive cycle for each 
 * client thread.
 *
 * \param[in] data_len Number of bytes to generate.
 * \param[in] tx_test  Vary the number of bytes sent with each transaction
 * \param[in] send_buf  Buffer to fill with data
 * \param[in] recv_buf  Buffer to initialize
 */
unsigned generate_data(unsigned data_len, unsigned tx_test, 
			uint8_t *send_buf, uint8_t *recv_buf)
{
	static unsigned index = 0;
	static unsigned data_lens[] = {128, 2048, 1024, 512,
					4096, 256, 32, 1536};
	unsigned j;
	unsigned length;

	if (tx_test) {
		length = data_lens[index];
		index = (index + 1) & (sizeof(data_lens)/sizeof(unsigned) - 1);
	} else {
		length = data_len;
	}

	for (j = 0; j < length; j++)
		send_buf[j] = j;

	memset(recv_buf, 0xAA, length);

	INFO("Generated %u bytes", length);
	return length;
} /* generate_data() */

unsigned repetitions = 1; /** Each thread send and receives this 
			* number of times
			*/
uint16_t destid = 0xFFFF; /** Destid of the the server */
int socket_number = RSKT_DEFAULT_SOCKET_NUMBER;  /** rskt_server socket number */
unsigned tx_test = 0; /** 0 - send constant data, 1 - vary transmit data length
			*/
int data_length = 512;	/** Length of data if tx_test is 0 */
sem_t client_done;		/** Accounting for exiting client threads */
int errorish_goodbye;	/** 0 if successful, 1 if an error was detected */

/**
 * \brief This is the body of the client thread which sends data to the
 *        server, reads the data back, and checks that what was received
 *        matches what was sent.
 *
 * \param[in] parms Points to integer client identifier
 *
 * \return None.
 *
 * Sets the errorish_goodbye variable to 1 if an error has occurred.
 * Performs the following steps:
 */
void *parallel_client(void *parms)
{
	unsigned i;
	int rc, j;
	uint8_t send_buf[RSKT_DEFAULT_SEND_BUF_SIZE];
	uint8_t recv_buf[RSKT_DEFAULT_RECV_BUF_SIZE];
	int client_num;
	char my_name[16];
	struct rskt_sockaddr sock_addr;
	rskt_h	client_socket;

	sock_addr.ct = destid;
	sock_addr.sn = socket_number;
	client_num = (NULL == parms ? -1 : *(int *)parms);

	/** Set thread name based on client  number */
        memset(my_name, 0, 16);
        snprintf(my_name, 15, "CLIENT_%d", client_num);
        pthread_setname_np(pthread_self(), my_name);

	/** Detach thread to allow easy process exit */
        rc = pthread_detach(pthread_self());
        if (rc) {
                WARN("Client %d pthread_detach rc %d", client_num, rc);
        };

	/** For each repetition, do */
	for (i = 0; i < repetitions; i++) {
		/** - Create a client socket */
		client_socket = rskt_create_socket();

		if (!client_socket) {
			ERR("Client %d: Create socket failed, rc = %d: %s",
				client_num, rc, strerror(errno));
			goto cleanup_rskt;
		}

		/** - Connect to server */
		rc = rskt_connect(client_socket, &sock_addr);
		if (rc) {
			ERR("Client %d: Connect to %u on %u failed",
				client_num, destid, socket_number);
			goto close_client_socket;
		}

		/** - Send, receive and check data 100 times */
		for (j = 0; j < 100; j ++) {
			data_length = generate_data(data_length, tx_test,
					send_buf, recv_buf);

			/* Send the data */
			rc = rskt_write(client_socket, send_buf, data_length);
			if (rc) {
				ERR("Client %d: iter %d %d  write fail %d: %s",
					client_num, i, j, rc, strerror(errno));
				goto close_client_socket;
			}

			/* Read data echoed back from the server */

			do {
				rc = rskt_read(client_socket, recv_buf,
						RSKT_DEFAULT_RECV_BUF_SIZE);
			} while (rc == -ETIMEDOUT);

			if (rc <= 0) {
				ERR("Client %d: iter %d read fail %d: %s",
					client_num, i, j, rc, strerror(errno));
				goto close_client_socket;
			}
			if (rc != data_length) {
				ERR("Client %d: iter %x %x Byte %u Got %u ",
					client_num, i, j, data_length, rc);
			}

			/* Compare with the original data that we'd sent */
			if (memcmp(send_buf, recv_buf, data_length)) {
				ERR("Client %d: !!! Iter %d %d Compare FAILED.",
					client_num, i, j);
			} else {
				INFO("Client %d: *** Iter %d %d DATA OK ***",
					client_num, i, j);
			}
		}

		/** -  Close the connected socket */
		INFO("Closing socket and destroying it.");
		rc = rskt_close(client_socket);
		if (rc) {
			CRIT("Client %d: Failed to close socket, rc=%d: %s\n",
				client_num, rc, strerror(errno));
			goto destroy_client_socket;
		}

		/** - Destroy the connected socket */
		rskt_destroy_socket(&client_socket);
		client_socket = NULL;

	} /* for() */

	/** Do accounting for thread exit */
	HIGH("Client %d: DONE!", client_num);
	sem_post(&client_done);
	pthread_exit(NULL);

cleanup_rskt:
close_client_socket:
	/** Ensure client socket is closed before exiting */
	if (client_socket) {
		rskt_close(client_socket);
	}

destroy_client_socket:
	/** Ensure client socket is destroyed before exiting */
	if (client_socket) {
		rskt_destroy_socket(&client_socket);
	}
	errorish_goodbye = 1;
	CRIT("Client %d: FAILED!", client_num);
	sem_post(&client_done);
	pthread_exit(NULL);
};

/**
 * \brief This is the entry point for the RSKT client example.
 *
 * \param[in] argc count of number of entries in argv
 * \param[in] argv Pointers to parameter strings
 *
 * \return 0.
 *
 * Performs the following steps:
 */

struct pthread_info {
	int *parm;
	pthread_t *pt;
};

int main(int argc, char *argv[])
{
	int c;
	time_t	cur_time;
	char	asc_time[26];
	unsigned parallel = 1;

	struct l_head_t pthread_list;
	struct l_item_t *li;
	void *l_item;
	struct pthread_info mem_info;

	unsigned i;
	int rc = 0;

        /** Parse command line parameters */
	if (argc < 2) {
		puts("Insufficient arguments. Must specify <destid>");
		usage();
		goto exit_main;
	}

	while ((c = getopt(argc, argv, "htd:l:L:r:s:p:")) != -1)
		switch (c) {

		case 'd':
			destid = (uint16_t)strtoul(optarg, NULL, 10);
			break;
		default :
		case 'h':
			usage();
			exit(1);
			break;
		case 'l':
			g_level = (unsigned)strtoul(optarg, NULL, 10);
			g_disp_level = g_level;
			break;
		case 'L':
			data_length = (int)strtol(optarg, NULL, 10);
			break;
		case 'p':
			parallel = (unsigned)strtoul(optarg, NULL, 10);
			break;
		case 'r':
			repetitions = (unsigned)strtoul(optarg, NULL, 10);
			break;
		case 's':
			socket_number = (int)strtol(optarg, NULL, 10);
			break;
		case 't':
			tx_test = 1;
			break;
		}

	/** Check entered parameters */
	if (destid == 0xFFFF) {
		puts("Error. Must specify <destid>");
		usage();
		exit(1);
	}

	/** Open log file for debug, initialize status variables */
	rdma_log_init("rskt_test.log", 1);

	/** Initialize RSKT library */
	rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
	if (rc) {
		CRIT("librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
		goto exit_main;
	}


	sem_init(&client_done, 0, 0);
	errorish_goodbye = 0;

	/** For each thread requested, start the thread with a 
	 * unique identifying number.
	 */
	l_init(&pthread_list);
	for (i = 0; i < parallel; i++) {
		int ret;
	
		mem_info.parm = (int *)malloc(sizeof(int));
		mem_info.pt = (pthread_t *)malloc(sizeof(pthread_t));
		l_push_tail(&pthread_list, (void *)&mem_info);
		if ((NULL == mem_info.parm) || (NULL == mem_info.pt)) {
			ERR("Could not allocate memory for client %d. EXITING", i);
			errorish_goodbye = 1;
			goto cleanup_rskt;
		}

		*(mem_info.parm) = i;
		ret = pthread_create(mem_info.pt, NULL, parallel_client, (void *)(mem_info.parm));
        	if (ret) {
                	ERR("Could not start client %d. EXITING", i);
			errorish_goodbye = 1;
                	goto cleanup_rskt;
        	};
		CRIT("Client %d started.", i);
	};

cleanup_rskt:
	/** Wait for all threads to exit, print a message as each finishes */
	for (i = 0; i < parallel; i++) {
		sem_wait(&client_done);
		CRIT("%d clients done.", i+1);
	};

	/** Free memory allocated for threads */
	l_item = l_head(&pthread_list, &li);
	while (NULL != l_item) {
		mem_info = *((struct pthread_info *)l_item);
		free(mem_info.parm);
		free(mem_info.pt);
		l_item = l_next(&li);
	}

	/** Close the RSKT library exit */
	librskt_finish();

	/** Log the status in log file */
	time(&cur_time);
	ctime_r(&cur_time, asc_time);
	asc_time[strlen(asc_time) - 1] = '\0';

exit_main:
	if (!errorish_goodbye) {
		CRIT("\n%s @@@ Graceful Goodbye! @@@\n\n", asc_time);
	} else {
		CRIT("\n#### Errorish Goodbye! ###\n\n");
	};
	/** Close the log file */
	rdma_log_close();
	return errorish_goodbye;

} /* main() */
