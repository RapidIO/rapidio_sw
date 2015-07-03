/*
 * Copyright 2015 Integrated Device Technology, Inc.
 *
 * User-space RapidIO DoorBell exchange test program.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

#include "rapidio_mport_lib.h"

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

static int do_dbrcv_test(int fd, uint32_t rioid, uint16_t start, uint16_t end)
{
#if 0
	int ret;
	struct rio_event evt;

	ret = riodp_dbrange_enable(fd, rioid, start, end);
	if (ret) {
		printf("Failed to enable DB range, err=%d\n", ret);
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

		if (evt.header == RIO_DOORBELL)
			printf("\tDB 0x%04x from destID %d\n",
				evt.u.doorbell.payload, evt.u.doorbell.rioid);
		else
			printf("\tIgnoring event type %d)\n", evt.header);
	}

	ret = riodp_dbrange_disable(fd, rioid, start, end);
	if (ret) {
		printf("Failed to disable DB range, err=%d\n", ret);
		return ret;
	}
#endif
	return 0;
}

static int do_dbsnd_test(int fd, uint32_t rioid, uint16_t dbval)
{
#if 0
	struct rio_event evt;
	int ret = 0;

	evt.header = RIO_DOORBELL;
	evt.u.doorbell.rioid = rioid;
	evt.u.doorbell.payload = dbval;

	ret = write(fd, &evt, sizeof(evt));
	if (ret < 0)
		printf("Write DB event failed, err=%d\n", errno);
	else
		ret = 0;

	return ret;
#else
	return 0;
#endif
}

static void display_help(char *program)
{
	printf("%s - test RapidIO DoorBell exchange\n",	program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("options are:\n");
	printf("Common:\n");
	printf("  -D xxxx\n");
	printf("  --destid xxxx\n");
	printf("    destination ID of sending/receiving RapidIO device\n");
	printf("    (defaults Rx:any, Tx:none)\n");
	printf("  --help (or -h)\n");
	/*printf("  --debug (or -d)\n");*/
	printf("Sender:\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -I xxxx\n");
	printf("    DoorBell Info field value (default 0x5a5a)\n");
	printf("Receiver:\n");
	printf("  -r run in DB recever mode\n");
	printf("  -n run recever in non-blocking mode\n");
	printf("  -S xxxx\n");
	printf("    start of doorbell range (default 0x5a5a)\n");
	printf("  -E xxxx\n");
	printf("    end of doorbell range (default 0x5a5a)\n");
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
	uint32_t rio_destid = 0xffffffff;
	uint32_t db_info = 0x5a5a;
	uint32_t db_start = 0x5a5a;
	uint32_t db_end = 0x5a5a;
	int fd;
	int flags = 0;
	int option;
	int do_dbrecv = 0;
	static const struct option options[] = {
		{ "destid", required_argument, NULL, 'D' },
		{ "mport",  required_argument, NULL, 'M' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};
	char *program = argv[0];
	struct riodp_mport_properties prop;
	struct sigaction action;
	int rc = EXIT_SUCCESS;

	while (1) {
		option = getopt_long_only(argc, argv,
				"rdhnD:I:M:S:E:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'D':
			rio_destid = strtol(optarg, NULL, 0);
			break;
		case 'r':
			do_dbrecv = 1;
			break;
		case 'n':
			flags = O_NONBLOCK;
			break;
		case 'I':
			db_info = strtol(optarg, NULL, 0);
			break;
		case 'M':
			mport_id = strtol(optarg, NULL, 0);
			break;
		case 'S':
			db_start = strtol(optarg, NULL, 0);
			break;
		case 'E':
			db_end = strtol(optarg, NULL, 0);
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

	if (!do_dbrecv && rio_destid == 0xffffffff) {
		printf("\tPlease specify RIO Target DestID to Send a DB\n");
		exit(EXIT_SUCCESS);
	}

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = test_sigaction;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGIO, &action, NULL);

	fd = riodp_mport_open(mport_id, flags);
	if (fd < 0) {
		printf("DB Test: unable to open mport%d device err=%d\n",
			mport_id, errno);
		exit(EXIT_FAILURE);
	}

	if (!riodp_mport_query(fd, &prop)) {
		riodp_mport_display_info(&prop);

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

	riodp_set_event_mask(fd, RIO_EVENT_DOORBELL);

	if (do_dbrecv) {
		printf("+++ RapidIO Doorbell Receive Mode +++\n");
		printf("\tmport%d PID:%d\n", mport_id, (int)getpid());
		printf("\tfilter: destid=%x start=%x end=%x\n",
			rio_destid, db_start, db_end);

		do_dbrcv_test(fd, rio_destid, db_start, db_end);
	} else {
		printf("+++ RapidIO Doorbell Send +++\n");
		printf("\tmport%d destID=%d db_info=0x%x\n",
			mport_id, rio_destid, db_info);

		do_dbsnd_test(fd, rio_destid, db_info);
	}

out:
	close(fd);
	exit(rc);
}
