#ifndef __MEMOPS_UMDD_H__
#define __MEMOPS_UMDD_H__

#include <errno.h>

#include "memops.h"
#include "memops_mport.h"

#include "dmachan.h"

class RIOMemOpsUMD : public RIOMemOpsMport {
public:
  RIOMemOpsUMD(const int mport, const int chan);
  virtual ~RIOMemOpsUMD();
  
  virtual bool queueFull() { return DMAChannelSHM_queueFull(m_dch); }

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/);

  virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
    throw std::runtime_error("RIOMemOpsUMD::alloc_umem: Unsupported memory type!");
  }

  virtual int getAbortReason();
  virtual const char* abortReasonToStr(const int dma_abort_reason);

private:
  DMAChannel*   m_dch;
  struct seq_ts m_stats;
};

#endif //__MEMOPS_UMDD_H__
