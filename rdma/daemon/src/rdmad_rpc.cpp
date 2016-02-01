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
		rc = -1;
	} else  if (ms == nullptr) {
		ERR("ms is null for '%s'\n", ms_name);
		rc = -2;
	} else {
		/* Obtain assigned physical & RIO addresses (which would be the
		 * same if we are using direct mapping). */
		*phys_addr = ms->get_phys_addr();
		*rio_addr  = ms->get_rio_addr();

		DBG("the_inbound->create_mspace(%s) %s\n", ms_name,
						rc ? "FAILED" : "PASSED");

		/* Add the memory space to the owner (if no errors) */
		try {
			owners[msoid]->add_ms(ms);
			rc = 0;
		}
		catch(...) {
			ERR("Invalid msoid(0x%X) caused segfault\n", msoid);
			rc = -3;
		}
	}
	return rc;
} /* rdmad_create_ms() */

int rdmad_close_ms(uint32_t msid, tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	int rc;
	mspace *ms = the_inbound->get_mspace(msid);
	if (ms == nullptr) {
		ERR("Could not find mspace with msid(0x%X)\n", msid);
		rc = -1;
	} else {
		/* Now close the memory space */
		rc = ms->close(app_tx_eng);
	}
	return rc;
} /* rdmad_close_ms() */

int rdmad_accept_ms(uint32_t server_msid, uint32_t server_msubid,
		    tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	int rc;

	mspace *ms = the_inbound->get_mspace(server_msid);
	if (!ms) {
		WARN("ms with msid(0x%X) does not exist\n", server_msid);
		rc =  RDMA_INVALID_MS;
	} else {
		rc = ms->accept(to_lib_tx_eng, server_msubid);
	}

	return rc;
} /* rdmad_accept_ms() */


int rdmad_undo_accept_ms(uint32_t server_msid,
		    tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	int rc;

	mspace *ms = the_inbound->get_mspace(server_msid);
	if (!ms) {
		WARN("ms with msid(0x%X) does not exist\n", server_msid);
		rc = RDMA_INVALID_MS;
	} else {
		rc = ms->undo_accept(to_lib_tx_eng);
	}

	return rc;
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
	DBG("ENTER\n");
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
	c->client_to_lib_tx_eng_h = htobe64((uint64_t)to_lib_tx_eng);

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

	/* Record the connection request in the list so when the remote
	 * daemon sends back an ACCEPT_MS, we can match things up. */
	sem_wait(&connected_to_ms_info_list_sem);
	connected_to_ms_info_list.emplace_back(client_msubid,
					       server_ms_name,
					       server_destid,
					       to_lib_tx_eng);
	sem_post(&connected_to_ms_info_list_sem);
	DBG("EXIT\n");
	return 0;
} /* rdmad_send_connect() */

int rdmad_undo_connect(const char *server_ms_name,
	        tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	int rc;

	/* Remove from the connected_to_ms_info_list. Must match
	 * on BOTH the server_ms_name and (client) to_lib_tx_eng */
	sem_wait(&connected_to_ms_info_list_sem);
	auto it = find_if(begin(connected_to_ms_info_list),
		       end(connected_to_ms_info_list),
		       [server_ms_name,to_lib_tx_eng](connected_to_ms_info& info)
		       {
				return (info.server_msname == server_ms_name)
					&& info.to_lib_tx_eng == to_lib_tx_eng;
		       });
	if (it == end(connected_to_ms_info_list)) {
		WARN("Could not find '%s' in connected_to_ms_info_list\n",
							server_ms_name);
		rc = -1;
	} else {
		DBG("Removing '%s' from connect_to_ms_info_list\n",
							server_ms_name);
		connected_to_ms_info_list.erase(it);
		rc = 0;
	}
	sem_post(&connected_to_ms_info_list_sem);

	return rc;
} /* rdmad_undo_connect() */

/**
 * 1. Calls send_disc_ms_cm() which sends a CM_DISCONNECT_MS to the remote daemon
 * 2. Removes the entry corresponding to this connection from
 * 'connected_to_ms_info_list'
 *
 * NOTE: If we fail to send the disconnection, we don't remove the item
 * from the connected_to_ms_info list to avoid inconsistency.
 */
int rdmad_send_disconnect(uint32_t server_destid,
			  uint32_t server_msid,
			  uint32_t client_msubid,
			  tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng)
{
	int rc = send_disc_ms_cm(server_destid,
			         server_msid,
			         client_msubid,
			         (uint64_t)to_lib_tx_eng);
	if (rc) {
		ERR("Failed to send disconnection to server daemon\n");
	} else {
		sem_wait(&connected_to_ms_info_list_sem);

		/* Find the appropriate entry in connect_to_ms_info_list and erase */
		auto it = find_if(begin(connected_to_ms_info_list),
			  end(connected_to_ms_info_list),
		  [&](connected_to_ms_info& info) {
			bool match = info.server_msid == server_msid;

			match &= info.connected;
			match &= info.client_msubid == client_msubid;
			match &= info.to_lib_tx_eng == to_lib_tx_eng;
			return match;
		  });
		if (it == end(connected_to_ms_info_list)) {
			ERR("Could not find entry for specified ms connection\n");
			rc = -1;
		} else {
			connected_to_ms_info_list.erase(it);
			rc = 0;
		}
		sem_post(&connected_to_ms_info_list_sem);
	}

	return rc;
} /* rdmad_send_disconnect() */
