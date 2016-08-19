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

#include <stdio.h>
#include <stdlib.h>
#include "libcli.h"
#include "regrw.h"
#include "regrw_log.h"

#ifdef __cplusplus
extern "C" {
#endif

char *level_strings[(int)REGRW_LL_LAST] = {
	(char *)"NOLOG",
	(char *)"CRIT",
	(char *)"ERROR",
	(char *)"WARN",
	(char *)"HIGH",
	(char *)"INFO",
	(char *)"Debug",
	(char *)"TRACE"
};

#define LOG_STR(x) (level_strings[(int)LOG_VALUE(x)])

int REGRWLogLevelCmd(struct cli_env *env, int argc, char **argv)
{
        uint8_t level = regrw_get_log_level();

	if (argc) {
		level = getHex(argv[0], 0);
		level = (enum regrw_log_level)regrw_set_log_level(level);
	};

	sprintf(env->output, "\nCompiled log level %d: %s\n", 
		REGRW_LL, LOG_STR(REGRW_LL));
	logMsg(env);

	sprintf(env->output, "Current  LOG level %d: %s\n", 
		level, LOG_STR(level));
	logMsg(env);

	return 0;
};

struct cli_cmd REGRWLogLevel = {
"REGRWll",
3,
0,
"Display or set current log level for the REGRW library.",
"{<level>}\n"
        "0: Logging disabled.\n"
        "1: Critical, Failure critical to correct operation\n"
        "2: Error   , Error occurred, may not affect operation\n"
        "3: Warning , Something a bit strange occurred\n"
        "4: High priority info, mainly thread synchronization info\n"
        "5: Info    , Informational, trace operation of the system\n"
        "6: Debug   , Detailed information about system operation\n"
        "7: Trace   , Tracing level for system operation.\n"
        "   NOTE: Levels above \"Error\" impact performance significantly.\n"
        "   NOTE: Maximum level available is set through the \"REGRW_LOG_LEVEL\"\n"
        "         Makefile compile option.",
REGRWLogLevelCmd,
ATTR_NONE
};

/**
 * Sets the debug level for messages displayed on the screen.
 */
int REGRWLogDLevelCmd(struct cli_env *env, int argc, char **argv)
{
        uint8_t level = regrw_get_log_dlevel();

	if (argc) {
		level = getHex(argv[0], 0);
		level = (enum regrw_log_level)regrw_set_log_dlevel(level);
	};

	sprintf(env->output, "\nCompiled log level %d: %s\n", 
		REGRW_LL, LOG_STR(REGRW_LL));
	logMsg(env);

	sprintf(env->output, "Current DISP level %d: %s\n", 
		level, LOG_STR(level));
	logMsg(env);

	return 0;
};

struct cli_cmd REGRWDispLevel = {
"REGRWld",
3,
0,
"Display or set REGRW DISPLAY level.",
"{<level>}\n"
        "1: Logging disabled.\n"
        "2: Critical, Failure critical to correct operation\n"
        "3: Error   , Error occurred, may not affect operation\n"
        "4: Warning , Something a bit strange occurred\n"
        "5: High priority info, mainly thread synchronization info\n"
        "6: Info    , Informational, trace operation of the system\n"
        "7: Debug   , Detailed information about system operation\n"
        "   NOTE: Levels above \"Error\" impact performance significantly.\n"
        "   NOTE: Maximum level available is set through the \"LOG_LEVEL\"\n"
        "         Makefile compile option.",
REGRWLogDLevelCmd,
ATTR_NONE
};

struct cli_cmd *regrw_cmds[] = 
	{ &REGRWLogLevel,
	  &REGRWDispLevel
	};

void regrw_bind_cli_cmds(void)
{
        add_commands_to_cmd_db(sizeof(regrw_cmds)/sizeof(regrw_cmds[0]), 
				regrw_cmds);
        return;
};

#ifdef __cplusplus
}
#endif
