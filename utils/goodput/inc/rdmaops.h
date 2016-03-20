#ifndef __RDMAOPS_H__
#define __RDMAOPS_H__

#include <stdint.h>

#include "mport.h"
#include "dmachan.h"

class RdmaOpsIntf {
public:
  virtual bool canRestart() { return false; }
  
  virtual void setCheckHwReg(bool sw) = 0;

  // T2 Ops
  virtual bool nread_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out) = 0;
  virtual bool nwrite_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, const uint8_t* data) = 0;

  // T1 Ops
  virtual bool nwrite_mem(DMAChannel::DmaOptions_t& dmaopt, RioMport::DmaMem_t& dmamem) = 0;

  virtual int getAbortReason() = 0;

  virtual const char* abortReasonToStr(const int dma_abort_reason) = 0;
};

#endif // __RDMAOPS_H__
