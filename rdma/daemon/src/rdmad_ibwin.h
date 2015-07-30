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

#ifndef IBWIN_H
#define IBWIN_H

#include <algorithm>
#include <vector>

#include <cstdio>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "rdma_types.h"
#include "riodp_mport_lib.h"
#include "liblog.h"
#include "libcli.h"

#include "rdmad_mspace.h"

using namespace std;

struct ibwin_map_exception {
	ibwin_map_exception(const char *msg) : err(msg)
	{
	}

	const char *err;
};

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

class ibwin 
{
public:
	/* Constructor */
	ibwin(int mport_fd, unsigned win_num, uint64_t size) :
	mport_fd(mport_fd), win_num(win_num), rio_addr(RIO_MAP_ANY_ADDR),
	phys_addr(0), size(size)
	{

		/* First, obtain an inbound handle from the mport driver */
		if (riodp_ibwin_map(mport_fd, &rio_addr, size, &phys_addr)) {
			CRIT("riodp_ibwin_map() failed: %s\n", strerror(errno));
			throw ibwin_map_exception(
				"ibwin::ibwin() failed in riodp_ibwin_map");
		}

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
	} /* Constructor */

	/* Called from destructor ~inbound() */
	void free()
	{
		/* Delete all memory spaces */
		for_each(begin(mspaces), end(mspaces), [](mspace *p){ delete p;});
		mspaces.clear();

		/* Free inbound window */
		INFO("win_num = %d, phys_addr = 0x%lX\n", win_num, phys_addr);
		if (riodp_ibwin_free(mport_fd, &phys_addr))
			perror("free(): riodp_ibwin_free()");
	} /* free() */

	void dump_info(struct cli_env *env)
	{
		sprintf(env->output, "%8d %16" PRIx64 " %16" PRIx64 " %16" PRIx64 "\n", win_num, size, rio_addr, phys_addr);
		logMsg(env);
	} /* dump_info() */

	void print_mspace_header(struct cli_env *env)
	{
		sprintf(env->output, "\n%8s %8d %16s %8s %16s %8s\n", "Window", win_num, "Name",
						"msid", "rio_addr", "size");
		logMsg(env);
		sprintf(env->output, "%8s %8s %16s %8s %16s %8s\n", "-------", "-------",
					"----------------", "--------",
					"----------------", "--------");
		logMsg(env);
	} /* print_mspace_header() */

	void dump_mspace_info(struct cli_env *env)
	{
		print_mspace_header(env);
		for (auto& ms : mspaces) {
			ms->dump_info(env);
		}
	} /* dump_mspace_info() */

	void dump_mspace_and_subs_info(cli_env *env)
	{
		print_mspace_header(env);
		for (auto& ms : mspaces) {
			ms->dump_info_with_msubs(env);
		}
	} /* dump_mspace_and_subs_info() */

	/* Returns iterator to memory space large enough to hold 'size' */
	vector<mspace *>::iterator free_ms_large_enough(uint64_t size)
	{
		has_room	hr(size);
		return find_if(mspaces.begin(), mspaces.end(), hr);
	} /* free_ms_large_enough() */

	/* Returns whether there is a memory space large enough to hold 'size' */
	bool has_room_for_ms(uint64_t size)
	{
		has_room	hr(size);
		return find_if(mspaces.begin(), mspaces.end(), hr) != mspaces.end();
	} /* has_room_for_ms() */

	/* Create memory space */
	int create_mspace(const char *name,
			  uint64_t size,
			  uint32_t msoid,
			  uint32_t *msid) 
	{
		/* Find the free memory space to use to allocate ours */
		auto orig_free = free_ms_large_enough(size);
		if (orig_free == mspaces.end()) {
			ERR("No memory space large enough\n");
			return -1;
		}

		/* Determine index of new, free, memory space */
		bool *fmlit  = find(begin(msindex_free_list),
				    end(msindex_free_list),
			    	    true);

		/* If none found, return error */
		if (fmlit == (end(msindex_free_list))) {
			CRIT("No free memory space indexes\n");
			return -2;
		}

		/* Compute values for new memory space */
		uint64_t new_rio_addr = (*orig_free)->get_rio_addr() + size;
		uint64_t new_phys_addr = (*orig_free)->get_phys_addr() + size;
		uint64_t new_size = (*orig_free)->get_size() - size;

		/* Modify original memory space with new parameters */
		(*orig_free)->set_size(size);
		(*orig_free)->set_used();
		(*orig_free)->set_msoid(msoid);
		(*orig_free)->set_name(name);
		*msid = (*orig_free)->get_msid();	/* Return as output param */

		/* Create memory space for the remaining free inbound space, but
		 * only if that space is non-zero in size */
		if (new_size) {
			/* The new free memory space has no owner, but has a
			 * win_num the same as the original free one, and has a
			 * new index */
			uint32_t new_msid = ((*orig_free)->get_msid() & MSID_WIN_MASK) |
					 (fmlit - begin(msindex_free_list));

			/* Create a new space for unused portion */
			mspace	 *new_free = new mspace("freemspace",
							new_msid,
							new_rio_addr,
							new_phys_addr,
							new_size);

			/* Add new free memory space to list */
			mspaces.push_back(new_free);
		}

		/* Mark new memory space index as unavailable */
		*fmlit = false;

		return 1;
	} /* create_mspace() */

	mspace* get_mspace(const char *name)
	{
		has_ms_name	hmn(name);

		auto msit = find_if(begin(mspaces), end(mspaces), hmn);
		return (msit == end(mspaces)) ? NULL : *msit;
	} /* get_mspace() */

	mspace* get_mspace(uint32_t msid)
	{
		has_msid	hmsid(msid);

		auto it = find_if(begin(mspaces), end(mspaces), hmsid);
		return (it == end(mspaces)) ? NULL : *it;
	} /* get_mspace() */

	mspace* get_mspace(uint32_t msoid, uint32_t msid)
	{
		has_msid	hmsid(msid);

		auto it = find_if(begin(mspaces), end(mspaces), hmsid);

		if (it == end(mspaces)) {
			WARN("Mspace with msid(0x%X) not found\n", msid);
			return NULL;
		}

		if ((*it)->get_msoid() != msoid) {
			ERR("Memspace with msi(0x%X) not owned by msoid(0x%X)\n",
								msid,msoid);
			return NULL;
		}
		return *it;
	} /* get_mspace() */

	bool find_mspace(const char *name, vector<mspace *>::iterator& msit)
	{
		has_ms_name	hmn(name);

		msit = find_if(begin(mspaces), end(mspaces), hmn);
		/* DEBUG */
		if (msit != end(mspaces)) {
			DBG("Found %s\n", name);
		}
		return (msit != end(mspaces)) ? true : false;
	} /* find_mspace() */

	bool find_mspace(uint32_t msid, vector<mspace *>::iterator& msit)
	{
		has_msid	hmsid(msid);

		msit = find_if(begin(mspaces), end(mspaces), hmsid);
		return (msit != end(mspaces)) ? true : false;
	} /* find_mspace() */

	vector<mspace *>& get_mspaces() { return mspaces; };

private:
	int		mport_fd;	/* Master port file descriptor */
	unsigned	win_num;	/* window number */
	uint64_t	rio_addr;	/* starting address in RIO space */
	uint64_t	phys_addr;	/* starting physical address */
	uint64_t	size;		/* window size in bytes */

	/* Memory space indexes */
	bool msindex_free_list[MSINDEX_MAX+1];	/* List of memory space IDs */

	vector<mspace*>	mspaces;
}; /* ibwin */


#endif


