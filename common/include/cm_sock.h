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
#include <fcntl.h>
#include <unistd.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <exception>

#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "liblog.h"

constexpr auto CM_MSG_OFFSET = 20;
constexpr auto CM_BUF_SIZE   = 4*1024;
constexpr auto CM_PAYLOAD_SIZE = CM_BUF_SIZE - CM_MSG_OFFSET;

using std::exception;

class cm_exception : public exception {
public:
	cm_exception(const char *msg) : err(msg) {}
	virtual const char *what() const throw()
	{
		return err;
	}
private:
	const char *err;
};

class cm_base {

public:
	virtual int send(size_t len = CM_BUF_SIZE) = 0;

	virtual int receive(size_t *rcvd_len = nullptr) = 0;

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
		memset(send_buf + CM_MSG_OFFSET, 0, CM_PAYLOAD_SIZE);
	}

	void flush_recv_buffer()
	{
		memset(recv_buf + CM_MSG_OFFSET, 0, CM_PAYLOAD_SIZE);
	}

	void dump_send_buffer()
	{
		dump_buffer(send_buf);
	}

	void dump_recv_buffer()
	{
		dump_buffer(recv_buf);
	}

	void dump_buffer(uint8_t *buffer)
	{
		unsigned row, col;
		const uint8_t numbers_per_line = 32;
		const uint8_t max_rows = 4;

		for (row = 0; row < max_rows; row++) {
			for (col = 0; col < numbers_per_line - 1; col++) {
				printf("0x%02X-", *(buffer + CM_MSG_OFFSET + row*numbers_per_line + col));
			}
			printf("0x%02X\n", *(buffer + CM_MSG_OFFSET + row*numbers_per_line + col));
		}
	}

protected:
	cm_base(const char *name, int mport_id,
		uint8_t mbox_id, uint16_t channel, bool *shutting_down) :
		name(name), mport_id(mport_id), mbox_id(mbox_id), channel(channel),
		shutting_down(shutting_down), mailbox(0),
		send_buf(new uint8_t[CM_BUF_SIZE]),
		recv_buf(new uint8_t[CM_BUF_SIZE])
	{
		/* Initialize buffers to 0 -- for Valgrind */
		memset(send_buf, 0, CM_BUF_SIZE);
		memset(recv_buf, 0, CM_BUF_SIZE);

		gdb = is_debugger_present();
	}

	virtual ~cm_base()
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
		return riomp_sock_mbox_create_handle(mport_id, mbox_id, &mailbox);
	}

	/* Returns 0 if successful, < 0 otherwise */
	int close_mailbox()
	{
		return riomp_sock_mbox_destroy_handle(&mailbox);
	}

	/* Uses specified buffer and length */
	int send_buffer(riomp_sock_t socket, void *buffer, size_t len)
	{
		int rc;
		if (len > CM_PAYLOAD_SIZE) {
			ERR("'%s' failed in send() due to large message size(%d)\n",
								name, len);
			rc = -1;
		} else {
			/* Buffer was specified, copy contents after CM header */
			if (buffer != nullptr)
				memcpy(send_buf + CM_MSG_OFFSET, buffer, len);
			/* else the message is at CM_MSG_OFFSET in recv_buf */
			rc = riomp_sock_send(socket, send_buf, CM_BUF_SIZE);
			if (rc) {
				ERR("riomp_sock_send failed for '%s': %s\n",
								name, strerror(rc));
			}
		}
		return rc;
	} /* send_buffer() */

	/* Send CM_BUF_SIZE bytes from 'send_buf' on specified socket */
	int send(riomp_sock_t socket, size_t len)
	{
		/* Must pass nullptr as the buffer */
		return send_buffer(socket, nullptr, len);
	} /* send() */

	/* Receive bytes to 'recv_buf' on specified socket */
	int receive(riomp_sock_t socket, size_t *rcvd_len)
	{
		/* Passing nullptr means use 'recv_buf' */
		return receive_buffer(socket, nullptr, rcvd_len);
	} /* receive() */

	/* Receive bytes to 'buffer' on specified socket */
	int receive_buffer(riomp_sock_t socket, void *buffer, size_t *rcvd_len)
	{
		return timed_receive_buffer(socket, 0, buffer, rcvd_len);
	} /* receive_buffer() */

	/* If returns ETIME then it timed out. 0 means success,
	 * EINTR means thread was killed (if not in gdb mode AND the)
	 * shutting_down flag is set) anything else is an error. */
	int timed_receive_buffer(riomp_sock_t socket, uint32_t timeout_ms,
			void *buffer, size_t *rcvd_len)
	{
		int rc;

		do {
			rc = riomp_sock_receive(socket, (void **)&recv_buf, CM_BUF_SIZE,
					timeout_ms);
		} while (rc && (errno==EINTR) && gdb && !*shutting_down);
		if (rc) {
			if (errno == EINTR) {
				WARN("%s: Abort receive() for pthread_kill()\n",
									name);
				rc = EINTR;
			} else {
				WARN("%s: receive() failed, errno = %d: %s\n",
						name, errno, strerror(errno));
			}
		}

		/* riomp_sock_receive(0 doesn't return actual bytes read, so
		 * the bytes read are considedered to be the max payload size */
		if (rcvd_len != nullptr)
			*rcvd_len = CM_PAYLOAD_SIZE;

		/* Only show buffer contents if we actually receive */
		if (rc == 0) {
#ifdef EXTRA_DEBUG
			DBG("recv_buf[0] = 0x%" PRIx64 "\n", *(uint64_t *)recv_buf);
			DBG("recv_buf[1] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)recv_buf + 8));
			DBG("recv_buf[2] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)recv_buf + 16));
			DBG("recv_buf[3] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)recv_buf + 24));
			DBG("recv_buf[4] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)recv_buf + 32));
			DBG("recv_buf[5] = 0x%" PRIx64 "\n", *(uint64_t *)((uint8_t *)recv_buf + 40));
#endif
		}

		/* A buffer was provided, copy the data to it */
		if (buffer != nullptr)
			memcpy(buffer, recv_buf + CM_MSG_OFFSET, CM_PAYLOAD_SIZE);

		return rc;
	} /* timed_receive_buffer() */

	int timed_receive(riomp_sock_t socket, uint32_t timeout)
	{
		return timed_receive_buffer(socket, timeout, nullptr, nullptr);
	} /* timed_receive() */

	const char *name;
	int mport_id;
	uint8_t mbox_id;
	uint16_t channel;
	bool	*shutting_down;
	riomp_mailbox_t mailbox;
	bool	gdb;

private:

	int is_debugger_present(void)
	{
	    char buf[1024];
	    int debugger_present = 0;

	    int status_fd = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
	    if (status_fd == -1)
	        return 0;

	    ssize_t num_read = read(status_fd, buf, sizeof(buf));

	    if (num_read > 0)
	    {
	        static const char TracerPid[] = "TracerPid:";
	        char *tracer_pid;

	        buf[num_read] = 0;
	        tracer_pid    = strstr(buf, TracerPid);
	        if (tracer_pid)
	            debugger_present = !!atoi(tracer_pid + sizeof(TracerPid) - 1);
	    }

	    close(status_fd);

	    return debugger_present;
	}

	uint8_t *send_buf;
	uint8_t *recv_buf;
}; /* cm_base */

class cm_server : public cm_base {

public:
	cm_server(const char *name, int mport_id, uint8_t mbox_id, uint16_t channel,
		  bool *shutting_down) :
		cm_base(name, mport_id, mbox_id, channel, shutting_down),
		listen_socket(0), accept_socket(0), accepted(false)
	{
		int rc;

		/* Create mailbox, throw exception if failed */
		DBG("name = %s, mport_id = %d, mbox_id = %u, channel = %u\n",
			name, mport_id, mbox_id, channel);
		rc = create_mailbox();
		if (rc) {
			CRIT("Failed to create mailbox for '%s'\n", name);
			if (rc == -1) {
				CRIT("Failed in riomp_sock_mbox_init(): %s\n",
						strerror(errno));
			} else if (rc == -2) {
				CRIT("Failed to allocate mailbox handle\n");
			}
			throw cm_exception("Failed to create mailbox");
		}

		/* Create listen socket, throw exception if failed */
		if (riomp_sock_socket(mailbox, &listen_socket)) {
			CRIT("Failed to create listen socket for '%s'\n",name);
			close_mailbox();
			throw cm_exception("Failed to create listen socket");
		}
		DBG("listen_socket = 0x%X\n", listen_socket);

		/* Bind listen socket, throw exception if failed */
		rc = riomp_sock_bind(listen_socket, channel);
		if (rc) {
			CRIT("Failed to bind listen socket for '%s': %s\n",
							name, strerror(errno));
			riomp_sock_close(&listen_socket);
			close_mailbox();
			throw cm_exception("Failed to bind listen socket");
		}
		DBG("Listen socket bound\n");

		/* Prepare listen socket */
		rc = riomp_sock_listen(listen_socket);
		if(rc) {
			ERR("Failed in riomp_sock_listen() for '%s': %s\n",
							name, strerror(rc));
			riomp_sock_close(&listen_socket);
			close_mailbox();
			throw cm_exception("Failed to listen on socket");
		}
		DBG("Listen successful on '%s'\n", name);
	} /* cm_server() */

	/* Construct from accept socket. Other attributes are unused */
	cm_server(const char *name, riomp_sock_t accept_socket,
		  bool *shutting_down) :
		cm_base(name, 0, 0, 0, shutting_down),
		listen_socket(0), accept_socket(accept_socket), accepted(false)
	{
		DBG("'%s': accept_socket = 0x%X\n", name, accept_socket);
	}

	~cm_server()
	{
		DBG("%s called for '%s'\n", __func__, name);

		/* Close socket, if open */
		if (accept_socket && accepted)
			if (riomp_sock_close(&accept_socket)) {
				ERR("Failed to close accept socket for '%s': %s\n",
							name, strerror(errno));
			}

		/* Close listen socket, opened during construction */
		if (listen_socket)
			if (riomp_sock_close(&listen_socket)) {
				ERR("Failed to close listen socket: for '%s': %s\n",
							name, strerror(errno));
			}

		/* Destroy mailbox handle, opened during construction */
		if (mailbox) {
			DBG("'%s': Destroying mailbox\n", name);
			if (close_mailbox()) {
				ERR("Failed to close mailbox for '%s'\n", name);
			}
		}
	} /* ~cm_server() */

	riomp_sock_t get_accept_socket() { return accept_socket; }

	/* Accept connection from client */
	int accept(riomp_sock_t *acc_socket)
	{
		accepted = false;

		/* Create accept socket */
		if( riomp_sock_socket(mailbox, &accept_socket)) {
			ERR("Failed to create accept socket for '%s'\n", name);
			return -1;
		}
		DBG("Created accept_socket = 0x%X\n", accept_socket);

		/* Wait for connection request from a client */
		int rc = 0;
		do {
			/* riomp_sock_accept() returns errno where appropriate */
			rc = riomp_sock_accept(listen_socket, &accept_socket, 3*60*1000);
			/* If ETIME, retry. If EINTR & NOT running gdb then that means
			 * the thread was killed. Exit with EINTR so the calling thread
			 * can clean up the socket and exit.
			 */
		} while (rc && ((errno == ETIME) || ((errno == EINTR) && gdb && !*shutting_down)));

		if (rc) {	/* failed */
			if (errno == EINTR) {
				WARN("terminated due to killing thread\n");
				rc = EINTR;
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
	int receive(size_t *rcvd_len = nullptr)
	{
		return cm_base::receive(accept_socket, rcvd_len);
	} /* receive() */

	int send_buffer(void *buffer, size_t len = CM_PAYLOAD_SIZE)
	{
		DBG("Called\n");
		return cm_base::send_buffer(accept_socket, buffer, len);
	} /* send_buffer() */

	/* Receive bytes to 'recv_buf' with timeout */
	int timed_receive(uint32_t timeout_ms)
	{
		return cm_base::timed_receive(accept_socket, timeout_ms);
	} /* receive() */

	/* Send bytes from 'send_buf' */
	int send(size_t len = CM_PAYLOAD_SIZE)
	{
		return cm_base::send(accept_socket, len);
	} /* send() */

private:
	riomp_sock_t listen_socket;
	riomp_sock_t accept_socket;
	bool	accepted;
}; /* cm_server */

class cm_client : public cm_base {

public:
	cm_client(const char *name, int mport_id, uint8_t mbox_id,
		  uint16_t channel, bool *shutting_down) :
		cm_base(name, mport_id, mbox_id, channel, shutting_down),
		server_destid(0xFFFF), client_socket(0)
	{
		/* Create mailbox, throw exception if failed */
		DBG("name = %s, mport_id = %d, mbox_id = %u, channel = %u\n",
					name, mport_id, mbox_id, channel);

		if (create_mailbox()) {
			CRIT("Failed to create mailbox for '%s'\n", name);
			throw cm_exception("Failed to create mailbox");
		}

		/* Create client socket, throw exception if failed */
		if (riomp_sock_socket(mailbox, &client_socket)) {
			CRIT("Failed to create socket for '%s'\n", name);
			close_mailbox();
			throw cm_exception("Failed to create client socket");
		}
		DBG("client_socket = 0x%X\n", client_socket);
	} /* Constructor */

	/* construct from client socket only */
	cm_client(const char *name, riomp_sock_t socket, bool *shutting_down) :
		cm_base(name, 0, 0, 0, shutting_down),
		server_destid(0xFFFF), client_socket(socket)
	{
	}

	riomp_sock_t get_socket() const { return client_socket; }

	~cm_client()
	{
		DBG("%s called for '%s'\n", __func__, name);

		/* Close client socket */
		if (riomp_sock_close(&client_socket)) {
			WARN("Failed to close client socket for '%s': %s\n",
							name, strerror(errno));
		}

		/* Destroy mailbox handle, opened during construction */
		if (close_mailbox()) {
			WARN("Failed to close mailbox for '%s'\n", name);
		}

	} /* Destructor */

	/* Connect to server specified by RapidIO destination ID */
	int connect(uint16_t destid, riomp_sock_t *socket)
	{
		int rc = riomp_sock_connect(client_socket,
					      destid,
					      mbox_id,
					      channel);
		if (rc == EADDRINUSE) {
			INFO("Requested channel already in use, reusing..");
		} else if (rc) {
			ERR("riomp_sock_connect failed for '%s':  %s\n",
							name, strerror(errno));
			ERR("channel = %d, mailbox = %d, destid = 0x%X\n",
						channel, mbox_id, destid);
			return -1;
		}
		/* Return the socket, now that we know it works */
		if (socket)
			*socket = this->client_socket;

		server_destid = destid;
		return 0;
	} /* connect() */

	/* Connect to the server specified by RapidIO destination ID */
	int connect(uint16_t destid)
	{
		return connect(destid, NULL);
	} /* connect() */

	/* Send bytes from 'send_buf' */
	int send(size_t len = CM_PAYLOAD_SIZE)
	{
		return cm_base::send(client_socket, len);
	} /* send() */

	/* Send from 'buffer' */
	int send_buffer(void *buffer, size_t len = CM_PAYLOAD_SIZE)
	{
		DBG("Calling cm_base::send_buffer\n");
		return cm_base::send_buffer(client_socket, buffer, len);
	} /* send_buffer() */

	/* Receive bytes to 'recv_buf' */
	int receive(size_t *rcvd_len = nullptr)
	{
		return cm_base::receive(client_socket, rcvd_len);
	} /* receive() */

	/* Receive bytes to 'recv_buf' with timeout */
	int timed_receive(uint32_t timeout_ms)
	{
		return cm_base::timed_receive(client_socket, timeout_ms);
	} /* receive() */

	uint16_t server_destid;
private:
	riomp_sock_t client_socket;
}; /* cm_client */

#endif

