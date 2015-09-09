/* Management implementation for FMDs in Slave mode. */
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

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_sock.h>

#include "libcli.h"
#include "liblog.h"
#include "libfmdd.h"
#include "fmd_mgmt_master.h"
#include "fmd_mgmt_slave.h"
#include "fmd_state.h"
#include "fmd_app_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIO_MPORT_DEV_PATH "/dev/rio_mport"

struct fmd_slave *slv;

int send_slave_hello_message(void)
{
	sem_wait(&slv->tx_mtx);
	slv->s2m->msg_type = htonl(FMD_P_REQ_HELLO);
	slv->s2m->src_did = htonl(slv->mast_did);
	memset(slv->s2m->hello_rq.peer_name, 0, MAX_P_NAME+1);
	strncpy(slv->s2m->hello_rq.peer_name, fmd->cfg->eps[0].name,
		MAX_P_NAME);
	slv->s2m->hello_rq.pid = htonl(getpid());
	slv->s2m->hello_rq.did =
		htonl(fmd->cfg->eps[0].ports[0].devids[FMD_DEV08].devid);
	slv->s2m->hello_rq.did_sz = htonl(FMD_DEV08);
	slv->s2m->hello_rq.ct = htonl(fmd->cfg->eps[0].ports[0].ct);
	slv->s2m->hello_rq.hc =
		htonl(fmd->cfg->eps[0].ports[0].devids[FMD_DEV08].hc);

	slv->tx_buff_used = 1;
	slv->tx_rc |= riomp_sock_send(slv->skt_h, slv->tx_buff,
				FMD_P_S2M_CM_SZ);
	if (slv->tx_rc)
		goto fail;
	sem_post(&slv->tx_mtx);
	
	return 0;
fail:
	return 1;
};

int add_device_to_dd(uint32_t ct, uint32_t did, uint32_t did_sz, uint32_t hc,
		uint32_t is_mast_pt, uint32_t flag, char *name)
{
	uint32_t idx, found_one = 0;

	if ((NULL == fmd->dd) || (NULL == fmd->dd_mtx))
		goto fail;

	sem_wait(&fmd->dd_mtx->sem);
	for (idx = 0; (idx < fmd->dd->num_devs) && !found_one; idx++) {
		if ((fmd->dd->devs[idx].ct == ct) && 
				(fmd->dd->devs[idx].destID == did)) {
			fmd->dd->devs[idx].destID_sz = did_sz;
			fmd->dd->devs[idx].hc = hc;
			fmd->dd->devs[idx].is_mast_pt = is_mast_pt;
			fmd->dd->devs[idx].flag |= flag;
			if (is_mast_pt)
				fmd->dd->loc_mp_idx = idx;
			strncpy(fmd->dd->devs[idx].name, name, FMD_MAX_NAME);
			found_one = 1;
		};
	};

	if (found_one)
		goto exit;

	if (fmd->dd->num_devs >= FMD_MAX_DEVS)
		goto fail;

	idx = fmd->dd->num_devs;
	memset(&fmd->dd->devs[idx], 0, sizeof(fmd->dd->devs[0]));
	fmd->dd->devs[idx].ct = ct;
	fmd->dd->devs[idx].destID = did;
	fmd->dd->devs[idx].destID_sz = did_sz;
	fmd->dd->devs[idx].hc = hc;
	fmd->dd->devs[idx].is_mast_pt = is_mast_pt;
	fmd->dd->devs[idx].flag = flag;
	strncpy(fmd->dd->devs[idx].name, name, FMD_MAX_NAME);
	if (is_mast_pt)
		fmd->dd->loc_mp_idx = idx;
	fmd->dd->num_devs++;
		
exit:
	sem_post(&fmd->dd_mtx->sem);
	return 0;
fail:
	sem_post(&fmd->dd_mtx->sem);
	return 1;
};
	
int del_device_from_dd(uint32_t ct, uint32_t did)
{
	uint32_t idx, found_idx, found_one = 0;

	if ((NULL == fmd->dd) || (NULL == fmd->dd_mtx))
		goto fail;

	sem_wait(&fmd->dd_mtx->sem);
	for (idx = 0; (idx < fmd->dd->num_devs) && !found_one; idx++) {
		if ((fmd->dd->devs[idx].ct == ct) && 
				(fmd->dd->devs[idx].destID == did)) {
			found_idx = idx;
			found_one = 1;
		};
	};

	if (!found_one)
		goto fail;

	memset(&fmd->dd->devs[found_idx], 0, sizeof(fmd->dd->devs[0]));

	for (idx = found_idx; (idx + 1) < fmd->dd->num_devs; idx++)
		fmd->dd->devs[idx] = fmd->dd->devs[idx+1];

	if (fmd->dd->num_devs >= FMD_MAX_DEVS)
		goto fail;

	fmd->dd->num_devs--;
		
	sem_post(&fmd->dd_mtx->sem);
	return 0;
fail:
	sem_post(&fmd->dd_mtx->sem);
	return 1;
};
	
void slave_process_mod(void)
{
	uint32_t rc = 0xFFFFFFFF;

	sem_wait(&slv->tx_mtx);

	slv->s2m->msg_type = slv->m2s->msg_type | htonl(FMD_P_MSG_RESP);
	slv->s2m->mod_rsp.did = slv->m2s->mod_rq.did;
	slv->s2m->mod_rsp.did_sz = slv->m2s->mod_rq.did_sz;
	slv->s2m->mod_rsp.hc = slv->m2s->mod_rq.hc;
	slv->s2m->mod_rsp.ct = slv->m2s->mod_rq.ct;
	slv->s2m->mod_rsp.is_mp = slv->m2s->mod_rq.is_mp;
	slv->s2m->mod_rsp.flag = slv->m2s->mod_rq.flag;
	slv->s2m->mod_rsp.rc = 0;

	switch (ntohl(slv->m2s->mod_rq.op)) {
	case FMD_P_OP_ADD: rc = riomp_mgmt_device_add(slv->mp_hnd,
				ntohl(slv->m2s->mod_rq.did), 
				ntohl(slv->m2s->mod_rq.hc), 
				ntohl(slv->m2s->mod_rq.ct),
				(const char *)slv->m2s->mod_rq.name);
		if ((rc != EEXIST) && rc) {
			slv->s2m->mod_rsp.rc = htonl(rc);
			break;
		};
		rc = add_device_to_dd( ntohl(slv->m2s->mod_rq.ct),
				ntohl(slv->m2s->mod_rq.did), 
				FMD_DEV08, 
				ntohl(slv->m2s->mod_rq.hc),
				ntohl(slv->m2s->mod_rq.is_mp),
				ntohl(slv->m2s->mod_rq.flag),
				slv->m2s->mod_rq.name);
		slv->s2m->mod_rsp.rc = htonl(rc);
		break;
				 
	case FMD_P_OP_DEL: 
		/* FIXME: Commented out for now as this can kill the platform.
			rc = riomp_mgmt_device_del(slv->fd, 
				ntohl(slv->m2s->mod_rq.did), 
				ntohl(slv->m2s->mod_rq.hc), 
				ntohl(slv->m2s->mod_rq.ct));
		if (rc) {
			slv->s2m->mod_rsp.rc = htonl(rc);
			break;
		};
		FIXME: END COMMENT
		*/
		rc = del_device_from_dd(ntohl(slv->m2s->mod_rq.ct),
				ntohl(slv->m2s->mod_rq.did));
		slv->s2m->mod_rsp.rc = htonl(rc);
		break;
	default: slv->s2m->mod_rsp.rc = 0xFFFFFFFF;
	};

	slv->tx_buff_used = 1;
	slv->tx_rc |= riomp_sock_send(slv->skt_h, slv->tx_buff, 
		FMD_P_S2M_CM_SZ);
	sem_post(&slv->tx_mtx);
	if (!rc)
		fmd_notify_apps();
};

void slave_process_fset(void)
{
        uint32_t i;
	uint32_t did = ntohl(slv->m2s->fset.did); 
	uint32_t ct = ntohl(slv->m2s->fset.ct);
	uint32_t flag = ntohl(slv->m2s->fset.flag);

        if ((NULL == fmd->dd) || (NULL == fmd->dd_mtx))
                return;

        sem_wait(&fmd->dd_mtx->sem);
	for (i = 0; i < fmd->dd->num_devs; i++) {
        	if ((fmd->dd->devs[i].destID == did) &&
        			(fmd->dd->devs[i].ct == ct)) {
                        fmd->dd->devs[i].flag = flag;
			break;
		};
	};
        sem_post(&fmd->dd_mtx->sem);

	fmd_notify_apps();
};

void cleanup_slave(void)
{
	if (NULL == slv)
		return;

	if (slv->tx_buff_used) {
		riomp_sock_release_send_buffer(slv->skt_h, slv->tx_buff);
		slv->tx_buff = NULL;
		slv->tx_buff_used = 0;
	};
	
	if (slv->rx_buff_used) {
		riomp_sock_release_receive_buffer(slv->skt_h, slv->rx_buff);
		slv->rx_buff = NULL;
		slv->rx_buff_used = 0;
	};
	
	if (slv->skt_valid) {
		int rc = riomp_sock_close(&slv->skt_h);
		if (rc) {
			ERR("Close RC is %d: %s\n", rc, strerror(errno));
		};
		slv->skt_valid = 0;
	};

	if (slv->mb_valid) {
		riomp_sock_mbox_destroy_handle(&slv->mb);
		slv->mb_valid = 0;
	};
};

void slave_rx_req(void)
{
	slv->rx_buff_used = 1;
	do {
		slv->rx_rc = riomp_sock_receive(slv->skt_h, 
			&slv->rx_buff, FMD_P_M2S_CM_SZ, 0);
	} while ((slv->rx_rc) && ((errno == EINTR) || (errno == ETIME)));

	if (slv->rx_rc) {
		ERR("SLV RX: %d (%d:%s)\n",
			slv->rx_rc, errno, strerror(errno));
		slv->slave_must_die = 1;
	};
};

void *mgmt_slave(void *unused)
{
	slv->slave_alive = 1;
	sem_post(&slv->started);

	while (!slv->slave_must_die && !slv->tx_rc && !slv->rx_rc) {
		slave_rx_req();

		if (slv->slave_must_die || slv->rx_rc || slv->tx_rc)
			break;

		switch (ntohl(slv->m2s->msg_type)) {
		case FMD_P_RESP_HELLO:
			slv->m_h_rsp = slv->m2s->hello_rsp;
			if (!slv->m2s->hello_rsp.pid &&
			!slv->m2s->hello_rsp.did && !slv->m2s->hello_rsp.ct) {
				ERR("Hello pi, did, ct all 0!\n");
				goto fail;
			}
			slv->m_h_resp_valid = 1;
			break;
		case FMD_P_REQ_MOD:
			slave_process_mod();
			break;
		case FMD_P_REQ_FSET:
			slave_process_fset();
			break;
		default:
			WARN("Slave RX Msg type %x\n", 
					ntohl(slv->m2s->msg_type));
			break;
		};
	};

fail:
	cleanup_slave();
	INFO("FMD Slave EXITING\n");
	pthread_exit(unused);
};

int start_peer_mgmt_slave(uint32_t mast_acc_skt_num, uint32_t mast_did,
                        uint32_t  mp_num, struct fmd_slave *slave, riomp_mport_t hnd)
{
	int rc = 1;
	int conn_rc;

	slv = slave;
	slv->mp_hnd = hnd;
	sem_init(&slv->started, 0, 0);
	slv->slave_alive = 0;
	slv->slave_must_die = 0;
	slv->mp_num = mp_num;
	slv->mast_did = mast_did;
	slv->mast_skt_num = mast_acc_skt_num;
	slv->mb_valid = 0;
	slv->skt_valid = 0;
	sem_init(&slv->tx_mtx, 0, 1);
	slv->tx_buff_used = 0;
	slv->tx_rc = 0;
	slv->tx_buff = NULL;
	slv->rx_buff_used = 0;
	slv->rx_rc = 0;
	slv->rx_buff = NULL;
	slv->m_h_resp_valid = 0;

	rc = riomp_sock_mbox_create_handle(slv->mp_num, 0, &slv->mb);
	if (rc) {
		ERR("riodp_mbox_create ERR %d\n", rc);
		goto fail;
	};
	slv->mb_valid = 1;

	do {
		rc = riomp_sock_socket(slv->mb, &slv->skt_h);
		if (rc) {
			ERR("riomp_sock_socket ERR %d\n", rc);
			goto fail;
		};

		conn_rc = riomp_sock_connect(slv->skt_h, slv->mast_did, 0,
					fmd->cfg->mast_cm_port);
		if (!conn_rc)
			break;

		ERR("riomp_sock_connect ERR %d\n", conn_rc);
		if ((ETIME == errno) || (EPERM == errno)) {
			struct timespec dly, rem;

 			dly.tv_sec = 0;
			dly.tv_nsec = 500000000; /* 500 msec */
			rem.tv_sec = 0;
			rem.tv_nsec = 0;

			do {
				rc = nanosleep(&dly, &rem);
				dly = rem;
				rem.tv_sec = 0;
				rem.tv_nsec = 0;
			} while (rc && (errno == EINTR));
		};
		rc = riomp_sock_close(&slv->skt_h);
		if (rc) {
			ERR("riomp_sock_close ERR %d\n", rc);
		};
	} while (conn_rc);

	if (conn_rc) {
		ERR("riomp_sock_connect ERR %d\n", conn_rc);
		goto fail;
	};

	slv->skt_valid = 1;
	
        if (riomp_sock_request_send_buffer(slv->skt_h, &slv->tx_buff)) {
                riomp_sock_close(&slv->skt_h);
                goto fail;
        };
	slv->rx_buff = malloc(4096);

        rc = pthread_create(&slv->slave_thr, NULL, mgmt_slave, NULL);
	if (rc) {
		ERR("pthread_create ERR %d\n", rc);
		goto fail;
	};
	sem_wait(&slv->started);

	rc = send_slave_hello_message();
	if (rc) {
		ERR("hello message tx fail ERR %d\n", rc);
		goto fail;
	};

	return rc;
fail:
	return 1;
	
};
		
void shutdown_slave_mgmt(void)
{
	if (slv->slave_alive) {
		slv->slave_must_die = 1;
		pthread_kill(slv->slave_thr, SIGHUP);
		pthread_join(slv->slave_thr, NULL);
		slv->slave_alive = 0;
	};
	cleanup_slave();
};

void update_master_flags_from_peer(void)
{
	uint32_t did, did_sz, ct, flag, i;

	if ((NULL == fmd->dd) || (NULL == fmd->dd_mtx))
		return;

	sem_wait(&fmd->dd_mtx->sem);

	if (fmd->dd->loc_mp_idx >= fmd->dd->num_devs) {
		sem_post(&fmd->dd_mtx->sem);
		return;
	}

	i = fmd->dd->loc_mp_idx;
	did = fmd->dd->devs[i].destID;
	did_sz = fmd->dd->devs[i].destID_sz;
	ct = fmd->dd->devs[i].ct;
	flag = (fmd->dd->devs[i].flag & ~FMDD_FLAG_OK_MP) | FMDD_FLAG_OK;

	sem_post(&fmd->dd_mtx->sem);
	sem_wait(&slv->tx_mtx);

	slv->s2m->msg_type = htonl(FMD_P_REQ_FSET);
	slv->s2m->src_did = htonl(did);
	slv->s2m->fset.did = htonl(did);
	slv->s2m->fset.did_sz = htonl(did_sz);
	slv->s2m->fset.ct = htonl(ct);
	slv->s2m->fset.flag = htonl(flag);
	
	slv->tx_buff_used = 1;
	slv->tx_rc |= riomp_sock_send(slv->skt_h, slv->tx_buff,
				FMD_P_S2M_CM_SZ);
	sem_post(&slv->tx_mtx);
};


#ifdef __cplusplus
}
#endif
