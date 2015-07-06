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

#include <vector>
#include <string>
#include <algorithm>
#include <map>

#include <cstring>
#include <cassert>

#include <stdint.h>
#include <stdio.h>
#include <mqueue.h>
#include <semaphore.h>
#include <pthread.h>

#include "ts_vector.h"
#include "ts_map.h"
#include "rdmad_ms_owner.h"
#include "rdmad_ms_owners.h"
#include "rdmad_msubspace.h"
#include "rdmad_mspace.h"
#include "rdmad_ibwin.h"
#include "rdmad_inbound.h"
#include "rdmad_peer_utils.h"
#include "rdmad_cm.h"
#include "rdmad_main.h"
#include "rdmad.h"
#include "rdmad_rdaemon.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_srvr_threads.h"

#include "riodp_mport_lib.h"
#include "rdma_types.h"
#include "rdma_mq_msg.h"
#include "liblog.h"

using std::vector;
using std::string;
using std::map;

/* Memory Space Owner data */
ms_owners owners;

/* Inbound space */
inbound *the_inbound;

/* Global flag for shutting down */
bool shutting_down = false;

/* Map of accept messages awaiting connect. Keyed by message queue name */
ts_map<string, cm_accept_msg>	accept_msg_map;

/* List of queue names awaiting accept */
ts_vector<string>	wait_accept_mq_names;


int close_or_destroy_action(mspace *ms)
{
	/* Get list of destids connected to memory space */
	vector<uint16_t> destids = ms->get_destids();
	DBG("msid(0x%X) '%s' has %u destids associated therewith\n",
			ms->get_msid(), ms->get_name(), destids.size());

	/* For each element in the destids list, send a destroy message */
	for (auto it = begin(destids); it != end(destids); it++) {
		uint32_t destid = *it;

		/* Need to use a 'prov' socket to send the DESTROY_MS */
		sem_wait(&prov_daemon_info_list_sem);
		auto prov_it = find(begin(prov_daemon_info_list), end(prov_daemon_info_list), destid);
		if (prov_it == end(prov_daemon_info_list)) {
			ERR("Could not find socket for destid(0x%X)\n", destid);
			sem_post(&prov_daemon_info_list_sem);
			continue;	/* Better luck next time? */
		}
		cm_server *destroy_server = prov_it->conn_disc_server;
		sem_post(&prov_daemon_info_list_sem);

		/* Prepare destroy message */
		cm_destroy_msg	*dm;
		destroy_server->get_send_buffer((void **)&dm);
		dm->type	= DESTROY_MS;
		strcpy(dm->server_msname, ms->get_name());
		dm->server_msid = ms->get_msid();

		/* Send to remote daemon @ 'destid' */
		if (destroy_server->send()) {
			WARN("Failed to send destroy to destid(0x%X)\n", destid);
			continue;
		}
		INFO("Sent cm_destroy_msg for %s to remote daemon\n",
							dm->server_msname);

		/* Wait for destroy acknowledge message, but with timeout.
		 * If no ACK within say 1 second, then move on */
		cm_destroy_ack_msg *dam;
		destroy_server->flush_send_buffer();
		destroy_server->get_send_buffer((void **)&dam);
		if (destroy_server->timed_receive(1000)) {
			/* In this case whether the return value is ETIME or a failure
			 * code is irrelevant. The main thing is NOT to be stuck here.
			 */
			ERR("Did not receive destroy_ack from destid(0x%X)\n", *it);
			continue;
		}
		if (dam->server_msid != dm->server_msid)
			ERR("Received destroy_ack with wrong msid(0x%X)\n",
							dam->server_msid);
		else {
			HIGH("destroy_ack received from daemon destid(0x%X)\n", *it);
		}
	} /* for() */

	return 0;
} /* close or destroy action() */


