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

#include <stdio.h> // snprintf

#include "memops.h"
#include "memops_mport.h"
#include "memops_umdd.h"

#define UMD_SLEEP_NS    1 // Setting this to 0 will compile out nanosleep syscall

RIOMemOpsUMDd::RIOMemOpsUMDd(const int mport_id, const int chan) : RIOMemOpsMport(mport_id)
{
  static char tmp[129] = {0};

  m_errno = 0;
  m_cookie_cutter = 0;

  memset(&m_stats, 0, sizeof(m_stats));

  m_dch = DMAChannelSHM_create(mport_id, chan); // Will throw on error

  if (m_dch == NULL) { // Cannot load SO
    snprintf(tmp, 128, "RIOMemOpsUMDd: DMAChannelSHM_create returned NULL!");
    goto thr;
  }

  if (! DMAChannelSHM_pingMaster(m_dch)) { // Ping master process via signal(0)
    snprintf(tmp, 128, "RIOMemOpsUMDd: Master UMDs is not running!");
    goto thr;
  }

  return;

thr:
  if (m_dch) DMAChannelSHM_destroy(m_dch);
  riomp_mgmt_mport_destroy_handle(&m_mp_h);
  throw std::runtime_error(tmp);
}

RIOMemOpsUMDd::~RIOMemOpsUMDd()
{
  m_asyncm.clear();
  DMAChannelSHM_destroy(m_dch);
  // Note: m_mp_h destroyed by ~RIOMemOpsMport
}

bool RIOMemOpsUMDd::nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  m_errno = 0;
  dmaopt.ticket = 0;

  if (dmaopt.mem.type != DMAMEM) 
    throw std::runtime_error("RIOMemOpsUMDd::nwrite_mem: Unsupported memory type!");
  
  if (! DMAChannelSHM_checkMasterReady(m_dch)) { m_errno = ENOTCONN; return false; }

  if (DMAChannelSHM_queueFull(m_dch)) { m_errno = ENOSPC; return false; }

#ifdef MEMOPS_DEBUG
  printf("UMDD %s: destid=%u handle=0x%lx rio_addr=0x%lx+0x%x bcount=%d op=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, wr_mode, sync);
#endif

  DMAChannelSHM::DmaOptions_t opt; memset(&opt, 0, sizeof(opt));
  opt.destid      = dmaopt.destid;
  opt.tt_16b      = (bool)dmaopt.tt_16b;
  opt.prio        = dmaopt.prio;
  opt.crf         = dmaopt.crf;
  opt.bcount      = dmaopt.bcount;
  opt.raddr.lsb64 = dmaopt.raddr.lsb64;
  opt.raddr.msb2  = dmaopt.raddr.msb2;

  RioMport::DmaMem_t dmamem; memset(&dmamem, 0, sizeof(dmamem));

  dmamem.type       = RioMport::DONOTCHECK;
  dmamem.win_handle = dmaopt.mem.win_handle + dmaopt.mem.offset;
  dmamem.win_size   = dmaopt.mem.win_size;

  bool q_was_full = false;
  uint32_t dma_abort_reason = 0;

  if (dmaopt.bcount > 16) {
    if (! DMAChannelSHM_queueDmaOpT1(m_dch, DMAChannelSHM::convert_riomp_dma_directio(dmaopt.wr_mode),
                                     &opt, &dmamem, &dma_abort_reason, &m_stats)) {
      if (q_was_full) { m_errno = ENOSPC; return false; }
      m_errno = EINVAL; return false;
    }
  } else {
    if (! DMAChannelSHM_queueDmaOpT2(m_dch, DMAChannelSHM::convert_riomp_dma_directio(dmaopt.wr_mode),
                                     &opt, (uint8_t *)dmaopt.mem.win_handle + dmaopt.mem.offset, dmaopt.bcount, &dma_abort_reason, &m_stats)) {
      if (q_was_full) { m_errno = ENOSPC; return false; }
      m_errno = EINVAL; return false;
    }
  }

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_FAF) return true;

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
    uint32_t cookie = ++m_cookie_cutter;
    m_asyncm[cookie] = opt;
    dmaopt.ticket = cookie;
    return cookie;
  }

  // Only left RIO_DIRECTIO_TRANSFER_SYNC
  for (int cnt = 0;; cnt++) {
    const DMAChannelSHM::TicketState_t st = (DMAChannelSHM::TicketState_t)DMAChannelSHM_checkTicket(m_dch, &opt);
    if (st == DMAChannelSHM::UMDD_DEAD) { m_errno = ENOTCONN; return false; }
    if (st == DMAChannelSHM::COMPLETED) break;
    if (st == DMAChannelSHM::INPROGRESS) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
      struct timespec sl = {0, UMD_SLEEP_NS};
      if (cnt == 0) {
        uint64_t total_data_pending = 0;
        DMAChannelSHM_getShmPendingData(m_dch, &total_data_pending, NULL);
        if (total_data_pending > UMD_SLEEP_NS) sl.tv_nsec = total_data_pending;
      }
      nanosleep(&sl, NULL);
#endif
      continue;
    }
    if (st == DMAChannelSHM::BORKED) {
      uint64_t t = 0;
      const int deq = DMAChannelSHM_dequeueFaultedTicket(m_dch, &t);
      if (deq)
           fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d) dequeued faulted ticket %lu\n", __func__, opt.ticket, st, t);
      else fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d)\n", __func__, opt.ticket, st);
      m_errno = EIO; return false;
    }
  }

  return true;
}

bool RIOMemOpsUMDd::nread_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  m_errno = 0;
  dmaopt.ticket = 0;

  if (dmaopt.mem.type != DMAMEM) 
    throw std::runtime_error("RIOMemOpsUMDd::nwrite_mem: Unsupported memory type!");

#ifdef MEMOPS_DEBUG
  printf("UMDD %s: destid=%u handle=0x%lx  rio_addr=0x%lx+0x%x\n bcount=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, sync);
#endif

  if (! DMAChannelSHM_checkMasterReady(m_dch)) { m_errno = ENOTCONN; return false; }

  if (DMAChannelSHM_queueFull(m_dch)) { m_errno = ENOSPC; return false; }

  DMAChannelSHM::DmaOptions_t opt;
  memset(&opt, 0, sizeof(opt));
  opt.destid      = dmaopt.destid;
  opt.tt_16b      = (bool)dmaopt.tt_16b;
  opt.prio        = dmaopt.prio;
  opt.crf         = dmaopt.crf;
  opt.bcount      = dmaopt.bcount;
  opt.raddr.lsb64 = dmaopt.raddr.lsb64;
  opt.raddr.msb2  = dmaopt.raddr.msb2;

  RioMport::DmaMem_t dmamem; memset(&dmamem, 0, sizeof(dmamem));

  dmamem.type       = RioMport::DONOTCHECK;
  dmamem.win_handle = dmaopt.mem.win_handle + dmaopt.mem.offset;
  dmamem.win_size   = dmaopt.bcount;

  bool q_was_full = false;
  uint32_t dma_abort_reason = 0;

  if (! DMAChannelSHM_queueDmaOpT1(m_dch, NREAD, &opt, &dmamem, &dma_abort_reason, &m_stats)) {
    if (q_was_full) { m_errno = ENOSPC; return false; }
    m_errno = EINVAL; return false;
  }

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_FAF) return true;

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
    uint32_t cookie = ++m_cookie_cutter;
    m_asyncm[cookie] = opt;
    dmaopt.ticket = cookie;
    return cookie;
  }

  // Only left RIO_DIRECTIO_TRANSFER_SYNC
  for (int cnt = 0;; cnt++) {
    const DMAChannelSHM::TicketState_t st = (DMAChannelSHM::TicketState_t)DMAChannelSHM_checkTicket(m_dch, &opt);
    if (st == DMAChannelSHM::UMDD_DEAD) { m_errno = ENOTCONN; return false; }
    if (st == DMAChannelSHM::COMPLETED) break;
    if (st == DMAChannelSHM::INPROGRESS) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
      struct timespec sl = {0, UMD_SLEEP_NS};
      if (cnt == 0) {
        uint64_t total_data_pending = 0;
        DMAChannelSHM_getShmPendingData(m_dch, &total_data_pending, NULL);
        if (total_data_pending > UMD_SLEEP_NS) sl.tv_nsec = total_data_pending;
      }
      nanosleep(&sl, NULL);
#endif
      continue;
    }
    if (st == DMAChannelSHM::BORKED) {
      uint64_t t = 0;
      const int deq = DMAChannelSHM_dequeueFaultedTicket(m_dch, &t);
      if (deq)
           fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d) dequeued faulted ticket %lu\n", __func__, opt.ticket, st, t);
      else fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d)\n", __func__, opt.ticket, st);
      m_errno = EIO; return false;
    }
  }

  return true;
}

bool RIOMemOpsUMDd::wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/)
{
  m_errno = 0;

  if (dmaopt.ticket <= 0) return false;

  if (! DMAChannelSHM_checkMasterReady(m_dch)) { m_errno = ENOTCONN; return false; }

  std::map<uint64_t, DMAChannelSHM::DmaOptions_t>::iterator it = m_asyncm.find(dmaopt.ticket);

  if (it == m_asyncm.end()) // Tough luck
    throw std::runtime_error("RIOMemOpsUMDd::wait_async: Requested cookie not found in internal database");

  DMAChannelSHM::DmaOptions_t opt = it->second;

  m_asyncm.erase(it); // XXX This takes a lot of time :()

  uint64_t now = 0;
  while ((now = rdtsc()) < opt.not_before) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
    struct timespec sl = {0, UMD_SLEEP_NS};
    nanosleep(&sl, NULL);
#endif
  }

  for (int cnt = 0 ;; cnt++) {
    const DMAChannelSHM::TicketState_t st = (DMAChannelSHM::TicketState_t)DMAChannelSHM_checkTicket(m_dch, &opt);
    if (st == DMAChannelSHM::UMDD_DEAD) { m_errno = ENOTCONN; return false; }
    if (st == DMAChannelSHM::COMPLETED) return true;
    if (st == DMAChannelSHM::BORKED) {
      uint64_t t = 0;
      DMAChannelSHM_dequeueFaultedTicket(m_dch, &t);
      fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d) dequeued faulted ticket %lu\n", __func__, opt.ticket, st, t);
      m_errno = EIO; return false;
    }

    // Last-ditch wait ??
    if (st == DMAChannelSHM::INPROGRESS) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
      struct timespec sl = {0, UMD_SLEEP_NS};
      sl.tv_nsec = opt.not_before_dns;
      nanosleep(&sl, NULL);
#endif
      continue;
    }
  }

  // Should not reach here
  m_errno = EINVAL;
  return false;
}

const char* RIOMemOpsUMDd::abortReasonToStr(int abort_reason)
{
  return strerror(m_errno);
}

int RIOMemOpsUMDd::getAbortReason() { return m_errno; }
