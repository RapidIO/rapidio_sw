#ifndef __RDMAOPS_H__
#define __RDMAOPS_H__

#include <stdint.h>

#include "mport.h"
#include "dmachan.h"

class RdmaOpsIntf {
public:
  virtual ~RdmaOpsIntf() {;}

  virtual bool canRestart() { return false; }
  
  virtual bool queueFull() = 0;

  virtual void setCheckHwReg(bool sw) = 0;
  virtual uint16_t getDestId() = 0;

  // T2 Ops
  virtual bool nread_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out) = 0;
  virtual bool nwrite_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, const uint8_t* data) = 0;

  // T1 Ops
  virtual bool nwrite_mem(DMAChannel::DmaOptions_t& dmaopt, RioMport::DmaMem_t& dmamem) = 0;

  virtual int getAbortReason() = 0;

  virtual const char* abortReasonToStr(const int dma_abort_reason) = 0;
};

static inline bool udma_nread_mem(RdmaOpsIntf* rdma, const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out)
{
  assert(rdma);
  return rdma->nread_mem_T2(destid, rio_addr, size, data_out);
}

static inline bool udma_nwrite_mem(RdmaOpsIntf* rdma, const uint16_t destid, const uint64_t rio_addr, const int size, const uint8_t* data)
{
  assert(rdma);
  return rdma->nwrite_mem_T2(destid, rio_addr, size, data);
}

static inline bool udma_nwrite_mem_T1(RdmaOpsIntf* rdma, DMAChannel::DmaOptions_t& dmaopt, RioMport::DmaMem_t& dmamem, int& dma_abort_reason)
{
  assert(rdma);
  const bool r = rdma->nwrite_mem(dmaopt, dmamem);

  dma_abort_reason = rdma->getAbortReason();
  return r;
}

#endif // __RDMAOPS_H__
