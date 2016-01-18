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
#ifndef RDMAD_RX_ENGINE_H
#define RDMAD_RX_ENGINE_H

#include <memory>

#include "msg_processor.h"
#include "rx_engine.h"
#include "cm_sock.h"
#include "rdmad_main.h"

using std::shared_ptr;

class unix_server;
struct unix_msg_t;
struct cm_msg_t;

class unix_rx_engine : public rx_engine<unix_server, unix_msg_t>
{
public:
	unix_rx_engine(shared_ptr<unix_server> client,
			msg_processor<unix_server, unix_msg_t> &message_processor,
			tx_engine<unix_server, unix_msg_t> *tx_eng,
			sem_t *engine_cleanup_sem) :
	rx_engine<unix_server, unix_msg_t>(client, message_processor, tx_eng, engine_cleanup_sem)
	{}

	 void cleanup()
	 {
		 owners.close_mso(tx_eng);
		 owners.destroy_mso(tx_eng);
	 }
};

class cm_server_rx_engine : public rx_engine<cm_server, cm_msg_t>
{
public:
	cm_server_rx_engine(shared_ptr<cm_server> client,
			msg_processor<cm_server, cm_msg_t> &message_processor,
			tx_engine<cm_server, cm_msg_t> *tx_eng,
			sem_t *engine_cleanup_sem) :
	rx_engine<cm_server, cm_msg_t>(client, message_processor, tx_eng, engine_cleanup_sem)
	{}
};

class cm_client_rx_engine : public rx_engine<cm_client, cm_msg_t>
{
public:
	cm_client_rx_engine(shared_ptr<cm_client> client,
			msg_processor<cm_client, cm_msg_t> &message_processor,
			tx_engine<cm_client, cm_msg_t> *tx_eng,
			sem_t *engine_cleanup_sem) :
	rx_engine<cm_client, cm_msg_t>(client, message_processor, tx_eng, engine_cleanup_sem)
	{}
};

#endif

