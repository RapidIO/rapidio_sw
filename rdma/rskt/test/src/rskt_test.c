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

int test_speer_connect(struct worker *info);
int test_wpeer_accept(struct worker *info);

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

	default: CRIT("Unknown worker action...");
		rc = 1;
	};
	return rc;
};
	
void create_priv(struct worker *info)
{
	struct rskt_test_info *test;
	
	test = (struct rskt_test_info *)malloc(sizeof(struct rskt_test_info));

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
		//free((void *)info->priv_info);
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

	if (l_size(&lib.msg_tx)) {
		CRIT("lib.msgtx");
		goto fail;
	};
	if (l_size(&lib.rsvp)) {
		CRIT("lib.rsvp");
		goto fail;
	};
	if (l_size(&lib.req)) {
		CRIT("lib.req");
		goto fail;
	};
	if (l_size(&lib_st.tx_msg_q)) {
		CRIT("lib.rtx_msg_q");
		goto fail;
	};
	if (l_size(&lib_st.acc)) {
		CRIT("lib_st.acc");
		goto fail;
	};
	if (l_size(&lib_st.con)) {
		CRIT("lib_st.con");
		goto fail;
	};
	if (l_size(&lib_st.creq)) {
		CRIT("lib_st.creq");
		goto fail;
	};
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

int test_speer_connect(struct worker *info)
{
	int fail_pt = 5;
	int ret;
	riomp_mailbox_t mbox;
	struct rskt_test_info *test = (struct rskt_test_info *)info->priv_info;
        struct rsktd_resp_msg *rsp_p = &test->resp;
	riomp_sock_t sock_h;

	/* There should not be any peers now */
	if (riomp_sock_mbox_create_handle(0, 0, &mbox))
		goto fail;

	if (riomp_sock_socket(mbox, &sock_h))
		goto fail;

	if (riomp_sock_connect(sock_h, info->idx, 0, 1))
		goto fail;

	sleep(1);

	/* Now its time to send the HELLO request */
	fail_pt = 10;
 
	while (info->stop_req == worker_running) {
		struct rsktd_resp_msg *resp =
			(struct rsktd_resp_msg *)malloc(DMN_RESP_SZ);

		INFO("SPEER %d waiting new request to TX", info->idx);
		while ((info->stop_req == worker_running) && !test->new_req)
			sched_yield();
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
		fail_pt = 40;
		if (memcmp(&test->resp, resp, sizeof(struct rsktd_resp_msg)))
			goto fail;
		test->new_req = 0;
	};
	INFO("SPEER %d exiting...", info->idx);
	test->rc = 0;
	return 0;
fail:
	INFO("SPEER %d exiting rc %d...", info->idx, fail_pt);
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

	if (riomp_sock_accept(sock_h, &new_h, 0))
		goto fail;

	sleep(1);
	
	while (info->stop_req == worker_running) {
		int ret;
		fail_pt = 20;
		INFO("WKR %d WPEER_ACC Waiting for new resp mesage",
			info->idx);
		while ((info->stop_req == worker_running) && !test->new_resp)
			sched_yield();

		fail_pt = 25;
		INFO("WKR %d WPEER_ACC Receiving message", info->idx);
		ret = riomp_sock_receive(new_h, (void **)&req, DMN_REQ_SZ, 0);
		INFO("WKR %d WPEER_ACC REceive ret %d", info->idx, ret);
		if (ret)
			goto fail;
		if (NULL == req)
			goto fail;
		memcpy(&test->resp.req, req, sizeof(union librsktd_req));

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

#ifdef NOT_DEFINED
	/* Code to compose hello request and get a response */
	/* Now to receive the HELLO request ... */
	fail_pt = 30;
	memset(&info->req, 0, DMN_REQ_SZ);
	if (riomp_sock_receive(sock_h, (void **)&rq_p, DMN_REQ_SZ, 0))
		goto fail;

	memcpy(info->req, req, DMN_REQ_SZ);

	if (test->req.msg_type  != htonl(RSKTD_HELLO_REQ))
		goto fail;

	if (test->msg.hello.ct != htonl(info->idx))
		goto fail;
	if (test->req.hello.cm_skt != htonl(0))
		goto fail;
	if (test->req.hello.cm_mp != htonl(0))
		goto fail;

	memcpy(&test->resp.req, &test->req.msg, sizeof(union librsktd_req));
	test->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	test->resp.msg_seq = test->req.msg_seq;
	test->resp.err = test->speer_resp_err;
	test->resp.msg.hello.peer_pid = htonl(getpid());

	/* And send the HELLO response */
	fail_pt = 10;
 
	memcpy(&test->resp.req, &test->req.msg, sizeof(union librsktd_req));
	test->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	test->resp.msg_seq = test->req.msg_seq;
	test->resp.err = test->speer_resp_err;
	test->resp.msg.hello.peer_pid = htonl(getpid());

	if (riomp_sock_send(sock_h, &test->resp, DMN_RESP_SZ))
		goto fail;
#endif
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
	struct l_item_t *li;
	uint32_t tx_ct = TX_CT;
	uint32_t tx_cm_skt = TX_CM_SKT;
	uint32_t tx_cm_mp = TX_CM_MP;
	char sp_name[16];
	char exp_name[16];
	struct rskt_test_info *test;

	/* There should not be any peers now */
	if (l_size(&dmn.speers))
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

	if (1 != l_size(&dmn.speers))
		goto fail; 
	
	sp = *(struct rskt_dmn_speer **)l_head(&dmn.speers, &li);

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
	if (l_size(&dmn.speers))
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
	struct l_item_t *li;
	struct rskt_test_info *s_t, *w_t;
	uint32_t sp_idx = 1;
	uint32_t wp_idx = (MAX_WORKERS/2);
	
	g_level = 7;
	/* Start two workers. */
	start_worker_thread(&wkr[sp_idx], -1);
	start_worker_thread(&wkr[wp_idx], -1);

	wait_for_worker_status(&wkr[sp_idx], worker_halted);
	wait_for_worker_status(&wkr[wp_idx], worker_halted);
	
	s_t = (struct rskt_test_info *)wkr[sp_idx].priv_info;
	w_t = (struct rskt_test_info *)wkr[wp_idx].priv_info;

	/* Set up HELLO req/resp for SPEER, this should go immediately */
        s_t->req.msg_type = htonl(RSKTD_HELLO_REQ);
        s_t->req.msg_seq = htonl(44);
        s_t->req.msg.hello.ct = htonl(wp_idx); /* Yes, wp_idx */
        s_t->req.msg.hello.cm_skt = htonl(3333);
        s_t->req.msg.hello.cm_mp = htonl(1);

	s_t->resp.msg_type = htonl(RSKTD_HELLO_RESP);
	s_t->resp.msg_seq = htonl(0);
	s_t->resp.err = htonl(0);
	memcpy(&s_t->resp.req, &s_t->req.msg, sizeof(union librsktd_req));
	s_t->resp.msg.hello.peer_pid = htonl(getpid());

	s_t->new_req = 1;

	/* Set up HELLO req/resp for WPEER */
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

	sleep(1);
	/* Not a typo, dmn.speers enqueues **speer. */
	sp = (struct rskt_dmn_speer **)l_head(&dmn.speers, &li);

	if (NULL == sp)
		goto fail;

	wp = (struct rskt_dmn_wpeer **)l_head(&dmn.wpeers, &li);

	if (NULL == wp)
		goto fail;

	return 0;
fail:
	return fail_pt;
};

#ifdef NOT_DEFINED
	/* Now its time to send a SPEER CONNECT request */
	fail_pt = 10;
 
        test->req.msg_type = htonl(RSKTD_CONNECT_REQ);
        test->req.msg_seq = htonl(tst_msg_seq);
        test->req.msg.con.dst_sn = htonl(tst_sn);
        test->req.msg.con.dst_ct = htonl(tst_ct);
        test->req.msg.con.src_sn = htonl(tst_con_sn);
        strncpy(test->req.msg.con.src_mso, tst_src_mso_name, MAX_MS_NAME);
        strncpy(test->req.msg.con.src_ms , tst_src_ms_name, MAX_MS_NAME);
        test->req.msg.con.src_msub_o = htonl(tst_csrc_msub_o);
        test->req.msg.con.src_msub_s = htonl(tst_csrc_msub_s);

	if (riomp_sock_send(sock_h, &test->req, DMN_REQ_SZ))
		goto fail;
	
	/* And send an accept request from the application */
	rskt_hnd = rskt_create_socket();
	if (NULL == rskt_hnd)
		goto fail;

	sa.ct = tst_ct;
	sa.sn = tst_sn;

	rc = rskt_bind(rskt_hnd, &sa);
	if (rc)
		goto fail;

	rc = rskt_listen(rskt_hnd, 50);
	if (rc)
		goto fail;

	/* This should return successfully with the connection. */
	rc = rskt_accept(rskt_hnd, acc_skt, &acc_sa);
	if (rc)
		goto fail;
	
	/* Receive the connect response ... */
	fail_pt = 30;
	
	memset(dresp, 0, DMN_RESP_SZ);
	if (riomp_sock_receive(sock_h, (void **)&dresp, DMN_RESP_SZ, 0))
		goto fail;

	if (dresp->msg_type  != htonl(RSKTD__RESP))
		goto fail;
	if (dresp->msg_seq != htonl(exp_sez))
		goto fail;
	if (dresp->err)
		goto fail;

	/* Check that response contains correct request */
        if (dresp->msg_type != htonl(RSKTD_CONNECT_REQ);
		goto fail;
        if (dresp->msg_seq != htonl(tst_msg_seq))
		goto fail;
        if (dresp->err)
		goto fail;
        if (dresp->req.msg.con.dst_sn != htonl(tst_sn))
		goto fail;
        if (dresp->req.msg.con.dst_ct != htonl(tst_ct))
		goto fail;
        if (dresp->req.msg.con.src_sn != htonl(tst_con_sn))
		goto fail;
        if (strncmp(dresp->req.msg.con.src_mso, tst_src_mso_name, MAX_MS_NAME))
		goto fail;
        if (strncmp(dresp->req.msg.con.src_ms , tst_src_ms_name, MAX_MS_NAME))
		goto fail;
        if (dresp->req.msg.con.src_msub_o != htonl(tst_csrc_msub_o))
		goto fail;
        if (dresp->req.msg.con.src_msub_s != htonl(tst_csrc_msub_s))
		goto fail;

	/* Check that response contains expected data */
	if (dresp->msg.con.acc_sn != htonl(4096))
		goto fail;
	if (dresp->msg.con.dst_sn != htonl(tst_sn))
		goto fail;
	if (dresp->msg.con.dst_ct != htonl(tst_ct))
		goto fail;
	if (dresp->msg.con.dst_dmn_cm_skt != htonl(TX_CM_SKT))
		goto fail;
        if (strncmp(dresp->msg.con.dst_ms, tst_dst_ms_name, MAX_MS_NAME))
		goto fail;
	if (dresp->msg.con.msub_sz != htonl(0x20000))
		goto fail;

	/* Check that RSKTD socket data reflects CONNECT/ACCEPT contents */
	if (skts[tst_sn] != rskt_connected)
		goto fail;
	if (1 != l_size(&lib_st.con))
		goto fail;

	con_skt = l_head(&lib_st.con);
	if (con_skt->loc_sn != tst_loc_sn)
		goto fail;
	if (con_skt->w != NULL)
		goto fail;
	if (NULL == con_skt->loc_ms)
		goto fail;
	if (con_skt->rem_ct != tst_rem_ct)
		goto fail;
	if (con_skt->rem_sn != tst_rem_sn)
		goto fail;

	/* Check that library socket data reflects CONNECT/ACCEPT contents */
	if (lib.all_must_die)
		goto fail;
	if (!l_size(&lib.msg_tx))
		goto fail;
	if (!l_size(&lib.rsvp))
		goto fail;
	if (!l_size(&lib.req))
		goto fail;
	if (2 != l_size(&lib.skts))
		goto fail;

	lib_skt = *(rskt_socket_t **)l_head(&lib.skts, &l_i);
	lib_skt = *(rskt_socket_t **)l_next(&lib.skts, &l_i);

	if (lib_skt->st != rskt_connected)
		goto fail;
	if (lib_skt->debug)
		goto fail;
	if (lib_skt->max_backlog != tst_bklog)
		goto fail;
	if ((lib_skt->sa.ct != tst_ct) || (lib_skt->sa.sn != tst_ct))
		goto fail;
	if ((lib_skt->sai.sa.ct != tst_ct) || (lib_skt->sai.sa.sn != tst_ct))
		goto fail;
	if (lib_skt->connector != skt_rdma_acceptor)
		goto fail;
        if (strncmp(lib_skt->msoh_name, "LOCAL_NAME", MAX_MS_NAME))
		goto fail;
	if (!lib_skt->msoh_valid)
		goto fail;
	if (!lib_skt->msoh != test_msoh)
		goto fail;
        if (strncmp(lib_skt->msh_name, "LOCAL_NAME", MAX_MS_NAME))
		goto fail;
	if (!lib_skt->msh_valid)
		goto fail;
	if (!lib_skt->msh != test_msh)
		goto fail;
	if (!lib_skt->msubh_valid)
		goto fail;
	if (!lib_skt->msubh != test_msubh)
		goto fail;
	if (!lib_skt->msub_sz != test_sz)
		goto fail;

        if (strncmp(lib_skt->con_msh_name, "LOCAL_NAME", MAX_MS_NAME))
		goto fail;
	if (!lib_skt->con_msh != test_con_msh)
		goto fail;
	if (!lib_skt->con_msubh != test_con_msubh)
		goto fail;
	if (!lib_skt->con_sz != test_sz)
		goto fail;
	if (lib_skt->stats.tx_bytes ||
			lib_skt->stats.rx_bytes ||
			lib_skt->stats.tx_trans ||
			lib_skt->stats.rx_trans)
		goto fail;

	/* Send CLOSE request, check for error parms */
	fail_pt = 50;
 
	dresp = (struct rsktd_resp_msg *)malloc(DMN_RESP_SZ);

        test->req.msg_type = htonl(RSKTD_CLOSE_REQ);
	tst_msg_seq++;
        test->req.msg_seq = htonl(tst_msg_seq);
        test->req.msg.close.rem_sn = htonl(tst_sn);
        test->req.msg.close.loc_sn = htonl(tst_con_sn);
        test->req.msg.close.force = htonl(1);

	rc = riomp_sock_send(sock_h, (void *)&test->req, DMN_REQ_SZ);
	if (rc)
		goto fail;

	/* And receive the corresponding response ... */
	fail_pt = 60;
	memset(dresp, 0, DMN_RESP_SZ);
	rc = riomp_sock_receive(sock_h, (void **)&dresp, DMN_RESP_SZ, 0);
	if (rc)
		goto fail;

	if (dresp->msg_type  != htonl(RSKTD_CLOSE_RESP))
		goto fail;
	if (dresp->msg_seq  != htonl(tst_msg_seq))
		goto fail;
	if (dresp->err)
		goto fail;

	if (dresp->req.close.status)
		goto fail;

	/* Check that speer data reflects HELLO contents */
#endif
	
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

	rdma_log_init("rsktd_test_log.txt", 1);

	/* Configure fake MPORT info before doing any initialization */
	dmn.qresp.hdid = FAKE_LIBMPORT_CT;

	g_level = 2;

	if (start_fm_thread()) {
		ERR("Could not start message procssor thread. EXITING");
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

/*
	if (test_case_1()) {
		CRIT("Test case 1 FAILED\n");
		goto fail;
	};
	CRIT("Test case 1 Passed\n");

	ret = test_case_2();
	if (ret) {
		CRIT("Test case 2 FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 2 Passed\n");

	ret = test_case_2A();
	if (ret) {
		CRIT("Test case 2A FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 2A Passed\n");

	ret = test_case_3();
	if (ret) {
		CRIT("Test case 3 FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 3 Passed\n");

	ret = test_case_3A();
	if (ret) {
		CRIT("Test case 3A FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 3A Passed\n");

	kill_all_worker_threads();
	ret = test_case_4();
	if (ret) {
		CRIT("Test case 4 FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 4 Passed\n");

	kill_all_worker_threads();
	ret = test_case_4A();
	if (ret) {
		CRIT("Test case 4A FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 4A Passed\n");

	kill_all_worker_threads();
        kill_acc_conn = 0;
	ret = test_case_5();
	if (ret) {
		CRIT("Test case 5 FAILED %d\n", ret);
		goto fail;
	};
	CRIT("Test case 5 Passed\n");
*/

	/* Start SPEER connection handler */
	kill_all_worker_threads();
	
	if (start_speer_handler(DFLT_LIBRSKTD_TEST_CM_SKT,
			DFLT_LIBRSKTD_TEST_MPNUM, TEST_ZERO_MS, TEST_ZERO_MS_SZ,
			TEST_SKIP_MS, NOT_TEST)) {
		CRIT("Could not start speer connection manager. EXITING");
		goto fail;
	};
	CRIT("Started SPEER connection manager.");

	ret =  test_case_6();
	if (ret) {
		CRIT("Test case 6 FAILED rc %d \n", ret);
		// goto fail;
	};
	CRIT("Test case 6 Passed\n");

	if (librskt_init(DFLT_LIBRSKTD_TEST_PORT, DFLT_LIBRSKTD_TEST_MPNUM)) {
		ERR("Could not start rskt library. EXITING");
		goto fail;
	};

	if (start_wpeer_handler()) {
		CRIT("Could not start WPEER connection manager. EXITING");
		goto fail;
	};

	/* Start up WPEER and SPEER, verify that its working */
	rc = test_case_7();
	if (rc) {
		CRIT("Test case 7 FAILED rc %d \n", rc);
		// goto fail;
	};
	CRIT("Test case 7 Passed\n");

        splashScreen((char *)"RSKT UNIT TEST");
	console((void *)"RSKT_TEST > ");
	

	g_level = 7;

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

