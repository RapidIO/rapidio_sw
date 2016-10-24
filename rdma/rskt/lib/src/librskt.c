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
#include "librskt_close.h"

struct librskt_globals lib;

int librskt_init(int rsktd_port, int rsktd_mpnum)
{
	int rc = 0;
	struct librskt_app_to_rsktd_msg *req = NULL;
	struct librskt_rsktd_to_app_msg *resp = NULL;

	lib.init_ok = 0;
	/* If library already running successfully, just return */
	if ((lib.portno == rsktd_port) && (lib.mpnum == rsktd_mpnum) &&
			(lib.init_ok == rsktd_port))
		return 0;

	/* Not connected to intended port/mport, so fail */
	if ((lib.portno == lib.init_ok) && (lib.portno) &&
		((lib.portno != rsktd_port) || (lib.mpnum != rsktd_mpnum)))
		return 1;

	DBG("ENTER");
	memset(&lib, 0, sizeof(struct librskt_globals));
	if (rsktl_init(&lib.skts)) {
		ERR("ERROR on rsktl_list_init %s", strerror(errno));
		goto fail;
	};

	lib.fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == lib.fd) {
		ERR("ERROR on librskt_init socket(): %s", strerror(errno));
		goto fail;
	};
	DBG("lib.fd = %d, rsktd_port = %d", lib.fd, rsktd_port);

	lib.addr_sz = sizeof(struct sockaddr_un);
	memset(&lib.addr, 0, lib.addr_sz);

	lib.portno = rsktd_port;
	lib.mpnum = rsktd_mpnum;
	lib.init_ok = 0;
	lib.ct = -1;

	lib.addr.sun_family = AF_UNIX;
	snprintf(lib.addr.sun_path, sizeof(lib.addr.sun_path) - 1,
		LIBRSKTD_SKT_FMT, rsktd_port, rsktd_mpnum);
	DBG("Attempting to connect to RSKTD via Unix sockets");
	if (connect(lib.fd, (struct sockaddr *) &lib.addr, 
				lib.addr_sz)) {
		ERR("ERROR on librskt_init connect: %s", strerror(errno));
		goto fail;
	};
	DBG("CONNECTED to RSKTD");

	lib.all_must_die = 0;

	if (librskt_init_threads()) {
		goto fail;
	};

	/* Socket appears to be open, say hello to RSKTD */
	req = alloc_app2d();
	if (req == NULL) {
		CRIT("Failed to calloc 'req'");
		goto fail;
	}

	resp = alloc_d2app();
	if (resp == NULL) {
		CRIT("Failed to calloc 'resp'");
		free_app2d(req);
		goto fail;
	}

	memset(req, 0, A2RSKTD_SZ);
	memset(resp, 0, RSKTD2A_SZ);
	req->msg_type = LIBRSKTD_HELLO;
	req->a_rq.msg.hello.proc_num = htonl(getpid());
	memset(req->a_rq.msg.hello.app_name, 0, MAX_APP_NAME);
	snprintf(req->a_rq.msg.hello.app_name, MAX_APP_NAME-1, "%d", getpid());
	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_HELLO (A2RSKTD_SZ)");
	if (librskt_dmsg_req_resp(req, resp, TIMEOUT)) {
		ERR("ERROR on LIBRSKTD_HELLO");
		goto fail;
	};

	lib.ct = ntohl(resp->a_rsp.msg.hello.ct);
	lib.use_mport = ntohl(resp->a_rsp.msg.hello.use_mport);

	if(lib.use_mport && getenv("RSKT_MEMOPS") != NULL) lib.use_mport = SIX6SIX_FLAG;

	if (free_d2app(resp)) {
		resp = NULL;
		ERR("Could not free response %d %s", errno, strerror(errno));
		goto fail;
	};
	resp = NULL;

	if (lib.use_mport && lib.use_mport != SIX6SIX_FLAG) {
		rc = riomp_mgmt_mport_create_handle(lib.mpnum, 0, &lib.mp_h);
		if (rc) {
			ERR("Could not open mport %d", lib.mpnum);
			goto fail;
		};
	};
	lib.init_ok = rsktd_port;
fail:
	if (NULL != resp) {
		free_d2app(resp);
	};
	rc = -!((lib.init_ok == lib.portno) && (lib.portno));
	DBG("EXIT, rc = %d", rc);
	return rc;
};

void librskt_finish(void)
{	
	INFO("ENTRY");

	librskt_finish_threads();

	if (lib.fd) {
		if (close(lib.fd)) {
			ERR("close(lib.fd) failed, fd %d %d %s", lib.fd,
				errno, strerror(errno));
		};
		lib.fd = 0;
	};
	
	INFO("EXIT");
};

int lib_uninit(void)
{
	int rc = !((lib.init_ok == lib.portno) && (lib.portno));

	if (rc) {
		ERR("FAILED: lib.init_ok = %d, lib.portno = %d",
				lib.init_ok, lib.portno);
		errno = EHOSTDOWN;
	};
	return rc;
};

rskt_h rskt_create_socket(void) 
{
	rskt_h skt_h = LIBRSKT_H_INVALID;

	if (lib_uninit()) {
		errno = EINVAL;
		goto fail;
	}

	skt_h = rsktl_get_socket(&lib.skts);
	if (LIBRSKT_H_INVALID == skt_h) {
		ERR("Failed %d %s", errno, strerror(errno));
	};

fail:
	return skt_h;
};

int rskt_get_so_sndbuf(rskt_h skt_h)
{
	struct rskt_socket_t *sock;

	sock = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == sock) {
		return 0;
	}
	return sock->con_sz/2;
}

int rskt_get_so_rcvbuf(rskt_h skt_h)
{
	struct rskt_socket_t *sock;

	sock = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == sock) {
		return 0;
	}
	return sock->con_sz/2;
}

int rskt_close_locked(rskt_h skt_h);

void rskt_destroy_socket(rskt_h *skt_h) 
{
	struct rskt_socket_t *sock;

	if (NULL == skt_h) {
		errno = EINVAL;
		goto exit;
	};

	if (LIBRSKT_H_INVALID == *skt_h) {
		WARN("INVALID handle");
		goto exit;
	};

	sock = rsktl_sock_ptr(&lib.skts, *skt_h);
	if (NULL == sock) {
		goto exit;
	};

	rskt_close_locked(*skt_h);

	rsktl_put_socket(&lib.skts, *skt_h);
	*skt_h = LIBRSKT_H_INVALID;
exit:
	DBG("EXIT");
};


int rskt_bind(rskt_h skt_h, struct rskt_sockaddr *sock_addr)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	enum rskt_state st_now = rskt_alloced;
	enum rskt_state exp_st = rskt_reqbound;
	struct rskt_socket_t *sock;

	DBG("ENTER");
	errno = EINVAL;
	if (lib_uninit()) {
		goto fail;
	}

	if (NULL == sock_addr) {
		goto fail;
	}

	if (rsktl_atomic_set_st(&lib.skts, skt_h, st_now, exp_st) != exp_st) {
		ERR("rsktl_atomic_set_st not %s", SKT_STATE_STR(exp_st));
		errno = EBADFD;
		goto fail;
	};
	sock = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == sock) {
		errno = EBADFD;
		goto fail;
	};

	sock->sa.sn = sock_addr->sn;
	sock->sa.ct = lib.ct;

	tx = alloc_app2d();
	rx = alloc_d2app();

	if ((NULL == tx) || (NULL == rx)) {
		errno = ENOMEM;
		goto fail;
	};

	tx->msg_type = LIBRSKTD_BIND;
	tx->a_rq.msg.bind.sn = htonl(sock_addr->sn);

	if (librskt_dmsg_req_resp(tx, rx, TIMEOUT)) {
		ERR("librskt_dmsg_req_resp() failed");
		goto fail;
	}

	if (rx->a_rsp.err) {
		errno = EADDRNOTAVAIL;
		ERR("%s", strerror(errno));
		goto fail;
	};

	if (free_d2app(rx)) {
		rx = NULL;
		errno = ENOMEM;
		goto fail;
	};
	rx = NULL;

	st_now = exp_st;
	exp_st = rskt_bound;
	if (rsktl_atomic_set_st(&lib.skts, skt_h, st_now, exp_st) != exp_st) {
		ERR("rsktl_atomic_set_st not %s", SKT_STATE_STR(exp_st));
		errno = EBADFD;
		goto fail;
	};

	DBG("EXIT");
	return 0;
fail:
	if (NULL != rx) {
		free_d2app(rx);
	};
	DBG("EXIT_FAIL");
	return -1;
};

int rskt_listen(rskt_h skt_h, int max_backlog)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	enum rskt_state st_now = rskt_bound;
	enum rskt_state exp_st = rskt_reqlisten;
	struct rskt_socket_t *sock;

	DBG("ENTER");
	errno = EINVAL;
	if (lib_uninit()) {
		goto fail;
	}
	if (max_backlog <= 0) {
		goto fail;
	};

	if (rsktl_atomic_set_st(&lib.skts, skt_h, st_now, exp_st) != exp_st) {
		errno = EBADFD;
		goto fail;
	};

	sock = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == sock) {
		errno = EBADFD;
		goto fail;
	};

	sock->max_backlog = max_backlog;

	tx = alloc_app2d();
	rx = alloc_d2app();

	if ((NULL == tx) || (NULL == rx)) {
		errno = ENOMEM;
		goto fail;
	};

	tx->msg_type = LIBRSKTD_LISTEN;
	tx->a_rq.msg.listen.sn = htonl(sock->sa.sn);
	tx->a_rq.msg.listen.max_bklog = htonl(sock->max_backlog);
	if (librskt_dmsg_req_resp(tx, rx, TIMEOUT)) {
		ERR("librskt_dmsg_req_resp failed");
		goto fail;
	}

	if (rx->a_rsp.err) {
		errno = EBUSY;
		goto fail;
	};

	if (free_d2app(rx)) {
		rx = NULL;
		goto fail;
	};
	rx = NULL;

	st_now = exp_st;
	exp_st = rskt_listening;
	if (rsktl_atomic_set_st(&lib.skts, skt_h, st_now, exp_st) != exp_st) {
		errno = EBADFD;
		goto fail;
	};
	DBG("EXIT");
	return 0;
fail:
	DBG("EXIT_FAIL");
	if (NULL != rx) {
		free_d2app(rx);
	};
	return -1;
};

int rskt_accept(rskt_h l_skt_h, rskt_h *skt_h, struct rskt_sockaddr *sktaddr)
{
	DBG("ENTER");
	errno = EINVAL;
	
	if ((NULL == skt_h) || (NULL == sktaddr)) {
		WARN("NULL parameter passed: l_skt_h=%lxskt_h=%p, sktaddr=%p",
				l_skt_h, skt_h, sktaddr);
		goto failed;
	};
	if (LIBRSKT_H_INVALID == l_skt_h) {
		WARN("l_skt_h invalid %lx", l_skt_h);
		goto failed;
	};

	do {
		if (rskt_accept_msg(l_skt_h, skt_h, sktaddr)) {
			goto failed;
		};

		if (!rskt_accept_init(l_skt_h, *skt_h)) {
			break;
		};
		rskt_close(*skt_h);
	} while (1);
	DBG("EXIT");
	return 0;
failed:
	DBG("EXITing, FAILED");
	return -1;
};

int rskt_connect(rskt_h skt_h, struct rskt_sockaddr *sock_addr)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	enum rskt_state exp_st = rskt_reqconnect;
	struct rskt_socket_t *sock;
	int rc = -1;

	DBG("ENTER");
	if (lib_uninit()) {
		goto fail;
	}

	if (NULL == sock_addr) {
		errno = EINVAL;
		goto fail;
	}

	if (rsktl_atomic_set_st(&lib.skts, skt_h, rskt_alloced, exp_st) != exp_st) {
		errno = EBADFD;
		goto fail;
	};

	tx = alloc_app2d();
	rx = alloc_d2app();

	if ((NULL == tx) || (NULL == rx)) {
		goto fail;
	};
	tx->msg_type = LIBRSKTD_CONN;
	tx->a_rq.msg.conn.sn = htonl(sock_addr->sn);
	tx->a_rq.msg.conn.ct = htonl(sock_addr->ct);

	/* Response indicates what mso, ms, and msub to use, and 
	 * what ms to rdma_connect with
	 */
	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_CONN (A2RSKTD_SZ)");
	rc = librskt_dmsg_req_resp(tx, rx, TIMEOUT);
	if (rc) {
		ERR("librskt_dmsg_req_resp() failed..closing");
		goto fail;
	}

	if (rx->a_rsp.err) {
		errno = EBUSY;
		goto fail;
	};

	if (!!rx->a_rsp.msg.conn.use_addr != !!lib.use_mport) {
		CRIT("Received reply with use_addr %d lib.use_mport %d",
			(int)rx->a_rsp.msg.conn.use_addr, (int)lib.use_mport);
		goto fail;
	};

	DBG("Received reply to LIBRSKTD_CONN containing:");
	if (lib.use_mport) {
		DBG("p_u 0x%x p_l 0x%x r_u 0x%x r_l 0x%x loc_sn %d rem_sn %d",
			rx->a_rsp.msg.conn.p_addr_u,
			rx->a_rsp.msg.conn.p_addr_l,
			rx->a_rsp.msg.conn.r_addr_u,
			rx->a_rsp.msg.conn.r_addr_l,
			ntohl(rx->a_rsp.msg.conn.new_sn),
			ntohl(rx->a_rsp.msg.conn.rem_sn));
	} else {
		DBG("mso = %s, ms = %s, msub_sz = %d",
			rx->a_rsp.msg.conn.mso,
			rx->a_rsp.msg.conn.ms,
			rx->a_rsp.msg.conn.msub_sz)
	};

	exp_st = rskt_connecting;
	if (rsktl_atomic_set_st(&lib.skts, skt_h, rskt_reqconnect, exp_st) != exp_st) {
		errno = EBADFD;
		goto fail;
	};

	sock = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == sock) {
		errno = EBADFD;
		goto fail;
	};
	
	sock->connector = skt_rdma_connector;
	sock->sa.ct = ntohl(rx->a_rsp.msg.conn.new_ct);
	sock->sa.sn = ntohl(rx->a_rsp.msg.conn.new_sn);
	sock->sai.sa.sn = ntohl(rx->a_rsp.msg.conn.rem_sn);
	sock->sai.sa.ct = ntohl(rx->a_rsp.req.msg.conn.ct);
	UNPACK_PTR(rx->a_rsp.msg.conn.r_addr_u, rx->a_rsp.msg.conn.r_addr_l,
			sock->rio_addr);
	memcpy(sock->msoh_name, rx->a_rsp.msg.conn.mso, MAX_MS_NAME);
	memcpy(sock->msh_name, rx->a_rsp.msg.conn.ms, MAX_MS_NAME);
	memcpy(sock->con_msh_name, rx->a_rsp.msg.conn.rem_ms, MAX_MS_NAME);
	sock->msub_sz = ntohl(rx->a_rsp.msg.conn.msub_sz);
	sock->max_backlog = 0;

	if (lib.use_mport == SIX6SIX_FLAG) { /// TODO memops rskt_connect
		UNPACK_PTR(rx->a_rsp.msg.conn.p_addr_u,
				rx->a_rsp.msg.conn.p_addr_l,
				sock->phy_addr);
		sock->con_sz = ntohl(rx->a_rsp.msg.conn.msub_sz);
		rskt_init_memops(sock);
	} else if (lib.use_mport) {
		UNPACK_PTR(rx->a_rsp.msg.conn.p_addr_u,
				rx->a_rsp.msg.conn.p_addr_l,
				sock->phy_addr);
		sock->con_sz = ntohl(rx->a_rsp.msg.conn.msub_sz);
		rc = riomp_dma_map_memory(lib.mp_h, sock->con_sz,
				sock->phy_addr,
				(void **)&sock->msub_p);
		if (rc) {
			CRIT("Failed to map 0x%lx size 0x%x",
				sock->phy_addr, sock->msub_sz);
			goto fail;
		}
		memset((void *)sock->msub_p, 0, sock->msub_sz);
		sock->msh_valid = 1;
	} else {
		rc = rskt_connect_rdma_open(sock);
	};
	if (rc) {
		goto fail;
	};

	/* At this point the local buffer is mapped and zeroed.
	 * We will initialize the buffer pointers below.
	 */

	if (free_d2app(rx)) {
		rx = NULL;
		ERR("Failed in free_d2app");
		goto fail;
	};
	rx = NULL;

	rc = setup_skt_ptrs(sock);
	if (rc) {
		ERR("Failed in setup_skt_ptrs");
		goto fail;
	}
	exp_st = rskt_connected;
	if (rsktl_atomic_set_st(&lib.skts, skt_h, rskt_connecting, exp_st) != exp_st) {
		errno = EBADFD;
		goto fail;
	};

	INFO("EXIT");
	return 0;
fail:
	DBG("EXIT rc = %d", rc);
	if (tx != NULL) {
		if (free_app2d(tx)) {
			WARN("Failed in free_app2d");
		};
	};
	if (rx != NULL) {
		if (free_d2app(rx)) {
			WARN("Failed in free_d2app");
		};
	};
	return -1;
}; /* rskt_connect() */

const struct timespec rw_dly = {0, 5000};

int rskt_write(rskt_h skt_h, void *data, uint32_t byte_cnt)
{
	int rc = -1;
	uint32_t free_bytes = 0; 
	struct rdma_xfer_ms_in hdr_in;
	struct rskt_socket_t * volatile skt = NULL;
	bool curr_wip;

	DBG("ENTER");

	errno = EINVAL;
	if (lib_uninit()) {
		goto exit;
	}

	if ((NULL == data) || (1 > byte_cnt)) {
		goto exit;
	}

	/* Set write in progress to true... */
	curr_wip = rsktl_atomic_set_wip(&lib.skts, skt_h, false, true);
	if (!curr_wip) {
		errno = EBADFD;
		ERR("Could not set WIP");
		goto exit;
	};

	skt = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == skt) {
		ERR("Could not get socket pointer");
		goto fail_writing;
	};

	if (byte_cnt >= skt->buf_sz) {
		errno = E2BIG;
		ERR("Buf_sz %d, buyte_cnt %d", skt->buf_sz, byte_cnt);
		goto fail_writing;
	};

	if (WR_SKT_CLOSED(skt)) {
		errno = ECONNRESET;
		ERR("Writing to closed socket.");
		goto fail_writing;
	};

	errno = 0;
	free_bytes = get_free_bytes(skt->hdr, skt->buf_sz);

	while ((free_bytes < byte_cnt) && !errno) {
		pthread_yield();
		if (WR_SKT_CLOSED(skt)) {
			WARN("Writing to closed socket.");
			goto fail_writing;
		};
		free_bytes = get_free_bytes(skt->hdr, skt->buf_sz);
	}

	if ((byte_cnt + ntohl(skt->hdr->loc_tx_wr_ptr)) <= skt->buf_sz) {
		DBG("byte_cnt=0x%X (%d), loc_tx_wr_ptr = 0x%X, skt->buf_sz = 0x%X",
			byte_cnt, byte_cnt, skt->hdr->loc_tx_wr_ptr, skt->buf_sz);
		rc = send_bytes(skt, data, byte_cnt, &hdr_in, 0);
		if (rc) {
			ERR("send_bytes failed");
			goto fail_writing;
		};
	} else {
		uint32_t first_bytes = skt->buf_sz - 
					ntohl(skt->hdr->loc_tx_wr_ptr);

		if (send_bytes(skt, data, first_bytes, &hdr_in, 0)) {
			goto fail_writing;
		}
		DBG("Now sending byte_cnt - first_bytes = %d",
							byte_cnt - first_bytes);
		if (send_bytes(skt, (uint8_t *)data + first_bytes, 
				byte_cnt - first_bytes, &hdr_in, 1)) {
			goto fail_writing;
		}
	};
	if (update_remote_hdr(skt, &hdr_in)) {
		goto fail_writing;
	}
	rsktl_atomic_set_wip(&lib.skts, skt_h, true, false);
	DBG("EXIT");
	errno = 0;
	return byte_cnt;

fail_writing:
	if (!errno) {
		errno = ECONNRESET;
	};
	rsktl_atomic_set_wip(&lib.skts, skt_h, true, false);
exit:
	DBG("EXIT fail");
	return -1;
}; /* rskt_write() */

int rskt_read(rskt_h skt_h, void *data, uint32_t max_byte_cnt)
{
	int avail_bytes = 0;
	struct rdma_xfer_ms_in hdr_in;
	uint32_t first_offset;
	struct rskt_socket_t * volatile skt = NULL;
	bool curr_rip;

	DBG("ENTER");
	errno = EINVAL;
	if (lib_uninit()) {
		goto exit;
	}

	if ((NULL == data) || !max_byte_cnt) {
		goto exit;
	}

	curr_rip = rsktl_atomic_set_rip(&lib.skts, skt_h, false, true);
	if (!curr_rip) {
		errno = EBADFD;
		ERR("Could not set RIP");
		goto exit;
	};
	/* If skt_h->skt is NULL, the socket was closed while we were waiting
	 * for the semaphore. Just exit.
	 */
	skt = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == skt) {
		ERR("Could not get socket pointer");
		goto fail_reading;
	};

	/* Wait forever, until socket closed or we receive something */
	errno = 0;
	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);

	// avail_btes < 0 on error/end, >0 when theres something available 
	while (!avail_bytes && !errno) {
		sched_yield();
		curr_rip = rsktl_atomic_set_rip(&lib.skts, skt_h, true, true);
		if (!curr_rip) {
			errno = EBADFD;
			ERR("Could not set RIP");
			goto exit;
		};

	 	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);
	};

	if ((AVAIL_BYTES_END == avail_bytes) ||
					(AVAIL_BYTES_ERROR == avail_bytes)) {
		rsktl_atomic_set_rip(&lib.skts, skt_h, true, false);
		if (DMA_FLUSHED(skt)) {
			rskt_close_locked(skt_h);
		}
		goto done;
	};

	if (avail_bytes > (int)max_byte_cnt) {
		avail_bytes = max_byte_cnt;
	};
	DBG("avail_bytes = %d", avail_bytes);
	first_offset = (ntohl(skt->hdr->loc_rx_rd_ptr) + 1) % skt->buf_sz;
	DBG("first_offset = 0x%X", first_offset);
	if ((avail_bytes + first_offset) < skt->buf_sz) {
		DBG("1");
		read_bytes(skt, data, avail_bytes);
	} else {
		uint32_t first_bytes = skt->buf_sz - first_offset;
		DBG("2");
		read_bytes(skt, data, first_bytes);
		read_bytes(skt, (uint8_t *)data + first_bytes, 
				avail_bytes - first_bytes);
	};

	skt->stats.rx_bytes += avail_bytes;
	skt->stats.rx_trans++;

	/* Only update remote header if bytes were read. */
	hdr_in.loc_msubh = skt->msubh;
	hdr_in.rem_msubh = skt->con_msubh;
	hdr_in.priority = 0;
	hdr_in.sync_type = rdma_sync_chk;
	if (update_remote_hdr(skt, &hdr_in)) {
		skt->hdr->loc_tx_wr_flags |= 
				htonl(RSKT_BUF_HDR_FLAG_ERROR);
	       	skt->hdr->loc_rx_rd_flags |= 
				htonl(RSKT_BUF_HDR_FLAG_ERROR);
		ERR("Failed in update_remote_hdr");
		goto fail_reading;
	};
	rsktl_atomic_set_rip(&lib.skts, skt_h, true, false);
done:
	switch(avail_bytes) {
	case AVAIL_BYTES_END: avail_bytes = 0;
			break;
	case AVAIL_BYTES_ERROR: avail_bytes = -1;
			break;
	default: break;
	};
	return avail_bytes;

fail_reading:
	rsktl_atomic_set_rip(&lib.skts, skt_h, true, false);
	if (errno == ECONNRESET) {
		WARN("Failed because the other side closed!");
	} else {
		/* Failed for another reason. Closing RSKT */
		WARN("Failed for some other reason. Closing connection");
		rskt_close_locked(skt_h);
	}
exit:
	return -1;
}; /* rskt_read() */

int rskt_close(rskt_h skt_h)
{
	int rc = EINVAL;

	if (lib_uninit()) {
		ERR("%s", strerror(errno));
		return -errno;
	}

	rc = rskt_close_locked(skt_h);
	return rc;
};

#ifdef __cplusplus
}
#endif

