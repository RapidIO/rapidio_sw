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

#include "riodp_mport_lib.h"
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"
#include "libcli.h"
#include "liblog.h"
#include "fmd_mgmt_master.h"
#include "fmd_mgmt_slave.h"
#include "fmd_state.h"
#include "fmd_app_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIO_MPORT_DEV_PATH "/dev/rio_mport"

struct fmd_slave *slv;

int slave_hello_message_exchange(void)
{
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
	slv->tx_rc = riodp_socket_send(slv->skt_h, slv->tx_buff,
				FMD_P_S2M_CM_SZ);
	if (slv->tx_rc)
		goto fail;

	slv->rx_buff_used = 1;
	slv->rx_rc = riodp_socket_receive(slv->skt_h, &slv->rx_buff,
				FMD_P_M2S_CM_SZ, 10);
	
	if (slv->rx_rc || (htonl(FMD_P_RESP_HELLO) != slv->m2s->msg_type))
		goto fail;

	slv->m_h_rsp = slv->m2s->hello_rsp;
	if (!slv->m2s->hello_rsp.pid && !slv->m2s->hello_rsp.did 
			&& !slv->m2s->hello_rsp.ct)
		goto fail;
	
	return 0;
fail:
	return 1;
};

void slave_process_mod(void )
{
	uint32_t rc;

	slv->s2m->mod_rsp.did = slv->m2s->mod_rq.did;
	slv->s2m->mod_rsp.did_sz = slv->m2s->mod_rq.did_sz;
	slv->s2m->mod_rsp.hc = slv->m2s->mod_rq.hc;
	slv->s2m->mod_rsp.ct = slv->m2s->mod_rq.ct;

	switch (ntohl(slv->m2s->mod_rq.op)) {
	case FMD_P_OP_ADD: rc = riodp_device_add(slv->fd, 
				ntohl(slv->m2s->mod_rq.did), 
				ntohl(slv->m2s->mod_rq.hc), 
				ntohl(slv->m2s->mod_rq.ct),
				(const char *)slv->m2s->mod_rq.name);
		slv->s2m->mod_rsp.rc = htonl(rc);
		break;
				 
	case FMD_P_OP_DEL: rc = riodp_device_del(slv->fd, 
				ntohl(slv->m2s->mod_rq.did), 
				ntohl(slv->m2s->mod_rq.hc), 
				ntohl(slv->m2s->mod_rq.ct));
		slv->s2m->mod_rsp.rc = htonl(rc);
		break;
	default: slv->s2m->mod_rsp.rc = 0xFFFFFFFF;
	};

	slv->tx_buff_used = 1;
	slv->tx_rc = riodp_socket_send(slv->skt_h, slv->tx_buff, 
		FMD_P_S2M_CM_SZ);
	fmd_notify_apps();
};

void close_slave(void)
{
	if (NULL == slv)
		return;

	if (slv->tx_buff_used) {
		riodp_socket_release_send_buffer(slv->skt_h, slv->tx_buff);
		slv->tx_buff = NULL;
		slv->tx_buff_used = 0;
	};
	
	if (slv->rx_buff_used) {
		riodp_socket_release_receive_buffer(slv->skt_h, slv->rx_buff);
		slv->rx_buff = NULL;
		slv->rx_buff_used = 0;
	};
	
	if (slv->mb_valid) {
		riodp_mbox_destroy_handle(&slv->mb);
		slv->mb_valid = 0;
	};
};

void slave_rx_req(void)
{
	slv->rx_buff_used = 1;
	do {
		slv->rx_rc = riodp_socket_receive(slv->skt_h, 
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

		if (slv->slave_must_die || slv->rx_rc)
			break;

		switch (ntohl(slv->m2s->msg_type)) {
		case FMD_P_REQ_MOD:
			slave_process_mod();
			break;
		default:
			WARN("Slave RX Msg type %x\n", 
					ntohl(slv->m2s->msg_type));
			break;
		};
	};

	close_slave();
	INFO("FMD Slave EXITING\n");
	pthread_exit(unused);
};

extern int start_peer_mgmt_slave(uint32_t mast_acc_skt_num, uint32_t mast_did,
                        uint32_t  mp_num, struct fmd_slave *slave, int fd)
{
	int rc = 1;

	slv = slave;
	slv->fd = fd;
	sem_init(&slv->started, 0, 0);
	slv->slave_alive = 0;
	slv->slave_must_die = 0;
	slv->mp_num = mp_num;
	slv->mast_did = mast_did;
	slv->mast_skt_num = mast_acc_skt_num;
	slv->mb_valid = 0;
	slv->skt_valid = 0;
	slv->tx_buff_used = 0;
	slv->tx_rc = 0;
	slv->tx_buff = NULL;
	slv->rx_buff_used = 0;
	slv->rx_rc = 0;
	slv->rx_buff = NULL;

	rc = riodp_mbox_create_handle(slv->mp_num, 0, &slv->mb);
	if (rc) {
		ERR("riodp_mbox_create ERR %d\n", rc);
		goto fail;
	};
	slv->mb_valid = 1;

	rc = riodp_socket_socket(slv->mb, &slv->skt_h);
	if (rc) {
		ERR("riodp_socket_socket ERR %d\n", rc);
		goto fail;
	};

	rc = riodp_socket_connect(slv->skt_h, slv->mast_did, 0,
				fmd->cfg->mast_cm_port);
	if (rc) {
		ERR("riodp_socket_connect ERR %d\n", rc);
		goto fail;
	};
	slv->skt_valid = 1;
	
        if (riodp_socket_request_send_buffer(slv->skt_h, &slv->tx_buff)) {
                riodp_socket_close(&slv->skt_h);
                goto fail;
        };
	slv->rx_buff = malloc(4096);

	rc = slave_hello_message_exchange();

	if (rc) {
		ERR("hello message fail ERR %d\n", rc);
		goto fail;
	};

        rc = pthread_create(&slv->slave_thr, NULL, mgmt_slave, NULL);
	if (rc) {
		ERR("pthread_create ERR %d\n", rc);
		goto fail;
	};
	sem_wait(&slv->started);

	return 0;
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

	close_slave();
};


#ifdef __cplusplus
}
#endif
