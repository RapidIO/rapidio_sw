/* Management of Worker RSKTD connections. */
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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include <rapidio_mport_sock.h>

#include "libcli.h"
#include "librskt_private.h"
#include "librdma.h"
#include "librsktd_sn.h"
#include "librsktd.h"
#include "librsktd_private.h"
#include "librsktd_dmn_info.h"
#include "librsktd_lib_info.h"
#include "librsktd_msg_proc.h"
#include "libfmdd.h"
#include "librsktd_fm.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIO_MPORT_DEV_PATH "/dev/rio_mport"

struct rskt_dmn_wpeer *alloc_wpeer(uint32_t ct, uint32_t cm_skt)
{ 
	struct rskt_dmn_wpeer *w = (struct rskt_dmn_wpeer *)
				malloc(sizeof(struct rskt_dmn_wpeer));;
	w->self_ref = (struct rskt_dmn_wpeer **)malloc(sizeof(void *));
	*w->self_ref = w;

	w->ct = ct;
	w->cm_skt = cm_skt;

	w->wpeer_alive = 0;
	w->i_must_die = 0;
	w->tx_buff_used = 0;
	w->tx_rc = 0;
	w->tx_buff = NULL;
	w->rx_buff_used = 0;
	w->rx_buff = NULL;

	sem_init(&w->started, 0, 0);
	sem_init(&w->w_rsp_mutex, 0, 1);
	l_init(&w->w_rsp);
	w->w_seq_num = 0;
	w->peer_pid = 0;
	
	return w;
};

void close_wpeer(struct rskt_dmn_wpeer *wpeer);
void cleanup_wpeer(struct rskt_dmn_wpeer *wpeer);

void *wpeer_rx_loop(void *p_i)
{
	struct rskt_dmn_wpeer *w = (struct rskt_dmn_wpeer *)p_i;

	DBG("Waiting on dmn.wpeers_mtx\n");
	sem_wait(&dmn.wpeers_mtx);
	HIGH("Adding ct(%d) to dmn.wpeers\n", w->ct);
	w->wp_li = l_add(&dmn.wpeers, w->ct, w->self_ref);
	sem_post(&dmn.wpeers_mtx);

	w->i_must_die = 0;
	sem_post(&w->started);
	DBG("started\n");
	while (!w->i_must_die) {
		struct librsktd_unified_msg *msg;
		uint32_t seq_num;
		struct l_item_t *li;

		w->rx_buff_used = 1;
                do {
			w->i_must_die = 0;
			w->i_must_die = riomp_sock_receive(w->cm_skt_h, 
				&w->rx_buff, DMN_RESP_SZ, 0);
		} while ((w->i_must_die) && !dmn.all_must_die && 
		((EINTR == errno) || (EAGAIN == errno) || (ETIME == errno)));

		if (w->i_must_die || dmn.all_must_die) {
			DBG("Either i_must_die or all_must_die\n");
			break;
		}

		seq_num = ntohl(w->resp->msg_seq);

		sem_wait(&w->w_rsp_mutex);
		msg = (struct librsktd_unified_msg *)
			l_find(&w->w_rsp, seq_num, &li);
		sem_post(&w->w_rsp_mutex);

		if (NULL == msg) {
			DBG("msg is NULL, continuing\n");
			continue;
		}

		msg->proc_stage = RSKTD_A2W_SEQ_DRESP;
		memcpy((void *)&msg->dresp->msg, (void *)&w->resp->msg, 
				sizeof(union librsktd_resp));
		msg->dresp->err = w->resp->err; 
		
		enqueue_mproc_msg(msg);
	};

	/* Stop others from using the wpeer */
	DBG("Call cleanup_wpeer\n");
	cleanup_wpeer(w);

	pthread_exit(NULL);
};

int init_wpeer(struct rskt_dmn_wpeer **wp, uint32_t ct, uint32_t cm_skt)
{
	int rc;
	struct rskt_dmn_wpeer *w;
	int conn_rc;

	w = alloc_wpeer(ct, cm_skt);
	*wp = w;

	DBG("ENTER\n");
	do {
		sem_wait(&dmn.mb_mtx);
		rc = riomp_sock_socket(dmn.mb, &w->cm_skt_h);
		DBG("dmn.mb created\n");
		sem_post(&dmn.mb_mtx);

        	conn_rc = riomp_sock_connect(w->cm_skt_h, ct, 0, cm_skt);

                if (!conn_rc) {
                	INFO("Connected to ct(%d)\n", ct);
                        break;
                }

                rc = riomp_sock_close(&w->cm_skt_h);
                if (rc) {
                        ERR("riomp_sock_close ERR %d\n", rc);
                };
	} while (conn_rc && ((EINTR == errno) || (ETIME == errno)));

        if (conn_rc == EADDRINUSE) {
                CRIT("init_wpeer %d: Requested channel %d in use...\n",
			ct, cm_skt);
        } else {
		if (conn_rc) {
               		CRIT("init_wpeer %d connect %d error: %d\n",
				ct, cm_skt, conn_rc);
        	}
        }
	if (conn_rc)
		goto exit;

        rc = riomp_sock_request_send_buffer(w->cm_skt_h, &w->tx_buff);
        if (rc) {
               	CRIT("init_wpeer %d: req_buffer: %d\n", ct, rc);
		goto exit;
        }

	w->rx_buff = malloc(RSKTD_CM_MSG_SIZE);

	DBG("Creating wpeer_rx_loop\n");
        rc = pthread_create(&w->w_rx, NULL, wpeer_rx_loop, (void*)w);
	if (!rc) {
		DBG("Waiting for wpeer_rx_loop() to start\n");
		sem_wait(&w->started);
		DBG("wpeer_rx_loop started\n");
	}
	return 0;
exit:
	free(w->self_ref);
	free(w);
	return rc;
};

void send_hello_to_wpeer(struct rskt_dmn_wpeer *w)
{
	struct librsktd_unified_msg *msg = (struct librsktd_unified_msg *)
		      malloc(sizeof(struct librsktd_unified_msg));

	DBG("Sending hello to wpeer\n");
	msg = alloc_msg(RSKTD_HELLO_REQ, RSKTD_PROC_A2W, RSKTD_A2W_SEQ_DREQ);
	msg->wp = w->self_ref;
	msg->dreq = (struct rsktd_req_msg *)malloc(DMN_REQ_SZ);
	msg->dresp = (struct rsktd_resp_msg *)malloc(DMN_RESP_SZ);

	msg->dreq->msg_type = htonl(RSKTD_HELLO_REQ);
	msg->dreq->msg.hello.ct = htonl(dmn.qresp.hdid);
	msg->dreq->msg.hello.cm_skt = htonl(dmn.cm_skt);
	msg->dreq->msg.hello.cm_mp = htonl(dmn.mpnum);

	msg->dresp->msg_type = msg->dreq->msg_type | htonl(RSKTD_RESP_FLAG);
	memcpy(&msg->dresp->req, &msg->dreq->msg, sizeof(union librsktd_req));
	enqueue_wpeer_msg(msg);
};

int open_wpeers_for_requests(int num_peers, struct peer_rsktd_addr *peers)
{
	int i;
	struct rskt_dmn_wpeer *w;

	sem_wait(&dmn.loop_started);
	sem_post(&dmn.loop_started);

	if ((!dmn.mb_valid && !dmn.cm_skt_tst) || !dmn.speer_conn_alive) {
		ERR("Invalid element in 'dmn'\n");
		return -1;
	}

	DBG("num_peers = %d\n", num_peers);
	for (i = 0; i < num_peers; i++) {
		if (init_wpeer(&w, peers[i].ct, peers[i].cm_skt)) {
			WARN("Peer %d not initialized...skipping\n", i);
			continue;
		}
		DBG("Sending 'hello' to wpeer %d\n", i);
		send_hello_to_wpeer(w);
	};
	return 0;
};

void enqueue_wpeer_msg(struct librsktd_unified_msg *msg)
{
	sem_wait(&dmn.wpeer_tx_mutex);
	l_push_tail(&dmn.wpeer_tx_q, msg);
	sem_post(&dmn.wpeer_tx_mutex);
	sem_post(&dmn.wpeer_tx_cnt);
};

void cleanup_wpeer(struct rskt_dmn_wpeer *wpeer)
{
	struct librsktd_unified_msg *msg;

	wpeer->i_must_die = 1;
	*wpeer->self_ref = NULL;
	sem_post(&wpeer->started);

	if (NULL != wpeer->rx_buff) {
		riomp_sock_release_receive_buffer(wpeer->cm_skt_h, 
							wpeer->rx_buff);
		wpeer->rx_buff = NULL;
	};
	
	if (NULL != wpeer->tx_buff) {
		riomp_sock_release_send_buffer(wpeer->cm_skt_h,
							wpeer->tx_buff);
		wpeer->tx_buff = NULL;
	};

	sem_wait(&wpeer->w_rsp_mutex);
	msg = (struct librsktd_unified_msg *)l_pop_head(&wpeer->w_rsp);
	sem_post(&wpeer->w_rsp_mutex);

	while (NULL != msg) {
		msg->proc_stage = RSKTD_A2W_SEQ_DRESP;
		if (wpeer == NULL) {
			WARN("wpeer == NULL\n");
		} else if (wpeer->resp == NULL) {
			WARN("wpeer->resp == NULL\n");
		} else {
			wpeer->resp->err = ECONNRESET;
		}
		enqueue_mproc_msg(msg);

		sem_wait(&wpeer->w_rsp_mutex);
		msg = (struct librsktd_unified_msg *)l_pop_head(&wpeer->w_rsp);
		sem_post(&wpeer->w_rsp_mutex);
	};
	sem_wait(&dmn.wpeers_mtx);
	l_remove(&dmn.wpeers, wpeer->wp_li);
	sem_post(&dmn.wpeers_mtx);
};

void close_wpeer(struct rskt_dmn_wpeer *wpeer)
{
	cleanup_wpeer(wpeer);

	pthread_kill(wpeer->w_rx, SIGUSR1);
	pthread_join(wpeer->w_rx, NULL);
};

void *wpeer_tx_loop(void *unused)
{
	struct librsktd_unified_msg *msg;
	struct rskt_dmn_wpeer *w;
	uint32_t seq_num;

	dmn.wpeer_tx_alive = 1;
	sem_post(&dmn.loop_started);

	while (!dmn.all_must_die) {
		sem_wait(&dmn.wpeer_tx_cnt);
		if (dmn.all_must_die)
			break;
		sem_wait(&dmn.wpeer_tx_mutex);
		if (dmn.all_must_die)
			break;
		msg = (struct librsktd_unified_msg *)
			l_pop_head(&dmn.wpeer_tx_q);
		sem_post(&dmn.wpeer_tx_mutex);
		if (dmn.all_must_die || (NULL == msg))
			break;

		if ((RSKTD_PROC_A2W != msg->proc_type) ||
				(NULL == msg->wp) ||
				(NULL == *msg->wp)) {
			w = NULL;
			seq_num = -1;
		} else {
			w = *msg->wp;
			seq_num = w->w_seq_num++;
		};

		/* Formulate request message to be sent... */
		msg->dreq->msg_seq = htonl(seq_num);
		msg->dresp->msg_seq = msg->dreq->msg_seq;

		if (NULL == w) {
			/* Enqueue error response for processing */
			if (NULL != msg->dresp)
				msg->dresp->err = htonl(ENETUNREACH);
			if (NULL != msg->tx)
				msg->tx->a_rsp.err = htonl(ENETUNREACH);
			msg->proc_stage = RSKTD_A2W_SEQ_DRESP;
			enqueue_mproc_msg(msg);
		} else {
			/* Enqueue response, send request to wpeer */
			sem_wait(&w->w_rsp_mutex);
			if (dmn.all_must_die)
				continue;
			l_add(&w->w_rsp, seq_num, msg);
			sem_post(&w->w_rsp_mutex);

			memcpy((void *)w->tx_buff, (void *)msg->dreq,
				DMN_REQ_SZ);
        		w->tx_buff_used = 1;
        		w->tx_rc = riomp_sock_send(w->cm_skt_h, w->tx_buff,
						DMN_REQ_SZ);
			if (w->tx_rc)
				close_wpeer(w);
		};
	};
	dmn.wpeer_tx_alive = 0;
	pthread_exit(unused);
};

void close_all_wpeers(void)
{
	void **l;

	sem_wait(&dmn.wpeers_mtx);
	l = (void **)l_pop_head(&dmn.wpeers);
	sem_post(&dmn.wpeers_mtx);

	while (NULL != l) {
		struct rskt_dmn_wpeer *w;
		if (NULL == *l)
			continue;
		w = *(struct rskt_dmn_wpeer **)l;
		close_wpeer(w);
		pthread_kill(w->w_rx, SIGUSR1);
		pthread_join(w->w_rx, NULL);

		sem_wait(&dmn.wpeers_mtx);
		l = (void **)l_pop_head(&dmn.wpeers);
		sem_post(&dmn.wpeers_mtx);
	};
};

void update_wpeer_list(uint32_t destid_cnt, uint32_t *destids)
{
	uint32_t i, found;
	struct rskt_dmn_wpeer **wp_p, **next_wp_p;
	struct rskt_dmn_wpeer *wp;
	struct l_item_t *li;

	/* Search for workers for destIDs that no longer exist */
	/* OR where the peer RSKTD has died... */
	sem_wait(&dmn.wpeers_mtx);
	wp_p = (struct rskt_dmn_wpeer **)l_head(&dmn.wpeers, &li);
	if (wp_p == NULL) {
		WARN("wp_p == NULL\n");
	}
	INFO("Checking for dead WPeers\n");
	while (wp_p != NULL) {
		next_wp_p = (struct rskt_dmn_wpeer **)l_next(&li);
		wp = *wp_p;
		if (NULL == wp) {
			ERR("wp is NULL..next!\n");
			wp_p = next_wp_p;
			continue;
		};
		found = 0;
		DBG("Checking for destID %d, destid_cnt = %d\n",
			wp->ct, destid_cnt);
		for (i = 0; (i < destid_cnt) && !found; i++) {
			if (wp->ct == destids[i]) {
				found = fmdd_check_did(dd_h, wp->ct, 
								FMDD_RSKT_FLAG);
			}
		};

		if (found) {
			DBG("found, wpeer is alive\n");
			wp_p = next_wp_p;
			continue;
		} else {
			DBG("not found, wpeer is DEAD\n");
		}

		sem_post(&dmn.wpeers_mtx);
		INFO("Closing wpeer destID %d\n", wp->ct);
		close_wpeer(wp);
		sem_wait(&dmn.wpeers_mtx);
		wp_p = next_wp_p;
	};
	sem_post(&dmn.wpeers_mtx);
	
	/* Search for destIDs without associated workers */
	INFO("Checking for NEW WPeers\n");
	for (i = 0; i < destid_cnt; i++) {
		struct peer_rsktd_addr new_wpeer;
		found = 0;
		sem_wait(&dmn.wpeers_mtx);
		wp_p = (struct rskt_dmn_wpeer **)l_head(&dmn.wpeers, &li);
		if (wp_p == NULL) {
			WARN("wp_p == NULL\n");
		}
		INFO("Checking wpeer DID %d\n", destids[i]);
		while ((wp_p != NULL) && !found) {
			wp = *wp_p;
			if ((NULL != wp) && (wp->ct == destids[i])) {
				INFO("FOUND!\n");
				found = 1;
			}
			wp_p = (struct rskt_dmn_wpeer **)l_next(&li);
		};
		sem_post(&dmn.wpeers_mtx);

		if (found) {
			DBG("Wpeer DID %d is ready running!\n", destids[i]);
			continue;
		}

		/* Check that RSKTD is running on the peer... */
		if (!fmdd_check_did(dd_h, destids[i], FMDD_RSKT_FLAG)) {
			DBG("RSKTD is NOT running on the peer destid(%d)\n",
					destids[i]);
			continue;
		}

		new_wpeer.ct = destids[i];
		new_wpeer.cm_skt = DFLT_DMN_CM_CONN_SKT;
		
		DBG("new_wpeer.ct = %d, new_wpeer.cm_skt = %d\n",
				new_wpeer.ct, new_wpeer.cm_skt);

		INFO("Openning new peer DID %d\n", new_wpeer.ct);
		open_wpeers_for_requests(1, &new_wpeer);
	};
};
		
#ifdef __cplusplus
}
#endif
