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
#include <stdint.h>

#include "unix_sock.h"
#include "rdmad_main.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_clnt_threads.h"

int rdmad_destroy_mso(uint32_t msoid)
{
	int	rc;

	ms_owner *owner;

	try {
		owner = owners[msoid];
		if (owner==nullptr) {
			ERR("Invalid msoid(0x%X)\n", msoid);
			rc = -1;
		} else if (owner->owns_mspaces()) {
			WARN("msoid(0x%X) owns spaces!\n", msoid);
			rc = -2;
		} else {
			/* No memory spaces owned by mso, just destroy it */
			int ret = owners.destroy_mso(msoid);
			rc = (ret > 0) ? 0 : ret;
			DBG("owners.destroy_mso() %s\n", rc ? "FAILED":"PASSED");
		}
	}
	catch(...) {
		ERR("Invalid msoid(0x%X) caused segfault\n", msoid);
		rc = -3;
	}

	return rc;
} /* rdmad_destroy_mso() */

int rdmad_create_ms(const char *ms_name, uint32_t bytes, uint32_t msoid,
			uint32_t *msid, uint64_t *phys_addr,
			uint64_t *rio_addr,
			tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	int rc;

	/* Create memory space in the inbound space */
	mspace *ms = nullptr;
	rc = the_inbound->create_mspace(
			ms_name, bytes, msoid, msid, &ms, to_lib_tx_eng);
	if (rc != 0) {
		ERR("Failed to create '%s'\n", ms_name);
		return -1;
	}

	/* Check that ms is not NULL */
	if (ms == nullptr) {
		ERR("ms is null for '%s'\n", ms_name);
		return -2;
	}

	/* Obtain assigned physical & RIO addresses (which would be the
	 * same if we are using direct mapping). */
	*phys_addr = ms->get_phys_addr();
	*rio_addr  = ms->get_rio_addr();

	DBG("the_inbound->create_mspace(%s) %s\n", ms_name,
						rc ? "FAILED" : "PASSED");

	/* Add the memory space to the owner (if no errors) */
	try {
		owners[msoid]->add_ms(ms);
	}
	catch(...) {
		ERR("Invalid msoid(0x%X) caused segfault\n", msoid);
		rc = -3;
	}
	return rc;
} /* rdmad_create_ms() */

int rdmad_close_ms(uint32_t msid, uint32_t ms_conn_id)
{
	mspace *ms = the_inbound->get_mspace(msid);
	if (ms == nullptr) {
		ERR("Could not find mspace with msid(0x%X)\n", msid);
		return -1;
	} else {
		/* Now close the memory space */
		return ms->close(ms_conn_id);
	}
} /* rdmad_close_ms() */

int rdmad_accept_ms(uint32_t server_msid,
		    tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	auto rc = 0;

	mspace *ms = the_inbound->get_mspace(server_msid);
	if (!ms) {
		WARN("ms with msid(0x%X) does not exist\n", server_msid);
		rc =  RDMA_INVALID_MS;
	} else {
		rc = ms->accept(to_lib_tx_eng);
	}
#if 0
	/* Get the memory space name, and prepend  '/' to make it a queue */
	string	s(loc_ms_name);
	s.insert(0, 1, '/');

	/* Prepare accept message from input parameters */
	struct cm_accept_msg	cmam;
	cmam.type		= htobe64(CM_ACCEPT_MS);
	strcpy(cmam.server_ms_name, loc_ms_name);
	cmam.server_msid	= htobe64(ms->get_msid());
	cmam.server_msubid	= htobe64(loc_msubid);
	cmam.server_bytes	= htobe64(loc_bytes);
	cmam.server_rio_addr_len= htobe64(loc_rio_addr_len);
	cmam.server_rio_addr_lo	= htobe64(loc_rio_addr_lo);
	cmam.server_rio_addr_hi	= htobe64(loc_rio_addr_hi);
	cmam.server_destid_len	= htobe64(16);
	cmam.server_destid	= htobe64(peer.destid);
	DBG("cm_accept_msg has server_destid = 0x%X\n",
						be64toh(cmam.server_destid));
	DBG("cm_accept_msg has server_destid_len = 0x%X\n",
					be64toh(cmam.server_destid_len));

	/* Add accept message content to map indexed by message queue name */
	if (accept_msg_map.contains(s)) {
		CRIT("%s is already in accept_msg_map\n");
		return -1;
	} else {
		DBG("Adding entry in accept_msg_map for '%s'\n", s.c_str());
		accept_msg_map.add(s, cmam);
	}
#endif
	return rc;
} /* rdmad_accept_ms() */


int rdmad_undo_accept_ms(uint32_t server_msid,
		    tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	auto rc = 0;

	mspace *ms = the_inbound->get_mspace(server_msid);
	if (!ms) {
		WARN("ms with msid(0x%X) does not exist\n", server_msid);
		rc = RDMA_INVALID_MS;
	} else {
		rc = ms->undo_accept(to_lib_tx_eng);
	}

	return rc;
#if 0
	/* An accept() must be in effect to undo it. Double-check */
	if (!ms->is_accepting()) {
		ERR("%s NOT in accept().\n", ms_name);
		return -2;
	}

	/* Get the memory space name, and prepend '/' to make it a queue */
	string	s(ms_name);
	s.insert(0, 1, '/');

	/* Remove accept message content from map indexed by message queue name */
	accept_msg_map.remove(s);

	/* TODO: How about if it is connected, but the server doesn't
	 * get the notification. If it is connected it cannot do undo_accept
	 * so let's think about that. */

	/* Now set it as unaccepted */
	ms->set_accepting(false);
#endif
} /* rdmad_undo_accept_ms() */

int rdmad_send_connect(const char *server_ms_name,
			uint32_t server_destid,
			uint32_t client_msid,
		        uint32_t client_msubid,
		        uint32_t client_bytes,
		        uint8_t client_rio_addr_len,
		        uint64_t client_rio_addr_lo,
		        uint8_t client_rio_addr_hi,
		        uint64_t seq_num,
		        tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	/* Do we have an entry for that destid ? */
	sem_wait(&hello_daemon_info_list_sem);
	auto it = find(begin(hello_daemon_info_list),
		       end(hello_daemon_info_list),
			server_destid);

	/* If the server's destid is not found, just fail */
	if (it == end(hello_daemon_info_list)) {
		ERR("destid(0x%X) was not provisioned\n", server_destid);
		sem_post(&hello_daemon_info_list_sem);
		return RDMA_REMOTE_UNREACHABLE;
	}
	sem_post(&hello_daemon_info_list_sem);

	/* Obtain pointer to socket object already connected to destid */
	cm_client *main_client = it->client;

	/* Obtain and flush send buffer for sending CM_CONNECT_MS message */
	cm_connect_msg *c;

	main_client->get_send_buffer((void **)&c);
	main_client->flush_send_buffer();

	/* Compose CONNECT_MS message */
	c->type			= htobe64(CM_CONNECT_MS);
	strcpy(c->server_msname, server_ms_name);
	c->client_msid		= htobe64((uint64_t)client_msid);
	c->client_msubid	= htobe64((uint64_t)client_msubid);
	c->client_bytes		= htobe64((uint64_t)client_bytes);
	c->client_rio_addr_len	= htobe64((uint64_t)client_rio_addr_len);
	c->client_rio_addr_lo	= htobe64(client_rio_addr_lo);
	c->client_rio_addr_hi	= htobe64((uint64_t)client_rio_addr_hi);
	c->client_destid_len	= htobe64(peer.destid_len);
	c->client_destid	= htobe64(peer.destid);
	c->seq_num		= htobe64((uint64_t)seq_num);

	DBG("WITHOUT CONVERSION:\n");
	DBG("c->type = 0x%016" PRIx64 "\n", c->type);
	DBG("c->server_msname = %s\n", c->server_msname);
	DBG("c->client_msid   = 0x%016" PRIx64 "\n", c->client_msid);
	DBG("c->client_msubid   = 0x%016" PRIx64 "\n", c->client_msubid);
	DBG("c->client_bytes   = 0x%016" PRIx64 "\n", c->client_bytes);
	DBG("c->client_rio_addr_len = 0x%016" PRIx64 "\n", c->client_rio_addr_len);
	DBG("c->client_rio_addr_lo = 0x%016" PRIx64 "\n", c->client_rio_addr_lo);
	DBG("c->client_rio_addr_hi = 0x%016" PRIx64 "\n", c->client_rio_addr_hi);
	DBG("c->client_destid_len = 0x%016" PRIx64 "\n", c->client_destid_len);
	DBG("c->client_destid = 0x%016" PRIx64 "\n", c->client_destid);
	DBG("c->seq_num = 0x%016" PRIx64 "\n", c->seq_num);
	DBG("WITH CONVERSTION:\n");
	DBG("c->type = 0x%016" PRIx64 "\n", be64toh(c->type));
	DBG("c->server_msname = %s\n", c->server_msname);
	DBG("c->client_msid   = 0x%016" PRIx64 "\n", be64toh(c->client_msid));
	DBG("c->client_msubid   = 0x%016" PRIx64 "\n", be64toh(c->client_msubid));
	DBG("c->client_bytes   = 0x%016" PRIx64 "\n", be64toh(c->client_bytes));
	DBG("c->client_rio_addr_len = 0x%016" PRIx64 "\n", be64toh(c->client_rio_addr_len));
	DBG("c->client_rio_addr_lo = 0x%016" PRIx64 "\n", be64toh(c->client_rio_addr_lo));
	DBG("c->client_rio_addr_hi = 0x%016" PRIx64 "\n", be64toh(c->client_rio_addr_hi));
	DBG("c->client_destid_len = 0x%016" PRIx64 "\n", be64toh(c->client_destid_len));
	DBG("c->client_destid = 0x%016" PRIx64 "\n", be64toh(c->client_destid));
	DBG("c->seq_num = 0x%016" PRIx64 "\n", be64toh(c->seq_num));
	main_client->dump_send_buffer();

	/* Send buffer to server */
	if (main_client->send()) {
		ERR("Failed to send CONNECT_MS to destid(0x%X)\n", server_destid);
		return -3;
	}
	INFO("cm_connect_msg sent to remote daemon\n");

	/* Add POSIX message queue name to list of queue names */
	string	mq_name(server_ms_name);
	mq_name.insert(0, 1, '/');

	/* Add to list of message queue names awaiting an 'accept' to 'connect' */
	if (wait_accept_mq_names.contains(mq_name)) {
		/* This would happen if we are retrying ... */
		WARN("'%s' already in wait_accept_mq_names\n", mq_name.c_str());
	} else {
		wait_accept_mq_names.push_back(mq_name);
		/* FIXME: This is not the best place for this. It really should be when
		 * we get the CM_ACCEPT_MS message, but the problem is that the CM_ACCEPT_MS
		 * message does not contain the client_msubid.
		 */
		/* Also add to the connected_to_ms_info_list */
		sem_wait(&connected_to_ms_info_list_sem);
		connected_to_ms_info_list.emplace_back(client_msubid,
						       server_ms_name,
						       server_destid,
						       to_lib_tx_eng);
		sem_post(&connected_to_ms_info_list_sem);
	}

	return 0;
} /* rdmad_send_connect() */

int rdmad_undo_connect(const char *server_ms_name)
{
	/* Add POSIX message queue name to list of queue names */
	string	mq_name(server_ms_name);
	mq_name.insert(0, 1, '/');

	/* Remove from list of mq names awaiting an 'accept' reply to 'connect' */
	wait_accept_mq_names.remove(mq_name);

	/* Also remove from the connected_to_ms_info_list */
	sem_wait(&connected_to_ms_info_list_sem);
	auto it = find(begin(connected_to_ms_info_list),
		       end(connected_to_ms_info_list),
		       server_ms_name);
	if (it == end(connected_to_ms_info_list)) {
		WARN("Could not find '%s' in connected_to_ms_info_list\n",
							server_ms_name);
	} else {
		DBG("Removing '%s' from connect_to_ms_info_list\n",
							server_ms_name);
		connected_to_ms_info_list.erase(it);
	}
	sem_post(&connected_to_ms_info_list_sem);

	return 0;
} /* rdmad_undo_connect() */

int rdmad_send_disconnect(uint32_t rem_destid,
			  uint32_t rem_msid,
			  uint32_t loc_msubid)
{
	int rc = send_disc_ms_cm(rem_destid, rem_msid, loc_msubid);

	/* Remove from the connected_to_ms_info_list */
	sem_wait(&connected_to_ms_info_list_sem);
	vector<connected_to_ms_info>::iterator it;
	for (it = begin(connected_to_ms_info_list);
		  it != end(connected_to_ms_info_list);
		  it++) {
		if (it->server_msid == rem_msid) {
			DBG("Found msid(0x%X)\n", rem_msid);
			break;
		}
	}
	if (it != end(connected_to_ms_info_list)) {
		DBG("Removing msid(0x%X) from connected list\n", rem_msid);
		connected_to_ms_info_list.erase(it);
	} else {
		DBG("msid(0x%X) NOT FOUND\n", rem_msid);
	}
	sem_post(&connected_to_ms_info_list_sem);

	return rc;
} /* rdmad_send_disconnect() */
