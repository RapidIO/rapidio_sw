/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * User-space RIO_CM client test program.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License(GPL) Version 2, or the BSD-3 Clause license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "rapidio_mport_lib.h"

#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */

struct args {
	uint32_t mport_id;
	uint16_t remote_destid;
	uint16_t remote_channel;
	uint32_t repeat;
};

static void usage(char *name)
{
	printf("riodp library test client\n");
	printf("Usage:\n");
	printf("    %s <loc_mport> <rem_destid> <rem_ch> <rep_num>\n", name);
}

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

	ret = riodp_mport_get_mport_list(&mport_list, &number_of_mports);
	if (ret) {
		printf("ERR: riodp_mport_get_mport_list() ERR %d\n", ret);
		return;
	}

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

		ret = riodp_mport_get_ep_list(mport_id, &ep_list, &number_of_eps);
		if (ret) {
			printf("ERR: riodp_ep_get_list() ERR %d\n", ret);
			break;
		}

		printf("\t%u Endpoints (dest_ID): ", number_of_eps);
		for (ep = 0; ep < number_of_eps; ep++)
			printf("%u ", *(ep_list + ep));
		printf("\n");

		ret = riodp_mport_free_ep_list(&ep_list);
		if (ret)
			printf("ERR: riodp_ep_free_list() ERR %d\n", ret);

	}

	printf("\n");

	ret = riodp_mport_free_mport_list(&mport_list);
	if (ret)
		printf("ERR: riodp_ep_free_list() ERR %d\n", ret);
}

struct timespec timediff(struct timespec start, struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

int main(int argc, char** argv)
{
	int ret = 0;
	uint32_t ep = 0;
	uint32_t number_of_eps = 0;
	uint32_t *ep_list = NULL;
	int ep_found = 0;
	void *msg_rx = NULL; 
	void *msg_tx = NULL;
	struct args arg;
	riodp_mailbox_t mailbox;
	riodp_socket_t socket;
	uint32_t i;
	struct timespec starttime;
	struct timespec endtime;
	struct timespec time;
	float totaltime;
	float mean;
	char* eth_emu = getenv("RIODP_EMU_IP_PREFIX");

	/* Parse console arguments */
	if (argc < 5) {
		usage(argv[0]);
		if (eth_emu == NULL) /* no IP-prefix set, so RapidIO is used */
			show_rio_devs();
		else
			printf("CM_CLIENT: RIODP_EMU_IP_PREFIX found, using ethernet...\n");
		exit(1);
	}
	arg.mport_id       = strtoul(argv[1], NULL, 10);
	arg.remote_destid  = strtoul(argv[2], NULL, 10);
	arg.remote_channel = strtoul(argv[3], NULL, 10);
	arg.repeat         = strtoul(argv[4], NULL, 10);

	printf("Start CM_CLIENT (PID %d)\n", (int)getpid());

	if (eth_emu == NULL) /* no IP-prefix set, so RapidIO is used */
	{
		/* Verify existence of remote RapidIO Endpoint */
		ret = riodp_mport_get_ep_list(arg.mport_id, &ep_list, &number_of_eps);
		if (ret) {
			printf("riodp_ep_get_list error: %d\n", ret);
			exit(1);
		}

		for (ep = 0; ep < number_of_eps; ep++) {
			if (ep_list[ep] == arg.remote_destid)
				ep_found = 1;
		}

		ret = riodp_mport_free_ep_list(&ep_list);
		if (ret) {
			printf("ERROR: riodp_ep_free_list error: %d\n",	ret);
		}

		if (!ep_found) {
			printf("CM_CLIENT(%d) invalid remote destID %d\n",
				(int)getpid(), arg.remote_destid);
			exit(1);
		}
	} else {
		printf("CM_CLIENT: RIODP_EMU_IP_PREFIX found, using ethernet...\n");
	}

	/* Create riodp_mailbox control structure */
	ret = riodp_mbox_create_handle(arg.mport_id, 0, &mailbox);
	if (ret) {
		printf("riodp_mbox_init error: %d\n", ret);
		exit(1);
	}

	/* Create a socket  structure associated with given mailbox */
	ret = riodp_socket_socket(mailbox, &socket);
	if (ret) {
		printf("riodp_socket_socket error: %d\n", ret);
		goto out;
	}

	ret = riodp_socket_connect(socket, arg.remote_destid, 0,
				   arg.remote_channel);
	if (ret == EADDRINUSE)
		printf("riodp_socket_connect: Requested channel already in use, reusing...\n");
	else if (ret) {
		printf("riodp_socket_connect error: %d\n", ret);
		goto out;
	}

	ret = riodp_socket_request_send_buffer(socket, &msg_tx);
	if (ret) {
		printf("riodp_socket_request_send_buffer error: %d\n", ret);
		goto out;
	}

	msg_rx = malloc(0x1000);
	if (msg_rx == NULL) {
		printf("CM_CLIENT(%d): error allocating rx buffer\n", (int)getpid());
		goto out;
	}

	clock_gettime(CLOCK_MONOTONIC, &starttime);

	for (i = 1; i <= arg.repeat; i++) {
		/* usleep(200 * 1000); */
		/* Place message into buffer with space reserved for msg_header */
		sprintf((char *)((char *)msg_tx + 20), "%d:%d\n", i, (int)getpid());

		ret = riodp_socket_send(socket, msg_tx, 0x1000);
		if (ret) {
			printf("CM_CLIENT(%d): riodp_socket_send() ERR %d\n",
				(int)getpid(), ret);
			break;
		}

		/* Get echo response from the server (blocking call, no timeout) */
		ret = riodp_socket_receive(socket, &msg_rx, 0x1000, 0);
		if (ret) {
			printf("CM_CLIENT(%d): riodp_socket_receive() ERR %d on roundtrip %d\n",
				(int)getpid(), ret, i);
			if (msg_rx)
				riodp_socket_release_receive_buffer(socket, msg_rx);
			break;
		}

		if (strcmp((char *)msg_tx + 20, (char *)msg_rx + 20)) {
			printf("CM_CLIENT(%d): MSG TRANSFER ERROR: data corruption detected @ %d\n",
				(int)getpid(), i);
			printf("CM_CLIENT(%d): MSG OUT: %s\n",
				(int)getpid(), (char *)msg_tx + 20);
			printf("CM_CLIENT(%d): MSG IN: %s\n",
				(int)getpid(), (char *)msg_rx + 20);
			riodp_socket_release_receive_buffer(socket, msg_rx);
			ret = -1;
			break;
		}
	}

	riodp_socket_release_receive_buffer(socket, msg_rx);
	riodp_socket_release_send_buffer(socket, msg_tx);

	clock_gettime(CLOCK_MONOTONIC, &endtime);

	if (ret)
		printf("CM_CLIENT(%d) ERROR.\n",
			(int)getpid());
	else
		printf("CM_CLIENT(%d) Test finished.\n",
			(int)getpid());

	/* getchar(); */
	time 	  = timediff(starttime,endtime);
	totaltime = ((double) time.tv_sec + (time.tv_nsec / 1000000000.0));
	mean	  = totaltime/arg.repeat * 1000.0; /* mean in us */

	printf("Total time:\t\t\t\t%4f s\n"
	       "Mean time per message roundtrip:\t%4f us\n"
	       "Data throughput:\t\t\t\%4f MB/s\n",
	       totaltime,
	       mean,
	       ((4096*i)/totaltime)/(1024*1024));


	ret = riodp_socket_close(&socket);
	if (ret)
		printf("riodp_socket_close error: %d\n", ret);

out:
	ret = riodp_mbox_destroy_handle(&mailbox);
	if (ret)
		printf("riodp_mbox_shutdown error: %d\n", ret);
	return ret;
}
