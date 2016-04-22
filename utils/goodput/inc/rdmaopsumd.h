/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
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

#ifndef __RDMAOPSUMD_H__
#define __RDMAOPSUMD_H__

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "libcli.h"
#include "liblog.h"

#include "libtime_utils.h"
#include "worker.h"

#include "dmachan.h"
#include "rdmaops.h"


class RdmaOpsUMD : public RdmaOpsIntf {
public:
  static const int DMA_CHAN2_BUFC = 0x20;
  static const int DMA_CHAN2_STS  = 0x20;

private:
  DMAChannel*   m_dmac;
  uint32_t      m_dma_abort_reason;
  int           m_mp_num;     ///< mport_cdev port ID
  riomp_mport_t m_mp_h; 

public:
  struct seq_ts m_meas_ts;

public:
  RdmaOpsUMD() { m_dmac = NULL; m_dma_abort_reason = 0; memset(&m_meas_ts, 0, sizeof(m_meas_ts)); }

  virtual ~RdmaOpsUMD() { delete m_dmac; }

  virtual bool canRestart() { return true; }

  virtual void setCheckHwReg(bool sw) { assert(m_dmac); m_dmac->setCheckHwReg(sw); }

  virtual bool queueFull() { assert(m_dmac); return m_dmac->queueFull(); }

  // T2 Ops
  virtual bool nread_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out);
  virtual bool nwrite_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, const uint8_t* data);

  // T1 Ops
  virtual bool nwrite_mem(DMAChannel::DmaOptions_t& dmaopt, RioMport::DmaMem_t& dmamem);

  virtual int getAbortReason() { return m_dma_abort_reason; }

  virtual const char* abortReasonToStr(const int dma_abort_reason)
  {
    return DMAChannel::abortReasonToStr(dma_abort_reason);
  }

  virtual uint16_t getDestId() { return m_dmac->getDestId(); }

// This is implementation-specific, not in base class (interface)

  inline void setMportInfo(int mportid, riomp_mport_t mp_h) { m_mp_num = mportid; m_mp_h = mp_h; }

  DMAChannel* setup_chan2(struct worker* info);
  DMAChannel* setup_chanN(struct worker* info, int chan, RioMport::DmaMem_t* dmamem);

  inline DMAChannel* getChannel() { return m_dmac; }
};

#endif // __RDMAOPSUMD_H__
