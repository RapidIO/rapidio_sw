/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../inc/librio_maint.h"

#define DEVNR	0

static rio_maint_handle handle;

void usage(const char *program)
{
	printf("Usage:\n");
	printf("\t%s <local> <read> <offset> <word count>\n", program);
	printf("\t%s <local> <write> <offset> <value0>[[:value1]:...]\n", program);
	printf("\t%s <remote> <read> <dest_id> <hop_count> <offset> <word count>\n", program);
	printf("\t%s <remote> <write> <dest_id> <hop_count> <offset> <value0>[[:value1]:...]\n", program);
}

int read_local(uint32_t offset, uint32_t word_count)
{
	int res = 0;
	uint32_t data;
	uint32_t i;

	for (i = 0; i < word_count; i++) {
		res = rio_maint_read_local(handle, offset + i * 4, &data);
		if (res != 0)
			goto exit;
		printf("0x%08X: 0x%08X\n", offset + i * 4, data);
	}
exit:
	return res;
}

int write_local(uint32_t start, char *values)
{
	int res = 0;
	uint32_t data;
	uint32_t addr = start;

	char *token = strtok(values, ":");
	data = strtoul(token, NULL, 0);
	res = rio_maint_write_local(handle, addr, data);
	if (res != 0) {
		fprintf(stderr, "could not write 0x%08x to 0x%08x\n", data, addr);
		goto exit;
	}
	printf("0x%08x: 0x%08x\n", addr, data);

	while ((token = strtok(NULL, ":")) && (token !=NULL)) {
		data = strtoul(token, NULL, 0);
		addr += 4;
		res = rio_maint_write_local(handle, addr, data);
		if (res != 0) {
			fprintf(stderr, "could not write 0x%08x to 0x%08x\n", data, addr);
			goto exit;
		}
		printf("0x%08x: 0x%08x\n", addr, data);


	}

exit:
	return res;
}

int read_remote(uint32_t dest_id, uint32_t hop_count, uint32_t start, uint32_t word_count)
{
	int res;
	uint32_t data[word_count];
	uint32_t i;

	res = rio_maint_read_remote(handle, dest_id, hop_count, start, data,
			word_count);
	if (res != 0)
		goto exit;

	for (i = 0; i < word_count; i++)
		printf("0x%08X: 0x%08X\n", start + i, data[i]);

exit:
	return res;
}

int write_remote(uint32_t dest_id, uint32_t hop_count, uint32_t offset, char *values)
{
	int res = 0;
	uint32_t data;
	uint32_t addr = offset;

	char *token = strtok(values, ":");
	data = strtoul(token, NULL, 0);
	res = rio_maint_write_remote(handle, dest_id, hop_count, addr, &data, 1);
	if (res != 0) {
		fprintf(stderr, "could not write destid 0x%08x hopcount 0x%08x addr 0x%08x data 0x%08x\n",
								dest_id, hop_count, addr, data);
		goto exit;
	}
	printf("0x%08x: 0x%08x\n", addr, data);

	while ((token = strtok(NULL, ":")) && (token !=NULL)) {
		data = strtoul(token, NULL, 0);
		addr += 4;
		res = rio_maint_write_remote(handle, dest_id, hop_count, addr, &data, 1);
		if (res != 0) {
			fprintf(stderr, "could not write 0x%08x to 0x%08x\n", data, addr);
			goto exit;
		}
		printf("0x%08x: 0x%08x\n", addr, data);


	}
exit:
	return res;
}

int main(int argc, char *argv[])
{
	uint32_t offset = 0, dest_id = 0, hop_count = 0, word_count = 0;
	int ret = 0;

	ret = rio_maint_init(DEVNR, &handle);
	if (ret != 0) {
		fprintf(stderr, "rio_maint_init: %s\n", strerror(-ret));
		goto out;
	}

	if (argc < 4) {
		usage(argv[0]);
		ret = 1;
		goto out;
	}

	switch(argv[1][0]) {
	case 'l':
	case 'L':
		if (argc < 5) {
			usage(argv[0]);
			ret = 1;
			goto out;
		}
		offset = strtoul(argv[3], NULL, 0);
		switch(argv[2][0]) {
		case 'r':
		case 'R':
			word_count = strtoul(argv[4], NULL, 0);
			if (read_local(offset, word_count)) {
				ret = 1;
				goto out;
			}
			break;
		case 'w':
		case 'W':
			if (write_local(offset, argv[4])) {
				ret = 1;
				goto out;
			}
			break;
		default:
			usage(argv[0]);
			goto out;
		}
		break;
	case 'r':
	case 'R':
		if (argc < 7) {
			usage(argv[0]);
			ret = 1;
			goto out;
		}
		dest_id = strtoul(argv[3], NULL, 0);
		hop_count = strtoul(argv[4], NULL, 0);
		offset = strtoul(argv[5], NULL, 0);

		switch(argv[2][0]) {
		case 'r':
		case 'R':
			word_count = strtoul(argv[6], NULL, 0);
			if (read_remote(dest_id, hop_count, offset, word_count)) {
				ret = 1;
				goto out;
			}
			break;
		case 'w':
		case 'W':
			if (write_remote(dest_id, hop_count, offset, argv[6])) {
				ret = 1;
				goto out;
			}
			break;
		default:
			usage(argv[0]);
			goto out;
		}
		break;
	default:
		usage(argv[0]);
		goto out;
	}
out:
	rio_maint_shutdown(&handle);

	return ret;
}
