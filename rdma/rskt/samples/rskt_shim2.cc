#include <stdio.h>
#include <assert.h>

#include <stdexcept>

#ifdef RDMA_LL
  #include "liblog.h"
#endif

#include "rskt_shim2.h"

#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"

#define RSKT_DEFAULT_DAEMON_SOCKET      3333

static uint16_t g_my_destid = 0xFFFF;

void shim_rskt_init()
{
#ifdef RDMA_LL
  rdma_log_init("rskt_shim2.txt", 1);
#endif

  int rc = 0;
  {{
    uint8_t   np = 8;
    uint32_t* dev_ids = NULL;
    if ((rc = riomp_mgmt_get_mport_list(&dev_ids, &np))) {
      char tmp[129] = {0};
      snprintf(tmp, 128, "RSKt Shim: riomp_mgmt_get_mport_list failed %d: %s\n", rc, strerror(errno));
      throw std::runtime_error(tmp);
    }
    /// \todo UNDOCUMENTED: Lower nibble has destid
    if (np == 0) {
      char tmp[129] = {0};
      snprintf(tmp, 128, "RSKt Shim: No mport instances found.\n");
      throw std::runtime_error(tmp);
    }
    g_my_destid = dev_ids[0] & 0xFFFF;
    riomp_mgmt_free_mport_list(&dev_ids);
  }}

  rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
  if (rc) {
    static char tmp[129] = {0};
    snprintf(tmp, 128, "RSKt Shim: librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
    throw std::runtime_error(tmp);
  }
}

uint16_t shim_rskt_get_my_destid() { return g_my_destid; }

void* shim_rskt_socket()
{
  return rskt_create_socket();
}

int shim_rskt_connect(void* sock, uint16_t destid, uint16_t port)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;

  struct rskt_sockaddr sock_addr;

  sock_addr.ct = destid;
  sock_addr.sn = port;

  return rskt_connect(r_sock, &sock_addr);
}

int shim_rskt_listen(void* sock, int max_backlog)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;
  return rskt_listen(r_sock, max_backlog);
}
int shim_rskt_bind(void* sock, const uint16_t destid /*=0*/, const uint16_t port)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;
  struct rskt_sockaddr sock_addr;

  sock_addr.ct = destid;
  sock_addr.sn = port;

  return rskt_bind(r_sock, &sock_addr);
}

int shim_rskt_accept(void* listen_sock, void* accept_socket, uint16_t& remote_destid, uint16_t& remote_port)
{
  assert(listen_sock);
  assert(accept_socket);
  rskt_h l_sock = (rskt_h)listen_sock;
  rskt_h a_socket = (rskt_h)accept_socket;
  struct rskt_sockaddr sock_addr; memset(&sock_addr, 0, sizeof(sock_addr));

  int rc = rskt_accept(l_sock, a_socket, &sock_addr);
  if (rc) return rc;

  remote_destid = sock_addr.ct;
  remote_port   = sock_addr.sn;
  return 0;
}

int shim_rskt_close(void* sock)
{
  assert(sock);
  rskt_h r_sock = (rskt_h)sock;
  return rskt_close(r_sock);
}

int shim_rskt_read(void* sock, void* data, const int data_len)
{
  assert(sock);
  assert(data);
  rskt_h r_sock = (rskt_h)sock;
  assert(r_sock->skt);

  int rc = 0;
  do {
    rc = rskt_read(r_sock, data, data_len);
  } while (rc == -ETIMEDOUT);

  return rc;
}

int shim_rskt_write(void* sock, void* data, const int data_len)
{
  assert(sock);
  assert(data);
  rskt_h r_sock = (rskt_h)sock;
  assert(r_sock->skt);

  return rskt_write(r_sock, data, data_len);
}
