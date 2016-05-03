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

#include <unistd.h>
#include "libcli.h"
#include "liblog.h"
#include "librsktd_lib.h"
#include "librsktd_lib_info.h"
#include "librskt.h"
#include "libunit_test.h"
#include "librskt_private.h"
#include "librsktd_sn.h"
#include "librsktd_dmn.h"
#include "librsktd_speer.h"
#include "fake_libmport.h"
#include "rskt_worker.h"
#include "librsktd_fm.h"
#include "fake_libfmdd.h"
#include "assert.h"

#define DFLT_LIBRSKTD_TEST_PORT 1234
#define DFLT_LIBRSKTD_TEST_MPNUM 0
#define DFLT_LIBRSKTD_TEST_BKLG 35
#define DFLT_LIBRSKTD_TEST_TEST 0
#define DFLT_LIBRSKTD_TEST_CM_SKT 4395
#define DFLT_LIBRSKTD_TEST_MPNUM 0
#define TEST_NUM_MS 16
#define TEST_MS_SZ (64*1024)
#define TEST_SKIP_MS 0
#define NOT_TEST 0

#ifdef __cplusplus
extern "C" {
#endif

/* BIG_TEST_MAX_SN could be up to 10000 */
#define BIG_TEST_MAX_SN 1000
/* BIG_TEST_MAX_ITER could be up to 100 */
#define BIG_TEST_MAX_ITER 1

void cleanup_proc(struct cli_env *env)
{
	int i;

	if (NULL == env)
		i = 0;

	lib.all_must_die = 1;
	dmn.all_must_die = 1;
        kill_acc_conn = 1;

	halt_msg_proc_q_thread();
	halt_lib_handler();
	halt_speer_handler();
	halt_wpeer_handler();
	halt_fm_thread();
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

	(char *)"SP_CONN",
	(char *)"WP_ACCE",

	(char *)"SPWP_DR"
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

int test_speer_connect(struct worker *info);
int test_wpeer_accept(struct worker *info);

int test_speer_wpeer_driver(struct worker *info);

void kill_all_worker_threads(void);

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

	case SPEER_CONN: rc = test_speer_connect(info);
			break;
	case WPEER_ACC: rc = test_wpeer_accept(info);
			break;

	case SPEER_WPEER_DRIVER: rc = test_speer_wpeer_driver(info);
			break;

	default: CRIT("Unknown worker action...");
		rc = 1;
	};
	return rc;
};
	
void create_priv(struct worker *info)
{
	struct rskt_test_info *test;
	
	if (NULL != info->priv_info)
		test = (struct rskt_test_info *)info->priv_info;
	else
		test = (struct rskt_test_info *)
			malloc(sizeof(struct rskt_test_info));

	memset(test, 0, sizeof(struct rskt_test_info));
	sem_init(&test->speer_acc, 0, 0);
	sem_init(&test->speer_con, 0, 0);
	l_init(&test->speer_req);
	sem_init(&test->speer_req_cnt, 0, 0);
	sem_init(&test->speer_req_mtx, 0, 1);
	test->speer_resp_err = 0;
	l_init(&test->speer_resp);
	sem_init(&test->speer_resp_cnt, 0, 0);
	sem_init(&test->speer_resp_mtx, 0, 1);
	memset((void *)&test->req, 0, sizeof(struct rsktd_req_msg));
	memset((void *)&test->resp, 0, sizeof(struct rsktd_resp_msg));
	test->acc_sent = 0;
	test->acc_received = 0;
	test->con_sent = 0;
	test->con_received = 0;

	info->priv_info = (void *)test;
};

void destroy_priv(struct worker *info)
{
	if (info->priv_info) {
		free((void *)info->priv_info);
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

int all_app_lib_queues_empty_checks(void)
{
	if (l_size(&lib.msg_tx)) {
		WARN("lib.msgtx");
		goto fail;
	};
	if (l_size(&lib.rsvp)) {
		WARN("lib.rsvp");
		goto fail;
	};
	if (l_size(&lib.req)) {
		WARN("lib.req");
		goto fail;
	};
	if (l_size(&lib_st.tx_msg_q)) {
		WARN("lib.rtx_msg_q");
		goto fail;
	};
	if (l_size(&lib_st.acc)) {
		WARN("lib_st.acc");
		goto fail;
	};
	if (l_size(&lib_st.con)) {
		WARN("lib_st.con");
		goto fail;
	};
	if (l_size(&lib_st.creq)) {
		WARN("lib_st.creq");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int all_app_lib_queues_empty(struct worker *info)
{
	struct rskt_test_info *test = (struct rskt_test_info *)
							info->priv_info;
	int attempts;

	/* Can't check for queue state with multiple threads active */
	if (test->num_wkrs > 1)
		return 0;

	for (attempts = 0; attempts < 2; attempts++) {
		if (!all_app_lib_queues_empty_checks())
			goto success;
		usleep(10);
	}
	return 1;
success:
	return 0;
};

int all_app_lib_queues_stat(struct worker *info,
				int acc_sz, int con_sz, int creq_sz)
{
	struct rskt_test_info *test = (struct rskt_test_info *)
							info->priv_info;
	int fail_pt = 1;

	/* Can't check for queue state with multiple threads active */
	if (test->num_wkrs > 1)
		return 0;

	if (l_size(&lib.msg_tx))
		goto fail;
	fail_pt = 2;
	if (l_size(&lib.rsvp)) {
		fail_pt += l_size(&lib.rsvp) << 4;
		goto fail;
	};
	fail_pt = 3;
	if (l_size(&lib.req))
		goto fail;
	fail_pt = 4;
	if (l_size(&lib_st.tx_msg_q))
		goto fail;
	fail_pt = 5;
	if (l_size(&lib_st.acc) != acc_sz)
		goto fail;
	fail_pt = 6;
	if (l_size(&lib_st.con) != con_sz)
		goto fail;
	fail_pt = 7;
	if (l_size(&lib_st.creq) != creq_sz)
		goto fail;
	fail_pt = 8;
	return 0;
fail:
	return fail_pt;
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
		sleep(1);
		rc = all_app_lib_queues_empty(info);
		if (rc) {
			fail_pt += rc << 8;
			goto fail;
		};
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

	free(skts);
	test->rc = 0;
	return 0;
fail:
	free(skts);
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
		};
		fail_pt = 7;
		rc = all_app_lib_queues_stat(info, num_skts, 0, 0);
		if (rc) {
			fail_pt += rc << 16;
			goto fail;
		};
		fail_pt = 8;
		for (sn = test->start_sn; sn <= test->end_sn; sn++) {
			sa.ct = sn + 1;
			sa.sn = sn;
			rc = rskt_listen(skts[sn - test->start_sn], sn + 2);
			if (!rc)
				goto fail;
		};
		fail_pt = 9;
		rc = all_app_lib_queues_stat(info, num_skts, 0, 0);
		if (rc) {
			fail_pt += rc << 16;
			goto fail;
		};
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
	l_skts = (rskt_h *)malloc(num_skts * sizeof(rskt_h));

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
 			* to close the accepting socket 
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
	rskt_destroy_socket(&acc_skt);
		fail_pt = 14;
	if (all_app_lib_queues_empty(info)) {
		sleep(1);
		if (all_app_lib_queues_empty(info)) {
			goto fail;
		};
	};

		fail_pt = 15;
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		sa.ct = sn + 1;
		sa.sn = sn;
		rc = rskt_listen(l_skts[sn - test->start_sn], sn + 2);
		if (!rc)
			goto fail;
	};
		fail_pt = 16;
	if (all_app_lib_queues_empty(info)) {
		sleep(1);
		if (all_app_lib_queues_empty(info)) {
			goto fail;
		};
		goto fail;
	};
	for (sn = test->start_sn; sn <= test->end_sn; sn++) {
		int idx = sn - test->start_sn;
		rskt_destroy_socket(&l_skts[idx]);
	};
	test->skts = NULL;
	free(l_skts);
	test->rc = 0;
	sem_post(&test->done_sema);
	return 0;
fail:
	free(l_skts);
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
				int rc;
				DBG("Closing skts[ %d ] idx %d", sn, skt_idx);
				rc = rskt_close(bud->skts[skt_idx]);
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

int test_speer_connect(struct worker *info)
{
	int fail_pt = 5;
	int ret = 0;
	riomp_mailbox_t mbox;
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;
        struct rsktd_resp_msg *rsp_p = &test->act_resp;
	riomp_sock_t sock_h;

	if (riomp_sock_mbox_create_handle(0, 0, &mbox))
		goto fail;

	if (riomp_sock_socket(mbox, &sock_h))
		goto fail;

	if (riomp_sock_connect(sock_h, info->idx, 0, 1))
		goto fail;

	sleep(1);

	fail_pt = 10;
 
	while (info->stop_req == worker_running) {
		struct rsktd_resp_msg *resp =
			(struct rsktd_resp_msg *)malloc(DMN_RESP_SZ);

		INFO("SPEER %d waiting new request to TX", info->idx);
		while ((info->stop_req == worker_running) && !test->new_req)
			sleep(0);
		fail_pt = 15;
		if (info->stop_req != worker_running)
			continue;
		fail_pt = 20;
		INFO("SPEER %d SENDing request", info->idx);
		ret = riomp_sock_send(sock_h, &test->req, DMN_REQ_SZ);
		if (ret) {
			INFO("SPEER %d SENDing request FAILED %x",
				 info->idx, ret);
			goto fail;
		};
		fail_pt = 25;
		if (info->stop_req != worker_running)
			continue;
		fail_pt = 30;
		memset(resp, 0, sizeof(struct rsktd_resp_msg));
		ret = riomp_sock_receive(sock_h, (void **)&rsp_p, DMN_RESP_SZ, 0);
		if (ret) {
			INFO("SPEER %d SENDing request FAILED %x",
				 info->idx, ret);
			goto fail;
		};
		if (NULL == rsp_p)
			goto fail;
		test->new_req = 0;
	};
	INFO("SPEER %d exiting...", info->idx);
	test->rc = 0;
	return 0;
fail:
	CRIT("SPEER %d exiting fail pt %d... rc %d",
			info->idx, fail_pt, ret);
	test->rc = fail_pt;
	sem_post(&test->done_sema);
	return 0;
};

#ifdef NOT_DEFINED
#endif

int test_wpeer_accept(struct worker *info)
{
	int fail_pt = 5;
        struct rsktd_req_msg *req = (struct rsktd_req_msg *)
					malloc(sizeof(struct rsktd_req_msg));
	riomp_mailbox_t mbox;
	struct rskt_test_info *test;
	riomp_sock_t sock_h, new_h;

	INFO("Started...");
	/* There should not be any peers now */
	test = (struct rskt_test_info *)info->priv_info; 
	if (riomp_sock_mbox_create_handle(0, 0, &mbox))
		goto fail;

	if (riomp_sock_socket(mbox, &sock_h))
		goto fail;

	if (riomp_sock_socket(mbox, &new_h))
		goto fail;

	new_h->wkr_idx = info->idx;
	new_h->acceptor = TEST_SKT_ACCEPT;

	INFO("Waiting for sock_accept");
	if (riomp_sock_accept(sock_h, &new_h, 0))
		goto fail;

	sleep(1);
	
	while (info->stop_req == worker_running) {
		int ret;
		fail_pt = 20;
		INFO("WKR %d WPEER_ACC Waiting for new resp mesage",
			info->idx);
		while ((info->stop_req == worker_running) && !test->new_resp)
			sleep(0);

		if (info->stop_req != worker_running)
			continue;

		fail_pt = 25;
		INFO("WKR %d WPEER_ACC Receiving message", info->idx);
		ret = riomp_sock_receive(new_h, (void **)&req, DMN_REQ_SZ, 0);
		INFO("WKR %d WPEER_ACC REceive ret %d", info->idx, ret);
		if (ret)
			goto fail;
		if (NULL == req)
			goto fail;
		
		if ((test->req.msg_type != req->msg_type) ||
		(test->req.msg.clos.rem_sn != req->msg.clos.rem_sn) ||
		(test->req.msg.clos.loc_sn != req->msg.clos.loc_sn) ||
		(test->req.msg.clos.force != req->msg.clos.force)) {
			INFO("WKR %d WPEER_ACC Request mismatch:\n\t"
			"Exp: type %d rem_sn %d loc_sn %d force %d\n\t"
			"Act: type %d rem_sn %d loc_sn %d force %d\n\t",
				info->idx, htonl(test->req.msg_type),
				htonl(test->req.msg.clos.rem_sn),
				htonl(test->req.msg.clos.loc_sn),
				htonl(test->req.msg.clos.force),
				htonl(req->msg_type),
				htonl(req->msg.clos.rem_sn),
				htonl(req->msg.clos.loc_sn),
				htonl(req->msg.clos.force));
		};
		memcpy(&test->resp.req, req, sizeof(union librsktd_req));
		test->resp.msg_seq = req->msg_seq;

		if (info->stop_req != worker_running)
			continue;
		fail_pt = 30;
		INFO("WKR %d WPEER_ACC SENDing message", info->idx);
		if (riomp_sock_send(new_h, &test->resp, DMN_RESP_SZ))
			goto fail;
		test->new_resp = 0;
		if (info->stop_req != worker_running)
			continue;
	};
	INFO("WKR %d WPEER_ACC EXITING", info->idx);
	return 0;

fail:
	test->rc = fail_pt;
	sem_post(&test->done_sema);
	INFO("WKR %d WPEER_ACC EXITING RC= %d", test->rc, test->rc);
	return 0;
};

int test_speer_wpeer_driver(struct worker *info)
{
	int fail_pt = 5;
	struct rskt_dmn_speer **sp;
	struct rskt_dmn_wpeer **wp;
	struct rskt_test_info *s_t, *w_t;
	uint32_t sp_idx;
	uint32_t wp_idx;
	uint32_t speer_seq_no;
	uint32_t speer_sn;
	uint32_t speer_cm_skt_num;
	uint32_t wpeer_seq_no = 1;
	uint32_t wpeer_cm_skt;
	int rc = 0;
	rskt_h *l_skts;
	struct rskt_sockaddr sa;
	uint32_t sn = 0;
	uint32_t num_skts;
	rskt_h acc_skt;
	struct rskt_sockaddr acc_sa;
	int skt_idx = 0;
	int iter = 0;
	struct rskt_test_info *test;
	
	test = (struct rskt_test_info *)info->priv_info; 
	
	num_skts = test->end_sn - test->start_sn + 1;
	sp_idx = test->sp_idx;
	wp_idx = test->wp_idx;
	speer_seq_no = test->speer_seq_no;
	speer_sn = test->speer_sn;
	speer_cm_skt_num = test->speer_cm_skt_num;
	wpeer_cm_skt = test->wpeer_cm_skt;

	/* Start two workers. */
	start_worker_thread(&wkr[sp_idx], -1);
	start_worker_thread(&wkr[wp_idx], -1);

	wait_for_worker_status(&wkr[sp_idx], worker_halted);
	wait_for_worker_status(&wkr[wp_idx], worker_halted);
	
	s_t = (struct rskt_test_info *)wkr[sp_idx].priv_info;
	w_t = (struct rskt_test_info *)wkr[wp_idx].priv_info;

	/* Set up HELLO req/resp for SPEER, this should go immediately */
	fail_pt = 5;
	while (s_t->new_req &&
			wait_for_worker_status(&wkr[sp_idx], worker_running))
		sleep(0);

	if (!wait_for_worker_status(&wkr[sp_idx], worker_running))
		goto fail;

        s_t->req.msg_type = htonl(RSKTD_HELLO_REQ);
        s_t->req.msg_seq = htonl(speer_seq_no);
        s_t->req.msg.hello.ct = htonl(wp_idx); /* Yes, wp_idx */
        s_t->req.msg.hello.cm_skt = htonl(speer_cm_skt_num);
        s_t->req.msg.hello.cm_mp = htonl(1);

	s_t->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	s_t->resp.msg_seq = htonl(speer_seq_no);
	s_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
	s_t->resp.msg.hello.peer_pid = htonl(getpid());

	s_t->new_req = 1;

	/* Set up HELLO req/resp for WPEER */
	while (w_t->new_resp)
		sleep(0);

        w_t->req.msg_type = htonl(RSKTD_HELLO_REQ);
        w_t->req.msg_seq = htonl(0);
        w_t->req.msg.hello.ct = htonl(wp_idx);
        w_t->req.msg.hello.cm_skt = htonl(wpeer_cm_skt);
        w_t->req.msg.hello.cm_mp = htonl(1);

	w_t->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	w_t->resp.msg_seq = htonl(0);
	w_t->resp.err = htonl(0);
	memcpy(&w_t->resp.req, &w_t->req.msg, sizeof(union librsktd_req));
	w_t->resp.msg.hello.peer_pid = htonl(getpid());
	
	w_t->new_resp = 1;

	sem_post(&test->speer_wpeer_init_complete);
	sem_wait(&test->all_workers_ready);
	/* Start up WPEER process, should connect with worker thread*/
	fail_pt = 10;
	
	sleep(1);
	/* Not a typo, dmn.speers enqueues **speer. */
	sp = dmn.speers[0].self_ref;

	if (NULL == sp)
		goto fail;

	fail_pt = 11;
	wp = dmn.wpeers[0].self_ref;

	if (NULL == wp)
		goto fail;

	/* Get Library to bind/connect/accept to a socket.
	* Then have speer send in connect request, confirm success.
	*/

	/* Note: All memory spaces already allocated by test_case_7 */

	fail_pt = 25;
	acc_skt = rskt_create_socket();
	l_skts = (rskt_h *)malloc( sizeof(rskt_h)*(num_skts));

	for (iter = 0; iter < test->max_iter; iter++) {
		CRIT("%d Iter %d", info->idx, iter);
		for (sn = test->speer_acc_sn;
				sn < test->speer_acc_sn + num_skts; sn++) {
			fail_pt = 30;
			skt_idx = sn - test->speer_acc_sn;

			l_skts[skt_idx] = rskt_create_socket();
			if (NULL == l_skts[skt_idx])
				goto fail;
	
			fail_pt = 30;
			sa.sn = sn;
			sa.ct = wp_idx;
			rc = rskt_bind(l_skts[skt_idx], &sa);
			if (rc)
				goto fail;
	
			fail_pt = 35;
			rc = rskt_listen(l_skts[skt_idx], 50);
			if (rc)
				goto fail;
		
			fail_pt = 40;
	
			/* Send in connect request from SPEER */
			while (s_t->new_req)
				sleep(0);
	
			speer_seq_no++;
        		s_t->req.msg_type = htonl(RSKTD_CONNECT_REQ);
        		s_t->req.msg_seq = htonl(speer_seq_no);
        		s_t->req.msg.con.dst_sn = htonl(sn);
        		s_t->req.msg.con.dst_ct = htonl(wp_idx); /* Yes, wp_idx */
        		s_t->req.msg.con.src_sn = htonl(speer_sn);
        		strncpy(s_t->req.msg.con.src_mso, "SRC_MSO", MAX_MS_NAME);
        		strncpy(s_t->req.msg.con.src_ms, "SRC_MS", MAX_MS_NAME);
        		s_t->req.msg.con.src_msub_o = 0;
        		s_t->req.msg.con.src_msub_s = 64*1024;
	
			s_t->resp.msg_type = htonl(RSKTD_CONNECT_RESP);
			s_t->resp.msg_seq = htonl(speer_seq_no);
			s_t->resp.err = htonl(0);
			memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        		s_t->resp.msg.con.acc_sn = htonl(RSKTD_DYNAMIC_SKT);
        		s_t->resp.msg.con.dst_sn = htonl(sn);
        		s_t->resp.msg.con.dst_ct = htonl(6);
        		s_t->resp.msg.con.dst_dmn_cm_skt = htonl(speer_cm_skt_num);
			snprintf(s_t->resp.msg.con.dst_ms, MAX_MS_NAME, 
				"RSKT_DAEMON%05d.%03d", getpid(), 0);
        		s_t->resp.msg.con.msub_sz = htonl(64*1024);
	
			s_t->new_req = 1;
	
			fail_pt = 45;
			rc = rskt_accept(l_skts[skt_idx], acc_skt, &acc_sa);
			if (rc)
				goto fail;
		
			fail_pt = 50;
			if (wait_for_worker_status(&wkr[sp_idx], worker_running))
				goto fail;
	
			fail_pt = 51;
			if (wait_for_worker_status(&wkr[wp_idx], worker_running))
				goto fail;
	
			HIGH("%d Iter %d Acc sn %d %s Con sn %d %s", 
				info->idx, iter,
				sn, SKT_STATE_STR(skts[sn]), 
				acc_sa.sn, SKT_STATE_STR(skts[acc_sa.sn]));
			/* Send in close request from LIBRARY */
			while (w_t->new_resp)
				sleep(0);
	
			memset(&w_t->req, 0, sizeof(w_t->req));
			memset(&w_t->resp, 0, sizeof(w_t->resp));
        		w_t->req.msg_type = htonl(RSKTD_CLOSE_REQ);
        		w_t->req.msg_seq = htonl(wpeer_seq_no);
        		w_t->req.msg.clos.rem_sn = htonl(acc_sa.sn);
        		w_t->req.msg.clos.loc_sn = htonl(speer_sn);
        		w_t->req.msg.clos.force = htonl(1);
	
			w_t->resp.msg_type = htonl(RSKTD_CLOSE_RESP);
			w_t->resp.msg_seq = htonl(1);
			w_t->resp.err = htonl(0);
			memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        		w_t->resp.msg.clos.status = htonl(0);
			w_t->speer_resp_err = 0;
	
			w_t->new_resp = 1;
	
			fail_pt = 60;
			rc = rskt_close(acc_skt);
			if (rc)
				goto fail;

			if (acc_skt->skt != NULL)
				ERR("Socket not closed correctly.");
			
			sleep(0);
			/* Send in connect request from SPEER */
			while (s_t->new_req && (wkr[sp_idx].stat == worker_running))
				sleep(0);
	
			fail_pt = 70;
			if (!(wkr[sp_idx].stat == worker_running))
				goto fail;
	
			fail_pt = 71;
			speer_seq_no++;
        		s_t->req.msg_type = htonl(RSKTD_CONNECT_REQ);
        		s_t->req.msg_seq = htonl(speer_seq_no);
        		s_t->req.msg.con.dst_sn = htonl(sn); /* Yes, wp_idx */
        		s_t->req.msg.con.dst_ct = htonl(wp_idx);
        		s_t->req.msg.con.src_sn = htonl(speer_sn);
        		strncpy(s_t->req.msg.con.src_mso, "SRC_MSO", MAX_MS_NAME);
        		strncpy(s_t->req.msg.con.src_ms, "SRC_MS", MAX_MS_NAME);
        		s_t->req.msg.con.src_msub_o = 0;
        		s_t->req.msg.con.src_msub_s = 64*1024;
	
			s_t->resp.msg_type = htonl(RSKTD_CONNECT_RESP);
			s_t->resp.msg_seq = htonl(speer_seq_no);
			s_t->resp.err = htonl(0);
			memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        		s_t->resp.msg.con.acc_sn = htonl(RSKTD_DYNAMIC_SKT);
        		s_t->resp.msg.con.dst_sn = htonl(sn);
        		s_t->resp.msg.con.dst_ct = htonl(wp_idx);
        		s_t->resp.msg.con.dst_dmn_cm_skt =
						htonl(speer_cm_skt_num);
			snprintf(s_t->resp.msg.con.dst_ms, MAX_MS_NAME, 
				"RSKT_DAEMON%05d.%03d", getpid(), 0);
        		s_t->resp.msg.con.msub_sz = htonl(64*1024);
	
			s_t->new_req = 1;
	
			fail_pt = 80;
			rc = rskt_accept(l_skts[skt_idx], acc_skt, &acc_sa);
			if (rc)
				goto fail;
		
			fail_pt = 81;
			if (wkr[sp_idx].stat != worker_running)
				goto fail;
		
			fail_pt = 82;
			if (wkr[wp_idx].stat != worker_running)
				goto fail;
	
			if (acc_skt->skt == NULL)
				ERR("Socket not actually accepted.");
			
			/* Send in close request from SPEER */
			fail_pt = 90;
			while (s_t->new_req &&
					(wkr[sp_idx].stat == worker_running))
				sleep(0);
			if (wkr[sp_idx].stat != worker_running)
				goto fail;
	
			fail_pt = 91;
	
			DBG("Closing from the other side");
			memset(&s_t->req, 0, sizeof(w_t->req));
			memset(&s_t->resp, 0, sizeof(w_t->resp));
			speer_seq_no++;
        		s_t->req.msg_type = htonl(RSKTD_CLOSE_REQ);
        		s_t->req.msg_seq = htonl(speer_seq_no);
			DBG("acc_sa.sn %d acc_skt->sn %d",
				acc_sa.sn, acc_skt->sa.sn);
        		s_t->req.msg.clos.loc_sn = htonl(acc_sa.sn);
        		s_t->req.msg.clos.rem_sn = htonl(speer_sn);
        		s_t->req.msg.clos.force = htonl(1);
		
			s_t->resp.msg_type = htonl(RSKTD_CLOSE_RESP);
			s_t->resp.msg_seq = htonl(speer_seq_no);
			s_t->resp.err = htonl(0);
			memcpy(&s_t->resp.req, &s_t->req.msg,
						sizeof(union librsktd_req));
        		s_t->resp.msg.clos.status = htonl(rskt_closed);
			s_t->speer_resp_err = 0;
	
			s_t->new_req = 1;
	
			fail_pt = 100;
			while (s_t->new_req && 
					(wkr[sp_idx].stat == worker_running))
				sleep(0);
			if (wkr[sp_idx].stat != worker_running)
				goto fail;
			fail_pt = 101;
			if (wkr[wp_idx].stat != worker_running)
				goto fail;
	
			while (acc_skt->skt != NULL)
				sleep(0);
			
			/* Now close the listening socket */
			/* This is a local operation, no need */
			/*   for message setup */
			fail_pt = 102;
			if (rskt_close(l_skts[skt_idx]))
				goto fail;
			sleep(0);
		};
	};
	
	return 0;
fail:
	test->rc = fail_pt + (1000 * sn) + (100000000 * iter);
	return 1;
};


/** @brief Test of the message allocation/deallocation tests.
 */
int test_case_0(void)
{
	struct librsktd_unified_msg *u_msg[MAX_MSG];
	int fail_pt;
	int i;

	for (i = 0; i < MAX_MSG; i++) {
		u_msg[i] = alloc_msg(i+1, (i+1) << 8, (i+1) << 16);
		u_msg[i]->dreq = alloc_dreq();
		u_msg[i]->dresp = alloc_dresp();
		u_msg[i]->tx = alloc_tx();
		u_msg[i]->rx = alloc_rx();

		if ((NULL == u_msg[i]->dreq) ||
			(NULL == u_msg[i]->dresp) ||
			(NULL == u_msg[i]->tx) ||
			(NULL == u_msg[i]->rx)) {
				fail_pt = 1000 + i;
				goto fail;
		}
	};
	
	for (i = 0; i < MAX_MSG; i++) {
		if (dealloc_msg(u_msg[i])) {
			fail_pt = 2100+i;
			goto fail;
		};
		u_msg[i] = NULL;
	};
	
	for (i = 0; i < MAX_MSG; i++) {
		if (!dealloc_msg(u_msg[i])) {
			fail_pt = 9100+i;
			goto fail;
		};

	};
	
	i = 100;

	return 0;
fail:
	return fail_pt;
};
		
		
	
/** @brief Test closing and reinitializing the library. 
 */
int test_case_1(void)
{
	struct sockaddr_un chk_addr;
	struct librskt_app *d_app;
	int sleep_more = 10;
	int fail_pt = 5;

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
	fail_pt = 10;
	if (lib_st.port != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	if (lib_st.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	if (lib_st.bklg != DFLT_LIBRSKTD_TEST_BKLG)
		goto fail;
	if (!lib_st.lib_conn_loop_alive)
		goto fail;
	if (dmn.all_must_die)
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
	if ((lib_st.apps[0].app_fd <= 0) || lib_st.apps[0].i_must_die ||
			!lib_st.apps[0].alive) 
		goto fail;

	fail_pt = 15;
	d_app = &lib_st.apps[0];
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
	
	fail_pt = 20;
	/* Close the library connection */
	librskt_finish();

	if (lib.init_ok)
		goto fail;
	if (lib.fd > 0)
		goto fail;
	/* Check daemons state , after ensuring that the thread has exited */
	while ((lib_st.apps[0].alive || lib_st.apps[0].app_fd > 0)
								&& sleep_more) {
		sleep (1);
		sleep_more--;
	};

	fail_pt = 25;
	if (lib_st.port != DFLT_LIBRSKTD_TEST_PORT)
		goto fail;
	fail_pt = 125;
	if (lib_st.mpnum != DFLT_LIBRSKTD_TEST_MPNUM)
		goto fail;
	fail_pt = 225;
	if (lib_st.bklg != DFLT_LIBRSKTD_TEST_BKLG)
		goto fail;
	fail_pt = 425;
	if (!lib_st.lib_conn_loop_alive)
		goto fail;
	fail_pt = 525;
	if (dmn.all_must_die)
		goto fail;
	fail_pt = 625;
	if (lib_st.fd <= 0)
		goto fail;
	fail_pt = 725;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	fail_pt = 825;
	if (strcmp(lib_st.addr.sun_path, chk_addr.sun_path))
		goto fail;
	fail_pt = 925;
	if (d_app->app_fd > 0)
		goto fail;


	/* REinitialize the socket, see how things go. */
	fail_pt = 30;
	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM))
		goto fail;

	fail_pt = 35;
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
	fail_pt = 40;
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
	return fail_pt;
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
	test->max_iter = BIG_TEST_MAX_ITER;
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
		test->max_iter = BIG_TEST_MAX_ITER;
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
	test->max_iter = BIG_TEST_MAX_ITER;
	test->rc = 0;

	run_worker_action(&wkr[idx], LIB_LISTEN_TEST);

	do {
		sleep(5);
	} while (wait_for_worker_status(&wkr[idx], worker_halted));

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
		test->max_iter = BIG_TEST_MAX_ITER;
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
	test->max_iter = BIG_TEST_MAX_ITER;
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
		test->max_iter = BIG_TEST_MAX_ITER/10 + 1;
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
		if (test->rc || buddy->rc)
			CRIT("RCs: wkr %d test %x buddy %x",
				idx,test->rc, buddy->rc); 

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
	while (lib_st.apps[0].alive && sleep_more) {
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
	if (!lib_st.lib_conn_loop_alive)
		goto fail;
	if (dmn.all_must_die)
		goto fail;
	if (lib_st.fd <= 0)
		goto fail;
	fail_pt = 35;
	if (lib.addr.sun_family != AF_UNIX)
		goto fail;
	fail_pt = 36;
	if (lib_st.apps[0].alive)
		goto fail;

	// Wait for RSKTD Daemon to clean up socket state. 
	sleep(5);
	for (idx = 0; idx < max_wkrs; idx++) {
		if (skts[idx + 1] != rskt_uninit) {
			CRIT("skts[ %d ]  is %d not %d", idx+1, skts[idx + 1], int(rskt_uninit));
			fail_pt = 40+idx;
			goto fail;
		};
	};

	return 0;
fail:
	return fail_pt;
};

/* Test exchange of Hello messages */

/* Shared between test case 6 and 7. */
#define TX_CT 0x9394
#define TX_CM_SKT  0x9399
#define TX_CM_MP  99

riomp_sock_t sock_h;

int test_case_6(void)
{
	int idx = 0, rc;
	int fail_pt = 5;
        struct rsktd_resp_msg dresp;
        struct rsktd_resp_msg *dresp_p = &dresp;
	riomp_mailbox_t mbox;
	struct rskt_dmn_speer *sp;
	uint32_t tx_ct = TX_CT;
	uint32_t tx_cm_skt = TX_CM_SKT;
	uint32_t tx_cm_mp = TX_CM_MP;
	char sp_name[16];
	char exp_name[16];
	struct rskt_test_info *test;

	/* There should not be any peers now */
	if (dmn.speers[0].alive)
		goto fail; 

	start_worker_thread(&wkr[idx], -1);
	wait_for_worker_status(&wkr[idx], worker_halted);
	
	test = (struct rskt_test_info *)wkr[idx].priv_info;

	if (riomp_sock_mbox_create_handle(0, 0, &mbox))
		goto fail;

	if (riomp_sock_socket(mbox, &sock_h))
		goto fail;

	if (riomp_sock_connect(sock_h, idx, 0, 1))
		goto fail;

	sleep(1);

	/* There should now be one SPEER waiting on this worker thread */
	fail_pt = 10;

	if (dmn.speers[0].alive || dmn.speers[0].i_must_die)
		goto fail; 
	
	sp = &dmn.speers[0];

	/* Not a typo, dmn.speers enqueues **speer. */
	if (NULL == sp)
		goto fail;

	if (!sp->cm_skt_h_valid || sp->i_must_die || sp->comm_fail)
		goto fail;

	if (sp->self_ref == NULL)
		goto fail;

	if (sp->ct || sp->cm_skt_num || sp->cm_mp || sp->got_hello)
		goto fail;

	/* Now its time to send the HELLO request */
	fail_pt = 10;
 
        test->req.msg_type = htonl(RSKTD_HELLO_REQ);
        test->req.msg.hello.ct = htonl(tx_ct);
        test->req.msg.hello.cm_skt = htonl(tx_cm_skt);
        test->req.msg.hello.cm_mp = htonl(tx_cm_mp);

	if (riomp_sock_send(sock_h, &test->req, DMN_REQ_SZ))
		goto fail;
	
	/* And receive the corresponding response ... */
	fail_pt = 30;
	memset(&dresp, 0, DMN_RESP_SZ);
	if (riomp_sock_receive(sock_h, (void **)&dresp_p, DMN_RESP_SZ, 0))
		goto fail;

	if (dresp.msg_type  != htonl(RSKTD_HELLO_RESP))
		goto fail;
	if (dresp.msg_seq)
		goto fail;
	if (dresp.err)
		goto fail;

	if (dresp.req.hello.ct != htonl(tx_ct))
		goto fail;
	if (dresp.req.hello.cm_skt != htonl(tx_cm_skt))
		goto fail;
	if (dresp.req.hello.cm_mp != htonl(tx_cm_mp))
		goto fail;

	if (dresp.msg.hello.peer_pid != htonl(getpid()))
		goto fail;

	/* Check that speer data reflects HELLO contents */
	fail_pt = 40;
	if (sp->ct != tx_ct)
		goto fail;
	if (sp->cm_skt_num != tx_cm_skt)
		goto fail;
	if (sp->cm_mp != tx_cm_mp)
		goto fail;
	if (!sp->got_hello)
		goto fail;
	snprintf(exp_name, 15, SPEER_THRD_NM_FMT, tx_ct);
	pthread_getname_np(sp->s_rx, sp_name, 16);
	if (strncmp(exp_name, sp_name, 16))
		goto fail;

	/* Send another HELLO request, check for error parms */
	fail_pt = 50;
 
	tx_ct /=7;
	tx_cm_skt /= 5;
	tx_cm_mp /= 13;

        test->req.msg_type = htonl(RSKTD_HELLO_REQ);
        test->req.msg_seq = htonl(0x12345678);
        test->req.msg.hello.ct = htonl(tx_ct);
        test->req.msg.hello.cm_skt = htonl(tx_cm_skt);
        test->req.msg.hello.cm_mp = htonl(tx_cm_mp);

	rc = riomp_sock_send(sock_h, (void *)&test->req, DMN_REQ_SZ);
	if (rc)
		goto fail;

	/* And receive the corresponding response ... */
	fail_pt = 60;
	memset(&dresp, 0, DMN_RESP_SZ);
	rc = riomp_sock_receive(sock_h, (void **)&dresp_p, DMN_RESP_SZ, 0);
	if (rc)
		goto fail;

	if (dresp.msg_type  != htonl(RSKTD_HELLO_RESP))
		goto fail;
	if (dresp.msg_seq  != htonl(0x12345678))
		goto fail;
	if (dresp.err)
		goto fail;

	if (dresp.req.hello.ct != htonl(tx_ct))
		goto fail;
	if (dresp.req.hello.cm_skt != htonl(tx_cm_skt))
		goto fail;
	if (dresp.req.hello.cm_mp != htonl(tx_cm_mp))
		goto fail;

	if (dresp.msg.hello.peer_pid != htonl(getpid()))
		goto fail;

	/* Check that speer data reflects HELLO contents */
	fail_pt = 70;
	if (sp->ct != tx_ct)
		goto fail;
	if (sp->cm_skt_num != tx_cm_skt)
		goto fail;
	if (sp->cm_mp != tx_cm_mp)
		goto fail;
	if (!sp->got_hello)
		goto fail;

	snprintf(exp_name, 15, SPEER_THRD_NM_FMT, tx_ct);
	pthread_getname_np(sp->s_rx, sp_name, 16);
	if (strncmp(exp_name, sp_name, 16))
		goto fail;

	kill_worker_thread(&wkr[idx]);
	sp->i_must_die = 1;
	close_speer(sp);
	sleep(1);
	if (dmn.speers[0].alive)
		goto fail; 
	
	return 0;
fail:
	return fail_pt;
};

/* @brief Get SPEER and WPEER threads running...
 */

int test_case_7(void)
{
	int fail_pt = 5;
	struct rskt_dmn_speer **sp;
	struct rskt_dmn_wpeer **wp;
	struct rskt_test_info *s_t, *w_t;
	uint32_t sp_idx = 1;
	uint32_t wp_idx = (MAX_WORKERS/2);
	uint32_t speer_seq_no = 44;
	uint32_t wpeer_seq_no = 1;
	uint32_t speer_sn = 0x3939;
	uint32_t speer_cm_skt_num = 3333;
	int rc = 0;
	rskt_h *l_skts = NULL;
	struct rskt_sockaddr sa;
	int sn;
	int num_skts = 1;
	rskt_h acc_skt;
	struct rskt_sockaddr acc_sa;
	int skt_idx = 0;
	
	/* Start two workers. */
	start_worker_thread(&wkr[sp_idx], -1);
	start_worker_thread(&wkr[wp_idx], -1);

	wait_for_worker_status(&wkr[sp_idx], worker_halted);
	wait_for_worker_status(&wkr[wp_idx], worker_halted);
	
	s_t = (struct rskt_test_info *)wkr[sp_idx].priv_info;
	w_t = (struct rskt_test_info *)wkr[wp_idx].priv_info;

	/* Set up HELLO req/resp for SPEER, this should go immediately */
	fail_pt = 5;
	s_t->new_req = 0;

        s_t->req.msg_type = htonl(RSKTD_HELLO_REQ);
        s_t->req.msg_seq = htonl(speer_seq_no);
        s_t->req.msg.hello.ct = htonl(wp_idx); /* Yes, wp_idx */
        s_t->req.msg.hello.cm_skt = htonl(speer_cm_skt_num);
        s_t->req.msg.hello.cm_mp = htonl(1);

	s_t->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	s_t->resp.msg_seq = htonl(speer_seq_no);
	s_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
	s_t->resp.msg.hello.peer_pid = htonl(getpid());

	s_t->new_req = 1;

	/* Set up HELLO req/resp for WPEER */
	while (w_t->new_resp)
		sleep(0);

        w_t->req.msg_type = htonl(RSKTD_HELLO_REQ);
        w_t->req.msg_seq = htonl(0);
        w_t->req.msg.hello.ct = htonl(wp_idx);
        w_t->req.msg.hello.cm_skt = htonl(3334);
        w_t->req.msg.hello.cm_mp = htonl(1);

	w_t->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	w_t->resp.msg_seq = htonl(0);
	w_t->resp.err = htonl(0);
	memcpy(&w_t->resp.req, &w_t->req.msg, sizeof(union librsktd_req));
	w_t->resp.msg.hello.peer_pid = htonl(getpid());
	
	w_t->new_resp = 1;

	/* Speer should connect/accept immedaitesly with SPEER_CONN handler */
	/* WPEER waits until the daemon WPEER is connected */
	run_worker_action(&wkr[sp_idx], SPEER_CONN);
	run_worker_action(&wkr[wp_idx], WPEER_ACC);
	
	/* Start up WPEER process, should connect with worker thread*/
	fail_pt = 10;
	
	destids[0].valid = 1;
	destids[0].destid = wp_idx;
	destids[0].flag = FMDD_RSKT_FLAG;
	sem_post(&fmdd_sem);

	INFO("Kicked fmdd_sem to start WPEER");

	sleep(1);
	/* Not a typo, dmn.speers enqueues **speer. */
	sp = dmn.speers[0].self_ref;

	if (NULL == sp)
		goto fail;

	fail_pt = 11;
	wp = dmn.wpeers[0].self_ref;

	if (NULL == wp)
		goto fail;

	/* Get Library to bind/connect/accept to a socket.
	* Then have speer send in connect request, confirm success.
	*/

/*
	fail_pt = 20;
	dmn.num_ms = 0x10;
	dmn.ms_size = 64*1024;
	if (alloc_mso_msh())
		goto fail;
*/

	fail_pt = 25;
	acc_skt = rskt_create_socket();
	l_skts = (rskt_h *)malloc( sizeof(rskt_h)*(num_skts));

	sn = 0x1234;

	fail_pt = 30;

	l_skts[skt_idx] = rskt_create_socket();
	if (NULL == l_skts[skt_idx])
		goto fail;

	fail_pt = 30;
	sa.sn = sn;
	sa.ct = wp_idx;
	rc = rskt_bind(l_skts[skt_idx], &sa);
	if (rc)
		goto fail;

	fail_pt = 35;
	rc = rskt_listen(l_skts[skt_idx], 50);
	if (rc)
		goto fail;

	fail_pt = 40;

	/* Send in connect request from SPEER */
	while (s_t->new_req)
		sleep(0);

	speer_seq_no++;
        s_t->req.msg_type = htonl(RSKTD_CONNECT_REQ);
        s_t->req.msg_seq = htonl(speer_seq_no);
        s_t->req.msg.con.dst_sn = htonl(sn); /* Yes, wp_idx */
        s_t->req.msg.con.dst_ct = htonl(wp_idx);
        s_t->req.msg.con.src_sn = htonl(speer_sn);
        strncpy(s_t->req.msg.con.src_mso, "SRC_MSO", MAX_MS_NAME);
        strncpy(s_t->req.msg.con.src_ms, "SRC_MS", MAX_MS_NAME);
        s_t->req.msg.con.src_msub_o = 0;
        s_t->req.msg.con.src_msub_s = 64*1024;

	s_t->resp.msg_type = htonl(RSKTD_CONNECT_RESP);
	s_t->resp.msg_seq = htonl(speer_seq_no);
	s_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        s_t->resp.msg.con.acc_sn = htonl(RSKTD_DYNAMIC_SKT);
        s_t->resp.msg.con.dst_sn = htonl(sn);
        s_t->resp.msg.con.dst_ct = htonl(6);
        s_t->resp.msg.con.dst_dmn_cm_skt = htonl(speer_cm_skt_num);
	snprintf(s_t->resp.msg.con.dst_ms, MAX_MS_NAME, 
		"RSKT_DAEMON%05d.%03d", getpid(), 0);
        s_t->resp.msg.con.msub_sz = htonl(64*1024);

	s_t->new_req = 1;

	fail_pt = 45;
	rc = rskt_accept(l_skts[skt_idx], acc_skt, &acc_sa);
	if (rc)
		goto fail;
	
	fail_pt = 50;
	if (wait_for_worker_status(&wkr[sp_idx], worker_running))
		goto fail;

	fail_pt = 51;
	if (wait_for_worker_status(&wkr[wp_idx], worker_running))
		goto fail;

	/* Send in close request from LIBRARY */
	while (w_t->new_resp)
		sleep(0);

	memset(&w_t->req, 0, sizeof(w_t->req));
	memset(&w_t->resp, 0, sizeof(w_t->resp));
        w_t->req.msg_type = htonl(RSKTD_CLOSE_REQ);
        w_t->req.msg_seq = htonl(wpeer_seq_no);
        w_t->req.msg.clos.rem_sn = htonl(speer_sn);
        w_t->req.msg.clos.loc_sn = htonl(acc_sa.sn);
        w_t->req.msg.clos.force = htonl(1);

	w_t->resp.msg_type = htonl(RSKTD_CLOSE_RESP);
	w_t->resp.msg_seq = htonl(1);
	w_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        w_t->resp.msg.clos.status = htonl(0);
	w_t->speer_resp_err = 0;

	w_t->new_resp = 1;

	fail_pt = 60;
	rc = rskt_close(acc_skt);
	if (rc)
		goto fail;

	/* Send in connect request from SPEER */
	while (s_t->new_req && (wkr[sp_idx].stat == worker_running))
		sleep(0);

	fail_pt = 70;
	if (!(wkr[sp_idx].stat == worker_running))
		goto fail;

	fail_pt = 71;
	speer_seq_no++;
        s_t->req.msg_type = htonl(RSKTD_CONNECT_REQ);
        s_t->req.msg_seq = htonl(speer_seq_no);
        s_t->req.msg.con.dst_sn = htonl(sn); /* Yes, wp_idx */
        s_t->req.msg.con.dst_ct = htonl(wp_idx);
        s_t->req.msg.con.src_sn = htonl(speer_sn);
        strncpy(s_t->req.msg.con.src_mso, "SRC_MSO", MAX_MS_NAME);
        strncpy(s_t->req.msg.con.src_ms, "SRC_MS", MAX_MS_NAME);
        s_t->req.msg.con.src_msub_o = 0;
        s_t->req.msg.con.src_msub_s = 64*1024;

	s_t->resp.msg_type = htonl(RSKTD_CONNECT_RESP);
	s_t->resp.msg_seq = htonl(speer_seq_no);
	s_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        s_t->resp.msg.con.acc_sn = htonl(RSKTD_DYNAMIC_SKT);
        s_t->resp.msg.con.dst_sn = htonl(sn);
        s_t->resp.msg.con.dst_ct = htonl(6);
        s_t->resp.msg.con.dst_dmn_cm_skt = htonl(speer_cm_skt_num);
	snprintf(s_t->resp.msg.con.dst_ms, MAX_MS_NAME, 
		"RSKT_DAEMON%05d.%03d", getpid(), 0);
        s_t->resp.msg.con.msub_sz = htonl(64*1024);

	s_t->new_req = 1;

	fail_pt = 80;
	rc = rskt_accept(l_skts[skt_idx], acc_skt, &acc_sa);
	if (rc)
		goto fail;
	
	fail_pt = 81;
	if (wkr[sp_idx].stat != worker_running)
		goto fail;

	fail_pt = 82;
	if (wkr[wp_idx].stat != worker_running)
		goto fail;

	/* Send in close request from SPEER */
	fail_pt = 90;
	while (s_t->new_req && (wkr[sp_idx].stat == worker_running))
		sleep(0);
	if (wkr[sp_idx].stat != worker_running)
		goto fail;

	fail_pt = 91;

	memset(&s_t->req, 0, sizeof(w_t->req));
	memset(&s_t->resp, 0, sizeof(w_t->resp));
	speer_seq_no++;
        s_t->req.msg_type = htonl(RSKTD_CLOSE_REQ);
        s_t->req.msg_seq = htonl(speer_seq_no);
        s_t->req.msg.clos.rem_sn = htonl(acc_sa.sn);
        s_t->req.msg.clos.loc_sn = htonl(speer_sn);
        s_t->req.msg.clos.force = htonl(1);

	s_t->resp.msg_type = htonl(RSKTD_CLOSE_RESP);
	s_t->resp.msg_seq = htonl(speer_seq_no);
	s_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
        s_t->resp.msg.clos.status = htonl(rskt_closed);
	s_t->speer_resp_err = 0;

	s_t->new_req = 1;

	fail_pt = 100;
	while (s_t->new_req && (wkr[sp_idx].stat == worker_running))
		sleep(0);
	if (wkr[sp_idx].stat != worker_running)
		goto fail;
	if (wkr[wp_idx].stat != worker_running)
		goto fail;

	/* Kill WPEER process */
	fail_pt = 110;
	
	destids[0].valid = 0;
	destids[0].destid = 0;
	destids[0].flag = 0;
	sem_post(&fmdd_sem);

	INFO("Kicked fmdd_sem to kill WPEER and SPEER");

	/* Note: kill_all_worker_threads sets kill_acc_conn, which will cause 
	* the riomp_sock_accept call in the RSKTD SPEER_CONN thread to fail,
	* which will in turn cause the daemon to kill all threads and exit.
	*/
	kill_all_worker_threads();

	sleep(1);
	free(l_skts);
	return 0;
fail:
	if (NULL != l_skts)
		free(l_skts);
	return fail_pt;
};

int test_case_8(void)
{

	int i, d_i, sp_idx, wp_idx;
	int num_drivers = 3;
	struct rskt_test_info *d_t, *w_t;
	int st_skt = RSKTD_DYNAMIC_SKT - BIG_TEST_MAX_SN - 1;
	int st_acc_sn = 1234;
	int skts_per_driver = BIG_TEST_MAX_SN / num_drivers;
	int ret;
	int found_one = 0;

/*
	dmn.num_ms = 0x10;
	dmn.ms_size = 64*1024;
	if (alloc_mso_msh())
		return 0x1000;
*/

	for (i = 0; i < num_drivers; i++) {
		/* Start worker driver thread */
		sp_idx = i;
		wp_idx = (MAX_WORKERS/2) + i;
		d_i = MAX_WORKER_IDX - i;

		start_worker_thread(&wkr[d_i], -1);
		start_worker_thread(&wkr[sp_idx], -1);
		start_worker_thread(&wkr[wp_idx], -1);

		wait_for_worker_status(&wkr[d_i], worker_halted);
		wait_for_worker_status(&wkr[sp_idx], worker_halted);
		wait_for_worker_status(&wkr[wp_idx], worker_halted);

		d_t = (struct rskt_test_info *)wkr[d_i].priv_info;

		d_t->start_sn = st_skt + (i * skts_per_driver);
		d_t->end_sn = st_skt + ((i + 1) * skts_per_driver) - 1;
		d_t->max_iter = 10;
		d_t->sp_idx = sp_idx;
		d_t->wp_idx = wp_idx;
		d_t->speer_acc_sn = st_acc_sn + (i * skts_per_driver);
		d_t->speer_seq_no = 99 * (i + 1);
		d_t->speer_sn = 3000 + (100 * (i + 1));
		d_t->speer_cm_skt_num = 4000 + (100 * (i + 1));
		d_t->wpeer_cm_skt = 5000 + (100 * (i + 1));

		sem_init(&d_t->speer_wpeer_init_complete, 0, 0);
		sem_init(&d_t->all_workers_ready, 0, 0);

		run_worker_action(&wkr[d_i], SPEER_WPEER_DRIVER);
		wait_for_worker_status(&wkr[d_i], worker_running);
		sem_wait(&d_t->speer_wpeer_init_complete);

		run_worker_action(&wkr[d_t->sp_idx], SPEER_CONN);
		wait_for_worker_status(&wkr[sp_idx], worker_running);

		run_worker_action(&wkr[d_t->wp_idx], WPEER_ACC);
		wait_for_worker_status(&wkr[wp_idx], worker_running);

		destids[i].valid = 1;
		destids[i].destid = d_t->wp_idx;
		destids[i].flag = FMDD_RSKT_FLAG;
		sem_post(&fmdd_sem);

		w_t = (struct rskt_test_info *)wkr[wp_idx].priv_info;
		while (w_t->new_resp || wait_for_worker_status(&wkr[wp_idx],
							worker_running)
				|| !dmn.wpeers[i].peer_pid)
			sleep(0);

		if (wait_for_worker_status(&wkr[wp_idx], worker_running)) {
			CRIT("Could not start WPEER %d", wp_idx);
			ret = 1;
			goto fail;
		};
		if (!dmn.wpeers[i].peer_pid) {
			CRIT("Could not start WPEER %d", wp_idx);
			ret = 1;
			goto fail;
		};
	};

	for (i = 0; i < num_drivers; i++) {
		d_i = MAX_WORKER_IDX - i;
		d_t = (struct rskt_test_info *)wkr[d_i].priv_info;
		sem_post(&d_t->all_workers_ready);
	};

	found_one = 1;
	while (found_one) {
		found_one = 0;
		for (i = 0; i < num_drivers; i++) {
			d_i = MAX_WORKER_IDX - i;
			if (wait_for_worker_status(&wkr[d_i], worker_halted))
				found_one = 1;
		};
	};

	ret = 0; 

	for (i = 0; i < num_drivers; i++) {
		d_i = MAX_WORKER_IDX - i;
		d_t = (struct rskt_test_info *)wkr[d_i].priv_info;

		if (d_t->rc) {
			CRIT("Worker %d FAILED, rc: 0d%d 0x%x", 
				d_i, d_t->rc, d_t->rc);
			ret = 1;
		} else {
			HIGH("Worker %d Passed, rc: 0d%d 0x%x", 
				d_i, d_t->rc, d_t->rc);
		};
	};
fail:
	return ret;
};

void kill_all_worker_threads(void) {
	int i;
	
        kill_acc_conn = 1;
	for (i = 0; i < MAX_WORKERS; i++) {
		kill_worker_thread(&wkr[i]);
	};
        kill_acc_conn = 0;
};

int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;
	int ret;

	if (0)
		argv[0][0] = argc;

	g_level = 2;

	assert(DMN_RESP_SZ >= sizeof(struct rsktd_resp_msg));
	assert(DMN_REQ_SZ >= sizeof(struct rsktd_req_msg));

	rdma_log_init("rsktd_test_log.txt", 1);

	/* Configure fake MPORT info before doing any initialization */
	dmn.qresp.hdid = FAKE_LIBMPORT_CT;

	if (start_fm_thread()) {
		ERR("Could not start message procssor thread. EXITING");
		goto fail;
	};

	if (start_wpeer_handler()) {
		CRIT("Could not start WPEER connection manager. EXITING");
		goto fail;
	} else {
		DBG("Started wpeer handler.");
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

	CRIT("Started SPEER connection manager.");
	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM)) {
		ERR("Could not start rskt library. EXITING");
		goto fail;
	};

	config_unit_test(&stub_utd);

	cli_init_base(cleanup_proc);
	bind_unit_test_thread_cli_cmds();
	librsktd_bind_cli_cmds();
	liblog_bind_cli_cmds();

	ret = test_case_0();
	if (ret) {
		CRIT("Test case 0 FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 0 Passed\n");

	ret = test_case_1();
	if (ret) {
		CRIT("Test case 1 FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 1 Passed\n");

	ret = test_case_2();
	if (ret) {
		CRIT("Test case 2 FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 2 Passed\n");

	ret = test_case_2A();
	if (ret) {
		CRIT("Test case 2A FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 2A Passed\n");

	ret = test_case_3();
	if (ret) {
		CRIT("Test case 3 FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 3 Passed\n");

	ret = test_case_3A();
	if (ret) {
		CRIT("Test case 3A FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 3A Passed\n");

	kill_all_worker_threads();
	ret = test_case_4();
	if (ret) {
		CRIT("Test case 4 FAILED 0x%x 0d%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 4 Passed\n");

	kill_all_worker_threads();
	ret = test_case_4A();
	if (ret) {
		CRIT("Test case 4A FAILED 0x%x 0x%d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 4A Passed\n");

	kill_all_worker_threads();
        kill_acc_conn = 0;
	ret = test_case_5();
	if (ret) {
		CRIT("Test case 5 FAILED 0x%x %d\n", ret, ret);
		goto fail;
	};
	CRIT("Test case 5 Passed\n");

	// Start SPEER connection handler
	kill_all_worker_threads();
	
	if (start_speer_handler(DFLT_LIBRSKTD_TEST_CM_SKT,
			DFLT_LIBRSKTD_TEST_MPNUM, TEST_NUM_MS, TEST_MS_SZ,
			TEST_SKIP_MS)) {
		CRIT("Could not start speer connection manager. EXITING");
		goto fail;
	};
	CRIT("Started SPEER connection manager.");

	ret =  test_case_6();
	if (ret) {
		CRIT("Test case 6 FAILED rc 0x%x %d \n", ret, ret);
		goto fail;
	};
	CRIT("Test case 6 Passed\n");

	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM)) {
		CRIT("Could not start rskt library. EXITING");
		goto fail;
	} else {
		CRIT("Restarted librskt");
	};

	// Start up WPEER and SPEER, verify that its working
	rc = test_case_7();
	if (rc) {
		CRIT("Test case 7 FAILED rc 0x%x %d \n", rc, rc);
		goto fail;
	};
	CRIT("Test case 7 Passed\n");

	// Since some daemon threads are killed by test_case_7, start up all
	// threads again.
	// Cleanup remaining threads, and details in fake_libmport
	cleanup_proc(NULL);
	sock_wkr_idx = 0;
	kill_acc_conn = 0;

	// Then start up everything again.
	dmn.all_must_die = 0;
	if (start_fm_thread()) {
		ERR("Could not start message procssor thread. EXITING");
		goto fail;
	};

	if (start_wpeer_handler()) {
		CRIT("Could not start WPEER connection manager. EXITING");
		goto fail;
	} else {
		DBG("Started wpeer handler.");
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

	if (start_speer_handler(DFLT_LIBRSKTD_TEST_CM_SKT,
			DFLT_LIBRSKTD_TEST_MPNUM, TEST_NUM_MS, TEST_MS_SZ,
			TEST_SKIP_MS)) {
		CRIT("Could not start speer connection manager. EXITING");
		goto fail;
	};

	/* Run multiple WPEER/SPEER in parallel */

	ret = test_case_8();
	if (ret) {
		CRIT("Test case 8 FAILED rc 0x%x %d \n", ret, ret);
	} else {
		CRIT("Test case 8 Passed\n");
	};

        splashScreen((char *)"RSKT UNIT TEST");
	console((void *)"RSKT_TEST > ");

	cleanup_proc(NULL);
	HIGH("Cleanup completed\n");
fail:
	CRIT("Test failed, waiting 100 seconds");
	sleep(100);
	exit(rc);
	return 0;
}

#ifdef __cplusplus
}
#endif

