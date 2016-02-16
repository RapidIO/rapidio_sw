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

#include "rdma_types.h"
#include "liblog.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_dma.h"
#include "rdmad_msubspace.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"
#include "rdmad_inbound.h"

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
			throw inbound_exception(e.what());
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
	for_each(begin(ibwins), end(ibwins), [](ibwin& ibw) {ibw.free();});
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
	mspace *ms = nullptr;

	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		ms = ibwin.get_mspace(name);
		if (ms != nullptr) /* Found it */
			break;
	}
	sem_post(&ibwins_sem);

	if (ms == nullptr) {
		WARN("%s not found\n", name);
	}

	return ms;
} /* get_mspace() */

/* get_mspace by msid */
mspace* inbound::get_mspace(uint32_t msid)
{
	mspace *ms = nullptr;

	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		ms = ibwin.get_mspace(msid);
		if (ms != nullptr)
			break;
	}
	sem_post(&ibwins_sem);

	if (ms == nullptr) {
		WARN("msid(0x%X) not found\n", msid);
	}
	return ms;
} /* get_mspace() */

int inbound::get_mspaces_connected_by_destid(uint32_t destid,
					     mspace_list& mspaces)
{
	mspace_iterator ins_point = begin(mspaces);

	sem_wait(&ibwins_sem);
	for (auto& ibwin : ibwins) {
		mspace_list  ibw_mspaces;
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

void inbound::close_and_destroy_mspaces_using_tx_eng(tx_engine<unix_server,
						     unix_msg_t> *app_tx_eng)
{
	sem_wait(&ibwins_sem);
	for (auto& ibw : ibwins) {
		INFO("Closing and destroying for ibwin %u\n", ibw.get_win_num());
		ibw.close_mspaces_using_tx_eng(app_tx_eng);
		ibw.destroy_mspaces_using_tx_eng(app_tx_eng);
	}
	sem_post(&ibwins_sem);
} /* close_mspaces_using_tx_eng() */

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
void inbound::dump_mspace_info(struct cli_env *env, const char *name)
{
	/* Find the memory space by name */
	mspace	*ms = get_mspace(name);
	if (ms == nullptr) {
		WARN("%s not found\n", name);
	} else {
		ms->dump_info(env);
	}
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

	if (win_num >= ibwins.size()) {
		ERR("Bad window number: %u\n", win_num);
		ret = -1;
	} else {
		auto& ibw = ibwins[win_num];
		ret = ibw.destroy_mspace(msoid, msid);
	}
	sem_post(&ibwins_sem);

	return ret;
} /* destroy_mspace() */

/* Create a memory space within a window that has enough space */
int inbound::create_mspace(const char *name,
			   uint64_t size,
			   uint32_t msoid,
			   mspace **ms,
			   tx_engine<unix_server, unix_msg_t> *creator_tx_eng)
{
	DBG("ENTER\n");
	/* Find first inbound window having room for memory space */
	sem_wait(&ibwins_sem);
	auto win_it = find_if(begin(ibwins), end(ibwins),
			[size](ibwin& ibw) { return ibw.has_room_for_ms(size); });

	/* If none found, fail */
	if (win_it == ibwins.end()) {
		WARN("No room for ms of size 0x%" PRIx64 "\n", size);
		sem_post(&ibwins_sem);
		return RDMA_NO_ROOM_FOR_MS;
	}

	/* Create the space */
	int ret = win_it->create_mspace(name, size, msoid, ms, creator_tx_eng);

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
				ret = RDMA_UNMAP_ERROR;
			}
		} else {
			ERR("Failed to MMAP mspace %s: %s\n",
						name, strerror(errno));
			ret = RDMA_MAP_ERROR;
		}
	}

	sem_post(&ibwins_sem);
	DBG("EXIT\n");
	return ret;
} /* create_mspace() */

/* Open memory space */
int inbound::open_mspace(const char *name,
			 tx_engine<unix_server, unix_msg_t> *user_tx_eng,
			 uint32_t *msid,
			 uint64_t *phys_addr,
			 uint64_t *rio_addr,
			 uint32_t *bytes)
{
	DBG("ENTER\n");
	int	ret;

	/* Find the memory space by name */
	mspace	*ms = get_mspace(name);
	if (ms == nullptr) {
		WARN("%s not found\n", name);
		ret = RDMA_INVALID_MS;
	} else {
		/* Open the memory space */
		ret = ms->open(user_tx_eng);
		if (ret) {
			ERR("Failed to open '%s'\n", name);
		} else {
			*msid      = ms->get_msid();
			*phys_addr = ms->get_phys_addr();
			*rio_addr  = ms->get_rio_addr();
			*bytes	   = ms->get_size();
			ret = 0;
		}
	}
	DBG("EXIT\n");
	return ret;
} /* open_mspace() */

/* Create a memory subspace */
int inbound::create_msubspace(uint32_t msid, uint32_t offset, uint32_t req_bytes,
			      uint32_t *size, uint32_t *msubid, uint64_t *rio_addr,
			      uint64_t *phys_addr,
			      const tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	uint8_t	win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;
	int	ret;

	if (win_num >= ibwins.size()) {
		ERR("Invalid window number: %u\n", win_num);
		ret = -1;
	} else {
		sem_wait(&ibwins_sem);

		mspace *ms = ibwins[win_num].get_mspace(msid);
		if (ms != nullptr) {
			ret = ms->create_msubspace(offset, req_bytes, size,
					     msubid, rio_addr, phys_addr,
					     user_tx_eng);
		} else {
			ERR("Failed to find mspace with msid(0x%X)\n", msid);
			ret = -2;
		}
		sem_post(&ibwins_sem);
	}

	return ret;
} /* create_msubspace() */

/* Destroy a memory subspace */
int inbound::destroy_msubspace(uint32_t msid, uint32_t msubid)
{
	int	ret;
	uint8_t	win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;

	DBG("msid = 0x%X, msubid = 0x%X\n", msid, msubid);
	if (win_num >= ibwins.size()) {
		ERR("Invalid window number: %u\n", win_num);
		ret = -1;
	} else {
		sem_wait(&ibwins_sem);
		mspace *ms = ibwins[win_num].get_mspace(msid);
		if (ms == nullptr) {
			ERR("Failed to find mspace with msid(0x%X)\n", msid);
			ret = -2;
		} else {
			ret = ms->destroy_msubspace(msubid);
		}
		sem_post(&ibwins_sem);
	}

	return ret;
} /* destroy_msubspace() */

