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
 * This is a simplistic point-to-point IP bridging over RapidIO+RSKT.
 *
 * One endpoint is "server" [-S]. The other is "client".
 * Both endpoints must be told what is the destid of peer.
 * This example does NOT attempt to connect to all endpoints reported by libmport.
 *
 * To use with iperf on endpoints 5 & 6:
 *   ep5# ./rskt_tun -d 6 -S
 *   ## Local tun IP address is (autonconfigured) as 169.254.1.5
 *   ep6# ./rskt_tun -d 5
 *   ## Local tun IP address is (autonconfigured) as 169.254.1.6
 * In a new window:
 *   ep5# iperf -fMB -d -s
 *   ep6# iperf -fMB -c 169.254.1.5
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
#include <sys/epoll.h>
#include <net/if.h>
#include <linux/if_tun.h>

#ifdef RDMA_LL
  #include "liblog.h"
#endif

#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"

#include "tun_ipv4.h"

#define MTU_SIZE  16*1024

#define RSKT_DEFAULT_DAEMON_SOCKET  3333

#define RSKT_DEFAULT_MAX_BACKLOG          50

static int RSKT_PORT = 666;

static volatile int  g_quit = 0;
static sem_t         g_start_sem;
static volatile int  g_tun_fd = -1;
static volatile int  g_epoll_fd = -1;
static rskt_h        g_comm_sock;
static rskt_h        g_listen_sock;

enum { // powers of 2
  TUN_QUIT = 2,
  RIO_QUIT = 4,
};

struct {
  volatile uint64_t tun_rx_pkt_cnt;
  volatile uint64_t tun_rx_pkt_bytes;
  volatile uint64_t rio_rx_pkt_cnt;
  volatile uint64_t rio_rx_pkt_bytes;
  volatile uint64_t rio_rx_pkt_byte_sizes[MTU_SIZE+1];
} g_stats;

static uint16_t g_my_destid = 0xFFFF;

int setup_TUN(const uint16_t my_destid, const uint16_t destid, const int DESTID_TRANSLATE)
{
  char if_name[IFNAMSIZ] = {0};
  char Tap_Ifconfig_Cmd[257] = {0};
  int flags = IFF_TUN | IFF_NO_PI;

  int tun_fd = -1;
  char tun_name[129] = {0};

  // Initialize tun/tap interface
  if ((tun_fd = tun_alloc(if_name, flags)) < 0) {
    fprintf(stderr, "Error connecting to tun/tap interface %s!\n", if_name);
    goto error;
  }
  strncpy(tun_name, if_name, sizeof(tun_name)-1);

  {{
    const int flags = fcntl(tun_fd, F_GETFL, 0);
    fcntl(tun_fd, F_SETFL, flags | O_NONBLOCK);
  }}

  printf("%s: my_destid=%u destid=%u DESTID_TRANSLATE=%d if_name=%s\n", __func__, my_destid, destid, DESTID_TRANSLATE, if_name);

  // Configure tun/tap interface for pointo-to-point IPv4, L2, no ARP, no multicast
  {{
    const uint16_t my_destid_tun   = my_destid + DESTID_TRANSLATE;
    const uint16_t peer_destid_tun = destid + DESTID_TRANSLATE;

    snprintf(Tap_Ifconfig_Cmd, 256, "169.254.%d.%d pointopoint 169.254.%d.%d",
             (my_destid_tun >> 8) & 0xFF,   my_destid_tun & 0xFF,
             (peer_destid_tun >> 8) & 0xFF, peer_destid_tun & 0xFF);

    char ifconfig_cmd[513] = {0};
    snprintf(ifconfig_cmd, 512, "/sbin/ifconfig %s %s mtu %d up; echo 1 > /proc/sys/net/ipv6/conf/%s/disable_ipv6",
             if_name, Tap_Ifconfig_Cmd, MTU_SIZE, if_name);
    const int rr = system(ifconfig_cmd);
    if(rr >> 8) {
      fprintf(stderr, "system() failed with error %d\n", rr);
      // No need to remove from epoll set, close does that as it isn't dup(2)'ed
      close(tun_fd); tun_fd = -1;
      goto error;
    }

    snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
    system(ifconfig_cmd);
  }}

  {{
    struct epoll_event event;
    event.data.fd = tun_fd;
    event.events = EPOLLIN; // | EPOLLET;
    if (epoll_ctl (g_epoll_fd, EPOLL_CTL_ADD, tun_fd, &event) < 0) {
      fprintf(stderr, "Failed to add tun_fd %d to epoll set %d\n", tun_fd, g_epoll_fd);
      close(tun_fd); tun_fd = -1;
      goto error;
    }
  }}

  return tun_fd;

error:
  return -1;
}

#define MAX_EPOLL_EVENTS	8

void* tun_read_thr(void* arg)
{
  uint8_t send_buf[MTU_SIZE + 0x10];

  struct epoll_event* events = NULL;
  const int events_sz = MAX_EPOLL_EVENTS * sizeof(struct epoll_event);
  if ((events = (struct epoll_event*)alloca(events_sz)) == NULL) goto exit;
  memset(events, 0, events_sz);

  pthread_setname_np(pthread_self(), "TUN_RX");

  sem_wait(&g_start_sem);

  for(; !g_quit && g_epoll_fd >= 0;) {
    const int epoll_cnt = epoll_wait (g_epoll_fd, events, MAX_EPOLL_EVENTS, -1);
    if (epoll_cnt < 0 && errno == EINTR) continue;

    if (epoll_cnt < 0) {
      fprintf(stderr, "epoll_wait failed: %s\n", strerror(errno));
      break;
    }

    for (int epi = 0; epi < epoll_cnt; epi++) {
      if ((events[epi].events & EPOLLERR) || (events[epi].events & EPOLLHUP) || (!(events[epi].events & EPOLLIN))) {
        fprintf(stderr, "epoll error for data.fd=%d: %s\n", events[epi].data.fd, strerror(errno));
        continue;
      }

      assert(g_tun_fd != -1);
      const int nread = read(events[epi].data.fd, send_buf, MTU_SIZE);
      if (nread <= 0) {
        fprintf(stderr, "epoll error for data.ptr=%p: %s\n", events[epi].data.ptr, strerror(errno));
        goto exit;
      }

      if (nread == 0) {
        fprintf(stderr, "read(2) from Tun device returned 0 errno=%d %s. Aborting.\n", errno, strerror(errno)); fflush(stderr);
        assert(nread);
      }

      //printf("TUNRX %d bytes\n", nread);
      g_stats.tun_rx_pkt_cnt++;
      g_stats.tun_rx_pkt_bytes += nread;

      /* Send the data */
      assert(g_comm_sock);
      assert(g_comm_sock->skt);
      int rc = rskt_write(g_comm_sock, send_buf, nread);
      if (nread != rc) {
        fprintf(stderr, "rskt_write %d bytes failed %d: %s\n", nread, rc, strerror(errno));
        goto exit;
      }
    } // END for epi
  } // END for infinite for

exit:
  g_quit |= TUN_QUIT;

  fprintf(stderr, "%s: QUIT\n", __func__);

  return NULL;
}

/** 
 * \brief display usage information for the RSKT client
 */
void usage()
{
  printf("rskt_tun [-S] -d<did> [-p <port>] [-l <lev>] -h\n");
  printf("-S         : Run as rskt_server.\n");
  printf("-d<did>    : Destination ID of node running rskt_server.\n");
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
    fprintf(stderr, "Connect to %u on %u failed\n", destid, RSKT_PORT);
    rskt_close(g_comm_sock);
    return 0;
  }

  return 1;
}

int setup_rskt_srv(uint16_t* destid)
{
  g_listen_sock = rskt_create_socket();
  if (!g_listen_sock) {
    fprintf(stderr, "Failed to create listen socket: %s\n", strerror(errno));
    return 0;
  }

  struct rskt_sockaddr sock_addr;

  sock_addr.ct = 0;
  sock_addr.sn = RSKT_PORT;

  int rc = rskt_bind(g_listen_sock, &sock_addr);
  if (rc) {
    fprintf(stderr, "Failed to bind listen socket, rc = %d: %s\n", rc, strerror(errno));
    rskt_close(g_listen_sock);
    return 0;
  }

  /** Set socket to listen for connection requests */
  rc = rskt_listen(g_listen_sock, RSKT_DEFAULT_MAX_BACKLOG);
  if (rc) {
    fprintf(stderr, "Failed to listen & set max backlog, rc = %d: %s\n", rc, strerror(errno));
    rskt_close(g_listen_sock);
    return 0;
  }

  /** Loop untill interrupted, doing the following */
  while (1) {
    /** - Create a new accept socket for the next connection */
    rskt_h accept_socket = rskt_create_socket();
    if (!accept_socket) {
      fprintf(stderr, "Cannot create accept socket, rc = %d: %s\n", rc, strerror(errno));
      rskt_close(accept_socket);
      rskt_close(g_listen_sock);
      return 0;
    }

    /** - Await connect requests from RSKT clients */
    rc = rskt_accept(g_listen_sock, accept_socket, &sock_addr);
    if (rc) {
      fprintf(stderr, "Failed in rskt_accept, rc = 0x%X, errno=%d: %s\n", rc, errno, strerror(errno));
      rskt_close(accept_socket);
      rskt_close(g_listen_sock);
      return 0;
    }

    g_comm_sock = accept_socket;
    if (destid != NULL) *destid = sock_addr.ct;

    return 1;
  }

  /*NOTREACHED*/
  return 0;
}

void* rskt_read_thr(void* arg)
{
  uint8_t recv_buf[MTU_SIZE + 0x10];

  pthread_setname_np(pthread_self(), "TUN_TX");

  sem_wait(&g_start_sem);

  for(;! g_quit && g_tun_fd >=0 ;) {
    int rc = 0;

    do {
      assert(g_comm_sock);
      assert(g_comm_sock->skt);
      rc = rskt_read(g_comm_sock, recv_buf, MTU_SIZE);
    } while (rc == -ETIMEDOUT);

    if (rc <= 0) {
      fprintf(stderr, "rskt_read failed %d: %s\n", rc, strerror(errno));
      break;
    } 

    //printf("TUNTX %d bytes\n", rc);
    g_stats.rio_rx_pkt_cnt++;
    g_stats.rio_rx_pkt_bytes += rc;
    if (rc >= 0 && rc <= MTU_SIZE) g_stats.rio_rx_pkt_byte_sizes[rc]++;

    assert(g_tun_fd != -1);
    int nwrite = write(g_tun_fd, recv_buf, rc);
    if (nwrite < 0) {
      fprintf(stderr, "write of %d bytes [obtained from rskt_read] failed ret=%d: %s\n", rc, nwrite, strerror(errno));
      break;
    }
  } // END for infinite

  g_quit |= RIO_QUIT;

  fprintf(stderr, "%s: QUIT\n", __func__);

  return NULL;
}

int main(int argc, char *argv[])
{
  int rc = 0;
  int server = 0;

  if (argc < 2) {
    puts("Insufficient arguments. Must specify <destid>");
    usage();
    return 0;
  }

#ifdef RDMA_LL
  rdma_log_init("rskt_tun.txt", 1);
#endif

  uint16_t destid = 0xFFFF;

  int c;
  while ((c = getopt(argc, argv, "Shd:p:l:")) != -1) {
    switch (c) {
      case 'd': destid = atoi(optarg); break;
      case 'p': RSKT_PORT = atoi(optarg); break;
      case 'S': server = 1; break;
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
  if (!server && destid == 0xFFFF) {
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

  if (server) printf("SRV: my_destid=%u\n", g_my_destid);
  else        printf("CLI: my_destid=%u remote_destid=%u\n", g_my_destid, destid);

  g_epoll_fd = epoll_create1 (0);

  /** Initialize RSKT library */
  rc = librskt_init(RSKT_DEFAULT_DAEMON_SOCKET, 0);
  if (rc) {
    fprintf(stderr, "librskt_init failed, rc=%d: %s\n", rc, strerror(errno));
    return 2;
  }
  
  if (! server) { // rskt_client
    if (! setup_rskt_cli(destid)) { rc = 3; goto done; }
  } else {
    if (! setup_rskt_srv(&destid)) { rc = 3; goto done; }
    //if (! setup_rskt_srv(NULL)) { rc = 3; goto done; } // XX work-around for fmr rskt_accept BUG
    printf("my_destid=%u connect from remote_destid=%u\n", g_my_destid, destid);
  }

  sem_init(&g_start_sem, 0, 0);

  pthread_t pt_tun;
  if (pthread_create(&pt_tun, NULL, tun_read_thr, NULL) < 0) {
    fprintf(stderr, "pthread_create failed: %s\n", strerror(errno));
    return 4;
  }

  pthread_t pt_rio;
  if (pthread_create(&pt_rio, NULL, rskt_read_thr, NULL) < 0) {
    fprintf(stderr, "pthread_create failed: %s\n", strerror(errno));
    return 4;
  }

  if ((g_tun_fd = setup_TUN(g_my_destid, destid, 0x100)) < 0) { rc = 5; goto done; }

  sem_post(&g_start_sem);
  sem_post(&g_start_sem);

  while (! g_quit) usleep(1000);

  if ((g_quit & TUN_QUIT) != TUN_QUIT) pthread_kill(pt_tun, SIGUSR1);
  if ((g_quit & RIO_QUIT) != RIO_QUIT) pthread_kill(pt_rio, SIGUSR1);

done:
  if (server) rskt_close(g_listen_sock);
  if (g_comm_sock != NULL) rskt_close(g_comm_sock);
  close(g_tun_fd);   g_tun_fd = -1;
  close(g_epoll_fd); g_epoll_fd = -1;

  librskt_finish();

  _exit(rc);
} 
