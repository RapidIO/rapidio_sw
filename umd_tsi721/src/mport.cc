/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <map>
#include <stdexcept>
#include <errno.h>
#include <string.h>

#include "pciebar.h"
#include "mport.h"

#include "IDT_Tsi721.h"
#include "tsi721_dma.h"
//#include "liblog.h"

extern "C" {
#include "rapidio_mport_dma.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
};

/** \brief Creates an instance of RioMport class
 * \throws std::runtime_error
 * \param mportid instance N of mport_cdev as seen in /dev/rio_mportN
 */
RioMport::RioMport(const int mportid)
{
  int rc;
  m_portid = mportid;
  is_my_mport_handle = 1;
  rc = riomp_mgmt_mport_create_handle(m_portid, UMD_RESERVED, &mp_h);

  if(rc) {
    XCRIT("Failed to open mport%d %d:%s", m_portid, rc, strerror(errno));
    throw std::runtime_error("Failed to open mport");
  }

  rc = riomp_mgmt_query(mp_h, &m_props);
  if (rc) {
    riomp_mgmt_mport_destroy_handle(&mp_h);

    XCRIT("Failed to query mport%d %d:%s", m_portid, rc, strerror(errno));
    throw std::runtime_error("Failed to query mport");
  };

  try {
    m_bar0 = new PCIeBAR(m_portid, 0);                    
  } catch(std::runtime_error e) {
    riomp_mgmt_mport_destroy_handle(&mp_h);
    throw std::runtime_error("failed PCIeBAR");
  }
  m_bar0_base_ptr = m_bar0->getMem(m_bar0_size); 
}

/** \brief Creates an instance of RioMport class
 * \throws std::runtime_error
 * \param[in] mportid instance N of mport_cdev as seen in /dev/rio_mportN
 * \param[in] mp_h_in mport handle passed in.
 *
 * An mport handle instance may be a copy of another MPORT handle, or a fresh
 * instance.  If the mport handle is not a copy, a separate DMA engine may be
 * allocated against the mport handle.
 */
RioMport::RioMport(const int mportid, riomp_mport_t mp_h_in)
{
  int rc;
  m_portid = mportid;


  is_my_mport_handle = 0;
  mp_h = mp_h_in;

  rc = riomp_mgmt_query(mp_h, &m_props);
  if (rc) {
    XCRIT("Failed to query mport%d %d:%s", m_portid, rc, strerror(errno));
    throw std::runtime_error("Failed to query mport");
  };

  try {
    m_bar0 = new PCIeBAR(m_portid, 0);                    
  } catch(std::runtime_error e) {
    throw std::runtime_error("failed PCIeBAR");
  }
  m_bar0_base_ptr = m_bar0->getMem(m_bar0_size); 
}

/** \brief Request an inbound HW memory window from mport_cdev
 * \param[in] size size of window XXX cannot exceed 16M
 * \param[in] ibwin Requested RapidIO memory address
 * \param[out] ibwin inbound window mapping, see \ref DmaMem_t
 */
bool RioMport::map_ibwin(const uint32_t size, DmaMem_t& ibwin)
{
  /* First, obtain an inbound handle from the mport driver */
  int ret = riomp_dma_ibwin_map(mp_h, &ibwin.rio_address, size, &ibwin.win_handle);
  if (ret) {
    XCRIT("Failed to map ibwin %d:%s", ret, strerror(errno));
    return false;
  };

  /* Memory-map the inbound window */
  ret = riomp_dma_map_memory(mp_h, size, ibwin.win_handle, &ibwin.win_ptr);
  if (ret) {
    riomp_dma_ibwin_free(mp_h, &ibwin.win_handle);
    XCRIT("Failed to memory map ibwin %d:%s", ret, strerror(errno));
    return false;
  };

  ibwin.type = IBWIN;
  ibwin.win_size = size;

  m_dmaibwin_reg[ibwin.win_handle] = ibwin;

  return true;
}

/** \brief Release an inbound HW memory window to mport_cdev
 * \throws std::runtime_error
 * \note It will barf if the ibwin was not mapped by this instance
 * \param[in] ibwin inbound window mapping, see \ref DmaMem_t
 */

bool RioMport::unmap_ibwin(DmaMem_t& ibwin)
{
  bool ret = true;
  int rc;

  if(ibwin.type != IBWIN)
    throw std::runtime_error("RioMport: Invalid type for ibwin!");

  std::map <uint64_t, DmaMem_t>::iterator it
        = m_dmaibwin_reg.find(ibwin.win_handle);
  if (it == m_dmaibwin_reg.end())
    throw std::runtime_error("RioMport: Invalid DMA ibwin to unmap"
          " -- does NOT belog to this instance!");

  m_dmaibwin_reg.erase(it);

  /* Memory-unmap the inbound window's virtual pointer */
  rc = riomp_dma_unmap_memory(mp_h, ibwin.win_size, ibwin.win_ptr);
  if (rc) {
    XCRIT("Failed to unmap inbound memory: @%p %d:%s\n",
         ibwin.win_ptr, ret, strerror(ret));
    /* Don't return; still try to free the inbound window */
    ret = false;
  }

  /* Free the inbound window via the mport driver */
  rc = riomp_dma_ibwin_free(mp_h, &ibwin.win_handle);
  if (rc) {
        XCRIT("Failed to free inbound memory: %d:%s\n",
              ret, strerror(ret));
    /* Don't return; still try to free the inbound window */
    ret = false;
  }

  return ret;
}

int RioMport::lcfg_read(uint32_t offset, uint32_t size, uint32_t *data)
{
  if(data == NULL)
    return -1;

  return riomp_mgmt_lcfg_read(mp_h, offset, size, data);
}

int RioMport::lcfg_read_u32(uint32_t offset, uint32_t *data)
{
  return riomp_mgmt_lcfg_read(mp_h, offset, 4, data);
}

bool RioMport::check_ibwin_reg()
{
  int found_one = 0;
  uint32_t regval, regval2;

  // Check that inbound window mapping isn't broken...
  for (int i = 0; i < 8; i++) {
    if (lcfg_read_u32(TSI721_IBWIN_LBX(i), &regval))
      goto fail;

    if (regval & TSI721_IBWIN_LBX_WIN_EN) {
      uint64_t ib_win = 0;

      if (lcfg_read_u32(TSI721_IBWIN_UBX(i), &regval2))
        goto fail;

      ib_win = ((uint64_t)(regval2) << 32) + regval;
      XCRIT("Tsi721: IBWIN %d RIOADDR 0x%lx\n", i, ib_win);
      if (found_one)
        XCRIT("Tsi721: Multiple ib win!\n");
      found_one++;
    }
  }

  return true;
fail:
  return false;
}

/** \brief Request a DMA buffer in HW memory from mport_cdev
 * \param size size of window, in bytes
 * \param[in] mem Requested RapidIO memory address
 * \param[out] mem DMA buffer, see \ref DmaMem_t
 */
bool RioMport::map_dma_buf(uint32_t size, DmaMem_t& mem)
{
  int ret;
  if ((size % 4096) != 0) {
    int k = size / 4096;
    k++;
    size = k * 4096;
  }

  ret = riomp_dma_dbuf_alloc(mp_h, size, &mem.win_handle);
  if (ret) {
        XCRIT("riodp_dbuf_alloc failed: %d:%s\n", ret, strerror(ret));
    return false;
  }

  ret = riomp_dma_map_memory(mp_h, size, mem.win_handle, &mem.win_ptr);
    if (ret) {
        riomp_dma_dbuf_free(mp_h, &mem.win_handle);
    mem.win_handle = 0;
        XCRIT("FAIL riomp_dma_map_memory: %d:%s\n", ret, strerror(ret));
    return false;
  };

  mem.type = DMAMEM;
  mem.win_size = size;

  m_dmatxmem_reg[mem.win_handle] = mem;

  return true;
}

/** \brief Release a DMA buffer in HW memory to mport_cdev
 * \throws std::runtime_error
 * \note It will barf if the buffer was not mapped by this instance
 * \param[in] mem DMA buffer, see \ref DmaMem_t
 */
bool RioMport::unmap_dma_buf(DmaMem_t& mem)
{
  int rc;
  bool ret = true;

  if(mem.type != DMAMEM)
    throw std::runtime_error("RioMport: Invalid type for DMA buffer!");

  std::map <uint64_t, DmaMem_t>::iterator it =
          m_dmatxmem_reg.find(mem.win_handle);
  if (it == m_dmatxmem_reg.end())
    throw std::runtime_error("RioMport: Invalid DMA buffer to unmap -- does NOT belog to this instance!");

  DmaMem_t mymem = it->second;

  m_dmatxmem_reg.erase(it);

  rc = riomp_dma_unmap_memory(mp_h, mymem.win_size, mymem.win_ptr);
  if (rc) {
        XCRIT("FAIL riomp_dma_unmap_memory: %d:%s\n", rc, strerror(rc));
    ret = false;
  }

  rc = riomp_dma_dbuf_free(mp_h, &mem.win_handle);
  if (rc) {
        XCRIT("FAIL riomp_dma_dbuf_free: %d:%s\n", rc, strerror(rc));
    ret = false;
  }

  return ret;
}
