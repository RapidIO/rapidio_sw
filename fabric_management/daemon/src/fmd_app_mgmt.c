/* Implementation of the RDMA Socket Daemon side of the "librskt" library */
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
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "fmd_state.h"
#include "fmd_app_mgmt.h"
#include "fmd_dd.h"
#include "fmd_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

struct app_mgmt_globals app_st;

#define INIT_SEM 1
#define NO_SEM 0

void init_app_mgmt(struct fmd_app_mgmt_state *app, int init_sem)
{
	app->alloced = 0;
	app->app_fd = 0;
	app->addr_size = 0;
	memset((void *)&app->addr, sizeof(struct sockaddr_un), 0);
	app->alive = 0;
	if (NO_SEM != init_sem) {
		sem_init(&app->started, 0, 0);
	} else {
		if (!sem_destroy(&app->started))
			sem_init(&app->started, 0, 0);
	};
	app->i_must_die = 0;
	app->proc_num = 0;
	memset((void *)app->app_name, MAX_APP_NAME+1, 0);
	memset((void *)&app->req, sizeof(struct libfmd_dmn_app_msg), 0);
	memset((void *)&app->resp, sizeof(struct libfmd_dmn_app_msg), 0);
};

void init_app_mgmt_st(void)
{
	int i;

	app_st.port = -1;
	app_st.bklg = -1;
	app_st.loop_alive = 0;
	sem_init(&app_st.loop_started, 0, 0);
	app_st.all_must_die = 0;
	app_st.ct = 0;
	app_st.fd = 0;
	memset((void *)&app_st.addr, sizeof(struct sockaddr_un), 0);
	for (i = 0; i < FMD_MAX_APPS; i++) {
		init_app_mgmt(&app_st.apps[i], INIT_SEM);
		app_st.apps[i].index = i;
	};
	sem_init(&app_st.apps_avail, 0, FMD_MAX_APPS);
};

void handle_app_msg(struct fmd_app_mgmt_state *app)
{
	memset((void *)&app->resp, sizeof(struct libfmd_dmn_app_msg), 0);

	app->resp.msg_type = app->req.msg_type | htonl(FMD_MSG_RESP);

	if (htonl(FMD_REQ_HELLO) != app->req.msg_type) {
		app->resp.msg_type |= htonl(FMD_MSG_FAIL);
		return;
	};

	app->proc_num = ntohl(app->proc_num);
	strncpy(app->app_name, app->req.hello_req.app_name, MAX_APP_NAME);

	app->resp.hello_resp.sm_dd_mtx_idx = htonl(app->index);
	strncpy(app->resp.hello_resp.dd_fn, app_st.dd_fn, MAX_DD_FN_SZ);
	strncpy(app->resp.hello_resp.dd_mtx_fn, app_st.dd_mtx_fn, 
		MAX_DD_MTX_FN_SZ);
};


/* Initializes and then monitors one application. */
/* This thread is responsible for managing the library connection if the 
 * app shuts down unexpectedly.
 */

void *app_loop(void *ip)
{
	struct fmd_app_mgmt_state *app = (struct fmd_app_mgmt_state *)ip;
	int msg_size = sizeof(struct libfmd_dmn_app_msg); 
	int rc;

	memset((void *)&app->app_name, 0, MAX_APP_NAME);
	app->alive = 1;
	sem_post(&app->started);
	
        while (!app->i_must_die) {
                do {
                        rc = recv(app->app_fd, &app->req, msg_size, 0);
                } while ((EINTR == errno) && !app->i_must_die);

                if ((rc <= 0) || app->i_must_die)
                        break;

		handle_app_msg(app);

		rc = send(app->app_fd, &app->resp, msg_size, 0);
		if ((rc != msg_size) || app->i_must_die)
			break;
	}

	if (app->app_fd) {
		close(app->app_fd);
		app->app_fd = 0;
	};

	app->alloced = 0;
	app->alive = 0;
	sem_post(&app_st.apps_avail);

	pthread_exit(NULL);
}
	
int open_app_conn_socket(void)
{
	int rc = 1;
	app_st.addr.sun_family = AF_UNIX;

	app_st.fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == app_st.fd) {
		perror("ERROR on open_app_conn socket");
		goto fail;
	};

	snprintf(app_st.addr.sun_path, sizeof(app_st.addr.sun_path) - 1,
		FMD_MSG_SKT_FMT, app_st.port);

	if (remove(app_st.addr.sun_path))
		if (ENOENT != errno)
			perror("ERROR on app_conn remove");

	snprintf(app_st.addr.sun_path, sizeof(app_st.addr.sun_path) - 1,
		FMD_MSG_SKT_FMT, app_st.port);

	if (-1 == bind(app_st.fd, (struct sockaddr *) &app_st.addr, 
			sizeof(struct sockaddr_un))) {
		perror("ERROR on app_conn bind");
		goto fail;
	};

	if (listen(app_st.fd, app_st.bklg) == -1) {
		perror("ERROR on app_conn listen");
		goto fail;
	};
	rc = 0;
fail:
	return rc;
};

void halt_app_handler(void);

void *app_conn_loop( void *unused )
{
	int rc = open_app_conn_socket(); 

	/* Open Unix domain socket */
	app_st.loop_alive = (!rc);
	app_st.all_must_die = !app_st.loop_alive;
	sem_post(&app_st.loop_started);

	while (!app_st.all_must_die) {
		int rc, found, i, new_app_i;
		struct fmd_app_mgmt_state *new_app;

		sem_wait(&app_st.apps_avail);
		found = 0;
		for (i = 0; (i < FMD_MAX_APPS) && !found; i++) {
			if (!app_st.apps[i].alloced) {
				new_app_i = i;
				found = 1;
			};
		};
		if (!found) {
			perror("FMD could not find free app!");
			goto fail;
		};
		new_app = &app_st.apps[new_app_i];
		init_app_mgmt(new_app, NO_SEM);

		new_app->alloced = 2;
		new_app->addr_size = sizeof(struct sockaddr_un);
		new_app->app_fd = accept(app_st.fd, 
			(struct sockaddr *)&new_app->addr,
                        &new_app->addr_size);
			
		if (-1 == new_app->app_fd) {
			if (app_st.fd) 
				perror("ERROR on app_conn accept");
			goto fail;
		};

        	rc = pthread_create(&new_app->app_thr, NULL, app_loop,
				(void *)new_app);
        	if (rc) {
                	printf("Error - app_rx_loop rc: %d\n", rc);
		} else {
        		sem_wait(&new_app->started);
			new_app->alloced = 1;
		};
	};
fail:
	printf("\nFMD Library Connection Thread Exiting\n");
	halt_app_handler();

	pthread_exit(unused);
	return unused;
}

int start_fmd_app_handler(uint32_t port, uint32_t backlog, int tst,
		char *dd_fn, char *dd_mtx_fn)
{
	int ret;

	init_app_mgmt_st();

        /* Prepare and start application connection handling threads */
	app_st.port = port;
	app_st.bklg = backlog;
	app_st.dd_fn = dd_fn;
	app_st.dd_mtx_fn = dd_mtx_fn;

        ret = pthread_create(&app_st.conn_thread, NULL, app_conn_loop, NULL);
        if (ret)
                printf("Error - start_fmd_app_handler rc: %d\n", ret);
	else
        	sem_wait(&app_st.loop_started);

	return ret;
};

int app_handler_dead(void)
{
	return !app_st.loop_alive;
};

void halt_app_handler(void)
{
	int i;

	if (app_st.loop_alive && !app_st.all_must_die) {
		pthread_kill(app_st.conn_thread, SIGHUP);
		app_st.all_must_die = 1;
		app_st.loop_alive = 0;
	}

	if (app_st.fd > 0) {
		close(app_st.fd);
		app_st.fd = -1;
	};

	sem_post(&app_st.loop_started);

	for (i = 0; i < FMD_MAX_APPS; i++) {
		app_st.apps[i].i_must_die = 1;
		if ((!app_st.apps[i].alloced) || (!app_st.apps[i].alive))
			continue;
		sem_post(&app_st.apps[i].started);
		if (app_st.apps[i].app_fd) {
			close(app_st.apps[i].app_fd);
			app_st.apps[i].app_fd = 0;
		};
	};
};

void cleanup_app_handler(void)
{
	if (app_st.loop_alive)
		pthread_join(app_st.conn_thread, NULL);
};
	
void fmd_notify_apps (void)
{
	int i;

	if (NULL == fmd->dd_mtx)
		return;

	for (i = 0; i < FMD_MAX_APPS; i++) {
		if (!app_st.apps[i].alloced || !app_st.apps[i].alive)
			continue;
		if (!fmd->dd_mtx->dd_ev[i].in_use || !app_st.apps[i].proc_num)
			continue;
		if (fmd->dd_mtx->dd_ev[i].proc != app_st.apps[i].proc_num)
			continue;
			
		sem_post(&fmd->dd_mtx->dd_ev[i].dd_event); 
	};
};

#ifdef __cplusplus
}
#endif
