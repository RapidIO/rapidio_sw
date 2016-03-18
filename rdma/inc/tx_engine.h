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
#include <pthread.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <queue>
#include <memory>
#include "memory_supp.h"

#include "liblog.h"
#include "rdmad_unix_msg.h"
#include "rdma_msg.h"

using std::queue;
using std::shared_ptr;
using std::unique_ptr;


template <typename T, typename M>
class rx_engine;

template <typename T, typename M>
struct tx_work_thread_info {
	bool 			*stop_worker_thread;
	pthread_mutex_t		*message_queue_lock;
	queue<unique_ptr<M>>	*message_queue;
	T			*client;
	bool 			*is_dead;
	bool 			*worker_is_dead;
	sem_t			*engine_cleanup_sem;
	sem_t			*messages_waiting_sem;
};

template <typename T, typename M>
void *tx_worker_thread_f(void *arg)
{
	tx_work_thread_info<T,M> *wti = (tx_work_thread_info<T,M> *)arg;

	bool *stop_worker_thread 	    = wti->stop_worker_thread;
	pthread_mutex_t	*message_queue_lock = wti->message_queue_lock;
	queue<unique_ptr<M>> *message_queue = wti->message_queue;
	T* client 			    = wti->client;
	bool *is_dead			    = wti->is_dead;
	sem_t		*engine_cleanup_sem = wti->engine_cleanup_sem;
	bool *worker_is_dead		    = wti->worker_is_dead;
	sem_t 	    *messages_waiting_sem   = wti->messages_waiting_sem;

	DBG("worker thread started\n");
	while(1) {
		/* Wait until a message is enqueued for transmission or the
		 * destructor posts the semaphore to terminate the thread */
		DBG("Waiting for message to be sent...\n");
		sem_wait(messages_waiting_sem);
		DBG("Not waiting anymore..\n");

		/* This happens when we are trying to exit the thread
		 * from the destructor: stop_worker_thread is set to true
		 * and the semaphore 'messages_waiting' is posted with the
		 * queue being empty. */
		if (*stop_worker_thread) {
			printf("%s: stop_worker_thread is set\n", __func__);
			break;
		}

		pthread_mutex_lock(message_queue_lock);
		DBG("Getting message at front of queue\n");
		M*	msg_ptr = message_queue->front().get();
		pthread_mutex_unlock(message_queue_lock);

		int rc = client->send_buffer(msg_ptr, sizeof(M));
		if (rc != 0) {
			/* This code may not be needed. The rx_engine always
			 * senses that the other peer has disappeared and
			 * sets our is_dead flag there. This then triggers
			 * the engine cleanup thread to kill us. */
			ERR("Failed to send, rc = %d: %s\n",
						rc, strerror(errno));
			*is_dead = true;
			sem_post(engine_cleanup_sem);
			break;
		} else {
			/* Remove message from queue */
			pthread_mutex_lock(message_queue_lock);
			DBG("Popping message out of queue\n");
			message_queue->pop();
			pthread_mutex_unlock(message_queue_lock);
		}

	}
	*worker_is_dead = true;
	printf("Exiting %s\n", __func__);
	pthread_exit(0);
}

template <typename T, typename M>
class tx_engine {

public:
	tx_engine(shared_ptr<T> client, sem_t *engine_cleanup_sem) :
			rx_eng(nullptr), client(client), stop_worker_thread(false),
			worker_is_dead(false),
			is_dead(false), engine_cleanup_sem(engine_cleanup_sem)
	{
		if (sem_init(&messages_waiting_sem, 0, 0)) {
			CRIT("Failed to sem_init 'messages_waiting': %s\n",
						strerror(errno));
			throw -1;
		}
		if (pthread_mutex_init(&message_queue_lock, NULL)) {
			CRIT("Failed to init message_queue_lock mutex\n");
			throw -1;
		}

		tx_work_thread_info<T,M> *wti;
		try {
			wti = new tx_work_thread_info<T,M>();
		}
		catch(...) {
			CRIT("Failed to create work thread info\n");
			throw -2;
		}
		wti->stop_worker_thread = &stop_worker_thread;
		wti->message_queue_lock = &message_queue_lock;
		wti->message_queue	= &message_queue;
		wti->client 		= client.get();
		wti->is_dead		= &is_dead;
		wti->engine_cleanup_sem = engine_cleanup_sem;
		wti->worker_is_dead	= &worker_is_dead;
		wti->messages_waiting_sem = &messages_waiting_sem;

		if (pthread_create(&work_thread, NULL, &tx_worker_thread_f<T,M>, (void *)wti)) {
			CRIT("Failed to start work_thread: %s\n", strerror(errno));
			throw -2;
		}
	} /* ctor */

	virtual ~tx_engine()
	{
		puts(__func__);
		/* If worker_is_dead was true, then the thread has already
		 * self-terminated and we can just delete the worker_thread object.
		 * Otherwise, the thread is alive and we need to kill it first.	 */
		if (!worker_is_dead) {
			printf("worker() still running...\n");
			stop_worker_thread = true;
			sem_post(&messages_waiting_sem); // Wake up the thread.
			if (pthread_join(work_thread, NULL)) {
				CRIT("Failed to join work_thread: %s\n",
							strerror(errno));
			}
		}
	} /* dtor */

	bool isdead() const { return is_dead; }

	void set_isdead() { is_dead = true; }

	void set_rx_eng(rx_engine<T,M> *rx_eng) { this->rx_eng = rx_eng; }

	rx_engine<T,M> *get_rx_eng() const { return rx_eng; }

	T *get_client() { return client.get(); }

	/* Returns sequence number to be used to receive reply */
	void send_message(unique_ptr<M> msg_ptr)
	{
		DBG("ENTER\n");
		DBG("Sending type:'%s',0x%" PRIx64 ", cat:'%s',0x%" PRIx64 " seq_no(0x%" PRIx64 ")\n",
			type_name(msg_ptr->type), msg_ptr->type,
			cat_name(msg_ptr->category), msg_ptr->category,
			msg_ptr->seq_no);
		pthread_mutex_lock(&message_queue_lock);
		message_queue.push(move(msg_ptr));
		pthread_mutex_unlock(&message_queue_lock);
		sem_post(&messages_waiting_sem);
		DBG("EXIT\n");
	} /* send_message() */

protected:
	rx_engine<T,M>	*rx_eng;
	queue<unique_ptr<M>>	message_queue;
	pthread_mutex_t	message_queue_lock;
	shared_ptr<T>	client;
	sem_t		messages_waiting_sem;
	pthread_t 	work_thread;
	bool		stop_worker_thread;
	bool		worker_is_dead;
	bool		is_dead;
	sem_t		*engine_cleanup_sem;
}; /* tx_engine */

#endif

