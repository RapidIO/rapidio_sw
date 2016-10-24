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
 * This is a simplistic WWW client over RapidIO+RSKT.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <semaphore.h>
#include <pthread.h>

#ifdef RDMA_LL
  #include "liblog.h"
#endif

#include "rapidio_mport_mgmt.h"
#include "librskt.h"
#include "librdma.h"

#define RX_SIZE  16*1024

#define RSKT_DEFAULT_DAEMON_SOCKET  3333

#define RSKT_DEFAULT_MAX_BACKLOG          50

static int RSKT_PORT = 80;

static rskt_h g_comm_sock;

static int g_wait = 0;
static uint16_t g_my_destid = 0xFFFF;

/** 
 * \brief display usage information for the RSKT client
 */
void usage()
{
  printf("rskt_tun [-S] -d<did> [-w] [-p <port>] [-l <lev>] -h\n");
  printf("-d<did>    : Destination ID of node running rskt_wwwserver.\n");
  printf("-p<port>   : Destination RSKT port of rskt_wwwserver.\n");
  printf("-w         : Add 1s delay before calling rskt_close.\n");
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

int setup_rskt_cli(uint16_t destid)
{
  g_comm_sock = rskt_create_socket();

  assert(destid != 0xFFFF);

  if (!g_comm_sock) {
    fprintf(stderr, "Create socket failed: %s\n", strerror(errno));
    return 0;
  }

  struct rskt_sockaddr sock_addr;

  sock_addr.ct = destid;
  sock_addr.sn = RSKT_PORT;

  int rc = rskt_connect(g_comm_sock, &sock_addr);
  if (rc) {
    fprintf(stderr, "Connect to %u on port %d failed\n", destid, RSKT_PORT);
    rskt_close(g_comm_sock);
    return 0;
  }

  return 1;
}

extern "C" int get_avail_bytes(struct rskt_buf_hdr volatile *hdr, uint32_t buf_sz);

int main(int argc, char *argv[])
{
  int rc = 0;

  if (argc < 2) {
    puts("Insufficient arguments. Must specify <destid>");
    usage();
    return 0;
  }

#ifdef RDMA_LL
  rdma_log_init("rskt_wwwclient.txt", 1);
#endif

  uint16_t destid = 0xFFFF;

  int c;
  while ((c = getopt(argc, argv, "whd:p:l:")) != -1) {
    switch (c) {
      case 'd': destid = atoi(optarg); break;
      case 'p': RSKT_PORT = atoi(optarg); break;
      case 'w': g_wait = 1; break;
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

  /** Check entered parameters */
  if (destid == 0xFFFF) {
    puts("Error. Must specify <destid>");
    usage();
    return 1;
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

  printf("my_destid=%u remote_destid=%u\n", g_my_destid, destid);

  /** Initialize RSKT library */
  rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
  if (rc) {
    fprintf(stderr, "librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
    return 2;
  }
  
  if (! setup_rskt_cli(destid)) { rc = 3; goto done; }
 
  {{
    char req[] = "GET / HTTP/1.1\r\n\r\n";
    char resp[RX_SIZE + 0x10] = {0};
    rskt_write(g_comm_sock, req, strlen(req));

#if 0
    struct rskt_socket_t* skt = (struct rskt_socket_t*)g_comm_sock->skt;
    assert(skt);

    int avb = 0;
    uint32_t count = 0;
    while((avb = get_avail_bytes(skt->hdr, skt->buf_sz)) == 0) {
      usleep(1);
      count++;
    }
    printf("get_avail_bytes indicated %d (non-zero) after %uus\n", avb, count);
#endif

    do {
      rc = rskt_read(g_comm_sock, resp, RX_SIZE);
    } while (rc == -ETIMEDOUT);
    if (rc < 0) {
      fprintf(stderr, "rskt_read failed %d: %s\n", rc, strerror(errno));
      rc = 4; goto done;
    }

    resp[sizeof(resp) - 1 ] = '\0';
    printf("Got back: [%s]\n", resp);
  }}

done:
  if (g_comm_sock != LIBRSKT_H_INVALID) {
    if (g_wait) sleep(1);
    printf("Close RSKT\n");
    rskt_close(g_comm_sock);
  }

  librskt_finish();

  _exit(rc);
} 
