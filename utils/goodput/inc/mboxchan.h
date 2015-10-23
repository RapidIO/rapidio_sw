/*****************************************************************************
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

#ifndef __MBOXCHAN_H__
#define __MBOXCHAN_H__

#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <map>
#include <vector>
#include <stdexcept>

#include "IDT_Tsi721.h"
#include "tsi721_msg.h"

#include "mport.h"
#include "dmadesc.h"
#include "rdtsc.h"
#include "debug.h"

#if defined(DEBUG) && DEBUG > 1
  #define DDBG(format, ...) DBG(stdout, format, __VA_ARGS__)
#else
  #define DDBG(format, ...)
#endif

#if defined(DEBUG) && DEBUG > 2
  #define DDDBG(format, ...) DBG(stdout, format, __VA_ARGS__)
#else
  #define DDDBG(format, ...)
#endif

struct hw_omsg_desc_ {
  uint32_t type_id;
  uint32_t msg_info;
  union {
    uint32_t bufptr_lo;  /* if DTYPE == 4 */
    uint32_t next_lo;    /* if DTYPE == 5 */
  };
  union {
    uint32_t bufptr_hi;  /* if DTYPE == 4 */
    uint32_t next_hi;    /* if DTYPE == 5 */
  };
} __attribute__ ((aligned(16)));
typedef struct hw_omsg_desc_ hw_omsg_desc;

struct hw_imsg_desc_ {
  uint32_t type_id;
  uint32_t msg_info;
  uint32_t bufptr_lo;
  uint32_t bufptr_hi;
  uint32_t reserved[12];
} __attribute__ ((aligned(64)));
typedef struct hw_imsg_desc_ hw_imsg_desc;

typedef struct {
  bool valid;
  uint64_t enq_ts;
} imq_ts_t;

typedef struct {
  uint32_t size;
  /* VA/PA of data buffers for incoming messages */
  RioMport::DmaMem_t buf;

  /* VA/PA of circular free buffer list */
  RioMport::DmaMem_t imfq;

  /* VA/PA of Inbound message descriptors */
  RioMport::DmaMem_t imd;

  std::vector<void*>    imq_base; ///< Inbound Queue buffer pointers provided by user
  std::vector<imq_ts_t> imq_ts; ///< rdtsc timestamp of the moment a BD was placed on the free list

  uint32_t rx_slot;
  uint32_t fq_wrptr;
  uint32_t desc_rdptr;
} hw_imsg_ring;

typedef struct {
  uint32_t size;
  /* VA/PA of OB Msg descriptors */
  RioMport::DmaMem_t omd;

  /* VA/PA of OB Msg data buffers */
  std::vector<RioMport::DmaMem_t> omq;

  /* VA/PA of OB Msg descriptor status FIFO */
  RioMport::DmaMem_t sts;
  uint32_t           sts_size;    /* # of allocated status entries */
  uint32_t           sts_rdptr;

  uint32_t           tx_slot;
  uint32_t           wr_count;
  volatile uint32_t  rd_count_soft;
} hw_omsg_ring;

class MboxChannel {
public:
  static const int RIO_MAX_MBOX = 4;

  static const int MBOX_OB_BUFF_DESCR_SIZE = 16;
  static const int MBOX_IB_BUFF_DESCR_SIZE = 64;

  typedef struct {
      uint8_t dtype;
      uint8_t mbox;
      uint8_t letter;
      uint16_t destid;
      bool iof;
      bool crf;
      bool tt_16b; // set to 1 if destid is 16 bytes
      uint8_t prio:2;
      uint32_t bd_wp; ///< Soft WP at the moment of enqueuing this
      uint32_t bd_idx; ///< index into buffer ring of buffer used to handle this op
      uint64_t ts_start, ts_end; ///< rdtsc timestamps for enq and popping up in FIFO
  } MboxOptions_t;

  typedef struct {
    uint32_t       valid;
    MboxOptions_t  opt;
    // add actions here
  } WorkItem_t;

  MboxChannel(const uint32_t mportid, const uint32_t chan);
  MboxChannel(const uint32_t mportid, const uint32_t chan, riomp_mport_t mp_h);

  ~MboxChannel() { cleanup(); delete m_mport; };

  void setInitState();
  bool open_mbox(const uint32_t entries, const uint32_t sts_entries);

  bool send_message(MboxOptions_t& opt, const void* data, const size_t len, bool& q_was_full);

  int add_inb_buffer(const int mbox, void* buf);
  bool inb_message_ready(const int ib_mbox, uint64_t& rx_ts);
  void* get_inb_message(const int ib_mbox, int& msg_size, uint64_t& enq_ts);

  void set_rx_destid(const uint16_t destid)
  {
    m_mport->wr32(TSI721_IB_DEVID, destid);
    DBG("\n\tSet own destid := %d; HW destid = %d\n", destid, m_mport->rd32(TSI721_IB_DEVID ));
  }

  inline int getDeviceId() { return m_mport->getDeviceId(); }
  inline uint32_t getDestId() { return m_mport->rd32(TSI721_IB_DEVID); }

  int scanFIFO(const int mbox, WorkItem_t* completed_work, const int max_work);

  inline int getTxReadCountSoft(const int mbox)
  {
    const int mboxmsk = 1<<mbox;
    if(! (m_mboxen & mboxmsk)) throw std::runtime_error("getReadCountSoft: Mailbox not opened!");

    return m_omsg_ring[mbox].rd_count_soft;
  }
  inline bool queueTxFull(const int mbox)
  {
    const int mboxmsk = 1<<mbox;
    if(! (m_mboxen & mboxmsk)) throw std::runtime_error("queueTxFull: Mailbox not opened!");

    return m_omsg_trk[mbox].bltx_busy_size >= (m_num_ob_desc[mbox] - 1); // Ignore D5 as it pops up very fast
  }

  inline int queueTxSize(const int mbox)
  {
    const int mboxmsk = 1<<mbox;
    if(! (m_mboxen & mboxmsk)) throw std::runtime_error("queueTxFull: Mailbox not opened!");

    return m_omsg_trk[mbox].bltx_busy_size;
  }

public: // test-public
  #define wr32mboxchan(o, d) _wr32mboxchan((o), #o, (d), #d)
  void _wr32mboxchan(uint32_t offset, const char* offset_str, uint32_t data, const char* data_str)
  {
    DDBG("\n\twr32mboxchan offset %s (0x%x) :=  %s (0x%x)\n", offset_str, offset, data_str, data);
    pthread_spin_lock(&m_hw_splock);
    m_mport->__wr32(offset, data);
    pthread_spin_unlock(&m_hw_splock);
  }
  #define rd32mboxchan(o) _rd32mboxchan((o), #o)
  uint32_t _rd32mboxchan(uint32_t offset, const char* offset_str)
  {
    pthread_spin_lock(&m_hw_splock);
    uint32_t ret = m_mport->__rd32(offset);
    pthread_spin_unlock(&m_hw_splock);
    DDDBG("\n\trd32mboxchan offset %s (0x%x) => 0x%x\n", offset_str, offset, ret);
    return ret;
  }
  void wr32mboxchan_nolock(uint32_t offset, uint32_t data)
  {
    m_mport->__wr32(offset, data);
  }
  uint32_t rd32mboxchan_nolock(uint32_t offset)
  {
    return m_mport->__rd32(offset);
  }

private:
  void init();
  int open_outb_mbox(int mbox, uint32_t entries, const uint32_t sts_entries);
  int open_inb_mbox(int mbox, uint32_t entries);
  void cleanup();
  void dumpBL(const int mbox);

private:
  RioMport*           m_mport;
  uint8_t             m_mboxen; // a bitmask
  pthread_spinlock_t  m_hw_splock; ///< Serialize access to MBOX chan registers
  pthread_spinlock_t  m_rx_splock[RIO_MAX_MBOX]; ///< Serialize access to MBOX RX data structures
  pthread_spinlock_t  m_tx_splock[RIO_MAX_MBOX]; ///< Serialize access to MBOX TX data structures
  pthread_spinlock_t  m_bltx_splock[RIO_MAX_MBOX]; ///< Serialize access to MBOX TX busy BD data structure
  uint32_t            m_num_ob_desc[RIO_MAX_MBOX];
  hw_imsg_ring        m_imsg_ring[RIO_MAX_MBOX];
  hw_omsg_ring        m_omsg_ring[RIO_MAX_MBOX];
  bool                m_imsg_init[RIO_MAX_MBOX];
  bool                m_omsg_init[RIO_MAX_MBOX];

  static const uint32_t WI_SIG = 0xb00fd00fL;

  // Cannot store this in struct hw_omsg_ring as m_omsg_init
  // gets memset to 0 and that screws up the std:: objects
  struct {
    WorkItem_t*  bltx_busy; 
    volatile int bltx_busy_size;
    int*         bl_busy;
  } m_omsg_trk[RIO_MAX_MBOX];
};

#endif // __MBOXCHAN_H__
