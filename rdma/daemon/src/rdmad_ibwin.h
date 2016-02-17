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

#include <pthread.h>

#include <vector>
#include <exception>

#include "libcli.h"

#include "tx_engine.h"

using std::vector;
using std::exception;

using mspace_list = vector<mspace *>;
using mspace_iterator = mspace_list::iterator;

class mspace;

/**
 * @brief Inbound window mapping exception
 */
class ibwin_map_exception : public exception {
public:
	ibwin_map_exception(const char *msg) : err(msg)
	{
	}

	const char *what() { return err; }
private:
	const char *err;
};

class ibwin 
{
public:
	/**
	 * @brief Construct an inbound window object
	 *
	 * @param mport_hnd	Master port handle
	 *
	 * @param win_num	Window number
	 *
	 * @param size		Size in bytes
	 *
	 * @throws ibwin_map_exception if failed during riomp_dma_ibwin_map()
	 * @throws integer exception if failed during mutex initialization
	 */
	ibwin(riomp_mport_t mport_hnd, unsigned win_num, uint64_t size);

	/**
	 * @brief	Copy constructor
	 */
	ibwin(const ibwin&) = default;

	/**
	 * @brief	Assignment operator deleted.
	 */
	ibwin& operator=(const ibwin&) = delete;

	/**
	 * @brief Free up memory spaces, if any, in the inbound window,
	 *  	  and then free up the window itself
	 */
	void free();

	/* Accessors */
	mspace_list& get_mspaces() { return mspaces; };

	unsigned get_win_num() const { return win_num; };

	/**
	 * @brief Dump inbound window information to the CLI console
	 *
	 * @param CLI console object
	 */
	void dump_info(struct cli_env *env);

	/**
	 * @brief Print a header for memory spaces that maybe in the window
	 *
	 * @param CLI console object
	 */
	void print_mspace_header(struct cli_env *env);

	/**
	 * @brief Dump info for memory spaces in this window to the CLI console
	 *
	 * @param CLI console object
	 */
	void dump_mspace_info(struct cli_env *env);

	/**
	 * @brief Dump info for memory spaces, and subspaces in this window
	 * 	  to the CLI console
	 *
	 * @param CLI console object
	 */
	void dump_mspace_and_subs_info(cli_env *env);

	/**
	 * @brief Populate a list of memory spaces "large enough" to hold 'size'
	 *
	 * @param size	Requested size of memory spaces to obtain a list of,
	 * 		in bytes
	 *
	 * @param le_mspaces	An empty list, to be populated with memory
	 * 			spaces large enough to hold 'size' bytes
	 *
	 */
	int get_free_mspaces_large_enough(
				uint64_t size, mspace_list& le_mspaces);

	/**
	 * @brief Returns whether this inbound window has room to be used to
	 * 	  create a memory space of the specified size
	 *
	 * @param size	Size in bytes to test for
	 *
	 * @return 0 if there is at least 1 space large enough to hold size,
	 * 	   non-zero otherwise
	 */
	bool has_room_for_ms(uint64_t size);

	/**
	 * @brief  Create a memory space with specified name, size and owner
	 *
	 * @param name	Desired memory space name
	 *
	 * @param size	Desired memory space size in bytes
	 *
	 * @param msoid	Memory space owner identifier
	 *
	 * @param creator_tx_eng Tx engine from daemon to mspace creator
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int create_mspace(const char *name,
			  uint64_t size,
			  uint32_t msoid,
			  mspace **ms,
			  tx_engine<unix_server, unix_msg_t> *creator_tx_eng);

	/**
	 * @brief  Destroy memory space specified by msoid and msid
	 *
	 * @param msoid	Memory space owner identifier
	 *
	 * @param msid	Memory space identifier
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int destroy_mspace(uint32_t msoid, uint32_t msid);

	/**
	 * @brief Return memory space with specified name
	 *
	 * @param name Memory space name
	 *
	 * @return pointer to memory space if successful, nullptr if not
	 */
	mspace* get_mspace(const char *name);

	/**
	 * @brief Return memory space with specified memory space identifier
	 *
	 * @param msid Memory space identifier
	 *
	 * @return pointer to memory space if successful, nullptr if not
	 */
	mspace* get_mspace(uint32_t msid);

	/**
	 * @brief Return memory space with specified memory space, and
	 * 	  memory space owner identifiers
	 *
	 * @param msoid Memory space owner identifier
	 *
	 * @param msid  Memory space identifier
	 *
	 * @return pointer to memory space if successful, nullptr if not
	 */
	mspace* get_mspace(uint32_t msoid, uint32_t msid);

	/**
	 * @brief Provided a list of memory spaces that have remote connections
	 * 	  from clients on a node having the specified destid
	 *
	 * @param destid Destination ID of remote node
	 *
	 * @param mspaces Empty list of memory spaces that gets populated
	 * 		  with matching memory spaces
	 */
	void get_mspaces_connected_by_destid(uint32_t destid,
					     	     mspace_list& mspaces);

	/**
	 * @brief CLose all memory spaces opened by the specified Tx
	 * 	  engine. Used when the engine detects that the application
	 * 	  has died, so the daemon needs to clean up the open
	 * 	  connection by that application.
	 *
	 * @param app_tx_eng Tx engine between the daemon and a server app
	 */
	void close_mspaces_using_tx_eng(
			tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	/**
	 * @brief Destroy all memory spaces created by the specified Tx
	 * 	  engine. Used when the engine detects that the application
	 * 	  has died, so the daemon needs to destroy the memory spaces
	 * 	  created by that application.
	 *
	 * @param app_tx_eng Tx engine between the daemon and a server app
	 */
	void destroy_mspaces_using_tx_eng(
			tx_engine<unix_server, unix_msg_t> *app_tx_eng);

private:
	/**
	 * @brief	Merges two free memory spaces together to form
	 * 		a larger memory space
	 *
	 * @param current First memory space
	 *
	 * @param other	  Second memory space
	 */
	void merge_other_with_mspace(mspace_iterator current,
				     mspace_iterator other);

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


