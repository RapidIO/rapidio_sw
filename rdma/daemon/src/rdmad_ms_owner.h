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

#ifndef MS_OWNER_H
#define MS_OWNER_H

#include <stdint.h>

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include "unix_sock.h"
#include "rdma_types.h"
#include "rdmad_unix_msg.h"
#include "tx_engine.h"

using std::vector;
using std::string;

/* Global constants */
static const uint32_t MSO_CONN_ID_START	= 0x1;

/* Referenced class declarations */
class mspace;

class mso_user
{
public:
	mso_user(uint32_t mso_conn_id,
		tx_engine<unix_server, unix_msg_t> *tx_eng) :
		mso_conn_id(mso_conn_id), tx_eng(tx_eng)
	{
	}

	bool operator==(uint32_t mso_conn_id)
	{
		return this->mso_conn_id == mso_conn_id;
	}

	bool operator==(tx_engine<unix_server, unix_msg_t> *tx_eng)
	{
		return this->tx_eng == tx_eng;
	}

	tx_engine<unix_server, unix_msg_t> *get_tx_engine() { return tx_eng; }

private:
	static constexpr uint32_t MSO_CONN_ID_START = 0x01;
	uint32_t mso_conn_id;
	tx_engine<unix_server, unix_msg_t> *tx_eng;
};

class ms_owner
{
public:
	/* Constructor */
	ms_owner(const char *owner_name,
		 tx_engine<unix_server, unix_msg_t> *tx_eng, uint32_t msoid);

	/* Destructor */
	~ms_owner();

	/* Accessor */
	uint32_t get_msoid() const { return msoid; }

	const char *get_mso_name() const { return name.c_str(); }

	/* For finding an ms_owner by its msoid */
	bool operator ==(uint32_t msoid) { return this->msoid == msoid; }

	/* For finding an ms_owner by its name */
	bool operator ==(const char *owner_name) { return this->name == owner_name; }

	/* Stores handle of memory spaces currently owned by owner */
	void add_ms(mspace *ms);

	/* Removes handle of memory space from list of owned spaces */
	int remove_ms(mspace* ms);

	/* Returns whether this owner sill owns some memory spaces */
	bool owns_mspaces() { return ms_list.size() != 0; }

	void dump_info(struct cli_env *env);

	int open(uint32_t *msoid, uint32_t *mso_conn_id,
		 tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	int close(uint32_t mso_conn_id);

	int close(tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	tx_engine<unix_server, unix_msg_t> *get_tx_eng() const { return tx_eng; }

	bool has_user_tx_eng(tx_engine<unix_server, unix_msg_t> *tx_eng)
	{
		auto it = find(begin(users), end(users), tx_eng);
		return (it != end(users));
	}

private:
	void close_connections();

	string			name;
	tx_engine<unix_server, unix_msg_t> *tx_eng;
	uint32_t		msoid;
	vector<mspace *>	ms_list;
	vector<mso_user>	users;
	uint32_t		mso_conn_id;	// Next available mso_conn_id
};


#endif

