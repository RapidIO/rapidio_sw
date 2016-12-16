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

/**
 * \file riodp_test_client.c
 * \brief Client part of client/server test program for RapidIO channelized messaging.
 *
 * Sends a message to target RapidIO device, receives an echo response from it
 * and verifies response.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <fcntl.h>
#include <rapidio_mport_dma.h>
#include <sys/ioctl.h>

#include "tok_parse.h"
#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_sock.h>

#ifdef __cplusplus
extern "C" {
#endif


/// @cond
struct args {
	uint32_t mport_id;	// local mport ID
	uint32_t remote_destid;	// RapidIO device destination ID
	uint16_t remote_channel;// remote channel number
	uint32_t repeat;	// number of repetitions
};
/// @endcond

static void usage(char *program)
{
	printf("%s - library test client\n", program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("Options are:\n");
	printf("    <mport> local mport device index\n");
	printf("    <destid> target RapidIO device destination ID\n");
	printf("    <channel> channel number on remote RapidIO device\n");
	printf("    <repetitions> number of repetitions (default 1)\n");
	printf("\n");
}

/**
 * \brief Display available devices
 */
void show_rio_devs(void)
{
	uint32_t *mport_list = NULL;
	uint32_t *ep_list = NULL;
	uint32_t *list_ptr;
	uint32_t number_of_eps = 0;
	uint8_t  number_of_mports = RIO_MAX_MPORTS;
	uint32_t ep = 0;
	int i;
	int mport_id;
	int ret = 0;

	/** - request from driver list of available local mport devices */
	ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
	if (ret) {
		printf("ERR: riomp_mgmt_get_mport_list() ERR %d\n", ret);
		return;
	}

	printf("\nAvailable %d local mport(s):\n", number_of_mports);
	if (number_of_mports > RIO_MAX_MPORTS) {
		printf("WARNING: Only %d out of %d have been retrieved\n",
				RIO_MAX_MPORTS, number_of_mports);
	}

	/** - for each local mport display list of remote RapidIO devices */
	list_ptr = mport_list;
	for (i = 0; i < number_of_mports; i++, list_ptr++) {
		mport_id = *list_ptr >> 16;
		printf("+++ mport_id: %u dest_id: %u\n",
				mport_id, *list_ptr & 0xffff);

		/* Display EPs for this MPORT */

		ret = riomp_mgmt_get_ep_list(mport_id, &ep_list, &number_of_eps);
		if (ret) {
			printf("ERR: riodp_ep_get_list() ERR %d\n", ret);
			break;
		}

		printf("\t%u Endpoints (dest_ID): ", number_of_eps);
		for (ep = 0; ep < number_of_eps; ep++)
			printf("%u ", *(ep_list + ep));
		printf("\n");

		ret = riomp_mgmt_free_ep_list(&ep_list);
		if (ret)
			printf("ERR: riodp_ep_free_list() ERR %d\n", ret);

	}

	printf("\n");

	ret = riomp_mgmt_free_mport_list(&mport_list);
	if (ret)
		printf("ERR: riodp_ep_free_list() ERR %d\n", ret);
}

static struct timespec timediff(struct timespec start, struct timespec end)
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

/**
 * \brief Starting point for the program
 *
 * \param[in] argc Command line parameter count
 * \param[in] argv Array of pointers to command line parameter null terminated
 *                 strings
 *
 * \retval 0 means success
 *
 * When running without expected number of arguments displays list of available local
 * and remote devices.
 *
 * Performs the following steps:
 */
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
	riomp_mailbox_t mailbox;
	riomp_sock_t socket = NULL;
	uint32_t i;
	struct timespec starttime;
	struct timespec endtime;
	struct timespec time;
	float totaltime;
	float mean;
	int tmp;

	/** - Parse console arguments */
	/** - If number of arguments is less than expected display help message and list of devices. */
	if (argc < 5) {
		usage(argv[0]);
		show_rio_devs();
		exit(EXIT_FAILURE);
	}

	if (tok_parse_mport_id(argv[1], &arg.mport_id, 0)) {
		printf(TOK_ERR_MPORT_MSG_FMT);
		exit(EXIT_FAILURE);
	}
	if (tok_parse_did(argv[2], &arg.remote_destid, 0)) {
		printf(TOK_ERR_DID_MSG_FMT);
		exit(EXIT_FAILURE);
	}
	if (tok_parse_socket(argv[3], &arg.remote_channel, 0)) {
		printf(TOK_ERR_SOCKET_MSG_FMT, "Remote channel");
		exit(EXIT_FAILURE);
	}
	if (tok_parse_ul(argv[4], &arg.repeat, 0)) {
		printf(TOK_ERR_UL_HEX_MSG_FMT, "Number of repetitions");
		exit(EXIT_FAILURE);
	}

	printf("Start CM_CLIENT (PID %d)\n", (int)getpid());

	/** - Verify existence of remote RapidIO Endpoint */
	ret = riomp_mgmt_get_ep_list(arg.mport_id, &ep_list, &number_of_eps);
	if (ret) {
		printf("riodp_ep_get_list error: %d\n", ret);
		exit(1);
	}

	for (ep = 0; ep < number_of_eps; ep++) {
		if (ep_list[ep] == arg.remote_destid) {
			ep_found = 1;
		}
	}

	ret = riomp_mgmt_free_ep_list(&ep_list);
	if (ret) {
		printf("ERROR: riodp_ep_free_list error: %d\n",	ret);
		exit(1);
	}

	if (!ep_found) {
		printf("CM_CLIENT(%d) invalid remote destID %d\n",
			(int)getpid(), arg.remote_destid);
		exit(1);
	}

	/** - Create rapidio_mport_mailbox control structure */
	ret = riomp_sock_mbox_create_handle(arg.mport_id, 0, &mailbox);
	if (ret) {
		printf("riodp_mbox_init error: %d\n", ret);
		exit(1);
	}

	/** - Create a socket structure associated with given mailbox */
	ret = riomp_sock_socket(mailbox, &socket);
	if (ret) {
		printf("riomp_sock_socket error: %d\n", ret);
		riomp_sock_mbox_destroy_handle(&mailbox);
		exit(1);
	}

	ret = riomp_sock_connect(socket, arg.remote_destid, arg.remote_channel);
	if (ret) {
		if (ret == EADDRINUSE) {
			printf("riomp_sock_connect: Requested channel already in use, reusing...\n");
		} else {
			printf("riomp_sock_connect error: %d\n", ret);
			goto out;
		}
	}

	ret = riomp_sock_request_send_buffer(socket, &msg_tx);
	if (ret) {
		printf("riomp_sock_request_send_buffer error: %d\n", ret);
		goto out;
	}

	msg_rx = malloc(0x1000);
	if (msg_rx == NULL) {
		printf("CM_CLIENT(%d): error allocating rx buffer\n", (int)getpid());
		riomp_sock_release_send_buffer(socket, msg_tx);
		goto out;
	}

	clock_gettime(CLOCK_MONOTONIC, &starttime);

	for (i = 1; i <= arg.repeat; i++) {
		/* usleep(200 * 1000); */
		/** - Place message into buffer with space reserved for msg_header */
		sprintf((char *)((char *)msg_tx + 20), "%d:%d\n", i, (int)getpid());

		/** - Send message to the destination */
		ret = riomp_sock_send(socket, msg_tx, 0x1000);
		if (ret) {
			printf("CM_CLIENT(%d): riomp_sock_send() ERR %d\n",
				(int)getpid(), ret);
			break;
		}

		/** - Get echo response from the server (blocking call, no timeout) */
		ret = riomp_sock_receive(socket, &msg_rx, 0x1000, 0);
		if (ret) {
			printf("CM_CLIENT(%d): riomp_sock_receive() ERR %d on roundtrip %d\n",
				(int)getpid(), ret, i);
			break;
		}

		if (strcmp((char *)msg_tx + 20, (char *)msg_rx + 20)) {
			printf("CM_CLIENT(%d): MSG TRANSFER ERROR: data corruption detected @ %d\n",
				(int)getpid(), i);
			printf("CM_CLIENT(%d): MSG OUT: %s\n",
				(int)getpid(), (char *)msg_tx + 20);
			printf("CM_CLIENT(%d): MSG IN: %s\n",
				(int)getpid(), (char *)msg_rx + 20);
			ret = -1;
			break;
		}
	}

	// free the rx and tx buffers
	riomp_sock_release_receive_buffer(socket, msg_rx);
	riomp_sock_release_send_buffer(socket, msg_tx);

	clock_gettime(CLOCK_MONOTONIC, &endtime);

	if (ret) {
		printf("CM_CLIENT(%d) ERROR.\n",
			(int)getpid());
	} else {
		printf("CM_CLIENT(%d) Test finished.\n",
			(int)getpid());
	}

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

out:
	/** - Close messaging channel */
	tmp = riomp_sock_close(&socket);
	if (tmp) {
		printf("riomp_sock_close error: %d\n", tmp);
	}

	// free the mailbox
	tmp = riomp_sock_mbox_destroy_handle(&mailbox);
	if (tmp) {
		printf("riodp_mbox_shutdown error: %d\n", tmp);
	}
	return ret;
}

#ifdef __cplusplus
}
#endif
