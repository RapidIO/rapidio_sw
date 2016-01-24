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
using std::list;

#include "rdma_types.h"
#include "msg_q.h"
#include "rdma_mq_msg.h"

int rdma_db_init();

/**
 * Memory space owners.
 */
class loc_mso {
public:
	loc_mso(const char *name, uint32_t msoid, uint32_t mso_conn_id, bool owned) :
		name(strdup(name)), msoid(msoid), mso_conn_id(mso_conn_id), owned(owned)
	{
	}
	~loc_mso()
	{
		free((void *)name);
	}
	const char *name;
	uint32_t msoid;
	uint32_t mso_conn_id;	// if 'owned' is false
	bool owned;
};

mso_h add_loc_mso(const char* mso_name,
		  uint32_t msoid,
		  uint32_t mso_conn_id,
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
	char *name;
	uint32_t   bytes;
	uint64_t   msoh;
	uint32_t   msid;
	uint64_t   phys_addr;	/* phys_addr and rio_addr maybe the same */
	uint64_t   rio_addr;	/* if direct mapping is used. */
	uint32_t   ms_conn_id;
	bool	   owned;
	bool	   accepted;

	loc_ms(const char *name, uint32_t bytes, mso_h msoh, uint32_t msid,
		uint64_t phys_addr, uint64_t rio_addr, uint32_t ms_conn_id,
		bool owned, pthread_t disc_thread, msg_q<mq_rdma_msg> *disc_notify_mq) :
	name(strdup(name)), bytes(bytes), msoh(msoh), msid(msid), phys_addr(phys_addr),
	rio_addr(rio_addr), ms_conn_id(ms_conn_id), owned(owned), accepted(false),
	disc_thread(disc_thread), disc_notify_mq(disc_notify_mq)
	{
	}

	~loc_ms()
	{
		free((void *)name);
	}
	/* The following fields are used for connect/disconnect notification. */
	pthread_t  disc_thread;
	msg_q<mq_rdma_msg> *disc_notify_mq;
};

ms_h add_loc_ms(const char *ms_name,
		uint64_t bytes,
		mso_h msoh,
		uint32_t msid,
		uint64_t phys_addr,
		uint64_t rio_addr,
		uint32_t mso_conn_id,
		bool owned,
		pthread_t disc_thread,
		msg_q<mq_rdma_msg> *disc_msg);

int remove_loc_ms(ms_h msh);
ms_h find_loc_ms(uint32_t msid);
ms_h find_loc_ms_by_name(const char *ms_name);
int get_info_from_loc_ms(ms_h msh, uint32_t *msid);
unsigned get_num_ms_by_msoh(mso_h msoh);
void get_list_msh_by_msoh(mso_h msoh, list<struct loc_ms *>& msh_list);

pthread_t loc_ms_get_disc_thread(ms_h msh);
msg_q<mq_rdma_msg> *loc_ms_get_disc_notify_mq(ms_h msh);

bool loc_ms_exists(ms_h msh);

struct rem_ms {
	char 		*name;
	uint32_t 	msid;
	pthread_t	wait_for_destroy_thread;
	msg_q<mq_destroy_msg>	*destroy_mq;
};

ms_h add_rem_ms(const char *name,
		uint32_t msid,
		pthread_t wait_for_destroy_thread,
		msg_q<mq_destroy_msg> *destroy_mq);

ms_h find_rem_ms(uint32_t msid);
int remove_rem_ms(ms_h msh);
bool rem_ms_exists(ms_h msh);

/**
 * Memory subspaces.
 */
struct loc_msub {
	uint32_t	msid;
	uint32_t	msubid;
	uint32_t	bytes;
	uint8_t		rio_addr_len;
	uint64_t	rio_addr_lo;
	uint8_t		rio_addr_hi;
	uint64_t	paddr;
	loc_msub(uint32_t msid, uint32_t msubid, uint32_t bytes, uint8_t rio_addr_len,
		 uint64_t rio_addr_lo, uint8_t	rio_addr_hi, uint64_t paddr) :
	msid(msid), msubid(msubid), bytes(bytes), rio_addr_len(rio_addr_len),
		rio_addr_lo(rio_addr_lo), rio_addr_hi(rio_addr_hi), paddr(paddr)
	{
	}
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
