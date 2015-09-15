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
#include "dmachan.h"

#pragma GCC diagnostic ignored "-fpermissive"

/* DMA Status FIFO */
#define DMA_STATUS_FIFO_LENGTH (4096)

static const uint8_t PATTERN[] = { 0xa1, 0xa2, 0xa3, 0xa4, 0xa4, 0xa6, 0xaf, 0xa8 };
#define PATTERN_SZ  sizeof(PATTERN)

void hexdump4byte(const char* msg, uint8_t* d, int len);

/*
class {
*/

void DMAChannel::init_splock()
{
  pthread_spin_init(&m_hw_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_pending_work_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_bl_splock, PTHREAD_PROCESS_PRIVATE);
}

DMAChannel::DMAChannel(const uint32_t mportid, const uint32_t chan):
     m_bd_num(0),
     m_sts_size(0),
     m_dma_wr(0),
     m_fifo_rd(0),
     m_T3_bd_hw(0)
{
  if(chan >= RioMport::DMA_CHAN_COUNT)
    throw std::runtime_error("DMAChannel: Invalid channel!");

  m_chan  = chan;

  m_mport = new RioMport(mportid);

  init_splock();
  m_fifo_scan_cnt = 0;
}

DMAChannel::DMAChannel(const uint32_t mportid,
                       const uint32_t chan,
                       riomp_mport_t mp_hd) :
     m_bd_num(0),
     m_sts_size(0),
     m_dma_wr(0),
     m_fifo_rd(0),
     m_T3_bd_hw(0)
{
  if(chan >= RioMport::DMA_CHAN_COUNT)
    throw std::runtime_error("DMAChannel: Invalid channel!");

  m_chan  = chan;

  m_mport = new RioMport(mportid, mp_hd);

  init_splock();
  m_fifo_scan_cnt = 0;
}

DMAChannel::~DMAChannel()
{
  shutdown();
  cleanup();
  delete m_mport;
};

char* dma_rtype_str[] = {
  "NREAD",
  "LAST_NWRITE_R",
  "ALL_NWRITE",
  "ALL_NWRITE_R",
  "MAINT_RD",
  "MAINT_WR",
  NULL
};

void DMAChannel::resetHw()
{
  wr32dmachan(TSI721_DMAC_INT,TSI721_DMAC_INT_ALL);
  wr32dmachan(TSI721_DMAC_CTL,TSI721_DMAC_CTL_INIT);
  usleep(10);
  wr32dmachan(TSI721_DMAC_DWRCNT, m_dma_wr = 0);
}

bool DMAChannel::dmaIsRunning()
{
  uint32_t channel_status = rd32dmachan(TSI721_DMAC_STS);
  if (channel_status & TSI721_DMAC_STS_RUN) return true;
  return false;
}

void DMAChannel::setInitState()
{
  wr32dmachan(TSI721_DMAC_CTL, TSI721_DMAC_CTL_INIT);
  wr32dmachan(TSI721_DMAC_DWRCNT, m_dma_wr = 0);
}

void DMAChannel::setInbound()
{
  // Enable inbound window and disable PHY error checking
  uint32_t reg = m_mport->rd32(TSI721_RIO_SP_CTL);
  m_mport->wr32(TSI721_RIO_SP_CTL, reg | TSI721_RIO_SP_CTL_INP_EN | TSI721_RIO_SP_CTL_OTP_EN);
}

bool DMAChannel::dmaCheckAbort(uint32_t& abort_reason)
{
  uint32_t channel_status = rd32dmachan(TSI721_DMAC_STS);

  if(channel_status & TSI721_DMAC_STS_RUN) return false;
  if((channel_status & TSI721_DMAC_STS_ABORT) != TSI721_DMAC_STS_ABORT) return false;

  abort_reason = (channel_status >> 16) & 0x1F;

  return true;
}

bool DMAChannel::checkPortOK()
{
  uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
  return status & TSI721_RIO_SP_ERR_STAT_PORT_OK;
}

bool DMAChannel::checkPortError()
{
  uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
  return status & TSI721_RIO_SP_ERR_STAT_PORT_ERR;
}

void DMAChannel::checkPortInOutError(bool& inp_err, bool& outp_err)
{
  uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);

  if(status & TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP) inp_err = true;
  else inp_err = false;

  if(status & TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP) outp_err = true;
  else outp_err = false;
}

/** \brief Decode to ASCII a DMA engine error
 * \note Do not call this if no error occured -- use \ref dmaCheckAbort to check first
 */
const char* DMAChannel::abortReasonToStr(const uint32_t abort_reason)
{
  switch (abort_reason) {
    case 5:  return "S-RIO response timeout"; break;
    case 6:  return "S-RIO I/O ERROR response"; break;
    case 7:  return "S-RIO implementation specific error"; break;
    default: return "PCIe error"; break;
  }

  /*NOTREACHED*/
  return "n/a";
}

/**
 * \return Contents of TSI721_DMAC_INT
 */
uint32_t DMAChannel::clearIntBits()
{
   wr32dmachan(TSI721_DMAC_INT, TSI721_DMAC_INT_ALL);
   uint32_t reg = rd32dmachan(TSI721_DMAC_INT);
   return reg;
}
uint32_t DMAChannel::getReadCount()
{
   uint32_t reg = rd32dmachan(TSI721_DMAC_DRDCNT); // XXX who is peer_src?
   return reg;
}
uint32_t DMAChannel::getWriteCount()
{
   uint32_t reg = rd32dmachan(TSI721_DMAC_DWRCNT);
   return reg;
}
uint32_t DMAChannel::getFIFOReadCount()
{
   uint32_t reg = rd32dmachan(TSI721_DMAC_DSRP);
   return reg;
}
uint32_t DMAChannel::getFIFOWriteCount()
{
   uint32_t reg = rd32dmachan(TSI721_DMAC_DSWP);
   return reg;
}

/** \brief Checks whether HW bd ring is empty
 * \note This reads 2 PCIe register so it is slow
 */
bool DMAChannel::queueEmptyHw()
{
   return getWriteCount() == getReadCount();
}

/** \brief Checks whether there's more space in HW bd ring
 * \note This reads 2 PCIe register so it is slow
 */
bool DMAChannel::queueFullHw()
{
  uint32_t wrc = getWriteCount();
  uint32_t rdc = getReadCount();
  if(wrc == rdc) return false; // empty

  // XXX unit-test logic
  if(rdc > 0  && wrc == (rdc-1))      return true;
  if(rdc == 0 && wrc == (m_bd_num-1)) return true;
  return false;
}
   
bool DMAChannel::queueFull()
{
  // XXX we have control over the soft m_dma_wr but how to divine the read pointer?
  //     that should come from the completion FIFO but for now we brute-force it!
  //     BEW FIXME: Should the first "unlock" be a lock???

  pthread_spin_unlock(&m_bl_splock);
  const int SZ = m_bl_busy.size();
  pthread_spin_unlock(&m_bl_splock);

  return SZ == (m_bd_num+1); // account for T3 BD as well
}

void DMAChannel::setWriteCount(uint32_t cnt)
{
//  if(cnt > (m_bd_num+1))
//    throw std::runtime_error("setWriteCount: Counter overflow!");

  wr32dmachan(TSI721_DMAC_DWRCNT, cnt);
}


/** \brief Queue DMA operation of DTYPE1 or DTYPE2
 * \param rtype transfer type
 * \param[in,out] opt transfer options
 * \param[in] mem a ref to a RioMport::DmaMem_t, for DTYPE2 this is NOT allocated by class \ref RioMport
 * \param[out] abort_reason HW reason for DMA abort if function returned false
 * \return true if buffer enqueued, false if queue full or HW error -- check abort_reason
 */
bool DMAChannel::queueDmaOpT12(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem, uint32_t& abort_reason)
{
  if(opt.dtype != DTYPE1 && opt.dtype != DTYPE2) return false;

  if(opt.dtype == DTYPE1 && ! m_mport->check_dma_buf(mem)) return false;

  abort_reason = 0;

  struct dmadesc desc;
  dmadesc_setdtype(desc, opt.dtype);

  if(opt.iof)  dmadesc_setiof(desc, 1);
  if(opt.crf)  dmadesc_setcrf(desc, 1);
  if(opt.prio) dmadesc_setprio(desc, opt.prio);

  opt.rtype = rtype;
  dmadesc_setrtype(desc, rtype); // NWRITE_R, etc

  dmadesc_set_raddr(desc, opt.raddr.msb2, opt.raddr.lsb64);

  if(opt.tt_16b) dmadesc_set_tt(desc, 1);
  dmadesc_setdevid(desc, opt.destid);

  if(opt.dtype == DTYPE1) {
    dmadesc_setT1_bufptr(desc, mem.win_handle);
    dmadesc_setT1_buflen(desc, opt.bcount);
    opt.win_handle = mem.win_handle; // this is good across processes
  } else { // T2
    if(rtype != NREAD) // copy data
      dmadesc_setT2_data(desc, (const uint8_t*)mem.win_ptr, mem.win_size);
    else {
      uint8_t ZERO[16] = {0};
      dmadesc_setT2_data(desc, ZERO, mem.win_size);
    }
  }

  bool queued_T3 = false;
  WorkItem_t wk_end; memset(&wk_end, 0 , sizeof(wk_end));

  // Check if queue full -- as late as possible in view of MT
  if(queueFull()) {
    ERR("FAILED: DMA TX Queue full!\n");
    return false;
  }
  
  struct hw_dma_desc* bd_hw = NULL;
  pthread_spin_lock(&m_pending_work_splock);
  if (umdemo_must_die)
	return false;
  pthread_spin_lock(&m_bl_splock); 
  if (umdemo_must_die)
	return false;
  {{
    const int bd_idx = m_dma_wr % (m_bd_num+1);

    DBG("\n\tDMA hw RP = %d | soft RP = %d | soft WP = %d | bd_idx = %d\n",
         getReadCount(), getSoftReadCount(true), m_dma_wr, bd_idx);

    // check-modulo in m_bl_busy[] if bd_idx is still busy!!
    if(m_bl_busy[bd_idx]) {
      pthread_spin_unlock(&m_bl_splock); 
      pthread_spin_unlock(&m_pending_work_splock);
      INFO("\n\tDMA TX queueDmaOpT?: BD %d still busy!\n", bd_idx);
      return false;
    }

    m_bl_busy[bd_idx] = true;
    m_bl_outstanding[(uint32_t)m_dma_wr] = 1 + bd_idx;

    bd_hw = (struct hw_dma_desc*)(m_dmadesc.win_ptr) + bd_idx;
    desc.pack(bd_hw);

    opt.bd_wp = m_dma_wr; opt.bd_idx = bd_idx;

    opt.ts_start = rdtsc();
    m_dma_wr++; setWriteCount(m_dma_wr);
    if(m_dma_wr == 0xFFFFFFFE) m_dma_wr = 0;

    if(((m_dma_wr+1) % (m_bd_num+1)) == 0) { // skip T3
      //DBG("Soft WP = %d, time to T3 + rollover\n", m_dma_wr);
      m_bl_outstanding[(uint32_t)m_dma_wr] = m_bd_num;

      wk_end.opt.bd_wp = m_dma_wr;

      wk_end.opt.ts_start = rdtsc();
      m_dma_wr++; setWriteCount(m_dma_wr);
      if(m_dma_wr == 0xFFFFFFFE) m_dma_wr = 0;

      queued_T3 = true;
    }
  }}
  pthread_spin_unlock(&m_bl_splock); 

  if(dmaCheckAbort(abort_reason)) {
    pthread_spin_unlock(&m_pending_work_splock);
    return false; // XXX maybe not, Barry says reading from PCIe is dog-slow
  }

  WorkItem_t wk; memset(&wk, 0, sizeof(wk));
  wk.mem = mem; wk.opt = opt;

  const uint64_t offset = (uint8_t*)bd_hw - (uint8_t*)m_dmadesc.win_ptr;

  m_pending_work[m_dmadesc.win_handle + offset] = wk;

  if(queued_T3) {
    wk_end.opt.dtype = DTYPE3;
    m_pending_work[m_T3_bd_hw] = wk_end;
  }
  pthread_spin_unlock(&m_pending_work_splock);

  DBG("\n\tQueued DTYPE%d op=%s as BD HW @0x%lx bd_wp=%d\n", wk.opt.dtype, dma_rtype_str[rtype] , m_dmadesc.win_handle + offset, wk.opt.bd_wp);
  if(queued_T3)
    DBG("\n\tQueued DTYPE%d as BD HW @0x%lx bd_wp=%d\n", wk_end.opt.dtype, m_T3_bd_hw, wk_end.opt.bd_wp);

  return true;
}

int DMAChannel::getSoftReadCount(bool locked) // call this from locked context -- m_bl_splock
{
  int ret = -1;

  if(! locked) pthread_spin_lock(&m_bl_splock); 
  if (umdemo_must_die)
	return false;
  do {{
    if(m_bl_outstanding.empty()) { ret = m_dma_wr; break; }

    // divine soft RP -- inefficient in this pass
 
    std::vector<uint32_t> v_idx;

    std::map<uint32_t, uint32_t>::iterator it = m_bl_outstanding.begin();
    for(; it != m_bl_outstanding.end(); it++) { if(it->second) v_idx.push_back(it->first); }

    if(v_idx.size() == 0) return m_dma_wr; // queue EMPTIED
    if(v_idx.size() == m_bd_num) { ret = -1; break; } // queue FULL

    if(v_idx.size() > 1) std::sort(v_idx.begin(), v_idx.end());

    const int idx_min = v_idx.front();
    //const int idx_max = v_idx.back();
 
#if 0
    std::stringstream ss;
    ss << "BL[] = { ";
    for(std::vector<uint32_t>::iterator it = v_idx.begin(); it != v_idx.end(); it++) {
      ss << *it << ":" << (m_bl_outstanding[*it] - 1) << " ";
    }
    ss << "}\n";
    DBG("\n\t%s", ss.str().c_str());
#endif

    ret = idx_min-1;
  }} while(0);

  if(! locked) pthread_spin_unlock(&m_bl_splock); 
  return ret;
}

bool DMAChannel::alloc_dmamem(const uint32_t size, RioMport::DmaMem_t& mem)
{
  if(size > SIXTYFOURMEG) return false;

  mem.rio_address = RIO_ANY_ADDR;
  if(! m_mport->map_dma_buf(size, mem))
    throw std::runtime_error("DMAChannel: Cannot alloc HW mem for DMA transfers!");

  assert(mem.win_ptr);

  memset(mem.win_ptr, 0, size);
  
  return true;
}
bool DMAChannel::free_dmamem(RioMport::DmaMem_t& mem)
{
   return m_mport->unmap_dma_buf(mem);
}

bool DMAChannel::alloc_dmatxdesc(const uint32_t bd_cnt)
{
  int size = (bd_cnt+1) * DMA_BUFF_DESCR_SIZE; 

  if(size < 4096)
    size = 4096;
  else 
    size = ((size + 4095) / 4096) * 4096;

  m_dmadesc.rio_address = RIO_ANY_ADDR;
  if(! m_mport->map_dma_buf(size, m_dmadesc)) {
    CRIT("DMAChannel: Cannot alloc DMA TX ring descriptors!");
    return false;
  }

  m_bd_num = bd_cnt;

  memset(m_dmadesc.win_ptr, 0, m_dmadesc.win_size);

  struct hw_dma_desc* end_bd_p = (struct hw_dma_desc*)
    ((uint8_t*)m_dmadesc.win_ptr + (m_bd_num * DMA_BUFF_DESCR_SIZE));

  DBG("\n\tWrap BD DTYPE3 @ HW 0x%lx [idx=%d] points back to HW 0x%lx\n",
      m_dmadesc.win_handle + (m_bd_num *DMA_BUFF_DESCR_SIZE),
      m_bd_num, m_dmadesc.win_handle);

  // Initialize DMA descriptors ring using added link descriptor 
  struct dmadesc end_bd; memset(&end_bd, 0, sizeof(end_bd));
  dmadesc_setdtype(end_bd, DTYPE3);
  dmadesc_setT3_nextptr(end_bd, (uint64_t)m_dmadesc.win_handle);

  end_bd.pack(end_bd_p); // XXX mask off lowest 5 bits

  m_T3_bd_hw = m_dmadesc.win_handle + (m_bd_num * DMA_BUFF_DESCR_SIZE);

  pthread_spin_lock(&m_bl_splock); 
  if (umdemo_must_die)
	return false;
  m_bl_outstanding.clear();
  pthread_spin_unlock(&m_bl_splock); 

  // Setup DMA descriptor pointers
  wr32dmachan(TSI721_DMAC_DPTRH, (uint64_t)m_dmadesc.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DPTRL, 
    (uint64_t)m_dmadesc.win_handle & TSI721_DMAC_DPTRL_MASK); 

  return true;
}

void DMAChannel::free_dmatxdesc()
{
  m_mport->unmap_dma_buf(m_dmadesc);
}

static inline bool is_pow_of_two(const uint32_t n)
{
  return ((n & (n - 1)) == 0) ? 1 : 0;
}

static inline uint32_t pow_of_two(const uint32_t n)
{
  uint32_t mask = 0x80000000;
  uint32_t pow_2 = 31;

  /* Find the highest '1' bit */
  while (((n & mask) == 0) && (mask != 0)) {
    mask >>= 1;
    pow_2--;
  }

  return pow_2;
};

static inline uint32_t roundup_pow_of_two(const uint32_t n)
{
  /* Corner case, n = 0 */
  if (!n)
    return 1;

  if (is_pow_of_two(n))
    return n;
    
  return 1 << (pow_of_two(n) + 1);
}

/**
 * __fls - find last (most-significant) set bit in a long word
 * @param[in] word the word to search
 *
 * Undefined if no set bit exists, so code should check against 0 first.
 */
#define BITS_PER_LONG 32
static inline unsigned long __fls(unsigned long word)
{
  int num = BITS_PER_LONG - 1;

#if BITS_PER_LONG == 64
  if (!(word & (~0ul << 32))) {
    num -= 32;
    word <<= 32;
  }
#endif
  if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
    num -= 16;
    word <<= 16;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
    num -= 8;
    word <<= 8;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
    num -= 4;
    word <<= 4;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
    num -= 2;
    word <<= 2;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-1))))
    num -= 1;
  return num;
}

bool DMAChannel::alloc_dmacompldesc(const uint32_t bd_cnt)
{
  bool rc = false;
  uint64_t sts_byte_cnt;
  uint64_t sts_entry_cnt;
  uint64_t sts_log_two;
  const uint64_t max_entry_cnt = 1 << (TSI721_OBDMACXDSSZ_SIZE + 4);

  // Tsi721 Requires completion queue size is the number of 
  // completion queue entries, LOG 2, minus 4.  
  //
  // The number of completion queue entries must be a power of 2
  // between 32 and 512K.
  //
  // Each completion queue entry is eight 8-byte pointers, total
  // of 64 bytes.

  sts_entry_cnt = bd_cnt;
  if (sts_entry_cnt < TSI721_DMA_MINSTSSZ) {
    DBG("\n\tDMA Completion Count too small: %d", bd_cnt);
    sts_entry_cnt = TSI721_DMA_MINSTSSZ;
  };
  if (sts_entry_cnt > max_entry_cnt) {
    DBG("\n\tDMA Completion Count TOO BIG: %d", bd_cnt);
    sts_entry_cnt = max_entry_cnt;
  };

  m_sts_size = roundup_pow_of_two(sts_entry_cnt);
  sts_log_two = pow_of_two(sts_entry_cnt) - 4;
  sts_byte_cnt = m_sts_size * 64;

  m_dmacompl.rio_address = RIO_ANY_ADDR;
  
  if (!m_mport->map_dma_buf(sts_byte_cnt, m_dmacompl)) {
    ERR("DMAChannel: Cannot alloc HW mem for DMA completion ring!");
    goto exit;
  };

  memset(m_dmacompl.win_ptr, 0, sts_byte_cnt);

  // Setup descriptor status FIFO 
  wr32dmachan(TSI721_DMAC_DSBH,
    (uint64_t)m_dmacompl.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DSBL,
    (uint64_t)m_dmacompl.win_handle & TSI721_DMAC_DSBL_MASK);
  wr32dmachan(TSI721_DMAC_DSSZ, sts_log_two);

  INFO("\n\tDMA compl entries %d bytes=%d @%p HW@0x%llx\n",
       m_sts_size, sts_byte_cnt,
       m_dmacompl.win_ptr, m_dmacompl.win_handle);
  rc = true;
exit:
  return rc;
}

void DMAChannel::free_dmacompldesc()
{
  m_mport->unmap_dma_buf(m_dmacompl);
}

void DMAChannel::shutdown()
{
  umdemo_must_die = 1;

// Unlock twice, once for each thread, to guarantee forward progress...
  pthread_spin_unlock(&m_pending_work_splock);
  pthread_spin_unlock(&m_pending_work_splock);
  pthread_spin_unlock(&m_hw_splock);
  pthread_spin_unlock(&m_hw_splock);
  pthread_spin_unlock(&m_bl_splock);
  pthread_spin_unlock(&m_bl_splock);
};

void DMAChannel::cleanup()
{
  // Reset HW here
  resetHw();
 
  try { // May not have these things allocated at all
    free_dmacompldesc();
    free_dmatxdesc();
  } catch(std::runtime_error e) {}

  // We clean up in case we'll be reusing this object
  memset(&m_dmadesc, 0, sizeof(m_dmadesc));
  memset(&m_dmacompl, 0, sizeof(m_dmacompl));
}

static inline bool hexdump64bit(const void* p, int len)
{
  if(p == NULL || len < 8) return false;

  char tmp[257] = {0};
  std::stringstream ss;

  bool empty = true;
  snprintf(tmp, 256, "Mem @%p size %d as 64-bit words:\n", p, len); ss << tmp;
  uint64_t* q = (uint64_t*)p; len /= 8;
  for(int i=0; i<len; i++) {
    if(! q[i]) continue;
    snprintf(tmp, 256, " 0x%04x/%d(d) = 0x%llx\n", i, i, q[i]); ss << tmp;
    empty = false;
  }
  if(empty) return false;
  DBG("%s", ss.str().c_str());
  return true;
}

void hexdump4byte(const char* msg, uint8_t* d, int len)
{
  if(msg != NULL)
    DBG("%s", msg);
  DBG("Mem @%p size %d:\n", d, len);

  for(int i = 0; i < len; i++) {
    uint32_t tmp;
    tmp = (tmp << 8) + d[i];
    if(((i + 1) % 4) == 0) {
      DBG("%8x\n", tmp);
      tmp = 0;
    }
  }
}

int DMAChannel::scanFIFO(std::vector<WorkItem_t>& completed_work)
{
  std::vector<DmaCompl_t> compl_hwbuf;

  m_fifo_scan_cnt++;

#if 0//def DEBUG
  if(hexdump64bit(m_dmacompl.win_ptr, m_dmacompl.win_size))
    DBG("\n\tFIFO hw RP=%u WP=%u\n", getFIFOReadCount(), getFIFOWriteCount());
#endif

  /* Check and clear descriptor status FIFO entries */
  uint64_t* sts_ptr = (uint64_t*)m_dmacompl.win_ptr;
  int j = m_fifo_rd * 8;
//DBG("\n\tFIFO START line=%d\n", j);
  while (sts_ptr[j] && !umdemo_must_die) {
      //for (int i = 0; i < 8 && sts_ptr[j]; i++, j++) {
      for (int i = j; i < (j+8) && sts_ptr[i]; i++) {
DBG("\n\tFIFO (sts_size=%d) line=%d off=%d 0x%llx\n", m_sts_size, j, i, sts_ptr[i]);
          DmaCompl_t c;
          c.ts_end = rdtsc();
          c.win_handle = sts_ptr[i]; c.fifo_offset = i;
          compl_hwbuf.push_back(c);
          sts_ptr[i] = 0;
      }

      ++m_fifo_rd;
      m_fifo_rd %= m_sts_size;
      j = m_fifo_rd * 8;
  }

  if(compl_hwbuf.empty()) {
    // No hw pointer to advance
    return 0;
  }

  std::vector<DmaCompl_t>::iterator itv = compl_hwbuf.begin();
  for(; itv != compl_hwbuf.end(); itv++) {
    pthread_spin_lock(&m_pending_work_splock);
  if (umdemo_must_die)
	return 0;

    std::map<uint64_t, WorkItem_t>::iterator itm = m_pending_work.find(itv->win_handle);
    if(itm == m_pending_work.end()) {
      pthread_spin_unlock(&m_pending_work_splock);
      ERR("Can't find BD HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          itv->win_handle, itv->fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    WorkItem_t item = itm->second; // XXX with the spinlock in mind we make a copy here
    m_pending_work.erase(itm);

    pthread_spin_unlock(&m_pending_work_splock);

    item.opt.ts_end = itv->ts_end;

    if(item.opt.dtype == DTYPE2 && item.opt.rtype == NREAD) {
      struct hw_dma_desc* bd = (struct hw_dma_desc*)(m_dmadesc.win_ptr) + item.opt.bd_wp;;
      memcpy(item.t2_rddata, bd->data, 16);
      item.t2_rddata_len = le32(bd->bcount & 0xf);
    }
    completed_work.push_back(item);

    // BDs might be completed out of order.
    pthread_spin_lock(&m_bl_splock); 
  if (umdemo_must_die)
	return 0;
    m_bl_busy.erase(item.opt.bd_idx);
    m_bl_outstanding[item.opt.bd_wp] = 0;
    pthread_spin_unlock(&m_bl_splock); 
  }

  // Before advancing FIFO RP I must have a "barrier" so no "older" BDs exist.

  wr32dmachan(TSI721_DMAC_DSRP, m_fifo_rd);
  return compl_hwbuf.size();
}