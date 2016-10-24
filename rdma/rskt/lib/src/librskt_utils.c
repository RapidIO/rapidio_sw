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

#define RDMA_ACC_TO_SECS 30
#define RDMA_CONN_TO_SECS 5
#define RDMA_CONN_POLL_USECS 50

int setup_skt_ptrs(struct rskt_socket_t *volatile skt)
{
	struct rdma_xfer_ms_in hdr_in;
	int    rc = 0;
	int delay;

	DBG("ENTER");
/*
	[11:48:20 AM] barry.wood99: At the start, set A,B,!C, poll for A,B,!C or !A,B,!C
	[11:48:40 AM] barry.wood99: Next, set !A, B, !C and poll for !A, B, !C or !A, !B, C.
	[11:49:07 AM] barry.wood99: Last, Set !A !B C and poll for !A !B C.

	A => RSKT_BUF_HDR_FLAG_INIT_DONE
	B => RSKT_BUF_HDR_FLAG_ZEROED
	C => RSKT_BUF_HDR_FLAG_INIT
*/
	/**
	 * Memory has been zeroed. Initialize the buffer flags, set the
	 * flags INIT_DONE and ZEROED, then update the remote header.
	 */
	skt->con_sz = (skt->msub_sz > skt->con_sz)?skt->con_sz:skt->msub_sz;
	skt->buf_sz = (skt->con_sz - sizeof(struct rskt_buf_hdr))/2;
	skt->tx_buf = skt->msub_p + sizeof(struct rskt_buf_hdr);
	skt->rx_buf = skt->tx_buf + skt->buf_sz;

	skt->hdr->loc_tx_wr_ptr = htonl(0);
	skt->hdr->loc_tx_wr_flags = htonl(RSKT_BUF_HDR_FLAG_ZEROED) |
				    htonl(RSKT_BUF_HDR_FLAG_INIT_DONE);
	skt->hdr->loc_rx_rd_ptr = htonl(skt->buf_sz - 1);
	skt->hdr->loc_rx_rd_flags = htonl(RSKT_BUF_HDR_FLAG_ZEROED) |
				    htonl(RSKT_BUF_HDR_FLAG_INIT_DONE);

	DBG("Set ZEROED and INIT_DONE");
	DBG("skt->buf_sz=0x%X, loc_tx_wr_ptr=0x%X, loc_rx_rd_ptr=0x%X",
						skt->buf_sz,
						ntohl(skt->hdr->loc_tx_wr_ptr),
						ntohl(skt->hdr->loc_rx_rd_ptr));
	hdr_in.loc_msubh = skt->msubh;
	hdr_in.rem_msubh = skt->con_msubh;
	hdr_in.priority = 0;
	hdr_in.sync_type = rdma_sync_chk;
	rc = update_remote_hdr(skt, &hdr_in);
	if (rc) {
		ERR("Failed to update remote header, rc = %d", rc);
		goto exit;
	}

	/**
	 * Poll for INIT_DONE, ZEROED, and !INIT in the remote header
	 */
	/* COND1: Both INIT_DONE and ZEROED are set but INIT is cleared (A,B,!C) */
#define ERR_FLAG ( \
	   (skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING | RSKT_BUF_HDR_FLAG_LP_EXIT)) \
	|| (skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING | RSKT_BUF_HDR_FLAG_LP_EXIT)) \
	|| (skt->hdr->loc_tx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING | RSKT_BUF_HDR_FLAG_LP_EXIT)) \
	|| (skt->hdr->loc_rx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING | RSKT_BUF_HDR_FLAG_LP_EXIT)) \
		)

#define COND1 ( \
	   (skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE | RSKT_BUF_HDR_FLAG_ZEROED)) \
       &&  (skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE | RSKT_BUF_HDR_FLAG_ZEROED)) \
       &&  ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) == 0) \
       &&  ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) == 0) \
              )
	/* COND2: !INIT_DONE, ZEROED, !INIT  (!A, B, !C) */
#define COND2  ( \
	 ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
      && ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
      &&  (skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) \
      &&  (skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) \
      && ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) == 0) \
      && ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) == 0) \
      	       )

	DBG("skt->hdr->rem_rx_wr_flags = 0x%08X, "
		"skt->hdr->rem_tx_rd_flags 0x%08X",
			ntohl(skt->hdr->rem_rx_wr_flags),
			ntohl(skt->hdr->rem_tx_rd_flags));
	DBG("Waiting for INIT_DONE and ZEROED or for ZEROED only");
	delay = 100000;
	while (!ERR_FLAG && !COND1 && !COND2 && delay--) {
		usleep(SETUP_SKT_PTRS_POLL);
	}

	if ((!COND1 && !COND2) || (ERR_FLAG)) {
		DBG("skt->hdr->rem_rx_wr_flags = 0x%08X,"
			" skt->hdr->rem_tx_rd_flags = 0x%08X",
				ntohl(skt->hdr->rem_rx_wr_flags),
				ntohl(skt->hdr->rem_tx_rd_flags));
		DBG("FAILED wait INIT_DONE and ZEROED or for ZEROED only");
		rc = -1;
		goto exit;
	};
#undef COND1
#undef COND2

	/**
	 * Clear local INIT_DONE and update the remote header again.
	 * !A, B, !C
	 */
	/* ZEROED set above (B), INIT cleared above (!C) */
	/* Just clear A (INIT_DONE) */
	skt->hdr->loc_rx_rd_flags &= htonl(~RSKT_BUF_HDR_FLAG_INIT_DONE);
	skt->hdr->loc_tx_wr_flags &= htonl(~RSKT_BUF_HDR_FLAG_INIT_DONE);
	DBG("Cleared INIT_DONE");
	rc = update_remote_hdr(skt, &hdr_in);
	if (rc) {
		ERR("Failed to update remote header, rc = %d", rc);
		goto exit;
	}

	/* COND1: !INIT_DONE, ZEROED, and !INIT (!A, B, !C) */
#define COND1 ( \
	   ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
	&& ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
	&&  (skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) \
	&&  (skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) \
	&& ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) == 0) \
	&& ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) == 0) \
	      )

	/* COND2: !INIT_DONE, !ZEROED, and INIT  (!A, !B, C) */
#define COND2 ( \
	   ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
	&& ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
	&& ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) == 0) \
	&& ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) == 0) \
	&&  (skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) \
	&&  (skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) \
	      )

	DBG("skt->hdr->rem_rx_wr_flags = 0x%08X, "
		"skt->hdr->rem_tx_rd_flags = 0x%08X",
			ntohl(skt->hdr->rem_rx_wr_flags),
			ntohl(skt->hdr->rem_tx_rd_flags));
	DBG("Waiting for ZEROED only or INIT only");
	delay = 100000;
	while (!ERR_FLAG && !COND1 && !COND2 && delay--) {
		usleep(SETUP_SKT_PTRS_POLL);
	}
	if ((!COND1 && !COND2) || ERR_FLAG) {
		DBG("skt->hdr->rem_rx_wr_flags = 0x%08X,"
			" skt->hdr->rem_tx_rd_flags = 0x%08X",
				ntohl(skt->hdr->rem_rx_wr_flags),
				ntohl(skt->hdr->rem_tx_rd_flags));
		DBG("FAILED for ZEROED only or INIT only");
		rc = -1;
		goto exit;
	};
#undef COND1
#undef COND2

	/**
	 * Clear INIT_DONE, ZEROED and set INIT flag then update remote header.
	 * (!A, !B, C)
	 */
	/* INIT_DONE already cleared above */
	skt->hdr->loc_rx_rd_flags &= htonl(~RSKT_BUF_HDR_FLAG_ZEROED);
	skt->hdr->loc_tx_wr_flags &= htonl(~RSKT_BUF_HDR_FLAG_ZEROED);
	skt->hdr->loc_rx_rd_flags |= htonl(RSKT_BUF_HDR_FLAG_INIT);
	skt->hdr->loc_tx_wr_flags |= htonl(RSKT_BUF_HDR_FLAG_INIT);
	DBG("Cleared ZEROED and Set INIT");
	rc = update_remote_hdr(skt, &hdr_in);
	if (rc) {
		ERR("Failed to update remote header, rc = %d", rc);
		goto exit;
	}

	/**
	 * Poll for !INIT_DONE, !ZEROED, INIT (!A, !B, C)
	 */
#define COND1 ( \
	   ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
	&& ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT_DONE)) == 0) \
	&& ((skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) == 0) \
	&& ((skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_ZEROED)) == 0) \
	&&  (skt->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) \
	&&  (skt->hdr->rem_tx_rd_flags & htonl(RSKT_BUF_HDR_FLAG_INIT)) \
	      )
	DBG("skt->hdr->rem_rx_wr_flags = 0x%08X, "
		"skt->hdr->rem_tx_rd_flags= 0x%08X",
			ntohl(skt->hdr->rem_rx_wr_flags),
			ntohl(skt->hdr->rem_tx_rd_flags));
	DBG("Waiting for INIT only");
	delay = 100000;
	while (!ERR_FLAG && !COND1 && delay--) {
		usleep(SETUP_SKT_PTRS_POLL);
	}
	if ((!COND1) || ERR_FLAG) {
		DBG("FAILED for INIT only");
		rc = -1;
		goto exit;
	};
#undef COND1

exit:
	DBG("EXIT rc=%d", rc);
	return rc;
}; /* setup_skt_ptrs() */

int rskt_accept_rdma_open(struct rskt_socket_t * volatile skt)
{
	int rc;

	DBG("ACCEPT OPEN_MSO %s", skt->msoh_name);
	rc = rdma_open_mso_h((const char *)skt->msoh_name, (mso_h *)&skt->msoh);
	DBG("ACCEPT OPEN_MSO %s DONE", skt->msoh_name);
	if (rc) {
		if (rc == RDMA_ALREADY_OPEN) {
			INFO("MSO was already open, got back the same handle");
		} else {
			ERR("Failed to open ms(%s)", skt->msh_name);
			goto fail;
		}
	}
	skt->msoh_valid = 1;

	DBG("ACCEPT OPEN_MS %s", skt->msh_name);
	rc = rdma_open_ms_h((const char *)skt->msh_name, skt->msoh, 0, 
			(uint32_t *)&skt->msub_sz, (ms_h *)&skt->msh);
	DBG("ACCEPT OPEN_MS %s DONE", skt->msh_name);
	if (rc) {
		ERR("Failed to open ms(%s) rc %x", skt->msh_name, rc);
		goto fail;
	}
	skt->msh_valid = 1;

	DBG("ACCEPT CREATE_MSUB");
	rc = rdma_create_msub_h(skt->msh, 0,
				skt->msub_sz, 0, (msub_h *)&skt->msubh);
	DBG("ACCEPT CREATE_MSUB 0x%lx", skt->msubh);
	if (rc) {
		ERR("Failed to create msub rc %d", rc);
		goto fail;
	}
	skt->msub_p = NULL;
	skt->msubh_valid = 1;

	DBG("ACCEPT MMAP_MSUB");
	rc = rdma_mmap_msub(skt->msubh, (void **)&skt->msub_p);
	if (rc) {
		ERR("Failed to mmap msub");
		goto fail;
	}

	DBG("ACCEPT: MSOH %p MSH %p MSUBH %p PTR %p",
		skt->msoh, skt->msh, skt->msubh, skt->msub_p);
	/* Zero the entire msub (we can do that because we'll initialize
	 * all pointers below).
	 */
	memset((void *)skt->msub_p, 0, skt->msub_sz);

	do {
		rc = rdma_accept_ms_h(skt->msh, skt->msubh,
				(conn_h *)&skt->connh,
				(msub_h *)&skt->con_msubh,
				(uint32_t *)&skt->con_sz, RDMA_ACC_TO_SECS);
	} while (rc == RDMA_ACCEPT_TIMEOUT);
	if (rc) {
		ERR("Failed in rdma_accept_ms_h()");
		goto fail;
	}
fail:
	return rc;
};

int rskt_accept_msg(rskt_h l_skt_h, rskt_h *skt_h, struct rskt_sockaddr *sktaddr)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	enum rskt_state exp_st = rskt_accepting;
	struct rskt_socket_t *sock, *l_sock;


	DBG("ENTER");

	l_sock = rsktl_sock_ptr(&lib.skts, l_skt_h);
	if (NULL == l_sock) {
		errno = EBADFD;
		goto fail;
	};

	tx = alloc_app2d();
	rx = alloc_d2app();

	tx->msg_type = LIBRSKTD_ACCEPT;
	tx->a_rq.msg.accept.sn = htonl(l_sock->sa.sn);
	
	if (rsktl_atomic_set_st(&lib.skts, l_skt_h, rskt_listening, exp_st) 
								!= exp_st) {
		errno = EBADFD;
		goto fail;
	};

	if (librskt_dmsg_req_resp(tx, rx, TIMEOUT)) {
		goto fail;
	};
	tx = NULL;

	exp_st = rskt_listening;
	if (rsktl_atomic_set_st(&lib.skts, l_skt_h, rskt_accepting, exp_st)
								!= exp_st) {
		errno = EBADFD;
		goto fail;
	};

	if (lib.use_mport != !!rx->a_rsp.msg.accept.use_addr) {
		ERR("ACCEPT response lib.use_mport %d use_addr %d",
			lib.use_mport, !!rx->a_rsp.msg.accept.use_addr);
		goto fail;
	};

	*skt_h = rskt_create_socket();
	sock = rsktl_sock_ptr(&lib.skts, *skt_h);
	if (NULL == sock) {
		ERR("Failed to allocate skt_h... exiting");
		goto fail;
	};
	l_sock = rsktl_sock_ptr(&lib.skts, l_skt_h);
	if (NULL == l_sock) {
		errno = EBADFD;
		goto fail;
	};
	memcpy((void *)sock, (void *)l_sock, sizeof(struct rskt_socket_t));
	sock->max_backlog = 0;
	
	exp_st = rskt_reqconnect;
	if (rsktl_atomic_set_st(&lib.skts, *skt_h, rskt_alloced, exp_st)
								!= exp_st) {
		errno = EBADFD;
		goto fail;
	};

	sock->connector = skt_rdma_acceptor;
	sock->sa.sn = ntohl(rx->a_rsp.msg.accept.new_sn);
	sock->sa.ct = ntohl(rx->a_rsp.msg.accept.new_ct);
	sock->sai.sa.ct = ntohl(rx->a_rsp.msg.accept.peer_sa.ct);
	sock->sai.sa.sn = ntohl(rx->a_rsp.msg.accept.peer_sa.sn);
	UNPACK_PTR(rx->a_rsp.msg.accept.r_addr_u, rx->a_rsp.msg.accept.r_addr_l,
								sock->rio_addr);
	memcpy((void *)sock->msoh_name, rx->a_rsp.msg.accept.mso_name,
								MAX_MS_NAME);
	memcpy((void *)sock->msh_name, rx->a_rsp.msg.accept.ms_name,
								MAX_MS_NAME);
	sock->msub_sz = ntohl(rx->a_rsp.msg.accept.ms_size);
	sock->con_sz = ntohl(rx->a_rsp.msg.accept.ms_size);
	UNPACK_PTR(rx->a_rsp.msg.accept.p_addr_u, rx->a_rsp.msg.accept.p_addr_l,
								sock->phy_addr);
	sktaddr->sn = sock->sai.sa.sn;
	sktaddr->ct = sock->sai.sa.ct;

	free(rx);

	DBG("EXIT");

	return 0;

fail:
	if (NULL != tx)
		free(tx);
	if (NULL != rx)
		free(rx);
	DBG("EXIT, FAIL");

	return 1;
};

int rskt_accept_init(rskt_h l_skt_h, rskt_h skt_h)
{
	int rc = 0;
	struct rskt_socket_t *sock = rsktl_sock_ptr(&lib.skts, skt_h);
	enum rskt_state exp_st = rskt_connected;

	DBG("ENTER");

	if (lib.use_mport) {
		DBG("ACCEPT: SN %d CT %d REM SN %d CT %d pa 0x%lx ra 0x%lx",
			sock->sa.sn, sock->sa.ct, sock->sai.sa.sn,
			sock->sai.sa.ct, sock->phy_addr, sock->rio_addr );
	} else {
		DBG("ACCEPT: SN %d CT %d REM SN %d CT %d MSOH \"%s\" MSH \"%s\"", 
			sock->sa.sn, sock->sa.ct, sock->sai.sa.sn, 
			sock->sai.sa.ct, sock->msoh_name, sock->msh_name);
	};

	if (lib.use_mport == SIX6SIX_FLAG) { /// TODO memops rskt_accept
		rskt_init_memops(sock);
	} else if (lib.use_mport) {
		rc = riomp_dma_map_memory(lib.mp_h, sock->con_sz,
				sock->phy_addr, (void **)&sock->msub_p);
		if (rc) {
			CRIT("Failed to map 0x%lx size 0x%x",
				sock->phy_addr, sock->msub_sz);
			goto fail;
		}
		memset((void *)sock->msub_p, 0, sock->msub_sz);
		sock->msh_valid = 1;
	} else {
		rc = rskt_accept_rdma_open(sock);
	};
	if (rc) {
		goto fail;
	}

	if (setup_skt_ptrs(sock)) {
		ERR("Failed in setup_skt_ptrs");
		goto fail;
	}
	if (rsktl_atomic_set_st(&lib.skts, skt_h, rskt_reqconnect, exp_st) != exp_st) {
		errno = EBADFD;
		goto fail;
	};

	DBG("EXIT");
	return 0;

fail:
	DBG("EXIT, FAILED.");
	return 1;
};

int rskt_connect_rdma_open(struct rskt_socket_t * volatile skt)
{
	int conn_retries = RDMA_CONN_TO_SECS * 1000000 / RDMA_CONN_POLL_USECS;
	int rc = rdma_open_mso_h(skt->msoh_name, &skt->msoh);

	if (rc) {
		ERR("rdma_open_mso_h() failed msoh_name(%s)..closing",
								skt->msoh_name);
		goto fail;
	}
	skt->msoh_valid = 1;

	if (lib.all_must_die)
		goto fail;

	rc = rdma_open_ms_h(skt->msh_name, skt->msoh, 0, 
			&skt->msub_sz, &skt->msh);
	if (rc || !skt->msub_sz) {
		ERR("rdma_open_ms_h() failed msh_name(%s)..closing",
								skt->msh_name);
		goto fail;
	}
	skt->msh_valid = 1;

	if (lib.all_must_die)
		goto fail;

	rc = rdma_create_msub_h(skt->msh, 0,
				skt->msub_sz, 0, &skt->msubh);
	if (rc) {
		ERR("rdma_create_msub() failed..closing");
		goto fail;
	}
	skt->msubh_valid = 1;

	if (lib.all_must_die)
		goto fail;

	rc = rdma_mmap_msub(skt->msubh, (void **)&skt->msub_p);
	if (rc) {
		ERR("rdma_mmap_msub() failed..closing");
	}

	memset((void *)skt->msub_p, 0, skt->msub_sz);

	if (lib.all_must_die)
		goto fail;

	rc = 0;
	do {
		if (RDMA_CONNECT_FAIL == rc) {
			struct timespec req = {0, RDMA_CONN_POLL_USECS * 1000};
			struct timespec rem = {0, 0};
			int rc = 0;
	
			errno = 0;
			do {
				if (rc && (EINTR == errno))
					req = rem;
				rc = (nanosleep(&req, &rem));
			} while (rc && (EINTR == errno));
		}

		rc = rdma_conn_ms_h(16, skt->sai.sa.ct,
				skt->con_msh_name, skt->msubh,
				&skt->connh,
				&skt->con_msubh, &skt->con_sz,
				&skt->con_msh, RDMA_CONN_TO_SECS);
	} while ((rc == RDMA_CONNECT_FAIL) && conn_retries-- && !lib.all_must_die);

	if (rc) {
		ERR("rdma_conn_ms_h() failed, retries = %d, rc = 0x%X..closing",
				rc, conn_retries);
		goto fail;
	}
	HIGH("CONNECTED, skt->con_msh = 0x%" PRIx64 "", skt->con_msh);
fail:
	return rc;
};

void rskt_init_memops(struct rskt_socket_t * volatile skt)
{
#ifdef CUT_OUT_MEMOPS
	assert(skt);

	// defaults for using libmport
	bool shared = true;
	int  chan = ANY_CHANNEL;

	DBG("ENTER");

	if (NULL != getenv("RSKT_UMD_UNSHARED")) shared = false; // flag use of UMD standalone
	if (NULL != getenv("RSKT_UMD_CHAN")) {
		int chn = -1;
		if (sscanf(getenv("RSKT_UMD_CHAN"), "%d", &chn) != 1)
			throw std::runtime_error("rskt_accept: Invalid content of $RSKT_UMD_CHAN");
		if (chn < 0) 
			throw std::runtime_error("rskt_accept: Negative $RSKT_UMD_CHAN");
		if (chn > 7) 
			throw std::runtime_error("rskt_accept: Invalid/exceeding-7 $RSKT_UMD_CHAN");
		chan = chn;
	}
	
	skt->memops = RIOMemOpsChanMgr(lib.mpnum, shared, chan);
	assert(skt->memops);

	if (!shared && chan != ANY_CHANNEL) { // Standalone UMD
		RIOMemOpsUMD* mops_umd = dynamic_cast<RIOMemOpsUMD*>(skt->memops);

		// TODO pick up bufc and sts from $RSKT_UMD_BUFC and $RSKT_UMD_STS

		bool r = mops_umd->setup_channel(0x100 /*bufc*/, 0x400 /*sts aka FIFO*/);
		assert(r);
		r = mops_umd->start_fifo_thr(-1 /*isolcpu*/);
		assert(r);
	}

	memset(&skt->memops_ibwin, 0, sizeof(skt->memops_ibwin));

	const uint64_t rio_address = RIOMP_MAP_ANY_ADDR;

        bool r = skt->memops->alloc_ibwin_fixd(skt->memops_ibwin /*out*/, rio_address, skt->phy_addr /*handle*/, skt->con_sz);
	assert(r);

	skt->msub_p = (volatile uint8_t*)skt->memops_ibwin.win_ptr;

	memset((void *)skt->msub_p, 0, skt->msub_sz);

	skt->msh_valid = 1;
	DBG("EXIT");
#endif
}

int cleanup_skt_rdma(rskt_h skt_h, volatile struct rskt_socket_t *skt)
{
	int dma_flushed = 1;
#if defined(RDMA_LL) // avoid stupid compiler unused param warning
	if(RDMA_LL < RDMA_LL_DBG) { skt_h += 0; }
#endif
	DBG("sn %d ENTER with skt->connector = %d", skt->sa.sn, skt->connector);
	if (skt_rmda_uninit != skt->connector) { 
		DBG("sn %d skt->connector != skt_rdma_uninit", skt->sa.sn);
		if (skt->msub_p != NULL)  {
			DBG("Unmapping skt->msub_p(%p)", skt->msub_p);
			rdma_munmap_msub(skt->msubh, (void *)skt->msub_p);
			skt->msub_p = NULL;
			skt->rx_buf = NULL;
			skt->tx_buf = NULL;
			skt->con_sz = 0;
			skt->msub_sz = 0;
		} else {
			DBG("sn %d skt->msub_p is NULL", skt->sa.sn);
		}
		if (skt_rdma_connector == skt->connector) {
			DBG("sn %d skt->connector == skt_rdma_connector, disconnecting msubh 0x%lx", skt->sa.sn, skt->msubh);
                        if (rdma_disc_ms_h(skt->connh, skt->con_msh,
								skt->msubh)) {
				dma_flushed = 0;
				ERR("rdma_disc_ms_h failed");
                        };
		}
		DBG("sn %d skt->connector != skt_rdma_connector", skt->sa.sn);
		if (skt->msubh_valid) {
			DBG("sn %d skt->msubh_valid is true. Closing skt->msubh 0x%lx", skt->sa.sn, skt->msubh);
			if (rdma_destroy_msub_h(skt->msh, skt->msubh)) {
				dma_flushed = 0;
				ERR("rdma_destroy_msub_h failed");
			};
			skt->msubh_valid = 0;
		} else {
			WARN("sn %d skt->msubh_valid is false", skt->sa.sn);
		}
		if (skt->msh_valid) {
			DBG("sn %d skt->msh_valid is true. Closing skt->msh", skt->sa.sn, skt->msh);
			if (rdma_close_ms_h(skt->msoh, skt->msh)) {
				dma_flushed = 0;
				ERR("rdma_close_ms_h failed");
			};
			skt->msh_valid = 0;
		} else {
			WARN("sn %d skt->msh_valid is false", skt->sa.sn);
		}
	} else {
		DBG("sn %d skt->connector == skt_rmda_uninit", skt->sa.sn);
	}
	// li can be null if we're closing a socket before it has been
	// connected.
	return dma_flushed;
};

#ifdef __cplusplus
}
#endif

