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
#include <time.h>
#include <signal.h>

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_rdma.h>
#include <rapidio_mport_sock.h>

static int debug = 0;
static int exit_no_dev;

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

static int do_pwrcv_test(int fd, uint32_t mask, uint32_t low, uint32_t high)
{
#if 0
	int ret;
	struct rio_event evt;

	ret = riomp_mgmt_pwrange_enable(fd, mask, low, high);
	if (ret) {
		printf("Failed to enable PW filter, err=%d\n", ret);
		return ret;
	}

	while (!rcv_exit) {
		if (exit_no_dev) {
			printf(">>> Device removal signaled <<<\n");
			break;
		}

		ret = read(fd, &evt, sizeof(struct rio_event));
		if (ret < 0) {
			if (errno == EAGAIN)
				continue;
			else {
				printf("Failed to read event, err=%d\n", errno);
				break;
			}
		}

		if (evt.header == RIO_PORTWRITE) {
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
		}
		else
			printf("\tIgnoring event type %d)\n", evt.header);
	}

	ret = riomp_mgmt_pwrange_disable(fd, mask, low, high);
	if (ret) {
		printf("Failed to disable PW range, err=%d\n", ret);
		return ret;
	}
#endif
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

static void test_sigaction(int sig, siginfo_t *siginfo, void *context)
{
	printf ("SIGIO info PID: %ld, UID: %ld CODE: 0x%x BAND: 0x%lx FD: %d\n",
			(long)siginfo->si_pid, (long)siginfo->si_uid, siginfo->si_code,
			siginfo->si_band, siginfo->si_fd);
	exit_no_dev = 1;
}

int main(int argc, char** argv)
{
	uint32_t mport_id = 0;
	uint32_t pw_mask = 0xffffffff;
	uint32_t pw_low = 0;
	uint32_t pw_high = 0xffffffff;
	int fd;
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
	struct sigaction action;
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

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = test_sigaction;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGIO, &action, NULL);

	fd = riomp_mgmt_mport_open(mport_id, flags);
	if (fd < 0) {
		printf("DB Test: unable to open mport%d device err=%d\n",
			mport_id, errno);
		exit(EXIT_FAILURE);
	}

	if (!riomp_mgmt_query(fd, &prop)) {
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

	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | FASYNC);

	/* Trap signals that we expect to receive */
	signal(SIGINT,  db_sig_handler);
	signal(SIGTERM, db_sig_handler);
	signal(SIGUSR1, db_sig_handler);

	err = riomp_mgmt_get_event_mask(fd, &evt_mask);
	if (err) {
		printf("Failed to obtain current event mask, err=%d\n", err);
		rc = EXIT_FAILURE;
		goto out;
	}

	riomp_mgmt_set_event_mask(fd, evt_mask | RIO_EVENT_PORTWRITE);

	printf("+++ RapidIO PortWrite Event Receive Mode +++\n");
	printf("\tmport%d PID:%d\n", mport_id, (int)getpid());
	printf("\tfilter: mask=%x low=%x high=%x\n",
		pw_mask, pw_low, pw_high);

	do_pwrcv_test(fd, pw_mask, pw_low, pw_high);

	riomp_mgmt_set_event_mask(fd, evt_mask);

out:
	close(fd);
	exit(rc);
}
