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
#ifndef RSKT_SOCK_H_
#define RSKT_SOCK_H_

#include <cstdint>
#include <cstring>
#include <cassert>
#include <cerrno>

#include <iostream>
#include <iomanip>
#include <iterator>

#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librdma.h"
#include "liblog.h"
#include "rskts_info.h"

using std::iterator;
using std::cout;
using std::endl;
using std::ostream_iterator;
using std::setfill;
using std::hex;
using std::setw;

#define RSKT_MAX_SEND_BUF_SIZE		16*1024	/* 16KB */
#define RSKT_MAX_RECV_BUF_SIZE		16*1024	/* 16KB */
#define RSKT_DEFAULT_SEND_BUF_SIZE	4096
#define RSKT_DEFAULT_RECV_BUF_SIZE	4096
#define RSKT_DEFAULT_BACKLOG		  50

struct rskt_exception {
	rskt_exception(const char *msg) : err(msg) {}

	const char *err;
};

class rskt_base {

public:
	/* Return pointer to pre-allocated send buffer */
	void get_send_buffer(void **buf)
	{
		*buf = (void *)send_buf;
	} /* get_send_buffer() */

	/* Return pointer to pre-allocated recv buffer */
	void get_recv_buffer(void **buf)
	{
		*buf = (void *)recv_buf;
	} /* request_recv_buffer() */

	void flush_send_buffer()
	{
		memset(send_buf, 0, send_size);
	}

	void flush_recv_buffer()
	{
		memset(recv_buf, 0, recv_size);
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
			copy(buffer +  offset,
			     buffer + offset + chars_per_line,
			     ostream_iterator<int>(cout, "-"));

			cout << endl;
		}
	}

protected:
	rskt_base(const char *name, uint32_t send_size, uint32_t recv_size) :
		name(name),
		send_size(send_size),
		recv_size(recv_size)
	{
		/* Specified send_size must be within limit */
		if (send_size > RSKT_MAX_SEND_BUF_SIZE) {
			throw rskt_exception("Send buffer size is > RSKT_MAX_SEND_BUF_SIZE");
		}

		/* Specified recv_size must be within limit */
		if (recv_size > RSKT_MAX_RECV_BUF_SIZE) {
			throw rskt_exception("Receive buffer size is > RSKT_MAX_RECV_BUF_SIZE");
		}

		/* Allocate the buffer and check for alloc failures */
		try {
			send_buf = new uint8_t[send_size];
			recv_buf = new uint8_t[recv_size];
		}
		catch(std::bad_alloc& ba) {
			throw rskt_exception("Bad allocation for recv_buf or send_buf\n");
		}
		DBG("send_size = %u, recv_size = %u\n", this->send_size, this->recv_size);

		/* Initialize buffers to 0 -- for Valgrind */
		flush_send_buffer();
		flush_recv_buffer();
	}

	~rskt_base()
	{
		/* Delete send buffer */
		if (send_buf)
			delete[] send_buf;

		/* Delete recv buffer */
		if (recv_buf)
			delete[] recv_buf;
	}

	int send(rskt_h socket, size_t size)
	{
		if (size > send_size) {
			ERR("Data is too large (%u) for send buffer (%u)\n",
					size, send_size);
			return -1;
		}

		int rc = rskt_write(socket, send_buf, size);
		if (rc) {
			ERR("rskt_write failed for '%s': rc = %d\n", name, rc);
			return rc;
		}
		return 0;
	} /* send() */

	/* Receive bytes to 'recv_buf' on specified socket. 'size'
	 * specifies requested number of bytes. Return code gives
	 * actual number of bytes (if > 0) */
	int receive(rskt_h socket, size_t size)
	{
		if (size > recv_size) {
			ERR("Receive buffer (%u) can't hold %u bytes\n",
					size, send_size);
			return -1;
		}
		int rc = rskt_read(socket, recv_buf, size);
		if (rc < 0) {
			if (errno != ETIMEDOUT) {
				ERR("rskt_read failed for '%s': rc = %d\n", name, rc);
			}
		}
		return rc;
	} /* receive() */

	const char *name;

private:
	uint32_t send_size;
	uint32_t recv_size;
	uint8_t *send_buf;
	uint8_t *recv_buf;
}; /* rskt_base */

class rskt_server : public rskt_base {

public:
	rskt_server(const char *name,
		    int socket_number,
		    int max_backlog = RSKT_DEFAULT_BACKLOG,
		    uint32_t send_size = RSKT_DEFAULT_SEND_BUF_SIZE,
		    uint32_t recv_size = RSKT_DEFAULT_RECV_BUF_SIZE) :
	rskt_base(name, send_size, recv_size),
	listen_socket(0),
	accept_socket(0),
	max_backlog(max_backlog),
	is_only(true),	/* Start as is_only unless we father children via 'accept' */
	is_parent(false),
	is_child(false)
	{
		/* Create listen socket */
		listen_socket = rskt_create_socket();
		if (!listen_socket) {
			CRIT("'%s': Failed to create listen socket\n", name);
			throw rskt_exception("Failed to create listen socket");
		}

		/* Form address from CT(0) and socket number */
		sock_addr.ct = 0;
		sock_addr.sn = socket_number;

		/* Bind listen socket to address */
		int rc = rskt_bind(listen_socket, &sock_addr);
		if (rc) {
			ERR("'%s': Failed to bind listen socket to address: %s\n",
					name, strerror(-rc));
			throw rskt_exception("Failed to bind listen socket");
		}

		/* Enable listening on socket */
		rc = rskt_listen(listen_socket, max_backlog);
		if (rc) {
			ERR("'%s': Failed to listen: %s\n",
					name, strerror(-rc));
			throw rskt_exception("Failed to listen on socket");
		}
	} /* rskt_server() */

	/* Construct from an accept socket */
	rskt_server(const char *name,
		    rskt_h accept_socket,
		    uint32_t send_size = RSKT_DEFAULT_SEND_BUF_SIZE,
		    uint32_t recv_size = RSKT_DEFAULT_RECV_BUF_SIZE) :
		rskt_base(name, send_size, recv_size),
		listen_socket(0),
		accept_socket(accept_socket),
		max_backlog(0),
		is_only(false),
		is_parent(false),
		is_child(true)	/* A child since accept_socket is given in ctor */
	{
	} /* rskt_server() */

	~rskt_server()
	{
		int rc;

		/* If we are a parent, i.e. we provided the socket to a caller
		 * then the caller owns that socket. We don't close or destroy it.
		 */
		if ((is_only || is_child) && accept_socket) {
			rc = rskt_close(accept_socket);
			if (rc) {
				WARN("'%s': Failed to close accept_socket rc = %d\n", rc);
			}
		}

		/* is_only or is_parent has a non-zero listen socket.
		 * The child's listen socket is initialized to 0 */
		if (listen_socket) {
			rc = rskt_close(listen_socket);
			if (rc) {
				WARN("'%s': Failed to close listen_socket rc = %d\n", rc);
			}
		}

		/* Destroy accept sockets for is_only and is_child objects */
		if ((is_only || is_child) && accept_socket)
			rskt_destroy_socket(&accept_socket);

		/* is_only or is_parent has a non-zero listen socket.
		 * The child's listen socket is initialized to 0 */
		if (listen_socket)
			rskt_destroy_socket(&listen_socket);

	} /* ~rskt_server() */

	int accept(rskt_h *acc_socket = nullptr)
	{
		/* A child server should ONLY do sending and receiving */
		if (is_child) {
			CRIT("'%s' is a child rskt_server. Can't accept\n", name);
			return -1;
		}

		/* Create accept socket */
		accept_socket = rskt_create_socket();
		if (!accept_socket) {
			ERR("'%s': Failed to create accept socket\n", name);
			return -1;
		}

		/* Accept connections */
		int rc = rskt_accept(listen_socket, accept_socket, &sock_addr);
		if (rc) {
			ERR("'%s': Failed in rskt_accept: %s\n", name, strerror(errno));
			return rc;
		}

		/* Return socket to caller if address of socket variable provided.
		 * If that is the case then we are a parent not an only object */
		if (acc_socket != nullptr) {
			*acc_socket = accept_socket;
			is_only = false;
			is_parent = true;
		}

		return 0;
	} /* accept() */

	/* Receive bytes to 'recv_buf' */
	int receive(size_t size)
	{
		return rskt_base::receive(accept_socket, size);
	} /* receive() */

	/* Send bytes from 'send_buf' */
	int send(size_t size)
	{
		return rskt_base::send(accept_socket, size);
	} /* send() */

private:
	rskt_h	listen_socket;
	rskt_h	accept_socket;
	struct rskt_sockaddr sock_addr;
	int	max_backlog;
	bool	is_only;	/* This is the only object; it accepts and rx/tx */
	bool	is_parent;	/* This is the parent of a child that rx/tx */
	bool	is_child;	/* This is a child that ONLY does rx/tx */
}; /* rskt_server */

class rskt_client : public rskt_base {

public:
	/* Constructor */
	rskt_client(const char *name,
		    uint32_t send_size = RSKT_DEFAULT_SEND_BUF_SIZE,
		    uint32_t recv_size = RSKT_DEFAULT_RECV_BUF_SIZE) :
		rskt_base(name, send_size, recv_size),
		client_socket(0)
	{
		/* Create listen socket */
		client_socket = rskt_create_socket();
		if (!client_socket) {
			CRIT("'%s': Failed to create client_socket\n", name);
			throw rskt_exception("Failed to create client socket");
		}
	} /* Constructor */

	/* Constructor for creating a client based on the client_socket of another */
	rskt_client(const char *name,
		    rskt_h client_socket,
		    uint32_t send_size = RSKT_DEFAULT_SEND_BUF_SIZE,
		    uint32_t recv_size = RSKT_DEFAULT_RECV_BUF_SIZE) :
		rskt_base(name, send_size, recv_size),
		client_socket(client_socket)
	{
	}

	~rskt_client()
	{
		if (client_socket) {
			int rc = rskt_close(client_socket);
			if (rc) {
				WARN("'%s': Failed to close client_socket rc = %d\n", rc);
			}
			rskt_destroy_socket(&client_socket);
		}
	} /* ~rskt_client() */

	int connect(uint32_t destid, int socket_number)
	{
		/* Prepare address from parameters */
		struct rskt_sockaddr sock_addr;
		sock_addr.ct = destid;
		sock_addr.sn = socket_number;

		int rc = rskt_connect(client_socket, &sock_addr);
		if (rc) {
			CRIT("'%s': Failed to connect to destid(%u) on socknum(%u)\n",
					name, destid, socket_number);
		}
		return rc;
	} /* connect() */

	/* Receive bytes to 'recv_buf' */
	int receive(uint32_t size)
	{
		return rskt_base::receive(client_socket, size);
	} /* receive() */

	/* Send bytes from 'send_buf' */
	int send(uint32_t size)
	{
		return rskt_base::send(client_socket, size);
	} /* send() */

private:
	rskt_h	client_socket;
};

#endif /* RSKT_SOCK_H_ */
