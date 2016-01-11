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

template <typename T, typename M>
class rx_engine {

public:
	rx_engine(T *client, msg_processor<T, M> &message_processor,
			tx_engine<T, M> *tx_eng) :
		message_processor(message_processor), client(client), tx_eng(tx_eng)
	{
		thread worker_thread(&rx_engine::worker, this);
	} /* ctor */

	/**
	 * Set the RX engine to post notify_sem when a message of the specified
	 * type, category, and seq_no is received by this rx_engine.
	 */
	int set_notify(uint32_t type, uint32_t category, uint32_t seq_no,
							sem_t *notify_sem)
	{
		/* We should not be setting the same notification twice */
		int rc = count(begin(notify_list),
			       end(notify_list),
			       notify_param(type, category, seq_no, notify_sem));
		if (rc != 0) {
			ERR("Duplicate notify entry ignored!\n");
		} else {
			notify_list.emplace_back(type, category, seq_no,
								notify_sem);
		}
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
		auto it = find_if(begin(message_queue), end(message_queue),
			[&](M msg)
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
		}
		return rc;
	} /* get_message() */

private:
	/* Notification parameters. */
	struct notify_param {

		notify_param(uint32_t type, uint32_t category, uint32_t seq_no,
							sem_t *notify_sem) :
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
		sem_t	 	*notify_sem;
	};

	/**
	 * Worker thread for receiving both API calls and requests/responses
	 */
	void worker()
	{
		M	*msg;
		client->get_recv_buffer((void **)&msg);

		while(1) {
			size_t	received_len;

			/* Always flush buffer to ensure no leftover data
			 * from prior messages */
			client->flush_recv_buffer();

			/* Wait for new message to arrive */
			int rc = client->receive(&received_len);
			if (rc != 0) {
				ERR("Failed to receive, rc = %d: %s\n",
							rc, strerror(errno));
			} else if (msg->category == RDMA_LIB_DAEMON_CALL) {
				/* If there is a notification set for the
				 * message then act on it. */
				auto it = find(begin(notify_list),
					       end(notify_list),
					       notify_param(msg->type,
							    msg->category,
							    msg->seq_no,
							    nullptr));
				if (it != end(notify_list)) {
					/* Found! Queue copy of message & post semaphore */
					message_queue.push_back(*msg);
					sem_post(it->notify_sem);
				} else {
					CRIT("Non-matching API type 0x%X\n",
								msg->type);
				}
			} else if (msg->category == RDMA_REQ_RESP) {
				/* Process request/resp by forwarding to message processor */
				rc = message_processor.process_msg(msg, tx_eng);
				if (rc) {
					ERR("Failed to process message, rc = %d\n", rc);
				}
			}
		}
	}

	list<M>			message_queue;
	msg_processor<T, M>	&message_processor;
	vector<notify_param> 	notify_list;
	T*			client;
	tx_engine<T, M>		*tx_eng;
};

#endif

