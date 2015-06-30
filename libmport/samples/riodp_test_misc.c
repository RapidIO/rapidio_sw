/*
 * Copyright 2014 Integrated Device Technology, Inc.
 *
 * User-space RapidIO basic ops test program.
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

#include "rapidio_mport_lib.h"

#define RIODP_MAX_MPORTS 8 /* max number of RIO mports supported by platform */

/*
 * Initialization patterns. All bytes in the source buffer has bit 7
 * set, all bytes in the destination buffer has bit 7 cleared.
 *
 * Bit 6 is set for all bytes which are to be copied by the DMA
 * engine. Bit 5 is set for all bytes which are to be overwritten by
 * the DMA engine.
 *
 * The remaining bits are the inverse of a counter which increments by
 * one for each byte address.
 */
#define PATTERN_SRC		0x80
#define PATTERN_DST		0x00
#define PATTERN_COPY		0x40
#define PATTERN_OVERWRITE	0x20
#define PATTERN_COUNT_MASK	0x1f

/* Maximum amount of mismatched bytes in buffer to print */
#define MAX_ERROR_COUNT		32

#define TEST_BUF_SIZE (2 * 1024 * 1024)
#define DEFAULT_IBWIN_SIZE (2 * 1024 * 1024)

static int debug = 0;

static void display_help(char *program)
{
	printf("%s - test register operations to/from RapidIO device\n", program);
	printf("Usage:\n");
	printf("  %s [options]\n", program);
	printf("Options are:\n");
	printf("  -M mport_id\n");
	printf("  --mport mport_id\n");
	printf("    local mport device index (default=0)\n");
	printf("  --debug (or -d)\n");
	printf("  --help (or -h)\n");
	printf("  -D xxxx\n");
	printf("  --destid xxxx\n");
	printf("    destination ID of target RapidIO device\n");
	printf("    If not specified access to local mport registers\n");
	printf("  -H xxxx\n");
	printf("  --hop xxxx\n");
	printf("    hop count target RapidIO device (default 0xff)\n");
	printf("  -S xxxx\n");
	printf("  --size xxxx\n");
	printf("    data transfer size in bytes (default 4)\n");
	printf("  -O xxxx\n");
	printf("  --offset xxxx\n");
	printf("    offset in register space (default=0)\n");
	printf("  -w\n");
	printf("    perform write operation\n");
	printf("  -V xxxx\n");
	printf("  --data xxxx\n");
	printf("    32-bit value to write into the device register\n");
	printf("  -q\n");
	printf("    query mport attributes\n");
	printf("\n");
}

int main(int argc, char** argv)
{
	uint32_t mport_id = 0;
	int option;
	static const struct option options[] = {
		{ "mport",  required_argument, NULL, 'M' },
		{ "destid", required_argument, NULL, 'D' },
		{ "hop",    required_argument, NULL, 'H' },
		{ "offset", required_argument, NULL, 'O' },
		{ "size",   required_argument, NULL, 'S' },
		{ "data",   required_argument, NULL, 'V' },
		{ "debug",  no_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};
	char *program = argv[0];
	struct rio_mport_properties prop;
	int fd;
	uint32_t tgt_destid = 0;
	uint32_t tgt_hc = 0xff;
	uint32_t tgt_remote = 0, tgt_write = 0, do_query = 0;
	uint32_t offset = 0;
	uint32_t op_size = sizeof(uint32_t);
	uint32_t data;
	int rc = EXIT_SUCCESS;

	while (1) {
		option = getopt_long_only(argc, argv,
				"wdhqH:D:O:M:S:V:", options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'D':
			tgt_destid = strtol(optarg, NULL, 0);
			tgt_remote = 1;
			break;
		case 'H':
			tgt_hc = strtol(optarg, NULL, 0);
			break;
		case 'O':
			offset = strtol(optarg, NULL, 0);
			break;
		case 'S':
			op_size = strtol(optarg, NULL, 0);
			break;
		case 'M':
			mport_id = strtol(optarg, NULL, 0);
			break;
		case 'V':
			data = strtoul(optarg, NULL, 0);
			break;
		case 'w':
			tgt_write = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'q':
			do_query = 1;
			break;
		case 'h':
		default:
			display_help(program);
			exit(EXIT_SUCCESS);
		}
	}

	fd = riodp_mport_open(mport_id, 0);
	if (fd < 0) {
		printf("DMA Test: unable to open mport%d device err=%d\n",
			mport_id, errno);
		exit(EXIT_FAILURE);
	}

	if (do_query) {
		rc = riodp_query_mport(fd, &prop);
		if (!rc) {
			display_mport_info(&prop);
			if (prop.link_speed == 0)
				printf("SRIO link is down. Test aborted.\n");
		}

		goto out;
	}

	if (tgt_remote) {
		if (tgt_write) {
			if (debug)
				printf("Write to dest=0x%x hc=0x%x offset=0x%x data=0x%08x\n",
					tgt_destid, tgt_hc, offset, data);
			rc = riodp_maint_write(fd, tgt_destid, tgt_hc, offset,
						op_size, data);
		} else {
			if (debug)
				printf("Read from dest=0x%x hc=0x%x offset=0x%x\n",
					tgt_destid, tgt_hc, offset);
			rc = riodp_maint_read(fd, tgt_destid, tgt_hc, offset,
						op_size, &data);
			if (!rc)
				printf("\tdata = 0x%08x\n", data);
		}
	} else {
		if (tgt_write) {
			if (debug)
				printf("Write to local offset=0x%x data=0x%08x\n",
					offset, data);
			rc = riodp_lcfg_write(fd, offset, op_size, data);
		} else {
			if (debug)
				printf("Read from local offset=0x%x\n", offset);
			rc = riodp_lcfg_read(fd, offset, op_size, &data);
			if (!rc)
				printf("\tdata = 0x%08x\n", data);
		}
	}

out:
	if (rc) {
		printf("ERR %d\n", rc);
		rc = EXIT_FAILURE;
	} else {
		rc = EXIT_SUCCESS;
	}

	close(fd);
	exit(rc);
}
