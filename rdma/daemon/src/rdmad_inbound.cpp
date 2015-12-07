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
#include <semaphore.h>
#include <sys/mman.h>

#include <cstdio>

#include <algorithm>
#include <vector>

#include "rdma_types.h"
#include "liblog.h"
#include "rapidio_mport_mgmt.h"

#include "rdmad_msubspace.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"
#include "rdmad_inbound.h"

using std::vector;

struct ibw_free {
	void operator()(ibwin& ibw) { ibw.free(); }
};

/* Does inbound window have room for a memory space of specified size? */
struct ibwin_has_room {
	ibwin_has_room(uint64_t size) : size(size) {}
	bool operator()(ibwin& ibw) {
		return ibw.has_room_for_ms(size);
	}
private:
	uint64_t size;
}; /* ibwin_has_room */

/* Constructor */
inbound::inbound(riomp_mport_t mport_hnd,
		 unsigned num_wins,
		 uint32_t win_size) : ibwin_size(win_size), mport_hnd(mport_hnd)
{
	/* Initialize inbound windows */
	for (unsigned i = 0; i < num_wins; i++) {
		try {
			ibwin win(mport_hnd, i, win_size);
			ibwins.push_back(win);
		}
		catch(ibwin_map_exception& e) {
			throw inbound_exception(e.err);
		}
	}

	/* Initialize semaphore for protecting access to ibwins */
	if (sem_init(&ibwins_sem, 0, 1) == -1) {
		CRIT("Failed to initialize ibwins_sem: %s\n", strerror(errno));
		throw mspace_exception("Failed to initialize ibwins_sem");
	}
} /* constructor */

/* Destructor */
inbound::~inbound()
{
	/* Free inbound windows mapped with riomp_dma_ibwin_map() */
	for_each(ibwins.begin(), ibwins.end(), ibw_free());
} /* destructor */

void inbound::dump_info(struct cli_env *env)
{
	sprintf(env->output, "%8s %16s %16s %16s\n", "Win num", "Win size", "RIO Addr", "PCIe Addr");
	logMsg(env);
	sprintf(env->output, "%8s %16s %16s %16s\n", "-------", "--------", "--------", "---------");
	logMsg(env);
	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		ibwin.dump_info(env);
	}
	sem_post(&ibwins_sem);
} /* dump_info */

/* get_mspace by name */
mspace* inbound::get_mspace(const char *name)
{
	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		mspace *ms = ibwin.get_mspace(name);
		if (ms) {
			sem_post(&ibwins_sem);
			return ms;
		}
	}
	WARN("%s not found\n", name);
	sem_post(&ibwins_sem);
	return NULL;
} /* get_mspace() */

/* get_mspace by msid */
mspace* inbound::get_mspace(uint32_t msid)
{
	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		mspace *ms = ibwin.get_mspace(msid);
		if (ms) {
			sem_post(&ibwins_sem);
			return ms;
		}
	}
	WARN("msid(0x%X) not found\n", msid);
	sem_post(&ibwins_sem);
	return NULL;
} /* get_mspace() */

/* Get mspace OPENED by msoid */
mspace* inbound::get_mspace_open_by_server(unix_server *user_server, uint32_t *ms_conn_id)
{
	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		mspace *ms = ibwin.get_mspace_open_by_server(user_server, ms_conn_id);
		if (ms) {
			sem_post(&ibwins_sem);
			return ms;
		}
	}
	WARN("msid open with user_server not found\n");
	sem_post(&ibwins_sem);
	return NULL;
} /* get_mspace_open_by_msoid() */

int inbound::get_mspaces_connected_by_destid(uint32_t destid, vector<mspace *>& mspaces)
{
	vector<mspace *>::iterator ins_point = begin(mspaces);

	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		vector<mspace *>  ibw_mspaces;
		DBG("Looking for mspaces connected to destid(0x%X)\n", destid);
		ibwin.get_mspaces_connected_by_destid(destid, ibw_mspaces);

		/* Insert each list of mspaces matching 'destid' in current IBW
		 * in the 'mspaces' list for the entire inbound. The insertion
		 * point is advanced after the mspaces for each IBW are copied
		 * to the master 'mspaces' list.
		 */
		ins_point = copy(begin(ibw_mspaces),
				 end(ibw_mspaces),
				 ins_point);
	}
	sem_post(&ibwins_sem);

	return 0;
} /* get_mspaces_connected_by_destid */

/* get_mspace by msoid & msid */
mspace* inbound::get_mspace(uint32_t msoid, uint32_t msid)
{
	mspace *ms = nullptr;

	sem_wait(&ibwins_sem);

	for (auto& ibw : ibwins) {
		ms = ibw.get_mspace(msoid, msid);
		if (ms != nullptr) {
			DBG("msid(0x%X) with msoid(0x%X) found in ibwin(%u)\n",
				msid, msoid, ibw.get_win_num());
			break;
		}
	}
	sem_post(&ibwins_sem);

	if (ms == nullptr) {
		WARN("msid(0x%X) with msoid(0x%X) not found\n", msid, msoid);
	}

	return ms;
} /* get_mspace() */


/* Dump memory space info for a memory space specified by name */
int inbound::dump_mspace_info(struct cli_env *env, const char *name)
{
	/* Find the memory space by name */
	mspace	*ms = get_mspace(name);
	if (!ms) {
		WARN("%s not found\n", name);
		return -1;
	}
	ms->dump_info(env);
	return 0;
} /* dump_mspace_info() */

/* Dump memory space info for all memory spaces */
void inbound::dump_all_mspace_info(struct cli_env *env)
{
	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		ibwin.dump_mspace_info(env);
	}
	sem_post(&ibwins_sem);
} /* dump_all_mspace_info() */

/* Dump memory space info for all memory spaces and their msubs */
void inbound::dump_all_mspace_with_msubs_info(struct cli_env *env)
{
	sem_wait(&ibwins_sem);
	for (auto& ibw : ibwins) {
		ibw.dump_mspace_and_subs_info(env);
	}
	sem_post(&ibwins_sem);
} /* dump_all_mspace_with_msubs_info() */

int inbound::destroy_mspace(uint32_t msoid, uint32_t msid)
{
	int ret;
	auto win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;

	sem_wait(&ibwins_sem);

	auto& ibw = ibwins[win_num];
	ret = ibw.destroy_mspace(msoid, msid);

	sem_post(&ibwins_sem);

	return ret;
} /* destroy_mspace() */

/* Create a memory space within a window that has enough space */
// TODO: Returning msid is redundant as it can be obtained from 'ms'
int inbound::create_mspace(const char *name,
			   uint64_t size,
			   uint32_t msoid,
			   uint32_t *msid,
			   mspace **ms)
{
	/* Find first inbound window having room for memory space */
	sem_wait(&ibwins_sem);
	auto win_it = find_if(begin(ibwins), end(ibwins), ibwin_has_room(size));

	/* If none found, fail */
	if (win_it == ibwins.end()) {
		WARN("No room for ms of size 0x%lX\n", size);
		sem_post(&ibwins_sem);
		return -1;
	}

	/* Create the space */
	int ret = win_it->create_mspace(name, size, msoid, msid, ms);
#if 0
	/* MMAP, zero, then UNMAP the space */
	void *vaddr;
	if (ret == 0 ) {
		ret = riomp_dma_map_memory(mport_hnd,
					   size,
					   (*ms)->get_phys_addr(),
					   &vaddr);
		if (ret == 0) {
			memset((uint8_t *)vaddr, 0, size);
			INFO("Memory space '%s' filled with 0s\n", name);
			if (munmap(vaddr, size) == -1) {
			        ERR("munmap(): %s\n", strerror(errno));
				ERR("phys_addr = 0x%" PRIx64 ", size = 0x%X\n",
						(*ms)->get_phys_addr(), size);
				ret = -2;
			}
		} else {
			ERR("Failed to MMAP mspace %s: %s\n",
						name, strerror(errno));
			ret = -3;
		}
	}
#endif
	sem_post(&ibwins_sem);
	return ret;
} /* create_mspace() */

/* Open memory space */
// TODO: Just return pointer to the 'ms'
int inbound::open_mspace(const char *name,
			 unix_server *user_server,
			 uint32_t *msid,
			 uint64_t *phys_addr,
			 uint64_t *rio_addr,
			 uint32_t *ms_conn_id,
			 uint32_t *bytes)
{
	DBG("ENTER\n");

	/* Find the memory space by name */
	mspace	*ms = get_mspace(name);
	if (!ms) {
		WARN("%s not found\n", name);
		return -1;
	}

	/* Open the memory space */
	if (ms->open(msid, user_server, ms_conn_id, bytes) < 0) {
		WARN("Failed to open '\%s\'\n", name);
		return -2;
	}
	*phys_addr = ms->get_phys_addr();
	*rio_addr  = ms->get_rio_addr();
	DBG("EXIT\n");
	return 0;
} /* open_mspace() */

/* Create a memory subspace */
int inbound::create_msubspace(uint32_t msid, uint32_t offset, uint32_t req_bytes,
			      uint32_t *size, uint32_t *msubid, uint64_t *rio_addr,
			      uint64_t *phys_addr)
{
	uint8_t	win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;

	sem_wait(&ibwins_sem);
	int ret = ibwins[win_num].get_mspace(msid)->create_msubspace(offset,
								     req_bytes,
								     size,
								     msubid,
								     rio_addr,
								     phys_addr);
	sem_post(&ibwins_sem);
	return ret;
} /* create_msubspace() */

/* Destroy a memory subspace */
int inbound::destroy_msubspace(uint32_t msid, uint32_t msubid)
{
	uint8_t	win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;

	sem_wait(&ibwins_sem);
	int ret = ibwins[win_num].get_mspace(msid)->destroy_msubspace(msubid);
	sem_post(&ibwins_sem);
	return ret;
} /* destroy_msubspace() */

