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
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/in.h>
#include <assert.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "librskt_private.h"
#include "librskt_test.h"
#include "librsktd.h"
#include "librsktd_private.h"
#include "liblist.h"
#include "libcli.h"

#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RDMA_CONN_ACC_TO_SECS 3

struct librskt_globals lib;

char *rskt_state_strs[rskt_max_state] = {
	(char *)"UNINIT",
	(char *)"Allocd",
	(char *)"Bound ",
	(char *)"NoConn",
	(char *)"Listen",
	(char *)"Accept",
	(char *)"Coning",
	(char *)"CONNED",
	(char *)"Shutng",
	(char *)"Closng",
	(char *)"SHTDWN",
	(char *)"CLOSED"
};

void rskt_clear_skt(struct rskt_socket_t *skt) 
{
        skt->st = rskt_uninit;
        skt->debug = 0;
        skt->max_backlog = 0;
        skt->sai.sa.ct = 0;
        skt->sai.sa.sn = 0;
        skt->sai.rtID = 0xFFFFFFFF;
        skt->sai.rtstat = 0;
        skt->sa.ct = 0;
        skt->sa.sn = 0;
	skt->connector = skt_rmda_uninit;
	memset((void *)skt->msoh_name, 0, MAX_MS_NAME);
	skt->msoh_valid = 0;
	memset((void *)skt->msh_name, 0, MAX_MS_NAME);
	skt->msh_valid = 0;
	skt->msubh_valid = 0;
        skt->msub_sz = 0;
	skt->msub_p = NULL;
	skt->tx_buf = NULL;
	skt->rx_buf = NULL;
	skt->buf_sz = 0;
	memset((void *)skt->con_msh_name, 0, MAX_MS_NAME);
        skt->con_sz = 0;
	skt->stats.tx_bytes = 0;
	skt->stats.rx_bytes = 0;
	skt->stats.tx_trans = 0;
	skt->stats.rx_trans = 0;
}; /* rskt_clear_skt() */

struct rsvp_li {
	sem_t resp_rx;
	struct librskt_rsktd_to_app_msg *resp; 
};

int lib_uninit(void);

int librskt_wait_for_sem(sem_t *sema, int err_code)
{
	int rc;

	do {
		rc = sem_wait(sema);
	} while (rc && (EINTR == errno) && !lib.all_must_die);

	if (rc) {
		ERR("Failed in sem_wait()\n");
		lib.all_must_die = err_code;
	}
	return rc;
};

struct librskt_app_to_rsktd_msg *alloc_app2d(void)
{
	struct librskt_app_to_rsktd_msg *ptr;
	assert(A2RSKTD_SZ >= sizeof(struct librskt_app_to_rsktd_msg)); 
	ptr = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	memset(ptr, 0, A2RSKTD_SZ);
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
	ptr = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);
	memset(ptr, 0, RSKTD2A_SZ);
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
		malloc(sizeof(struct rsvp_li));
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

int librskt_dmsg_req_resp(struct librskt_app_to_rsktd_msg *tx, 
			struct librskt_rsktd_to_app_msg *rx)
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
	DBG("Waiting for lib.rsvp_mtx\n");
	if (librskt_wait_for_sem(&lib.rsvp_mtx, 0x1001)) {
		ERR("Failed on rspv_mtx\n");
		goto fail;
	}
	seq_num = lib.lib_req_seq++;
	tx->a_rq.app_seq_num = htonl(seq_num);
	rx->a_rsp.req.app_seq_num = tx->a_rq.app_seq_num;
	li = l_add(&lib.rsvp, seq_num, (void *)rsvp);
	sem_post(&lib.rsvp_mtx);

	DBG("Waiting for lib.msg_tx_mtx\n");
	if (librskt_wait_for_sem(&lib.msg_tx_mtx, 0x1002)) {
		ERR("Failed on msg_tx_mtx\n");
		goto fail;
	}
	l_push_tail(&lib.msg_tx, (void *)tx); 
	sem_post(&lib.msg_tx_mtx);
	sem_post(&lib.msg_tx_cnt);

	DBG("Waiting for rsvp->resp_rx\n");
	if (librskt_wait_for_sem(&rsvp->resp_rx, 0x1003)) {
		ERR("Failed on resp_rx\n");
		goto fail;
	}
	DBG("NOT Waiting for rsvp->resp_rx\n");
	if (rx->a_rsp.err) {
		li = NULL;
		rc = -1;
		errno = ntohl(rx->a_rsp.err);
		ERR("a_rsp.err is not 0: %s\n", strerror(errno));
		goto fail;
	};

	if (free_rsvp(rsvp))
		ERR("ERR freeing rsvp");
	DBG("Returning!\n");
	return rc;
fail:
	if (NULL != li) {
		int saved_errno = errno;

		librskt_wait_for_sem(&lib.rsvp_mtx, 0x1004);
		DBG("Calling l_lremove()\n");
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
		ERR("Failed on msg_tx_mtx\n");
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

	DBG("ENTER\n");

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

		DBG("App Sending Type 0d%d 0x%x swap 0d%d 0x%x",
			tx->msg_type, tx->msg_type,
			htonl(tx->msg_type), htonl(tx->msg_type));
		rc = send(lib.fd, (void *)tx, A2RSKTD_SZ, MSG_EOR);
		if (rc < 0) {
			lib.all_must_die = 3;
		}
		// free(tx);
	};
exit:
	CRIT("EXIT\n");
	pthread_exit(unused);
};

/* RX Thread to receive requests/responses and avoid blocking */
		
void rsvp_loop_resp(struct librskt_rsktd_to_app_msg *rxd)
{
	struct l_item_t *li;
	struct rsvp_li *dlyd;

	DBG("ENTER\n");
	if (librskt_wait_for_sem(&lib.rsvp_mtx, 0x1030)) {
		WARN("lib.all_must_die = 20");
		lib.all_must_die = 20;
		goto exit;
	};
	dlyd = (struct rsvp_li *)
		l_find(&lib.rsvp, ntohl(rxd->a_rsp.req.app_seq_num), &li);
	if (NULL != dlyd) {
		memcpy((void *)&dlyd->resp->a_rsp.msg, (void *)&rxd->a_rsp.msg, 
					sizeof(union librskt_resp_u));
		dlyd->resp->a_rsp.err = rxd->a_rsp.err;
		sem_post(&dlyd->resp_rx);
		DBG("Calling l_lremove\n");
		l_lremove(&lib.rsvp, li);
	} else {
		DBG("dlyd is NULL\n");
	}
exit:
	sem_post(&lib.rsvp_mtx);
};

void rsvp_loop_req(struct librskt_rsktd_to_app_msg *rxd)
{
	if (htonl(LIBRSKT_CLOSE_CMD) == rxd->msg_type) {
		if (librskt_wait_for_sem(&lib.req_mtx, 0x1040)) {
			WARN("lib.all_must_die = 30");
			lib.all_must_die = 30;
			sem_post(&lib.req_mtx);
			return;
		};
		l_push_tail(&lib.req, (void *)rxd);
		sem_post(&lib.req_mtx);
		sem_post(&lib.req_cnt);
		return;
	};
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
		CRIT("Failed to allocate librskt_rsktd_to_app_msg\n");
		return NULL;
	}

	sigaction(SIGUSR1, &rsvp_loop_sig, NULL);

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "LIBRSKT_RSVP");
        pthread_setname_np(lib.rsvp_thr, my_name);

	DBG("ENTER\n");
	while (!lib.all_must_die) {
		memset((void *)rxd, 0, RSKTD2A_SZ);
		rc = recv(lib.fd, (void *)rxd, RSKTD2A_SZ, 0);
		if (rc <= 0) {
			ERR("Failed in recv()\n");
			lib.all_must_die = 10;
		};
		if (lib.all_must_die)
			goto exit;
		if (rxd->msg_type & htonl(LIBRSKTD_RESP | LIBRSKTD_FAIL)) {
			rsvp_loop_resp(rxd);
		} else {
			rsvp_loop_req(rxd);
			rxd = alloc_d2app();
			if (rxd == NULL) {
				CRIT("Failed to allocate librskt_rsktd_to_app_msg\n");
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

	CRIT("EXIT\n");
	pthread_exit(unused);
};

void prep_response(struct librskt_rsktd_to_app_msg *req, 
		struct librskt_app_to_rsktd_msg *resp)
{
	resp->msg_type = req->msg_type | htonl(LIBRSKTD_RESP);
	resp->rsp_a.err = 0;
	resp->rsp_a.req_a = req->rq_a;
};

void lib_rem_skt_from_list(rskt_h skt_h, volatile struct rskt_socket_t *skt);

void cleanup_skt(rskt_h skt_h, volatile struct rskt_socket_t *skt)
{
	DBG("ENTER with skt->connector = %d\n", skt->connector);
	if (skt_rmda_uninit != skt->connector) { 
		DBG("skt->connector != skt_rdma_uninit\n");
		if (skt->msub_p != NULL)  {
			DBG("Unmapping skt->msub_p(%p)\n", skt->msub_p);
			rdma_munmap_msub(skt->msubh, (void *)skt->msub_p);
			skt->msub_p = NULL;
			skt->rx_buf = NULL;
			skt->tx_buf = NULL;
			skt->con_sz = 0;
			skt->msub_sz = 0;
		} else {
			DBG("skt->msub_p is NULL\n");
		}
		if (skt_rdma_connector == skt->connector) {
			DBG("skt->connector == skt_rdma_connector\n");
			rdma_disc_ms_h(skt->con_msh, skt->msubh);
			skt->msubh_valid = 0;
			skt->msh_valid = 0;
		} else {
			DBG("skt->connector != skt_rdma_connector\n");
			if (skt->msubh_valid) {
				DBG("skt->msubh_valid is true. Closing skt->msubh\n");
				rdma_destroy_msub_h(skt->msh, skt->msubh);
				skt->msubh_valid = 0;
			} else {
				WARN("skt->msubh_valid is false\n");
			}
			if (skt->msh_valid) {
				DBG("skt->msh_valid is true. Closing skt->msh\n");
				rdma_close_ms_h(skt->msoh, skt->msh);
				skt->msh_valid = 0;
			} else {
				WARN("skt->msh_valid is false\n");
			}
		}
		if (skt->msoh_valid) {
			DBG("skt->msoh_valid is true. Closing msoh\n");
			rdma_close_mso_h(skt->msoh);
			skt->msoh_valid = 0;
		} else {
			WARN("skt->msoh_valid is false\n");
		}
	} else {
		DBG("skt->connector == skt_rmda_uninit\n");
	}
	lib_rem_skt_from_list(skt_h, skt);
	free((void *)skt);
	skt_h->skt = NULL;
};

void lib_handle_dmn_close_req(rskt_h skt_h)
{
	volatile struct rskt_socket_t *skt;

	DBG("ENTER\n");
	if (NULL == skt_h) {
		WARN("skt_h is NULL...returning\n");
		return;
	}

 	librskt_wait_for_sem(&skt_h->mtx, 0);

	skt = skt_h->skt;
	if (skt == NULL) {
 		sem_post(&skt_h->mtx);
		return;
	}
	DBG("skt->sa.sn = %d skt->sai.sn = %d\n", skt->sa.sn, skt->sai.sa.sn);
	cleanup_skt(skt_h, skt);

	sem_post(&skt_h->mtx);
	DBG("EXIT\n");
};

/* Request Processing Thread */
void *req_loop(void *unused)
{
	struct librskt_rsktd_to_app_msg *req;
	struct librskt_app_to_rsktd_msg *resp = NULL;
	struct l_item_t *li;
	rskt_h skt_h;
	char my_name[16];

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "LIBRSKT_REQ");
        pthread_setname_np(lib.req_thr, my_name);

	while (!lib.all_must_die) {
	       
		if (NULL == resp) {
			resp = alloc_app2d();

			if (resp == NULL) {
				ERR("Failed to allocate 'resp'\n");
				return NULL;
			}
		}

		if (librskt_wait_for_sem(&lib.req_cnt, 0x1060)) {
			ERR("librskt_wait_for_sem() failed. Exiting\n");
			goto exit;
		}
		if (lib.all_must_die)
			goto exit;
		if (librskt_wait_for_sem(&lib.req_mtx, 0x1061)) {
			ERR("librskt_wait_for_sem() failed. Exiting\n");
			goto exit;
		}
		if (lib.all_must_die)
			goto exit;
		req = (struct librskt_rsktd_to_app_msg *)l_pop_head(&lib.req);
		sem_post(&lib.req_mtx);

		if (NULL == req)
			continue;

		prep_response(req, resp);

		switch (ntohl(req->msg_type)) {
		case LIBRSKT_CLOSE_CMD:
			/* Got this request from the RSKTD.
			* Send response when are sure that app can't use this
			* socket any more.
			*/
			INFO("Received LIBRSKT_CLOSE_CMD from RSKTD\n");
			if (librskt_wait_for_sem(&lib.skts_mtx, 0x1062)) {
				ERR("librskt_wait_for_sem() failed. Exiting\n");
				goto exit;
			}
			skt_h = (rskt_h)l_find(&lib.skts,
					ntohl(req->rq_a.msg.clos.sn), &li);
			sem_post(&lib.skts_mtx);

			if (NULL == skt_h) {
				WARN("skt_h is NULL\n");
				break;
			}
			lib_handle_dmn_close_req(skt_h);
			break;
		default: resp->msg_type |= htonl(LIBRSKTD_FAIL);
		};
		if (librskt_dmsg_tx_resp(resp)) {
			ERR("librskt_dmsg_tx_resp failed. Exiting\n");
			goto exit;
		}
		resp = NULL;
	};
exit:
	CRIT("EXIT\n");
	pthread_exit(unused);
};

int librskt_init(int rsktd_port, int rsktd_mpnum)
{
	struct librskt_app_to_rsktd_msg *req;
	struct librskt_rsktd_to_app_msg *resp;

	/* If library already running successfully, just return */
	if ((lib.portno == rsktd_port) && (lib.mpnum == rsktd_mpnum) &&
			(lib.init_ok == rsktd_port))
		return 0;

	/* Not connected to intended port/mport, so fail */
	if ((lib.portno == lib.init_ok) && (lib.portno) &&
		((lib.portno != rsktd_port) || (lib.mpnum != rsktd_mpnum)))
		return 1;

	DBG("ENTER\n");
	lib.fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == lib.fd) {
		ERR("ERROR on librskt_init socket(): %s\n", strerror(errno));
		goto fail;
	};
	DBG("lib.fd = %d, rsktd_port = %d\n", lib.fd, rsktd_port);

	lib.addr_sz = sizeof(struct sockaddr_un);
	memset(&lib.addr, 0, lib.addr_sz);

	lib.portno = rsktd_port;
	lib.mpnum = rsktd_mpnum;
	lib.init_ok = 0;
	lib.ct = -1;

	lib.addr.sun_family = AF_UNIX;
	snprintf(lib.addr.sun_path, sizeof(lib.addr.sun_path) - 1,
		LIBRSKTD_SKT_FMT, rsktd_port, rsktd_mpnum);
	DBG("Attempting to connect to RSKTD via Unix sockets\n");
	if (connect(lib.fd, (struct sockaddr *) &lib.addr, 
				lib.addr_sz)) {
		ERR("ERROR on librskt_init connect: %s\n", strerror(errno));
		goto fail;
	};
	DBG("CONNECTED to RSKTD\n");

	lib.all_must_die = 0;

	sem_init(&lib.msg_tx_mtx, 0, 1);
	sem_init(&lib.msg_tx_cnt, 0, 0);
	l_init(&lib.msg_tx);

	sem_init(&lib.rsvp_mtx, 0, 1);
	l_init(&lib.rsvp);
	lib.lib_req_seq = 0;

	sem_init(&lib.req_mtx, 0, 1);
	sem_init(&lib.req_cnt, 0, 0);
	l_init(&lib.req);

	lib.test = 0;
	sem_init(&lib.skts_mtx, 0, 1);

	l_init(&lib.skts);

	/* Startup the threads */
	DBG("Starting tx_loop\n");
	if (pthread_create( &lib.tx_thr, NULL, tx_loop, NULL)) {
		lib.all_must_die = 1;
		CRIT("ERROR:librskt_init, tx_loop thread: %s\n", strerror(errno));
		goto fail;
	};
	DBG("Starting rsvp_loop\n");
	if (pthread_create( &lib.rsvp_thr, NULL, rsvp_loop, NULL)) {
		lib.all_must_die = 2;
		CRIT("ERROR:librskt_init rsvp_loop thread: %s\n", strerror(errno));
		goto fail;
	};
	DBG("Starting req_loop\n");
	if (pthread_create( &lib.req_thr, NULL, req_loop, NULL)) {
		lib.all_must_die = 3;
		CRIT("ERROR:librskt_init, req_loop thread: %s\n", strerror(errno));
		goto fail;
	};
	/* Socket appears to be open, say hello to RSKTD */
	req = alloc_app2d();
	if (req == NULL) {
		CRIT("Failed to malloc 'req'\n");
		goto fail;
	}

	resp = alloc_d2app();
	if (resp == NULL) {
		CRIT("Failed to malloc 'resp'\n");
		free(req);
		goto fail;
	}

	memset(req, 0, A2RSKTD_SZ);
	memset(resp, 0, RSKTD2A_SZ);
	req->msg_type = LIBRSKTD_HELLO;
	req->a_rq.msg.hello.proc_num = htonl(getpid());
	memset(req->a_rq.msg.hello.app_name, 0, MAX_APP_NAME);
	snprintf(req->a_rq.msg.hello.app_name, MAX_APP_NAME-1, "%d", getpid());
	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_HELLO (A2RSKTD_SZ)\n");
	if (librskt_dmsg_req_resp(req, resp)) {
		ERR("ERROR on LIBRSKTD_HELLO\n");
		free(resp);
		goto fail;
	};

	lib.init_ok = rsktd_port;
	lib.ct = ntohl(resp->a_rsp.msg.hello.ct);

	free(resp);
fail:
	int rc = -!((lib.init_ok == lib.portno) && (lib.portno));
	DBG("EXIT, rc = %d\n", rc);
	return rc;
};

void librskt_finish(void)
{	
	INFO("ENTRY\n");
	struct rsvp_li *delayed;

	lib.all_must_die = 1;
	if ((lib.init_ok == lib.portno) && lib.portno) {
		DBG("Joining librskt threads\n");
		/* Kill the receiver thread first */
		/* Allow req_loop to terminate */
		sem_post(&lib.req_cnt);
		pthread_join(lib.req_thr, NULL);
		DBG("Joined req_thr\n");

		/* Then close the rsvp thread, add responses to TX loop */
		DBG("Closing socket handle\n");
		pthread_kill(lib.rsvp_thr, SIGUSR1);
		pthread_join(lib.rsvp_thr, NULL);
		DBG("Joined rsvp_thr\n");
	
		/* Lastly close tx loop */
		sem_post(&lib.msg_tx_cnt);
		pthread_join(lib.tx_thr, NULL);
		DBG("Joined tx_thr\n");
	
		lib.init_ok = 0;
	};

	if (lib.fd) {
		close(lib.fd);
		lib.fd = 0;
	};
	
	delayed = (struct rsvp_li *)l_pop_head(&lib.rsvp);

	while (NULL != delayed) {
		delayed->resp->a_rsp.err = ECONNRESET;
		sem_post(&delayed->resp_rx);
		delayed = (struct rsvp_li *)l_pop_head(&lib.rsvp);
	};
	INFO("EXIT\n");
};

int lib_uninit(void)
{
	int rc = !((lib.init_ok == lib.portno) && (lib.portno));

	if (rc) {
		ERR("lib.init_ok = %d, lib.portno = %d\n",
				lib.init_ok, lib.portno);
		errno = EHOSTDOWN;
	} else {
		errno = 0;
	}

	return rc;
};

int rskt_alloc_skt(rskt_h skt_h)
{
	int rc = 1;

       	skt_h->skt = (struct rskt_socket_t *)
		malloc(sizeof(struct rskt_socket_t));
	if (NULL == skt_h->skt) {
		CRIT("Failed to malloc skt_h->skt\n");
		goto fail;
	};

	rskt_clear_skt(skt_h->skt);
	skt_h->skt->st = rskt_alloced;
	rc = 0;
	errno = 0;
fail:
	return rc;
};

rskt_h rskt_create_socket(void) 
{
	rskt_h skt_h = (rskt_h)malloc(sizeof(struct rskt_handle_t));

	if (NULL == skt_h) {
		ERR("Failed to malloc skt_h\n: %s\n", strerror(errno));
		goto fail;
	}

	if (lib_uninit()) {
		ERR("Failed in lib_uninit()\n");
		goto fail;
	}

	sem_init(&skt_h->mtx, 0, 1);
	skt_h->skt = NULL;

	if (rskt_alloc_skt(skt_h)) {
		ERR("Failed in rskt_alloc_skt\n");
		free(skt_h);
		skt_h = NULL;
	};
	return skt_h;
fail:
	if (NULL != skt_h)
		free(skt_h);
	skt_h = NULL;
	errno = ENOMEM;
	return skt_h;
};

void rskt_destroy_socket(rskt_h *skt_h) 
{
	if ((NULL != skt_h) && (NULL != *skt_h)) {
		if (NULL != (*skt_h)->skt)
			rskt_close(*skt_h);
		free(*skt_h);
		*skt_h = NULL;
	} else {
		WARN("NULL parameter\n");
	}
};

void lib_add_skt_to_list(rskt_h skt_h)
{
	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1070)) {
		ERR("Failed in librskt_wait_for_sem\n");
		return;
	}

	l_add(&lib.skts, skt_h->skt->sa.sn, (void *)skt_h);
	sem_post(&lib.skts_mtx);
};

void lib_rem_skt_from_list(rskt_h skt_h, volatile struct rskt_socket_t *skt)
{
	struct l_item_t *li;
	rskt_h l_skt;

	DBG("ENTER\n");
	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1080)) {
		ERR("Failed in librskt_wait_for_sem\n");
		return;
	}

	l_skt = (rskt_h)l_find(&lib.skts, skt->sa.sn, &li);
	if (skt_h == l_skt) {
		DBG("Calling l_lremove()\n");
		l_lremove(&lib.skts, li); /* Do not deallocate socket */
	}
	DBG("Posting lib.skts_mtx\n");
	sem_post(&lib.skts_mtx);
};

int rskt_bind(rskt_h skt_h, struct rskt_sockaddr *sock_addr)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;

	volatile struct rskt_socket_t *skt;

	if (lib_uninit()) {
		ERR("Fail due to lib_uninit()\n");
		goto exit;
	}

	errno = EINVAL;
	if ((NULL == skt_h) || (NULL == sock_addr)) {
		ERR("NULL parameter\n");
		goto exit;
	}

	if (librskt_wait_for_sem(&skt_h->mtx, 0)) {
		ERR("rskt_alloc_skt failed\n");
		goto close;
	};

	if (NULL == skt_h->skt) {
		if (rskt_alloc_skt(skt_h)) {
			ERR("rskt_alloc_skt failed\n");
			goto close;
		}
	};

	skt = skt_h->skt;

	if (rskt_alloced != skt->st) {
		ERR("skt->st != rskt_alloced\n");
		errno = EBADFD;
		goto close;
	};

	skt->sa.sn = sock_addr->sn;
	skt->sa.ct = lib.ct;
	skt->sai.sa.ct = sock_addr->ct;

	tx = alloc_app2d();
	rx = alloc_d2app();

	tx->msg_type = LIBRSKTD_BIND;
	tx->a_rq.msg.bind.sn = htonl(sock_addr->sn);

	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_BIND (A2RSKTD_SZ)\n");
	if (librskt_dmsg_req_resp(tx, rx)) {
		ERR("librskt_dmsg_req_resp() failed\n");
		goto close;
	}

	if (rx->a_rsp.err) {
		errno = EADDRNOTAVAIL;
		ERR("%s\n", strerror(errno));
	} else {
		skt->st = rskt_bound;
		lib_add_skt_to_list(skt_h);
		errno = 0;
	};
close:
	sem_post(&skt_h->mtx);
exit:
	if (NULL != rx)
		free(rx);
	return -errno;
};

int rskt_listen(rskt_h skt_h, int max_backlog)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	volatile struct rskt_socket_t *skt;

	if (lib_uninit()) {
		ERR("Failed in lib_uninit()\n");
		goto exit;
	}

	errno = EINVAL;
	if (NULL == skt_h) {
		ERR("skt_h is NULL\n");
		goto exit;
	}
	if (librskt_wait_for_sem(&skt_h->mtx, 0)) {
		ERR("rskt_alloc_skt failed\n");
		goto close;
	};

	skt = skt_h->skt;
	if (NULL == skt) {
		ERR("skt is NULL\n");
		goto close;
	}

	if (rskt_bound != skt->st) {
		errno = EBADFD;
		ERR("%s\n", strerror(errno));
		goto close;
	};

	skt->max_backlog = max_backlog;

	tx = alloc_app2d();
	rx = alloc_d2app();

	tx->msg_type = LIBRSKTD_LISTEN;
	tx->a_rq.msg.listen.sn = htonl(skt->sa.sn);
	tx->a_rq.msg.listen.max_bklog = htonl(skt->max_backlog);
	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_LISTEN (A2RSKTD_SZ)\n");
	if (librskt_dmsg_req_resp(tx, rx)) {
		ERR("librskt_dmsg_req_resp failed\n");
		goto close;
	}

	if (rx->a_rsp.err) {
		errno = EBUSY;
		ERR("%s\n", strerror(errno));
	} else {
		errno = 0;
		skt->st = rskt_listening;
	}

close:
	sem_post(&skt_h->mtx);
exit:
	if (NULL != rx)
		free(rx);
	return -errno;
};

int update_remote_hdr(struct rskt_socket_t *skt,
			struct rdma_xfer_ms_in *hdr_in);

int setup_skt_ptrs(struct rskt_socket_t *volatile skt)
{
	struct rdma_xfer_ms_in hdr_in;
	int    rc = 0;

	DBG("ENTER\n");
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

	DBG("Set ZEROED and INIT_DONE\n");
	DBG("skt->buf_sz=0x%X, loc_tx_wr_ptr=0x%X, loc_rx_rd_ptr=0x%X\n",
						skt->buf_sz,
						ntohl(skt->hdr->loc_tx_wr_ptr),
						ntohl(skt->hdr->loc_rx_rd_ptr));
	hdr_in.loc_msubh = skt->msubh;
	hdr_in.rem_msubh = skt->con_msubh;
	hdr_in.priority = 0;
	hdr_in.sync_type = rdma_sync_chk;
	rc = update_remote_hdr(skt, &hdr_in);
	if (rc) {
		ERR("Failed to update remote header, rc = %d\n", rc);
		goto exit_setup_skt_ptrs;
	}

	/**
	 * Poll for INIT_DONE, ZEROED, and !INIT in the remote header
	 */
	/* COND1: Both INIT_DONE and ZEROED are set but INIT is cleared (A,B,!C) */
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

	DBG("skt->hdr->rem_rx_wr_flags = 0x%08X, skt->hdr->rem_tx_rd_flags\n",
			ntohl(skt->hdr->rem_rx_wr_flags), ntohl(skt->hdr->rem_tx_rd_flags));
	DBG("Waiting for INIT_DONE and ZEROED or for ZEROED only\n");
	while (!COND1 && !COND2) {
		usleep(10);
	}
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
	DBG("Cleared INIT_DONE\n");
	rc = update_remote_hdr(skt, &hdr_in);
	if (rc) {
		ERR("Failed to update remote header, rc = %d\n", rc);
		goto exit_setup_skt_ptrs;
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

	while (!COND1 && !COND2) {
		usleep(10);
	}
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
	DBG("Cleared ZEROED and Set INIT\n");
	rc = update_remote_hdr(skt, &hdr_in);
	if (rc) {
		ERR("Failed to update remote header, rc = %d\n", rc);
		goto exit_setup_skt_ptrs;
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
	while (!COND1) {
		usleep(10);
	}
#undef COND1

exit_setup_skt_ptrs:
	DBG("EXIT\n");
	return rc;
}; /* setup_skt_ptrs() */

int rskt_accept(rskt_h l_skt_h, rskt_h skt_h, 
		struct rskt_sockaddr *sktaddr)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	struct rskt_socket_t *l_skt, *skt;
	int rc = -1;

	if (lib_uninit()) {
		CRIT("lib_uninit() failed..exiting\n");
		goto exit;
	}
	errno = EINVAL;
	if ((NULL == l_skt_h) || (NULL == skt_h) || (NULL == sktaddr)) {
		CRIT("NULL parameter passed: l_skt_h=%p, skt_h=%p, sktaddr=%p\n",
				l_skt_h, skt_h, sktaddr);
		goto exit;
	}

	if (librskt_wait_for_sem(&l_skt_h->mtx, 0x6335)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto exit;
	}

	l_skt = (struct rskt_socket_t *)l_skt_h->skt;
	if (NULL == l_skt) {
		ERR("l_skt_h->skt is NULL\n");
		goto close1;
	}

	if (librskt_wait_for_sem(&skt_h->mtx, 0x6337)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto close1;
	}

	if (NULL == skt_h->skt) {
		INFO("skt_h->skt is NULL\n");
		if (rskt_alloc_skt(skt_h)) {
			CRIT("Failed to allocated skt_h..exiting\n");
			goto close2;
		}
	}
	skt = skt_h->skt;

	if (rskt_listening != l_skt->st) {
		ERR("rskt_listening != l_skt->st..exiting\n");
		goto close2;
	}
	if (rskt_alloced != skt->st) {
		ERR("rskt_alloced != skt->st, st is %d\n", skt->st);
		goto close2;
	}

	tx = alloc_app2d();
	rx = alloc_d2app();

	tx->msg_type = LIBRSKTD_ACCEPT;
	tx->a_rq.msg.accept.sn = htonl(l_skt->sa.sn);
	
	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1090)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto close2;
	}
	l_skt->st = rskt_accepting;
	sem_post(&lib.skts_mtx);

	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_ACCEPT (A2RSKTD_SZ)\n");
	if (librskt_dmsg_req_resp(tx, rx)) {
		WARN("librskt_dmsg_req_resp() failed..closing\n");
		goto close2;
	}

	memcpy((void *)skt, (void *)l_skt, sizeof(struct rskt_socket_t));
	skt->max_backlog = 0;
	skt->st = rskt_connecting;
	skt->connector = skt_rdma_acceptor;
	skt->sa.sn = ntohl(rx->a_rsp.msg.accept.new_sn);
	skt->sa.ct = ntohl(rx->a_rsp.msg.accept.new_ct);
	skt->sai.sa.ct = ntohl(rx->a_rsp.msg.accept.peer_sa.ct);
	skt->sai.sa.sn = ntohl(rx->a_rsp.msg.accept.peer_sa.sn);
	memcpy((void *)skt->msoh_name, rx->a_rsp.msg.accept.mso_name, MAX_MS_NAME);
	memcpy((void *)skt->msh_name, rx->a_rsp.msg.accept.ms_name, MAX_MS_NAME);
	DBG("ACCEPT: SN %d CT %d REM SN %d CT %d MSOH \"%s\" MSH \"%s\"", 
		skt->sa.sn, skt->sa.ct, skt->sai.sa.sn, skt->sai.sa.ct,
		skt->msoh_name, skt->msh_name);
	skt->msub_sz = ntohl(rx->a_rsp.msg.accept.ms_size);

	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1091)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto exit;
	}
	l_skt->st = rskt_listening;
	sem_post(&lib.skts_mtx);

	rc = rdma_open_mso_h((const char *)skt->msoh_name, (mso_h *)&skt->msoh);
	if (rc) {
		if (rc == RDMA_ALREADY_OPEN) {
			INFO("MSO was already open, got back the same handle\n");
		} else {
			ERR("Failed to open ms(%s)\n", skt->msh_name);
			goto close2;
		}
	}
	skt->msoh_valid = 1;

	rc = rdma_open_ms_h((const char *)skt->msh_name, skt->msoh, 0, 
			&skt->msub_sz, (ms_h *)&skt->msh);
	if (rc) {
		ERR("Failed to open ms(%s)\n", skt->msh_name);
		goto close2;
	}
	skt->msh_valid = 1;

	rc = rdma_create_msub_h(skt->msh, 0,
				skt->msub_sz, 0, (msub_h *)&skt->msubh);
	if (rc) {
		ERR("Failed to create msub\n");
		goto close2;
	}
	skt->msub_p = NULL;
	skt->msubh_valid = 1;

	rc = rdma_mmap_msub(skt->msubh, (void **)&skt->msub_p);
	if (rc) {
		ERR("Failed to mmap msub\n");
		goto close2;
	}

	DBG("ACCEPT: MSOH %p MSH %p MSUBH %p PTR %p",
		skt->msoh, skt->msh, skt->msubh, skt->msub_p);
	/* Zero the entire msub (we can do that because we'll initialize
	 * all pointers below).
	 */
	memset((void *)skt->msub_p, 0, skt->msub_sz);

	do {
		rc = rdma_accept_ms_h(skt->msh, skt->msubh, 
				(msub_h *)&skt->con_msubh,
				(uint32_t *)&skt->con_sz, RDMA_CONN_ACC_TO_SECS);
	} while (-EINTR == rc);	/* FIXME: should it be ETIME? */
	if (rc) {
		ERR("Failed in rdma_accept_ms_h()\n");
		goto close2;
	}

	skt->st = rskt_connected;
	rc = setup_skt_ptrs(skt);
	if (rc) {
		ERR("Failed in setup_skt_ptrs\n");
		goto close2;
	}
	sktaddr->sn = skt_h->skt->sa.sn;
	sktaddr->ct = skt_h->skt->sa.ct;
	lib_add_skt_to_list(skt_h);
	sem_post(&l_skt_h->mtx);
	sem_post(&skt_h->mtx);
	free(rx);
	INFO("Exiting with SUCCESS\n");
	return 0;

close2:
	sem_post(&skt_h->mtx);
close1:
	sem_post(&l_skt_h->mtx);
	ERR("FAILED: closing socket\n");
	rskt_close(skt_h);
exit:
	if (NULL != rx)
		free(rx);
	return rc;
}; /* rskt_accept() */

int rskt_connect(rskt_h skt_h, struct rskt_sockaddr *sock_addr )
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	struct rskt_socket_t *skt;
	int temp_errno;
	int rc = -1;
	int conn_retries = 5;

	DBG("ENTER\n");
	if (lib_uninit()) {
		CRIT("lib_uninit() failed..exiting\n");
		goto exit;
	}
	if ((NULL == skt_h) || (NULL == sock_addr)) {
		CRIT("NULL parameter: skt_h = %p, sock_addr = %p\n",
			skt_h, sock_addr);
		goto exit;
	}

	rc = librskt_wait_for_sem(&skt_h->mtx, 0);
	if (rc) {
		ERR("librskt_wait_for_sem failed...exiting\n");
		goto close;
	}

	if (NULL == skt_h->skt) {
		WARN("skt_h->skt is NULL\n");
		if (rskt_alloc_skt(skt_h)) {
			CRIT("Failed to alloc skt_h\n");
			goto close;
		}
	}
	skt = skt_h->skt;

	if ((rskt_uninit == skt->st) ||
	(rskt_bound == skt->st) ||
	(rskt_listening == skt->st) ||
	(rskt_accepting == skt->st) ||
	(rskt_connecting == skt->st) ||
	(rskt_connected == skt->st) ||
	(rskt_shutting_down == skt->st) ||
	(rskt_closing == skt->st)) {
		ERR("Condition failed..exiting\n");
		goto close;
	}

	tx = alloc_app2d();
	rx = alloc_d2app();

	tx->msg_type = LIBRSKTD_CONN;
	tx->a_rq.msg.conn.sn = htonl(sock_addr->sn);
	tx->a_rq.msg.conn.ct = htonl(sock_addr->ct);

	/* Response indicates what mso, ms, and msub to use, and 
	 * what ms to rdma_connect with
	 */
	DBG("Calling librskt_dmsg_req_resp to send LIBRSKTD_CONN (A2RSKTD_SZ)\n");
	if (lib.all_must_die)
		goto close;

	rc = librskt_dmsg_req_resp(tx, rx);
	if (rc) {
		ERR("librskt_dmsg_req_resp() failed..closing\n");
		goto close;
	}

	DBG("Received reply to LIBRSKTD_CONN containing:\n");
	DBG("mso = %s, ms = %s, msub_sz = %d\n",
			rx->a_rsp.msg.conn.mso,
			rx->a_rsp.msg.conn.ms,
			rx->a_rsp.msg.conn.msub_sz)
	skt->st = rskt_connecting;
	skt->connector = skt_rdma_connector;
	skt->sa.ct = ntohl(rx->a_rsp.msg.conn.new_ct);
	skt->sa.sn = ntohl(rx->a_rsp.msg.conn.new_sn);
	skt->sai.sa.sn = ntohl(rx->a_rsp.msg.conn.rem_sn);
	skt->sai.sa.ct = ntohl(rx->a_rsp.req.msg.conn.ct);
	memcpy(skt->msoh_name, rx->a_rsp.msg.conn.mso, MAX_MS_NAME);
	memcpy(skt->msh_name, rx->a_rsp.msg.conn.ms, MAX_MS_NAME);
	memcpy(skt->con_msh_name, rx->a_rsp.msg.conn.rem_ms, MAX_MS_NAME);
	skt->msub_sz = ntohl(rx->a_rsp.msg.conn.msub_sz);
	skt->max_backlog = 0;

	if (lib.all_must_die)
		goto close;

	rc = rdma_open_mso_h(skt->msoh_name, &skt->msoh);
	if (rc) {
		ERR("rdma_open_mso_h() failed msoh_name(%s)..closing\n", skt->msoh_name);
		goto close;
	}
	skt->msoh_valid = 1;

	if (lib.all_must_die)
		goto close;

	rc = rdma_open_ms_h(skt->msh_name, skt->msoh, 0, 
			&skt->msub_sz, &skt->msh);
	if (rc || !skt->msub_sz) {
		ERR("rdma_open_ms_h() failed msh_name(%s)..closing\n", skt->msh_name);
		goto close;
	}
	skt->msh_valid = 1;

	if (lib.all_must_die)
		goto close;

	rc = rdma_create_msub_h(skt->msh, 0,
				skt->msub_sz, 0, &skt->msubh);
	if (rc) {
		ERR("rdma_create_msub() failed..closing\n");
		goto close;
	}
	skt->msubh_valid = 1;

	if (lib.all_must_die)
		goto close;

	rc = rdma_mmap_msub(skt->msubh, (void **)&skt->msub_p);
	if (rc) {
		ERR("rdma_mmap_msub() failed..closing\n");
		goto close;
	}

	/* Zero the entire msub (we can do that because we'll initialize
	 * all pointers below).
	 */
	memset((void *)skt->msub_p, 0, skt->msub_sz);

	if (lib.all_must_die)
		goto close;

	do {
		rc = rdma_conn_ms_h(16, skt->sai.sa.ct,
				skt->con_msh_name, skt->msubh, 
				&skt->con_msubh, &skt->con_sz,
				&skt->con_msh, RDMA_CONN_ACC_TO_SECS);
	} while (rc == RDMA_CONNECT_TIMEOUT && conn_retries-- && 
		!lib.all_must_die);

	if (rc) {
		ERR("rdma_conn_ms_h() failed, rc = %d..closing\n", rc);
		goto close;
	} else {
		HIGH("CONNECTED, skt->con_msh = %016" PRIx64 "\n", skt->con_msh);
	}

	skt->st = rskt_connected;
	if (lib.all_must_die)
		goto close;

	rc = setup_skt_ptrs(skt);
	if (rc) {
		ERR("Failed in setup_skt_ptrs\n");
		goto close;
	}
	sem_post(&skt_h->mtx);
	lib_add_skt_to_list(skt_h);
	INFO("Exiting with SUCCESS\n");
	return 0;
close:
	temp_errno = errno;
	sem_post(&skt_h->mtx);
	CRIT("FAILED: closing socket\n");
	rskt_close(skt_h);
	errno = temp_errno;
exit:
	if (rx != NULL)
		free(rx);
	return rc;
}; /* rskt_connect() */

const struct timespec rw_dly = {0, 5000};

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

	return free_bytes;
}; /* get_free_bytes() */

#define INC_PTR(x,y,z) x=htonl((ntohl(x)+y)%z)

// FIXME: Change to static inline
int send_bytes(rskt_h skt_h, void *data, int byte_cnt, 
			struct rdma_xfer_ms_in *hdr_in, int inited) {
	struct rdma_xfer_ms_out hdr_out;
	uint32_t dma_rd_offset, dma_wr_offset;
	struct rskt_socket_t *skt = skt_h->skt;

	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X\n",
		ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	dma_rd_offset = ntohl(skt->hdr->loc_tx_wr_ptr) + RSKT_TOT_HDR_SIZE;
	dma_wr_offset = dma_rd_offset + skt->buf_sz;
	DBG("dma_rd_offset = 0x%X, dma_wr_offset = 0x%X, byte_cnt = %u\n",
				dma_rd_offset, dma_wr_offset, byte_cnt);
	memcpy((void *)(skt->tx_buf + ntohl(skt->hdr->loc_tx_wr_ptr)),
		data, byte_cnt);
	INC_PTR(skt->hdr->loc_tx_wr_ptr, byte_cnt, skt->buf_sz);
	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X\n",
		ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	if (!inited) {
		DBG("!inited, assigning hdr values from skt\n");
		DBG("hdr_in->loc_msubh = %016" PRIx64 "\n", hdr_in->loc_msubh);
		DBG("hdr_in->rem_msubh = %016" PRIx64 "\n", hdr_in->rem_msubh);
		hdr_in->loc_msubh = skt->msubh;
		hdr_in->rem_msubh = skt->con_msubh;
		hdr_in->priority = 0;
		hdr_in->sync_type = rdma_sync_chk;
	};

	hdr_in->loc_offset = dma_rd_offset;
	hdr_in->num_bytes = byte_cnt;
	hdr_in->rem_offset = dma_wr_offset;

	DBG("Calling push_msub\n");
	if (rdma_push_msub(hdr_in, &hdr_out)) {
		skt->hdr->loc_tx_wr_flags |= htonl(RSKT_FLAG_ERROR);
		skt->hdr->loc_rx_rd_flags |= htonl(RSKT_FLAG_ERROR);
		ERR("Failed in rdma_push_msub()..exiting\n");
		return -1;
	};
	skt->stats.tx_bytes += byte_cnt;
	skt->stats.tx_trans++;
	DBG("EXIT, no errors\n");
	return 0;
}; /* send_bytes */

int update_remote_hdr(struct rskt_socket_t *skt, struct rdma_xfer_ms_in *hdr_in)
{
	struct rdma_xfer_ms_out hdr_out;
	int rc;

	/* NOTE: Assumes that hdr_in->loc-msubh, rem_msubh, 
	 * 	priority and sync_type have been filled in already!
	 */
	hdr_in->loc_offset = RSKT_LOC_TX_WR_PTR_OFFSET;
	hdr_in->rem_offset = RSKT_REM_RX_WR_PTR_OFFSET;
	hdr_in->num_bytes = RSKT_LOC_HDR_SIZE;
	DBG("loc_offset = 0x%X, rem_offset = 0x%X, num_bytes = %d\n",
		hdr_in->loc_offset, hdr_in->rem_offset, hdr_in->num_bytes);
	DBG("Calling rdma_push_msub\n");
	rc = rdma_push_msub(hdr_in, &hdr_out);
	if (rc) {
		ERR("Failed to push update to remote header\n");
		skt->hdr->loc_tx_wr_flags |= htonl(RSKT_FLAG_ERROR);
		skt->hdr->loc_rx_rd_flags |= htonl(RSKT_FLAG_ERROR);
	};

	return rc;
}; /* update_remote_hdr() */

int rskt_write(rskt_h skt_h, void *data, uint32_t byte_cnt)
{
	int rc = -1;
	uint32_t free_bytes = 0; 
	struct timespec unused;
	struct rdma_xfer_ms_in hdr_in;
	struct rskt_socket_t *skt;

	DBG("ENTER\n");

	errno = EINVAL;
	if ((NULL == skt_h) || (NULL == data) || (1 > byte_cnt)) {
		ERR("Invalid input parameter\n");
		goto skt_ok;
	}

	rc = librskt_wait_for_sem(&skt_h->mtx, 0);
	if (rc) {
		ERR("librskt_wait_for_sem failed...exiting\n");
		goto fail;
	}

	if (lib_uninit()) {
		ERR("lib_uninit() failed\n");
		goto skt_ok;
	}

	/* If skt_h->sk is NULL, the socket was closed while we were waiting
	 * on skt_h->mtx.
	 */
	skt = skt_h->skt;
	if (NULL == skt) {
		ERR("skt_h->skt is NULL\n");
		goto skt_ok;
	}

	DBG("skt = %p\n", skt);
	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X\n",
			ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	DBG("rem_tx_rd_ptr = 0x%X, rem_rx_wr_ptr = 0x%X\n",
			ntohl(skt->hdr->rem_tx_rd_ptr), ntohl(skt->hdr->rem_rx_wr_ptr));

	if (rskt_connected != skt->st) {
		ERR("skt->st is NOT skt_connected\n");
		goto skt_ok;
	}

	errno = 0;
	free_bytes = get_free_bytes(skt->hdr, skt->buf_sz);

	DBG("byte_cnt=0x%X, free_bytes=%x%X\n", byte_cnt, free_bytes);
	while (!free_bytes && !errno) {
		nanosleep(&rw_dly, &unused);
		free_bytes = get_free_bytes(skt->hdr, skt->buf_sz);
		sem_post(&skt_h->mtx);
		sleep(0);
		rc = librskt_wait_for_sem(&skt_h->mtx, 0);
		if (rc) {
			ERR("librskt_wait_for_sem failed...exiting\n");
			goto fail;
		}
		skt = skt_h->skt;
		if ((NULL == skt) || errno) {
			DBG("skt is NULL\n");
			errno = ECONNRESET;
			goto fail;
		}

		if (rskt_connected != skt->st) {
			WARN("Not connected");
			errno = ENOTCONN;
			goto skt_ok;
		};
	}

	if (errno) {
		WARN("Errno = %d", errno);
		errno = ENOTCONN;
		goto skt_ok;
	};
		
	DBG("byte_cnt=0x%X, free_bytes=%x%X\n", byte_cnt, free_bytes);

	if ((byte_cnt + ntohl(skt->hdr->loc_tx_wr_ptr)) 
						<= skt->buf_sz) {
		DBG("byte_cnt=0x%X (%d), loc_tx_wr_ptr = 0x%X, skt->buf_sz = 0x%X\n",
			byte_cnt, byte_cnt, skt->hdr->loc_tx_wr_ptr, skt->buf_sz);
		rc = send_bytes(skt_h, data, byte_cnt, &hdr_in, 0);
		if (rc) {
			ERR("send_bytes failed\n");
		}
	} else {
		uint32_t first_bytes = skt->buf_sz - 
					ntohl(skt->hdr->loc_tx_wr_ptr);

		DBG("first_bytes = %d - %d = %d\n",
			skt->buf_sz, ntohl(skt->hdr->loc_tx_wr_ptr), first_bytes);
		rc = send_bytes(skt_h, data, first_bytes, &hdr_in, 0);
		if (rc) {
			ERR("send_bytes failed..exiting\n");
			goto fail;
		}
		DBG("Now sending byte_cnt - first_bytes = %d\n",
							byte_cnt - first_bytes);
		rc = send_bytes(skt_h, (uint8_t *)data + first_bytes, 
				byte_cnt - first_bytes, &hdr_in, 1);
		if (rc) {
			ERR("send_bytes failed..exiting\n");
			goto fail;
		}
	};
	DBG("@@@@ Updating remote header\n");
	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X\n",
			ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	DBG("rem_tx_rd_ptr = 0x%X, rem_rx_wr_ptr = 0x%X\n",
			ntohl(skt->hdr->rem_tx_rd_ptr), ntohl(skt->hdr->rem_rx_wr_ptr));
	if (update_remote_hdr(skt, &hdr_in)) {
		ERR("updated_remote_hdr failed..exiting\n");
		goto fail;
	}
	sem_post(&skt_h->mtx);
	DBG("EXIT with success\n");
	return 0;
fail:
	sem_post(&skt_h->mtx);
	WARN("Closing skt_t due to failure condition\n");
	rskt_close(skt_h);
skt_ok:
	sem_post(&skt_h->mtx);
	return -1;
}; /* rskt_write() */

static inline
uint32_t get_avail_bytes(struct rskt_buf_hdr volatile *hdr,
					uint32_t buf_sz)
{
	uint32_t avail_bytes = 0;

	if (hdr == NULL) {
		return 0;
	}

	uint32_t rrw = ntohl(hdr->rem_rx_wr_ptr);
	uint32_t lrr = ntohl(hdr->loc_rx_rd_ptr);

	errno = 0;
	if (hdr->rem_rx_wr_flags & htonl(RSKT_FLAG_CLOS_CHK)) {
		errno = ECONNRESET;
		ERR("%s\n", strerror(errno));
		return 0;
	};
	
	if (!(hdr->rem_rx_wr_flags & htonl(RSKT_FLAG_INIT))) {
		/* Not an error; just means there are no bytes available */
		return 0;
	}

	avail_bytes = rrw - lrr - 1;
	if (rrw < lrr)
		avail_bytes = buf_sz - lrr + rrw - 1;

	return avail_bytes;
}; /* get_avail_bytes() */

void read_bytes(struct rskt_socket_t *skt, void *data, uint32_t byte_cnt)
{
	uint32_t first_offset = (ntohl(skt->hdr->loc_rx_rd_ptr) + 1) 
			% skt->buf_sz;
	memcpy(data, (void *)(skt->rx_buf + first_offset), byte_cnt);
	INC_PTR(skt->hdr->loc_rx_rd_ptr, byte_cnt, skt->buf_sz);
}; /* read_bytes() */

int rskt_read(rskt_h skt_h, void *data, uint32_t max_byte_cnt)
{
	uint32_t avail_bytes = 0;
	struct timespec unused;
	struct rdma_xfer_ms_in hdr_in;
	uint32_t first_offset;
	struct rskt_socket_t *skt;
	int	rc;

	DBG("ENTER\n");
	errno = EINVAL;
	if ((NULL == skt_h) || (NULL == data) || (1 > max_byte_cnt)) {
		ERR("Invalid input parameter\n");
		goto skt_ok;
	}

	if (lib_uninit()) {
		ERR("lib_uninit() failed\n");
		goto fail;
	}

	rc = librskt_wait_for_sem(&skt_h->mtx, 0);
	if (rc) {
		ERR("librskt_wait_for_sem failed...exiting\n");
		goto fail;
	}

	/* If skt_h->skt is NULL, the socket was closed while we were waiting
	 * for the semaphore. Just exit.
	 */
	skt = skt_h->skt;
	if (NULL == skt) {
		DBG("skt is NULL\n");
		errno = ECONNRESET;
		goto fail;
	}
	DBG("skt = %p\n", skt);

	if (rskt_connected != skt->st) {
		WARN("Not connected");
		errno = ENOTCONN;
		goto skt_ok;
	};

	errno = 0;
	DBG("avail_bytes = %d\n", avail_bytes);
	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X\n",
			ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	DBG("rem_tx_rd_ptr = 0x%X, rem_rx_wr_ptr = 0x%X\n",
			ntohl(skt->hdr->rem_tx_rd_ptr), ntohl(skt->hdr->rem_rx_wr_ptr));

	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);

	DBG("avail_bytes = %d\n", avail_bytes);
	DBG("loc_tx_wr_ptr = 0x%X, loc_rx_rd_ptr = 0x%X\n",
			ntohl(skt->hdr->loc_tx_wr_ptr), ntohl(skt->hdr->loc_rx_rd_ptr));
	DBG("rem_tx_rd_ptr = 0x%X, rem_rx_wr_ptr = 0x%X\n",
			ntohl(skt->hdr->rem_tx_rd_ptr), ntohl(skt->hdr->rem_rx_wr_ptr));

	/* Wait forever, until socket closed or we receive something */
	errno = 0;
	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);
	
	while (!avail_bytes && !errno) {
		nanosleep(&rw_dly, &unused);
	 	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);
		sem_post(&skt_h->mtx);
		sleep(0);
		rc = librskt_wait_for_sem(&skt_h->mtx, 0);
		if (rc) {
			ERR("librskt_wait_for_sem failed...exiting\n");
			goto fail;
		}
		skt = skt_h->skt;
		if (NULL == skt) {
			DBG("skt is NULL\n");
			errno = ECONNRESET;
			goto fail;
		}

		if (rskt_connected != skt->st) {
			WARN("Not connected");
			errno = ENOTCONN;
			goto skt_ok;
		};
	};

	DBG("avail_bytes = %d\n", avail_bytes);

	/* Check if the connection has dropped (errno set) */
	if (errno) {
		errno = ECONNRESET;
		ERR("Socket %d Err %s\n", skt->sa.sn, strerror(errno));
		goto fail;
	};

	if (avail_bytes > max_byte_cnt)
		avail_bytes = max_byte_cnt;
	DBG("avail_bytes = %d\n", avail_bytes);
	first_offset = (ntohl(skt->hdr->loc_rx_rd_ptr) + 1) % skt->buf_sz;
	DBG("first_offset = 0x%X\n", first_offset);
	if ((avail_bytes + first_offset) < skt->buf_sz) {
		DBG("1\n");
		read_bytes(skt, data, avail_bytes);
	} else {
		uint32_t first_bytes = skt->buf_sz - first_offset;
		DBG("2\n");
		read_bytes(skt, data, first_bytes);
		read_bytes(skt, (uint8_t *)data + first_bytes, 
				avail_bytes - first_bytes);
	};

	skt->stats.rx_bytes += avail_bytes;
	skt->stats.rx_trans++;

	hdr_in.loc_msubh = skt->msubh;
	hdr_in.rem_msubh = skt->con_msubh;
	hdr_in.priority = 0;
	hdr_in.sync_type = rdma_sync_chk;
	if (update_remote_hdr(skt, &hdr_in)) {
		skt->hdr->loc_tx_wr_flags |= 
					htonl(RSKT_BUF_HDR_FLAG_ERROR);
	       	skt->hdr->loc_rx_rd_flags |= 
					htonl(RSKT_BUF_HDR_FLAG_ERROR);
		ERR("Failed in update_remote_hdr\n");
		goto fail;
	};
	sem_post(&skt_h->mtx);
	return avail_bytes;
fail:
	/* FIXME: Needs review */
	if (errno == ECONNRESET) {
		WARN("Failed because the other side closed!\n");
	} else {
		/* Failed for another reason. Closing RSKT */
		WARN("Failed for some other reason. Closing connection\n");
		rskt_close(skt_h);
	}
	sem_post(&skt_h->mtx);
	return -1;
skt_ok:
	sem_post(&skt_h->mtx);
	return -1;
}; /* rskt_read() */

int rskt_recv( rskt_h skt_h, void *data, uint32_t max_byte_cnt)
{
	struct rskt_socket_t *skt;

	if (lib_uninit()) {
		ERR("lib_uninit() failed\n");
		return -errno;
	}

	if ((NULL == skt_h) || (NULL == data) || (1 > max_byte_cnt)) {
		ERR("Invalid input parameter\n");
		return -EINVAL;
	}

	skt = skt_h->skt;
	if (rskt_connected != skt->st) {
		ERR("Not connected\n");
		return -ENOTCONN;
	}

	if (lib_uninit()) {
		ERR("lib_uninit() failed\n");
		return -errno;
	}

	skt->stats.rx_bytes += max_byte_cnt;

	DBG("Exit with success\n");
	return 0;
}; /* rskt_recv() */

int rskt_flush(rskt_h skt_h, struct timespec timeout)
{
	struct rskt_socket_t *skt;

	DBG("ENTER");
	if (lib_uninit()) {
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	if (NULL == skt_h) {
		errno = EINVAL;
		ERR("%s\n", strerror(errno));
		return -errno;
	}
	skt = skt_h->skt;
	if (NULL == skt) {
		errno = EINVAL;
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	if (rskt_connected != skt->st) {
		errno = ENOTCONN;
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	if (!timeout.tv_nsec && !timeout.tv_sec) {
		errno = EINVAL;
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	skt->stats.rx_bytes += timeout.tv_sec;

	return 0;
}; /* rskt_flush() */

int rskt_shutdown(rskt_h skt_h)
{
	if (lib_uninit()) {
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	/* FIXME: Need to implement proper shutdown */
	WARN("NEED TO IMPLEMENT PROPER SHUTDOWN\n");
	if (rskt_connected == skt_h->skt->st)
		skt_h->skt->st = rskt_shutting_down;

	return 0;
};
	

int rskt_close(rskt_h skt_h)
{
	struct librskt_app_to_rsktd_msg *tx;
	struct librskt_rsktd_to_app_msg *rx;
	struct rskt_socket_t *skt;
	struct rdma_xfer_ms_in hdr_in;

	DBG("ENTER\n");
	if (lib_uninit()) {
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	if (NULL == skt_h) {
		errno = EINVAL;
		ERR("%s\n", strerror(errno));
		return -errno;
	}

	if (librskt_wait_for_sem(&skt_h->mtx, 0)) {
		ERR("%s\n", strerror(errno));
		sem_post(&skt_h->mtx);
		return -errno;
	}

	skt = skt_h->skt;

	if (NULL == skt) {
		ERR("skt is NULL\n");
		sem_post(&skt_h->mtx);
		return 0;
	}

	switch(skt->st) {
        case rskt_connected:
		/* Indicate to remote side that the connection was closed. 
		 * This should translate to rskt_read() returning ECONNRESET. */
		skt->hdr->loc_tx_wr_flags |= htonl(RSKT_FLAG_CLOSING);
		skt->hdr->loc_rx_rd_flags |= htonl(RSKT_FLAG_CLOSING);
		hdr_in.loc_msubh = skt->msubh;
		hdr_in.rem_msubh = skt->con_msubh;
		hdr_in.priority = 0;
		hdr_in.sync_type = rdma_sync_chk;
		if (update_remote_hdr(skt, &hdr_in)) {
			skt->hdr->loc_tx_wr_flags |=
					htonl(RSKT_BUF_HDR_FLAG_ERROR);
			skt->hdr->loc_rx_rd_flags |=
					htonl(RSKT_BUF_HDR_FLAG_ERROR);
			ERR("Failed in update_remote_hdr\n");
			sem_post(&skt_h->mtx);
			goto exit;
		}
        case rskt_bound  :
        case rskt_listening:
        case rskt_accepting:
        case rskt_connecting:
        case rskt_shutting_down:
        case rskt_closing:
		tx = alloc_app2d();
		rx = alloc_d2app();
	
		tx->msg_type = LIBRSKTD_CLOSE;
		tx->a_rq.msg.close.sn = htonl(skt->sa.sn);
	
		librskt_dmsg_req_resp(tx, rx);
		free(rx);
	default:
		break;
	};

	DBG("Calling cleanup_skt()\n");
	cleanup_skt(skt_h, skt);
	sem_post(&skt_h->mtx);
exit:
	return -errno;
}; /* rskt_close() */

void librskt_test_init(uint32_t test)
{
	lib.test = test;
};

#ifdef __cplusplus
}
#endif

