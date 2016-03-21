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
#include <stdint.h>
#include <semaphore.h>

#include <thread>
#include <memory>
#include "memory_supp.h"

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include "liblog.h"
#include "cm_sock.h"

#include "rdmad_cm.h"
#include "rdmad_inbound.h"
#include "rdmad_main.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_peer_utils.h"
#include "rdmad_msg_processor.h"
#include "rdmad_tx_engine.h"
#include "rdmad_rx_engine.h"

using std::thread;
using std::make_shared;
using std::unique_ptr;
using std::move;



static cm_server_msg_processor d2d_msg_proc;

/* List of destids provisioned via the provisioning thread */
daemon_list<cm_server>	prov_daemon_info_list;


/**
 * @brief Provisioning thread.
 * 	  For waiting for HELLO messages from other daemons and updating the
 * 	  provisioned daemon info list.
 *
 * @param mport_id Master port identifier
 *
 * @param prov_mbox_id	Mailbox ID to be used by the provisioning CM socket
 *
 * @param prov_channel	Channel to be used by the provisioning CM socket
 */
void *prov_thread_f(void *arg)
{
	DBG("*** Start provisioning thread...\n");
	try {
		if (arg == nullptr) {
			CRIT("NULL argument passed!!!\n");
			abort();
		}
		peer_info *peer = (peer_info *)arg;
		unique_ptr<cm_server> prov_server = make_unique<cm_server>(
				"prov_server",
				peer->mport_id,
				peer->prov_mbox_id,
				peer->prov_channel,
				&shutting_down);
		while(1) {
			/* Accept connections from other daemons */
			DBG("Accepting connections from other daemons...\n");
			int ret = prov_server->accept();
			if (ret) {
				if (ret == EINTR) {
					WARN("pthread_kill() called. Exiting!\n");
					break;
				} else {
					CRIT("Failed to accept on prov_server: %s\n",
									strerror(ret));
					/* Not much we can do here if we can't accept
					 * connections from remote daemons. This is
					 * a fatal error so we fail in a big way! */
					abort();
				}
			}
			HIGH("Remote daemon connected!\n");

			/* Create other server for handling connections with daemons */
			auto accept_socket = prov_server->get_accept_socket();
			auto other_server = make_shared<cm_server>(
							"other_server",
							accept_socket,
							&shutting_down);

			/* Create Tx and Rx engines per connection */
			auto cm_tx_eng = make_unique<cm_server_tx_engine>(
					"prov_tx_eng",
					other_server,
					cm_engine_cleanup_sem);

			auto cm_rx_eng = make_unique<cm_server_rx_engine>(
					"prov_rx_eng",
					other_server,
					d2d_msg_proc,
					cm_tx_eng.get(),
					cm_engine_cleanup_sem);

			/* We passed 'other_server' to both engines. Now
			 * relinquish ownership. They own it. Together.	 */
			other_server.reset();

			/* Create entry for remote daemon. At this point we don't
			 * know the destid, but it will be assigned to the created
			 * entry upon the HELLO exchange */
			prov_daemon_info_list.add_daemon(move(cm_tx_eng), move(cm_rx_eng));
			INFO("Added entry for remote daemon, destid TBD from HELLO\n");
		} /* while */
	} /* try */
	catch(exception& e) {
		CRIT("Failed: %s. EXITING\n", e.what());
	}
	pthread_exit(0);
} /* prov_thread_f() */
