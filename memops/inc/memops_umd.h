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

#ifndef __MEMOPS_UMD_H__
#define __MEMOPS_UMD_H__

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "libtime_utils.h"

#include "memops.h"
#include "memops_mport.h"

#include "dmachan.h"
#include "chanlock.h"

/** \bried UMD/monolithic plugin for RIOMemOpsMport
 * \note This class will spawn a thread to service the channel FIFO
 */
class RIOMemOpsUMD : public RIOMemOpsMport {
public:
  RIOMemOpsUMD(const int mport, const int chan);
  virtual ~RIOMemOpsUMD();
  
  virtual bool queueFull() { return m_dch->queueFull(); }

  virtual bool canRestart() { return true; }
  virtual bool restartChannel() { m_dch->softRestart(); return true; }

  /** \brief Check whether channel has aborted
   * \return true on abort, false otherwise
   */
  virtual bool checkAbort() {
    uint32_t abort_reason = 0;
    if (! m_dch->dmaCheckAbort(abort_reason)) return false;
    abortReasonToErrno(abort_reason);
    return true;
  }

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking, milisec*/);

  virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
    throw std::runtime_error("RIOMemOpsUMD::alloc_umem: Unsupported memory type!");
  }

  virtual int getAbortReason();
  virtual const char* abortReasonToStr(const int dma_abort_reason);

  void set_fifo_thr_callback(void (*cbk)(void*), void* cbk_arg = NULL) {
    m_dma_fifo_callback     = cbk;
    m_dma_fifo_callback_arg = cbk_arg;
  }

  bool setup_channel(const int buf, const int sts);

  // isolcpu = -1 means any CPU for FIFO thread
  // Note: Call set_fifo_thr_callback first!
  // Note: Call setup_channel first!
  bool start_fifo_thr(int isolcpu, bool thruput = false) {
    m_isolcpu = isolcpu;
    m_thruput = thruput;
    return _start_fifo_thr();
  }

private:
  void run_fifo_thr();
  bool _start_fifo_thr();
  friend void* RIOMemOpsUMD_fifo_proc_thr(void* arg); // libpthread helper, cannot be member

  void inline abortReasonToErrno(const uint32_t dma_abort_reason) {
    switch (dma_abort_reason) {
      case 0:  m_errno = 0; break;  /*No error*/
      case 5:  m_errno = ETIMEDOUT; /*S-RIO response timeout*/; break;
      case 6:  m_errno = EIO;       /*S-RIO I/O ERROR response*/; break;
      case 7:  m_errno = ENXIO;     /*S-RIO implementation specific error*/; break;
      default: m_errno = EHWPOISON; /*PCIe error*/; break;
    }
  }

  bool poll_ticket(DMAChannel::DmaOptions_t& opt, int timeout /*0=blocking, milisec*/);

private:
  DMAChannel*   m_dch;
  int           m_chan;
  int           m_bufc;
  int           m_sts_entries;
  uint64_t      m_ticks_total;
  struct seq_ts m_stats;
  int           m_isolcpu;
  int           m_run_cpu;
  bool          m_thruput;

  pthread_t     m_fifo_thr;
  sem_t         m_fifo_proc_started;
  volatile bool m_fifo_proc_alive;
  bool          m_fifo_proc_thr_started;
  volatile int  m_fifo_proc_must_die;

  void          (*m_dma_fifo_callback)(void*);
  void*         m_dma_fifo_callback_arg;

  LockChannel*  m_lock; ///< On-disk channel lock

  volatile uint32_t                            m_cookie_cutter;
  std::map<uint64_t, DMAChannel::DmaOptions_t> m_asyncm;
  pthread_mutex_t                              m_asyncm_mutex;
};

#endif //__MEMOPS_UMD_H__
