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
#include "librskt_threads.h"
#include "librskt_private.h"
#include "liblist.h"
#include "libcli.h"
#include "rapidio_mport_dma.h"
#include "libtime_utils.h"
#include "liblog.h"

struct rsvp_li {
	sem_t resp_rx;
	struct librskt_rsktd_to_app_msg *resp; 
};

int librskt_wait_for_sem(sem_t *sema, int err_code)
{
	int rc;

	do {
		rc = sem_wait(sema);
	} while (rc && (EINTR == errno) && !lib.all_must_die);

	if (rc) {
		ERR("Failed in sem_wait() loc 0x%x rc= %d errno = %d %s",
			err_code, rc, errno, strerror(errno));
		lib.all_must_die = err_code;
	}
	return rc;
};

#define DEFAULT_RESPONSE_TIMEOUT ((struct timespec){60, 500000000})

int wait_for_response(sem_t *sema, int err_code)
{
        int rc;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        ts = time_add(ts, DEFAULT_RESPONSE_TIMEOUT);

        do {
                rc = sem_timedwait(sema, &ts);
        } while (rc && (EINTR == errno) && !lib.all_must_die);

        if (rc && (ETIMEDOUT != errno)) {
                ERR("Failed in sem_timedwait() loc 0x%x rc= %d errno = %d %s",
                        err_code, rc, errno, strerror(errno));
                lib.all_must_die = err_code;
        }
        return rc;
};

struct librskt_app_to_rsktd_msg *alloc_app2d(void)
{
	struct librskt_app_to_rsktd_msg *ptr;
	assert(A2RSKTD_SZ >= sizeof(struct librskt_app_to_rsktd_msg)); 
	ptr = (struct librskt_app_to_rsktd_msg *)calloc(1, A2RSKTD_SZ);
	ptr->in_use = 1;

	return ptr;
};

int free_app2d(struct librskt_app_to_rsktd_msg *ptr)
{
	if (NULL == ptr)
		goto fail;

	free(ptr);
	return 0;
fail:
	return 1;
};
struct librskt_rsktd_to_app_msg *alloc_d2app(void)
{
	struct librskt_rsktd_to_app_msg *ptr;
	assert(RSKTD2A_SZ >= sizeof(struct librskt_rsktd_to_app_msg));
	ptr = (struct librskt_rsktd_to_app_msg *)calloc(1, RSKTD2A_SZ);
	ptr->in_use = 1;
	return ptr;
};

int free_d2app(struct librskt_rsktd_to_app_msg *ptr)
{
	if (NULL == ptr)
		goto fail;

	free(ptr);
	return 0;
fail:
	return 1;
};

struct rsvp_li *alloc_rsvp(void)
{
	return (struct rsvp_li *)
		calloc(1, sizeof(struct rsvp_li));
};

int free_rsvp(struct rsvp_li *ptr)
{
	if (NULL == ptr)
		goto fail;

	free(ptr);
	return 0;
fail:
	return 1;
};

#define TIMEOUT true
#define NO_TIMEOUT false

int librskt_dmsg_req_resp(struct librskt_app_to_rsktd_msg *tx, 
			struct librskt_rsktd_to_app_msg *rx,
			bool chk_rsp_to)
{
	struct rsvp_li *rsvp = alloc_rsvp();
	int rc = 0;
	struct l_item_t *li = NULL;
	uint32_t seq_num;

	sem_init(&rsvp->resp_rx, 0, 0);
	rsvp->resp = rx;

	/* Always add response to queue before sending request to avoid
	 * losing the race condition.
	 */
	tx->msg_type = htonl(tx->msg_type);
	rx->msg_type = tx->msg_type | htonl(LIBRSKTD_RESP);
	rx->a_rsp.err = 0;
	rx->a_rsp.req = tx->a_rq;
	DBG("Waiting for lib.rsvp_mtx");
	if (librskt_wait_for_sem(&lib.rsvp_mtx, 0x1001)) {
		ERR("Failed on rspv_mtx");
		goto fail;
	}
	seq_num = lib.lib_req_seq++;
	tx->a_rq.app_seq_num = htonl(seq_num);
	rx->a_rsp.req.app_seq_num = tx->a_rq.app_seq_num;
	li = l_add(&lib.rsvp, seq_num, (void *)rsvp);
	sem_post(&lib.rsvp_mtx);

	DBG("Waiting for lib.msg_tx_mtx");
	if (librskt_wait_for_sem(&lib.msg_tx_mtx, 0x1002)) {
		ERR("Failed on msg_tx_mtx");
		goto fail;
	}
	l_push_tail(&lib.msg_tx, (void *)tx); 
	sem_post(&lib.msg_tx_mtx);
	sem_post(&lib.msg_tx_cnt);

	DBG("Waiting for rsvp->resp_rx");

        if (chk_rsp_to && 0) {
                if (wait_for_response(&rsvp->resp_rx, 0x1003)) {
                        ERR("Failed on resp_rx");
                        goto fail;
                }
        } else {
                if (librskt_wait_for_sem(&rsvp->resp_rx, 0x1004)) {
                        ERR("Failed on resp_rx");
                        goto fail;
                }
        };
	DBG("NOT Waiting for rsvp->resp_rx");
	if (rx->a_rsp.err) {
		li = NULL;
		rc = -1;
		errno = ntohl(rx->a_rsp.err);
		ERR("a_rsp.err is not 0: %s", strerror(errno));
		goto fail;
	};

	if (free_rsvp(rsvp))
		{ ERR("ERR freeing rsvp"); }
	DBG("Returning!");
	
	return rc;
fail:
	if (NULL != li) {
		int saved_errno = errno;

		librskt_wait_for_sem(&lib.rsvp_mtx, 0x1005);
		DBG("Calling l_lremove()");
		l_lremove(&lib.rsvp, li);
		sem_post(&lib.rsvp_mtx);
		errno = saved_errno;
		rx->a_rsp.err = errno;
	};
	free(rsvp);
	return -1;
}; /* librskt_dmsg_req_resp() */
	
int librskt_dmsg_tx_resp(struct librskt_app_to_rsktd_msg *tx)
{
	int rc = librskt_wait_for_sem(&lib.msg_tx_mtx, 0x1010);

	if (rc) {
		ERR("Failed on msg_tx_mtx");
		goto fail;
	}
	l_push_tail(&lib.msg_tx, tx); 
	sem_post(&lib.msg_tx_mtx);
	sem_post(&lib.msg_tx_cnt);
	rc = 0;
fail:
	return rc;
}; /* librskt_dmsg_tx_resp() */
/* TX Thread to send requests/responses and avoid blocking */
/* FIXME: Is this really necessary??? */
void *tx_loop(void *unused)
{
	struct librskt_app_to_rsktd_msg *tx;
	int rc;
	char my_name[16];

	memset(my_name, 0, 16);
	snprintf(my_name, 15, "LIBRSKT_TX");
	pthread_setname_np(lib.tx_thr, my_name);

	DBG("ENTER");

	while (!lib.all_must_die) {
		if (librskt_wait_for_sem(&lib.msg_tx_cnt, 0x1020))
			lib.all_must_die = 1;
		if (lib.all_must_die)
			goto exit;
		if (librskt_wait_for_sem(&lib.msg_tx_mtx, 0x1021))
			lib.all_must_die = 2;
		if (lib.all_must_die)
			goto exit;
		tx = (struct librskt_app_to_rsktd_msg *)l_pop_head(&lib.msg_tx);
		sem_post(&lib.msg_tx_mtx);

		if (lib.all_must_die)
			goto exit;
		if (NULL == tx)
			continue;

		DBG("App Sending %s Seq %d",
			LIBRSKT_APP_MSG_TO_STR(htonl(tx->msg_type)),
			LIBRSKT_APP_2_DMN_MSG_SEQ_NO(tx, htonl(tx->msg_type)) );
		rc = send(lib.fd, (void *)tx, A2RSKTD_SZ, MSG_EOR);
		if (rc < 0) {
			lib.all_must_die = 3;
		}
		// free(tx);
	};
exit:
	CRIT("EXIT");
	pthread_exit(unused);
};

/* RX Thread to receive requests/responses and avoid blocking */
		
void rsvp_loop_resp(struct librskt_rsktd_to_app_msg *rxd)
{
	struct l_item_t *li;
	struct rsvp_li *dlyd;

	DBG("ENTER");
	if (librskt_wait_for_sem(&lib.rsvp_mtx, 0x1030)) {
		WARN("lib.all_must_die = 20");
		lib.all_must_die = 20;
		goto exit;
	};
	dlyd = (struct rsvp_li *)
		l_find(&lib.rsvp, ntohl(rxd->a_rsp.req.app_seq_num), &li);
	if (NULL != dlyd) {
		DBG("Response thread found for %s Seq %d",
			LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
		memcpy((void *)&dlyd->resp->a_rsp.msg, (void *)&rxd->a_rsp.msg, 
					sizeof(union librskt_resp_u));
		dlyd->resp->a_rsp.err = rxd->a_rsp.err;
		sem_post(&dlyd->resp_rx);
		DBG("Calling l_lremove");
		l_lremove(&lib.rsvp, li);
	} else {
		DBG("%s Seq %d Discard, dlyd is NULL",
			LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
	}
exit:
	sem_post(&lib.rsvp_mtx);
};

void rsvp_loop_req(struct librskt_rsktd_to_app_msg *rxd)
{
	if (htonl(LIBRSKT_CLOSE_CMD) == rxd->msg_type) {
		DBG("%s Seq %d Add to lib.req",
			LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
		if (librskt_wait_for_sem(&lib.req_mtx, 0x1040)) {
			WARN("lib.all_must_die = 30");
			lib.all_must_die = 30;
			sem_post(&lib.req_mtx);
			return;
		};
		l_push_tail(&lib.req, (void *)rxd);
		sem_post(&lib.req_mtx);
		sem_post(&lib.req_cnt);
		DBG("%s Seq %d Added to lib.req",
			LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
		return;
	};
	DBG("Discarding %s Seq %d",
		LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
		LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
	free(rxd);
};

void rsvp_loop_sig_handler(int sig)
{
	if (sig)
		return;
};

void *rsvp_loop(void *unused)
{
	int rc;
	struct librskt_rsktd_to_app_msg *rxd = alloc_d2app();
	struct sigaction rsvp_loop_sig;
	struct rsvp_li *delayed;
	char my_name[16];

	memset(&rsvp_loop_sig, 0, sizeof(rsvp_loop_sig));
	rsvp_loop_sig.sa_handler = rsvp_loop_sig_handler;

	if (rxd == NULL) {
		CRIT("Failed to allocate librskt_rsktd_to_app_msg");
		return NULL;
	}

	sigaction(SIGUSR1, &rsvp_loop_sig, NULL);

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "LIBRSKT_RSVP");
        pthread_setname_np(lib.rsvp_thr, my_name);

	DBG("ENTER");
	while (!lib.all_must_die) {
		memset((void *)rxd, 0, RSKTD2A_SZ);
		rc = recv(lib.fd, (void *)rxd, RSKTD2A_SZ, 0);
		if (rc <= 0) {
			ERR("Failed in recv()");
			lib.all_must_die = 10;
		};
		if (lib.all_must_die)
			goto exit;

		DBG("Received %d bytes max %d, %s Seq %d", rc, RSKTD2A_SZ,
			LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );

		if (rxd->msg_type & htonl(LIBRSKTD_RESP | LIBRSKTD_FAIL)) {
			DBG("%s Seq %d Calling rsvp_loop_resp",
				LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
			rsvp_loop_resp(rxd);
		} else {
			DBG("%s Seq %d Calling rsvp_loop_req",
				LIBRSKT_APP_MSG_TO_STR(htonl(rxd->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(rxd, htonl(rxd->msg_type)) );
			rsvp_loop_req(rxd);
			rxd = alloc_d2app();
			if (rxd == NULL) {
				CRIT("Failed to allocate librskt_rsktd_to_app_msg");
			}
		};
	};
exit:
	free(rxd);

	sem_wait(&lib.rsvp_mtx);
	delayed = (struct rsvp_li *)l_pop_head(&lib.rsvp);

	while (NULL != delayed) {
		delayed->resp->a_rsp.err = ECONNRESET;
		sem_post(&delayed->resp_rx);
		delayed = (struct rsvp_li *)l_pop_head(&lib.rsvp);
	};
	sem_post(&lib.rsvp_mtx);

	CRIT("EXIT");
	pthread_exit(unused);
};

int lib_handle_dmn_close_req(rskt_h skt_h)
{
	volatile struct rskt_socket_t *skt;
	int rc = 0;

	DBG("ENTER");
	skt = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == skt) {
		DBG("Socket already closed");
		return 0;
	}

	DBG("skt->sa.sn = %d skt->sai.sn = %d", skt->sa.sn, skt->sai.sa.sn);

	if (rskt_connecting == rsktl_get_st(&lib.skts, skt_h)) {
		/* Special case where the socket is in use and the 
		* semaphore is not taken.  Set an error flag, 
		* wait for a bit, then resume and see what has
		* changed.
		*/
		skt->hdr->loc_tx_wr_flags |= htonl(RSKT_BUF_HDR_FLAG_CLOSING | RSKT_BUF_HDR_FLAG_LP_EXIT);
		skt->hdr->loc_rx_rd_flags |= htonl(RSKT_BUF_HDR_FLAG_CLOSING | RSKT_BUF_HDR_FLAG_LP_EXIT);
		sem_post(&lib.req_mtx);

		usleep(5*SETUP_SKT_PTRS_POLL);

		if (librskt_wait_for_sem(&lib.req_mtx, 0x1061)) {
			ERR("librskt_wait_for_sem() failed. Exiting");
			goto exit;
		}
		skt = rsktl_sock_ptr(&lib.skts, skt_h);
		if (skt == NULL) {
			DBG("skt->sa.sn already closed");
			return 0;
		}
	};
	rc = cleanup_skt(skt_h, skt);

exit:
	DBG("EXIT");
	return rc;
};

void prep_response(struct librskt_rsktd_to_app_msg *req,
                struct librskt_app_to_rsktd_msg *resp)
{
        resp->msg_type = req->msg_type | htonl(LIBRSKTD_RESP);
        resp->rsp_a.err = 0;
        resp->rsp_a.req_a = req->rq_a;
};

/* Request Processing Thread */
void *req_loop(void *unused)
{
	struct librskt_rsktd_to_app_msg *req;
	struct librskt_app_to_rsktd_msg *resp = NULL;
	rskt_h skt_h;
	char my_name[16];
	int flushed;

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "LIBRSKT_REQ");
        pthread_setname_np(lib.req_thr, my_name);

	while (!lib.all_must_die) {
	       
		if (NULL == resp) {
			resp = alloc_app2d();

			if (resp == NULL) {
				ERR("Failed to allocate 'resp'");
				return NULL;
			}
		}

		if (librskt_wait_for_sem(&lib.req_cnt, 0x1060)) {
			ERR("librskt_wait_for_sem() failed. Exiting");
			goto exit;
		}
		if (lib.all_must_die)
			goto exit;
		if (librskt_wait_for_sem(&lib.req_mtx, 0x1061)) {
			ERR("librskt_wait_for_sem() failed. Exiting");
			goto exit;
		}
		if (lib.all_must_die)
			goto exit;
		req = (struct librskt_rsktd_to_app_msg *)l_pop_head(&lib.req);
		sem_post(&lib.req_mtx);

		if (NULL == req) {
			DBG("Popped a NULL request???");
			continue;
		}

		DBG("%s Seq %d Processing",
			LIBRSKT_APP_MSG_TO_STR(htonl(req->msg_type)),
			LIBRSKT_DMN_2_APP_MSG_SEQ_NO(req, htonl(req->msg_type)) );
		prep_response(req, resp);

		switch (ntohl(req->msg_type)) {
		case LIBRSKT_CLOSE_CMD:
			/* Got this request from the RSKTD.
			* Send response when are sure that app can't use this
			* socket any more.
			*/
			INFO("Received LIBRSKT_CLOSE_CMD from RSKTD");
			skt_h = rsktl_find_skt_h(&lib.skts,
						ntohl(req->rq_a.msg.clos.sn));
			if (LIBRSKT_H_INVALID == skt_h) {
                                WARN("%s Seq %d SN %d No socket found",
                                        LIBRSKT_APP_MSG_TO_STR(
                                                        htonl(req->msg_type)),
                                        LIBRSKT_DMN_2_APP_MSG_SEQ_NO(
                                                req, htonl(req->msg_type)),
                                         ntohl(req->rq_a.msg.clos.sn));
				break;
			}
			DBG("%s Seq %d SN %d",
				LIBRSKT_APP_MSG_TO_STR(htonl(req->msg_type)),
				LIBRSKT_DMN_2_APP_MSG_SEQ_NO(
						req, htonl(req->msg_type)),
				ntohl(req->rq_a.msg.clos.sn));
			lib_handle_dmn_close_req(skt_h);
			flushed = lib_handle_dmn_close_req(skt_h);
			resp->rsp_a.req_a.msg.clos.dma_flushed = htonl(flushed);
			break;
		default: resp->msg_type |= htonl(LIBRSKTD_FAIL);
			ERR("Unknown daemon request type: 0x%x", ntohl(req->msg_type));
		};
		if (librskt_dmsg_tx_resp(resp)) {
			ERR("librskt_dmsg_tx_resp failed. Exiting");
			goto exit;
		}
		resp = NULL;
	};
exit:
	CRIT("EXIT");
	pthread_exit(unused);
};

int librskt_init_threads(void)
{
	int rc = -1;

	DBG("ENTER");

	sem_init(&lib.msg_tx_mtx, 0, 1);
	sem_init(&lib.msg_tx_cnt, 0, 0);
	l_init(&lib.msg_tx);

	sem_init(&lib.rsvp_mtx, 0, 1);
	l_init(&lib.rsvp);
	lib.lib_req_seq = 0;

	sem_init(&lib.req_mtx, 0, 1);
	sem_init(&lib.req_cnt, 0, 0);
	l_init(&lib.req);

	/* Startup the threads */
	DBG("Starting tx_loop");
	if (pthread_create( &lib.tx_thr, NULL, tx_loop, NULL)) {
		lib.all_must_die = 1;
		CRIT("ERROR:librskt_init, tx_loop thread: %s", strerror(errno));
		goto fail;
	};
	DBG("Starting rsvp_loop");
	if (pthread_create( &lib.rsvp_thr, NULL, rsvp_loop, NULL)) {
		lib.all_must_die = 2;
		CRIT("ERROR:librskt_init rsvp_loop thread: %s", strerror(errno));
		goto fail;
	};
	DBG("Starting req_loop");
	if (pthread_create( &lib.req_thr, NULL, req_loop, NULL)) {
		lib.all_must_die = 3;
		CRIT("ERROR:librskt_init, req_loop thread: %s", strerror(errno));
		goto fail;
	};
	rc = 0;
fail:
	DBG("EXIT, rc = %d", rc);
	return rc;
};

void librskt_finish_threads(void)
{	
	INFO("ENTRY");
	struct rsvp_li *delayed;

	lib.all_must_die = 1;
	if ((lib.init_ok == lib.portno) && lib.portno) {
		DBG("Joining librskt threads");
		/* Kill the receiver thread first */
		/* Allow req_loop to terminate */
		sem_post(&lib.req_cnt);
		pthread_join(lib.req_thr, NULL);
		DBG("Joined req_thr");

		/* Then close the rsvp thread, add responses to TX loop */
		DBG("Closing socket handle");
		pthread_kill(lib.rsvp_thr, SIGUSR1);
		pthread_join(lib.rsvp_thr, NULL);
		DBG("Joined rsvp_thr");
	
		/* Lastly close tx loop */
		sem_post(&lib.msg_tx_cnt);
		pthread_join(lib.tx_thr, NULL);
		DBG("Joined tx_thr");
	
		lib.init_ok = 0;
	};

	delayed = (struct rsvp_li *)l_pop_head(&lib.rsvp);

	while (NULL != delayed) {
		delayed->resp->a_rsp.err = ECONNRESET;
		sem_post(&delayed->resp_rx);
		delayed = (struct rsvp_li *)l_pop_head(&lib.rsvp);
	};
	INFO("EXIT");
};

#ifdef __cplusplus
}
#endif
