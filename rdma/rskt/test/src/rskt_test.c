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

#include "libcli.h"
#include "liblog.h"
#include "librsktd_lib.h"
#include "librsktd_lib_info.h"
#include "librskt.h"
#include "libunit_test.h"
#include "librskt_private.h"
#include "librsktd_sn.h"
#include "librsktd_dmn.h"
#include "fake_libmport.h"
#include "rskt_worker.h"

#define DFLT_LIBRSKTD_TEST_PORT 1234
#define DFLT_LIBRSKTD_TEST_MPNUM 0
#define DFLT_LIBRSKTD_TEST_BKLG 35
#define DFLT_LIBRSKTD_TEST_TEST 0
#define DFLT_LIBRSKTD_TEST_CM_SKT 4395
#define DFLT_LIBRSKTD_TEST_MPNUM 0
#define TEST_ZERO_MS 0
#define TEST_ZERO_MS_SZ 0
#define TEST_SKIP_MS 1
#define NOT_TEST 0

#ifdef __cplusplus
extern "C" {
#endif

#define BIG_TEST_MAX_SN 10000

void cleanup_proc(struct cli_env *env)
{
	int i;

	if (NULL == env)
		i = 0;

	halt_msg_proc_q_thread();
	halt_lib_handler();
	halt_speer_conn_handler();
	librskt_finish();
	for (i = 0; i < MAX_WORKERS; i++)
		kill_worker_thread(&wkr[i]);
};

char *unknown = (char *)"Unknonwn";

char *actions[LAST_TEST+1] = {
	(char *)"BadAct",
	(char *)"BIND_T",
	(char *)"LISN_T",
	(char *)"ACC__T",
	(char *)"ACCBDY",

	(char *)"SPEER ",

	(char *)"OORnge"
};

char *action_str(int action)
{
	if (action < LAST_TEST) 
		return actions[action];
	else
		return actions[LAST_TEST];
	
	return action?unknown:unknown;
};

int ts_sel(char *parm)
{
	return parm[0]*0;
};

int test_bind_and_close(struct worker *info);
int test_bind_listen_and_close(struct worker *info);
int test_bind_listen_accept_and_close(struct worker *info);
int test_buddy(struct worker *info);

int test_speer_xchg(struct worker *info);

int worker_body(struct worker *info)
{
	int rc = 0;

	switch (info->action) {
	case LIB_BIND_TEST: rc = test_bind_and_close(info);
			break;
	case LIB_LISTEN_TEST: rc = test_bind_listen_and_close(info);
			break;
	case LIB_ACCEPT_TEST: rc = test_bind_listen_accept_and_close(info);
			break;
	case LIB_ACCEPT_BUDDY: rc = test_buddy(info);
			break;

	case SPEER_MSG_XCHG: rc = test_speer_xchg(info);
			break;

	default: CRIT("Unknown worker action...");
		rc = 1;
	};
	return rc;
};
	
void create_priv(struct worker *info)
{
	info->priv_info = malloc(sizeof(struct rskt_test_info));
};

void destroy_priv(struct worker *info)
{
	if (info->priv_info) {
		free(info->priv_info);
		info->priv_info = NULL;
	};
};

struct unit_test_driver stub_utd = {
        action_str,
        ts_sel,
        worker_body,
        create_priv,
        destroy_priv
};
/* Test bind and close interactions:
 * Bind to a fresh socket
 * Bind fails to a bound socket
 * Close a bound socket
 * Can re-bind to a closed socket
 * Can destroy a socket
 * Bind fails to a destroyed socket.
 */

int all_app_lib_queues_empty(struct worker *info)
{
	struct rskt_test_info *test = (struct rskt_test_info *)
							info->priv_info;

	/* Can't check for queue state with multiple threads active */
	if (test->num_wkrs > 1)
		return 0;

	if (l_size(&lib.msg_tx))
		goto fail;
	if (l_size(&lib.rsvp))
		goto fail;
	if (l_size(&lib.req))
		goto fail;
	if (l_size(&lib_st.tx_msg_q))
		goto fail;
	if (l_size(&lib_st.acc))
		goto fail;
	if (l_size(&lib_st.con))
		goto fail;
	if (l_size(&lib_st.creq))
		goto fail;
	return 0;
fail:
	return 1;
};

int all_app_lib_queues_stat(struct worker *info,
				int acc_sz, int con_sz, int creq_sz)
{
	struct rskt_test_info *test = (struct rskt_test_info *)
							info->priv_info;

	/* Can't check for queue state with multiple threads active */
	if (test->num_wkrs > 1)
		return 0;

	if (l_size(&lib.msg_tx))
		goto fail;
	if (l_size(&lib.rsvp))
		goto fail;
	if (l_size(&lib.req))
		goto fail;
	if (l_size(&lib_st.tx_msg_q))
		goto fail;
	if (l_size(&lib_st.acc) != acc_sz)
		goto fail;
	if (l_size(&lib_st.con) != con_sz)
		goto fail;
	if (l_size(&lib_st.creq) != creq_sz)
		goto fail;
	return 0;
fail:
	return 1;
};

int test_bind_and_close(struct worker *info)
{
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;
	int i, rc = 0;
	rskt_h *skts;
	int fail_pt = 1;
	struct rskt_sockaddr sa;
	int sn;

	test->rc = 1;
	skts = (rskt_h *)malloc(
			sizeof(rskt_h)*(test->end_sn - test->start_sn + 1));
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		int idx = sn - test->start_sn;
		skts[idx] = rskt_create_socket();
		if (NULL == skts[idx])
			goto fail;
	};

	fail_pt = 2;

	for (i = 0; i < test->max_iter; i++) {
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_bind(skts[sn - test->start_sn], &sa);
			if (rc)
				goto fail;
		};
	fail_pt = 3;

		if (all_app_lib_queues_empty(info))
			goto fail;
	fail_pt = 4;

		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_bind(skts[sn - test->start_sn], &sa);
			if (!rc)
				goto fail;
		};
	fail_pt = 5;
		if (all_app_lib_queues_empty(info))
			goto fail;
	fail_pt = 6;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_close(skts[sn - test->start_sn]);
			if (rc)
				goto fail;
		};
	fail_pt = 7;
		if (all_app_lib_queues_empty(info))
			goto fail;
	};

	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		int idx = sn - test->start_sn;
		rskt_destroy_socket(&skts[idx]);
		rc = rskt_bind(skts[idx], &sa);
		if (!rc)
			goto fail;
	};
	fail_pt = 8;
	if (all_app_lib_queues_empty(info))
		goto fail;

	fail_pt = 9;
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		sa.ct = sn + 1;
		sa.sn = sn;
		rc = rskt_bind(skts[sn - test->start_sn], &sa);
		if (!rc)
			goto fail;
	};
	fail_pt = 10;
	if (all_app_lib_queues_empty(info))
		goto fail;

	test->rc = 0;
	return 0;
fail:
	test->rc = fail_pt;
	return 0;
}

int test_bind_listen_and_close(struct worker *info)
{
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;
	int i, rc = 0;
	rskt_h *skts;
	struct rskt_sockaddr sa;
	int sn;
	int fail_pt = 1;
	int num_skts = test->end_sn - test->start_sn + 1;

	test->rc = 1;
	skts = (rskt_h *)malloc( sizeof(rskt_h)*(num_skts));
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		int idx = sn - test->start_sn;
		skts[idx] = rskt_create_socket();
		if (NULL == skts[idx])
			goto fail;
	};

	for (i = 0; i < test->max_iter; i++) {
		fail_pt = 2;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_bind(skts[sn - test->start_sn], &sa);
			if (rc)
				goto fail;
		};
		fail_pt = 3;
		if (all_app_lib_queues_empty(info))
			goto fail;
		fail_pt = 4;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_bind(skts[sn - test->start_sn], &sa);
			if (!rc)
				goto fail;
		};
		fail_pt = 5;
		if (all_app_lib_queues_empty(info))
			goto fail;
		fail_pt = 6;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_listen(skts[sn - test->start_sn], sn + 2);
			if (rc)
				goto fail;
/*
			if (l_size(&lib_st.acc) != (sn- test->start_sn + 1))
				goto fail;
*/
		};
		fail_pt = 7;
		if (all_app_lib_queues_stat(info, num_skts, 0, 0))
			goto fail;
		fail_pt = 8;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_listen(skts[sn - test->start_sn], sn + 2);
			if (!rc)
				goto fail;
		};
		fail_pt = 9;
		if (all_app_lib_queues_stat(info, num_skts, 0, 0))
			goto fail;
		fail_pt = 10;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_close(skts[sn - test->start_sn]);
			if (rc)
				goto fail;
		};
		fail_pt = 11;
		if (all_app_lib_queues_empty(info))
			goto fail;
	};

		fail_pt = 12;
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		int idx = sn - test->start_sn;
		rskt_destroy_socket(&skts[idx]);
		rc = rskt_bind(skts[idx], &sa);
		if (!rc)
			goto fail;
	};
		fail_pt = 13;
	if (all_app_lib_queues_empty(info))
		goto fail;

		fail_pt = 14;
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		sa.ct = sn + 1;
		sa.sn = sn;
		rc = rskt_listen(skts[sn - test->start_sn], sn + 2);
		if (!rc)
			goto fail;
	};
		fail_pt = 15;
	if (all_app_lib_queues_empty(info))
		goto fail;

	free(skts);
	test->rc = 0;
	return 0;
fail:
	test->rc = fail_pt;
	return 0;
}

int test_bind_listen_accept_and_close(struct worker *info)
{
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;
	int i, rc = 0;
	rskt_h *l_skts;
	struct rskt_sockaddr sa;
	int sn;
	int fail_pt = 1;
	int num_skts = test->end_sn - test->start_sn + 1;
	rskt_h acc_skt;
	struct rskt_sockaddr acc_sa;

	acc_skt = rskt_create_socket();
	test->rc = 1;
	l_skts = (rskt_h *)malloc( sizeof(rskt_h)*(num_skts));
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
	};

	test->skts = l_skts;

	sn = test->start_sn;

	for (i = 0; i < test->max_iter; i++) {
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			int skt_idx = sn - test->start_sn;

			fail_pt = 1 + (i << 8);
			l_skts[skt_idx] = rskt_create_socket();
			if (NULL == l_skts[skt_idx])
				goto fail;

			fail_pt = 2 + (i << 8);
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_bind(l_skts[skt_idx], &sa);
			if (rc)
				goto fail;

			fail_pt = 6 + (i << 8);
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_listen(l_skts[skt_idx], sn + 2);
			if (rc)
				goto fail;

			fail_pt = 10 + (i << 8);
			sa.ct = sn + 1;
			sa.sn = sn;

			sem_post(&test->accepting);
			/* At this point, we wait for buddy
 			* to close the socket 
 			*/
			rc = rskt_accept(l_skts[skt_idx], acc_skt, &acc_sa);
			if (!rc)
				goto fail;
	
			fail_pt = 12 + (i << 8);
	
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_close(l_skts[sn - test->start_sn]);
		};
	};

		fail_pt = 13;
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		int idx = sn - test->start_sn;
		rskt_destroy_socket(&l_skts[idx]);
		rc = rskt_bind(l_skts[idx], &sa);
		if (!rc)
			goto fail;
	};
		fail_pt = 14;
	if (all_app_lib_queues_empty(info))
		goto fail;

		fail_pt = 15;
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		sa.ct = sn + 1;
		sa.sn = sn;
		rc = rskt_listen(l_skts[sn - test->start_sn], sn + 2);
		if (!rc)
			goto fail;
	};
		fail_pt = 16;
	if (all_app_lib_queues_empty(info))
		goto fail;
	test->skts = NULL;
	free(l_skts);
	test->rc = 0;
	sem_post(&test->done_sema);
	return 0;
fail:
	test->rc = fail_pt;
	sem_post(&test->done_sema);
	return 0;
}

int test_buddy(struct worker *info)
{
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;
	struct rskt_test_info *bud;
	int iter;
	int fail_pt = 1;
	int sn;
	timespec ten_usecs = {0, 10*1000};

	if (NULL == test)
		goto fail;

	if (NULL == test->buddy)
		goto fail;

	bud = (struct rskt_test_info *)test->buddy->priv_info;

	for (iter = 0; iter < test->max_iter; iter++) {
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			int skt_idx = sn - test->start_sn;

			while ((info->stop_req == worker_running) &&
				(test->buddy->stat == worker_running) &&
				(skts[sn] != rskt_accepting))
			{
				nanosleep(&ten_usecs, NULL);
			};
			if ((skts[sn] == rskt_accepting)) {
				int rc = rskt_close(bud->skts[skt_idx]);
				if (rc)
					goto fail;
			};
			if ((info->stop_req != worker_running) ||
			(test->buddy->stat != worker_running)) {
				CRIT("Aborting %d %d ",
					info->stop_req, test->buddy->stat);
				goto exit;
			};
		};
	};
exit:
	test->rc = 0;
	sem_post(&test->done_sema);
	return 0;
fail:
	test->rc = fail_pt;
	sem_post(&test->done_sema);
	return 0;
}

int test_speer_xchg(struct worker *info)
{
	struct rsktd_req_msg *req;
	struct rsktd_resp_msg *resp;
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;

	if (NULL == test)
		goto fail;

	req = (rsktd_req_msg *)malloc(sizeof(struct rsktd_req_msg));
	memcpy((void *)req, (void *)&test->req, sizeof(struct rsktd_req_msg));

	sem_wait(&test->speer_req_mtx);
	l_push_tail(&test->speer_req, (void *)req);
	sem_post(&test->speer_req_mtx);
	sem_post(&test->speer_req_cnt);

	sem_wait(&test->speer_resp_cnt);
	sem_wait(&test->speer_resp_mtx);
	resp = (rsktd_resp_msg *)l_pop_head(&test->speer_resp);
	sem_post(&test->speer_resp_mtx);
	memcpy((void *)&test->resp, (void *)resp, sizeof(struct rsktd_resp_msg));

	test->rc = 0;
	return 0;
fail:
	test->rc = 1;
	return 0;
}

/** @brief Test closing and reinitializing the library. 
 */
int test_case_1(void)
{
	struct sockaddr_un chk_addr;
	struct librskt_app *d_app;
	struct l_item_t *l_i;
	int sleep_more = 10;

	snprintf(chk_addr.sun_path, sizeof(chk_addr.sun_path) - 1,
                DMN_LSKT_FMT, DFLT_LIBRSKTD_TEST_PORT,
		DFLT_LIBRSKTD_TEST_MPNUM);

	/* Check library values */
	if (lib.portno != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	if (lib.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	if (lib.init_ok != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	/* FIXME: Should have a non-zero value here... */
	if (lib.ct != FAKE_LIBMPORT_CT)
		goto fail;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	if (strcmp(lib.addr.sun_path, chk_addr.sun_path))
		goto fail;
	if (lib.addr_sz != sizeof(struct sockaddr_un))
		goto fail;
	if (lib.fd <= 0)
		goto fail;
	if (lib.all_must_die)
		goto fail;
	if (l_size(&lib.msg_tx))
		goto fail;
	if (l_size(&lib.rsvp))
		goto fail;
	if (lib.lib_req_seq != 1)
		goto fail;
	if (l_size(&lib.req))
		goto fail;
	if (lib.test)
		goto fail;
	if (l_size(&lib.skts))
		goto fail;

	/* Check daemon values */
	if (lib_st.port != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	if (lib_st.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	if (lib_st.bklg != DFLT_LIBRSKTD_TEST_BKLG)
		goto fail;
	if (lib_st.tst != DFLT_LIBRSKTD_TEST_TEST)
		goto fail;
	if (!lib_st.loop_alive)
		goto fail;
	if (lib_st.all_must_die)
		goto fail;
	if (lib.ct != FAKE_LIBMPORT_CT)
		goto fail;
	if (lib_st.fd <= 0)
		goto fail;
	if (lib_st.fd <= 0)
		goto fail;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	if (strcmp(lib_st.addr.sun_path, chk_addr.sun_path))
		goto fail;
	if (1 != l_size(&lib_st.app))
		goto fail;

	d_app = (struct librskt_app *)l_head(&lib_st.app, &l_i);
	if (d_app->app_fd <= 0)
		goto fail;
	if (*d_app->self_ptr != d_app)
		goto fail;
	if (d_app->addr_size != 2)
		goto fail;
	if (!d_app->alive)
		goto fail;
	if (d_app->i_must_die)
		goto fail;
	if (d_app->dmn_req_num)
		goto fail;
	if (d_app->rx_req_num)
		goto fail;
	if (l_size(&d_app->app_resp_q))
		goto fail;
	if (l_size(&lib_st.tx_msg_q))
		goto fail;
	if (l_size(&lib_st.acc))
		goto fail;
	if (l_size(&lib_st.con))
		goto fail;
	if (l_size(&lib_st.creq))
		goto fail;
	
	/* Close the library connection */
	librskt_finish();

	if (lib.init_ok)
		goto fail;
	if (lib.fd > 0)
		goto fail;
	/* Check daemons state , after ensuring that the thread has exited */
	while ((1 == l_size(&lib_st.app)) && sleep_more) {
		sleep (1);
		sleep_more--;
	};

	if (lib_st.port != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	if (lib_st.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	if (lib_st.bklg != DFLT_LIBRSKTD_TEST_BKLG)
		goto fail;
	if (lib_st.tst != DFLT_LIBRSKTD_TEST_TEST)
		goto fail;
	if (!lib_st.loop_alive)
		goto fail;
	if (lib_st.all_must_die)
		goto fail;
	if (lib_st.fd <= 0)
		goto fail;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	if (strcmp(lib_st.addr.sun_path, chk_addr.sun_path))
		goto fail;
	if (l_size(&lib_st.app))
		goto fail;


	/* REinitialize the socket, see how things go. */
	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM))
		goto fail;

	if (lib.portno != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	if (lib.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	if (lib.init_ok != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	/* FIXME: Should have a non-zero value here... */
	if (lib.ct != FAKE_LIBMPORT_CT)
		goto fail;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	if (strcmp(lib.addr.sun_path, chk_addr.sun_path))
		goto fail;
	if (lib.addr_sz != sizeof(struct sockaddr_un))
		goto fail;
	if (lib.fd <= 0)
		goto fail;
	if (lib.all_must_die)
		goto fail;
	if (l_size(&lib.msg_tx))
		goto fail;
	if (l_size(&lib.rsvp))
		goto fail;
	if (lib.lib_req_seq != 1)
		goto fail;
	if (l_size(&lib.req))
		goto fail;
	if (lib.test)
		goto fail;
	if (l_size(&lib.skts))
		goto fail;

	/* Initialize an already running library */
	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM))
		goto fail;

	if (!librskt_init(DFLT_LIBRSKTD_TEST_PORT + 1,
						DFLT_LIBRSKTD_TEST_MPNUM))
		goto fail;

	if (!librskt_init(DFLT_LIBRSKTD_TEST_PORT,
					DFLT_LIBRSKTD_TEST_MPNUM + 1))
		goto fail;

	return 0;
fail:
	return 1;
};

/** @brief Test for binding and then closing sockets
 */
int test_case_2(void)
{
	int idx = 0;
	int rc = 1;
	int dly;
	struct rskt_test_info *test;

	start_worker_thread(&wkr[idx], -1);
	wait_for_worker_status(&wkr[idx], worker_halted);
	
	test = (struct rskt_test_info *)wkr[idx].priv_info;

	if (NULL == test)
		goto fail;

	test->num_wkrs = 1;
	test->start_sn = 1;
	test->end_sn = BIG_TEST_MAX_SN/10 + 1;
	test->max_iter = 100;
	test->rc = 0;

	run_worker_action(&wkr[0], LIB_BIND_TEST);

	for (dly = 0; dly < 1000; dly++) {
		sleep(5);
		if (!wait_for_worker_status(&wkr[idx], worker_halted))
			break;
	};

	rc = test->rc;
fail:
	return rc;
};

/** @brief Test for binding and then closing a lot of sockets, multiple threads
 */
int test_case_2A(void)
{
	int idx = 0, max_wkrs = 10;
	int max_sn = BIG_TEST_MAX_SN/10 + 1;
	int sn_per_wkr = max_sn/max_wkrs;
	int rc = 1;
	int dly;
	struct rskt_test_info *test;

	for (idx = 0; idx < max_wkrs; idx++) {
		start_worker_thread(&wkr[idx], -1);
		wait_for_worker_status(&wkr[idx], worker_halted);
	
		test = (struct rskt_test_info *)wkr[idx].priv_info;

		if (NULL == test)
			goto fail;

		test->num_wkrs = max_wkrs;
		test->start_sn = (idx * sn_per_wkr) + 1;
		test->end_sn = ((idx + 1) * sn_per_wkr);
		test->max_iter = 100;
		test->rc = 0;
		run_worker_action(&wkr[idx], LIB_BIND_TEST);
	};

	rc = 0;
	for (idx = 0; idx < max_wkrs; idx++) {
		test = (struct rskt_test_info *)wkr[idx].priv_info;

		for (dly = 0; dly < 1000; dly++) {
			if (!wait_for_worker_status(&wkr[idx],
							worker_halted))
				break;
			sleep(5);
		};
		if (wait_for_worker_status(&wkr[idx], worker_halted))
			rc = 1;
		if (test->rc)
			rc = 1;
	};
fail:
	return rc;
};

/** @brief Test for bind/listen/close a lot of sockets
 */
int test_case_3(void)
{
	int idx = 11;
	int rc = 1;
	struct rskt_test_info *test;

	start_worker_thread(&wkr[idx], -1);
	wait_for_worker_status(&wkr[idx], worker_halted);
	
	test = (struct rskt_test_info *)wkr[idx].priv_info;

	if (NULL == test)
		goto fail;

	test->num_wkrs = 1;
	test->start_sn = 100;
	test->end_sn = 200;
	test->max_iter = 100;
	test->rc = 0;

	run_worker_action(&wkr[idx], LIB_LISTEN_TEST);

	sleep(5);
	wait_for_worker_status(&wkr[idx], worker_halted);

	rc = test->rc;
fail:
	return rc;
};

/** @brief Test for bind/listen/close a lot of sockets, multiple threads
 */
int test_case_3A(void)
{
	int max_wkrs = 5;
	int idx = 11;
	int rc = 1;
	int dly;
	int max_sn = BIG_TEST_MAX_SN + 1;
	int sn_per_wkr = max_sn/max_wkrs;
	struct rskt_test_info *test;

	start_worker_thread(&wkr[idx], -1);
	wait_for_worker_status(&wkr[idx], worker_halted);
	
	for (idx = 0; idx < max_wkrs; idx++) {
		start_worker_thread(&wkr[idx], -1);
		wait_for_worker_status(&wkr[idx], worker_halted);
	
		test = (struct rskt_test_info *)wkr[idx].priv_info;

		if (NULL == test)
			goto fail;

		test->num_wkrs = max_wkrs;
		test->start_sn = (idx * sn_per_wkr) + 1;
		test->end_sn = ((idx + 1) * sn_per_wkr);
		test->max_iter = 100;
		test->rc = 0;

		run_worker_action(&wkr[idx], LIB_LISTEN_TEST);
	}

	rc = 0;
	for (idx = 0; idx < max_wkrs; idx++) {
		test = (struct rskt_test_info *)wkr[idx].priv_info;

		for (dly = 0; dly < 1000; dly++) {
			if (!wait_for_worker_status(&wkr[idx],
							worker_halted))
				break;
			sleep(5);
		};
		if (wait_for_worker_status(&wkr[idx], worker_halted))
			rc = 1;
		if (test->rc)
			rc = 1;
	};

	rc = test->rc;
fail:
	return rc;
};

/** @brief Test for bind/listen/accept/close a lot of sockets
 */
int test_case_4(void)
{
	int idx = 0;
	int buddy_idx = 1;
	int rc = 1;
	struct rskt_test_info *test, *buddy;;

	start_worker_thread(&wkr[idx], -1);
	wait_for_worker_status(&wkr[idx], worker_halted);
	
	start_worker_thread(&wkr[buddy_idx], -1);
	wait_for_worker_status(&wkr[buddy_idx], worker_halted);
	
	test = (struct rskt_test_info *)wkr[idx].priv_info;

	if (NULL == test)
		goto fail;

	test->num_wkrs = 1;
	test->start_sn = 1;
	test->end_sn = 100;
	test->max_iter = 100;
	test->rc = 0;
	sem_init(&test->done_sema, 0, 0);
	sem_init(&test->accepting, 0, 0);

	run_worker_action(&wkr[idx], LIB_ACCEPT_TEST);
	sem_wait(&test->accepting);

	buddy = (struct rskt_test_info *)wkr[buddy_idx].priv_info;

	if (NULL == test)
		goto fail;

	buddy->num_wkrs = 1;
	buddy->start_sn = test->start_sn;
	buddy->end_sn = test->end_sn;
	buddy->max_iter = test->max_iter;
	buddy->rc = 0;
	buddy->buddy = &wkr[idx];
	sem_init(&buddy->done_sema, 0, 0);
	
	run_worker_action(&wkr[buddy_idx], LIB_ACCEPT_BUDDY);

	sem_wait(&buddy->done_sema);
	sem_wait(&test->done_sema);

	CRIT("RCs: test %x buddy %x", test->rc, buddy->rc);

	rc = test->rc | buddy->rc;
fail:
	return rc;
};

/* Pound on multiple outstanding accept() requests to make sure the
 * right one alwyas gets closed...
 */
int test_case_4A(void)
{
	int idx = 0;
	int buddy_idx;
	int rc = 0;
	struct rskt_test_info *test, *buddy;;
	int max_wkrs = 5;
	int max_sn = BIG_TEST_MAX_SN + 1;
	int sn_per_wkr = max_sn/max_wkrs;

	for (idx = 0; idx < max_wkrs; idx++) {
		buddy_idx = idx + max_wkrs;

		start_worker_thread(&wkr[idx], -1);
		wait_for_worker_status(&wkr[idx], worker_halted);
		
		start_worker_thread(&wkr[buddy_idx], -1);
		wait_for_worker_status(&wkr[buddy_idx], worker_halted);
	
		test = (struct rskt_test_info *)wkr[idx].priv_info;

		if (NULL == test)
			goto fail;

		test->num_wkrs = max_wkrs;
		test->start_sn = (idx * sn_per_wkr) + 1;;
		test->end_sn = (idx + 1) * sn_per_wkr;;
		test->max_iter = 10;
		test->rc = 0;
		sem_init(&test->done_sema, 0, 0);
		sem_init(&test->accepting, 0, 0);

		run_worker_action(&wkr[idx], LIB_ACCEPT_TEST);
		sem_wait(&test->accepting);

		buddy = (struct rskt_test_info *)wkr[buddy_idx].priv_info;
	
		if (NULL == test)
			goto fail;
	
		buddy->num_wkrs = test->num_wkrs;
		buddy->start_sn = test->start_sn;
		buddy->end_sn = test->end_sn;
		buddy->max_iter = test->max_iter;
		buddy->rc = 0;
		buddy->buddy = &wkr[idx];
		sem_init(&buddy->done_sema, 0, 0);
		
		run_worker_action(&wkr[buddy_idx], LIB_ACCEPT_BUDDY);
	};

	for (idx = 0; idx < max_wkrs; idx++) {
		buddy_idx = idx + max_wkrs;
		test = (struct rskt_test_info *)wkr[idx].priv_info;
		buddy = (struct rskt_test_info *)wkr[buddy_idx].priv_info;

		sem_wait(&buddy->done_sema);
		sem_wait(&test->done_sema);
		CRIT("RCs: wkr %d test %x buddy %x", idx, test->rc, buddy->rc);

		rc |= test->rc | buddy->rc;
	};

fail:
	return rc;
};

/** @brief Test for bind/listen/accept some sockets, then close the library
 * Verify cleanup of the library, threads, and RSKTD.
 */
int test_case_5(void)
{
	int idx = 0;
	struct rskt_test_info *test;
	int max_wkrs = 10;
	int sleep_more = 5;
	timespec ten_usecs = {0, 10*1000};
	int fail_pt = 5;

	for (idx = 0; idx < max_wkrs; idx++) {
		start_worker_thread(&wkr[idx], -1);
		wait_for_worker_status(&wkr[idx], worker_halted);
	
		test = (struct rskt_test_info *)wkr[idx].priv_info;

		if (NULL == test)
			goto fail;

		test->num_wkrs = 1;
		test->start_sn = idx + 1;
		test->end_sn = idx + 1;
		test->max_iter = 1;
		test->rc = 0;
		sem_init(&test->done_sema, 0, 0);
		sem_init(&test->accepting, 0, 0);

		run_worker_action(&wkr[idx], LIB_ACCEPT_TEST);
		sem_wait(&test->accepting);
		while ((wkr[idx].stop_req == worker_running) &&
			(wkr[idx].stat == worker_running) &&
				(skts[idx+1] != rskt_accepting))
		{
			nanosleep(&ten_usecs, NULL);
		};
	};

	/* Now have num_wkrs workers accepting on sockets. */

	if (max_wkrs != l_size(&lib_st.acc))
		goto fail;

	fail_pt = 10;
	for (idx = 0; idx < max_wkrs; idx++) {
		if (skts[idx + 1] != rskt_accepting)
			goto fail;
	};

	if (max_wkrs != l_size(&lib.rsvp))
		goto fail;

	/* Close the library, see what hatches */

	librskt_finish();

	fail_pt = 20;
	if (lib.init_ok)
		goto fail;
	if (lib.fd > 0)
		goto fail;

	if (l_size(&lib.rsvp))
		goto fail;

	/* Check daemons state, after ensuring that the thread has exited */
	while ((1 == l_size(&lib_st.app)) && sleep_more) {
		sleep (1);
		sleep_more--;
	};

	fail_pt = 30;
	if (lib_st.port != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	if (lib_st.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	if (lib_st.bklg != DFLT_LIBRSKTD_TEST_BKLG)
		goto fail;
	if (lib_st.tst != DFLT_LIBRSKTD_TEST_TEST)
		goto fail;
	if (!lib_st.loop_alive)
		goto fail;
	if (lib_st.all_must_die)
		goto fail;
	if (lib_st.fd <= 0)
		goto fail;
	fail_pt = 35;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	if (l_size(&lib_st.app))
		goto fail;

	if (l_size(&lib_st.acc))
		goto fail;

	for (idx = 0; idx < max_wkrs; idx++) {
		if (skts[idx + 1] != rskt_uninit)
			goto fail;
	};

	return 0;
fail:
	return fail_pt;
};

/* Test exchange of Hello messages */

	
void kill_all_worker_threads(void) {
	int i;
	for (i = 0; i < MAX_WORKERS; i++) {
		kill_worker_thread(&wkr[i]);
	};
};

int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;

	if (0)
		argv[0][0] = argc;

	rdma_log_init("rsktd_test_log.txt", 1);

	/* Configure fake MPORT info before doing any initialization */
	dmn.qresp.hdid = FAKE_LIBMPORT_CT;

	g_level = 2;

	if (init_mport_and_mso_ms()) {
		ERR("init_mport_and_mso_ms failed. EXITING");
		goto fail;
	};

	if (start_msg_proc_q_thread()) {
		ERR("Could not start message procssor thread. EXITING");
		goto fail;
	};
	if (start_lib_handler(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM,
			DFLT_LIBRSKTD_TEST_BKLG, DFLT_LIBRSKTD_TEST_TEST)) {
		ERR("Could not start lib handler. EXITING");
		goto fail;
	};

	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM)) {
		ERR("Could not start rskt library. EXITING");
		goto fail;
	};

	config_unit_test(&stub_utd);

	cli_init_base(cleanup_proc);
	bind_unit_test_thread_cli_cmds();
	librsktd_bind_cli_cmds();
	liblog_bind_cli_cmds();

	if (test_case_1()) {
		CRIT("Test case 1 FAILED\n");
		goto fail;
	};
	CRIT("Test case 1 Passed\n");

	if (test_case_2()) {
		CRIT("Test case 2 FAILED\n");
		goto fail;
	};
	CRIT("Test case 2 Passed\n");

	if (test_case_2A()) {
		CRIT("Test case 2A FAILED\n");
		goto fail;
	};
	CRIT("Test case 2A Passed\n");

	if (test_case_3()) {
		CRIT("Test case 3 FAILED\n");
		goto fail;
	};
	CRIT("Test case 3 Passed\n");

	if (test_case_3A()) {
		CRIT("Test case 3A FAILED\n");
		goto fail;
	};
	CRIT("Test case 3A Passed\n");

	kill_all_worker_threads();
	if (test_case_4()) {
		CRIT("Test case 4 FAILED\n");
		goto fail;
	};
	CRIT("Test case 4 Passed\n");

	kill_all_worker_threads();
	if (test_case_4A()) {
		CRIT("Test case 4A FAILED\n");
		goto fail;
	};
	CRIT("Test case 4A Passed\n");

	kill_all_worker_threads();
	rc = test_case_5();
	if (rc) {
		CRIT("Test case 5 FAILED %d \n", rc);
		goto fail;
	};
	CRIT("Test case 5 Passed\n");

	/* Now start SPEER tests */
	if (start_speer_conn(DFLT_LIBRSKTD_TEST_CM_SKT,
			DFLT_LIBRSKTD_TEST_MPNUM, TEST_ZERO_MS, TEST_ZERO_MS_SZ,
			TEST_SKIP_MS, NOT_TEST)) {
		CRIT("Could not start speer connection manager. EXITING");
		goto fail;
	};
	CRIT("Started connection manager.");

        splashScreen((char *)"RSKT UNIT TEST");
	console((void *)"RSKT_TEST > ");
	

	g_level = 7;

	cleanup_proc(NULL);
	HIGH("Cleanup completed\n");
fail:
	exit(rc);
	return 0;
}

#ifdef __cplusplus
}
#endif

