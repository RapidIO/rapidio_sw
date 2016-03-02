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

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <algorithm>
#include <vector>

#include "cm_sock.h"
#include "liblog.h"
#include "rdma_types.h"
#include "rdmad_cm.h"
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
                uint64_t phys_addr, uint64_t size) :
                		name(name),
                		msid(msid),
                		rio_addr(rio_addr),
                		phys_addr(phys_addr),
                		size(size),
                		msoid(0),
                		free(true),
		                connected_to(false),
		                accepting(false),
		                server_msubid(0),
		                creator_tx_eng(nullptr),
		                client_destid(0xFFFF),
		                client_msubid(NULL_MSUBID),
		                client_to_lib_tx_eng_h(0)
{
	INFO("name=%s, msid=0x%08X, rio_addr=0x%" PRIx64 ", size=0x%X\n",
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

int mspace::send_cm_force_disconnect_ms(cm_server *server, uint32_t server_msubid,
				uint64_t client_to_lib_tx_eng_h)
{
	int rc;

	/* Prepare destroy message */
	cm_force_disconnect_msg	*dm;
	server->get_send_buffer((void **)&dm);
	dm->type	= htobe64(CM_FORCE_DISCONNECT_MS);
	strcpy(dm->server_msname, name.c_str());
	dm->server_msid = htobe64(msid);
	dm->server_msubid = htobe64(server_msubid);
	dm->client_to_lib_tx_eng_h = htobe64(client_to_lib_tx_eng_h);

	/* Send to remote daemon @ 'client_destid' */
	if (server->send()) {
		WARN("Failed to send CM_FORCE_DISCONNECT_MS to client_destid(0x%X)\n",
							client_destid);
		rc = RDMA_REMOTE_UNREACHABLE;
	} else {
		DBG("CM_FORCE_DISCONNECT_MS sent to client_destid(0x%X)\n",
							client_destid);
		rc = 0;
	}

	return rc;
} /* send_cm_force_disconnect_ms() */

int mspace::send_disconnect_to_remote_daemon(uint32_t client_msubid,
				     uint64_t client_to_lib_tx_eng_h)
{
	int rc;
	cm_base *server = nullptr;

	DBG("ENTER with client_msubid=0x%X, client_to_lib_tx_eng=0x%"
			PRIx64 "\n", client_msubid, client_to_lib_tx_eng_h);

	/* If the mspace creator matches on the client_msubid,
	 * client_to_lib_tx_eng_h (connh) and is 'connected_to', then
	 * look up its destid and send the force disconnect message. */
	if (connected_to &&
	   (this->client_msubid == client_msubid) &&
	   (this->client_to_lib_tx_eng_h == client_to_lib_tx_eng_h)) {
		server =
		     prov_daemon_info_list.get_cm_sock_by_destid(client_destid);
	} else {
		/* If the mspace creator does NOT match on the client_msubid,
		 * client_to_lib_tx_eng_h (connh) or is NOT 'connected_to', then
		 * search all the users for a match and, for the matched user
		 * look up its destid and send the force disconnect message. */
		auto it = find_if(begin(users), end(users),
		[client_to_lib_tx_eng_h, client_msubid](ms_user& u) {
			DBG("u.client_msubid=0x%X, u.client_to_lib_tx_eng=0x%"
				PRIx64 "\n", u.client_msubid, u.client_to_lib_tx_eng_h);
			return u.client_to_lib_tx_eng_h == client_to_lib_tx_eng_h
			    && u.client_msubid == client_msubid
			    && u.connected_to;
		});
		if (it == end(users)) {
			CRIT("No user matches specified parameter(s)\n");
			CRIT("client_msubid=0x%X, client_to_lib_tx_eng=0x%"
				PRIx64 "\n", client_msubid, client_to_lib_tx_eng_h);
			rc = -1;
		} else {
			/* Now find the cm_server to use by looking up the
			 * user's destid in the prov_daemon_info_list */
			server = prov_daemon_info_list.get_cm_sock_by_destid(
					     	     	    it->client_destid);
		}
	}

	if (server != nullptr) {
		rc = send_cm_force_disconnect_ms(dynamic_cast<cm_server *>(server),
						server_msubid,
						client_to_lib_tx_eng_h);
		if (rc) {
			ERR("Failed to send CM_FORCE_DISCONNECT_MS\n");
		}
	}

	return rc;
} /* send_disconnect_to_remote_daemon() */

int mspace::notify_remote_clients()
{
	int rc;

	DBG("ENTER\n");
	if (connected_to) {
		/* Need to use a 'prov' socket to send the CM_DESTROY_MS */
		cm_base *server =
				prov_daemon_info_list.get_cm_sock_by_destid(
								client_destid);
		rc = send_cm_force_disconnect_ms(dynamic_cast<cm_server *>(server),
						 server_msubid,
						 client_to_lib_tx_eng_h);
	} else {
		rc = 0;
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

			/* Need to use a 'prov' socket to send the CM_DESTROY_MS */
			cm_base *server =
				prov_daemon_info_list.get_cm_sock_by_destid(
							u.client_destid);

			rc = send_cm_force_disconnect_ms(dynamic_cast<cm_server *>(server),
							u.server_msubid,
							u.client_to_lib_tx_eng_h);
		}
		if (locked) {
			users_mutex.unlock();
			DBG("Unlocked users_mutex\n");
		}
	}
	DBG("EXIT\n");
	return rc;
} /* notify_remote_clients() */

int mspace::close_connections()
{
	lock_guard<mutex> users_lock(users_mutex);

	HIGH("FORCE_CLOSE_MS to apps which have 'open'ed '%s'\n", name.c_str());

	/* Tell local apps which have opened the ms that the ms will be destroyed */
	for (ms_user& user : users) {
		tx_engine<unix_server, unix_msg_t> *user_tx_eng =
			user.tx_eng;

		unix_msg_t	in_msg;

		in_msg.type 	= FORCE_CLOSE_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.seq_no   = 0;
		in_msg.force_close_ms_req.msid = msid;
		user_tx_eng->send_message(&in_msg);
	}

	users.clear();
	DBG("EXIT\n");
	return 0;
} /* close_connections() */

int mspace::destroy()
{
	DBG("name=%s, msid=0x%08X, rio_addr=0x%" PRIx64 ", size=0x%X\n", name.c_str(),
	                msid, rio_addr, size);

	int rc;

	try {
		/* Before destroying a memory space, tell its clients that
		 * it is being destroyed and have them acknowledge that.
		 * Then remove their destids */
		rc = notify_remote_clients();
		if (rc) {
			WARN("Failed to notify some or all remote clients\n");
			throw rc;
		}

		/* Close connections from other local 'user' applications and
		 * delete message queues used to communicate with those apps. */
		rc = close_connections();
		if (rc) {
			WARN("Connection(s) to msid(0x%X) did not close\n", msid);
			throw rc;
		}

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

		/* Clear the owner. No free space should have an owner */
		set_msoid(0);
	}
	catch(int& e) {
		rc = e;
	}


	return rc;
} /* destroy() */

int mspace::add_rem_connection(uint16_t client_destid,
			        uint32_t client_msubid,
			        uint64_t client_to_lib_tx_eng_h)
{
	int rc;
	DBG("Adding destid(0x%X), msubid(0x%X),	client_to_lib_tx_eng_h(0x%"
			PRIx64 ")to '%s'\n", client_destid,
			client_msubid, client_to_lib_tx_eng_h, name.c_str());
	if (accepting) {
		DBG("Adding to creator since it is 'accepting'\n");
		this->client_destid = client_destid;
		this->client_msubid = client_msubid;
		this->client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;
		this->accepting = false;
		this->connected_to = true;
		rc = 0;
	} else {
		lock_guard<mutex> users_lock(users_mutex);

		DBG("The creator was not accepting..checking the users\n");
		DBG("There are %u user(s)\n", users.size());
		auto it = find_if(begin(users), end(users), [](ms_user& u) {
			return u.accepting;
		});
		if (it == end(users)) {
			CRIT("Failed to find a user in 'accepting' mode.\n");
			rc = -1;
		} else {
			DBG("Adding to user whose tx_eng = 0x%" PRIx64 "\n",
					(uint64_t)it->tx_eng);
			it->client_destid = client_destid;
			it->client_msubid = client_msubid;
			it->client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;
			it->accepting = false;
			it->connected_to = true;
			rc = 0;
		}
	}
	return rc;
} /* add_rem_connection() */

int mspace::remove_rem_connection(uint16_t client_destid,
				  uint32_t client_msubid,
				  uint64_t client_to_lib_tx_eng_h)
{
	int rc;

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
		rc = 0;
	} else {
		DBG("Not found in the creator; checking the users\n");

		/* It is possibly in one of the users so search for it */
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
			rc = -1;
		} else {
			it->client_destid = 0xFFFF;
			it->client_msubid = NULL_MSUBID;
			it->client_to_lib_tx_eng_h = 0;
			it->connected_to = false;
			it->server_msubid = 0;
			it->connected_to = false;
			rc = 0;
		}
	}

	return rc;
} /* remove_rem_connection() */

void mspace::send_disconnect_to_lib(uint32_t client_msubid,
		uint32_t server_msubid,
		uint64_t client_to_lib_tx_eng_h,
		tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t	in_msg;

	in_msg.category = RDMA_REQ_RESP;
	in_msg.type	= DISCONNECT_MS;
	in_msg.disconnect_from_ms_req_in.client_msubid = client_msubid;
	in_msg.disconnect_from_ms_req_in.server_msubid = server_msubid;
	in_msg.disconnect_from_ms_req_in.client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;

	tx_eng->send_message(&in_msg);
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

int mspace::disconnect(bool is_client, uint32_t client_msubid,
		       	       	       uint64_t client_to_lib_tx_eng_h)
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
						client_to_lib_tx_eng_h);
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
							client_to_lib_tx_eng_h);
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
	return disconnect(true, client_msubid, client_to_lib_tx_eng_h);
} /* client_disconnect() */

/**
 * This is called when the server initiates the disconnection.
 */
int mspace::server_disconnect(uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h)
{
	return disconnect(false, client_msubid, client_to_lib_tx_eng_h);

} /* server_disconnect() */

set<uint16_t> mspace::get_rem_destids()
{
	set<uint16_t>	rem_destids;

	/* Check if the memory space creator is connected */
	if (connected_to)
		rem_destids.insert(client_destid);

	/* Now check the memory space users for connections */
	lock_guard<mutex> users_lock(users_mutex);

	for( auto& u : users) {
		if (u.connected_to)
			rem_destids.insert(u.client_destid);
	}

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
	for (auto& msub : msubspaces) {
		msub.dump_info(env);
	}
} /* dump_info_msubs_only() */

void mspace::dump_info_with_msubs(struct cli_env *env)
{
	dump_info(env);

	lock_guard<mutex> msubspaces_lock(msubspaces_mutex);
	if (msubspaces.size()) {
		dump_info_msubs_only(env);
	} else {
		puts("No subspaces in above memory space");
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
	int rc;

	DBG("ENTER\n");
	lock_guard<mutex> users_lock(users_mutex);

	/* Altough LIBRDMA should contain some safegaurds against the same
	 * memory space being opened twice by the same application, it doesn't
	 * hurt to add a quick check here. */
	auto it = find(begin(users), end(users), user_tx_eng);
	if (it != end(users)) {
		ERR("'%s' already open by this application\n", name.c_str());
		rc = RDMA_ALREADY_OPEN;
	} else {
		/* Store info about user that opened the ms in 'users' */
		users.emplace_back(user_tx_eng);
		DBG("user with user_tx_eng(%p) stored in msid(0x%X)\n",
							user_tx_eng, msid);
		rc = 0;
	}
	DBG("EXIT\n");
	return rc;
} /* open() */

tx_engine<unix_server, unix_msg_t> *mspace::get_accepting_tx_eng()
{
	tx_engine<unix_server, unix_msg_t> *tx_eng = nullptr;


	if (accepting)
		tx_eng = creator_tx_eng;
	else {
		lock_guard<mutex> users_lock(users_mutex);
		auto it = find_if(begin(users),
			       end(users),
			       [](ms_user& user)
			       {
					return user.accepting;
			       });
		if (it != end(users))
			tx_eng = it->tx_eng;
	}

	return tx_eng;
} /* get_accepting_tx_eng() */

int mspace::accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng,
		   uint32_t server_msubid)
{
	int rc;

	/**
	 * app_tx_eng could be the tx_eng from the daemon to either the creator of
	 * the mspace, or of one of its users. Set the appropriate is_accepting
	 * flag (i.e. either in the main class or in one of the ms_users).
	 *
	 * Only 1 connection to the memory space can be accepting at a time.
	 * Furthermore, the one that accepts cannot be already connected-to.
	 */

	/* First check if it is the creator who is accepting */
	if (app_tx_eng == creator_tx_eng) {
		if (accepting || connected_to) {
			/* Cannot accept twice from the same app, or accept
			 * from an already connected app. */
			ERR("Creator app already accepting or connected\n");
			rc = RDMA_DUPLICATE_ACCEPT;
		} else {
			/* Cannot accept from creator app if we are already
			 * accepting from a user app. */
			lock_guard<mutex> users_lock(users_mutex);
			auto n = count_if(begin(users),
					  end(users),
					  [](ms_user& user)
					 {
						return user.accepting;
					 });
			if (n > 0) {
				ERR("'%s' already accepting from a user app\n");
				rc = RDMA_ACCEPT_FAIL;
			} else {
				/* All is good, set 'accepting' flag */
				HIGH("'%s' set to 'accepting'\n", name.c_str());
				accepting = true;
				this->server_msubid = server_msubid;
				rc = 0;
			}
		}
	} else { /* It is not the creator who is trying to 'accept' */
		lock_guard<mutex> users_lock(users_mutex);
		auto it = find(begin(users), end(users), app_tx_eng);
		if (it == end(users)) {
			ERR("Could not find matching tx_eng\n");
			rc = RDMA_ACCEPT_FAIL;
		} else {
			if (it->accepting || it->connected_to) {
				/* The user can't already be accepting or connected_to */
				rc = RDMA_ACCEPT_FAIL;
			} else if (accepting) {
				/* The owner can't be accepting either */
				ERR("'%s' already accepting from creator app\n",
						name.c_str());
				rc = RDMA_ACCEPT_FAIL;
			} else {
				/* And none of the users should be accepting! */
				auto n = count_if(begin(users),
						  end(users),
						  [](ms_user& user)
						 {
							return user.accepting;
						 });
				if (n > 0) {
					ERR("'%s' already accepting by user\n",
							name.c_str());
					rc = RDMA_ACCEPT_FAIL;
				} else {
					/* All is good, set 'accepting' flag */
					INFO("'%s' set to accepting for a user\n",
							name.c_str());
					DBG("tx_eng = 0x%" PRIx64 "\n",
							(uint64_t)it->tx_eng);
					it->accepting = true;;
					it->server_msubid = server_msubid;
					rc = 0;
				}
			}
		}
	}

	return rc;
} /* accept() */

int mspace::undo_accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	int rc;

	/* First check if it is the creator who is accepting */
	if (app_tx_eng == creator_tx_eng) {
		if (!accepting) {
			ERR("'%s' was not accepting!\n", name.c_str());
			rc = -1;
		} else {
			/* Set accepting to 'false' and clear server_msubid */
			HIGH("Setting ms('%s' to 'NOT accepting'\n", name.c_str());
			accepting = false;
			server_msubid = 0;
			rc = 0;
		}
	} else {
		/* It wasn't the creator so search the users by app_tx_eng */
		lock_guard<mutex> users_lock(users_mutex);
		auto it = find(begin(users), end(users), app_tx_eng);
		if (it == end(users)) {
			ERR("Could not find matching tx_eng\n");
			rc = -1;
		} else {
			if (!it->accepting) {
				ERR("'%s' was not accepting!\n", name.c_str());
				rc = RDMA_ACCEPT_FAIL;
			} else {
				/* Set accepting to 'false' and clear server_msubid */
				HIGH("Setting ms('%s' to 'NOT accepting'\n", name.c_str());
				it->accepting = false;
				it->server_msubid = 0;
				rc = 0;
			}
		}
	}
	return rc;
} /* undo_accept() */

bool mspace::has_user_with_user_tx_eng(
		tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	bool has_user = false;

	lock_guard<mutex> users_lock(users_mutex);
	auto it = find(begin(users), end(users), user_tx_eng);

	if (it != end(users)) {
		has_user = true;
	} else {
		DBG("mspace '%s' does not use tx_eng\n", name.c_str());
	}

	return has_user;
} /* has_user_with_user_server() */

bool mspace::connected_by_destid(uint16_t client_destid)
{
	bool connected = (this->client_destid == client_destid);

	lock_guard<mutex> users_lock(users_mutex);
	connected = connected ||
		(find(begin(users), end(users), client_destid) != end(users));

	return connected;
} /* connected_by_destid() */

int mspace::close(tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	int rc;

	DBG("ENTER\n");

	try {
		/* A creator of an ms cannot open/close it */
		if (app_tx_eng == creator_tx_eng) {
			ERR("Creator of memory space cannot open/close it\n");
			throw RDMA_MS_CLOSE_FAIL;
		}

		lock_guard<mutex> users_lock(users_mutex);

		auto it = find(begin(users), end(users), app_tx_eng);
		if (it == end(users)) {
			WARN("Failed to find open connection!\n");
			throw RDMA_MS_CLOSE_FAIL;
		}

		/* Before closing a memory space, tell its clients that it is being
		 * closed (connection must be dropped) and have them acknowledge. */
		rc = send_disconnect_to_remote_daemon(it->client_msubid,
						      it->client_to_lib_tx_eng_h);
		if (rc) {
			ERR("Failed to send disconnection to remote daemon\n");
		}

		/* Erase user element */
		users.erase(it);

		/* Destroy msubs that belong to the same 'app_tx_eng' */
		lock_guard<mutex> msubspaces_lock(msubspaces_mutex);
		msubspaces.erase(
			remove(begin(msubspaces), end(msubspaces), app_tx_eng),
			end(msubspaces)
		);
	}
	catch(int& e) {
		rc = e;
	}

	DBG("EXIT\n");

	return rc;
} /* close() */

int mspace::destroy_msubspace(uint32_t msubid)
{
	int rc;

	lock_guard<mutex> msubspaces_lock(msubspaces_mutex);

	/* Find memory sub-space in list within this memory space */
	auto msub_it = find(msubspaces.begin(), msubspaces.end(), msubid);

	/* Not found, return with error */
	if (msub_it == msubspaces.end()) {
		ERR("msubid 0x%X not found in %s\n", msubid, name.c_str());
		rc = -1;
	} else {
		/* Erase the subspace */
		msubspaces.erase(msub_it);
		rc = 0;	/* Success */
	}

	return rc;
} /* destroy_msubspace() */

