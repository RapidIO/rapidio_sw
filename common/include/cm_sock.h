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
#ifndef CM_SOCK_H
#define CM_SOCK_H

#include <stdint.h>
#include <errno.h>

#include <cstring>
#include <iostream>
#include <iomanip>
#include <iterator>

#include "rapidio_mport_lib.h"
#include "liblog.h"

#define	CM_MSG_OFFSET 20
#define	CM_BUF_SIZE	4096

using namespace std;

struct cm_exception {
	cm_exception(const char *msg) : err(msg) {}

	const char *err;
};

class cm_base {

public:
	/* Return pointer to pre-allocated send buffer */
	void get_send_buffer(void **buf)
	{
		*buf = (void *)(send_buf + CM_MSG_OFFSET);
	} /* get_send_buffer() */

	/* Return pointer to pre-allocated recv buffer */
	void get_recv_buffer(void **buf)
	{
		*buf = (void *)(recv_buf + CM_MSG_OFFSET);
	} /* request_recv_buffer() */

	void flush_send_buffer()
	{
		memset(send_buf + CM_MSG_OFFSET, 0, CM_BUF_SIZE - CM_MSG_OFFSET);
	}

	void flush_recv_buffer()
	{
		memset(recv_buf + CM_MSG_OFFSET, 0, CM_BUF_SIZE - CM_MSG_OFFSET);
	}

	void dump_send_buffer()
	{
		dump_buffer(send_buf);
	}

	void dump_recv_buffer()
	{
		dump_buffer(recv_buf);
	}

private:
	void dump_buffer(uint8_t *buffer)
	{
		unsigned offset = 0;
		const uint8_t chars_per_line = 32;

		cout << hex << setfill('0') << setw(2);

		for (unsigned i = 0; i < 4; i++, offset += chars_per_line) {
			copy(buffer + CM_MSG_OFFSET + offset,
			     buffer + CM_MSG_OFFSET + offset + chars_per_line,
			     ostream_iterator<int>(cout, "-"));

			cout << endl;
		}
	}

protected:
	cm_base(const char *name, int mport_id, uint8_t mbox_id, uint16_t channel) :
		name(name), mport_id(mport_id), mbox_id(mbox_id), channel(channel),
		mailbox(0),
		send_buf(new uint8_t[CM_BUF_SIZE]),
		recv_buf(new uint8_t[CM_BUF_SIZE])
	{
		/* Initialize buffers to 0 -- for Valgrind */
		memset(send_buf, 0, CM_BUF_SIZE);
		memset(recv_buf, 0, CM_BUF_SIZE);
	}

	~cm_base()
	{
		/* Delete send buffer */
		if (send_buf)
			delete[] send_buf;

		/* Delete recv buffer */
		if (recv_buf)
			delete[] recv_buf;
	}

	/* Returns 0 if successful, < 0 otherwise */
	int create_mailbox()
	{
		return riodp_mbox_create_handle(mport_id, mbox_id, &mailbox);
	}

	/* Returns 0 if successful, < 0 otherwise */
	int close_mailbox()
	{
		return riodp_mbox_destroy_handle(&mailbox);
	}

	/* Send CM_BUF_SIZE bytes from 'send_buf' on specified socket */
	int send(riodp_socket_t socket)
	{
		int rc;
		rc = riodp_socket_send(socket, (void *)send_buf, CM_BUF_SIZE);
		if (rc) {
			ERR("riodp_socket_send failed for '%s': %s\n", name, strerror(rc));
			return -1;
		}
		return 0;
	} /* send() */

	/* Receive bytes to 'recv_buf' on specified socket */
	int receive(riodp_socket_t socket)
	{
		int rc = 0;
		rc = riodp_socket_receive(socket, (void **)&recv_buf, CM_BUF_SIZE, 0);
		if (rc) {
			ERR("riodp_socket_receive failed for '%s': %s\n",
						name, strerror(errno));
		}
		return rc;
	} /* receive() */

	/* If returns ETIME then it timed out. 0 means success, anything else
	 * is an error. */
	int timed_receive(riodp_socket_t socket, uint32_t timeout_ms)
	{
		return riodp_socket_receive(socket, (void **)&recv_buf, CM_BUF_SIZE, timeout_ms);
	} /* timed_receive() */

	const char *name;
	int mport_id;
	uint8_t mbox_id;
	uint16_t channel;
	riodp_mailbox_t mailbox;
private:
	uint8_t *send_buf;
	uint8_t *recv_buf;
}; /* cm_base */

class cm_server : public cm_base {

public:
	cm_server(const char *name, int mport_id, uint8_t mbox_id, uint16_t channel) :
		cm_base(name, mport_id, mbox_id, channel),
		listen_socket(0), accept_socket(0), accepted(false)
	{
		int rc;

		/* Create mailbox, throw exception if failed */
		DBG("name = %s, mport_id = %d, mbox_id = %u, channel = %u\n",
			name, mport_id, mbox_id, channel);
		if (create_mailbox()) {
			CRIT("Failed to create mailbox for '%s'\n", name);
			throw cm_exception("Failed to create mailbox");
		}

		/* Create listen socket, throw exception if failed */
		if (riodp_socket_socket(mailbox, &listen_socket)) {
			CRIT("Failed to create listen socket for '%s'\n",name);
			close_mailbox();
			throw cm_exception("Failed to create listen socket");
		}
		DBG("listen_socket = 0x%X\n", listen_socket);

		/* Bind listen socket, throw exception if failed */
		rc = riodp_socket_bind(listen_socket, channel);
		if (rc) {
			CRIT("Failed to bind listen socket for '%s': %s\n",
							name, strerror(errno));
			riodp_socket_close(&listen_socket);
			close_mailbox();
			throw cm_exception("Failed to bind listen socket");
		}
		DBG("Listen socket bound\n");

		/* Prepare listen socket */
		rc = riodp_socket_listen(listen_socket);
		if(rc) {
			ERR("Failed in riodp_socket_listen() for '%s': %s\n",
							name, strerror(rc));
			riodp_socket_close(&listen_socket);
			close_mailbox();
			throw cm_exception("Failed to listen on socket");
		}
		DBG("Listen successful on '%s'\n", name);
	} /* cm_server() */

	/* Construct from accept socket. Other attributes are unused */
	cm_server(const char *name, riodp_socket_t accept_socket) :
		cm_base(name, 0, 0, 0),
		accept_socket(accept_socket)
	{
	}

	~cm_server()
	{
		/* Close accept socket, if open */
		DBG("accept_socket = 0x%X\n", accept_socket);
		if (accept_socket && accepted)
			if (riodp_socket_close(&accept_socket)) {
				WARN("Failed to close accept socket for '%s': %s\n",
							name, strerror(errno));
			}

		/* Close listen socket, opened during construction */
		DBG("Closing listen_socket = 0x%X\n", listen_socket);
		if (listen_socket)
			if (riodp_socket_close(&listen_socket)) {
				WARN("Failed to close listen socket: for '%s': %s\n",
							name, strerror(errno));
			}

		/* Destroy mailbox handle, opened during construction */
		if (mailbox) {
			DBG("Destroying mailbox\n");
			if (close_mailbox()) {
				WARN("Failed to close mailbox for '%s'\n", name);
			}
		}
	} /* ~cm_server() */

	riodp_socket_t get_accept_socket() { return accept_socket; }

	/* Accept connection from client */
	int accept(riodp_socket_t *acc_socket)
	{
		accepted = false;

		/* Create accept socket */
		if( riodp_socket_socket(mailbox, &accept_socket)) {
			ERR("Failed to create accept socket for '%s'\n", name);
			return -1;
		}
		DBG("Created accept_socket = 0x%X\n", accept_socket);

		/* Wait for connection request from a client */
		int rc = 0;
		do {
			/* riodp_socket_accept() returns errno where appropriate */
			rc = riodp_socket_accept(listen_socket, &accept_socket, 3*60*1000);
		} while (rc == ETIME);

		if (rc) {	/* failed */
			if (rc == EINTR) {
				INFO("accept() aborted due to killing thread\n");
			} else {
				ERR("Failed to accept connections for '%s' (0x%X)\n",
								name, accept_socket);
			}
		} else {
			if (acc_socket)
				*acc_socket = accept_socket;
			accepted = true;
		}

		return rc;
	} /* accept() */

	int accept()
	{
		return accept(NULL);
	} /* accept() */

	/* Receive bytes to 'recv_buf' */
	int receive()
	{
		return cm_base::receive(accept_socket);
	} /* receive() */

	/* Receive bytes to 'recv_buf' with timeout */
	int timed_receive(uint32_t timeout_ms)
	{
		return cm_base::timed_receive(accept_socket, timeout_ms);
	} /* receive() */

	/* Send bytes from 'send_buf' */
	int send()
	{
		return cm_base::send(accept_socket);
	} /* send() */

private:
	riodp_socket_t listen_socket;
	riodp_socket_t accept_socket;
	bool	accepted;
}; /* cm_server */

class cm_client : public cm_base {

public:
	cm_client(const char *name, int mport_id, uint8_t mbox_id, uint16_t channel) :
		cm_base(name, mport_id, mbox_id, channel), client_socket(0)
	{
		/* Create mailbox, throw exception if failed */
		DBG("name = %s, mport_id = %d, mbox_id = %u, channel = %u\n",
					name, mport_id, mbox_id, channel);

		if (create_mailbox()) {
			CRIT("Failed to create mailbox for '%s'\n", name);
			throw cm_exception("Failed to create mailbox");
		}

		/* Create client socket, throw exception if failed */
		if (riodp_socket_socket(mailbox, &client_socket)) {
			CRIT("Failed to create socket for '%s'\n", name);
			close_mailbox();
			throw cm_exception("Failed to create client socket");
		}
		DBG("client_socket = 0x%X\n", client_socket);
	} /* Constructor */

	/* construct from client socket only */
	cm_client(const char *name, riodp_socket_t socket) :
		cm_base(name, 0, 0, 0), client_socket(socket)
	{
	}

	riodp_socket_t get_socket() const { return client_socket; }

	~cm_client()
	{
		/* Close client socket */
		DBG("client_socket = 0x%X\n", client_socket);
		if (riodp_socket_close(&client_socket)) {
			WARN("Failed to close client socket for '%s': %s\n",
							name, strerror(errno));
		}
		DBG("Client socket closed\n");
		/* Destroy mailbox handle, opened during construction */
		if (close_mailbox()) {
			WARN("Failed to close mailbox for '%s'\n", name);
		}
		DBG("Mailbox destroyed\n");
	} /* Destructor */

	/* Connect to server specified by RapidIO destination ID */
	int connect(uint16_t destid, riodp_socket_t *socket)
	{
		int rc = riodp_socket_connect(client_socket,
					      destid,
					      mbox_id,
					      channel);
		if (rc == EADDRINUSE) {
			INFO("Requested channel already in use, reusing..");
		} else if (rc) {
			ERR("riodp_socket_connect failed for '%s':  %s\n",
							name, strerror(errno));
			ERR("channel = %d, mailbox = %d, destid = 0x%X\n",
						channel, mbox_id, destid);
			return -1;
		}
		/* Return the socket, now that we know it works */
		if (socket)
			*socket = this->client_socket;
		return 0;
	} /* connect() */

	/* Connect to the server specified by RapidIO destination ID */
	int connect(uint16_t destid)
	{
		return connect(destid, NULL);
	} /* connect() */

	/* Send bytes from 'send_buf' */
	int send()
	{
		return cm_base::send(client_socket);
	} /* send() */

	/* Receive bytes to 'recv_buf' */
	int receive()
	{
		return cm_base::receive(client_socket);
	} /* receive() */

	/* Receive bytes to 'recv_buf' with timeout */
	int timed_receive(uint32_t timeout_ms)
	{
		return cm_base::timed_receive(client_socket, timeout_ms);
	} /* receive() */

private:
	riodp_socket_t client_socket;
}; /* cm_client */

#endif

