/* Management of memory spaces, handling received daemon requests,
 * and making requests of other daemons.
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
#include <rapidio_mport_dma.h>

#include "libcli.h"
#include "librskt_private.h"
#include "librdma.h"
#include "librsktd_sn.h"
#include "librsktd.h"
#include "librsktd_dmn_info.h"
#include "librsktd_lib_info.h"
#include "librsktd_lib.h"
#include "librsktd_msg_proc.h"
#include "librsktd_wpeer.h"
#include "librsktd_speer.h"
#include "librsktd_fm.h"
#include "liblog.h"


#ifdef __cplusplus
extern "C" {
#endif

struct dmn_globals dmn;

int d_rdma_get_mso_h(char *name, mso_h *mso)
{
        return rdma_create_mso_h(name, mso);
};

int d_rdma_drop_mso_h(void) {
        int rc = 0;

	if (dmn.mso.valid) {
        	memset(dmn.mso.msoh_name, 0, MAX_MS_NAME+1);

        	rc = rdma_destroy_mso_h(dmn.mso.rskt_mso);
        	dmn.mso.rskt_mso = 0;
		dmn.mso.valid = 0;
	};

        return rc;
};

int d_rdma_drop_ms_h(struct ms_info *msi)
{
	int rc = 0;

	if (msi->valid && dmn.mso.valid) {
        	rc = rdma_destroy_ms_h(dmn.mso.rskt_mso, msi->ms);

        	msi->valid = 0;
        	memset(msi->ms_name, 0, MAX_MS_NAME+1);
        	msi->ms = 0;
        	msi->ms_size = 0;
	};

        return rc;
}

int d_rdma_get_ms_h(struct ms_info *msi, const char *name,
                        mso_h msoh, uint32_t req_ms_size, uint32_t flags)
{
        int rc;
	uint32_t act_ms_size = req_ms_size;

        if (msi->valid) {
                rc = d_rdma_drop_ms_h(msi);
                if (rc)
                        return rc;
        };

        rc = rdma_create_ms_h(name, msoh, req_ms_size, flags, &msi->ms, 
				&act_ms_size);
	if (rc) {
		CRIT("Could not get ms_h for '%s': Bailing out...\n",name);
		return rc;
	};

	if (act_ms_size < req_ms_size) {
		CRIT("WARNING: MS %s got %d bytes not %d bytes.\n",
			name, act_ms_size, req_ms_size);
	};
	msi->valid = 1;
	msi->ms_size = act_ms_size;

	return rc;
};

int d_rdma_drop_msub_h(struct ms_info *msi)
{
        int rc;

        if (!msi->valid)
                return 0;

        rc = rdma_destroy_msub_h(msi->ms, msi->skt.msubh);

        rskt_clear_skt(&msi->skt);
        return rc;
}

int d_rdma_get_msub_h(struct ms_info *msi, int size,
                                uint32_t flags)
{
        int rc;

	if (msi->skt.msub_sz) {
        	d_rdma_drop_msub_h(msi);
		msi->skt.msub_sz = 0;
	};

        rc = rdma_create_msub_h(msi->ms, 0, size, flags, 
				&msi->skt.msubh);

        if (rc) {
		CRIT("Could not get msub for MS %s.\n", msi->ms_name);
		CRIT("Bailing out...\n");
                rskt_clear_skt(&msi->skt);
        } else {
                msi->skt.msub_sz = size;
	}

        return rc;
}

int init_mport_memory(int num_ms, int ms_size)
{
	int rc = -1;
	int i;
	uint64_t tot_size = num_ms * ms_size;
	uint32_t ibwin_size = 0x10000; /* Minimum window size) */
	uint64_t rio_addr = RIO_ANY_ADDR;
	uint64_t phys_addr = RIO_ANY_ADDR;

	while ((ibwin_size < tot_size) && (ibwin_size < 0x10000000))
		ibwin_size = ibwin_size << 1;

	rc = riomp_dma_ibwin_map(dmn.mp_hnd, &rio_addr, ibwin_size, &phys_addr);
	if (rc) {
                CRIT("Cannot map inbound memory rc %d %d %s\n", rc, errno,
							strerror(errno));
		goto fail;
	}

	dmn.mso.valid = true;
	dmn.mso.num_ms = num_ms;

	for (i = 0; i < num_ms; i++) {
		dmn.mso.ms[i].valid = true;
		dmn.mso.ms[i].ms_size = ms_size;
		dmn.mso.ms[i].rio_addr = rio_addr + (ms_size * i);
		dmn.mso.ms[i].phy_addr = phys_addr + (ms_size * i);
	};
	
	rc = 0;
fail:
	return rc;
};

int init_mport_and_mso_ms(void)
{
	int rc = -1;
	uint32_t i;

	dmn.mb_valid = 0;
	dmn.skt_valid = 0;

        rc = riomp_mgmt_mport_create_handle(
					dmn.mpnum, RIO_MPORT_DMA, &dmn.mp_hnd);
        if (rc < 0) {
                CRIT("Unable to open mport %d...\n", rc);
                goto exit;
        };
	INFO("Mport %d opened\n", dmn.mpnum);

        if (riomp_mgmt_query(dmn.mp_hnd, &dmn.qresp)) {
                CRIT("Unable to query mport %d...\n", dmn.mpnum);
                goto exit;
        };

	riomp_mgmt_display_info(&dmn.qresp);

        if (!(dmn.qresp.flags & RIO_MPORT_DMA)) {
                CRIT("Mport %d has no DMA support...\n", dmn.mpnum);
                goto exit;
        };

	rc = riomp_sock_mbox_create_handle(dmn.mpnum, 0, &dmn.mb);
	if (rc) {
		ERR("riodp_mbox_create_ ERR %d\n", rc);
		goto exit;
	};
	dmn.mb_valid = 1;
	sem_init(&dmn.mb_mtx, 0, 1);

	rc = riomp_sock_socket(dmn.mb, &dmn.cm_acc_h);
	if (rc) {
		ERR("riomp_sock_socket ERR %d\n", rc);
		goto exit;
	};

	rc = riomp_sock_bind(dmn.cm_acc_h, dmn.cm_skt);
	if (rc) {
		ERR("speer_conn: riomp_sock_bind() %d ERR %d %d:%s\n",
			dmn.cm_skt, rc, errno, strerror(errno));
		goto exit;
	};
	dmn.skt_valid = 1;

	rc = riomp_sock_listen(dmn.cm_acc_h);
	if (rc) {
		CRIT("speer_conn:riomp_sock_listen() ERR %d %d:%s\n",
			rc, errno, strerror(errno));
		goto exit;
	};

	/* Initialize the rest of the dmn structure */
	dmn.mso.valid = 0;
	dmn.mso.num_ms = 0;
	dmn.mso.next_ms = 0;
	memset(dmn.mso.msoh_name, 0, MAX_MS_NAME+1);
	for (i = 0; i < MAX_DMN_NUM_MS; i++) {
		dmn.mso.ms[i].valid = 0;
		dmn.mso.ms[i].state = rsktd_ms_free;
		memset(dmn.mso.ms[i].ms_name, 0, MAX_MS_NAME+1);
		dmn.mso.ms[i].ms_size = 0;
		rskt_clear_skt(&dmn.mso.ms[i].skt); 
	};
		
	if (dmn.skip_ms) {
		rc = 0;
		goto exit;
	};

	if (dmn.use_mport) {
		rc = init_mport_memory(dmn.num_ms, dmn.ms_size);
		if (rc) {
			CRIT("Could not allocate mport memory. Exiting...\n");
		};
		goto exit;
	};

	snprintf(dmn.mso.msoh_name, MAX_MS_NAME, "RSKT_DAEMON%d", getpid());
        if (d_rdma_get_mso_h(dmn.mso.msoh_name, &dmn.mso.rskt_mso)) {
		CRIT("Could not get mso_h for '%s'. Bailing out...\n",
				dmn.mso.msoh_name);
		goto exit;
        };

        for (i = 0; i < dmn.num_ms; i++) {
                dmn.mso.ms[i].ms_size = dmn.ms_size;

                snprintf(dmn.mso.ms[i].ms_name, MAX_MS_NAME,
                        "RSKT_DAEMON%05d.%03d", getpid(), i);

                if (d_rdma_get_ms_h(&dmn.mso.ms[i], dmn.mso.ms[i].ms_name,
                        	dmn.mso.rskt_mso, dmn.ms_size, 0))
			goto exit;
                if (d_rdma_get_msub_h(&dmn.mso.ms[i], 
                                        dmn.mso.ms[i].ms_size, 0))
                        goto exit;
		dmn.mso.num_ms++;
        };
	rc = 0;
exit:
	return rc;
};

int cleanup_dmn(void)
{
	int rc = 0;
	uint32_t i;

        dmn.all_must_die = 1;

        halt_msg_proc_q_thread();
        halt_lib_handler();
	halt_fm_thread();
        halt_speer_handler();
        halt_wpeer_handler();

	if (dmn.skt_valid) {
		dmn.skt_valid = 0;
		rc = riomp_sock_close(&dmn.cm_acc_h);
		if (rc)
			CRIT("speer_conn: riomp_sock_close ERR %d\n", rc);
	};

	if (dmn.mb_valid) {
		dmn.mb_valid = 0;
		rc = riomp_sock_mbox_destroy_handle(&dmn.mb);
		memset(&dmn.mb, 0, sizeof(dmn.mb));
		if (rc)
			CRIT("speer_conn: riodp_mbox_shutdown ERR: %d\n", rc);
	};

	if (dmn.mp_hnd) {
		riomp_mgmt_mport_destroy_handle(&dmn.mp_hnd);
		memset(&dmn.mp_hnd, 0, sizeof(dmn.mp_hnd));
                dmn.mp_hnd = 0;
        };

        for (i = 0; i < MAX_DMN_NUM_MS; i++) {
                rc = d_rdma_drop_ms_h(&dmn.mso.ms[i]);
                if (rc)
                        CRIT("\nd_rdma_drop_ms_h returned %d: %s", rc,
                                        strerror(rc));
        };

        rc = d_rdma_drop_mso_h();
        if (rc)
                CRIT("\rdma_ndrop_mso_h: %d: %s", rc, strerror(rc));

	sem_post(&dmn.graceful_exit);
	return rc;
};

int spawn_daemon_threads(struct control_list *ctrls)
{
	int rc = 1;

	if (start_msg_proc_q_thread()) {
		CRIT("start_msg_proc_q_thread failed\n");
		goto exit;
	};
	DBG("start_msg_proc_q_thread successful.\n");

	dmn.use_mport = ctrls->use_mport;
        if (start_speer_handler( ctrls->rsktd_cskt, ctrls->rsktd_c_mp,
			ctrls->num_ms, ctrls->ms_size, !ctrls->init_ms)) {
                CRIT("start_speer_handler failed");
		goto exit;
	};
	DBG("start_speer_handler successful.\n");

        if (start_wpeer_handler()) {
		CRIT("start_wpeer_conn failed\n");
		goto exit;
	};
	DBG("start_wpeer_conn successful.\n");

	if (start_lib_handler(ctrls->rsktd_uskt, ctrls->rsktd_u_mp,
			ctrls->rsktd_u_bklg, ctrls->rsktd_uskt_tst)) {
		CRIT("start_lib_handler failed\n");
		goto exit;
	};
	DBG("start_lib_handler successful.\n");

	if (start_fm_thread()) {
		CRIT("start_fm_thread failed\n");
		goto exit;
	}
	DBG("start_fm_thread successful\n");
	rc = 0;
exit:
	return rc;
};

int daemon_threads_failed(void)
{
	DBG("WP_Tx %d SP_Tx %d AppTx %d MsgP %d SP_C %d AppC %d",
		dmn.wpeer_tx_alive, dmn.speer_tx_alive, dmn.app_tx_alive, 
		mproc.msg_proc_alive, dmn.speer_conn_alive, 
		lib_st.lib_conn_loop_alive);
	return !dmn.wpeer_tx_alive || !dmn.speer_tx_alive ||
		!dmn.app_tx_alive || !mproc.msg_proc_alive || 
		!dmn.speer_conn_alive || !lib_st.lib_conn_loop_alive;
};

void kill_daemon_threads(void)
{
	dmn.all_must_die = 1;
	sem_post(&dmn.graceful_exit);
	cleanup_dmn();
};

#ifdef __cplusplus
}
#endif
