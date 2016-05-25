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

#ifndef __MEMOPS_UMDD_H__
#define __MEMOPS_UMDD_H__

#include <errno.h>

#include "memops.h"
#include "memops_mport.h"

#include "dmachanshm.h"

/** \brief UMDd/SHM plugin for RIOMemOpsMport */
class RIOMemOpsUMDd : public RIOMemOpsMport {
public:
  RIOMemOpsUMDd(const int mport, const int chan);
  virtual ~RIOMemOpsUMDd();
  
  virtual bool queueFull() { return DMAChannelSHM_queueFull(m_dch); }

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking, milisec*/);

  virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
    throw std::runtime_error("RIOMemOpsUMDd::alloc_umem: Unsupported memory type!");
  }

  virtual int getAbortReason();
  virtual const char* abortReasonToStr(const int dma_abort_reason);

private:
  bool poll_ticket(DMAChannelSHM::DmaOptions_t& opt, int timeout /*0=blocking, milisec*/);

private:
  void*         m_dch;
  struct seq_ts m_stats;

  volatile uint32_t                               m_cookie_cutter;
  std::map<uint64_t, DMAChannelSHM::DmaOptions_t> m_asyncm;
  pthread_mutex_t                                 m_asyncm_mutex;
};

#endif //__MEMOPS_UMDD_H__
