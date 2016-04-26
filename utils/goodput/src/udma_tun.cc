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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>

#include <sched.h>

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/inotify.h>

#include <sstream>

#include "libcli.h"
#include "liblog.h"

#include "libtime_utils.h"
#include "worker.h"
#include "goodput.h"
#include "mhz.h"

#include "dmachan.h"
#include "rdmaops.h"
#include "rdmaopsumd.h"
#include "rdmaopsmport.h"
#include "lockfile.h"
#include "tun_ipv4.h"

#include "dmapeer.h" // This encapsulates in inlined code the IB window logic

extern "C" {
  void zero_stats(struct worker *info);
  int migrate_thread_to_cpu(struct thread_cpu *info);
  bool umd_check_cpu_allocation(struct worker *info);
  bool TakeLock(struct worker* info, const char* module, const int mport, const int instance);
  uint32_t crc32(uint32_t crc, const void *buf, size_t size);
};

#define PAGE_4K    4096

#define MBOX_BUFC  0x20
#define MBOX_STS   0x20

void umd_dma_goodput_tun_del_ep(struct worker* info, const uint32_t destid, bool signal);
void umd_dma_goodput_tun_del_ep_signal(struct worker* info, const uint32_t destid);
static bool inline umd_dma_tun_process_tun_RX(struct worker *info, DmaChannelInfo_t* dci, DmaPeer* peer, const uint16_t my_destid);

static inline uint64_t max_u64(const uint64_t a, const uint64_t b) { return a > b? a: b; }

///< This be fishy! There's no standard for ntonll :(
static inline uint64_t htonll(uint64_t value)
{
     int num = 42;
     if(*(char *)&num == 42)
          return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32LL) | htonl(value >> 32);
     else 
          return value;
}

static inline int Q_THR(const int sz) { return (sz * 99) / 100; }

const int DESTID_TRANSLATE = 1;

/** \brief Thread that services Tun TX, sends L3 frames to peer and does RIO TX throttling
 * Update RP via NREAD every 8 buffers; when local q is 2/3 full do it after each send
 */
void* umd_dma_tun_RX_proc_thr(void *parm)
{
  struct epoll_event* events = NULL;

  if (NULL == parm) return NULL;

  struct worker* info = (struct worker *)parm;
  if (NULL == info->umd_dci_list[info->umd_chan]) goto exit;
  if (NULL == info->umd_dci_nread) goto exit;

  if ((events = (struct epoll_event*)calloc(MAX_EPOLL_EVENTS, sizeof(struct epoll_event))) == NULL) goto exit;

#if 0
  info->umd_dma_tap_thr.cpu_req = GetDecParm("$cpu2", -1);

  migrate_thread_to_cpu(&info->umd_dma_tap_thr);

  if (info->umd_dma_tap_thr.cpu_req != info->umd_dma_tap_thr.cpu_run) {
    CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing out!\n",
         info->umd_dma_tap_thr.cpu_req, info->umd_dma_tap_thr.cpu_run);
    goto exit;
  }

  INFO("\n\tReady to receive from multiplexed Tun devices {isolcpu $cpu2=%d}. MAX_PEERS=%d\n", info->umd_dma_tap_thr.cpu_req, MAX_PEERS);
#else
  INFO("\n\tReady to receive from multiplexed Tun devices. MAX_PEERS=%d\n", MAX_PEERS);
#endif

  {{
  DmaChannelInfo_t* dch_list[6] = {0};

  int dch_cnt = 0;
  int dch_cur = 0;

  int CHAN_N = info->umd_chan_n;
#ifndef UDMA_TUN_DEBUG_NWRITE_CH2
  if (info->umd_chan < CHAN_N && info->umd_chann_reserve) CHAN_N--;
#endif
  for (int ch = info->umd_chan; ch <= CHAN_N; ch++) dch_list[dch_cnt++] = info->umd_dci_list[ch];

  uint64_t tx_cnt = 0;

  info->umd_dma_tap_proc_alive = 1;
  sem_post(&info->umd_dma_tap_proc_started);

  const uint16_t my_destid = info->my_destid;

again:
  while(! info->stop_req) {
    // XXX Maybe pick the one with the least full queue??
    DmaChannelInfo_t* dci = dch_list[dch_cur++];
    if (dch_cur >= dch_cnt) dch_cur = 0; // Wrap-around

  poll_again:
    const int epoll_cnt = epoll_wait (info->umd_epollfd, events, MAX_EPOLL_EVENTS, -1);
    if (epoll_cnt < 0 && errno == EINTR) goto poll_again;
  
    if (epoll_cnt < 0) {
      CRIT("\n\tepoll_wait failed: %s\n", strerror(errno))
      break;
    }

    for (int epi = 0; epi < epoll_cnt; epi++) {
      if (info->stop_req) break;
      if (events[epi].data.fd == info->umd_sockp_quit[1]) {
        INFO("\n\tSocketpair said quit!\n");
        char c = 0;
        read(events[epi].data.fd, &c, 1);
        goto exit; // Time to quit!
      }

      // We do NOT use edge-triggered epoll for Tun.
      // We want to process one L3 frame per incoming Tun in round-robin
      // fashion. If more frames are available epoll_wait will not block.

      if ((events[epi].events & EPOLLERR) || (events[epi].events & EPOLLHUP) || (!(events[epi].events & EPOLLIN))) {
        CRIT("\n\tepoll error for data.ptr=%p: %s\n", events[epi].data.ptr, strerror(errno));
        //close (tun_fd); // really? destroy peer as well????
        continue;
      }

      for (int cnt = 0; cnt < 4; cnt++) { // Maximum per-Tun
        if (umd_dma_tun_process_tun_RX(info, dci, (DmaPeer*)events[epi].data.ptr, my_destid)) tx_cnt++;
        else break; // Read all I could from Tun fd
      }
    } // END for epoll_cnt
  } // END while info->stop_req

  if (info->stop_req == SOFT_RESTART) {
    DBG("\n\tSoft restart requested, sleeping on semaphore\n");
    sem_wait(&info->umd_dma_tap_proc_started);
    DBG("\n\tAwakened after Soft restart!\n");
    goto again;
  }

  goto no_post;
  }}
exit:
  sem_post(&info->umd_dma_tap_proc_started);

no_post:
  free(events);
  info->umd_dma_tap_proc_alive = 0;

  return parm;
} // END umd_dma_tun_RX_proc_thr

/** \brief Process data from Tun and send it over DMA NWRITE
 */
static bool inline umd_dma_tun_process_tun_RX(struct worker *info, DmaChannelInfo_t* dci, DmaPeer* peer, const uint16_t my_destid)
{
  if (info == NULL || dci == NULL || peer == NULL || my_destid == 0xffff) return false;

  const int tun_fd = peer->get_tun_fd();
  if (tun_fd < 0) return false; // peer is destroyed

  bool ret = false;
  int destid_dpi = -1;
  int is_bad_destid = 0;
  bool first_message = false;
  int force_nread = 0;
  int outstanding = 0; // This is our guess of what's not consumed at other end, per-destid

  //const bool q_fullish = dci->dch->queueSize() > Q_THR(info->umd_tx_buf_cnt-1);

  DMAChannel::DmaOptions_t& dmaopt = dci->dmaopt[dci->oi];

  assert(dci->tx_buf_cnt);

  uint8_t* buffer = (uint8_t*)dci->dmamem[dci->oi].win_ptr;
  assert(buffer);

  const int nread = read(peer->get_tun_fd(), buffer+DMA_L2_SIZE, info->umd_tun_MTU);
  const uint64_t now = rdtsc();

  if (nread < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false;

  assert(nread > 0);

  DMA_L2_t* pL2 = (DMA_L2_t*)buffer;

  {{
  uint32_t* pkt = (uint32_t*)(buffer+DMA_L2_SIZE);
  const uint32_t dest_ip_v4 = ntohl(pkt[4]); // XXX IPv6 will stink big here

  destid_dpi = (dest_ip_v4 & 0xFFFF) - DESTID_TRANSLATE;
  }}

  is_bad_destid += (destid_dpi == info->my_destid);


  if (info->stop_req || peer->stop_req) goto unlock;

#ifdef UDMA_TUN_DEBUG
  {{
  const uint32_t crc = crc32(0, buffer+DMA_L2_SIZE, nread);
  DBG("\n\tGot from tun? %d+%d bytes (L7 CRC32 0x%x) to RIO destid %u%s\n",
     nread, DMA_L2_SIZE,
     crc, destid_dpi,
     is_bad_destid? " BLACKLISTED": "");
  }};
#endif

  peer->m_stats.tun_rx_cnt++;

  if (is_bad_destid) {
    ERR("\n\tBad destid %u -- score=%d\n", destid_dpi, is_bad_destid);
    send_icmp_host_unreachable(peer->get_tun_fd(), buffer+DMA_L2_SIZE, nread);
    goto error;
  }

  first_message = peer->m_stats.tx_cnt == 0;

  do {{
    if (peer->get_WP() == peer->get_RP()) { break; }
    if (peer->get_WP() > peer->get_RP())  { outstanding = peer->get_WP()-peer->get_RP(); break; }
    //if (WP == (info->umd_tx_buf_cnt-2)) { outstanding = RP; break; }
    outstanding = peer->get_WP() + dci->tx_buf_cnt-2 - peer->get_RP();
  }} while(0);

  DBG("\n\tWP=%d guessed { RP=%d outstanding=%d } %s\n", peer->get_WP(), peer->get_RP(), outstanding, (outstanding==(dci->tx_buf_cnt-1))? "FULL": "");

  // We force reading RP from a "new" destid as a RIO ping as
  // NWRITE does not barf  on bad destids
  {{
    const uint64_t last_seen_rp_update = max_u64(peer->get_RP_lastSeen(), peer->m_stats.nread_ts);
    if ((rdtsc() - last_seen_rp_update) > info->umd_nread_threshold)
    force_nread = 1;
  }}

  if (first_message || force_nread /*|| q_fullish*/ || outstanding >= Q_THR(info->umd_tx_buf_cnt-1)) do { // This must be done per-destid
    if (info->umd_disable_nread && !first_message) break;
#if 0
INFO("first_message=%d force_nread=%d q_fullish=%d \"outstanding[%d] >= Q_THR(info->umd_tx_buf_cnt-1) [%d]\"=%d\n",
first_message, force_nread, q_fullish, outstanding, Q_THR(info->umd_tx_buf_cnt-1), outstanding >= Q_THR(info->umd_tx_buf_cnt-1));
#endif
    uint32_t newRP = ~0;
    if (udma_nread_mem(info->umd_dci_nread->rdma, destid_dpi, peer->get_rio_addr(), sizeof(newRP), (uint8_t*)&newRP)) {
      peer->m_stats.nread_ts = rdtsc();
      DBG("\n\tPulled RP from destid %u old RP=%d actual RP=%d\n", destid_dpi, peer->get_RP(), newRP);
      peer->set_RP(newRP);
      if (first_message) peer->set_WP(newRP); // XXX maybe newRP+1 ?? Test
    } else {
      RdmaOpsIntf* rdma = info->umd_dci_nread->rdma;

      send_icmp_host_unreachable(peer->get_tun_fd(), buffer+DMA_L2_SIZE, nread); // XXX which tun fd??

      CRIT("\n\tHW error (NREAD), something is FOOBAR with Chan %u reason %d (%s). Nuke peer.\n", info->umd_chan2, 
           rdma->getAbortReason(), rdma->abortReasonToStr(rdma->getAbortReason()));

      peer->stop_req = 1;

      umd_dma_goodput_tun_del_ep(info, destid_dpi, false); // Nuke Tun & Peer

      goto error;
    }
  } while(0);

  if (info->stop_req || peer->stop_req) goto error;

  if (outstanding == (dci->tx_buf_cnt-1)) {
    CRIT("\n\tPeer destid=%u is FULL, dropping frame!\n", destid_dpi);
    peer->m_stats.rio_tx_peer_full++;
    // XXX Maybe send back ICMP Host Unreachable? TODO: Study RFCs
    goto error;
  }

  pL2->destid = htons(my_destid);
  pL2->len    = htonl(DMA_L2_SIZE + nread);
  pL2->RO     = 1;

  if (info->stop_req) goto unlock;

  // Barry dixit "If full sleep for (queue size / 2) nanoseconds"
  for (int i = 0; i < (dci->tx_buf_cnt-1)/2; i++) {
    if (! dci->rdma->queueFull()) break;
    struct timespec tv = { 0, 1};
    nanosleep(&tv, NULL);
  }

  if (dci->rdma->queueFull()) {
    DBG("\n\tQueue full #1 on chan %d!\n", dci->chan);
    goto error; // Drop L3 frame
  }

  if (info->stop_req) goto unlock;

  {{
    const int peer_WP = peer->get_WP();

    dmaopt.destid      = destid_dpi;
    dmaopt.bcount      = ntohl(pL2->len);
    dmaopt.raddr.lsb64 = peer->get_rio_addr() + sizeof(DmaPeerRP_t) + peer_WP * BD_PAYLOAD_SIZE(info);
    dmaopt.u_data      = now;

    const uint64_t PEER_END_WIN = peer->get_rio_addr() + peer->get_ibwin_size();

    if (dmaopt.raddr.lsb64 > (PEER_END_WIN-1)) {
      CRIT("\n\tPeer did %u [WP=%d] address 0x%llx falls beyond allocated memory mark 0x%llx\n",
           dmaopt.destid, peer_WP, dmaopt.raddr.lsb64, PEER_END_WIN-1);
      assert(dmaopt.raddr.lsb64 < PEER_END_WIN-1);
      goto error; // In case asserts disabled
    }

    if ((dmaopt.raddr.lsb64 + dmaopt.bcount) > PEER_END_WIN) {
      CRIT("\n\tPeer did %u [WP=%d] mem exent 0x%llx+0x%x falls beyond allocated memory mark 0x%llx\n",
           dmaopt.destid, peer_WP, dmaopt.raddr.lsb64, dmaopt.bcount, PEER_END_WIN);
      assert((dmaopt.raddr.lsb64 + dmaopt.bcount) < PEER_END_WIN);
      goto error; // In case asserts disabled
    }
  }}

  DBG("\n\tSending to RIO %d+%d bytes to RIO destid %u addr 0x%llx WP=%d chan=%d oi=%d\n",
      nread, DMA_L2_SIZE, destid_dpi, dmaopt.raddr.lsb64, peer->get_WP(), dci->chan, dci->oi);

  dci->rdma->setCheckHwReg(first_message);

  info->umd_dma_abort_reason = 0;

  if (! udma_nwrite_mem_T1(dci->rdma, dmaopt, dci->dmamem[dci->oi], (int&)info->umd_dma_abort_reason)) {
    if (info->dma_method == ACCESS_MPORT) {
      if (info->umd_dma_abort_reason == EBUSY) {
        DBG("\n\tQueue full #2 on Mport Chan %d!\n", dci->chan);
        usleep(RdmaOpsMport::QUEUE_FULL_DELAY_MS);
      } else { // all other errors
        send_icmp_host_unreachable(peer->get_tun_fd(), buffer+DMA_L2_SIZE, nread); // XXX which tun

        ERR("\n\tHW error (NWRITE_R), something is FOOBAR with Mport Chan %d reason %d (%s). Nuke peer.\n",
            dci->chan, 
            dci->rdma->getAbortReason(), dci->rdma->abortReasonToStr(dci->rdma->getAbortReason()));

        umd_dma_goodput_tun_del_ep(info, destid_dpi, false); // Nuke Tun & Peer
      }
      goto error;
    }

    if (info->dma_method == ACCESS_UMD && info->umd_dma_abort_reason != 0) { // HW error
      // ICMPv4 dest unreachable id bad destid 
      send_icmp_host_unreachable(peer->get_tun_fd(), buffer+DMA_L2_SIZE, nread); // XXX which tun

      ERR("\n\tHW error (NWRITE_R), something is FOOBAR with Chan %d reason %d (%s). Nuke peer.\n",
          dci->chan, 
          dci->rdma->getAbortReason(), dci->rdma->abortReasonToStr(dci->rdma->getAbortReason()));

      umd_dma_goodput_tun_del_ep(info, destid_dpi, false); // Nuke Tun & Peer

      if (dci->rdma->canRestart()) {
        INFO("\n\tHW error, triggering soft restart\n");
        info->stop_req = SOFT_RESTART; // XXX of which channel?
      }
    } else { // queue really full
      DBG("\n\tQueue full #2 on Chan %d!\n", dci->chan);
    }
    goto error; // Drop L3 frase
  }

  dci->oi++; if (dci->oi == (dci->tx_buf_cnt-1)) dci->oi = 0; // Account for T3

  // For just one destdid WP==oi but we keep them separate

  // This must be done per-destid
  if (peer->inc_WP() == (dci->tx_buf_cnt-1)) peer->set_WP(0); // Account for T3 missing IBwin cell, ALL must have the same bufc!

  peer->m_stats.tx_cnt++;
  ret = true;

unlock:
  return ret;

error:  goto unlock;
}

/** \brief Callback from FIFO isolcpu thread
 * Look quickly at L2 headers of IB buffers, signal to main thread if there is work to do
 */
void umd_dma_goodput_tun_callback(struct worker *info)
{
  if(info == NULL) return;

  assert(info->ib_ptr);

  static uint64_t cbk_iter = 0;

  cbk_iter++;

  if (info->umd_dma_did_peer.size() == 0) return;

  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);

  for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
    DmaPeer* peer = info->umd_dma_did_peer_list[epi];
    if (peer == NULL) continue; // Just vacated one slot?

    if (peer->sig == PEER_SIG_INIT) continue; // Not UP yet

    assert(peer->sig == PEER_SIG_UP);

    const uint16_t destid = peer->get_destid();

    // XXX Do I need this paranoia check in production?
    if (info->umd_dma_did_enum_list.find(destid) == info->umd_dma_did_enum_list.end()) {
      CRIT("\n\tBUG Peer for destid %u exists in only one map!\n", destid);
      break; // Better luck next time we're called
    }

    int ret = peer->scan_RO(); ret += 0;

#ifdef UDMA_TUN_DEBUG_IB
    DBG("\n\tFound %d ready buffers at iter %llu\n", ret, cbk_iter);
#endif
  } // END for each peer

  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
}

/** \brief This helper thread wakes up the Peer thread(s) and the main thread at quitting time */
void* umd_dma_wakeup_proc_thr(void* arg)
{
  if(arg == NULL) return NULL;
  struct worker* info = (struct worker*)arg;

  while (!info->stop_req) // every 1 mS which is about HZ/10. stupid
    usleep(1000);
  
  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  do {{
    if (info->umd_dma_did_peer_list_high_wm == 0) break;

    for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
      DmaPeer* peer = info->umd_dma_did_peer_list[epi];
      if (peer == NULL) continue; // Just vacated one slot?

      assert(peer->sig == PEER_SIG_UP);

      peer->stop_req = QUIT_IN_PROGRESS;
      peer->rx_work_sem_post();
    }
  }} while(0);
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  return NULL;
}


DmaChannelInfo_t* umd_rdma_factory(struct worker *info, int chan)
{
  assert(info);

  DmaChannelInfo_t* dci = (DmaChannelInfo_t*)calloc(1, sizeof(DmaChannelInfo_t));
  if (dci == NULL) return NULL;

  switch (info->dma_method) {
    case ACCESS_UMD:   dci->rdma = new RdmaOpsUMD();   break;
    case ACCESS_MPORT: dci->rdma = new RdmaOpsMport(); break;
    default: assert(0); break;
  }

  assert(dci->rdma);
  dci->chan = chan;

  return dci;
}

bool umd_dma_goodput_tun_setup_chan2(struct worker *info)
{
  info->umd_dci_nread = umd_rdma_factory(info, info->umd_chan2);
  if (info->umd_dci_nread == NULL) return false;

  switch (info->dma_method) {
    case ACCESS_UMD: {{
              assert(info->umd_dci_nread->rdma);
              DMAChannel* ch = (dynamic_cast<RdmaOpsUMD*>(info->umd_dci_nread->rdma))->setup_chan2(info);
              assert(ch);
              info->umd_dci_nread->tx_buf_cnt = RdmaOpsUMD::DMA_CHAN2_BUFC;
              info->umd_dci_nread->sts_entries= RdmaOpsUMD::DMA_CHAN2_STS;
            }}
            break;
    case ACCESS_MPORT:
            (dynamic_cast<RdmaOpsMport*>(info->umd_dci_nread->rdma))->setup_chan2(info); 
            break;
    default: return false; break;
  }

  return true;
}


bool umd_dma_goodput_tun_setup_chanN(struct worker *info, const int n)
{
  if (info == NULL) return false;

  if (n < 0 || n > 7) return false;
  if (n < info->umd_chan || n > info->umd_chan_n) return false;

  if (info->umd_dci_list[n] != NULL) return false; // already setup

  DmaChannelInfo_t* dci = umd_rdma_factory(info, n);
  if (dci == NULL) return false;

  RdmaOpsIntf* rdma = dci->rdma;
  assert(rdma);

  switch (info->dma_method) {
    case ACCESS_UMD: {{
              DMAChannel* ch = (dynamic_cast<RdmaOpsUMD*>(rdma))->setup_chanN(info, n, dci->dmamem);
              assert(ch);
              dci->tx_buf_cnt  = info->umd_tx_buf_cnt;
              dci->sts_entries = info->umd_sts_entries;
            }}
            break;
    case ACCESS_MPORT: {{
              bool r = (dynamic_cast<RdmaOpsMport*>(rdma))->setup_chanN(info, n, dci->dmamem);
              assert(r);
              dci->tx_buf_cnt  = info->umd_tx_buf_cnt;
            }}
            break;
    default: assert(0); break;
  }

  info->umd_dci_list[n] = dci;

  return true;
}

bool umd_dma_tun_update_peer_RP(struct worker* info, DmaPeer* peer)
{
  assert(info);
  assert(peer);

  bool ret = true;

  DmaPeerUpdateRP_t upeer;
  upeer.RP = peer->get_IB_RP(); // this one will assert internally on bad RP
  upeer.UC = peer->get_serial();

  const uint16_t destid = peer->get_destid();
  assert(offsetof(DmaPeerRP_t, rpeer) < peer->get_ibwin_size());
  const uint64_t rio_addr = peer->get_rio_addr() + offsetof(DmaPeerRP_t, rpeer);

#ifdef UDMA_TUN_DEBUG_NWRITE_CH2
  DmaChannelInfo_t* dci = info->umd_dci_nread;
#else
  DmaChannelInfo_t* dci = info->umd_dci_list[info->umd_chan_n]; // pick the last channel

  // We cannot set prio=2 via Mport so we force the 2nd channel here
  if (info->dma_method == ACCESS_MPORT)
    dci = info->umd_dci_nread;
#endif

  if (! udma_nwrite_mem(dci->rdma, destid, rio_addr, sizeof(DmaPeerUpdateRP_t), (uint8_t*)&upeer)) do {
    if (info->dma_method == ACCESS_MPORT && dci->rdma->getAbortReason() == EBUSY) {
      usleep(RdmaOpsMport::QUEUE_FULL_DELAY_MS);
      return false;
    }

    ERR("\n\tHW error (NWRITE/T2), something is FOOBAR with Chan %u reason %d (%s). Nuke peer.\n", info->umd_chan2, 
        dci->rdma->getAbortReason(), dci->rdma->abortReasonToStr(dci->rdma->getAbortReason()));

    peer->stop_req = 1;

    umd_dma_goodput_tun_del_ep(info, destid, false); // Nuke Tun & Peer

    ret = false;
  } while(0);

  return ret;
}

/** \brief This waits for \ref umd_dma_goodput_tun_callback to signal that IBwin buffers are ready for this peer. Then stuffs L3 frames into Tun
 */
void* umd_dma_tun_TX_proc_thr(void* arg)
{
  if (arg == NULL) return NULL;

  DmaPeer* peer = (DmaPeer*)arg;
  assert(peer->sig == PEER_SIG_UP);

  struct worker* info = peer->get_info();

  uint64_t rx_ok = 0; ///< TX'ed into Tun device

  const int MHz = getCPUMHz();

  // TODO: Compute this function of MTU and RIO transfer speed
  const uint64_t MAX_RP_INTERVAL = 1000 * MHz; // no more that 1 ms for MTU=16k

  volatile DmaPeerRP_t* pRP = (DmaPeerRP_t*)peer->get_ib_ptr();

  struct thread_cpu myself; memset(&myself, 0, sizeof(myself));

  myself.thr = pthread_self();
  myself.cpu_req = GetDecParm("$cpu2", -1);

  migrate_thread_to_cpu(&myself);

  if (myself.cpu_req != myself.cpu_run) {
    CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing out!\n",
         myself.cpu_req, myself.cpu_run);
    goto exit;
  }

  INFO("\n\tReady to receive from RIO from destid %u {isolcpu $cpu2=%d}\n", peer->get_destid(), myself.cpu_req);

again: // Receiver (from RIO), TUN TX: Ingest L3 frames into Tun (zero-copy), update RP, set RO=0
  while (!info->stop_req && !peer->stop_req) {
    if (peer->get_rio_rx_bd_ready_size() == 0) // Unlocked op, we take a sneak peek at volatile counter
      peer->rx_work_sem_wait();

    if (info->stop_req || peer->stop_req) goto stop_req;
    assert(peer->sig == PEER_SIG_UP);

    DBG("\n\tInbound %d buffers(s) ready RP=%u\n", peer->get_rio_rx_bd_ready_size(), pRP->RP);

    int cnt = peer->service_TUN_TX(MAX_RP_INTERVAL);
    rx_ok += cnt;
  } // END while NOT stop requested

stop_req:
  if (info->stop_req == SOFT_RESTART && !peer->stop_req) {
    DBG("\n\tSoft restart requested, sleeping in a 10uS loop\n");
    while(info->stop_req != 0) usleep(10);
    DBG("\n\tAwakened after Soft restart!\n");
    goto again;
  }

  // XXX clean up this dead peer

exit:
  return arg;
}

/** \brief Setup a peer and spawn its associated thread
 * \param[in] info this-like
 * \param     destid peer's RIO destid
 * \param[in] from_info IBwin info peer sent us, fields in host order
 * \param[in] ib_ptr address allocated for peer into LOCAL IBwin
 * \return true if everything is setup, false otherwise
 */
bool umd_dma_goodput_tun_setup_peer(struct worker* info, const uint16_t destid, const DMA_MBOX_PAYLOAD_t* from_info, const void* ib_ptr)
{
  int rc = 0;
  int slot = -1;
  pthread_t tun_TX_thr;
  bool peer_list_full = false;

  if (info == NULL || from_info == NULL) {
    CRIT("Invalid argument(s) info=%p from_info=%p\n", info, from_info);
    return false;
  }
  if (from_info->rio_addr == 0) {
    CRIT("Invalid argument from_info->rio_addr == 0");
    return false;
  }

  DmaPeer* tmppeer = new DmaPeer();
  if (tmppeer == NULL) return false;

  tmppeer->sig = PEER_SIG_INIT;
  tmppeer->set_destid(destid);
  tmppeer->set_rio_addr(from_info->rio_addr);
  tmppeer->set_ibwin_size(from_info->ibwin_size);

  // Set up array of pointers to IB L2 headers
  if (! tmppeer->init(info, ib_ptr)) {
    CRIT("DmaPeer::init FAILED!\n");
    goto exit;
  }

  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
    for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
      if (info->umd_dma_did_peer_list[epi] != NULL) continue; // No a vacant slot?
      slot = epi;
      break;
    }
    if (slot != -1) {
      info->umd_dma_did_peer_list[slot] = tmppeer;
      info->umd_dma_did_peer[destid] = slot;
    } else if (info->umd_dma_did_peer_list_high_wm < MAX_PEERS) {
      slot = info->umd_dma_did_peer_list_high_wm;
      info->umd_dma_did_peer_list[slot] = tmppeer;
      info->umd_dma_did_peer[destid] = slot;
      info->umd_dma_did_peer_list_high_wm++;
    } else {
      peer_list_full = true;
    }
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
        
  if (peer_list_full) {
    CRIT("\n\tPeer list is full -- no room for NEW destid %u\n", destid);
    goto exit;
  }

  if (! tmppeer->setup_TUN(info->my_destid, DESTID_TRANSLATE)) goto exit;

  rc = pthread_create(&tun_TX_thr, NULL, umd_dma_tun_TX_proc_thr, (void *)tmppeer);
  if (rc) {
    CRIT("\n\tCould not create umd_dma_tun_TX_proc_thr thread, exiting...");
    goto exit;
  }

  return true;

exit:
  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
    info->umd_dma_did_peer.erase(destid);
    if (slot != -1) info->umd_dma_did_peer_list[slot] = NULL;;
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  delete tmppeer;

  return false;
}

/** \brief Thread to service FIFOs for all TX/NWRITE DMA channels
 * \note This must be running on an isolcpu core.
 * \note It has a callback for other code to share the isolcpu resource.
 */
void* umd_dma_tun_fifo_proc_thr(void* parm)
{
  struct worker* info = NULL;

  int dch_cnt = 0;
  DmaChannelInfo_t* dch_list[6] = {0};

  if (NULL == parm) goto exit;

  info = (struct worker *)parm;

  if (info->dma_method == ACCESS_UMD) {
    if (NULL == info->umd_dci_list[info->umd_chan]) goto exit;
    if (NULL == info->umd_dci_list[info->umd_chan_n]) goto exit;

    for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) dch_list[dch_cnt++] = info->umd_dci_list[ch];
  
    assert(dch_cnt);
  }

  DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

  migrate_thread_to_cpu(&info->umd_fifo_thr);

  if (info->umd_fifo_thr.cpu_req != info->umd_fifo_thr.cpu_run) {
    CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing out!\n",
         info->umd_fifo_thr.cpu_req, info->umd_fifo_thr.cpu_run);
    goto exit;
  }

  INFO("\n\tFIFO Scanner thread ready, optimised for %s\n", info->umd_tun_thruput? "thruput": "latency");

  info->umd_fifo_proc_alive = 1;
  sem_post(&info->umd_fifo_proc_started);

  // Emulate isolcpu scanning for mport
  if (info->dma_method == ACCESS_MPORT) {
    while (!info->umd_fifo_proc_must_die) {
      // This is a hook to do stuff for IB buffers in isolcpu thread
      // Note: No relation to TX FIFO/buffers, just CPU sharing
      if (info->umd_dma_fifo_callback != NULL)
        info->umd_dma_fifo_callback(info);

      struct timespec tv = { 0, 1 };
      nanosleep(&tv, NULL);
    }
    goto exit;
  }

  // Vanilla UMD
  while (!info->umd_fifo_proc_must_die) {
    for (int ch = 0; ch < dch_cnt; ch++) {
      ts_now_mark(&info->fifo_ts, 1);

      // This is a hook to do stuff for IB buffers in isolcpu thread
      // Note: No relation to TX FIFO/buffers, just CPU sharing
      if (info->umd_dma_fifo_callback != NULL)
        info->umd_dma_fifo_callback(info);

      ts_now_mark(&info->fifo_ts, 2);

      DMAChannel* dch = dynamic_cast<RdmaOpsUMD*>(dch_list[ch]->rdma)->getChannel();
      assert(dch);

      const int cnt = dch->scanFIFO(wi, info->umd_sts_entries*8);
      if (!cnt) {
        if (info->umd_tun_thruput) {
          // for(int i = 0; i < 1000; i++) {;} continue; // "1000" busy wait seemd OK for latency, bad for thruput
          ts_now_mark(&info->fifo_ts, 3);
          struct timespec tv = { 0, 1 };
          nanosleep(&tv, NULL);
        }
        continue;
      }

      ts_now_mark(&info->fifo_ts, 4);

      for (int i = 0; i < cnt; i++) {
        DMAChannel::WorkItem_t& item = wi[i];

        switch (item.opt.dtype) {
        case DTYPE1:
        case DTYPE2:
          if (item.ts_end > item.ts_start) {
            dch_list[ch]->ticks_total += (item.ts_end - item.ts_start);  
          }
          // These are from read from Tun
          if (item.opt.u_data != 0 && item.ts_end > item.opt.u_data) {
            dch_list[ch]->total_ticks_tx += item.ts_end - item.opt.u_data;
          }
          break;
        case DTYPE3:
          break;
        default:
          ERR("\n\tUNKNOWN BD %d bd_wp=%u\n",
            item.opt.dtype, item.bd_wp);
          break;
        }

        wi[i].valid = 0xdeadabba;
      } // END for WorkItem_t vector
    } // END for each channel
    clock_gettime(CLOCK_MONOTONIC, &info->end_time);
  } // END while
  goto no_post;

exit:
  sem_post(&info->umd_fifo_proc_started);

no_post:
  info->umd_fifo_proc_alive = 0;

  return parm;
}

/* \brief Interact with other peers via MBOX and set up Tun devices as appropriate
 * This code evolved to be RDMAD-redux.
 * It is fed by MboxWatch via socketpairs
 * and EpWatch via a (polled) list of enumerated EPs.
 * \note MboxWatch may or may not be running; we do not barf if the socketpair is/becomes -1!
 * \param[in] info "this" replacement
 * \param min_bcasts Put up Tun device only after hearing min_bcasts from peer
 * \param bcast_interval Interval between broadcasts
 */
void umd_dma_goodput_tun_RDMAD(struct worker *info, const int min_bcasts, const int bcast_interval)
{
  time_t next_bcast = time(NULL);

  while (!info->stop_req) {
// Broadcast my IBwin mapping Urbi & Orbi, once per second
    time_t now = 0;
    std::vector<uint32_t> peer_list;

    // Collect peers quickly
    pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
    std::map<uint16_t, DmaPeerCommsStats_t>::iterator itp = info->umd_dma_did_enum_list.begin();
    for (; itp != info->umd_dma_did_enum_list.end(); itp++) {
      if (itp->first == info->my_destid) continue;
      peer_list.push_back(itp->first);
    }
    pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

    now = time(NULL);

    if (peer_list.size() == 0 || info->umd_mbox_tx_fd < 0 || now <= next_bcast) goto receive;

    next_bcast = now + bcast_interval;

    if (7 <= g_level) {
      std::stringstream ss;
      for (int ip = 0; ip < peer_list.size(); ip++) ss << peer_list[ip] << " ";
      DBG("\n\tGot %d peers to broadcast to [tx_fd=%d]: %s\n", peer_list.size(), info->umd_mbox_tx_fd, ss.str().c_str());
    }

    for (int ip = 0; ip < peer_list.size(); ip++) {
      const uint16_t destid = peer_list[ip];

      uint64_t rio_addr = 0; // Where I want the peer to deposit its stuff
      void*    ib_ptr   = NULL;
      if (! info->umd_peer_ibmap->lookup(destid, rio_addr, ib_ptr)) {
        CRIT("\n\tCan't find IBwin mapping for destid %u!\n", destid);
        goto exit;
      }

      assert(rio_addr);
      if (info->umd_mbox_tx_fd < 0) break; // FOR ip

      uint8_t buf[sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t)] = {0};

      DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;

      // ENCAP
      DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));
      payload->rio_addr      = htonll(rio_addr);
      payload->ibwin_size    = htonl(info->umd_peer_ibmap->getIBwinSize());
      payload->base_rio_addr = htonll(info->umd_peer_ibmap->getBaseRioAddr());
      payload->base_size     = htonl(info->umd_peer_ibmap->getBaseSize());
      payload->MTU           = htonl(info->umd_tun_MTU);
      payload->bufc          = htons(info->umd_tx_buf_cnt-1);
      payload->action        = ACTION_ADD;

      pL2->destid_src = htons(info->my_destid);
      pL2->destid_dst = htons(destid);
      pL2->len        = htons(sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t));

      DBG("\n\tSignalling [tx_fd=%d] peer destid %u {base_rio_addr=0x%llx base_size=0x%lx ibwin_size=0x%lx rio_addr=0x%llx bufc=%u MTU=%lu}\n",
          info->umd_mbox_tx_fd, destid,
          info->umd_peer_ibmap->getBaseRioAddr(), info->umd_peer_ibmap->getBaseSize(),
          info->umd_peer_ibmap->getIBwinSize(),
          rio_addr, info->umd_tx_buf_cnt-1, info->umd_tun_MTU);

      if (info->umd_mbox_tx_fd < 0) break; // FOR ip

      int nsend = send(info->umd_mbox_tx_fd, buf, sizeof(buf), 0);
      if (nsend > 0) {
        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
        info->umd_dma_did_enum_list[destid].bcast_cnt_out++;
        info->umd_dma_did_enum_list[destid].my_rio_addr = rio_addr;
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
      } else {
        ERR("\n\tSend to MboxWatch thread failed [tx_fd=%d]: %s\n", strerror(errno), info->umd_mbox_tx_fd);
      }
    } // END for peers

// Receive broadcasts from other peers
  receive:
    if (info->umd_mbox_rx_fd < 0) { // Other thread not ready!
      usleep(10 * 1000);
      continue;
    }

    uint8_t buf[PAGE_4K] = {0};

    struct timeval to = { 0, 100 * 1000 }; // 100 ms
    fd_set rfds; FD_ZERO (&rfds); FD_SET(info->umd_mbox_rx_fd, &rfds);

    int ret = select(info->umd_mbox_rx_fd+1, &rfds, NULL, NULL, &to);
    if (ret < 0) {
      DBG("\n\tselect failed on umd_mbox_rx_fd: %s\n", strerror(errno));
      continue; // MboxWatch conked or was stopped. Better luck next time.
    }

    if (info->stop_req) goto exit;

    if (FD_ISSET(info->umd_mbox_rx_fd, &rfds)) do {{ // Sumthin to came over on MBOX, read only 1st dgram
      const int nread = recv(info->umd_mbox_rx_fd, buf, PAGE_4K, 0);

      DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
      assert(nread == ntohs(pL2->len));

      uint16_t from_destid = ntohs(pL2->destid_src);

      if (from_destid == info->my_destid) { // Myself?
        DBG("\n\tGot %d bytes on [rx_fd=%d] from myself!\n", nread, info->umd_mbox_rx_fd);
        break;
      }

      time_t now = time(NULL);
      bool peer_setup = false;
      uint64_t peer_rio_addr = 0;
      pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
      {{
        if (info->umd_dma_did_enum_list[from_destid].on_time == 0) {
          info->umd_dma_did_enum_list[from_destid].destid = from_destid;
          info->umd_dma_did_enum_list[from_destid].on_time = now;
        }

        info->umd_dma_did_enum_list[from_destid].ls_time = now;
        info->umd_dma_did_enum_list[from_destid].bcast_cnt_in++;

        std::map<uint16_t, int>::iterator itp = info->umd_dma_did_peer.find(from_destid);
        if (itp != info->umd_dma_did_peer.end()) {
          peer_setup = true;
          peer_rio_addr = info->umd_dma_did_peer_list[itp->second]->get_rio_addr();
        }
      }}
      pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

      // We have an IBwin mapping belonging to from_destid waiting in (buf + sizeof(DMA_MBOX_L2_t))
      // So put up a new peer, Tun and start up a new Tun thread!!

      assert(ntohs(pL2->len) >= (sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t)));

      DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));

      if (payload->action == ACTION_DEL) {
        if (peer_setup) {
          INFO("\n\tPeer destid %u told us to go away. Nuking peer\n", from_destid);
          umd_dma_goodput_tun_del_ep(info, from_destid, false);
          break; // F*ck g++
        } else {
          ERR("\n\tPeer destid %u told us to go away but we have no peer for it!\n", from_destid);
          break; // F*ck g++
        }
      }

      // If rio_addr comes and != what we have for peer then nuke peer as it had restarted!!!!!

      const uint64_t from_rio_addr      = htonll(payload->rio_addr);
      assert(from_rio_addr);

      if (peer_setup) {
        assert(peer_rio_addr);
        if (from_rio_addr != peer_rio_addr) {
          INFO("\n\tGot info [rx_fd=%d] for STALE destid %u NEW peer.rio_addr=0x%llx changed from stored peer.rio_addr=0x%llx. Nuking peer.\n",
               info->umd_mbox_rx_fd, from_destid, from_rio_addr, peer_rio_addr);
          umd_dma_goodput_tun_del_ep(info, from_destid, true);
          break;
        }

        break; // Don't put up Tun twice // F*ck g++
      }

      DMA_MBOX_PAYLOAD_t from_info; memset(&from_info, 0, sizeof(from_info));

      // DECAP
      from_info.rio_addr      = htonll(payload->rio_addr);
      from_info.ibwin_size    = ntohl(payload->ibwin_size);
      from_info.base_rio_addr = htonll(payload->base_rio_addr);
      from_info.base_size     = ntohl(payload->base_size);
      from_info.bufc          = ntohs(payload->bufc);
      from_info.MTU           = ntohl(payload->MTU);

      assert(from_info.base_rio_addr);
      assert(from_info.base_size);

      do {{
        if (from_info.MTU != info->umd_tun_MTU) {
          INFO("\n\tGot a mismatched MTU %lu from peer destid %u (expecting %lu). Ignoring peer.\n",
               from_info.MTU, from_destid, info->umd_tun_MTU);
          break;
        }
        if (from_info.bufc != (info->umd_tx_buf_cnt-1)) {
          INFO("\n\tGot a mismatched bufc %u from peer destid %u (expecting %d). Ignoring peer.\n",
               from_info.bufc, from_destid, (info->umd_tx_buf_cnt-1));
          break;
        }
        if (from_info.rio_addr == 0) {
          INFO("\n\tGot a 0 rio_addr from peer destid %u. Ignoring peer.\n", from_destid);
          break;
        }

        uint64_t rio_addr    = 0; 
        void*    peer_ib_ptr = NULL;
        if (! info->umd_peer_ibmap->alloc(from_destid, rio_addr, peer_ib_ptr)) {
          CRIT("\n\tWARNING: Can't alloc IBwin mapping for NEW peer destid %u!\n", from_destid);
          break;
        } 

        // If EndpointWatch thread is not started bcast_cnt will stay
        // at -1 and this thread will never attemp to set up a tun for
        // peer
        int bcast_cnt = -1;
        int bcast_cnt_in = -1;
        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
        {{
          std::map<uint16_t, DmaPeerCommsStats_t>::iterator itp = info->umd_dma_did_enum_list.find(from_destid);
          if (itp != info->umd_dma_did_enum_list.end())
          bcast_cnt = (bcast_cnt_in = itp->second.bcast_cnt_in) + itp->second.bcast_cnt_out;
        }}
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

        DBG("\n\tGot info [rx_fd=%d] for NEW destid %u peer.{base_rio_addr=0x%llx base_size=%lu rio_addr=0x%llx bufc=%u MTU=%lu} allocated rio_addr=%llu ib_ptr=%p bcast_cnt=%d\n",
            info->umd_mbox_rx_fd,
            from_destid, from_info.base_rio_addr, from_info.base_size, from_info.rio_addr, from_info.bufc, from_info.MTU,
            rio_addr, peer_ib_ptr, bcast_cnt);

         // We want to be doubleplus-sure the peer is ready to receive us once we put the Tun up
        if (bcast_cnt < (min_bcasts * 2) || bcast_cnt_in < min_bcasts) break;

        DBG("\n\tSetting up Tun for NEW peer destid %u\n", from_destid);
        if (! umd_dma_goodput_tun_setup_peer(info, from_destid, &from_info, peer_ib_ptr)) {
          CRIT("\n\tFailed to set up peer for destid=%u, quitting!\n", from_destid);
          goto exit;
        }
      }} while(0);
    }} while(0); // END if FD_ISSET
  } // END while !info->stop_req

exit:
  CRIT("STOPped running! stop_req=%d\n", info->stop_req);
  return;
}

/** \brief Main Battle Tank thread -- Pobeda Nasha!
 * \verbatim
We maintain the (per-peer destid) IB window (or
sub-section thereof) in the following format:
+-4b-+--------+-----+---L2 size+MTU----+------------------+
| RP | peerRP | ... | L2 | L3  payload | ... repeat L2+L3 |
+----+--------+-----+----+-------------+------------------+
We keep (bufc-1) IB L2+L3 combos
 * \endverbatim
 */
extern "C"
void umd_dma_goodput_tun_demo(struct worker *info)
{
  int rc = 0;
  bool dma_fifo_proc_thr_started = false;
  bool dma_tap_thr_started = false;
  
  info->owner_func = umd_dma_goodput_tun_demo;

  const int MHz = getCPUMHz();

  info->umd_nread_threshold = 15 * 1000 * 1000 * MHz; // every 15 sec

  // Note: There's no reason to link info->umd_tx_buf_cnt other than
  // convenience. However the IB ring should never be smaller than
  // info->umd_tx_buf_cnt-1 members -- (dis)counting T3 BD on TX side
  {{
    const int IBWIN_SIZE = sizeof(DmaPeerRP_t) + BD_PAYLOAD_SIZE(info) * (info->umd_tx_buf_cnt-1);

    if (info->ib_ptr == NULL)
      throw std::runtime_error("umd_dma_goodput_tun_demo: NULL IBwin -- CMA disabled?");
    assert(info->ib_byte_cnt >= IBWIN_SIZE);
  }}

  memset(info->ib_ptr, 0, info->ib_byte_cnt);

  if (! umd_check_cpu_allocation(info)) return;
  if (! TakeLock(info, "DMA", info->mp_num, info->umd_chan2)) return;

  for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
    if (! TakeLock(info, "DMA", info->mp_num, ch)) goto exit;
  }

  {{ // Clear on read
    RioMport* mport = new RioMport(info->mp_num, info->mp_h);
    mport->rd32(TSI721_RIO_SP_ERR_STAT);
    mport->rd32(TSI721_RXRSP_BDMA_CNT);
    mport->rd32(TSI721_TXPKT_BDMA_CNT);
    mport->rd32(TSI721_RXPKT_BRG_CNT);
    mport->rd32(TSI721_BRG_PKT_ERR_CNT);
    delete mport;
  }}

  info->umd_dma_fifo_callback = umd_dma_goodput_tun_callback;

  info->umd_peer_ibmap = new IBwinMap(info->ib_rio_addr, info->ib_ptr, info->ib_byte_cnt, (info->umd_tx_buf_cnt-1), info->umd_tun_MTU);

  info->tick_data_total = 0;
  info->tick_count = info->tick_total = 0;

  for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
    if (!umd_dma_goodput_tun_setup_chanN(info, ch)) goto exit;
  }

  if (!umd_dma_goodput_tun_setup_chan2(info)) goto exit;

  info->my_destid = info->umd_dci_nread->rdma->getDestId();

  if (info->my_destid == 0xffff) {
    CRIT("\n\tMy_destid=0x%x which is BORKED -- bad enumeration?\n", info->my_destid);
    goto exit;
  }

  info->umd_epollfd = epoll_create1 (0);
  if (info->umd_epollfd < 0) goto exit;

  socketpair(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, info->umd_sockp_quit);

  {{
    struct epoll_event event;
    event.data.fd = info->umd_sockp_quit[1];
    event.events = EPOLLIN | EPOLLET;
    if(epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, info->umd_sockp_quit[1], &event) < 0) {
      CRIT("\n\tFailed to add umd_sockp_quit[1]=%d to pollset: %s\n", info->umd_sockp_quit[1], strerror(errno));
      goto exit;
    }
  }}

  init_seq_ts(&info->desc_ts, MAX_TIMESTAMPS);
  init_seq_ts(&info->fifo_ts, MAX_TIMESTAMPS);
  init_seq_ts(&info->meas_ts, MAX_TIMESTAMPS);

  info->umd_fifo_proc_must_die = 0;
  info->umd_fifo_proc_alive = 0;

  info->umd_disable_nread  = GetDecParm("$disable_nread", 0);
  info->umd_push_rp_thr    = GetDecParm("$push_rp_thr", 0);
  info->umd_chann_reserve  = GetDecParm("$chann_reserve", 0);

  rc = pthread_create(&info->umd_fifo_thr.thr, NULL,
                      umd_dma_tun_fifo_proc_thr, (void *)info);
  if (rc) {
    CRIT("\n\tCould not create umd_dma_tun_fifo_proc_thr thread, exiting...");
    goto exit;
  }
  sem_wait(&info->umd_fifo_proc_started);

  if (!info->umd_fifo_proc_alive) {
    CRIT("\n\tumd_fifo_proc thread is dead, exiting..");
    goto exit;
  }
  dma_fifo_proc_thr_started = true;

  // Spawn Tap Transmitter Thread
  rc = pthread_create(&info->umd_dma_tap_thr.thr, NULL,
                      umd_dma_tun_RX_proc_thr, (void *)info);
  if (rc) {
    CRIT("\n\tCould not create umd_dma_tun_RX_proc_thr thread, exiting...");
    goto exit;
  }
  sem_wait(&info->umd_dma_tap_proc_started);

  if (!info->umd_dma_tap_proc_alive) {
    CRIT("\n\tumd_dma_tun_RX_proc_thr thread is dead, exiting..");
    goto exit;
  }
  dma_tap_thr_started = true;

  // We need to wake up the RX thread upon quitting so we spawn
  // this lightweight helper thread
  pthread_t wakeup_thr;
  rc = pthread_create(&wakeup_thr, NULL, umd_dma_wakeup_proc_thr, (void *)info);
  if (rc) {
    CRIT("\n\tCould not create umd_dma_wakeup_proc_thr thread, exiting...");
    goto exit;
  }

  zero_stats(info);

  clock_gettime(CLOCK_MONOTONIC, &info->st_time);

again:
  umd_dma_goodput_tun_RDMAD(info,
                            (info->umd_dma_bcast_min = GetDecParm("$bcast_min", 3)) /* min_bcasts */,
                            (info->umd_dma_bcast_interval = GetDecParm("$bcast_interval", 10)) /* bcast_interval, in sec */);

  if (info->stop_req == SOFT_RESTART) {
    CRIT("\n\tSoft restart requested, nuking MBOX hardware!\n");
    //info->umd_dch->softRestart(); // XXX which one?!?!?
    info->stop_req = 0;
    sem_post(&info->umd_fifo_proc_started);
    sem_post(&info->umd_dma_tap_proc_started);
    goto again;
  }

exit:
  CRIT("\n\tEXITING stop_req=%d\n", info->stop_req);

  //if (!info->stop_req) info->stop_req = 1; // Maybe we got here on a local error

  write(info->umd_sockp_quit[0], "X", 1); // Signal Tun/Tap RX thread to eXit
        info->umd_fifo_proc_must_die = 1;

  usleep(500 * 1000); // let detached threads quit

  if (dma_fifo_proc_thr_started) pthread_join(info->umd_fifo_thr.thr, NULL);
  if (dma_tap_thr_started) pthread_join(info->umd_dma_tap_thr.thr, NULL);

  for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
    if (info->umd_dci_list[ch] == NULL) continue;
    DmaChannelInfo_t* dci = info->umd_dci_list[ch];

    if (info->dma_method == ACCESS_UMD) {
      assert(dci->rdma);
      DMAChannel* dch = dynamic_cast<RdmaOpsUMD*>(dci->rdma)->getChannel();

      for (int i = 0; i < dci->tx_buf_cnt; i++) {
        if(dci->dmamem[i].type == 0) continue;
        dch->free_dmamem(dci->dmamem[i]);
      }
    }

    if (info->dma_method == ACCESS_MPORT) {
      assert(dci->rdma);
      RdmaOpsMport* rdma_mport = dynamic_cast<RdmaOpsMport*>(dci->rdma);

      for (int i = 0; i < dci->tx_buf_cnt; i++) {
        if(dci->dmamem[i].type == 0) continue;
        rdma_mport->free_dmamem(dci->dmamem[i]);
      }
    }
  }

  delete info->umd_peer_ibmap;

  info->umd_dma_fifo_callback = NULL;

  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
    do {{
      if (info->umd_dma_did_peer.size() == 0) break;

      for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
        DmaPeer* peer = info->umd_dma_did_peer_list[epi];
        if (peer == NULL) continue; // Just vacated one slot?

        assert(peer->sig == PEER_SIG_UP);

        {{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
          struct epoll_event event;
          event.data.fd = peer->get_tun_fd();
          event.events = EPOLLIN;
          epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
        }}

        // Tell peer we stop?
        umd_dma_goodput_tun_del_ep_signal(info, peer->get_destid());

        peer->stop_req = 1;
        peer->rx_work_sem_post();

        info->umd_dma_did_peer.erase(peer->get_destid());
        info->umd_dma_did_peer_list[epi] = NULL;

        peer->sig = ~0;
        delete peer;
      } // END for peers
    }} while(0);
    info->umd_dma_did_peer.clear();
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  close(info->umd_sockp_quit[0]); close(info->umd_sockp_quit[1]);
  info->umd_sockp_quit[0] = info->umd_sockp_quit[1] = -1;

  close(info->umd_epollfd); info->umd_epollfd = -1;

  for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
    if (info->umd_dci_list[ch] == NULL) continue;
    delete info->umd_dci_list[ch]->rdma;
    free(info->umd_dci_list[ch]); info->umd_dci_list[ch] = NULL;
  }
  delete info->umd_dci_nread->rdma;
  free(info->umd_dci_nread); info->umd_dci_nread = NULL;
  for (int i = 0; i < UMD_NUM_LOCKS; i++) {
    if (info->umd_lock[i] == NULL) continue;
    delete info->umd_lock[i]; info->umd_lock[i] = NULL;
  }
  info->umd_tun_name[0] = '\0';

  // Do NOT close these as are manipulated from another thread!
  info->umd_mbox_rx_fd = -1;
  info->umd_mbox_tx_fd = -1;
}

/** \brief Check whether we have an endpoint
 */
bool umd_dma_goodput_tun_has_ep(struct worker* info, const uint32_t destid)
{
  bool ret = false;
  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  {{
    std::map<uint16_t, DmaPeerCommsStats_t>::iterator it = info->umd_dma_did_enum_list.find(destid);
    if (it != info->umd_dma_did_enum_list.end()) ret = true;
  }}
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
  return ret;
}

void umd_dma_goodput_tun_add_ep(struct worker* info, const uint32_t destid)
{
  time_t now = time(NULL);

  DmaPeerCommsStats_t tmp; memset(&tmp, 0, sizeof(tmp));
  tmp.destid    = destid;
  tmp.on_time   = now;

  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  {{
    info->umd_dma_did_enum_list[destid] = tmp;
  }}
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
}

/** \brief Tell peer (via MBOX) to go away
 */
void umd_dma_goodput_tun_del_ep_signal(struct worker* info, const uint32_t destid)
{
  assert(info);
  assert(info->umd_chan2 >= 0);

  if (info->umd_mbox_tx_fd < 0) return;

  uint8_t buf[sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t)] = {0};

  DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;

  DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));
  payload->action = ACTION_DEL;

  pL2->destid_src = htons(info->my_destid);
  pL2->destid_dst = htons(destid);
  pL2->len        = htons(sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t));

  DBG("\n\tSignalling [tx_fd=%d] peer destid %u that is deleted!\n", info->umd_mbox_tx_fd, destid);

  if (info->umd_mbox_tx_fd < 0) return;

  int nsend = send(info->umd_mbox_tx_fd, buf, sizeof(buf), 0);
  if (nsend < 0) {
    ERR("\n\tSend to MboxWatch thread failed [tx_fd=%d]: %s\n", strerror(errno), info->umd_mbox_tx_fd);
  }
}

/** \brief Check whether we have an "up" Tun for an endpoint 
 */
bool umd_dma_goodput_tun_ep_has_peer(struct worker* info, const uint16_t destid)
{
  assert(info);
  assert(info->umd_chan2 >= 0);

  bool found = false;

  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  {{
    std::map <uint16_t, int>::iterator itp = info->umd_dma_did_peer.find(destid);
    if (itp != info->umd_dma_did_peer.end()) found = true;
  }}
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  return found;
}

void umd_dma_goodput_tun_del_ep(struct worker* info, const uint32_t destid, bool signal)
{
  assert(info);
  //assert(info->umd_dci_nread);

  const uint16_t my_destid = info->my_destid;
  assert(my_destid != destid);

  bool found = false;
  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  {{
    DDBG("\n\t Enum EPs: %d Peers: %d\n", info->umd_dma_did_enum_list.size(), info->umd_dma_did_peer.size());

    // Is it sane to do this hoping the peer will rebroadcast to us
    // when it comes back? This means that unless mport/FMD notifies
    // use AGAIN of this peer we shall never initiate comms with this
    // peer?!?!
    //
    // NO! If both peers nuke each other they shall never talk again.

    if (GetDecParm("$ignore_deadbeats", -1) != -1)
      info->umd_dma_did_enum_list.erase(destid);
    else {
      info->umd_dma_did_enum_list[destid].ls_time       = 0;
      info->umd_dma_did_enum_list[destid].bcast_cnt_in  = 0;
      info->umd_dma_did_enum_list[destid].bcast_cnt_out = 0;
    }

    std::map <uint16_t, int>::iterator itp = info->umd_dma_did_peer.find(destid);
    if (itp == info->umd_dma_did_peer.end()) { goto done; }

    found = true;
    {
    const int slot = itp->second;
    DmaPeer* peer = info->umd_dma_did_peer_list[slot];
    assert(peer);

    {{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
      struct epoll_event event;
      event.data.fd = peer->get_tun_fd();
      event.events = EPOLLIN;
      epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
    }}

    peer->stop_req = 1;
    peer->rx_work_sem_post();
    sched_yield(); // Allow peer to wake up and quit... hopefully. And with mutex held. Yay!

    info->umd_dma_did_peer.erase(itp);
    info->umd_dma_did_peer_list[slot] = NULL;

    peer->sig = ~0;
    delete peer;

    if (slot == (info->umd_dma_did_peer_list_high_wm-1))
      info->umd_dma_did_peer_list_high_wm--;
    }

    // Mark IBwin mapping for peer as free and make RP=0
    info->umd_peer_ibmap->free(destid);
  }}

done:
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  if (found) {
    if (signal) umd_dma_goodput_tun_del_ep_signal(info, destid); // Just in case it was forced we tell him for F*ck Off
    INFO("\n\tNuked peer destid %u%s\n", destid, (signal? " with MBOX notification": " without notification")); 
  } else DBG("\n\tCannot find a peer/Tun for destid %u\n", destid);
}

#ifdef UDMA_TUN_DEBUG_IN
static void decodeInotifyEvent(const struct inotify_event* i, std::string& str)
{
  if (i == NULL) return;

  std::stringstream ss;
  ss << "wd = " << i->wd << " ";
  if (i->cookie > 0)
      ss << "cookie = " << i->cookie;
 
  ss << "mask = ";
  if (i->mask & IN_ACCESS)        ss << " IN_ACCESS ";
  if (i->mask & IN_ATTRIB)        ss << " IN_ATTRIB ";
  if (i->mask & IN_CLOSE_NOWRITE) ss << " IN_CLOSE_NOWRITE ";
  if (i->mask & IN_CLOSE_WRITE)   ss << " IN_CLOSE_WRITE ";
  if (i->mask & IN_CREATE)        ss << " IN_CREATE ";
  if (i->mask & IN_DELETE)        ss << " IN_DELETE ";
  if (i->mask & IN_DELETE_SELF)   ss << " IN_DELETE_SELF ";
  if (i->mask & IN_IGNORED)       ss << " IN_IGNORED ";
  if (i->mask & IN_ISDIR)         ss << " IN_ISDIR ";
  if (i->mask & IN_MODIFY)        ss << " IN_MODIFY ";
  if (i->mask & IN_MOVE_SELF)     ss << " IN_MOVE_SELF ";
  if (i->mask & IN_MOVED_FROM)    ss << " IN_MOVED_FROM ";
  if (i->mask & IN_MOVED_TO)      ss << " IN_MOVED_TO ";
  if (i->mask & IN_OPEN)          ss << " IN_OPEN ";
  if (i->mask & IN_Q_OVERFLOW)    ss << " IN_Q_OVERFLOW ";
  if (i->mask & IN_UNMOUNT)       ss << " IN_UNMOUNT ";
 
  if (i->len > 0)
      ss << " name = " << i->name;

  str = ss.str();
}
#endif // UDMA_TUN_DEBUG_IN

bool umd_check_dma_tun_thr_running(struct worker* info)
{
  if (info == NULL) return false;

  const int tundmathreadindex = info->umd_chan_to; // FUDGE
  if (tundmathreadindex < 0) return false;

  if (1 != wkr[tundmathreadindex].stat) return false;

  return true;
}

/** \brief Thread which watches RIO endpoint coming and going by querying libmport
 * \note It wants the wkr index of the Main Battle Tank thread as it will notify it OOB about ADD/DEL events
 */
extern "C"
void umd_epwatch_demo(struct worker* info)
{
  if (info == NULL) return;

  info->owner_func = umd_epwatch_demo;

  info->umd_mbox_rx_fd = -1;
  info->umd_mbox_tx_fd = -1;

  const int tundmathreadindex = info->umd_chan_to; // FUDGE
  if (tundmathreadindex < 0) return;

  if (info->did != ~0) {
    if (!umd_check_dma_tun_thr_running(info)) {
      CRIT("\n\tWorker thread %d (DMA Tun Thr) is not running. Bye!\n", tundmathreadindex);
      return;
    }
    DBG("\n\tForce-deleting EP destid %u\n", info->did);
    umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], info->did, true);
    return;
  }

  const char* SYS_RAPIDIO_DEVICES = "/sys/bus/rapidio/devices";

  int inf_fd = inotify_init1(IN_CLOEXEC);

  inotify_add_watch(inf_fd, SYS_RAPIDIO_DEVICES, IN_CREATE|IN_DELETE);

  bool first_time = true;
  std::map <uint16_t, bool> ep_map;

  if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

  INFO("\n\tWatching %s for change. Fetching RIO EPs from mport %d\n", SYS_RAPIDIO_DEVICES, info->mp_num);

  while (! info->stop_req) {
    bool inotify_change = false;

    for (int i = 0; i < 10 && !info->stop_req; i++) {
       uint32_t ep_count = 0;
      uint32_t* ep_list = NULL;

      if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

      if (i != 0) {{ // We use select(2) as a fancy sleep and looking at what FMD is doing to /sys/bus/rapidio/devices
        struct timeval to = { 0, 100 * 1000 }; // 100 ms
        fd_set rfds; FD_ZERO (&rfds); FD_SET(inf_fd, &rfds);

        int ret = select(inf_fd+1, &rfds, NULL, NULL, &to);
        if (ret < 0) {
        CRIT("\n\tselect failed on inotify fd: %s\n", strerror(errno));
        goto exit;
        }

        if (FD_ISSET(inf_fd, &rfds)) { // consume inotify event but we use the riomp_mgmt_get_ep_list method to learn ep changes
        uint8_t buf[8192] = {0};;
        int nread = read(inf_fd, buf, 8192); nread += 0;
        inotify_change = true;
#ifdef UDMA_TUN_DEBUG_IN
        const int N = nread / sizeof(struct inotify_event);
        struct inotify_event* in = (struct inotify_event*)buf;
        for (int i = 0; 7 <= g_level && i < N; i++, in++) {
          std::string s;
          decodeInotifyEvent(in, s);
          DBG("\n\tInotify event %d: %s\n", i, s.c_str());
        }
#endif
        }
      }}

      if (info->stop_req) goto exit;
      if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

      if (i == 0 || inotify_change) { // We force reading EP list once a second
        inotify_change = false;
        if (0 != riomp_mgmt_get_ep_list(info->mp_num, &ep_list, &ep_count) || ep_list == NULL) {
          CRIT("\n\triomp_mgmt_get_ep_list failed. All your base are belong to us.\n");
          goto exit;
        }

        if (first_time && 7 <= g_level && ep_count > 0) {
          std::stringstream ss;
          for (int i = 0; i < ep_count; i++) ss << ep_list[i] << " ";
          DBG("\n\tMport %d EPs%s: %s\n", info->mp_num, (first_time? "(1st time)": ""), ss.str().c_str());
        }

        if (first_time) {
          first_time = false;
          for (int i = 0; i < ep_count; i++) {
            const uint16_t destid = ep_list[i];
            ep_map[destid] = true;
            umd_dma_goodput_tun_add_ep(&wkr[tundmathreadindex], destid);
          }
        } else {
          std::map <uint32_t, bool> ep_map_now;
          for (int i = 0; i < ep_count; i++) {
            const uint16_t destid = ep_list[i];
            ep_map_now[destid] = true;
            if (ep_map.find(destid) == ep_map.end()) {
              ep_map[destid] = true;
              DBG("\n\tMport %d EP %u ADD\n", info->mp_num, destid);
              umd_dma_goodput_tun_add_ep(&wkr[tundmathreadindex], destid);
            }
          }
          // Set diff -- find out which destids have gone
          for (std::map <uint16_t, bool>::iterator itm = ep_map.begin(); itm != ep_map.end(); itm++) {
            const uint16_t destid = itm->first;
            if (ep_map_now.find(destid) == ep_map_now.end()) {
              ep_map.erase(destid);
              DBG("\n\tMport %d EP %u DEL\n", info->mp_num, destid);
              umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], destid, true); // notify just in case FMD is wrong
            }
          }
        }
        riomp_mgmt_free_ep_list(&ep_list);
      }
    }
  }
exit:
  close(inf_fd);
  CRIT("STOPped running! stop_req=%d\n", info->stop_req);
  return;

exit_bomb:
  CRIT("\n\tWorker thread %d (DMA Tun Thr) is not running. Bye!\n", tundmathreadindex);
  goto exit;
}

/** \brief Look at destid/EP's Last Seen time stamp and kick out if too old
 * \return true if endpoint exists and has been kicked
 */
bool umd_epwatch_timeout_ep(struct worker *info, const uint16_t destid)
{
  assert(info);

  time_t ls_time = 0;
  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  {{
    std::map<uint16_t, DmaPeerCommsStats_t>::iterator ite = info->umd_dma_did_enum_list.begin();
    if (ite != info->umd_dma_did_enum_list.end()) 
    ls_time = ite->second.ls_time;
  }}
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  do {{
    if (! ls_time) break; // Never contacted us

    // No point nuking enumerated peers which haven't 
    // established a Tun "connection" with us. Perhaps UMD Tun not started yet?
    if (! umd_dma_goodput_tun_ep_has_peer(info, destid)) break;

    int dT = time(NULL) - ls_time;
    if (dT < (info->umd_dma_bcast_min * info->umd_dma_bcast_interval)) break; // Grace period

    umd_dma_goodput_tun_del_ep(info, destid, false);
    return true;
  }} while(0);

  return false;
}
/** \brief Thread which minds a MBOX and a socketpair
 * \note It wants the wkr index of the Main Battle Tank thread so it can communicate with it via socketpair(2): reads data from socketpair, sends over MBOX, reads data from MBOX, sends over socketpair
 * \note It modifies Main Battle Tank thread's struct worker { umd_mbox_tx_fd, umd_mbox_rx_fd }
 */
extern "C"
void umd_mbox_watch_demo(struct worker *info)
{
  uint64_t rx_ok = 0;
  uint64_t tx_ok = 0;
  int sockp1[2] = {-1};
  int sockp2[2] = {-1};
  int umd_mbox_tx_fd = -1;
  int umd_mbox_rx_fd = -1;

  info->owner_func = umd_mbox_watch_demo;

  info->umd_mbox_rx_fd = -1;
  info->umd_mbox_tx_fd = -1;

  const int tundmathreadindex = info->umd_chan_to; // FUDGE
  if (tundmathreadindex < 0) return;

  RioMport* mport = new RioMport(info->mp_num, info->mp_h);

  // Clear on read
  mport->rd32(TSI721_TXPKT_SMSG_CNT); // aka 0x41410  Sent Packet Count of Messaging Engine Register
  mport->rd32(TSI721_RXPKT_SMSG_CNT); // aka 0x29900  Received Packet Count for Messaging Engine Register

  if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

  if (! TakeLock(info, "MBOX", info->mp_num, info->umd_chan)) goto exit;

  info->umd_mch = new MboxChannel(info->mp_num, info->umd_chan, info->mp_h);

  if (NULL == info->umd_mch) {
    CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
         info->umd_chan, info->mp_num, info->mp_h);
    info->umd_tun_name[0] = '\0';
    close(info->umd_tun_fd); info->umd_tun_fd = -1;
    delete info->umd_lock[0]; info->umd_lock[0] = NULL;
    goto exit_bomb;
  };

  // I only send only 1 MBOX message at a time

  if (! info->umd_mch->open_mbox(MBOX_BUFC, MBOX_STS)) {
    CRIT("\n\tMboxChannel: Failed to open mbox!");
    info->umd_tun_name[0] = '\0';
    close(info->umd_tun_fd); info->umd_tun_fd = -1;
    delete info->umd_mch; info->umd_mch = NULL;
    delete info->umd_lock[0]; info->umd_lock[0] = NULL;
    goto exit_bomb;
  }

  socketpair(PF_LOCAL, SOCK_DGRAM | SOCK_CLOEXEC, 0, sockp1); // SOCK_DGRAM so we have a message delineation
  socketpair(PF_LOCAL, SOCK_DGRAM | SOCK_CLOEXEC, 0, sockp2); // SOCK_DGRAM so we have a message delineation

  umd_mbox_tx_fd = sockp1[0];
  umd_mbox_rx_fd = sockp2[1];

  if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

  wkr[tundmathreadindex].umd_mbox_tx_fd = sockp2[0];
  if (wkr[tundmathreadindex].umd_set_rx_fd != NULL)
       wkr[tundmathreadindex].umd_set_rx_fd(&wkr[tundmathreadindex], sockp1[1]);
  else wkr[tundmathreadindex].umd_mbox_rx_fd = sockp1[1];

  INFO("\n\tWatching MBOX %d [tx_fd=%d, rx_fd=%d] sockp (%d,%d) (%d,%d)\n",
       info->umd_chan, umd_mbox_tx_fd, umd_mbox_rx_fd,
       sockp1[0], sockp1[1],
       sockp2[0], sockp2[1]);

  info->umd_mch->setInitState();

  {{
    uint8_t buf[PAGE_4K] = {0};

    MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
    opt.mbox   = info->umd_chan;

    for(int i = 0; i < MBOX_BUFC; i++) {
      void* b = calloc(1, PAGE_4K);
      info->umd_mch->add_inb_buffer(b);
    }

    MboxChannel::WorkItem_t wi[MBOX_STS*8]; memset(wi, 0, sizeof(wi));

// Try a soft restart before running the loop to see if that avoids later timeout.
   info->umd_mch->softRestart();

    while (! info->stop_req) {
      int i = 0;
      if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

// Receive from socketpair(2), send on MBOX

      struct timeval to = { 0, 1 * 1000 }; // 1 ms
      fd_set rfds; FD_ZERO (&rfds); FD_SET(umd_mbox_rx_fd, &rfds);

      int ret = select(umd_mbox_rx_fd+1, &rfds, NULL, NULL, &to);
      if (ret < 0) {
        CRIT("\n\tselect failed on umd_mbox_rx_fd: %s\n", strerror(errno));
        goto exit;
      }

      if (info->stop_req) goto exit;
      if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

      if (FD_ISSET(umd_mbox_rx_fd, &rfds)) { // Sumthin to send on MBOX, read only 1st dgram
        const int nread = recv(umd_mbox_rx_fd, buf, PAGE_4K, 0);

        DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
        assert(nread == ntohs(pL2->len));
        pL2->mbox_src = info->umd_chan;

        DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));

        bool q_was_full = false;
        opt.destid = ntohs(pL2->destid_dst);
        DBG("\n\tSending %d bytes to RIO destid %u over MBOX action=0x%x\n", nread, opt.destid, payload->action);
        MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;
        if (! info->umd_mch->send_message(opt, buf, nread, true, fail_reason)) {
          if (fail_reason == MboxChannel::STOP_REG_ERR) {
            DBG("\n\tsend_message FAILED! TX q_size=%d destid=%u. Soft MBOX restart.\n",
                info->umd_mch->queueTxSize(), opt.destid);

            info->umd_mch->softRestart();

            // No point nuking enumerated peers which haven't 
            // established a Tun "connection" with us. Perhaps UMD Tun not started yet?
            if (umd_check_dma_tun_thr_running(info) /*&& umd_dma_goodput_tun_ep_has_peer(&wkr[tundmathreadindex], opt.destid)*/)
              umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], opt.destid, false);

            goto receive;
          } else { q_was_full = true; }
        } else { tx_ok++; }

        DDBG("\n\tPolling FIFO transfer completion destid=%d TX q_size = %d\n", opt.destid, info->umd_mch->queueTxSize());
        for (i = 0;
               !q_was_full && !info->stop_req && (i < 100000) &&
               info->umd_mch->queueTxSize();
               i++) {
          info->umd_mch->scanFIFO(wi, MBOX_STS*8);
          usleep(1);
        }

        if (info->stop_req) goto exit;
        if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

        if (info->umd_mch->queueTxSize() > 0) {
          DBG("\n\tTX queue non-empty for destid=%u. Soft MBOX restart.%s tx_ok=%d Run \"udd %d\" for perf counters.  %d attempts\n",
              opt.destid, (q_was_full? "Q FULL?": ""), tx_ok, tundmathreadindex, i);

          info->umd_mch->softRestart();

          if (umd_check_dma_tun_thr_running(info) /*&& umd_dma_goodput_tun_ep_has_peer(&wkr[tundmathreadindex], opt.destid)*/)
            umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], opt.destid, false);

          goto receive;
        } // END if queueSize

        if (info->stop_req) goto exit;
        if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

        // Hmm go and lock a mutex unprovoked. Despicable
        umd_epwatch_timeout_ep(&wkr[tundmathreadindex], opt.destid);
      } // END if FD_ISSET

  receive:
      if (info->stop_req) goto exit;
      if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

// Receive from MBOX, send to umd_mbox_rx_fd

      bool message_ready = false;
      for (int i = 0; !info->stop_req && i < 10000; i++) {
        uint64_t rx_ts = 0;
        if (info->umd_mch->inb_message_ready(rx_ts)) { message_ready = true; break; }
        if (info->stop_req) goto exit;
        if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;
        usleep(1);
      }

      if (info->stop_req) goto exit;
      if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

      if (!message_ready) continue;

      uint8_t* buf = NULL;
      MboxChannel::MboxOptions_t opt_rx; memset(&opt_rx, 0, sizeof(opt));
      while ((buf = (uint8_t*)info->umd_mch->get_inb_message(opt_rx)) != NULL) {
        if (info->stop_req) goto exit;
        if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

        DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
        assert(opt_rx.bcount >= ntohs(pL2->len));

        DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));
        DBG("\n\tGot %d/%d bytes on RIO MBOX from destid %u [tx_fd=%d] action=0x%x\n", opt_rx.bcount,
            ntohs(pL2->len), ntohs(pL2->destid_src),
            umd_mbox_tx_fd, payload->action);

        if (send(umd_mbox_tx_fd, buf, ntohs(pL2->len), MSG_DONTWAIT) < 0 &&
            (errno != EAGAIN && errno != EWOULDBLOCK)) {
          CRIT("\n\tsend failed [tx_fd=%d]: %s\n", umd_mbox_tx_fd, strerror(errno));
          goto exit;
        }
        
        info->umd_mch->add_inb_buffer(buf); // recycle
        rx_ok++;
      } // END while get_inb_message
    } // END while !stop_req
  }}

exit:
  if (umd_check_dma_tun_thr_running(info)) {
    wkr[tundmathreadindex].umd_mbox_tx_fd = -1;
    wkr[tundmathreadindex].umd_mbox_rx_fd = -1;
  }

  close(sockp1[0]); close(sockp1[1]);
  close(sockp2[0]); close(sockp2[1]);

  delete mport;
  delete info->umd_mch;  info->umd_mch = NULL;
  delete info->umd_lock[0]; info->umd_lock[0] = NULL;

  CRIT("STOPped running! stop_req=%d\n", info->stop_req);
  return;

exit_bomb:
  CRIT("\n\tWorker thread %d (DMA Tun Thr) is not running. Bye!\n", tundmathreadindex);
  goto exit;
}

void UMD_DD_SS(struct worker* info, std::stringstream& out)
{
  int q_size[6] = {0};
  bool     port_ok[6] = {0};
  bool     port_err[6] = {0};
  uint32_t port_WP[6] = {0};
  uint64_t port_FIFO_WP[6] = {0};
  uint64_t port_ticks_total[6] = {0};
  uint64_t port_total_ticks_tx[6] = {0};
  uint64_t* port_tx_histo[6] = {NULL};
  DmaChannelInfo_t* dch_list[6] = {0};

  assert(info->umd_dci_nread);

  std::string s;
  if (info->umd_peer_ibmap->toString(s) > 0)
    out << "IBwin mappings:\n" << s;

  RioMport* mport = new RioMport(info->mp_num, info->mp_h);

  {{
    char tmp[257] = {0};
    std::stringstream ss;

    snprintf(tmp, 256, "RIO_SP_ERR_STAT=0x%x\n", mport->rd32(TSI721_RIO_SP_ERR_STAT));
    ss << "\t" << tmp;

    snprintf(tmp, 256, "MBOX: TXPKT_SMSG_CNT=%u RXPKT_SMSG_CNT=%u\n",
                       mport->rd32(TSI721_TXPKT_SMSG_CNT), mport->rd32(TSI721_RXPKT_SMSG_CNT));
    ss << "\t" << tmp;
    snprintf(tmp, 256, "DMA: TXPKT_BDMA_CNT=%u RXRSP_BDMA_CNT=%u\n",
                        mport->rd32(TSI721_TXPKT_BDMA_CNT), mport->rd32(TSI721_RXRSP_BDMA_CNT));
    ss << "\t" << tmp;
    snprintf(tmp, 256, "IBwin: RXPKT_BRG_CNT=%u BRG_PKT_ERR_CNT=%u\n",
                        mport->rd32(TSI721_RXPKT_BRG_CNT), mport->rd32(TSI721_BRG_PKT_ERR_CNT));
    ss << "\t" << tmp;

    out << "Perf counters:\n" << ss.str();
  }}

  const int TX_HISTO_SZ = sizeof(uint64_t) * info->umd_tx_buf_cnt;
  const int RX_HISTO_SZ = TX_HISTO_SZ;

  const int MHz = getCPUMHz();

  int dch_cnt = 0;
  std::stringstream ss;

  if (info->dma_method == ACCESS_UMD) {
    for (int ch = info->umd_chan; info->umd_chan >= 0 && ch <= info->umd_chan_n; ch++) {
      assert(info->umd_dci_list[ch]);

      dch_list[dch_cnt]         = info->umd_dci_list[ch];

      DMAChannel* dch = dynamic_cast<RdmaOpsUMD*>(info->umd_dci_list[ch]->rdma)->getChannel();
      assert(dch);

      q_size[dch_cnt]           = dch->queueSize();
      port_ok[dch_cnt]          = dch->checkPortOK();
      port_err[dch_cnt]         = dch->checkPortError();
      port_WP[dch_cnt]          = dch->getWP();
      port_FIFO_WP[dch_cnt]     = dch->m_tx_cnt;
      port_ticks_total[dch_cnt] = info->umd_dci_list[ch]->ticks_total;

      port_total_ticks_tx[dch_cnt] = info->umd_dci_list[ch]->total_ticks_tx;

#ifdef UDMA_TUN_DEBUG_HISTO
      if (dch->m_bl_busy_histo != NULL) {
        port_tx_histo[dch_cnt] = (uint64_t*)alloca(TX_HISTO_SZ);
        memcpy(port_tx_histo[dch_cnt], (void*)dch->m_bl_busy_histo, TX_HISTO_SZ);
      }
#endif

      dch_cnt++;
    } // END for umd_chan

    DMAChannel* dch2 = dynamic_cast<RdmaOpsUMD*>(info->umd_dci_nread->rdma)->getChannel();
    assert(dch2);

    char tmp[257] = {0};
    snprintf(tmp, 256, "Chan2 %d q_size=%d", info->umd_chan2, dch2->queueSize());
    ss << "\n\t" << tmp;
    ss << "      WP=" << dch2->getWP() << " FIFO.WP=" << dch2->m_tx_cnt;
#if 0
    if (dch2->m_tx_cnt > 0) {
      float AvgUS = ((float)info->umd_ticks_total_chan2 / dch2->m_tx_cnt) / MHz;
      ss << " AvgNRTxRx=" << AvgUS << "uS";
    }
#endif
    if (dch2->checkPortOK()) ss << " ok";
    if (dch2->checkPortError()) ss << " ERROR";

    for (int ch = 0; ch < dch_cnt; ch++) {
      assert(dch_list[ch]);
      const uint64_t* tx_histo = port_tx_histo[ch];

      char tmp[257] = {0};
      snprintf(tmp, 256, "Chan  %d q_size=%d oi=%d", dch_list[ch]->chan, q_size[ch], dch_list[ch]->oi);
      ss << "\n\t" << tmp;
      ss << " WP=" << port_WP[ch];
      ss << " FIFO.WP=" << port_FIFO_WP[ch];
      if (port_FIFO_WP[ch] > 0) {
        float AvgUS = ((float)port_ticks_total[ch] / port_FIFO_WP[ch]) / MHz;
        ss << " AvgNWTX=" << AvgUS << "uS";
        float AvgUSTun = ((float)port_total_ticks_tx[ch] / port_FIFO_WP[ch]) / MHz;
        ss << " AvgTxTun=" << AvgUSTun << "uS";
      }
      if (port_ok[ch]) ss << " ok";
      if (port_err[ch]) ss << " ERROR";

      std::stringstream ss_histo;
      for (int i = 0; i < info->umd_tx_buf_cnt && tx_histo != NULL; i++) {
        if (tx_histo[i] == 0) continue;
        ss_histo << i << "->" << tx_histo[i] << " ";
      }

      if (GetEnv("verb") != NULL && ss_histo.str().size() > 0) ss << "\n\tTX Histo: " << ss_histo.str();
    }
    if (dch_cnt > 0)
      out << "DMA Channel stats: " << ss.str() << "\n";
  } // END if dma_method == ACCESS_UMD 

  std::vector<DmaPeer>     peer_list;
  std::vector<int>         peer_list_rocnt;
  std::vector<uint64_t*>   peer_list_rxhisto;
  std::vector<DmaPeerCommsStats_t> peer_list_enum;

  time_t now = time(NULL);

  // Collect snapshot of peers quickly

  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
  {{
    std::map<uint16_t, DmaPeerCommsStats_t>::iterator ite = info->umd_dma_did_enum_list.begin();
    for (; ite != info->umd_dma_did_enum_list.end(); ite++) {
      if (ite->first == info->my_destid) continue;
      peer_list_enum.push_back(ite->second);
    }

    std::map <uint16_t, int>::iterator itp = info->umd_dma_did_peer.begin();
    for (; itp != info->umd_dma_did_peer.end(); itp++) {
      uint64_t* rxhisto = NULL;

#ifdef UDMA_TUN_DEBUG_HISTO
      if (info->umd_dma_did_peer_list[itp->second]->m_ib_histo != NULL) { // operator= does not copy m_ib_histo
        rxhisto = (uint64_t*)alloca(RX_HISTO_SZ);
        memcpy(rxhisto, (void*)info->umd_dma_did_peer_list[itp->second]->m_ib_histo, RX_HISTO_SZ);
      }
#endif
      peer_list_rxhisto.push_back(rxhisto);

      peer_list_rocnt.push_back(info->umd_dma_did_peer_list[itp->second]->count_RO()); // operator= does not copy m_rio_rx_bd_L2_ptr

      peer_list.push_back(*info->umd_dma_did_peer_list[itp->second]); // Yeehaw! Make a copy of class!!
    } // END for peers
  }}
  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

  // Prevent freeing of resources in copy object
  for (int ip = 0; ip < peer_list.size(); ip++) { peer_list[ip].set_copy(); }

  if (peer_list_enum.size() > 0) {
    std::stringstream ss;
    for (int ip = 0; ip < peer_list_enum.size(); ip++) {
      char tmp[257] = {0};
      snprintf(tmp, 256, "Did %u Age %ds LS %ds bcast_cnt {out=%d in=%d} RIO addr sent 0x%llx",
                         peer_list_enum[ip].destid,
                         (peer_list_enum[ip].on_time > 0)? now-peer_list_enum[ip].on_time: -1,
                         (peer_list_enum[ip].ls_time > 0)? now-peer_list_enum[ip].ls_time: -1,
                         peer_list_enum[ip].bcast_cnt_out,
                         peer_list_enum[ip].bcast_cnt_in,
                         peer_list_enum[ip].my_rio_addr);
      ss << "\n\t" << tmp;
    }
    char tmp[65] = {0};
    snprintf(tmp, 64, "Got %d enumerated peer(s): ", peer_list_enum.size());
    out << tmp << ss.str() << "\n";
  }

  if (peer_list.size() > 0) {
    std::stringstream ss;
    for (int ip = 0; ip < peer_list.size(); ip++) {
      DmaPeer& peer = peer_list[ip];
      const int rocnt = peer_list_rocnt[ip];
      const uint64_t* rx_histo = peer_list_rxhisto[ip];

      float dT_RP = 0;
      const uint64_t LS_rp_update = peer.get_RP_lastSeen();
      if (LS_rp_update > 0) {
        dT_RP = (rdtsc() - LS_rp_update) / MHz; // this is uS
        dT_RP /= 1000; // convert to mS
      }

      char tmp[257] = {0};
      snprintf(tmp, 256, "Did %u %s peer.rio_addr=0x%llx/0x%x tx.WP=%u tx.RP~%u tx.rpUC=%u tx.rpLS=%fmS\n\t\ttx.RIO=%llu rx.RIO=%llu\n\t\tTun.rx=%llu Tun.tx=%llu Tun.txerr=%llu\n\t\ttx.peer_full=%llu", 
         peer.get_destid(), peer.get_tun_name(),
         peer.get_rio_addr(), peer.get_ibwin_size(),
         peer.get_WP(), peer.get_RP(), peer.get_RP_serial(), dT_RP,
         peer.m_stats.tx_cnt, peer.m_stats.rx_cnt,
         peer.m_stats.tun_rx_cnt, peer.m_stats.tun_tx_cnt, peer.m_stats.tun_tx_err,
               peer.m_stats.rio_tx_peer_full
        );
      ss << "\n\t" << tmp;

      snprintf(tmp, 256, "\n\t\tpushRP=%llu pushForceRP=%llu",
                         peer.m_stats.push_rp_cnt, peer.m_stats.push_rp_force_cnt);
      ss << tmp;

      volatile DmaPeerRP_t* pRP = (DmaPeerRP_t*)peer.get_ib_ptr();
      assert(pRP);

      float TotalTimeSpentFull = (float)peer.m_stats.rio_rx_peer_full_ticks_total / MHz;
      snprintf(tmp, 256, "\n\t\trx.RP=%u #IBBdRO=%d IBBdReady=%d #IsolIBPass=%llu #IsolIBPassAdd=%llu #IBPass=%llu IBBDFullTotal=%fuS",
                         pRP->RP, rocnt, peer.get_rio_rx_bd_ready_size(),
                         peer.m_stats.rio_isol_rx_pass, 
                         peer.m_stats.rio_isol_rx_pass_add, 
                         peer.m_stats.rio_rx_pass,
                         TotalTimeSpentFull);
      ss << tmp;

      if (peer.m_stats.tun_tx_cnt > 0) {
        char tmp[65] = {0};
        float AvgUS = ((float)peer.m_stats.total_ticks_rx / peer.m_stats.tun_tx_cnt) / MHz;
        snprintf(tmp, 64, "\n\t\tAvgSoftRX=%fuS", AvgUS);
        ss << tmp;
      }
#ifdef UDMA_TUN_DEBUG_SPLOCK
      if (peer.m_stats.rio_isol_rx_pass_spl > 0) {
        float AvgTck = ((float)peer.m_stats.rio_isol_rx_pass_spl_ts / peer.m_stats.rio_isol_rx_pass_spl);
        char tmp[65] = {0};
        snprintf(tmp, 64, "\n\t\tAvgTckSplock=%f (%fuS) MaxTckSplock=%llu", AvgTck, AvgTck/MHz,
                          peer.m_stats.rio_isol_rx_pass_spl_ts_max);
        ss << tmp;
      }
#endif // UDMA_TUN_DEBUG_SPLOCK

      std::stringstream ss_histo;
      for (int i = 0; i < info->umd_tx_buf_cnt && rx_histo != NULL; i++) {
        if (rx_histo[i] == 0) continue;
        ss_histo << i << "->" << rx_histo[i] << " ";
      }

      if (GetEnv("verb") != NULL && ss_histo.str().size() > 0) ss << "\n\tRX Histo: " << ss_histo.str();

      if (peer.get_mutex().__data.__lock) {
        snprintf(tmp, 256, "\n\t\tlocker.tid=0x%x", peer.get_mutex().__data.__owner);
        ss << tmp;
      }
    }

    char tmpo[65] = {0};
    snprintf(tmpo, 64, "Got %d UP peer(s): ", peer_list.size());
    out << tmpo << ss.str() << "\n";
  }

  delete mport;
}


/** \brief Dump UMD DMA Tun status
 * \note This executes within the CLI thread but targets Main Battle Tank thread's data
 */
extern "C"
void UMD_DD(struct worker* info)
{
  assert(info);
  assert(info->umd_dci_nread);

  std::stringstream out;
  UMD_DD_SS(info, out);
  printf("%s", out.str().c_str());
}

extern "C"
void UMD_Test(struct worker* info)
{
  assert(info->umd_chan2 >= 0);
  assert(info->ib_rio_addr);

  uint32_t u4 = 0;
  bool r = udma_nread_mem(info->umd_dci_nread->rdma, info->did, info->ib_rio_addr, sizeof(u4), (uint8_t*)&u4);

  INFO("\n\tNREAD %s did=%d rio_addr=0x%llx => %u\n", (r? "ok": "FAILED"), info->did, info->ib_rio_addr, u4);
}
