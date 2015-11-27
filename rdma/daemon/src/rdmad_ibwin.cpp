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
	 * msindex of 0x0000 and occupies the entire window */
	mspace	*ms = new mspace("freemspace",
				win_num << MSID_WIN_SHIFT,
				rio_addr,
				phys_addr,
				size);
	mspaces.push_back(ms);

	/* Initially all free list indexes are available except the first */
	fill(msindex_free_list, msindex_free_list + MSINDEX_MAX + 1, true);
	msindex_free_list[0] = false;

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
	for (auto& ms : mspaces) {
		if (ms != nullptr) {
			INFO("Deleting %s\n", ms->get_name());
			delete ms;
		} else {
			WARN("NOT deleting nullptr\n");
		}
	}
	mspaces.clear();

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
	sprintf(env->output, "\n%8s %8d %16s %8s %16s %8s\n", "Window", win_num, "Name",
					"msid", "rio_addr", "size");
	logMsg(env);
	sprintf(env->output, "%8s %8s %16s %8s %16s %8s\n", "-------", "-------",
				"----------------", "--------",
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
mspace* ibwin::free_ms_large_enough(uint64_t size)
{
	has_room	hr(size);
	mspace		*ms = nullptr;

	pthread_mutex_lock(&mspaces_lock);
	auto it = find_if(mspaces.begin(), mspaces.end(), hr);
	if (it != end(mspaces))
		ms = *it;
	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* free_ms_large_enough() */

/* Returns whether there is a memory space large enough to hold 'size' */
bool ibwin::has_room_for_ms(uint64_t size)
{
	has_room	hr(size);
	pthread_mutex_lock(&mspaces_lock);
	bool mspace_has_room = find_if(mspaces.begin(), mspaces.end(), hr)
						!= mspaces.end();
	pthread_mutex_unlock(&mspaces_lock);

	return mspace_has_room;
} /* has_room_for_ms() */

/* Create memory space */
int ibwin::create_mspace(const char *name,
		  	  uint64_t size,
		  	  uint32_t msoid,
		  	  uint32_t *msid,
		  	  mspace **ms)
{
	/* Find the free memory space to use to allocate ours */
	*ms = free_ms_large_enough(size);
	if (*ms == nullptr) {
		ERR("No memory space large enough\n");
		return -1;
	}

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
	if (new_size) {
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
	has_ms_name	hmn(name);

	pthread_mutex_lock(&mspaces_lock);
	auto msit = find_if(begin(mspaces), end(mspaces), hmn);
	mspace *ms = (msit == end(mspaces)) ? NULL : *msit;
	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* get_mspace() */

mspace* ibwin::get_mspace(uint32_t msid)
{
	has_msid	hmsid(msid);

	pthread_mutex_lock(&mspaces_lock);
	auto it = find_if(begin(mspaces), end(mspaces), hmsid);
	mspace *ms = (it == end(mspaces)) ? NULL : *it;
	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* get_mspace() */

mspace* ibwin::get_mspace(uint32_t msoid, uint32_t msid)
{
	has_msid	hmsid(msid);

	pthread_mutex_lock(&mspaces_lock);
	auto it = find_if(begin(mspaces), end(mspaces), hmsid);

	mspace *ms;

	if (it == end(mspaces)) {
		WARN("Mspace with msid(0x%X) not found in ibwin(%u)\n", msid);
		return nullptr;
	} else {
		ms = *it;
	}
	pthread_mutex_unlock(&mspaces_lock);


	if (ms->get_msoid() != msoid) {
		ERR("msid(0x%X) not owned by msoid(0x%X) in ibwin(%u)\n",
				msid, msoid, win_num );
		ms = nullptr;;
	}

	return ms;
} /* get_mspace() */

mspace* ibwin::get_mspace_open_by_server(unix_server *server, uint32_t *ms_conn_id)
{
	mspace *ms = nullptr;

	pthread_mutex_lock(&mspaces_lock);
	for (auto& ms : mspaces) {
		if (ms->has_user_with_user_server(server, ms_conn_id))
			break;
	}
	pthread_mutex_unlock(&mspaces_lock);

	return ms;
} /* get_mspace_open_by_server() */

void ibwin::get_mspaces_connected_by_destid(uint32_t destid, vector<mspace *>& mspaces)
{
	pthread_mutex_lock(&mspaces_lock);
	for (auto& ms : mspaces) {
		if (ms->connected_by_destid(destid)) {
			mspaces.push_back(ms);
		}
	}
	pthread_mutex_unlock(&mspaces_lock);
} /* get_mspaces_connected_by_destid() */
