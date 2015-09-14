/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * RapidIO mport device API library
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

#if defined(LIBMPORT_TRACE) || defined(LIBMPORT_SIMULATOR)
#include <stdarg.h>
#endif

#ifndef LIBMPORT_SIMULATOR
#include <dirent.h>
#include <linux/rio_cm_cdev.h>
#define CONFIG_RAPIDIO_DMA_ENGINE
#include <linux/rio_mport_cdev.h>
#define RIO_MPORT_DEV_PATH "/dev/rio_mport"
#define RIO_CMDEV_PATH "/dev/rio_cm"
#define RIOCP_PE_DEV_DIR  "/dev"
#define RIOCP_PE_DEV_NAME "rio_mport"
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#endif

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_dma.h>
#include <rapidio_mport_sock.h>

#ifdef LIBMPORT_TRACE
struct rapidio_libmport_trace {
	FILE *output;
};
static struct rapidio_libmport_trace trace;

unsigned long long get_ns_timestamp(void)
{
	unsigned long long ts;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    ts = (unsigned long long)now.tv_sec * 1000000000ull + (unsigned long long)now.tv_nsec;
    return ts;
}

#define LIBTRACE(fmt, ...) {if(trace.output) {fprintf(trace.output, "libmport TRC(%lluns):", get_ns_timestamp()); fprintf(trace.output, fmt, ##__VA_ARGS__);}}
#define LIBPRINT(fmt, ...) {if(trace.output) {fprintf(trace.output, "libmport %s(): ", __func__); fprintf(trace.output, fmt, ##__VA_ARGS__);}}
#define LIBERROR(fmt, ...) {if(trace.output) {fprintf(trace.output, "libmport ERROR %s(): ", __func__); fprintf(trace.output, fmt, ##__VA_ARGS__);}}
#else
#define LIBTRACE(fmt, ...)
#define LIBPRINT(fmt, ...)
#define LIBERROR(fmt, ...)
#endif

#ifdef LIBMPORT_SIMULATOR
struct rapidio_libmport_simulator_sync {
	int lib_version;
	int simulator_version;
	enum riomp_mgmt_sys_size sys_size;
};
struct rapidio_libmport_simulator {
	const char *tcp_port;
	const char *tcp_server;
	int timeout;
};
static struct rapidio_libmport_simulator simulator;
#endif

struct rapidio_mport_mailbox {
	int fd;
	uint8_t mport_id;
};

struct rio_channel {
	uint16_t id;
	uint32_t remote_destid;
	uint32_t remote_mbox;
	uint16_t remote_channel;
	uint8_t mport_id;
};

struct rapidio_mport_socket {
	struct rapidio_mport_mailbox *mbox;
	struct rio_channel ch;
	uint8_t	*rx_buffer;
	uint8_t	*tx_buffer;
};

/**
 * @brief mport opaque handle structure
 */
struct rapidio_mport_handle {
	int fd;				/**< posix api compatible fd to be used with poll/select */
#ifdef LIBMPORT_SIMULATOR
	struct rapidio_libmport_simulator_sync sim_data;
#endif
};

#ifdef LIBMPORT_SIMULATOR
#define RIO_MAX_ENC_PACKET (1024)
#define RIO_LIB_SYM_VERSION (1)

enum packet_type {
	simulator_sync_req,
	simulator_sync_resp,
	pkt_local_maint_read_req,
	pkt_local_maint_read_resp,
	pkt_local_maint_write_req,
	pkt_local_maint_write_resp,
	pkt_remote_maint_read_req,
	pkt_remote_maint_read_resp,
	pkt_remote_maint_write_req,
	pkt_remote_maint_write_resp,
	pkt_port_write_req,
	pkt_get_event_mask_req,
	pkt_get_event_mask_resp,
	pkt_set_event_mask_req,
	pkt_set_event_mask_resp,
	pkt_pwrange_enable_req,
	pkt_pwrange_enable_resp,
	pkt_pwrange_disable_req,
	pkt_pwrange_disable_resp
};

const char *fmt_str[] = {
		"simulator_sync_req,LVER=%d\n",
		"simulator_sync_resp,SVER=%d,SYS_SIZE=%d\n",
		"maint_local_read_req,OFF=0x%x,SIZE=0x%x\n",
		"maint_local_read_resp,DATA=0x%x\n",
		"maint_local_write_req,OFF=0x%x,SIZE=0x%x,DATA=0x%x\n",
		"maint_local_write_resp\n",
		"maint_remote_read_req,DESTID=0x%x,HOP=%u,OFF=0x%x,SIZE=0x%x\n",
		"maint_remote_read_resp,DATA=0x%x\n",
		"maint_remote_write_req,DESTID=0x%x,HOP=%u,OFF=0x%x,SIZE=0x%x,DATA=0x%x\n",
		"maint_remote_write_resp\n",
		"port_write_req,ARRAY32=%ms\n",
		"get_event_mask_req\n",
		"get_event_mask_resp,DATA=0x%x\n",
		"set_event_mask_req,DATA=0x%x\n",
		"set_event_mask_resp\n",
		"pwrange_enable_req,MASK=0x%x,LOW=0x%x,HIGH=0x%x\n",
		"pwrange_enable_resp\n",
		"pwrange_disable_req,MASK=0x%x,LOW=0x%x,HIGH=0x%x\n",
		"pwrange_disable_resp\n"
};

static int encode_packet(enum packet_type t, char *buffer, size_t len, ...)
{
	va_list ap;
	int ret;

	va_start(ap, len);
	ret = vsnprintf(buffer, len, fmt_str[t], ap);
	va_end(ap);

	return ret;
}
#endif

void __attribute__ ((constructor)) trace_begin (void)
{
#ifdef LIBMPORT_SIMULATOR
	simulator.tcp_server = getenv("LIBMPORT_TCP_SERVER");
	if (!simulator.tcp_server) {
		simulator.tcp_server = "localhost";
	}
	simulator.tcp_port = getenv("LIBMPORT_TCP_PORT");
	if (!simulator.tcp_port) {
		simulator.tcp_port = "4567";
	}
	simulator.timeout = 3; // 3 secs
#endif
#ifdef LIBMPORT_TRACE
	char *trc_name = getenv("LIBMPORT_TRACE_FILE");
	if (trc_name && strcmp(trc_name, "stderr") == 0)
		trace.output = stderr;
	else if (trc_name && strcmp(trc_name, "stdout") == 0)
		trace.output = stdout;
	else if (trc_name)
		trace.output = fopen(trc_name, "a");
	else
		trace.output = NULL;
	if (!trace.output)
		fprintf(stderr, "ERROR: failed to init libmport trace\n");
#endif
	LIBTRACE("init\n");
}

void __attribute__ ((destructor)) trace_end (void)
{
	LIBTRACE("exit\n");
#ifdef LIBMPORT_TRACE
	if (trace.output && (trace.output != stderr || trace.output != stdout))
		fclose(trace.output);
#endif
}

#ifdef LIBMPORT_SIMULATOR
static int send_packet(int sd, const char *pkt)
{
	int len, ret, sent = 0;

	if (!pkt)
		return -EINVAL;

	len = strlen(pkt);

	while(len > sent) {
		ret = write(sd, pkt+sent, len-sent);
		if (ret == -1)
			return -errno;
		sent += ret;
	}

	return sent;
}

static int recv_packet(int sd, char *pkt, size_t len)
{
	int ret, received = 0;
	struct pollfd pfd;

	if (!pkt || !len)
		return -EINVAL;

	while(received < (int)len) {
		pfd.fd = sd;
		pfd.revents = 0;
		pfd.events = POLLIN | POLLPRI | POLLERR | POLLHUP;
		ret = poll(&pfd, 1, 1000 * simulator.timeout);
		if (ret == 0)
			return -ETIMEDOUT;
		else if (ret == -1)
			return -errno;
		else if (ret == 1) {
			if (pfd.revents & (POLLERR | POLLHUP)) {
				LIBTRACE("EIO\n");
				return -EIO;
			} else {
				ret = recv(sd, pkt+received, len-received, 0);
				if (ret == -1)
					return -errno;
				else
					received += ret;
				if (strchr(pkt, '\n'))
					return received;
			}
		}
	}

	return received;
}

static int decode_packet(enum packet_type t, char *buffer, ...)
{
	va_list ap;
	int ret;

	va_start(ap, buffer);
	ret = vsscanf(buffer, fmt_str[t], ap);
	va_end(ap);

	return (ret >= 0)?(0):(ret);
}

static int simulator_sync(int sock, struct rapidio_libmport_simulator_sync *data)
{
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;

	if(!data)
		return -EINVAL;

	ret = encode_packet(simulator_sync_req, pkt, sizeof(pkt), data->lib_version);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(sock, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(sock, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(simulator_sync_resp, pkt, &data->simulator_version, &data->sys_size);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
}
#endif

int riomp_mgmt_mport_available(uint8_t mport)
{
#ifndef LIBMPORT_SIMULATOR
	int ret;
	DIR *dev_dir;
	struct dirent *dev_ent = NULL;
	unsigned int _mport;

	dev_dir = opendir(RIOCP_PE_DEV_DIR);
	if (dev_dir == NULL) {
		ret = -errno;
		LIBERROR("Could not open %s\n", RIOCP_PE_DEV_DIR);
		return ret;
	}

	while ((dev_ent = readdir(dev_dir)) != NULL) {
		if (dev_ent->d_name[0] == '.' || strstr(dev_ent->d_name, RIOCP_PE_DEV_NAME) == NULL)
			continue;
		ret = sscanf(dev_ent->d_name, RIOCP_PE_DEV_NAME "%u", &_mport);
		if (ret != 1)
			goto err;
		if (mport == _mport)
			goto found;
	}

err:
	closedir(dev_dir);
	return -ENODEV;

found:
	closedir(dev_dir);
	return 1;
#else
	if(mport == 0)
		return 1;
	else
		return -ENODEV;
#endif
}

int riomp_mgmt_mport_list(size_t *count, uint8_t *list)
{
	uint8_t i,j=0;
	int ret;

	if(!count)
		return -EINVAL;

	for(i=0;i<RIODP_MAX_MPORTS;i++)	{
		if(j < *count) {
			ret = riomp_mgmt_mport_available(i);
			if(ret > 0) {
				if(list)
					list[j] = i;
				j++;
			}
		}
	}

	*count = j;

	return 0;
}

int riomp_mgmt_mport_create_handle(uint32_t mport_id, int flags, riomp_mport_t *mport_handle)
{
#ifndef LIBMPORT_SIMULATOR
	char path[32];
	int fd, ret;
	struct rapidio_mport_handle *hnd = NULL;

	snprintf(path, sizeof(path), RIO_MPORT_DEV_PATH "%d", mport_id);
	fd = open(path, O_RDWR | flags);
	if (fd == -1) {
		ret = -errno;
		LIBERROR("%s --> %s\n", __func__, strerror(-ret));
		return ret;
	}

	hnd = (struct rapidio_mport_handle *)malloc(sizeof(struct rapidio_mport_handle));
	if(!(hnd)) {
		ret = -errno;
		LIBERROR("%s --> %s\n", __func__, strerror(-ret));
		close(fd);
		return ret;
	}

	hnd->fd = fd;
	*mport_handle = hnd;
#else
	int ret, sd;
	struct rapidio_mport_handle *hnd = NULL;
#if 0
	struct addrinfo *addrinf;
	struct addrinfo hints;

	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=AI_ADDRCONFIG;

	ret = getaddrinfo(simulator.tcp_server, simulator.tcp_port, &hints, &addrinf);
	if (ret != 0) {
		LIBERROR("getaddrinfo failed with %s\n", gai_strerror(ret));
		return ret;
	}

	sd = socket(addrinf->ai_family, addrinf->ai_socktype, addrinf->ai_protocol);
	if (sd == -1) {
		ret = -errno;
		freeaddrinfo(addrinf);
		LIBERROR("socket() failed with %s\n", strerror(-ret));
		return ret;
	}

	ret = connect(sd, addrinf->ai_addr, addrinf->ai_addrlen);
	if (ret == -1) {
		ret = -errno;
		close(sd);
		freeaddrinfo(addrinf);
		LIBERROR("socket() failed with %s\n", strerror(-ret));
		return ret;
	}

	freeaddrinfo(addrinf);
#else
	socklen_t ai_addrlen;		/* Length of socket address.  */
	struct sockaddr_in ai_addr;	/* Socket address for socket.  */
	ai_addrlen = sizeof(ai_addr);

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		ret = -errno;
		LIBERROR("socket() failed with %s\n", strerror(-ret));
		return ret;
	}
	ai_addr.sin_family = AF_INET;
	ai_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	ai_addr.sin_port=htons(4567);

	ret = connect(sd, (struct sockaddr *)&ai_addr, ai_addrlen);
	if (ret == -1) {
		ret = -errno;
		close(sd);
		LIBERROR("socket() failed with %s\n", strerror(-ret));
		return ret;
	}
#endif

#if 0
	{
		struct sockaddr_in sin;
		socklen_t addrlen = sizeof(sin);
		if(getsockname(sd, (struct sockaddr *)&sin, &addrlen) == 0 &&
		   sin.sin_family == AF_INET &&
		   addrlen == sizeof(sin)) {
		    int local_port = ntohs(sin.sin_port);
		    LIBTRACE("%s local_port=%d\n", __func__, local_port);
		} else {
			LIBERROR("getsockname() failed\n");
		}
	}
#endif

	hnd = (struct rapidio_mport_handle *)malloc(sizeof(struct rapidio_mport_handle));
	if(!(hnd)) {
		ret = -errno;
		close(sd);
		LIBERROR("malloc(%lu) failed with %s\n", sizeof(struct rapidio_mport_handle), strerror(-ret));
		return ret;
	}

	bzero(&hnd->sim_data, sizeof(hnd->sim_data));
	hnd->sim_data.lib_version = RIO_LIB_SYM_VERSION;
	ret = simulator_sync(sd, &hnd->sim_data);
	if(ret != 0) {
		close(sd);
		LIBERROR("failed to sync with simulator: %s\n", strerror(-ret));
		free(hnd);
		return ret;
	}

	hnd->fd = sd;
	*mport_handle = hnd;

	LIBTRACE("%s SIMULATOR lib_version:%d simulator_version:%d\n", __func__, hnd->sim_data.lib_version, hnd->sim_data.simulator_version);
#endif

	LIBTRACE("%s mport%d hnd_id=%d\n", __func__, mport_id, hnd->fd);
	return 0;
}

int riomp_mgmt_mport_destroy_handle(riomp_mport_t *mport_handle)
{
	struct rapidio_mport_handle *hnd = *mport_handle;

	if(hnd == NULL) {
		LIBERROR("%s --> %s\n", __func__, strerror(EINVAL));
		return -EINVAL;
	}

	LIBTRACE("%s hnd_id=%d\n", __func__, hnd->fd);

	close(hnd->fd);
	free(hnd);

	return 0;
}

int riomp_mgmt_get_handle_id(riomp_mport_t mport_handle, int *id)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || id == NULL)
		return -EINVAL;

	/*
	 * if the architecture does not support posix file handles return a different identifier.
	 */
	*id = hnd->fd;
	return 0;
}

int riomp_sock_mbox_init(void)
{
#ifndef LIBMPORT_SIMULATOR
	LIBTRACE("%s\n", __func__);
	return open(RIO_CMDEV_PATH, O_RDWR);
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_mgmt_get_mport_list(uint32_t **dev_ids, uint8_t *number_of_mports)
{
#ifndef LIBMPORT_SIMULATOR
	int fd;
	uint32_t entries = *number_of_mports;
	uint32_t *list;
	int ret = -1;


	/* Open RapidIO Channel Manager */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	list = (uint32_t *)calloc((entries + 1), sizeof(*list));
	if (list == NULL)
		goto outfd;

	/* Request MPORT list from the driver (first entry is list size) */
	list[0] = entries;
	if (ioctl(fd, RIO_CM_MPORT_GET_LIST, list)) {
		ret = errno;
		goto outfd;
	}

	/* Return list information */
	*dev_ids = &list[1]; /* pointer to the list */
	*number_of_mports = *list; /* return real number of mports */
	ret = 0;
	LIBTRACE("%s\n", __func__);
outfd:
	close(fd);
	return ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_mgmt_free_mport_list(uint32_t **dev_ids)
{
#ifndef LIBMPORT_SIMULATOR
	/* Get head of the list, because we did hide the list size and mport ID
	 * parameters
	 */
	uint32_t *list;

	if(dev_ids == NULL)
		return -1;
	list = (*dev_ids) - 1;
	free(list);
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

int riomp_mgmt_get_ep_list(uint8_t mport_id, uint32_t **destids, uint32_t *number_of_eps)
{
#ifndef LIBMPORT_SIMULATOR
	int fd;
	int ret = 0;
	uint32_t entries;
	uint32_t *list;

	/* Open mport */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	/* Get list size */
	entries = mport_id;
	if (ioctl(fd, RIO_CM_EP_GET_LIST_SIZE, &entries)) {
#ifdef DEBUG
		printf("%s ep_get_list_size ioctl failed: %s\n", __func__, strerror(errno));
#endif
		ret = errno;
		goto outfd;
	}
#ifdef DEBUG
	printf("RIODP: %s() has %d entries\n", __func__,  entries);
#endif
	/* Get list */
	list = (uint32_t *)calloc((entries + 2), sizeof(*list));
	if (list == NULL) {
		ret = -1;
		goto outfd;
	}

	/* Get list (first entry is list size) */
	list[0] = entries;
	list[1] = mport_id;
	if (ioctl(fd, RIO_CM_EP_GET_LIST, list)) {
		ret = errno;
		goto outfd;
	}

	/* Pass to callee, first entry of list is entries in list */
	*destids = &list[2];
	*number_of_eps = entries;

	LIBTRACE("%s\n", __func__);
outfd:
	close(fd);
	return ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_mgmt_free_ep_list(uint32_t **destids)
{
#ifndef LIBMPORT_SIMULATOR
	/* Get head of the list, because we did hide the list size and mport ID
	 * parameters
	 */
	uint32_t *list;

	if(destids == NULL)
		return -1;
	list = (*destids) - 2;
	free(list);
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

#ifndef LIBMPORT_SIMULATOR
static inline enum rio_exchange convert_directio_type(enum riomp_dma_directio_type type)
{
	switch(type) {
	case RIO_DIRECTIO_TYPE_NWRITE: return RIO_EXCHANGE_NWRITE;
	case RIO_DIRECTIO_TYPE_NWRITE_R: return RIO_EXCHANGE_NWRITE_R;
	case RIO_DIRECTIO_TYPE_NWRITE_R_ALL: return RIO_EXCHANGE_NWRITE_R_ALL;
	case RIO_DIRECTIO_TYPE_SWRITE: return RIO_EXCHANGE_SWRITE;
	case RIO_DIRECTIO_TYPE_SWRITE_R: return RIO_EXCHANGE_SWRITE_R;
	default: return RIO_EXCHANGE_DEFAULT;
	}
}

static inline enum rio_transfer_sync convert_directio_sync(enum riomp_dma_directio_transfer_sync sync)
{
	switch(sync) {
	default: /* sync as default is the smallest pitfall */
	case RIO_DIRECTIO_TRANSFER_SYNC: return RIO_TRANSFER_SYNC;
	case RIO_DIRECTIO_TRANSFER_ASYNC: return RIO_TRANSFER_ASYNC;
	case RIO_DIRECTIO_TRANSFER_FAF: return RIO_TRANSFER_FAF;
	}
}
#endif

/*
 * Perform DMA data write to target transfer using user space source buffer
 */
int riomp_dma_write(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr, void *buf,
		uint32_t size, enum riomp_dma_directio_type wr_mode,
		enum riomp_dma_directio_transfer_sync sync)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = buf;
	xfer.length = size;
	xfer.handle = 0;
	xfer.offset = 0;
	xfer.method = convert_directio_type(wr_mode);

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_WRITE;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	LIBTRACE("%s\n", __func__);
	return (ret < 0)?errno:ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Perform DMA data write to target transfer using kernel space source buffer
 */
int riomp_dma_write_d(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr,
		      uint64_t handle, uint32_t offset, uint32_t size,
		      enum riomp_dma_directio_type wr_mode,
		      enum riomp_dma_directio_transfer_sync sync)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = NULL;
	xfer.length = size;
	xfer.handle = handle;
	xfer.offset = offset;
	xfer.method = convert_directio_type(wr_mode);

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_WRITE;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	LIBTRACE("%s\n", __func__);
	return (ret < 0)?errno:ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Perform DMA data read from target transfer using user space destination buffer
 */
int riomp_dma_read(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr, void *buf,
		   uint32_t size, enum riomp_dma_directio_transfer_sync sync)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = buf;
	xfer.length = size;
	xfer.handle = 0;
	xfer.offset = 0;

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_READ;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	LIBTRACE("%s\n", __func__);
	return (ret < 0)?errno:ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Perform DMA data read from target transfer using kernel space destination buffer
 */
int riomp_dma_read_d(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr,
		     uint64_t handle, uint32_t offset, uint32_t size,
		     enum riomp_dma_directio_transfer_sync sync)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = NULL;
	xfer.length = size;
	xfer.handle = handle;
	xfer.offset = offset;

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_READ;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	LIBTRACE("%s\n", __func__);
	return (ret < 0)?errno:ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Wait for DMA transfer completion
 */
int riomp_dma_wait_async(riomp_mport_t mport_handle, uint32_t cookie, uint32_t tmo)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_async_tx_wait wparam;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	wparam.token = cookie;
	wparam.timeout = tmo;

	if (ioctl(hnd->fd, RIO_WAIT_FOR_ASYNC, &wparam))
		return errno;
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

/*
 * Allocate and map into RapidIO space a local kernel space data buffer
 * (for inbound RapidIO data read/write requests)
 */
int riomp_dma_ibwin_map(riomp_mport_t mport_handle, uint64_t *rio_base, uint32_t size, uint64_t *handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_mmap ib;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ib.rio_addr = *rio_base;
	ib.length = size;

	if (ioctl(hnd->fd, RIO_MAP_INBOUND, &ib))
		return errno;
	*handle = ib.handle;
	*rio_base = ib.rio_addr;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Free and unmap from RapidIO space a local kernel space data buffer
 */
int riomp_dma_ibwin_free(riomp_mport_t mport_handle, uint64_t *handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_UNMAP_INBOUND, handle))
		return errno;
	LIBTRACE("%s\n", __func__);
#else
#endif
	return 0;
}

int riomp_dma_obwin_map(riomp_mport_t mport_handle, uint16_t destid, uint64_t rio_base, uint32_t size,
		    uint64_t *handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_mmap ob;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ob.rioid = destid;
	ob.rio_addr = rio_base;
	ob.length = size;

	if (ioctl(hnd->fd, RIO_MAP_OUTBOUND, &ob))
		return errno;
	*handle = ob.handle;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_dma_obwin_free(riomp_mport_t mport_handle, uint64_t *handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_UNMAP_OUTBOUND, handle))
		return errno;
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

/*
 * Allocate a local kernel space data buffer for DMA data transfers
 */
int riomp_dma_dbuf_alloc(riomp_mport_t mport_handle, uint32_t size, uint64_t *handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_dma_mem db;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	db.length = size;

	if (ioctl(hnd->fd, RIO_ALLOC_DMA, &db))
		return errno;
	*handle = db.dma_handle;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Free previously allocated local kernel space data buffer
 */
int riomp_dma_dbuf_free(riomp_mport_t mport_handle, uint64_t *handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_FREE_DMA, handle))
		return errno;
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

/*
 * map phys address range to process virtual memory
 */
int riomp_dma_map_memory(riomp_mport_t mport_handle, size_t size, off_t paddr, void **vaddr)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || !size || !vaddr)
		return -EINVAL;

	*vaddr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, hnd->fd, paddr);

	if (*vaddr == MAP_FAILED) {
		return -errno;
	}

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * unmap phys address range to process virtual memory
 */
int riomp_dma_unmap_memory(riomp_mport_t mport_handle, size_t size, void *vaddr)
{
#ifndef LIBMPORT_SIMULATOR
	LIBTRACE("%s\n", __func__);
	return munmap(vaddr, size);
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Query mport status/capabilities
 */
int riomp_mgmt_query(riomp_mport_t mport_handle, struct riomp_mgmt_mport_properties *qresp)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;
	struct rio_mport_properties prop;
	if (!qresp || !hnd)
		return -EINVAL;

	memset(&prop, 0, sizeof(prop));
	if (ioctl(hnd->fd, RIO_MPORT_GET_PROPERTIES, &prop))
		return errno;

	qresp->hdid               = prop.hdid;
	qresp->id                 = prop.id;
	qresp->index              = prop.index;
	qresp->flags              = prop.flags;
	qresp->port_ok            = prop.port_ok;
	qresp->link_speed         = prop.link_speed;
	qresp->link_width         = prop.link_width;
	qresp->dma_max_sge        = prop.dma_max_sge;
	qresp->dma_max_size       = prop.dma_max_size;
	qresp->dma_align          = prop.dma_align;
	qresp->transfer_mode      = prop.transfer_mode;
	qresp->cap_sys_size       = prop.cap_sys_size;
	qresp->cap_addr_size      = prop.cap_addr_size;
	qresp->cap_transfer_mode  = prop.cap_transfer_mode;
	qresp->cap_mport          = prop.cap_mport;
	switch(prop.sys_size) {
	case 0:	qresp->sys_size = RIO_SYS_SIZE_8; break;
	case 1:	qresp->sys_size = RIO_SYS_SIZE_16; break;
	case 2:	qresp->sys_size = RIO_SYS_SIZE_32; break;
	default:
		LIBERROR("%s: sys_size=%d not supported\n", __func__, prop.sys_size);
		return -ENOTSUP;
		break;
	}
	LIBTRACE("%s\n", __func__);
#else
	struct rapidio_mport_handle *hnd = mport_handle;

	if (!qresp || !hnd)
		return -EINVAL;

	bzero(qresp, sizeof(*qresp));
	qresp->sys_size = hnd->sim_data.sys_size;
	qresp->link_speed = RIO_LINK_625;
	qresp->link_width = RIO_LINK_16X;
#endif
	return 0;
}

/*
 * Read from local (mport) device register
 */
int riomp_mgmt_lcfg_read(riomp_mport_t mport_handle, uint32_t offset, uint32_t size, uint32_t *data)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = data;

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_READ_LOCAL, &mt))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || size != 4)
		return -EINVAL;

	ret = encode_packet(pkt_local_maint_read_req, pkt, sizeof(pkt), offset, size);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_local_maint_read_resp, pkt, data);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Write to local (mport) device register
 */
int riomp_mgmt_lcfg_write(riomp_mport_t mport_handle, uint32_t offset, uint32_t size, uint32_t data)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.offset = offset;
	mt.length = size;
//		uint32_t __user value; /* when length == 0 */
	mt.u.buffer = &data;   /* when length != 0 */

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_WRITE_LOCAL, &mt))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || size != 4)
		return -EINVAL;

	ret = encode_packet(pkt_local_maint_write_req, pkt, sizeof(pkt), offset, size, data);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_local_maint_write_resp, pkt);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Maintenance read from target RapidIO device register
 */
int riomp_mgmt_rcfg_read(riomp_mport_t mport_handle, uint32_t destid, uint32_t hc, uint32_t offset,
		     uint32_t size, uint32_t *data)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.rioid = destid;
	mt.hopcount = hc;
	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = data;   /* when length != 0 */

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_READ_REMOTE, &mt))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || size != 4)
		return -EINVAL;

	ret = encode_packet(pkt_remote_maint_read_req, pkt, sizeof(pkt), destid, hc, offset, size);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_remote_maint_read_resp, pkt, data);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Maintenance write to target RapidIO device register
 */
int riomp_mgmt_rcfg_write(riomp_mport_t mport_handle, uint32_t destid, uint32_t hc, uint32_t offset,
		      uint32_t size, uint32_t data)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.rioid = destid;
	mt.hopcount = hc;
	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = &data;   /* when length != 0 */

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_WRITE_REMOTE, &mt))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || size != 4)
		return -EINVAL;

	ret = encode_packet(pkt_remote_maint_write_req, pkt, sizeof(pkt), destid, hc, offset, size, data);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_remote_maint_write_resp, pkt);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Enable (register) receiving range of RapidIO doorbell events
 */
int riomp_mgmt_dbrange_enable(riomp_mport_t mport_handle, uint32_t rioid, uint16_t start, uint16_t end)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_doorbell_filter dbf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dbf.rioid = rioid;
	dbf.low = start;
	dbf.high = end;

	if (ioctl(hnd->fd, RIO_ENABLE_DOORBELL_RANGE, &dbf))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Disable (unregister) range of inbound RapidIO doorbell events
 */
int riomp_mgmt_dbrange_disable(riomp_mport_t mport_handle, uint32_t rioid, uint16_t start, uint16_t end)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_doorbell_filter dbf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dbf.rioid = rioid;
	dbf.low = start;
	dbf.high = end;

	if (ioctl(hnd->fd, RIO_DISABLE_DOORBELL_RANGE, &dbf))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Enable (register) filter for RapidIO port-write events
 */
int riomp_mgmt_pwrange_enable(riomp_mport_t mport_handle, uint32_t mask, uint32_t low, uint32_t high)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_pw_filter pwf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	pwf.mask = mask;
	pwf.low = low;
	pwf.high = high;

	if (ioctl(hnd->fd, RIO_ENABLE_PORTWRITE_RANGE, &pwf))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ret = encode_packet(pkt_pwrange_enable_req, pkt, sizeof(pkt), mask, low, high);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_pwrange_enable_resp, pkt);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Disable (unregister) filter for RapidIO port-write events
 */
int riomp_mgmt_pwrange_disable(riomp_mport_t mport_handle, uint32_t mask, uint32_t low, uint32_t high)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_pw_filter pwf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	pwf.mask = mask;
	pwf.low = low;
	pwf.high = high;

	if (ioctl(hnd->fd, RIO_DISABLE_PORTWRITE_RANGE, &pwf))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ret = encode_packet(pkt_pwrange_disable_req, pkt, sizeof(pkt), mask, low, high);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_pwrange_disable_resp, pkt);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Set event notification mask
 */
int riomp_mgmt_set_event_mask(riomp_mport_t mport_handle, unsigned int mask)
{
#ifndef LIBMPORT_SIMULATOR
	unsigned int evt_mask = 0;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (mask & RIO_EVENT_DOORBELL) evt_mask |= RIO_DOORBELL;
	if (mask & RIO_EVENT_PORTWRITE) evt_mask |= RIO_PORTWRITE;
	if (ioctl(hnd->fd, RIO_SET_EVENT_MASK, evt_mask))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || !mask)
		return -EINVAL;

	ret = encode_packet(pkt_set_event_mask_req, pkt, sizeof(pkt), mask);
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_set_event_mask_resp, pkt);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Get current value of event mask
 */
int riomp_mgmt_get_event_mask(riomp_mport_t mport_handle, unsigned int *mask)
{
#ifndef LIBMPORT_SIMULATOR
	int evt_mask = 0;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (!mask) return -EINVAL;
	if (ioctl(hnd->fd, RIO_GET_EVENT_MASK, &evt_mask))
		return errno;
	*mask = 0;
	if (evt_mask & RIO_DOORBELL) *mask |= RIO_EVENT_DOORBELL;
	if (evt_mask & RIO_PORTWRITE) *mask |= RIO_EVENT_PORTWRITE;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	char pkt[RIO_MAX_ENC_PACKET] = {0};
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || !mask)
		return -EINVAL;

	ret = encode_packet(pkt_get_event_mask_req, pkt, sizeof(pkt));
	if (ret > 0) {
		LIBTRACE(pkt);
		ret = send_packet(hnd->fd, pkt);
		if (ret < 0)
			return ret;
		bzero(pkt, sizeof(pkt));
		ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
		if (ret < 0)
			return ret;
		ret = decode_packet(pkt_get_event_mask_resp, pkt, mask);
		if (ret < 0)
			return ret;
		LIBTRACE(pkt);
	}
	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * Get current event data
 */
int riomp_mgmt_get_event(riomp_mport_t mport_handle, struct riomp_mgmt_event *evt)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_event revent;
	ssize_t bytes = 0;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (!evt) return -EINVAL;

	bytes = read(hnd->fd, &revent, sizeof(revent));
	if (bytes == -1)
		return -errno;
	if (bytes != sizeof(revent)) {
		return -EIO;
	}

	if (revent.header == RIO_EVENT_DOORBELL) {
		evt->u.doorbell.payload = revent.u.doorbell.payload;
		evt->u.doorbell.rioid = revent.u.doorbell.rioid;
	} else if (revent.header == RIO_EVENT_PORTWRITE) {
		memcpy(&evt->u.portwrite.payload, &revent.u.portwrite.payload, sizeof(evt->u.portwrite.payload));
	} else {
		return -EIO;
	}
	evt->header = revent.header;

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	struct rapidio_mport_handle *hnd = mport_handle;
	int ret;
	unsigned int i;
	char pkt[RIO_MAX_ENC_PACKET] = {0}, *p, *p2;

	if(hnd == NULL)
		return -EINVAL;

	if (!evt) return -EINVAL;

	ret = recv_packet(hnd->fd, pkt, sizeof(pkt));
	if (ret < 0)
		return ret;
	ret = decode_packet(pkt_port_write_req, pkt, &p);
	if (ret < 0)
		return ret;
	LIBTRACE(pkt);
	//LIBPRINT("payload (%p): %s\n", p, p);
	evt->header = RIO_EVENT_PORTWRITE;
	p2 = p;
	for (i=0;i<(sizeof(evt->u.portwrite.payload)/sizeof(evt->u.portwrite.payload[0]));i++) {
		evt->u.portwrite.payload[i] = strtoul(p2, &p2, 0);
		if(!p2)
			break;
		p2++;
	}
	free(p);

	return (ret >= 0)?(0):(ret);
#endif
}

/*
 * send an event (only doorbell supported)
 */
int riomp_mgmt_send_event(riomp_mport_t mport_handle, struct riomp_mgmt_event *evt)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;
	struct rio_event sevent;
	char *p = (char*)&sevent;
	ssize_t ret;
	unsigned int len=0;

	if(hnd == NULL)
		return -EINVAL;

	if (!evt) return -EINVAL;

	if (evt->header != RIO_EVENT_DOORBELL) return -EOPNOTSUPP;

	sevent.header = RIO_DOORBELL;
	sevent.u.doorbell.rioid = evt->u.doorbell.rioid;
	sevent.u.doorbell.payload = evt->u.doorbell.payload;

	while (len < sizeof(sevent)) {
		ret = write(hnd->fd, p+len, sizeof(sevent)-len);
		if (ret == -1) {
			return -errno;
		} else {
			len += ret;
		}
	}

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

/*
 * Set destination ID of local mport device
 */
int riomp_mgmt_destid_set(riomp_mport_t mport_handle, uint16_t destid)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_HDID_SET, &destid))
		return errno;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	int ret;
	uint32_t val;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ret = riomp_mgmt_lcfg_read(mport_handle, 0x60, 4, &val);
	if(ret)
		return ret;

	if(hnd->sim_data.sys_size == 0) {
		val &= ~0x00ff0000;
		val |= ((destid & 0xff) << 16);
	} else if(hnd->sim_data.sys_size == 1) {
		val &= ~0x0000ffff;
		val |= (destid & 0xffff);
	} else {
		LIBERROR("not implemented sys_size==%d\n", hnd->sim_data.sys_size);
		return -ENOTSUP;
	}

	ret = riomp_mgmt_lcfg_write(mport_handle, 0x60, 4, val);
	if(ret)
		return ret;

	LIBTRACE("%s\n", __func__);
	return ret;
#endif
}

/*
 * Create a new kernel device object
 */
int riomp_mgmt_device_add(riomp_mport_t mport_handle, uint16_t destid, uint8_t hc, uint32_t ctag,
		    const char *name)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_rdev_info dev;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dev.destid = destid;
	dev.hopcount = hc;
	dev.comptag = ctag;
	if (name)
		strncpy(dev.name, name, RIO_MAX_DEVNAME_SZ);
	else
		*dev.name = '\0';

	if (ioctl(hnd->fd, RIO_DEV_ADD, &dev))
		return errno;
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

/*
 * Delete existing kernel device object
 */
int riomp_mgmt_device_del(riomp_mport_t mport_handle, uint16_t destid, uint8_t hc, uint32_t ctag)
{
#ifndef LIBMPORT_SIMULATOR
	struct rio_rdev_info dev;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dev.destid = destid;
	dev.hopcount = hc;
	dev.comptag = ctag;

	if (ioctl(hnd->fd, RIO_DEV_DEL, &dev))
		return errno;
	LIBTRACE("%s\n", __func__);
#else
	LIBERROR("not implemented\n");
#endif
	return 0;
}

/* Mailbox functions */
int riomp_sock_mbox_create_handle(uint8_t mport_id, uint8_t mbox_id,
			     riomp_mailbox_t *mailbox)
{
#ifndef LIBMPORT_SIMULATOR
	int fd;
	struct rapidio_mport_mailbox *lhandle = NULL;

	/* Open mport */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	/* TODO claim mbox_id */

	/* Create handle */
	lhandle = (struct rapidio_mport_mailbox *)malloc(sizeof(struct rapidio_mport_mailbox));
	if(!(lhandle)) {
		close(fd);
		return -2;
	}

	lhandle->fd = fd;
	lhandle->mport_id = mport_id;
	*mailbox = lhandle;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_socket(riomp_mailbox_t mailbox, riomp_sock_t *socket_handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_socket *handle = NULL;

	/* Create handle */
	handle = (struct rapidio_mport_socket *)calloc(1, sizeof(struct rapidio_mport_socket));
	if(!handle) {
		printf("error in calloc\n");
		return -1;
	}

	handle->mbox = mailbox;
	handle->ch.id = 0;
	*socket_handle = handle;
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_send(riomp_sock_t socket_handle, void *buf, uint32_t size)
{
#ifndef LIBMPORT_SIMULATOR
	int ret;
	struct rapidio_mport_socket *handle = socket_handle;
	struct rio_cm_msg msg;

	msg.ch_num = handle->ch.id;
	msg.size = size;
	msg.msg = buf;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_SEND, &msg);
	if (ret) {
		printf("SEND IOCTL: returned %d for ch_num=%d (errno=%d)\n", ret, msg.ch_num, errno);
		return errno;
	}

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_receive(riomp_sock_t socket_handle, void **buf,
			 uint32_t size, uint32_t timeout)
{
#ifndef LIBMPORT_SIMULATOR
	int ret;
	struct rapidio_mport_socket *handle = socket_handle;
	struct rio_cm_msg msg;

	msg.ch_num = handle->ch.id;
	msg.size = size;
	msg.msg = *buf;
	msg.rxto = timeout;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_RECEIVE, &msg);
	if (ret)
		return errno;

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_release_receive_buffer(riomp_sock_t socket_handle,
					void *buf) /* always 4k aligned buffers */
{
#ifndef LIBMPORT_SIMULATOR
	free(buf);
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_close(riomp_sock_t *socket_handle)
{
#ifndef LIBMPORT_SIMULATOR
	int ret;
	struct rapidio_mport_socket *handle = *socket_handle;
	uint16_t ch_num;

	if(!handle)
		return -1;

	ch_num = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CLOSE, &ch_num);
	if (ret < 0) {
		printf("CLOSE IOCTL: returned %d for ch_num=%d (errno=%d)\n", ret, (*socket_handle)->ch.id, errno);
		ret = errno;
	}

	free(handle);
	LIBTRACE("%s\n", __func__);
	return ret;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_mbox_destroy_handle(riomp_mailbox_t *mailbox)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_mailbox *mbox = *mailbox;

	if(mbox != NULL) {
		close(mbox->fd);
		free(mbox);
		LIBTRACE("%s\n", __func__);
		return 0;
	}

	return -1;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_bind(riomp_sock_t socket_handle, uint16_t local_channel)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num;
	int ret;
	struct rio_cm_channel cdev;

	ch_num = local_channel;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CREATE, &ch_num);
	if (ret < 0)
		return errno;

	cdev.id = ch_num;
	cdev.mport_id = handle->mbox->mport_id;
	handle->ch.id = cdev.id;
	handle->ch.mport_id = cdev.mport_id;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_BIND, &cdev);
	if (ret < 0)
		return errno;

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_listen(riomp_sock_t socket_handle)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num;
	int ret;

	ch_num = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_LISTEN, &ch_num);
	if (ret)
		return errno;

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_accept(riomp_sock_t socket_handle, riomp_sock_t *conn,
			uint32_t timeout)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_socket *handle = socket_handle;
	struct rapidio_mport_socket *new_handle = *conn;
	struct rio_cm_accept param;
	int ret;

	if(!handle || !conn)
		return -1;

	param.ch_num = handle->ch.id;
	param.wait_to = timeout;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_ACCEPT, &param);
	if (ret)
		return errno;

#ifdef DEBUG
	printf("%s: new ch_num=%d\n", __func__, param.ch_num);
#endif

	if (new_handle)
		new_handle->ch.id = param.ch_num;

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_connect(riomp_sock_t socket_handle, uint32_t remote_destid,
			 uint8_t remote_mbox, uint16_t remote_channel)
{
#ifndef LIBMPORT_SIMULATOR
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num = 0;
	struct rio_cm_channel cdev;
	int ret;

	if (handle->ch.id == 0) {
		if (ioctl(handle->mbox->fd, RIO_CM_CHAN_CREATE, &ch_num)) {
			ret = errno;
			LIBERROR("ioctl RIO_CM_CHAN_CREATE rc(%d): %s\n", ret, strerror(ret));
			return ret;
		}
		handle->ch.id = ch_num;
	}

	/* Configure and Send Connect IOCTL */
	handle->ch.remote_destid  = remote_destid;
	handle->ch.remote_mbox    = remote_mbox;
	handle->ch.remote_channel = remote_channel;
	handle->ch.mport_id = handle->mbox->mport_id;
	cdev.remote_destid  = remote_destid;
	cdev.remote_mbox    = remote_mbox;
	cdev.remote_channel = remote_channel;
	cdev.mport_id = handle->mbox->mport_id;
	cdev.id = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CONNECT, &cdev);
	if (ret) {
		ret = errno;
		LIBERROR("ioctl RIO_CM_CHAN_CONNECT rc(%d): %s\n", ret, strerror(ret));
		return ret;
	}

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_request_send_buffer(riomp_sock_t socket_handle,
				     void **buf) //always 4k aligned buffers
{
#ifndef LIBMPORT_SIMULATOR
	/* socket_handle won't be used for now */

	*buf = malloc(0x1000); /* Always allocate maximum size buffers */
	if (*buf == NULL)
		return -1;

	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

int riomp_sock_release_send_buffer(riomp_sock_t socket_handle,
				     void *buf) /* always 4k aligned buffers */
{
#ifndef LIBMPORT_SIMULATOR
	free(buf);
	LIBTRACE("%s\n", __func__);
	return 0;
#else
	LIBERROR("not implemented\n");
	return -ENOTSUP;
#endif
}

const char *speed_to_string(int speed)
{
	switch(speed){
		case RIO_LINK_DOWN:
			return "LINK DOWN";
		case RIO_LINK_125:
			return "1.25Gb";
		case RIO_LINK_250:
			return "2.5Gb";
		case RIO_LINK_312:
			return "3.125Gb";
		case RIO_LINK_500:
			return "5.0Gb";
		case RIO_LINK_625:
			return "6.25Gb";
		default:
			return "ERROR";
	}
}

const char *width_to_string(int width)
{
	switch(width){
		case RIO_LINK_1X:
			return "1x";
		case RIO_LINK_1XR:
			return "1xR";
		case RIO_LINK_2X:
			return "2x";
		case RIO_LINK_4X:
			return "4x";
		case RIO_LINK_8X:
			return "8x";
		case RIO_LINK_16X:
			return "16x";
		default:
			return "ERROR";
	}
}

void riomp_mgmt_display_info(struct riomp_mgmt_mport_properties *attr)
{
	char const *sys_size_text[] = {
			"8bit",
			"16bit",
			"32bit"
	};
	printf("\n+++ SRIO mport configuration +++\n");
	printf("mport: hdid=%d, id=%d, idx=%d, flags=0x%x, sys_size=%s\n",
		attr->hdid, attr->id, attr->index, attr->flags,
		sys_size_text[attr->sys_size]);

	printf("link: speed=%s width=%s\n", speed_to_string(attr->link_speed),
		width_to_string(attr->link_width));

	if (attr->flags & RIO_MPORT_DMA) {
		printf("DMA: max_sge=%d max_size=%d alignment=%d (%s)\n",
			attr->dma_max_sge, attr->dma_max_size, attr->dma_align,
			(attr->flags & RIO_MPORT_DMA_SG)?"HW SG":"no HW SG");
	} else
		printf("No DMA support\n");
	printf("\n");
}


