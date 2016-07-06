/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file dot.c
 * Processing element dot graphing
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "pe.h"
#include "llist.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print dot graph node from PE to file stream
 * @param file Opened output filestream
 * @param pe   Target PE
 */
static int riocp_pe_dot_print_node(FILE *file, struct riocp_pe *pe)
{
	uint32_t route_destid;
	uint16_t route_port;
	int ret = 0;

	fprintf(file, "\t\"0x%08x\" ", pe->comptag);
	fprintf(file, "[label=\"%s\\nct: 0x%08x", riocp_pe_get_device_name(pe), pe->comptag);

	if (!RIOCP_PE_IS_SWITCH(pe->cap))
		fprintf(file, "\\nid:0x%02x", pe->destid);

	fprintf(file, "\" URL=\"javascript:parent.nodeselect(%08x)\" tooltip=\"", pe->comptag);
	fprintf(file, "%s %s (0x%08x)", riocp_pe_get_vendor_name(pe),
		riocp_pe_get_device_name(pe), pe->cap.dev_id);

	/* Put the switch routes in the tooltip */
	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		for (route_destid = 0; route_destid < 256; route_destid++) {
			ret = riocp_sw_get_route_entry(pe, 0xff, route_destid, &route_port);
			if (ret) {
				RIOCP_ERROR("Could not get route for pe 0x%08x\n", pe->comptag);
				break;
			}
			if (RIOCP_PE_IS_EGRESS_PORT(route_port))
				fprintf(file, " %02x:%u ", route_destid, route_port);
		}
	}
	fprintf(file, "&#10;\"];\n");

	return(0);
}

/**
 * Dump DOT language of connected peers
 * @param file Filestream to dump output to
 * @param list List of already seen PEs
 * @param pe   Target PE to dump
 */
static int riocp_pe_dot_dump_foreach(FILE *file, struct riocp_pe_llist_item *list,
	struct riocp_pe *pe, riocp_get_user_port_counters_t pt_get_user_port_counters)
{
	unsigned int n = 0;
	struct riocp_pe *peer;
	int ret = 0;
	char buf_pe_counters[512];
	char buf_peer_counters[512];
	int edge_color_pe;
	int edge_color_peer; /** black=0 displaying of counters is off in the dot graph, green=1 when none retry and drop counters are set,
			      * orange=2 when at least one retry counter is set and red=3 when at least one drop counter is set */

	/* Check if we already seen PE */
	if (riocp_pe_llist_find(list, pe) == NULL)
		riocp_pe_llist_add(list, pe);
	else
		return 0;

	/* Print current node */
	ret = riocp_pe_dot_print_node(file, pe);
	if (ret)
		return ret;

	/* Print links */
	for (n = 0; n < RIOCP_PE_PORT_COUNT(pe->cap); n++) {
		peer = pe->peers[n].peer;
		if (peer == NULL)
			continue;

		edge_color_pe = edge_color_peer = 0;

		/* Crude hack to not print links double, only print them from the lowest comptag */
		if (pe->comptag < peer->comptag) {
			if (RIOCP_PE_IS_SWITCH(pe->cap) && (pt_get_user_port_counters != NULL)) {
				ret = pt_get_user_port_counters(pe, n, buf_pe_counters, sizeof(buf_pe_counters), &edge_color_pe);
				if (ret)
				return(ret);
			}

			if (RIOCP_PE_IS_SWITCH(peer->cap) && (pt_get_user_port_counters != NULL)) {
				ret = pt_get_user_port_counters(peer, n, buf_peer_counters, sizeof(buf_peer_counters), &edge_color_peer);
				if (ret)
					return(ret);
			}

			if ((edge_color_pe == 3) || (edge_color_peer == 3)) { /** one drop counter is set */
				fprintf(file, "\t\t\"0x%08x\" -- \"0x%08x\" [color=%s taillabel=%u headlabel=%u ",
					pe->comptag, peer->comptag, "red", n, pe->peers[n].remote_port);
			} else if ((edge_color_pe == 2) || (edge_color_peer == 2)) { /** one retry counter is set */
				fprintf(file, "\t\t\"0x%08x\" -- \"0x%08x\" [color=%s taillabel=%u headlabel=%u ",
					pe->comptag, peer->comptag, "orange", n, pe->peers[n].remote_port);
			} else if ((edge_color_pe == 1) || (edge_color_peer == 1)) { /** none retry and drop counters are set */
				fprintf(file, "\t\t\"0x%08x\" -- \"0x%08x\" [color=%s taillabel=%u headlabel=%u ",
					pe->comptag, peer->comptag, "green", n, pe->peers[n].remote_port);
			} else {
				fprintf(file, "\t\t\"0x%08x\" -- \"0x%08x\" [taillabel=%u headlabel=%u ",
					pe->comptag, peer->comptag, n, pe->peers[n].remote_port);
			}

			if (RIOCP_PE_IS_SWITCH(pe->cap) && (pt_get_user_port_counters != NULL)) {
				fprintf(file, "URL=\"javascript:parent.edgeselect(%08x:%u)\"", pe->comptag, n);
				fprintf(file, " tailtooltip=\"");
				fprintf(file, "%s", buf_pe_counters);
			}

			if (RIOCP_PE_IS_SWITCH(peer->cap) && (pt_get_user_port_counters != NULL)) {
				fprintf(file, " headURL=\"javascript:parent.edgeselect(%08x:%u)\"", peer->comptag, pe->peers[n].remote_port);
				fprintf(file, " headtooltip=\"");
				fprintf(file, "%s", buf_peer_counters);
			}

			fprintf(file, " URL=\"javascript:parent.edgeselect(%08x:%u)\"", pe->comptag, n);
			fprintf(file, " tooltip=\"");
			if (!RIOCP_PE_IS_SWITCH(pe->cap)) {
				fprintf(file, "widh=%dX speed=%d&#10;\"",pe->port->width,pe->port->speed);
			} else {
				fprintf(file, "widh=%dX speed=%d&#10;\"",peer->port->width,peer->port->speed);
			}
			fprintf(file, "];\n");
		}
	}

	/* Recursively do the same for peer */
	for (n = 0; n < RIOCP_PE_PORT_COUNT(pe->cap); n++) {
		peer = pe->peers[n].peer;
		if (peer == NULL)
			continue;

		ret = riocp_pe_dot_dump_foreach(file, list, peer, pt_get_user_port_counters);
		if (ret)
			return(ret);
	}

	return ret;
}

/**
 * Dump DOT graph from RapidIO master port
 * @param filename File to dump dot graph to
 * @param mport    RapidIO master port (root node of graph)
 * @retval -EINVAL Invalid root node
 * @retval -EPERM  Unable to open filename for writing
 * @retval -EIO    Error with RapidIO maintenance access
 */
int RIOCP_SO_ATTR riocp_pe_dot_dump(char *filename, riocp_pe_handle mport, riocp_get_user_port_counters_t pt_get_user_port_counters)
{
	FILE *file;
	struct riocp_pe_llist_item *seen;
	int ret = 0;

	if (mport == NULL)
		return -EINVAL;

	file = fopen(filename, "w");
	if (file == NULL)
		return -EPERM;

	seen = (struct riocp_pe_llist_item *)calloc(1, sizeof(*seen));
	if(!seen){
		fclose(file);	
		return -ENOMEM;
	}

	fprintf(file, "# Autogenerated DOT graph by libriocp_pe\n");
	fprintf(file, "graph network {\n");
	fprintf(file, "overlap=false;\n");
	fprintf(file, "splines=line;\n");
	fprintf(file, "pad=\"0.2,0.0\";\n");
	fprintf(file, "ranksep=1.1;\n");

	ret = riocp_pe_dot_dump_foreach(file, seen, mport, pt_get_user_port_counters);
	if (ret) {
		RIOCP_ERROR("Could not dump dot graph\n");
	}
	else{
		fprintf(file, "}\n");
	}
	
	fclose(file);
	riocp_pe_llist_free(seen);

	return ret;
}



#ifdef __cplusplus
}
#endif
