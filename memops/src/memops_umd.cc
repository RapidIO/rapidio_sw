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

#include <stdio.h>
#include <sched.h>
#include <assert.h>

#include "memops.h"
#include "memops_mport.h"
#include "memops_umd.h"

#include "dmachanshm.h"


static int getCPUCount()
{
  FILE* f = fopen("/proc/cpuinfo", "rte");

  int count = 0;
  while (! feof(f)) {
    char buf[257] = {0};
    fgets(buf, 256, f);
    if (buf[0] == '\0') break;
    if (strstr(buf, "processor\t:")) count++;
  }

  fclose(f);

  return count;
}

static bool migrate_thread_to_cpu(const int cpu_req, int& cpu_run, const pthread_t thr)
{
  cpu_set_t cpuset; CPU_ZERO(&cpuset);

  int chk_cpu_lim = 10;

  const int cpu_count = getCPUCount();

  if (-1 == cpu_req) {
    for(int c = 0; c < cpu_count; c++) CPU_SET(c, &cpuset);
  } else {
    if (cpu_req >= cpu_count) {
      fprintf(stderr, "\n\tInvalid cpu %d cpu count is %d\n", cpu_req, cpu_count);
      return false;
    }
    CPU_SET(cpu_req, &cpuset);
  }

  int rc = pthread_setaffinity_np(thr, sizeof(cpu_set_t), &cpuset);
  if (rc) {
    fprintf(stderr, "pthread_setaffinity_np rc %d:%s\n", rc, strerror(errno));
    return false;
  }

  if (-1 == cpu_req) {
    cpu_run = cpu_req;
    return true;
  }

  rc = pthread_getaffinity_np(thr, sizeof(cpu_set_t), &cpuset);
  if (rc) {
    fprintf(stderr, "pthread_getaffinity_np rc %d:%s\n", rc, strerror(errno));
    return false;
  }

  cpu_run = sched_getcpu();
  while ((cpu_run != cpu_req) && chk_cpu_lim) {
    usleep(1);
    cpu_run = sched_getcpu();
    chk_cpu_lim--;
  }

  if (cpu_run != cpu_req) {
    fprintf(stderr, "Unable to schedule thread on cpu %d\n", cpu_req);
    return false;
  }

  return true;
}

RIOMemOpsUMD::RIOMemOpsUMD(const int mport_id, const int chan) : RIOMemOpsMport(mport_id)
{
  m_errno = 0;

  m_run_cpu = 0;
  m_ticks_total = 0;
  m_thruput = false;

  m_fifo_proc_alive       = false;
  m_fifo_proc_must_die    = 0;
  m_fifo_proc_thr_started = false;

  m_dma_fifo_callback     = NULL;
  m_dma_fifo_callback_arg = NULL;

  m_bufc = 0;
  m_sts_entries = 0;

  sem_init(&m_fifo_proc_started, 0, 0);

  memset(&m_fifo_thr, 0, sizeof(m_fifo_thr));
  memset(&m_stats, 0, sizeof(m_stats));

  m_lock = ChannelLock::TakeLock("DMA", mport_id, chan); // Will throw on error

  m_dch = new DMAChannel(mport_id, chan, m_mp_h); // Will throw on error
  m_chan = chan;
}

RIOMemOpsUMD::~RIOMemOpsUMD()
{
  if (m_fifo_proc_thr_started) {
    m_fifo_proc_must_die = 1;

    usleep(500 * 1000); // let detached threads quit

    pthread_join(m_fifo_thr, NULL);
  }

  delete m_dch;
  // Note: m_mp_h destroyed by ~RIOMemOpsMport
  delete m_lock; 
}

void* RIOMemOpsUMD_fifo_proc_thr(void* arg)
{
  if (arg == NULL) throw std::runtime_error("fifo_proc_thr: Invalid argument!");

  RIOMemOpsUMD* mops = static_cast<RIOMemOpsUMD*>(arg);
  assert(mops);

  mops->run_fifo_thr();

  return NULL;
}

void RIOMemOpsUMD::run_fifo_thr()
{
  DMAChannel::WorkItem_t wi[m_sts_entries*8]; memset(wi, 0, sizeof(wi));

  if (m_isolcpu != -1) {
    migrate_thread_to_cpu(m_isolcpu, m_run_cpu, m_fifo_thr);

    if (m_isolcpu != m_run_cpu) {
      static char tmp[129] = {0};
      snprintf(tmp, 128, "Requested CPU %d does not match migrated cpu %d, bailing out!\n", m_isolcpu, m_run_cpu);
      throw std::runtime_error(tmp);
    }
  }

  m_fifo_proc_alive = 1;
  sem_post(&m_fifo_proc_started);

  while (!m_fifo_proc_must_die) {
    // This is a hook to do stuff in isolcpu thread
    if (m_dma_fifo_callback != NULL)
      m_dma_fifo_callback(m_dma_fifo_callback_arg);

    const int cnt = m_dch->scanFIFO(wi, m_sts_entries*8);
    if (!cnt) {
      if (m_thruput) {
        struct timespec tv = { 0, 1 };
        nanosleep(&tv, NULL);
      }
      continue;
    }

    for (int i = 0; i < cnt; i++) {
      DMAChannel::WorkItem_t& item = wi[i];

      switch (item.opt.dtype) {
        case DTYPE1:
        case DTYPE2:
          if (item.ts_end > item.ts_start) {
            m_ticks_total += (item.ts_end - item.ts_start);
          }
          break;
        case DTYPE3:
          break;
        default:
          fprintf(stderr, "UNKNOWN BD %d bd_wp=%u\n", item.opt.dtype, item.bd_wp);
          break;
      }

      wi[i].valid = 0xdeadabba;
    } // END for WorkItem_t vector
  } // END while

  m_fifo_proc_alive = 0;
}

bool RIOMemOpsUMD::_start_fifo_thr()
{
  int rc = pthread_create(&m_fifo_thr, NULL, RIOMemOpsUMD_fifo_proc_thr, (void*)this);
  if (rc) {
    fprintf(stderr, "Could not create fifo_proc_thr thread: %s\n", strerror(errno));
    return false;
  }
  sem_wait(&m_fifo_proc_started);

  if (!m_fifo_proc_alive) {
    fprintf(stderr, "fifo_proc_thr is dead");
    return false;
  }

  return (m_fifo_proc_thr_started = true);
}

bool RIOMemOpsUMD::setup_channel(const int buf, const int sts)
{
  m_bufc = buf;
  m_sts_entries = sts;

  if (! m_dch->alloc_dmatxdesc(m_bufc)) {
    fprintf(stderr, "alloc_dmatxdesc failed: bufs %d", m_bufc);
    goto error;
  }
  if (! m_dch->alloc_dmacompldesc(m_sts_entries)) {
    fprintf(stderr, "alloc_dmacompldesc failed: entries %d", m_sts_entries);
    goto error;
  }
  m_dch->resetHw();
  if (! m_dch->checkPortOK()) {
    fprintf(stderr, "Port %d is not OK!!! Exiting...", m_chan);
    goto error;
  }
  return true;

error:
  return false;
}

bool RIOMemOpsUMD::nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  if (dmaopt.mem.type != DMAMEM) 
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Unsupported memory type!");

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC || dmaopt.sync == RIO_DIRECTIO_TRANSFER_SYNC)
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Operation not supported! Use FAF.");

  m_errno = 0;
  dmaopt.ticket = 0;

  if (m_dch->queueFull()) { m_errno = ENOSPC; return false; }

  // printf("UMDD %s: destid=%u handle=0x%lx rio_addr=0x%lx+0x%x bcount=%d op=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, wr_mode, sync);

  DMAChannel::DmaOptions_t opt; memset(&opt, 0, sizeof(opt));
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
    if (! m_dch->queueDmaOpT1((int)DMAChannelSHM::convert_riomp_dma_directio(dmaopt.wr_mode),
                              opt, dmamem, dma_abort_reason, &m_stats)) {
      if (q_was_full) { m_errno = ENOSPC; return false; }

      switch (dma_abort_reason) {
        case 5:  m_errno = ETIMEDOUT; /*S-RIO response timeout*/; break;
        case 6:  m_errno = EIO;       /*S-RIO I/O ERROR response*/; break;
        case 7:  m_errno = ENXIO;     /*S-RIO implementation specific error*/; break;
        default: m_errno = EHWPOISON; /*PCIe error*/; break;
      }

      return false;
    }
  } else {
    if (! m_dch->queueDmaOpT2((int)DMAChannelSHM::convert_riomp_dma_directio(dmaopt.wr_mode),
                              opt, (uint8_t *)dmaopt.mem.win_handle + dmaopt.mem.offset, dmaopt.bcount, dma_abort_reason, &m_stats)) {
      if (q_was_full) { m_errno = ENOSPC; return false; }
      m_errno = EINVAL; return false;
    }
  }

  return true;
}

bool RIOMemOpsUMD::nread_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC || dmaopt.sync == RIO_DIRECTIO_TRANSFER_SYNC)
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Operation not supported! Use FAF.");

  if (dmaopt.mem.type != DMAMEM) 
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Unsupported memory type!");

  m_errno = 0;
  dmaopt.ticket = 0;

#ifdef MEMOPS_DEBUG
  printf("UMDD %s: destid=%u handle=0x%lx  rio_addr=0x%lx+0x%x\n bcount=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, sync);
#endif

  if (m_dch->queueFull()) { m_errno = ENOSPC; return false; }

  DMAChannel::DmaOptions_t opt;
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

  if (! m_dch->queueDmaOpT1(NREAD, opt, dmamem, dma_abort_reason, &m_stats)) {
    if (q_was_full) { m_errno = ENOSPC; return false; }

    switch (dma_abort_reason) {
      case 5:  m_errno = ETIMEDOUT; /*S-RIO response timeout*/; break;
      case 6:  m_errno = EIO;       /*S-RIO I/O ERROR response*/; break;
      case 7:  m_errno = ENXIO;     /*S-RIO implementation specific error*/; break;
      default: m_errno = EHWPOISON; /*PCIe error*/; break;
    }

    return false;
  }

  return true;
}

const char* RIOMemOpsUMD::abortReasonToStr(int abort_reason)
{
  return strerror(m_errno);
}

int RIOMemOpsUMD::getAbortReason() { return m_errno; }
