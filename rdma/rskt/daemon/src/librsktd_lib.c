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
#include "librsktd.h"
#include "librdma.h"
#include "librsktd.h"
#include "librsktd_sn.h"
#include "librsktd_dmn.h"
#include "librsktd_lib_info.h"
#include "librsktd_msg_proc.h"
#include "liblist.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

struct librsktd_connect_globals lib_st;


void enqueue_app_msg(struct librsktd_unified_msg *msg)
{
	sem_wait(&dmn.app_tx_mutex);
	l_push_tail(&dmn.app_tx_q, msg);
	sem_post(&dmn.app_tx_mutex);
	sem_post(&dmn.app_tx_cnt);
};

/* Sends requests and responses to all apps */
void *app_tx_loop(void *unused)
{
	struct librsktd_unified_msg *msg = NULL;
	int free_flag;
	int valid_flag;

        l_init(&lib_st.app);

        sem_init(&dmn.app_tx_mutex, 0, 1);
        sem_init(&dmn.app_tx_cnt, 0, 0);
	l_init(&dmn.app_tx_q);

	dmn.app_tx_alive = 1;
	sem_post(&dmn.loop_started);

	while (!dmn.all_must_die) {
		free_flag = 1;
		valid_flag = 0;
		sem_wait(&dmn.app_tx_cnt);
		if (dmn.all_must_die) {
			DBG("dmn.all_must_die is true\n");
			break;
		}
		sem_wait(&dmn.app_tx_mutex);
		if (dmn.all_must_die) {
			DBG("dmn.all_must_die is true\n");
			break;
		}
		msg = (struct librsktd_unified_msg *)l_pop_head(&dmn.app_tx_q);
		sem_post(&dmn.app_tx_mutex);
		if (dmn.all_must_die || (NULL == msg)) {
			DBG("dmn.all_must_die is true or 'msg' is NULL\n");
			break;
		}

		/* Can't send request/response if connection has closed.
		* Note: Cleanup of closed app is responsibility of
		* the app receive thread */
		if ((NULL == msg->app) || (NULL == *msg->app)) {
			DBG("Connection has closed. Can't send request/response\n");
			goto dealloc;
		}

		if (((RSKTD_PROC_AREQ == msg->proc_type) && (RSKTD_AREQ_SEQ_ARESP == msg->proc_stage)) ||
		    ((RSKTD_PROC_A2W == msg->proc_type) && (RSKTD_A2W_SEQ_ARESP == msg->proc_stage))) {
			/* Sending response to application.
			* Nothing to do?
			*/
			free_flag = 1;
			valid_flag = 1;
		};

		if ((RSKTD_PROC_S2A == msg->proc_type) && (RSKTD_S2A_SEQ_AREQ == msg->proc_stage)) {
			uint32_t seq_num;
			/* Sending request, must not deallocate */
			/* Get rsktd sequence number for request */
			free_flag = 0;
			valid_flag = 1;
			sem_wait(&(*msg->app)->app_resp_mutex);
			seq_num = (*msg->app)->dmn_req_num++;
			msg->tx->rq_a.rsktd_seq_num = htonl(seq_num);
			msg->rx->rsp_a.req_a.rsktd_seq_num = htonl(seq_num);
			l_add(&(*msg->app)->app_resp_q, seq_num, (void *)msg);
			sem_post(&(*msg->app)->app_resp_mutex);
			msg->proc_stage = RSKTD_S2A_SEQ_ARESP;
		};

		/* Send message to application */
		if (valid_flag) {
			if (send((*msg->app)->app_fd, (void *)msg->tx, 
					RSKTD2A_SZ, MSG_EOR) != RSKTD2A_SZ) {
				(*msg->app)->i_must_die = 33;
				ERR("send() failed: %s\n", strerror(errno));
			}
		};
dealloc:
		if (free_flag || !valid_flag)
			dealloc_msg(msg);
	};
	dmn.app_tx_alive = 0;
	pthread_exit(unused);
};

void handle_app_msg(struct librskt_app *app, 
			struct librskt_app_to_rsktd_msg *rxed)
{
	struct librsktd_unified_msg *msg;
	struct l_item_t *li;
	uint32_t seq_num;

	if (rxed->msg_type & htonl(LIBRSKTD_RESP)) {
		/* Have a valid response for a SPEER to APP request. */
		/* Update msg with rxed, and post for processing */
		seq_num = ntohl(rxed->rsp_a.req_a.rsktd_seq_num);
		sem_wait(&app->app_resp_mutex);
		msg = (struct librsktd_unified_msg *)
			l_find(&app->app_resp_q, seq_num, &li); 
		if (NULL != msg) {
			DBG("Attempting to remove from list...\n");
			l_lremove(&app->app_resp_q, li);
		}
		sem_post(&app->app_resp_mutex);
		if (NULL == msg) {
			free(rxed);
			return;
		};

		if ((msg->proc_type != RSKTD_PROC_S2A) || 
			(msg->proc_stage != RSKTD_S2A_SEQ_ARESP)) {
			dealloc_msg(msg);
			return;
		};
		msg->proc_stage = RSKTD_S2A_SEQ_ARESP;
		msg->dresp->err = rxed->rsp_a.err;
	} else {
		uint32_t msg_type = ntohl(rxed->msg_type);
		uint32_t proc_type;
		uint32_t proc_stage;

		if ((msg_type == LIBRSKTD_BIND)
		|| (msg_type == LIBRSKTD_LISTEN)
		|| (msg_type == LIBRSKTD_ACCEPT)
		|| (msg_type == LIBRSKTD_HELLO)
		|| (msg_type == LIBRSKTD_CLI)) {
			proc_type = RSKTD_PROC_AREQ;
			proc_stage = RSKTD_AREQ_SEQ_AREQ;
		} else {
			/* LIBRSKTD_CONN, LIBRSKTD_CLOSE */
			proc_type = RSKTD_PROC_A2W;
			proc_stage = RSKTD_A2W_SEQ_AREQ;
		}

		app->rx_req_num = ntohl(rxed->a_rq.app_seq_num);
		msg = alloc_msg(msg_type, proc_type, proc_stage);
		msg->app = app->self_ptr;
		msg->rx = rxed;
		msg->tx = (struct librskt_rsktd_to_app_msg *)malloc(RSKTD2A_SZ);
		msg->tx->msg_type = rxed->msg_type | htonl(LIBRSKTD_RESP);
		memcpy((void *)&msg->tx->a_rsp.req, (void *)&rxed->a_rq, 
			sizeof(struct librskt_req));
		msg->tx->a_rsp.err = 0xFFFFFFFF;
	};
	enqueue_mproc_msg(msg);
};

void *app_rx_loop(void *ip)
{
	struct librskt_app *app = (struct librskt_app *)ip;
	struct l_item_t *app_li;
	struct l_item_t *li;
	struct l_item_t *next_li;
	int rc;
	struct acc_skts *acc;
	struct con_skts *con;
	struct librskt_app_to_rsktd_msg *rxed;

	app->self_ptr = (struct librskt_app **)malloc(sizeof(void *));
	*(app->self_ptr) = app;
	sem_init(&app->started, 0, 0);
	app->i_must_die = 0;
	sem_init(&app->test_msg_tx, 0, 0);
	sem_init(&app->test_msg_rx, 0, 0);
	sem_init(&app->app_resp_mutex, 0, 1);
	app->dmn_req_num = 0;
	l_init(&app->app_resp_q);
	memset(&app->app_name, 0, MAX_APP_NAME);
	app->proc_num = 0; 

	app_li = l_add(&lib_st.app, app->app_fd, ip);

	app->alive = 1;
	
	DBG("*** ENTER\n");
	sem_post(&lib_st.new_app->up_and_running);	/* Added by SAK, Sherif */
	int value;
	if (sem_getvalue(&lib_st.new_app->up_and_running, &value)) {
		WARN("Failed to obtain semaphore value!")
	} else {
		DBG("Posted new_app->up_and_running(%p), now value = %d\n",
					&lib_st.new_app->up_and_running, value);
	}

        while (!app->i_must_die) {
		rxed = (struct librskt_app_to_rsktd_msg *)malloc(A2RSKTD_SZ);
                do {
                	DBG("*** Waiting for A2RSKTD_SZ...\n");
                        rc = recv(app->app_fd, rxed, A2RSKTD_SZ, 0);
                } while ((EINTR == errno) && !app->i_must_die);

                if ((rc <= 0) || app->i_must_die) {
                	if (rc <= 0) {
                		HIGH("App has died!\n");
                	}
                        break;
                }

		handle_app_msg(app, rxed);
	}

	app->alive = 0;
	DBG("Posting app->started\n");
	sem_post(&app->started);

	*app->self_ptr = NULL;

	/* Close all sockets associated with this app */
	DBG("Closing all sockets associated with app\n");
	acc = (struct acc_skts *)l_head(&lib_st.acc, &li);
	next_li = li;
	while (NULL != acc) {
		struct acc_skts *next_acc;

		next_acc = (struct acc_skts *)l_next(&next_li);
		
		if (*acc->app == NULL) {
			rsktd_sn_set(acc->skt_num, rskt_uninit);
			l_remove(&lib_st.acc, li);
		};
		acc = next_acc;
		li = next_li;
	};
	con = (struct con_skts *)l_head(&lib_st.con, &li);
	next_li = li;
	while (NULL != con) {
		struct con_skts *next_con;

		next_con = (struct con_skts *)l_next(&next_li);
		
		if (*con->app == NULL) {
			DBG("*con->app == NULL\n");
			con->loc_ms->state = 0;
			rsktd_sn_set(con->loc_sn, rskt_uninit);
			l_remove(&lib_st.con, li);
		};
		con = next_con;
		li = next_li;
	};

	if (app->app_fd > 0) {
		DBG("Closing socket\n");
		close(app->app_fd);
		app->app_fd = 0;
	};

	l_remove(&lib_st.app, app_li);

	DBG("EXIT\n");
	pthread_exit(NULL);
}
	
int open_lib_conn_socket(void )
{
	int rc = 1;

	lib_st.fd = -1;
	memset(&lib_st.addr, 0, sizeof(lib_st.addr));
	lib_st.addr.sun_family = AF_UNIX;

	DBG("ENTER\n");
	lib_st.fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == lib_st.fd) {
		ERR("ERROR on lib_conn socket: %s\n", strerror(errno));
		goto fail;
	};

	snprintf(lib_st.addr.sun_path, 
		sizeof(lib_st.addr.sun_path) - 1,
		LIBRSKTD_SKT_FMT, lib_st.port, lib_st.mpnum);

	if (remove(lib_st.addr.sun_path)) {
		if (ENOENT != errno) {
			ERR("ERROR on lib_conn remove: %s\n", strerror(errno));
		};
	};

	snprintf(lib_st.addr.sun_path, 
		sizeof(lib_st.addr.sun_path) - 1,
		DMN_LSKT_FMT, lib_st.port, lib_st.mpnum);

	if (-1 == bind(lib_st.fd, (struct sockaddr *) &lib_st.addr, 
			sizeof(struct sockaddr_un))) {
		ERR("ERROR on lib_conn bind: %s\n", strerror(errno));
		goto fail;
	};

	HIGH("Bound lib_st.fd(%d) to %s\n", lib_st.fd, lib_st.addr.sun_path);

	if (listen(lib_st.fd, lib_st.bklg) == -1) {
		ERR("ERROR on lib_conn listen: %s\n", strerror(errno));
		goto fail;
	};
	rc = 0;
fail:
	return rc;
};

void halt_lib_handler(void);

void *lib_conn_loop( void *unused )
{
	DBG("ENTER\n");
	/* Open Unix domain socket */
	if (open_lib_conn_socket()) {
		ERR("Failed in open_lib_conn_socket()\n");
		goto fail;
	}

	lib_st.loop_alive = 1;
	sem_post(&lib_st.loop_started);

	while (!lib_st.all_must_die) {
		int rc;

		lib_st.new_app = (struct librskt_app *)
			malloc(sizeof(struct librskt_app));
		lib_st.new_app->self_ptr = (struct librskt_app **)
			malloc(sizeof(struct librskt_app *));
		*lib_st.new_app->self_ptr = lib_st.new_app;

		lib_st.new_app->alive = 0;
		sem_init(&lib_st.new_app->started, 0, 0);
		sem_init(&lib_st.new_app->up_and_running, 0, 0);
		lib_st.new_app->addr_size = sizeof(struct sockaddr_un);

		INFO("Accepting connections from apps...\n");
		lib_st.new_app->app_fd = accept(lib_st.fd, 
			(struct sockaddr *)&lib_st.new_app->addr,
                        &lib_st.new_app->addr_size);
			
		if (-1 == lib_st.new_app->app_fd) {
			ERR("new_app->app_fd is -1. Exiting\n");
			if (lib_st.fd) {
				ERR("ERROR on l_conn accept: %s\n",
							strerror(errno));
			}
			free(lib_st.new_app);
			lib_st.new_app = NULL;
			goto fail;
		};
		INFO("*** CONNECTED ***\n");

		DBG("Creating app_rx_loop for app\n");
        	rc = pthread_create(&lib_st.new_app->thread, NULL, app_rx_loop,
				(void *)lib_st.new_app);
        	if (rc) {
                	ERR("Error - app_rx_loop rc: %d\n", rc);
        	} else {
        		int value;
        		if (sem_getvalue(&lib_st.new_app->up_and_running, &value)) {
        			WARN("Failed to obtain semaphore value!")
        		} else {
        			DBG("Waiting for new_app->up_and_running(%p) value = %d\n",
        					&lib_st.new_app->up_and_running, value);
        		}
        		sem_wait(&lib_st.new_app->up_and_running);
        		DBG("new_app->up_and_running POSTED!\n");
        	}

		if (lib_st.tst) {
			WARN("lib_st.tst is true, EXITING\n");
			break;
		}
	};
fail:
	DBG("RSKTD Library Connection Thread Exiting\n");
	halt_lib_handler();

	unused = malloc(sizeof(int));
	*(int *)(unused) = lib_st.port;
	pthread_exit(unused);
	return unused;
}

int start_lib_handler(uint32_t port, uint32_t mpnum, uint32_t backlog, int tst)
{
	int ret;

        /* Prepare and start library connection handling threads */

	lib_st.port = port;
	lib_st.mpnum = mpnum;
	lib_st.bklg = backlog;
	lib_st.tst = tst;

	lib_st.loop_alive = 0;
	lib_st.new_app = NULL;

        l_init(&lib_st.app);
        l_init(&lib_st.acc);
        l_init(&lib_st.con);
        l_init(&lib_st.creq);

        DBG("ENTER\n");
        sem_init(&lib_st.loop_started, 0, 0);

        ret = pthread_create(&lib_st.conn_thread, NULL, lib_conn_loop, NULL);
        if (ret) {
                ERR("Error - lib_thread rc: %d\n", ret);
	} else {
        	sem_wait(&lib_st.loop_started);
        	sem_post(&lib_st.loop_started);
	};

	return ret;
};

int lib_handler_dead(void)
{
	return !lib_st.loop_alive;
};

void halt_lib_handler(void)
{
	struct l_item_t *li;
	struct librskt_app *app;
	struct librsktd_unified_msg *msg;

	DBG("ENTER\n");
	if (lib_st.loop_alive && !lib_st.all_must_die) {
		lib_st.all_must_die = 1;
		lib_st.loop_alive = 0;
		DBG("Killing lib_st.conn_thread\n");
		pthread_kill(lib_st.conn_thread, SIGUSR1);
	}

	if (NULL != lib_st.new_app) {
		DBG("lib_st.new_app != NULL\n");
		DBG("################# POSTING!!!\n");
		sem_post(&lib_st.new_app->started);
	}

	if (lib_st.fd > 0) {
		HIGH("Closing lib_st.fd\n");
		close(lib_st.fd);
		lib_st.fd = -1;
	};

	sem_post(&lib_st.loop_started);

	app = (struct librskt_app *)l_head(&lib_st.app, &li);
	while (NULL != app) {
		app->i_must_die = 1;
		sem_post(&app->test_msg_rx);
		app = (struct librskt_app *)l_next(&li);
	};

	if (lib_st.addr.sun_path[0]) {
		DBG("Unlinking %s\n", lib_st.addr.sun_path);
		if (-1 == unlink(lib_st.addr.sun_path)) {
			ERR("ERROR on l_conn unlink: %s\n", strerror(errno));
		}
		lib_st.addr.sun_path[0] = 0;
	};

	msg = (struct librsktd_unified_msg *)l_pop_head(&lib_st.creq);
	while (NULL != msg) {
		enqueue_mproc_msg(msg);
		msg = (struct librsktd_unified_msg *)l_pop_head(&lib_st.creq);
	};
};

void cleanup_lib_handler(void)
{
	DBG("ENTER\n");
	pthread_join(lib_st.conn_thread, NULL);
};
	
#ifdef __cplusplus
}
#endif
