#ifndef __MEMOPS_UMD_H__
#define __MEMOPS_UMD_H__

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "memops.h"
#include "memops_mport.h"

#include "dmachan.h"
#include "chanlock.h"

class RIOMemOpsUMD : public RIOMemOpsMport {
public:
  RIOMemOpsUMD(const int mport, const int chan);
  virtual ~RIOMemOpsUMD();
  
  virtual bool queueFull() { return m_dch->queueFull(); }
  virtual bool canRestart() { return true; }

  virtual bool canSync() { return false; }
  virtual bool canAsync() { return false; }

  virtual bool restartChannel() { m_dch->softRestart(); return true; }

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/) {
    throw std::runtime_error("RIOMemOpsUMD::wait_async: Operation not supported! Use FAF.");
  }

  virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
    throw std::runtime_error("RIOMemOpsUMD::alloc_umem: Unsupported memory type!");
  }

  virtual int getAbortReason();
  virtual const char* abortReasonToStr(const int dma_abort_reason);

  void set_fifo_thr_callback(void (*cbk)(void*), void* cbk_arg = NULL) {
    m_dma_fifo_callback     = cbk;
    m_dma_fifo_callback_arg = cbk_arg;
  }

  bool setup_channel(const int buf, const int sts);

  // isolcpu = -1 means any CPU for FIFO thread
  // Note: Call set_fifo_thr_callback first!
  // Note: Call setup_channel first!
  bool start_fifo_thr(int isolcpu, bool thruput = false) {
    m_isolcpu = isolcpu;
    m_thruput = thruput;
    return _start_fifo_thr();
  }

private:
  void run_fifo_thr();
  bool _start_fifo_thr();
  friend void* RIOMemOpsUMD_fifo_proc_thr(void* arg); // libpthread helper, cannot be member

private:
  DMAChannel*   m_dch;
  int           m_chan;
  int           m_bufc;
  int           m_sts_entries;
  uint64_t      m_ticks_total;
  struct seq_ts m_stats;
  int           m_isolcpu;
  int           m_run_cpu;
  bool          m_thruput;

  pthread_t     m_fifo_thr;
  sem_t         m_fifo_proc_started;
  volatile bool m_fifo_proc_alive;
  bool          m_fifo_proc_thr_started;
  volatile int  m_fifo_proc_must_die;

  void          (*m_dma_fifo_callback)(void*);
  void*         m_dma_fifo_callback_arg;

  LockFile*     m_lock;
};

#endif //__MEMOPS_UMD_H__
