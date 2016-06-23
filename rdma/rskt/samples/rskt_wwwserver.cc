/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

/*
 * This is a simplistic WWW server over RapidIO+RSKT.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

#ifdef RDMA_LL
  #include "liblog.h"
#endif

#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"

#define RX_SIZE  16*1024

#define RSKT_DEFAULT_DAEMON_SOCKET  3333

#define RSKT_DEFAULT_MAX_BACKLOG          50

static int RSKT_PORT = 80;

static uint16_t g_my_destid = 0xFFFF;

void usage()
{
  printf("rskt_tun [-p <port>] [-l <lev>] -h\n");
  printf("-p<port>   : Destination RSKT port of rskt_server.\n");
  printf("-l<lev>    : Debug level\n");
  printf("               1 - No logs\n");
  printf("               2 - critical\n");
  printf("               3 - Errors and above\n");
  printf("               4 - Warnings and above\n");
  printf("               5 - High priority info and above\n");
  printf("               6 - Information logs and above\n");
  printf("               7 - Debug information and above\n");
  printf("-h         : Display this help message and exit.\n");
} /* usage() */

void* www_read_thr(void* arg);

int rskt_main_loop()
{
  rskt_h listen_sock = rskt_create_socket();
  if (!listen_sock) {
    fprintf(stderr, "Failed to create listen socket: %s\n", strerror(errno));
    return 0;
  }

  struct rskt_sockaddr sock_addr;

  sock_addr.ct = 0;
  sock_addr.sn = RSKT_PORT;

  int rc = rskt_bind(listen_sock, &sock_addr);
  if (rc) {
    fprintf(stderr, "Failed to bind listen socket, rc = %d: %s\n", rc, strerror(errno));
    rskt_close(listen_sock);
    return 0;
  }

  /** Set socket to listen for connection requests */
  rc = rskt_listen(listen_sock, RSKT_DEFAULT_MAX_BACKLOG);
  if (rc) {
    fprintf(stderr, "Failed to listen & set max backlog, rc = %d: %s\n", rc, strerror(errno));
    rskt_close(listen_sock);
    return 0;
  }

  /** Loop untill interrupted, doing the following */
  while (1) {
    /** - Create a new accept socket for the next connection */
    rskt_h accept_socket = rskt_create_socket();
    if (!accept_socket) {
      fprintf(stderr, "Cannot create accept socket, rc = %d: %s\n", rc, strerror(errno));
      rskt_close(accept_socket);
      rskt_close(listen_sock);
      return 0;
    }

    /** - Await connect requests from RSKT clients */
    rc = rskt_accept(listen_sock, accept_socket, &sock_addr);
    if (rc) {
      fprintf(stderr, "Failed in rskt_accept, rc = 0x%X, errno=%d: %s\n", rc, errno, strerror(errno));
      rskt_close(accept_socket);
      rskt_close(listen_sock);
      return 0;
    }

    pthread_t pt_wkr;
    if (pthread_create(&pt_wkr, NULL, www_read_thr, accept_socket) < 0) {
      fprintf(stderr, "pthread_create failed: %s\n", strerror(errno));
      return 0;
    }
  }

  rskt_close(listen_sock);

  /*NOTREACHED*/
  return 0;
}

void* www_read_thr(void* arg)
{
  assert(arg);
  rskt_h comm_sock = (rskt_h)arg;

  char recv_buf[RX_SIZE + 0x10] = {0};

  pthread_setname_np(pthread_self(), "WWW_RIO_RX");

  int rc = 0;

  do {
      rc = rskt_read(comm_sock, recv_buf, RX_SIZE);
  } while (rc == -ETIMEDOUT);
  if (rc < 0) {
    fprintf(stderr, "rskt_read failed %d: %s\n", rc, strerror(errno));
    return NULL;
  } 

  fprintf(stderr, "%s: REQ %d bytes.\n", __func__, rc);

  // Ignore request for now
  char resp[] = "Content/type: text/plain\r\n\r\nTest text from RSKT server.\r\n";

  rskt_write(comm_sock, resp, strlen(resp));

  rskt_close(comm_sock);

  fprintf(stderr, "%s: QUIT -- sent %lu bytes\n", __func__, strlen(resp));

  return NULL;
}

int main(int argc, char *argv[])
{
  int rc = 0;

#ifdef RDMA_LL
  rdma_log_init("rskt_wwwserver..txt", 1);
#endif

  int c;
  while ((c = getopt(argc, argv, "hp:l:")) != -1) {
    switch (c) {
      case 'p': RSKT_PORT = atoi(optarg); break;
      case 'h': usage(); exit(0); break;
      case 'l':
#ifdef RDMA_LL
                {{
                int temp = atoi(optarg);
                if (temp < RDMA_LL_CRIT) temp = RDMA_LL_CRIT - 1;
                if (temp > RDMA_LL)      temp = RDMA_LL;
                g_level = temp;
                }}
#endif
                break;
      default: usage(); exit(0); break;
    }
  }

  {{
    uint8_t   np = 8;
    uint32_t* dev_ids = NULL;
    if ((rc = riomp_mgmt_get_mport_list(&dev_ids, &np))) {
      fprintf(stderr, "riomp_mgmt_get_mport_list failed %d: %s\n", rc, strerror(errno));
      return 69;
    }
    /// \todo UNDOCUMENTED: Lower nibble has destid
    if (np == 0) { fprintf(stderr, "No mport instances found.\n"); return 69; }
    g_my_destid = dev_ids[0] & 0xFFFF;
    riomp_mgmt_free_mport_list(&dev_ids);
  }}

  printf("my_destid=%u port=%d\n", g_my_destid, RSKT_PORT);

  /** Initialize RSKT library */
  rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
  if (rc) {
    fprintf(stderr, "librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
    return 2;
  }
 
  rskt_main_loop(); 

  librskt_finish();

  _exit(rc);
} 
