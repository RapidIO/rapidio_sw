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

#include "IDT_Tsi721.h"

#include "mport.h"
#include "pshm.h"
#include "dmadesc.h"
#include "rdtsc.h"
#include "debug.h"
#include "time_utils.h"

#ifndef __DMACHAN_H__
#define __DMACHAN_H__

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

  static const int DMA_SHM_MAX_CLIENTS = 64;
  static const int DMA_SHM_MAX_ITEMS_PER_CLIENT = 1024;

  enum {
    SIM_INJECT_TIMEOUT = 1,
    SIM_INJECT_ERR_RSP = 2,
    SIM_INJECT_INP_ERR = 4,
    SIM_INJECT_OUT_ERR = 8
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
      uint64_t win_handle; ///< populated when queueing for TX
      uint32_t bd_wp; ///< soft WP at the moment of enqueuing this
      uint32_t bd_idx; ///< index into buffer ring of buffer used to handle this op
      uint64_t ts_start, ts_end; ///< rdtsc timestamps for enq and popping up in FIFO
      uint64_t u_data; ///< whatever the user puts in here
      uint64_t ticket; ///< ticket issued at enq time
      uint64_t not_before; ///< earliest rdtsc ts when ticket can be checked
      pid_t    pid; ///< process id of enqueueing process
      pid_t    tid; ///< thread id of enqueueing thread; this is NOT a pthread id, it is issued by gettid(2)
      int      cliidx;
  } DmaOptions_t;

  static const uint32_t WI_SIG = 0xb00fd00fL;
  typedef struct {
    uint32_t valid;
    DmaOptions_t       opt;
    RioMport::DmaMem_t mem;
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

  inline void setCheckHwReg(bool b) { m_check_reg = true; }

  void resetHw();
  void setInbound();
  bool dmaIsRunning();
  uint32_t clearIntBits();

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
    return m_state->bl_busy_size;
  }

  void shutdown();
  void open_txdesc_shm(const uint32_t mportid, const uint32_t chan);
  void init(const uint32_t chan);

  int scanFIFO(WorkItem_t* completed_work, const int max_work, const int force_scan = 0);

  /** \brief Switch to simulation mode. \ref simFIFO must be called frequently
   * \note Call this before \ref alloc_dmatxdesc
   * \note Call this before \ref alloc_dmacompldesc
   * \note Object cannot be switched out of simulation mode.
   */
  void setSim() { m_check_reg = false; m_sim = true; }
  inline bool isSim() { return m_sim; }
  int simFIFO(const int max_bd, const uint32_t fault_bmask);

public: // XXX test-public, make this section private

  // NOTE: These functions can be inlined only if they live in a
  //       header file
  #define wr32dmachan(o, d) _wr32dmachan((o), #o, (d), #d)
  inline void _wr32dmachan(uint32_t offset, const char* offset_str,
                          uint32_t data, const char* data_str)
  {
    REGDBG("\n\tW chan=%d offset %s (0x%x) :=  %s (0x%x)\n",
        m_state->chan, offset_str, offset, data_str, data);
    pthread_spin_lock(&m_state->hw_splock);
    m_mport->__wr32dma(m_state->chan, offset, data);
    pthread_spin_unlock(&m_state->hw_splock);
  }

  #define rd32dmachan(o) _rd32dmachan((o), #o)
  inline uint32_t _rd32dmachan(uint32_t offset, const char* offset_str)
  {
    pthread_spin_lock(&m_state->hw_splock);
    uint32_t ret = m_mport->__rd32dma(m_state->chan, offset);
    pthread_spin_unlock(&m_state->hw_splock);
    REGDBG("\n\tR chan=%d offset %s (0x%x) => 0x%x\n", m_state->chan, offset_str, offset, ret);
    return ret;
  }

  inline void wr32dmachan_nolock(uint32_t offset, uint32_t data)
  {
    m_mport->__wr32dma(m_state->chan, offset, data);
  }
  inline uint32_t rd32dmachan_nolock(uint32_t offset)
  {
    return m_mport->__rd32dma(m_state->chan, offset);
  }

public:
  inline uint32_t getReadCount()      { return rd32dmachan(TSI721_DMAC_DRDCNT); }
  inline uint32_t getWriteCount()     { return rd32dmachan(TSI721_DMAC_DWRCNT); }
  inline uint32_t getFIFOReadCount()  { return rd32dmachan(TSI721_DMAC_DSRP); }
  inline uint32_t getFIFOWriteCount() { return rd32dmachan(TSI721_DMAC_DSWP); }
  
  /** \brief Checks whether HW bd ring is empty
   * \note This reads 2 PCIe registers so it is slow
   */
  inline bool queueEmptyHw()
  {
     return getWriteCount() == getReadCount();
  }
  
  /** \brief Checks whether there's more space in HW bd ring
   * \note This reads 2 PCIe registers so it is slow
   */
  inline bool queueFullHw()
  {
    if (m_sim) return false;

    uint32_t wrc = getWriteCount();
    uint32_t rdc = getReadCount();
    if(wrc == rdc) return false; // empty
  
    // XXX unit-test logic
    if(rdc > 0  && wrc == (rdc-1))      return true;
    if(rdc == 0 && wrc == (m_state->bd_num-1)) return true;
    return false;
  }
  
  inline bool queueFull()
  {
    // XXX we have control over the soft m_state->dma_wr but how to divine the read pointer?
    //     that should come from the completion FIFO but for now we brute-force it!
  
    //return SZ == (m_state->bd_num+1); // account for T3 BD as well
    return (m_state->bl_busy_size + 2 + 1 /*BD0 is T3*/ >= m_state->bd_num); // account for T3 BD as well
  }
  
  inline bool dmaCheckAbort(uint32_t& abort_reason)
  {
    if (m_sim) {
      if (m_sim_abort_reason != 0) { abort_reason = m_sim_abort_reason; return true; }
      return false;
    }

    uint32_t channel_status = rd32dmachan(TSI721_DMAC_STS);
  
    if(channel_status & TSI721_DMAC_STS_RUN) return false;
    if((channel_status & TSI721_DMAC_STS_ABORT) != TSI721_DMAC_STS_ABORT) return false;
  
    abort_reason = (channel_status >> 16) & 0x1F;
  
    return true;
  }
  
  inline bool checkPortOK()
  {
    if (m_sim) return (m_sim_err_stat == 0);

    uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
    return status & TSI721_RIO_SP_ERR_STAT_PORT_OK;
  }
  
  inline bool checkPortError()
  {
    if (m_sim) return !!m_sim_err_stat;

    uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
    return status & TSI721_RIO_SP_ERR_STAT_PORT_ERR;
  }
  
  inline void checkPortInOutError(bool& inp_err, bool& outp_err)
  {
    if (m_sim) {
      inp_err  = !! (m_sim_err_stat & TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP);
      outp_err = !! (m_sim_err_stat & TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP);
      return;
    }

    uint32_t status = m_mport->rd32(TSI721_RIO_SP_ERR_STAT);
  
    if(status & TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP) inp_err = true;
    else inp_err = false;
  
    if(status & TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP) outp_err = true;
    else outp_err = false;
  }

  void softRestart(const bool nuke_bds = true);
  int cleanupBDQueue(bool multithreaded_fifo);

  volatile uint64_t   m_fifo_scan_cnt;
  volatile uint64_t   m_tx_cnt; ///< Number of DMA ops that succeeded / showed up in FIFO

  /** \brief Returns the number of BDs submitted to DMA engine */
  inline uint32_t getWP() { return m_state->dma_wr; }

private:
  int umdemo_must_die = 0;
  pid_t               m_pid;
  pid_t               m_tid;
  int                 m_cliidx; ///< Index into m_state->client_completion if running as client 
  bool                m_sim;        ///< Simulation: do not progtam HW with linear addrs of FIFO and BD array; do not read HW regs
  uint32_t            m_sim_dma_rp; ///< Simulated Tsi721 RP
  uint32_t            m_sim_fifo_wp; ///< Simulated Tsi721 FIFO WP
  volatile uint32_t   m_sim_abort_reason; ///< Simulated abort error, cleared on reset
  volatile uint32_t   m_sim_err_stat; ///< Simulated port error, cleared on reset
  volatile bool       m_check_reg;
  RioMport*           m_mport;
  int                 m_mportid;
  riomp_mport_t       m_mp_hd;
  RioMport::DmaMem_t  m_dmadesc; ///< Populated from m_state->dmadesc_win_*
  RioMport::DmaMem_t  m_dmacompl;
 
  POSIXShm*           m_shm_state;
  char                m_shm_state_name[129] = {0};

  POSIXShm*           m_shm_bl;
  char                m_shm_bl_name[129] = {0};

  // These two live in m_shm_bl back-to-back
  bool*               m_bl_busy;
  WorkItem_t*         m_pending_work;

  bool                m_hw_master;

  typedef struct {
    uint64_t ticket;
    uint16_t bcount;
    uint8_t  data[16];
  } NREAD_Result_t;

public:
  typedef struct {
    bool     busy; ///< Did any client claim this?
    pid_t    owner_pid;
    uint64_t change_cnt; ///< Master bumps this when adds stuff

    struct { ///< All per-client bad transactions reported here
      uint32_t WP;
      uint32_t RP;
      uint64_t tickets[DMA_SHM_MAX_ITEMS_PER_CLIENT];
    } bad_tik;

    struct {
      uint32_t WP;
      uint32_t RP;
      NREAD_Result_t results[DMA_SHM_MAX_ITEMS_PER_CLIENT];
    } NREAD_T2_results;
  } ShmClientCompl_t;

  typedef struct {
    pid_t               master_pid;
    volatile int        restart_pending;
    uint64_t            dmadesc_win_handle; ///< Sharable among processes, mmap'able
    uint64_t            dmadesc_win_size;
    uint32_t            sts_log_two; ///< Remember the calculation in alloc_dmacompldesc and re-use it at softReset
    pthread_spinlock_t  hw_splock; ///< Serialize access to DMA chan registers
    pthread_spinlock_t  pending_work_splock; ///< Serialize access to DMA pending queue object
    uint32_t            chan;
    uint32_t            bd_num;
    uint32_t            sts_size;
    volatile uint32_t   dma_wr;      ///< Mirror of Tsi721 write pointer
    int32_t             fifo_rd;
    volatile int32_t    bl_busy_size;
    pthread_spinlock_t  bl_splock; ///< Serialize access to BD list
    uint64_t            T3_bd_hw;
    volatile int        hw_ready; ///< Set to 2 only in Master when BD and FIFO CMA mem allocated & hw programmed
    struct hw_dma_desc  BD0_T3_saved; ///< Pack this once, save, reuse when needed

    volatile uint64_t   serial_number;
    volatile uint64_t   acked_serial_number; ///< Arriere-garde of completed tickets

    pthread_spinlock_t  client_splock; ///< Serialize access to clients' data structures.
    ShmClientCompl_t    client_completion[DMA_SHM_MAX_CLIENTS];
  } DmaChannelState_t;

private:
  DmaChannelState_t*  m_state;

  void cleanup();

  bool queueDmaOpT12(int rtype, DmaOptions_t& opt, RioMport::DmaMem_t& mem, uint32_t& abort_reason, struct seq_ts *ts_p);

public:
  typedef enum {
    BORKED = -1,
    INPROGRESS = 1,
    COMPLETED  = 2
  } TicketState_t;

  TicketState_t checkTicket(const DmaOptions_t& opt);
 
  /** \brief Crude Seventh Edition-style check for SHM Master liveliness */
  inline bool pingMaster() {
    assert(m_state);
    if (m_hw_master) return true; // No-op
    return (kill(m_state->master_pid, 0) == 0);
  }

  /** \brief Brutal way of cleaning up dead clients. Locks out ALL clients during the proceedings
   * \note This takes a trip into the kernel for each client. VERY SLOW.
   */
  inline void cleanupDeadClients() {
    assert(m_state);

    pthread_spin_lock(&m_state->client_splock);
    for (int i = 0; i < DMA_SHM_MAX_CLIENTS; i++) {
      if (!m_state->client_completion[i].busy) continue;
      if (kill(m_state->client_completion[i].owner_pid, 0) == 0) continue;

      // Cleanup
      m_state->client_completion[i].owner_pid = -1;
      m_state->client_completion[i].busy      = false;
    }
    pthread_spin_unlock(&m_state->client_splock);
  }

  /** \brief List clients. Locks out ALL clients shortly during the proceedings */
  inline void listClients(ShmClientCompl_t* client_compl, const int client_compl_size) {
    if (client_compl_size < sizeof(m_state->client_completion)) return;

    assert(m_state);

    pthread_spin_lock(&m_state->client_splock);
    memcpy(client_compl, m_state->client_completion, sizeof(m_state->client_completion));
    pthread_spin_unlock(&m_state->client_splock);
  }

  inline void setWriteCount(uint32_t cnt) { if (!m_sim) wr32dmachan(TSI721_DMAC_DWRCNT, cnt); }

public:
  void dumpBDs(std::string& s);

  inline void trace_dmachan(uint32_t offset, uint32_t val)
  {
	wr32dmachan_nolock(offset, val);
  };
};

#endif /* __DMACHAN_H__ */
