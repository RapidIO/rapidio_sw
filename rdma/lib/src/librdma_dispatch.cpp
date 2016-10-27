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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>
#include <inttypes.h>

#include <stdint.h>

#include "liblog.h"

#include "rdmad_unix_msg.h"
#include "librdma_tx_engine.h"
#include "librdma_db.h"

/* From librdma.cpp */
extern "C" int destroy_msubs_in_msh(ms_h msh);

void force_close_mso_disp(unix_msg_t *msg, unix_tx_engine *tx_eng)
{
	(void)tx_eng;

	HIGH("FORCE_CLOSE_MSO received\n");

	auto msoid = msg->force_close_mso_req.msoid;

	/* Find the mso in the local database by its msoid */
	auto msoh = find_mso(msoid);
	if (!msoh) {
		CRIT("Could not find msoid(0x%X) in database\n", msoid);
	} else {
		/* Remove mso with specified msoid, msoh from database */
		if (remove_loc_mso(msoh)) {
			WARN("Failed removing msoid(0x%X) msoh(0x%" PRIx64 ")\n",
								msoid, msoh);
		} else {
			HIGH("msoid(0x%X) force-closed\n", msoid);
		}
	}
} /* force_close_mso_disp() */

void force_close_ms_disp(unix_msg_t *msg, unix_tx_engine *tx_eng)
{
	(void)tx_eng;

	auto msid = msg->force_close_ms_req.msid;

	/* Find the ms in the local database by its msid */
	auto msh = find_loc_ms(msid);
	if (!msh) {
		CRIT("Could not find ms(0x%X)\n", msid);
	} else if (destroy_msubs_in_msh(msh)) {
		WARN("Failed to destroy msubs in msid(0x%X)\n", msid);
	} else if (remove_loc_ms(msh)) {
		WARN("Failed for msid(0x%X)\n", msid);
	} else {
		INFO("msid(0x%X) removed from database\n", msid);
	}
} /* force_close_ms_disp() */

void disconnect_ms_disp(unix_msg_t *msg, unix_tx_engine *tx_eng)
{
	(void)tx_eng;

	auto client_msubid = msg->disconnect_from_ms_in.client_msubid;
	auto server_msubid = msg->disconnect_from_ms_in.server_msubid;
	auto client_to_lib_tx_eng_h =
			msg->disconnect_from_ms_in.client_to_lib_tx_eng_h;

	/* Remove client msub from the database, if not NULL_MSUB */
	if (client_msubid != NULL_MSUBID) {
		msub_h client_msubh = find_rem_msub(client_msubid);
		if (!client_msubh) {
			ERR("client_msubid(0x%X) not found!\n", client_msubid);
		} else {
			/* Remove client subspace from remote msub database */
			remove_rem_msub(client_msubh);
			INFO("client_msubid(0x%X) removed from database\n",
							client_msubid);
		}
	}

	/* Find the server msub, then remove the matching connection therefrom */
	msub_h server_msubh = find_loc_msub(server_msubid);
	if (server_msubh) {
		loc_msub *server_msub = (loc_msub *)server_msubh;
		/* Locate connection entry matching client_to_lib_tx_eng_h */
		auto it = find_if(begin(server_msub->connections),
				  end(server_msub->connections),
				  [client_to_lib_tx_eng_h](client_connection& cc)
				  {
				  	  return cc.client_to_lib_tx_eng_h ==
				  			client_to_lib_tx_eng_h;
				  });
		if (it == end(server_msub->connections)) {
			ERR("Could not find specified connection in msub\n");
		} else {
			/* Erase the connection entry */
			INFO("Removed connection from msubid(0x%X)\n",
								server_msubid);
			server_msub->connections.erase(it);
		}
	} else {
		ERR("server_msubid(0x%X) not found!\n", server_msubid);
	}
} /* disconnect_ms_disp() */

void force_disconnect_ms_disp(unix_msg_t *msg, unix_tx_engine *tx_eng)
{
	auto server_msid   = msg->force_disconnect_ms_in.server_msid;
	auto server_msubid = msg->force_disconnect_ms_in.server_msubid;

	/* Remove the remote memory space and subspace from the database */
	remove_rem_msubs_in_ms(server_msid);
	remove_rem_ms(server_msid);

	/* ACK that the MS_DESTROYED has arrived and was acted upon */
	auto in_msg = make_unique<unix_msg_t>();
	in_msg->category = RDMA_CALL;
	in_msg->type = FORCE_DISCONNECT_MS_ACK;
	in_msg->force_disconnect_ms_ack_in.server_msid = server_msid;
	in_msg->force_disconnect_ms_ack_in.server_msubid = server_msubid;
	in_msg->seq_no = 0;

	tx_eng->send_message(move(in_msg));

} /* force_disconnect_ms_disp() */

