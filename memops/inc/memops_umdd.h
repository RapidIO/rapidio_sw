#ifndef __MEMOPS_UMDD_H__
#define __MEMOPS_UMDD_H__

#include <errno.h>

#include "memops.h"
#include "memops_mport.h"

#include "dmachanshm.h"

class RIOMemOpsUMDd : public RIOMemOpsMport {
public:
  RIOMemOpsUMDd(const int mport, const int chan);
  virtual ~RIOMemOpsUMDd();
  
  virtual bool queueFull() { return DMAChannelSHM_queueFull(m_dch); }

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/);

  virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
    throw std::runtime_error("RIOMemOpsUMDd::alloc_umem: Unsupported memory type!");
  }

  virtual int getAbortReason();
  virtual const char* abortReasonToStr(const int dma_abort_reason);

private:
  void*         m_dch;
  riomp_mport_t m_mp_h;
  int           m_errno;
  uint32_t      m_cookie_cutter;
  struct seq_ts m_stats;
  std::map<uint64_t, DMAChannelSHM::DmaOptions_t> m_asyncm;
};

#endif //__MEMOPS_UMDD_H__
