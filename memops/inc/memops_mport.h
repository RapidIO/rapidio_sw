#ifndef __MEMOPS_MPORT_H__
#define __MEMOPS_MPORT_H__

#include <errno.h>

#include <memops.h>

class RIOMemOpsMport : public RIOMemOpsIntf {
public:
  RIOMemOpsMport(const int mport);
  virtual ~RIOMemOpsMport();

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/);

  virtual bool alloc_cmawin(DmaMem_t& mem /*out*/, const int size);
  virtual bool alloc_ibwin(DmaMem_t& mem /*out*/, const int size);
  virtual bool alloc_obwin(DmaMem_t& mem /*out*/, const uint16_t destid, const int size);

  virtual int getAbortReason() { return m_errno; }
  virtual const char* abortReasonToStr(const int dma_abort_reason) { return strerror(m_errno); }

private:
  virtual bool free_cmawin(DmaMem_t& mem /*out*/);
  virtual bool free_ibwin(DmaMem_t& mem /*out*/);
  virtual bool free_obwin(DmaMem_t& mem /*out*/);
  virtual bool freelloc_obwin(DmaMem_t& mem /*out*/);

private:
  riomp_mport_t m_mp_h;
  int           m_errno;
};

#endif //__MEMOPS_MPORT_H__
