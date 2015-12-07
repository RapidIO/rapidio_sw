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
#include <rapidio_mport_mgmt.h>
#include "rapidio_mport_dma.h"
#include "liblog.h"
#include "libcli.h"

#include "rdmad_mspace.h"

using std::vector;

// TODO: Use a list instead of vector?
typedef vector<mspace*>		mspace_list;
typedef mspace_list::iterator	mspace_iterator;

struct ibwin_map_exception {
	ibwin_map_exception(const char *msg) : err(msg)
	{
	}

	const char *err;
};

class ibwin 
{
public:
	/* Constructor */
	ibwin(riomp_mport_t mport_hnd, unsigned win_num, uint64_t size);

	/* Called from destructor ~inbound() */
	void free();

	void dump_info(struct cli_env *env);

	void print_mspace_header(struct cli_env *env);

	void dump_mspace_info(struct cli_env *env);

	void dump_mspace_and_subs_info(cli_env *env);

	int get_free_mspaces_large_enough(uint64_t size, mspace_list& le_mspaces);

	bool has_room_for_ms(uint64_t size);

	/* Create memory space */
	int create_mspace(const char *name,
			  uint64_t size,
			  uint32_t msoid,
			  uint32_t *msid,
			  mspace **ms);

	int destroy_mspace(uint32_t msoid, uint32_t msid);

	void merge_other_with_mspace(mspace_iterator current, mspace_iterator other);

	mspace* get_mspace(const char *name);

	mspace* get_mspace(uint32_t msid);

	mspace* get_mspace(uint32_t msoid, uint32_t msid);

	mspace *get_mspace_open_by_server(unix_server *server, uint32_t *ms_conn_id);

	void get_mspaces_connected_by_destid(uint32_t destid, mspace_list& mspaces);

	vector<mspace *>& get_mspaces() { return mspaces; };

	unsigned get_win_num() const { return win_num; };

private:
	riomp_mport_t mport_hnd;	/* Master port handle */
	unsigned	win_num;	/* window number */
	uint64_t	rio_addr;	/* starting address in RIO space */
	uint64_t	phys_addr;	/* starting physical address */
	uint64_t	size;		/* window size in bytes */

	/* Memory space indexes */
	bool msindex_free_list[MSINDEX_MAX+1];	/* List of memory space IDs */
	pthread_mutex_t msindex_lock;

	mspace_list	mspaces;
	pthread_mutex_t mspaces_lock;
}; /* ibwin */


#endif


