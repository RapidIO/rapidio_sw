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
#ifndef UNIX_SOCK_H
#define UNIX_SOCK_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

#include <iterator>
#include <iostream>
#include <iomanip>

#include "liblog.h"

/* Default paramters for RDMA */
#define UNIX_PATH_RDMA	"/var/tmp/rdma"
#define UNIX_SOCK_DEFAULT_BUFFER_SIZE	512
#define UNIX_SOCK_DEFAULT_BACKLOG	5

using std::iterator;
using std::cout;
using std::ostream_iterator;
using std::endl;
using std::setfill;
using std::setw;
using std::hex;

struct unix_sock_exception {
	unix_sock_exception(const char *msg) : err(msg) {}

	const char *err;
};

class unix_base {
public:
	/* Return pointer to pre-allocated send buffer */
	void get_send_buffer(void **buf)
	{
		pthread_mutex_lock(&send_buf_mutex);
		*buf = (void *)send_buf;
		pthread_mutex_unlock(&send_buf_mutex);
	} /* get_send_buffer() */

	/* Return pointer to pre-allocated recv buffer */
	void get_recv_buffer(void **buf)
	{
		pthread_mutex_lock(&recv_buf_mutex);
		*buf = (void *)recv_buf;
		pthread_mutex_unlock(&recv_buf_mutex);
	} /* request_recv_buffer() */

	void flush_send_buffer()
	{
		pthread_mutex_lock(&send_buf_mutex);
		memset(send_buf, 0, UNIX_SOCK_DEFAULT_BUFFER_SIZE);
		pthread_mutex_unlock(&send_buf_mutex);
	}

	void flush_recv_buffer()
	{
		pthread_mutex_lock(&recv_buf_mutex);
		memset(recv_buf, 0, UNIX_SOCK_DEFAULT_BUFFER_SIZE);
		pthread_mutex_unlock(&recv_buf_mutex);
	}

	void dump_send_buffer()
	{
		pthread_mutex_lock(&send_buf_mutex);
		dump_buffer(send_buf);
		pthread_mutex_unlock(&send_buf_mutex);
	}

	void dump_recv_buffer()
	{
		pthread_mutex_lock(&recv_buf_mutex);
		dump_buffer(recv_buf);
		pthread_mutex_unlock(&recv_buf_mutex);
	}

protected:
	unix_base(const char *name, const char *sun_path) :
		name(name),
		send_buf(new uint8_t[UNIX_SOCK_DEFAULT_BUFFER_SIZE]),
		recv_buf(new uint8_t[UNIX_SOCK_DEFAULT_BUFFER_SIZE])
	{
		/* Create listen socket */
		if( (the_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
			ERR("Failed to create socket: s\n", strerror(errno));
			throw unix_sock_exception("Failed to create socket");
		}

		/* Set up address parameters */
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, sun_path);
		addr_len = sizeof(addr);

		if (pthread_mutex_init(&send_buf_mutex, NULL)) {
			CRIT("Failed to initialized loc_mso_mutex: %s\n",
					strerror(errno));
			throw unix_sock_exception("Failed to init send_buf_mutex");
		}
		if (pthread_mutex_init(&recv_buf_mutex, NULL)) {
			CRIT("Failed to initialized loc_mso_mutex: %s\n",
					strerror(errno));
			throw unix_sock_exception("Failed to init recv_buf_mutex");
		}
	}

	/**
	 * When constructing a server using this constructor, the resulting
	 * server never calls accept() so it has no valid 'the_socket'.
	 * Initialize it to -1 here.
	 */
	unix_base(const char *name) :
		the_socket(-1),
		name(name),
		addr_len(0),
		send_buf(new uint8_t[UNIX_SOCK_DEFAULT_BUFFER_SIZE]),
		recv_buf(new uint8_t[UNIX_SOCK_DEFAULT_BUFFER_SIZE])
	{
		if (pthread_mutex_init(&send_buf_mutex, NULL)) {
			CRIT("Failed to initialized loc_mso_mutex: %s\n",
					strerror(errno));
			throw unix_sock_exception("Failed to init send_buf_mutex");
		}
		if (pthread_mutex_init(&recv_buf_mutex, NULL)) {
			CRIT("Failed to initialized loc_mso_mutex: %s\n",
					strerror(errno));
			throw unix_sock_exception("Failed to init recv_buf_mutex");
		}
	}

	~unix_base()
	{
		/* Delete send buffer */
		pthread_mutex_lock(&send_buf_mutex);
		if (send_buf)
			delete[] send_buf;
		pthread_mutex_unlock(&send_buf_mutex);

		/* Delete recv buffer */
		pthread_mutex_lock(&recv_buf_mutex);
		if (recv_buf)
			delete[] recv_buf;
		pthread_mutex_unlock(&recv_buf_mutex);

		/* For classes derived using the minimal constructor which
		 * does NOT create a socket, there is no socket to close since
		 * it is set to -1.
		 */
		if (the_socket != -1)
			close(the_socket);
	}

	/* Uses specified buffer */
	int send_buffer(int sock, void *buffer, size_t len)
	{
		int rc = 0;

		if (len > UNIX_SOCK_DEFAULT_BUFFER_SIZE) {
			ERR("'%s' failed in send() due to large message size\n", name);
			rc = -1;
		} else if (::send(sock, buffer, len, MSG_EOR) == -1) {
			ERR("Failed in send(): %s, errno=%d\n", strerror(errno), errno);
			rc = errno;
		}

		return rc;
	}

	/* Uses internal buffer */
	int send(int sock, size_t len)
	{
		int rc = 0;

		pthread_mutex_lock(&send_buf_mutex);
		rc = send_buffer(sock, send_buf, len);
		pthread_mutex_unlock(&send_buf_mutex);

		return rc;
	}

	int receive_buffer(int sock, void *buffer, size_t *rcvd_len)
	{
		int rc = 0;

		rc = ::recv(sock, buffer, UNIX_SOCK_DEFAULT_BUFFER_SIZE, 0);
		if (rc < 0) {
			ERR("'%s': failed in recv(): %s, errno = %d\n",
					name, strerror(errno), errno);
			rc = errno;
		} else {
			*rcvd_len = rc;
			rc = 0;
		}

		return rc;
	}

	int receive(int sock, size_t *rcvd_len)
	{
		int rc = 0;

		pthread_mutex_lock(&recv_buf_mutex);
		rc = receive_buffer(sock, recv_buf, rcvd_len);
		pthread_mutex_unlock(&recv_buf_mutex);

		return rc;
	}

	int		the_socket;	/* listen or client socket */
	const char 	*name;
	sockaddr_un	addr;
	socklen_t	addr_len;
	pthread_mutex_t send_buf_mutex;
	pthread_mutex_t recv_buf_mutex;

private:
	void dump_buffer(uint8_t *buffer)
	{
		unsigned offset = 0;
		const uint8_t chars_per_line = 32;

		cout << hex << setfill('0') << setw(2);

		for (unsigned i = 0; i < 4; i++, offset += chars_per_line) {
			copy(buffer + offset,
			     buffer + offset + chars_per_line,
			     ostream_iterator<int>(cout, "-"));

			cout << endl;
		}
	}

	uint8_t *send_buf;
	uint8_t *recv_buf;
}; /* unix_base */

class unix_server : public unix_base
{
public:
	unix_server(const char *name,
		    const char *sun_path = UNIX_PATH_RDMA,
		    int backlog = UNIX_SOCK_DEFAULT_BACKLOG)
	try : unix_base(name, sun_path), accept_socket(0), can_accept(true)
	{
		/* If file exists, delete it before binding */
		struct stat st;
		if (stat(UNIX_PATH_RDMA, &st) >= 0) {
			if (unlink(addr.sun_path) == -1) {
				WARN("'%s' failed to unlink '%s': '%s'\n",
						name, addr.sun_path, strerror(errno));
			}
		}

		/* Bind listen socket to the address */
		if ( bind(the_socket, (struct sockaddr *)&addr, addr_len) == -1) {
			ERR("'%s' failed to bind socket to address: %s\n",
						name, strerror(errno));
			throw unix_sock_exception("Failed to bind socket");
		}

		/* Listen */
		if (listen(the_socket, backlog) == -1) {
			ERR("'%s' failed to listen on socket: %s\n",
							name, strerror(errno));
			throw unix_sock_exception("Failed to listen on socket");
		}
	} /* constructor */
	catch(...)	/* Catch failures in unix_base::unix_base() */
	{
		throw unix_sock_exception("Failed in base constructor");
	}

	/* Minimal constructor for creating objects that ONLY have an
	 * accept socket. Such objects may receive and send but may NOT
	 * accept new connections. Deleting those objects does NOT kill
	 * the base class socket ('the_socket').
	 */
	unix_server(const char *name, int accept_socket)
	try : unix_base(name), accept_socket(accept_socket), can_accept(false)
	{
	}
	catch(...)	/* Catch failures in unix_base::unix_base() */
	{
		throw unix_sock_exception("Failed in base constructor");
	}

	~unix_server()
	{
		close(accept_socket);
	}

	int get_accept_socket() const { return accept_socket; }

	int accept()
	{
		if (!can_accept) {
			CRIT("'%s': Can't accept since created with wrong ctor\n");
			return -1;
		}

		/* The client address is not used for anything post the accept() */
		struct sockaddr_un	client_addr;
		socklen_t len = sizeof(client_addr);


		/* Accept connections */
		accept_socket = ::accept(the_socket, (struct sockaddr *)&client_addr, &len);
		if( accept_socket == -1) {
			ERR("'%s' failed in accept(): %s\n", name, strerror(errno));
			return errno;
		}

		return 0;
	}

	int send(size_t len)
	{
		return unix_base::send(accept_socket, len);
	}

	int receive(size_t *rcvd_len)
	{
		return unix_base::receive(accept_socket, rcvd_len);
	}

	int send_buffer(void *buffer, size_t len)
	{
		return unix_base::send_buffer(accept_socket, buffer, len);
	}

	int receive_buffer(void *buffer, size_t *rcvd_len)
	{
		return unix_base::receive_buffer(accept_socket, buffer, rcvd_len);
	}
private:
	int	accept_socket;
	bool	can_accept;	/* Objects created with the minimal constructor can't */
};

class unix_client : public unix_base
{
public:
	unix_client(const char *name = "client",
		    const char *sun_path = UNIX_PATH_RDMA)
	try : unix_base(name, sun_path)
	{
	}
	catch(...)	/* Catch failures in unix_base::unix_base() */
	{
		throw unix_sock_exception("Failed in base constructor");
	}

	~unix_client()
	{
	}

	int connect()
	{
		if (::connect(the_socket, (struct sockaddr *)&addr, addr_len) == -1) {
			ERR("%s: failed to connect: %s\n", name, strerror(errno));
		        return errno;
		}
		return 0;
	}

	int send(size_t len)
	{
		return unix_base::send(the_socket, len);
	}

	int receive(size_t *rcvd_len)
	{
		return unix_base::receive(the_socket, rcvd_len);
	}

	int send_buffer(void *buffer, size_t len)
	{
		return unix_base::send_buffer(the_socket, buffer, len);
	}

	int receive_buffer(void *buffer, size_t *rcvd_len)
	{
		return unix_base::receive_buffer(the_socket, buffer, rcvd_len);
	}
};

#endif
