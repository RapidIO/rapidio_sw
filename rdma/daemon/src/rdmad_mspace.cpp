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

#include <cstdio>
#include <cstring>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <algorithm>
#include <utility>
#include <vector>
#include <sstream>

#include "cm_sock.h"
#include "liblog.h"
#include "rdma_types.h"
#include "rdmad_cm.h"
#include "rdma_mq_msg.h"
#include "rdmad_mspace.h"
#include "rdmad_msubspace.h"
#include "rdmad_ms_owner.h"
#include "rdmad_ms_owners.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_main.h"

using std::fill;
using std::remove_if;
using std::find;

mspace::mspace(const char *name, uint32_t msid, uint64_t rio_addr,
                uint64_t phys_addr, uint64_t size) :
		name(name), msid(msid), rio_addr(rio_addr), phys_addr(
		                phys_addr), size(size), msoid(0), free(true),
		                current_ms_conn_id(MS_CONN_ID_START),
		                connected_to(false), accepting(false),
		                server_msubid(0), creator_tx_eng(nullptr)
{
	INFO("name=%s, msid=0x%08X, rio_addr=0x%" PRIx64 ", size=0x%X\n",
						name, msid, rio_addr, size);

	/* Initially all free list sub-indexes are available */
	fill(msubindex_free_list, msubindex_free_list + MSUBINDEX_MAX + 1, true);
	msubindex_free_list[0] = false; /* Start at 1 */

	if (sem_init(&users_sem, 0, 1) == -1) {
		CRIT("Failed to initialize users_sem: %s\n", strerror(errno));
		throw mspace_exception("Failed to initialize users_sem");
	}

	if (sem_init(&msubspaces_sem, 0, 1) == -1) {
		CRIT("Failed to initialize subspaces_sem: %s\n", strerror(errno));
		throw mspace_exception("Failed to initialize subpaces_sem");
	}

	if (sem_init(&msubindex_free_list_sem, 0, 1) == -1) {
		CRIT("Failed to initialize subspaces_sem: %s\n", strerror(errno));
		throw mspace_exception("Failed to initialize msubindex_free_list_sem");
	}
} /* constructor */

mspace::~mspace()
{
	if (!free)
		destroy();
} /* destructor */

int mspace::send_cm_destroy_ms(cm_server *server, uint32_t server_msubid,
				uint64_t client_to_lib_tx_eng_h)
{
	int rc;

	/* Prepare destroy message */
	cm_destroy_msg	*dm;
	server->get_send_buffer((void **)&dm);
	dm->type	= htobe64(CM_DESTROY_MS);
	strcpy(dm->server_msname, name.c_str());
	dm->server_msid = htobe64(msid);
	dm->server_msubid = htobe64(server_msubid);
	dm->client_to_lib_tx_eng_h = htobe64(client_to_lib_tx_eng_h);

	/* Send to remote daemon @ 'client_destid' */
	if (server->send()) {
		WARN("Failed to send CM_DESTROY_MS to client_destid(0x%X)\n",
							client_destid);
		rc = -2;
	} else {
		DBG("CM_DESTROY_MS sent to client_destid(0x%X)\n",
							client_destid);
		rc = 0;
	}

	return rc;
} /* send_cm_destroy_ms() */

int mspace::notify_remote_clients()
{
	int rc;

	if (connected_to) {
		/* Need to use a 'prov' socket to send the CM_DESTROY_MS */
		sem_wait(&prov_daemon_info_list_sem);
		auto prov_it = find(begin(prov_daemon_info_list),
		                end(prov_daemon_info_list), client_destid);
		if (prov_it == end(prov_daemon_info_list)) {
			ERR("Could not find entry for client_destid(0x%X)\n",
							client_destid);
			rc = -1;
		} else {
			rc = send_cm_destroy_ms(prov_it->conn_disc_server,
						server_msubid,
						client_to_lib_tx_eng_h);
		}
		sem_post(&prov_daemon_info_list_sem);
	} else {
		rc = 0;
		/* It is not the creator who has a connection; search users */
		for(auto& u : users) {
			/* Need to use a 'prov' socket to send the CM_DESTROY_MS */
			sem_wait(&prov_daemon_info_list_sem);
			auto prov_it = find(begin(prov_daemon_info_list),
			                end(prov_daemon_info_list), u.client_destid);
			if (prov_it == end(prov_daemon_info_list)) {
				ERR("Could not find entry for client_destid(0x%X)\n",
								client_destid);
				rc = -1;
			} else if (u.connected_to) {
				rc = send_cm_destroy_ms(prov_it->conn_disc_server,
							u.server_msubid,
							u.client_to_lib_tx_eng_h);
			}
				/* If the entry is not 'connected_to' then skip it */
			sem_post(&prov_daemon_info_list_sem);
		}
	}

	return rc;
} /* notify_remote_clients() */

int mspace::close_connections()
{
	sem_wait(&users_sem);

	HIGH("FORCE_CLOSE_MS to apps which have 'open'ed '%s'\n", name.c_str());

	/* Tell local apps which have opened the ms that the ms will be destroyed */
	for (ms_user& user : users) {
		tx_engine<unix_server, unix_msg_t> *user_tx_eng =
			user.tx_eng;

		unix_msg_t	in_msg;

		in_msg.type 	= FORCE_CLOSE_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.force_close_ms_req.msid = msid;
		user_tx_eng->send_message(&in_msg);
	}

	users.clear();

	sem_post(&users_sem);

	return 0;
} /* close_connections() */

int mspace::destroy()
{
	DBG("name=%s, msid=0x%08X, rio_addr=0x%lX, size=0x%X\n", name.c_str(),
	                msid, rio_addr, size);

	/* Before destroying a memory space, tell its clients that it is being
	 * destroyed and have them acknowledge that. Then remove their destids */
	if (notify_remote_clients()) {
		WARN("Failed to notify some or all remote clients\n");
		return -1;
	}

	/* Close connections from other local 'user' applications and
	 * delete message queues used to communicate with those apps.
	 */
	if (close_connections()) {
		WARN("Connection(s) to msid(0x%X) did not close\n", msid);
		return -2;
	}

	/* Remove all subspaces; they can't exist when the memory space is
	 * marked as free.
	 */
	INFO("Destroying all subspaces in '%s'\n", name.c_str());
	msubspaces.clear();

	/* Mark the memory space as free, and having no owner */
	free = true;
	name = "freemspace";
	connected_to = false; /* No connections */
	accepting = false;    /* Not accepting connections either */
	creator_tx_eng = nullptr;	/* No tx_eng associated therewith */

	/* Remove memory space identifier from owner */
	ms_owner *owner;
	int 	 ret = 0;

	try {
		owner = owners[msoid];
	} catch (...) {
		ERR("Failed to find owner msoid(0x%X)\n", msoid);
		ret = -3;
	}

	if (ret == 0) {
		if (!owner) {
			ERR("Failed to find owner msoid(0x%X)\n", msoid);
			ret = -4;
		} else if (owner->remove_ms(this) < 0) {
			WARN("Failed to remove ms from owner\n");
			ret = -5;
		}
	}

	/* Clear the owner. No free space should have an owner */
	set_msoid(0);

	return ret;
} /* destroy() */

int mspace::add_rem_connection(uint16_t client_destid,
			        uint32_t client_msubid,
			        uint64_t client_to_lib_tx_eng_h)
{
	int rc;
	DBG("Adding destid(0x%X), msubid(0x%X),	client_to_lib_tx_eng_h(0x%"
			PRIx64 ")to '%s'\n", client_destid,
			client_msubid, client_to_lib_tx_eng_h, name.c_str());
	sem_wait(&users_sem);
	if (accepting) {
		this->client_destid = client_destid;
		this->client_msubid = client_msubid;
		this->client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;
		accepting = false;
		connected_to = true;
		rc = 0;
	} else {
		auto it = find_if(begin(users), end(users), [](ms_user& u) {
			return u.accepting;
		});
		if (it == end(users)) {
			CRIT("Failed to find a user in 'accepting' mode.\n");
			rc = -1;
		} else {
			it->client_destid = client_destid;
			it->client_msubid = client_msubid;
			it->client_to_lib_tx_eng_h = client_to_lib_tx_eng_h;
			accepting = false;
			connected_to = true;
			rc = 0;
		}
	}
	sem_post(&users_sem);
	return rc;
} /* add_rem_connection() */


int mspace::remove_rem_connection(uint16_t client_destid,
				  uint32_t client_msubid,
				  uint64_t client_to_lib_tx_eng_h)
{
	int rc;

	DBG("Removing destid(0x%X), msubid(0x%X) from '%s'\n",
			client_destid, client_msubid, name.c_str());
	/* First check to see if the connection belongs to the creator */
	if ((this->client_destid == client_destid) &&
	    (this->client_msubid == client_msubid) &&
	    (this->client_to_lib_tx_eng_h)) {
		INFO("Found connection info in creator of '%s'\n", name.c_str());
		this->client_destid = 0xFFFF;
		this->client_msubid = 0;
		this->client_to_lib_tx_eng_h = 0;
		connected_to = false;
		rc = 0;
	} else {
		sem_wait(&users_sem);
		/* It is possibly in one of the users so search for it */
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
			it->client_msubid = 0;
			it->client_to_lib_tx_eng_h = 0;
			it->connected_to = false;
		}
		sem_post(&users_sem);
	}

	return rc;
} /* remove_rem_connection() */

/* Disconnect all connections from the specified client_destid */
int mspace::disconnect_from_destid(uint16_t client_destid)
{
	int rc;

	if ((this->client_destid == client_destid) && connected_to) {
		rc = disconnect(client_msubid);
		if (rc) {
			ERR("Failed to disconnect destid(0x%X), msubid(0x%X)\n",
			client_destid, client_msubid);
		} else {
			INFO("%s disconnected from destid(0x%X)\n",
					name.c_str(), client_destid);
			this->client_destid = 0xFFFF;
			this->client_msubid = 0;
			this->client_to_lib_tx_eng_h = 0;
			this->connected_to = false;
			rc = 0;
		}
	} else {
		/* Search all users looking for ones that are connected
		 * to the specified destid. There can be more than one
		 * user who have accepted connections on this memory space
		 * but from different clients on the same remote node.
		 */
		rc = 0;	/* It is not an error if there are no users! */
		sem_wait(&users_sem);
		for (auto& u : users) {
			if ((u.client_destid == client_destid) && u.connected_to) {
				rc = disconnect(u.client_msubid);
				if (rc) {
					ERR("Failed to disconnect destid(0x%X), msubid(0x%X)\n",
					client_destid, u.client_msubid);
				} else {
					INFO("%s disconnected from destid(0x%X)\n",
							name.c_str(), u.client_destid);
					u.client_destid = 0xFFFF;
					u.client_msubid = 0;
					u.client_to_lib_tx_eng_h = 0;
					u.connected_to = false;
					rc = 0;
				}
			}
		}
		sem_post(&users_sem);
	}

	return rc;
} /* disconnect_from_destid() */

/* Disconnect only connection with specified client_msubid */
// FIXME: Get rid of POSIX message queues.
int mspace::disconnect(uint32_t client_msubid)
{
	/* Form message queue name from memory space name */
	stringstream mq_name;
	mq_name << '/' << name;

	/* Open message queue */
	msg_q<mq_rdma_msg>	*disconnect_mq;
	try {
		disconnect_mq = new msg_q<mq_rdma_msg>(mq_name.str(), MQ_OPEN);
	}
	catch(msg_q_exception& e) {
		ERR("Failed to open disconnect_mq('%s'): %s\n",
					mq_name.str().c_str(), e.msg.c_str());
		return -1;
	}

	/* Message buffer */
	mq_rdma_msg	*rdma_msg;
	disconnect_mq->get_send_buffer(&rdma_msg);

	/* Message type */
	rdma_msg->type = MQ_DISCONNECT_MS;

	/* Message contents */
	mq_disconnect_msg *disconnect_msg = &rdma_msg->disconnect_msg;
	disconnect_msg->client_msubid = client_msubid;

	int rc = 0;
	/* Send MQ_DISCONNECT_MS to RDMA library */
	if (disconnect_mq->send()) {
		ERR("Failed to send back MQ_DISCONNECT_MS on '%s'\n",
							mq_name.str().c_str());
		rc = -2;
	} else {
		HIGH("MQ_DISCONNECT_MS relayed to librdma on '%s', msubid(0x%X)\n",
				mq_name.str().c_str(), client_msubid);
	}

	/* Close message queue */
	delete disconnect_mq;

	return rc;
} /* disconnect() */

set<uint16_t> mspace::get_rem_destids()
{
	sem_wait(&users_sem);
	set<uint16_t>	rem_destids;
	if (connected_to)
		rem_destids.insert(client_destid);
	for( auto& u : users) {
		if (u.connected_to)
			rem_destids.insert(u.client_destid);
	}
	sem_post(&users_sem);
	return rem_destids;
} /* get_rem_destids() */

/* Debugging */
void mspace::dump_info(struct cli_env *env)
{
	sprintf(env->output, "%34s %08X %08X %016" PRIx64 " %08X\n", name.c_str(),
	                msoid, msid, rio_addr, size);
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

void mspace::dump_info_msubs_only(struct cli_env *env) {
	sprintf(env->output, "%8s %16s %8s %8s %16s\n", "msubid", "rio_addr",
	                "size", "msid", "phys_addr");
	logMsg(env);
	sprintf(env->output, "%8s %16s %8s %8s %16s\n", "------", "--------",
	                "----", "----", "---------");
	logMsg(env);

	for (auto& msub : msubspaces) {
		msub.dump_info(env);
	}
} /* dump_info_msubs_only() */

void mspace::dump_info_with_msubs(struct cli_env *env) {
	dump_info(env);
	sem_wait(&msubspaces_sem);
	if (msubspaces.size()) {
		dump_info_msubs_only(env);
	} else {
		puts("No subspaces in above memory space");
	}
	sem_post(&msubspaces_sem);
	sprintf(env->output, "\n"); /* Extra line */
	logMsg(env);
} /* dump_info_with_msubs() */

/* For creating a memory sub-space */
int mspace::create_msubspace(uint32_t offset, uint32_t req_size, uint32_t *size,
                uint32_t *msubid, uint64_t *rio_addr, uint64_t *phys_addr)
{
	/* Make sure we don't straddle memory space boundaries */
	if ((offset + req_size) > this->size) {
		ERR("Out of range: offset(0x%X)+req_size(0x%X) > size(0x%X)\n",
				offset, req_size, this->size);
		return -1;
	}

	/* Determine index of new, free, memory sub-space */
	sem_wait(&msubindex_free_list_sem);
	bool *fmsubit = find(msubindex_free_list,
	                msubindex_free_list + MSUBINDEX_MAX + 1, true);
	if (fmsubit == (msubindex_free_list + MSUBINDEX_MAX + 1)) {
		ERR("No free subspace handles!\n");
		sem_post(&msubindex_free_list_sem);
		return -2;
	}
	sem_post(&msubindex_free_list_sem);

	/* Set msub ID as being used */
	*fmsubit = false;

	/* msid in upper 2 bytes, msubindex in lower 2 bytes */
	*msubid = (msid << 16) + (fmsubit - msubindex_free_list);

	/* Msub will be at offset of memory space rio_addr, and same
	 * goes for the physical address */
	*rio_addr = this->rio_addr + offset;
	*phys_addr = this->phys_addr + offset;

	/* Determine actual size of msub */
	*size = req_size;

	/* Add to list of subspaces */
	sem_wait(&msubspaces_sem);
	msubspaces.emplace_back(msid, *rio_addr, *phys_addr, *size, *msubid);
	sem_post(&msubspaces_sem);

	return 0;
} /* create_msubspace() */

int mspace::open(uint32_t *msid, tx_engine<unix_server, unix_msg_t> *user_tx_eng,
					uint32_t *ms_conn_id, uint32_t *bytes)
{
	/* Return msid, mso_open_id, and bytes */
	*ms_conn_id 	= this->current_ms_conn_id++;
	*msid 		= this->msid;
	*bytes 		= this->size;

	/* Store info about user that opened the ms in the 'users' list */
	sem_wait(&users_sem);
	DBG("user with user_tx_eng(%p), ms_conn_id(0x%X) stored in msid(0x%X)\n",
						user_tx_eng, *ms_conn_id, *msid);
	users.emplace_back(*ms_conn_id, user_tx_eng);
	sem_post(&users_sem);

	return 0;
} /* open() */

tx_engine<unix_server, unix_msg_t> *mspace::get_accepting_tx_eng()
{
	tx_engine<unix_server, unix_msg_t> *tx_eng = nullptr;

	if (accepting)
		tx_eng = creator_tx_eng;
	else {
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

/**
 * @app_tx_eng could be the tx_eng of the creator of the mspace, or
 * one of its users. Find it and SET the appropriate is_accepting
 * flag (i.e. either in the main class or in one of the ms_users.
 *
 * Only 1 connection to the memory space can be accepting at a time.
 * So make sure none are before allowing one to accept. Furthermore,
 * the one that accepts cannot be already connected-to.
 */
int mspace::accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng,
		   uint32_t server_msubid)
{
	int rc;

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
			sem_wait(&users_sem);
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
			sem_post(&users_sem);
		}
	} else { /* It is not the creator who is trying to 'accept' */
		sem_wait(&users_sem);
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
				ERR("'%s' already accepting from creator app\n");
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
					ERR("'%s' already accepting from a user app\n");
					rc = RDMA_ACCEPT_FAIL;
				} else {
					/* All is good, set 'accepting' flag */
					it->accepting = true;;
					it->server_msubid = server_msubid;
					rc = 0;
				}
			}
		}
		sem_post(&users_sem);
	}

	return rc;
} /* accept() */

/**
 * @app_tx_eng could be the tx_eng of the creator of the mspace, or
 * one of its users. Find it and CLEAR the appropriate is_accepting
 * flag (i.e. either in the main class or in one of the ms_users).
 * Also remove the server_msubid (by setting it to 0). */
int mspace::undo_accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	auto rc = 0;

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
		}
	} else {
		/* It wasn't the creator so search the users by app_tx_eng */
		sem_wait(&users_sem);
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
				it->accepting = false;
				it->server_msubid = 0;
			}
		}
		sem_post(&users_sem);
	}
	return rc;
} /* undo_accept() */

bool mspace::has_user_with_user_tx_eng(
		tx_engine<unix_server, unix_msg_t> *user_tx_eng,
		uint32_t *ms_conn_id)
{
	bool has_user = false;

	sem_wait(&users_sem);
	auto it = find(begin(users), end(users), user_tx_eng);

	if (it != end(users)) {
		*ms_conn_id = it->get_ms_conn_id();
		has_user = true;
	}
	sem_post(&users_sem);

	return has_user;
} /* has_user_with_user_server() */

bool mspace::connected_by_destid(uint16_t client_destid)
{
	bool connected = false;

	connected = (this->client_destid == client_destid);

	sem_wait(&users_sem);
	for(auto& u : users) {
		if (u.connected_to_destid(client_destid)) {
			connected = true;
			break;
		}
	}
	sem_post(&users_sem);

	return connected;
} /* connected_by_destid() */

int mspace::close(tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	int rc;

	/* A creator of an ms cannot open/close it */
	if (app_tx_eng == creator_tx_eng) {
		ERR("Creator of memory space cannot open/close it\n");
		return RDMA_MS_CLOSE_FAIL;
	}

	/* Before closing a memory space, tell its clients that it is being
	 * closed (connection must be dropped) and have them acknowledge. */
	if (notify_remote_clients()) {
		WARN("Failed to notify some or all remote clients\n");
	}

	/* Locate a user element by the app's tx engine. Then erase it! */
	sem_wait(&users_sem);
	auto it = find(begin(users), end(users), app_tx_eng);
	if (it == end(users)) {
		WARN("Failed to find open connection!\n");
		rc = RDMA_MS_CLOSE_FAIL;
	} else {
		users.erase(it);
		rc = 0;	/* Success */
	}
	sem_post(&users_sem);

	return rc;
} /* close() */

int mspace::destroy_msubspace(uint32_t msubid)
{
	int rc;

	sem_wait(&msubspaces_sem);
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
	sem_post(&msubspaces_sem);

	return rc;
} /* destroy_msubspace() */

