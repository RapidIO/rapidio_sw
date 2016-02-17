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

#ifndef INBOUND_H
#define INBOUND_H

#include <stdint.h>

#include <exception>
#include <vector>
#include <mutex>

#include "libcli.h"

#include "tx_engine.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"

using std::vector;
using std::mutex;

class inbound_exception : public exception {
public:
	inbound_exception(const char *msg) : err(msg) {}
	~inbound_exception() throw() {}
	const char *what() throw() { return err; }
private:
	const char *err;
};

class unix_msg_t;
class unix_server;

class inbound
{
public:
	/**
	 * @brief Constructor
	 *
	 * @param mport_hand	Master port handle
	 *
	 * @param num_wins	Number of inbound windows to create
	 *
	 * @param win_size	Size of each inbound window, in bytes
	 */
	inbound(riomp_mport_t mport_hnd, unsigned num_wins, uint32_t win_size);

	/**
	 * @brief Destructor
	 */
	~inbound();

	/**
	 * @brief Copy constructor - unimplemented
	 */
	inbound(const inbound&) = delete;

	/**
	 * @brief Assignment operator - unimplemented
	 */
	inbound& operator=(const inbound&) = delete;

	/**
	 * @brief Dump inbound info to CLI console
	 *
	 * @param env	CLI console environment object
	 */
	void dump_info(struct cli_env *env);

	/**
	 * Accessors
	 */
	unsigned get_num_ibwins() { return ibwins.size(); }
	uint64_t get_ibwin_size() { return ibwin_size; }

	/**
	 * @brief Searches for and returns a memory space given its name
	 *
	 * @param name	Memory space name
	 *
	 * @return pointer to memory space, nullptr if not found
	 */
	mspace  *get_mspace(const char *name);

	/**
	 * @brief Searches for and returns a memory space given its msid
	 *
	 * @param msid	Memory space identifier
	 *
	 * @return pointer to memory space, nullptr if not found
	 */
	mspace *get_mspace(uint32_t msid);

	/**
	 * @brief Searches for and returns a memory space given its msid & msoid
	 *
	 * @param msoid Memory space owner identifier
	 *
	 * @param msid	Memory space identifier
	 *
	 * @return pointer to memory space, nullptr if not found
	 */
	mspace *get_mspace(uint32_t msoid, uint32_t msid);

	/**
	 * @brief Searches for and returns a list of memory spaces that have
	 * 	  remote connections from client(s) on a node having
	 * 	  a specified destination ID
	 *
	 * @param destid Destination ID of remote node
	 *
	 * @param mspaces  Empty list for holding the resulting memory spaces
	 */
	void get_mspaces_connected_by_destid(
				uint32_t destid, mspace_list& mspaces);

	/**
	 * @brief Close and destroy memory spaces using a specific Tx engine
	 * 	  (used when connection from apps using that tx_eng drops)
	 *
	 * @param app_tx_eng Daemon to library Tx engine
	 */
	void close_and_destroy_mspaces_using_tx_eng(
			tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	/**
	 * @brief Dump memory space info for a memory space specified by name
	 * 	  to the CLI console
	 *
	 * @param env CLI console environment object
	 */
	void dump_mspace_info(struct cli_env *env, const char *name);

	/**
	 * @brief Dump memory space info for all  memory spaces
	 * 	  to the CLI console
	 *
	 * @param env CLI console environment object
	 */
	void dump_all_mspace_info(struct cli_env *env);

	/**
	 * @brief Dump memory space info for all memory spaces and their msubs
	 * 	  to the CLI console
	 *
	 * @param env CLI console environment object
	 */
	void dump_all_mspace_with_msubs_info(struct cli_env *env);

	/**
	 * @brief Create a memory space within a window that has enough space
	 *
	 * @param name	  Memory space name
	 *
	 * @param size	  Desired memory space name, in bytes
	 *
	 * @param msoid  Memory space owner identifier
	 *
	 * @param ms	  Pointer for holding the created memory space
	 *
	 * @param tx_eng Tx engine used by the daemon to communicate with
	 *		 the app that created the memory space
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int create_mspace(const char *name,
			  uint64_t size,
			  uint32_t msoid,
			  mspace **ms,
			  tx_engine<unix_server, unix_msg_t> *creator_tx_eng);

	/**
	 * @brief Open a memory space
	 *
	 * @param user_tx_eng Tx engine used by the daemon to communicate with
	 *		      the app that opened the memory space
	 *
	 * @param msid	  Memory space identifier
	 *
	 * @param phys_addr	Physical (PCIe) address of memory space
	 *
	 * @param rio_addr	RapidIO address of memory space (same as
	 *  			phys_addr if directly mapped)
	 *
	 * @param size  Memory space size, in bytes
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int open_mspace(const char *name,
			tx_engine<unix_server, unix_msg_t> *user_tx_eng,
			uint32_t *msid,
			uint64_t *phys_addr,
			uint64_t *rio_addr,
			uint32_t *size);

	/**
	 * @brief Destroy specified memory space
	 *
	 * @param msoid	Memory space owner identifier
	 *
	 * @param msid	Memory space identifier
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int destroy_mspace(uint32_t msoid, uint32_t msid);

	/**
	 * @brief Create a memory subspace in a specified memory space
	 *
	 * @param msid	Memory space identifier
	 *
	 * @param offset  Subspace offset within the memory space,
	 * 		  must be a multiple of page size (4K)
	 *
	 * @param size  Requested subspace size, must be a multiple of
	 * 		    page size (4K)
	 *
	 * @param msubid  Created memory subspace identifier
	 *
	 * @param rio_addr   Created memory subspace RapidIO address (same
	 * 		     as physical address if directly mapped)
	 *
	 * @param phys_addr  Created memory subspace physical (PCIe) address
	 *
	 * @param user_tx_eng  Tx engine used by daemon to communicate with
	 * 		       app that requested the subspace creation
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int create_msubspace(uint32_t msid,
			     uint32_t offset,
			     uint32_t size,
			     uint32_t *msubid,
			     uint64_t *rio_addr,
			     uint64_t *phys_addr,
			     const tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	/**
	 *  @brief Destroy a memory subspace given its msid and msubid
	 *
	 * @param msid	Memory space identifier
	 *
	 * @param msubid  Memory subspace identifier
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int destroy_msubspace(uint32_t msid, uint32_t msubid);

private:
	using ibwin_list = vector<ibwin>;

	uint32_t 	ibwin_size;
	ibwin_list	ibwins;
	mutex		ibwins_mutex;
	riomp_mport_t	mport_hnd;
}; /* inbound */

#endif
