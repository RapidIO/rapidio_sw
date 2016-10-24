/* Implementation of librskt library integrated into applications */
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdbool.h>

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>

#include "memops.h"
#include "memops_umd.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "librsktd.h"
#include "librskt_states.h"
#include "librskt_buff.h"
#include "librskt_utils.h"
#include "librskt_socket.h"
#include "librskt_list.h"
#include "librskt_info.h"
#include "liblist.h"
#include "libcli.h"
#include "rapidio_mport_dma.h"
#include "libtime_utils.h"
#include "liblog.h"
#include "librskt_private.h"
#include "librskt_threads.h"

// FIXME: Change to static inline
int send_bytes(volatile struct rskt_socket_t *skt, void *data, int byte_cnt, 
			struct rdma_xfer_ms_in *hdr_in, int inited) {
	struct rdma_xfer_ms_out hdr_out;
	uint32_t dma_rd_offset, dma_wr_offset;

	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X",
		ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	dma_rd_offset = ntohl(skt->hdr->loc_tx_wr_ptr) + RSKT_TOT_HDR_SIZE;
	dma_wr_offset = dma_rd_offset + skt->buf_sz;
	DBG("dma_rd_offset = 0x%X, dma_wr_offset = 0x%X, byte_cnt = %u",
				dma_rd_offset, dma_wr_offset, byte_cnt);
	memcpy((void *)(skt->tx_buf + ntohl(skt->hdr->loc_tx_wr_ptr)),
		data, byte_cnt);
	INC_PTR(skt->hdr->loc_tx_wr_ptr, byte_cnt, skt->buf_sz);
	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X",
		ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));

	if (lib.use_mport == SIX6SIX_FLAG) { /// TODO memops send_bytes
		const uint16_t destID = skt->sai.sa.ct;

                MEMOPSRequest_t req; memset(&req, 0, sizeof(req));

		req.mem = ((struct rskt_socket_t*)skt)->memops_ibwin;

                req.destid      = destID;
                req.bcount      = byte_cnt;
                req.raddr.lsb64 = skt->rio_addr + dma_wr_offset;
                req.mem.offset  = dma_rd_offset;
                req.sync        = RIO_DIRECTIO_TRANSFER_SYNC;
                req.wr_mode     = RIO_DIRECTIO_TYPE_NWRITE;

                int rc = skt->memops->nwrite_mem(req);
		if (!rc) {
                         ERR("File TX: DMA op failed with rc=%d reason=%d (%s)",
                              rc,
                              skt->memops->getAbortReason(),
                              skt->memops->abortReasonToStr(skt->memops->getAbortReason()));
		}

		if (skt->memops->canRestart() && skt->memops->checkAbort()) {
			int abort = skt->memops->getAbortReason();
			ERR("NWRITE ABORTed with reason %d (%s). Restarting channel.", abort, skt->memops->abortReasonToStr(abort));
			skt->memops->restartChannel();
		}

		if (!rc) {
			goto fail;
		};
	} else if (lib.use_mport) {
		int dma_err;
		DBG("riomp_dma_write_d ");
		do {
			dma_err = riomp_dma_write_d(lib.mp_h,
				skt->sai.sa.ct,			// destid
				skt->rio_addr + dma_wr_offset,  // tgt_addr
				skt->phy_addr,			// handle -- kernel allocated!
				dma_rd_offset,			// offset
				byte_cnt,			// bcount
				RIO_DIRECTIO_TYPE_NWRITE,
				RIO_DIRECTIO_TRANSFER_SYNC);
		} while (dma_err && ((EINTR == errno) || (EAGAIN == errno)));
		if (dma_err) {
			ERR("riomp_dma_write_d rc %d %d %s",
				dma_err, errno, strerror(errno));
			goto fail;
		};
	} else {
		if (!inited) {
			DBG("!inited, assigning hdr values from skt");
			hdr_in->loc_msubh = skt->msubh;
			hdr_in->rem_msubh = skt->con_msubh;
			hdr_in->priority = 0;
			hdr_in->sync_type = rdma_sync_chk;
			DBG("hdr_in->loc_msubh = %016" PRIx64 " ",
							hdr_in->loc_msubh);
			DBG("hdr_in->rem_msubh = %016" PRIx64 " ",
							hdr_in->rem_msubh);
		};

		hdr_in->loc_offset = dma_rd_offset;
		hdr_in->num_bytes = byte_cnt;
		hdr_in->rem_offset = dma_wr_offset;

		DBG("Calling push_msub");
		if (rdma_push_msub(hdr_in, &hdr_out)) {
			skt->hdr->loc_tx_wr_flags |=
						htonl(RSKT_BUF_HDR_FLAG_ERROR);
			skt->hdr->loc_rx_rd_flags |=
						htonl(RSKT_BUF_HDR_FLAG_ERROR);
			ERR("Failed in rdma_push_msub()..exiting");
			goto fail;
		};
	};
	skt->stats.tx_bytes += byte_cnt;
	skt->stats.tx_trans++;
	DBG("EXIT");
	return 0;
fail:
	DBG("EXIT, failed");
	return -1;
}; /* send_bytes */

int update_remote_hdr(struct rskt_socket_t * volatile skt,
			struct rdma_xfer_ms_in *hdr_in)
{
	struct rdma_xfer_ms_out hdr_out;
	int rc = -1;

	if (lib.use_mport == SIX6SIX_FLAG) { /// TODO memops update_remote_hdr
               const uint16_t destID = skt->sai.sa.ct;

                MEMOPSRequest_t req; memset(&req, 0, sizeof(req));

                req.mem = ((struct rskt_socket_t*)skt)->memops_ibwin;

                req.destid      = destID;
                req.bcount      = RSKT_LOC_HDR_SIZE;
                req.raddr.lsb64 = skt->rio_addr + RSKT_REM_RX_WR_PTR_OFFSET;
                req.mem.offset  = RSKT_LOC_TX_WR_PTR_OFFSET;
                req.sync        = RIO_DIRECTIO_TRANSFER_SYNC;
                req.wr_mode     = RIO_DIRECTIO_TYPE_NWRITE;

                rc = skt->memops->nwrite_mem(req);
                if (!rc) {
                         ERR("File TX: DMA op failed with rc=%d reason=%d (%s)",
                              rc,
                              skt->memops->getAbortReason(),
                              skt->memops->abortReasonToStr(skt->memops->getAbortReason()));
                }

                if (skt->memops->canRestart() && skt->memops->checkAbort()) {
                        int abort = skt->memops->getAbortReason();
                        ERR("NWRITE ABORTed with reason %d (%s). Restarting channel.", abort, skt->memops->abortReasonToStr(abort));
                        skt->memops->restartChannel();
                }

                if (!rc) return -1;
		rc = 0;
	} else if (lib.use_mport) {
		do {
			rc = riomp_dma_write_d(lib.mp_h,
				skt->sai.sa.ct,					// destid
				skt->rio_addr + RSKT_REM_RX_WR_PTR_OFFSET,	// tgt_addr
				skt->phy_addr,					// handle
				RSKT_LOC_TX_WR_PTR_OFFSET,			// offset
				RSKT_LOC_HDR_SIZE,				// bcount
				RIO_DIRECTIO_TYPE_NWRITE,
				RIO_DIRECTIO_TRANSFER_SYNC);
		} while (rc && ((EINTR == errno) || (EAGAIN == errno)));

		if (rc) {
			ERR("riomp_dma_write_d rc %d %d %s",
				rc, errno, strerror(errno));
		};
	} else {
		/* NOTE: Assumes that hdr_in->loc-msubh, rem_msubh, 
	 	* 	priority and sync_type have been filled in already!
	 	*/
		hdr_in->loc_offset = RSKT_LOC_TX_WR_PTR_OFFSET;
		hdr_in->rem_offset = RSKT_REM_RX_WR_PTR_OFFSET;
		hdr_in->num_bytes = RSKT_LOC_HDR_SIZE;
		DBG("loc_offset = 0x%X, rem_offset = 0x%X, num_bytes = %d",
			hdr_in->loc_offset, hdr_in->rem_offset, hdr_in->num_bytes);
		rc = rdma_push_msub(hdr_in, &hdr_out);
		if (rc) {
			ERR("Failed to push update to remote header");
			skt->hdr->loc_tx_wr_flags |=
						htonl(RSKT_BUF_HDR_FLAG_ERROR);
			skt->hdr->loc_rx_rd_flags |=
						htonl(RSKT_BUF_HDR_FLAG_ERROR);
		};
	};
	
	return rc;
}; /* update_remote_hdr() */


// FIXME static inline 
uint32_t get_free_bytes(volatile struct rskt_buf_hdr *hdr,
				uint32_t buf_sz)
{
	if (hdr == NULL) {
		return 0;
	}

	uint32_t ltw = ntohl(hdr->loc_tx_wr_ptr);
	uint32_t rtr = ntohl(hdr->rem_tx_rd_ptr);
	uint32_t free_bytes = rtr - ltw;

	if (ltw > rtr) {
		free_bytes = buf_sz - ltw + rtr;
	}

	uint32_t rrwf = ntohl(hdr->rem_rx_wr_flags);
	uint32_t rtrf = ntohl(hdr->rem_tx_rd_flags);
	errno = (RSKT_BUF_HDR_FLAG_ERROR & (rrwf | rtrf))?ECONNRESET:0;

	return free_bytes;
}; /* get_free_bytes() */

#define INC_PTR(x,y,z) x=htonl((ntohl(x)+y)%z)

//static inlineo
// Returns:
// 0-xxx Number of bytes sent by the other end of the connection.
// -1 - no more bytes will ever be seen from this link partner
#define AVAIL_BYTES_END -1
#define AVAIL_BYTES_ERROR -2
int  get_avail_bytes(struct rskt_buf_hdr volatile *hdr, uint32_t buf_sz)
{
	uint32_t avail_bytes = 0;

	DBG("ENTER");
	if (hdr == NULL) {
		return 0;
	}

	uint32_t rrw = ntohl(hdr->rem_rx_wr_ptr);
	uint32_t lrr = ntohl(hdr->loc_rx_rd_ptr);

	errno = 0;
	
	if (!(hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT))) {
		/* There cannot be any bytes available */
		return 0;
	};

	if ((hdr->loc_rx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_ERROR)) ||
		(hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_ERROR))) {
		/* Error condition signalled, something's busted... */
		return AVAIL_BYTES_ERROR;
	};

	avail_bytes = rrw - lrr - 1;
	if (rrw < lrr) {
		avail_bytes = buf_sz - lrr + rrw - 1;
	};

	if (!avail_bytes) {
		if (hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING)) {
			avail_bytes = AVAIL_BYTES_END;
		};
	};

	DBG("EXIT %d", avail_bytes);
	return avail_bytes;
}; /* get_avail_bytes() */

void read_bytes(struct rskt_socket_t *skt, void *data, uint32_t byte_cnt)
{
	uint32_t first_offset = (ntohl(skt->hdr->loc_rx_rd_ptr) + 1)
			% skt->buf_sz;
	memcpy(data, (void *)(skt->rx_buf + first_offset), byte_cnt);
	INC_PTR(skt->hdr->loc_rx_rd_ptr, byte_cnt, skt->buf_sz);
};

#ifdef __cplusplus
}
#endif

