/* Implementation of the RSKTD Message Processor Thread */
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
#include <sys/socket.h>
#include <errno.h>
#include <time.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>

#include "rapidio_mport_dma.h"

#include "string_util.h"
#include "librsktd.h"
#include "librdma.h"
#include "librsktd.h"
#include "librsktd_sn.h"
#include "librsktd_dmn.h"
#include "librsktd_lib.h"
#include "librsktd_lib_info.h"
#include "liblist.h"
#include "librsktd_msg_proc.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

struct librsktd_msg_proc_info mproc;

struct rsktd_req_msg *alloc_dreq(void)
{
	struct rsktd_req_msg *ret_p = NULL;

	ret_p = (struct rsktd_req_msg *)calloc(1, DMN_REQ_SZ);
	ret_p->in_use = 1;
	return ret_p;
};
	
int free_dreq(struct rsktd_req_msg *dreq)
{

	if (NULL == dreq)
		goto fail;

	free(dreq);
	return 0;
fail:
	return 1;
};
	
struct rsktd_resp_msg *alloc_dresp(void)
{
	struct rsktd_resp_msg *ret_p = NULL;

	ret_p = (struct rsktd_resp_msg *)calloc(1, DMN_RESP_SZ);
	ret_p->in_use = 1;
	return ret_p;
};
	
int free_dresp(struct rsktd_resp_msg *dresp)
{
	if (NULL == dresp)
		goto fail;

	free(dresp);
	return 0;
fail:
	return 1;
};
	

struct librskt_app_to_rsktd_msg *alloc_rx(void)
{
	struct librskt_app_to_rsktd_msg *ret_p = NULL;

	ret_p = (struct librskt_app_to_rsktd_msg *)
		calloc(1, sizeof(struct librskt_app_to_rsktd_msg));
	ret_p->in_use = 1;

	if (NULL == ret_p)
		ERR("Exhausted mproc.rxs pool! %d entries", MAX_MSG);

	return ret_p;
};
	
int free_rx(struct librskt_app_to_rsktd_msg *rx)
{
	if (NULL == rx)
		goto fail;
	free(rx);
	return 0;
fail:
	return 1;
};
	
struct librskt_rsktd_to_app_msg *alloc_tx(void)
{
	struct librskt_rsktd_to_app_msg *ret_p = NULL;

	ret_p = (struct librskt_rsktd_to_app_msg *)
		calloc(1, sizeof(struct librskt_rsktd_to_app_msg));
	return ret_p;
};
	
int free_tx(struct librskt_rsktd_to_app_msg *tx)
{
	if (NULL == tx)
		goto fail;

	free(tx);
	return 0;
fail:
	return 1;
};
	

struct librsktd_unified_msg *alloc_msg(uint32_t msg_type,
					uint32_t proc_type,
					uint32_t proc_stage)
{
	struct librsktd_unified_msg *lum = NULL;
	lum = (struct librsktd_unified_msg *)
			calloc(1, sizeof(struct librsktd_unified_msg));
	if (NULL == lum) {
		CRIT("Exhausted mproc.u_msg pool! %d entries", MAX_MSG);
		goto exit;
	};
	memset(lum, 0, sizeof(struct librsktd_unified_msg));
	lum->msg_type = msg_type;
	lum->proc_type = proc_type;
	lum->proc_stage = proc_stage;
exit:
	return lum;
};

int dealloc_msg(struct librsktd_unified_msg *u_msg)
{
	if (NULL == u_msg)
		goto fail;

	DBG("Freeing u_msg");
	if (NULL != u_msg->dreq)
		free_dreq(u_msg->dreq);
	if (NULL != u_msg->dresp)
		free_dresp(u_msg->dresp);
	if (NULL != u_msg->rx)
		free_rx(u_msg->rx);
	if (NULL != u_msg->tx)
		free_tx(u_msg->tx);
	if (NULL != u_msg->loc_ms)
		u_msg->loc_ms->state = rsktd_ms_free;
	DBG("Freed u_msg");
	free(u_msg);
	return 0;
fail:
	return 1;
};

void rsktd_areq_bind(struct librsktd_unified_msg *msg)
{
	struct librskt_bind_req *req = NULL;
	enum rskt_state stat;

	/* Check for NULL pointer */
	if (msg == NULL) {
		ERR("'msg' is NULL");
		return;
	}

	req = &msg->rx->a_rq.msg.bind;

	/* Check for NULL pointer */
	if (req == NULL) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		"&msg->rx->a_rq.msg.bind is NULL",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg));
		return;
	}

	uint32_t sn = ntohl(req->sn);

	stat = rsktd_sn_get(sn);
	if ((rskt_uninit == stat) || (rskt_closed == stat)) {
		INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s sn %d %s",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg), 
			sn, SKT_STATE_STR(stat));
		rsktd_sn_set(sn, rskt_alloced);
		msg->tx->a_rsp.err = htonl(0);
	} else {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s sn %d %s",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg),
			sn, SKT_STATE_STR(stat));
		msg->tx->a_rsp.err = htonl(EBUSY);
	}
};

int rsktd_areq_release(struct librsktd_unified_msg *msg)
{
	int i;
	bool found = false;

	struct librskt_release_req *req = &msg->rx->a_rq.msg.release;

	msg->tx->a_rsp.err = htonl(0);
	req->ms_name[MAX_MS_NAME] = '\0';
	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s MS %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg), req->ms_name);

	for (i = 0; (i < dmn.mso.num_ms) && !found; i++) {
		if (!memcmp(dmn.mso.ms[i].ms_name, req->ms_name, MAX_MS_NAME+1))
		{
			int sn = (int)ntohl(req->sn);

			found = true;

			DBG("Msg %s 0x%x Type 0x%x %s Proc %s "
				"Stage %s MS %s State %d %s "
				"MS SN %d REQ SN %d",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg), 
			req->ms_name, dmn.mso.ms[i].state,
			RSKTD_MS_STATE_TO_STR(dmn.mso.ms[i].state),
			dmn.mso.ms[i].rem_sn, sn);

			/* MS may have been freed prior to this message */
			if (rsktd_ms_free == dmn.mso.ms[i].state)
				continue;

			if (rsktd_ms_flux == dmn.mso.ms[i].state) {
				dmn.mso.ms[i].state = rsktd_ms_free;
				continue;
			};
			/* MS may have been reallocated prior to this message */
			if (sn != dmn.mso.ms[i].rem_sn)
				continue; 

			msg->tx->a_rsp.err = htonl(EBADFD);
			ERR("Msg %s 0x%x Type 0x%x %s Proc %s "
				"Stage %s MS %s State %d %s ILLEGAL "
				"MS SN %d REQ SN %d",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg), 
			req->ms_name, dmn.mso.ms[i].state,
			RSKTD_MS_STATE_TO_STR(dmn.mso.ms[i].state),
			dmn.mso.ms[i].rem_sn, sn);
		};

	}

	if (!found) {
		msg->tx->a_rsp.err = htonl(ENOENT);
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s MS %s !found",
				UMSG_W_OR_S(msg),
				UMSG_CT(msg),
				msg->msg_type,
				UMSG_TYPE_TO_STR(msg),
				UMSG_PROC_TO_STR(msg),
				UMSG_STAGE_TO_STR(msg), req->ms_name);
	}
	return 1;
};

void rsktd_areq_listen(struct librsktd_unified_msg *msg)
{
	struct librskt_listen_req *req = &msg->rx->a_rq.msg.listen;
	uint32_t sn = ntohl(req->sn);
	struct acc_skts* new_skt = NULL;

	if (rskt_alloced != rsktd_sn_get(sn)) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s sn %d"
		"rskt of sn(%d) not allocated", 
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg), sn);
		msg->tx->a_rsp.err = htonl(EBUSY);
		return;
	};
	rsktd_sn_set(sn, rskt_listening);
	msg->tx->a_rsp.err = htonl(0); 
	
	new_skt = (struct acc_skts *)calloc(1, sizeof(struct acc_skts));
	if (new_skt == NULL) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		"Failed to allocate new_skt",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg));
		return;
	}
	new_skt->app = msg->app;
	new_skt->skt_num = sn;
	new_skt->max_backlog = ntohl(req->max_bklog);
	l_init(&new_skt->conn_req);
	new_skt->acc_req = NULL;

	l_add(&lib_st.acc, sn, (void *)new_skt);
};

struct rskt_dmn_wpeer **find_wpeer_by_ct(uint32_t ct)
{
	struct rskt_dmn_wpeer **wp = NULL;
	int i;

	for (i = 0; i < MAX_PEER; i++) {
		if (dmn.wpeers[i].wpeer_alive && !dmn.wpeers[i].i_must_die &&
				(dmn.wpeers[i].ct == ct)) {
			if (wp) {
				CRIT(">=2 WPEERs with ct of 0x%x", ct);
			};
			wp = dmn.wpeers[i].self_ref;
		};
	};
	return wp;
};

void rsktd_connect_accept(struct acc_skts *acc)
{
	int i;
	int err = 0;
	struct ms_info *loc_ms_info = NULL;
	struct librsktd_unified_msg *acc_req = NULL;
	struct librskt_accept_resp *a_resp = NULL;
	struct librsktd_unified_msg *con_req = NULL;
	struct librsktd_connect_req *dreq = NULL;
	struct librsktd_connect_resp *dresp = NULL;
	struct con_skts *con = NULL;

	if ((NULL == acc->acc_req) || !l_size(&acc->conn_req)) {
		ERR("NULL parameter or list member size");
		return;
	}

	con_req = (struct librsktd_unified_msg *)l_pop_head(&acc->conn_req);
	if (con_req == NULL) {
		ERR("con_req is NULL");
		return;
	}

	acc_req = acc->acc_req; 
	acc->acc_req = NULL;
	a_resp = &acc_req->tx->a_rsp.msg.accept;
	dreq = &con_req->dreq->msg.con;
	dresp = &con_req->dresp->msg.con;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s con_req",
		UMSG_W_OR_S(con_req),
		UMSG_CT(con_req),
		con_req->msg_type,
		UMSG_TYPE_TO_STR(con_req),
		UMSG_PROC_TO_STR(con_req),
		UMSG_STAGE_TO_STR(con_req));
	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s acc_req",
		UMSG_W_OR_S(acc_req),
		UMSG_CT(acc_req),
		acc_req->msg_type,
		UMSG_TYPE_TO_STR(acc_req),
		UMSG_PROC_TO_STR(acc_req),
		UMSG_STAGE_TO_STR(acc_req));

	/* Find a free memory space on this RSKTD to rdma_connect to */
	for (i = dmn.mso.next_ms; i < dmn.mso.num_ms; i++) {
		if (dmn.mso.ms[i].valid && 
				(rsktd_ms_free == dmn.mso.ms[i].state)) {
			loc_ms_info = &dmn.mso.ms[i];
			dmn.mso.next_ms = (i + 1) % dmn.mso.num_ms;
			break;
		};
	};

	if (NULL == loc_ms_info) {
		for (i = 0; i < dmn.mso.next_ms; i++) {
			if (dmn.mso.ms[i].valid && 
					(rsktd_ms_free == dmn.mso.ms[i].state))
			{
				loc_ms_info = &dmn.mso.ms[i];
				dmn.mso.next_ms = (i + 1) % dmn.mso.num_ms;
				break;
			};
		};
	};

	if (NULL == loc_ms_info) {
		err = EAFNOSUPPORT;
		ERR("loc_ms is NULL");
		goto fail;
	};

	/* Find a free socket number for the accept */
	a_resp->new_sn = rsktd_sn_find_free();
	if (RSKTD_INVALID_SKT == a_resp->new_sn) {
		err = EADDRNOTAVAIL;
		ERR("a_resp->new_sn is an invalid socket");
		goto fail;
	};

	/* No more failures possible, so mark resources as in use */
	rsktd_sn_set(ntohl(acc_req->rx->a_rq.msg.accept.sn), rskt_listening);
	rsktd_sn_set(a_resp->new_sn, rskt_connecting);
	loc_ms_info->state = rsktd_ms_used;
	loc_ms_info->loc_sn = a_resp->new_sn;
	loc_ms_info->rem_sn = ntohl(dreq->src_sn);
	loc_ms_info->rem_ct = (*con_req->sp)->ct;
	a_resp->new_sn = htonl(a_resp->new_sn);
	a_resp->new_ct = htonl(dmn.qresp.hdid);

	/* Compose accept response first */
	a_resp->peer_sa.ct = htonl((*con_req->sp)->ct);
	a_resp->peer_sa.sn = dreq->src_sn;
	a_resp->ms_size = htonl(loc_ms_info->ms_size);
	SAFE_STRNCPY(a_resp->mso_name, dmn.mso.msoh_name,
			sizeof(a_resp->mso_name));
	SAFE_STRNCPY(a_resp->ms_name, loc_ms_info->ms_name,
			sizeof(a_resp->ms_name));

	/* Compose connect response next */
	dresp->acc_sn = a_resp->new_sn;
	dresp->dst_sn = dreq->dst_sn;
	dresp->dst_ct = dreq->dst_ct;
	dresp->dst_dmn_cm_skt = htonl((*con_req->sp)->cm_skt_num);
	dresp->msub_sz = a_resp->ms_size;
	SAFE_STRNCPY(dresp->dst_ms, loc_ms_info->ms_name,
			sizeof(dresp->dst_ms));

	/* Add connected socket to list */
	con = (struct con_skts *)calloc(1, sizeof(struct con_skts));
	con->app = acc_req->app;
	con->loc_sn = ntohl(a_resp->new_sn);
	con->loc_ms = loc_ms_info;
	con->rem_ct = (*con_req->sp)->ct;
	con->rem_sn = ntohl(dreq->src_sn);
	con->w = find_wpeer_by_ct(con->rem_ct);
	l_add(&lib_st.con, con->loc_sn, (void *)con);
fail:
	acc_req->tx->a_rsp.err = htonl(err);
	acc_req->proc_stage = RSKTD_AREQ_SEQ_ARESP;
	con_req->dresp->err = htonl(err);
	con_req->proc_stage = RSKTD_SPEER_SEQ_DRESP;

	enqueue_app_msg(acc_req);
	enqueue_speer_msg(con_req);
};

int rsktd_areq_accept(struct librsktd_unified_msg *msg)
{
	struct librskt_accept_req *req = &msg->rx->a_rq.msg.accept;
	struct librskt_resp *resp = &msg->tx->a_rsp;
	uint32_t sn = ntohl(req->sn);

	struct acc_skts *acc_skt = NULL;
	struct l_item_t *li = NULL;
	uint32_t send_resp_now = 1;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));

	if (rskt_listening != rsktd_sn_get(sn)) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s sn %d"
		" Socket not listening",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg), sn);

		resp->err = htonl(ECONNREFUSED);
		return send_resp_now;
	};

	acc_skt = (struct acc_skts *)l_find(&lib_st.acc, sn, &li);
	if ((NULL == acc_skt) || (NULL != acc_skt->acc_req) ||
		(acc_skt->app != msg->app)) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s sn %d"
		"Not found or invalid acc_skt",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg), sn);
		resp->err = htonl(ECONNREFUSED);
		return send_resp_now;
	};

	rsktd_sn_set(sn, rskt_accepting);
	acc_skt->acc_req = msg;

	/* Save request messages */

	send_resp_now = 0;
	if (l_size(&acc_skt->conn_req))
		rsktd_connect_accept(acc_skt);

	return send_resp_now;
};

void rsktd_areq_hello(struct librsktd_unified_msg *msg)
{
	struct librskt_hello_req *req = &msg->rx->a_rq.msg.hello;
	struct librskt_resp *resp = &msg->tx->a_rsp;
	char app_name[16] = {0};

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));

	memcpy((*msg->app)->app_name, req->app_name, MAX_APP_NAME);
	(*msg->app)->proc_num = ntohl(req->proc_num);
	resp->err = 0;
	resp->msg.hello.ct = htonl(dmn.qresp.hdid);

        memset(app_name, 0, 16);
        snprintf(app_name, 15, "AppRx_%8s", (*msg->app)->app_name);
        pthread_setname_np((*msg->app)->thread, app_name);
	resp->err = 0;
};

/* Response message initialized when request received.
 * Process request, fill out response, and send.
 */
void msg_q_handle_areq(struct librsktd_unified_msg *msg)
{
	uint32_t send_resp_now = 1;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));

	switch (msg->proc_stage) {
	case RSKTD_AREQ_SEQ_AREQ:
		switch (msg->msg_type) {
		case LIBRSKTD_BIND:
				rsktd_areq_bind(msg);
				break;
		case LIBRSKTD_LISTEN:
				rsktd_areq_listen(msg);
				break;
		case LIBRSKTD_ACCEPT:
				send_resp_now = rsktd_areq_accept(msg);
				break;
		case LIBRSKTD_HELLO:
				rsktd_areq_hello(msg);
				break;
		case LIBRSKTD_RELEASE:
				send_resp_now = rsktd_areq_release(msg);
				break;
		default:
			ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
			"AREQ Rx UNKNOWN Msg Type %d 0x%x",
				UMSG_W_OR_S(msg),
				UMSG_CT(msg),
				msg->msg_type,
				UMSG_TYPE_TO_STR(msg),
				UMSG_PROC_TO_STR(msg),
				UMSG_STAGE_TO_STR(msg),
				htonl(msg->msg_type),
				htonl(msg->msg_type));
			msg->tx->msg_type |= htonl(LIBRSKTD_FAIL);
		};
		break;
	default:
		ERR("AREQ Stage: %d", msg->proc_stage);
		msg->tx->msg_type |= htonl(LIBRSKTD_FAIL);
	};

	if (send_resp_now) {
		msg->proc_stage = RSKTD_AREQ_SEQ_ARESP;
		enqueue_app_msg(msg);
	};
};

int rsktd_a2w_connect_req(struct librsktd_unified_msg *r)
{
	struct librskt_connect_req *a_rq = &r->rx->a_rq.msg.conn;
	struct librskt_resp *a_rsp = &r->tx->a_rsp;
	struct  librsktd_connect_req *d_con = &r->dreq->msg.con;
	uint32_t sn = ntohl(a_rq->sn);
	ct_t ct = ntohl(a_rq->ct);
	uint32_t new_sn = rsktd_sn_find_free();
	int i, err = 0;
	struct rskt_dmn_wpeer *w = NULL;
	
	/* Initialize application response message */
	r->dreq->msg_type = htonl(RSKTD_CONNECT_REQ);
	r->dresp->msg_type = r->dreq->msg_type | htonl(RSKTD_RESP_FLAG);
	a_rsp->err = 0;
	a_rsp->msg.conn.new_sn = 0;
	memset(a_rsp->msg.conn.mso, 0, sizeof(a_rsp->msg.conn.mso));
	memset(a_rsp->msg.conn.ms, 0, sizeof(a_rsp->msg.conn.ms));
	a_rsp->msg.conn.msub_sz = 0;
	memset(a_rsp->msg.conn.rem_ms, 0, sizeof(a_rsp->msg.conn.rem_ms));

	/* If can't find peer by component tag, fail */
	r->wp = find_wpeer_by_ct(ct);
	if (NULL == r->wp) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		" Could not find wpeer with CT(%d) in wpeers",
			UMSG_W_OR_S(r),
			UMSG_CT(r),
			r->msg_type,
			UMSG_TYPE_TO_STR(r),
			UMSG_PROC_TO_STR(r),
			UMSG_STAGE_TO_STR(r),
			htonl(r->msg_type),
			htonl(r->msg_type), ct);
		err = ENODEV;
		goto fail;
	};

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r));

	w = *(r->wp);

	if ((NULL == w) || (w->i_must_die)) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		"Either r->wp is NULL or w->i_must_die is true",
			UMSG_W_OR_S(r),
			UMSG_CT(r),
			r->msg_type,
			UMSG_TYPE_TO_STR(r),
			UMSG_PROC_TO_STR(r),
			UMSG_STAGE_TO_STR(r),
			htonl(r->msg_type),
			htonl(r->msg_type));
		err = ENETDOWN;
		goto fail;
	};

	/* If can't find a free memory space for this request, fail */
	for (i = 0; i < dmn.mso.num_ms; i++) {
		if (dmn.mso.ms[i].valid && !dmn.mso.ms[i].state) {
			r->loc_ms = &dmn.mso.ms[i];
			r->loc_ms->state = rsktd_ms_rsvd;
			break;
		};
	};

	if (NULL == r->loc_ms) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		"No available memory spaces for this request!",
			UMSG_W_OR_S(r),
			UMSG_CT(r),
			r->msg_type,
			UMSG_TYPE_TO_STR(r),
			UMSG_PROC_TO_STR(r),
			UMSG_STAGE_TO_STR(r),
			htonl(r->msg_type),
			htonl(r->msg_type));
		err = ENOMEM;
		goto fail;
	};

	/* If there aren't any free socket numbers available, fail */
	if (RSKTD_INVALID_SKT == new_sn) {
		ERR("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		"No free socket numbers available",
			UMSG_W_OR_S(r),
			UMSG_CT(r),
			r->msg_type,
			UMSG_TYPE_TO_STR(r),
			UMSG_PROC_TO_STR(r),
			UMSG_STAGE_TO_STR(r),
			htonl(r->msg_type),
			htonl(r->msg_type));
		err = EADDRNOTAVAIL;
		goto fail;
	};

	/* Can't fail now, so send msg to peer RSKTD */
	r->dreq->msg_type = htonl(RSKTD_CONNECT_REQ);
	r->dreq->msg_seq = 0;
	d_con->dst_sn = htonl(sn);
	d_con->dst_ct = htonl(ct);
	d_con->src_sn = htonl(new_sn);
	SAFE_STRNCPY(d_con->src_mso, dmn.mso.msoh_name, sizeof(d_con->src_mso));
	SAFE_STRNCPY(d_con->src_ms, r->loc_ms->ms_name, sizeof(d_con->src_ms));
	d_con->src_msub_o = 0;
	d_con->src_msub_s = htonl(r->loc_ms->ms_size);

	rsktd_sn_set(new_sn, rskt_connecting);
	
	r->proc_stage = RSKTD_A2W_SEQ_DREQ;

	/* Message contents */
	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s"
		"dst_sn = %d, dst_ct = 0x%X, src_sn = %d"
		"src_mso = %s, src_ms = %s", 
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r),
		sn, ct, new_sn,
		d_con->src_mso, d_con->src_ms);
fail:
	r->tx->a_rsp.err = htonl(err);
	return err;
};

void rsktd_a2w_connect_resp(struct librsktd_unified_msg *r)
{
	struct librskt_connect_resp *a_rsp = &r->tx->a_rsp.msg.conn;
	struct librsktd_connect_req *d_req = &r->dreq->msg.con;
	struct librsktd_connect_resp *d_resp = &r->dresp->msg.con;
	struct con_skts *con = NULL;

	r->tx->a_rsp.err = r->dresp->err;

	if (r->dresp->err)
		goto fail;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r));

	/* Pass con_resp info into a_rsp */
	a_rsp->new_sn = d_req->src_sn;
	a_rsp->new_ct = htonl(dmn.qresp.hdid);
	SAFE_STRNCPY(a_rsp->mso, d_req->src_mso, sizeof(a_rsp->mso));
	SAFE_STRNCPY(a_rsp->ms, d_req->src_ms, sizeof(a_rsp->ms));
	SAFE_STRNCPY(a_rsp->rem_ms, d_resp->dst_ms, sizeof(a_rsp->rem_ms));
	a_rsp->rem_sn = d_resp->acc_sn;
	a_rsp->msub_sz = d_resp->msub_sz;
	r->loc_ms->state = rsktd_ms_used;
	r->tx->a_rsp.err = htonl(0);

	/* Add connected socket to list */
	con = (struct con_skts *)calloc(1, sizeof(struct con_skts));
	con->app = r->app;
	con->loc_sn = ntohl(d_req->src_sn);
	con->loc_ms = r->loc_ms;
	con->rem_ct = (*r->wp)->ct;
	con->rem_sn = ntohl(d_resp->acc_sn);
	con->w = find_wpeer_by_ct(con->rem_ct);
	l_add(&lib_st.con, con->loc_sn, (void *)con);

	r->loc_ms = NULL; /* Prevent freeing MS by dealloc_ms */
	return;
fail:
	if (NULL != r->loc_ms)
		r->loc_ms->state = rsktd_ms_free;
	if (RSKTD_INVALID_SKT != ntohl(d_req->src_sn))
		rsktd_sn_set(htonl(d_req->src_sn), rskt_uninit);
};

void terminate_accept_and_conn_reqs(uint32_t sn)
{
	struct l_item_t *li = NULL;
	struct acc_skts *acc = (struct acc_skts *)l_find(&lib_st.acc, sn, &li);
	struct librsktd_unified_msg *con_req;

	if (NULL == acc)
		return;

	while (l_size(&acc->conn_req)) {
		con_req = (struct librsktd_unified_msg *)
					l_pop_head(&acc->conn_req);
		con_req->dresp->err = htonl(ECONNRESET);
		enqueue_speer_msg(con_req);
	};

	if (NULL != acc->acc_req) {
		acc->acc_req->tx->a_rsp.err = htonl(ECONNRESET);
		acc->acc_req->proc_stage = RSKTD_AREQ_SEQ_ARESP;
		enqueue_app_msg(acc->acc_req);
		acc->acc_req = NULL;
	};
	
	l_remove(&lib_st.acc, li);
};

uint32_t terminate_connected_socket(struct librsktd_unified_msg *msg, 
					int sn)
{
	uint32_t send_resp_now = 1;
	struct l_item_t *li = NULL;
	struct con_skts *con = (struct con_skts *)l_find(&lib_st.con, sn, &li);

	if (NULL == con)
		goto done;

	if ((NULL == con->app) || (NULL == con->w))
		goto done;

	if ((NULL == *con->app) || (NULL == *con->w))
		goto done;

	/* There are two flavous of this routine:
	 * s2a: Request that application close the local socket
	 * a2w: Request that remote peer close the local socket
	 */
	send_resp_now = 0;
	msg->closing_skt = con;
	
	if (NULL != con->loc_ms) {
		DBG("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s MS %s "
		"State %d %s REQ SN %d MS RSN %d LSN %d CON RSN %d LSN %d",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg), 
		con->loc_ms->ms_name, con->loc_ms->state,
		RSKTD_MS_STATE_TO_STR(con->loc_ms->state),
		sn, con->loc_ms->rem_sn, con->loc_ms->loc_sn,
		con->rem_sn, con->loc_sn);
	};

	if (RSKTD_PROC_S2A == msg->proc_type) {
		/* If the MS is still there, set it to free as
		* the daemon originating the close action   
		* stopped accessing this sm before sending the 
		* close request.
		*/
		if (NULL != con->loc_ms) {
			if (((rsktd_ms_used == con->loc_ms->state) ||
				(rsktd_ms_flux == con->loc_ms->state)) &&
				(con->loc_ms->rem_sn == sn))
				con->loc_ms->state = rsktd_ms_free;
		}
		/* Request application close the local socket */
		msg->tx->msg_type = htonl(LIBRSKT_CLOSE_CMD);
		msg->rx->msg_type = msg->tx->msg_type | htonl(LIBRSKTD_RESP);
		msg->tx->rq_a.msg.clos.sn = htonl(con->loc_sn);
		msg->proc_stage = RSKTD_S2A_SEQ_AREQ;
		msg->app = con->app;
	} else {
		if (NULL != con->loc_ms) {
			/* If remote daemon has already said it  memory space.
			* Mark it as "in flux" so that the local process can
			* kill it.
			*/
			if (con->loc_ms->loc_sn == sn) {
				if (rsktd_ms_used == con->loc_ms->state)
					con->loc_ms->state = rsktd_ms_flux;
			}
		};
		if (NULL != con->loc_ms) {
			if (rsktd_ms_used == con->loc_ms->state)
				con->loc_ms->state = rsktd_ms_flux;
		}
		msg->dreq->msg_type = htonl(RSKTD_CLOSE_REQ);
		msg->dresp->msg_type = msg->dreq->msg_type | 
					htonl(RSKTD_RESP_FLAG);
		msg->dreq->msg.clos.rem_sn = htonl(con->loc_sn);
		msg->dreq->msg.clos.loc_sn = htonl(con->rem_sn);
		msg->dreq->msg.clos.force = htonl(1);
		msg->proc_stage = RSKTD_A2W_SEQ_DREQ;
		msg->wp = con->w;
	};
done:
	return send_resp_now;
};

uint32_t rsktd_a2w_close_req(struct librsktd_unified_msg *r)
{
	uint32_t sn = ntohl(r->rx->a_rq.msg.close.sn);
	uint32_t send_resp_now = 1;

	r->tx->a_rsp.err = htonl(0);

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s SN %d ST %d", 
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r),
		sn,
		rsktd_sn_get(sn));

	switch (rsktd_sn_get(sn)) {
	case rskt_uninit:
	case rskt_noconn:
	case rskt_shutting_down:
	case rskt_closing:
	case rskt_shut_down:
	case rskt_closed:
	default:
		/* Socket is not in use or is shutting down */
		break;

	case rskt_alloced:
	case rskt_bound:
		/* Clear up socket usage */
		rsktd_sn_set(sn, rskt_uninit);
		break;

	case rskt_listening:
	case rskt_accepting:
		/* Listening, clean up any pending connection requests */
		rsktd_sn_set(sn, rskt_closing);
		terminate_accept_and_conn_reqs(sn);
		DBG("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s SN %d ST %d", 
			UMSG_W_OR_S(r),
			UMSG_CT(r),
			r->msg_type,
			UMSG_TYPE_TO_STR(r),
			UMSG_PROC_TO_STR(r),
			UMSG_STAGE_TO_STR(r),
			sn,
			rsktd_sn_get(sn));
		rsktd_sn_set(sn, rskt_closed);
		break;

	case rskt_connecting:
	case rskt_connected:
		/* Tell the wpeer to close the socket */
		rsktd_sn_set(sn, rskt_closing);
		r->dreq->msg_type = htonl(RSKTD_CLOSE_REQ);
		r->dresp->msg_type = r->dreq->msg_type | htonl(RSKTD_RESP_FLAG);
		send_resp_now = terminate_connected_socket(r, sn);
		break;
	};

	return send_resp_now;
};

/* NOTE: A2W_HELLO_RESP is a special case, as there is no application 
 * associated with this request: HELLO requests are originated by the
 * RSKTD openning a worker peer connection.
 *
 * Deallocate the message when processing is complete.
 */
void rsktd_a2w_hello_resp(struct librsktd_unified_msg *r)
{
	struct rskt_dmn_wpeer *w = *r->wp;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r));

	if ((NULL != w) && (NULL != r->dresp)) {
		char name[16];
		w->peer_pid = ntohl(r->dresp->msg.hello.peer_pid);
		w->wpeer_alive = 1;
		memset(name, 0, 16);
		snprintf(name, 15, "WPEER_RX%d", w->ct);
		pthread_setname_np(w->w_rx, name);
	};

	dealloc_msg(r);
};

void rsktd_a2w_close_resp(struct librsktd_unified_msg *r)
{
	struct rsktd_req_msg *dreq = r->dreq;
	uint32_t sn = ntohl(dreq->msg.clos.rem_sn);
	struct l_item_t *li = NULL;
	struct con_skts *con = (struct con_skts *)l_find(&lib_st.con, sn, &li);

	/* App has confirmed that socket is closed. */
	/* Free up local resources associated with the socket */
	/* NOTE: MS cannot be reused until application releases it in RDMAD */

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s SN %d ST %d", 
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r),
		sn,
		rsktd_sn_get(sn));

	r->closing_skt->loc_ms->state = rsktd_ms_flux;
	rsktd_sn_set(sn, rskt_closed);

	if (NULL != con)
		l_remove(&lib_st.con, li);

	/* Prepare response */
	r->tx->a_rsp.err = htonl(0);
};

void msg_q_handle_a2w(struct librsktd_unified_msg *r)
{
	int send_app_resp = 1;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r));

	switch (r->proc_stage) {
	case RSKTD_A2W_SEQ_AREQ:
		r->dreq = (struct rsktd_req_msg *)calloc(1, DMN_REQ_SZ);
		r->dresp = (struct rsktd_resp_msg *)calloc(1, DMN_RESP_SZ);
		r->dresp->err = 0;

		switch(r->msg_type) {
		case LIBRSKTD_CONN: send_app_resp = 
				    rsktd_a2w_connect_req(r);
				    break;
		case LIBRSKTD_CLOSE: 
				    send_app_resp = rsktd_a2w_close_req(r);
				    break;
		default:
			ERR("A2W UNKNOWN Msg Type: %d", r->msg_type);
			r->tx->msg_type |= htonl(LIBRSKTD_FAIL);
		};
		memcpy((void *)&r->dresp->req, (void *)&r->dreq->msg,
				sizeof(union librsktd_req));
		break;
	case RSKTD_A2W_SEQ_DRESP:
		switch(r->msg_type) {
		case RSKTD_HELLO_REQ: rsktd_a2w_hello_resp(r);
				     return;
				    break;
		case LIBRSKTD_CONN: rsktd_a2w_connect_resp(r);
				    break;
		case LIBRSKTD_CLOSE: rsktd_a2w_close_resp(r);
				    break;
		default:
			ERR("A2W Msg Type: %d", r->msg_type);
			r->dresp->msg_type |= htonl(LIBRSKTD_FAIL);
		};
		break;
	default:
		ERR("A2W Stage: %d", r->proc_stage);
		send_app_resp = 1;
	};
	/* After message processing, must send response message to app
	 * or request message to wpeer.
	 */
	if (send_app_resp) {
		r->proc_stage = RSKTD_A2W_SEQ_ARESP;
		enqueue_app_msg(r);
	} else {
		r->proc_stage = RSKTD_A2W_SEQ_DREQ;
		enqueue_wpeer_msg(r);
	};
};

void rsktd_sreq_hello_req(struct librsktd_unified_msg *r)
{
	struct rsktd_req_msg *dreq = r->dreq;
	struct rsktd_resp_msg *dresp = r->dresp;
	struct rskt_dmn_speer *sp = *r->sp;
        char sp_name[16] = {0};

	if (NULL == sp) {
		dresp->err = htonl(ECONNREFUSED);
		return;
	};

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r));

	sp->ct = ntohl(dreq->msg.hello.ct);
	sp->cm_skt_num = ntohl(dreq->msg.hello.cm_skt);
	sp->cm_mp = ntohl(dreq->msg.hello.cm_mp);

	sp->got_hello = 1;
	sp->alive = 1;

        memset(sp_name, 0, 16);
        snprintf(sp_name, 15, SPEER_THRD_NM_FMT, sp->ct);
        pthread_setname_np(sp->s_rx, sp_name);

	dresp->msg.hello.peer_pid = htonl(getpid());

	DBG("RSKTD HELLO SPEER %d Received", sp->ct);
};

uint32_t rsktd_sreq_connect_req(struct librsktd_unified_msg *r)
{
	uint32_t send_resp_msg = 1;
	enum rskt_state st;
	struct acc_skts *acc = NULL;
	struct l_item_t *li = NULL;
	int l_sz = 0;
	struct rsktd_req_msg *dreq = r->dreq;
	struct rsktd_resp_msg *dresp = r->dresp;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r));

	dresp->msg.con.acc_sn = 0;
        dresp->msg.con.dst_sn = htonl(dreq->msg.con.dst_sn);
        dresp->msg.con.dst_ct = htonl(dreq->msg.con.dst_ct);
        dresp->msg.con.dst_dmn_cm_skt = htonl((*r->sp)->cm_skt_num);
	memset(dresp->msg.con.dst_ms, 0, sizeof(dresp->msg.con.dst_ms));

	st = rsktd_sn_get(ntohl(dreq->msg.con.dst_sn));
	if ((rskt_listening == st) || (rskt_accepting == st))
		acc = (struct acc_skts *)l_find(&lib_st.acc, 
				ntohl(dreq->msg.con.dst_sn), &li);

	if (NULL == acc) {
        	dresp->err = htonl(ECONNREFUSED);
		goto exit;
	};

	l_sz = l_size(&acc->conn_req);
	if ((l_sz + 1) > acc->max_backlog) {
        	dresp->err = htonl(ECONNREFUSED);
		goto exit;
	}

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s\tAdded to acc.creq for app %s skt %d",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r),
		(*acc->app)->app_name,
		acc->skt_num);
	l_push_tail(&acc->conn_req, r);

	send_resp_msg = 0;
	if (NULL != acc->acc_req)
		rsktd_connect_accept(acc);

exit:
	return send_resp_msg;
};

void msg_q_handle_sreq(struct librsktd_unified_msg *msg)
{
	uint32_t send_resp_now = 1;
	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));

	switch (msg->proc_stage) {
	case RSKTD_SPEER_SEQ_DREQ:
		switch(msg->msg_type) {
		case RSKTD_HELLO_REQ:
			rsktd_sreq_hello_req(msg);
			break;
		case RSKTD_CONNECT_REQ:
			send_resp_now = rsktd_sreq_connect_req(msg);
			break;
		default:
			ERR("SREQ Msg_type: %d", msg->msg_type);
			break;
		};
		break;
	default:
		ERR("SREQ Stage: %d", msg->proc_stage);
	};
	if (send_resp_now)
		enqueue_speer_msg(msg);
};

uint32_t rsktd_s2a_close_req(struct librsktd_unified_msg *r)
{
	uint32_t send_resp_now = 1;
	struct rsktd_req_msg *dreq = r->dreq;
	struct rsktd_resp_msg *dresp = r->dresp;
	uint32_t sn = ntohl(dreq->msg.clos.loc_sn);

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s SN %d ST %d",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r),
		sn,
		rsktd_sn_get(sn));

	dresp->err = 0;

	switch (rsktd_sn_get(sn)) {
	case rskt_uninit:
	case rskt_noconn:
	case rskt_shutting_down:
	case rskt_closing:
	case rskt_shut_down:
	case rskt_closed:
	default:
		/* Socket is not in use or is shutting down */
		r->proc_stage = RSKTD_S2A_SEQ_ARESP;
		break;

	case rskt_alloced:
	case rskt_bound:
		rsktd_sn_set(sn, rskt_uninit);
		break;

	case rskt_listening:
	case rskt_accepting:
		/* Remote side not allowed to mess with this sides 
		* listening/accepting sockets... */
		dresp->err = EPERM;
		break;

	case rskt_connecting:
	case rskt_connected:
		/* Tell the app to close the socket */
		rsktd_sn_set(sn, rskt_closing);
		send_resp_now = terminate_connected_socket(r, sn);
		break;
	};
	return send_resp_now;
};

void rsktd_s2a_close_resp(struct librsktd_unified_msg *r)
{
	struct rsktd_req_msg *dreq = r->dreq;
	uint32_t sn = ntohl(dreq->msg.clos.loc_sn);
	struct l_item_t *li = NULL;
	struct con_skts *con = (struct con_skts *)l_find(&lib_st.con, sn, &li);

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s SN %d ST %d",
		UMSG_W_OR_S(r),
		UMSG_CT(r),
		r->msg_type,
		UMSG_TYPE_TO_STR(r),
		UMSG_PROC_TO_STR(r),
		UMSG_STAGE_TO_STR(r),
		sn,
		rsktd_sn_get(sn));

	/* App has confirmed that socket is closed. */
	/* App has also confirmed that the memory space is freed in RDMAD */
	/* Other side has confirmed that it will not write to this memory */
	/* Free up local resources associated with the socket */

	r->closing_skt->loc_ms->state = rsktd_ms_free;
	rsktd_sn_set(sn, rskt_closed);

	/* And remove the connected socket from the queues */
	if (NULL != con)
		l_remove(&lib_st.con, li);

	/* Prepare response */
	r->dresp->err = 0;
	r->dresp->msg.clos.status = htonl((uint32_t)rskt_closed);
};

void msg_q_handle_s2a(struct librsktd_unified_msg *msg)
{
	uint32_t send_resp_now = 1;

	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));

	switch (msg->proc_stage) {
	case RSKTD_S2A_SEQ_DREQ:
		msg->rx = (struct librskt_app_to_rsktd_msg *)calloc(1, A2RSKTD_SZ);
		msg->tx = (struct librskt_rsktd_to_app_msg *)calloc(1, RSKTD2A_SZ);
		msg->dresp->err = 0; 
		switch(msg->msg_type) {
		case RSKTD_CLOSE_REQ:
			send_resp_now = rsktd_s2a_close_req(msg);
			break;
		default:
			ERR("S2A Req Msg_type: %d", msg->msg_type);
			break;
		};
		msg->dresp->msg_type = msg->dreq->msg_type |
				htonl(RSKTD_RESP_FLAG);
		break;
	case RSKTD_S2A_SEQ_ARESP:
		switch(msg->msg_type) {
		case RSKTD_CLOSE_REQ:
			rsktd_s2a_close_resp(msg);
			break;
		default:
			ERR("S2A Msg_type: %d", msg->msg_type);
			break;
		};
		break;
	default:
		ERR("S2A Stage: %d", msg->proc_stage);
	};

	if (send_resp_now)
		enqueue_speer_msg(msg);
	else
		enqueue_app_msg(msg);
};

void enqueue_mproc_msg(struct librsktd_unified_msg *msg) 
{
	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));

	sem_wait(&mproc.msg_q_mutex);
	l_push_tail(&mproc.msg_q, msg);
	sem_post(&mproc.msg_q_mutex);
	sem_post(&mproc.msg_q_cnt);
};

void *msg_q_loop(void *unused)
{
	struct librsktd_unified_msg *msg = NULL;
	char my_name[16] = {0};

	DBG("ENTER");
	mproc.msg_proc_alive = 1;

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "RSKTD_MSG_PROC");
        pthread_setname_np(mproc.msg_q_thread, my_name);

	sem_post(&mproc.msg_q_started);

	while (!dmn.all_must_die) {
		sem_wait(&mproc.msg_q_cnt);
		if ((dmn.all_must_die) || (!mproc.msg_proc_alive)) {
			INFO("all_must_die or !msg_proc_alive!");
			break;
		}
		sem_wait(&mproc.msg_q_mutex);
		msg = (struct librsktd_unified_msg *)l_pop_head(&mproc.msg_q);
		sem_post(&mproc.msg_q_mutex);

		if (dmn.all_must_die || (!mproc.msg_proc_alive)
				|| (NULL == msg)) {
			WARN("msg == NULL. Exiting loop");
			break;
		}

		INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg));

		switch (msg->proc_type) {
		case RSKTD_PROC_AREQ:
					INFO("RSKTD_PROC_AREQ");
					msg_q_handle_areq(msg);
					break;
		case RSKTD_PROC_A2W:
					INFO("RSKTD_PROC_A2W");
					msg_q_handle_a2w(msg);
					break;
		case RSKTD_PROC_SREQ:
					INFO("RSKTD_PROC_SREQ");
					msg_q_handle_sreq(msg);
					break;

		case RSKTD_PROC_S2A:
					INFO("RSKTD_PROC_S2A");
					msg_q_handle_s2a(msg);
					break;
		default: 
			ERR("MSG_Q_LOOP: Unknown proc type %d",
					msg->proc_type);
		}
	};
	mproc.msg_proc_alive = 0;
	dmn.all_must_die = 1;
	sem_post(&dmn.graceful_exit);
	pthread_exit(0);
	return unused;
};

int start_msg_proc_q_thread(void)
{
	int rc;

	DBG("ENTER");
	rsktd_sn_init(RSKTD_MAX_SKT_NUM);
	sem_init(&mproc.msg_q_mutex, 0, 1);
	sem_init(&mproc.msg_q_cnt, 0, 0);
	sem_init(&mproc.msg_q_started, 0, 0);
	l_init(&mproc.msg_q);

	memset(&mproc.u_msg, 0, sizeof(mproc.u_msg));
	sem_init(&mproc.u_msg_mtx, 0, 1);
	memset(&mproc.dreqs, 0, sizeof(mproc.dreqs));
	sem_init(&mproc.dreqs_mtx, 0, 1);
	memset(&mproc.dresps, 0, sizeof(mproc.dresps));
	sem_init(&mproc.dresps_mtx, 0, 1);
	memset(&mproc.rxs, 0, sizeof(mproc.rxs));
	sem_init(&mproc.rxs_mtx, 0, 1);
	memset(&mproc.txs, 0, sizeof(mproc.txs));
	sem_init(&mproc.txs_mtx, 0, 1);

	DBG("Creating msg_q_loop");
        rc = pthread_create(&mproc.msg_q_thread, NULL, msg_q_loop, NULL);

	if (!rc)
		sem_wait(&mproc.msg_q_started);
	DBG("EXIT");
	return rc;
};

void halt_msg_proc_q_thread(void)
{
	mproc.msg_proc_alive = 0;
	dmn.all_must_die = 1;
	sem_post(&mproc.msg_q_cnt);
	sem_post(&mproc.msg_q_mutex);
	pthread_join(mproc.msg_q_thread, NULL);
}
	
#ifdef __cplusplus
}
#endif
