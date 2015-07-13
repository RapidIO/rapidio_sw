/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <IDT_Tsi721.h>
#include <sched.h>
#include "tsi721_config.h"
#include "time_utils.h"
#include "latency_threads.h"

#include <sys/types.h>
#include <dirent.h>


#include "rapidio_mport_lib.h"

#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */

// Define configuration constants:
// Single Tsi721 in loopback
// Single Tsi721 either CLIENT or SERVER
// Dual Tsi721 in the same chassis

#define CFG_LPBK 0
#define CFG_1_CL 1
#define CFG_1_SV 2
#define CFG_DUAL 3

#define MAX_MSG_SIZE 0x1000
#define DEFAULT_CHAN (0x1234)

int srv_exit;

struct demo_chan_setup {
	uint32_t mport_id;
	int fd;
	struct rio_mport_properties props;

	uint16_t my_destid;
	uint16_t remote_destid;

	riodp_mailbox_t mailbox;

	char *tx_buf;
	char *rx_buf;
};

struct demo_setup {
	int debug; /* Print additional messages during operation. */
	int verify; /* Check message size and contents. */
	uint32_t repeat; /* Number of messages to exchange */
	int cfg; /* One of CFG_LPBK, CFG_1_RX, CFG_1_TX, or CFG_DUAL */
	int size; /* Payload of message in bytes.  Max is 4096 */
	int got_switch; /* True if a RapidIO switch is present, false otherwise. */

	uint16_t c_chan_no;
	uint16_t s_chan_no; /* Server channel number */
	uint16_t s_c_chan_no; /* Server client channel number */

	riodp_socket_t c_sock;
	riodp_socket_t s_sock;
	riodp_socket_t s_c_sock;

	struct demo_chan_setup ep[2];
};

struct demo_setup demo;

static void display_help(char *program)
{
	printf("%s - test channelized messaging to/from RapidIO device\n",
		program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("options are:\n");
	printf("  -c 0|1|2|3\n");
	printf("  --config 0|1|2|3\n");
	printf("    0 - Tsi721 in loopback\n");
	printf("    1 - Client Tsi721 connected to another chassis\n");
	printf("    2 - Server Tsi721 connected to another chassis\n");
	printf("    3 - Two Tsi721 in same chassis, cabled to each other\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -C channel\n");
	printf("  --chanid channel\n");
	printf("    channel the server is ACCEPTing\n");
	printf("  -D xxxx\n");
	printf("  --destid xxxx\n");
	printf("    destination ID to send to\n");
	printf("  -S xxxx\n");
	printf("  --size xxxx\n");
	printf("    data payload size in bytes (default 0x1000)\n");
	printf("    Allowed range is 0x20 (32) to 0x1000 (4096)\n");
	printf("  -T or --repeat n\n");
	printf("    repeat test n times (default=1)\n");
	printf("  -W : RapidIO switch is present\n");
	printf("  -v turn off buffer data verification\n");
	printf("  -o or --other : parameters after this point \n");
	printf("                  refer to the other end of the link.");
	printf("  -d or --debug : turn on debug info\n");
	printf("  -h or --help  : display this text and exit\n");
	printf("\n");
}

void parse_options(int argc, char** argv)
{
	char *program = argv[0];
	int idx;
	int option;
	static const struct option options[] = {
		{ "destid", required_argument, NULL, 'D' },
		{ "chanid", required_argument, NULL, 'C' },
		{ "size",   required_argument, NULL, 'S' },
		{ "repeat", required_argument, NULL, 'T' },
		{ "mport",  required_argument, NULL, 'M' },
		{ "config", required_argument, NULL, 'c' },
		{ "other",  no_argument, NULL, 'o' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};

	demo.debug = 0;
	demo.verify = 1;
	demo.repeat = 1;
	demo.cfg = CFG_LPBK;
	demo.size = 128;
	demo.got_switch = 0;
	demo.c_chan_no = 0;
	demo.s_chan_no = 0x1234; /* Server channel number */
	demo.s_c_chan_no = 0; /* Server client channel number */

	for (idx = 0; idx < 2; idx++) {
		demo.ep[idx].mport_id = idx;
		demo.ep[idx].fd = -1;
		demo.ep[idx].my_destid = -1;
		demo.ep[idx].remote_destid = -1;
		demo.ep[idx].tx_buf = NULL;
		demo.ep[idx].rx_buf = NULL;
	};

	idx = 0;

	while (1) {
		option = getopt_long_only(argc, argv,
				"c:M:C:D:S:T:vodhW", options, NULL);
		if (option == -1) {
			if (CFG_DUAL == demo.cfg) {
				if (demo.ep[0].mport_id)
					demo.ep[1].mport_id = 0;
				else
					demo.ep[1].mport_id = 1;
			}
			return;
		}
		switch (option) {
			/* DMA Data Transfer Mode options*/
		case 'c':
			demo.cfg = strtol(optarg, NULL, 0);
			if ((demo.cfg >= CFG_LPBK) && (demo.cfg <= CFG_DUAL))
				break;
			printf("\nIllegal config value: %d", demo.cfg);
			goto print_help;
		case 'C':
			demo.s_chan_no = strtol(optarg, NULL, 0);
			break;
		case 'D':
			demo.ep[idx].remote_destid = strtol(optarg, NULL, 0);
			break;
		case 'S':
			demo.size = strtol(optarg, NULL, 0);
			if ((demo.size >= 32) || (demo.size <= MAX_MSG_SIZE))
				break;
			printf("\nIllegal message size: %d", demo.size);
			goto print_help;
			break;
		case 'T':
			demo.repeat = strtol(optarg, NULL, 0);
			break;
			/* Options common for all modes */
		case 'M':
			demo.ep[idx].mport_id = strtol(optarg, NULL, 0);
			break;
		case 'o':
			idx = 1;
			break;
		case 'v':
			demo.verify = 0;
			break;
		case 'd':
			demo.debug = 1;
			break;
		case 'W':
			demo.got_switch = 1;
			break;
		default:
			printf("\nUnknown option: \'%c\'", option);
		case 'h':
			goto print_help;
			break;
		}
	}
print_help:
	display_help(program);
	exit(EXIT_SUCCESS);
};

static void show_rio_devs(void)
{
	uint32_t *mport_list = NULL;
	uint32_t *ep_list = NULL;
	uint32_t *list_ptr;
	uint32_t number_of_eps = 0;
	uint8_t  number_of_mports = RIODP_MAX_MPORTS;
	uint32_t ep = 0;
	int i;
	int mport_id;
	int ret = 0;

	if (riodp_mport_get_mport_list(&mport_list, &number_of_mports))
		return;

	printf("\nAvailable %d local mport(s):\n", number_of_mports);
	if (number_of_mports > RIODP_MAX_MPORTS) {
		printf("WARNING: Only %d out of %d have been retrieved\n",
			RIODP_MAX_MPORTS, number_of_mports);
	}

	list_ptr = mport_list;
	for (i = 0; i < number_of_mports; i++, list_ptr++) {
		mport_id = *list_ptr >> 16;
		printf("+++ mport_id: %u dest_id: %u\n",
				mport_id, *list_ptr & 0xffff);

		/* Display EPs for this MPORT */

		if (riodp_mport_get_ep_list(mport_id, &ep_list, &number_of_eps))
			break;

		printf("\t%u Endpoints (dest_ID): ", number_of_eps);
		for (ep = 0; ep < number_of_eps; ep++)
			printf("%u ", *(ep_list + ep));
		printf("\n");

		if (riodp_mport_free_ep_list(&ep_list))
			printf("ERR: riodp_ep_free_list() ERR %d\n", ret);
	}

	printf("\n");

	riodp_mport_free_mport_list(&mport_list);
};


int open_mports(int last_idx) 
{
	int idx;
	int rc = EXIT_FAILURE;

	for (idx = 0; idx <= last_idx; idx++) {
		demo.ep[idx].fd = riodp_mport_open(demo.ep[idx].mport_id, 0);
		if (demo.ep[idx].fd < 0) {
			printf("Unable to open idx %d mport%d device "
				"err=%d:%s\n",
				idx, demo.ep[idx].mport_id, errno, 
				strerror(errno));
			goto exit;
		}

		if (!riodp_query_mport(demo.ep[idx].fd, &demo.ep[idx].props)) {
			display_mport_info(&demo.ep[idx].props);
		} else {
			printf("Failed to obtain mport information\n");
			printf("Using default configuration\n\n");
		}
	}
	rc = EXIT_SUCCESS;
exit:
	return rc;
};

void close_mports(int last_idx)
{
        int idx;

        printf("\nClosing master port(s)...\n");
        for (idx = 0; idx <= last_idx; idx++) {
                if (demo.ep[idx].fd > 0)
                        close(demo.ep[idx].fd);
        }
};

#define RIO_SYSFS	"/sys/bus/rapidio/devices"

static int fixup_options(void)
{
	int rc = 1;
	uint32_t number_of_eps;
	uint32_t *ep_list;
	uint32_t ep_found, ep;
	int i, idx, max_idx = 1;

	if (!(demo.s_chan_no)) {
		printf("\nERROR: Server channel is 0.");
		goto exit;
	};

	switch (demo.cfg) {
	case CFG_LPBK:
		demo.ep[0].my_destid = demo.ep[0].remote_destid = 
		demo.ep[1].my_destid = demo.ep[1].remote_destid = 
			demo.ep[0].props.hdid;
		demo.ep[1].mport_id = demo.ep[0].mport_id;
		break;
	case CFG_1_SV: 
		demo.ep[0].my_destid = demo.ep[0].props.hdid;
		break;
	case CFG_1_CL: 
		demo.ep[0].my_destid = demo.ep[0].props.hdid;
		break;
	case CFG_DUAL:
		max_idx = 2;
		for (idx = 0; idx < 2; idx++) {
			demo.ep[idx].my_destid = demo.ep[idx].props.hdid;
			demo.ep[idx].remote_destid = demo.ep[idx^1].props.hdid;
		};
		break;
	default: printf("software error!");
		goto exit;
		break;
	}

	if (demo.debug)
		show_rio_devs();

	for (idx = 0; (idx < max_idx) && (demo.cfg != CFG_LPBK); idx++) {
		/* Verify existence of remote RapidIO Endpoint */
		if (riodp_mport_get_ep_list(demo.ep[idx].mport_id, &ep_list,
				&number_of_eps))
			exit(1);

		ep_found = 0;
		for (ep = 0; ep < number_of_eps; ep++) {
			if (ep_list[ep] == demo.ep[idx].remote_destid)
				ep_found = 1;
		}

		riodp_mport_free_ep_list(&ep_list);

		if (!ep_found) {
			printf("Index %d: Could not find remote destID %d\n",
				idx, demo.ep[idx].remote_destid);
			exit(1);
		}
	};

        /* First configure all of the mports.
         * Then, clean them all up by resetting the mport and, if necessary,
         * resetting the link partner and configuring the switch.
         */
	rc = -1;
        for (i = 0; i < max_idx; i++) {
                if (EXIT_FAILURE == config_tsi721(demo.cfg == CFG_LPBK,
                                demo.got_switch, demo.ep[i].fd,
                                demo.debug,
                                demo.ep[i].props.link_speed >= RIO_LINK_500 ))
                        goto exit;

                if (demo.cfg == CFG_LPBK) {
					char rio_device[256];
					DIR *dp;
					int err;

					snprintf(rio_device, sizeof(rio_device), "%s/mport%d_lb", RIO_SYSFS, idx);
					dp = opendir(rio_device);
					if (dp == NULL)	{
						sprintf(rio_device, "mport%d_lb", idx);
						err = riodp_device_add(demo.ep[idx].fd, demo.ep[idx].props.hdid,
												0xff, 0x11223344, rio_device);
						if (err)
							printf("Failed to create loopback device. ERR=%d\n", err);
					} else {
						closedir(dp);
					}
				}
		}

        for (i = 0; i < max_idx; i++) {
                int rst_lp = (i == (max_idx-1)) &&
                        ((demo.cfg == CFG_1_CL) || (demo.cfg == CFG_DUAL));
                if (EXIT_FAILURE == cleanup_tsi721( demo.got_switch,
                                demo.ep[i].fd, demo.debug,
                                demo.ep[i].props.hdid, rst_lp))
                        goto exit;
        }

	rc = 0;
exit:
	return rc;
}

sem_t server_started;
pthread_t server_thread;
pthread_t client_thread;

struct client_parms {
	int idx;
	int wait_for_sema;
	struct timespec tot_time;
	struct timespec min_time;
	struct timespec max_time;
};

static void srv_sig_handler(int signum)
{
	switch(signum) {
	case SIGTERM:
		srv_exit = 1;
		break;
	case SIGINT:
		srv_exit = 1;
		break;
	case SIGUSR1:
		srv_exit = 1;
		break;
	}
}

void *client(void *client_rc)
{
	struct client_parms *p = (struct client_parms *)client_rc;
	int idx = p->idx;
	struct timespec starttime, endtime;
	int ret;
	uint32_t i;

	if (migrate_thread_to_cpu(&client_thread, "CLIENT", 0, demo.debug))
		goto exit;

	printf("%s: Running on RapidIO rio_mport%d\n", __func__, 
			demo.ep[idx].mport_id);

	p->tot_time = p->min_time = p->max_time = (struct timespec){0, 0};

	/* Create riodp_mailbox control structure */
	if (riodp_mbox_create_handle(demo.ep[idx].mport_id, 0, 
				&demo.ep[idx].mailbox)) {
		printf("%s: riodp_mbox_create_handle error\n", __func__);
		exit(1);
	}

	/* Create a socket  structure associated with given mailbox */
	if (riodp_socket_socket(demo.ep[idx].mailbox, &demo.c_sock)) {
		printf("%s: riodp_socket_socket error\n", __func__);
		goto cleanup_handle;
	}

	if (p->wait_for_sema) {
		srv_exit = 0;
		/* Trap signals that we expect to receive */
		signal(SIGINT, srv_sig_handler);
		signal(SIGTERM, srv_sig_handler);
		signal(SIGUSR1, srv_sig_handler);

		sem_wait(&server_started);
		if (srv_exit)
			goto cleanup_handle;
	}

	ret = riodp_socket_connect(demo.c_sock, demo.ep[idx].remote_destid, 0,
				   demo.s_chan_no);
	if (ret == EADDRINUSE)
		printf("riodp_socket_connect: Requested channel already in use,"
			       " reusing...\n");
	else if (ret) {
		printf("%s: riodp_socket_connect error: %d\n", __func__, ret);
		goto cleanup_socket;
	}

	if (riodp_socket_request_send_buffer(demo.c_sock, 
					(void **)&demo.ep[idx].tx_buf))
		goto cleanup_buffers;

	demo.ep[idx].rx_buf = (char *)malloc(MAX_MSG_SIZE);
	if (demo.ep[idx].rx_buf == NULL) {
		printf("%s: error allocating rx buffer\n", __func__); 
		goto cleanup_buffers;
	}

	clock_gettime(CLOCK_MONOTONIC, &starttime);

	for (i = 0; i < demo.repeat; i++) {
		memset((void *)demo.ep[idx].tx_buf, 0, MAX_MSG_SIZE);
		memset((void *)demo.ep[idx].rx_buf, 0, MAX_MSG_SIZE);
		sprintf((char *)(demo.ep[idx].tx_buf + 20), "%8d\n", i);

		if (demo.debug)
			printf("%s: TX_MSG=%s\n",
				__func__, demo.ep[idx].tx_buf + 20);

		__sync_synchronize();
		clock_gettime(CLOCK_MONOTONIC, &starttime);
		__sync_synchronize();

		if (riodp_socket_send(demo.c_sock, demo.ep[idx].tx_buf, 
					demo.size)) {
			printf("%s: riodp_socket_send() ERR\n", __func__ );
			break;
		}

		/* Get echo response from the server */
		if (riodp_socket_receive(demo.c_sock, 
					(void **)&demo.ep[idx].rx_buf, 
					MAX_MSG_SIZE, 0)) {
			printf("%s: riodp_socket_receive() ERR\n", __func__ );
			break;
		}

		__sync_synchronize();
		clock_gettime(CLOCK_MONOTONIC, &endtime);
		__sync_synchronize();

		if (demo.debug)
			printf("%s: RX_MSG=%s\n",
				__func__, demo.ep[idx].rx_buf + 20);

		if (demo.verify) {
			if (strcmp((char *)demo.ep[idx].tx_buf + 20, 
				   (char *)demo.ep[idx].rx_buf + 20)) {
				printf("%s: MSG TRANSFER ERROR:"
					" data corruption detected @ %d\n",
					__func__, i);
				printf("%s: MSG OUT: %s\n",
					__func__, demo.ep[idx].tx_buf + 20);
				printf("%s: MSG IN: %s\n",
					__func__, demo.ep[idx].rx_buf + 20);
				break;
			};
		}

		time_track(i, starttime, endtime, &p->tot_time, &p->min_time,
				&p->max_time);
	}


cleanup_buffers:
	if (demo.ep[idx].rx_buf)
		riodp_socket_release_receive_buffer(demo.c_sock,
						demo.ep[idx].rx_buf);
	if (demo.ep[idx].tx_buf)
		riodp_socket_release_send_buffer(demo.c_sock,
						demo.ep[idx].tx_buf);

cleanup_socket:
	if (riodp_socket_close(&demo.c_sock))
		printf("%s: riodp_socket_close error\n", __func__);

cleanup_handle:
	if (riodp_mbox_destroy_handle(&demo.ep[idx].mailbox))
		printf("%s: riodp_mbox_destroy_handle error\n", __func__);

exit:
	return client_rc;
};

void init_thread_mutex(void)
{
	sem_init(&server_started, 0, 0);
};

int start_client_thread(void)
{
	int client_ret;
	struct client_parms *client_parms = 
		(struct client_parms *)malloc(sizeof(struct client_parms));

	client_parms->idx = 0;
	client_parms->wait_for_sema = 1;

	client_ret = pthread_create(&client_thread, NULL, client, 
				(void *)(client_parms));
	if (client_ret)
		printf("Error - client_thread rc: %d\n", client_ret);

	return client_ret;
};

struct server_parms {
	int idx;
};


void *server(void *server_rc)
{
	struct server_parms *p = (struct server_parms *)server_rc;
	int idx = p->idx;
	int ret = -1;
	uint32_t i;

	/* Trap signals that we expect to receive */
	signal(SIGINT, srv_sig_handler);
	signal(SIGTERM, srv_sig_handler);
	signal(SIGUSR1, srv_sig_handler);

	if (migrate_thread_to_cpu(&server_thread, "SERVER", 1, demo.debug))
		goto exit;

	printf("%s: Running on RapidIO rio_mport%d channel %d\n", __func__, 
			demo.ep[idx].mport_id, demo.s_chan_no);

	/* Create riodp_mailbox control structure */
	if (riodp_mbox_create_handle(demo.ep[idx].mport_id, 0, 
					&demo.ep[idx].mailbox)) {
		printf("%s: riodp_mbox_create_handle ERR\n", __func__);
		goto exit;
	}

	/* Create an unbound socket structure */
	if (riodp_socket_socket(demo.ep[idx].mailbox, &demo.s_sock)) {
		printf("%s: riodp_socket_socket s_sock ERR\n", __func__);
		goto cleanup_handle;
	}

	/* Bind the listen channel to opened MPORT device */
	if (riodp_socket_bind(demo.s_sock, demo.s_chan_no)) {
		printf("%s: riodp_socket_bind ERR\n", __func__);
		goto cleanup_socket;
	}

	/* Initiate LISTEN on the specified channel */
	if (riodp_socket_listen(demo.s_sock)) {
		printf("%s: riodp_socket_listen ERR\n", __func__);
		goto cleanup_socket;
	}

	/* Create new socket object for accept */
	if (riodp_socket_socket(demo.ep[idx].mailbox, &demo.s_c_sock)) {
		printf("%s: riodp_socket_socket s_c_sock ERR\n", __func__);
		goto cleanup_socket;
	}

	/* Allocate rx buffer for accept */
	/* use malloc to get rx buffer, because buffers aren't managed yet */
	demo.ep[idx].rx_buf = (char *)malloc(MAX_MSG_SIZE); 
	if (demo.ep[idx].rx_buf == NULL){
		printf("%s: error allocating rx buffer\n", __func__);
		exit(1);
	}

	sem_post(&server_started);

	/* Wait for client to connect() */
repeat:
	ret = riodp_socket_accept(demo.s_sock, &demo.s_c_sock, 3*60000); // TO = 3 min
	if ((ret == ETIME) && !srv_exit)
		goto repeat;

	if (ret) {
		printf("%s: riodp_socket_accept() ERR %d\n", __func__, ret);
		goto cleanup_socket;
	}

	for (i = 0; (i < demo.repeat) && !srv_exit; i++) {
		ret = riodp_socket_receive(demo.s_c_sock, 
				(void **)&demo.ep[idx].rx_buf, 
				MAX_MSG_SIZE, 0);
		if (ret) {
			printf("%s: riodp_socket_receive() ERR %d (%d)\n",
				__func__, ret, errno);
			break;
		}

		if (demo.debug)
			printf("%s: RX_MSG=%s\n",
				__func__, demo.ep[idx].rx_buf + 20);

		/* Send  a message back to the client */
		ret = riodp_socket_send(demo.s_c_sock, demo.ep[idx].rx_buf, 
					demo.size);
		if (ret) {
			printf("%s: riodp_socket_send() ERR %d (%d)\n",
				__func__, ret, errno);
			break;
		}
	}

	if (demo.ep[idx].rx_buf) 
		riodp_socket_release_receive_buffer(demo.s_c_sock, 
						demo.ep[idx].rx_buf);
	ret = 0;

	/* Exit closing listening channel */
cleanup_socket:
	if (demo.s_sock) {
		ret = riodp_socket_close(&demo.s_sock);
		if (ret)
			printf("%s: riodp_socket_close() s_sock ERR\n", 
					__func__);
	};
	if (demo.s_c_sock) {
		struct riodp_socket *handle = demo.s_c_sock;
		if (handle->cdev.id) {
			ret = riodp_socket_close(&demo.s_c_sock);
			if (ret)
				printf("%s: riodp_socket_close()"
					" s_c_sock channel %d ERR\n",
					__func__, handle->cdev.id);
		};
	};
cleanup_handle:
	/* Release riodp_mailbox control structure */
	if (demo.ep[idx].mailbox) {
		ret = riodp_mbox_destroy_handle(&demo.ep[idx].mailbox);
		if (ret)
			printf("riodp_mbox_shutdown error: %d\n", ret);
	};
exit:
	sem_post(&server_started);
	p->idx = ret;
	return server_rc;
}

int start_server_thread(void)
{
	struct server_parms *s_parms;
	int server_ret;

	s_parms = (struct server_parms *)
		(malloc(sizeof(struct server_parms)));
	s_parms->idx = 1;

	server_ret = pthread_create( &server_thread, NULL, server, 
				(void *)(s_parms));
	if (server_ret)
		printf("Error - server_thread rc: %d\n", server_ret);

	return server_ret;
};

int main(int argc, char** argv)
{
	int ret = 0;
	int last_idx = 0;
	int s_rc = 0, c_rc = 0;
	struct client_parms client_rc, *p_c_parms = &client_rc;
	struct server_parms server_rc, *p_s_parms = &server_rc;

	parse_options(argc, argv);

	if (CFG_DUAL == demo.cfg)
		last_idx = 1;

	if (EXIT_FAILURE == open_mports(last_idx)) /* Succeeds or exits */
		goto close_ports;

	ret = fixup_options();
	if (ret)
		goto exit;

	srv_exit = 0;
	init_thread_mutex();
	signal(SIGINT, srv_sig_handler);
	signal(SIGTERM, srv_sig_handler);
	signal(SIGUSR1, srv_sig_handler);

	switch (demo.cfg) {
	case CFG_LPBK:
	case CFG_DUAL:
		if (start_server_thread())
			goto close_ports;
		if (start_client_thread())
			goto close_ports;
		c_rc = pthread_join(client_thread, (void **)&p_c_parms);
		s_rc = pthread_join(server_thread, (void **)&p_s_parms);
		if (s_rc)
			printf("Server rc: %d\n", s_rc);
		if (c_rc)
			printf("Client rc: %d\n", c_rc);
		break;
	case CFG_1_CL:
		client_rc.idx = 0;
		client_rc.wait_for_sema = 0;
		client_thread=pthread_self();
		p_s_parms = NULL;
		p_c_parms = (struct client_parms *)client((void *)p_c_parms);
		break;
	case CFG_1_SV:
		server_rc.idx = 0;
		server_thread=pthread_self();
		p_s_parms = (struct server_parms *)server((void *)p_s_parms);
		break;
	default: printf("\nIllegal demo.config\n");
		 goto close_ports;
	};

	ret = EXIT_SUCCESS;

	if (p_s_parms)
		printf("\nSERVER rc: %d\n", p_s_parms->idx);

	if ((demo.cfg == CFG_1_SV) || (NULL == p_c_parms))
		goto close_ports;

	printf("CLIENT rc: %d\n", p_c_parms->idx);
	printf("Total time  :\t %9d sec %9d nsec\n", 
			(int)p_c_parms->tot_time.tv_sec,
			(int)p_c_parms->tot_time.tv_nsec);
	printf("\nRound Trip Times:\n");
	printf("Minimum:\t %9d sec %9d nsec\n",
			(int)p_c_parms->min_time.tv_sec,
			(int)p_c_parms->min_time.tv_nsec);
	p_c_parms->tot_time = time_div(p_c_parms->tot_time, demo.repeat);
	printf("Average:\t %9d sec %9d nsec\n",
			(int)p_c_parms->tot_time.tv_sec,
			(int)p_c_parms->tot_time.tv_nsec);
	printf("Maximum:\t %9d sec %9d nsec\n",
			(int)p_c_parms->max_time.tv_sec,
			(int)p_c_parms->max_time.tv_nsec);
	printf("\nOne-way Times:\n");
	p_c_parms->min_time = time_div(p_c_parms->min_time, 2);
	p_c_parms->max_time = time_div(p_c_parms->max_time, 2);
	p_c_parms->tot_time = time_div(p_c_parms->tot_time, 2);
	printf("Minimum:\t %9d sec %9d nsec\n",
			(int)p_c_parms->min_time.tv_sec,
			(int)p_c_parms->min_time.tv_nsec);
	printf("Average:\t %9d sec %9d nsec\n",
			(int)p_c_parms->tot_time.tv_sec,
			(int)p_c_parms->tot_time.tv_nsec);
	printf("Maximum:\t %9d sec %9d nsec\n",
			(int)p_c_parms->max_time.tv_sec,
			(int)p_c_parms->max_time.tv_nsec);

close_ports:
	close_mports(last_idx);
exit:
	exit(ret);
}
