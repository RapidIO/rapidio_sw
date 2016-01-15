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
#ifndef RDMAD_MSG_PROCESSOR_H
#define RDMAD_MSG_PROCESSOR_H

#include "rdma_msg.h"
#include "msg_processor.h"
#include "liblog.h"
#include "rdmad_dispatch.h"

class unix_tx_engine;
class unix_server;
struct unix_msg_t;

class unix_msg_processor : public msg_processor<unix_server, unix_msg_t>
{
public:
	int process_msg(void *vmsg, void *vtx_eng)
	{
		auto rc = 0;
		unix_msg_t *msg = static_cast<unix_msg_t *>(vmsg);
		unix_tx_engine *tx_eng = static_cast<unix_tx_engine *>(vtx_eng);

		switch(msg->type) {
		case GET_MPORT_ID:
			rc = get_mport_id_disp(msg, tx_eng);
			break;
		case CREATE_MSO:
			rc = create_mso_disp(msg, tx_eng);
			break;
		case DESTROY_MSO:
			rc = destroy_mso_disp(msg, tx_eng);
			break;
		default:
			assert(!"Unhandled message");
		}
		return rc;
	}
};

class cm_server_msg_processor : public msg_processor<cm_server, cm_msg_t>
{
public:
	int process_msg(void *vmsg, void *vtx_eng)
	{
		auto rc = 0;
		cm_msg_t *msg = static_cast<cm_msg_t *>(vmsg);
		cm_server_tx_engine *tx_eng =
			static_cast<cm_server_tx_engine *>(vtx_eng);

		(void)tx_eng;	// Temporary

		switch(msg->type) {

		default:
			assert(!"Unhandled message");
		}
		return rc;
	}
};

class cm_client_msg_processor : public msg_processor<cm_client, cm_msg_t>
{
public:
	int process_msg(void *vmsg, void *vtx_eng)
	{
		auto rc = 0;
		cm_msg_t *msg = static_cast<cm_msg_t *>(vmsg);
		cm_client_tx_engine *tx_eng =
			static_cast<cm_client_tx_engine *>(vtx_eng);

		(void)tx_eng;	// Temporary
		switch(msg->type) {

		default:
			assert(!"Unhandled message");
		}
		return rc;
	}
};

#endif

