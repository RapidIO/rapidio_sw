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
#ifndef LIBRDMA_MSG_PROCESSOR_H
#define LIBRDMA_MSG_PROCESSOR_H

#include <cassert>

#include "rdma_msg.h"
#include "librdma_tx_engine.h"
#include "msg_processor.h"
#include "liblog.h"
#include "librdma_db.h"
#include "librdma_dispatch.h"

class unix_tx_engine;
class unix_client;

struct unix_msg_t;

class unix_msg_processor : public msg_processor<unix_client, unix_msg_t>
{
public:
	int process_msg(void *vmsg, void *vtx_eng)
	{
		unix_msg_t *msg = static_cast<unix_msg_t *>(vmsg);
		unix_tx_engine *tx_eng = static_cast<unix_tx_engine *>(vtx_eng);

		(void)tx_eng;

		auto rc = 0;

		switch(msg->type) {

		case FORCE_CLOSE_MSO:
			force_close_mso_disp(msg->force_close_mso_req.msoid);
		break;

		case FORCE_CLOSE_MS:
			force_close_ms_disp(msg->force_close_ms_req.msid);
		break;

		case DISCONNECT_MS:
			disconnect_ms_disp(msg->disconnect_from_ms_req_in.client_msubid);
		break;

		case FORCE_DISCONNECT_MS:
			force_disconnect_ms_disp(msg->force_disconnect_ms_in.server_msid,
					  msg->force_disconnect_ms_in.server_msubid);
		break;

		default:
			CRIT("Unhandled message type 0x%X cat 0x%X\n",
						msg->type, msg->category);
			assert(!"Unhandled message");
		}
		return rc;
	}
};
#endif


