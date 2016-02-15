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
#include <semaphore.h>

#include <iostream>
#include <exception>

#include "libcli.h"

#include "rdmad_unix_msg.h"
#include "tx_engine.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"

class inbound_exception : public exception {
public:
	inbound_exception(const char *msg) : err(msg) {}
	~inbound_exception() throw() {}
	const char *what() throw() { return err; }
private:
	const char *err;
};

class inbound
{
public:
	/* Constructor */
	inbound(riomp_mport_t mport_hnd, unsigned num_wins, uint32_t win_size);

	/* Destructor */
	~inbound();

	/* Dump inbound info */
	void dump_info(struct cli_env *env);

	unsigned get_num_ibwins() {
		return ibwins.size();
	}
	uint64_t get_ibwin_size() { return ibwin_size; }

	/* get_mspace by name */
	mspace  *get_mspace(const char *name);

	/* get_mspace by msid */
	mspace *get_mspace(uint32_t msid);

	/* get_mspace by msoid, msid */
	mspace *get_mspace(uint32_t msoid, uint32_t msid);

	mspace* get_mspace_open_by_server(
			tx_engine<unix_server, unix_msg_t> *user_tx_eng,
			uint32_t *ms_conn_id);

	int get_mspaces_connected_by_destid(
				uint32_t destid, vector<mspace *>& mspaces);

	void close_and_destroy_mspaces_using_tx_eng(
			tx_engine<unix_server, unix_msg_t> *app_tx_eng);

	/* Dump memory space info for a memory space specified by name */
	void dump_mspace_info(struct cli_env *env, const char *name);

	/* Dump memory space info for all memory spaces */
	void dump_all_mspace_info(struct cli_env *env);

	/* Dump memory space info for all memory spaces and their msubs */
	void dump_all_mspace_with_msubs_info(struct cli_env *env);

	/* Create a memory space within a window that has enough space */
	int create_mspace(const char *name,
			  uint64_t size,
			  uint32_t msoid,
			  uint32_t *msid,
			  mspace **ms,
			  tx_engine<unix_server, unix_msg_t> *creator_tx_eng);

	/* Open memory space */
	int open_mspace(const char *name,
			tx_engine<unix_server, unix_msg_t> *user_tx_eng,
			uint32_t *msid,
			uint64_t *phys_addr,
			uint64_t *rio_addr,
			uint32_t *bytes);

	/* Destroy memory space */
	int destroy_mspace(uint32_t msoid, uint32_t msid);

	/* Create a memory subspace */
	int create_msubspace(uint32_t msid,
			     uint32_t offset,
			     uint32_t req_bytes,
			     uint32_t *size,
			     uint32_t *msubid,
			     uint64_t *rio_addr,
			     uint64_t *phys_addr,
			     const tx_engine<unix_server, unix_msg_t> *user_tx_eng);

	/* Destroy a memory subspace */
	int destroy_msubspace(uint32_t msid, uint32_t msubid);

private:
	uint32_t 	ibwin_size;
	vector<ibwin>	ibwins;
	sem_t		ibwins_sem;
	riomp_mport_t	mport_hnd;
}; /* inbound */

#endif
