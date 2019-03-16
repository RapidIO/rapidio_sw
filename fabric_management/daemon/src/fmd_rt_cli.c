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

#include "rio_misc.h"
#include "rio_ecosystem.h"
#include "fmd.h"
#include "fmd_rt_cli.h"
#include "liblog.h"
#include "libcli.h"
#include "tok_parse.h"
#include "riocp_pe_internal.h"
#include "pe_mpdrv_private.h"
#include "string_util.h"

#include "rio_standard.h"
#include "RXS2448.h"
#include "RapidIO_Routing_Table_API.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTV_STRING(x) \
	RIO_RTV_IS_PORT(x) ? "PORT" :			\
	RIO_RTV_IS_MC_MSK(x) ? "MULTICAST" :	\
	RIO_RTV_IS_LVL_GRP(x) ? "LEVEL" :		\
	(RIO_RTE_DROP == x) ? "DROP" :			\
	(RIO_RTE_DFLT_PORT == x) ? "DEFAULT_PORT" : "UNKNOWN"

int rt_parse_port(struct cli_env *env, char *token, uint32_t *port_val)
{
	switch (parm_idx(token, (char *)"ALL")) {
	case 0:
		*port_val = RIO_ALL_PORTS;
		break;
	default:
		if (tok_parse_port_num(token, port_val, 0)) {
			LOGMSG(env, TOK_ERR_PORT_NUM_MSG_FMT);
			return -1;
		}
	}
	return 0;
}

#define BUFF_SZ 5
void get_rt_lable(uint32_t rtv, char *buffer) {

	memset(buffer, 0, BUFF_SZ);
	if (RIO_RTV_IS_PORT(rtv)) {
		snprintf(buffer, BUFF_SZ, " %2d ", RIO_RTV_GET_PORT(rtv));
	}
	if (RIO_RTV_IS_MC_MSK(rtv)) {
		snprintf(buffer, BUFF_SZ, "M%3d", RIO_RTV_GET_MC_MSK(rtv));
	}
	if (RIO_RTV_IS_LVL_GRP(rtv)) {
		snprintf(buffer, BUFF_SZ, "G%3d", RIO_RTV_GET_LVL_GRP(rtv));
	}
	if (RIO_RTE_DROP == rtv) {
		snprintf(buffer, BUFF_SZ, " -- ");
	}
	if (RIO_RTE_DFLT_PORT == rtv) {
		snprintf(buffer, BUFF_SZ, " dft");
	}
}

void rt_print(struct cli_env *env, rio_rt_uc_info_t *rt, uint32_t default_route)
{
	char dflt_rt[BUFF_SZ];
	char rt_lable[BUFF_SZ];

	get_rt_lable(default_route, dflt_rt);
	LOGMSG(env, "\nDefault port routing: %s\n", dflt_rt);
	LOGMSG(env, "DID    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F")
	for (did_val_t did = 0; did < RIO_RT_GRP_SZ; did++) {
		if (!(did & 0xF)) {
			LOGMSG(env, "\n0x%2x ", did);
		}
		get_rt_lable(rt[did].rte_val, rt_lable);
		LOGMSG(env, "%4s", rt_lable);
	}
	LOGMSG(env, "\n");
}

void mc_print(struct cli_env *env, rio_rt_mc_info_t *mc, uint32_t max)
{
	uint32_t found_one = 0;
	for (uint32_t idx = 0; idx < max; idx++) {
		uint32_t printed_port = 0;
		if (!mc[idx].allocd) {
			continue;
		}
		if (!found_one) {
			LOGMSG(env, "\nIdx Sz DestID Ports\n")
			found_one = 1;
		}
		LOGMSG(env, "%3d ", idx);
		if (mc[idx].in_use) {
			LOGMSG(env, "%2s 0x%4x ", (char *)((tt_dev8 == mc[idx].tt) ? "8 " : "16"),
					mc[idx].mc_destID);
		} else {
			LOGMSG(env, "          ");
		}
		for (rio_port_t port = 0; port < sizeof(uint32_t) * 8; port++) {
			if (!((1 << port) & mc[idx].mc_mask)) {
				continue;
			}
			if (printed_port) {
				LOGMSG(env, ", ")
			}
			printed_port = 1;
			LOGMSG(env, "%d", port)
		}
		LOGMSG(env, "\n");
	}
	if (!found_one) {
		LOGMSG(env, "\nNo masks allocated.\n");
	}
}

int CLIProbeCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;

	rio_rt_probe_all_in_t pr_in;
	rio_rt_probe_all_out_t pr_out;
	rio_rt_probe_in_t probe_in;
	rio_rt_probe_out_t probe_out;
	uint32_t temp_pt;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	int found_one = 0;

	if (NULL == pe_h) {
		LOGMSG(env, "\nNo Device Selected...\n");
		goto exit;
	}

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	if (NULL == priv) {
		LOGMSG(env, "\nCannot retrieve device state...\n");
		goto exit;
	}

	dev_h = &priv->dev_h;
	if (tok_parse_did(argv[0], &probe_in.destID, 0)) {
		goto exit;
	}
	if (rt_parse_port(env, argv[1], &temp_pt)) {
		goto exit;
	}
	if (dev16_sz == riocp_get_did_sz()) {
		probe_in.tt = tt_dev16;
	} else {
		probe_in.tt = tt_dev8;
	}


	// Only read the table if a specific port is selected.
	if (RIO_ALL_PORTS == temp_pt) {
		probe_in.rt = &priv->st.g_rt;
		probe_in.probe_on_port = 0;
	} else {
		pr_in.probe_on_port = temp_pt;
		pr_in.rt = &priv->st.pprt[temp_pt];
		rc = rio_rt_probe_all(dev_h, &pr_in, &pr_out);

		if (RIO_SUCCESS != rc) {
			LOGMSG(env, "\nRouting table read FAILED: rc: %d imp rc 0x%x\n",
							rc, probe_out.imp_rc);
			goto exit;
		}
		probe_in.probe_on_port = temp_pt;
		probe_in.rt = pr_in.rt;
	}
	rc = rio_rt_probe(dev_h, &probe_in, &probe_out);

	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nProbe FAILED: rc: %d imp rc 0x%x\n",
							rc, probe_out.imp_rc);
	}
	LOGMSG(env,"\nProbe successful.\n");

	if (!probe_out.valid_route) {
		LOGMSG(env, "\nPackets discarded, reason %d %s\n",
			probe_out.reason_for_discard,
			DISC_REASON_STR(probe_out.reason_for_discard));
	}
	LOGMSG(env, "\nPackets routed by value 0x%x %s",
			probe_out.routing_table_value,
			RTV_STRING(probe_out.routing_table_value));
	LOGMSG(env, "\nDefault route: 0x%x %s",
			probe_out.default_route,
			RTV_STRING(probe_out.default_route));
	LOGMSG(env, "\nFilter active: %s",
			probe_out.filter_function_active ? "TRUE" : "FALSE");
	LOGMSG(env, "\nTRace  active: %s",
			probe_out.trace_function_active ? "TRUE" : "FALSE");
	LOGMSG(env, "\nTime to live : %s",
			probe_out.time_to_live_active ? "TRUE" : "FALSE");
	LOGMSG(env, "\nMulticast ports: ");
	for (uint16_t i = 0; i < RIO_MAX_PORTS; i++) {
		if (probe_out.mcast_ports[i]) {
			if (found_one) {
				LOGMSG(env, ", %d", i)
			} else {
				LOGMSG(env, "%d", i)
			}
			found_one = 1;
		}
	}
	LOGMSG(env, "%s\n", (found_one ? "" : "NONE"));
		
	// Avoid compile error for unused parameter
	if (0) {
		argv[0][0] = argc;
	}
exit:
	return 0;
}

struct cli_cmd CLIProbe = {
(char *)"probe",
4,
2,
(char *)"Probe route of a packet through a switch.",
(char *)"<devid> <port>\n"
"Determine how <devid> is routed when received by <port>"
"devid  : destination ID for routing check\n"
"port   : the device port where routing occurs. Use ALL for global routing table.\n",
CLIProbeCmd,
ATTR_NONE
};

int CLIRoutingTableCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;

	rio_rt_probe_all_in_t pr_in;
	rio_rt_probe_all_out_t pr_out;
	uint32_t temp_pt;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	rio_rt_uc_info_t *rt;

	if (NULL == pe_h) {
		LOGMSG(env, "\nNo Device Selected...\n");
		goto exit;
	}

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	if (NULL == priv) {
		LOGMSG(env, "\nCannot retrieve device state...\n");
		goto exit;
	}

	dev_h = &priv->dev_h;
	if (rt_parse_port(env, argv[0], &temp_pt)) {
		goto exit;
	}

	// Only read the table if a specific port is selected.
	if (RIO_ALL_PORTS == temp_pt) {
		pr_in.rt = &priv->st.g_rt;
	} else {
		pr_in.probe_on_port = temp_pt;
		pr_in.rt = &priv->st.pprt[temp_pt];
		rc = rio_rt_probe_all(dev_h, &pr_in, &pr_out);

		if (RIO_SUCCESS != rc) {
			LOGMSG(env, "\nRouting table read FAILED: rc: %d imp rc 0x%x\n",
								rc, pr_out.imp_rc);
			goto exit;
		}
	}

	switch (argc) {
	default:
		LOGMSG(env, "Too many parameters, exiting...\n");
		goto exit;
	case 2: 
		switch (argv[1][0]) {
		case 'U': 
			rt = pr_in.rt->dom_table;
			break;
		case 'M':
			mc_print(env, pr_in.rt->mc_masks, NUM_MC_MASKS(dev_h));
			goto exit;
		default:
			LOGMSG(env, "%s is not M or U, exiting...\n", argv[1]);
			goto exit;
		}
		break;
	case 1:
		rt = pr_in.rt->dev_table;
	}
	rt_print(env, rt, pr_in.rt->default_route);
		
exit:
	return 0;
}

struct cli_cmd CLIRoutingTable = {
(char *)"rtdump",
3,
1,
(char *)"Dump the lower/upper/multicast routing info for a switch port.",
(char *)"<port> {U|M}\n"
"port   : the device port where routing occurs.\n" 
"{U|M}  : U : dev16 upper byte routing.\n"
"         M : multicast information.\n",
CLIRoutingTableCmd,
ATTR_NONE
};

int CLISetRoutingTableCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;

	rio_rt_change_rte_in_t rt_in;
	rio_rt_change_rte_out_t rt_out;
	rio_rt_set_changed_in_t set_in;
	rio_rt_set_changed_out_t set_out;
	uint32_t temp_pt;
	uint16_t temp_idx;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	uint8_t need_val = 0;
	rio_rt_uc_info_t *rt = NULL;

	if (NULL == pe_h) {
		LOGMSG(env, "\nNo Device Selected...\n");
		goto exit;
	}

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	if (NULL == priv) {
		LOGMSG(env, "\nCannot retrieve device state...\n");
		goto exit;
	}

	dev_h = &priv->dev_h;
	switch (argc) {
	default:
		LOGMSG(env, "Too many parameters, exiting...\n");
		goto exit;
	case 5: 
		if ('X' == argv[3][0] || 'D' == argv[3][0]) {
			LOGMSG(env, "No value needed for drop or default port.\n");
			goto exit;
		}
		if (tok_parse_ul(argv[4], &rt_in.rte_value, 0)) {
			LOGMSG(env, "Unrecognized token %s\n", argv[4]);
			goto exit;
		}
		// No break!
	case 4: 
		if (rt_parse_port(env, argv[0], &temp_pt)) {
			goto exit;
		}
		if (RIO_ALL_PORTS == temp_pt) {
			rt_in.rt = &priv->st.g_rt;
		} else {
			rt_in.rt = &priv->st.pprt[temp_pt];
		}
		switch (argv[1][0]) {
		case 'L' :
			rt_in.dom_entry = 0;
			rt = rt_in.rt->dev_table;
			break;
		case 'U' :
			rt_in.dom_entry = 1;
			rt = rt_in.rt->dom_table;
			break;
		default:
			LOGMSG(env, "Unknown domain/device selection.\n");
			goto exit;
		}
		if (tok_parse_ushort(argv[2], &temp_idx, 0, 255, 0)) {
			LOGMSG(env, "Illegal index value.\n");
			goto exit;
		}
		rt_in.idx = temp_idx;
		switch(argv[3][0]) {
		case 'P':
			rt_in.rte_value = RIO_RTV_PORT(rt_in.rte_value);
			need_val = 1;
			break;
		case 'M':
			rt_in.rte_value = RIO_RTV_MC_MSK(rt_in.rte_value);
			need_val = 1;
			break;
		case 'L':
			rt_in.rte_value = RIO_RTV_LVL_GRP(rt_in.rte_value);
			need_val = 1;
			break;
		case 'X':
			rt_in.rte_value = RIO_RTE_DROP;
			break;
		case 'D':
			rt_in.rte_value = RIO_RTE_DFLT_PORT;
			break;
		default:
			LOGMSG(env, "Unrecognized routing value type.\n");
			goto exit;
		}
		if (need_val && (5 != argc)) {
			LOGMSG(env, "Missing value for port, MC group, or level.\n");
			goto exit;
		}

		if (RIO_RTE_BAD == rt_in.rte_value) {
			LOGMSG(env, "Value entered is invalid for routing value type.\n");
			goto exit;
		}
	}

	rc = rio_rt_change_rte(dev_h, &rt_in, &rt_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nRouting table change FAILED: rc: %d imp rc 0x%x\n",
							rc, rt_out.imp_rc);
		goto exit;
	}

	set_in.rt = rt_in.rt;
	set_in.set_on_port = temp_pt;
		
	rc = rio_rt_set_changed(dev_h, &set_in, &set_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nRouting table write FAILED: rc: %d imp rc 0x%x\n",
							rc, set_out.imp_rc);
	}
	rt_print(env, rt, set_in.rt->default_route);

exit:
	return 0;
}

struct cli_cmd CLISetRoutingTable = {
(char *)"rtset",
3,
4,
(char *)"Set a routing table entry for a lower/upper destID byte .",
(char *)"<port> <L|U> <idx> <rt_type> {<rt_val>}\n"
"On <port>, change Lower/Upper entry at <idx> to <rt_type> {<rt_val>}\n"
"port   : the device port for the routing table, or ALL for all ports.\n"
"L|U    : selects the dev8 device (L) or upper dev16 domain (U) table.\n"
"idx    : routing tabe entry index.\n"
"         Lower: dev8 destID, or lower byte of dev16\n"
"         Upper: Upper byte of dev16\n"
"rt_type: One of:\n"
"         P: port\n"
"         M: multicast group\n"
"         L: level group\n"
"         D: Default port\n"
"         X: Drop\n"
"rt_val : Value for Port, multicast group or level group",
CLISetRoutingTableCmd,
ATTR_NONE
};

int CLIAllocMcCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;

	rio_rt_alloc_mc_mask_in_t mc_in;
	rio_rt_alloc_mc_mask_out_t mc_out;
	uint32_t temp_pt;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		LOGMSG(env, "\nNo Device Selected...\n");
		goto exit;
	}

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	if (NULL == priv) {
		LOGMSG(env, "\nCannot retrieve device state...\n");
		goto exit;
	}

	dev_h = &priv->dev_h;
	if (rt_parse_port(env, argv[0], &temp_pt)) {
		goto exit;
	}

	if (argc > 1) {
		LOGMSG(env, "Too many parameters, exiting...\n");
		goto exit;
	}

	if (RIO_ALL_PORTS == temp_pt) {
		mc_in.rt = &priv->st.g_rt;
	} else {
		mc_in.rt = &priv->st.pprt[temp_pt];
	}

	rc = rio_rt_alloc_mc_mask(dev_h, &mc_in, &mc_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nMC Mask allocation FAILED: rc: %d imp rc 0x%x\n",
							rc, mc_out.imp_rc);
		goto exit;
	}

	LOGMSG(env, "Successfully allocated MC mask %d\n",
		RIO_RTV_GET_MC_MSK(mc_out.mc_mask_rte));

exit:
	return 0;
}

struct cli_cmd CLIAllocMc = {
(char *)"rtalloc",
3,
1,
(char *)"Allocate a multicast mask.",
(char *)"<port>\n"
"Allocate a multicast mask on port.\n"
"port   : the device port for the routing table, or ALL for all ports.\n",
CLIAllocMcCmd,
ATTR_NONE
};

int CLIFreeMcCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;

	rio_rt_dealloc_mc_mask_in_t mc_in;
	rio_rt_dealloc_mc_mask_out_t mc_out;
	rio_rt_set_changed_in_t rt_in;
	rio_rt_set_changed_out_t rt_out;
	uint32_t temp_pt;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		LOGMSG(env, "\nNo Device Selected...\n");
		goto exit;
	}

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	if (NULL == priv) {
		LOGMSG(env, "\nCannot retrieve device state...\n");
		goto exit;
	}

	dev_h = &priv->dev_h;
	if (rt_parse_port(env, argv[0], &temp_pt)) {
		goto exit;
	}
	if (tok_parse_ulong(argv[1], &mc_in.mc_mask_rte, 0, NUM_MC_MASKS(dev_h) - 1, 0)) {
		LOGMSG(env, "Bad mc mask index, should be between 0 and %d\n",
				NUM_MC_MASKS(dev_h) - 1);
		goto exit;
	}

	if (argc > 2) {
		LOGMSG(env, "Too many parameters, exiting...\n");
		goto exit;
	}

	if (RIO_ALL_PORTS == temp_pt) {
		mc_in.rt = &priv->st.g_rt;
	} else {
		mc_in.rt = &priv->st.pprt[temp_pt];
	}
	mc_in.mc_mask_rte = RIO_RTV_MC_MSK(mc_in.mc_mask_rte);

	rc = rio_rt_dealloc_mc_mask(dev_h, &mc_in, &mc_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nMC Mask free FAILED: rc: %d imp rc 0x%x\n",
							rc, mc_out.imp_rc);
		goto exit;
	}

	rt_in.rt = mc_in.rt;
	rt_in.set_on_port = temp_pt;

	rc = rio_rt_set_changed(dev_h, &rt_in, &rt_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nRouting table update FAILED rc: %d imp rc 0x%x\n",
							rc, mc_out.imp_rc);
		goto exit;
	}

	LOGMSG(env, "Successfully deallocated MC mask %d and updated routing tables.\n",
		RIO_RTV_GET_MC_MSK(mc_in.mc_mask_rte));

exit:
	return 0;
}

struct cli_cmd CLIFreeMc = {
(char *)"rtfree",
3,
2,
(char *)"Free a multicast mask, and remove all use of that MC mask.",
(char *)"<port> <mc>\n"
"Free multicast mask <mc> on port <port>.\n"
"port : the device port for the routing table, or ALL for all ports.\n"
"mc   : index of multicast mask.\n",
CLIFreeMcCmd,
ATTR_NONE
};

int CLIPortsMcCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;

	rio_rt_change_mc_mask_in_t mc_in;
	rio_rt_change_mc_mask_out_t mc_out;
	rio_rt_set_changed_in_t rt_in;
	rio_rt_set_changed_out_t rt_out;
	uint32_t temp_pt;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		LOGMSG(env, "\nNo Device Selected...\n");
		goto exit;
	}

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	if (NULL == priv) {
		LOGMSG(env, "\nCannot retrieve device state...\n");
		goto exit;
	}

	dev_h = &priv->dev_h;
	if (rt_parse_port(env, argv[0], &temp_pt)) {
		goto exit;
	}

	if (tok_parse_ulong(argv[1], &mc_in.mc_mask_rte, 0, NUM_MC_MASKS(dev_h) - 1, 0)) {
		LOGMSG(env, "Bad mc mask index, should be between 0 and %d\n",
				NUM_MC_MASKS(dev_h) - 1);
		goto exit;
	}
	if (RIO_ALL_PORTS == temp_pt) {
		mc_in.rt = &priv->st.g_rt;
	} else {
		mc_in.rt = &priv->st.pprt[temp_pt];
	}

	memcpy(&mc_in.mc_info, &mc_in.rt->mc_masks[mc_in.mc_mask_rte],
		sizeof(mc_in.mc_info));
	
	mc_in.mc_mask_rte = RIO_RTV_MC_MSK(mc_in.mc_mask_rte);
	mc_in.mc_info.in_use = 1;
	mc_in.mc_info.tt = tt_dev8;
	mc_in.mc_info.mc_mask = 0;

	if (tok_parse_did(argv[2], &mc_in.mc_info.mc_destID, 0)) {
		LOGMSG(env, TOK_ERR_DID_MSG_FMT);
		goto exit;
	}

	for (int idx = 3; idx < argc; idx++) {
		uint32_t mc_port;
		if (tok_parse_port_num(argv[idx], &mc_port, 0)) {
			LOGMSG(env, TOK_ERR_PORT_NUM_MSG_FMT);
			goto exit;
		}
		mc_in.mc_info.mc_mask |=  1 << mc_port;
	}

	rc = rio_rt_change_mc_mask(dev_h, &mc_in, &mc_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nMC Mask change FAILED: rc: %d imp rc 0x%x\n",
							rc, mc_out.imp_rc);
		goto exit;
	}

	rt_in.rt = mc_in.rt;
	rt_in.set_on_port = temp_pt;

	rc = rio_rt_set_changed(dev_h, &rt_in, &rt_out);
	if (RIO_SUCCESS != rc) {
		LOGMSG(env, "\nRouting table update FAILED rc: %d imp rc 0x%x\n",
							rc, mc_out.imp_rc);
		goto exit;
	}

	LOGMSG(env, "Successfully changed MC mask %d and updated routing tables.\n",
		RIO_RTV_GET_MC_MSK(mc_in.mc_mask_rte));

exit:
	return 0;
}

struct cli_cmd CLIPortsMc = {
(char *)"rtport",
3,
3,
(char *)"Set the multicast port mask and destID, then update routing table.",
(char *)"<port> <mc> <did> {list}\n"
"Change multicast mask <mc> on port <port> to send to <port_list>\n"
"port : the device port for the routing table, or ALL for all ports.\n"
"mc   : index of multicast mask.\n"
"did  : destination ID to associate with the MC mask.\n"
"list : Zero or more ports, in the range 0 to max for the device\n",
CLIPortsMcCmd,
ATTR_NONE
};


struct cli_cmd *rt_cmd_list[] = {
&CLIProbe,
&CLIRoutingTable,
&CLISetRoutingTable,
&CLIAllocMc,
&CLIFreeMc,
&CLIPortsMc,
};

void fmd_bind_dev_rt_cmds(void)
{
	// Init globals used by repeatable commands

	add_commands_to_cmd_db(sizeof(rt_cmd_list)/
			sizeof(struct cli_cmd *), rt_cmd_list);
}

#ifdef __cplusplus
}
#endif
