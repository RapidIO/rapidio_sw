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

#include <list>
#include <algorithm>
#include <exception>
#include <mutex>
#include <string>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "rdma_types.h"
#include "liblog.h"

#include "librdma_db.h"

using std::list;
using std::exception;
using std::mutex;
using std::lock_guard;
using std::string;

using loc_mso_list_t  = list<loc_mso *>;
using loc_ms_list_t   = list<loc_ms *>;
using rem_ms_list_t   = list<rem_ms *>;
using loc_msub_list_t = list<loc_msub *>;
using rem_msub_list_t = list<rem_msub *>;

static loc_mso_list_t loc_mso_list;
static loc_ms_list_t  loc_ms_list;
static rem_ms_list_t  rem_ms_list;
static loc_msub_list_t loc_msub_list;
static rem_msub_list_t rem_msub_list;

mutex loc_mso_mutex;
mutex loc_ms_mutex;
mutex rem_ms_mutex;
mutex loc_msub_mutex;
mutex rem_msub_mutex;

mso_h add_loc_mso(const char *mso_name, uint32_t msoid, bool owned)
{
	loc_mso *mso;
	try {
		/* Allocate */
		mso = new loc_mso(mso_name, msoid, owned);

		/* Add to list */
		lock_guard<mutex> lock(loc_mso_mutex);
		loc_mso_list.push_back(mso);
	}
	catch(exception& e) {
		WARN("Failed to allocate local mso: %s\n", e.what());
		mso = NULL;
	}
	return (mso_h)mso;
} /* add_loc_mso() */

int remove_loc_mso(mso_h msoh)
{
	int rc;

	/* Check for NULL msh */
	if (!msoh) {
		WARN("NULL msoh passed\n");
		rc = -1;
	} else {
		/* Find the mso defined by msoh */
		lock_guard<mutex> lock(loc_mso_mutex);
		auto it = find(begin(loc_mso_list), end(loc_mso_list), (loc_mso *)msoh);
		if (it == end(loc_mso_list)) {
			WARN("msoh = 0x%" PRIx64 " not found\n", msoh);
			rc = -2;
		} else {
			/* Free the mso, and remove from list */
			delete *it;		/* Free mso struct */
			loc_mso_list.erase(it);	/* Remove pointer from list */
			rc = 0;
		}
	}

	return rc;
} /* remove_loc_mso() */

/**
 * Finds mso with a particular msoid
 */
struct has_msoid {
	has_msoid(uint32_t msoid) : msoid(msoid) {}
	bool operator()(loc_mso* mso) {
		return mso->msoid == msoid;
	}
private:
	uint32_t msoid;
}; /* has_msoid */

int remove_loc_mso(uint32_t msoid)
{
	int rc;

	/* Find the mso identified by msoid */
	lock_guard<mutex> lock(loc_mso_mutex);
	auto it = find_if(begin(loc_mso_list), end(loc_mso_list), has_msoid(msoid));
	if (it == end(loc_mso_list)) {
		WARN("msoid = 0x%X not found\n", msoid);
		rc = -1;
	} else {
		/* Free the mso, and remove from list */
		delete *it;		/* Free mso struct */
		loc_mso_list.erase(it);	/* Remove pointer from list */
		rc = 0;
	}

	return rc;
} /* remove_loc_mso() */

void purge_loc_mso_list()
{
	lock_guard<mutex> lock(loc_mso_mutex);
	for_each(begin(loc_mso_list), end(loc_mso_list), [](loc_mso *mso) { delete mso; });
	loc_mso_list.clear();
	HIGH("Local mso list purged!!!!\n");
} /* purge_loc_mso() */

bool mso_h_exists(mso_h msoh)
{
	bool rc;

	loc_mso *mso = (loc_mso *)msoh;
	if (mso == NULL) {
		WARN("Null argument (msoh). Returning false\n");
		rc = false;
	} else {
		lock_guard<mutex> lock(loc_mso_mutex);
		rc = find(begin(loc_mso_list), end(loc_mso_list), mso)
							!= end(loc_mso_list);
	}

	return rc;
} /* mso_h_exists() */

mso_h find_mso(uint32_t msoid)
{
	lock_guard<mutex> lock(loc_mso_mutex);
	auto it = find_if(begin(loc_mso_list), end(loc_mso_list), has_msoid(msoid));
	return (it == end(loc_mso_list)) ? (mso_h)(*it) : (mso_h)NULL;
} /* find_mso */

struct has_mso_name {
	has_mso_name(const char *name) : name(name) {}
	bool operator()(loc_mso* mso) {
		return this->name == mso->name;
	}
private:
	const char *name;
}; /* has_mso_name */

mso_h find_mso_by_name(const char *name)
{
	mso_h	msoh;

	lock_guard<mutex> lock(loc_mso_mutex);
	auto it = find_if(begin(loc_mso_list), end(loc_mso_list),
							has_mso_name(name));
	if (it == end(loc_mso_list)) {
		WARN("mso with name = '%s' not found\n", name);
		msoh = (mso_h)NULL;
	} else {
		msoh =  (mso_h)(*it);
	}

	return msoh;
} /* find_mso_by_name() */

ms_h add_loc_ms(const char *ms_name,
		uint64_t bytes,
		mso_h msoh,
		uint32_t msid,
		uint64_t phys_addr,
		uint64_t rio_addr,
		bool owned)
{
	loc_ms *msp;
	try {
		/* Construct */
		msp = new loc_ms(ms_name, bytes, msoh, msid, phys_addr,
				rio_addr, owned);
		/* Add to list */
		lock_guard<mutex> lock(loc_ms_mutex);
		loc_ms_list.push_back(msp);
		DBG("Added %s, 0x%X to db\n", ms_name, msid);
	}
	catch(exception& e) {
		CRIT("Failed to allocate local ms: %s\n", e.what());
		msp = NULL;
	}

	return (ms_h)msp;
} /* add_loc_ms() */

int remove_loc_ms(ms_h msh)
{
	int rc;

	/* Check for NULL msh */
	if (!msh) {
		ERR("NULL msh passed\n");
		rc = -1;
	} else {
		/* Find the ms defined by msh */
		lock_guard<mutex> lock(loc_ms_mutex);
		auto it = find(begin(loc_ms_list), end(loc_ms_list), (loc_ms *)msh);
		if (it == loc_ms_list.end()) {
			ERR("msh = 0x%" PRIx64 " not found\n", msh);
			rc = -2;
		} else {
			/* Free the ms, and remove from list */
			delete *it;		/* Free ms struct */
			loc_ms_list.erase(it);	/* Remove pointer from list */
			rc = 0;
		}
	}

	return rc;
} /* remove_loc_ms() */

void purge_loc_ms_list()
{
	lock_guard<mutex> lock(loc_ms_mutex);
	for (auto& ms : loc_ms_list)	delete ms;
	loc_ms_list.clear();
	HIGH("Local ms list purged!!!!\n");
} /* purge_loc_ms_list() */

/**
 * @brief Matches a memory space (local or remote) by msid
 *
 * @param msid Memory space identifier
 *
 * @return true if memory space has the specified msid, false otherwise
 */
template <typename T>
struct has_this_msid {
	has_this_msid(uint32_t msid) : msid(msid) {}

	bool operator()(T *msp) {
		return msp->msid == this->msid;
	}
private:
	uint32_t msid;
};

ms_h find_loc_ms(uint32_t msid)
{
	lock_guard<mutex> lock(loc_ms_mutex);
	auto it = find_if(begin(loc_ms_list), end(loc_ms_list),
				has_this_msid<loc_ms>(msid));
	return (it != end(loc_ms_list)) ? (ms_h)(*it) : (ms_h)NULL;
} /* find_loc_ms() */

struct has_ms_name {
	has_ms_name(const char *name) : name(name) {}
	bool operator()(loc_ms* ms) {
		return this->name == ms->name;
	}
private:
	const char *name;
};

ms_h find_loc_ms_by_name(const char *ms_name)
{
	lock_guard<mutex> lock(loc_ms_mutex);
	auto it = find_if(begin(loc_ms_list), end(loc_ms_list), has_ms_name(ms_name));
	return (it != end(loc_ms_list)) ? (ms_h)(*it) : (ms_h)NULL;
} /* find_loc_ms_by_name() */

/**
 * @brief returns true if the ms has owner msoh.
 */
struct has_this_owner {
	has_this_owner(mso_h msoh) : msoh(msoh) {}

	bool operator()(loc_ms *msp) {
		return msp->msoh == this->msoh;
	}

private:
	mso_h msoh;
}; /* has_this_owner() */

unsigned get_num_ms_by_msoh(mso_h msoh)
{
	lock_guard<mutex> lock(loc_ms_mutex);
	return count_if(begin(loc_ms_list), end(loc_ms_list), has_this_owner(msoh));
} /* get_num_ms_by_msoh() */

void get_list_msh_by_msoh(mso_h msoh, msp_list& ms_list)
{
	lock_guard<mutex> lock(loc_ms_mutex);
	copy_if(begin(loc_ms_list),
		end(loc_ms_list),
		begin(ms_list),
		has_this_owner(msoh));
} /* get_list_msh_by_msoh() */

bool loc_ms_exists(ms_h msh)
{
	lock_guard<mutex> lock(loc_ms_mutex);
	return find(begin(loc_ms_list), end(loc_ms_list), (loc_ms *)msh)
							!= end(loc_ms_list);
} /* loc_ms_exists() */

void dump_loc_ms(void)
{
	lock_guard<mutex> lock(loc_ms_mutex);
	for (auto it = begin(loc_ms_list); it != end(loc_ms_list); it++) {
		DBG("msh = 0x%p\t", (*it));
	}
} /* dump_loc_ms() */

ms_h add_rem_ms(const char *name, uint32_t msid)
{
	rem_ms *msp;

	try {
		msp = new rem_ms(name, msid);

		/* Add to list */
		lock_guard<mutex> lock(rem_ms_mutex);
		rem_ms_list.push_back(msp);

		DBG("Added %s, msid(0x%X) to db, database size=%u\n",
				name, msid, rem_ms_list.size());
	}
	catch(...) {
		CRIT("Failed to allocate local ms.\n");
		msp = NULL;
	}
	return (ms_h)msp;
} /* add_rem_ms() */

int remove_rem_ms(uint32_t msid)
{
	int rc;

	lock_guard<mutex> lock(rem_ms_mutex);
	auto it = find_if(begin(rem_ms_list),
			  end(rem_ms_list),
			  has_this_msid<rem_ms>(msid));
	if (it == end(rem_ms_list)) {
		ERR("msid(0x%X) not found in database!\n", msid);
		rc = -1;
	} else {
		/* Free the ms, and remove from list */
		delete *it;		/* Free ms struct */
		rem_ms_list.erase(it);	/* Remove pointer from list */
		DBG("Now database has size = %d\n", rem_ms_list.size());
		rc = 0;
	}

	return rc;
} /* remove_rem_ms() */

int remove_rem_ms(ms_h msh)
{
	int rc;

	/* Check for NULL msh */
	if (!msh) {
		ERR("NULL msh passed\n");
		rc = -1;
	} else {
		/* Find the ms defined by msh */
		lock_guard<mutex> lock(rem_ms_mutex);
		auto it = find(begin(rem_ms_list), end(rem_ms_list), (rem_ms *)msh);
		if (it == end(rem_ms_list)) {
			WARN("msh = 0x%" PRIx64 " not found\n", msh);
			rc = -2;
		} else {
			/* Free the ms, and remove from list */
			delete *it;		/* Free ms struct */
			rem_ms_list.erase(it);	/* Remove pointer from list */
			DBG("Now database has size = %d\n", rem_ms_list.size());
			rc = 0;
		}
	}

	return rc;
} /* remove_rem_ms() */

ms_h find_rem_ms(uint32_t msid)
{
	lock_guard<mutex> lock(rem_ms_mutex);
	auto it = find_if(begin(rem_ms_list),
			  end(rem_ms_list),
			  has_this_msid<rem_ms>(msid));
	return (it != end(rem_ms_list)) ? (ms_h)(*it) : (ms_h)NULL;
} /* find_rem_ms() */

bool rem_ms_exists(ms_h msh)
{
	lock_guard<mutex> lock(rem_ms_mutex);
	return find(begin(rem_ms_list), end(rem_ms_list), (rem_ms *)msh)
							!= end(rem_ms_list);
} /* rem_ms_exists() */

ms_h add_loc_msub(uint32_t 	msubid,
		  uint32_t	msid,
		  uint32_t	bytes,
		  uint8_t	rio_addr_len,
		  uint64_t	rio_addr_lo,
		  uint8_t	rio_addr_hi,
		  uint64_t	paddr)
{
	loc_msub *msubp;
	try {	/* Create msub */
		msubp = new loc_msub(msid, msubid, bytes, rio_addr_len,
					rio_addr_lo, rio_addr_hi, paddr);
		/* Store */
		lock_guard<mutex> lock(loc_msub_mutex);
		loc_msub_list.push_back(msubp);
	}
	catch(std::exception& e) {
		ERR("Failed to create msub: %s\n", e.what());
		msubp = NULL;
	}
	return (msub_h)msubp;
} /* add_loc_msub() */

int remove_loc_msub(msub_h msubh)
{
	int rc;

	/* Check for NULL msubh */
	if (!msubh) {
		ERR("NULL msubh passed\n");
		rc = -1;
	}

	/* Find the msub defined by msubh */
	lock_guard<mutex> lock(loc_msub_mutex);
	auto it = find(begin(loc_msub_list),
		       end(loc_msub_list),
		       (loc_msub *)msubh);
	if (it == end(loc_msub_list)) {
		WARN("msubh = 0x%" PRIx64 " not found\n", msubh);
		rc = -2;
	} else {
		/* Free the msub, and remove from list */
		delete *it;		/* Free msub struct */
		loc_msub_list.erase(it);/* Remove pointer from list */
		rc = 0;
		INFO("msubh successfully removed, returning %d\n", rc);
	}

	return rc;
} /* remove_loc_msub() */

/**
 * Returns true if loc_msub has the specified msubid.
 */
struct loc_has_this_msubid {
	loc_has_this_msubid(uint32_t msubid) : msubid(msubid) {}

	bool operator()(struct loc_msub *msubp) {
		return msubp->msubid == this->msubid;
	}
private:
	uint32_t msubid;
};

msub_h find_loc_msub(uint32_t msubid)
{
	lock_guard<mutex> lock(loc_msub_mutex);
	auto it = find_if(begin(loc_msub_list),
			  end(loc_msub_list),
			  loc_has_this_msubid(msubid));
	return (it != end(loc_msub_list)) ? (msub_h)(*it) : (msub_h)NULL;
} /* find_loc_msub() */

msub_h find_loc_msub_by_connh(conn_h connh)
{
	lock_guard<mutex> lock(loc_msub_mutex);
	auto it = find_if(begin(loc_msub_list),
			  end(loc_msub_list),
			  [connh](loc_msub *msubp)
			  {
				return find_if(begin(msubp->connections),
					    end(msubp->connections),
					    [connh](client_connection& c)
					    {
						return c.connh == connh;
					    }) != end(msubp->connections);
			  });

	return (it != loc_msub_list.end()) ? (msub_h)(*it) : (msub_h)NULL;
} /* find_loc_msub_by_connh() */

/**
 * Returns true if loc_msub has memory space identifier 'msid'
 */
struct loc_msub_has_this_msid {
	loc_msub_has_this_msid(uint32_t msid) : msid(msid) {}

	bool operator()(struct loc_msub *msubp) {
		return msubp->msid == this->msid;
	}

private:
	uint32_t msid;
}; /* loc_msub_has_this_msid */

unsigned get_num_loc_msub_in_ms(uint32_t msid)
{
	lock_guard<mutex> lock(loc_msub_mutex);
	return count_if(begin(loc_msub_list),
		        end(loc_msub_list),
			loc_msub_has_this_msid(msid));
} /* get_num_loc_msub_in_ms() */

void get_list_loc_msub_in_msid(uint32_t msid, msubp_list& msub_list)
{
	lock_guard<mutex> lock(loc_msub_mutex);
	copy_if(begin(loc_msub_list),
		end(loc_msub_list),
		begin(msub_list),
		loc_msub_has_this_msid(msid));
} /* get_list_loc_msubh_in_msid() */

void purge_loc_msub_list()
{
	lock_guard<mutex> lock(loc_msub_mutex);
	for (auto& msub : loc_msub_list) delete msub;
	loc_msub_list.clear();
	HIGH("Local msub list purged!!!!\n");
} /* purge_loc_msub_list() */

msub_h add_rem_msub(uint32_t	rem_msubid,
		    uint32_t	rem_msid,
		    uint32_t	rem_bytes,
		    uint8_t	rem_rio_addr_len,
		    uint64_t	rem_rio_addr_lo,
		    uint8_t	rem_rio_addr_hi,
		    uint8_t	destid_len,
		    uint32_t	destid,
		    ms_h	loc_msh)
{
	rem_msub *msubp;
	try {
		/* Allocate space for new memory subspace */
		msubp = new rem_msub(rem_msubid, rem_msid, rem_bytes,
			rem_rio_addr_len, rem_rio_addr_lo, rem_rio_addr_hi,
			destid_len, destid, loc_msh);

		/* Add to list */
		lock_guard<mutex> lock(rem_msub_mutex);
		rem_msub_list.push_back(msubp);
	}
	catch(...) {
		CRIT("Failed to allocate rem_msub");
		msubp = NULL;
	}
	return (msub_h)msubp;
} /* add_rem_msub() */

/**
 * @brief Returns true if remote msub has specified msubid.
 */
struct rem_has_this_msubid {
	rem_has_this_msubid(uint32_t msubid) : msubid(msubid) {}

	bool operator()(struct rem_msub *msubp) {
		return msubp->msubid == this->msubid;
	}
private:
	uint32_t msubid;
};

int remove_rem_msub(uint32_t msubid)
{
	int rc;
	lock_guard<mutex> lock(rem_msub_mutex);
	auto it = find_if(begin(rem_msub_list),
			  end(rem_msub_list),
			  rem_has_this_msubid(msubid));
	if (it == rem_msub_list.end()) {
		WARN("msubid(0x%X) not found\n", msubid);
		rc = -2;
	} else {
		/* Free the msub, and remove from list */
		delete *it;		/* Free msub struct */
		rem_msub_list.erase(it);/* Remove pointer from list */
		rc = 0;
	}
	return rc;
} /* remove_rem_msub() */

int remove_rem_msub(msub_h msubh)
{
	int rc;

	/* Check for NULL msubh */
	if (!msubh) {
		ERR("NULL msubh passed\n");
		rc = -1;
	} else {
		/* Find the msub defined by msubh */
		lock_guard<mutex> lock(rem_msub_mutex);
		auto it = find(begin(rem_msub_list), end(rem_msub_list), (rem_msub *)msubh);
		if (it == rem_msub_list.end()) {
			WARN("msubh = 0x%" PRIx64 " not found\n", msubh);
			rc = -2;
		} else {
			/* Free the msub, and remove from list */
			delete *it;		/* Free msub struct */
			rem_msub_list.erase(it);/* Remove pointer from list */
			rc = 0;
		}
	}

	return rc;
} /* remove_rem_msub() */

msub_h find_rem_msub(uint32_t msubid)
{
	lock_guard<mutex> lock(rem_msub_mutex);
	auto it = find_if(begin(rem_msub_list),
			  end(rem_msub_list),
			  rem_has_this_msubid(msubid));
	return (it != rem_msub_list.end()) ? (msub_h)(*it) : (msub_h)NULL;
} /* find_rem_msub() */

unsigned get_num_rem_msubs(void)
{
	lock_guard<mutex> lock(rem_msub_mutex);
	return rem_msub_list.size();
} /* get_num_rem_msubs() */

/**
 * @brief Returns true if rem_msub has memory space identifier 'msid'
 */
struct rem_msub_has_this_msid {
	rem_msub_has_this_msid(uint32_t msid) : msid(msid) {}

	bool operator()(struct rem_msub *msubp) {
		return msubp->msid == this->msid;
	}
private:
	uint32_t msid;
};

void remove_rem_msubs_in_ms(uint32_t msid)
{
	lock_guard<mutex> lock(rem_msub_mutex);
	rem_msub_list.erase(
		remove_if(
			begin(rem_msub_list),
			end(rem_msub_list),
			rem_msub_has_this_msid(msid)),
		end(rem_msub_list)
	);
} /* remove_rem_msub_in_ms() */

msub_h find_any_rem_msub_in_ms(uint32_t msid)
{
	lock_guard<mutex> lock(rem_msub_mutex);
	auto it = find_if(begin(rem_msub_list),
			  end(rem_msub_list),
			  rem_msub_has_this_msid(msid));
	return (it != end(rem_msub_list)) ? (msub_h)(*it) : (msub_h)NULL;
} /* find_any_rem_msub_in_ms() */

/**
 * Returns true if rem_msub is associated with loc_msh
 */
struct rem_msub_has_this_loc_msh {
	rem_msub_has_this_loc_msh(ms_h loc_msh) : loc_msh(loc_msh) {}

	bool operator()(struct rem_msub *msubp) {
		return msubp->loc_msh == this->loc_msh;
	}
private:
	ms_h loc_msh;
};

void remove_rem_msub_by_loc_msh(ms_h loc_msh)
{
	lock_guard<mutex> lock(rem_msub_mutex);

	rem_msub_list.erase(
		remove_if(begin(rem_msub_list),
			  end(rem_msub_list),
			  rem_msub_has_this_loc_msh(loc_msh)),
		end(rem_msub_list)
	);
} /* remove_rem_msub_by_loc_msh() */

void purge_local_database(void)
{
	/* FIXME: How about the remote ms database for example? */
	purge_loc_msub_list();
	purge_loc_ms_list();
	purge_loc_mso_list();
} /* purge_local_database() */
