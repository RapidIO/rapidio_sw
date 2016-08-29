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

#ifndef __RDMAOPSMPORT_H__
#define __RDMAOPSMPORT_H__

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

//#include "libcli.h"
#include "liblog.h"

#include "libtime_utils.h"
#include "worker.h"

#include "dmachan.h"
#include "rdmaops.h"

class RdmaOpsMport : public RdmaOpsIntf {
public:
  static const int QUEUE_FULL_DELAY_MS = 50; // microseconds

private:
  int             m_errno;
  bool            m_check_reg; ///< Means faf or async
  volatile bool   m_q_full;
  struct timespec m_q_full_ts;
  riomp_mport_t   m_mp_h;
  bool            m_mp_h_mine;
  RioMport*       m_mport;

  inline void flagQFull()
  {
    clock_gettime(CLOCK_MONOTONIC, &m_q_full_ts);
    m_q_full = true;
  }

public:
  RdmaOpsMport()
  {
    m_errno = 0;
    m_check_reg = false;
    m_mp_h_mine = false;
    m_mport = NULL;
    m_q_full = false;
    memset(&m_q_full_ts, 0, sizeof(m_q_full_ts));
  }
  virtual ~RdmaOpsMport() { if (m_mp_h_mine) riomp_mgmt_mport_destroy_handle(&m_mp_h); delete m_mport; }

  virtual bool canRestart() { return false; }
  virtual void setCheckHwReg(bool sw) { m_check_reg = sw; }

  /** \brief Report kernel queue fullness but only if it happend less than \ref QUEUE_FULL_DELAY_MS miliseconds ago
   */
  virtual bool queueFull()
  {
    if (!m_q_full) return false;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    struct timespec elapsed = time_difference(m_q_full_ts, now);
    const uint64_t dTnsec = elapsed.tv_nsec + (elapsed.tv_sec * 1000000000);
    
    if (dTnsec > (QUEUE_FULL_DELAY_MS * 1000)) return false;

    return true;
  }

  // T2 Ops
  virtual bool nread_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out)
  {
    m_errno = 0;
    m_q_full = false;
    int dma_rc = riomp_dma_read(m_mp_h, destid, rio_addr,
                                data_out, size,
                                RIO_DIRECTIO_TRANSFER_SYNC);
    if (dma_rc == 0) return true;
    if (dma_rc == -EBUSY) flagQFull();
    DBG("\n\t%s riomp_dma_read => %d (%s)\n", __func__, dma_rc, strerror(-dma_rc));
    m_errno = -dma_rc;
    return false;
  }
  virtual bool nwrite_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, const uint8_t* data)
  {
    m_errno = 0;
    m_q_full = false;
    int dma_rc = riomp_dma_write(m_mp_h, destid, rio_addr,
                                 (void*)data, size,
                                 RIO_DIRECTIO_TYPE_NWRITE_R, RIO_DIRECTIO_TRANSFER_SYNC);
    if (dma_rc == 0) return true;
    if (dma_rc == -EBUSY) flagQFull();
    DBG("\n\t%s riomp_dma_write => %d (%s)\n", __func__, dma_rc, strerror(-dma_rc));
    m_errno = -dma_rc;
    return false;
  }

  // T1 Ops
  virtual bool nwrite_mem(DMAChannel::DmaOptions_t& dmaopt, RioMport::DmaMem_t& dmamem)
  {
    enum riomp_dma_directio_transfer_sync rd_sync = m_check_reg?
       RIO_DIRECTIO_TRANSFER_SYNC:
       RIO_DIRECTIO_TRANSFER_FAF;

    m_errno = 0;
    m_q_full = false;
    int dma_rc = riomp_dma_write_d(m_mp_h, dmaopt.destid, dmaopt.raddr.lsb64,
                                   dmamem.win_handle, 0 /*offset*/,
                                   dmaopt.bcount,
                                   RIO_DIRECTIO_TYPE_NWRITE_R, rd_sync);
    if (dma_rc == 0) return true;
    if (dma_rc == -EBUSY) flagQFull();
    DBG("\n\t%s riomp_dma_write_d => %d (%s)\n", __func__, dma_rc, strerror(-dma_rc));
    m_errno = -dma_rc;
    return false;
  }

  virtual int getAbortReason() { return m_errno; }

  virtual const char* abortReasonToStr(const int dma_abort_reason)
  {
    return strerror(dma_abort_reason);
  }

  virtual uint16_t getDestId()
  {
    struct riomp_mgmt_mport_properties qresp;
    memset(&qresp, 0, sizeof(qresp));
    const int rc = riomp_mgmt_query(m_mp_h, &qresp);
    if (rc) return 0xFFFF;
    return qresp.hdid;
  }

// This is implementation-specific, not in base class (interface)
  inline void setup_chan2(struct worker* info) { m_mp_h = info->mp_h; m_mp_h_mine = false; }
  inline bool setup_chanN(struct worker* info, int chan, RioMport::DmaMem_t* dmamem)
  {
    assert(info);
    if (dmamem == NULL) return false;

    int rc = riomp_mgmt_mport_create_handle(info->mp_num, 0, &m_mp_h);
    if (rc) return false;
    m_mp_h_mine = true;

    m_mport = new RioMport(info->mp_num, m_mp_h);

    const int size = BD_PAYLOAD_SIZE(info);
    for (int i = 0; i < info->umd_tx_buf_cnt; i++) {
      RioMport::DmaMem_t& mem = dmamem[i];
      mem.rio_address = RIO_ANY_ADDR;
      if(! m_mport->map_dma_buf(size, mem))
        throw std::runtime_error("DMAChannel: Cannot alloc HW mem for DMA transfers!");

      assert(mem.win_ptr);
      memset(mem.win_ptr, 0, size);
    }

    return true;
  }

  inline bool free_dmamem(RioMport::DmaMem_t& mem)
  {
    assert(m_mport);
    return m_mport->unmap_dma_buf(mem);
  }
};

#endif // __RDMAOPSMPORT_H__
