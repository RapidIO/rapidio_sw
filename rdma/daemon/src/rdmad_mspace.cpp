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
#include "rdmad_cm.h"
#include "rdma_mq_msg.h"
#include "rdmad_mspace.h"
#include "rdmad_msubspace.h"
#include "rdmad_ms_owner.h"
#include "rdmad_ms_owners.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_main.h"

mspace::mspace(const char *name, uint32_t msid, uint64_t rio_addr,
                uint64_t phys_addr, uint64_t size) :
		name(name), msid(msid), rio_addr(rio_addr), phys_addr(
		                phys_addr), size(size), free(true),
		                current_ms_conn_id(MS_CONN_ID_START),
		                accepted(false)
{
	INFO("name=%s, msid=0x%08X, rio_addr=0x%lX, size=0x%lX\n", name, msid,
	                rio_addr, size);

	/* Initially all free list sub-indexes are available */
	fill(msubindex_free_list, msubindex_free_list + MSUBINDEX_MAX + 1,
	                true);

	/* Initialize semaphores that will protect the lists */
	if (sem_init(&destids_sem, 0, 1) == -1) {
		CRIT("Failed to initialize destids_sem: %s\n", strerror(errno));
		throw mspace_exception("Failed to initialize destids_sem");
	}

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

mspace::~mspace() {
	if (!free)
		destroy();
} /* destructor */

int mspace::notify_remote_clients() {
	DBG("msid(0x%X) '%s' has %u destids associated therewith\n", msid,
	                name.c_str(), destids.size());

	/* For each element in the destids list, send a destroy message */
	sem_wait(&destids_sem);
	for (auto& destid : destids ) {
		/* Need to use a 'prov' socket to send the CM_DESTROY_MS */
		sem_wait(&prov_daemon_info_list_sem);
		auto prov_it = find(begin(prov_daemon_info_list),
		                end(prov_daemon_info_list), destid);
		if (prov_it == end(prov_daemon_info_list)) {
			ERR("Could not find entry for destid(0x%X)\n", destid);
			sem_post(&prov_daemon_info_list_sem);
			continue; /* Better luck next time? */
		}
		if (!prov_it->conn_disc_server) {
			ERR("conn_disc_server for destid(0x%X) is NULL",
			                destid);
			sem_post(&prov_daemon_info_list_sem);
			continue; /* Better luck next time? */
		}
		cm_server *destroy_server = prov_it->conn_disc_server;
		sem_post(&prov_daemon_info_list_sem);

		/* Prepare destroy message */
		cm_destroy_msg *dm;
		destroy_server->get_send_buffer((void **) &dm);
		dm->type = htobe64(CM_DESTROY_MS);
		strcpy(dm->server_msname, name.c_str());
		dm->server_msid = htobe64(msid);

		/* Send to remote daemon @ 'destid' */
		if (destroy_server->send()) {
			WARN("Failed to send destroy to destid(0x%X)\n",
			                destid);
			continue;
		}
		INFO("Sent cm_destroy_msg for %s to remote daemon on destid(0x%X)\n",
		                dm->server_msname, destid);

		/* Wait for destroy acknowledge message, but with timeout.
		 * If no ACK within say 1 second, then move on */
		cm_destroy_ack_msg *dam;
		destroy_server->flush_recv_buffer();
		destroy_server->get_recv_buffer((void **) &dam);
		if (destroy_server->timed_receive(1000)) {
			/* In this case whether the return value is ETIME or a failure
			 * code is irrelevant. The main thing is NOT to be stuck here.
			 */
			ERR("Did not receive destroy_ack from destid(0x%X)\n",
			                destid);
			continue;
		}
		if (dam->type == htobe64(CM_DESTROY_ACK_MS)) {
			if (dam->server_msid != dm->server_msid) { /* Both are BE */
				ERR("Received destroy_ack with wrong msid(0x%X)\n",
						dam->server_msid);
			} else {
				HIGH("destroy_ack received from daemon destid(0x%X)\n",
						destid);
			}
		} else {
			ERR("INVALID MESSAGE TYPE: 0x%X\n", htobe64(dam->type));
		}
	} /* for() */

	/* Now clear the list */
	destids.clear();

	sem_post(&destids_sem);

	return 0;
} /* notify_remote_clients() */

int mspace::close_connections()
{
	sem_wait(&users_sem);

	HIGH("Sending close messages to applications which have 'open'ed '%s'\n",
	                name.c_str());

	/* Tell local apps which have opened the ms that the ms will be destroyed */
	for (ms_user& user : users) {
		struct mq_close_ms_msg *close_msg;

		user.get_mq()->get_send_buffer(&close_msg);
		close_msg->msid = msid;

		if (user.get_mq()->send()) {
			ERR("Failed to close queue for msid(0x%X)\n", msid);
		}
		delete user.get_mq();
	}

	users.clear();

	sem_post(&users_sem);

	return 0;
} /* close_connections() */

int mspace::destroy() {
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
	set_msoid(0x000); /* No owner */
	name = "freemspace";
	accepted = false; /* No connections */
	DBG("name=%s, msid=0x%08X, rio_addr=0x%lX, size=0x%lX\n", name.c_str(),
	                msid, rio_addr, size);

	/* Remove memory space identifier from owner */
	ms_owner *owner;
	try {
		owner = owners[msoid];
	} catch (...) {
		ERR("Failed to find owner msoid(0x%X)\n", msoid);
		return -3;

	}
	if (!owner) {
		ERR("Failed to find owner msoid(0x%X)\n", msoid);
		return -4;
	} else if (owner->remove_ms(this) < 0) {
		WARN("Failed to remove ms from owner\n");
		return -5;
	}

	return 0;
} /* destroy() */

void mspace::add_destid(uint16_t destid) {
	sem_wait(&destids_sem);
	destids.push_back(destid);
	sem_post(&destids_sem);
} /* add_destid() */

int mspace::remove_destid(uint16_t destid) {
	sem_wait(&destids_sem);
	auto it = find(begin(destids), end(destids), destid);
	if (it == end(destids)) {
		WARN("%u not found in %s\n", destid, name.c_str());
		sem_post(&destids_sem);
		return -1;
	}
	destids.erase(it);
	sem_post(&destids_sem);
	return 1;
} /* remove_destid() */

/* Debugging */
void mspace::dump_info(struct cli_env *env) {
	sprintf(env->output, "%34s %08X %016" PRIx64 " %08X\n", name.c_str(),
	                msid, rio_addr, size);
	logMsg(env);
	sprintf(env->output, "destids: ");
	logMsg(env);

	sem_wait(&destids_sem);
	for (auto& destid : destids) {
		sprintf(env->output, "%u ", destid);
		logMsg(env);
	}
	sem_post(&destids_sem);
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

	sem_wait(&msubspaces_sem);
	for (auto& msub : msubspaces) {
		msub.dump_info(env);
	}
	sem_post(&msubspaces_sem);
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
		ERR("offset(0x%X)+req_size(0x%X) OOR\n", offset, req_size);
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

	return 1;
} /* create_msubspace() */

int mspace::open(uint32_t *msid, unix_server *user_server, uint32_t *ms_conn_id, uint32_t *bytes)
{
	/* Return msid, mso_open_id, and bytes */
	*ms_conn_id 	= this->current_ms_conn_id++;
	*msid 		= this->msid;
	*bytes 		= this->size;

	/* Create POSIX message queue */
	stringstream qname;
	qname << '/' << name << *ms_conn_id;
	msg_q<mq_close_ms_msg> *close_mq;
	try {
		close_mq = new msg_q<mq_close_ms_msg>(qname.str(), MQ_CREATE);
	} catch (msg_q_exception e) {
		CRIT("Failed to create close_mq: %s\n", e.msg.c_str());
		return -1;
	}

	/* Store info about user that opened the ms in the 'users' list */
	sem_wait(&users_sem);
	DBG("user with user_server(%p), ms_conn_id(0x%X) stored in msid(0x%X)\n",
			user_server, *ms_conn_id, *msid);
	users.emplace_back(user_server, *ms_conn_id, close_mq);
	sem_post(&users_sem);

	return 1;
} /* open() */

bool mspace::has_user_with_user_server(unix_server *server, uint32_t *ms_conn_id)
{
	auto it = find(begin(users), end(users), server);

	if (it != end(users)) {
		*ms_conn_id = it->get_ms_conn_id();
		return true;
	}
	return false;
}

int mspace::close(uint32_t ms_conn_id)
{
	int rc;

	/* Before closing a memory space, tell its clients that it is being
	 * closed (connection must be dropped) and have them acknowledge. */
	if (notify_remote_clients()) {
		WARN("Failed to notify some or all remote clients\n");
	}

	/* If the memory space was in accepted state, clear that state */
	/* FIXME: This assumes only 1 'open' to the ms and that the creator
	 * of the ms does not call 'accept' on it.
	 * There is another problem. If the owner of the ms does accept
	 * and the user tries to accept and fails the user may try to close
	 * the 'ms. The statement below will cause the 'ms' to have
	 * accepted = false even though the owner had set it to true. Next
	 * time a user tries to do accept the flag will be false and 2 accepts
	 * will be in effect. This needs to be thought through */
	accepted = false;

	sem_wait(&users_sem);
	auto it = find(begin(users), end(users), ms_conn_id);
	if (it == end(users)) {
		WARN("ms_conn_id(0x%X) not found in user list\n", ms_conn_id);
		rc = -1;
	} else {
		delete (*it).get_mq();
		users.erase(it);
		rc = 1;
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
		rc = 1;
	}
	sem_post(&msubspaces_sem);

	return rc;
} /* destroy_msubspace() */

