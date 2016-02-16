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

#ifndef RDMALIB_DB_H
#define RDMALIB_DB_H

#include <stdint.h>
#include <pthread.h>

#include <cstring>

#include <list>
#include <vector>
using std::list;
using std::vector;

#include "rdma_types.h"

int rdma_db_init();

typedef vector<conn_h>	conn_h_list;

/**
 * Memory space owners.
 */
class loc_mso {
public:
	loc_mso(const char *name, uint32_t msoid, bool owned) :
		name(strdup(name)), msoid(msoid), owned(owned)
	{
	}
	~loc_mso()
	{
		free((void *)name);
	}
	const char *name;
	uint32_t msoid;
	bool owned;
};

mso_h add_loc_mso(const char* mso_name,
		  uint32_t msoid,
		  bool owned);

void dump_loc_ms(void);

int remove_loc_mso(mso_h msoh);

int remove_loc_mso(uint32_t msoid);

bool mso_is_open(const char *name);

mso_h	find_mso(uint32_t msoid);

mso_h	find_mso_by_name(const char *name);

bool mso_h_exists(mso_h msoh);

/**
 * Memory spaces.
 */
struct loc_ms {

	loc_ms(const char *name, uint32_t bytes, mso_h msoh, uint32_t msid,
		uint64_t phys_addr, uint64_t rio_addr, bool owned) :
	name(strdup(name)), bytes(bytes), msoh(msoh), msid(msid), phys_addr(phys_addr),
	rio_addr(rio_addr), owned(owned)
	{
	}

	~loc_ms()
	{
		free((void *)name);
	}

	char *name;
	uint32_t   bytes;
	uint64_t   msoh;
	uint32_t   msid;
	uint64_t   phys_addr;	/* phys_addr and rio_addr maybe the same */
	uint64_t   rio_addr;	/* if direct mapping is used. */
	bool	   owned;
};

ms_h add_loc_ms(const char *ms_name,
		uint64_t bytes,
		mso_h msoh,
		uint32_t msid,
		uint64_t phys_addr,
		uint64_t rio_addr,
		bool owned);

int remove_loc_ms(ms_h msh);
ms_h find_loc_ms(uint32_t msid);
ms_h find_loc_ms_by_name(const char *ms_name);
int get_info_from_loc_ms(ms_h msh, uint32_t *msid);
unsigned get_num_ms_by_msoh(mso_h msoh);
void get_list_msh_by_msoh(mso_h msoh, list<struct loc_ms *>& msh_list);
bool loc_ms_exists(ms_h msh);

struct rem_ms {
	char 		*name;
	uint32_t 	msid;
};

ms_h add_rem_ms(const char *name, uint32_t msid);
ms_h find_rem_ms(uint32_t msid);
int remove_rem_ms(ms_h msh);
int remove_rem_ms(uint32_t msid);
bool rem_ms_exists(ms_h msh);

struct client_connection {
	client_connection(uint64_t connh, uint64_t client_to_lib_tx_eng_h)
		: connh(connh), client_to_lib_tx_eng_h(client_to_lib_tx_eng_h)
	{}
	uint64_t connh;
	uint64_t client_to_lib_tx_eng_h;
};

typedef vector<client_connection> connection_list;
/**
 * Memory subspaces.
 */
struct loc_msub {
	loc_msub(uint32_t msid, uint32_t msubid, uint32_t bytes, uint8_t rio_addr_len,
		 uint64_t rio_addr_lo, uint8_t	rio_addr_hi, uint64_t paddr) :
	msid(msid), msubid(msubid), bytes(bytes), rio_addr_len(rio_addr_len),
		rio_addr_lo(rio_addr_lo), rio_addr_hi(rio_addr_hi), paddr(paddr)
	{
	}

	uint32_t	msid;
	uint32_t	msubid;
	uint32_t	bytes;
	uint8_t		rio_addr_len;
	uint64_t	rio_addr_lo;
	uint8_t		rio_addr_hi;
	uint64_t	paddr;
	connection_list	connections;
};

struct rem_msub {
	uint32_t	msubid;
	uint32_t	msid;
	uint32_t	bytes;
	uint8_t		rio_addr_len;
	uint64_t	rio_addr_lo;
	uint8_t		rio_addr_hi;
	uint8_t		destid_len;
	uint32_t	destid;
	ms_h		loc_msh;
};

/* Local msub functions */
ms_h add_loc_msub(uint32_t msubid,
		  uint32_t	msid,
		  uint32_t	bytes,
		  uint8_t	rio_addr_len,
		  uint64_t	rio_addr_lo,
		  uint8_t	rio_addr_hi,
		  uint64_t	paddr);
msub_h find_loc_msub(uint32_t msubid);
int remove_loc_msub(msub_h msubh);
unsigned get_num_loc_msub_in_ms(uint32_t msid);
void get_list_loc_msub_in_msid(uint32_t msid, list<loc_msub *>& msub_list);
msub_h find_loc_msub_by_connh(conn_h connh);

/* Remote msub functions */
msub_h add_rem_msub(uint32_t	rem_msubid,
		    uint32_t	rem_msid,
		    uint32_t	rem_bytes,
		    uint8_t	rem_rio_addr_len,
		    uint64_t	rem_rio_addr_lo,
		    uint8_t	rem_rio_addr_hi,
		    uint8_t	destid_len,
		    uint32_t	destid,
		    ms_h	loc_msh);
msub_h find_rem_msub(uint32_t msubid);
int remove_rem_msub(uint32_t msubid);
int remove_rem_msub(msub_h msub);
unsigned get_num_rem_msubs(void);
void remove_rem_msubs_in_ms(uint32_t msid);
msub_h find_any_rem_msub_in_ms(uint32_t msid);
void remove_rem_msub_by_loc_msh(ms_h loc_msh);

void purge_loc_msub_list(void);
void purge_loc_ms_list(void);
void purge_loc_mso_list(void);
void purge_local_database(void);

#endif
