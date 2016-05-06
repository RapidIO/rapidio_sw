/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include <stdexcept>

#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "memops.h"
#include "mportcmsock.h"
#include "mportmgmt.h"
#include "rapidio_mport_dma.h"
#include "libfxfr_private.h"
#include "fxfr_msg.h"

#define MAX_TX_SEGMENTS 2

#ifdef __cplusplus
extern "C" {
#endif

struct fxfr_tx_state {
	uint8_t fail_abort; 	/* 0 - Proceed with transfer.
				 * 1 - Abort transfer
				 */
	uint8_t done; 		/* 0 - Transfer continuing
				 * 1 - Transfer successful completion
				 */
	uint8_t debug;

	/* MPORT selection and data */
	uint8_t         mport_num;
	RIOMemOpsIntf*  mops;
	MEMOPSRequest_t mops_req;
        int             mp_h_valid;

	/* Mailbox data */
	MportCMSocket* req_skt;
	int      svr_skt;
	void*    msg_rx;
	void*    msg_tx;
	struct   fxfr_svr_to_client_msg *rxed_msg;
	struct   fxfr_client_to_svr_msg *tx_msg;
	int      msg_buff_size;

	/* File name data */
	char     src_name[MAX_FILE_NAME+1];
	int      src_fd;
	char     dest_name[MAX_FILE_NAME+1];
	uint8_t  end_of_file;

	/* RapidIO target/rx data */
	uint16_t destID; /* DestID of fxfr server */
	uint8_t  use_kbuf; /* 1 => Use kernel buffers, 0 => use malloc/free */
	uint8_t* buffers[MAX_TX_SEGMENTS]; /* Data to DMA to fxfr server */
	int      next_buff_idx;
	uint64_t bytes_txed; /* Bytes transmitted by this message */
	uint64_t tot_bytes_txed; /* Total bytes transmitted so far */
	uint64_t bytes_rxed; /* Bytes received/acknowledged by this message */
	uint64_t tot_bytes_rxed; /* Total bytes rxed by fxfr server */
	uint64_t rx_rapidio_addr; /* Base address of fxfr server window */
	uint64_t rx_rapidio_size; /* Size of fxfr server window */
};

static volatile sig_atomic_t srv_exit;

void process_msg_from_server(struct fxfr_tx_state *info)
{
	if (info->fail_abort)
		return;

	if (info->rxed_msg->tot_bytes_rx || info->rxed_msg->rx_file_name[0]) {
		/* Process transfer acknowledgement message from server.  
		 */
		if ((info->rxed_msg->tot_bytes_rx > info->tot_bytes_txed) ||
	     		!(info->rxed_msg->tot_bytes_rx) ||
			strncmp(info->rxed_msg->rx_file_name, 
				info->dest_name, MAX_FILE_NAME)) {
			if(info->debug) printf("Server msg: incorrect param(s)\n");

			goto fail;
		}

		info->tot_bytes_rxed = info->rxed_msg->tot_bytes_rx;

		if ((info->tot_bytes_txed == info->rxed_msg->tot_bytes_rx) && 
		     info->end_of_file)
			info->done = 1;
	} else {
		/* Connection acknowledgement message from server.  
		 * Set up RapidIO DMA addresses and DMA transfer sizes
		 */
		if (!info->rxed_msg->rapidio_addr || 
		    !info->rxed_msg->rapidio_size ||
		     info->rxed_msg->fail_abort) {
			if(info->debug)
				printf("Server msg has incorrect field(s) [rio_addr=%llx size=%x] fail_abort=%d\n",
					info->rxed_msg->rapidio_addr, info->rxed_msg->rapidio_size,
					info->rxed_msg->fail_abort);
			goto fail;
		}

		info->rx_rapidio_addr = info->rxed_msg->rapidio_addr;
		if (info->rxed_msg->rapidio_size > 
					(MAX_TX_SEGMENTS*MAX_TX_BUFF_SIZE))
			info->rx_rapidio_size = MAX_TX_BUFF_SIZE;
		else
			info->rx_rapidio_size = 
				(info->rxed_msg->rapidio_size/MAX_TX_SEGMENTS)
				& ((2*MAX_TX_BUFF_SIZE) - 0x1000);
	};

	return;
fail:
	info->fail_abort = 1;
};

void rx_msg_from_server(struct fxfr_tx_state *info)
{
	int ret = info->req_skt->read(info->msg_rx, info->msg_buff_size, 0);
	if (ret) {
		printf("File TX: riomp_sock_receive() ERR %d (%s)\n", ret, strerror(ret));
		info->fail_abort = 1;
	       	info->rxed_msg->fail_abort = 1;
		goto fail;
	};

	info->rxed_msg->rapidio_addr = be64toh(info->rxed_msg->rapidio_addr);
	info->rxed_msg->rapidio_size = be64toh(info->rxed_msg->rapidio_size);
	info->rxed_msg->tot_bytes_rx = be64toh(info->rxed_msg->tot_bytes_rx);
	info->rxed_msg->fail_abort = be64toh(info->rxed_msg->fail_abort);

	if (info->debug) {
		printf("Client: RX from Server\n");
		printf("rapidio_addr = %16lx\n", 
			(long unsigned int)info->rxed_msg->rapidio_addr);
		printf("rapidio_size = %16lx\n", 
			(long unsigned int)info->rxed_msg->rapidio_size);
		printf("tot_bytes_rx = %16lx\n", 
			(long unsigned int)info->rxed_msg->tot_bytes_rx);
		printf("fail_abort   = %16lx\n", 
			(long unsigned int)info->rxed_msg->fail_abort);
		printf("file name    = %s\n", info->rxed_msg->rx_file_name);
	};

	process_msg_from_server(info);
fail:
	return;
};

void fill_dma_buffer(struct fxfr_tx_state *info, int idx)
{
	if (info->src_fd < 0)
		throw std::logic_error("fill_dma_buffer: Invalid file descriptor!");

	if (info->buffers[idx] == NULL)
		throw std::logic_error("fill_dma_buffer: NULL file buffer!");

	const int nread = read(info->src_fd, info->buffers[idx], info->rx_rapidio_size);
	if (nread < 0) {
		printf("%s: read from fd %d (buf=%p) returned error %d (%s)\n", __func__, info->src_fd, info->buffers[idx], errno, strerror(errno));
		fflush(NULL);
		throw std::logic_error("fill_dma_buffer: Invalid readfile!");
	}

	info->bytes_txed = nread;
	if (info->bytes_txed < info->rx_rapidio_size)
		info->end_of_file = 1;
};

void send_dma_buffer(struct fxfr_tx_state *info, int idx)
{
	bool rc = false;
       
	if (info->bytes_txed) {
		MEMOPSRequest_t& req = info->mops_req;
		req.destid      = info->destID;
		req.bcount      = info->bytes_txed,
		req.raddr.lsb64 = info->rx_rapidio_addr + (idx * info->rx_rapidio_size);
		req.mem.offset  = idx * MAX_TX_BUFF_SIZE;
		req.sync        = RIO_DIRECTIO_TRANSFER_SYNC;
		req.wr_mode     = RIO_DIRECTIO_TYPE_NWRITE_R;

		rc = info->mops->nwrite_mem(req);
#if 0
		if (info->use_kbuf) {
			rc = riomp_dma_write_d(info->mp_h, 
				info->destID, 
				info->rx_rapidio_addr +
					(idx * info->rx_rapidio_size),
				info->buf_h,
				idx * MAX_TX_BUFF_SIZE,
				info->bytes_txed,
				RIO_DIRECTIO_TYPE_NWRITE,
				RIO_DIRECTIO_TRANSFER_SYNC);
		} else {
			rc = riomp_dma_write(info->mp_h, 
				info->destID, 
				info->rx_rapidio_addr +
					(idx * info->rx_rapidio_size),
				info->buffers[idx], 
				info->bytes_txed,
				RIO_DIRECTIO_TYPE_NWRITE,
				RIO_DIRECTIO_TRANSFER_SYNC);
		};
#endif
	};
	if (!rc) {
		info->fail_abort = 1;
		if(info->debug)
			 printf("File TX: DMA op failed with %d (%s)\n",
				info->mops->getAbortReason(),
				info->mops->abortReasonToStr(info->mops->getAbortReason()));

		info->bytes_txed = 0;
	} else {
		info->tot_bytes_txed += info->bytes_txed;
	};
};

void send_transfer_msg(struct fxfr_tx_state *info, int idx)
{
	int ret;

	info->tx_msg->rapidio_addr = htobe64(info->rx_rapidio_addr + 
			(idx * info->rx_rapidio_size));
	info->tx_msg->bytes_tx_now = htobe64(info->bytes_txed);
	info->tx_msg->tot_bytes_tx = htobe64(info->tot_bytes_txed);
	info->tx_msg->end_of_file = htobe64(info->end_of_file);
	info->tx_msg->fail_abort = htobe64(info->fail_abort);
	strncpy(info->tx_msg->rx_file_name, info->dest_name, MAX_FILE_NAME);

	if (info->debug) {
		printf("	Client: TX to Server\n");
		printf("	rapidio_addr = %16lx %16lx\n", 
			(long unsigned int)(info->rx_rapidio_addr 
					+ (idx * info->rx_rapidio_size)),
			(long unsigned int)info->tx_msg->rapidio_addr);
		printf("	bytes_tx_now = %16lx %16lx\n", 
			(long unsigned int)info->bytes_txed,
			(long unsigned int)info->tx_msg->bytes_tx_now);
		printf("	tot_bytes_tx = %16lx %16lx\n", 
			(long unsigned int)info->tot_bytes_txed,
			(long unsigned int)info->tx_msg->tot_bytes_tx);
		printf("	end_of_file  = %16lx %16lx\n", 
			(long unsigned int)info->end_of_file,
			(long unsigned int)info->tx_msg->end_of_file);
		printf("	fail_abort   = %16lx %16lx\n", 
			(long unsigned int)info->fail_abort,
			(long unsigned int)info->tx_msg->fail_abort);
		printf("	file name    = %s\n",
			info->tx_msg->rx_file_name);
	};

	/* Send  a message back to the client */
	ret = info->req_skt->write(info->msg_tx, MAX_MSG_SIZE);
	if (ret) {
		printf("File TX(%d): riomp_sock_send() ERR %d (%d)\n",
			(int)getpid(), ret, errno);
		info->fail_abort = 1;
	}
};

void send_msgs_to_server(struct fxfr_tx_state *info, struct timespec *st_time)
{
	if (info->fail_abort) {
	       	if (!info->rxed_msg->fail_abort) {
			info->bytes_txed = 0;
			send_transfer_msg(info, 0);
		};
	} else {
		uint64_t diff = info->tot_bytes_txed - info->tot_bytes_rxed;
		uint64_t max_diff = MAX_TX_SEGMENTS*info->rx_rapidio_size;
		
		while (!info->fail_abort && !info->done && !info->end_of_file
				&& (diff < max_diff)) {
			fill_dma_buffer(info, info->next_buff_idx);
			if (NULL != st_time)
				clock_gettime(CLOCK_MONOTONIC, st_time);
			st_time = NULL;

			send_dma_buffer(info, info->next_buff_idx);
			send_transfer_msg(info, info->next_buff_idx);
			diff = info->tot_bytes_txed - info->tot_bytes_rxed;
			info->next_buff_idx = 
				(info->next_buff_idx + 1) % MAX_TX_SEGMENTS;
		};
	};
};

int init_info_vals(struct fxfr_tx_state *info)
{	
	memset(info, 0, sizeof(*info));

	info->mport_num = -1;

	info->src_fd = -1;

	info->destID = -1;

	info->use_kbuf = 1;

	return 0;
};

int init_file_info(struct fxfr_tx_state *info, char *src_name, char *dst_name)
{
	strncpy(info->dest_name, dst_name, MAX_FILE_NAME);
	strncpy(info->src_name, src_name, MAX_FILE_NAME);

	info->src_fd = open(info->src_name, O_RDONLY);
	if (info->src_fd == -1) {
		perror ("open");
		printf("\nFile \"%s\" open read-only failed.\n", 
			info->src_name);
		return 1;
	};

	return 0;
};

void cleanup_file_info(struct fxfr_tx_state *info)
{
	if (info->src_fd > 0)
		close(info->src_fd);
};

int init_message_buffers(struct fxfr_tx_state *info, int buf_size)
{
	info->msg_buff_size = buf_size;
        info->msg_rx = malloc(info->msg_buff_size); 
        if (info->msg_rx == NULL) {
                printf("File TX: malloc rx msg failed\n");
                return 1;
        };

        info->msg_tx = malloc(info->msg_buff_size); 
        if (info->msg_tx == NULL) {
                printf("File TX: malloc tx msg failed\n");
                return 1;
        };

	info->rxed_msg = (struct fxfr_svr_to_client_msg *)
		(&(((char *)(info->msg_rx))[0]));
	info->tx_msg = (struct fxfr_client_to_svr_msg *)
		(&(((char *)(info->msg_tx))[0]));

	return 0;
};

void cleanup_msg_buffers(struct fxfr_tx_state *info)
{
	free(info->msg_rx); info->msg_rx = NULL;
	free(info->msg_tx); info->msg_tx = NULL;
};

int init_server_connect(struct fxfr_tx_state *info, 
			uint8_t mport_num, uint16_t destID, int svr_skt,
			uint8_t k_buff)
{
        int rc = -1;
	bool memrc = false;
	int i;
        struct riomp_mgmt_mport_properties qresp;

        info->mport_num = mport_num;
	info->svr_skt = svr_skt;
	info->use_kbuf = k_buff;
	info->mp_h_valid = 0;

	info->mops = RIOMemOpsChanMgr(info->mport_num, true /*shared*/, ANY_CHANNEL);
	info->mp_h_valid = 1;

	{{
	  MportMgmt* mp_h = new MportMgmt(info->mport_num);
          if (mp_h->query(qresp)) {
                printf("\nUnable to query mport %d...\n", info->mport_num);
		delete mp_h;
                goto fail;
          };
	  delete mp_h;
	}}

	if (info->debug) {
		std::string s =  MportMgmt::toString(qresp);
		printf("%s", s.c_str());
	}
	
        if (!(qresp.flags & RIO_MPORT_DMA)) {
                printf("\nMport %d has no DMA support...\n", info->mport_num);
                goto fail;
        };

        info->req_skt = new MportCMSocket(info->mport_num, 0);

	info->destID = destID;
        rc = info->req_skt->connect(info->destID, 0, info->svr_skt);
        if (rc) {
                printf("riomp_sock_connect ERR %d (%s)\n", rc, strerror(rc));
                goto fail;
        }

	memrc = info->use_kbuf?
			info->mops->alloc_dmawin(info->mops_req.mem, TOTAL_TX_BUFF_SIZE):
			info->mops->alloc_umem(info->mops_req.mem, TOTAL_TX_BUFF_SIZE);
	if (!memrc) {
		info->buffers[0] = NULL;
		if (rc && info->debug)
			printf("riomp_dbuf_free failed err=%d\n", rc);
		goto fail;
	} else {
		info->buffers[0] = (uint8_t*)info->mops_req.mem.win_ptr;

		for (i = 1; i < MAX_TX_SEGMENTS; i++) {
			info->buffers[i] = info->buffers[0] +
				(i * MAX_TX_BUFF_SIZE);
		};
	};

	return 0;
fail:
	return 1;
};

void cleanup_server_connect(struct fxfr_tx_state *info)
{
	int i, rc;

	if (info->buffers[0])
		info->mops->free_xwin(info->mops_req.mem);

	if (info->req_skt) {
		delete info->req_skt;
		info->req_skt = NULL;
	};

	if (info->mp_h_valid) {
		delete info->mops; info->mops = NULL;
		info->mp_h_valid = 0;
	};
};

void cleanup_all(struct fxfr_tx_state *info)
{
	cleanup_file_info(info);
	cleanup_msg_buffers(info);
	cleanup_server_connect(info);
}

int init_info( struct fxfr_tx_state *info, char *src_name, char *dest_name,
                	uint16_t destID, int svr_skt, uint8_t mport_num,
			uint8_t debug, uint8_t k_buff)
{
	init_info_vals(info);
	
	info->debug = debug;

	if (init_file_info(info, src_name, dest_name))
		goto fail;

	if (init_message_buffers(info, MAX_MSG_SIZE))
		goto fail;

	if (init_server_connect(info, mport_num, destID, svr_skt, k_buff))
		goto fail;

	return 0;
fail:
	cleanup_all(info);
	return 1;
};

static void srv_sig_handler(int signum)
{
        switch(signum) {
        case SIGTERM:
                srv_exit = 1;
                break;
        case SIGINT:
                srv_exit = 1;
                break;
        case SIGUSR1:
                srv_exit = 1;
                break;
	}
};

int   send_file(char *src_file, /* Local source file name */
		char *dst_file, /* Requested destination file name */
		int destID, /* DestID of fxfr server */
		int skt_num, /* Channel # of fxfr server */
		int mport_num, /* MPORT index to use */
		uint8_t debug, /* MPORT index to use */
		struct timespec *st_time,
		uint64_t *bytes_sent,
		uint8_t k_buff)
{
	struct fxfr_tx_state info;

        /* Trap signals that we expect to receive */
        signal(SIGINT, srv_sig_handler);
        signal(SIGTERM, srv_sig_handler);
        signal(SIGUSR1, srv_sig_handler);

	/* Confirm local file, connectivity to remote mport/socket etc. */
	if (init_info(&info, src_file, dst_file, destID, skt_num, 
			mport_num, debug, k_buff))
		return -errno;

	while (!srv_exit && !info.fail_abort && !info.done) {
		rx_msg_from_server(&info);
		if (info.end_of_file)
			break;
		send_msgs_to_server(&info, st_time);
		st_time = NULL;
	};

	*bytes_sent = info.tot_bytes_txed;
	cleanup_all(&info);

	return info.fail_abort;
};

#ifdef __cplusplus
}
#endif
