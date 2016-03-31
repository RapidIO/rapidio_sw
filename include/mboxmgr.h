#ifndef __MBOXMGR_H__
#define __MBOXMGR_H__

#include "mboxchan.h"
#include "lockfile.h"

class MboxChannelMgr {
private:
  MboxChannel* m_super;
  LockFile*    m_lock;
  uint32_t     m_mportid;
  uint32_t     m_mbox;


  /** \brief Lock other processes out of this UMD MBOX mport:channel
   * \note Due to POSIX locking semantics this has no effect on the current process
   * \note Using the same channel twice in this process will NOT be prevented
   */
  inline void init()
  {
    char lock_name[81] = { 0 };
    snprintf(lock_name, 80, "/var/lock/UMD-MBOX-%d:%d..LCK", m_mportid, m_mbox);
    try {
      m_lock = new LockFile(lock_name);
    } catch(std::runtime_error ex) {
      delete m_super;
      CRIT("\n\tTaking lock %s failed: %s\n", lock_name, ex.what());
      throw ex;
    }
  }

public:
  enum { PAGE_4K = 4096 };

  MboxChannelMgr(const uint32_t mportid, const uint32_t mbox) :
    m_mportid(mportid), m_mbox(mbox)
  {
    m_super = new MboxChannel(mportid, mbox);
    init();
  }
  MboxChannelMgr(const uint32_t mportid, const uint32_t mbox, riomp_mport_t mp_h) :
    m_mportid(mportid), m_mbox(mbox)
  {
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

    for (int i = 0; i < entries; i++) {
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
