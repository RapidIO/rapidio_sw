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

#include <queue>
#include <thread>
#include <vector>

#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>

#include "liblog.h"

#include "rdma_msg.h"

using std::queue;
using std::thread;
using std::vector;

template <typename T, typename M>
class rx_engine {

public:
	rx_engine() : client(nullptr) {
		thread worker_thread(&rx_engine::worker, this);
		sem_init(&start, 0, 0);
	}

	void set_client(T* client) {
		this->client = client;

		/* Make sure client is not null so we don't segfault */
		if (client == nullptr) {
			CRIT("'client' is null\n");
			abort();
		}

		/* Client has been given a value. OK to start receiving */
		sem_post(&start);
	} /* set_client() */

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

private:
	struct notify_param {

		notify_param(uint32_t type, uint32_t category, uint32_t seq_no) :
			type(type), category(category), seq_no(seq_no),
			notify_sem(notify_sem)
		{}

		bool operator ==(const notify_param& other) {
			return (other.type == this->type) &&
			       (other.category == this->category) &&
			       (other.seq_no == this->seq_no);
		}

		uint32_t type;
		uint32_t category;
		uint32_t seq_no;
		sem_t	 *notify_sem;
	};

	void worker() {
		sem_wait(&start);

		while(1) {
			M	*msg;
			size_t	received_len;

			client->get_recv_buffer((void **)&msg);
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
							    msg->seq_no));
				if (it != end(notify_list))
					/* Found match, post semaphore */
					sem_post(it->notify_sem);
				else {
					CRIT("Non-matching API type 0x%X\n",
								msg->type);
				}
			} else if (msg->category == RDMA_REQ_RESP) {
				/* FIXME: Unimplemented */
				message_queue.push(msg);
			}
		}
	}
	queue<M*>	message_queue;
	vector<notify_param> notify_list;
	T*		client;
	sem_t		start;
};

#endif

