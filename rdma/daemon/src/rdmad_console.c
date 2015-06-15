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
#include <cstdio>
#include <cstring>

#include <signal.h>
#include <pthread.h>

#include "liblog.h"

#include "rdmad_svc.h"
#include "rdmad_main.h"
#include "libcli.h"

int ibwin_info_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)env;
	(void)argc;
	(void)argv;

	the_inbound->dump_info();
	return 0;
} /* ibwin_info_cmd_f() */

int all_ms_info_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)env;
	(void)argc;
	(void)argv;
	the_inbound->dump_all_mspace_info();
	return 0;
} /* all_ms_info_cmd_f() */

int all_ms_msub_info_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)env;
	(void)argc;
	(void)argv;
	the_inbound->dump_all_mspace_with_msubs_info();
	return 0;
} /* all_ms_info_cmd_f() */

int log_dump_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)env;
	(void)argc;
	(void)argv;
	rdma_log_dump();
	return 0;
} /* log_dump_cmd_f() */

struct cli_cmd ibwin_info_cmd = {
	"ibinfo",
	1,
	0,
	"Inbound Window Info.",
	"{None}\n"
	"Displays info about all inbound windows.\n",
	ibwin_info_cmd_f,
	ATTR_NONE
};

struct cli_cmd all_ms_info_cmd = {
	"allmsinfo",
	1,
	0,
	"Memory Space Info.",
	"{None}\n"
	"Displays info about all memory spaces.\n",
	all_ms_info_cmd_f,
	ATTR_NONE
};

struct cli_cmd all_ms_msub_info_cmd = {
	"msmsub",
	1,
	0,
	"Memory Space & Subspace Info.",
	"{None}\n"
	"Displays info about all memory spaces and subspaces.\n",
	all_ms_msub_info_cmd_f,
	ATTR_NONE
};

struct cli_cmd log_dump_cmd = {
	"dlog",
	1,
	0,
	"Dump log to screen.",
	"{None}\n"
	"Dumps log to screen.\n",
	log_dump_cmd_f,
	ATTR_NONE
};

struct cli_cmd *rdmad_cmds[] = {
	&ibwin_info_cmd,
	&all_ms_info_cmd,
	&all_ms_msub_info_cmd,
	&log_dump_cmd,
};

unsigned rdmad_cmds_size(void)
{
	return sizeof(rdmad_cmds);
}

void custom_quit(struct cli_env *e)
{
	(void)e;
	shutdown(&peer);
}
