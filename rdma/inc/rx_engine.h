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
#include "tx_engine.h"
#include "msg_processor.h"

using std::list;
using std::thread;
using std::vector;
using std::shared_ptr;

template <typename T, typename M>
class rx_engine {

public:
	rx_engine(shared_ptr<T> client, msg_processor<T, M> &message_processor,
			tx_engine<T, M> *tx_eng, sem_t *engine_cleanup_sem) :
		message_processor(message_processor), client(client), tx_eng(tx_eng),
		worker_thread(nullptr), is_dead(false), worker_is_dead(false),
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
		worker_thread = new thread(&rx_engine::worker, this);
		worker_thread->detach();
	} /* ctor */

	virtual ~rx_engine()
	{
		DBG("dtor\n");
		if (!worker_is_dead) {
			HIGH("Stopping worker thread\n");
			stop_worker_thread = true;
		}
		DBG("Deleting worker thread\n");
		delete worker_thread;
	} /* dtor */

	bool isdead() const { return is_dead; }

	/**
	 * Set the RX engine to post notify_sem when a message of the specified
	 * type, category, and seq_no is received by this rx_engine.
	 */
	int set_notify(uint32_t type, uint32_t category, uint32_t seq_no,
						shared_ptr<sem_t> notify_sem)
	{
		/* We should not be setting the same notification twice */
		pthread_mutex_lock(&notify_list_lock);
		int rc = count(begin(notify_list),
			       end(notify_list),
			       notify_param(type, category, seq_no, notify_sem));
		if (rc != 0) {
			ERR("Duplicate notify entry ignored!\n");
		} else {
			DBG("type=0x%X, category=0x%X, seq_no=0x%X\n",
					type, category, seq_no);
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
		auto rc = 0;
		pthread_mutex_lock(&message_queue_lock);
		auto it = find_if(begin(message_queue), end(message_queue),
			[type, category, seq_no](const M& msg)
			{
				return (msg.type == type) &&
				       (msg.category == category) &&
				       (msg.seq_no == seq_no);
			});
		if (it == end(message_queue)) {
			ERR("Message not found!\n");
			rc = -1;
		} else {
			/* Copy message to caller's message buffer */
			memcpy(msg_ptr, &(*it), sizeof(*it));

			/* Remove from queue */
			message_queue.erase(it);
			DBG("Message removed, now message_queue.size() = %u\n",
							message_queue.size());
		}
		pthread_mutex_unlock(&message_queue_lock);
		return rc;
	} /* get_message() */

protected:
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

	/**
	 * Clean up action specific to application using this Rx engine
	 */
	virtual void cleanup()
	{

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

	/**
	 * Worker thread for receiving both API calls and requests/responses
	 */
	void worker()
	{
		M	*msg;
		client->get_recv_buffer((void **)&msg);

		while(!stop_worker_thread) {
			size_t	received_len = 0;

			/* Always flush buffer to ensure no leftover data
			 * from prior messages */
			client->flush_recv_buffer();

			/* Wait for new message to arrive */
			DBG("Waiting for new message to arrive...\n");
			int rc = client->receive(&received_len);
			if (rc != 0) {
				CRIT("Failed to receive, rc = %d: %s\n",
							rc, strerror(errno));
				die();
			} else if (received_len > 0 ) {
				if (msg->category == RDMA_LIB_DAEMON_CALL) {
					DBG("Got RDMA_LIB_DAEMON_CALL\n");

					/* If there is a notification set for the
					 * message then act on it. */
					pthread_mutex_lock(&notify_list_lock);
					auto it = find(begin(notify_list),
							end(notify_list),
							notify_param(msg->type,
									msg->category,
									msg->seq_no,
									nullptr));
					if (it != end(notify_list)) {
						/* Found! Queue copy of message & post semaphore */
						DBG("Found message of type 0x%X, seq_no=0x%X\n",
									it->type, it->seq_no);
						pthread_mutex_lock(&message_queue_lock);
						message_queue.push_back(*msg);
						pthread_mutex_unlock(&message_queue_lock);

						/* Remove notify entry from notify list */
						notify_list.erase(it);

						/* Post the notification semaphore */
						sem_post(it->notify_sem.get());
					} else {
						CRIT("Non-matching API type(0x%X) seq_no(0x%X)\n",
								msg->type, msg->seq_no);
					}
					pthread_mutex_unlock(&notify_list_lock);
				} else if (msg->category == RDMA_REQ_RESP) {
					/* Process request/resp by forwarding to message processor */
					DBG("Got RDMA_REQ_RESP\n");
					rc = message_processor.process_msg(msg, tx_eng);
					if (rc) {
						ERR("Failed to process message, rc = %d\n", rc);
					}
				} else {
					CRIT("msg->category = 0x%X\n", msg->category);
					CRIT("mst->type = 0x%X\n", msg->type);
					abort();
				}
			} else if (received_len == 0) {
				CRIT("Other side has closed connection\n");
				cleanup();
				die();
			} else { /* received_len < 0 */
				assert(!"received_len < 0");
			}
		} /* while() */
		worker_is_dead = true;
		DBG("worker_thread exiting!\n");
	} /* worker() */

	vector<M>		message_queue;
	pthread_mutex_t		message_queue_lock;
	msg_processor<T, M>	&message_processor;
	vector<notify_param> 	notify_list;
	pthread_mutex_t		notify_list_lock;
	shared_ptr<T>		client;
	tx_engine<T, M>		*tx_eng;
	thread 			*worker_thread;
	bool			is_dead;
	bool			worker_is_dead;
	bool			stop_worker_thread;
	sem_t			*engine_cleanup_sem;
}; /* rx_engine */

#endif

