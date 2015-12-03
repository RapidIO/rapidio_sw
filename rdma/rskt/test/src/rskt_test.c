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

#ifdef __cplusplus
extern "C" {
#endif

void cleanup_proc(struct cli_env *env)
{
	int i;

	if (NULL == env)
		i = 0;

	halt_lib_handler();
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

int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;
	int librskt_port = 0x1234;
	int librskt_mpnum = 0;

	if (0)
		argv[0][0] = argc;

	rdma_log_init("rsktd_test_log.txt", 1);

	g_level = 7;

	sem_init(&dmn.app_tx_mutex, 0, 1);
	sem_init(&dmn.app_tx_cnt, 0, 0);
	l_init(&dmn.app_tx_q);
	if (pthread_create(&dmn.app_tx_thread, NULL, app_tx_loop, NULL)) {
		ERR("Could not start app_tx_loop thread. EXITING");
		goto exit;
	};

	if (start_msg_proc_q_thread()) {
		ERR("Could not start message procssor thread. EXITING");
		goto exit;
	};
	if (start_lib_handler(librskt_port, librskt_mpnum, 0, 0)) {
		ERR("Could not start lib handler. EXITING");
		goto exit;
	};

	if (librskt_init(librskt_port, librskt_mpnum)) {
		ERR("Could not start rskt library. EXITING");
		goto exit;
	};

	config_unit_test(&stub_utd);

	cli_init_base(cleanup_proc);
	bind_unit_test_thread_cli_cmds();
	librsktd_bind_cli_cmds();
        splashScreen((char *)"RSKT UNIT TEST");
	console((void *)"RSKT_TEST > ");
	
	cleanup_proc(NULL);
	
exit:
	exit(rc);
}

#ifdef __cplusplus
}
#endif

