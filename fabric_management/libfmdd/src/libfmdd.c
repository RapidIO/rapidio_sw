/* Implementation of Fabric Management Device Directory Library */
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

#include "libcli.h"
#include "fmd_app_msg.h"
#include "fmd_dd.h"
#include "liblog.h"
#include "libfmdd_info.h"
#include "libfmdd.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fml_globals fml;

int open_socket_to_fmd(void )
{
	if (!fml.fd) {
		fml.fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
		if (-1 == fml.fd) {
			perror("ERROR on libfm_init socket");
			goto fail;
		};
		fml.addr_sz = sizeof(struct sockaddr_un);
		memset(&fml.addr, 0, fml.addr_sz);

		fml.addr.sun_family = AF_UNIX;
		snprintf(fml.addr.sun_path, sizeof(fml.addr.sun_path) - 1,
			FMD_APP_MSG_SKT_FMT, fml.portno);
		if (connect(fml.fd, (struct sockaddr *) &fml.addr, 
				fml.addr_sz)) {
			perror("ERROR on libfm_init connect");
			goto fail;
		};
	};
	return 0;
fail:
	return -1;
};

int get_dd_names_from_fmd(void)
{
	fml.req.msg_type = htonl(FMD_REQ_HELLO);
	fml.req.hello_req.app_pid = htonl(getpid());
	memset(fml.req.hello_req.app_name, 0, MAX_APP_NAME+1);
	strncpy(fml.req.hello_req.app_name, fml.app_name, MAX_APP_NAME);

	if (send(fml.fd, (void *)&(fml.req), sizeof(fml.req), MSG_EOR) < 0)
		goto fail;

	if (recv(fml.fd, (void *)&(fml.resp), sizeof(fml.resp), 0) < 0)
		goto fail;

	if (FMD_RESP_HELLO != ntohl(fml.resp.msg_type))
		goto fail;

	fml.app_idx = ntohl(fml.resp.hello_resp.sm_dd_mtx_idx);
	if ((fml.app_idx < 0) || (fml.app_idx >= FMD_MAX_APPS))
		goto fail;
	return 0;
fail:
	return -1;
};

int open_dd(void)
{
	memset(fml.dd_fn, 0, MAX_DD_FN_SZ+1);
	memset(fml.dd_mtx_fn, 0, MAX_DD_MTX_FN_SZ+1);

	strncpy(fml.dd_fn, fml.resp.hello_resp.dd_fn, MAX_DD_FN_SZ);
	strncpy(fml.dd_mtx_fn, fml.resp.hello_resp.dd_mtx_fn, 
		MAX_DD_MTX_FN_SZ);

	if (fmd_dd_mtx_open((char *)&fml.dd_mtx_fn, &fml.dd_mtx_fd, &fml.dd_mtx)) 
		goto fail;
	if (fmd_dd_open_ro((char *)&fml.dd_fn, &fml.dd_fd, &fml.dd, fml.dd_mtx))
		goto fail;

	return 0;
fail:
	return -1;
};

void shutdown_fml(void)
{
	fml.mon_must_die = 1;

	if (fml.dd_mtx != NULL) {
		if (fml.dd_mtx->dd_ev[fml.app_idx].waiting) {
			fml.dd_mtx->dd_ev[fml.app_idx].waiting = 0;
			fml.dd_mtx->dd_ev[fml.app_idx].proc = 0;
			fml.dd_mtx->dd_ev[fml.app_idx].in_use = 0;
			sem_post(&fml.dd_mtx->dd_ev[fml.app_idx].dd_event);
		};
	};

	fmd_dd_cleanup( fml.dd_mtx_fn, &fml.dd_mtx_fd, &fml.dd_mtx, 
			fml.dd_fn, &fml.dd_fd, &fml.dd, 0);

	fml.dd_mtx = NULL;
	fml.dd = NULL;

	if (fml.dd_fd) {
		close(fml.dd_fd);
		fml.dd_fd = 0;
	};
	if (fml.dd_mtx_fd) {
		close(fml.dd_mtx_fd);
		fml.dd_mtx_fd = 0;
	};

	if (fml.fd) {
		close(fml.fd);
		fml.fd = 0;
	};
	fml.portno = 0;
	fml.init_ok = 0;
};

void init_devid_status(void)
{
        uint32_t i;

        for (i = 0; i < FMD_MAX_DEVS; i++)
                fml.devid_status[i] = CHK_NOK;
};

void update_devid_status(void)
{
        uint32_t i, j, found;

        for (i = 0; i < FMD_MAX_DEVS; i++) {
                found = 0;
                for (j = 0; j < fml.num_devs; j++) {
                        if (fml.devs[j].destID == i) {
				if (fml.devs[j].is_mast_pt) 
                                	fml.devid_status[i] = CHK_OK_MP;
				else
                                	fml.devid_status[i] = CHK_OK;
                                found = 1;
                                break;
                        };
                };
                if (!found)
                        fml.devid_status[i] = CHK_NOK;
        };
};

void notify_app_of_events(void)
{
	struct fml_wait_4_chg *wt;	

	sem_wait(&fml.pend_waits_mtx);
	wt = (struct fml_wait_4_chg *)l_pop_head(&fml.pend_waits);

	while (NULL != wt) {
		sem_post(&wt->sema);
		wt = (struct fml_wait_4_chg *)l_pop_head(&fml.pend_waits);
	};
	sem_post(&fml.pend_waits_mtx);
};

void *mon_loop(void *parms)
{
	int rc;
 
	fml.dd_mtx->dd_ev[fml.app_idx].in_use = 1;
	fml.dd_mtx->dd_ev[fml.app_idx].proc = getpid();

	if (0 >= fmd_dd_atomic_copy(fml.dd, fml.dd_mtx, &fml.num_devs,
					fml.devs, FMD_MAX_DEVS)) {
		sem_post(&fml.mon_started);
		goto exit;
	};
	update_devid_status();
	fml.mon_alive = 1;
	sem_post(&fml.mon_started);
	do {
		fml.dd_mtx->dd_ev[fml.app_idx].waiting = 1;
		rc = sem_wait(&fml.dd_mtx->dd_ev[fml.app_idx].dd_event);
		if (rc || (NULL == fml.dd_mtx))
			goto exit;
		fml.dd_mtx->dd_ev[fml.app_idx].waiting = 0;

		if (0 >= fmd_dd_atomic_copy(fml.dd, fml.dd_mtx, &fml.num_devs,
					fml.devs, FMD_MAX_DEVS))
			goto exit;
		update_devid_status();
		notify_app_of_events();
	} while (fml.num_devs && !fml.mon_must_die && fml.mon_alive);
exit:
	fml.mon_alive = 0;
	shutdown_fml();
	return parms;
};

fmdd_h fmdd_get_handle(char *my_name)
{
	if (!fml.portno) {
		fml.portno = FMD_DEFAULT_SKT;
		strncpy(fml.app_name, my_name, MAX_APP_NAME+1);
	};

	if (!fml.mon_alive) {
		sem_init(&fml.pend_waits_mtx, 0, 1);
		l_init(&fml.pend_waits);
		if (open_socket_to_fmd())
			goto fail;
		if (get_dd_names_from_fmd())
			goto fail;
		if (open_dd())
			goto fail;
		sem_init(&fml.mon_started, 0, 0);
		fml.all_must_die = 0;
		fml.mon_alive = 0;

		/* Startup the connection monitoring thread */
		if (pthread_create( &fml.mon_thr, NULL, mon_loop, NULL)) {
			fml.all_must_die = 1;
			perror("ERROR:fmdd_get_handle, mon_loop thread");
			goto fail;
		};
		sem_wait(&fml.mon_started);
		sem_post(&fml.mon_started);
	};
	if (fml.mon_alive)
		return (void *)&fml;
fail:
	shutdown_fml();
	return NULL;
};

void fmdd_destroy_handle(fmdd_h *dd_h)
{
	shutdown_fml();
	*dd_h = NULL;
};

int fmdd_check_ct(fmdd_h h, uint32_t ct)
{
	uint32_t i;

	if (h != &fml)
		goto fail;

	for (i = 0; i < fml.num_devs; i++) {
		if (fml.devs[i].ct == ct) {
			if (fml.devs[i].is_mast_pt)
				return CHK_OK_MP;
			else
				return CHK_OK;
		};
	};
fail:
	return CHK_NOK;
};

int fmdd_check_did(fmdd_h h, uint32_t did)
{
	if (h != &fml)
		goto fail;

	if (did >= FMD_MAX_DEVS)
		goto fail;

	return fml.devid_status[did];

fail:
	return CHK_NOK;
};

int fmdd_get_did_list(fmdd_h h, uint32_t *did_list_sz, uint32_t **did_list)
{
	uint32_t i;

	if (h != &fml)
		goto fail;

	*did_list_sz = fml.num_devs;

	if (!fml.num_devs) {
		*did_list = NULL;
		goto exit;
	};

	*did_list = (uint32_t *)malloc(sizeof(uint32_t) * fml.num_devs);
	for (i = 0; i < fml.num_devs; i++)
		(*did_list)[i] = fml.devs[i].destID;
exit:
	return 0;
fail:
	return 1;
};

int fmdd_free_did_list(fmdd_h h, uint32_t **did_list)
{
	if (h != &fml)
		goto fail;

	free(*did_list);
	*did_list = NULL;

	return 0;
fail:
	return 1;
};

int fmdd_wait_for_dd_change(fmdd_h h)
{
	struct fml_wait_4_chg *chg_sem;
	int rc;

	if ((h != &fml) || fml.mon_must_die || !fml.mon_alive)
		goto fail;

	chg_sem = (struct fml_wait_4_chg *)
			malloc(sizeof(struct fml_wait_4_chg));

	sem_init(&chg_sem->sema, 0, 0);

	sem_wait(&fml.pend_waits_mtx);
	l_push_tail(&fml.pend_waits, (void *)&chg_sem);
	sem_post(&fml.pend_waits_mtx);

	rc = sem_wait(&chg_sem->sema);

	/* Note: The notification process removes all items from the list. */
	free(chg_sem);
	
	if (fml.mon_must_die || !fml.mon_alive || rc)
		goto fail;

	return 0;
fail:
	return 1;
};

#ifdef __cplusplus
}
#endif

