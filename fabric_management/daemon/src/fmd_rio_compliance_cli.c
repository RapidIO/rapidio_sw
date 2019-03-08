/*
****************************************************************************
Copyright (c) 2018, Integrated Device Technology Inc.
Copyright (c) 2018, RapidIO Trade Association
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

#include "rio_route.h"
#include "tok_parse.h"
#include "did.h"
#include "ct.h"
#include "fmd.h"
#include "fmd_dev_rw_cli.h"
#include "liblog.h"
#include "libcli.h"
#include "riocp_pe_internal.h"
#include "pe_mpdrv.h"
#include "pe_mpdrv_private.h"
#include "string_util.h"

#include "rio_standard.h"
#include "RXS2448.h"
#include "RapidIO_Routing_Table_API.h"

#ifdef __cplusplus
extern "C" {
#endif

int find_device(struct cli_env *env, size_t pes_count, riocp_pe_handle *pes,
		char *tok, riocp_pe_handle *pe)
{
	size_t i;
	ct_t comptag = 0;
	ct_t pe_ct;

	*pe = NULL;

	// Find device based on device name
	for (i = 0; i < pes_count; i++) {
		// try as a device name
		if (!strcmp(pes[i]->sysfs_name, tok)
				&& (strlen(pes[i]->sysfs_name)
						== strlen(tok))) {
			*pe = pes[i];
			LOGMSG(env, "\nFound device named \"%s\"\n", tok);
                        break;
		}
	}

        if (NULL == *pe) {
		// try again with a comptag
		if (tok_parse_ct(tok, &comptag, 0)) {
			LOGMSG(env, "\n");
			LOGMSG(env, TOK_ERR_CT_MSG_FMT);
			goto exit;
		}

		for (i = 0; i < pes_count; i++) {
			int rc = riocp_pe_get_comptag(pes[i], &pe_ct);
			if (rc) {
				LOGMSG(env, "\nFailed reading CT: %d\n", rc);
				goto exit;
			}
			if (comptag == pe_ct) {
				*pe = pes[i];
				LOGMSG(env, "\nFound CT 0x%08x\n", pe_ct);
				break;
			}
		}
	}
exit:
	return NULL == *pe;
}

riocp_pe_handle cli_validate_pe;
rio_port_t cli_validate_port;

int CLIValidateCmd(struct cli_env *env, int argc, char **argv)
{
	riocp_pe_handle *pes = NULL;
	riocp_pe_handle pe;
	size_t pes_count;
	int rc;
	ct_t comptag = 0;
        uint32_t port_num;

	if (argc) {
		rc = riocp_mport_get_pe_list(mport_pe, &pes_count, &pes);
		if (rc) {
			LOGMSG(env, "\nCould not get PE list\n");
			goto exit;
		}

		if (!pes_count) {
			LOGMSG(env, "\nNo PEs discovered!\n");
			goto exit;
		}

		if (tok_parse_port_num(argv[1], &port_num, 0)) {
			LOGMSG(env, TOK_ERR_PORT_NUM_MSG_FMT)
			goto exit;
		}

		if (find_device(env, pes_count, pes, argv[0], &pe)) {
			goto exit;
		}

		rc = riocp_pe_get_comptag(pe, &comptag);
		if (rc) {
			LOGMSG(env, "\nFailed reading CT: %d\n", rc);
			goto exit;
		}

		if (port_num >= RIOCP_PE_PORT_COUNT(pe->cap)) {
			LOGMSG(env, "%s maximum port number is %d.\n",
					pe->sysfs_name,
					RIOCP_PE_PORT_COUNT(pe->cap) - 1)
			goto exit;
		}
		cli_validate_pe = pe;
		cli_validate_port = port_num;
	} else {
		// Command repeated, retrieve previously referenced PE
		pe = cli_validate_pe;
		port_num = cli_validate_port;
	}

	if (NULL == pe->peers[port_num].peer) {
		LOGMSG(env, "%s port %d is not connected to anything.\n",
				pe->sysfs_name, port_num)
		goto exit;
	}

	rc = mpsw_verify_pe(pe, port_num);
	if (rc) {
		LOGMSG(env, "%s port %d failed, rc: 0x%x\n", 
			pe->sysfs_name, port_num, rc)
	} else {
		LOGMSG(env, "%s port %d PASSED!\n",
			pe->sysfs_name, port_num)
	}

exit:
	rc = riocp_mport_free_pe_list(&pes);
	if (rc) {
		LOGMSG(env, "\nFailed freeing PE list %d\n", rc);
	}
	return 0;
}

struct cli_cmd CLIValidate = {
(char *)"validate",
1,
2,
(char *)"validate RapidIO compliance for specific device.",
(char *)"{<comptag> <port>}\n"
	"<comptag> Component tag value or device name of device under test\n"
	"<port>    Port number of device under test.\n",
CLIValidateCmd,
ATTR_NONE
};

struct cli_cmd *val_cmd_list[] = {
&CLIValidate,
};

void fmd_bind_compliance_cmds(void)
{
	cli_validate_pe = NULL;
	cli_validate_port = RIO_ANY_PORT;
	
	add_commands_to_cmd_db(sizeof(val_cmd_list)/
			sizeof(struct cli_cmd *), val_cmd_list);
};

#ifdef __cplusplus
}
#endif
