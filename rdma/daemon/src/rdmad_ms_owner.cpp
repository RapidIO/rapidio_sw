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

#include "rdmad_unix_msg.h"
#include "rdmad_ms_owner.h"
#include "rdmad_mspace.h"
#include "rdmad_main.h"

void ms_owner::close_connections()
{
	if (!shutting_down) {
		INFO("Sending messages for all apps which have 'open'ed this mso\n");
	}

	/* Send messages for all connections indicating mso will be destroyed */
	for (mso_user& user : users) {
		tx_engine<unix_server, unix_msg_t> *user_tx_eng =
			user.get_tx_engine();

		unix_msg_t	in_msg;

		in_msg.category = RDMA_REQ_RESP;
		in_msg.type     = FORCE_CLOSE_MSO;
		in_msg.force_close_mso_req.msoid = msoid;
		user_tx_eng->send_message(&in_msg);
	}
} /* close_connections() */

ms_owner::ms_owner(const char *owner_name,
		   tx_engine<unix_server, unix_msg_t> *tx_eng, uint32_t msoid) :
	           name(owner_name), tx_eng(tx_eng), msoid(msoid),
	           mso_conn_id(MSO_CONN_ID_START)
{
}

ms_owner::~ms_owner()
{
	/* Force-close all open connections to this memory space owner */
	close_connections();

	/* Destroy all memory spaces owned by this memory space owner */
	for (mspace* ms : ms_list) {
		DBG("Destroying '%s'\n", ms->get_name());
		ms->destroy();
	}
} /* ~ms_owner() */

int ms_owner::open(uint32_t *msoid, uint32_t *mso_conn_id,
		tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	/* Don't allow the same application to open the same mso twice */
	auto it = find(begin(users), end(users), user_tx_eng);
	if (it != end(users)) {
		ERR("mso('%s') already open by same app\n", name.c_str());
		return -1;
	}

	/* Return msoid and mso_conn_id */
	*mso_conn_id = this->mso_conn_id++;
	*msoid = this->msoid;
	users.emplace_back(*mso_conn_id, user_tx_eng);

	return 1;
} /* open() */

int ms_owner::close(tx_engine<unix_server, unix_msg_t> *user_tx_eng)
{
	auto rc = 0;
	auto it = find(begin(users), end(users), user_tx_eng);
	if (it == end(users)) {
		/* Not found */
		ERR("mso is not using specified tx engine\n");
		rc = -1;
	} else {
		/* Erase user element */
		users.erase(it);
	}

	return rc;
} /* close() */

int ms_owner::close(uint32_t mso_conn_id)
{
	auto rc = 0;
	auto it = find(begin(users), end(users), mso_conn_id);
	if (it == end(users)) {
		ERR("Cannot find user with mso_conn_id(0x%X)\n", mso_conn_id);
		rc = -1;
	} else {
		/* Erase user element */
		users.erase(it);
	}
	return rc;
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
