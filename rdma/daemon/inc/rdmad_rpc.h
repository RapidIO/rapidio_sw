/*
 * rdmad_rpc.cpp
 *
 *  Created on: Nov 24, 2015
 *      Author: srio
 */
#include <stdint.h>

#include "unix_sock.h"
#include "rdmad_main.h"

inline int rdmad_get_mport_id(int *mport_id)
{
	if (peer.mport_id > 0) {
		*mport_id = peer.mport_id;
		return 0;
	} else {
		return -1;
	}
} /* rdmad_get_mport_id() */

inline int rdmad_create_mso(const char *mso_name,
			    uint32_t *msoid, unix_server *server=nullptr)
{
	return owners.create_mso(mso_name, server, msoid);
} /* rdmad_create_mso() */

inline int rdmad_open_mso(const char *mso_name, uint32_t *msoid,
		   uint32_t *mso_conn_id, unix_server *server=nullptr)
{
	return owners.open_mso(mso_name, msoid, mso_conn_id, server);
} /* rdmad_open_mso() */

inline int rdmad_close_mso(uint32_t msoid, uint32_t mso_conn_id)
{
	return owners.close_mso(msoid, mso_conn_id);
} /* rdmad_close_mso() */

int rdmad_destroy_mso(uint32_t msoid);

int rdmad_create_ms(const char *ms_name, uint32_t bytes, uint32_t msoid,
			uint32_t *msid, uint64_t *phys_addr);

inline int rdmad_open_ms(const char *ms_name, uint32_t *msid, uint64_t *phys_addr,
		  uint32_t *ms_conn_id, uint32_t *bytes,
		  unix_server *server=nullptr)
{
	return the_inbound->open_mspace(
				ms_name,
				server,
				msid,
				phys_addr,
				ms_conn_id,
				bytes);
} /* rdmad_open_ms() */

int rdmad_close_ms(uint32_t msid, uint32_t ms_conn_id);

int rdmad_destroy_ms(uint32_t msoid, uint32_t msid);

inline int rdmad_create_msub(uint32_t msid, uint32_t offset,
			     uint32_t req_bytes, uint32_t *bytes,
			     uint32_t *msubid, uint64_t *rio_addr,
			     uint64_t *phys_addr)
{
	return the_inbound->create_msubspace(
				msid,
				offset,
				req_bytes,
				bytes,
                                msubid,
				rio_addr,
				phys_addr);
} /* rdmad_create_msub() */

inline int rdmad_destroy_msub(uint32_t msid, uint32_t msubid)
{
	return the_inbound->destroy_msubspace(msid, msubid);
} /* rdmad_destroy_msub() */

int rdmad_accept_ms(const char *loc_ms_name, uint32_t loc_msubid,
		    uint32_t loc_bytes, uint64_t loc_rio_addr_len,
		    uint64_t loc_rio_addr_log, uint8_t loc_rio_addr_hi);

int rdmad_undo_accept_ms(const char *ms_name);

int rdmad_send_connect(const char *server_ms_name,
			uint32_t server_destid,
			uint32_t client_msid,
		        uint32_t client_msubid,
		        uint32_t client_bytes,
		        uint8_t client_rio_addr_len,
		        uint64_t client_rio_addr_lo,
		        uint8_t client_rio_addr_hi,
		        uint64_t seq_num,
		        unix_server *server=nullptr);

int rdmad_undo_connect(const char *server_ms_name);

int rdmad_send_disconnect(uint32_t rem_destid,
			  uint32_t rem_msid,
			  uint32_t loc_msubid);
