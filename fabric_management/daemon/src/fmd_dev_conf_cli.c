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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fmd.h"
#include "fmd_dev_rw_cli.h"
#include "liblog.h"
#include "libcli.h"
#include "riocp_pe_internal.h"
#include "pe_mpdrv_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIConfigCmd(struct cli_env *env, int argc, char **argv)
{
	int rc, info_rc;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	struct mpsw_drv_private_data *h;
	int j;
	struct riocp_pe_port pe_port_info[24];
	riocp_pe_handle peer_pe;

	if (0) {
		argv[0][0] = argc;
	}

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	rc = riocp_pe_handle_get_private(pe_h, (void **)&h);
	if (rc) {
		sprintf(env->output, "\nCould not get device info\n");
		logMsg(env);
		goto exit;
	};
	
	if (NULL == h) {
		sprintf(env->output, "\nInfo pointer is NULL\n");
		logMsg(env);
		goto exit;
	};
	
	if (!h->st.pc.num_ports || !h->st.ps.num_ports) {
		sprintf(env->output,
				"\nPort information incorrect for device..\n");
		logMsg(env);
		goto exit;
	};

	info_rc = riocp_pe_get_ports(pe_h, pe_port_info);
	if (info_rc) {
		sprintf(env->output, "\nGet port information failed: rc %d.",
			info_rc);
		logMsg(env);
	};

	sprintf(env->output, "\nPhysLink TO: %8.1f microseconds..",
		((float)(h->st.pc.lrto))/10.0);
	logMsg(env);
	sprintf(env->output, "\nLogResp  TO: %8.1f microseconds..",
		((float)(h->st.pc.log_rto))/10.0);
	logMsg(env);

	sprintf(env->output, "\nPt A OK Port Width  Speed   FC   Idle TxEn Enables TX L_INV RX L_INV CONN    \n");
	logMsg(env);
	
	for (int i = 0; i < h->st.pc.num_ports; i++) {
		char *enables;
		char tx_linvert[5] = {' '};
		char rx_linvert[5] = {' '};
		char *name = (char *)" ";
		uint32_t port = 0;

		if (!info_rc) {
			if (NULL == pe_port_info[i].peer) {
				name = (char *)"NO_CONN";
			} else {
				port = pe_port_info[i].peer->id;
				if (!riocp_pe_find_comptag(*fmd->mp_h,
					pe_port_info[i].peer->pe->comptag,
					&peer_pe)) {
						name =
						(char *)peer_pe->sysfs_name;
				};
			};
		};
		if (h->st.pc.pc[i].port_lockout) {
			enables = (char *)"LOCKOUT";
		} else if (h->st.pc.pc[i].nmtc_xfer_enable) {
			enables = (char *)"ENABLED";
		} else {
			enables = (char *)"MtcONLY";
		};

		for (j = 0; j < PW_TO_LANES(h->st.pc.pc[i].pw); j++) {
			tx_linvert[j] = h->st.pc.pc[i].tx_linvert[j]?'I':'-';
			rx_linvert[j] = h->st.pc.pc[i].rx_linvert[j]?'I':'-';
		};
		tx_linvert[4] = rx_linvert[4] = '\0';

		for (j = 0; j < h->st.ps.num_ports; j++) {
			if (h->st.pc.pc[i].pnum == h->st.ps.ps[j].pnum) {
				break;
			};
		}
		if (j == h->st.ps.num_ports) {
			j = -1;
		};

		sprintf(env->output,
			"%2d %1s %2s %5s/%5s %5s %2s/%2s %2s/%2s %4s %7s %2s %5s %2s %5s %8s.%2d\n", 
			i, h->st.pc.pc[i].port_available?"Y":"-",
			(j != -1)?(h->st.ps.ps[j].port_ok?"OK":"no"):"--",
			PW_TO_STR(h->st.pc.pc[i].pw),
			(j != -1)?PW_TO_STR(h->st.ps.ps[j].pw):"-----",
			LS_TO_STR(h->st.pc.pc[i].ls),
			FC_TO_STR(h->st.pc.pc[i].fc),
			(j != -1)?FC_TO_STR(h->st.ps.ps[j].fc):"--",
			ISEQ_TO_STR(h->st.pc.pc[i].iseq),
			(j != -1)?ISEQ_TO_STR(h->st.ps.ps[i].iseq):"-----",
			h->st.pc.pc[i].xmitter_disable?"N":"Y",
			enables, 
			h->st.pc.pc[i].tx_lswap?"Y":"N", tx_linvert,
			h->st.pc.pc[i].rx_lswap?"Y":"N", rx_linvert,
			name, port);
		logMsg(env);
		
	};
exit:
	return 0;
}

struct cli_cmd CLIConfig = {
(char *)"config",
2,
0,
(char *)"display device configuration",
(char *)"No parameters.",
CLIConfigCmd,
ATTR_RPT
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIResetCmd(struct cli_env *env, int argc, char **argv)

{
	int rc;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	int port;
	bool reset_lp = false;

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	port = getDecParm(argv[0], 0);
	if (argc > 1) {
		reset_lp = true;
	};

	/* read data back */
	rc = riocp_pe_reset_port(pe_h, port, reset_lp);
	if (rc) {
		sprintf(env->output, "\nFailed: %d %s ", rc, strerror(rc));
	} else {
		sprintf(env->output, "\nSuccess!\n");
	}
	logMsg(env);

exit:
	return 0;
}

struct cli_cmd CLIReset = {
(char *)"reset",
1,
1,
(char *)"Reset port on current device.",
(char *)"<port> <lp>\n"
	"REset <port> on current device.\n"
	"If <lp> is present, reset the link partner first.\n",
CLIResetCmd,
ATTR_NONE
};

struct cli_cmd *conf_cmd_list[] = {
&CLIConfig,
&CLIReset
};

void fmd_bind_dev_conf_cmds(void)
{
	add_commands_to_cmd_db(sizeof(conf_cmd_list)/
			sizeof(struct cli_cmd *), conf_cmd_list);
};

#ifdef __cplusplus
}
#endif
