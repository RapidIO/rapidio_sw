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
#include <errno.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>
#include <inttypes.h>

#include <algorithm>
#include <vector>

#include "cm_sock.h"
#include "liblog.h"
#include "rdma_types.h"
#include "rdmad_cm.h"
#include "rdmad_ms_owners.h"
#include "rdmad_mspace.h"
#include "rdmad_msubspace.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_main.h"
#include "rdmad_unix_msg.h"

using std::fill;
using std::find;
using std::remove;
using std::lock_guard;
using std::unique_lock;

mspace::mspace(	const char *name, uint32_t msid, uint64_t rio_addr,
                uint64_t phys_addr, uint64_t size, ms_owners& owners) :
                		name(name),
                		msid(msid),
                		rio_addr(rio_addr),
                		phys_addr(phys_addr),
                		size(size),
                		msoid(0),
                		free(true),
                		owners(owners),
		                connected_to(false),
		                accepting(false),
		                server_msubid(0),
		                creator_tx_eng(nullptr),
		                client_destid(0xFFFF),
		                client_msubid(NULL_MSUBID),
		                client_to_lib_tx_eng_h(0)
{
	INFO("name=%s, msid=0x%08X, rio_addr=0x%" PRIX64 ", size=0x%" PRIX64 "\n",
						name, msid, rio_addr, size);

	/* Initially all free list sub-indexes are available */
	fill(msubindex_free_list, msubindex_free_list + MSUBINDEX_MAX + 1, true);
	msubindex_free_list[0] = false; /* Start at 1 */
} /* constructor */

mspace::~mspace()
{
	if (!free) {
		DBG("Destroying %s\n", name.c_str());
		destroy();
	}
} /* destructor */

void mspace::send_cm_force_disconnect_ms(tx_engine<cm_server, cm_msg_t>* tx_eng,
				uint32_t server_msubid,
				uint64_t client_to_lib_tx_eng_h)
{
	/* Prepare destroy message */
	auto the_msg = make_unique<cm_msg_t>();
	the_msg->type = htobe64(CM_FORCE_DISCONNECT_MS);
	the_msg->category = RDMA_REQ_RESP;
	the_msg->seq_no = 0;
	cm_force_disconnect_ms_msg	*dm = &the_msg->cm_force_disconnect_ms;
	strcpy(dm->server_msname, name.c_str());
	dm->server_msid = htobe64(msid);
	dm->server_msubid = htobe64(server_msubid);
	dm->client_to_lib_tx_eng_h = htobe64(client_to_lib_tx_eng_h);

	/* Send to remote daemon @ 'client_destid' */
	tx_eng->send_message(move(the_msg));
} /* send_cm_force_disconnect_ms() */

void mspace::send_cm_server_disconnect_ms(tx_engine<cm_server, cm_msg_t>* tx_eng,
				uint32_t server_msubid,
				uint64_t client_to_lib_tx_eng_h,
				uint64_t server_to_lib_tx_eng_h)
{
	/* Prepare destroy message */
	auto the_msg = make_unique<cm_msg_t>();
	the_msg->type = htobe64(CM_SERVER_DISCONNECT_MS);
	the_msg->category = RDMA_REQ_RESP;
	the_msg->seq_no = 0;
	cm_server_disconnect_ms_msg	*dm = &the_msg->cm_server_disconnect_ms;
	strcpy(dm->server_msname, name.c_str());
	dm->server_msid = htobe64(msid);
	dm->server_msubid = htobe64(server_msubid);
	dm->client_to_lib_tx_eng_h = htobe64(client_to_lib_tx_eng_h);
	dm->server_to_lib_tx_eng_h = htobe64(server_to_lib_tx_eng_h);

	/* Send to remote daemon @ 'client_destid' */
	tx_eng->send_message(move(the_msg));
} /* send_cm_force_disconnect_ms() */

int mspace::send_disconnect_to_remote_daemon(uint32_t client_msubid,
				     uint64_t client_to_lib_tx_eng_h,
				     uint64_t server_to_lib_tx_eng_h)
{
	tx_engine<cm_server, cm_msg_t>* tx_eng = nullptr;

	DBG("ENTER with client_msubid=0x%X, client_to_lib_tx_eng=0x%"
			PRIx64 "\n", client_msubid, client_to_lib_tx_eng_h);

	/* If the mspace creator matches on the client_msubid,
	 * client_to_lib_tx_eng_h (connh) and is 'connected_to', then
	 * look up its destid and send the force disconnect message. */
	if (connected_to &&
	   (this->client_msubid == client_msubid) &&
	   (this->client_to_lib_tx_eng_h == client_to_lib_tx_eng_h)) {
		tx_eng =
		     prov_daemon_info_list.get_tx_eng_by_destid(client_destid);
	} else {
		/* If the mspace creator does NOT match on the client_msubid,
		 * client_to_lib_tx_eng_h (connh) or is NOT 'connected_to', then
		 * search all the users for a match and, for the matched user
		 * look up its destid and send the force disconnect message. */
		auto it = find_if(begin(users), end(users),
		[client_to_lib_tx_eng_h, client_msubid](ms_user& u) {
			DBG("u.client_msubid=0x%X, u.client_to_lib_tx_eng=0x%"
				PRIx64 "\n",
				u.client_msubid, u.client_to_lib_tx_eng_h);
			return u.client_to_lib_tx_eng_h == client_to_lib_tx_eng_h
			    && u.client_msubid == client_msubid
			    && u.connected_to;
		});
		if (it == end(users)) {
			CRIT("No user matches specified parameter(s)\n");
			CRIT("client_msubid=0x%X, client_to_lib_tx_eng=0x%"
				PRIx64 "\n", client_msubid, client_to_lib_tx_eng_h);
		} else {
			/* Now find the tx_eng to use by looking up the
			 * user's destid in the prov_daemon_info_list */
			tx_eng = prov_daemon_info_list.get_tx_eng_by_destid(
					     	     	    it->client_destid);
		}
	}

	/* Could we find a tx engine for the specified destid? */
	if (tx_eng != nullptr) {
		/* Yes, use it to send the server disconnect message */
		send_cm_server_disconnect_ms(tx_eng,
					server_msubid,
					client_to_lib_tx_eng_h,
					server_to_lib_tx_eng_h);
		DBG("Sent force disconnect to remote daemon\n");
		return 0;
	} else {
		ERR("Failed to obtain a tx_eng\n");
		return  -1;
	}
} /* send_disconnect_to_remote_daemon() */

void mspace::notify_remote_clients()
{
	DBG("ENTER\n");
	if (connected_to) {
		/* Need to use the tx engine created on provisioning via
		 * incoming HELLO (prov_daemon_info_list), to send the
		 * force-disconnect message.
		 */
		tx_engine<cm_server, cm_msg_t>* tx_eng =
				prov_daemon_info_list.get_tx_eng_by_destid(
						client_destid);

		if (tx_eng) {
			send_cm_force_disconnect_ms(tx_eng, server_msubid,
					client_to_lib_tx_eng_h);
		}
	} else {
		/* It is not the creator who has a connection; search users */
		DBG("Searching users...\n");
		/* If called during shutdown, don't block if mutex already locked */
		bool locked;
		if (shutting_down) {
			if (users_mutex.try_lock()) {
				locked = true;
				DBG("Locked users_mutex\n");
			} else {
				DBG("users_mutex already locked\n");
				locked = false;
			}
		} else {
			users_mutex.lock();
			DBG("Locked users_mutex\n");
			locked = true;
		}
		for(auto& u : users) {
			/* If the entry is not 'connected_to' then skip it */
			if (!u.connected_to)
				continue;

			/* Need to use the tx engine created on provisioning via
			 * incoming HELLO (prov_daemon_info_list), to send the
			 * force-disconnect message.
			 */
			tx_engine<cm_server, cm_msg_t>* tx_eng =
				prov_daemon_info_list.get_tx_eng_by_destid(
							u.client_destid);

			send_cm_force_disconnect_ms(tx_eng,
						    u.server_msubid,
						    u.client_to_lib_tx_eng_h);
		}
		if (locked) {
			users_mutex.unlock();
			DBG("Unlocked users_mutex\n");
		}
	}
	DBG("EXIT\n");
} /* notify_remote_clients() */

void mspace::close_connections()
{
	lock_guard<mutex> users_lock(users_mutex);

	HIGH("FORCE_CLOSE_MS to apps which have 'open'ed '%s'\n", name.c_str());

	/* Tell local apps which have opened the ms that the ms will be destroyed */
	for (ms_user& user : users) {
		auto in_msg = make_unique<unix_msg_t>();

		in_msg->type 	= FORCE_CLOSE_MS;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->seq_no   = 0;
		in_msg->force_close_ms_req.msid = msid;
		user.tx_eng->send_message(move(in_msg));
	}

	users.clear();
	DBG("All users for '%s' now cleared\n", name.c_str());
} /* close_connections() */

int mspace::destroy()
{
	DBG("name=%s, msid=0x%08X, rio_addr=0x%" PRIx64 ", size=0x%X\n", name.c_str(),
	                msid, rio_addr, size);

	/* Before destroying a memory space, tell its clients that
	 * it is being destroyed and have them acknowledge that.
	 * Then remove their destids */
	notify_remote_clients();

	/* Close connections from other local 'user' applications and
	 * delete message queues used to communicate with those apps. */
	close_connections();

	/* Remove all subspaces; they can't exist when the memory space
	 * is  marked as free. */
	INFO("Destroying all subspaces in '%s'\n", name.c_str());
	unique_lock<mutex> msubspaces_lock(msubspaces_mutex);
	msubspaces.clear();
	msubspaces_lock.unlock();

	/* Mark the memory space as free, and having no owner */
	free = true;
	name = "freemspace";
	connected_to = false; /* No connections */
	accepting = false;    /* Not accepting connections either */
	creator_tx_eng = nullptr;	/* No tx_eng associated therewith */
	client_destid = 0xFFFF;
	client_msubid = NULL_MSUBID;
	client_to_lib_tx_eng_h = 0;

	/* Remove from owners then clear the owner.
	 * No free space should have an owner */
	auto mso = owners[msoid];
	if (mso == nullptr) {
		ERR("Failed to find owner msoid(0x%X)\n", msoid);
		return RDMA_INVALID_MSO;
	}
	auto rc = mso->remove_ms(this);
	if (rc) {
		WARN("Failed to remove ms from owner\n");
		return rc;
	}

	/* No owner */
	set_msoid(0);

	return 0;
} /* destroy() */

int mspace::add_rem_connection(uint16_t client_destid,
			        uint32_t client_msubid,
			        uint64_t client_to_lib_tx_eng_h)
{
	DBG("Adding destid(0x%X), msubid(0x%X),	client_to_lib_tx_eng_h(0x%"
			PRIx64 ")to '%s'\n", client_destid,
			client_msubid, client_to_lib_tx_eng_h, name.c_str());

	/* We are accepting by the owner then this is where the remote
	 * connection belongs. */
	if (accepting) {
		DBG("Adding to creator since it is 'accepting'\n");
		this->client_destid = client_destid;
		this->client_msubid = client_msubid;
		this->client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;
		this->accepting = false;
		this->connected_to = true;
		return 0;
	}

	/* We were NOT accepting by the owner, so check the users */
	lock_guard<mutex> users_lock(users_mutex);
	DBG("The creator was not accepting..checking the users\n");
	DBG("There are %zu user(s)\n", users.size());
	auto it = find_if(begin(users), end(users), [](ms_user& u)
						{return u.accepting;});
	if (it == end(users)) {
		CRIT("Failed to find a user in 'accepting' mode.\n");
		return -1;
	}

	DBG("Adding rem connection to user whose tx_eng = 0x%" PRIx64 "\n",
							(uint64_t)it->tx_eng);
	it->client_destid = client_destid;
	it->client_msubid = client_msubid;
	it->client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;
	it->accepting = false;
	it->connected_to = true;
	return 0;
} /* add_rem_connection() */

int mspace::remove_rem_connection(uint16_t client_destid,
				  uint32_t client_msubid,
				  uint64_t client_to_lib_tx_eng_h)
{
	DBG("Removing client_destid(0x%X), client_msubid(0x%X) from '%s'\n",
			client_destid, client_msubid, name.c_str());
	DBG("client_to_lib_tx_eng_h = 0x%" PRIx64 "\n", client_to_lib_tx_eng_h);

	DBG("this->client_destid = 0x%X, this->client_msubid = 0x%X\n",
				this->client_destid, this->client_msubid);
	DBG("this->client_to_lib_tx_eng_h = 0x%" PRIx64 "\n",
						this->client_to_lib_tx_eng_h);

	/* First check to see if the connection belongs to the creator */
	if ((this->client_destid == client_destid) &&
	    (this->client_msubid == client_msubid) &&
	    (this->client_to_lib_tx_eng_h == client_to_lib_tx_eng_h)) {
		INFO("Found connection info in creator of '%s'\n", name.c_str());
		this->client_destid = 0xFFFF;
		this->client_msubid = NULL_MSUBID;
		this->client_to_lib_tx_eng_h = 0;
		this->server_msubid = 0;
		connected_to = false;
		return 0;
	}

	/* It is possibly in one of the users so search for it */
	DBG("Not found in the creator; checking the users\n");
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find_if(begin(users), end(users),
	[client_destid, client_msubid, client_to_lib_tx_eng_h](ms_user& u)
	{
		return (u.client_destid == client_destid) &&
		       (u.client_msubid == client_msubid) &&
		       (u.client_to_lib_tx_eng_h == client_to_lib_tx_eng_h);
	});

	if (it == end(users)) {
		ERR("Failed to find remote connection in %s\n", name.c_str());
		return -1;
	}

	it->client_destid = 0xFFFF;
	it->client_msubid = NULL_MSUBID;
	it->client_to_lib_tx_eng_h = 0;
	it->connected_to = false;
	it->server_msubid = 0;
	it->connected_to = false;
	return 0;
} /* remove_rem_connection() */

void mspace::send_disconnect_to_lib(uint32_t client_msubid,
		uint32_t server_msubid,
		uint64_t client_to_lib_tx_eng_h,
		tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	auto in_msg = make_unique<unix_msg_t>();

	in_msg->category = RDMA_REQ_RESP;
	in_msg->type	= DISCONNECT_MS;
	in_msg->disconnect_from_ms_in.client_msubid = client_msubid;
	in_msg->disconnect_from_ms_in.server_msubid = server_msubid;
	in_msg->disconnect_from_ms_in.client_to_lib_tx_eng_h =
							client_to_lib_tx_eng_h;
	tx_eng->send_message(move(in_msg));
} /* send_disconnect_to_lib() */

/* Disconnect all connections from the specified client_destid */
void mspace::disconnect_from_destid(uint16_t client_destid)
{
	if ((this->client_destid == client_destid) && connected_to) {
		send_disconnect_to_lib(client_msubid,
				       server_msubid,
				       client_to_lib_tx_eng_h,
				       creator_tx_eng);
		INFO("%s disconnected from destid(0x%X)\n",
						name.c_str(), client_destid);
		this->client_destid = 0xFFFF;
		this->client_msubid = NULL_MSUBID;
		this->client_to_lib_tx_eng_h = 0;
		this->server_msubid = 0;
		this->connected_to = false;
	} else {
		/* Search all users looking for ones that are connected
		 * to the specified destid. There can be more than one
		 * user who have accepted connections on this memory space
		 * but from different clients on the same remote node. */
		lock_guard<mutex> users_lock(users_mutex);
		for (auto& u : users) {
			if ((u.client_destid == client_destid) && u.connected_to) {
				send_disconnect_to_lib(u.client_msubid,
						       u.server_msubid,
						       u.client_to_lib_tx_eng_h,
						       u.tx_eng);
				INFO("%s disconnected from destid(0x%X)\n",
						name.c_str(), u.client_destid);
				u.client_destid = 0xFFFF;
				u.client_msubid = NULL_MSUBID;
				u.client_to_lib_tx_eng_h = 0;
				u.server_msubid = 0;
				u.connected_to = false;
			}
		}
	}
} /* disconnect_from_destid() */

int mspace::disconnect(bool is_client,
		       uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h,
		       uint64_t server_to_lib_tx_eng_h)
{
	int rc;
	int ret = 0;

	bool found = (this->client_msubid == client_msubid) &&
		     (this->client_to_lib_tx_eng_h == client_to_lib_tx_eng_h) &&
		     connected_to;
	if (found) {
		DBG("Creator has the connection\n");
		if (is_client) {
			/* This was client-initiated. Need to relay this to
			 * the server library to clear connection info there. */
			send_disconnect_to_lib(client_msubid,
					       server_msubid,
					       client_to_lib_tx_eng_h,
					       creator_tx_eng);
			rc = 0;
		} else {
			/* Server-initiated: The client needs to be told via
			 * its daemon, that this connection is now invalid. */
			rc = send_disconnect_to_remote_daemon(client_msubid,
						client_to_lib_tx_eng_h,
						server_to_lib_tx_eng_h);
		}
		this->client_destid = 0xFFFF;
		this->client_msubid = NULL_MSUBID;
		this->client_to_lib_tx_eng_h = 0;
		this->connected_to = false;
	} else {
		lock_guard<mutex> users_lock(users_mutex);
		rc = 0;	/* For the case where there are NO users */
		for (auto& u : users) {
			found = (u.client_msubid == client_msubid) &&
				(u.client_to_lib_tx_eng_h == client_to_lib_tx_eng_h) &&
				 u.connected_to;
			if (found) {
				DBG("A user has the connection\n");
				if (is_client) {/* Disconnect by remote client */
					send_disconnect_to_lib(
						u.client_msubid,
						u.server_msubid,
						u.client_to_lib_tx_eng_h,
						u.tx_eng);
					rc = 0;
				} else { /* Self disconnection by local server */
					rc = send_disconnect_to_remote_daemon(
							client_msubid,
							client_to_lib_tx_eng_h,
							server_to_lib_tx_eng_h);
					if (rc) {
						ERR("Failed to send disconnect to remote daemon");
						ret = rc;
					}
				}
				u.client_destid = 0xFFFF;
				u.client_msubid = NULL_MSUBID;
				u.client_to_lib_tx_eng_h = 0;
				u.connected_to = false;
			}
		}
	}

	/* If we had failed during the loop, error status was saved in 'ret' */
	if (ret)
		rc = ret;

	return rc;
} /* disconnect() */

int mspace::client_disconnect(uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h)
{
	return disconnect(true, client_msubid, client_to_lib_tx_eng_h, 0);
} /* client_disconnect() */

/**
 * This is called when the server initiates the disconnection.
 */
int mspace::server_disconnect(uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h,
		       uint64_t server_to_lib_tx_eng_h)
{
	return disconnect(false, client_msubid, client_to_lib_tx_eng_h,
			  server_to_lib_tx_eng_h);

} /* server_disconnect() */

set<uint16_t> mspace::get_rem_destids()
{
	set<uint16_t>	rem_destids;

	/* Check if the memory space creator is connected */
	if (connected_to)
		rem_destids.insert(client_destid);

	/* Now check the memory space users for connections */
	lock_guard<mutex> users_lock(users_mutex);

	for( auto& u : users)
		if (u.connected_to)
			rem_destids.insert(u.client_destid);

	return rem_destids;
} /* get_rem_destids() */

/* Debugging */
void mspace::dump_info(struct cli_env *env)
{
	sprintf(env->output, "%34s %08X %08X %016" PRIx64 " %08X\n",
				name.c_str(), msoid, msid, rio_addr, size);
	logMsg(env);
	sprintf(env->output, "destids: ");
	logMsg(env);

	set<uint16_t> rem_destids = get_rem_destids();
	for (auto& destid : rem_destids) {
		sprintf(env->output, "%u ", destid);
		logMsg(env);
	}

	sprintf(env->output, "\n");
	logMsg(env);
} /* dump_info() */

void mspace::dump_info_msubs_only(struct cli_env *env)
{
	sprintf(env->output, "%8s %16s %8s %8s %16s\n", "msubid", "rio_addr",
	                "size", "msid", "phys_addr");
	logMsg(env);
	sprintf(env->output, "%8s %16s %8s %8s %16s\n", "------", "--------",
	                "----", "----", "---------");
	logMsg(env);

	/* Called from dump_info_with_msubs --> no mutex locking needed here */
	for (auto& msub : msubspaces)
		msub.dump_info(env);
} /* dump_info_msubs_only() */

void mspace::dump_info_with_msubs(struct cli_env *env)
{
	dump_info(env);

	lock_guard<mutex> msubspaces_lock(msubspaces_mutex);
	if (msubspaces.size()) {
		dump_info_msubs_only(env);
	} else {
		sprintf(env->output, "No subspaces in above memory space\n");
		logMsg(env);
	}

	sprintf(env->output, "\n"); /* Extra line */
	logMsg(env);
} /* dump_info_with_msubs() */

/* For creating a memory sub-space */
int mspace::create_msubspace(uint32_t offset, uint32_t size,
                uint32_t *msubid, uint64_t *rio_addr, uint64_t *phys_addr,
                const tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	/* Make sure we don't straddle memory space boundaries */
	if ((offset + size) > this->size) {
		ERR("Out of range: offset(0x%X)+req_size(0x%X) > size(0x%X)\n",
				offset, size, this->size);
		return -1;
	}

	/* Determine index of new, free, memory sub-space */
	lock_guard<mutex> msubindex_free_list_lock(msubindex_free_list_mutex);
	bool *fmsubit = find(msubindex_free_list,
	                msubindex_free_list + MSUBINDEX_MAX + 1, true);
	if (fmsubit == (msubindex_free_list + MSUBINDEX_MAX + 1)) {
		ERR("No free subspace handles!\n");
		return -2;
	}

	/* Set msub ID as being used */
	*fmsubit = false;

	/* msid in upper 2 bytes, msubindex in lower 2 bytes */
	*msubid = (msid << 16) + (fmsubit - msubindex_free_list);

	/* Msub will be at offset of memory space rio_addr, and same
	 * goes for the physical address */
	*rio_addr = this->rio_addr + offset;
	*phys_addr = this->phys_addr + offset;

	/* Add to list of subspaces */
	lock_guard<mutex> msubspaces_lock(msubspaces_mutex);
	msubspaces.emplace_back(msid, *rio_addr, *phys_addr, size, *msubid, tx_eng);

	return 0;
} /* create_msubspace() */

int mspace::open(tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	/* Altough LIBRDMA should contain some safegaurds against the same
	 * memory space being opened twice by the same application, it doesn't
	 * hurt to add a quick check here. */
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find(begin(users), end(users), user_tx_eng);
	if (it != end(users)) {
		ERR("'%s' already open by this application\n", name.c_str());
		return RDMA_ALREADY_OPEN;
	}

	/* Store info about user that opened the ms in 'users' */
	users.emplace_back(user_tx_eng);
	INFO("user with user_tx_eng(%p) stored in msid(0x%X)\n",
							user_tx_eng, msid);
	return 0;
} /* open() */

tx_engine<unix_server, unix_msg_t> *mspace::get_accepting_tx_eng()
{
	/* Owner is accepting? */
	if (accepting) {
		return creator_tx_eng;
	}

	/* One of the users is accepting? */
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find_if(begin(users), end(users),
		       [](ms_user& user) { return user.accepting; });
	if (it != end(users))
		return it->tx_eng;

	/* No one is accepting! */
	WARN("'%s': No accepting tx_eng() found\n", name.c_str());
	return nullptr;
} /* get_accepting_tx_eng() */

int mspace::accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng,
		   uint32_t server_msubid)
{
	auto is_accepting = [](ms_user& user) { return user.accepting; };

	/* app_tx_eng could be the tx_eng from the daemon to either the creator of
	 * the mspace, or of one of its users. Set the appropriate is_accepting
	 * flag (i.e. either in the main class or in one of the ms_users).
	 *
	 * Only 1 connection to the memory space can be accepting at a time.
	 * Furthermore, the one that accepts cannot be already connected-to. */

	/* First check if it is the creator who is accepting */
	if (app_tx_eng == creator_tx_eng) {
		if (accepting || connected_to) {
			/* Cannot accept twice from the same app, or accept
			 * from an already connected app. */
			ERR("Creator app already accepting or connected to\n");
			return RDMA_DUPLICATE_ACCEPT;
		}

		/* Cannot accept from creator app if we are already
		 * accepting from a user app. */
		lock_guard<mutex> users_lock(users_mutex);
		if (count_if(begin(users), end(users), is_accepting) > 0) {
			ERR("'%s' already accepting from a user app\n");
			return RDMA_ACCEPT_FAIL;
		}

		/* All is good, set 'accepting' flag */
		HIGH("'%s' set to 'accepting'\n", name.c_str());
		accepting = true;
		this->server_msubid = server_msubid;
		return 0;
	}

	/* It is not the creator who is trying to 'accept' - check 'users' */
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find(begin(users), end(users), app_tx_eng);
	if (it == end(users)) {
		ERR("%s Could not find a user matching app_tx_eng\n",
			name.c_str());
		return RDMA_ACCEPT_FAIL;
	}

	/* The user can't already be accepting or connected_to */
	if (it->accepting || it->connected_to) {
		WARN("Cannot accept since already accepting or connected_to\n");
		return RDMA_ACCEPT_FAIL;
	}

	/* The creator can't be accepting (using another tx_eng) */
	if (accepting) {
		ERR("'%s' Already accepting from creator\n", name.c_str());
		return RDMA_ACCEPT_FAIL;
	}

	/* And none of the users should be accepting! */
	if (count_if(begin(users), end(users), is_accepting) > 0) {
		ERR("'%s' already accepting by a user.\n", name.c_str());
		return RDMA_ACCEPT_FAIL;
	}

	/* All is good, set 'accepting' flag */
	INFO("'%s' set to accepting for a user with tx_eng(%" PRIx64 ")\n",
			name.c_str(), (uint64_t)it->tx_eng);
	it->accepting = true;;
	it->server_msubid = server_msubid;
	return 0;
} /* accept() */

int mspace::undo_accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	/* First check if it is the creator who is accepting */
	if (app_tx_eng == creator_tx_eng) {
		/* Not accepting, return with error */
		if (!accepting) {
			ERR("'%s' was not accepting!\n", name.c_str());
			return -1;
		}

		/* Set accepting to 'false' and clear server_msubid */
		HIGH("Setting ms('%s' to 'NOT accepting'\n", name.c_str());
		accepting = false;
		server_msubid = 0;
		return 0;
	}

	/* It wasn't the creator so search the users by app_tx_eng */
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find(begin(users), end(users), app_tx_eng);
	if (it == end(users)) {
		ERR("Could not find matching tx_eng\n");
		return -1;
	}

	if (!it->accepting) {
		ERR("'%s' was not accepting!\n", name.c_str());
		return -1;
	}

	/* Set accepting to 'false' and clear server_msubid */
	HIGH("Setting ms('%s' to 'NOT accepting'\n", name.c_str());
	it->accepting = false;
	it->server_msubid = 0;
	return 0;
} /* undo_accept() */

bool mspace::has_user_with_user_tx_eng(
		tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find(begin(users), end(users), user_tx_eng);

	if (it != end(users)) {
		return true;
	} else {
		DBG("mspace '%s' does not use tx_eng\n", name.c_str());
		return false;
	}
} /* has_user_with_user_server() */

bool mspace::connected_by_destid(uint16_t client_destid)
{
	lock_guard<mutex> users_lock(users_mutex);
	return (this->client_destid == client_destid) ||
		(find(begin(users), end(users), client_destid) != end(users));
} /* connected_by_destid() */

int mspace::close(tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	/* A creator of an ms cannot open/close it */
	if (app_tx_eng == creator_tx_eng) {
		ERR("Creator of memory space cannot open/close it\n");
		return RDMA_MS_CLOSE_FAIL;
	}

	/* Find a the user that has opened the tx_eng connection to the ms */
	lock_guard<mutex> users_lock(users_mutex);
	auto it = find(begin(users), end(users), app_tx_eng);
	if (it == end(users)) {
		WARN("Failed to find open connection!\n");
		return RDMA_MS_CLOSE_FAIL;
	}

	/* Before closing a memory space, tell its clients that it is being
	 * closed (connection must be dropped) and have them acknowledge. */
	auto rc = send_disconnect_to_remote_daemon(it->client_msubid,
					      it->client_to_lib_tx_eng_h,
					      (uint64_t)app_tx_eng);
	if (rc) {
		WARN("Failed to send disconnection to remote daemon\n");
		/* I think we should proceed with closing the connection even
		 * if we cannot notify the remote daemon. So no error here.*/
	}
	rc = 0;

	/* Erase user element */
	users.erase(it);

	/* Destroy msubs that belong to the same 'app_tx_eng' */
	lock_guard<mutex> msubspaces_lock(msubspaces_mutex);
	msubspaces.erase(
			remove(begin(msubspaces), end(msubspaces), app_tx_eng),
			end(msubspaces));

	return rc;
} /* close() */

int mspace::destroy_msubspace(uint32_t msubid)
{
	lock_guard<mutex> msubspaces_lock(msubspaces_mutex);

	/* Find memory sub-space in list within this memory space */
	auto msub_it = find(begin(msubspaces), end(msubspaces), msubid);

	/* Not found, return with error */
	if (msub_it == end(msubspaces)) {
		ERR("msubid 0x%X not found in %s\n", msubid, name.c_str());
		return -1;
	}

	/* Erase the subspace */
	msubspaces.erase(msub_it);

	return 0;
} /* destroy_msubspace() */

