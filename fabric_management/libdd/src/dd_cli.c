/* Fabric Management Daemon Device Directory CLI Commands Implementation */
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


// #ifdef __LINUX__
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
// #endif

#include "fmd_dd.h"
#include "liblog.h"
#include "dev_db.h"
#include "cli_cmd_db.h"
#include "cli_cmd_line.h"
#include "cli_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fmd_dd *cli_dd;
struct fmd_dd_mtx *cli_dd_mtx;

char *cli_dd_fn;
char *cli_dd_mtx_fn;

extern const struct cli_cmd CLIDDDump;

int CLIDDDumpCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t i, found = 0;

	if (0)
		argv[0][0] = argc;

	if (NULL == cli_dd) {
		sprintf(env->output, "\nDevice Directory not available.\n");
		goto exit;
	};

	sprintf(env->output, "\nTime %lld.%.9ld ChgIdx: 0x%8x\n", 
		(long long)cli_dd->chg_time.tv_sec,
		cli_dd->chg_time.tv_nsec,  cli_dd->chg_idx);
	logMsg(env);
	sprintf(env->output, "fmd_dd: md_ct %x num_devs %d\n", 
		cli_dd->md_ct, cli_dd->num_devs);
	logMsg(env);
	if (cli_dd->num_devs > 0)  {
		sprintf(env->output, "Idx ---CT--- -destID- HC MP\n");
		logMsg(env);
		for (i = 0; (i < cli_dd->num_devs) && (i < FMD_MAX_DEVS); i++) {
			sprintf(env->output, "%3d %8x %8x %2x %2s\n", i, 
				cli_dd->devs[i].ct, 
				cli_dd->devs[i].destID, 
				cli_dd->devs[i].hc,
				cli_dd->devs[i].is_mast_pt?"MP":"..");
			logMsg(env);
		};
	};

	if (NULL == cli_dd_mtx) {
		sprintf(env->output, 
			"\nDevice Directory Mutex not available.\n");
		logMsg(env);
		goto exit;
	};

	sprintf(env->output, 
			"Mutex: mtx_ref_cnt %x dd_ref_cnt %x init_done %x\n",
			cli_dd_mtx->mtx_ref_cnt, cli_dd_mtx->dd_ref_cnt,
			cli_dd_mtx->init_done );
	logMsg(env);

	found = 0;
	for (i = 0; i < FMD_MAX_APPS; i++) {
		if (!cli_dd_mtx->dd_ev[i].in_use)
			continue;

		if (!found) {
			sprintf(env->output, "\nIdx --Proc-- Waiting\n");
			logMsg(env);
			found = 1;
		};
		
		sprintf(env->output, "%3d %8d %d\n", i,
			cli_dd_mtx->dd_ev[i].proc,
			cli_dd_mtx->dd_ev[i].waiting);
		logMsg(env);
	};
	if (!found) {
		sprintf(env->output, "\nNo applications connected.\n");
		logMsg(env);
	};
		
exit:
	return 0;
};

const struct cli_cmd CLIDDDump = {
(char *)"dddump",
3,
0,
(char *)"Device Directory Dump, no parameters.",
(char *)"Prints current state of the the device diretory and mutext.",
CLIDDDumpCmd,
ATTR_RPT
};

extern const struct cli_cmd CLIDDInc;

int CLIDDIncCmd(struct cli_env *env, int argc, char **argv)
{
	if (0)
		argv[0][0] = argc;

	fmd_dd_incr_chg_idx(cli_dd, 1);
	sprintf(env->output, "\nIncrement idx value: 0x%8x\n", 
		fmd_dd_get_chg_idx(cli_dd));
	logMsg(env);
	return 0;
};

const struct cli_cmd CLIDDInc = {
(char *)"inc",
3,
0,
(char *)"Increment change index, no parameters.",
(char *)"Increments and prints current change index for the device database.",
CLIDDIncCmd,
ATTR_NONE
};

extern const struct cli_cmd CLIClean;

int CLICleanCmd(struct cli_env *env, int argc, char **argv)
{

	if ((NULL == cli_dd) || (NULL == cli_dd_mtx) ||
	(NULL == cli_dd_fn) || (NULL == cli_dd_mtx_fn)) {
		sprintf(env->output, "\nState pointer is null.\n");
		goto exit;
	};  
	argv[0] = NULL;
	if (argc) {
		sprintf(env->output, "\nFreeing Mutex, current state:\n");
		logMsg(env);
		if (NULL == cli_dd_mtx) {
			sprintf(env->output, "\ndd_mtx is NULL\n");
		} else {
			sprintf(env->output, "dd_ref_cnt   : %x\n",
				cli_dd_mtx->dd_ref_cnt);
			logMsg(env);
			sprintf(env->output, "mtx_ref_cnt: %x\n",
				cli_dd_mtx->mtx_ref_cnt);
			logMsg(env);
			sprintf(env->output, "init_done : %x\n",
				cli_dd_mtx->init_done);
		};
		shm_unlink(cli_dd_mtx_fn);
	} else {
		sprintf(env->output, "\nFreeing dd, current state:\n");
		logMsg(env);
		if (NULL == cli_dd) {
			sprintf(env->output, "\ndd is NULL\n");
		} else {
			sprintf(env->output, "dd_ref_cnt   : %x\n",
				cli_dd_mtx->dd_ref_cnt);
			logMsg(env);
			sprintf(env->output, "mtx_ref_cnt: %x\n",
				cli_dd_mtx->mtx_ref_cnt);
			logMsg(env);
			sprintf(env->output, "init_done : %x\n",
				cli_dd_mtx->init_done);
		};
		shm_unlink(cli_dd_fn);
	};
exit:
	logMsg(env);
	return 0;
};

const struct cli_cmd CLIClean = {
(char *)"clean",
3,
0,
(char *)"Drops shared memory blocks.",
(char *)"No parms drops sm block, any part drops mutex.",
CLICleanCmd,
ATTR_NONE
};

const struct cli_cmd *dd_cmds[3] = 
	{&CLIDDDump, 
	 &CLIDDInc,
	 &CLIClean };

void bind_dd_cmds(struct fmd_dd *dd, struct fmd_dd_mtx *dd_mtx,
			char *dd_fn, char *dd_mtx_fn)
{
	cli_dd = dd;
	cli_dd_mtx = dd_mtx;
	cli_dd_fn = dd_fn;
	cli_dd_mtx_fn = dd_mtx_fn;
	add_commands_to_cmd_db(sizeof(dd_cmds)/sizeof(dd_cmds[0]), &dd_cmds[0]);
}

#ifdef __cplusplus
}
#endif
