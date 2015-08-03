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
#include <errno.h>
#include <unistd.h>

#include <cstdio>

#include <sstream>
#include <utility>
#include <vector>
#include <string>

#include "rdma_types.h"
#include "liblog.h"

#include "rdma_mq_msg.h"
#include "rdmad_ms_owner.h"
#include "rdmad_mspace.h"
#include "rdmad_main.h"

struct close_conn_to_mso {
	close_conn_to_mso(uint32_t msoid) : ok(true), msoid(msoid) {}

	void operator()(msg_q<mq_close_mso_msg> *close_mq)
	{
		struct mq_close_mso_msg	*close_msg;

		close_mq->get_send_buffer(&close_msg);
		close_msg->msoid = msoid;
		if (close_mq->send())
			ok = false;
	}

	bool ok;

private:
	uint32_t msoid;
};

int ms_owner::close_connections()
{
	close_conn_to_mso	cctm(msoid);
	
	if (!shutting_down) {
		INFO("Sending messages for all apps which have 'open'ed this mso\n");
	}

	/* Send messages for all connections indicating mso will be dropped */
	for_each(begin(mq_list), end(mq_list), cctm);

	return cctm.ok ? 0 : -1;

} /* close_connections() */


ms_owner::~ms_owner()
{
	/* Do not bother with warning if we are shutting down. It causes Valgrind
	 * issues as well. */
	if (!shutting_down && close_connections()) {
		WARN("One or more connections to msoid(0x%X) failed to close\n",
				msoid);
	}

	/* Destroy all memory spaces */
	for (mspace* ms : ms_list) {
		DBG("Destroying '%s'\n", ms->get_name());
		ms->destroy();
	}

	/* Destroy message queues before dying! */
	for (auto mq_ptr : mq_list)
		delete mq_ptr;
} /* ~ms_owner() */

int ms_owner::open(uint32_t *msoid, uint32_t *mso_conn_id)
{
	stringstream		qname;

	/* Prepare POSIX message queue name */
	qname << '/' << name << this->mso_conn_id;
	
	/* Create POSIX message queue for request-close messages */
	msg_q<mq_close_mso_msg>	*close_mq;
	try {
		close_mq = new msg_q<mq_close_mso_msg>(qname.str(), MQ_CREATE);
	}
	catch(msg_q_exception e) {
		CRIT("Failed to create close_mq: %s\n", e.msg.c_str());
		return -1;
	}
	INFO("Message queue %s created\n", qname.str().c_str());

	/* Add message queue info to list */
	mq_list.push_back(close_mq);

	/* Return msoid and mso_conn_id */
	*mso_conn_id = this->mso_conn_id++;
	*msoid = this->msoid;
	
	return 1;
} /* open() */

struct close_mq_has_name {
	close_mq_has_name(const string& name) : name(name) {}
	bool operator()(msg_q<mq_close_mso_msg> *close_mq)
	{
		return close_mq->get_name() == name;
	}
private:
	string name;
};

int ms_owner::close(uint32_t mso_conn_id)
{
	stringstream		qname;

	/* Prepare queue name */
	qname << '/' << name << mso_conn_id;

	/* Find entry with queue name */
	close_mq_has_name	cmhn(qname.str());
	auto it = find_if(begin(mq_list), end(mq_list), cmhn);

	/* Delete queue corresponding to connection, & remove entry from list */
	delete *it;
	mq_list.erase(it);
	
	return 1;
} /* close() */

void ms_owner::add_ms(mspace *ms)
{
	INFO("Adding msid(0x%X) to msoid(0x%X)\n", ms->get_msid(), msoid);
	ms_list.push_back(ms);
} /* add_ms() */

int ms_owner::remove_ms(mspace* ms)
{
	/* Find memory space by the handle, return error if not there */
	auto it = find(begin(ms_list), end(ms_list), ms);
	if (it == end(ms_list)) {
		WARN("ms('%s', 0x%X) not owned by msoid('%s',0x%X)\n",
				ms->get_name(), ms->get_msid(),
				name.c_str(), msoid);
		return -1;
	}

	/* Erase memory space handle from list */
	INFO("Removing msid(0x%X) from msoid(0x%X)\n", ms->get_msid(), msoid);
	ms_list.erase(it);

	return 1;
} /* remove_ms_h() */

void ms_owner::dump_info(struct cli_env *env)
{
	sprintf(env->output, "%8X %32s\t", msoid, name.c_str());
	logMsg(env);
	for (auto& ms : ms_list) {
		sprintf(env->output, "%8X ", ms->get_msid());
		logMsg(env);
	}
	sprintf(env->output, "\n");
	logMsg(env);
} /* dump_info() */
