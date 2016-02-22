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
#ifndef DAEMON_INFO_H_
#define DAEMON_INFO_H_

#include <stdint.h>
#include <pthread.h>

#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>

#include "memory_supp.h"
#include "cm_sock.h"

using std::vector;
using std::unique_ptr;
using std::move;
using std::find;
using std::mutex;

class daemon_list;

/**
 * @brief Info for remote daemons provisined by the provisioning thread
 * 	  (i.e. by receiving a HELLO message).
 */
class daemon_info
{
public:
	friend daemon_list;

	/**
	 * @brief Constructor
	 *
	 * @param destid  Destination ID for remote daemon
	 *
	 * @param cm_sock CM socket for communicating with remote daemon
	 *
	 * @param tid	  Thread for receiving messages from remote daemon
	 */
	daemon_info(uint32_t destid,
	   	   cm_base *cm_sock,
		   pthread_t tid);

	/**
	 * @brief Destructor
	 */
	~daemon_info();

	/**
	 * @brief Equality operator matching daemon with destid
	 *
	 * @param destid Destination ID for remote daemon
	 */
	bool operator==(uint32_t destid);

private:
	uint32_t 	destid;
	cm_base		*cm_sock;
	pthread_t	tid;
};

class daemon_list
{
public:
	/**
	 * @brief Default constructor
	 */
	daemon_list() = default;

	/**
	 * @brief Destructor
	 */
	~daemon_list();

	/**
	 * @brief Returns CM socket used to communicate with remote daemons
	 * 	  using specified destination ID.
	 */
	cm_base* get_cm_sock_by_destid(uint32_t destid);

	/**
	 * @brief Add daemon entry. If an entry exists for the specified destid
	 * 	  the corresponding thread is killed and the entry is updated
	 * 	  with the new information specified here.
	 *
	 * @param destid	Destination ID for remote daemon
	 *
	 * @param cm_sock	CM socket object used to communicate with
	 * 			remote node
	 *
	 * @param tid		Thread handling incoming messages from remote
	 * 			daemon
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	void add_daemon(uint32_t destid, cm_base *cm_sock, pthread_t tid);


	/**
	 * @brief Remove daemon entry specified by destid
	 *
	 * @param destid  Destination ID for remote daemon
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int remove_daemon(uint32_t destid);


private:
	using daemon_element  = unique_ptr<daemon_info>;
	using daemon_list_t   = vector<daemon_element>;
	using daemon_iterator = daemon_list_t::iterator;

	daemon_list_t	daemons;
	mutex		daemons_mutex;
};

#endif
