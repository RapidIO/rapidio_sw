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

#ifndef __MEMOPS_MPORT_H__
#define __MEMOPS_MPORT_H__

#include <pthread.h>
#include <errno.h>

#include <map>

#include "memops.h"

/** \brief Libmport implementation of RIOMemOpsIntf */
class RIOMemOpsMport : public RIOMemOpsIntf {
public:
  RIOMemOpsMport(const int mport);
  virtual ~RIOMemOpsMport();

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/);
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/);

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/);

  virtual bool alloc_dmawin(DmaMem_t& mem /*out*/, const int size);
  virtual bool alloc_ibwin(DmaMem_t& mem /*out*/, const int size);
  virtual bool alloc_ibwin_rsvd(DmaMem_t& mem /*out*/, const int size, const char* RegionName);

  virtual bool alloc_ibwin_fixd(DmaMem_t& mem /*out*/, const uint64_t rio_address, const uint64_t handle, const int size);

  virtual int getAbortReason() { return -m_errno; }
  virtual const char* abortReasonToStr(const int dma_abort_reason) { return strerror(dma_abort_reason); }

private:
  virtual bool free_dmawin(DmaMem_t& mem);
  virtual bool free_ibwin(DmaMem_t& mem);
  virtual bool free_fixd(DmaMem_t& mem);

protected:
  riomp_mport_t m_mp_h;
  int           m_errno;

private:
  std::map<uint64_t, DmaMem_t*> m_memreg;
  pthread_mutex_t               m_memreg_mutex;
};

#endif //__MEMOPS_MPORT_H__
