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

#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <map>
#include <vector>
#include <stdexcept>
#include <string>
#include <sstream>
#include <algorithm> // std::sort

#include "IDT_Tsi721.h"

#include "mport.h"
#include "dmadesc.h"
#include "rdtsc.h"
#include "debug.h"

/* DMA Status FIFO */
#define DMA_STATUS_FIFO_LENGTH (4096)
#define DMA_RUNPOLL_US 10

#if defined(REGDEBUG)
  #define REGDBG(format, ...) DBG(stdout, format, __VA_ARGS__)
#else
  #define REGDBG(format, ...) 
#endif

void hexdump4byte(const char* msg, uint8_t* d, int len);

class DMAChannel {
public:
  static const int DMA_BUFF_DESCR_SIZE = 32;

  typedef struct {
      uint8_t dtype;
      uint8_t rtype:4;
      uint16_t destid;
      bool iof;
      bool crf;
      bool tt_16b; // set to 1 if destid is 16 bytes
      uint8_t prio:2;
      uint32_t bcount; ///< size of transfer in bytes
      struct raddr_s {
        uint8_t  msb2;
        uint64_t lsb64;
      } raddr;
      uint64_t win_handle; ///< populated when queueing for TX
      uint32_t bd_wp; ///< Soft WP at the moment of enqueuing this
      uint32_t bd_idx; ///< index into buffer ring of buffer used to handle this op
  } DmaOptions_t;

  typedef struct {
    DmaOptions_t       opt;
    RioMport::DmaMem_t mem;
    // add actions here
    uint8_t  t2_rddata[16]; // DTYPE2 NREAD incoming data
    uint32_t t2_rddata_len;
  } WorkItem_t;

  typedef struct {
    uint64_t win_handle;
    uint32_t fifo_offset;
  } DmaCompl_t;

  DMAChannel(const uint32_t mportid, const uint32_t chan);
  DMAChannel(const uint32_t mportid, const uint32_t chan, riomp_mport_t mp_h);
  ~DMAChannel();

  void resetHw();
  void setInitState();
  void setInbound();
  bool dmaIsRunning();
  uint32_t clearIntBits();

  uint32_t getReadCount();
  uint32_t getWriteCount();

  uint32_t getDestId() { return m_mport->rd32(TSI721_IB_DEVID); }

  int getSoftReadCount(bool locked = false);

  bool queueEmptyHw();
  bool queueFullHw(); ///< Handle with care
  bool queueFull();

  uint32_t getFIFOReadCount();
  uint32_t getFIFOWriteCount();

  bool checkPortOK();
  bool checkPortError();
  void checkPortInOutError(bool& inp_err, bool& outp_err);

  bool dmaCheckAbort(uint32_t& abort_reason);
  static const char* abortReasonToStr(const uint32_t abort_reason);

  bool alloc_dmatxdesc(const uint32_t bd_num);
  void free_dmatxdesc();
  bool alloc_dmacompldesc(const uint32_t bd_num);
  void free_dmacompldesc();

  bool alloc_dmamem(const uint32_t size, RioMport::DmaMem_t& mem);
  bool free_dmamem(RioMport::DmaMem_t& mem);

  bool check_ibwin_reg() { return m_mport->check_ibwin_reg(); }

  inline bool queueDmaOpT1(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem)
  {
    opt.dtype = DTYPE1;
    return queueDmaOpT12(rtype, opt, mem);
  }
  inline bool queueDmaOpT2(int rtype, DmaOptions_t& opt, uint8_t* data, const int data_len)
  {
    if(rtype != NREAD && (data == NULL || data_len < 1 || data_len > 16)) return false;
  
    RioMport::DmaMem_t lmem; memset(&lmem, 0, sizeof(lmem));
  
    lmem.win_ptr  = data;
    lmem.win_size = data_len;

    opt.dtype = DTYPE2;
    return queueDmaOpT12(rtype, opt, lmem);
  }

  void cleanup();
  void init_splock();

  int scanFIFO(std::vector<WorkItem_t>& completed_work);

  volatile uint64_t   m_fifo_scan_cnt;

private:
  pthread_spinlock_t  m_hw_splock; ///< Serialize access to DMA chan registers
  pthread_spinlock_t  m_pending_work_splock; ///< Serialize access to DMA pending queue object
  RioMport*           m_mport;
  uint32_t            m_chan;
  uint32_t            m_bd_num;
  uint32_t            m_sts_size;
  RioMport::DmaMem_t  m_dmadesc;
  RioMport::DmaMem_t  m_dmacompl;
  volatile uint32_t   m_dma_wr;      ///< Mirror of Tsi721 write pointer
  uint32_t            m_fifo_rd;
  std::map<uint32_t, bool> m_bl_busy; ///< BD busy list, this [0...(bd_num-1)]
  std::map<uint32_t, uint32_t> m_bl_outstanding; ///< BD map {wp, bd_idx+1}
  pthread_spinlock_t  m_bl_splock; ///< Serialize access to BD list
  uint64_t            m_T3_bd_hw;
  
  std::map<uint64_t, WorkItem_t> m_pending_work;

  bool queueDmaOpT12(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem);

public:
  void setWriteCount(uint32_t cnt);

public: // test-public
  // NOTE: These functions can be inlined only if they live in a
  //       header file
  #define wr32dmachan(o, d) _wr32dmachan((o), #o, (d), #d)
  inline void _wr32dmachan(uint32_t offset, const char* offset_str,
                          uint32_t data, const char* data_str)
  {
    REGDBG("\n\tW chan=%d offset %s (0x%x) :=  %s (0x%x)\n",
        m_chan, offset_str, offset, data_str, data);
    pthread_spin_lock(&m_hw_splock);
    m_mport->__wr32dma(m_chan, offset, data);
    pthread_spin_unlock(&m_hw_splock);
  }

  #define rd32dmachan(o) _rd32dmachan((o), #o)
  inline uint32_t _rd32dmachan(uint32_t offset, const char* offset_str)
  {
    pthread_spin_lock(&m_hw_splock);
    uint32_t ret = m_mport->__rd32dma(m_chan, offset);
    pthread_spin_unlock(&m_hw_splock);
    REGDBG("\n\tR chan=%d offset %s (0x%x) => 0x%x\n", m_chan, offset_str, offset, ret);
    return ret;
  }

  inline void wr32dmachan_nolock(uint32_t offset, uint32_t data)
  {
    m_mport->__wr32dma(m_chan, offset, data);
  }
  inline uint32_t rd32dmachan_nolock(uint32_t offset)
  {
    return m_mport->__rd32dma(m_chan, offset);
  }
};

