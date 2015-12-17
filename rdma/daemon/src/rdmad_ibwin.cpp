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
#include <algorithm>
#include <vector>
#include <list>
#include <iterator>

#include <cstdio>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "rdma_types.h"
#include <rapidio_mport_mgmt.h>
#include "rapidio_mport_dma.h"
#include "liblog.h"
#include "libcli.h"

#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"

using std::vector;
using std::list;
using std::find;
using std::fill;
using std::begin;
using std::end;


/* Memory space is free and is equal to or larger than 'size'  */
struct has_room
{
	has_room(uint64_t size) : size(size) {}

	bool operator()(mspace* ms) {
		return (ms->get_size() >= size) && ms->is_free();
	}
private:
	uint64_t size;
};

struct has_msid {
	has_msid(uint32_t msid) : msid(msid) {}
	bool operator()(mspace *ms) {
		return ms->get_msid() == msid;
	}
private:
	uint32_t msid;
};

struct has_msid_and_msoid {
	has_msid_and_msoid(uint32_t msid, uint32_t msoid) :
		msid(msid), msoid(msoid) {}
	bool operator()(mspace *ms) {
		if (ms->get_msid() == msid) {
			if (ms->get_msoid() == msoid) {
				return true;
			} else {
				ERR("Found msid(0x%X) but with wrong msoid(0x%X)\n",
					msid, ms->get_msoid());
			}
		}
		return false;
	}
private:
	uint32_t msid, msoid;
};


struct has_ms_name {
	has_ms_name(const char *name) : name(name) {}
	bool operator()(mspace *ms) {
		return *ms == name;	/* mspace::operator==(const char *s) */
	}
private:
	const char *name;
};

	/* Constructor */
ibwin::ibwin(riomp_mport_t mport_hnd, unsigned win_num, uint64_t size) :
	mport_hnd(mport_hnd), win_num(win_num), rio_addr(RIO_MAP_ANY_ADDR),
	phys_addr(0), size(size)
{
	/* First, obtain an inbound handle from the mport driver */
	if (riomp_dma_ibwin_map(mport_hnd, &rio_addr, size, &phys_addr)) {
		CRIT("riomp_dma_ibwin_map() failed: %s\n", strerror(errno));
		throw ibwin_map_exception(
			"ibwin::ibwin() failed in riomp_dma_ibwin_map");
	}

	/**
	 * If direct mapping is used then the physical address and the
	 * RIO address are the same. We currently only return the physical
	 * address to the library. I think we should return both.
	 */
	INFO("%d, rio_addr = 0x%lX, size = 0x%lX, phys_addr = 0x%lX\n",
		win_num, rio_addr, size, phys_addr);

	/* Create first memory space. It is free, has no owner, has
	 * msindex of 0x00000001 and occupies the entire window */
	mspace	*ms = new mspace("freemspace",
				win_num << MSID_WIN_SHIFT | 0x00000001,
				rio_addr,
				phys_addr,
				size);
	mspaces.push_back(ms);

	/* Skip msindex of 0 and msindex of 1 used above. Free list starts at 2 */
	fill(msindex_free_list, msindex_free_list + MSINDEX_MAX + 1, true);
	msindex_free_list[0] = false;
	msindex_free_list[1] = false;

	if (pthread_mutex_init(&mspaces_lock, NULL)) {
		CRIT("Failed to init mspaces_lock mutex\n");
		throw -1;
	}

	if (pthread_mutex_init(&msindex_lock, NULL)) {
		CRIT("Failed to init msindex_lock mutex\n");
		throw -1;
	}
} /* Constructor */

/* Called from destructor ~inbound() */
void ibwin::free()
{
	/* Delete all memory spaces */
	pthread_mutex_lock(&mspaces_lock);
	for (auto& ms : mspaces) {
		if (ms != nullptr) {
			INFO("Deleting %s\n", ms->get_name());
			delete ms;
		} else {
			WARN("NOT deleting nullptr\n");
		}
	}
	mspaces.clear();
	pthread_mutex_unlock(&mspaces_lock);

	/* Free inbound window */
	INFO("win_num = %d, phys_addr = 0x%16" PRIx64 "\n",
			win_num, phys_addr);
	int ret = riomp_dma_ibwin_free(mport_hnd, &phys_addr);
	if (ret) {
		CRIT("riomp_dma_ibwin_free() failed, ret = %d: %s\n",
				ret, strerror(errno));
	} else {
		HIGH("Inbound window %d freed successfully\n", win_num);
	}
} /* free() */

void ibwin::dump_info(struct cli_env *env)
{
	sprintf(env->output, "%8d %16" PRIx64 " %16" PRIx64 " %16" PRIx64 "\n", win_num, size, rio_addr, phys_addr);
	logMsg(env);
} /* dump_info() */

void ibwin::print_mspace_header(struct cli_env *env)
{
	sprintf(env->output, "\n%8s %8u %16s %8s %8s %16s %8s\n", "Window", win_num, "Name",
				"msoid", "msid", "rio_addr", "size");
	logMsg(env);
	sprintf(env->output, "%8s %8s %16s %8s %8s %16s %8s\n", "-------", "-------",
				"----------------", "--------", "--------",
				"----------------", "--------");
	logMsg(env);
} /* print_mspace_header() */

void ibwin::dump_mspace_info(struct cli_env *env)
{
	print_mspace_header(env);
	pthread_mutex_lock(&mspaces_lock);
	for (auto& ms : mspaces) {
		ms->dump_info(env);
	}
	pthread_mutex_unlock(&mspaces_lock);
} /* dump_mspace_info() */

void ibwin::dump_mspace_and_subs_info(cli_env *env)
{
	pthread_mutex_lock(&mspaces_lock);
	print_mspace_header(env);
	for (auto& ms : mspaces) {
		ms->dump_info_with_msubs(env);
	}
	pthread_mutex_unlock(&mspaces_lock);
} /* dump_mspace_and_subs_info() */

/* Returns pointer to memory space large enough to hold 'size' */
int ibwin::get_free_mspaces_large_enough(uint64_t size, mspace_list& le_mspaces)
{
	pthread_mutex_lock(&mspaces_lock);
	auto it = begin(mspaces);
	while (1) {
		it = find_if(it, end(mspaces), has_room(size));
		if (it != end(mspaces)) {
			le_mspaces.push_back(*it);
			it++;
		} else { /* end(mspaces) */
			DBG("Found %u mspaces that can hold %" PRIx64 "\n",
						le_mspaces.size(), size);
			break;
		}
	}
	pthread_mutex_unlock(&mspaces_lock);

	return (le_mspaces.size() > 0) ? 0 : -1;
} /* free_ms_large_enough() */

/* Returns whether there is a memory space large enough to hold 'size' */
bool ibwin::has_room_for_ms(uint64_t size)
{
	pthread_mutex_lock(&mspaces_lock);
	bool mspace_has_room = find_if(mspaces.begin(), mspaces.end(), has_room(size))
						!= mspaces.end();
	pthread_mutex_unlock(&mspaces_lock);

	return mspace_has_room;
} /* has_room_for_ms() */

struct ms_compare_t {
	bool operator()(mspace *ms1, mspace *ms2) {
		return ms1->get_size() < ms2->get_size();
	}
} ms_compare;

/* Create memory space */
int ibwin::create_mspace(const char *name,
		  	  uint64_t size,
		  	  uint32_t msoid,
		  	  uint32_t *msid,
		  	  mspace **ms)
{
	/* First get a list of the memory spaces large enough for 'size' */
	mspace_list	le_mspaces;
	if (get_free_mspaces_large_enough(size, le_mspaces)) {
		ERR("No memory space large enough to hold '%s'\n", name);
		return -1;
	}
	/* Then find the smallest space that can accomodate 'size' */
	*ms = *std::min_element(begin(le_mspaces), end(le_mspaces), ms_compare);

	/* Determine index of new, free, memory space */
	pthread_mutex_lock(&msindex_lock);
	bool *fmlit  = find(begin(msindex_free_list),
			    end(msindex_free_list),
		    	    true);

	/* If none found, return error */
	if (fmlit == (end(msindex_free_list))) {
		CRIT("No free memory space indexes\n");
		pthread_mutex_unlock(&msindex_lock);
		return -2;
	}
	pthread_mutex_unlock(&msindex_lock);

	/* Compute values for new memory space */
	uint64_t new_rio_addr	= (*ms)->get_rio_addr() + size;
	uint64_t new_phys_addr 	= (*ms)->get_phys_addr() + size;
	uint64_t new_size 	= (*ms)->get_size() - size;

	/* Modify original memory space with new parameters */
	(*ms)->set_size(size);
	(*ms)->set_used();
	(*ms)->set_msoid(msoid);
	(*ms)->set_name(name);
	*msid = (*ms)->get_msid();	/* Return as output param */

	/* Create memory space for the remaining free inbound space, but
	 * only if that space is non-zero in size */
	if (new_size > 0) {
		/* The new free memory space has no owner, but has a
		 * win_num the same as the original free one, and has a
		 * new index */
		uint32_t new_msid = ((*ms)->get_msid() & MSID_WIN_MASK) |
				 (fmlit - begin(msindex_free_list));
		/* Create a new space for unused portion */
		mspace	 *new_free = new mspace("freemspace",
						new_msid,
						new_rio_addr,
						new_phys_addr,
						new_size);

		/* Add new free memory space to list */
		pthread_mutex_lock(&mspaces_lock);
		mspaces.push_back(new_free);
		pthread_mutex_unlock(&mspaces_lock);
	}

	/* Mark new memory space index as unavailable */
	*fmlit = false;
	return 0;
} /* create_mspace() */

mspace* ibwin::get_mspace(const char *name)
{
	pthread_mutex_lock(&mspaces_lock);
	auto msit = find_if(begin(mspaces), end(mspaces), has_ms_name(name));
	mspace *ms = (msit == end(mspaces)) ? NULL : *msit;
	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* get_mspace() */

mspace* ibwin::get_mspace(uint32_t msid)
{
	pthread_mutex_lock(&mspaces_lock);

	auto it = find_if(begin(mspaces), end(mspaces), has_msid(msid));
	mspace *ms = (it == end(mspaces)) ? NULL : *it;

	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* get_mspace() */

mspace* ibwin::get_mspace(uint32_t msoid, uint32_t msid)
{
	pthread_mutex_lock(&mspaces_lock);

	auto it = find_if(begin(mspaces), end(mspaces), has_msid_and_msoid(msid, msoid));
	mspace *ms = (it == end(mspaces)) ? nullptr : *it;

	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* get_mspace() */

void ibwin::merge_other_with_mspace(mspace_iterator current, mspace_iterator other)
{
	/* Current mspace size is inflated by the size of the 'other' mspace */
	uint64_t other_size = (*other)->get_size();
	uint64_t curr_size = (*current)->get_size();

	DBG("ENTER\n");
	DBG("Old size for 'current' ms was %u bytes\n", curr_size);
	(*current)->set_size(curr_size  + other_size);
	DBG("New size for 'current' ms is %u bytes\n", (*current)->get_size());

	/* The ms index belonging to the 'other' ms will be freed for reuse */
	DBG("Freeing the msindex 0x%X for reuse\n", (*other)->get_msindex());
	pthread_mutex_lock(&msindex_lock);
	msindex_free_list[(*other)->get_msindex()] = true;
	pthread_mutex_unlock(&msindex_lock);

	/* Delete the next item and erase from the list */
	DBG("Calling the destructor for 'other'\n");
	delete *other;

	/* Remove the 'other' memory space from the list */
	DBG("Removing 'other' from mspace lists altogether\n");
	mspaces.erase(other);
	DBG("EXIT\n");
} /* merge_next_with_mspace() */

int ibwin::destroy_mspace(uint32_t msoid, uint32_t msid)
{
	int ret = 0;

	DBG("ENTER\n");
	pthread_mutex_lock(&mspaces_lock);

	mspace_iterator current_ms =
			find_if(begin(mspaces),
				end(mspaces),
				has_msid_and_msoid(msid, msoid));


	if (current_ms != end(mspaces)) {
		/* Destroy the memory space */
		ret = (*current_ms)->destroy();
		if (ret) {
			ERR("Failed to destroy msid(0x%X)\n", msid);
			goto exit;
		}

		/* If it is the last mspace in the list there is no 'next'
		 * mspace to merge it with. Otherwise, merge and remove
		 * the 'next'. Removing 'next' should not alter the value of 'prev'
		 */
		if ((current_ms + 1) != end(mspaces)) {
			mspace_iterator next_ms = current_ms + 1;
			if ((*next_ms)->is_free()) {
				DBG("Next ms is also free. Merging!\n");
				merge_other_with_mspace(current_ms, next_ms);
			}
		} else {
			DBG("Last ms in the list. Cannot merge with next!\n");
		}

		/* If it is the first mspace in the list there is no 'prev'
		 * mspace to merge it with. Otherwise, merge and remove the
		 * 'prev'.
		 */
		if (current_ms != begin(mspaces)) {
			mspace_iterator prev_ms = current_ms -1;
			if ((*prev_ms)->is_free()) {
				DBG("Prev ms is also free. Merging!\n");
				merge_other_with_mspace(prev_ms, current_ms);
			}
		} else {
			DBG("First ms in the list. Cannot merge with prev!\n");
		}
	} else {
		ERR("Cannot find ms w/ msoid(0x%X) and msid(0x%X) in ibwin%u\n",
				msoid, msid, win_num);
		ret = -1;	/* Not found */
	}
exit:
	pthread_mutex_unlock(&mspaces_lock);

	DBG("EXIT\n");
	return ret;
} /* destroy_mspace() */

struct mspace_is_open_by_server {
	mspace_is_open_by_server(unix_server *server, uint32_t *ms_conn_id) :
		server(server), ms_conn_id(ms_conn_id) {}
	bool operator ()(mspace *ms) {
		return ms->has_user_with_user_server(server, ms_conn_id);
	}
private:
	unix_server *server;
	uint32_t    *ms_conn_id;
};

// FIXME: What if there is more than one? In the new world there will be!
mspace* ibwin::get_mspace_open_by_server(unix_server *server, uint32_t *ms_conn_id)
{
	mspace *ms_ptr = nullptr;

	pthread_mutex_lock(&mspaces_lock);
	for (auto& ms : mspaces) {
		if (ms->has_user_with_user_server(server, ms_conn_id)) {
			ms_ptr = ms;
			break;
		}
	}
	pthread_mutex_unlock(&mspaces_lock);

	return ms_ptr;
} /* get_mspace_open_by_server() */

void ibwin::get_mspaces_connected_by_destid(uint32_t destid, mspace_list& mspaces)
{
	pthread_mutex_lock(&mspaces_lock);
	for (auto& ms : mspaces) {
		if (ms->connected_by_destid(destid)) {
			mspaces.push_back(ms);
		}
	}
	pthread_mutex_unlock(&mspaces_lock);
} /* get_mspaces_connected_by_destid() */
