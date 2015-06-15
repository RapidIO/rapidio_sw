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
#ifdef WINDOWS
#include <io.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "riocp_pe_internal.h"
#include "dev_db_cli_cmds.h"
#include "cli_cmd_db.h"
#include "cli_cmd_line.h"
#include "cli_parse.h"
#include "IDT_Port_Config_API.h"

#ifdef __cplusplus
extern "C" {
#endif

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLILsRioCmd(struct cli_env *env, int argc, char **argv)
{
	
	sprintf(env->output,"(%d) lsrio stubbed out.", __LINE__);
	return 0;
};

const struct cli_cmd CLILsRio = {
(char *)"lsrio",
2,
0,
(char *)"list devices in DB.\n",
(char *)"{<mport#> {<CT> {<port>}}}\n"
	"<CT>     : Component tag of device to start from.\n"
	"           Optional, if omitted list all devices\n"
	"<port>   : Limit display to this port.\n"
	"           Optional, if omitted list <CT> and devices\n"
	"           connected to all <CT> ports\n",
CLILsRioCmd,
ATTR_NONE
};

extern const struct cli_cmd CLIProbe;

int CLIProbeCmd(struct cli_env *env, int argc, char **argv)
{
	sprintf(env->output,"(%d) Probe stubbed out.", __LINE__);
	logMsg(env);
	return 0;
}

const struct cli_cmd CLIProbe = {
(char *)"probe",
5,
1,
(char *)"Probe command, attempts to access a device.",
(char *)"<compTag>, or P <port>\n"
	"If <compTag> is entered, change focus to existing device\n"
	"If \"P\" <port> is entered, probe <port> on the current device\n",
CLIProbeCmd,
ATTR_NONE
};

int CLIRstPtCmd(struct cli_env *env, int argc, char **argv)
{
	STATUS rc;
	idt_pc_reset_port_in_t rst_in;
	idt_pc_reset_port_out_t rst_out;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	DAR_DEV_INFO_t *dev_h;

	if (riocp_pe_handle_get_private(pe_h, (void **)&dev_h)) {
		printf("Current device invalid, port not reset.\n");
		goto exit;
	};

	rst_in.port_num = getHex(argv[0], -1);
	rst_in.oob_reg_acc = TRUE;
	rst_in.reg_acc_port = RIO_ALL_PORTS;
	rst_in.reset_lp = getHex(argv[1], 0);
	rst_in.preserve_config = !getHex(argv[2], 0);
	
	sprintf(env->output, "\nResetting port %d, lp %d, clr cfg %d\n",
		rst_in.port_num, rst_in.reset_lp, !rst_in.preserve_config);
	logMsg(env);
	rc = idt_pc_reset_port(dev_h, &rst_in, &rst_out); 
	if (RIO_SUCCESS == rc)
		sprintf(env->output, "Passed.\n");
	else
		sprintf(env->output, "FAILED: RC: 0x%8x IMP_RC 0x%8x\n", 
			rc, rst_out.imp_rc);
	logMsg(env);

exit:
	return 0;
};

const struct cli_cmd CLIRstPt = {
(char *)"rstpt",
3,
3,
(char *)"Reset port on device.\n",
(char *)"<port> <lp> <clrcfg>\n"
	"<port>  : port number on current device to be reset\n"
	"<lp>    : non zero value will reset link partner\n" 
	"<clrcfg>: non zero value clears registers on this port\n",
CLIRstPtCmd,
ATTR_NONE
};

const struct cli_cmd *dev_db_cmd_list[] = {
&CLILsRio,
&CLIProbe,
&CLIRstPt
};

int bind_dev_db_cli_cmds(void)
{
	return add_commands_to_cmd_db(sizeof(dev_db_cmd_list)/
			sizeof(struct cli_cmd *), dev_db_cmd_list);
};

#ifdef __cplusplus
}
#endif

