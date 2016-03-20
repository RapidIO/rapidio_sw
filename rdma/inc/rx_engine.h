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
#ifndef RX_ENGINE_H
#define RX_ENGINE_H

#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <list>
#include <thread>
#include <vector>
#include <memory>

#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cassert>

#include "liblog.h"

#include "rdma_msg.h"
#include "rdmad_unix_msg.h"
#include "tx_engine.h"
#include "msg_processor.h"

using std::list;
using std::vector;
using std::shared_ptr;

/* Notification parameters. */
struct notify_param {

	notify_param(uint32_t type, uint32_t category, uint32_t seq_no,
					shared_ptr<sem_t> notify_sem) :
		type(type), category(category), seq_no(seq_no),
		notify_sem(notify_sem)
	{}

	bool operator ==(const notify_param& other) {
		return (other.type == this->type) &&
		       (other.category == this->category) &&
		       (other.seq_no == this->seq_no);
	}

	rdma_msg_type 	type;
	rdma_msg_cat 	category;
	rdma_msg_seq_no seq_no;
	shared_ptr<sem_t>	notify_sem;
};

template <typename T, typename M>
struct rx_work_thread_info {
	rx_work_thread_info(msg_processor<T, M>	&message_processor) :
		message_processor(message_processor)
	{}
	bool 			*stop_worker_thread;
	pthread_mutex_t		*message_queue_lock;
	vector<M>		*message_queue;
	T			*client;
	bool 			*is_dead;
	bool 			*worker_is_dead;
	sem_t			*engine_cleanup_sem;
	pthread_mutex_t		*notify_list_lock;
	vector<notify_param> 	*notify_list;
	msg_processor<T, M>	&message_processor;
	tx_engine<T, M>		*tx_eng;
};

template <typename T, typename M>
void *rx_worker_thread_f(void *arg)
{
	rx_work_thread_info<T,M> *wti = (rx_work_thread_info<T,M> *)arg;

	bool *stop_worker_thread 	    = wti->stop_worker_thread;
	pthread_mutex_t	*message_queue_lock = wti->message_queue_lock;
	vector<M> 	*message_queue 	    = wti->message_queue;
	T* client 			    = wti->client;
	bool *is_dead			    = wti->is_dead;
	sem_t		*engine_cleanup_sem = wti->engine_cleanup_sem;
	bool *worker_is_dead		    = wti->worker_is_dead;
	pthread_mutex_t *notify_list_lock   = wti->notify_list_lock;
	vector<notify_param> 	*notify_list= wti->notify_list;
	msg_processor<T, M> &message_processor = wti->message_processor;
	tx_engine<T, M>		*tx_eng	    = wti->tx_eng;

	M	*msg;
	client->get_recv_buffer((void **)&msg);

	while(1) {
		size_t	received_len = 0;

		/* Always flush buffer to ensure no leftover data
		 * from prior messages */
		client->flush_recv_buffer();

		/* Wait for new message to arrive */
		DBG("Waiting for new message to arrive...\n");
		int rc = client->receive(&received_len);
		if (rc) {
			/* If we fail to receive, the engine dies, whatever the reason! */
			if (rc == EINTR) {
				if (*stop_worker_thread) {
					WARN("pthread_kill() called from destructor\n");
					break;
				} else {
					WARN("Someone called pthread_kill(). Who???\n");
					break;
				}
			} else {
				ERR("Failed to receive for some UNKNOWN reason!\n");
				break;
			}
		} else if (received_len == 0) {
			WARN("Other side has disconnected\n");
			break;
		} else if (received_len < 0) {
			assert(!"received_len < 0");
		} else { /* received_len > 0. All is good */
#ifdef EXTRA_DEBUG
			DBG("msg[0] = 0x%" PRIx64 "\n", *(uint64_t *)msg);
			DBG("msg[1] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)msg + 8));
			DBG("msg[2] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)msg + 16));
			DBG("msg[3] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)msg + 24));
#endif
			DBG("Got category=0x%" PRIx64 ",'%s'\n", msg->category,
						cat_name(msg->category));
			if (msg->category == RDMA_CALL) {
				/* If there is a notification set for the
				 * message then act on it. */
				pthread_mutex_lock(notify_list_lock);
				auto it = find(begin(*notify_list),
						end(*notify_list),
						notify_param(msg->type,
								msg->category,
								msg->seq_no,
								nullptr));
				if (it != end(*notify_list)) {
					/* Found! Queue copy of message & post semaphore */
					DBG("Found message of type '%s',0x%X, seq_no=0x%X\n",
						type_name(it->type),
						it->type,
						it->seq_no);
					pthread_mutex_lock(message_queue_lock);
					message_queue->push_back(*msg);
					pthread_mutex_unlock(message_queue_lock);

					/* Remove notify entry from notify list */
					notify_list->erase(it);

					/* Post the notification semaphore */
					sem_post(it->notify_sem.get());
				} else {
					CRIT("Non-matching API type('%s',0x%X) seq_no(0x%X)\n",
							type_name(msg->type),
							msg->type,
							msg->seq_no);
				}
				pthread_mutex_unlock(notify_list_lock);
			} else if (msg->category == RDMA_REQ_RESP) {
				/* Process request/resp by forwarding to message processor */
				rc = message_processor.process_msg(msg, tx_eng);
				if (rc) {
					ERR("Failed to process message, rc = 0x%X\n", rc);
				}
			} else {
				CRIT("Unhandled msg->category = 0x%" PRIx64 "\n",
							msg->category);
				CRIT("msg->type='%s',0x%X\n",
						type_name(msg->type),
						msg->type);
				abort();
			}
		}
	}
	*worker_is_dead = true;
	*is_dead = true;
	tx_eng->set_isdead();	// Kill corresponding tx_engine too
	sem_post(engine_cleanup_sem);
	DBG("Exiting %s\n", __func__);
	pthread_exit(0);
}

template <typename T, typename M>
class rx_engine {

public:
	rx_engine(shared_ptr<T> client, msg_processor<T, M> &message_processor,
			tx_engine<T, M> *tx_eng, sem_t *engine_cleanup_sem) :
		message_processor(message_processor), client(client), tx_eng(tx_eng),
		is_dead(false), worker_is_dead(false),
		stop_worker_thread(false), engine_cleanup_sem(engine_cleanup_sem)
	{
		if (pthread_mutex_init(&notify_list_lock, NULL)) {
			CRIT("Failed to init notify_list_lock mutex\n");
			throw -1;
		}
		if (pthread_mutex_init(&message_queue_lock, NULL)) {
			CRIT("Failed to init message_queue_lock mutex\n");
			throw -1;
		}

		tx_eng->set_rx_eng(this);

		rx_work_thread_info<T,M> *wti;
		try {
			wti = new rx_work_thread_info<T,M>(message_processor);
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
		wti->notify_list	= &notify_list;
		wti->notify_list_lock	= &notify_list_lock;
		wti->tx_eng		= tx_eng;

		if (pthread_create(&rx_work_thread, NULL, &rx_worker_thread_f<T,M>, (void *)wti)) {
			CRIT("Failed to start work_thread: %s\n", strerror(errno));
			throw -2;
		}
	} /* ctor */

	virtual ~rx_engine()
	{
		/**
		 * @brief There are two possiblities:
		 * 1. The worker thread detected an error, it self exited, set the
		 * is_dead flag, and then set the engine clean up semaphore so
		 * that the engine monitoring thread would kill us.
		 *
		 * 2. The RDMA daemon is simply being quit and all engines are
		 * being removed. In this case the worker thread is still running
		 * and we need to kill it.
		 *
		 */
		if (!worker_is_dead) {
			DBG("%s: Stopping worker thread\n", __func__);
			pthread_kill(rx_work_thread, SIGUSR1);
			pthread_join(rx_work_thread, NULL);
			DBG("%s: rx_work_thread terminated.\n", __func__);
		}
	} /* dtor */

	bool isdead() const { return is_dead; }

	/**
	 * Set the RX engine to post notify_sem when a message of the specified
	 * type, category, and seq_no is received by this rx_engine.
	 */
	int set_notify(uint32_t type, uint32_t category, uint32_t seq_no,
						shared_ptr<sem_t> notify_sem)
	{

		pthread_mutex_lock(&notify_list_lock);
		DBG("type='%s',0x%X, category='%s',0x%X, seq_no=0x%X\n",
		type_name(type), type, cat_name(category), category, seq_no);

		/* We should not be setting the same notification twice */
		int rc = count(begin(notify_list),
			       end(notify_list),
			       notify_param(type, category, seq_no,
					       	       	       notify_sem));
		if (rc != 0) {
			ERR("Duplicate notify entry ignored!\n");
		} else {
			notify_list.emplace_back(type, category, seq_no,
								notify_sem);
		}
		pthread_mutex_unlock(&notify_list_lock);
		return rc;
	} /* set_notify() */

	/**
	 * Extract message having the specified type, category, and seq_no
	 * from the queue.
	 */
	int get_message(rdma_msg_type type, rdma_msg_cat category,
					    rdma_msg_seq_no seq_no,
					    M* msg_ptr)
	{
		int rc;
		pthread_mutex_lock(&message_queue_lock);
		auto it = find_if(begin(message_queue), end(message_queue),
			[type, category, seq_no](const M& msg)
			{
				return (msg.type == type) &&
				       (msg.category == category) &&
				       (msg.seq_no == seq_no);
			});
		if (it == end(message_queue)) {
			ERR("Message (type='%s',0x%X, cat='%s',0x%X, seq_no=0x%X) not found!\n",
			type_name(type), type, cat_name(category), category, seq_no);
			rc = -1;
		} else {
			/* Copy message to caller's message buffer */
			memcpy(msg_ptr, &(*it), sizeof(*it));

			/* Remove from queue */
			message_queue.erase(it);
			DBG("(type='%s',0x%X, cat='%s',0x%X, seq_no=0x%X) removed\n",
			type_name(type), type, cat_name(category), category, seq_no);
			rc = 0;
		}
		pthread_mutex_unlock(&message_queue_lock);
		return rc;
	} /* get_message() */

protected:
	/**
	 * Clean up action specific to application using this Rx engine
	 */
	virtual void cleanup()
	{
		HIGH("cleanup in rx_engine (base class)\n");
	} /* cleanup() */

	/**
	 * Mark engines as dead, and trigger engine cleanup code.
	 */
	void die()
	{
		HIGH("Dying...\n");
		stop_worker_thread = true;
		is_dead = true;
		tx_eng->set_isdead();	// Kill corresponding tx_engine too
		sem_post(engine_cleanup_sem);
	} /* die() */

	vector<M>		message_queue;
	pthread_mutex_t		message_queue_lock;
	msg_processor<T, M>	&message_processor;
	vector<notify_param> 	notify_list;
	pthread_mutex_t		notify_list_lock;
	shared_ptr<T>		client;
	tx_engine<T, M>		*tx_eng;
	bool			is_dead;
	pthread_t		rx_work_thread;
	bool			worker_is_dead;
	bool			stop_worker_thread;
	sem_t			*engine_cleanup_sem;
}; /* rx_engine */

#endif

