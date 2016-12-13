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

struct rskt_dmn_wpeer *alloc_wpeer(ct_t ct, uint32_t cm_skt)
{ 
	struct rskt_dmn_wpeer *w = NULL;
	int i;

	for (i = 0; i < MAX_PEER; i++) {
		if (dmn.wpeers[i].wpeer_alive || dmn.wpeers[i].cm_skt_h_valid)
			continue;
		w = &dmn.wpeers[i];
		break;
	};

	if (NULL == w) {
		ERR("Max WPEERS is %d, cannot connect to another.", MAX_PEER);
		return NULL;
	};
	w->self_ref = &w->self_ref_ref;
	w->self_ref_ref = w;

	w->ct = ct;
	w->cm_skt = cm_skt;
	w->cm_skt_h_valid = 0;

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

void wpeer_loop_sig_handler(int sig)
{
        if (sig)
                return;
}

void cleanup_wpeer(struct rskt_dmn_wpeer *wpeer)
{
	struct librsktd_unified_msg *msg = NULL;

	if (NULL == wpeer) {
		return;
	}

	*wpeer->self_ref = NULL;
	wpeer->wpeer_alive = 0;
	wpeer->i_must_die = 1;
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

	if (wpeer->cm_skt_h_valid) {
		int rc;
		wpeer->cm_skt_h_valid = 0;
		rc = riomp_sock_close(&wpeer->cm_skt_h);
		if (rc) {
			ERR("riomp_sock_close ERR %d\n", rc);
		};
	};

	sem_wait(&wpeer->w_rsp_mutex);
	msg = (struct librsktd_unified_msg *)l_pop_head(&wpeer->w_rsp);
	sem_post(&wpeer->w_rsp_mutex);

	/* Send "failed" responses to application */
	while (NULL != msg) {
		msg->proc_stage = RSKTD_A2W_SEQ_DRESP;
		if (wpeer->resp == NULL) {
			WARN("wpeer->resp == NULL\n");
		} else {
			wpeer->resp->err = ECONNRESET;
		}
		enqueue_mproc_msg(msg);

		sem_wait(&wpeer->w_rsp_mutex);
		msg = (struct librsktd_unified_msg *)l_pop_head(&wpeer->w_rsp);
		sem_post(&wpeer->w_rsp_mutex);
	};
};


void *wpeer_rx_loop(void *p_i)
{
	struct rskt_dmn_wpeer *w = (struct rskt_dmn_wpeer *)p_i;
        struct sigaction sigh;
	int rc;

	rc = pthread_detach(w->w_rx);
	if (rc) {
		WARN("pthread_detach rc %d", rc);
	}

        memset(&sigh, 0, sizeof(sigh));
        sigh.sa_handler = wpeer_loop_sig_handler;
        sigaction(SIGUSR1, &sigh, NULL);

	/* Note: Thread name is set when the HELLO response is received. */
	w->i_must_die = 0;
	sem_post(&w->started);
	DBG("started\n");
	while (!w->i_must_die) {
		struct librsktd_unified_msg *msg;
		uint32_t seq_num;
		struct l_item_t *li;

		w->rx_buff_used = 1;
                do {
			riomp_sock_receive(w->cm_skt_h, 
				&w->rx_buff, DMN_RESP_SZ, 0);
		} while (!w->i_must_die && !dmn.all_must_die && 
		((EINTR == errno) || (EAGAIN == errno) || (ETIME == errno)));

		if (w->i_must_die || dmn.all_must_die) {
			DBG("Either i_must_die or all_must_die\n");
			break;
		}

		seq_num = ntohl(w->resp->msg_seq);

		sem_wait(&w->w_rsp_mutex);
		msg = (struct librsktd_unified_msg *)
			l_find(&w->w_rsp, seq_num, &li);
		if (NULL != msg)
			l_lremove(&w->w_rsp, li);
		sem_post(&w->w_rsp_mutex);

		if (NULL == msg) {
			DBG("msg is NULL, continuing\n");
			continue;
		}

		msg->proc_stage = RSKTD_A2W_SEQ_DRESP;
		memcpy((void *)&msg->dresp->msg, (void *)&w->resp->msg, 
				sizeof(union librsktd_resp));
		msg->dresp->err = w->resp->err; 
		
		INFO("WPeer %d Rx: Type %x seq %x err %x\n",
			w->ct,
			w->resp->msg_type,
			w->resp->msg_seq,
			w->resp->err);
		INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s Seq %x err %x",
			UMSG_W_OR_S(msg),
			UMSG_CT(msg),
			msg->msg_type,
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg),
			UMSG_STAGE_TO_STR(msg),
			w->resp->msg_seq,
			w->resp->err);
		enqueue_mproc_msg(msg);
	};

	/* Stop others from using the wpeer */
	HIGH("Wpeer %d EXITING, cleaning up\n", w->ct);
	cleanup_wpeer(w);

	pthread_exit(NULL);
};

int init_wpeer(struct rskt_dmn_wpeer **wp, ct_t ct, uint32_t cm_skt)
{
	int rc, conn_rc;
	struct rskt_dmn_wpeer *w = NULL;
	int attempts = 5;

	w = alloc_wpeer(ct, cm_skt);
	if (NULL == w) {
		return -ENOMEM;
	}
	*wp = w;

	DBG("ENTER\n");
	do {
		sem_wait(&dmn.mb_mtx);
		rc = riomp_sock_socket(dmn.mb, &w->cm_skt_h);
		sem_post(&dmn.mb_mtx);

		if (rc) {
                        ERR("riomp_sock_socket ERR %d\n", rc);
			goto fail;
                };

		conn_rc = riomp_sock_connect(w->cm_skt_h, w->ct, w->cm_skt);
                if (!conn_rc) {
                	HIGH("ct %d connected\n", ct);
			w->cm_skt_h_valid = 1;
                        break;
                }

                rc = riomp_sock_close(&w->cm_skt_h);
                if (rc) {
                        ERR("riomp_sock_close ERR %d\n", rc);
                };
		sleep(1);
	} while (conn_rc && attempts-- && 
		((ENODEV == errno) || (EINTR == errno) || (ETIME == errno)));

	switch (conn_rc) {
	case 0:
		break;
        case EADDRINUSE:
                CRIT("init_wpeer %d: Requested channel %d in use...\n",
			ct, cm_skt);
		break;
	default:
		CRIT("init_wpeer %d connect %d error: %d %s\n",
				ct, cm_skt, conn_rc, strerror(conn_rc));
		break;
        }
	if (conn_rc) {
		rc = conn_rc;
		goto fail;
	}

        rc = riomp_sock_request_send_buffer(w->cm_skt_h, &w->tx_buff);
        if (rc) {
               	CRIT("init_wpeer %d: req_buffer: %d\n", ct, rc);
		goto fail;
        }

	w->rx_buff = calloc(1, RSKTD_CM_MSG_SIZE);
	if (NULL == w->rx_buff) {
		CRIT("Could not allocate rx buffer %d %s\n",
							errno, strerror(errno));
		rc = -ENOMEM;
		goto fail;
	};

	DBG("Creating wpeer_rx_loop\n");
        rc = pthread_create(&w->w_rx, NULL, wpeer_rx_loop, (void*)w);
	if (rc) {
		CRIT("Could not start wpeer_rx_loop for ct 0x%x\n rc %d %d %s",
			ct, rc, errno, strerror(errno));
		goto fail;
	};
	DBG("Waiting for wpeer_rx_loop() to start\n");
	sem_wait(&w->started);
	DBG("wpeer_rx_loop started\n");

	return 0;
fail:
	cleanup_wpeer(w);
	*wp = NULL;
	return rc;
};

void send_hello_to_wpeer(struct rskt_dmn_wpeer *w)
{
	struct librsktd_unified_msg *msg;

	DBG("Sending hello to wpeer\n");
	msg = alloc_msg(RSKTD_HELLO_REQ, RSKTD_PROC_A2W, RSKTD_A2W_SEQ_DREQ);
	if (NULL == msg) {
		ERR("No message buffers\n");
		return;
	}

	msg->wp = w->self_ref;
	msg->dreq = alloc_dreq();
	msg->dresp = alloc_dresp();
	if ((NULL == msg->dreq) || (NULL == msg->dresp)) {
		ERR("No buffers %p, %p\n", msg->dreq, msg->dresp);
		dealloc_msg(msg);
		return;
	}

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
	struct rskt_dmn_wpeer *w = NULL;

	if (!dmn.mb_valid || !dmn.speer_conn_alive) {
		ERR("Mailbox invalid or no speer connection thread\n");
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
	INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s",
		UMSG_W_OR_S(msg),
		UMSG_CT(msg),
		msg->msg_type,
		UMSG_TYPE_TO_STR(msg),
		UMSG_PROC_TO_STR(msg),
		UMSG_STAGE_TO_STR(msg));
	sem_wait(&dmn.wpeer_tx_mutex);
	l_push_tail(&dmn.wpeer_tx_q, msg);
	sem_post(&dmn.wpeer_tx_mutex);
	sem_post(&dmn.wpeer_tx_cnt);
	INFO("EXIT");
};

void close_wpeer(struct rskt_dmn_wpeer *wpeer)
{
	wpeer->i_must_die = 1;

	if (NULL != wpeer->self_ref) {
		pthread_kill(wpeer->w_rx, SIGUSR1);
		wpeer->wpeer_alive = 0;
	};
};

void halt_wpeer_tx_loop(void)
{
        pthread_kill(dmn.wpeer_tx_thread, SIGUSR1);
        pthread_join(dmn.wpeer_tx_thread, NULL);
};

void *wpeer_tx_loop(void *unused)
{
	struct librsktd_unified_msg *msg = NULL;
	struct rskt_dmn_wpeer *w = NULL;
	uint32_t seq_num;
        struct sigaction sigh;
        char my_name[16] = {0};

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "WPEER_TX_LOOP");
        pthread_setname_np(dmn.wpeer_tx_thread, my_name);

        memset(&sigh, 0, sizeof(sigh));
        sigh.sa_handler = wpeer_loop_sig_handler;
        sigaction(SIGUSR1, &sigh, NULL);

	dmn.wpeer_tx_alive = 1;
	sem_post(&dmn.wpeer_tx_loop_started);

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
		if (NULL != msg->dreq) {
			msg->dreq->msg_seq = htonl(seq_num);
		}
		if (NULL != msg->dresp) {
			msg->dresp->msg_seq = htonl(seq_num);
		}

		if (NULL == w) {
			/* Enqueue error response for processing */
			if (NULL != msg->dresp)
				msg->dresp->err = htonl(ENETUNREACH);
			if (NULL != msg->tx)
				msg->tx->a_rsp.err = htonl(ENETUNREACH);
			msg->proc_stage = RSKTD_A2W_SEQ_DRESP;
			INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s ERROR - NO WP!",
				UMSG_W_OR_S(msg),
				UMSG_CT(msg),
				msg->msg_type,
				UMSG_TYPE_TO_STR(msg),
				UMSG_PROC_TO_STR(msg),
				UMSG_STAGE_TO_STR(msg));
			enqueue_mproc_msg(msg);
		} else {
			/* Enqueue response, send request to wpeer */
			sem_wait(&w->w_rsp_mutex);
			if (dmn.all_must_die)
				continue;
			l_add(&w->w_rsp, seq_num, msg);
			sem_post(&w->w_rsp_mutex);

			if (NULL != msg->dreq) {
				memcpy((void *)w->tx_buff, (void *)msg->dreq,
					DMN_REQ_SZ);
			} else {
				ERR("Setting tx_buff null, msg->dreq is null\n");
				memset((void *)w->tx_buff, 0,
					DMN_REQ_SZ);
			}
        		w->tx_buff_used = 1;
			INFO("Msg %s 0x%x Type 0x%x %s Proc %s Stage %s Sent to WP!",
				UMSG_W_OR_S(msg),
				UMSG_CT(msg),
				msg->msg_type,
				UMSG_TYPE_TO_STR(msg),
				UMSG_PROC_TO_STR(msg),
				UMSG_STAGE_TO_STR(msg));
        		w->tx_rc = riomp_sock_send(w->cm_skt_h, w->tx_buff,
						DMN_REQ_SZ);
			if (w->tx_rc)
				close_wpeer(w);
		};
	};
	dmn.wpeer_tx_alive = 0;
	sem_post(&dmn.graceful_exit);
	pthread_exit(unused);
};

void close_all_wpeers(void)
{
	int i;

	for (i = 0; i < MAX_PEER; i++) {
		if (dmn.wpeers[i].wpeer_alive || !dmn.wpeers[i].i_must_die)
			close_wpeer(&dmn.wpeers[i]);
	};
};

void update_wpeer_list(uint32_t destid_cnt, uint32_t *destids)
{
	uint32_t i, j, found;

	/* Search for workers for destIDs that no longer exist */
	/* OR where the peer RSKTD has died... */

	INFO("Checking for dead WPeers\n");
	for (j = 0; j < MAX_PEER; j++) {
		if (!dmn.wpeers[j].wpeer_alive || dmn.wpeers[j].i_must_die) 
			continue;
		found = 0;
		DBG("Checking for destID %d, destid_cnt = %d\n",
			dmn.wpeers[j].ct, destid_cnt);
		for (i = 0; (i < destid_cnt) && !found; i++) {
			if (dmn.wpeers[j].ct == destids[i]) {
				found = fmdd_check_did(dd_h, dmn.wpeers[j].ct, 
								FMDD_RSKT_FLAG);
			}
		};

		if (found) {
			DBG("found, wpeer is alive\n");
			continue;
		} else {
			DBG("not found, wpeer is DEAD\n");
		}

		INFO("Closing wpeer destID %d\n", dmn.wpeers[j].ct);
		close_wpeer(&dmn.wpeers[j]);
	};
	
	/* Search for destIDs without associated workers */
	INFO("Checking for NEW WPeers\n");
	for (i = 0; i < destid_cnt; i++) {
		struct peer_rsktd_addr new_wpeer;
		struct rskt_dmn_wpeer **wp;

		found = 0;
		INFO("Checking wpeer DID %d\n", destids[i]);
		wp = find_wpeer_by_ct(destids[i]);

		if (NULL != wp) {
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
		new_wpeer.cm_skt = RSKT_DFLT_DMN_CM_PORT;
		
		DBG("new_wpeer.ct = %d, new_wpeer.cm_skt = %d\n",
				new_wpeer.ct, new_wpeer.cm_skt);

		INFO("Openning new peer DID %d\n", new_wpeer.ct);
		open_wpeers_for_requests(1, &new_wpeer);
	};
};
		
int start_wpeer_handler(void)
{
	int rc;

	dmn.wpeer_tx_alive = 0;

	sem_init(&dmn.wpeer_tx_loop_started, 0, 0);
        sem_init(&dmn.wpeer_tx_mutex, 0, 1);
        sem_init(&dmn.wpeer_tx_cnt, 0, 0);
        l_init(&dmn.wpeer_tx_q);
        rc = pthread_create(&dmn.wpeer_tx_thread, NULL, wpeer_tx_loop, NULL);
        if (rc)
                goto fail;
	sem_wait(&dmn.wpeer_tx_loop_started);

	return 0;
fail:
	return rc;
};

void halt_wpeer_handler(void)
{
	close_all_wpeers();
	halt_wpeer_tx_loop();
};

#ifdef __cplusplus
}
#endif
