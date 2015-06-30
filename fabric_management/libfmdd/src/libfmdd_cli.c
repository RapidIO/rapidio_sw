/* Fabric Management Daemon Device Directory Library */
/* CLI Commands Implementation */
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
#include "fmd_app_msg.h"
#include "liblog.h"
// #include "dev_db.h"
// #include "cli_cmd_db.h"
// #include "cli_cmd_line.h"
// #include "cli_parse.h"
#include "libfmdd_info.h"

#ifdef __cplusplus
extern "C" {
#endif

fmdd_h ddl_h_cli;

extern struct cli_cmd CLIDDLCheckCT;

int CLIDDLCheckCTCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t ct;
	int rc;

	if (0)
		argv[0][0] = argc;
	ct = getHex(argv[0], 0);

	sprintf(env->output, "\nChecking component tag %8x\n", ct);
	logMsg(env);

	rc = fmdd_check_ct(ddl_h_cli, ct);

	sprintf(env->output, "Return code was      : %8x\n", rc);
	logMsg(env);

	return 0;
};

struct cli_cmd CLIDDLCheckCT = {
(char *)"ddlct",
5,
1,
(char *)"Device Directory Library CT check.",
(char *)"<ct>\n"
	"Checks whether or not an entered component tag is present\n",
CLIDDLCheckCTCmd,
ATTR_NONE
};

extern struct cli_cmd CLIDDLCheckDID;

int CLIDDLCheckDIDCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t did;
	int rc;

	if (0)
		argv[0][0] = argc;

	did = getHex(argv[0], 0);

	sprintf(env->output, "\nChecking Device ID : %8x\n", did);
	logMsg(env);

	rc = fmdd_check_did(ddl_h_cli, did);

	sprintf(env->output,   "Return code was    : %8x\n", rc);
	logMsg(env);

	return 0;
};

struct cli_cmd CLIDDLCheckDID = {
(char *)"ddldid",
5,
1,
(char *)"Device Directory Library Device ID check.",
(char *)"<did>\n"
	"Checks whether or not an entered device ID is present\n",
CLIDDLCheckDIDCmd,
ATTR_NONE
};

extern struct cli_cmd CLIDDList;

int CLIDDListCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t did_list_sz, *did_list, i;
	int rc;

	if (0)
		argv[0][0] = argc;

	rc = fmdd_get_did_list(ddl_h_cli, &did_list_sz, &did_list);

	sprintf(env->output, "\nfmdd_get_did_list returned : %d\n", rc);
	logMsg(env);

	if (rc)
		goto exit;

	sprintf(env->output, "\nNumber of device IDs: %d\n", did_list_sz);
	logMsg(env);

	if (!did_list_sz)
		goto exit;

	sprintf(env->output, "\nIdx ---DID--\n");
	logMsg(env);

	for (i = 0; i < did_list_sz; i++) {
		sprintf(env->output, "%d %8x\n", did_list_sz, did_list[i]);
		logMsg(env);
	};
exit:
	if (NULL != did_list)
		free(did_list);
	return 0;
};

struct cli_cmd CLIDDList = {
(char *)"ddlli",
5,
0,
(char *)"(No parameters) Display Device Directory device ID list.\n",
(char *)"<No Parameters>\n"
	"Display Device Directory device ID list.\n",
CLIDDListCmd,
ATTR_NONE
};

struct cli_cmd *libfmdd_cmds[3] = {
	&CLIDDLCheckCT,
	&CLIDDLCheckDID,
	&CLIDDList
};

void fmdd_bind_dbg_cmds(void *fmdd_h)
{
	struct fml_globals *t_fml = (struct fml_globals *)fmdd_h;

	if  (&fml == t_fml) {
		ddl_h_cli = fmdd_h;
		add_commands_to_cmd_db(sizeof(libfmdd_cmds)/
					sizeof(libfmdd_cmds[0]), 
					&libfmdd_cmds[0]);
		bind_dd_cmds(fml.dd, fml.dd_mtx, fml.dd_fn, fml.dd_mtx_fn);
	};
}

#ifdef __cplusplus
}
#endif
