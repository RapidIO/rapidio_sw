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

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <string>
#include <vector>
#include <algorithm>

#include "rdma_types.h"
#include "rdmad_ms_owner.h"
#include "rdmad_ms_owners.h"
#include "rdmad_main.h"
#include "rdmad_tx_engine.h"

#include "libcli.h"
#include "liblog.h"
#include "unix_sock.h"

using std::vector;
using std::string;
using std::fill;
using std::find;

struct has_msoid {
	has_msoid(uint32_t msoid) : msoid(msoid) {}
	bool operator()(ms_owner *mso) {
		if (!mso) {
			CRIT("NULL mso\n");
			return false;
		}

		return mso->get_msoid() == msoid;
	}
private:
	uint32_t msoid;
};

struct has_mso_name {
	has_mso_name(const char *name) : name(name) {}
	bool operator()(ms_owner *mso) {
		return *mso == name;	/* Use operator==(const char *s) */
	}
private:
	const char *name;
};

struct has_tx_eng {
	has_tx_eng(tx_engine<unix_server, unix_msg_t> *tx_eng) :
		tx_eng(tx_eng) {}
	bool operator()(ms_owner *mso) {
		return mso->get_tx_eng() == this->tx_eng;
	}
private:
	tx_engine<unix_server, unix_msg_t> *tx_eng;
};

ms_owners::ms_owners()
{
	/* Initially all memory space owner handles are free */
	fill(msoid_free_list,msoid_free_list + MSOID_MAX + 1, true);

	/* Reserve msoid to mean "no owner" */
	msoid_free_list[0] = false;

	if (pthread_mutex_init(&owners_lock, NULL)) {
		throw ms_owners_exception("Failed to initialize owners_lock mutex");
	}
} /* Constructor */

ms_owners::~ms_owners()
{
	/* Delete owners */
	for_each(begin(owners), end(owners), [](ms_owner *p) { if (p) delete p;});
}

void ms_owners::dump_info(struct cli_env *env)
{
	sprintf(env->output, "%8s %32s %8s\n", "msoid", "name", "MSIDs owned by mso");
	logMsg(env);
	sprintf(env->output, "%8s %32s %8s\n", "-----", "----", "------------------");
	logMsg(env);
	pthread_mutex_lock(&owners_lock);
	for (auto& owner : owners) {
		owner->dump_info(env);
	}
	pthread_mutex_unlock(&owners_lock);
} /* dump_info() */

int ms_owners::create_mso(const char *name,
			  tx_engine<unix_server, unix_msg_t> *tx_eng,
			  uint32_t *msoid)
{
	int rc;

	try {
		if (!name || !msoid) {
			ERR("Null parameter passed: %p, %p\n", name, msoid);
			throw RDMA_NULL_PARAM;
		}

		/* Find a free memory space owner handle */
		pthread_mutex_lock(&owners_lock);
		bool *fmsoid = find(msoid_free_list,
				msoid_free_list + MSOID_MAX + 1,
				true);

		/* Not found, return with error */
		if (fmsoid == (msoid_free_list + MSOID_MAX + 1)) {
			ERR("Too many memory space owners!\n");
			throw RDMA_INVALID_MSO;
		}
		
		/* Get the free handle */
		*msoid = fmsoid - msoid_free_list;
	
		/* Mark msoid as used */
		*fmsoid = false;

		/* Create an owner with the free ID */
		ms_owner *mso = new ms_owner(name, tx_eng, *msoid);

		/* Store in owners list */
		owners.push_back(mso);
		rc = 0;	/* Success */
		pthread_mutex_unlock(&owners_lock);
	}
	catch(int& e) {
		rc = e;
	}
	return rc;
} /* get_mso() */

int ms_owners::open_mso(const char *name,
			uint32_t *msoid,
			uint32_t *mso_conn_id,
			tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	int rc;

	pthread_mutex_lock(&owners_lock);
	try {
		/* Find the owner having specified name */
		auto mso_it = find_if(begin(owners), end(owners),
						has_mso_name(name));
		if (mso_it == end(owners)) {
			ERR("%s is not a memory space owner's name\n", name);
			throw RDMA_INVALID_MSO;
		}

		/* Open the memory space owner */
		rc = (*mso_it)->open(msoid, mso_conn_id, tx_eng);
		if (rc) {
			ERR("Failed to open memory space owner %s\n", name);
			throw rc;
		}
	}
	catch(int& e) {
		rc = e;
	}
	pthread_mutex_unlock(&owners_lock);

	return 0;
} /* open_mso() */

int ms_owners::close_mso(uint32_t msoid, uint32_t mso_conn_id)
{
	int rc;

	pthread_mutex_lock(&owners_lock);
	try {
		/* Find the mso */
		auto it = find_if(begin(owners), end(owners), has_msoid(msoid));
		if (it == end(owners)) {
			ERR("msoid(0x%X) not found\n", msoid);
			throw RDMA_INVALID_MSO;
		}

		/* Close the connection */
		rc = (*it)->close(mso_conn_id);
		if (rc) {
			ERR("Failed to close connection (0x%X)\n", mso_conn_id);
			throw rc;
		}
	}
	catch(int& e) {
		rc = e;
	}
	pthread_mutex_unlock(&owners_lock);

	return rc;
} /* close_mso() */

void ms_owners::close_mso(tx_engine<unix_server, unix_msg_t>  *user_tx_eng)
{
	for(auto& owner : owners) {
		if (owner->has_user_tx_eng(user_tx_eng)) {
			INFO("Closing conn to owner due to dead socket\n");
			owner->close(user_tx_eng);
		}
	}
} /* close_mso() */

int ms_owners::destroy_mso(tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	int rc;

	pthread_mutex_lock(&owners_lock);

	auto mso_it = find_if(begin(owners), end(owners),
					has_tx_eng(tx_eng));

	/* Not found, warn and return code */
	if (mso_it == owners.end()) {
		WARN("No MSOs with the specified tx_eng(0x%" PRIx64 ")\n",
				(uint64_t)tx_eng);
		rc = -1;
	} else {
		DBG("mso with specified socket found, name='%s'\n",
					(*mso_it)->get_mso_name());

		auto msoid = (*mso_it)->get_msoid();
		assert(msoid <= MSOID_MAX);

		/* Mark msoid as being free */
		msoid_free_list[msoid] = true;
		DBG("msoid(0x%X) now marked as 'free'\n");

		/* Remove owner */
		delete *mso_it;
		owners.erase(mso_it);
		DBG("mso object deleted, and removed from owners list\n");
		rc = 0;
	}

	pthread_mutex_unlock(&owners_lock);

	return rc;
} /* destroy_mso() */

int ms_owners::destroy_mso(uint32_t msoid)
{
	int rc;

	/* Find the owner belonging to msoid */
	pthread_mutex_lock(&owners_lock);
	try {
		auto mso_it = find_if(begin(owners), end(owners),
							has_msoid(msoid));
		/* Not found, return error */
		if (mso_it == owners.end()) {
			ERR("msoid(0x%X) not found\n", msoid);
			throw RDMA_INVALID_MSO;
		}

		DBG("mso with msoid(0x%X) found, name='%s'\n", msoid,
						(*mso_it)->get_mso_name());
		/* Remove owner */
		delete *mso_it;
		owners.erase(mso_it);
		DBG("mso(0x%X) object deleted, and removed from owners list\n",
									msoid);
		/* Mark msoid as being free */
		msoid_free_list[msoid] = true;

		DBG("msoid(0x%X) now marked as 'free'\n");
		rc = 0;	/* Success */
	}
	catch(int& e) {
		rc = e;
	}
	pthread_mutex_unlock(&owners_lock);

	return rc;
} /* destroy_msoid() */

ms_owner* ms_owners::operator[](uint32_t msoid)
{
	auto it = find_if(begin(owners), end(owners), has_msoid(msoid));
	if (it == end(owners)) {
		ERR("Could not find owner with msoid(0x%X)\n", msoid);
		return nullptr;
	} else {
		return *it;
	}
} /* operator[] */
