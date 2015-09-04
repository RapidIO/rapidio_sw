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

#include <stdint.h>
#include <errno.h>

#include <cstring>
#include <iostream>
#include <iomanip>
#include <iterator>

#include <rapidio_mport_mgmt.h>

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
		send_buf(new uint8_t[send_size]),
		recv_buf(new uint8_t[recv_size])
	{
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

	int send(rskt_h socket, uint32_t size)
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

	/* Receive bytes to 'recv_buf' on specified socket */
	int receive(rskt_h socket, uint32_t size)
	{
		if (size > send_size) {
			ERR("Receive buffer (%u) can't hold %u bytes\n",
					size, send_size);
			return -1;
		}
		int rc = rskt_read(socket, recv_buf, size);
		if (rc) {
			ERR("rskt_read failed for '%s': rc = %d\n", name, rc);
			return rc;
		}
		return 0;
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
	max_backlog(max_backlog)
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
	}

	~rskt_server()
	{
		int rc;

		rc = rskt_close(listen_socket);
		if (rc) {
			WARN("'%s': Failed to close listen_socket rc = %d\n", rc);
		}

		rc = rskt_close(accept_socket);
		if (rc) {
			WARN("'%s': Failed to close accept_socket rc = %d\n", rc);
		}
		rskt_destroy_socket(&listen_socket);

		rskt_destroy_socket(&accept_socket);

	}

private:
	int max_backlog;
	struct rskt_sockaddr sock_addr;
	rskt_h	listen_socket;
	rskt_h	accept_socket;
}; /* rskt_server */


#endif /* RSKT_SOCK_H_ */
