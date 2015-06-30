/*
 * Copyright 2015 Integrated Device Technology, Inc.
 *
 * User-space RapidIO kernel device object creation/removal test program.
 *
 * This program uses code fragments from Linux kernel dmaengine
 * framework test driver developed by Atmel and Intel. Please, see
 * drivers/dma/dmatest.c file in Linux kernel source code tree for more
 * details.
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
#include <pthread.h>

#include "rapidio_mport_lib.h"

#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */
#define RIODP_MAX_DEV_NAME_SZ 20 /* max number of RIO mports supported by platform */

static int fd;
static uint16_t tgt_destid;
static uint8_t tgt_hop;
static uint32_t comptag = 0;
static int exit_no_dev;

static char dev_name[RIODP_MAX_DEV_NAME_SZ + 1];

void test_create(void)
{
	int ret;

	ret = riodp_device_add(fd, tgt_destid, tgt_hop, comptag,
			       (*dev_name == '\0')?NULL:dev_name);
	if(ret)
		printf("Failed to create device object, err=%d\n", ret);
}

void test_delete(void)
{
	int ret;

	ret = riodp_device_del(fd, tgt_destid, tgt_hop, comptag);
	if(ret)
		printf("Failed to delete device object, err=%d\n", ret);
}

static void display_help(char *program)
{
	printf("%s - test device object creation/removal\n",	program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("available options:\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  -c create device using provided parameters (-D, -H, -T and -N)\n");
	printf("  -d delete device using provided parameters (-D, -H, -T and -N)\n");
	printf("  -D xxxx\n");
	printf("  --destid xxxx\n");
	printf("    destination ID of target RapidIO device\n");
	printf("  -H xxxx\n");
	printf("  --hop xxxx\n");
	printf("    hop count to target RapidIO device (default 0xff)\n");
	printf("  -T xxxx\n");
	printf("  --tag xxxx\n");
	printf("    component tag of target RapidIO device\n");
	printf("  -N <device_name>\n");
	printf("  --name <device_name>\n");
	printf("    RapidIO device name\n");
	printf("  --help (or -h)\n");
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
	int option;
	int do_create = 0;
	int do_delete = 0;
	int discovered = 0;
	uint32_t regval;
	static const struct option options[] = {
		{ "mport",  required_argument, NULL, 'M' },
		{ "destid", required_argument, NULL, 'D' },
		{ "hop",  required_argument, NULL,   'H' },
		{ "tag", required_argument, NULL,    'T' },
		{ "name",   required_argument, NULL, 'N' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};
	char *program = argv[0];
	struct rio_mport_properties prop;
	struct sigaction action;
	int rc = EXIT_SUCCESS;
	int err;

	while (1) {
		option = getopt_long_only(argc, argv,
				"dhcM:D:H:T:N:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
			/* Data Transfer Mode options*/
		case 'M':
			mport_id = strtol(optarg, NULL, 0);
			break;
		case 'D':
			tgt_destid = strtol(optarg, NULL, 0);
			break;
		case 'H':
			tgt_hop = strtoull(optarg, NULL, 0);
			break;
		case 'T':
			comptag = strtol(optarg, NULL, 0);
			break;
		case 'N':
			strncpy(dev_name, optarg, RIODP_MAX_DEV_NAME_SZ);
			break;
		case 'c':
			do_create = 1;
			break;
		case 'd':
			do_delete = 1;
			break;
		case 'h':
		default:
			display_help(program);
			exit(EXIT_SUCCESS);
		}
	}

	if (do_create && do_delete) {
		printf("%s: Unable to create and delete device object simultaneously\n",
			program);
		exit(EXIT_FAILURE);
	}

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = test_sigaction;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGIO, &action, NULL);

	fd = riodp_mport_open(mport_id, 0);
	if (fd < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, errno);
		exit(EXIT_FAILURE);
	}

	if (!riodp_query_mport(fd, &prop)) {
		display_mport_info(&prop);

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

	err = riodp_lcfg_read(fd, 0x13c, sizeof(uint32_t), &regval);
	if (err) {
		printf("Failed to read from PORT_GEN_CTL_CSR, err=%d\n", err);
		rc = EXIT_FAILURE;
		goto out;
	}

	if (regval & 0x20000000)
		discovered = 1;
	else
		printf("ATTN: Port DISCOVERED flag is not set\n");

	if (discovered && prop.hdid == 0xffff ) {
		err = riodp_lcfg_read(fd, 0x60, sizeof(uint32_t), &regval);
		prop.hdid = (regval >> 16) & 0xff;
		err = riodp_destid_set(fd, prop.hdid);
		if (err)
			printf("Failed to update local destID, err=%d\n", err);
		else
			printf("Updated destID=0x%x\n", prop.hdid);
	}

	printf("[PID:%d]\n", (int)getpid());
	if (do_create) {
		printf("+++ Create RapidIO device object as specified +++\n");
		printf("\tmport%d destID=0x%x hop_count=%d CTag=0x%x",
			mport_id, tgt_destid, tgt_hop, comptag);
		if (strlen(dev_name))
			printf(" name=%s\n", dev_name);
		else
			printf("\n");

		test_create();

	} else if (do_delete) {
		printf("+++ Delete RapidIO device object as specified +++\n");
		printf("\tmport%d destID=0x%x hop_count=%d CTag=0x%x",
			mport_id, tgt_destid, tgt_hop, comptag);
		if (strlen(dev_name))
			printf(" name=%s\n", dev_name);
		else
			printf("\n");

		test_delete();
	} else
		printf("Please specify the action to perform\n");

out:
	close(fd);
	exit(rc);
}
