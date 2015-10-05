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
#include <netinet/in.h>
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
	memset(skt->msoh_name, 0, MAX_MS_NAME);
	skt->msoh_valid = 0;
	memset(skt->msh_name, 0, MAX_MS_NAME);
	skt->msh_valid = 0;
	skt->msubh_valid = 0;
        skt->msub_sz = 0;
	skt->msub_p = NULL;
	skt->tx_buf = NULL;
	skt->rx_buf = NULL;
	skt->buf_sz = 0;
	memset(skt->con_msh_name, 0, MAX_MS_NAME);
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

int librskt_wait_for_sem(sem_t *sema, int err_code)
{
	int rc = sem_wait(sema);
	while (rc && (EINTR == errno))
		rc = sem_wait(sema);
	if (rc) {
		ERR("Failed in sem_wait()\n");
		lib.all_must_die = err_code;
	}
	return rc;
};

int librskt_dmsg_req_resp(struct librskt_app_to_rsktd_msg *tx, 
			struct librskt_rsktd_to_app_msg *rx)
{
	struct rsvp_li *rsvp = (struct rsvp_li *)malloc(sizeof(struct rsvp_li));
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
	if (rx->a_rsp.err) {
		li = NULL;
		rc = -1;
		errno = ntohl(rx->a_rsp.err);
		ERR("a_rsp.err is not 0: %s\n", strerror(errno));
		goto fail;
	};

	free(rsvp);
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

	while (!lib.all_must_die) {
		if (librskt_wait_for_sem(&lib.msg_tx_cnt, 0x1020)) {
			lib.all_must_die = 1;
			goto exit;
		};
		if (librskt_wait_for_sem(&lib.msg_tx_mtx, 0x1021)) {
			lib.all_must_die = 2;
			goto exit;
		};
		tx = (struct librskt_app_to_rsktd_msg *)l_pop_head(&lib.msg_tx);
		sem_post(&lib.msg_tx_mtx);

		rc = send(lib.fd, (void *)tx, A2RSKTD_SZ, MSG_EOR);
		if (rc < 0) {
			lib.all_must_die = 3;
		}
	};
exit:
	pthread_exit(unused);
};

/* RX Thread to receive requests/responses and avoid blocking */
		
void rsvp_loop_resp(struct librskt_rsktd_to_app_msg *rxd)
{
	struct l_item_t *li;
	struct rsvp_li *dlyd;

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

	if (librskt_wait_for_sem(&lib.cli_mtx, 0x1041)) {
		WARN("lib.all_must_die = 31");
		lib.all_must_die = 31;
		sem_post(&lib.cli_mtx);
		return;
	};
	l_push_tail(&lib.cli, (void *)rxd);
	sem_post(&lib.cli_mtx);
	sem_post(&lib.cli_cnt);
};

void *rsvp_loop(void *unused)
{
	int rc;
	struct librskt_rsktd_to_app_msg *rxd = 
			(struct librskt_rsktd_to_app_msg *)
			malloc(RSKTD2A_SZ);

	if (rxd == NULL) {
		CRIT("Failed to allocate librskt_rsktd_to_app_msg\n");
		return NULL;
	}

	while (!lib.all_must_die) {
		memset((void *)rxd, 0, RSKTD2A_SZ);
		rc = recv(lib.fd, (void *)rxd, RSKTD2A_SZ, 0);
		if (rc < 0) {
			ERR("Failed in recv()\n");
			lib.all_must_die = 10;
			goto exit;
		};
		if (rxd->msg_type & htonl(LIBRSKTD_RESP | LIBRSKTD_FAIL)) {
			rsvp_loop_resp(rxd);
		} else {
			DBG("msg_type is OK\n");
			rsvp_loop_req(rxd);
			rxd = (struct librskt_rsktd_to_app_msg *)
				malloc(RSKTD2A_SZ);
			if (rxd == NULL) {
				CRIT("Failed to allocate librskt_rsktd_to_app_msg\n");
			}
		};
	};
exit:
	pthread_exit(unused);
};

void prep_response(struct librskt_rsktd_to_app_msg *req, 
		struct librskt_app_to_rsktd_msg *resp)
{
	resp->msg_type = req->msg_type | htonl(LIBRSKTD_RESP);
	resp->rsp_a.err = 0;
	resp->rsp_a.req_a = req->rq_a;
};

void *cli_loop(void *unused)
{
	/* FIXME: Should this processing occur in a separate thread? */
	struct cli_env cons_env;
	struct librskt_rsktd_to_app_msg *cmd;
	struct librskt_app_to_rsktd_msg *cmd_resp;

	cons_env.sess_socket = -1;
	cons_env.script = NULL;
	cons_env.fout = NULL;
	bzero(cons_env.prompt, PROMPTLEN+1);
	strncpy(cons_env.prompt, "REMCMD> ", PROMPTLEN);
	bzero(cons_env.output, BUFLEN);
	bzero(cons_env.input, BUFLEN);
	cons_env.DebugLevel = 0;
	cons_env.progressState = 0;
	cons_env.h = NULL;
	cons_env.cmd_prev = NULL;

	while (!lib.all_must_die) {
		cmd_resp = (struct librskt_app_to_rsktd_msg *)
				malloc(RSKTD2A_SZ);
		sem_wait(&lib.cli_cnt);
		if (lib.all_must_die)
			break;
		sem_wait(&lib.cli_mtx);
		if (lib.all_must_die)
			break;
		cmd = (struct librskt_rsktd_to_app_msg *)l_pop_head(&lib.cli);
		sem_post(&lib.cli_mtx);
		prep_response(cmd, cmd_resp);

		if (htonl(LIBRSKT_CLI_CMD) != cmd->msg_type) {
			cmd_resp->msg_type |= htonl(LIBRSKTD_FAIL);
			cmd_resp->rsp_a.err = EBADRQC;
		} else {
			process_command(&cons_env, cmd->rq_a.msg.cli.cmd_line);
		}
		if (librskt_dmsg_tx_resp(cmd_resp))
			break;
		free(cmd);
	};
	lib.all_must_die = 100;
	pthread_exit(unused);
};

void lib_rem_skt_from_list(rskt_h skt_h, struct rskt_socket_t *skt);

void cleanup_skt(rskt_h skt_h, struct rskt_socket_t *skt)
{
	if (skt_rmda_uninit != skt->connector) { 
		if (NULL != skt->msub_p)  {
			DBG("Unmapping skt->msub_p(%p)\n", skt->msub_p);
			rdma_munmap_msub(skt->msubh, (void *)skt->msub_p);
			skt->msub_p = NULL;
			skt->rx_buf = NULL;
			skt->tx_buf = NULL;
			skt->con_sz = 0;
			skt->msub_sz = 0;
		};
		if (skt_rdma_connector != skt->connector) { 
			rdma_disc_ms_h(skt->con_msh, skt->msubh);
			skt->msubh_valid = 0;
			skt->msh_valid = 0;
		} else {
			if (skt->msubh_valid) {
				rdma_destroy_msub_h(skt->msh, skt->msubh);
				skt->msubh_valid = 0;
			};
			if (skt->msh_valid) {
				rdma_close_ms_h(skt->msoh, skt->msh);
				skt->msh_valid = 0;
			};
		};
		if (skt->msoh_valid) {
			rdma_close_mso_h(skt->msoh);
			skt->msoh_valid = 0;
		};
	};
	lib_rem_skt_from_list(skt_h, skt);
	free(skt);
};

void lib_handle_dmn_close_req(rskt_h skt_h)
{
	struct rskt_socket_t *skt;

	if (NULL == skt_h)
		return;
	skt = skt_h->skt;
 	if (NULL == skt)
		return;

	/* FIXME: What does the failure of librskt_wait_for_sem mean? */
	/* How should this be handled? */
	librskt_wait_for_sem(&skt_h->mtx, 0);
	skt_h->skt = NULL;
	sem_post(&skt_h->mtx);

	cleanup_skt(skt_h, skt);
};

/* Request Processing Thread */
void *req_loop(void *unused)
{
	struct librskt_rsktd_to_app_msg *req;
	struct librskt_app_to_rsktd_msg *resp = NULL;
	struct l_item_t *li;
	rskt_h skt_h;

	while (!lib.all_must_die) {
	       
		if (NULL == resp) {
			resp = (struct librskt_app_to_rsktd_msg *)
				malloc(RSKTD2A_SZ);

			if (resp == NULL) {
				ERR("Failed to allocate 'resp'\n");
				return NULL;
			}
		}

		if (librskt_wait_for_sem(&lib.req_cnt, 0x1060)) {
			ERR("librskt_wait_for_sem() failed. Exiting\n");
			goto exit;
		}
		if (librskt_wait_for_sem(&lib.req_mtx, 0x1061)) {
			ERR("librskt_wait_for_sem() failed. Exiting\n");
			goto exit;
		}
		req = (struct librskt_rsktd_to_app_msg *)l_pop_head(&lib.req);
		sem_post(&lib.req_mtx);

		prep_response(req, resp);

		switch (ntohl(req->msg_type)) {
		case LIBRSKT_CLOSE_CMD:
			/* Got this request from the RSKTD.
			* Send response when are sure that app can't use this
			* socket any more.
			*/
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
	pthread_exit(unused);
};

int librskt_init(int rsktd_port, int rsktd_mpnum)
{
	struct librskt_app_to_rsktd_msg *req;
	struct librskt_rsktd_to_app_msg *resp;

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
	if (connect(lib.fd, (struct sockaddr *) &lib.addr, 
				lib.addr_sz)) {
		ERR("ERROR on librskt_init connect: %s\n", strerror(errno));
		goto fail;
	};

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

	sem_init(&lib.cli_mtx, 0, 1);
	sem_init(&lib.cli_cnt, 0, 0);
	l_init(&lib.cli);

	lib.test = 0;
	sem_init(&lib.skts_mtx, 0, 1);
	l_init(&lib.skts);

	/* Startup the threads */
	if (pthread_create( &lib.tx_thr, NULL, tx_loop, NULL)) {
		lib.all_must_die = 1;
		CRIT("ERROR:librskt_init, tx_loop thread: %s\n", strerror(errno));
		goto fail;
	};

	if (pthread_create( &lib.rsvp_thr, NULL, rsvp_loop, NULL)) {
		lib.all_must_die = 2;
		CRIT("ERROR:librskt_init rsvp_loop thread: %s\n", strerror(errno));
		goto fail;
	};

	if (pthread_create( &lib.req_thr, NULL, req_loop, NULL)) {
		lib.all_must_die = 3;
		CRIT("ERROR:librskt_init, req_loop thread: %s\n", strerror(errno));
		goto fail;
	};

	if (pthread_create( &lib.cli_thr, NULL, cli_loop, NULL)) {
		lib.all_must_die = 4;
		CRIT("ERROR:librskt_init, cli_loop thread: %s\n", strerror(errno));
		goto fail;
	};

	/* Socket appears to be open, say hello to RSKTD */
	req = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	if (req == NULL) {
		CRIT("Failed to malloc 'req'\n");
		goto fail;
	}

	resp = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);
	if (resp == NULL) {
		CRIT("Failed to malloc 'resp'\n");
		free(req);
		goto fail;
	}
	req->msg_type = LIBRSKTD_HELLO;
	req->a_rq.msg.hello.proc_num = htonl(getpid());
	memset(req->a_rq.msg.hello.app_name, 0, MAX_APP_NAME);
	snprintf(req->a_rq.msg.hello.app_name, MAX_APP_NAME-1, "%d", getpid());

	if (librskt_dmsg_req_resp(req, resp)) {
		perror("ERROR on librskt_init hello");
	};
	free(req);
	free(resp);

	lib.init_ok = rsktd_port;
	lib.ct = ntohl(resp->a_rsp.msg.hello.ct);
fail:
	DBG("EXIT\n");
	return -!((lib.init_ok == lib.portno) && (lib.portno));
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
		ERR("Failed in rskt_allo_skt\n");
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

void lib_rem_skt_from_list(rskt_h skt_h, struct rskt_socket_t *skt)
{
	struct l_item_t *li;
	rskt_h l_skt;

	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1080)) {
		ERR("Failed in librskt_wait_for_sem\n");
		return;
	}

	l_skt = (rskt_h)l_find(&lib.skts, skt->sa.sn, &li);
	if (skt_h == l_skt) {
		DBG("Calling l_lremove()\n");
		l_lremove(&lib.skts, li); /* Do not deallocate socket */
	}
	sem_post(&lib.skts_mtx);
};

int rskt_bind(rskt_h skt_h, struct rskt_sockaddr *sock_addr)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;

	struct rskt_socket_t *skt;

	if (lib_uninit()) {
		ERR("Fail due to lib_uninit()\n");
		goto exit;
	}

	errno = EINVAL;
	if ((NULL == skt_h) || (NULL == sock_addr)) {
		ERR("NULL parameter\n");
		goto exit;
	}

	if (NULL == skt_h->skt)
		if (rskt_alloc_skt(skt_h)) {
			ERR("rskt_alloc_skt failed\n");
			goto exit;
		}
	skt = skt_h->skt;

	if (rskt_alloced != skt->st) {
		ERR("skt->st != rskt_alloced\n");
		errno = EBADFD;
		goto exit;
	};

	skt->sa.sn = sock_addr->sn;
	skt->sa.ct = lib.ct;
	skt->sai.sa.ct = sock_addr->ct;

	tx = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	rx = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);

	tx->msg_type = LIBRSKTD_BIND;
	tx->a_rq.msg.bind.sn = htonl(sock_addr->sn);

	if (librskt_dmsg_req_resp(tx, rx)) {
		ERR("librskt_dmsg_req_resp() failed\n");
		goto exit;
	}

	if (rx->a_rsp.err) {
		errno = EADDRNOTAVAIL;
		ERR("%s\n", strerror(errno));
	} else {
		skt->st = rskt_bound;
		lib_add_skt_to_list(skt_h);
		errno = 0;
	};
exit:
	if (NULL != tx)
		free(tx);
	if (NULL != rx)
		free(rx);
	return -errno;
};

int rskt_listen(rskt_h skt_h, int max_backlog)
{
	struct librskt_app_to_rsktd_msg *tx = NULL;
	struct librskt_rsktd_to_app_msg *rx = NULL;
	struct rskt_socket_t *skt;

	if (lib_uninit()) {
		ERR("Failed in lib_uninit()\n");
		goto exit;
	}

	errno = EINVAL;
	if (NULL == skt_h) {
		ERR("skt_h is NULL\n");
		goto exit;
	}
	skt = skt_h->skt;
	if (NULL == skt) {
		ERR("skt is NULL\n");
		goto exit;
	}

	if (lib_uninit()) {
		ERR("Failed in lib_uninit()\n");
		goto exit;
	}

	if (rskt_bound != skt->st) {
		errno = EBADFD;
		ERR("%s\n", strerror(errno));
		goto exit;
	};

	skt->max_backlog = max_backlog;

	tx = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	rx = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);

	tx->msg_type = LIBRSKTD_LISTEN;
	tx->a_rq.msg.listen.sn = htonl(skt->sa.sn);
	tx->a_rq.msg.listen.max_bklog = htonl(skt->max_backlog);

	if (librskt_dmsg_req_resp(tx, rx)) {
		ERR("librskt_dmsg_req_resp failed\n");
		goto exit;
	}

	if (rx->a_rsp.err) {
		errno = EBUSY;
		ERR("%s\n", strerror(errno));
	} else {
		errno = 0;
		skt->st = rskt_listening;
	}

exit:
	if (NULL != tx)
		free(tx);
	if (NULL != rx)
		free(rx);
	return -errno;
};

int update_remote_hdr(struct rskt_socket_t *skt,
			struct rdma_xfer_ms_in *hdr_in);

int setup_skt_ptrs(struct rskt_socket_t *skt)
{
	struct rdma_xfer_ms_in hdr_in;

	skt->con_sz = (skt->msub_sz > skt->con_sz)?skt->con_sz:skt->msub_sz;
	skt->buf_sz = (skt->con_sz - sizeof(struct rskt_buf_hdr))/2;
	skt->tx_buf = skt->msub_p + sizeof(struct rskt_buf_hdr);
	skt->rx_buf = skt->tx_buf + skt->buf_sz;

	skt->hdr->loc_tx_wr_ptr = htonl(0);
	skt->hdr->loc_tx_wr_flags = htonl(RSKT_BUF_HDR_FLAG_INIT);
	skt->hdr->loc_rx_rd_ptr = htonl(skt->buf_sz - 1);
	skt->hdr->loc_rx_rd_flags = htonl(RSKT_BUF_HDR_FLAG_INIT);

	hdr_in.loc_msubh = skt->msubh;
	hdr_in.rem_msubh = skt->con_msubh;
	hdr_in.priority = 0;
	hdr_in.sync_type = rdma_sync_chk;

	return update_remote_hdr(skt, &hdr_in);
};

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

	l_skt = l_skt_h->skt;
	if (NULL == l_skt) {
		ERR("l_skt_h->skt is NULL\n");
		goto exit;
	}
	if (NULL == skt_h->skt) {
		INFO("skt_h->skt is NULL\n");
		if (rskt_alloc_skt(skt_h)) {
			CRIT("Failed to allocated skt_h..exiting\n");
			goto exit;
		}
	}
	skt = skt_h->skt;

	if (rskt_listening != l_skt->st) {
		ERR("rskt_listening != l_skt->st..exiting\n");
		goto exit;
	}
	if (rskt_alloced != skt->st) {
		ERR("rskt_alloced != skt->st\n");
		goto exit;
	}

	tx = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	rx = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);

	tx->msg_type = LIBRSKTD_ACCEPT;
	tx->a_rq.msg.accept.sn = htonl(l_skt->sa.sn);
	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1090)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto exit;
	}
	l_skt->st = rskt_accepting;
	sem_post(&lib.skts_mtx);

	if (librskt_dmsg_req_resp(tx, rx)) {
		WARN("librskt_dmsg_req_resp() failed..closing\n");
		goto close;
	}

	*skt = *l_skt;
	skt->max_backlog = 0;
	skt->st = rskt_connecting;
	skt->connector = skt_rdma_acceptor;
	skt->sa.sn = ntohl(rx->a_rsp.msg.accept.new_sn);
	skt->sa.ct = ntohl(rx->a_rsp.msg.accept.new_ct);
	skt->sai.sa.ct = ntohl(rx->a_rsp.msg.accept.peer_sa.ct);
	skt->sai.sa.sn = ntohl(rx->a_rsp.msg.accept.peer_sa.sn);
	memcpy(skt->msoh_name, rx->a_rsp.msg.accept.mso_name, MAX_MS_NAME);
	memcpy(skt->msh_name, rx->a_rsp.msg.accept.ms_name, MAX_MS_NAME);
	skt->msub_sz = ntohl(rx->a_rsp.msg.accept.ms_size);

	if (librskt_wait_for_sem(&lib.skts_mtx, 0x1091)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto exit;
	}
	l_skt->st = rskt_listening;
	sem_post(&lib.skts_mtx);

	if (librskt_wait_for_sem(&skt_h->mtx, 0)) {
		ERR(" librskt_wait_for_sem() failed..exiting\n");
		goto exit;
	}

	rc = rdma_open_mso_h(skt->msoh_name, &skt->msoh);
	if (rc) {
		ERR("Failed to open mso(%s)\n", skt->msoh_name);
		goto close;
	}
	skt->msoh_valid = 1;

	DBG("9\n");
	rc = rdma_open_ms_h(skt->msh_name, skt->msoh, 0, 
			&skt->msub_sz, &skt->msh);
	if (rc) {
		ERR("Failed to open ms(%s)\n", skt->msh_name);
		goto close;
	}
	skt->msh_valid = 1;

	rc = rdma_create_msub_h(skt->msh, 0,
				skt->msub_sz, 0, &skt->msubh);
	if (rc) {
		ERR("Failed to create msub\n");
		goto close;
	}
	skt->msubh_valid = 1;

	rc = rdma_mmap_msub(skt->msubh, (void **)&skt->msub_p);
	if (rc) {
		ERR("Failed to mmap msub\n");
		goto close;
	}

	do {
		rc = rdma_accept_ms_h(skt->msh, skt->msubh, 
				&skt->con_msubh, &skt->con_sz, 0);
	} while (-EINTR == rc);	/* FIXME: should it be ETIME? */
	if (rc) {
		ERR("Failed in rdma_accept_ms_h()\n");
		goto close;
	}

	skt->st = rskt_connected;
	setup_skt_ptrs(skt);
	sem_post(&skt_h->mtx);
	DBG("d\n");
	lib_add_skt_to_list(skt_h);
	INFO("Exiting with SUCCESS\n");
	return 0;

close:
	sem_post(&skt_h->mtx);
	rskt_close(skt_h);
exit:
	if (NULL != tx)
		free(tx);
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

	if (lib_uninit()) {
		CRIT("lib_uninit() failed..exiting\n");
		goto exit;
	}
	if ((NULL == skt_h) || (NULL == sock_addr)) {
		CRIT("NULL parameter: skt_h = %p, sock_addr = %p\n",
			skt_h, sock_addr);
		goto exit;
	}
	if (NULL == skt_h->skt) {
		WARN("skt_h->skt is NULL\n");
		if (rskt_alloc_skt(skt_h)) {
			CRIT("Failed to alloc skt_h\n");
			goto exit;
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
		goto exit;
	}

	tx = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	if (!tx) {
		CRIT("Failed to malloc() tx..exiting\n");
		goto exit;
	}
	rx = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);
	if (!rx) {
		CRIT("Failed to malloc() rx..exiting\n");
		goto exit;
	}
	tx->msg_type = LIBRSKTD_CONN;
	tx->a_rq.msg.conn.sn = htonl(sock_addr->sn);
	tx->a_rq.msg.conn.ct = htonl(sock_addr->ct);

	/* Response indicates what mso, ms, and msub to use, and 
	 * what ms to rdma_connect with
	 */
	rc = librskt_dmsg_req_resp(tx, rx);
	if (rc) {
		ERR("librskt_dmsg_req_resp() failed..closing\n");
		goto close;
	}

	if (lib.all_must_die) {
		rc = -1;
		INFO("all_must_die is true...exiting\n");
		goto exit;
	};

	rc = librskt_wait_for_sem(&skt_h->mtx, 0);
	if (rc) {
		ERR("librskt_wait_for_sem failed...exiting\n");
		goto exit;
	}

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

	rc = rdma_open_mso_h(skt->msoh_name, &skt->msoh);
	if (rc) {
		ERR("rdma_open_mso_h() failed msoh_name(%s)..closing\n", skt->msoh_name);
		goto close;
	}
	skt->msoh_valid = 1;

	rc = rdma_open_ms_h(skt->msh_name, skt->msoh, 0, 
			&skt->msub_sz, &skt->msh);
	if (rc || !skt->msub_sz) {
		ERR("rdma_open_ms_h() failed msh_name(%s)..closing\n", skt->msh_name);
		goto close;
	}
	skt->msh_valid = 1;

	rc = rdma_create_msub_h(skt->msh, 0,
				skt->msub_sz, 0, &skt->msubh);
	if (rc) {
		ERR("rdma_create_msub() failed..closing\n");
		goto close;
	}
	skt->msubh_valid = 1;

	rc = rdma_mmap_msub(skt->msubh, (void **)&skt->msub_p);
	if (rc) {
		ERR("rdma_mmap_msub() failed..closing\n");
		goto close;
	}

	rc = rdma_conn_ms_h(16, skt->sai.sa.ct,
				skt->con_msh_name, skt->msubh, 
				&skt->con_msubh, &skt->con_sz,
				&skt->con_msh, 0);
	if (rc) {
		ERR("rdma_conn_ms_h() failed..closing\n");
		goto close;
	}

	skt->st = rskt_connected;
	setup_skt_ptrs(skt);
	sem_post(&skt_h->mtx);
	lib_add_skt_to_list(skt_h);
	INFO("Exiting with SUCCESS\n");
	return 0;
close:
	temp_errno = errno;
	sem_post(&skt_h->mtx);
	rskt_close(skt_h);
	errno = temp_errno;
exit:
	if (tx != NULL)
		free(tx);
	if (rx != NULL)
		free(rx);
	return rc;
}; /* rskt_connect() */

const struct timespec rw_dly = {0, 5000};

// FIXME static inline 
uint32_t get_free_bytes(volatile struct rskt_buf_hdr *hdr,
				uint32_t buf_sz)
{
	uint32_t ltw = ntohl(hdr->loc_tx_wr_ptr);
	uint32_t rtr = ntohl(hdr->rem_tx_rd_ptr);
	uint32_t free_bytes = rtr - ltw;

	if (ltw > rtr)
		free_bytes = buf_sz - ltw + rtr;

	return free_bytes;
}; /* get_free_bytes() */

#define INC_PTR(x,y,z) x=htonl((ntohl(x)+y)%z)

// FIXME: Change to static inline
int send_bytes(rskt_h skt_h, void *data, int byte_cnt, 
			struct rdma_xfer_ms_in *hdr_in, int inited) {
	struct rdma_xfer_ms_out hdr_out;
	uint32_t dma_rd_offset, dma_wr_offset;
	struct rskt_socket_t *skt = skt_h->skt;

	DBG("ENTER\n");
	dma_rd_offset = ntohl(skt->hdr->loc_tx_wr_ptr) + RSKT_TOT_HDR_SIZE;
	dma_wr_offset = dma_rd_offset + skt->buf_sz;
	DBG("dma_rd_offset = %u, dma_wr_offset = %u, byte_cnt = %u\n",
				dma_rd_offset, dma_wr_offset, byte_cnt);
	memcpy((void *)(skt->tx_buf + ntohl(skt->hdr->loc_tx_wr_ptr)),
		data, byte_cnt);
	INC_PTR(skt->hdr->loc_tx_wr_ptr, byte_cnt, skt->buf_sz);

	if (!inited) {
		DBG("!inited\n");
		hdr_in->loc_msubh = skt->msubh;
		hdr_in->rem_msubh = skt->con_msubh;
		hdr_in->priority = 0;
		hdr_in->sync_type = rdma_sync_chk;
	};

	hdr_in->loc_offset = dma_rd_offset;
	hdr_in->num_bytes = byte_cnt;
	hdr_in->rem_offset = dma_wr_offset;

	if (rdma_push_msub(hdr_in, &hdr_out)) {
		skt->hdr->loc_tx_wr_flags |= htonl(RSKT_FLAG_ERROR);
		skt->hdr->loc_rx_rd_flags |= htonl(RSKT_FLAG_ERROR);
		ERR("Failed in rdma_push_msub()..exiting\n");
		return -1;
	};
	skt->stats.tx_bytes += byte_cnt;
	skt->stats.tx_trans++;
	DBG("tx_bytes = %u\n", skt->stats.tx_bytes);
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
	rc = rdma_push_msub(hdr_in, &hdr_out);
	if (rc) {
		skt->hdr->loc_tx_wr_flags |= htonl(RSKT_FLAG_ERROR);
		skt->hdr->loc_rx_rd_flags |= htonl(RSKT_FLAG_ERROR);
	};

	return rc;
}; /* update_remote_hdr() */

int rskt_write(rskt_h skt_h, void *data, uint32_t byte_cnt)
{
	int rc = -1;
	uint32_t free_bytes = 0; 
	int time_remains = 5000;
	struct timespec unused;
	struct rdma_xfer_ms_in hdr_in;
	struct rskt_socket_t *skt;

	errno = EINVAL;
	if ((NULL == skt_h) || (NULL == data) || (1 > byte_cnt)) {
		ERR("Null parameter of byte_cnt < 1\n");
		goto skt_ok;
	}

	if (lib_uninit()) {
		ERR("lib_uninit() failed\n");
		goto skt_ok;
	}

	skt = skt_h->skt;
	if (NULL == skt) {
		ERR("skt_h->skt is NULL\n");
		goto skt_ok;
	}

	if (rskt_connected != skt->st) {
		ERR("skt->st is NOT skt_connected\n");
		goto skt_ok;
	}

	errno = 0;
	free_bytes = get_free_bytes(skt->hdr, skt->buf_sz);
	DBG("byte_cnt = %d, free_bytes = %d\n", byte_cnt, free_bytes);
	while ((free_bytes < byte_cnt) && time_remains) {
		nanosleep(&rw_dly, &unused);
	 	free_bytes = get_free_bytes(skt->hdr, 
						skt->buf_sz);
		time_remains--;
	};

	if (!time_remains) {
		errno = ETIMEDOUT;
		goto skt_ok;
	};

	if ((byte_cnt + ntohl(skt->hdr->loc_tx_wr_ptr)) 
						<= skt->buf_sz) {
		DBG("byte_cnt = %d, loc_tx_wr_ptr = %d, skt->buf_sz = %d\n",
			byte_cnt, skt->hdr->loc_tx_wr_ptr, skt->buf_sz);
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

	if (update_remote_hdr(skt, &hdr_in)) {
		ERR("updated_remote_hdr failed..exiting\n");
		goto fail;
	}

	DBG("EXIT with success\n");
	return 0;
fail:
	WARN("Closing skt_t due to failure condition\n");
	rskt_close(skt_h);
skt_ok:
	return -1;
}; /* rskt_write() */

static inline
uint32_t get_avail_bytes(struct rskt_buf_hdr volatile *hdr,
					uint32_t buf_sz)
{
	uint32_t avail_bytes = 0;
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
	int time_remains = 5000;
	struct timespec unused;
	struct rdma_xfer_ms_in hdr_in;
	uint32_t first_offset;
	struct rskt_socket_t *skt;

	if (lib_uninit()) {
		ERR("lib_uninit() failed\n");
		goto fail;
	}

	errno = EINVAL;
	if ((NULL == skt_h) || (NULL == data) || (1 > max_byte_cnt)) {
		ERR("Invalid input parameter\n");
		goto skt_ok;
	}

	skt = skt_h->skt;
	if (NULL == skt) {
		DBG("skt is NULL\n");
		goto skt_ok;
	}

	if (rskt_connected != skt->st) {
		WARN("Not connected");
		errno = ENOTCONN;
		goto skt_ok;
	};

	errno = 0;
	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);

	while (!avail_bytes && time_remains && !errno) {
		nanosleep(&rw_dly, &unused);
	 	avail_bytes = get_avail_bytes(skt->hdr, skt->buf_sz);
		time_remains--;
	};

	if (!time_remains) {
		errno = ETIMEDOUT;
		ERR("%s\n", strerror(errno));
		goto skt_ok;
	};

	if (!avail_bytes || errno) {
		errno = ECONNRESET;
		ERR("%s\n", strerror(errno));
		goto fail;
	};

	if (avail_bytes > max_byte_cnt)
		avail_bytes = max_byte_cnt;

	first_offset = (ntohl(skt->hdr->loc_rx_rd_ptr) + 1) % skt->buf_sz;
	if ((avail_bytes + first_offset) < skt->buf_sz) {
		read_bytes(skt, data, avail_bytes);
	} else {
		uint32_t first_bytes = skt->buf_sz - first_offset;

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

	return avail_bytes;
fail:
	WARN("Failed..closing skt_h\n");
	rskt_close(skt_h);
skt_ok:
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
		return -errno;
	}
	skt = skt_h->skt;
	skt_h->skt = NULL;
	sem_post(&skt_h->mtx);

	if (NULL == skt) {
		ERR("skt is NULL\n");
		return 0;
	}

	tx = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
	rx = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);

	tx->msg_type = LIBRSKTD_CLOSE;
	tx->a_rq.msg.close.sn = htonl(skt->sa.sn);

	librskt_dmsg_req_resp(tx, rx);

	if (lib.all_must_die) {
		DBG("all_must_die\n");
		goto exit;
	}

	cleanup_skt(skt_h, skt);
	errno = 0;
	DBG("Freeing 'tx' and 'rx'\n");
exit:
	free(tx);
	free(rx);
	return -errno;
};

void librskt_test_init(uint32_t test)
{
	lib.test = test;
};

#ifdef __cplusplus
}
#endif

