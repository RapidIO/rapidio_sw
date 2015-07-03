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

#include <vector>
#include <string>
#include <algorithm>
#include <map>

#include <cstring>
#include <cassert>

#include <stdint.h>
#include <stdio.h>
#include <mqueue.h>
#include <semaphore.h>
#include <pthread.h>

#include "ts_vector.h"
#include "ts_map.h"
#include "rdmad_ms_owner.h"
#include "rdmad_ms_owners.h"
#include "rdmad_msubspace.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"
#include "rdmad_inbound.h"
#include "rdmad_peer_utils.h"
#include "rdmad_cm.h"
#include "rdmad_main.h"
#include "rdmad.h"
#include "rdmad_rdaemon.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_srvr_threads.h"

#include "riodp_mport_lib.h"
#include "rdma_types.h"
#include "rdma_mq_msg.h"
#include "liblog.h"

using std::vector;
using std::string;
using std::map;

/* Memory Space Owner data */
ms_owners owners;

/* Inbound space */
inbound *the_inbound;

/* Global flag for shutting down */
bool shutting_down = false;

/* Map of accept messages awaiting connect. Keyed by message queue name */
ts_map<string, cm_accept_msg>	accept_msg_map;

/* List of queue names awaiting accept */
ts_vector<string>	wait_accept_mq_names;

/* Remote daemon info */
vector<rdaemon_t*>	client_rdaemon_list;
sem_t			client_rdaemon_list_sem;
#if 0
get_mport_id_output *
get_mport_id_1_svc(get_mport_id_input *in)
{
	(void)in;

	get_mport_id_output *out = new get_mport_id_output();

	out->mport_id = peer.mport_id;
	out->status = 0;	/* Alway successful */

	return out;
} /* get_mport_id_1_svc() */


create_mso_output *
create_mso_1_svc(create_mso_input *in)
{
	(void)in;
	create_mso_output *out = new create_mso_output();
 
	DBG("ENTER\n");
	printf("in->owner_name = %s\n", in->owner_name);

	int ret = owners.create_mso(in->owner_name, &out->msoid);

	out->status = (ret > 0) ? 0 : ret;
	DBG("owners.create_mso() %s\n", out->status ? "FAILED" : "PASSED");

	printf("out = %p\n", out);
        return out;
} /* create_mso_1_svc() */


open_mso_output *
open_mso_1_svc(open_mso_input *in)
{
	open_mso_output	*out = new open_mso_output();

	DBG("ENTER\n");
	int ret = owners.open_mso(in->owner_name,
			&out->msoid,
			&out->mso_conn_id);

	out->status = (ret > 0) ? 0 : ret;
	DBG("owners.open_mso() %s\n", out->status ? "FAILED" : "PASSED");

	return out;
} /* open_mso_1_svc() */
close_mso_output *
close_mso_1_svc(close_mso_input *in)
{
	static close_mso_output	out;

	DBG("ENTER\n");

	int ret = owners.close_mso(in->msoid, in->mso_conn_id);

	out.status = (ret > 0) ? 0 : ret;
	DBG("owners.close_mso() %s\n", out.status ? "FAILED" : "PASSED");

	return &out;
} /* close_mso_1_svc() */

destroy_mso_output *
destroy_mso_1_svc(destroy_mso_input *in)
{
	static destroy_mso_output out;

	DBG("ENTER\n");

	/* Check if the memory space owner still owns memory spaces */
	if (owners[in->msoid]->owns_mspaces()) {
		WARN("msoid(0x%X) still owns memory spaces!\n", in->msoid);
		out.status = -1;
	} else {
		/* No memory spaces owned by mso, just destroy it */
		int ret = owners.destroy_mso(in->msoid);
		out.status = (ret > 0) ? 0 : ret;
		DBG("owners.destroy_mso() %s\n", out.status ? "FAILED":"PASSED");
	}

	return &out;
} /* destroy_mso_1_svc() */


create_ms_output *
create_ms_1_svc(create_ms_input *in)
{
	static create_ms_output	out;

	DBG("ENTER\n");

	/* Create memory space in the inbound space */
	int ret = the_inbound->create_mspace(in->ms_name, in->bytes, in->msoid,
								    &out.msid);
	out.status = (ret > 0) ? 0 : ret;
	DBG("the_inbound->create_mspace(%s) %s\n", in->ms_name,
					      out.status ? "FAILED" : "PASSED");

	/* If successful, add the memory space handle to the owner */
	if (!out.status) 
		owners[in->msoid]->add_msid(out.msid);
	DBG("owners[%u]->add_msid(%u) returned\n", in->msoid, out.msid);

	return &out;
} /* create_ms_1_svc() */

open_ms_output *
open_ms_1_svc(open_ms_input *in)
{
	static open_ms_output	out;

	DBG("ENTER\n");

	/* Find memory space, return its msid, ms_conn_id, and size in bytes */
	int ret = the_inbound->open_mspace(in->ms_name,
					   in->msoid,
					   &out.msid,
					   &out.ms_conn_id,
					   &out.bytes);
	out.status = (ret > 0) ? 0 : ret;
	DBG("the_inbound->open_mspace(%s) %s\n", in->ms_name,
						out.status ? "FAILED":"PASSED");
	return &out;
} /* open_ms_1_svc() */
#endif

int close_or_destroy_action(mspace *ms)
{
	/* Get list of destids connected to memory space */
	vector<uint16_t> destids = ms->get_destids();
	DBG("msid(0x%X) has %u destids associated therewith\n", ms->get_msid(),
							destids.size());

	/* For each element in the destids list, send a destroy message */
	for (auto it = begin(destids); it != end(destids); it++) {
		uint32_t destid = *it;

		/* Need to use a 'prov' socket to send the DESTROY_MS */
		sem_wait(&prov_daemon_info_list_sem);
		auto prov_it = find(begin(prov_daemon_info_list), end(prov_daemon_info_list), destid);
		if (prov_it == end(prov_daemon_info_list)) {
			ERR("Could not find socket for destid(0x%X)\n", destid);
			sem_post(&prov_daemon_info_list_sem);
			continue;	/* Better luck next time? */
		}
		cm_server *destroy_server = prov_it->conn_disc_server;
		sem_post(&prov_daemon_info_list_sem);

		/* Prepare destroy message */
		cm_destroy_msg	*dm;
		destroy_server->get_send_buffer((void **)&dm);
		dm->type	= DESTROY_MS;
		strcpy(dm->server_msname, ms->get_name());
		dm->server_msid = ms->get_msid();

		/* Send to remote daemon @ 'destid' */
		if (destroy_server->send()) {
			WARN("Failed to send destroy to destid(0x%X)\n", destid);
			continue;
		}
		INFO("Sent cm_destroy_msg for %s to remote daemon\n",
							dm->server_msname);

		/* Wait for destroy acknowledge message, but with timeout.
		 * If no ACK within say 1 second, then move on */
		cm_destroy_ack_msg *dam;
		destroy_server->flush_send_buffer();
		destroy_server->get_send_buffer((void **)&dam);
		if (destroy_server->timed_receive(1000)) {
			/* In this case whether the return value is ETIME or a failure
			 * code is irrelevant. The main thing is NOT to be stuck here.
			 */
			ERR("Did not receive destroy_ack from destid(0x%X)\n", *it);
			continue;
		}
		if (dam->server_msid != dm->server_msid)
			ERR("Received destroy_ack with wrong msid(0x%X)\n",
							dam->server_msid);
		else {
			HIGH("destroy_ack received from daemon destid(0x%X)\n", *it);
		}
	} /* for() */

	return 0;
} /* close or destroy action() */

#if 0
close_ms_output *
close_ms_1_svc(close_ms_input *in)
{
	static close_ms_output	out;

	DBG("ENTER, msid=%u, ms_conn_id=%u\n", in->msid, in->ms_conn_id);
	mspace *ms = the_inbound->get_mspace(in->msid);
	if (!ms) {
		ERR("Could not find mspace with msid(0x%X)\n", in->msid);
		out.status = -1;
		return &out;
	}

	/* Before closing the memory space, tell the clients the memory space
	 *  that it is being closed and have them acknowledge that */
	out.status = close_or_destroy_action(ms);
	if (out.status)
		return &out;

	/* If the memory space was in accepted state, clear that state */
	/* FIXME: This assumes only 1 'open' to the ms and that the creator
	 * of the ms does not call 'accept' on it. */
	ms->set_accepted(false);

	/* Now close the memory space */
	int ret = the_inbound->close_mspace(in->msid, in->ms_conn_id);
	out.status = (ret > 0) ? 0 : ret;
	DBG("the_inbound->close_mspace() %s\n", out.status ? "FAILED":"PASSED");

	return &out;
} /* close_ms_1_svc() */

destroy_ms_output *
destroy_ms_1_svc(destroy_ms_input *in)
{
	static destroy_ms_output	out;
	mspace *ms = the_inbound->get_mspace(in->msid);
	if (!ms) {
		ERR("Could not find mspace with msid(0x%X)\n", in->msid);
		out.status = -1;
		return &out;
	}

	/* Before destroying a memory space, tell its clients that it is being
	 * destroyed and have them acknowledge that */
	out.status = close_or_destroy_action(ms);
	if (out.status)
		return &out;

	/* Now destroy the memory space */
	int ret  = the_inbound->destroy_mspace(in->msoid, in->msid);
	out.status = (ret > 0) ? 0 : ret;

	/* Remove memory space identifier from owner */
	if (!out.status)
		if (owners[in->msoid]->remove_msid(in->msid) < 0) {
			WARN("Failed to remove msid from owner\n");
		}

	return &out;
} /* destroy_ms_1_svc() */
#endif

create_msub_output *
create_msub_1_svc(create_msub_input *in)
{
	static create_msub_output	out;

	int ret = the_inbound->create_msubspace(in->msid,
					        in->offset,
					        in->req_bytes,
					        &out.bytes,
                                                &out.msubid,
					        &out.rio_addr,
						&out.phys_addr);
	out.status = (ret > 0) ? 0 : ret;

	DBG("msubid=0x%X, bytes=%d, rio_addr = 0x%lX\n",
					out.msubid, out.bytes, out.rio_addr);
	return &out;
} /* create_msub_1_svc() */

destroy_msub_output *
destroy_msub_1_svc(destroy_msub_input *in)
{
	static destroy_msub_output	out;

	int ret = the_inbound->destroy_msubspace(in->msid, in->msubid);

	out.status = (ret > 0) ? 0 : ret;

	return &out;
} /* destroy_msub_1_svc() */

accept_output *
accept_1_svc(accept_input *in)
{
	static accept_output	out;

	DBG("ENTER with msname = %s\n", in->loc_ms_name);

	/* Does it exist? */
	mspace *ms = the_inbound->get_mspace(in->loc_ms_name);
	if (!ms) {
		WARN("%s does not exist\n", in->loc_ms_name);
		out.status = -1;
		return &out;
	}

	/* Prevent concurrent accept() calls to the same ms from different
	 * applications. */
	if (ms->is_accepted()) {
		ERR("%s already in accept() or connected.\n", in->loc_ms_name);
		out.status = -2;
		return &out;
	}
	ms->set_accepted(true);

	/* Get the memory space name, and prepend '/' to make it a queue */
	string	s(in->loc_ms_name);
	s.insert(0, 1, '/');

	/* Prepare accept message from input parameters */
	struct cm_accept_msg	cmam;
	cmam.type		= CM_ACCEPT_MS;
	strcpy(cmam.server_ms_name, in->loc_ms_name);
	cmam.server_msid	= ms->get_msid();
	cmam.server_msubid	= in->loc_msubid;
	cmam.server_bytes	= in->loc_bytes;
	cmam.server_rio_addr_len= in->loc_rio_addr_len;
	cmam.server_rio_addr_lo	= in->loc_rio_addr_lo;
	cmam.server_rio_addr_hi	= in->loc_rio_addr_hi;
	cmam.server_destid_len	= 16;
	cmam.server_destid	= peer.destid;
	DBG("cm_accept_msg has server_destid = 0x%X\n", cmam.server_destid);
	DBG("cm_accept_msg has server_destid_len = 0x%X\n", cmam.server_destid_len);

	/* Add accept message content to map indexed by message queue name */
	DBG("Adding entry in accept_msg_map for '%s'\n", s.c_str());
	accept_msg_map.add(s, cmam);

	out.status = 0;

	return &out;
} /* accept_1_svc() */

undo_accept_output *
undo_accept_1_svc(undo_accept_input *in)
{
	static undo_accept_output out;

	DBG("ENTER with msname = %s\n", in->server_ms_name);

	/* Does it exist? */
	mspace *ms = the_inbound->get_mspace(in->server_ms_name);
	if (!ms) {
		WARN("%s does not exist\n", in->server_ms_name);
		out.status = -1;
		return &out;
	}

	/* An accept() must be in effect to undo it. Double-check */
	if (!ms->is_accepted()) {
		ERR("%s NOT in accept().\n", in->server_ms_name);
		out.status = -2;
		return &out;
	}
	/* Get the memory space name, and prepend '/' to make it a queue */
	string	s(in->server_ms_name);
	s.insert(0, 1, '/');

	/* Remove accept message content from map indexed by message queue name */
	accept_msg_map.remove(s);

	/* TODO: How about if it is connected, but the server doesn't
	 * get the notification. If it is connected it cannot do undo_accept
	 * so let's think about that. */

	/* Now set it as unaccepted */
	ms->set_accepted(false);

	return &out;
} /* undo_accept_1_svc() */

send_connect_output *
send_connect_1_svc(send_connect_input *in)
{
	static send_connect_output out;

	/* Do we have an entry for that destid ? */
	sem_wait(&hello_daemon_info_list_sem);
	auto it = find(begin(hello_daemon_info_list),
		       end(hello_daemon_info_list),
		       in->server_destid);

	/* If the server's destid is not found, just fail */
	if (it == end(hello_daemon_info_list)) {
		ERR("destid(0x%X) was not provisioned\n", in->server_destid);
		sem_post(&hello_daemon_info_list_sem);
		out.status = -1;
		return &out;
	}
	sem_post(&hello_daemon_info_list_sem);

	/* Obtain pointer to socket object already connected to destid */
	cm_client *main_client = it->client;

	/* Obtain and flush send buffer for sending CM_CONNECT_MS message */
	cm_connect_msg *c;
	main_client->get_send_buffer((void **)&c);
	main_client->flush_send_buffer();

	/* Compose CONNECT_MS message */
	c->type			= CM_CONNECT_MS;
	strcpy(c->server_msname, in->server_msname);
	c->client_msid		= in->client_msid;
	c->client_msubid	= in->client_msubid;
	c->client_bytes		= in->client_bytes;
	c->client_rio_addr_len	= in->client_rio_addr_len;
	c->client_rio_addr_lo	= in->client_rio_addr_lo;
	c->client_rio_addr_hi	= in->client_rio_addr_hi;
	c->client_destid_len	= peer.destid_len;
	c->client_destid	= peer.destid;

	/* Send buffer to server */
	if (main_client->send()) {
		ERR("Failed to send CONNECT_MS to destid(0x%X)\n",
							in->server_destid);
		out.status = -3;
		return &out;
	}
	INFO("cm_connect_msg sent to remote daemon\n");

	/* Add POSIX message queue name to list of queue names */
	string	mq_name(in->server_msname);
	mq_name.insert(0, 1, '/');

	/* Add to list of message queue names awaiting an 'accept' to 'connect' */
	wait_accept_mq_names.push_back(mq_name);

	out.status = 0;

	DBG("EXIT\n");
	return &out;

} /* send_connect_1_svc() */

undo_connect_output *
undo_connect_1_svc(undo_connect_input *in)
{
	static undo_connect_output out;

	/* Add POSIX message queue name to list of queue names */
	string	mq_name(in->server_ms_name);
	mq_name.insert(0, 1, '/');

	/* Remove from list of mq names awaiting an 'accept' reply to 'connect' */
	wait_accept_mq_names.remove(mq_name);

	return &out;
} /* undo_connect_1_svc() */

send_disconnect_output *
send_disconnect_1_svc(send_disconnect_input *in)
{
	static send_disconnect_output out;

	out.status = 0;

	DBG("Client to disconnect from destid = 0x%X\n", in->rem_destid);
	/* Do we have an entry for that destid ? */
	sem_wait(&hello_daemon_info_list_sem);
	auto it = find(begin(hello_daemon_info_list),
		       end(hello_daemon_info_list),
		       in->rem_destid);

	/* If the server's destid is not found, just fail */
	if (it == end(hello_daemon_info_list)) {
		ERR("destid(0x%X) was not provisioned\n", in->rem_destid);
		sem_post(&hello_daemon_info_list_sem);
		out.status = -1;
		return &out;
	}
	sem_post(&hello_daemon_info_list_sem);

	/* Obtain pointer to socket object already connected to destid */
	cm_client *the_client = it->client;

	cm_disconnect_msg *disc_msg;

	/* Get and flush send buffer */
	the_client->flush_send_buffer();
	the_client->get_send_buffer((void **)&disc_msg);

	disc_msg->type		= CM_DISCONNECT_MS;
	disc_msg->client_msubid	= in->loc_msubid;	/* For removal from server database */
	disc_msg->server_msid    = in->rem_msid;	/* For removing client's destid from server's
							 * info on the daemon */
	disc_msg->client_destid = peer.destid;		/* For knowing which destid to remove */
	disc_msg->client_destid_len = 16;

	/* Send buffer to server */
	if (the_client->send()) {
		out.status = -1;
		return &out;
	}
	DBG("Sent DISCONNECT_MS for msid = 0x%lX, client_destid = 0x%lX\n",
				disc_msg->server_msid, disc_msg->client_destid);

	return &out;
} /* send_disconnect_1_svc() */

