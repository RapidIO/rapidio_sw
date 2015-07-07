/* Management implementation for FMDs operating in Master mode */
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

#include "riodp_mport_lib.h"
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"
#include "libcli.h"
#include "fmd_mgmt_master.h"
#include "fmd_mgmt_slave.h"
#include "fmd.h"
#include "fmd_cfg.h"
#include "liblog.h"
#include "fmd_dd.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fmd_mgmt fmp;

void master_process_hello_peer(struct fmd_peer *peer)
{
	struct fmd_cfg_ep *peer_ep;
	int add_to_list = 0;

	peer->p_pid = ntohl(peer->s2m->hello_rq.pid);
	peer->p_did = ntohl(peer->s2m->hello_rq.did);
	peer->p_did_sz = ntohl(peer->s2m->hello_rq.did_sz);
	peer->p_ct = ntohl(peer->s2m->hello_rq.ct);
	peer->p_hc = ntohl(peer->s2m->hello_rq.hc);

	peer_ep = find_cfg_ep_by_ct(peer->p_ct, fmd->cfg);
	sem_wait(&peer->tx_mtx);

	peer->m2s->msg_type = htonl(FMD_P_RESP_HELLO);
	memset(peer->m2s->hello_rsp.peer_name, MAX_P_NAME+1, 0);
	if (NULL == peer_ep) {
		snprintf(peer->m2s->hello_rsp.peer_name, (size_t)MAX_P_NAME,
			"%s", "REQUEST_DENIED!");
		peer->m2s->hello_rsp.pid = htonl(0);
		peer->m2s->hello_rsp.did = htonl(0);
		peer->m2s->hello_rsp.did_sz = htonl(0);
		peer->m2s->hello_rsp.ct = htonl(0);
		peer->m2s->hello_rsp.hc = htonl(0);
	} else {
		struct fmd_mport_info *mpi = &fmd->cfg->mport_info[
							fmd->cfg->mast_idx];
		strncpy(peer->m2s->hello_rsp.peer_name, mpi->ep->name, 
			MAX_P_NAME);
		peer->m2s->hello_rsp.pid = htonl(getpid());
		peer->m2s->hello_rsp.did = htonl(fmd->cfg->mast_devid);
		peer->m2s->hello_rsp.did_sz = htonl(fmd->cfg->mast_devid_sz);
		peer->m2s->hello_rsp.ct = htonl(mpi->ep->ports[0].ct);
		peer->m2s->hello_rsp.hc = htonl(0);
		add_to_list = 1;
	};
	peer->tx_buff_used = 1;
	peer->tx_rc = riodp_socket_send(peer->cm_skt_h, peer->tx_buff,
				FMD_P_M2S_CM_SZ);
	
	if (!peer->tx_rc && add_to_list) {
		peer->rx_alive = 1;
		sem_post(&peer->started);
		sem_wait(&fmp.peers_mtx);
		peer->li = l_add(&fmp.peers, peer->p_did, peer);
		sem_post(&fmp.peers_mtx);
	};
	sem_post(&peer->tx_mtx);
};

void peer_rx_req(struct fmd_peer *peer)
{
	peer->rx_buff_used = 1;
	do {
		peer->rx_rc = riodp_socket_receive(peer->cm_skt_h, 
			&peer->rx_buff, FMD_P_S2M_CM_SZ, 3*60*1000);
	} while ((peer->rx_rc) && ((errno == EINTR) || (errno == ETIME)));

	if (peer->rx_rc) {
		ERR("PEER RX(%x): %d (%d:%s)\n",
			peer->p_ct, peer->rx_rc, errno, strerror(errno));
		peer->rx_must_die = 1;
	};
};

void *peer_rx_loop(void *p_i)
{
	struct fmd_peer *peer = (struct fmd_peer *)p_i;

	while (!peer->rx_must_die && !peer->tx_rc && !peer->rx_rc) {
		peer_rx_req(peer);

		if (peer->rx_must_die || peer->rx_rc)
			break;

		switch (ntohl(peer->s2m->msg_type)) {
		case FMD_P_REQ_HELLO:
			master_process_hello_peer(peer);
			INFO("Peer(%x) RX HELLO Req %s %x %x %x %x %x\n",
				peer->p_ct,
				peer->s2m->hello_rq.peer_name,
				ntohl(peer->s2m->hello_rq.pid),
				ntohl(peer->s2m->hello_rq.did),
				ntohl(peer->s2m->hello_rq.did_sz),
				ntohl(peer->s2m->hello_rq.ct),
				ntohl(peer->s2m->hello_rq.hc));
			break;
		case FMD_P_RESP_MOD:
			/* Nothing to do for a modification response */
			INFO("Peer(%x) RX MOD Resp %x %x %x %x %x\n",
				peer->p_ct,
				ntohl(peer->s2m->mod_rsp.did),
				ntohl(peer->s2m->mod_rsp.did_sz),
				ntohl(peer->s2m->mod_rsp.ct),
				ntohl(peer->s2m->mod_rsp.hc),
				ntohl(peer->s2m->mod_rsp.rc));
			break;
		default:
			WARN("Peer(%x) RX Msg type %x\n", peer->p_ct,
					ntohl(peer->s2m->msg_type));
			break;
		};
	};

	peer->rx_alive = 0;
	if (peer->tx_buff_used) {
		riodp_socket_release_send_buffer(peer->cm_skt_h,
						peer->tx_buff);
		peer->tx_buff = NULL;
		peer->tx_buff_used = 0;
	};
	
	if (peer->rx_buff_used) {
		riodp_socket_release_receive_buffer(peer->cm_skt_h,
						peer->rx_buff);
		peer->rx_buff = NULL;
		peer->rx_buff_used = 0;
	};

	INFO("Peer(%x) EXITING\n", peer->p_ct);
	pthread_exit(NULL);
};


int start_new_peer(riodp_socket_t new_skt)
{
	int rc;
	struct fmd_peer *peer;
	uint32_t i;
	struct fmd_dd_dev_info devs[FMD_MAX_DEVS];
	uint32_t num_devs;

	peer = (struct fmd_peer *) malloc(sizeof(struct fmd_peer));

	peer->p_pid = 0;
	peer->p_did = 0;
	peer->p_did_sz = 0;
	peer->p_ct = 0;
	peer->p_hc = 0;
	peer->cm_skt = 0;
	peer->cm_skt_h = new_skt;
	sem_init(&peer->started, 0, 0);
	peer->got_hello = 0;
	sem_init(&peer->init_cplt_mtx, 0, 1);
	peer->init_cplt = 0;
	peer->rx_must_die = 0;
	peer->tx_buff_used = 0;
	peer->tx_rc = 0;
	sem_init(&peer->tx_mtx, 0, 1);
	sem_init(&peer->started, 0, 0);
	peer->rx_alive = 0;
	peer->rx_buff_used = 0;
	peer->rx_buff = malloc(4096);

	if (riodp_socket_request_send_buffer(new_skt, &peer->tx_buff)) {
		riodp_socket_close(&new_skt);
		goto fail;
	};

        rc = pthread_create(&peer->rx_thr, NULL, peer_rx_loop, (void*)peer);
	if (!rc)
		sem_wait(&peer->started);

	if (rc || !peer->rx_alive)
		goto fail;

	/* Send all device creation messages to new peer */
	if (NULL == fmd->dd)
		goto fail;

	/* FIXME: */
	/* The loop below could be more elegant, but it should do for now */
	while (!peer->init_cplt) {
		fmd_dd_atomic_copy(fmd->dd, fmd->dd_mtx, 
				&num_devs, devs, FMD_MAX_DEVS);

		for (i = 0; (i < num_devs) && !peer->restart_init && 
				!peer->tx_rc && !peer->rx_must_die; i++) {
			if (devs[i].is_mast_pt || 
					(devs[i].destID == peer->p_did))
				continue;
			sem_wait(&peer->tx_mtx);
			peer->m2s->msg_type = htonl(FMD_P_REQ_MOD);
			peer->m2s->dest_did = htonl(peer->p_did);
			peer->m2s->mod_rq.op = htonl(FMD_P_OP_ADD);
			peer->m2s->mod_rq.did = htonl(devs[i].destID);
			peer->m2s->mod_rq.did_sz = htonl(devs[i].destID_sz);
			peer->m2s->mod_rq.hc = htonl(devs[i].hc);
			peer->m2s->mod_rq.ct = htonl(devs[i].ct);
			peer->tx_rc = riodp_socket_send(peer->cm_skt_h, 
				peer->tx_buff, FMD_P_M2S_CM_SZ);
			sem_post(&peer->tx_mtx);
		};

		if (peer->tx_rc)
			goto fail;

		sem_wait(&peer->init_cplt_mtx);
		if (!peer->restart_init)
			peer->init_cplt = 1;
		sem_post(&peer->init_cplt_mtx);

		if (peer->init_cplt)
			break;

		for (i = 0; (i < num_devs) && !peer->restart_init && 
				!peer->tx_rc && !peer->rx_must_die; i++) {
			if (devs[i].is_mast_pt || 
					(devs[i].destID == peer->p_did))
				continue;
			sem_wait(&peer->tx_mtx);
			peer->m2s->msg_type = htonl(FMD_P_REQ_MOD);
			peer->m2s->dest_did = htonl(peer->p_did);
			peer->m2s->mod_rq.op = htonl(FMD_P_OP_DEL);
			peer->m2s->mod_rq.did = htonl(devs[i].destID);
			peer->m2s->mod_rq.did_sz = htonl(devs[i].destID_sz);
			peer->m2s->mod_rq.ct = htonl(devs[i].ct);
			peer->tx_rc = riodp_socket_send(peer->cm_skt_h, 
				peer->tx_buff, FMD_P_M2S_CM_SZ);
			sem_post(&peer->tx_mtx);
		};
		if (!peer->tx_rc)
			goto fail;
		sem_wait(&peer->init_cplt_mtx);
		peer->restart_init = 0;
		peer->init_cplt = 0;
		sem_post(&peer->init_cplt_mtx);
	};

	return 0;
fail:
	return 1;
};

void *mast_acc(void *unused)
{
	int rc = 1;
	riodp_socket_t new_skt = NULL;

	fmp.acc.mb_valid = 0;
	fmp.acc.cm_acc_valid = 0;
	rc = riodp_mbox_create_handle(fmp.acc.mp_num, 0, &fmp.acc.mb);
	if (rc) {
		ERR("riodp_mbox_create ERR %d\n", rc);
		goto exit;
	};
	fmp.acc.mb_valid = 1;
	sem_init(&fmp.acc.mb_mtx, 0, 1);

	rc = riodp_socket_socket(fmp.acc.mb, &fmp.acc.cm_acc_h);
	if (rc) {
		ERR("riodp_socket_socket ERR %d %d: %s\n", rc, errno,
			strerror(errno));
		goto exit;
	};

	rc = riodp_socket_bind(fmp.acc.cm_acc_h, fmp.acc.cm_skt_num);
	if (rc) {
		ERR("riodp_socket_bind() ERR %d errno %d: %s\n", rc, errno,
			strerror(errno));
		goto exit;
	};
	fmp.acc.cm_acc_valid = 1;

	rc = riodp_socket_listen(fmp.acc.cm_acc_h);
	if (rc) {
		ERR("riodp_socket_listen() ERR %d %d: %s\n", rc, errno,
			strerror(errno));
		goto exit;
	};
	fmp.acc.acc_alive = 1;
	sem_post(&fmp.acc.started);

	while (!fmp.acc.acc_must_die) {
		if (NULL == new_skt) {
			rc = riodp_socket_socket(fmp.acc.mb, &new_skt);
			if (rc) {
				ERR("socket() ERR %d\n", rc);
				break;
			};
		};
		do {
			rc = riodp_socket_accept(fmp.acc.cm_acc_h,
				&new_skt, 3*60*1000);
		} while (rc && ((errno == ETIME) || (errno == EINTR)));

		if (rc) {
			ERR("riodp_accept() ERR %d\n", rc);
			break;
		};

		if (fmp.acc.acc_must_die) {
			riodp_socket_close(&new_skt);
			continue;
		};

		if (start_new_peer(new_skt)) {
			WARN("Could not start peer after accept\n", rc);
			free(new_skt);
		};
			
		new_skt = NULL;
	}

	if (NULL != new_skt)
		free((void *)new_skt);

exit:
	fmp.acc.acc_alive = 0;
	if (fmp.acc.cm_acc_valid) {
		riodp_socket_close(&fmp.acc.cm_acc_h);
		fmp.acc.cm_acc_valid = 0;
	};
	CRIT("\nFMD Peer Connection Handler EXITING\n");
	sem_post(&fmp.acc.started);
	pthread_exit(unused);
};

int start_peer_mgmt_master(uint32_t mast_acc_skt_num, uint32_t mp_num, 
		uint32_t mast_did)
{
	uint32_t rc;

	fmp.acc.cm_skt_num = mast_acc_skt_num;
	fmp.acc.mp_num = mp_num;

	sem_init(&fmp.acc.started, 0, 0);
	sem_init(&fmp.peers_mtx, 0, 1);

        rc = pthread_create(&fmp.acc.acc, NULL, mast_acc, NULL);
        if (rc)
                goto fail;

        sem_wait(&fmp.acc.started);
	if (!fmp.acc.acc_alive)
		goto fail;

	return 0;
fail:
	return 1;
};

int start_peer_mgmt(uint32_t mast_acc_skt_num, uint32_t mp_num, 
		uint32_t mast_did, uint32_t master)
{
	uint32_t rc = 0;

	fmp.acc.cm_skt_num = mast_acc_skt_num;
	fmp.acc.mp_num = mp_num;
	fmp.mode = master;

	if (master)
		rc = start_peer_mgmt_master(mast_acc_skt_num, mp_num, mast_did);
	else
		rc = start_peer_mgmt_slave(mast_acc_skt_num, mast_did, mp_num, 
			&fmp.slv, fmd->fd);

	if (rc)
		shutdown_mgmt();
	return rc;
};
		
void shutdown_master_mgmt(void)
{
	struct fmd_peer *peer;
	void *unused;

	/* Shut down accept thread first... */
	if (fmp.acc.acc_alive) {
		fmp.acc.acc_must_die = 1;
		pthread_kill(fmp.acc.acc, SIGHUP);
		pthread_join(fmp.acc.acc, &unused);
	};

	/* Then kill every listening thread, if they're not dead already */

	peer = (struct fmd_peer *)l_pop_head(&fmp.peers);
	while (peer != NULL) {
		peer->li = NULL;
		if (!peer->rx_alive) {
			/* Should never get here */
			DBG("Peer %d dead but still in list?", peer->p_did);
		};

		if (peer->tx_buff_used) {
			riodp_socket_release_send_buffer(peer->cm_skt_h,
							peer->tx_buff);
			peer->tx_buff = NULL;
			peer->tx_buff_used = 0;
		};
	
		if (peer->rx_buff_used) {
			riodp_socket_release_receive_buffer(peer->cm_skt_h,
							peer->rx_buff);
			peer->rx_buff = NULL;
			peer->rx_buff_used = 0;
		};
	
		pthread_kill(peer->rx_thr, SIGHUP);
		pthread_join(peer->rx_thr, &unused);
		free(peer);
		peer = (struct fmd_peer *)l_pop_head(&fmp.peers);
	};
	/* Cleanup CM accept socket */
	if (fmp.acc.cm_acc_valid) {
		riodp_socket_close(&fmp.acc.cm_acc_h);
		fmp.acc.cm_acc_valid = 0;
	};

	/* Cleanup mailbox */
	if (fmp.acc.mb_valid) {
		riodp_mbox_destroy_handle(&fmp.acc.mb);
		fmp.acc.mb_valid = 0;
	};

};

void shutdown_mgmt(void)
{
	if (fmp.mode)
		shutdown_master_mgmt();
	else
		shutdown_slave_mgmt();
};


#ifdef __cplusplus
}
#endif
