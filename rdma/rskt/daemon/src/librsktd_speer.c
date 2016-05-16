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
	DBG("SPEER %d: Waiting for request", speer->ct);

	do {
		rc = riomp_sock_receive(speer->cm_skt_h, 
			&speer->rx_buff, RSKTD_CM_MSG_SIZE, 0);
	} while (rc && !speer->i_must_die && !dmn.all_must_die &&
		((errno == EINTR) || (errno == ETIME) || (errno == EAGAIN)));

	if (rc) {
		ERR("SPEER %d: riomp_sock_receive: %d (%d:%s)",
			speer->ct, rc, errno, strerror(errno));
		speer->i_must_die = 1;
	} else {
		DBG("SPEER %d: riomp_sock_receive: %s SEQ %d",
			speer->ct, RSKTD_REQ_STR(ntohl(speer->req->msg_type)),
			ntohl(speer->req->msg_seq));
	};

	return rc;
};

void rsktd_prep_resp(struct librsktd_unified_msg *msg)
{
	memset((*msg->sp)->req, 0, sizeof((*msg->sp)->req->unusable));
	memset(msg->dresp, 0, DMN_RESP_SZ);
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
	DBG("\n\tSPEER %d: closing speer!\n", speer->ct);

	sem_post(&speer->started);
	sleep(0);

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

	DBG("\n\tSPEER %d: killing speer thread!\n", speer->ct);
	if (speer->alive)
		pthread_kill(speer->s_rx, SIGUSR1);
};

void speer_loop_sig_handler(int sig)
{
        if (sig)
                return;
}

void *speer_rx_loop(void *p_i)
{
	struct rskt_dmn_speer *speer = (struct rskt_dmn_speer *)p_i;
	int rc = -1;
        struct sigaction sigh;
	struct librsktd_unified_msg *msg = NULL;

	INFO("\n\tSPEER %d: Starting!\n", speer->ct);

	rc = pthread_detach(speer->s_rx);
	if (rc) {
                WARN("pthread_detach rc %d", rc);
	};
	sem_post(&speer->started);
	DBG("\n\tSPEER %d: Running\n", speer->ct);

        memset(&sigh, 0, sizeof(sigh));
        sigh.sa_handler = speer_loop_sig_handler;
        sigaction(SIGUSR1, &sigh, NULL);

	while (!speer->i_must_die && !speer->comm_fail) {
		msg = alloc_msg(0, RSKTD_PROC_SREQ, RSKTD_SPEER_SEQ_DREQ);

		msg->dreq = alloc_dreq();
		msg->dresp = alloc_dresp();
		msg->sp = speer->self_ref;

		rc = speer_rx_req(speer);

		if (speer->i_must_die || speer->comm_fail || dmn.all_must_die 
				|| rc)
			break;

		rsktd_prep_resp(msg);

		DBG("SPEER %d: Req %s Seq %x", speer->ct,
			RSKTD_REQ_STR(ntohl(speer->req->msg_type)), 
			ntohl(speer->req->msg_seq));
		switch (msg->msg_type) {
		case RSKTD_HELLO_REQ:
		case RSKTD_CONNECT_REQ:
		default:
			msg->proc_type = RSKTD_PROC_SREQ;
			msg->proc_stage = RSKTD_SPEER_SEQ_DREQ;
			break;
		case RSKTD_CLOSE_REQ:
			msg->proc_type = RSKTD_PROC_S2A;
			msg->proc_stage = RSKTD_S2A_SEQ_DREQ;
			break;
		};
		DBG( "SPEER %d: enqueue : REQ %s RESP %s SEQ %d "
				"PROC %s STAGE %s",
			speer->ct, RSKTD_REQ_STR(msg->msg_type),
			RSKTD_RESP_STR(ntohl(msg->dresp->msg_type)),
        		ntohl(msg->dresp->msg_seq),
			UMSG_TYPE_TO_STR(msg),
			UMSG_PROC_TO_STR(msg));
		enqueue_mproc_msg(msg);
	};

	DBG("\n\tSPEER %d: Exiting, startin to cleanup!\n", speer->ct);
	speer->comm_fail = 1;
	speer->alive = 0;

	if (NULL != msg)
		dealloc_msg(msg);

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
	int rc, i;
	struct rskt_dmn_speer *speer = NULL;

	for (i = 0; i < MAX_PEER; i++) {
		if (dmn.speers[i].alive || dmn.speers[i].cm_skt_h_valid)
			continue;
		speer = &dmn.speers[i];
		break;
	};

	if (NULL == speer) {
		ERR("Max peers is %d, all in use now.", MAX_PEER);
		return;
	};

	speer->self_ref = &speer->self_ref_ref;
	speer->self_ref_ref = speer;

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
	speer->rx_buff = calloc(1, 4*1024);
	speer->tx_buff_used = 0;

	if (riomp_sock_request_send_buffer(new_socket, &speer->tx_buff)) {
		riomp_sock_close(&new_socket);
	};
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


void halt_speer_tx_loop(void)
{
	if (dmn.speer_tx_alive) {
        	pthread_kill(dmn.speer_tx_thread, SIGUSR1);
        	pthread_join(dmn.speer_tx_thread, NULL);
	};
};

/* Sends responses to all speers */
void *speer_tx_loop(void *unused)
{
	struct librsktd_unified_msg *msg = NULL;
	struct rskt_dmn_speer *s = NULL;
        struct sigaction sigh;
        char my_name[16] = {0};

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "SPEER_TX_LOOP");
        if (!dmn.speer_tx_thread) {
        	WARN("Null thread handle\n");
        }
        pthread_setname_np(dmn.speer_tx_thread, my_name);

        memset(&sigh, 0, sizeof(sigh));
        sigh.sa_handler = speer_loop_sig_handler;
        sigaction(SIGUSR1, &sigh, NULL);

	dmn.speer_tx_alive = 1;

	sem_post(&dmn.speer_tx_loop_started);

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
		if(dealloc_msg(msg))
			DBG("Dealloc_msg FAILED\n");
	};
	dmn.speer_tx_alive = 0;
	dmn.all_must_die = 1;
	sem_post(&dmn.graceful_exit);
	pthread_exit(unused);
};

void close_all_speers(void)
{
	int i;

	DBG("ENTER\n");

	for (i = 0; i < MAX_PEER; i++) {
		if (dmn.speers[i].alive)
			close_speer(&dmn.speers[i]);
	}
	DBG("EXIT\n");
};

int start_speer_tx_thread(void) 
{
	int rc;

	sem_init(&dmn.speer_tx_loop_started, 0, 0);
	dmn.speer_tx_alive = 0;

	sem_init(&dmn.speer_tx_mutex, 0, 1);
	sem_init(&dmn.speer_tx_cnt, 0, 0);
	l_init(&dmn.speer_tx_q);
        rc = pthread_create(&dmn.speer_tx_thread, NULL, speer_tx_loop, NULL);
	if (rc) {
		ERR("Failed to create speer_tx_thread: %s\n", strerror(errno));
		goto fail;
	}
	
	sem_wait(&dmn.speer_tx_loop_started);
	return 0;
fail:
	return rc;
};

void speer_conn_sig_handler(int sig)
{
        if (sig)
                return;
}

void halt_speer_conn_handler(void)
{
	if (dmn.speer_conn_alive) {
		pthread_kill(dmn.speer_conn_thread, SIGUSR1);
		pthread_join(dmn.speer_conn_thread, NULL);
	};
};

void *speer_conn(void *unused)
{
	int rc = 1;
	riomp_sock_t new_socket = NULL;
	char my_name[16] = {0};
        struct sigaction sigh;
	
	dmn.speer_conn_alive = 0;
	if (init_mport_and_mso_ms()) 
		goto exit;

	dmn.speer_conn_alive = 1;

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "SPEER_CONN");
	pthread_setname_np(dmn.speer_conn_thread, my_name);

        memset(&sigh, 0, sizeof(sigh));
        sigh.sa_handler = speer_conn_sig_handler;
        sigaction(SIGUSR1, &sigh, NULL);

	sem_post(&dmn.speer_conn_thr_started);

	INFO("\rALIVE.");
	while (!dmn.all_must_die) {
		if (NULL == new_socket) {
			rc = riomp_sock_socket(dmn.mb, &new_socket);
			if (rc) {
				CRIT("speer_conn: socket() ERR %d\n", rc);
				break;
			};
		};
		DBG("accepting...\n");
		do {
			rc = riomp_sock_accept(dmn.cm_acc_h, &new_socket, 3*60*1000);
		} while (!dmn.all_must_die && rc && 
		((errno == ETIME) || (errno == EINTR) || (EAGAIN == errno)));

		if (dmn.all_must_die)
			continue;

		if (rc) {
			ERR("speer_conn: riodp_accept() RC %d errno %d:%s\n",
				rc, errno, strerror(errno));
			break;
		}

		DBG("start new SPEER\n");
		start_new_speer(new_socket);

		new_socket = NULL;
	}

exit:
	dmn.speer_conn_alive = 0;
	if (NULL != new_socket)
		riomp_sock_close(&new_socket);
	CRIT("\nRSKTD Peer Connection Handler EXITING\n");
	sem_post(&dmn.speer_conn_thr_started);
	dmn.all_must_die = 1;
	sem_post(&dmn.graceful_exit);
	pthread_exit(unused);
};

int start_speer_conn(uint32_t cm_skt, uint32_t mpnum, 
			uint32_t num_ms, uint32_t ms_size, uint32_t skip_ms)
{
	int rc;

	dmn.cm_skt = cm_skt;
	dmn.mpnum = mpnum;
	dmn.skip_ms = skip_ms;
	dmn.num_ms = num_ms;
	dmn.ms_size = ms_size;
	sem_init(&dmn.speer_conn_thr_started, 0, 0);

        rc = pthread_create(&dmn.speer_conn_thread, NULL, speer_conn, NULL);
	if (rc)
		goto fail;
	
	sem_wait(&dmn.speer_conn_thr_started);
fail:
	return rc;
};
int start_speer_handler(uint32_t cm_skt, uint32_t mpnum,
			uint32_t num_ms, uint32_t ms_size, uint32_t skip_ms)
{
	int rc;

	rc = start_speer_tx_thread();
	if (rc)
		return rc;

	return start_speer_conn(cm_skt, mpnum, num_ms, ms_size, skip_ms);
};

void halt_speer_handler(void)
{
	dmn.all_must_die = 1;

	halt_speer_conn_handler();
	close_all_speers();
	halt_speer_tx_loop();
};

#ifdef __cplusplus
}
#endif

