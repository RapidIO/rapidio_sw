/*
 * Copyright 2015 Integrated Device Technology, Inc.
 *
 * User-space RapidIO kernel device object creation/removal test program.
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
 * \file riodp_test_devs.c
 * \brief RapidIO kernel device object creation/removal test program.
 *
 * Usage:
 *   ./riodp_test_devs [options]
 *
 * Options are:
 * - -M mport_id | --mport mport_id : local mport device index (default=0)
 * - -c : create device using provided parameters (-D, -H, -T and -N)
 * - -d delete device using provided parameters (-D, -H, -T and -N)
 * - -D xxxx | --destid xxxx : destination ID of RapidIO device
 * - -H xxxx | --hop xxxx : hop count to RapidIO device (default 0xff)
 * - -T xxxx | --tag xxxx : component tag of RapidIO device
 * - -N <device_name> | --name <device_name> : RapidIO device name
 * - --help (or -h)
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
#include <rapidio_mport_dma.h>

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_sock.h>

/// Max device name size in characters.
#define RIODP_MAX_DEV_NAME_SZ 20

static riomp_mport_t mport_hnd;
static uint16_t tgt_destid;
static uint8_t tgt_hop;
static uint32_t comptag = 0;

static char dev_name[RIODP_MAX_DEV_NAME_SZ + 1];

/**
 * \brief Called by main() when device object create operation is requested
 *
 * Calls mport API function to create kernel space device object. New device
 * object will be created in rapidio-specific sysfs location:
 * "/sys/bus/rapidio/devices".
 * The device object will have sysfs attributes compatible to ones created
 * by kernel mode enumeration. If device name is not provided as a command
 * line parameter it will be generated automatically according to rapidio
 * device name format defined by kernel enumeration.
 */
void test_create(void)
{
	int ret;

	ret = riomp_mgmt_device_add(mport_hnd, tgt_destid, tgt_hop, comptag,
			       (*dev_name == '\0')?NULL:dev_name);
	if(ret)
		printf("Failed to create device object, err=%d\n", ret);
}

/**
 * \brief Called by main() when device object delete operation is requested
 *
 * Calls mport API function to delete kernel space device object. The device
 * object will be deleted regardless of how it was created: by kernel-space
 * enumerator or user-space application using create API (see test_create()).
 */
void test_delete(void)
{
	int ret;

	ret = riomp_mgmt_device_del(mport_hnd, tgt_destid, tgt_hop, comptag,
			       (*dev_name == '\0')?NULL:dev_name);
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

/**
 * \brief Starting point for the test program
 *
 * \param[in] argc Command line parameter count
 * \param[in] argv Array of pointers to command line parameter null terminated
 *                 strings
 *
 * \retval 0 means success
 */
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
	struct riomp_mgmt_mport_properties prop;
	int rc = EXIT_SUCCESS;
	int err;

	/** - Parse command line options */
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

	/** - Open mport devide and query RapidIO link status.
          Exit if link is not active */
	err = riomp_mgmt_mport_create_handle(mport_id, 0, &mport_hnd);
	if (err < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, err);
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

	err = riomp_mgmt_lcfg_read(mport_hnd, 0x13c, sizeof(uint32_t), &regval);
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
		err = riomp_mgmt_lcfg_read(mport_hnd, 0x60, sizeof(uint32_t), &regval);
		prop.hdid = (regval >> 16) & 0xff;
		err = riomp_mgmt_destid_set(mport_hnd, prop.hdid);
		if (err)
			printf("Failed to update local destID, err=%d\n", err);
		else
			printf("Updated destID=0x%x\n", prop.hdid);
	}

	/** - Perform the specified operation. */
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

	riomp_mgmt_mport_destroy_handle(&mport_hnd);
	exit(rc);
}
