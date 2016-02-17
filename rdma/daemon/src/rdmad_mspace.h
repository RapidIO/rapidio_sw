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

#include <string>
#include <vector>
#include <set>
#include <exception>
#include <mutex>

#include "tx_engine.h"

using std::set;
using std::string;
using std::vector;
using std::exception;
using std::mutex;

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
class unix_msg_t;
class cm_server;
class msubspace;

/**
 * @brief Exception thrown by mspace on error during construction
 */
class mspace_exception : public exception {
public:
	mspace_exception(const char *msg) : err(msg) {}
	const char *what() { return err; }
private:
	const char *err;
};

class mspace
{
	class ms_user
	{
		friend class mspace;
	public:
		/**
		 *  @brief Construct ms_user from specified parameters
		 */
		ms_user(tx_engine<unix_server, unix_msg_t> *tx_eng) :
			tx_eng(tx_eng),
			accepting(false),
			connected_to(false),
			server_msubid(0),
			client_destid(0xFFFF),
			client_msubid(NULL_MSUBID),
			client_to_lib_tx_eng_h(0)
		{
		} /* Constructor */

		/**
		 * @brief Copy constructor
		 */
		ms_user(const ms_user&) = default;

		/**
		 * @brief Assignment operator
		 */
		ms_user& operator=(const ms_user&) = default;

		/**
		 * @brief Tells whether this ms is connected to specified client destid
		 *
		 * @param client_destid Destination ID of client daemon
		 */
		bool operator==(const uint32_t client_destid)
		{
			return this->client_destid == client_destid;
		} /* operator == */

		/**
		 * @brief Tells whether the daemon is connected to this user
		 * 	  via the specified tx engine
		 *
		 * @param Tx engine between daemon and user app/library
		 */
		bool operator==(tx_engine<unix_server, unix_msg_t> *tx_eng)
		{
			return tx_eng == this->tx_eng;
		} /* operator == */

	private:
		tx_engine<unix_server, unix_msg_t> *tx_eng;
		bool	accepting;		 /* Set when 'accept' is called */
		bool	connected_to;		 /* Set when connected */
		uint32_t server_msubid;		 /* Assigned upon accepting */
		uint16_t client_destid;		 /* Assigned When connected */
		uint32_t client_msubid;		 /* Assigned When connected */
		uint64_t client_to_lib_tx_eng_h; /* Assigned When connected */
	}; /* ms_user */

public:
	/* Constructor */
	mspace(const char *name, uint32_t msid, uint64_t rio_addr,
					uint64_t phys_addr, uint64_t size);
	/* Destructor */
	~mspace();

	/**
	 * @brief Destroys the memory space parameters and turns it into
	 * 	  a free memory space. Merges with continguous memory spaces
	 * 	  if possible.
	 */
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

	/**
	 * @brief Upon having a remote connection from a client app to this
	 * 	  memory space, store information about that client.
	 *
	 * @param client_destid	DestID of node on which client app resides
	 *
	 * @param client_msubid Memory subspace identifier provided by client
	 * 			in rdma_conn_ms_h(). May be NULL_MSUB.
	 *
	 * @param client_to_lib_tx_eng_h Handle of the client-daemon to
	 *  				 client-app Tx engine
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int  add_rem_connection(uint16_t client_destid,
				uint32_t client_msubid,
				uint64_t client_to_lib_tx_eng_h);

	/**
	 * @brief When this memory space is disconnected from a remote client
	 * 	  app, remote relevant information.
	 *
	 * @param client_destid	DestID of node on which client app resides
	 *
	 * @param client_msubid Memory subspace identifier provided by client
	 * 			in rdma_conn_ms_h(). May be NULL_MSUB.
	 *
	 * @param client_to_lib_tx_eng_h Handle of the client-daemon to
	 *  				 client-app Tx engine
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int remove_rem_connection(uint16_t client_destid,
				  uint32_t client_msubid,
				  uint64_t client_to_lib_tx_eng_h);

	/* Debugging */
	/**
	 * @brief Dumps infor about memory space such as name, size, free
	 * 	  status, and so on.
	 *
	 * @param env CLI console environment object
	 */
	void dump_info(struct cli_env *env);

	/**
	 * @brief Dumps information about the memory subspaces contained
	 * 	  within this memory space.
	 *
	 * @param env CLI console environment object
	 */
	void dump_info_msubs_only(struct cli_env *env);


	/**
	 * @brief Dumps infor about memory space such as name, size, free
	 * 	  status..etc. as well as information about the memory
	 * 	  subspaces contained within this memory space.
	 *
	 * @param env CLI console environment object
	 */
	void dump_info_with_msubs(struct cli_env *env);

	/**
	 * @brief Equality operator for matching the memory space by
	 * 	  a memory space identifier (msid)
	 *
	 * @param msid	Memory space identifier to compare with
	 *
	 * @return true if matching, false if not
	 */
	bool operator==(uint32_t msid) { return this->msid == msid; }

	/**
	 * @brief Equality operator for matching the memory space by
	 * 	  a memory space name.
	 *
	 * @param name	Memory space name to compare with
	 *
	 * @return true if matching, false if not
	 */
	bool operator==(const char *name) { return this->name == name; }

	/**
	 * @brief Opens memory space and associates it with a tx_eng
	 *
	 * @param user_tx_eng	Tx engine used by the daemon to communicate
	 * 			with the user app which opened the memory space
	 *
	 */
	int open(tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	/**
	 * @brief Returns the tx_eng either of the owner of the ms, or of
	 * 	  one of its users, whoever happens to be in the 'accepting'
	 * 	  state (and there can only be one at a single time).
	 *
	 * @return Tx engine used by the daemon to communicate with the owner
	 * 	   or the user, which happens to be in 'accepting' state.
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	tx_engine<unix_server, unix_msg_t> *get_accepting_tx_eng();

	/**
	 * @brief Place this ms in accepting state by either its owner
	 * 	  or by one of its user apps.
	 *
	 * @param app_tx_eng Tx engine used by the daemon to communicate with
	 * 		     either the owner or one of its users, whoever
	 * 		     called rdma_accept_ms_h()
	 *
	 * @param server_msubid A memory subspace identifier specifying
	 * 			the memory subspace provided by the app
	 * 			that called rdma_accept_ms_h()
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng,
			uint32_t server_msubid);

	/**
	 * @brief Clears previous accept request, most likely due
	 * 	  to a failure detected at the RDMA library side.
	 *
	 * @param  app_tx_eng Tx engine used by the daemon to communicate with
	 * 		     either the owner or one of its users, whoever
	 * 		     tried to undo the previous 'accept' request
	 */
	int undo_accept(tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	/**
	 * @brief Determines whether mspace was created by app connected
	 * 	  to the daemon via specified Tx engine
	 *
	 * @param app_tx_eng Tx engine
	 *
	 * @return true if matching, false if not
	 */
	bool created_using_tx_eng(
			tx_engine<unix_server, unix_msg_t> *app_tx_eng)
	{
		return this->creator_tx_eng == app_tx_eng;
	}

	/**
	 * @brief Determines whether mspace was opened by app connected
	 * 	  to the daemon via specified Tx engine
	 *
	 * @param app_tx_eng Tx engine
	 *
	 * @return true if matching, false if not
	 */
	bool has_user_with_user_tx_eng(
			tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	/**
	 * @brief Determines whether mspace has a remote connection with
	 * 	  a client on a node having the specified destid
	 *
	 * @param destid Destination ID of remote node to check
	 *
	 * @return true if matching, false if not
	 */
	bool connected_by_destid(uint16_t destid);

	/**
	 * @brief Close memory space by user app specified by app_tx_eng
	 *
	 * @param app_tx_eng  Tx engine used by daemon to connect to app
	 * 		      requesting memory space closure
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int close(tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	/**
	 * @brief Create a memory subspace a specified offset with requested
	 * 	  size
	 *
	 * @param offset Subspace offset within memory space
	 *
	 * @param req_size Requested subspace size
	 *
	 * @param size	  Actual allocated size for the subspace
	 *
	 * @param msubid  Memory subspace identifier assigned to subspace
	 *
	 * @param rio_addr RapidIO address of the first byte of the subspace
	 *
	 * @param phys_addr Physical address of the first byte of the subspace
	 * 		    (same as RapidIO address if directly mapped)
	 * @param tx_eng  Tx engine used by daemon to communicate with either
	 * 		  the memory space creator or whichever user that
	 * 		  has created the memory subspace
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int create_msubspace(uint32_t offset,
			     uint32_t req_size,
			     uint32_t *size,
			     uint32_t *msubid,
			     uint64_t *rio_addr,
			     uint64_t *phys_addr,
			     const tx_engine<unix_server, unix_msg_t> *tx_eng);

	/**
	 * @brief Destroys subspace denoted by msubid
	 *
	 * @param Memory subspace identifier
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int destroy_msubspace(uint32_t msubid);

	/**
	 * @brief Client-initiated disconnection from memory space
	 *
	 * @param client_msubid Client memory subspace identifier.
	 * 			Can be NULL_MSUB.
	 *
	 * @param client_to_lib_tx_eng_h Handle of Tx engine between
	 * 				 client daemon and client app
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int client_disconnect(uint32_t client_msubid,
			      uint64_t client_to_lib_tx_eng_h);

	/**
	 * @brief Server-initiated disconnection from memory space
	 *
	 * @param client_msubid Client memory subspace identifier.
	 * 			Can be NULL_MSUB.
	 *
	 * @param client_to_lib_tx_eng_h Handle of Tx engine between
	 * 				 client daemon and client app
	 *
	 * @return 0 if successful, non-zero otherwise
	 */
	int server_disconnect(uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h);

	/**
	 * @brief Disconnect memory space from specified destid
	 * 	  Used when a remote node dies and we need to tell
	 * 	  apps that had connections with that remote node
	 * 	  to clear their databases of corresponding connections.
	 *
	 * @param Remote (client) node destination ID.
	 */
	void disconnect_from_destid(uint16_t client_destid);

private:
	/**
	 *@brief Copy constructor unimplemented
	 */
	mspace(const mspace&) = delete;

	/**
	 *@brief Assignment operator unimplemented
	 */
	mspace& operator=(const mspace&) = delete;

	/**
	 * @brief Return a set of remote destination IDs that have active
	 * 	  connections with this mspace. Used for displaying info about
	 * 	  this memory space from the CLI commands.
	 */
	set<uint16_t> get_rem_destids();

	/**
	 * @brief Sends CM_FORCE_DISCONNECT_MS to the client of an ms.
	 *
	 * @param server  CM socket server between server and client daemons
	 * @param server_msubid  ID of msub provided by server during 'accept'
	 * @param client_to_lib_tx_eng Tx engine handle between the client
	 *                             daemon and the client app
	 */
	int send_cm_force_disconnect_ms(cm_server *server,
					uint32_t server_msubid,
					uint64_t client_to_lib_tx_eng_h);

	/**
	 * @brief Disconnect this memory space from a particular client
	 *
	 * @param is_client true if called by client, false if by server
	 * @param client_msubid the msubid provided by client during connect
	 * 			request, maybe NULL_MSUBID.
	 * @param client_to_lib_tx_eng_h Tx engine handle between the client
	 * 				 daemon and the client app
	 */
	int disconnect(bool is_client, uint32_t client_msubid,
		       uint64_t client_to_lib_tx_eng_h);

	/**
	 * @brief Looks up a connection to the ms by client parameters
	 * then calls send_cm_force_disconnect_ms() with the correct
	 * server parameter to send CM_FORCE_DISCONNECT_MS
	 *
	 * @param client_msubid the msubid provided by client during connect
	 * 			request, maybe NULL_MSUBID.
	 * @param client_to_lib_tx_eng_h Tx engine handle between the client
	 * 				 daemon and the client app
	 */
	int send_disconnect_to_remote_daemon(uint32_t client_msubid,
					     uint64_t client_to_lib_tx_eng_h);

	/**
	 * @brief Calls send_cm_force_disconnect_ms for ALL connections
	 * to this ms. Called from destroy()
	 */
	int notify_remote_clients();

	/**
	 * @brief Closes connections with applications that have called
	 * rdma_open_ms_h(). Called from destroy().
	 */
	int close_connections();

	/**
	 * @brief Sends message to server library telling it that
	 * a specific connection to the ms has dropped and the library
	 * needs to clean the database. Typically called from
	 * disconnect_from_destid() which is called when a remote daemon dies.
	 *
	 * @param client_msubid	Client msubid. Could be NULL_MSUB
	 * @param server_msubid Server msubid provided during accept
	 * @param client_to_lib_tx_eng_h Client daemon-to-library tx engine
	 * @param tx_eng Server daemon to library tx engine used to relay
	 * 		 the disconnection message.
	 */
	void send_disconnect_to_lib(uint32_t client_msubid,
				    uint32_t server_msubid,
				    uint64_t client_to_lib_tx_eng_h,
				    tx_engine<unix_server, unix_msg_t> *tx_eng);

	using ms_user_list   = vector<ms_user>;
	using msubspace_list = vector<msubspace>;

	string		name;
	uint32_t	msid;
	uint64_t	rio_addr;
	uint64_t	phys_addr;
	uint32_t	size;
	uint32_t	msoid;
	bool		free;

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
	ms_user_list	users;
	mutex		users_mutex;

	/* Memory sub-space indexes */
	bool msubindex_free_list[MSUBINDEX_MAX+1];	/* List of memory sub-space IDs */
	mutex 		msubindex_free_list_mutex;

	/* Memory subspaces */
	msubspace_list	msubspaces;
	mutex		msubspaces_mutex;
}; /* mspace */


#endif


