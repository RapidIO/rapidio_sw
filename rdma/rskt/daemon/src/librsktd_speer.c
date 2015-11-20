/* Management of slave RSKTD connections.
 * Slave RSKTD connections receive requests from other daemons, 
 * and send responses.
 */
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
#include "librsktd_dmn_info.h"
#include "librsktd_lib_info.h"
#include "librsktd_msg_proc.h"
#include "liblog.h"

#define RIO_MPORT_DEV_PATH "/dev/rio_mport"

#ifdef __cplusplus
extern "C" {
#endif

int speer_rx_req(struct rskt_dmn_speer *speer)
{
	int rc = 0;

	speer->rx_buff_used = 1;
	DBG("\n\tSPEER %d: Waiting for request\n", speer->ct);

	do {
		rc = riomp_sock_receive(speer->cm_skt_h, 
			&speer->rx_buff, RSKTD_CM_MSG_SIZE, 0);
	} while (rc && !speer->i_must_die && !dmn.all_must_die &&
		((errno == EINTR) || (errno == ETIME) || (errno == EAGAIN)));

	if (rc) {
		CRIT("SPEER %d: riomp_sock_receive: %d (%d:%s)\n",
			speer->ct, rc, errno, strerror(errno));
		speer->i_must_die = 1;
	} else {
		DBG("\n\tSPEER %d: riomp_sock_receive: TYPE %3x %s SEQ %3x\n",
			speer->ct, speer->req->msg_type,
			RSKTD_REQ_STR(ntohl(speer->req->msg_type)),
			speer->req->msg_seq);
	};

	return rc;
};

void rsktd_prep_resp(struct librsktd_unified_msg *msg)
{
	memcpy(msg->dreq, (*msg->sp)->req, DMN_REQ_SZ);
        msg->dresp->msg_type = msg->dreq->msg_type | htonl(RSKTD_RESP_FLAG);
        msg->dresp->msg_seq = msg->dreq->msg_seq;
        msg->dresp->err = 0;
        memcpy((void *)&msg->dresp->req, (void *)&msg->dreq->msg,
                        sizeof(union librsktd_req));
	msg->msg_type = ntohl(msg->dreq->msg_type);
};

void close_speer(struct rskt_dmn_speer *speer)
{
	struct l_item_t *l_i;
	struct rskt_dmn_speer *chk_sp;

	DBG("\n\tSPEER %d: closing speer!\n", speer->ct);

	speer->alive = 0;
	sem_post(&speer->started);

	speer->comm_fail = 1;

	if (NULL != speer->rx_buff) {
		DBG("\n\tSPEER %d: release receive buffer\n", speer->ct);
		riomp_sock_release_receive_buffer(speer->cm_skt_h,
							speer->rx_buff);
		speer->rx_buff = NULL;
	};
	
	if (NULL != speer->tx_buff) {
		DBG("\n\tSPEER %d: release transmit buffer\n", speer->ct);
		riomp_sock_release_send_buffer(speer->cm_skt_h, 
							speer->tx_buff);
		speer->tx_buff = NULL;
	};

	if (speer->cm_skt_h_valid) {
		speer->cm_skt_h_valid = 0;
		riomp_sock_close(&speer->cm_skt_h);
	};

	sem_wait(&dmn.speers_mtx);
	chk_sp = (struct rskt_dmn_speer *)l_head(&dmn.speers, &l_i);
	while ((NULL != chk_sp) && (NULL != chk_sp->self_ref)) {
		if (chk_sp == speer) {
			l_lremove(&dmn.speers, l_i);
			break;
		}
		chk_sp = (struct rskt_dmn_speer *)l_next(&l_i);
	};
	sem_post(&dmn.speers_mtx);

	DBG("\n\tSPEER %d: killing speer thread!\n", speer->ct);
	pthread_kill(speer->s_rx, SIGUSR1);
	DBG("\n\tSPEER %d: Waiting for  speer to die!\n", speer->ct);
	pthread_join(speer->s_rx, NULL);

	sem_post(&speer->resp_ready);
};

void *speer_rx_loop(void *p_i)
{
	struct rskt_dmn_speer *speer = (struct rskt_dmn_speer *)p_i;
	int rc = -1;

	INFO("\n\tSPEER %d: Starting!\n", speer->ct);
	sem_wait(&dmn.speers_mtx);
	l_push_tail(&dmn.speers, speer->self_ref);
	sem_post(&dmn.speers_mtx);

	sem_post(&speer->started);
	DBG("\n\tSPEER %d: Running\n", speer->ct);

	while (!speer->i_must_die && !speer->comm_fail) {
		struct librsktd_unified_msg *msg =
		(struct librsktd_unified_msg *)
				malloc(sizeof(struct librsktd_unified_msg));

		msg->dreq = (struct rsktd_req_msg *)
				malloc(sizeof(struct rsktd_req_msg));
		msg->dresp = (struct rsktd_resp_msg *)
				malloc(sizeof(struct rsktd_resp_msg));
		msg->sp = speer->self_ref;

		rc = speer_rx_req(speer);

		if (speer->i_must_die || speer->comm_fail || dmn.all_must_die 
				|| rc)
			break;

		rsktd_prep_resp(msg);

		DBG("\n\tSPEER %d: Rx Req %x %s Seq %x \n", speer->ct,
			speer->req->msg_type,
			RSKTD_REQ_STR(ntohl(speer->req->msg_type)), 
			speer->req->msg_seq);
		switch (msg->msg_type) {
		case RSKTD_HELLO_REQ:
		case RSKTD_CONNECT_REQ:
		case RSKTD_CLI_CMD_REQ:
		default:
			msg->proc_type = RSKTD_PROC_SREQ;
			msg->proc_stage = RSKTD_SPEER_SEQ_DREQ;
			break;
		case RSKTD_CLOSE_REQ:
			msg->proc_type = RSKTD_PROC_S2A;
			msg->proc_stage = RSKTD_S2A_SEQ_DREQ;
			break;
		};
		DBG(
	"\n\tSPEER %d: enqueue : REQ %3x %s RESP %3x %s SEQ %3x\n\t\tPROC %x STAGE %x\n",
			speer->ct, msg->msg_type,
			RSKTD_REQ_STR(msg->msg_type),
        		msg->dresp->msg_type,
			RSKTD_RESP_STR(ntohl(msg->dresp->msg_type)),
        		msg->dresp->msg_seq,
			msg->proc_type,
			msg->proc_stage);
		enqueue_mproc_msg(msg);
	};

	DBG("\n\tSPEER %d: Exiting, startin to cleanup!\n", speer->ct);
	speer->comm_fail = 1;

	if (NULL != speer->rx_buff) {
		riomp_sock_release_receive_buffer(speer->cm_skt_h,
							speer->rx_buff);
		speer->rx_buff = NULL;
	};
	
	if (NULL != speer->tx_buff) {
		riomp_sock_release_send_buffer(speer->cm_skt_h, 
							speer->tx_buff);
		speer->tx_buff = NULL;
	};
	CRIT("\n\tSPEER %d: Exiting!\n", speer->ct);

	pthread_exit(NULL);
};

void start_new_speer(riomp_sock_t new_socket)
{
	int rc;
	struct rskt_dmn_speer *speer;
	struct rskt_dmn_speer **self_ref;

	speer = (struct rskt_dmn_speer *) malloc(sizeof(struct rskt_dmn_speer));
	self_ref = (struct rskt_dmn_speer **)malloc(sizeof(void *));
	speer->self_ref = self_ref;
	*self_ref = speer;

	speer->cm_skt_h = new_socket;
	speer->cm_skt_h_valid = 1;
	speer->i_must_die = 0;
	speer->got_hello = 0;
	speer->comm_fail = 0;
	speer->ct = 0;
	speer->cm_skt_num = 0;
	speer->cm_mp = 0;
	sem_init(&speer->started, 0, 0);
	speer->alive = 0;
	speer->rx_buff_used = 0;
	speer->rx_buff = malloc(RSKTD_CM_MSG_SIZE);
	speer->tx_buff_used = 0;

	if (riomp_sock_request_send_buffer(new_socket, &speer->tx_buff)) {
		riomp_sock_close(&new_socket);
	};
	speer->req = (struct rsktd_req_msg *)speer->rx_buff;
	speer->resp = (struct rsktd_resp_msg *)speer->tx_buff;
	sem_init(&speer->req_ready, 0, 0);
	sem_init(&speer->resp_ready, 0, 0);

        rc = pthread_create(&speer->s_rx, NULL, speer_rx_loop, (void*)speer);
	if (!rc)
		sem_wait(&speer->started);
};

void enqueue_speer_msg(struct librsktd_unified_msg *msg)
{
	DBG(
	"\n\tSPEER %d: RESP TX enqueue : REQ %3x %s RESP %3x %s SEQ %3x\n\t\tPROC %x STAGE %x\n",
		(*msg->sp)->ct, msg->msg_type,
		RSKTD_REQ_STR(msg->msg_type),
        	msg->dresp->msg_type,
		RSKTD_RESP_STR(ntohl(msg->dresp->msg_type)),
        	msg->dresp->msg_seq,
		msg->proc_type,
		msg->proc_stage);

	sem_wait(&dmn.speer_tx_mutex);
	l_push_tail(&dmn.speer_tx_q, msg);
	sem_post(&dmn.speer_tx_mutex);
	DBG("\n\tSPEER %d: message enqueued\n", (*msg->sp)->ct);

	sem_post(&dmn.speer_tx_cnt);
};

/* Sends responses to all speers */
void *speer_tx_loop(void *unused)
{
	struct librsktd_unified_msg *msg;
	struct rskt_dmn_speer *s;

	dmn.speer_tx_alive = 1;
	sem_post(&dmn.loop_started);

	while (!dmn.all_must_die) {
		DBG("\n\tSPEER TX: Waiting to TX\n");
		sem_wait(&dmn.speer_tx_cnt);
		if (dmn.all_must_die)
			break;
		sem_wait(&dmn.speer_tx_mutex);
		if (dmn.all_must_die)
			break;
		msg = (struct librsktd_unified_msg *)
			l_pop_head(&dmn.speer_tx_q);
		sem_post(&dmn.speer_tx_mutex);
		if (dmn.all_must_die || (NULL == msg))
			break;

		if ((RSKTD_PROC_SREQ != msg->proc_type) &&
			!((RSKTD_PROC_S2A == msg->proc_type) &&
			 (RSKTD_S2A_SEQ_ARESP == msg->proc_stage))) {
			CRIT(
			"\n\tSPEER TX: DISCARDING REQ %3x %s %d RESP %3x %s %3x\n\t\tPROC %x STAGE %x\n",
				msg->msg_type,
				RSKTD_REQ_STR(msg->msg_type),
        			msg->dreq->msg_seq,
        			msg->dresp->msg_type,
				RSKTD_RESP_STR(msg->dresp->msg_type),
        			msg->dresp->msg_seq,
				msg->proc_type,
				msg->proc_stage);
			continue;
		};

		s = *msg->sp;

		/* Can't send response if connection has closed */
		if (NULL == s) {
			CRIT(
			"\n\tSPEER TX: DISCARDING REQ %3x %s %d RESP %3x %s %3x\n\t\tPROC %x STAGE %x\n\tNO SPEER TO SEND TO!\n",
				msg->msg_type,
				RSKTD_REQ_STR(msg->msg_type),
        			msg->dreq->msg_seq,
        			msg->dresp->msg_type,
				RSKTD_RESP_STR(msg->dresp->msg_type),
        			msg->dresp->msg_seq,
				msg->proc_type,
				msg->proc_stage);
			continue;
		};

		if (s->comm_fail) {
			CRIT( "\n\tSPEER %d TX: Comm failure!\n", s->ct);
			CRIT(
			"\n\tSPEER %d TX: DISCARDING REQ %3x %s %d RESP %3x %s %3x\n\t\tPROC %x STAGE %x\n",
				s->ct, msg->msg_type,
				RSKTD_REQ_STR(msg->msg_type),
        			msg->dreq->msg_seq,
        			msg->dresp->msg_type,
				RSKTD_RESP_STR(msg->dresp->msg_type),
        			msg->dresp->msg_seq,
				msg->proc_type,
				msg->proc_stage);
			continue;
		};

		/* Send response to speer */
		memcpy((void *)s->tx_buff, (void *)msg->dresp,
				sizeof(struct rsktd_resp_msg));
        	s->tx_buff_used = 1;
		DBG("\n\tSPEER %d TX: RESP %3x %s %3x\n",
			s->ct,
        		s->resp->msg_type,
			RSKTD_RESP_STR(ntohl(s->resp->msg_type)),
        		s->resp->msg_seq);

        	s->tx_rc = riomp_sock_send(s->cm_skt_h, s->tx_buff,
			DMN_RESP_SZ);
		DBG("\n\tSPEER %d TX: rc %d\n", s->ct, s->tx_rc);
	};
	dmn.speer_tx_alive = 0;
	pthread_exit(unused);
};

void close_all_speers(void)
{
	struct rskt_dmn_speer **sp;

	DBG("ENTER\n");
	sem_wait(&dmn.speers_mtx);
	sp = (struct rskt_dmn_speer **)l_pop_head(&dmn.speers);
	sem_post(&dmn.speers_mtx);

	while (NULL != sp) {
		if (NULL != *sp) {
			DBG("SPEER %d: Closing!\n", (*sp)->ct);
			close_speer(*sp);
		}
		sem_wait(&dmn.speers_mtx);
		sp = (struct rskt_dmn_speer **)l_pop_head(&dmn.speers);
		sem_post(&dmn.speers_mtx);
	}
	DBG("EXIT\n");
};

#ifdef __cplusplus
}
#endif

