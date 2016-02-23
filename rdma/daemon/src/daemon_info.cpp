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
#include <pthread.h>
#include <signal.h>	// pthread_kill()

#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>

#include "memory_supp.h"
#include "cm_sock.h"
#include "daemon_info.h"

using std::vector;
using std::unique_ptr;
using std::move;
using std::find;
using std::lock_guard;
using std::for_each;


daemon_info::daemon_info(uint32_t destid, cm_base *cm_sock, pthread_t tid)
				: destid(destid), cm_sock(cm_sock), tid(tid)
{
} /* ctor */

daemon_info::~daemon_info()
{
} /* dtor */

void daemon_info::kill()
{
	pthread_kill(tid, SIGUSR1);
} /* kill() */

bool daemon_info::operator==(uint32_t destid)
{
	return this->destid == destid;
} /* operator== */

daemon_list::~daemon_list()
{
	daemons.clear();
} /* dtor */

cm_base* daemon_list::get_cm_sock_by_destid(uint32_t destid)
{
	lock_guard<mutex> daemons_lock(daemons_mutex);

	daemon_iterator it = find_if(begin(daemons), end(daemons),
		[destid](daemon_element& d) { return d->destid == destid;});
	return (it == end(daemons)) ? nullptr : (*it)->cm_sock;
} /* get_server_by_destid() */


void daemon_list::add_daemon(uint32_t destid,
			     cm_base *cm_sock,
			     pthread_t tid)
{
	lock_guard<mutex> daemons_lock(daemons_mutex);

	/* If destid already in our list, kill its thread, and replace the old
	 * info with the new parameters passed-in. */
	daemon_iterator it = find_if(begin(daemons), end(daemons),
		[destid](daemon_element& d) { return d->destid == destid;});
	if (it != end(daemons)) {
		WARN("Killing thread for known destid(0x%X).\n", destid);
		/* Killing the thread causes the conn_disc_server to be freed */
		pthread_kill((*it)->tid, SIGUSR1);
		(*it)->tid = tid;
		(*it)->cm_sock = cm_sock;
	} else {
		/* This is a brand new entry */
		auto pdi = make_unique<daemon_info>(
						destid, cm_sock, tid);
		daemons.push_back(move(pdi));
	}
} /* add_prov_daemon() */

int daemon_list::remove_daemon(uint32_t destid)
{
	int rc;

	lock_guard<mutex> daemons_lock(daemons_mutex);
	daemon_iterator it = find_if(begin(daemons), end(daemons),
		[destid](daemon_element& d) { return d->destid == destid;});

	if (it == end(daemons)) {
		ERR("Cannot find entry with destid(0x%X)\n", destid);
		rc = -1;
	} else {
		daemons.erase(it);
		INFO("Daemon entry for destid(0x%X) removed\n", destid);
		rc = 0;
	}

	return rc;
} /* remove_daemon() */

void daemon_list::kill_all_threads()
{
	for(auto& de : daemons) {
		de->kill();
	}
}
