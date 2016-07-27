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

#ifndef __MBOXMGR_H__
#define __MBOXMGR_H__

#include <stdexcept>

#include "mboxchan.h"
#include "chanlock.h"


/**
 * \verbatim
In /sys/module/tsi721_mport/parameters/

- 'MBOX_sel' - DMA channel selection mask. Bitmask that defines which hardware
        DMA channels (0 ... 6) will be registered with DmaEngine core.
        If bit is set to 1, the corresponding DMA channel will be registered.
        DMA channels not selected by this mask will not be used by this device
        driver. Default value is 0x7f (use all channels).


- 'mbox_sel' - RIO messaging MBOX selection mask. This is a bitmask that defines
        messaging MBOXes are managed by this device driver. Mask bits 0 - 3
        correspond to MBOX0 - MBOX3. MBOX is under driver's control if the
        corresponding bit is set to '1'. Default value is 0x0f (= all).
\endverbatim
 */

/** \brief Tsi721 MBOX channel availability bitmask vs kernel driver.
 * \note The kernel uses channels 0 & 1. ALWAYS
 * \note The kernel SHOULD publish this [exclusion bitmask] using a sysfs interface.
 * \note WARNING Absent sysfs exclusion bitmask we can ony make educated guesses about what DMA channel the kernel is NOT using.
 */
#define MBOX_CHAN_MASK_DEFAULT 0x0C

static inline uint8_t MboxChanMask()
{
  char buf[257] = {0};
  uint32_t ret = MBOX_CHAN_MASK_DEFAULT;

  FILE* fp = fopen("/sys/module/tsi721_mport/parameters/mbox_sel", "r");
  if (fp == NULL)
	goto done;
  if (NULL == fgets(buf, 256, fp))
	goto done;
  fclose(fp);

  if (buf[0] != '\0') {
    if (sscanf(buf, "%x", &ret) != 1) goto done;
    ret &= 0xFF;
    ret = ~ret;
  }

done:
  return (ret & 0xFF);
}

static inline bool check_chan_mask_mbox(int chan)
{
  const int chanb = 1 << chan;
  return (chanb & MboxChanMask()) == chanb;
}

class MboxChannelMgr {
private:
  MboxChannel* m_super;
  LockChannel* m_lock;
  uint32_t     m_mportid;
  uint32_t     m_mbox;


  /** \brief Lock other processes out of this UMD MBOX mport:channel
   * \note Due to POSIX locking semantics this has no effect on the current process
   * \note Using the same channel twice in this process will NOT be prevented
   */
  inline void init()
  {
    try {
      m_lock = ChannelLock::TakeLock("MBOX", m_mportid, m_mbox);
    } catch(std::runtime_error& ex) {
      delete m_super;
      CRIT("\n\tTaking lock %s failed: %s\n", ex.what());
      throw ex;
    }
  }

public:
  enum { PAGE_4K = 4096 };

  MboxChannelMgr(const uint32_t mportid, const uint32_t mbox) :
    m_mportid(mportid), m_mbox(mbox)
  {
    if (!check_chan_mask_mbox(mbox))
      throw std::runtime_error("MboxChannelMgr: Invalid mailbox!");
    m_super = new MboxChannel(mportid, mbox);
    init();
  }
  MboxChannelMgr(const uint32_t mportid, const uint32_t mbox, riomp_mport_t mp_h) :
    m_mportid(mportid), m_mbox(mbox)
  {
    if (!check_chan_mask_mbox(mbox))
      throw std::runtime_error("MboxChannelMgr: Invalid mailbox!");
    m_super = new MboxChannel(mportid, mbox, mp_h);
    init();
  }
  ~MboxChannelMgr() { delete m_super; delete m_lock; }

  inline int getDeviceId() { return m_super->getDeviceId(); }
  inline uint32_t getDestId() { return m_super->getDestId(); }

  inline bool open_mbox(const uint32_t entries, const uint32_t sts_entries)
  {
    if (! m_super->open_mbox(entries, sts_entries)) return false;

    m_super->setInitState();
    m_super->softRestart();

    for (uint32_t i = 0; i < entries; i++) {
      void *b = calloc(1, PAGE_4K);
      m_super->add_inb_buffer(b);
    }

    return true;
  }

  inline int add_inb_buffer(void* buf) { return m_super->add_inb_buffer(buf); }

// TX
  inline bool queueTxFull() { return m_super->queueTxFull(); }
  inline int  queueTxSize() { return m_super->queueTxSize(); }
  inline void softRestart(const bool nuke_bds = true) { m_super->softRestart(nuke_bds); }

  inline bool send_message(MboxChannel::MboxOptions_t& opt, const void* data, const size_t len, const bool check_reg, MboxChannel::StopTx_t& fail_reason)
  {
    return m_super->send_message(opt, data, len, check_reg, fail_reason);
  }

  inline int scanFIFO(MboxChannel::WorkItem_t* completed_work, const int max_work)
  {
    return m_super->scanFIFO(completed_work, max_work);
  }

// RX
  inline bool inb_message_ready(uint64_t& rx_ts) { return m_super->inb_message_ready(rx_ts); }
  inline void* get_inb_message(MboxChannel::MboxOptions_t& opt) { return m_super->get_inb_message(opt); }
};
#endif // __MBOXMGR_H__
