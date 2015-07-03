/* Fabric Management Daemon Configuration CLI Commands */
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
#define __STDC_FORMAT_MACROS
// #include <cinttypes>

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
#include "fmd_app_mgmt.h"
#include "fmd_cfg.h"
#include "fmd_state.h"
#include "fmd.h"
// #include "cli_cmd_line.h"
// #include "cli_cmd_db.h"
// #include "cli_parse.h"
#include "libcli.h"
#include "fmd_app_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct cli_cmd CLIFCfgDump;

int CLIFCfgDumpCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t i;

	if (0)
		argv[0][0] = argc;

	sprintf(env->output, 
		"\nMPI NUM --MPORT-HANDLE-- -------EP-------  M V --DevId- ---HC---\n");
	logMsg(env);
	for (i = 0; i < FMD_MAX_MPORTS; i++) {
		struct fmd_mport_info *mpi;
		mpi = &fmd->cfg->mport_info[i];
			sprintf(env->output, 
			"%3d %3d %16p %16s %2d %1d %8x %8x\n",
			i, mpi->num, mpi->mp_h,
			(NULL == mpi->ep)?"NO EP":mpi->ep->name,
			mpi->op_mode, mpi->devids[0].valid, 
			mpi->devids[0].devid, 
			mpi->devids[0].hc);
		logMsg(env);
	};

	if (!fmd->cfg->ep_cnt) {
		sprintf(env->output,  "\nNo endpoints to display\n");
		goto switches;
	};
		
	sprintf(env->output, 
"\nidx V ENDPOINT-HANDLE- ------NAME------ PCnt V P# ---CT--- V --DevId- ---HC---\n");
	logMsg(env);
	for (i = 0; i < fmd->cfg->ep_cnt; i++) {
		struct fmd_cfg_ep *ep;
		ep = &fmd->cfg->eps[i];
	
		sprintf(env->output,
			"%3d %1d %16p %16s %4d %1d %2d %8x %1d %8x %8x\n",
			i, ep->valid, ep->ep_h, ep->name, ep->port_cnt,
			ep->ports[0].valid, ep->ports[0].port, ep->ports[0].ct, 
			ep->ports[0].devids[0].valid, 
			ep->ports[0].devids[0].devid,
			ep->ports[0].devids[0].hc); 
		logMsg(env);
	};
	
switches:
	if (!fmd->cfg->sw_cnt) {
		sprintf(env->output,  "\nNo Switches to display\n");
		goto connections;
	};
		
	sprintf(env->output, 
"\nidx V  -SWITCH-HANDLE-- ------NAME----- --TYPE-- SZ ---DID-- ---HC--- ---CT---\n");
	logMsg(env);
	for (i = 0; i < fmd->cfg->sw_cnt; i++) {
		struct fmd_cfg_sw *sw;
		sw = &fmd->cfg->sws[i];
	
		sprintf(env->output,
			"%3d %1d %16p %16s %8s %2d %8x %8x %8x\n",
			i, sw->valid, sw->sw_h, sw->name, sw->dev_type,
			sw->did_sz, sw->did, sw->hc, sw->ct);
		logMsg(env);
	};

connections:
	if (!fmd->cfg->conn_cnt) {
		sprintf(env->output,  "\nNo connections to display\n");
		goto exit;
	};

	sprintf(env->output, "\nCN V  ----Name---- Pt ----Name---- Pt\n");
	logMsg(env);
	for (i = 0; i < fmd->cfg->conn_cnt; i++) {
		struct fmd_cfg_conn *con = &fmd->cfg->cons[i];
		char *e_name[2];
		char *no_name = (char *)"NO NAME";
		uint32_t e;

		e_name[0] = e_name[1] = no_name;
		if (!con->valid)
			continue;
		for (e = 0; e < 2; e++) {
			struct fmd_cfg_sw *sw_h = con->ends[e].sw_h;
			struct fmd_cfg_ep *ep_h = con->ends[e].ep_h;
			if (con->ends[e].ep) {
				if (NULL != ep_h) 
					e_name[e] = 
					(NULL == ep_h->name)?no_name:ep_h->name;
			} else {
				if (NULL != sw_h) 
					e_name[e] = 
					(NULL == sw_h->name)?no_name:sw_h->name;
			};
		};
				
		sprintf(env->output, "%2d %1d %12s %2d %12s %2d\n",
			i, con->valid, e_name[0], con->ends[0].port_num,
			e_name[1], con->ends[1].port_num);
		logMsg(env);
	};
 
exit:
	return 0;
};

struct cli_cmd CLIFCDump  = {
(char *)"fcdump",
2,
0,
(char *)"FMD Configuration Dump.",
(char *)"<No Parameters>\n",
CLIFCfgDumpCmd,
ATTR_RPT
};

extern struct cli_cmd CLIFSelect;

int CLIFSelectCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t idx;

	idx = getHex(argv[1], 0);

	switch (argv[0][0]) {
	default:
	case 'm':
	case 'M': /* Mport */
		env->h = fmd->cfg->mport_info[0].mp_h;
		break;
	case 's':
	case 'S': /* Switch */
		if (idx >= fmd->cfg->sw_cnt) {
			sprintf(env->output, "Invalid index, max is %d",
				fmd->cfg->sw_cnt - 1);
			logMsg(env);
			goto exit;
		};
		if ((NULL == fmd->cfg->sws[idx].sw_h) || 
				!fmd->cfg->sws[idx].valid) {
			sprintf(env->output, "Invalid switch..."); 
			logMsg(env);
			goto exit;
		};
		env->h = (void *)fmd->cfg->sws[idx].sw_h;
		break;
		
	case 'e':
	case 'E': /* Endpoint */
		if (idx >= fmd->cfg->ep_cnt) {
			sprintf(env->output, "Invalid index, max is %d",
				fmd->cfg->ep_cnt - 1);
			logMsg(env);
			goto exit;
		};
		if ((NULL == fmd->cfg->eps[idx].ep_h) || 
			!fmd->cfg->eps[idx].valid) {
			sprintf(env->output, "Invalid endpoint..."); 
			logMsg(env);
			goto exit;
		};
		env->h = (void *)fmd->cfg->eps[idx].ep_h;
		break;
	};
	set_prompt(env);
exit:
	return 0;
};

struct cli_cmd CLIFSelect = {
(char *)"fcsel",
3,
2,
(char *)"Fabric Management CLI Select device.",
(char *)"<type> <idx>\n"
	"Changes CLI focus to a switch or endpoint.\n"
	"<type>: M - Mport, S - switch, E - endpoint.\n"
	"<idx> : selects particular mport/switch/endpoint.\n"
	"        <idx> matches display from fcdump command\n",
CLIFSelectCmd,
ATTR_NONE
};

extern struct cli_cmd CLIFStatus;

int CLIFStatusCmd(struct cli_env *env, int argc, char **argv)
{
	sprintf(env->output,
		"\nApp Thread: Alive %1d Must Die %d FD %d Port %d\n",
		app_st.loop_alive, app_st.all_must_die, app_st.fd, 
		app_st.port);
	logMsg(env);
	sprintf(env->output, "App Thread: Skt %s\n", app_st.addr.sun_path);
	logMsg(env);

	return 0;
};

struct cli_cmd CLIFStatus = {
(char *)"fstat",
2,
0,
(char *)"Fabric Management Status command.",
(char *)"No Parameters\n"
	"Prints current status of application connection thread.\n",
CLIFStatusCmd,
ATTR_NONE
};

struct cli_cmd *fmd_cli_cmds[3] = {
	&CLIFCDump,
	&CLIFSelect,
	&CLIFStatus
};

void fmd_bind_dbg_cmds(void)
{
	add_commands_to_cmd_db(sizeof(fmd_cli_cmds)/
			sizeof(fmd_cli_cmds[0]), 
					&fmd_cli_cmds[0]);
}

#ifdef __cplusplus
}
#endif
