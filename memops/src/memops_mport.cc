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

#include <stdio.h> // snprintf
#include <assert.h>

#include <vector>

#include "memops_mport.h"
#include "librsvdmem.h"

#define XCRIT(fmt, ...)

RIOMemOpsMport::RIOMemOpsMport(const int mport_id)
{
  m_errno = 0;

  pthread_mutexattr_t mattr; pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&m_memreg_mutex, &mattr);

  int rc = riomp_mgmt_mport_create_handle(mport_id, 0 /*flags*/, &m_mp_h);
  if (rc == 0) return;

  static char tmp[129] = {0};
  snprintf(tmp, 128, "RIOMemOpsMport: riomp_mgmt_mport_create_handle barfed with error %d (%s)", rc, strerror(-rc));

  throw std::runtime_error(tmp);
}
RIOMemOpsMport::~RIOMemOpsMport()
{
  if (! m_memreg.empty()) {
    // Have to collect the map keys here as free_xxx will delete from map
    // and deleting-while-iterating is a BAD idea.
    std::vector<uint64_t> handle_vec;

    // NO unlock (recursive mutex) -- shut out other threads
    pthread_mutex_lock(&m_memreg_mutex);

    std::map<uint64_t, DmaMem_t*>::iterator it = m_memreg.begin();
    for (; it != m_memreg.end(); it++) handle_vec.push_back(it->first);

    std::vector<uint64_t>::iterator itv = handle_vec.begin();
    for (; itv != handle_vec.end(); itv++) free_xwin(*m_memreg[*itv]);
  }

  riomp_mgmt_mport_destroy_handle(&m_mp_h);
  
  // XXX Don't destroy m_memreg_mutex (cannot do it while locked)
}

bool RIOMemOpsMport::nread_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  dmaopt.ticket = 0;

  m_errno = (dmaopt.mem.type == MALLOC)?
    riomp_dma_read(m_mp_h,   dmaopt.destid, dmaopt.raddr.lsb64, dmaopt.mem.win_ptr,                       dmaopt.bcount, dmaopt.sync):
    riomp_dma_read_d(m_mp_h, dmaopt.destid, dmaopt.raddr.lsb64, dmaopt.mem.win_handle, dmaopt.mem.offset, dmaopt.bcount, dmaopt.sync);

  if (m_errno < 0) return false;

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC && m_errno > 0) {
    dmaopt.ticket = m_errno;
    m_errno = 0;
  }
  
  return true;
}

bool RIOMemOpsMport::nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  dmaopt.ticket = 0;

  m_errno = (dmaopt.mem.type == MALLOC)?
    riomp_dma_write(m_mp_h,   dmaopt.destid, dmaopt.raddr.lsb64, dmaopt.mem.win_ptr,                       dmaopt.bcount, dmaopt.wr_mode, dmaopt.sync):
    riomp_dma_write_d(m_mp_h, dmaopt.destid, dmaopt.raddr.lsb64, dmaopt.mem.win_handle, dmaopt.mem.offset, dmaopt.bcount, dmaopt.wr_mode, dmaopt.sync);

  if (m_errno < 0) return false;

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC && m_errno > 0) {
    dmaopt.ticket = m_errno;
    m_errno = 0;
  }

  return true;
}

bool RIOMemOpsMport::wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/)
{
  m_errno = 0;
  if (dmaopt.ticket <= 0) return false;

  int rc = riomp_dma_wait_async(m_mp_h, dmaopt.ticket, timeout);
  if (!rc) return true;

  m_errno = rc;
  return false;
}

bool RIOMemOpsMport::alloc_dmawin(DmaMem_t& mem /*out*/, const int _size)
{
  int size = _size;

  if ((size % 4096) != 0) {
    int k = size / 4096;
    k++;
    size = k * 4096;
  }

  int ret = riomp_dma_dbuf_alloc(m_mp_h, size, &mem.win_handle);
  if (ret) {
    XCRIT("riodp_dbuf_alloc failed: %d:%s\n", ret, strerror(ret));
    return false;
  }

  ret = riomp_dma_map_memory(m_mp_h, size, mem.win_handle, &mem.win_ptr);
  if (ret) {
    riomp_dma_dbuf_free(m_mp_h, &mem.win_handle);
    mem.win_handle = 0;
    XCRIT("FAIL riomp_dma_map_memory: %d:%s\n", ret, strerror(ret));
    return false;
  };

  mem.type = DMAMEM;
  mem.win_size = size;
  mem.offset = 0;

  pthread_mutex_lock(&m_memreg_mutex);
    m_memreg[mem.win_handle] = &mem;
  pthread_mutex_unlock(&m_memreg_mutex);

  return true;
}

bool RIOMemOpsMport::alloc_ibwin_rsvd(DmaMem_t& mem /*out*/, const int _size, const char* RegionName)
{
  int size = _size;

  if ((size % 4096) != 0) {
    int k = size / 4096;
    k++;
    size = k * 4096;
  }

  if (RegionName == NULL || RegionName[0] == '\0') {
    XCRIT("FAIL Must supply a RegionName\n");
    return false;
  }

  uint64_t rsvd_addr = 0;
  uint64_t rsvd_size = 0;
  int ret = get_rsvd_phys_mem((char*)RegionName, &rsvd_addr, &rsvd_size);
  if (ret) {
    XCRIT("FAIL get_rsvd_phys_mem for RegionName=%s -- check /etc/rapidio/rsvd_phys_mem.conf\n", RegionName);
    return false;
  }

  assert(rsvd_addr);
  assert(rsvd_size);

  if ((uint32_t)size < rsvd_size) {
    XCRIT("FAIL RegionName=%s of size 0x%x is smaller than requested size 0x%x\ns -- check /etc/rapidio/rsvd_phys_mem.conf",
          RegionName, rsvd_size, _size);
    return false;
  }

  mem.rio_address = RIO_ANY_ADDR;
  mem.win_handle  = rsvd_addr;
  mem.offset = 0;

  ret = riomp_dma_ibwin_map(m_mp_h, &mem.rio_address, rsvd_size, &mem.win_handle);
  if (ret) {
    mem.win_handle = 0;
    XCRIT("FAIL riomp_dma_ibwin_map/get_rsvd_phys_mem: %d:%s\n", ret, strerror(ret));
    return false;
  }

  ret = riomp_dma_map_memory(m_mp_h, rsvd_size, mem.win_handle, &mem.win_ptr);
  if (ret) {
    mem.win_handle = 0;
    XCRIT("FAIL riomp_dma_map_memory: %d:%s\n", ret, strerror(ret));
    return false;
  }

  mem.type     = IBWIN;
  mem.win_size = rsvd_size;

  pthread_mutex_lock(&m_memreg_mutex);
    m_memreg[mem.win_handle] = &mem;
  pthread_mutex_unlock(&m_memreg_mutex);

  return true;
}

bool RIOMemOpsMport::alloc_ibwin_fixd(DmaMem_t& mem /*out*/, const uint64_t rio_address, const uint64_t handle, const int _size)
{
  assert(handle);
  assert(_size);

  int size = _size;

  if ((size % 4096) != 0) {
    int k = size / 4096;
    k++;
    size = k * 4096;
  }

  mem.rio_address = mem.rio_address;
  mem.win_handle  = handle;

  int ret = riomp_dma_map_memory(m_mp_h, size, mem.win_handle, &mem.win_ptr);
  if (ret) {
    mem.win_handle = 0;
    XCRIT("FAIL riomp_dma_map_memory: %d:%s\n", ret, strerror(ret));
    return false;
  }

  mem.type     = IBWIN_FIXD;
  mem.win_size = size;
  mem.offset   = 0;

  pthread_mutex_lock(&m_memreg_mutex);
    m_memreg[mem.win_handle] = &mem;
  pthread_mutex_unlock(&m_memreg_mutex);

  return true;
}

bool RIOMemOpsMport::alloc_ibwin(DmaMem_t& ibwin /*out*/, const int size)
{
  /* First, obtain an inbound handle from the mport driver */
  int ret = riomp_dma_ibwin_map(m_mp_h, &ibwin.rio_address, size, &ibwin.win_handle);
  if (ret) {
    XCRIT("Failed to map ibwin %d:%s", ret, strerror(errno));
    return false;
  };

  /* Memory-map the inbound window */
  ret = riomp_dma_map_memory(m_mp_h, size, ibwin.win_handle, &ibwin.win_ptr);
  if (ret) {
    riomp_dma_ibwin_free(m_mp_h, &ibwin.win_handle);
    XCRIT("Failed to memory map ibwin %d:%s", ret, strerror(errno));
    return false;
  };

  ibwin.type = IBWIN;
  ibwin.win_size = size;
  ibwin.offset = 0;

  pthread_mutex_lock(&m_memreg_mutex);
    m_memreg[ibwin.win_handle] = &ibwin;
  pthread_mutex_unlock(&m_memreg_mutex);

  return true;
}

bool RIOMemOpsMport::free_dmawin(DmaMem_t& mem)
{
  bool ret = true;

  if(mem.type != DMAMEM)
    throw std::runtime_error("RioMport: Invalid type for DMA buffer!");

  pthread_mutex_lock(&m_memreg_mutex);
    std::map<uint64_t, DmaMem_t*>::iterator it = m_memreg.find(mem.win_handle);
    if (it != m_memreg.end()) m_memreg.erase(it);
  pthread_mutex_unlock(&m_memreg_mutex);

  int rc = riomp_dma_unmap_memory(m_mp_h, mem.win_size, mem.win_ptr);
  if (rc) {
    XCRIT("FAIL riomp_dma_unmap_memory: %d:%s\n", rc, strerror(rc));
    ret = false;
  }

  rc = riomp_dma_dbuf_free(m_mp_h, &mem.win_handle);
  if (rc) {
    XCRIT("FAIL riomp_dma_dbuf_free: %d:%s\n", rc, strerror(rc));
    ret = false;
  }

  return ret;
}

bool RIOMemOpsMport::free_ibwin(DmaMem_t& ibwin)
{
  bool ret = true;

  if(ibwin.type != IBWIN)
    throw std::runtime_error("RioMport: Invalid type for ibwin!");

  pthread_mutex_lock(&m_memreg_mutex);
    std::map<uint64_t, DmaMem_t*>::iterator it = m_memreg.find(ibwin.win_handle);
    if (it != m_memreg.end()) m_memreg.erase(it);
  pthread_mutex_unlock(&m_memreg_mutex);

  /* Memory-unmap the inbound window's virtual pointer */
  int rc = riomp_dma_unmap_memory(m_mp_h, ibwin.win_size, ibwin.win_ptr);
  if (rc) {
    XCRIT("Failed to unmap inbound memory: @%p %d:%s\n",
         ibwin.win_ptr, ret, strerror(ret));
    /* Don't return; still try to free the inbound window */
    ret = false;
  }

  /* Free the inbound window via the mport driver */
  rc = riomp_dma_ibwin_free(m_mp_h, &ibwin.win_handle);
  if (rc) {
    XCRIT("Failed to free inbound memory: %d:%s\n", ret, strerror(ret));
    ret = false;
  }

  return ret;
}

bool RIOMemOpsMport::free_fixd(DmaMem_t& ibwin)
{
  bool ret = true;

  if(ibwin.type != IBWIN_FIXD)
    throw std::runtime_error("RioMport: Invalid type for ibwin/RSKTD!");

  pthread_mutex_lock(&m_memreg_mutex);
    std::map<uint64_t, DmaMem_t*>::iterator it = m_memreg.find(ibwin.win_handle);
    if (it != m_memreg.end()) m_memreg.erase(it);
  pthread_mutex_unlock(&m_memreg_mutex);

  /* Memory-unmap the inbound window's virtual pointer */
  int rc = riomp_dma_unmap_memory(m_mp_h, ibwin.win_size, ibwin.win_ptr);
  if (rc) {
    XCRIT("Failed to unmap inbound memory: @%p %d:%s\n",
          ibwin.win_ptr, ret, strerror(ret));
    ret = false;
  }

  return ret;
}
