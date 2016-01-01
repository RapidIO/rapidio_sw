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

#ifndef MSPACE_H
#define MSPACE_H

#include <stdint.h>
#include <unistd.h>
#include <semaphore.h>

#include <cstdio>

#include <string>
#include <vector>
#include <set>

#include "msg_q.h"

using std::set;
using std::string;
using std::vector;

/* Global constants */
static const uint32_t MSID_WIN_SHIFT = 28;
static const uint32_t MSID_WIN_MASK  = 0xF0000000;
static const uint32_t MSID_MSINDEX_MASK = 0x0000FFFF;
static const uint32_t MSID_MSOID_MASK = 0x0FFF000;
static const uint32_t MSID_MSOID_SHIFT = 16;
static const uint32_t MSINDEX_MAX = 0xFFFF;
static const uint32_t MSUBINDEX_MAX = 0xFFFF;

/* Referenced classes declarations */
class unix_server;
class msubspace;

struct mspace_exception {
	mspace_exception(const char *msg) : err(msg) {}

	const char *err;
};

class ms_user
{
public:
	ms_user(unix_server *server,
		uint32_t ms_conn_id,
		msg_q<mq_close_ms_msg> *close_mq) :
	server(server), ms_conn_id(ms_conn_id), close_mq(close_mq)
	{}

	msg_q<mq_close_ms_msg> *get_mq() { return close_mq; }
	uint32_t get_ms_conn_id() const { return ms_conn_id; }
	bool operator ==(uint32_t ms_conn_id) { return ms_conn_id == this->ms_conn_id; }
	bool operator ==(unix_server *server) { return server == this->server; }

private:
	unix_server *server;
	uint32_t ms_conn_id;
	msg_q<mq_close_ms_msg> *close_mq;
};

struct remote_connection
{
	remote_connection(uint16_t client_destid, uint32_t client_msubid) :
		client_destid(client_destid), client_msubid(client_msubid)
	{}

	bool operator==(uint16_t client_destid)
	{
		return this->client_destid == client_destid;
	}

	bool operator==(uint32_t client_msubid)
	{
		return this->client_msubid == client_msubid;
	}

	uint16_t client_destid;
	uint32_t client_msubid;
};

class mspace 
{
public:
	/* Constructor */
	mspace(const char *name, uint32_t msid, uint64_t rio_addr,
	       uint64_t phys_addr, uint64_t size);

	/* Destructor */
	~mspace();

	int destroy();

	/* Accessors */
	uint64_t get_size() const { return size; }
	uint64_t get_rio_addr() const { return rio_addr; }
	uint64_t get_phys_addr() const { return phys_addr; }
	uint32_t get_msid() const { return msid; }
	uint16_t get_msindex() const { return msid & MSID_MSINDEX_MASK; }
	uint32_t get_msoid() const { return msoid; }
	bool is_free() const { return free;}
	const char* get_name() const { return name.c_str(); }
	bool is_accepted() const { return accepted;}

	/* Mutators */
	void set_size(uint64_t size) { this->size = size; }
	void set_msid(uint32_t msid) { this->msid = msid;}
	void set_used() { free = false; }
	void set_free() { free = true; }
	void set_msoid(uint32_t msoid) {
		this->msoid = msoid;
		this->msid &= ~MSID_MSOID_MASK;	/* Clear previous owner */
		this->msid |= ((msoid << MSID_MSOID_SHIFT) & MSID_MSOID_MASK);
	}
	void set_name(const char *name) { this->name = name; }
	void set_accepted(bool accepted) { this->accepted = accepted; }

	/* Connections by clients that have connected to this memory space */
	void add_rem_connection(uint16_t client_destid, uint32_t client_msubid);
	int remove_rem_connection(uint16_t destid, uint32_t client_msubid);

	set<uint16_t> get_rem_destids();

	/* Debugging */
	void dump_info(struct cli_env *env);
	void dump_info_msubs_only(struct cli_env *env);
	void dump_info_with_msubs(struct cli_env *env);

	/* For finding a memory space by its msid */
	bool operator==(uint32_t msid) { return this->msid == msid; }

	/* For finding a memory space by its name */
	bool operator==(const char *name) { return this->name == name; }

	int open(uint32_t *msid, unix_server *user_server,
					uint32_t *ms_conn_id, uint32_t *bytes);

	bool has_user_with_user_server(unix_server *server, uint32_t *ms_conn_id);

	bool connected_by_destid(uint16_t destid);

	int close(uint32_t ms_conn_id);

	/* For creating a memory sub-space */
	int create_msubspace(uint32_t offset,
			     uint32_t req_size,
			     uint32_t *size,
			     uint32_t *msubid,
			     uint64_t *rio_addr,
			     uint64_t *phys_addr);

	int destroy_msubspace(uint32_t msubid);
	int disconnect(uint32_t client_msubid);
	int disconnect_from_destid(uint16_t client_destid);

private:
	/* Constants */
	static constexpr uint32_t MS_CONN_ID_START = 0x01;

	int notify_remote_clients();
	int close_connections();

	string		name;
	uint32_t	msid;
	uint64_t	rio_addr;
	uint64_t	phys_addr;
	uint32_t	size;
	uint32_t	msoid;
	bool		free;
	uint32_t	current_ms_conn_id;
	bool		accepted;

	/* Info about users that have opened the ms */
	vector<ms_user>		users;
	sem_t			users_sem;

	/* Memory sub-space indexes */
	bool msubindex_free_list[MSUBINDEX_MAX+1];	/* List of memory sub-space IDs */
	sem_t 			msubindex_free_list_sem;

	/* Memory subspaces */
	vector<msubspace>	msubspaces;
	sem_t			msubspaces_sem;

	/* List of connections to remote clients of this memory space */
	vector<remote_connection>	rem_connections;
	sem_t				rem_connections_sem;
}; /* mspace */


#endif


