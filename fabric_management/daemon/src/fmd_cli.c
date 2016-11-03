/* Fabric Management Daemon Thread CLI Commands */
/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>


#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "fmd_dd.h"
#include "fmd_app_msg.h"
#include "liblog.h"
#include "cfg.h"
#include "fmd_state.h"
#include "fmd.h"
#include "fmd_app.h"
#include "fmd_app.h"
#include "fmd_master.h"
#include "fmd_slave.h"
// #include "cli_cmd_line.h"
// #include "cli_cmd_db.h"
// #include "cli_parse.h"
#include "libcli.h"

#ifdef __cplusplus
extern "C" {
#endif

void display_apps_dd(struct cli_env *env)
{
	int found_one = 0, i;
	for (i = 0; i < FMD_MAX_APPS; i++) {
		if (app_st.apps[i].alloced) {
			if (!found_one) {
				sprintf(env->output,
				"         Idx V Fd A D ProcNum- Name\n");
				logMsg(env);
			};
			found_one = 1;
			sprintf(env->output,
				"         %3d %1d %2d %1d %1d %8d %s\n",
				app_st.apps[i].index, 
				app_st.apps[i].alloced, 
				app_st.apps[i].app_fd, 
				app_st.apps[i].alive, 
				app_st.apps[i].i_must_die, 
				app_st.apps[i].proc_num, 
				app_st.apps[i].app_name); 
			logMsg(env);
		};
	};

	if (!found_one) {
		sprintf(env->output, "         No apps connected...\n");
		logMsg(env);
	};
}
extern struct cli_cmd CLIStatus;

int CLIStatusCmd(struct cli_env *env, int argc, char **argv)
{
	int app_cnt = 0, i;
	struct fmd_peer *peer;
	struct l_item_t *li;

	if (0)
		argv[0][0] = argc;

	for (i = 0; i < FMD_MAX_APPS; i++) {
		if (app_st.apps[i].alloced)
			app_cnt++;
	};
	sprintf(env->output,
		"AppMgmt Alive: %1d Exit: %1d  NumApps: %4d Skt: %5d\n",
		app_st.loop_alive, app_st.all_must_die, app_cnt, app_st.port);
	logMsg(env);
	sprintf(env->output, "\nThread   A D Conn\n");
	logMsg(env);
	display_apps_dd(env);

	if (!fmp.mode) {
		sprintf(env->output, "\nPeerMgmt Alive: %1d Exit: %1d SLAVE %5d %s\n", 
			fmp.slv.slave_alive, fmp.slv.slave_must_die,
			fmp.slv.mast_skt_num,
			fmp.slv.m_h_resp_valid?"OK":"No Hello Rsp");
		logMsg(env);
		goto exit;
	};

	sprintf(env->output,
		"\nPeerMgmt Alive %1d Exit %1d PeerCnt %4d MASTER %5d\n",
		fmp.acc.acc_alive, fmp.acc.acc_must_die, 
		l_size(&fmp.peers), fmp.acc.cm_skt_num);
	logMsg(env);

	if (!l_size(&fmp.peers)) {
		sprintf(env->output, "No connected peers.\n");
		logMsg(env);
		goto exit;
	};

	sprintf(env->output, "\n         ---CT--- ---DID-- HC A D I R\n");
	logMsg(env);

	peer = (struct fmd_peer *)l_head(&fmp.peers, &li);
	while (NULL != peer) {
		sprintf(env->output,
			"         %8x %8x %2x %1d %1d %1d %1d %s\n",
			peer->p_ct, peer->p_did, peer->p_hc, peer->rx_alive,
			peer->rx_must_die, peer->init_cplt, peer->restart_init,
			peer->peer_name);
		logMsg(env);
		peer = (struct fmd_peer *)l_next(&li);
	};
exit: 
	return 0;
};

struct cli_cmd CLIStatus  = {
(char *)"status",
2,
0,
(char *)"Print FMD thread status.",
(char *)"<No Parameters>\n",
CLIStatusCmd,
ATTR_RPT
};

extern struct cli_cmd CLIApp;

int CLIAppCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;

	if (argc) {
		idx = atoi(argv[0]);
		if ((idx >= FMD_MAX_APPS) || (idx < 0)) {
			sprintf(env->output, "Illegal idx, range 0 - %d\n",
				FMD_MAX_APPS-1);
			logMsg(env);
			goto exit;
		};
		if (app_st.apps[idx].alive) {
			app_st.apps[idx].i_must_die = 1;
			pthread_kill(app_st.apps[idx].app_thr, SIGHUP);
			pthread_join(app_st.apps[idx].app_thr, NULL);
		};
		app_st.apps[idx].alive = 0;
		app_st.apps[idx].i_must_die = 0;
		app_st.apps[idx].proc_num = 0;
		memset(app_st.apps[idx].app_name, 0, MAX_APP_NAME+1);
		app_st.apps[idx].alloced = 0;
	};
	display_apps_dd(env);

exit: 
	return 0;
};

struct cli_cmd CLIApp  = {
(char *)"app",
3,
0,
(char *)"Print FMD Application connections status\n",
(char *)"{{idx>}\n"
	"<idx> : Optionally shutdown and clear application at this index.\n",
CLIAppCmd,
ATTR_NONE
};

extern struct cli_cmd CLINotify;

int CLINotifyCmd(struct cli_env *env, int argc, char **argv)
{
	if (0) {
		argv[0][0] = argc;
		(void) env;
	}

	fmd_notify_apps();
	return 0;
};

struct cli_cmd CLINotify  = {
(char *)"notify",
3,
0,
(char *)"Nofifies all applications of a change in the device directory\n",
(char *)"<No Parameters>\n",
CLINotifyCmd,
ATTR_NONE
};

struct cli_cmd *fmd_mgmt_cli_cmds[3] = {
	&CLIStatus,
	&CLIApp,
	&CLINotify
};

void fmd_bind_mgmt_dbg_cmds(void)
{
	add_commands_to_cmd_db(sizeof(fmd_mgmt_cli_cmds)/
			sizeof(fmd_mgmt_cli_cmds[0]), 
					&fmd_mgmt_cli_cmds[0]);
}

#ifdef __cplusplus
}
#endif
