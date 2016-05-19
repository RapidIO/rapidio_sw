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

#include <string>
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

using std::string;
using std::list;
using std::vector;
using std::shared_ptr;

/* Notification parameters. */
struct notify_param {

	notify_param(rdma_msg_type type, rdma_msg_cat category, uint32_t seq_no,
					shared_ptr<sem_t> notify_sem) :
		type(type), category(category), seq_no(seq_no),
		notify_sem(notify_sem)
	{}

	bool operator ==(const notify_param& other)
	{
		return (other.type == this->type) &&
		       (other.category == this->category) &&
		       (other.seq_no == this->seq_no);
	}

	bool identical(rdma_msg_type type, rdma_msg_cat category, uint32_t seq_no,
			sem_t *notify_sem)
	{
		return (type == this->type) &&
		       (category == this->category) &&
		       (seq_no == this->seq_no) &&
		       (notify_sem == this->notify_sem.get());
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

	string			name;
	volatile bool 		*stop_worker_thread;
	pthread_mutex_t		*message_queue_lock;
	vector<M>		*message_queue;
	T			*client;
	bool 			*is_dead;
	bool 			*worker_is_dead;
	sem_t			*engine_cleanup_sem;
	sem_t			*notify_list_sem;
	vector<notify_param> 	*notify_list;
	msg_processor<T, M>	&message_processor;
	tx_engine<T, M>		*tx_eng;
};

template <typename T, typename M>
void *rx_worker_thread_f(void *arg)
{
	rx_work_thread_info<T,M> *wti = (rx_work_thread_info<T,M> *)arg;

	string name			    = wti->name;
	volatile bool *stop_worker_thread   = wti->stop_worker_thread;
	pthread_mutex_t	*message_queue_lock = wti->message_queue_lock;
	vector<M> 	*message_queue 	    = wti->message_queue;
	T* client 			    = wti->client;
	bool *is_dead			    = wti->is_dead;
	bool *worker_is_dead		    = wti->worker_is_dead;
	sem_t *notify_list_sem   	    = wti->notify_list_sem;
	vector<notify_param> 	*notify_list= wti->notify_list;
	msg_processor<T, M> &message_processor = wti->message_processor;
	tx_engine<T, M>		*tx_eng	    = wti->tx_eng;

	M	*msg = NULL;
	client->get_recv_buffer((void **)&msg);

	DBG("'%s': Started rx_worker_thread_f()\n", name.c_str());

	while(1) {
		size_t	received_len = 0;

		/* Always flush buffer to ensure no leftover data
		 * from prior messages */
		client->flush_recv_buffer();

		/* Wait for new message to arrive */
		DBG("'%s': Waiting for new message to arrive...\n", name.c_str());
		int rc = client->receive(&received_len);
		if (rc) {
			/* If we fail to receive, the engine dies, whatever the reason! */
			if (rc == EINTR) {
				if (*stop_worker_thread) {
					WARN("'%s': pthread_kill() called from destructor\n",
							name.c_str());
					break;
				} else {
					WARN("'%s': pthread_kill() called during shutdown!\n",
							name.c_str());
					break;
				}
			} else {
				ERR("'%s': Failed to receive. Reason UNKNOWN!\n",
							name.c_str());
				break;
			}
		} else if (received_len == 0) {
			WARN("'%s': Other side has disconnected\n", name.c_str());
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
			DBG("'%s': Got category=0x%" PRIx64 ",'%s'\n",
							name.c_str(),
							msg->category,
							cat_name(msg->category));

			if (msg->category == RDMA_CALL) {
				/* There is never a notification set for 
				 * SERVER_DISCONNECT_MS_ACK responses.
				 */
				if ((SERVER_DISCONNECT_MS_ACK == msg->type)
					&& !msg->seq_no) 
					continue;
				/* If there is a notification set for the
				 * message then act on it. */
				sem_wait(notify_list_sem);
				auto it = find(begin(*notify_list),
						end(*notify_list),
						notify_param(msg->type,
							     msg->category,
							     msg->seq_no,
							     nullptr));
				if (it != end(*notify_list)) {
					/* Found! Queue copy of message & post semaphore */
					DBG("'%s': Found message of type '%s',0x%X, seq_no=0x%X\n",
						name.c_str(),
						type_name(it->type),
						it->type,
						it->seq_no);
					pthread_mutex_lock(message_queue_lock);
					message_queue->push_back(std::move(*msg));
					pthread_mutex_unlock(message_queue_lock);

					/* Post the notification semaphore */
					sem_post(it->notify_sem.get());

					/* Remove notify entry from notify list */
					notify_list->erase(it);
				} else {
					CRIT("'%s': Non-matching API type('%s',0x%X) seq_no(0x%X)\n",
							name.c_str(),
							type_name(msg->type),
							msg->type,
							msg->seq_no);
				}
				sem_post(notify_list_sem);
			} else if (msg->category == RDMA_REQ_RESP) {
				/* Process request/resp by forwarding to message processor */
				rc = message_processor.process_msg(msg, tx_eng);
				if (rc) {
					ERR("'%s': Failed to process message, rc = 0x%X\n",
									name.c_str(), rc);
				}
			} else {
				CRIT("'%s': Unhandled category = 0x%" PRIx64 "\n",
						name.c_str(), msg->category);
				CRIT("'%s': type='%s',0x%X\n",
						name.c_str(),
						type_name(msg->type),
						msg->type);
				abort();
			}
		}
	} // END while(1)

	*worker_is_dead = true;
	*is_dead = true;
	tx_eng->set_isdead();	// Kill corresponding tx_engine too
	sem_post(wti->engine_cleanup_sem);
	DBG("'%s': Exiting %s\n", name.c_str(), __func__);
	pthread_exit(0);
}

template <typename T, typename M>
class rx_engine {

public:
	rx_engine(const char *name, shared_ptr<T> client,
			msg_processor<T, M> &message_processor,
			tx_engine<T, M> *tx_eng, sem_t *engine_cleanup_sem) :
		name(name), message_processor(message_processor), client(client),
		tx_eng(tx_eng), is_dead(false), worker_is_dead(false),
		stop_worker_thread(false), engine_cleanup_sem(engine_cleanup_sem)
	{
		if (sem_init(&notify_list_sem, 0, 1)) {
			CRIT("'%s': Failed to init notify_list_sem\n",name);
			throw -1;
		}

		if (pthread_mutex_init(&message_queue_lock, NULL)) {
			CRIT("'%s': Failed to init message_queue_lock\n", name);
			throw -1;
		}

		tx_eng->set_rx_eng(this);

		try {
			wti = make_unique<rx_work_thread_info<T,M>>(message_processor);
		}
		catch(...) {
			CRIT("'%s': Failed to create work thread info\n", name);
			throw -2;
		}
		wti->name		= name;
		wti->stop_worker_thread = &stop_worker_thread;
		wti->message_queue_lock = &message_queue_lock;
		wti->message_queue	= &message_queue;
		wti->client 		= client.get();
		wti->is_dead		= &is_dead;
		wti->engine_cleanup_sem = engine_cleanup_sem;
		wti->worker_is_dead	= &worker_is_dead;
		wti->notify_list	= &notify_list;
		wti->notify_list_sem	= &notify_list_sem;
		wti->tx_eng		= tx_eng;

		auto rc = pthread_create(&rx_work_thread,
					 NULL,
					 &rx_worker_thread_f<T,M>,
					 (void *)wti.get());
		if (rc) {
			CRIT("'%s': Failed to start work_thread: %s\n",
							name, strerror(errno));
			throw -2;
		}

		DBG("'%s' created\n", this->name.c_str());
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
			DBG("'%s': Stopping worker thread\n", name.c_str());
			pthread_kill(rx_work_thread, SIGUSR1);
			pthread_join(rx_work_thread, NULL);
			DBG("'%s': rx_work_thread terminated.\n", name.c_str());
		}

		// Just in case there is a junk pointer to this object after destruction
	
		tx_eng = NULL;
		engine_cleanup_sem = NULL;
		wti->engine_cleanup_sem = NULL;
		wti.reset();
		sem_destroy(&notify_list_sem);
	} /* dtor */

	bool isdead() const { return is_dead; }

	/**
	 * Set the RX engine to post notify_sem when a message of the specified
	 * type, category, and seq_no is received by this rx_engine.
	 */
	void set_notify(rdma_msg_type type, rdma_msg_cat category, uint32_t seq_no,
						shared_ptr<sem_t> notify_sem)
	{

		sem_wait(&notify_list_sem);
		DBG("'%s': type='%s',0x%X, category='%s',0x%X, seq_no=0x%X\n",
				name.c_str(),
				type_name(type),
				type,
				cat_name(category),
				category,
				seq_no);

		/* We should not be setting the same notification twice */
		int rc = count_if(begin(notify_list),
			       end(notify_list),
			       [type, category, seq_no, notify_sem](notify_param& np)
			       { return np.identical(type, category, seq_no, notify_sem.get());});
		if (rc != 0) {
			ERR("'%s': Duplicate notify entry ignored!\n",
								name.c_str());
		} else {
			notify_list.emplace_back(type, category, seq_no,
								notify_sem);
		}
		sem_post(&notify_list_sem);
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
			ERR("'%s': Message (type='%s',0x%X, cat='%s',0x%X, seq_no=0x%X) not found!\n",
			name.c_str(), type_name(type), type, cat_name(category), category, seq_no);
			rc = -1;
		} else {
			/* Copy message to caller's message buffer */
			memcpy(msg_ptr, &(*it), sizeof(*it));

			/* Remove from queue */
			message_queue.erase(it);
			DBG("'%s': (type='%s',0x%X, cat='%s',0x%X, seq_no=0x%X) removed\n",
			name.c_str(), type_name(type), type, cat_name(category), category, seq_no);
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
		HIGH("'%s': cleanup in rx_engine (base class)\n", name.c_str());
	} /* cleanup() */

	/**
	 * Mark engines as dead, and trigger engine cleanup code.
	 */
	void die()
	{
		HIGH("'%s': Dying...\n", name.c_str());
		stop_worker_thread = true;
		is_dead = true;
		tx_eng->set_isdead();	// Kill corresponding tx_engine too
		sem_post(engine_cleanup_sem);
	} /* die() */

	unique_ptr<rx_work_thread_info<T,M>> wti;
	string			name;
	vector<M>		message_queue;
	pthread_mutex_t		message_queue_lock;
	msg_processor<T, M>	&message_processor;
	vector<notify_param> 	notify_list;
	sem_t			notify_list_sem;
	shared_ptr<T>		client;
	volatile tx_engine<T, M>*tx_eng;
	bool			is_dead;
	pthread_t		rx_work_thread;
	bool			worker_is_dead;
	bool			stop_worker_thread;
	volatile sem_t		*engine_cleanup_sem;
}; /* rx_engine */

#endif

