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


static FILE *log_file;

void show_help()
{
	printf("rskt_client -d<did> -s<socknum> -h -l <log level> -L<len> -s <skts>"
							" -t -r<rpt> \n");
	printf("-d<did>    : Destination ID of machine running rskt_server.\n");
	printf("-s<socknum>: Socket number used by rskt_server\n");
	printf("             Default is 1234\n");
	printf("-h         : This help message.\n");
	printf("-l<log level>    : Log severity to display and capture\n");
	printf("-L<len>    : Specify length of data to send (0 to 8192).\n");
	printf("             Default is 512 bytes\n");
	printf("-t         : Use varying data length data. Overrides -l\n");
	printf("-r<rpt>    : Repeat test this many times. Default is 1\n");
	printf("-p<skts>   : Number of sockets to execute in parallel.\n");
	printf("             Default is 1.\n");
} /* show_help() */

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

	/* Data to be sent */
	for (j = 0; j < length; j++)
		send_buf[j] = j;

	/* Fill receive buffer with 0xAA */
	memset(recv_buf, 0xAA, length);

	INFO("Generated %u bytes", length);
	return length;
} /* generate_data() */

unsigned repetitions = 1;
uint16_t destid = 0xFFFF;
int socket_number = RSKT_DEFAULT_SOCKET_NUMBER;
unsigned data_length = 512;
sem_t client_done;
int errorish_goodbye;
pthread_t clients[25];
unsigned tx_test = 0;

void *parallel_client(void *parms)
{
	int i, j, rc;
	uint8_t send_buf[RSKT_DEFAULT_SEND_BUF_SIZE];
	uint8_t recv_buf[RSKT_DEFAULT_RECV_BUF_SIZE];
	int *client_num = (int *)parms;
	char my_name[16];
	struct rskt_sockaddr sock_addr;
	rskt_h	client_socket;

	sock_addr.ct = destid;
	sock_addr.sn = socket_number;

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "CLIENT_%d", *client_num);
        pthread_setname_np(pthread_self(), my_name);

        rc = pthread_detach(pthread_self());
        if (rc) {
                WARN("Client %d pthread_detach rc %d", *client_num, rc);
        };

	for (i = 0; i < repetitions; i++) {
		/* Create a client socket */
		client_socket = rskt_create_socket();

		if (!client_socket) {
			ERR("Client %d: Create socket failed, rc = %d: %s",
				*client_num, rc, strerror(errno));
			goto cleanup_rskt;
		}

		/* Connect to server */
		rc = rskt_connect(client_socket, &sock_addr);
		if (rc) {
			ERR("Client %d: Connect to %u on %u failed",
				*client_num, destid, socket_number);
			goto close_client_socket;
		}

		for (j = 0; j < 100; j ++) {
			/* Generate data to send to server */
			data_length = generate_data(data_length, tx_test,
					send_buf, recv_buf);

			/* Send the data */
			rc = rskt_write(client_socket, send_buf, data_length);
			if (rc) {
				ERR("Client %d: iter %d %d  write fail %d: %s",
					*client_num, i, j, rc, strerror(errno));
				goto close_client_socket;
			}

			/* Read data echoed back from the server */

			do {
				rc = rskt_read(client_socket, recv_buf,
						RSKT_DEFAULT_RECV_BUF_SIZE);
			} while (rc == -ETIMEDOUT);

			if (rc <= 0) {
				ERR("Client %d: iter %d read fail %d: %s",
					*client_num, i, j, rc, strerror(errno));
				goto close_client_socket;
			}
			if (rc != data_length) {
				ERR("Client %d: iter %x %x Byte %u Got %u ",
					*client_num, i, j, data_length, rc);
			}

			/* Compare with the original data that we'd sent */
			if (memcmp(send_buf, recv_buf, data_length)) {
				ERR("Client %d: !!! Iter %d %d Compare FAILED.",
					*client_num, i, j);
			} else {
				INFO("Client %d: *** Iter %d %d DATA OK ***",
					*client_num, i, j);
			}
		}

		/* Close the socket and destroy it */
		INFO("Closing socket and destroying it.");
		rc = rskt_close(client_socket);
		if (rc) {
			CRIT("Client %d: Failed to close socket, rc=%d: %s\n",
				*client_num, rc, strerror(errno));
			goto destroy_client_socket;
		}

		rskt_destroy_socket(&client_socket);

	} /* for() */
	HIGH("Client %d: DONE!", *client_num);
	sem_post(&client_done);
	pthread_exit(NULL);

cleanup_rskt:
close_client_socket:
	rskt_close(client_socket);

destroy_client_socket:
	rskt_destroy_socket(&client_socket);
	errorish_goodbye = 1;
	CRIT("Client %d: FAILED!", *client_num);
	sem_post(&client_done);
	pthread_exit(NULL);
};

int main(int argc, char *argv[])
{
	char c;

	time_t	cur_time;
	char	asc_time[26];
	int parallel = 1;

	unsigned i;
	int rc = 0;

	/* Must specify at least 1 argument (the destid) */
	if (argc < 2) {
		puts("Insufficient arguments. Must specify <destid>");
		show_help();
		goto exit_main;
	}

	while ((c = getopt(argc, argv, "htd:l:L:r:s:p:")) != -1)
		switch (c) {

		case 'd':
			destid = atoi(optarg);
			break;
		case 'h':
			show_help();
			exit(1);
			break;
		case 'l':
			g_level = atoi(optarg);
			g_disp_level = g_level;
			break;
		case 'L':
			data_length = atoi(optarg);
			break;
		case 'p':
			parallel = atoi(optarg);
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

	char logfilename[FILENAME_MAX];
	sprintf(logfilename, "/var/log/rdma/rskt_test.log");
	log_file = fopen(logfilename, "a");
	assert(log_file);
	sem_init(&client_done, 0, 0);
	errorish_goodbye = 0;

	for (i = 0; i < parallel; i++) {
		int *parm;
		int ret;
		pthread_t *pt;
	
		parm = (int *)malloc(sizeof(int));
		pt = (pthread_t *)malloc(sizeof(pthread_t));
		*parm = i;
        	ret = pthread_create(pt, NULL, parallel_client, (void *)(parm));
        	if (ret) {
                	ERR("Could not start client %d. EXITING", i);
			errorish_goodbye = 1;
                	goto cleanup_rskt;
        	};
		CRIT("Client %d started.", i);
	};

	for (i = 0; i < parallel; i++) {
		sem_wait(&client_done);
		CRIT("%d clients done.", i+1);
	};

	librskt_finish();

	/* Log the status in log file */
cleanup_rskt:
	time(&cur_time);
	ctime_r(&cur_time, asc_time);
	asc_time[strlen(asc_time) - 1] = '\0';
exit_main:
	if (!errorish_goodbye) {
		CRIT("\n%s @@@ Graceful Goodbye! @@@\n\n", asc_time);
	} else {
		CRIT("\n#### Errorish Goodbye! ###\n\n");
	};
	fclose(log_file);
	return errorish_goodbye;

} /* main() */
