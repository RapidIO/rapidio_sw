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
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <queue>
#include <thread>

#include "liblog.h"
#include "rdma_msg.h"

using std::queue;
using std::thread;

template <typename T, typename M>
class tx_engine {

public:
	tx_engine(T *client, sem_t *engine_cleanup_sem) :
			client(client), stop_worker_thread(false),
			worker_is_dead(false),
			is_dead(false), engine_cleanup_sem(engine_cleanup_sem)
	{
		sem_init(&messages_waiting, 0, 0);
		DBG("Starting thread...\n");
		worker_thread = new thread(&tx_engine::worker, this);
	} /* ctor */

	~tx_engine()
	{
		HIGH("Destructor\n");
		/* If worker_is_dead was true, then the thread has already
		 * self-terminated and we can just delete the worker_thread object.
		 * Otherwise, the thread is alive and we need to kill it first.	 */
		if (!worker_is_dead) {
			stop_worker_thread = true;
			sem_post(&messages_waiting); // Wake up the thread.
			auto timeout = 100;
			while (timeout-- && !worker_is_dead)
				usleep(10000);
			if (timeout == 0) {
				ERR("Failed to kill worker thread function\n");
			}
		}
		worker_thread->join();
		delete worker_thread;

		// FIXME: When 'client' is shared, release a reference here.
	} /* dtor */

	bool isdead() const { return is_dead; }

	T *get_client() { return client; }

	/* Returns sequence number to be used to receive reply */
	rdma_msg_seq_no send_message(M* msg_ptr)
	{
		static rdma_msg_seq_no seq_no = MSG_SEQ_NO_START;

		DBG("Sending 0x%X\n", msg_ptr->type);
		msg_ptr->seq_no = seq_no;
		message_queue.push(msg_ptr);
		sem_post(&messages_waiting);

		return seq_no++;
	} /* send_message() */

private:
	static constexpr uint32_t MSG_SEQ_NO_START = 0x0000000A;

	void worker() {
		DBG("worker thread started\n");
		while(!stop_worker_thread) {
			/* Wait until a message is enqueued for transmission */
			DBG("Waiting for message to be sent...\n");
			sem_wait(&messages_waiting);

			/* This happens when we are trying to exit the thread
			 * from the destructor: stop_worker_thread is set to true
			 * and the semaphore 'messages_waiting is posted with the
			 * queue being empty. */
			if (message_queue.empty()) {
				WARN("'messages_waiting' posted but queue empty\n");
				continue;
			}

			/* Grab next message to be sent and send it */
			M*	msg_ptr = message_queue.front();
			int rc = client->send_buffer(msg_ptr, sizeof(*msg_ptr));
			if (rc != 0) {
				ERR("Failed to send, rc = %d: %s\n",
							rc, strerror(errno));
				is_dead = true;
				sem_post(engine_cleanup_sem);
				stop_worker_thread = true;
			} else {
				/* Remove message from queue */
				message_queue.pop();
			}
		}
		worker_is_dead = true;
	} /* worker() */

	queue<M*>	message_queue;
	T*		client;		// FIXME: Need a shared_ptr here since
					// this is shared with rx_engine
	sem_t		messages_waiting;
	sem_t		start_worker_thread;
	thread		*worker_thread;
	bool		stop_worker_thread;
	bool		worker_is_dead;
	bool		is_dead;
	sem_t		*engine_cleanup_sem;
}; /* tx_engine */

#endif

