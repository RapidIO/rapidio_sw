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

#include <string>
#include <list>
#include <vector>

#include "rdma_types.h"


using std::list;
using std::vector;
using std::string;

class loc_ms;
class loc_msub;
using conn_h_list = vector<conn_h>;
using msp_list	  = list<loc_ms *>;
using msubp_list = list<loc_msub*>;

/**
 * @brief Memory space owner
 */
struct loc_mso {

	loc_mso(const char *name, uint32_t msoid, bool owned) :
		name(name), msoid(msoid), owned(owned)
	{}

	string name;
	uint32_t msoid;
	bool owned;
};

/**
 * @brief Stores a local memory space owner's name and ID in database.
 *
 * @param mso_name Memory space owner name
 *
 * @param msoid	Memory space owner identifier
 *
 * @param owned	true if app created mso, false if it just opened it
 *
 * @return 	Handle to memory space owner, could be 0 (NULL) if failed
 */
mso_h add_loc_mso(const char* mso_name,
		  uint32_t msoid,
		  bool owned);

/**
 * @brief Removes the specified local memory space owner from the database.
 *
 * @param msoh	Memory space owner's handle
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_loc_mso(mso_h msoh);

/**
 * @brief Removes the specified local memory space owner from the database.
 *
 * @param msoid	Memory space owner's identifier
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_loc_mso(uint32_t msoid);

/**
 * @brief Removes all memory space owners from database
 */
void purge_loc_mso_list(void);

/**
 * @brief Determines if memory space owner is present in the database
 *
 * @param msoh	Memory space owner handle
 *
 * @return true if the msoh is in the database, false otherwise.
 */
bool mso_h_exists(mso_h msoh);

/**
 * @brief Finds a memory space owner by its msoid
 *
 * @param msoid	Memory space owner identifier
 *
 * @return memory space owner handle, 0 if not found
 */
mso_h find_mso(uint32_t msoid);

/**
 * @brief Finds a memory space owner by its name
 *
 * @param msoid	Memory space owner name
 *
 * @return memory space owner handle, 0 if not found
 */
mso_h find_mso_by_name(const char *name);

/**
 * @brief Memory space
 */
struct loc_ms {

	loc_ms(const char *name, uint32_t bytes, mso_h msoh, uint32_t msid,
		uint64_t phys_addr, uint64_t rio_addr, bool owned) :
	name(name), bytes(bytes), msoh(msoh), msid(msid), phys_addr(phys_addr),
	rio_addr(rio_addr), owned(owned)
	{
	}

	string name;
	uint32_t   bytes;
	uint64_t   msoh;
	uint32_t   msid;
	uint64_t   phys_addr;	/* phys_addr and rio_addr maybe the same */
	uint64_t   rio_addr;	/* if direct mapping is used. */
	bool	   owned;
};

/**
 * @brief Stores a local memory space's info in the database
 *
 * @param ms_name	Memory space name
 *
 * @param bytes		Size of memory space in bytes
 *
 * @param msoh		Memory space owner handle
 *
 * @param msid		Memory space identifier
 *
 * @param ms_conn_id	Memory space connection ID (for opened ones)
 *
 * @param owned		true if creator, false if just opened ms
 *
 * @return pointer to stored struct, NULL on failure
 */
ms_h add_loc_ms(const char *ms_name,
		uint64_t bytes,
		mso_h msoh,
		uint32_t msid,
		uint64_t phys_addr,
		uint64_t rio_addr,
		bool owned);

/**
 * @brief Removes the specified local memory space from the database.
 *
 * @param msh	Memory space handle
 *
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_loc_ms(ms_h msh);

/**
 * @brief Removes all memory spaces from database
 */
void purge_loc_ms_list(void);

/**
 * @brief Searches the local memory space database for a memory space
 * 	  having the specified memory space identifier
 *
 * @param msid Identifier of the memory space to be located in database
 *
 * @return Handle to memory space which could be NULL (0) if not found
 */
ms_h find_loc_ms(uint32_t msid);

/**
 * @brief Searches the local memory space database for a memory space
 * 	  having the specified memory space name
 *
 * @param ms_name Name of the memory space to be located in database
 *
 * @return Handle to memory space which could be NULL (0) if not found
 */
ms_h find_loc_ms_by_name(const char *ms_name);

/**
 * @brief Returns number of memory spaces owned by specified memory space owner
 *
 * @param msoh	Memory space owner handle
 *
 * @return Number of memory space owners owned by msoh
 */
unsigned get_num_ms_by_msoh(mso_h msoh);

/**
 * @brief Returns a list of memory space handles owned by specified memory space owner
 *
 * @param msoh	Memory space owner handle
 *
 * @param msh	Pointer to pre-allocated space for holding the list
 */
void get_list_msh_by_msoh(mso_h msoh, msp_list& ms_list);

/**
 * @brief Determines whether the specified memory space exists
 *
 * @param msh	Memory space handle
 *
 * @return true if the msh is in the database, false otherwise.
 */
bool loc_ms_exists(ms_h msh);

/**
 * @brief Dumps pointers of all memory spaces in local database
 */
void dump_loc_ms(void);

struct rem_ms {
	rem_ms(const char *name, uint32_t msid) :
		name(name), msid(msid)
	{}
	string 	 name;
	uint32_t msid;
};

/**
 * @brief Stores a remote memory space's name and identifier in database
 *
 * @param name	Memory space name
 *
 * @param msid	Memory space identifier
 *
 * @return pointer to stored struct, NULL on failure
 */
ms_h add_rem_ms(const char *name, uint32_t msid);

/**
 * @brief Removes the specified remote memory space from the database
 *
 * @param msid	Memory space handle
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_rem_ms(ms_h msh);

/**
 * @brief Removes the specified remote memory space from the database
 *
 * @param msid	Identifier of the memory space to be removed
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_rem_ms(uint32_t msid);

/**
 * @brief Searches the remote memory space database for a memory space
 * 	  having the specified memory space identifier
 *
 * @param msid	Identifier of the memory space to be located in database
 *
 * @return	Handle to memory space which could be NULL (0) if not found
 */
ms_h find_rem_ms(uint32_t msid);

/**
 * @brief Determines if a a remote memory space exists in the database
 *
 * @param msh	Memory space handle
 *
 * @return true if remote ms denoted by 'msh' is found in the rem_ms_list
 * otherwise returns false
 */
bool rem_ms_exists(ms_h msh);

struct client_connection {
	client_connection(uint64_t connh, uint64_t client_to_lib_tx_eng_h)
		: connh(connh), client_to_lib_tx_eng_h(client_to_lib_tx_eng_h)
	{}
	uint64_t connh;
	uint64_t client_to_lib_tx_eng_h;
};

using connection_list = vector<client_connection>;

/**
 * Memory subspaces.
 */
struct loc_msub
{
	loc_msub(uint32_t msid, uint32_t msubid, uint32_t bytes, uint8_t rio_addr_len,
		 uint64_t rio_addr_lo, uint8_t	rio_addr_hi, uint64_t paddr) :
	msid(msid), msubid(msubid), bytes(bytes), rio_addr_len(rio_addr_len),
		rio_addr_lo(rio_addr_lo), rio_addr_hi(rio_addr_hi), paddr(paddr)
	{
	}

	uint32_t	msid;
	uint32_t	msubid;
	uint32_t	bytes;
	uint8_t		rio_addr_len;
	uint64_t	rio_addr_lo;
	uint8_t		rio_addr_hi;
	uint64_t	paddr;
	connection_list	connections;
};

/**
 * @brief Creates a local memory sub-space from its components, and adds it
 * 	  to the database
 *
 * @param msubid Memory sub-space identifier
 *
 * @param msid	Memory space identifier
 *
 * @param bytes	Length of subspace, in bytes
 *
 * @param rio_addr_len Rapid IO address length
 *
 * @param rio_addr_lo	Rapid IO address lower 64-bits
 *
 * @param rio_addr_hi	Rapid IO address bits 66 & 65 (if 66-bit address)
 *
 * @param paddr	Physical address of mem sub-space buffer
 *
 * @return Handle to memory sub-space, could be 0 (NULL) if failed
 */
ms_h add_loc_msub(uint32_t msubid,
		  uint32_t msid,
		  uint32_t bytes,
		  uint8_t  rio_addr_len,
		  uint64_t rio_addr_lo,
		  uint8_t  rio_addr_hi,
		  uint64_t paddr);

/**
 * @brief Removes the specified local memory sub-space from the database
 *
 * @param msub_h	Handle to memory sub-space
 *
 * @return	> 0 if successful < 0 if unsuccessful
 */
int remove_loc_msub(msub_h msubh);

/**
 * @brief Purge local msub list
 */
void purge_loc_msub_list(void);

/**
 * @brief Searches the local memory sub-space database for a memory sub-space
 *	  having the specified sub-space ID
 *
 * @param msubid  ID of the memory sub-space to be located in database
 *
 * @return	Handle to memory sub-space, could be 0 (NULL) if not found
 */
msub_h find_loc_msub(uint32_t msubid);

/**
 * @brief Searches the local memory sub-space database for a memory sub-space
 *	  having the specified connection handle
 *
 * @param connh  Handle representing a connection between the local msub
 * 		 and a remote client
 *
 * @return	Handle to memory sub-space, could be 0 (NULL) if not found
 */
msub_h find_loc_msub_by_connh(conn_h connh);

/**
 * @brief Returns number of memory subspaces allocated within the
 * 	  specified memory space
 *
 * @param msid	Memory space identifier
 *
 * @return	Number of memory subspaces within msid
 */
unsigned get_num_loc_msub_in_ms(uint32_t msid);

/**
 * @brief Returns list of local memory subspace handles owned by specified
 * 	  memory space
 *
 * @param msh	Memory space handle
 *
 * @param msubh	Pointer to pre-allocated space for holding the list
 */
void get_list_loc_msub_in_msid(uint32_t msid, msubp_list& msub_list);

class rem_msub
{
public:
	rem_msub(uint32_t msubid, uint32_t msid, uint32_t bytes,
	uint8_t rio_addr_len, uint64_t	rio_addr_lo, uint8_t rio_addr_hi,
	uint8_t destid_len, uint32_t destid, ms_h loc_msh) :
		msubid(msubid), msid(msid), bytes(bytes),
		rio_addr_len(rio_addr_len), rio_addr_lo(rio_addr_lo),
		rio_addr_hi(rio_addr_hi), destid_len(destid_len),
		destid(destid), loc_msh(loc_msh)
	{}
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

/**
 * @brief Creates a remote memory subspace, and adds it to the database
 *
 * @param rem_msubid	Remote memory sub-space ID
 *
 * @param rem_msid	Identifier of remote memory space containing the subspace
 *
 * @param rem_bytes	Length of subspace, in bytes
 *
 * @param rem_rio_addr_len Rapid IO address length
 *
 * @param rem_rio_addr_lo  Rapid IO address lowest 64-bits
 *
 * @param rem_rio_addr_hi  Rapid IO address upper 2 bits (if 66-bit RIO address)
 *
 * @param destid_len	Destination ID length (e.g. 8-bit, 16-bit, or 32-bit)
 *
 * @param destid	Destination ID of node providing the mem sub-space
 *
 * @param loc_msh	Handle to local memory space handle to which the client
 * 			has connected when it provided the rem_msubid
 *
 * @return 	Memory subspace handle. Could be 0 (NULL) if failed.
 */
msub_h add_rem_msub(uint32_t	rem_msubid,
		    uint32_t	rem_msid,
		    uint32_t	rem_bytes,
		    uint8_t	rem_rio_addr_len,
		    uint64_t	rem_rio_addr_lo,
		    uint8_t	rem_rio_addr_hi,
		    uint8_t	destid_len,
		    uint32_t	destid,
		    ms_h	loc_msh);

/**
 * @brief Removes the specified remote memory sub-space from the database
 *
 * @param msubid Identifier of the memory sub-space to be removed
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_rem_msub(uint32_t msubid);

/**
 * @brief Removes the specified remote memory sub-space from the database
 *
 * @param 	msubh	Handle to memory sub-space to be removed
 *
 * @return	0 if successful < 0 if unsuccessful
 */
int remove_rem_msub(msub_h msub);

/**
 * @brief Searches the remote memory sub-space database for a memory sub-space
 *	  having the specified handle.
 *
 * @param msubid  Identifier of the memory sub-space to be located in database
 *
 * @return	Handle to memory sub-space or 0 (NULL) if not found
 */
msub_h find_rem_msub(uint32_t msubid);

/**
 * @brief Returns number of remote memory subspaces allocated in database
 *
 * @return	Number of memory subspaces within database
 */
unsigned get_num_rem_msubs(void);

/**
 * @brief Removes all remote msubs belonging to specified msid
 *
 * @param msh	Memory space handle for which remote msubs are to be removed.
 */
void remove_rem_msubs_in_ms(uint32_t msid);

/**
 * @brief Finds any remote msub belonging to specified msid
 *
 * @param msid	Memory space identifier for which a remote msubs is to be found
 *
 * @return 0 if successful, < 0 if no msubs found for specified msh
 */
msub_h find_any_rem_msub_in_ms(uint32_t msid);

/**
 * @brief Removes the remote msub which is associated with loc_msh from database
 *
 * @param loc_msh	Handle to local memory space
 */
void remove_rem_msub_by_loc_msh(ms_h loc_msh);

/**
 * @brief Purge local database tables
 */
void purge_local_database(void);

#endif
