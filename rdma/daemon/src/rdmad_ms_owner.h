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
#include <pthread.h>

#include <string>
#include <vector>
#include <exception>

#include "tx_engine.h"

using std::vector;
using std::string;
using std::exception;

/* Referenced class declarations */
class mspace;
class unix_msg_t;
class unix_server;

/**
 * @brief Inbound window mapping exception
 */
class ms_owner_exception : public exception {
public:
	ms_owner_exception(const char *msg) : err(msg)
	{
	}

	const char *what() { return err; }
private:
	const char *err;
};

class ms_owner
{
	friend class ms_owners;

public:
	/**
	 * @brief Constructor
	 *
	 * @param owner_name	Owner name
	 *
	 * @param tx_eng	Daemon-to-app Tx engine
	 *
	 * @param msoid		Memory space owner identifier
	 *
	 * @throws ms_owner_exception
	 */
	ms_owner(const char *owner_name,
		 tx_engine<unix_server, unix_msg_t> *tx_eng, uint32_t msoid);

	/**
	 * @brief Destructor
	 */
	~ms_owner();

	/* Accessors */
	uint32_t get_msoid() const { return msoid; }

	const char *get_mso_name() const { return name.c_str(); }

	/**
	 * @brief Equality operator fr finding an ms_owner by its msoid
	 */
	bool operator ==(uint32_t msoid) { return this->msoid == msoid; }

	/**
	 * @brief Equality operator fr finding an ms_owner by its name
	 */
	bool operator ==(const char *owner_name) const
	{
		return this->name == owner_name;
	}

	/**
	 * @brief Stores memory space in list of owned spaces
	 *
	 * @param ms	Pointer to memory space
	 */
	void add_ms(mspace *ms);

	/**
	 * @brief Removes memory space from list of owned spaces
	 *
	 * @param ms	Pointer to memory space
	 *
	 * @return 0 if successful, non-zer otherwise
	 */
	int remove_ms(mspace* ms);

	/**
	 * @brief Returns whether this owner still owns memory spaces
	 *
	 * @return true if still owns, false otherwise
	 */
	bool owns_mspaces();

	void dump_info(struct cli_env *env);

	int open(uint32_t *msoid, tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	int close(tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	tx_engine<unix_server, unix_msg_t> *get_tx_eng() const { return tx_eng; }

	bool has_user_tx_eng(tx_engine<unix_server, unix_msg_t> *tx_eng)
	{
		auto it = find(begin(users_tx_eng), end(users_tx_eng), tx_eng);
		return (it != end(users_tx_eng));
	}

private:
	using mspace_list 	= vector<mspace *>;
	using user_tx_eng	= tx_engine<unix_server, unix_msg_t>;
	using user_tx_eng_list  = vector<user_tx_eng *>;

	void close_connections();

	string		name;
	tx_engine<unix_server, unix_msg_t> *tx_eng;
	uint32_t	msoid;
	mspace_list	ms_list;
	pthread_mutex_t	ms_list_lock;
	user_tx_eng_list   users_tx_eng;
};

#endif

