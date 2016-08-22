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
#include <algorithm> // std::sort

#include "rio_misc.h"
#include "IDT_Tsi721.h"

#include "mhz.h"
#include "mport.h"
#include "dmadesc.h"
#include "rdtsc.h"
#include "debug.h"
#include "libtime_utils.h"

#ifdef RDMA_LL
 #include "liblog.h"
#endif

#ifdef DMACHAN_TICKETED
 #include "dmashmpdata.h"
#endif

#ifndef __DMACHAN_H__
#define __DMACHAN_H__

/* DMA Status FIFO */
#define DMA_STATUS_FIFO_LENGTH (4096)
#define DMA_RUNPOLL_US 10

#if defined(REGDEBUG) && defined(RDMA_LL)
  #define REGDBG(format, ...) DBG(format, ## __VA_ARGS__)
#else
  #define REGDBG(format, ...) if (0) fprintf(stderr, format, ## __VA_ARGS__)
#endif

void hexdump4byte(const char* msg, uint8_t* d, int len);

class DMAChannel 
#ifdef DMACHAN_TICKETED
  : public DMAShmPendingData
#endif
{
public:
  static const int DMA_BUFF_DESCR_SIZE = 32;

  enum {
    SIM_INJECT_TIMEOUT = 1,
    SIM_INJECT_INP_ERR = 2,
    SIM_INJECT_OUT_ERR = 4
  };

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
#ifdef DMACHAN_TICKETED
      uint64_t ts_start;
      uint64_t ticket; ///< ticket issued at enq time
      uint64_t not_before; ///< earliest rdtsc ts when ticket can be checked
      uint64_t not_before_dns; ///< delta nanoseconds wait
#endif
      uint64_t u_data; ///< whatever the user puts in here
  } DmaOptions_t;

  static const uint32_t WI_SIG = 0xb00fd00fL;
  typedef struct {
    volatile uint32_t  valid;
    DmaOptions_t       opt;
    RioMport::DmaMem_t mem;
    uint64_t           ts_start, ts_end; ///< rdtsc timestamps for enq and popping up in FIFO
    uint32_t           bd_wp; ///< Soft WP at the moment of enqueuing this BD [DOCUMENTATION]
    uint32_t           bd_idx; ///< index into buffer ring of buffer used to handle this op BD [DOCUMENTATION]
    uint32_t           bl_busy_size; ///< How big was the tx q when this was enq'd [DOCUMENTATION]
    // add actions here
    uint8_t  t2_rddata[16]; // DTYPE2 NREAD incoming data
    uint32_t t2_rddata_len;
  } WorkItem_t;

  static const uint32_t COMPL_SIG = 0xf00fb00fL;
  typedef struct {
    uint32_t valid;
    uint64_t win_handle;
    uint32_t fifo_offset;
    uint64_t ts_end;
  } DmaCompl_t;

  DMAChannel(const uint32_t mportid, const uint32_t chan);
  DMAChannel(const uint32_t mportid, const uint32_t chan, riomp_mport_t mp_h);
  ~DMAChannel();

  inline int getChannel() { return m_chan; }

  inline void setCheckHwReg(bool UNUSED_PARM(b)) { m_check_reg = true; }

  void resetHw();
  void setInbound();
  bool dmaIsRunning();
  uint32_t clearIntBits();

  inline bool isMaster() { return true; }

  uint32_t getDestId() { return m_mport->rd32(TSI721_IB_DEVID); }

  static const char* abortReasonToStr(const uint32_t abort_reason);

  bool alloc_dmatxdesc(const uint32_t bd_num);
  void free_dmatxdesc();
  bool alloc_dmacompldesc(const uint32_t bd_num);
  void free_dmacompldesc();

  bool alloc_dmamem(const uint32_t size, RioMport::DmaMem_t& mem);
  bool free_dmamem(RioMport::DmaMem_t& mem);

  bool check_ibwin_reg() { return m_mport->check_ibwin_reg(); }

  inline bool queueDmaOpT1(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem, uint32_t& abort_reason, struct seq_ts *ts_p)
  {
    opt.dtype = DTYPE1;
    return queueDmaOpT12(rtype, opt, mem, abort_reason, ts_p);
  }
  inline bool queueDmaOpT2(int rtype, DmaOptions_t& opt, uint8_t* data, const int data_len, uint32_t& abort_reason, struct seq_ts *ts_p)
  {
    if(rtype != NREAD && (data == NULL || data_len < 1 || data_len > 16))
	return false;
  
    RioMport::DmaMem_t lmem; memset(&lmem, 0, sizeof(lmem));
  
    lmem.win_ptr  = data;
    lmem.win_size = data_len;

    opt.dtype = DTYPE2;
    return queueDmaOpT12(rtype, opt, lmem, abort_reason, ts_p);
  }

  inline uint32_t queueSize()
  {
    return m_bl_busy_size;
  }

  void cleanup();
  void shutdown();
  void init();

  int scanFIFO(WorkItem_t* completed_work, const int max_work);

public: // XXX test-public, make this section private

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

public:
  inline uint32_t getReadCount()      { return rd32dmachan(TSI721_DMAC_DRDCNT); }
  inline uint32_t getWriteCount()     { return rd32dmachan(TSI721_DMAC_DWRCNT); }
  inline uint32_t getFIFOReadCount()  { return rd32dmachan(TSI721_DMAC_DSRP); }
  inline uint32_t getFIFOWriteCount() { return rd32dmachan(TSI721_DMAC_DSWP); }
  
  /** \brief Checks whether HW bd ring is empty
   * \note This reads 2 PCIe register so it is slow
   */
  inline bool queueEmptyHw()
  {
     return getWriteCount() == getReadCount();
  }
  
  /** \brief Checks whether there's more space in HW bd ring
   * \note This reads 2 PCIe register so it is slow
   */
  inline bool queueFullHw()
  {
    uint32_t wrc = getWriteCount();
    uint32_t rdc = getReadCount();
    if(wrc == rdc) return false; // empty
  
    // XXX unit-test logic
    if(rdc > 0  && wrc == (rdc-1))      return true;
    if(rdc == 0 && wrc == (m_bd_num-1)) return true;
    return false;
  }
  
  inline bool queueFull()
  {
    // XXX we have control over the soft m_dma_wr but how to divine the read pointer?
    //     that should come from the completion FIFO but for now we brute-force it!
  
    //return SZ == (m_bd_num+1); // account for T3 BD as well
    return ((uint32_t)(m_bl_busy_size + 2) >= m_bd_num); // account for T3 BD as well
  }
  
  inline bool dmaCheckAbort(uint32_t& abort_reason)
  {
    uint32_t channel_status = rd32dmachan(TSI721_DMAC_STS);
  
    if(channel_status & TSI721_DMAC_STS_RUN) return false;
    if((channel_status & TSI721_DMAC_STS_ABORT) != TSI721_DMAC_STS_ABORT) return false;
  
    abort_reason = (channel_status >> 16) & 0x1F;
  
    return true;
  }
  
  inline bool checkPortOK()
  {
    uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
    return status & TSI721_RIO_SP_ERR_STAT_PORT_OK;
  }
  
  inline bool checkPortError()
  {
    uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
    return status & TSI721_RIO_SP_ERR_STAT_PORT_ERR;
  }
  
  inline void checkPortInOutError(bool& inp_err, bool& outp_err)
  {
    uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
  
    if(status & TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP) inp_err = true;
    else inp_err = false;
  
    if(status & TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP) outp_err = true;
    else outp_err = false;
  }

  void softRestart(const bool nuke_bds = true);

  volatile uint64_t   m_fifo_scan_cnt;
  volatile uint64_t   m_tx_cnt; ///< Number of DMA ops that succeeded / showed up in FIFO

  /** \brief Returns the number of BDs submitted to DMA engine */
  inline uint32_t getWP() { return m_dma_wr; }

private:
  int umdemo_must_die;
  volatile bool       m_check_reg;
  pthread_spinlock_t  m_hw_splock; ///< Serialize access to DMA chan registers
  pthread_spinlock_t  m_pending_work_splock; ///< Serialize access to DMA pending queue object
  RioMport*           m_mport;
  uint32_t            m_chan;
  uint32_t            m_bd_num;
  uint32_t            m_sts_size;
  RioMport::DmaMem_t  m_dmadesc;
  RioMport::DmaMem_t  m_dmacompl;
  volatile uint32_t   m_dma_wr;      ///< Mirror of Tsi721 write pointer
  int32_t             m_fifo_rd;
  bool*               m_bl_busy;
  volatile int32_t    m_bl_busy_size;
  pthread_spinlock_t  m_bl_splock; ///< Serialize access to BD list
  uint64_t            m_T3_bd_hw;
  volatile int        m_restart_pending;
  uint32_t            m_sts_log_two; ///< Remember the calculation in alloc_dmacompldesc and re-use it at softReset
  
  WorkItem_t*         m_pending_work;

  bool queueDmaOpT12(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem, uint32_t& abort_reason, struct seq_ts *ts_p);

  inline void setWriteCount(uint32_t cnt) { wr32dmachan(TSI721_DMAC_DWRCNT, cnt); }

public:
  volatile uint64_t*  m_bl_busy_histo;

public:
  inline void trace_dmachan(uint32_t offset, uint32_t val)
  {
	wr32dmachan_nolock(offset, val);
  };

#ifdef DMACHAN_TICKETED
private:
  // EVIL PLAN: Keep WP, RP as 64-bit and use them modulo DMA_SHM_MAX_ITEMS
  static const int DMA_SHM_MAX_ITEMS = 1024;

  typedef struct { ///< All per-client bad transactions reported here
    volatile uint64_t WP;
    volatile uint64_t RP;
    uint64_t tickets[DMA_SHM_MAX_ITEMS];

    inline uint64_t queueSize() {
      assert(RP <= WP);
      return WP-RP;
    }

    // Call following in splocked context!
    inline bool enq(const uint64_t tik) {
      if ((WP-RP) >= DMA_SHM_MAX_ITEMS) return false; // FULL
      tickets[(WP++ % DMA_SHM_MAX_ITEMS)] = tik;
      return true;
    }
    inline bool deq(uint64_t& tik) {
      assert(RP <= WP);
      if (WP == RP) return false; // empty
      tik = tickets[(RP++ % DMA_SHM_MAX_ITEMS)];
      return true;
    }
  } Faulted_Ticket_t;

  uint64_t            MHz;

  Faulted_Ticket_t    m_bad_tik;
  uint64_t*           m_pending_tickets;
  uint64_t            m_pending_tickets_RP;
  volatile uint64_t   m_serial_number;
  volatile uint64_t   m_acked_serial_number; ///< Arriere-garde of completed tickets
  pthread_spinlock_t  m_fault_splock;

  inline void initTicketed()
  {
    MHz = getCPUMHz();

    memset(&m_bad_tik, 0, sizeof(m_bad_tik));
    m_pending_tickets    = NULL;
    m_pending_tickets_RP = 0;
    m_serial_number      = 0;
    m_acked_serial_number= 0;

    pthread_spin_init(&m_fault_splock, PTHREAD_PROCESS_PRIVATE);
  }

  inline void computeNotBefore(DmaOptions_t& opt)
  {
    uint64_t ns = 0;

    if (m_pendingdata_tally != NULL) {
      uint64_t max_data = m_pendingdata_tally->data[m_chan];
      for(int i = 1 /*Kern uses 0 for maint*/; i < DMA_MAX_CHAN; i++) {
        if (m_pendingdata_tally->data[i] < max_data)
                ns += m_pendingdata_tally->data[i];
        else
                ns += max_data;
      }
      ns = ns/2;
    } else { // Fall back to information at hand
      switch(opt.rtype) {
        case NREAD:         ns = opt.bcount; break;
        case LAST_NWRITE_R: ns = opt.bcount/2; break;
        case ALL_NWRITE:    ns = opt.bcount/2; break;
        case ALL_NWRITE_R:  ns = opt.bcount; break;
        case MAINT_RD:
        case MAINT_WR:
             throw std::runtime_error("DMAChannel: Maint operations not supported!");
             break;
        default: assert(0); break;
      }
    }

    // Eq: (rdtsc * 1000) / MHz = nsec <=> rdtsc = (nsec * MHz) / 1000
    opt.not_before_dns = ns;
    //opt.not_before     = opt.ts_start + (ns * 1000 / MHz); // convert to microseconds, then to rdtsc units
    opt.not_before     = opt.ts_start + (ns * MHz / 1000); // convert to microseconds, then to rdtsc units
  }

public:
  typedef enum {
    BORKED     = -1,
    INPROGRESS = 1,
    COMPLETED  = 2
  } TicketState_t;

  /** \brief Dequeue 1st available faulted ticket
   * \return true if something was dequeued
   */
  inline bool dequeueFaultedTicket(uint64_t& res)
  {
    pthread_spin_lock(&m_fault_splock);
    const bool r = m_bad_tik.deq(res);
    pthread_spin_unlock(&m_fault_splock);

    return r;
  }

  /** \brief Check whether the transaction associated with this ticket has completed
   * \note It could be completed or in error, true is returned anyways
   */
  inline TicketState_t checkTicket(const DmaOptions_t& opt)
  {
    if (opt.ticket == 0 || opt.ticket > m_serial_number)
      throw std::runtime_error("DMAChannel: Invalid ticket!");

    if (rdtsc() < opt.not_before) return INPROGRESS;

    bool found_bad = false;

    if (m_bad_tik.queueSize() > 0) {
      pthread_spin_lock(&m_fault_splock);
      for (uint64_t idx = m_bad_tik.RP; idx < m_bad_tik.WP; idx++) {
        if (opt.ticket == m_bad_tik.tickets[idx % DMA_SHM_MAX_ITEMS]) {
          found_bad = true;
          break;
        }
      }
      pthread_spin_unlock(&m_fault_splock);
    }

    if (found_bad) return BORKED;

    if (m_acked_serial_number >= opt.ticket) return COMPLETED;

    // Should never get here
    return INPROGRESS;
  }

  inline uint64_t getAckedSN() { return m_acked_serial_number; }

  /** \brief Tally up all pending data across all channels managed
   * \note Kernel may have in-flight data fighting for the same bandwidth. We cannot account for that.
   */
  inline void getShmPendingData(uint64_t& total, DmaShmPendingData_t& per_client)
  {
    if (m_pendingdata_tally == NULL) { total = 0; return; }

    memcpy(&per_client, m_pendingdata_tally, sizeof(DmaShmPendingData_t));

    uint64_t max_mem = per_client.data[m_chan];

    total = 0;
    for(int i = 0; i < DMA_MAX_CHAN; i++)
      total += (per_client.data[i] < max_mem)?per_client.data[i]:max_mem;
  }
#endif // DMACHAN_TICKETED
};

#endif /* __DMACHAN_H__ */
