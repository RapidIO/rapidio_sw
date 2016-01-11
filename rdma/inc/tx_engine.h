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
#ifndef TX_ENGINE_H
#define TX_ENGINE_H

#include <semaphore.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <queue>
#include <thread>

#include "liblog.h"

using std::queue;
using std::thread;

template <typename T, typename M>
class tx_engine {

public:
	tx_engine() : client(nullptr)
	{
		sem_init(&start, 0, 0);
		sem_init(&messages_waiting, 0, 0);
		thread worker_thread(&tx_engine::worker, this);
	}

	/* Returns sequence number to be used to receive reply */
	uint32_t send_message(M* msg_ptr)
	{
		static uint32_t seq_no = MSG_SEQ_NO_START;

		msg_ptr->seq_no = seq_no;
		message_queue.push(msg_ptr);
		sem_post(&messages_waiting);

		return seq_no++;
	}

	void set_client(T* client)
	{
		assert(client != nullptr);
		this->client = client;
		sem_post(&start);
	}

private:
	static constexpr uint32_t MSG_SEQ_NO_START = 0x0000000A;

	void worker() {
		/* Wait until client is initialized */
		sem_wait(&start);

		while(1) {
			/* Wait until a message is enqueued for transmission */
			sem_wait(&messages_waiting);

			/* Grab next message to be sent and send it */
			M*	msg_ptr = message_queue.front();
			int rc = client->send_buffer(msg_ptr, sizeof(*msg_ptr));
			if (rc != 0) {
				ERR("Failed to send, rc = %d: %s\n", rc, strerror(errno));
			}

			/* Remove and delete sent message */
			message_queue.pop();
			delete msg_ptr;
		}
	}
	queue<M*>	message_queue;
	T*		client;
	sem_t		start;
	sem_t		messages_waiting;
};

#endif

