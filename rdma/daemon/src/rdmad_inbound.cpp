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

#include <cstdio>

#include <algorithm>
#include <vector>

#include "rdma_types.h"
#include "liblog.h"

#include "rdmad_msubspace.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"
#include "rdmad_functors.h"
#include "rdmad_inbound.h"

using namespace std;


struct ibw_free {
	void operator()(ibwin& ibw) { ibw.free(); }
};

struct ibw_dump_mspaces {
	void operator()(ibwin& ibw) { ibw.dump_mspace_info(); }
};

struct ibw_dump_mspaces_with_msubs {
	void operator()(ibwin& ibw) { ibw.dump_mspace_and_subs_info(); }
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
inbound::inbound(int mport_fd,
		 unsigned num_wins,
		 uint64_t win_size)
{
	/* Initialize inbound windows */
	for (unsigned i = 0; i < num_wins; i++) {
		try {
			ibwin win(mport_fd, i, win_size);
			ibwins.push_back(win);
		}
		catch(ibwin_map_exception e) {
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
	/* Free inbound windows mapped with riodp_ibwin_map() */
	for_each(ibwins.begin(), ibwins.end(), ibw_free());
} /* destructor */

void inbound::dump_info()
{
	printf("%8s %16s %16s %16s\n", "Win num", "Win size", "RIO Addr", "PCIe Addr");
	printf("%8s %16s %16s %16s\n", "-------", "--------", "--------", "---------");
	sem_wait(&ibwins_sem);
	for_each(ibwins.begin(), ibwins.end(), call_dump_info<ibwin>());
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

/* Dump memory space info for a memory space specified by name */
int inbound::dump_mspace_info(const char *name)
{
	/* Find the memory space by name */
	mspace	*ms = get_mspace(name);
	if (!ms) {
		WARN("%s not found\n", name);
		return -1;
	}
	ms->dump_info();
	return 0;
} /* dump_mspace_info() */

/* Dump memory space info for all memory spaces */
void inbound::dump_all_mspace_info()
{
	sem_wait(&ibwins_sem);
	for_each(ibwins.begin(), ibwins.end(), ibw_dump_mspaces());
	sem_post(&ibwins_sem);
} /* dump_all_mspace_info() */

/* Dump memory space info for all memory spaces and their msubs */
void inbound::dump_all_mspace_with_msubs_info()
{
	sem_wait(&ibwins_sem);
	for_each(ibwins.begin(), ibwins.end(), ibw_dump_mspaces_with_msubs());
	sem_post(&ibwins_sem);
} /* dump_all_mspace_with_msubs_info() */

/* Create a memory space within a window that has enough space */
int inbound::create_mspace(const char *name,
			   uint64_t size,
			   uint32_t msoid,
			   uint32_t *msid)
{
	ibwin_has_room	ibwhr(size);

	/* Find first inbound window having room for memory space */
	sem_wait(&ibwins_sem);
	auto win_it = find_if(begin(ibwins), end(ibwins), ibwhr);

	/* If none found, fail */
	if (win_it == ibwins.end()) {
		WARN("No room for ms of size 0x%lX\n", size);
		sem_post(&ibwins_sem);
		return -1;
	}

	/* Create the space */
	int ret = win_it->create_mspace(name, size, msoid, msid);
	sem_post(&ibwins_sem);
	return ret;
} /* create_mspace() */

/* Open memory space */
int inbound::open_mspace(const char *name,
			 uint32_t msoid,
			 uint32_t *msid,
			 uint32_t *ms_conn_id,
			 uint32_t *bytes)
{
	(void)msoid;
	DBG("ENTER\n");

	/* Find the memory space by name */
	mspace	*ms = get_mspace(name);
	if (!ms) {
		WARN("%s not found\n", name);
		return -1;
	}

	/* Open the memory space */
	if (ms->open(msid, ms_conn_id, bytes) < 0) {
		WARN("Failed to open '\%s\'\n", name);
		return -2;
	}
	DBG("EXIT\n");
	return 1;
} /* open_mspace() */

int inbound::close_mspace(uint32_t msid, uint32_t ms_conn_id)
{
	uint8_t	win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;

	sem_wait(&ibwins_sem);
	int ret = ibwins[win_num].close_mspace(msid, ms_conn_id);
	sem_post(&ibwins_sem);
	return ret;
} /* close_mspace() */

/* Destroy memory space */
int inbound::destroy_mspace(uint32_t msoid, uint32_t msid)
{
	uint8_t	win_num = (msid & MSID_WIN_MASK) >> MSID_WIN_SHIFT;

	sem_wait(&ibwins_sem);
	int ret = ibwins[win_num].destroy_mspace(msoid, msid);
	sem_post(&ibwins_sem);
	return ret;
} /* destroy_mspace() */

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

