/*
 * Copyright 2015 Integrated Device Technology, Inc.
 *
 * User-space RapidIO Port-Write event test program.
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <rapidio_mport_dma.h>
#include <time.h>
#include <signal.h>

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_sock.h>

static int debug = 0;

static volatile sig_atomic_t rcv_exit;
static volatile sig_atomic_t report_status;

static void db_sig_handler(int signum)
{
	switch(signum) {
	case SIGTERM:
		rcv_exit = 1;
		break;
	case SIGINT:
		rcv_exit = 1;
		break;
	case SIGUSR1:
		report_status = 1;
		break;
	}
}

static int do_pwrcv_test(riomp_mport_t hnd, uint32_t mask, uint32_t low, uint32_t high)
{
	int ret;
	struct riomp_mgmt_event evt;
	unsigned long pw_count = 0, ignored_count = 0;

	ret = riomp_mgmt_pwrange_enable(hnd, mask, low, high);
	if (ret) {
		printf("Failed to enable PW filter, err=%d\n", ret);
		return ret;
	}

	while (!rcv_exit) {

		if (report_status) {
			printf("port writes count: %lu\n", pw_count);
			printf("ignored events count: %lu\n", ignored_count);
			report_status = 0;
		}

		ret = riomp_mgmt_get_event(hnd, &evt);
		if (ret < 0) {
			if (ret == -EAGAIN)
				continue;
			else {
				printf("Failed to read event, err=%d\n", ret);
				break;
			}
		}

		if (evt.header == RIO_EVENT_PORTWRITE) {
			int i;

			printf("\tPort-Write message:\n");
			for (i = 0; i < 16; i +=4) {
				printf("\t0x%02x: 0x%08x %08x %08x %08x\n",
					i*4, evt.u.portwrite.payload[i],
					evt.u.portwrite.payload[i + 1],
					evt.u.portwrite.payload[i + 2],
					evt.u.portwrite.payload[i + 3]);
			}
			printf("\n");
			pw_count++;
		} else {
			printf("\tIgnoring event type %d)\n", evt.header);
			ignored_count++;
		}
	}

	ret = riomp_mgmt_pwrange_disable(hnd, mask, low, high);
	if (ret) {
		printf("Failed to disable PW range, err=%d\n", ret);
		return ret;
	}

	return 0;
}

static void display_help(char *program)
{
	printf("%s - test RapidIO PortWrite events\n",	program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("options are:\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -m xxxx\n");
	printf("    mask (default 0xffffffff)\n");
	printf("  -L xxxx\n");
	printf("    low filter value (default 0)\n");
	printf("  -H xxxx\n");
	printf("    high filter value (default 0xffffffff)\n");
	printf("  -n run in non-blocking mode\n");
	printf("  --help (or -h)\n");
	/*printf("  --debug (or -d)\n");*/
	printf("\n");
}

int main(int argc, char** argv)
{
	uint32_t mport_id = 0;
	uint32_t pw_mask = 0xffffffff;
	uint32_t pw_low = 0;
	uint32_t pw_high = 0xffffffff;
	riomp_mport_t mport_hnd;
	int flags = 0;
	int option;
	static const struct option options[] = {
		{ "mport",  required_argument, NULL, 'M' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};
	char *program = argv[0];
	struct riomp_mgmt_mport_properties prop;
	unsigned int evt_mask;
	int err;
	int rc = EXIT_SUCCESS;

	while (1) {
		option = getopt_long_only(argc, argv,
				"dhnm:M:L:H:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'm':
			pw_mask = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			flags = O_NONBLOCK;
			break;
		case 'M':
			mport_id = strtol(optarg, NULL, 0);
			break;
		case 'L':
			pw_low = strtoul(optarg, NULL, 0);
			break;
		case 'H':
			pw_high = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			debug = 1;
			break;
		case 'h':
		default:
			display_help(program);
			exit(EXIT_SUCCESS);
		}
	}

	rc = riomp_mgmt_mport_create_handle(mport_id, flags, &mport_hnd);
	if (rc < 0) {
		printf("DB Test: unable to open mport%d device err=%d\n",
			mport_id, rc);
		exit(EXIT_FAILURE);
	}

	if (!riomp_mgmt_query(mport_hnd, &prop)) {
		riomp_mgmt_display_info(&prop);

		if (prop.link_speed == 0) {
			printf("SRIO link is down. Test aborted.\n");
			rc = EXIT_FAILURE;
			goto out;
		}
	} else {
		printf("Failed to obtain mport information\n");
		printf("Using default configuration\n\n");
	}

	/* Trap signals that we expect to receive */
	signal(SIGINT,  db_sig_handler);
	signal(SIGTERM, db_sig_handler);
	signal(SIGUSR1, db_sig_handler);

	err = riomp_mgmt_get_event_mask(mport_hnd, &evt_mask);
	if (err) {
		printf("Failed to obtain current event mask, err=%d\n", err);
		rc = EXIT_FAILURE;
		goto out;
	}

	riomp_mgmt_set_event_mask(mport_hnd, evt_mask | RIO_EVENT_PORTWRITE);

	printf("+++ RapidIO PortWrite Event Receive Mode +++\n");
	printf("\tmport%d PID:%d\n", mport_id, (int)getpid());
	printf("\tfilter: mask=%x low=%x high=%x\n",
		pw_mask, pw_low, pw_high);

	do_pwrcv_test(mport_hnd, pw_mask, pw_low, pw_high);

	riomp_mgmt_set_event_mask(mport_hnd, evt_mask);

out:
	riomp_mgmt_mport_destroy_handle(&mport_hnd);
	exit(rc);
}