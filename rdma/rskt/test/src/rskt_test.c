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
#include "fake_libmport.h"

#define DFLT_LIBRSKTD_TEST_PORT 1234
#define DFLT_LIBRSKTD_TEST_MPNUM 0
#define DFLT_LIBRSKTD_TEST_BKLG 35
#define DFLT_LIBRSKTD_TEST_TEST 0

#ifdef __cplusplus
extern "C" {
#endif

void cleanup_proc(struct cli_env *env)
{
	int i;

	if (NULL == env)
		i = 0;

	halt_msg_proc_q_thread();
	halt_lib_handler();
	librskt_finish();
	for (i = 0; i < MAX_WORKERS; i++)
		kill_worker_thread(&wkr[i]);
};

char *unknown = (char *)"Unknonwn";

char *action_str(int action)
{
	return action?unknown:unknown;
};

int ts_sel(char *parm)
{
	return parm[0]*0;
};

int worker_body(struct worker *info)
{
	return info->idx * 0;
};
	
void create_priv(struct worker *info)
{
	info->priv_info = NULL;
};

void destroy_priv(struct worker *info)
{
	info->priv_info = NULL;
};

struct unit_test_driver stub_utd = {
        action_str,
        ts_sel,
        worker_body,
        create_priv,
        destroy_priv
};

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

int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;
/*
	struct sigaction sigchld_action;

        memset(&sigchld_action, 0, sizeof(sigchld_action));
	sigchld_action.sa_handler = SIG_DFL;
        sigchld_action.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sigchld_action, NULL);
*/

	if (0)
		argv[0][0] = argc;

	rdma_log_init("rsktd_test_log.txt", 1);

	/* Configure fake MPORT info before doing any initialization */
	dmn.qresp.hdid = FAKE_LIBMPORT_CT;

	g_level = 7;

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

	if (test_case_1()) {
		ERR("Test case 1 FAILED\n");
		goto fail;
	};
	HIGH("Test case 1 Passed\n");

        splashScreen((char *)"RSKT UNIT TEST");
	console((void *)"RSKT_TEST > ");
	
	cleanup_proc(NULL);
	HIGH("Cleanup completed\n");
fail:
	exit(rc);
	return 0;
}

#ifdef __cplusplus
}
#endif

