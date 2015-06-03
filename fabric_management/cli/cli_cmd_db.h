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
#ifndef __CLI_CMD_DB_H__
#define __CLI_CMD_DB_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions for command database */
#ifdef __CLI_WINDOWS__
#include <windows.h>
#include <io.h>
#include "tsi721api.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "DAR_DevDriver.h"
#include "cli_cmd_line.h"

#define ATTR_NONE  0x0
#define ATTR_RPT   0x1

struct cli_cmd {
	char *name;
	int   min_match; /* Minimum # of characters to match for this cmd */
	int   min_parms; /* Minimum # of parameters to enter for this cmd */
	char *shortHelp;
	char *longHelp;
	int (*func)(struct cli_env *env, int argc, char **argv);
	int attributes;
};

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

extern int init_cmd_db(void);

extern int add_commands_to_cmd_db(int num_cmds,
				  const struct cli_cmd **cmd_list);

extern int find_cmd(char *cmd_name, const struct cli_cmd **cmd);

extern int cli_print_help(struct cli_env *env, const struct cli_cmd *cmd);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_CMD_DB_H__ */
