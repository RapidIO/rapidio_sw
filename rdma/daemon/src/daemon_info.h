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
#include "rdmad_cm.h"
#include "tx_engine.h"

using std::vector;
using std::unique_ptr;
using std::move;
using std::find;
using std::mutex;
using std::lock_guard;

template <typename C>
class daemon_list;

/**
 * @brief Info for remote daemons provisioned by sending (client) or
 * 	  receiving (server) HELLO.
 *
 * @param C	CM socket client or server (i.e. cm_client or cm_server)
 */
template <typename C>
class daemon_info
{
public:
	friend daemon_list<C>;

	/**
	 * @brief Constructor
	 *
	 * @param tx_eng Tx engine for communicating with remote daemon
	 *
	 * @param rx_eng Rx engine for communicating with remote daemon
	 */
	daemon_info(unique_ptr<tx_engine<C,cm_msg_t> > tx_eng,
		    unique_ptr<rx_engine<C,cm_msg_t> > rx_eng) :
		destid(NULL_DESTID),
		m_tx_eng(move(tx_eng)),
		m_rx_eng(move(rx_eng)),
		provisioned(false)
	{
	} /* ctor */

	/**
	 * @brief Destructor
	 */
	~daemon_info() = default;

	/**
	 * @brief Mutator(s)
	 */
	void set_destid(uint32_t destid) { this->destid = destid; }

	void set_provisioned(bool provisioned) { this->provisioned = provisioned; }

private:
	uint32_t 			   destid;
	unique_ptr<tx_engine<C, cm_msg_t>> m_tx_eng;
	unique_ptr<rx_engine<C, cm_msg_t>> m_rx_eng;
	bool				   provisioned;
};

/**
 * @brief List of daemon info elements containing the Tx engine and destid
 *
 * @param C	CM socket client or server (i.e. cm_client or cm_server)
 */
template <typename C>
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
	~daemon_list() { daemons.clear(); }

	void clear() { daemons.clear(); }

	/**
	 * @brief Returns pointer to Tx engine used to communicate with
	 * 	  remote daemons using specified destination ID.
	 */
	tx_engine<C, cm_msg_t>* get_tx_eng_by_destid(uint32_t destid)
	{
		lock_guard<mutex> daemons_lock(daemons_mutex);

		auto it = find_if(begin(daemons), end(daemons),
			[destid](daemon_element_t& daemon_element)
				{return daemon_element->destid == destid;});

		return (it == end(daemons)) ? nullptr : (*it)->m_tx_eng.get();
	} /* get_tx_eng_by_destid() */

	/**
	 * @brief Create daemon element with only tx_eng/rx_eng populated
	 *
	 * @param tx_eng	Tx engine used to communicate with remote daemon
	 *
	 * @param rx_eng	Rx engine used to communicate with remote daemon
	 */
	void add_daemon(unique_ptr<tx_engine<C, cm_msg_t>> tx_eng,
			unique_ptr<rx_engine<C, cm_msg_t>> rx_eng)
	{
		(void)rx_eng;
		lock_guard<mutex> daemons_lock(daemons_mutex);

		auto it = find_if(begin(daemons), end(daemons),
			[&tx_eng](daemon_element_t& daemon_element)
				{ return daemon_element->m_tx_eng == tx_eng;});
		if (it != end(daemons)) {
			CRIT("Trying to add same Tx engine twice\n");
			abort();
		} else {
			/* This is a brand new entry */
			daemon_element_t daemon =
				make_unique<daemon_info<C>>(move(tx_eng), move(rx_eng));

			daemons.push_back(move(daemon));
		}
	} /* add_daemon() */

	/**
	 * @brief Sets destid for an existing entry specified by tx_eng
	 * 	  Typically called after HELLO exchange.
	 *
	 * @param destid	Destination ID to be added to the existing
	 * 			daemon element
	 *
	 * @param tx_eng	Pointer to tx engine specifying the daemon
	 * 			element
	 */
	int set_destid(uint32_t destid, tx_engine<C, cm_msg_t> *tx_eng)
	{
		int rc;

		lock_guard<mutex> daemons_lock(daemons_mutex);
		auto it = find_if(begin(daemons), end(daemons),
			[tx_eng](daemon_element_t& daemon_element)
				{ return daemon_element->m_tx_eng.get() == tx_eng;});
		if (it != end(daemons)) {
			(*it)->destid = destid;
			(*it)->provisioned = true;
			rc = 0;
		} else {
			ERR("Failed to find tx_eng in daemons list\n");
			rc = -1;
		}
		return rc;
	} /* set_destid() */

	int set_provisioned(uint32_t destid, tx_engine<C, cm_msg_t> *tx_eng)
	{
		int rc;
		lock_guard<mutex> daemons_lock(daemons_mutex);
		auto it = find_if(begin(daemons), end(daemons),
			[tx_eng, destid](daemon_element_t& daemon_element)
			{ return (daemon_element->m_tx_eng.get() == tx_eng) &&
				  daemon_element->destid == destid;});
		if (it != end(daemons)) {
			(*it)->provisioned = true;
			rc = 0;
		} else {
			ERR("Failed to find tx_eng/destid in daemons list\n");
			rc = -1;
		}
		return rc;
	} /* set_provisioned() */

	int set_rx_eng(unique_ptr<rx_engine<C, cm_msg_t>> rx_eng, tx_engine<C, cm_msg_t> *tx_eng)
	{
		int rc;

		lock_guard<mutex> daemons_lock(daemons_mutex);
		auto it = find_if(begin(daemons), end(daemons),
			[tx_eng](daemon_element_t& daemon_element)
				{ return daemon_element->m_tx_eng.get() == tx_eng;});
		if (it != end(daemons)) {
			(*it)->m_rx_eng = move(rx_eng);
			rc = 0;
		} else {
			ERR("Failed to find tx_eng in daemons list\n");
			rc = -1;
		}
		return rc;
	} /* set_destid() */
	/**
	 * @brief Remove daemon entry specified by destid
	 *
	 * @param destid  Destination ID for remote daemon
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int remove_daemon(uint32_t destid)
	{
		int rc;

		lock_guard<mutex> daemons_lock(daemons_mutex);
		auto it = find_if(begin(daemons), end(daemons),
			[destid](daemon_element_t& daemon_element)
			{ return daemon_element->destid == destid;});

		if (it != end(daemons)) {
			daemons.erase(it);
			INFO("Daemon entry for destid(0x%X) removed\n", destid);
			rc = 0;
		} else {
			ERR("Cannot find entry with destid(0x%X)\n", destid);
			rc = -1;
		}

		return rc;
	} /* remove_daemon() */

private:
	using daemon_element_t  = unique_ptr<daemon_info<C>>;
	using daemon_list_t   = vector<daemon_element_t>;

	daemon_list_t	daemons;
	mutex		daemons_mutex;
};

#endif
