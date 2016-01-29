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

#include <stdexcept>
#include <string>
#include <sstream>

#include "IDT_Tsi721.h"

#include "mport.h"
#include "dmadesc.h"
#include "rdtsc.h"
#include "debug.h"
#include "dmachan.h"
#include "time_utils.h"

#pragma GCC diagnostic ignored "-fpermissive"

/* DMA Status FIFO */
#define DMA_STATUS_FIFO_LENGTH (4096)

static const uint8_t PATTERN[] = { 0xa1, 0xa2, 0xa3, 0xa4, 0xa4, 0xa6, 0xaf, 0xa8 };
#define PATTERN_SZ  sizeof(PATTERN)

void hexdump4byte(const char* msg, uint8_t* d, int len);

/*
class {
*/

void DMAChannel::init()
{
  pthread_spin_init(&m_hw_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_pending_work_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_bl_splock, PTHREAD_PROCESS_PRIVATE);

  m_bd_num        = 0;
  m_sts_size      = 0;
  m_dma_wr        = 0;
  m_fifo_rd       = 0;
  m_T3_bd_hw      = 0;
  m_fifo_scan_cnt = 0;
  m_tx_cnt        = 0;
  m_bl_busy       = NULL;
  m_bl_busy_size  = -666;
  m_check_reg     = false;
  m_restart_pending = 0;
  m_sts_log_two   = 0;

  memset(&m_BD0_T3_saved, 0, sizeof(m_BD0_T3_saved));

  m_sim           = false;
  m_sim_dma_rp    = 0;
  m_sim_fifo_wp   = 0;

  m_sim_abort_reason = 0;
  m_sim_err_stat = 0;
}

DMAChannel::DMAChannel(const uint32_t mportid, const uint32_t chan)
{
  if(chan >= RioMport::DMA_CHAN_COUNT)
    throw std::runtime_error("DMAChannel: Invalid channel!");

  m_chan  = chan;

  m_mport = new RioMport(mportid);

  init();
}

DMAChannel::DMAChannel(const uint32_t mportid,
                       const uint32_t chan,
                       riomp_mport_t mp_hd)
{
  if(chan >= RioMport::DMA_CHAN_COUNT)
    throw std::runtime_error("DMAChannel: Invalid channel!");

  m_chan  = chan;

  m_mport = new RioMport(mportid, mp_hd);

  init();
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

bool DMAChannel::dmaIsRunning()
{
  // XXX What about sim mode?
 
  uint32_t channel_status = rd32dmachan(TSI721_DMAC_STS);
  if (channel_status & TSI721_DMAC_STS_RUN) return true;
  return false;
}

void DMAChannel::resetHw()
{
  m_sim_abort_reason = 0;
  m_sim_err_stat     = 0;
  m_sim_fifo_wp      = 0;
  m_sim_dma_rp       = 0;

  m_dma_wr = 1; // BD0 is T3
 
  if (m_sim) return;

  if(dmaIsRunning()) {
    wr32dmachan(TSI721_DMAC_CTL, TSI721_DMAC_CTL_SUSP);

    for(int i = 0; i < 1000; i++) {
      if(! rd32dmachan(TSI721_DMAC_CTL) & TSI721_DMAC_CTL_SUSP) break;
    }
  }

  wr32dmachan(TSI721_DMAC_CTL, TSI721_DMAC_CTL_INIT);
  wr32dmachan(TSI721_DMAC_INT, TSI721_DMAC_INT_ALL);
  usleep(10);
  wr32dmachan(TSI721_DMAC_DWRCNT, m_dma_wr);
}

void DMAChannel::setInbound()
{
  // Enable inbound window and disable PHY error checking
  uint32_t reg = m_mport->rd32(TSI721_RIO_SP_CTL);
  m_mport->wr32(TSI721_RIO_SP_CTL, reg | TSI721_RIO_SP_CTL_INP_EN | TSI721_RIO_SP_CTL_OTP_EN);
}

/** \brief Decode to ASCII a DMA engine error
 * \note Do not call this if no error occured -- use \ref dmaCheckAbort to check first
 */
const char* DMAChannel::abortReasonToStr(const uint32_t abort_reason)
{
  switch (abort_reason) {
    case 0:  return "No abort"; break;
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

/** \brief Queue DMA operation of DTYPE1 or DTYPE2
 * \para[in] rtype transfer type
 * \param[in,out] opt transfer options
 * \param[in] mem a ref to a RioMport::DmaMem_t, for DTYPE2 this is NOT allocated by class \ref RioMport, for DTYPE2 NREAD it is NOT USED
 * \param[out] abort_reason HW reason for DMA abort if function returned false
 * \return true if buffer enqueued, false if queue full or HW error -- check abort_reason
 */
bool DMAChannel::queueDmaOpT12(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem, uint32_t& abort_reason, struct seq_ts *ts_p)
{
  struct dmadesc desc;
  bool queued_T3 = false;
  WorkItem_t wk_end, wk_0;
  WorkItem_t wk;
  struct hw_dma_desc* bd_hw = NULL;


  if ((opt.dtype != DTYPE1) && (opt.dtype != DTYPE2))
    return false;

  if ((opt.dtype == DTYPE1) && !m_mport->check_dma_buf(mem))
    return false;

  abort_reason = 0;

  get_seq_ts_m(ts_p, 1);

  dmadesc_setdtype(desc, opt.dtype);

  if(opt.iof)
    dmadesc_setiof(desc, 1);
  if(opt.crf)
    dmadesc_setcrf(desc, 1);
  if(opt.prio)
    dmadesc_setprio(desc, opt.prio);

  opt.rtype = rtype;
  dmadesc_setrtype(desc, rtype); // NWRITE_R, etc

  dmadesc_set_raddr(desc, opt.raddr.msb2, opt.raddr.lsb64);

  if(opt.tt_16b)
    dmadesc_set_tt(desc, 1);
  dmadesc_setdevid(desc, opt.destid);

  if(opt.dtype == DTYPE1) {
    dmadesc_setT1_bufptr(desc, mem.win_handle);
    dmadesc_setT1_buflen(desc, opt.bcount);
    opt.win_handle = mem.win_handle; // this is good across processes
  } else { // T2
    if(rtype != NREAD)  { // copy data
      dmadesc_setT2_data(desc, (const uint8_t*)mem.win_ptr,
        mem.win_size);
    } else {
      uint8_t ZERO[16] = {0};
      dmadesc_setT2_data(desc, ZERO, mem.win_size);
    }
  }

  memset(&wk_end, 0 , sizeof(wk_end));
  memset(&wk_0, 0 , sizeof(wk_0));

  // Check if queue full -- as late as possible in view of MT
  if(queueFull()) {
    ERR("\n\tFAILED: DMA TX Queue full! chan=%u\n", m_chan);
    return false;
  }
  
  pthread_spin_lock(&m_bl_splock); 
  if (umdemo_must_die)
    return false;

  int bd_idx = m_dma_wr % m_bd_num;
  {{
    // If at end of buffer, account for T3 and
    // wrap around to beginning of buffer.
    if ((bd_idx + 1) == m_bd_num) {
      wk_end.opt.bd_wp = m_dma_wr;
      wk_0.opt.bd_wp   = m_dma_wr+1;

      wk_end.opt.ts_start = wk_0.opt.ts_start = rdtsc();
      // FIXME: Should this really be FF..E, or should it be FF..F ???
      if (m_dma_wr == 0xFFFFFFFE)
        m_dma_wr = 1; // Process BD0 which is a T3
      else
        m_dma_wr += 2; // Process BD0 which is a T3

      memcpy((uint8_t*)m_dmadesc.win_ptr, &m_BD0_T3_saved, DMA_BUFF_DESCR_SIZE); // Reset BD0 as jump+1

      setWriteCount(m_dma_wr);
      queued_T3 = true;
      m_bl_busy_size += 2; // XXX BUG??
      bd_idx = 1; // Skip BD0 which is a T3
    }

    // check-modulo in m_bl_busy[] if bd_idx is still busy!!
    if(m_bl_busy[bd_idx]) {
      pthread_spin_unlock(&m_bl_splock); 
#ifdef DEBUG_BD
      INFO("\n\tDMA TX queueDmaOpT?: BD %d still busy!\n", bd_idx);
#endif
      return false;
    }

    m_bl_busy[bd_idx] = true;
    m_bl_busy_size++;

    bd_hw = (struct hw_dma_desc*)(m_dmadesc.win_ptr) + bd_idx;
    desc.pack(bd_hw);

    opt.bd_wp = m_dma_wr;
    opt.bd_idx = bd_idx;

    opt.ts_start = rdtsc();

    m_dma_wr++;
    setWriteCount(m_dma_wr);

    if(m_dma_wr == 0xFFFFFFFE) m_dma_wr = 1; // Process BD0 which is a T3
  }}
  pthread_spin_unlock(&m_bl_splock); 

  if(m_check_reg && dmaCheckAbort(abort_reason)) {
    return false; // XXX maybe not, Barry says reading from PCIe is dog-slow
  }

  memset(&wk, 0, sizeof(wk));
  wk.mem = mem;
  wk.opt = opt;
  wk.valid = WI_SIG;

  pthread_spin_lock(&m_pending_work_splock);
  m_pending_work[bd_idx] = wk;

  if(queued_T3) {
    wk_end.opt.dtype = DTYPE3;
    wk_end.valid = WI_SIG;
    m_pending_work[m_T3_bd_hw] = wk_end;
    wk_0.opt.dtype = DTYPE3;
    wk_0.valid = WI_SIG;
    m_pending_work[0] = wk_0; // BD0 is T3
  }
  pthread_spin_unlock(&m_pending_work_splock);

  get_seq_ts_m(ts_p, 9);

#ifdef DEBUG_BD
  const uint64_t offset = (uint8_t*)bd_hw - (uint8_t*)m_dmadesc.win_ptr;

  DBG("\n\tQueued DTYPE%d op=%s as BD HW @0x%lx bd_wp=%d\n",
      wk.opt.dtype, dma_rtype_str[rtype] , m_dmadesc.win_handle + offset, wk.opt.bd_wp);

  if(queued_T3)
     DBG("\n\tQueued DTYPE%d as BD HW @0x%lx bd_wp=%d\n", wk_end.opt.dtype, m_dmadesc.win_handle + m_T3_bd_hw, wk_end.opt.bd_wp);
#endif

  return true;
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

  m_bl_busy = (bool*)calloc(m_bd_num, sizeof(bool));
  m_bl_busy_size = 0;

  m_pending_work = (WorkItem_t*)calloc(m_bd_num+1, sizeof(WorkItem_t)); // +1 to have a guard, NOT used

  memset(m_dmadesc.win_ptr, 0, m_dmadesc.win_size);

  struct hw_dma_desc* end_bd_p = (struct hw_dma_desc*)
    ((uint8_t*)m_dmadesc.win_ptr + ((m_bd_num-1) * DMA_BUFF_DESCR_SIZE));

#ifdef DEBUG_BD
  DBG("\n\tWrap BD DTYPE3 @ HW 0x%lx [idx=%d] points back to HW 0x%lx\n",
      m_dmadesc.win_handle + ((m_bd_num-1) *DMA_BUFF_DESCR_SIZE),
      m_bd_num-1, m_dmadesc.win_handle);
#endif

  // Initialize DMA descriptors ring using added link descriptor 
  struct dmadesc T3_bd; memset(&T3_bd, 0, sizeof(T3_bd));
  dmadesc_setdtype(T3_bd, DTYPE3);
  dmadesc_setT3_nextptr(T3_bd, (uint64_t)m_dmadesc.win_handle);

  T3_bd.pack(end_bd_p); // XXX mask off lowest 5 bits

  m_T3_bd_hw = m_bd_num-1;

  // Initialize BD0 DMA descriptor as jump+1
  struct hw_dma_desc* bd0_p = (struct hw_dma_desc*)m_dmadesc.win_ptr; // Set BD0 as T3

#ifdef DEBUG_BD
  DBG("\n\tBD0 DTYPE3 @ HW 0x%lx [idx=0] jump+1 to HW 0x%lx\n",
      m_dmadesc.win_handle,
      m_dmadesc.win_handle + DMA_BUFF_DESCR_SIZE);
#endif

  dmadesc_setT3_nextptr(T3_bd, (uint64_t)m_dmadesc.win_handle + DMA_BUFF_DESCR_SIZE);
  T3_bd.pack(bd0_p); // XXX mask off lowest 5 bits

  memcpy(&m_BD0_T3_saved, (uint8_t*)m_dmadesc.win_ptr, DMA_BUFF_DESCR_SIZE);

  if (m_sim) return true;

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
  uint64_t sts_entry_cnt = 0;
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
  }
  if (sts_entry_cnt > max_entry_cnt) {
    DBG("\n\tDMA Completion Count TOO BIG: %d", bd_cnt);
    sts_entry_cnt = max_entry_cnt;
  }

  m_sts_size = roundup_pow_of_two(sts_entry_cnt);
  m_sts_log_two = pow_of_two(sts_entry_cnt) - 4;

  m_dmacompl.rio_address = RIO_ANY_ADDR;
  if (!m_mport->map_dma_buf(m_sts_size * 64, m_dmacompl)) {
    ERR("DMAChannel: Cannot alloc HW mem for DMA completion ring!");
    goto exit;
  }

  memset(m_dmacompl.win_ptr, 0, m_dmacompl.win_size);

  if (m_sim) { rc = true; goto exit; }

  // Setup descriptor status FIFO 
  wr32dmachan(TSI721_DMAC_DSBH,
    (uint64_t)m_dmacompl.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DSBL,
    (uint64_t)m_dmacompl.win_handle & TSI721_DMAC_DSBL_MASK);
  wr32dmachan(TSI721_DMAC_DSSZ, m_sts_log_two);

#if 0
  INFO("\n\tDMA compl entries %d bytes=%d @%p HW@0x%llx\n",
       m_sts_size, sts_byte_cnt,
       m_dmacompl.win_ptr, m_dmacompl.win_handle);
#endif
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
  
  if(m_pending_work != NULL) {
    assert(m_pending_work[m_bd_num].valid == 0);
    free(m_pending_work); m_pending_work = NULL;
  }

  if(m_bl_busy != NULL) {
    free(m_bl_busy); m_bl_busy = NULL;
    m_bl_busy_size = -42;
  }
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

   uint32_t tmp = 0;
  for(int i = 0; i < len; i++) {
  /* To make descriptor dump match Tsi721 manual,
 * must push byte 3 to most significant position, not least significant
 */
    tmp = tmp + ((uint32_t)(d[i]) << (8 * (i%4)));
    if(((i + 1) % 4) == 0) {
      DBG("%08x\n", tmp);
      tmp = 0;
    }
  }
}

/** \brief DMA Channel FIFO scanner
 * \param[out] completed_work list of completed \ref WorkItem_t
 * \param max_work max number of items; should be STS_SIZE*8
 * \param force_scan DO NOT USE this outside DMAChannel class
 * \return Number of completed items
 */
int DMAChannel::scanFIFO(WorkItem_t* completed_work, const int max_work, const int force_scan)
{
  if(m_restart_pending && !force_scan) return 0;

#ifdef UDMA_SIM_DEBUG
  if (7 <= g_level) { // DEBUG
    std::string s;
    dumpBDs(s);
    DBG("\n\tm_bl_busy_size=%d fifoRP=%u fifoWP=%u BD map before scan: %s\n",
        m_bl_busy_size, m_fifo_rd, (m_sim? m_sim_fifo_wp: getFIFOWriteCount()),
        s.c_str());
  }
#endif

  int compl_size = 0;
  DmaCompl_t compl_hwbuf[m_bd_num*2];
  uint64_t* sts_ptr = (uint64_t*)m_dmacompl.win_ptr;
  int j = m_fifo_rd * 8;
  const uint64_t HW_END = m_dmadesc.win_handle + m_dmadesc.win_size;
  int cwi = 0; // completed work index

  m_fifo_scan_cnt++;

  /* Check and clear descriptor status FIFO entries */
  while (sts_ptr[j] && !umdemo_must_die) {
    if(m_restart_pending && !force_scan) return 0;

    for (int i = j; i < (j+8) && sts_ptr[i]; i++) {
      if(m_restart_pending && !force_scan) return 0;
      DmaCompl_t c;
      c.ts_end = rdtsc();
      c.win_handle = sts_ptr[i]; c.fifo_offset = i;
      c.valid = COMPL_SIG;
      compl_hwbuf[compl_size++] = c;
      if(m_restart_pending && !force_scan) return 0;
      sts_ptr[i] = 0;
      m_tx_cnt++; // rather number of successfuly completed DMA ops
    } // END for line-of-8

    ++m_fifo_rd;
    m_fifo_rd %= m_sts_size;
    j = m_fifo_rd * 8;
  } // END while sts_ptr

  if(compl_size == 0) // No hw pointer to advance
    return 0;

#ifdef DEBUG_BD
    DBG("compl_size=%d\n", compl_size);
#endif

  for(int ci = 0; ci < compl_size; ci++) {
    if(m_restart_pending && !force_scan) return 0;

    pthread_spin_lock(&m_pending_work_splock);
  
    if (umdemo_must_die)
      return 0;

    if(compl_hwbuf[ci].valid != COMPL_SIG) {
      pthread_spin_unlock(&m_pending_work_splock);

      const int idx = (compl_hwbuf[ci].win_handle - m_dmadesc.win_handle) / DMA_BUFF_DESCR_SIZE; 

      ERR("\n\tFound INVALID completion iten for BD HW @0x%lx bd_idx=%d FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          compl_hwbuf[ci].win_handle,
          idx,
          compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    if((compl_hwbuf[ci].win_handle < m_dmadesc.win_handle) ||
       (compl_hwbuf[ci].win_handle >= HW_END)) {
      pthread_spin_unlock(&m_pending_work_splock);
      ERR("\n\tCan't find BD HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          compl_hwbuf[ci].win_handle, compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    // This should be optimised by g++
    const int idx = (compl_hwbuf[ci].win_handle - m_dmadesc.win_handle) / DMA_BUFF_DESCR_SIZE; 

    if(idx < 0 || idx >= m_bd_num) {
      pthread_spin_unlock(&m_pending_work_splock);

      ERR("\n\tCan't find bd_idx=%d IN RANGE for HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          idx,
          compl_hwbuf[ci].win_handle,
          compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    if(! m_pending_work[idx].valid) {
      pthread_spin_unlock(&m_pending_work_splock);

      ERR("\n\tCan't find VALID entry for HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          compl_hwbuf[ci].win_handle,
          compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    if(m_pending_work[idx].valid == 0xdeaddeedL) { // cleaned by cleanup?
      pthread_spin_unlock(&m_pending_work_splock);
      continue;
    }


    // With the spinlock in mind we make a copy here
    WorkItem_t item = m_pending_work[idx];
    m_pending_work[idx].valid = 0xdeadbeefL;

    pthread_spin_unlock(&m_pending_work_splock);

    if(m_restart_pending && !force_scan) return 0;

#ifdef DEBUG_BD
    DBG("\n\tFound idx=%d for HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
        idx,
        compl_hwbuf[ci].win_handle, compl_hwbuf[ci].fifo_offset,
        getFIFOReadCount(), getFIFOWriteCount());
#endif

    item.opt.ts_end = compl_hwbuf[ci].ts_end;

    if(item.opt.dtype == DTYPE2 && item.opt.rtype == NREAD) {
      const struct hw_dma_desc* bda = (struct hw_dma_desc*)(m_dmadesc.win_ptr);
      const struct hw_dma_desc* bd = &bda[item.opt.bd_idx];

      assert(bd->data >= m_dmadesc.win_ptr);
      assert((uint8_t*)bd->data < (((uint8_t*)m_dmadesc.win_ptr) + m_dmadesc.win_size));

      memcpy(item.t2_rddata, bd->data, 16);
      item.t2_rddata_len = le32(bd->bcount & 0xf);
    }

    completed_work[cwi++] = item;
    if(cwi == max_work)
      break;

#ifdef UDMA_SIM_DEBUG
    if ((m_bl_busy_size == 0 || m_pending_work[idx].opt.dtype > 3) && 7 <= g_level) { // DEBUG
      std::string s;
      dumpBDs(s);
      DBG("\n\tAt 0BUG point idx=%d bd_idx=%d WP=%u RP=%u BD map: %s\n", idx, item.opt.bd_idx, m_dma_wr, (m_sim? m_sim_dma_rp: getReadCount()), s.c_str());
    }
#endif

    assert(m_pending_work[idx].opt.dtype <= 3);

    // BDs might be completed out of order.
    pthread_spin_lock(&m_bl_splock); 

#ifdef UDMA_SIM_DEBUG
    if (idx > 0 && idx < (m_bd_num - 1)) { // clear DTYPE - DEBUG
      struct hw_dma_desc* bd_p = (struct hw_dma_desc*)((uint8_t*)m_dmadesc.win_ptr + (item.opt.bd_idx * DMA_BUFF_DESCR_SIZE));
      uint32_t* bytes03 = (uint32_t*)bd_p;
      *bytes03 &= 0x0FFFFFFFUL;
      *bytes03 |= 0x7 << 29;
      m_pending_work[idx].opt.dtype = 7;
    }
#endif

    if (umdemo_must_die)
      return 0;
    m_bl_busy[item.opt.bd_idx] = false;
    m_bl_busy_size--;
    assert(m_bl_busy_size >= 0);

    pthread_spin_unlock(&m_bl_splock); 
  } // END for compl_size

  // Before advancing FIFO RP I must have a "barrier" so no "older" BDs exist.

  if (!m_sim) wr32dmachan(TSI721_DMAC_DSRP, m_fifo_rd);

  return cwi;
}

/** \brief Restarts DMA channel
 * \param nuke_bds Do a clean slate restart, everything is zero'ed
 * \note IF nuke_bds==false THEN the current DMA WP is reprogrammed with the expectation that the list of BDs was cleaned by \ref cleanupBDQueue
 */
void DMAChannel::softRestart(const bool nuke_bds)
{
  const uint64_t ts_s = rdtsc();
  m_restart_pending = 1;

  pthread_spin_lock(&m_bl_splock);

  // Clear FIFO for good measure
  memset(m_dmacompl.win_ptr, 0, m_dmacompl.win_size);
  m_fifo_rd = 0;

  if (nuke_bds) {
    // Clear BDs
    memset(m_dmadesc.win_ptr, 0, m_dmadesc.win_size);

    memcpy((uint8_t*)m_dmadesc.win_ptr, &m_BD0_T3_saved, DMA_BUFF_DESCR_SIZE); // Reset BD0 as jump+1

    memset(m_bl_busy, 0,  m_bd_num * sizeof(bool));
    m_bl_busy_size = 0;

    memset(m_pending_work, 0, (m_bd_num+1) * sizeof(WorkItem_t));
  }

  // Just be paranoid about wrap-around descriptor
  struct hw_dma_desc* end_bd_p = (struct hw_dma_desc*)
    ((uint8_t*)m_dmadesc.win_ptr + ((m_bd_num-1) * DMA_BUFF_DESCR_SIZE));

  struct dmadesc end_bd; memset(&end_bd, 0, sizeof(end_bd));
  dmadesc_setdtype(end_bd, DTYPE3);
  dmadesc_setT3_nextptr(end_bd, (uint64_t)m_dmadesc.win_handle);

  end_bd.pack(end_bd_p);

  const uint32_t DMA_WP = m_dma_wr;

  resetHw(); // clears m_dma_wr

  if (m_sim) goto done;

  // Setup DMA descriptor pointers
  wr32dmachan(TSI721_DMAC_DPTRH, (uint64_t)m_dmadesc.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DPTRL, (uint64_t)m_dmadesc.win_handle & TSI721_DMAC_DPTRL_MASK);

  // Setup descriptor status FIFO
  wr32dmachan(TSI721_DMAC_DSBH, (uint64_t)m_dmacompl.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DSBL, (uint64_t)m_dmacompl.win_handle & TSI721_DMAC_DSBL_MASK);
  wr32dmachan(TSI721_DMAC_DSSZ, m_sts_log_two);

done:
  m_dma_wr = nuke_bds? 1 /*BD0 is T3*/: DMA_WP;

  setWriteCount(m_dma_wr); // knows about sim

  pthread_spin_unlock(&m_bl_splock);
  m_restart_pending = 0;
  const uint64_t ts_e = rdtsc();

  INFO("dT = %llu TICKS; DMA WP := %d%s\n", (ts_e - ts_s), DMA_WP, (nuke_bds? "; NUKED BDs": ""));
}

/** \brief Simulate FIFO completions; NO errors are injected
 * \note Should be called in isolcpu thread before \ref scanFIFO
 * \param max_bd Process at most max_bd then report fault. If 0 no faults reported
 * \param fault_bmask Which fault registers to fake
 * \return How many BDs have been processed (but not necessarily reported in FIFO)
 */
int DMAChannel::simFIFO(const int max_bd, const uint32_t fault_bmask)
{
  if (!m_sim)
    throw std::runtime_error("DMAChannel: Simulation was not flagged!");

  uint64_t* sts_ptr = (uint64_t*)m_dmacompl.win_ptr;
  const uint64_t HW_END = m_dmadesc.win_handle + m_dmadesc.win_size;

#ifdef UDMA_SIM_DEBUG
  if (7 <= g_level) { // DEBUG
    DBG("START: DMA simRP=%d WP=%d simFifoWP=%d m_bl_busy_size=%d\n", m_sim_dma_rp, m_dma_wr, m_sim_fifo_wp, m_bl_busy_size);

    std::string s;
    dumpBDs(s);
    DBG("\n\tm_bl_busy_size=%d BD map: %s\n", m_bl_busy_size, s.c_str());
  }
#endif

  pthread_spin_lock(&m_bl_splock);

  int bd_cnt = 0;
  bool faulted = false;
  for (; m_sim_dma_rp < m_dma_wr; /*DONOTUSE continue*/) { // XXX "<=" ??
    // Handle wrap-arounds in BD array, m_dma_wr can go up to 0xFFFFFFFFL
    const int idx = m_sim_dma_rp % m_bd_num;

    struct hw_dma_desc* bd_p = (struct hw_dma_desc*)((uint8_t*)m_dmadesc.win_ptr + (idx * DMA_BUFF_DESCR_SIZE));
    uint32_t* bytes03 = (uint32_t*)bd_p;
    const uint32_t dtype = *bytes03 >> 29;

#ifdef UDMA_SIM_DEBUG
    if (dtype > 3) {
      CRIT("BUG: BD%d dtype=%d DMA WP=%d simRP=%d\n", idx, dtype, m_dma_wr, m_sim_dma_rp);
      assert(dtype <= 3);
    }
#endif

    // Soft restart might have inserted T3 anywhere. Skip checks on them.
    if (dtype != DTYPE3 && idx != (m_bd_num-1)) { assert(m_bl_busy[idx]); } // We don't mark the T3 BD as "busy"

    const uint64_t bd_linear = m_dmadesc.win_handle + idx * DMA_BUFF_DESCR_SIZE;
    assert(bd_linear < HW_END);

    bd_cnt++;
    assert(bd_cnt <= m_bd_num);

    if (idx == (m_bd_num-1)) {
      assert(dtype == DTYPE3); // BD(bufc-1) is T3
      goto next;
    }

    if (idx == 0) {
      assert(dtype == DTYPE3); // BD0 is T3
    
      if (! memcmp(m_dmadesc.win_ptr, &m_BD0_T3_saved, DMA_BUFF_DESCR_SIZE)) { goto next; } // BD0 jumps to BD1

      // BD0 does not jump to BD1

      const hw_dma_desc* bd_ptr = (hw_dma_desc*)m_dmadesc.win_ptr;

      uint64_t next_linear = le32(bd_ptr->next_hi);
      next_linear <<= 32;
      next_linear |= le32(bd_ptr->next_lo);

      assert(next_linear);
      assert(m_dmadesc.win_handle < next_linear);
      assert(next_linear < HW_END);

      uint64_t next_off = next_linear - m_dmadesc.win_handle;
      const int next_idx = next_off / DMA_BUFF_DESCR_SIZE; 

      assert(next_idx > 0); // cannot be BD0
      assert(next_idx < (m_bd_num - 1)); // cannot be BD(bufc-1)

      DBG("BD0 jumps to linear address 0x%llx (offset 0x%llx) as BD%d -- DMA simRP %d->%d\n",
          next_linear, next_off, next_idx, m_sim_dma_rp, m_sim_dma_rp + next_idx);

      m_sim_dma_rp += next_idx;

      goto move_fifo_wp;
    }

    assert(dtype == DTYPE1 || dtype == DTYPE3); // no other T3s than BD0 and BD(bufc-1)

    // FAULT INJECTION
    if (max_bd > 0 && fault_bmask != 0 && bd_cnt == max_bd) { // T3 does not fault
      // DO NOT report BD in FIFO
      // Pretend registers report fault
      do {
        if (fault_bmask & SIM_INJECT_TIMEOUT) { m_sim_abort_reason = 5; break; }
        if (fault_bmask & SIM_INJECT_ERR_RSP) { m_sim_abort_reason = 6; break; }
        if (fault_bmask & SIM_INJECT_INP_ERR) { m_sim_err_stat = TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP; break; }
        if (fault_bmask & SIM_INJECT_OUT_ERR) { m_sim_err_stat = TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP; break; }
      } while(0);

      faulted = true;

      break; // for loop
    } // END FAULT

next:
    m_sim_dma_rp++;

move_fifo_wp:
    sts_ptr[m_sim_fifo_wp*8] = bd_linear;

    m_sim_fifo_wp++;
    m_sim_fifo_wp %= m_sts_size;
  } // END for DMA RP

  pthread_spin_unlock(&m_bl_splock);

  DBG("END: simRP=%d WP=%d simFifoWP=%d processed %d BDs%s\n", m_sim_dma_rp, m_dma_wr, m_sim_fifo_wp, bd_cnt, faulted? " WITH FAULT": "");

  return bd_cnt;
}

void DMAChannel::dumpBDs(std::string& s)
{
  std::stringstream ss;
  for (int idx = 0; idx < m_bd_num; idx++) {
    char* star = "";
    struct hw_dma_desc* bd_p = (struct hw_dma_desc*)((uint8_t*)m_dmadesc.win_ptr + (idx * DMA_BUFF_DESCR_SIZE));
    if (idx == 0 && memcmp(m_dmadesc.win_ptr, &m_BD0_T3_saved, DMA_BUFF_DESCR_SIZE)) { star = "*"; }
   
    uint32_t* bytes03 = (uint32_t*)bd_p;
    const uint32_t dtype = *bytes03 >> 29;
    ss << star << dtype << " ";
  }
  s.append(ss.str());
}

/** \brief Clean up queue of offending BDs. Replace them with T3 (jump+1) NOPs.
 * \note If there's nothing pending call \ref softRestart(true)
 * \param multithreaded_fifo Hints whether we should expect another (isolcpu) thread to run scanFIFO and reap the results
 * \return Count of pending BDs left after cleanup
 */
int DMAChannel::cleanupBDQueue(bool multithreaded_fifo)
{
  int pending = 0;

  const uint64_t fifo_scan_cnt = m_fifo_scan_cnt;

#ifdef UDMA_SIM_DEBUG
  if (7 <= g_level) { // DEBUG
    DBG("DMA %sRP=%d WP=%d m_bl_busy_size=%d\n",
        m_sim? "sim": "Tsi721",
        m_sim? m_sim_dma_rp: getReadCount(),
        m_dma_wr, m_bl_busy_size);

    std::stringstream ss;
    uint32_t rp = m_sim? m_sim_dma_rp: getReadCount();
    const int badidx = rp % m_bd_num;
    assert(badidx != (m_bd_num-1));
    for (int idx = 0; idx < m_bd_num; idx++) {
      char* star = "";
      if (idx == 0 && memcmp(m_dmadesc.win_ptr, &m_BD0_T3_saved, DMA_BUFF_DESCR_SIZE)) { star = "*"; }
      struct hw_dma_desc* bd_p = (struct hw_dma_desc*)((uint8_t*)m_dmadesc.win_ptr + (idx * DMA_BUFF_DESCR_SIZE));
      uint32_t* bytes03 = (uint32_t*)bd_p;
      const uint32_t dtype = *bytes03 >> 29;
      if (idx != badidx) ss << dtype << " ";
      else ss << star << "!" << dtype << "! ";
    }
    DBG("\n\tBD map before cleanup: %s\n", ss.str().c_str());
  }
#endif

  struct dmadesc T3_bd; memset(&T3_bd, 0, sizeof(T3_bd));
  dmadesc_setdtype(T3_bd, DTYPE3);

// We must have the FIFO cleared of completed entries.
// First wait a bit and hope there's another thread running it

  if (multithreaded_fifo) {
    for (int i = 0; i < 5; i++) {
      if (fifo_scan_cnt < m_fifo_scan_cnt) break;
      struct timespec tv = { 0, 1 };
      nanosleep(&tv, NULL);
    }
  }

// Nope, wasn't. DIY scanFIFO
  if (! multithreaded_fifo || fifo_scan_cnt == m_fifo_scan_cnt) { // Run FIFO scanner ourselves
    DMAChannel::WorkItem_t wi[m_sts_size*8]; memset(wi, 0, sizeof(wi));
    int tmp = m_restart_pending;
    m_restart_pending = 1; // Tell scanFIFO running from other thread to back off
    scanFIFO(wi, m_sts_size*8, 1 /*force_scan*/);
    m_restart_pending = tmp;
  }

  const uint64_t ts_s = rdtsc();
  pthread_spin_lock(&m_bl_splock);

  do {
// This is the place where we faulted // XXX Really or rp-1?
    uint32_t rp = m_sim? m_sim_dma_rp: getReadCount();

    // Is there more stuff queued after faulting BD? -- Does NOT handle FFFFFFFF wrap-around!
    if (m_bl_busy_size < 2) break;
    if (! (m_dma_wr > rp)) break; // signal upstairs to nuke all BDs, etc. Nothing to salvage.

    const int badidx = rp % m_bd_num;

    assert(badidx != 0 && badidx != (m_bd_num-1)); // Should never fault at BD0 or wrap-around BD!!

    struct hw_dma_desc* bd_p = (struct hw_dma_desc*)((uint8_t*)m_dmadesc.win_ptr + (badidx * DMA_BUFF_DESCR_SIZE));
    uint32_t* bytes03 = (uint32_t*)bd_p;
    const uint32_t dtype = *bytes03 >> 29;

    if (dtype != DTYPE3) { assert(m_bl_busy[badidx]); } // We don't mark the T3 BD as "busy"

    pending = m_dma_wr - rp - 1;

// Replace BD0 with T3 jump to next BD unless fault is at (bufc - 2)
    if (badidx < (m_bd_num-2)) {
      const uint32_t next_off = (badidx + 1) * DMA_BUFF_DESCR_SIZE;
      struct hw_dma_desc* bd0_p = (struct hw_dma_desc*)m_dmadesc.win_ptr; // BD0

      dmadesc_setT3_nextptr(T3_bd, (uint64_t)(m_dmadesc.win_handle + next_off));
      T3_bd.pack(bd0_p);

      m_bl_busy[badidx] = false;
      // m_bl_busy_size--; // NO: BD0 must be accounted for
      m_pending_work[0].valid = WI_SIG; 
      assert(m_bl_busy_size >= 0);

      DBG("\n\tPatch BD0 to jump at idx=%d with T3 pending=%d\n", badidx+1, pending);

#ifdef UDMA_SIM_DEBUG
      {
        struct hw_dma_desc* bd_p = (struct hw_dma_desc*)((uint8_t*)m_dmadesc.win_ptr + (badidx * DMA_BUFF_DESCR_SIZE));
        uint32_t* bytes03 = (uint32_t*)bd_p;
        *bytes03 &= 0x0FFFFFFFUL;
        *bytes03 |= 0x5 << 29;
        m_pending_work[badidx].opt.dtype = 5;
      }
#endif
    } 

    m_dma_wr = 1 + pending; // Just skip BD0 which is T3 // Barry sez this is BD count
    DBG("\n\tAfter reset DMA WP := %u\n", m_dma_wr);
  } while(0);

  pthread_spin_unlock(&m_bl_splock);
  const uint64_t ts_e = rdtsc();

#ifdef UDMA_SIM_DEBUG
  if (7 <= g_level) { // DEBUG
    std::string s;
    dumpBDs(s);
    DBG("\n\tm_bl_busy_size=%d BD map after cleanup: %s\n", m_bl_busy_size, s.c_str());
  }
#endif

  INFO("dT = %llu TICKS\n", (ts_e - ts_s));

  return pending;
}
