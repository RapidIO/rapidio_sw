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

#include <stdio.h>
#include <assert.h>

#include <stdexcept>

#ifdef RDMA_LL
  #include "liblog.h"
#endif

#include "rskt_shim2.h"

#include "rapidio_mport_mgmt.h"
#include "librskt.h"
#include "librskt_list.h"
#include "librskt_info.h"
#include "librdma.h"

#define RSKT_DEFAULT_DAEMON_SOCKET      3333

void shim_rskt_init()
{
#ifdef RDMA_LL
  rdma_log_init("rskt_shim2.txt", 1);
#endif

  const char* cRDMA_LL = getenv("RDMA_LL");
  if (cRDMA_LL != NULL) {
    int temp = atoi(cRDMA_LL);
    if (temp < RDMA_LL_CRIT) temp = RDMA_LL_CRIT - 1;
    if (temp > RDMA_LL)      temp = RDMA_LL;
    g_level = temp;
  }

  int rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
  if (rc) {
    static char tmp[129] = {0};
    snprintf(tmp, 128, "RSKT Shim: librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
    throw std::runtime_error(tmp);
  }
}

rskt_h shim_rskt_socket()
{
  return rskt_create_socket();
}

int shim_rskt_connect(rskt_h sock, uint16_t destid, uint16_t port)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;

  struct rskt_sockaddr sock_addr;

  sock_addr.ct = destid;
  sock_addr.sn = port;

  return rskt_connect(r_sock, &sock_addr);
}

int shim_rskt_listen(rskt_h sock, int max_backlog)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;
  return rskt_listen(r_sock, max_backlog);
}
int shim_rskt_bind(rskt_h sock, const uint16_t destid /*=0*/, const uint16_t port)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;
  struct rskt_sockaddr sock_addr;

  sock_addr.ct = destid;
  sock_addr.sn = port;

  return rskt_bind(r_sock, &sock_addr);
}

int shim_rskt_accept(rskt_h listen_sock, rskt_h *accept_socket, uint16_t* remote_destid, uint16_t* remote_port)
{
  assert(listen_sock);
  assert(accept_socket);
  struct rskt_sockaddr sock_addr; memset(&sock_addr, 0, sizeof(sock_addr));

  int rc = rskt_accept(listen_sock, accept_socket, &sock_addr);
  if (rc) return rc;

  if (remote_destid != NULL) *remote_destid = sock_addr.ct;
  if (remote_port != NULL)   *remote_port   = sock_addr.sn;
  return 0;
}

int shim_rskt_close(rskt_h sock)
{
  assert(sock);
  return rskt_close(sock);
}

int shim_rskt_read(rskt_h sock, void* data, const int data_len)
{
  assert(sock);
  assert(data);

  int rc = 0;
  do {
    rc = rskt_read(sock, data, data_len);
  } while (rc == -ETIMEDOUT);

  return rc;
}

extern "C" int get_avail_bytes(struct rskt_buf_hdr volatile *hdr, uint32_t buf_sz);

int shim_rskt_get_avail_bytes(rskt_h sock)
{
  assert(sock);
  struct rskt_socket_t* skt = rsktl_sock_ptr(&lib.skts, sock);

  if (skt == NULL) // socket closed?
    return -1;

  return get_avail_bytes(skt->hdr, skt->buf_sz);
}

int shim_rskt_write(rskt_h sock, void* data, const int data_len)
{
  assert(sock);
  assert(data);

  return rskt_write(sock, data, data_len);
}
