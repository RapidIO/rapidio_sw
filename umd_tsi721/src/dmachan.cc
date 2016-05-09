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
#include "libtime_utils.h"

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
  umdemo_must_die = 0;

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
  m_bl_busy_size  = -1;
  m_check_reg     = false;
  m_restart_pending = 0;
  m_sts_log_two   = 0;
  m_bl_busy_histo = NULL;

#ifdef DHACHAN_TICKETED
  initTicketed();
#endif
}

DMAChannel::DMAChannel(const uint32_t mportid, const uint32_t chan)
#ifdef DHACHAN_TICKETED
  : DMAShmPendingData(mportid)
#endif
{
  if(chan >= RioMport::DMA_CHAN_COUNT)
    throw std::runtime_error("DMAChannel: Invalid channel!");

  m_chan  = chan;

  m_mport = new RioMport(mportid);

  init();
}

DMAChannel::DMAChannel(const uint32_t mportid, const uint32_t chan, riomp_mport_t mp_hd)
#ifdef DHACHAN_TICKETED
  : DMAShmPendingData(mportid)
#endif
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
  (char *)"NREAD",
  (char *)"LAST_NWRITE_R",
  (char *)"ALL_NWRITE",
  (char *)"ALL_NWRITE_R",
  (char *)"MAINT_RD",
  (char *)"MAINT_WR",
  (char *)NULL
};

bool DMAChannel::dmaIsRunning()
{
  uint32_t channel_status = rd32dmachan(TSI721_DMAC_STS);
  if (channel_status & TSI721_DMAC_STS_RUN) return true;
  return false;
}

void DMAChannel::resetHw()
{
  if(dmaIsRunning()) {
    wr32dmachan(TSI721_DMAC_CTL, TSI721_DMAC_CTL_SUSP);

    for(int i = 0; i < 1000; i++) {
      if(! rd32dmachan(TSI721_DMAC_CTL) & TSI721_DMAC_CTL_SUSP) break;
    }
  }

  wr32dmachan(TSI721_DMAC_CTL, TSI721_DMAC_CTL_INIT);
  wr32dmachan(TSI721_DMAC_INT, TSI721_DMAC_INT_ALL);
  usleep(10);
  wr32dmachan(TSI721_DMAC_DWRCNT, m_dma_wr = 0);
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
  if ((opt.dtype != DTYPE1) && (opt.dtype != DTYPE2))
    return false;

  if ((opt.dtype == DTYPE1) && !m_mport->check_dma_buf(mem))
    return false;

  struct dmadesc desc;
  bool queued_T3 = false;

  WorkItem_t wk_end; memset(&wk_end, 0 , sizeof(wk_end));
  WorkItem_t wk; memset(&wk, 0, sizeof(wk));

  abort_reason = 0;

  ts_now_mark(ts_p, 1);

  opt.rtype = rtype;
  dmadesc_setdtype(desc, opt.dtype);

  if (opt.iof)    dmadesc_setiof(desc, 1);
  if (opt.crf)    dmadesc_setcrf(desc, 1);
  if (opt.prio)   dmadesc_setprio(desc, opt.prio);
  if (opt.tt_16b) dmadesc_set_tt(desc, 1);

  dmadesc_setrtype(desc, rtype); // NWRITE_R, etc
  dmadesc_set_raddr(desc, opt.raddr.msb2, opt.raddr.lsb64);
  dmadesc_setdevid(desc, opt.destid);

  if (opt.dtype == DTYPE1) {
    dmadesc_setT1_bufptr(desc, mem.win_handle);
    dmadesc_setT1_buflen(desc, opt.bcount);
  } else { // T2
    if (rtype != NREAD)  { // copy data
      dmadesc_setT2_data(desc, (const uint8_t*)mem.win_ptr, mem.win_size);
    } else {
      uint8_t ZERO[16] = {0};
      dmadesc_setT2_data(desc, ZERO, mem.win_size);
    }
  }

  // Check if queue full -- as late as possible in view of MT
  if(queueFull()) {
    XERR("\n\tFAILED: DMA TX Queue full! chan=%u\n", m_chan);
    return false;
  }
  
  wk.mem = mem;

  pthread_spin_lock(&m_bl_splock); 
  if (umdemo_must_die)
    return false;

  int bd_idx = m_dma_wr % m_bd_num;
  {{
    // If at end of buffer, account for T3 and
    // wrap around to beginning of buffer.
    if ((bd_idx + 1) == m_bd_num) {
      wk_end.bd_wp = m_dma_wr;
      wk_end.ts_start = rdtsc();

      // FIXME: Should this really be FF..E, or should it be FF..F ???
      if (m_dma_wr == 0xFFFFFFFE)
           m_dma_wr = 0;
      else m_dma_wr++;
      ///setWriteCount(m_dma_wr);

      queued_T3 = true;

      assert(m_bl_busy_size >= 0);
      m_bl_busy_size++;
      wk_end.bl_busy_size = m_bl_busy_size;
      bd_idx = 0;
    }

    // check-modulo in m_bl_busy[] if bd_idx is still busy!!
    if(m_bl_busy[bd_idx]) {
      pthread_spin_unlock(&m_bl_splock); 
      assert(m_bl_busy_size >= 0);
#ifdef DEBUG_BD
      XINFO("\n\tDMA TX queueDmaOpT?: BD %d still busy!\n", bd_idx);
#endif
      return false;
    }

    m_bl_busy[bd_idx] = true;
    assert(m_bl_busy_size >= 0);
    m_bl_busy_size++;
    wk.bl_busy_size = m_bl_busy_size;

    if (m_bl_busy_histo != NULL) m_bl_busy_histo[m_bl_busy_size]++;

    struct hw_dma_desc* bd_hw = (struct hw_dma_desc*)(m_dmadesc.win_ptr) + bd_idx;
    desc.pack(bd_hw);

    wk.bd_wp    = m_dma_wr;
    wk.bd_idx   = bd_idx;
    wk.ts_start = rdtsc();

#ifdef DHACHAN_TICKETED
    opt.ts_start = wk.ts_start;
    opt.ticket   = ++m_serial_number;

    computeNotBefore(opt);
#endif

    pthread_spin_lock(&m_pending_work_splock);
     wk.opt   = opt;
     wk.valid = WI_SIG;

     m_pending_work[bd_idx] = wk;

     if(queued_T3) {
       wk_end.opt.dtype = DTYPE3;
       wk_end.valid     = WI_SIG;
       m_pending_work[m_T3_bd_hw] = wk_end;
     }
    pthread_spin_unlock(&m_pending_work_splock);

    m_dma_wr++;
    setWriteCount(m_dma_wr);
    if(m_dma_wr == 0xFFFFFFFE) m_dma_wr = 0;

#ifdef DHACHAN_TICKETED
    assert(m_pending_tickets[bd_idx] == 0);
    m_pending_tickets[bd_idx] = opt.ticket;

    if (m_pendingdata_tally != NULL) {
      assert(m_pendingdata_tally->data[m_chan] >= 0);
      m_pendingdata_tally->data[m_chan] += opt.bcount;
    }
#endif
  }}

  pthread_spin_unlock(&m_bl_splock); 

  if(m_check_reg && dmaCheckAbort(abort_reason)) {
    m_pending_work[bd_idx].valid = 0xbeefbaadL;
    return false; // XXX maybe not, Barry says reading from PCIe is dog-slow
  }

  ts_now_mark(ts_p, 9);

#ifdef DEBUG_BD
  const uint64_t offset = (uint8_t*)bd_hw - (uint8_t*)m_dmadesc.win_ptr;

  XDBG("\n\tQueued DTYPE%d op=%s as BD HW @0x%lx bd_wp=%d\n",
      wk.opt.dtype, dma_rtype_str[rtype] , m_dmadesc.win_handle + offset, wk.opt.bd_wp);

  if(queued_T3)
     XDBG("\n\tQueued DTYPE%d as BD HW @0x%lx bd_wp=%d\n", wk_end.opt.dtype, m_dmadesc.win_handle + m_T3_bd_hw, wk_end.opt.bd_wp);
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
    XCRIT("DMAChannel: Cannot alloc DMA TX ring descriptors!");
    return false;
  }

  m_bd_num = bd_cnt;

  m_bl_busy_histo = (uint64_t*)calloc(m_bd_num, sizeof(uint64_t));

  m_bl_busy = (bool*)calloc(m_bd_num, sizeof(bool));
  m_bl_busy_size = 0;

  m_pending_work = (WorkItem_t*)calloc(m_bd_num+1, sizeof(WorkItem_t)); // +1 to have a guard, NOT used

#ifdef DHACHAN_TICKETED
  m_pending_tickets = (uint64_t*)calloc((m_bd_num+1), sizeof(uint64_t));
#endif

  memset(m_dmadesc.win_ptr, 0, m_dmadesc.win_size);

  struct hw_dma_desc* end_bd_p = (struct hw_dma_desc*)
    ((uint8_t*)m_dmadesc.win_ptr + ((m_bd_num-1) * DMA_BUFF_DESCR_SIZE));

#ifdef DEBUG_BD
  XDBG("\n\tWrap BD DTYPE3 @ HW 0x%lx [idx=%d] points back to HW 0x%lx\n",
      m_dmadesc.win_handle + ((m_bd_num-1) *DMA_BUFF_DESCR_SIZE),
      m_bd_num-1, m_dmadesc.win_handle);
#endif

  // Initialize DMA descriptors ring using added link descriptor 
  struct dmadesc end_bd; memset(&end_bd, 0, sizeof(end_bd));
  dmadesc_setdtype(end_bd, DTYPE3);
  dmadesc_setT3_nextptr(end_bd, (uint64_t)m_dmadesc.win_handle);

  end_bd.pack(end_bd_p); // XXX mask off lowest 5 bits

  m_T3_bd_hw = m_bd_num-1;

  // Setup DMA descriptor pointers
  wr32dmachan(TSI721_DMAC_DPTRH, (uint64_t)m_dmadesc.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DPTRL, 
    (uint64_t)m_dmadesc.win_handle & TSI721_DMAC_DPTRL_MASK); 

  return true;
}

void DMAChannel::free_dmatxdesc()
{
  m_mport->unmap_dma_buf(m_dmadesc);

#ifdef DHACHAN_TICKETED
  free(m_pending_tickets); m_pending_tickets = NULL;
#endif
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
    XDBG("\n\tDMA Completion Count too small: %d", bd_cnt);
    sts_entry_cnt = TSI721_DMA_MINSTSSZ;
  }
  if (sts_entry_cnt > max_entry_cnt) {
    XDBG("\n\tDMA Completion Count TOO BIG: %d", bd_cnt);
    sts_entry_cnt = max_entry_cnt;
  }

  m_sts_size = roundup_pow_of_two(sts_entry_cnt);
  m_sts_log_two = pow_of_two(sts_entry_cnt) - 4;

  m_dmacompl.rio_address = RIO_ANY_ADDR;
  if (!m_mport->map_dma_buf(m_sts_size * 64, m_dmacompl)) {
    XERR("DMAChannel: Cannot alloc HW mem for DMA completion ring!");
    goto exit;
  }

  memset(m_dmacompl.win_ptr, 0, m_dmacompl.win_size);

  // Setup descriptor status FIFO 
  wr32dmachan(TSI721_DMAC_DSBH,
    (uint64_t)m_dmacompl.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DSBL,
    (uint64_t)m_dmacompl.win_handle & TSI721_DMAC_DSBL_MASK);
  wr32dmachan(TSI721_DMAC_DSSZ, m_sts_log_two);

#if 0
  XINFO("\n\tDMA compl entries %d bytes=%d @%p HW@0x%llx\n",
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
    m_bl_busy_size = -1;
  }

  if (m_bl_busy_histo != NULL) {
    free((void*)m_bl_busy_histo);
    m_bl_busy_histo = NULL;
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
  XDBG("%s", ss.str().c_str());
  return true;
}

void hexdump4byte(const char* msg, uint8_t* d, int len)
{
  if(msg != NULL)
    XDBG("%s", msg);
  XDBG("Mem @%p size %d:\n", d, len);

   uint32_t tmp = 0;
  for(int i = 0; i < len; i++) {
  /* To make descriptor dump match Tsi721 manual,
 * must push byte 3 to most significant position, not least significant
 */
    tmp = tmp + ((uint32_t)(d[i]) << (8 * (i%4)));
    if(((i + 1) % 4) == 0) {
      XDBG("%08x\n", tmp);
      tmp = 0;
    }
  }
}

int DMAChannel::scanFIFO(WorkItem_t* completed_work, const int max_work)
{
  if(m_restart_pending) return 0;

  int compl_size = 0;
  DmaCompl_t compl_hwbuf[m_bd_num*2];
  uint64_t* sts_ptr = (uint64_t*)m_dmacompl.win_ptr;
  int j = m_fifo_rd * 8;
  const uint64_t HW_END = m_dmadesc.win_handle + m_dmadesc.win_size;
  int cwi = 0; // completed work index

  m_fifo_scan_cnt++;

#if 0//def DEBUG
  if(hexdump64bit(m_dmacompl.win_ptr, m_dmacompl.win_size))
    XDBG("\n\tFIFO hw RP=%u WP=%u\n", getFIFOReadCount(), getFIFOWriteCount());
#endif

  /* Check and clear descriptor status FIFO entries */
  while (sts_ptr[j] && !umdemo_must_die) {
    if(m_restart_pending) return 0;

    for (int i = j; i < (j+8) && sts_ptr[i]; i++) {
      DmaCompl_t c;
      c.ts_end = rdtsc();
      c.win_handle = sts_ptr[i]; c.fifo_offset = i;
      c.valid = COMPL_SIG;
      compl_hwbuf[compl_size++] = c;
      sts_ptr[i] = 0;
      m_tx_cnt++; // rather number of successfuly completed DMA ops
    } // END for line-of-8

    ++m_fifo_rd;
    m_fifo_rd %= m_sts_size;
    j = m_fifo_rd * 8;
  } // END while sts_ptr

  if(compl_size == 0) // No hw pointer to advance
    return 0;

  for(int ci = 0; ci < compl_size; ci++) {
    if(m_restart_pending) return 0;

    pthread_spin_lock(&m_pending_work_splock);
  
    if (umdemo_must_die)
      return 0;

    if(compl_hwbuf[ci].valid != COMPL_SIG) {
      pthread_spin_unlock(&m_pending_work_splock);

      const int idx = (compl_hwbuf[ci].win_handle - m_dmadesc.win_handle) / DMA_BUFF_DESCR_SIZE; 

      XERR("\n\tFound INVALID completion item for BD HW @0x%lx bd_idx=%d FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          compl_hwbuf[ci].win_handle,
          idx,
          compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    if((compl_hwbuf[ci].win_handle < m_dmadesc.win_handle) ||
       (compl_hwbuf[ci].win_handle >= HW_END)) {
      pthread_spin_unlock(&m_pending_work_splock);
      XERR("\n\tCan't find BD HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          compl_hwbuf[ci].win_handle, compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    // This should be optimised by g++
    const int idx = (compl_hwbuf[ci].win_handle - m_dmadesc.win_handle) / DMA_BUFF_DESCR_SIZE; 

    if(idx < 0 || idx >= m_bd_num) {
      pthread_spin_unlock(&m_pending_work_splock);

      XERR("\n\tCan't find bd_idx=%d IN RANGE for HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          idx,
          compl_hwbuf[ci].win_handle,
          compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    const uint32_t valid = m_pending_work[idx].valid;

    if(WI_SIG != valid) {
      pthread_spin_unlock(&m_pending_work_splock);

      XERR("\n\tCan't find VALID (invalid sig=0x%x) [bd_idx=%d] entry for HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
          valid, idx,
          compl_hwbuf[ci].win_handle,
          compl_hwbuf[ci].fifo_offset,
          getFIFOReadCount(), getFIFOWriteCount());
      continue;
    }

    // With the spinlock in mind we make a copy here
    WorkItem_t item = m_pending_work[idx];
    m_pending_work[idx].valid = 0xdeadbeefL;

    pthread_spin_unlock(&m_pending_work_splock);

    if(m_restart_pending) return 0;

#ifdef DEBUG_BD
    XDBG("\n\tFound idx=%d for HW @0x%lx FIFO offset 0x%x in m_pending_work -- FIFO hw RP=%u WP=%u\n",
        idx,
        compl_hwbuf[ci].win_handle, compl_hwbuf[ci].fifo_offset,
        getFIFOReadCount(), getFIFOWriteCount());
#endif

    item.ts_end = compl_hwbuf[ci].ts_end;

    if(item.opt.dtype == DTYPE2 && item.opt.rtype == NREAD) {
      const struct hw_dma_desc* bda = (struct hw_dma_desc*)(m_dmadesc.win_ptr);
      const struct hw_dma_desc* bd = &bda[item.bd_idx];

      assert(bd->data >= m_dmadesc.win_ptr);
      assert((uint8_t*)bd->data < (((uint8_t*)m_dmadesc.win_ptr) + m_dmadesc.win_size));

      memcpy(item.t2_rddata, bd->data, 16);
      item.t2_rddata_len = le32(bd->bcount & 0xf);
    }

    completed_work[cwi++] = item;
    if(cwi == max_work)
      break;

    // BDs might be completed out of order.
    pthread_spin_lock(&m_bl_splock); 

    if (umdemo_must_die)
      return 0;
    m_bl_busy[item.bd_idx] = false;
    m_bl_busy_size--;
    assert(m_bl_busy_size >= 0);

#ifdef DHACHAN_TICKETED
    if (item.opt.dtype == DTYPE3 || idx == (m_bd_num-1)) goto unlock;

    {{

    if (m_pendingdata_tally != NULL) {
      assert(m_pendingdata_tally->data[m_chan] >= 0);
      m_pendingdata_tally->data[m_chan] -= item.opt.bcount;
    }

///    if (item.opt.ticket > m_state->acked_serial_number) m_state->acked_serial_number = item.opt.ticket;

    assert(m_pending_tickets_RP <= m_serial_number);

    const int P = m_serial_number - m_pending_tickets_RP; // Pending issued tickets
    //assert(P); // If we're here it cannot be 0

    assert(m_pending_tickets[item.bd_idx] > 0);
    assert(m_pending_tickets[item.bd_idx] == item.opt.ticket);

    m_pending_tickets[item.bd_idx] = 0; // cancel ticket

    int k = 0;
    int i = m_pending_tickets_RP % m_bd_num;
    for (; P > 0 && k < m_bd_num; i++) {
      assert(i < m_bd_num);
      if (i == 0) continue; // T3 BD0 does not get a ticket
      if (i == (m_bd_num-1)) { i = 0; continue; } // T3 BD(bufc-1) does not get a ticket
      if (m_pending_tickets[i] > 0) break; // still in flight
      k++;
      if (k == P) break; // Upper bound
    }
#ifdef DEBUG_BD
    XDBG("\n\tDMA bd_idx=%d rtype=%d Ticket=%llu S/N=%llu Pending=%d pending_tickets_RP=%llu => k=%d\n",
         item.bd_idx, item.opt.rtype, item.opt.ticket, m_state->serial_number, P, m_pending_tickets_RP, k);
#endif
    if (k > 0) {
      m_pending_tickets_RP += k;
      assert(m_pending_tickets_RP <= m_serial_number);
      m_acked_serial_number = m_pending_tickets_RP; // XXX Perhaps +1?
    }

    }}
unlock:
#endif // DHACHAN_TICKETED

    pthread_spin_unlock(&m_bl_splock); 
  } // END for compl_size

  // Before advancing FIFO RP I must have a "barrier" so no "older" BDs exist.

  wr32dmachan(TSI721_DMAC_DSRP, m_fifo_rd);

  return cwi;
}

void DMAChannel::softRestart(const bool nuke_bds)
{
  const uint64_t ts_s = rdtsc();
  m_restart_pending = 1;

  // Clear FIFO for good measure
  memset(m_dmacompl.win_ptr, 0, m_dmacompl.win_size);
  m_fifo_rd = 0;

  if (nuke_bds) {
    // Clear BDs
    memset(m_dmadesc.win_ptr, 0, m_dmadesc.win_size);

    memset(m_bl_busy, 0,  m_bd_num * sizeof(bool));
    m_bl_busy_size = 0;

    memset(m_pending_work, 0, (m_bd_num+1) * sizeof(WorkItem_t));

#ifdef DHACHAN_TICKETED
    memset(m_pending_tickets, 0, (m_bd_num+1)*sizeof(uint64_t));
#endif
  }

  struct hw_dma_desc* end_bd_p = (struct hw_dma_desc*)
    ((uint8_t*)m_dmadesc.win_ptr + ((m_bd_num-1) * DMA_BUFF_DESCR_SIZE));

  // Initialize DMA descriptors ring using added link descriptor
  struct dmadesc end_bd; memset(&end_bd, 0, sizeof(end_bd));
  dmadesc_setdtype(end_bd, DTYPE3);
  dmadesc_setT3_nextptr(end_bd, (uint64_t)m_dmadesc.win_handle);

  end_bd.pack(end_bd_p);

  resetHw(); // clears m_dma_wr

  // Setup DMA descriptor pointers
  wr32dmachan(TSI721_DMAC_DPTRH, (uint64_t)m_dmadesc.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DPTRL, (uint64_t)m_dmadesc.win_handle & TSI721_DMAC_DPTRL_MASK);

  // Setup descriptor status FIFO
  wr32dmachan(TSI721_DMAC_DSBH, (uint64_t)m_dmacompl.win_handle >> 32);
  wr32dmachan(TSI721_DMAC_DSBL, (uint64_t)m_dmacompl.win_handle & TSI721_DMAC_DSBL_MASK);
  wr32dmachan(TSI721_DMAC_DSSZ, m_sts_log_two);

  m_restart_pending = 0;
  const uint64_t ts_e = rdtsc();

  XINFO("dT = %llu TICKS\n", (ts_e - ts_s));
}
