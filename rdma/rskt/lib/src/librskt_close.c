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
#include "librskt_utils.h"
#include "librskt_buff.h"

int cleanup_skt(rskt_h skt_h, volatile struct rskt_socket_t *skt)
{
	int dma_flushed = 0;
	
	if (skt->msubh_valid && skt->hdr) {
		if (0 > get_avail_bytes(skt->hdr, skt->buf_sz)) {
			dma_flushed = 1;
		};
	};

	if (lib.use_mport == SIX6SIX_FLAG) { /// TODO memops
		if (skt->memops != NULL) {
			skt->memops->free_xwin(((struct rskt_socket_t*)skt)->memops_ibwin); // XXX g++ makes me de-volatile skt
			delete skt->memops;
			skt->memops = NULL;
		}
		skt->msub_p = NULL;
	} else if (lib.use_mport) {
		if (NULL != skt->msub_p) {
			int rc = riomp_dma_unmap_memory(lib.mp_h, skt->msub_sz, 
						(void *)skt->msub_p);
			if (rc) {
				ERR("sn %d failed to unmap memory",
					skt->sa.sn);
				dma_flushed = 0;
			}
		};
	} else {
		dma_flushed &= cleanup_skt_rdma(skt_h, skt);
	};

	INFO("dma_flushed = %d", dma_flushed);
	if (rsktl_put_socket(&lib.skts, skt_h)) {
		ERR("rskt_put_socket`failed.");
	};
	return dma_flushed;
};

int rskt_close_locked(rskt_h skt_h)
{
	struct librskt_app_to_rsktd_msg *tx;
	struct librskt_rsktd_to_app_msg *rx;
	struct rdma_xfer_ms_in hdr_in;
	struct rskt_socket_t *l_skt;
	bool ms_name_valid = false;
	char ms_name[MAX_MS_NAME+1] = {0};
	uint64_t phy_addr = 0;
	uint32_t saved_sn = 0; 
	int close_rc = 0;
	int dma_flushed = 1;
	int delay_cnt = 10000;

	DBG("ENTER");
	l_skt = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == l_skt) {
		errno = 0;
		return 0;
	};

	switch(rsktl_get_st(&lib.skts, skt_h)) {
	case rskt_max_state:
		/* Can't access state, just exit */
		goto exit;
		break;

	case rskt_close_by_remote:
                /* Close by remote means that this end has been flushed by
		* the remote.  We don't know if the remote has seen our
		* dma flush yet.
		*
		* Wait until the socket cannot be found, indicating that the
		* other end closed it.  On timeout, proceed to close the socket.
		*/
		while (delay_cnt-- && (NULL != l_skt)) {
			usleep(100);
			l_skt = rsktl_sock_ptr(&lib.skts, skt_h);
		};
		if (NULL == l_skt) {
               		goto exit;
		};
		goto cleanup;
		break;

        case rskt_connecting:
        case rskt_connected:
		/* Check to see if we're the end that initiateed closure,
		* or if the other end has closed transmission.
		*/

		INFO("Flags Loc 0x%x Rem 0x%x",
			ntohl(l_skt->hdr->loc_tx_wr_flags),
			ntohl(l_skt->hdr->rem_rx_wr_flags));
		if (RD_SKT_CLOSING(l_skt)) {
			/* Other side already set the flag */
			rsktl_set_st(&lib.skts, skt_h, rskt_close_by_remote);
		} else {
			rsktl_set_st(&lib.skts, skt_h, rskt_close_by_local);
		};

		if (l_skt->hdr->loc_tx_wr_flags &
					htonl(RSKT_BUF_HDR_FLAG_CLOSING)) {
			/* Something stupid happenned - we're connected, but
			* our flags are set to indicate closure. Print an
			* error log and cleanup.
			*/

			ERR("SN %d connected but close flag already set?");
			goto cleanup;
		};

		/* Indicate to remote side that the connection is closing.
		* This should translate to rskt_read()
		* returning 0 bytes read, and rskt_write returning 
		* -EPIPE, or allow rskt_close_locked to continue to completion.
		*/
		l_skt->hdr->loc_tx_wr_flags |= htonl(RSKT_BUF_HDR_FLAG_CLOSING);
		l_skt->hdr->loc_rx_rd_flags |= htonl(RSKT_BUF_HDR_FLAG_CLOSING);
		hdr_in.loc_msubh = l_skt->msubh;
		hdr_in.rem_msubh = l_skt->con_msubh;
		hdr_in.priority = 0;
		hdr_in.sync_type = rdma_sync_chk;
		if (update_remote_hdr(l_skt, &hdr_in)) {
			l_skt->hdr->loc_tx_wr_flags |=
					htonl(RSKT_BUF_HDR_FLAG_ERROR);
			l_skt->hdr->loc_rx_rd_flags |=
					htonl(RSKT_BUF_HDR_FLAG_ERROR);
			ERR("Failed in update_remote_hdr");
			goto cleanup;
		}

		if (rskt_close_by_remote == rsktl_get_st(&lib.skts, skt_h)) {
			goto exit;
		};

		/* Already set our own flag.  If remote is not done, continue.*/
	case rskt_shutting_down:
	case rskt_close_by_local:
		/* We're the side that initiated socket closure. Wait a while
 		* to see if the other side sets the close flag, then cleanup.  
		*/
		while (delay_cnt--) {
			dma_flushed = rsktl_atomic_chk_flush(&lib.skts, skt_h);
			if (!dma_flushed) {
				usleep(30);
				continue;
			};
			break;
		};
		if (dma_flushed < 0) {
			dma_flushed = 0;
		};

	case rskt_reqbound:
        case rskt_bound  :
	case rskt_reqlisten:
        case rskt_listening:
        case rskt_accepting:
	case rskt_reqconnect:
cleanup:
		tx = alloc_app2d();
		rx = alloc_d2app();
	
		tx->msg_type = LIBRSKTD_CLOSE;
		tx->a_rq.msg.close.sn = htonl(l_skt->sa.sn);
		tx->a_rq.msg.close.dma_flushed = htonl(1);
	
		close_rc = librskt_dmsg_req_resp(tx, rx, TIMEOUT);
		free(rx);
	default:
		break;
	};

	DBG("Calling cleanup_skt()");
	l_skt = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == l_skt) {
		errno = 0;
		return 0;
	};
	if (rsktl_set_st(&lib.skts, skt_h, rskt_closed)) {
		errno = 0;
		return 0;
	};

	if (l_skt->msh_valid) {
		ms_name_valid = true;
		phy_addr = l_skt->phy_addr;
		memcpy(ms_name, (void *)l_skt->msh_name, MAX_MS_NAME);
		saved_sn = l_skt->sa.sn;
	};

	dma_flushed = cleanup_skt(skt_h, l_skt);

	/* Confirm to Daemon that memory space has been closed */
	if (ms_name_valid) {
		struct librskt_rsktd_to_app_msg *resp;

		tx = alloc_app2d();
		rx = alloc_d2app();
	
		tx->msg_type = LIBRSKTD_RELEASE;
		tx->a_rq.msg.release.sn = htonl(saved_sn);
		tx->a_rq.msg.release.use_addr = htonl(!!(uint32_t)lib.use_mport);
		tx->a_rq.msg.release.dma_flushed =
						htonl(dma_flushed && !close_rc);

		PACK_PTR(phy_addr, tx->a_rq.msg.release.p_addr_u,
				tx->a_rq.msg.release.p_addr_l);
		memcpy(tx->a_rq.msg.release.ms_name, ms_name, MAX_MS_NAME+1);
		
		DBG("SN %d MS %s LIBRSKTD_RELEASE", saved_sn,
			tx->a_rq.msg.release.ms_name);
		librskt_dmsg_req_resp(tx, rx, TIMEOUT);
		resp = (struct librskt_rsktd_to_app_msg *)rx;
		if (resp->a_rsp.err) 
			CRIT("SN %d MS %s LIBRSKTD_RELEASE Error %d %s", 
				l_skt->sa.sn, 
				resp->a_rsp.req.msg.release.ms_name,
				ntohl(resp->a_rsp.err),
				strerror(ntohl(resp->a_rsp.err)));
		free(rx);
	};
exit:
	return -errno;
}; /* rskt_close_locked() */

#ifdef __cplusplus
}
#endif

