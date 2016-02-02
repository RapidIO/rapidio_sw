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
#include <exception>

#include "cm_sock.h"
#include "unix_sock.h"
#include "rdma_types.h"
#include "rdmad_unix_msg.h"
#include "tx_engine.h"

using std::set;
using std::string;
using std::vector;
using std::exception;

/* Global constants */
constexpr uint32_t MSID_WIN_SHIFT 	= 28;
constexpr uint32_t MSID_WIN_MASK  	= 0xF0000000;
constexpr uint32_t MSID_MSINDEX_MASK 	= 0x0000FFFF;
constexpr uint32_t MSID_MSOID_MASK 	= 0x0FFF000;
constexpr uint32_t MSID_MSOID_SHIFT 	= 16;
constexpr uint32_t MSINDEX_MAX 		= 0xFFFF;
constexpr uint32_t MSUBINDEX_MAX 	= 0xFFFF;

/* Referenced classes declarations */
class unix_server;
class msubspace;

class mspace_exception : public exception {
public:
	mspace_exception(const char *msg) : err(msg) {}
	const char *what() { return err; }
private:
	const char *err;
};

class ms_user
{
	friend class mspace;
public:
	ms_user(uint32_t ms_conn_id, tx_engine<unix_server, unix_msg_t> *tx_eng) :
		tx_eng(tx_eng),
		ms_conn_id(ms_conn_id),
		accepting(false),
		connected_to(false),
		server_msubid(0),
		client_destid(0xFFFF),
		client_msubid(0),
		client_to_lib_tx_eng_h(0)
	{
	}

	ms_user(const ms_user& other) :
		tx_eng(other.tx_eng),
		ms_conn_id(other.ms_conn_id),
		accepting(other.accepting),
		connected_to(other.connected_to),
		server_msubid(other.server_msubid),
		client_destid(other.client_destid),
		client_msubid(other.client_msubid),
		client_to_lib_tx_eng_h(other.client_to_lib_tx_eng_h)
	{
	}

	ms_user& operator=(const ms_user& rhs)
	{
		tx_eng 		= rhs.tx_eng;
		ms_conn_id	= rhs.ms_conn_id;
		accepting	= rhs.accepting;
		connected_to	= rhs.connected_to;
		server_msubid	= rhs.server_msubid;
		client_destid	= rhs.client_destid;
		client_msubid	= rhs.client_msubid;
		client_to_lib_tx_eng_h = rhs.client_to_lib_tx_eng_h;
		return *this;
	}

	bool connected_to_destid(uint16_t client_destid)
	{
		return this->client_destid == client_destid;
	}

	uint32_t get_ms_conn_id() const
	{
		return ms_conn_id;
	}

	bool operator ==(tx_engine<unix_server, unix_msg_t> *tx_eng)
	{
		return tx_eng == this->tx_eng;
	}

private:
	tx_engine<unix_server, unix_msg_t> *tx_eng;
	uint32_t ms_conn_id;
	bool	accepting;
	bool	connected_to;
	uint32_t server_msubid;		 /* Upon accepting */
	uint16_t client_destid;		 /* When user is 'connected_to' */
	uint32_t client_msubid;		 /* Ditto */
	uint64_t client_to_lib_tx_eng_h; /* Ditto */
}; /* ms_user */

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
	bool is_connected_to() const { return connected_to;}
	bool is_accepting() const { return accepting; }

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
	void set_connected_to(bool connected_to)
					{ this->connected_to = connected_to; }
	void set_accepting(bool accepting) { this->accepting = accepting; }
	void set_creator_tx_eng(
			tx_engine<unix_server, unix_msg_t> *creator_tx_eng)
	{
		this->creator_tx_eng = creator_tx_eng;
	}

	/* Connections by clients that have connected to this memory space */
	int  add_rem_connection(uint16_t client_destid,
				uint32_t client_msubid,
				uint64_t client_to_lib_tx_eng_h);

	int remove_rem_connection(uint16_t client_destid,
				  uint32_t client_msubid,
				  uint64_t client_to_lib_tx_eng_h);

	set<uint16_t> get_rem_destids();

	/* Debugging */
	void dump_info(struct cli_env *env);
	void dump_info_msubs_only(struct cli_env *env);
	void dump_info_with_msubs(struct cli_env *env);

	/* For finding a memory space by its msid */
	bool operator==(uint32_t msid) { return this->msid == msid; }

	/* For finding a memory space by its name */
	bool operator==(const char *name) { return this->name == name; }

	int open(uint32_t *msid,
		 tx_engine<unix_server, unix_msg_t> *user_tx_eng,
		 uint32_t *ms_conn_id,
		 uint32_t *bytes);

	tx_engine<unix_server, unix_msg_t> *get_accepting_tx_eng();

	int accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng,
			uint32_t server_msubid);

	int undo_accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	bool has_user_with_user_tx_eng(
			tx_engine<unix_server, unix_msg_t> *user_tx_eng,
			uint32_t *ms_conn_id);
	bool connected_by_destid(uint16_t destid);

	int close(tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	/* For creating a memory sub-space */
	int create_msubspace(uint32_t offset,
			     uint32_t req_size,
			     uint32_t *size,
			     uint32_t *msubid,
			     uint64_t *rio_addr,
			     uint64_t *phys_addr);

	int destroy_msubspace(uint32_t msubid);

	int client_disconnect(uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h);

	int server_disconnect(uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h);

	void disconnect_from_destid(uint16_t client_destid);

	void send_disconnect_to_lib(uint32_t client_msubid,
				tx_engine<unix_server, unix_msg_t> *tx_eng);

private:
	/* Private methods */
	mspace(const mspace&);
	mspace& operator=(const mspace&);
	int send_cm_force_disconnect_ms(cm_server *server, uint32_t server_msubid,
					uint64_t client_to_lib_tx_eng);
	int disconnect(bool is_client, uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h);
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

	/* Data members specific to the mspace creator */
	bool		connected_to;	/* Has a connection from a remote client
					   to the 'accepting' CREATOR */
	bool		accepting;	/* Has called accept from the CREATOR */
	uint32_t	server_msubid;	/* Used when accepting from the CREATOR */
	tx_engine<unix_server, unix_msg_t> *creator_tx_eng;
	uint16_t client_destid;		 /* When creator is 'connected_to' */
	uint32_t client_msubid;		 /* Ditto */
	uint64_t client_to_lib_tx_eng_h; /* Ditto */

	/* Info about users that have opened the ms */
	vector<ms_user>		users;
	sem_t			users_sem;

	/* Memory sub-space indexes */
	bool msubindex_free_list[MSUBINDEX_MAX+1];	/* List of memory sub-space IDs */
	sem_t 			msubindex_free_list_sem;

	/* Memory subspaces */
	vector<msubspace>	msubspaces;
	sem_t			msubspaces_sem;
}; /* mspace */


#endif


