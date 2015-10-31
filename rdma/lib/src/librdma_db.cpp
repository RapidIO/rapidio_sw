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

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "rdma_types.h"
#include "liblog.h"

#include "librdma_db.h"

using std::list;

static list<struct loc_mso *> loc_mso_list;
static list<struct loc_ms *> loc_ms_list;
static list<struct rem_ms *> rem_ms_list;
static list<struct loc_msub *> loc_msub_list;
static list<struct rem_msub *> rem_msub_list;

/**
 * add_loc_mso
 *
 * Stores a local memory space owner's name and ID in database.
 *
 * @mso_name	Memory space owner name
 * @msoid	Memory space owner identifier
 * @mso_conn_id	Memory space owner connection ID to an app which has it open
 * @owned	true if app created mso, false if it just opened it
 *
 * @return 	Handle to memory space owner, could be 0 (NULL) if failed
 */
mso_h add_loc_mso(const char *mso_name, uint32_t msoid, uint32_t mso_conn_id,
		  bool owned, pthread_t close_notify_thread, msg_q<mq_close_mso_msg> *close_notify_mq)
{
	/* Allocate */
	struct loc_mso *mso = (struct loc_mso *)malloc(sizeof(struct loc_mso));
	if (!mso) {
		WARN("Failed to allocate local mso\n");
		return (mso_h)NULL;
	}

	/* Populate values */
	mso->name 	= strdup(mso_name);
	mso->msoid	= msoid;
	mso->mso_conn_id= mso_conn_id;
	mso->owned	= owned;
	mso->close_notify_thread = close_notify_thread;
	mso->close_notify_mq	 = close_notify_mq;

	/* Add to list */
	loc_mso_list.push_back(mso);

	return (mso_h)mso;
} /* add_loc_mso() */

/**
 * remove_loc_mso
 *
 * Removes the specified local memory space owner from the database.
 *
 * @msoh	Memory space owner's handle
 *
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_loc_mso(mso_h msoh)
{
	/* Check for NULL msh */
	if (!msoh) {
		WARN("NULL msoh passed\n");
		return -1;
	}

	/* Find the mso defined by msoh */
	auto it = find(loc_mso_list.begin(), loc_mso_list.end(), (loc_mso *)msoh);
	if (it == loc_mso_list.end()) {
		WARN("msoh = 0x%lX not found\n", msoh);
		return -2;
	}

	/* Free the mso, and remove from list */
	free((*it)->name);	/* Free name string */
	free((*it));		/* Free mso struct */
	loc_mso_list.erase(it);	/* Remove pointer from list */

	return 0;
} /* remove_loc_mso() */

void purge_loc_mso_list()
{
	for (auto& mso : loc_mso_list) {
		free(mso->name);
		free(mso);
	}
	loc_mso_list.clear();
	HIGH("Local mso list purged!!!!\n");
} /* purge_loc_mso() */

/**
 * loc_mso_get_close_notify_thread
 */
pthread_t loc_mso_get_close_notify_thread(mso_h msoh)
{
	/* Check for NULL msoh */
	if (!msoh) {
		WARN("NULL msoh passed\n");
		return (pthread_t)0;
	}

	/* An mso that is created (owned) has no threads */
	if (((struct loc_mso *)msoh)->owned) {
		WARN("msoh(0x%lX) is owned!\n", msoh);
		return (pthread_t)0;
	}

	return ((struct loc_mso *)msoh)->close_notify_thread;
} /* loc_mso_get_close_notify_thread() */

/**
 * loc_mso_get_close_notify_thread
 */
msg_q<mq_close_mso_msg> *loc_mso_get_close_notify_mq(mso_h msoh)
{
	/* Check for NULL msoh */
	if (!msoh) {
		WARN("NULL msoh passed\n");
		return nullptr;
	}

	/* An mso that is created (owned) has no threads */
	if (((struct loc_mso *)msoh)->owned) {
		WARN("msoh(0x%lX) is owned!\n", msoh);
		return nullptr;
	}

	return ((struct loc_mso *)msoh)->close_notify_mq;
} /* loc_mso_get_close_notify_thread() */

/**
 * Finds mso with a particular msoid
 */
struct has_msoid {
	has_msoid(uint32_t msoid) : msoid(msoid) {}
	bool operator()(struct loc_mso* mso) {
		return mso->msoid == msoid;
	}
private:
	uint32_t msoid;
}; /* has_msoid */

/**
 * mso_h_exists
 *
 * Returns true if the msoh is in the database, false otherwise.
 */
bool mso_h_exists(mso_h msoh)
{
	struct loc_mso *mso = (struct loc_mso *)msoh;

	return find(begin(loc_mso_list), end(loc_mso_list), mso) != end(loc_mso_list);
} /* mso_h_exists() */

mso_h	find_mso(uint32_t msoid)
{
	has_msoid	hmi(msoid);

	auto it = find_if(loc_mso_list.begin(), loc_mso_list.end(), hmi);
	if (it == loc_mso_list.end()) {
		WARN("msoid = 0x%X not found\n", msoid);
		return 0;
	} else {
		return (mso_h)(*it);
	}
} /* find_mso */

struct has_mso_name {
	has_mso_name(const char *name) : name(name) {}
	bool operator()(struct loc_mso* mso) {
		return !strcmp(mso->name, this->name);
	}
private:
	const char *name;
}; /* has_mso_name */

mso_h	find_mso_by_name(const char *name)
{
	has_mso_name	hmn(name);

	auto it = find_if(loc_mso_list.begin(), loc_mso_list.end(), hmn);
	if (it == loc_mso_list.end()) {
		WARN("mso with name = \'%s\' not found\n", name);
		return 0;
	} else {
		return (mso_h)(*it);
	}
} /* find_mso_by_name() */

/**
 * remove_loc_mso
 *
 * Removes the specified local memory space owner from the database.
 *
 * @msoid	Memory space owner's identifier
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_loc_mso(uint32_t msoid)
{
	has_msoid	hmi(msoid);

	/* Find the mso identified by msoid */
	auto it = find_if(loc_mso_list.begin(), loc_mso_list.end(), hmi);
	if (it == loc_mso_list.end()) {
		WARN("msoid = 0x%X not found\n", msoid);
		return -2;
	}

	/* Free the mso, and remove from list */
	free((*it)->name);	/* Free name string */
	free((*it));		/* Free mso struct */
	loc_mso_list.erase(it);	/* Remove pointer from list */

	return 0;
} /* remove_loc_mso() */



/**
 * mso_is_open
 *
 * @name	Memory space owner's name
 *
 * @return	true if memory space is open, false otherwise
 */
bool mso_is_open(const char *name)
{
	has_mso_name	hmn(name);
	return find_if(loc_mso_list.begin(), loc_mso_list.end(), hmn)
							!= loc_mso_list.end();
} /* mso_is_open() */

/**
 * add_loc_ms
 *
 * Stores a local memory space's info in the database
 *
 * @ms_name	Memory space name
 * @bytes	Size of memory space in bytes
 * @msoh	Memory space owner handle
 * @msid	Memory space identifier
 * @ms_conn_id	Memory space connection ID (for opened ones)
 * @owned	true if creator, false if just opened ms
 * @disc_thread	thread that handles disconnections for this ms
 * @disc_notify_mq	message queue for disconnect notifications
 * @close_thread  thread that closes ms in response to destroy notification
 * @destroy_notify_mq	message queue for destroy notification
 *
 * @return pointer to stored struct, NULL on failure
 */
ms_h add_loc_ms(const char *ms_name,
		uint64_t bytes,
		mso_h msoh,
		uint32_t msid,
		uint64_t phys_addr,
		uint32_t ms_conn_id,
		bool owned,
		pthread_t disc_thread,
		msg_q<mq_rdma_msg> *disc_notify_mq,
		pthread_t close_thread,
		msg_q<mq_close_ms_msg> *close_mq)
{
	/* Allocate */
	struct loc_ms *msp = (struct loc_ms *)malloc(sizeof(struct loc_ms));
	if (msp == NULL) {
		WARN("Failed to allocate local ms.\n");
		return (ms_h)NULL;
	}

	/* Populate */
	msp->name 	= strdup(ms_name);
	msp->bytes	= bytes;
	msp->msoh	= msoh;
	msp->msid	= msid;
	msp->phys_addr 	= phys_addr;
	msp->ms_conn_id	= ms_conn_id;
	msp->owned	= owned;
	msp->disc_thread = disc_thread;
	msp->disc_notify_mq = disc_notify_mq;
	msp->close_thread = close_thread;
	msp->close_mq	 = close_mq;
	msp->accepted	= false;

	/* Add to list */
	loc_ms_list.push_back(msp);

	DBG("Added %s, 0x%X to db\n", ms_name, msid);

	return (ms_h)msp;
} /* add_loc_ms() */

/**
 * Returns true if the ms has owner msoh.
 */
struct has_this_owner {
	has_this_owner(mso_h msoh) : msoh(msoh) {}

	bool operator()(struct loc_ms *msp) {
		return msp->msoh == this->msoh;
	}

private:
	mso_h msoh;
}; /* has_this_owner() */

/**
 * get_num_ms_by_msoh
 *
 * Returns number of memory spaces owned by specified memory space owner
 *
 * @msoh	Memory space owner handle
 *
 * @return	Number of memory space owners owned by msoh
 */
unsigned get_num_ms_by_msoh(mso_h msoh)
{
	has_this_owner	hto(msoh);
	return count_if(loc_ms_list.begin(), loc_ms_list.end(), hto);
} /* get_num_ms_by_msoh() */

/**
 * get_list_msh_by_msoh
 *
 * Returns a list of memory space handles owned by specified memory space owner
 * @msoh	Memory space owner handle
 * @msh		Pointer to pre-allocated space for holding the list
 */
void get_list_msh_by_msoh(mso_h msoh, list<struct loc_ms *>& ms_list)
{
	has_this_owner	hto(msoh);
	copy_if(loc_ms_list.begin(),
		loc_ms_list.end(),
		ms_list.begin(),
		hto);
} /* get_list_msh_by_msoh() */

/**
 * remove_loc_ms
 *
 * Removes the specified local memory space from the database.
 *
 * @msh		Memory space handle
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_loc_ms(ms_h msh)
{
	/* Check for NULL msh */
	if (!msh) {
		ERR("NULL msh passed\n");
		return -1;
	}

	/* Find the ms defined by msh */
	auto it = find(loc_ms_list.begin(), loc_ms_list.end(), (loc_ms *)msh);
	if (it == loc_ms_list.end()) {
		ERR("msh = 0x%lX not found\n");
		return -2;
	}

	/* Free the ms, and remove from list */
	free((*it)->name);	/* Free name string */
	free((*it));		/* Free ms struct */
	loc_ms_list.erase(it);	/* Remove pointer from list */

	return 0;
} /* remove_loc_ms() */

void purge_loc_ms_list()
{
	for (auto& ms : loc_ms_list) {
		free(ms->name);
		free(ms);
	}
	loc_ms_list.clear();
	HIGH("Local ms list purged!!!!\n");
} /* purge_loc_ms_list() */

/**
 * Returns true if memory space has the specified msid.
 */
template <class T>
struct has_this_msid {
	has_this_msid(uint32_t msid) : msid(msid) {}

	bool operator()(T *msp) {
		return msp->msid == this->msid;
	}
private:
	uint32_t msid;
};

/**
 * find_loc_ms
 *
 * Searches the local memory space database for a memory space
 * having the specified memory space identifier
 *
 * @msid	Identifier of the memory space to be located in database
 * @return	Handle to memory space which could be NULL (0) if not found
 */
ms_h find_loc_ms(uint32_t msid)
{
	has_this_msid<loc_ms>	htm(msid);

	auto it = find_if(loc_ms_list.begin(), loc_ms_list.end(), htm);
	return (it != loc_ms_list.end()) ? (ms_h)(*it) : (ms_h)NULL;
} /* find_loc_ms() */

struct has_ms_name {
	has_ms_name(const char *name) : name(name) {}
	bool operator()(struct loc_ms* ms) {
		return !strcmp(ms->name, this->name);
	}
private:
	const char *name;
};

/**
 * find_loc_ms_by_name
 *
 * Searches the local memory space database for a memory space
 * having the specified memory space name
 *
 * @ms_name	Name of the memory space to be located in database
 * @return	Handle to memory space which could be NULL (0) if not found
 */
ms_h find_loc_ms_by_name(const char *ms_name)
{
	has_ms_name	hmn(ms_name);

	auto it = find_if(loc_ms_list.begin(), loc_ms_list.end(), hmn);
	if (it == loc_ms_list.end()) {
		WARN("ms with name = \'%s\' not found\n", ms_name);
		return 0;
	} else {
		return (ms_h)(*it);
	}
} /* find_loc_ms_by_name() */

/**
 * loc_ms_get_close_thread
 */
pthread_t loc_ms_get_close_thread(ms_h msh)
{
	/* Check for NULL msh */
	if (!msh) {
		WARN("NULL msh passed\n");
		return (pthread_t)0;
	}

	return ((struct loc_ms *)msh)->close_thread;
} /* loc_ms_get_close_thread() */

/**
 * loc_ms_get_destroy_notify_mq
 */
msg_q<mq_close_ms_msg> *loc_ms_get_destroy_notify_mq(ms_h msh)
{
	/* Check for NULL msh */
	if (!msh) {
		WARN("NULL msh passed\n");
		return nullptr;
	}

	return ((struct loc_ms *)msh)->close_mq;
} /* loc_ms_get_destroy_notify_thread() */

/**
 * loc_ms_get_disc_thread
 */
pthread_t loc_ms_get_disc_thread(ms_h msh)
{
	/* Check for NULL msh */
	if (!msh) {
		WARN("NULL msh passed\n");
		return (pthread_t)0;
	}

	return ((struct loc_ms *)msh)->disc_thread;
} /* loc_ms_get_disc_thread() */

/**
 * loc_ms_get_disc_notify_mq
 */
msg_q<mq_rdma_msg> *loc_ms_get_disc_notify_mq(ms_h msh)
{
	/* Check for NULL msh */
	if (!msh) {
		WARN("NULL msh passed\n");
		return NULL;
	}

	return ((struct loc_ms *)msh)->disc_notify_mq;
} /* loc_ms_get_disc_notify_thread() */

/**
 * loc_ms_exists
 *
 * Returns true if the msh is in the database, false otherwise.
 */
bool loc_ms_exists(ms_h msh)
{
	struct loc_ms *ms = (struct loc_ms *)msh;

	return find(begin(loc_ms_list), end(loc_ms_list), ms) != end(loc_ms_list);
} /* loc_ms_exists() */

/**
 * add_rem_ms
 *
 * Stores a remote memory space's name and identifier in database.
 *
 * @name	Memory space name
 * @msid	Memory space identifier
 * @wait_for_destroy_thread	Thread which waits for 'destroy' message
 * @destroy_mq	Message queue for receiving 'destroy' message
 *
 * @return pointer to stored struct, NULL on failure
 */
ms_h add_rem_ms(const char *name,
		uint32_t msid,
		pthread_t wait_for_destroy_thread,
		msg_q<mq_destroy_msg> *destroy_mq)
{
	/* Allocate */
	struct rem_ms *msp = (struct rem_ms *)malloc(sizeof(struct rem_ms));
	if (msp == NULL) {
		CRIT("Failed to allocate local ms.\n");
		return (ms_h)NULL;
	}

	/* Populate */
	msp->name 	= strdup(name);
	msp->msid	= msid;
	msp->wait_for_destroy_thread = wait_for_destroy_thread;
	msp->destroy_mq = destroy_mq;

	/* Add to list */
	rem_ms_list.push_back(msp);

	DBG("Added %s, 0x%X to db\n", name, msid);
	DBG("Now database has size = %d\n", rem_ms_list.size());

	return (ms_h)msp;
} /* add_rem_ms() */

/**
 * find_rem_ms
 *
 * Searches the remote memory space database for a memory space
 * having the specified memory space identifier
 *
 * @msid	Identifier of the memory space to be located in database
 * @return	Handle to memory space which could be NULL (0) if not found
 */
ms_h find_rem_ms(uint32_t msid)
{
	has_this_msid<rem_ms>	htm(msid);

	auto it = find_if(rem_ms_list.begin(), rem_ms_list.end(), htm);
	return (it != rem_ms_list.end()) ? (ms_h)(*it) : (ms_h)NULL;
} /* find_rem_ms() */

/**
 * rem_ms_exists
 *
 * Returns true if remote ms denoted by 'msh' is found in the rem_ms_list
 * otherwise returns false
 */
bool rem_ms_exists(ms_h msh)
{
	rem_ms *ms = (rem_ms *)msh;

	return find(begin(rem_ms_list), end(rem_ms_list), ms) != end(rem_ms_list);
} /* rem_ms_exists() */

/**
 * remove_rem_ms
 *
 * Removes the specified remote memory space from the database.
 *
 * @msh		Memory space handle
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_rem_ms(ms_h msh)
{
	/* Check for NULL msh */
	if (!msh) {
		ERR("NULL msh passed\n");
		return -1;
	}

	/* Find the ms defined by msh */
	auto it = find(rem_ms_list.begin(), rem_ms_list.end(), (rem_ms *)msh);
	if (it == rem_ms_list.end()) {
		WARN("msh = 0x%lX not found\n");
		return -2;
	}

	/* Free the ms, and remove from list */
	free((*it)->name);	/* Free name string */
	free((*it));		/* Free ms struct */
	rem_ms_list.erase(it);	/* Remove pointer from list */
	DBG("Now database has size = %d\n", rem_ms_list.size());

	return 0;
} /* remove_rem_ms() */

/**
 * add_loc_msub:
 *
 * Creates a local memory sub-space from its components, and adds it to the 
 * database
 * 
 * @msubid	Memory sub-space identifier
 * @msid	Memory space identifier
 * @bytes	Length of subspace, in bytes
 * @rio_addr_len Rapid IO address length
 * @rio_addr_lo	Rapid IO address lower 64-bits
 * @rio_addr_hi	Rapid IO address bits 66 & 65 (if 66-bit address)
 * @paddr	Physical address of mem sub-space buffer
 * @return Handle to memory sub-space, could be 0 (NULL) if failed
 */
ms_h add_loc_msub(uint32_t 	msubid,
		  uint32_t	msid,
		  uint32_t	bytes,
		  uint8_t	rio_addr_len,
		  uint64_t	rio_addr_lo,
		  uint8_t	rio_addr_hi,
		  uint64_t	paddr)
{
	/* Allocate */
	struct loc_msub *msubp = (struct loc_msub *)malloc(sizeof(struct loc_msub));

	if (msubp == NULL) {
		CRIT("Failed to allocate local msubh.\n");
		return (msub_h)NULL;
	}

	/* Populate */
	msubp->msubid	= msubid;
	msubp->msid	= msid;
	msubp->bytes	= bytes;
	msubp->rio_addr_len = rio_addr_len;
	msubp->rio_addr_lo  = rio_addr_lo;
	msubp->rio_addr_hi  = rio_addr_hi;
	msubp->paddr	= paddr;

	DBG("rio_addr_len=%d, rio_addr_lo=0x%016" PRIx64 ", paddr=0x%016" PRIx64 "\n",
			rio_addr_len, rio_addr_lo, paddr);
	/* Store */
	loc_msub_list.push_back(msubp);

	return (msub_h)msubp;
} /* add_loc_msub() */

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

/**
 * find_loc_msub:
 *
 * Searches the local memory sub-space database for a memory sub-space
 * having the specified sub-space ID.
 *
 * @msubid	ID of the memory sub-space to be located in database
 * @return	Handle to memory sub-space, could be 0 (NULL) if not found
 */
msub_h find_loc_msub(uint32_t msubid)
{
	loc_has_this_msubid	lhtm(msubid);
	
	auto it = find_if(loc_msub_list.begin(), loc_msub_list.end(), lhtm);
	return (it != loc_msub_list.end()) ? (msub_h)(*it) : (msub_h)NULL;
} /* find_loc_msub() */

/**
 * Returns true if loc_msub has memory space handle 'msh'.
 */
struct loc_msub_has_this_msid {
	loc_msub_has_this_msid(uint32_t msid) : msid(msid) {}

	bool operator()(struct loc_msub *msubp) {
		return msubp->msid == this->msid;
	}

private:
	uint32_t msid;
}; /* loc_msub_has_this_msid */

/**
 * get_num_loc_msub_in_ms
 *
 * Returns number of memory subspaces allocated within specified memory space
 *
 * @msid	Memory space identifier
 *
 * @return	Number of memory subspaces within msid
 */
unsigned get_num_loc_msub_in_ms(uint32_t msid)
{
	loc_msub_has_this_msid	lmhtm(msid);

	return count_if(loc_msub_list.begin(), loc_msub_list.end(), lmhtm);
} /* get_num_loc_msub_in_ms() */

/**
 * get_list_loc_msubh_in_msid
 *
 * Returns list of local memory subspace handles owned by specified memory space
 *
 * @msh		Memory space handle
 * @msubh	Pointer to pre-allocated space for holding the list
 */
void get_list_loc_msub_in_msid(uint32_t msid, list<loc_msub *>& msub_list)
{
	loc_msub_has_this_msid	lmhtm(msid);
	copy_if(loc_msub_list.begin(),
		loc_msub_list.end(),
		msub_list.begin(),
		lmhtm);
} /* get_list_loc_msubh_in_msid() */

/**
 * remove_loc_msub:
 *
 * Removes the specified local memory sub-space from the database.
 *
 * @msub_h	Handle to memory sub-space
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_loc_msub(msub_h msubh)
{
	/* Check for NULL msubh */
	if (!msubh) {
		ERR("NULL msubh passed\n");
		return -1;
	}

	/* Find the msub defined by msubh */
	auto it = find(loc_msub_list.begin(), loc_msub_list.end(), (loc_msub *)msubh);
	if (it == loc_msub_list.end()) {
		WARN("msubh = 0x%lX not found\n");
		return -2;
	}

	/* Free the msub, and remove from list */
	free((*it));		/* Free msub struct */
	loc_msub_list.erase(it);/* Remove pointer from list */

	return 0;
} /* remove_loc_msub() */

void purge_loc_msub_list()
{
	for (auto& msub : loc_msub_list) {
		free(msub);
	}
	loc_msub_list.clear();
	HIGH("Local msub list purged!!!!\n");
} /* purge_loc_msub_list() */

/**
 * add_rem_msub:
 *
 * Creates a remote memory sub-space from its components, and adds it to the 
 * database
 * 
 * @rem_msubid	  Remote memory sub-space ID
 * @rem_msid	  Identifier of remote memory space containing the sub-space
 * @rem_bytes	  Length of subspace, in bytes
 * @rem_rio_addr_len Rapid IO address length
 * @rem_rio_addr_lo  Rapid IO address lowest 64-bits
 * @rem_rio_addr_hi  Rapid IO address upper 2 bits (if 66-bit RIO address)
 * @destid_len	  Destination ID length (e.g. 8-bit, 16-bit, or 32-bit)
 * @destid	  Destination ID of node providing the mem sub-space
 * @loc_msh	Handle to local memory space handle to which the client has
 * 		connected when it provided the rem_msubid
 * @return 	Memory subspace handle. Could be 0 (NULL) if failed.
 */
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
	/* Allocate space for new memory subspace */
	struct rem_msub *msubp = (struct rem_msub *)malloc(sizeof(struct rem_msub));
	if (msubp == NULL) {
		CRIT("Cannot allocate local msubh.\n");
		return (msub_h)NULL;
	}

	/* Populate subspace record */
	msubp->msubid	    = rem_msubid;
	msubp->msid	    = rem_msid;
	msubp->bytes        = rem_bytes;
	msubp->rio_addr_len = rem_rio_addr_len;
	msubp->rio_addr_lo  = rem_rio_addr_lo;
	msubp->rio_addr_hi  = rem_rio_addr_hi;
	msubp->destid_len   = destid_len;
	msubp->destid	    = destid;
	msubp->loc_msh	    = loc_msh;

	/* Add to list */
	rem_msub_list.push_back(msubp);

	DBG("*** STORING info about remote msubh ***\n");
	DBG("rem_msubid = 0x%X\n", msubp->msubid);
	DBG("rem_msid = 0x%lX\n", msubp->msid);
	DBG("rem_rio_addr_len = %d\n", rem_rio_addr_len);
	DBG("rem_rio_addr_lo = 0x%016" PRIx64 "\n", rem_rio_addr_lo);
	DBG("rem_rio_addr_hi = 0x%X\n", rem_rio_addr_hi);
	DBG("destid = 0x%X\n", msubp->destid);
	DBG("destid_len = 0x%X\n", msubp->destid_len);
	DBG("msubh = 0x%lX\n", (msub_h)msubp);

	return (msub_h)msubp;
} /* add_rem_msub() */

/**
 * Returns true if remote msub has specified msubid.
 */
struct rem_has_this_msubid {
	rem_has_this_msubid(uint32_t msubid) : msubid(msubid) {}

	bool operator()(struct rem_msub *msubp) {
		return msubp->msubid == this->msubid;
	}
private:
	uint32_t msubid;
};

/**
 * find_rem_msub:
 *
 * Searches the remote memory sub-space database for a memory sub-space
 * having the specified handle.
 *
 * @msubid	Identifier of the memory sub-space to be located in database
 * @return	Handle to memory sub-space or 0 (NULL) if not found
 */
msub_h find_rem_msub(uint32_t msubid)
{
	rem_has_this_msubid	rhtm(msubid);
	
	auto it = find_if(rem_msub_list.begin(), rem_msub_list.end(), rhtm);
	return (it != rem_msub_list.end()) ? (msub_h)(*it) : (msub_h)NULL;
} /* find_rem_msub() */

/**
 * remove_rem_msub:
 *
 * Removes the specified remote memory sub-space from the database.
 * 
 * @msubh	Handle to memory sub-space to be removed
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_rem_msub(msub_h msubh)
{
	/* Check for NULL msubh */
	if (!msubh) {
		ERR("NULL msubh passed\n");
		return -1;
	}

	/* Find the msub defined by msubh */
	auto it = find(rem_msub_list.begin(), rem_msub_list.end(), (rem_msub *)msubh);
	if (it == rem_msub_list.end()) {
		WARN("msubh = 0x%lX not found\n");
		return -2;
	}

	/* Free the msub, and remove from list */
	free((*it));		/* Free msub struct */
	rem_msub_list.erase(it);/* Remove pointer from list */

	return 0;
} /* remove_rem_msub() */

/**
 * Returns true if rem_msub has memory space identifier 'msid'.
 */
struct rem_msub_has_this_msid {
	rem_msub_has_this_msid(uint32_t msid) : msid(msid) {}

	bool operator()(struct rem_msub *msubp) {
		return msubp->msid == this->msid;
	}
private:
	uint32_t msid;
};

/**
 * remove_rem_msub_in_ms
 *
 * Removes all remote msubs belonging to specified msid
 *
 * @msh	Memory space handle for which remote msubs are to be removed.
 */
void remove_rem_msubs_in_ms(uint32_t msid)
{
	rem_msub_has_this_msid	rmhtm(msid);

	/* Save old end of the list */
	auto old_end = end(rem_msub_list);

	/* remove_if() returns the new end after elements are removed */
	auto new_end = remove_if(rem_msub_list.begin(), rem_msub_list.end(),rmhtm);

	/* If the new end is the same as the old one, nothing was removed */
	if (new_end == old_end) {
		/* Since we allow a client to connect without providing
		 * an msub, it is possible that the server may not have
		 * an msub belonging to the client's ms. */
		INFO("No remote msubs stored for msid(0x%X)\n", msid);
	}
} /* remove_rem_msub_in_ms() */

/**
 * find_any_rem_msub_in_ms
 *
 * Fins any remote msub belonging to specified msid
 *
 * @msid	Memory space identifier for which a remote msubs is to be found.
 *
 * @return 0 if successful, < 0 if no msubs found for specified msh
 */
msub_h find_any_rem_msub_in_ms(uint32_t msid)
{
	rem_msub_has_this_msid rmhtm(msid);
	list<rem_msub *>::iterator it;

	it = find_if(rem_msub_list.begin(), rem_msub_list.end(), rmhtm);
	if (it == rem_msub_list.end()) {
		WARN("No remote msubs stored for msid(0x%X)\n", msid);
		return (msub_h)NULL;
	}
	return (msub_h)(*it);
} /* find_any_rem_msub_in_ms() */

/**
 * get_num_rem_msubs
 *
 * Returns number of remote memory subspaces allocated in database
 *
 * @return	Number of memory subspaces within database
 */
unsigned get_num_rem_msubs(void)
{
	return rem_msub_list.size();
} /* get_num_rem_msubs() */

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

/**
 * remove_rem_msub_by_loc_msh
 *
 * Removes the remote msub which is associated with loc_msh from database
 *
 * @loc_msh	Handle to local memory space
 */
void remove_rem_msub_by_loc_msh(ms_h loc_msh)
{
	rem_msub_has_this_loc_msh	has_it(loc_msh);

	/* Save old end of the list */
	auto old_end = end(rem_msub_list);

	/* remove_if() returns the new end after elements are removed */
	auto new_end = remove_if(rem_msub_list.begin(), rem_msub_list.end(), has_it);

	/* If the new end is the same as the old one, nothing was removed */
	if (new_end == old_end) {
		/* Since we allow a client to connect without providing
		 * an msub, it is possible that the server may not have
		 * an msub belonging to the client's ms. */
		WARN("No remote msubs stored for loc_msh(0x%lX)\n", loc_msh);
	}
} /* remove_rem_msub_by_loc_msh() */

void purge_local_database(void)
{
	purge_loc_msub_list();
	purge_loc_ms_list();
	purge_loc_mso_list();
}
