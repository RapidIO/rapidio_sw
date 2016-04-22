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
#include <iterator>
#include <memory>
#include "memory_supp.h"

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "rdma_types.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_dma.h"
#include "liblog.h"
#include "libcli.h"

#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"
#include "rdmad_ms_owner.h"
#include "rdmad_ms_owners.h"

using std::find;
using std::fill;
using std::mutex;
using std::lock_guard;
using std::move;
using std::unique_ptr;


/**
 * @brief Function object that determines whether a memory space has
 * 	  the specified memory space identifier
 *
 * @param msid Memory space identifier
 *
 * @return true if larger or equal to desired size and is free, false otherwise
 */
struct has_msid {
	has_msid(uint32_t msid) : msid(msid) {}

	bool operator()(const unique_ptr<mspace>& ms) {
		return ms->get_msid() == msid;
	}
private:
	uint32_t msid;
};

/**
 * @brief Function object that determines whether a memory space has
 * 	  the specified memory space, and memory space owner identifiers
 *
 * @param msid Memory space identifier
 *
 * @return true if larger or equal to desired size and is free, false otherwise
 */
struct has_msid_and_msoid {
	has_msid_and_msoid(uint32_t msid, uint32_t msoid) :
		msid(msid), msoid(msoid) {}

	bool operator()(const unique_ptr<mspace>& ms) {
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

/**
 * @brief Function object that determines whether a memory space has
 * 	  the specified memory space name
 *
 * @param msid Memory space name
 *
 * @return true if larger or equal to desired size and is free, false otherwise
 */
struct has_ms_name {
	has_ms_name(const char *name) : name(name) {}
	bool operator()(const unique_ptr<mspace>& ms) {
		return *ms == name;	/* mspace::operator==(const char *s) */
	}
private:
	const char *name;
};

ibwin::ibwin(ms_owners &owners, riomp_mport_t mport_hnd,
				unsigned win_num, uint64_t size) :
	owners(owners), mport_hnd(mport_hnd), win_num(win_num),
	rio_addr(RIOMP_MAP_ANY_ADDR), phys_addr(0), size(size)
{
	/* First, obtain an inbound handle from the mport driver */
	if (riomp_dma_ibwin_map(mport_hnd, &rio_addr, size, &phys_addr)) {
		CRIT("riomp_dma_ibwin_map() failed: %s\n", strerror(errno));
		throw ibwin_map_exception(
			"ibwin::ibwin() failed in riomp_dma_ibwin_map");
	}

	/* If direct mapping is used then the physical address and the
	 * RIO address are the same. */
	INFO("%d, rio_addr = 0x%" PRIx64 ", size = 0x%" PRIx64
					", phys_addr = 0x%lX\n",
		win_num, rio_addr, size, phys_addr);

	/* Create first memory space. It is free, has no owner, has
	 * msindex of 0x00000001 and occupies the entire window */
	auto	ms = make_unique<mspace>("freemspace",
				win_num << MSID_WIN_SHIFT | 0x00000001,
				rio_addr,
				phys_addr,
				size,
				owners);
	mspaces.push_back(move(ms));

	/* Skip msindex of 0 and msindex of 1 used above. Free list starts at 2 */
	fill(msindex_free_list, msindex_free_list + MSINDEX_MAX + 1, true);
	msindex_free_list[0] = false;
	msindex_free_list[1] = false;
} /* Constructor */

/* Called from destructor ~inbound() */
void ibwin::free()
{
	/* Delete all memory spaces */
	INFO("Freeing %u mspaces in ibwin %u\n", mspaces.size(), win_num);
	mspaces.clear();

	/* Free inbound window */
	INFO("win_num = %d, phys_addr = 0x%" PRIx64 "\n", win_num, phys_addr);
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
	sprintf(env->output, "%8d %16" PRIx64 " %16" PRIx64 " %16" PRIx64 "\n",
			win_num, size, rio_addr, phys_addr);
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
	lock_guard<mutex> lock(mspaces_mutex);
	for (auto& ms : mspaces)
		ms->dump_info(env);
} /* dump_mspace_info() */

void ibwin::dump_mspace_and_subs_info(cli_env *env)
{
	print_mspace_header(env);
	lock_guard<mutex> lock(mspaces_mutex);
	for (auto& ms : mspaces)
		ms->dump_info_with_msubs(env);
} /* dump_mspace_and_subs_info() */

int ibwin::get_free_mspaces_large_enough(uint64_t size, mspace_list& le_mspaces)
{
	lock_guard<mutex> lock(mspaces_mutex);
	auto it = begin(mspaces);
	while (1) {
		it = find_if(it, end(mspaces),
			[size](const unique_ptr<mspace>& ms)
			{ return (ms->get_size() >= size) && ms->is_free(); });

		if (it != end(mspaces)) {
			le_mspaces.push_back((*it).get());
			it++;
		} else { /* end(mspaces) */
			DBG("Found %u mspaces that can hold %" PRIx64 "\n",
						le_mspaces.size(), size);
			break;
		}
	}
	return (le_mspaces.size() > 0) ? 0 : -1;
} /* free_ms_large_enough() */

bool ibwin::has_room_for_ms(uint64_t size)
{
	lock_guard<mutex> lock(mspaces_mutex);
	return find_if(begin(mspaces), end(mspaces),
			[size](const unique_ptr<mspace>& ms)
			{ return (ms->get_size() >= size) && ms->is_free(); })
			!= end(mspaces);
} /* has_room_for_ms() */

int ibwin::create_mspace(const char *name,
		  	 uint64_t size,
		  	 uint32_t msoid,
		  	 mspace **ms,
		  	 tx_engine<unix_server, unix_msg_t> *creator_tx_eng)
{
	/* As a precaution, check if a memory space by that name
	 * already exists.  */
	if (get_mspace(name) != nullptr) {
		ERR("There is already an ms named '%s'\n", name);
		return RDMA_DUPLICATE_MS;
	}

	/* First get a list of the memory spaces large enough for 'size' */
	mspace_list	le_mspaces;
	if (get_free_mspaces_large_enough(size, le_mspaces)) {
		ERR("No memory space large enough to hold '%s'\n", name);
		return RDMA_NO_ROOM_FOR_MS;
	}

	/* Then find the smallest space that can accomodate 'size' */
	*ms = *std::min_element(begin(le_mspaces),
				     end(le_mspaces),
			     [](const mspace *ms1, const mspace *ms2)
			     {return ms1->get_size() < ms2->get_size();});

	/* Determine index of new, free, memory space */
	lock_guard<mutex> lock(msindex_mutex);
	bool *fmlit  = find(begin(msindex_free_list), end(msindex_free_list),
		    	    	    	    	    	    	    true);
	/* If none found, return error */
	if (fmlit == (end(msindex_free_list))) {
		CRIT("No free memory space indexes\n");
		return -2;
	}

	/* Compute values for new memory space */
	uint64_t new_rio_addr	= (*ms)->get_rio_addr() + size;
	uint64_t new_phys_addr 	= (*ms)->get_phys_addr() + size;
	uint64_t new_size 	= (*ms)->get_size() - size;

	/* Modify original memory space with new parameters */
	(*ms)->set_size(size);
	(*ms)->set_used();
	(*ms)->set_msoid(msoid);
	(*ms)->set_name(name);
	(*ms)->set_creator_tx_eng(creator_tx_eng);

	/* Create memory space for the remaining free inbound space, but
	 * only if that space is non-zero in size */
	if (new_size > 0) {
		/* The new free memory space has no owner, but has a win_num
		 * the same as the original free one, and has a new index */
		uint32_t new_msid = ((*ms)->get_msid() & MSID_WIN_MASK) |
				 (fmlit - begin(msindex_free_list));
		/* Create a new space for unused portion */
		auto new_free = make_unique<mspace>("freemspace",
						new_msid,
						new_rio_addr,
						new_phys_addr,
						new_size,
						owners);

		/* Add new free memory space to list */
		lock_guard<mutex> lock(mspaces_mutex);
		mspaces.push_back(move(new_free));
	}

	/* Mark new memory space index as unavailable */
	*fmlit = false;
	return 0;
} /* create_mspace() */

mspace* ibwin::get_mspace(const char *name)
{
	lock_guard<mutex> lock(mspaces_mutex);
	auto msit = find_if(begin(mspaces), end(mspaces), has_ms_name(name));
	return (msit == end(mspaces)) ? nullptr : (*msit).get();
} /* get_mspace() */

mspace* ibwin::get_mspace(uint32_t msid)
{
	lock_guard<mutex> lock(mspaces_mutex);
	auto it = find_if(begin(mspaces), end(mspaces), has_msid(msid));
	return (it == end(mspaces)) ? nullptr : (*it).get();
} /* get_mspace() */

mspace* ibwin::get_mspace(uint32_t msoid, uint32_t msid)
{
	lock_guard<mutex> lock(mspaces_mutex);
	auto it = find_if(begin(mspaces), end(mspaces),
					has_msid_and_msoid(msid, msoid));
	return (it == end(mspaces)) ? nullptr : (*it).get();
} /* get_mspace() */

void ibwin::merge_other_with_mspace(mspace_ptr_iterator current, mspace_ptr_iterator other)
{
	/* Called from destroy_mspace() which already locks mspaces access */

	/* Current mspace size is inflated by the size of the 'other' mspace */
	uint64_t other_size = (*other)->get_size();
	uint64_t curr_size = (*current)->get_size();
	(*current)->set_size(curr_size  + other_size);

	/* The ms index belonging to the 'other' ms will be freed for reuse */
	lock_guard<mutex> lock(msindex_mutex);
	msindex_free_list[(*other)->get_msindex()] = true;

	/* Delete the other item and erase from the list */
	mspaces.erase(other);
} /* merge_next_with_mspace() */

/* Private method called from other methods which have already locked
 * the mutex around mspaces so don't lock it here */
int ibwin::destroy_mspace(mspace_ptr_iterator current_ms)
{
	/* Destroy the memory space */
	if ((*current_ms)->destroy()) {
		ERR("Failed to destroy '%s', msid(0x%X)\n",
			(*current_ms)->get_name(), (*current_ms)->get_msid());
		return RDMA_MS_DESTROY_FAIL;
	}

	/* If it is the last mspace in the list there is no 'next'
	 * mspace to merge it with. Otherwise, merge and remove the
	 * 'next'. Removing 'next' should not alter 'prev' */
	if ((current_ms + 1) != end(mspaces)) {
		mspace_ptr_iterator next_ms = current_ms + 1;
		if ((*next_ms)->is_free()) {
			DBG("Next ms is also free. Merging!\n");
			merge_other_with_mspace(current_ms, next_ms);
		}
	} else {
		DBG("Last ms in the list. Cannot merge with next!\n");
	}

	/* If it is the first mspace in the list there is no 'prev'
	 * mspace to merge with. Otherwise, merge and remove 'prev' */
	if (current_ms != begin(mspaces)) {
		mspace_ptr_iterator prev_ms = current_ms -1;
		if ((*prev_ms)->is_free()) {
			DBG("Prev ms is also free. Merging!\n");
			merge_other_with_mspace(prev_ms, current_ms);
		}
	} else {
		DBG("First ms in the list. Cannot merge with prev!\n");
	}

	return 0;
} /* destroy_mspace() */

int ibwin::destroy_mspace(uint32_t msoid, uint32_t msid)
{
	/* Locate the memory space by its mosid and msid */
	lock_guard<mutex> lock(mspaces_mutex);
	auto current_ms = find_if(begin(mspaces),
				end(mspaces),
				has_msid_and_msoid(msid, msoid));

	if (current_ms == end(mspaces)) {
		ERR("Cannot find ms w/ msoid(0x%X) and msid(0x%X) in ibwin%u\n",
				msoid, msid, win_num);
		return RDMA_INVALID_MS;	/* Not found */
	}

	return destroy_mspace(current_ms);
} /* destroy_mspace() */

void ibwin::close_mspaces_using_tx_eng(
		tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	lock_guard<mutex> lock(mspaces_mutex);
	for(auto& ms : mspaces) {
		if (ms->has_user_with_user_tx_eng(user_tx_eng)) {
			INFO("Closing connection to '%s'\n", ms->get_name());
			ms->close(user_tx_eng);
		} else {
			DBG("Skipping '%s' since it doesn't use app_tx_eng\n");
		}
	}
} /* close_mspaces_using_tx_eng() */

void ibwin::destroy_mspaces_using_tx_eng(
		tx_engine<unix_server, unix_msg_t> *app_tx_eng)
{
	lock_guard<mutex> lock(mspaces_mutex);

	/* NOTE: destroy_mspace() may potentially alter 'mspaces'
	 * if it decides to merge 2 contiguous free spaces. That
	 * is why a simple iteration across 'mspaces' won't work. */
	bool done;
	do {
		auto it = find_if(begin(mspaces),
				  end(mspaces),
				  [app_tx_eng](unique_ptr<mspace>& ms)
				  {
					return ms->created_using_tx_eng(app_tx_eng);
				  });
		if (it != end(mspaces)) {
			INFO("Destroying '%s'\n", (*it)->get_name());
			destroy_mspace(it);
			done = false;
		} else
			done = true;
	} while (!done);
} /* destroy_mspaces_using_tx_eng() */

void ibwin::get_mspaces_connected_by_destid(uint32_t destid, mspace_list& mspaces)
{
	lock_guard<mutex> lock(mspaces_mutex);
	for (auto& ms : this->mspaces)
		if (ms->connected_by_destid(destid))
			mspaces.push_back(ms.get());
} /* get_mspaces_connected_by_destid() */
