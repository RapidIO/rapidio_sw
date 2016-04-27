#include <stdio.h> // snprintf

#include "memops_mport.h"

#define XCRIT(fmt, ...)

RIOMemOpsMport::RIOMemOpsMport(const int mport_id)
{
  m_errno = 0;

  int rc = riomp_mgmt_mport_create_handle(mport_id, 0 /*flags*/, &m_mp_h);
  if (rc == 0) return;

  static char tmp[129] = {0};
  snprintf(tmp, 128, "RIOMemOpsMport: riomp_mgmt_mport_create_handle barfed with error %d (%d)", rc, strerror(rc));

  throw std::runtime_error(tmp);
}
RIOMemOpsMport::~RIOMemOpsMport()
{
  riomp_mgmt_mport_destroy_handle(&m_mp_h);
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

  return 0 == riomp_dma_wait_async(m_mp_h, timeout, dmaopt.ticket);
}

bool RIOMemOpsMport::alloc_cmawin(DmaMem_t& mem /*out*/, const int _size)
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

  mem.type = CMAMEM;
  mem.win_size = size;

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

  return true;
}

bool RIOMemOpsMport::alloc_obwin(DmaMem_t& obwin /*out*/, const uint16_t destid, const int size)
{
  /* First, obtain an outbound handle from the mport driver */
  int ret = riomp_dma_obwin_map(m_mp_h, destid, obwin.rio_address, size, &obwin.win_handle);
  if (ret) {
    XCRIT("Failed to map obwin %d:%s", ret, strerror(errno));
    return false;
  };

  /* Memory-map the outbound window */
  ret = riomp_dma_map_memory(m_mp_h, size, obwin.win_handle, &obwin.win_ptr);
  if (ret) {
    riomp_dma_obwin_free(m_mp_h, &obwin.win_handle);
    XCRIT("Failed to memory map obwin %d:%s", ret, strerror(errno));
    return false;
  };

  obwin.type = OBWIN;
  obwin.win_size = size;

  return true;
}

bool RIOMemOpsMport::free_cmawin(DmaMem_t& mem)
{
  bool ret = true;

  if(mem.type != CMAMEM)
    throw std::runtime_error("RioMport: Invalid type for DMA buffer!");

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
        XCRIT("Failed to free inbound memory: %d:%s\n",
              ret, strerror(ret));
    /* Don't return; still try to free the inbound window */
    ret = false;
  }

  return ret;
}

bool RIOMemOpsMport::free_obwin(DmaMem_t& obwin)
{
  bool ret = true;

  if(obwin.type != OBWIN)
    throw std::runtime_error("RioMport: Invalid type for obwin!");

  /* Memory-unmap the inbound window's virtual pointer */
  int rc = riomp_dma_unmap_memory(m_mp_h, obwin.win_size, obwin.win_ptr);
  if (rc) {
    XCRIT("Failed to unmap outbound memory: @%p %d:%s\n",
          obwin.win_ptr, ret, strerror(ret));
    /* Don't return; still try to free the inbound window */
    ret = false;
  }

  /* Free the outbound window via the mport driver */
  rc = riomp_dma_obwin_free(m_mp_h, &obwin.win_handle);
  if (rc) {
    XCRIT("Failed to free outbound memory: %d:%s\n",
           ret, strerror(ret));
    /* Don't return; still try to free the inbound window */
    ret = false;
  }

  return ret;
}
