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

#ifndef __MPORTMGMT_H__
#define __MPORTMGMT_H__

#include <stdint.h>
#include <stdio.h> // snprintf
#include <errno.h>

#include <vector>
#include <stdexcept>

#include <rapidio_mport_mgmt.h>

extern char* speed_to_string(int);
extern char* width_to_string(int);

class MportMgmt {
public:
  MportMgmt(int mport_id) : m_mportid(mport_id)
  {
    if (!riomp_mgmt_mport_create_handle(m_mportid, 0, &m_handle))
      throw std::runtime_error("MportMgmt: Failed to create mport handle!");
  }
  ~MportMgmt() { riomp_mgmt_mport_destroy_handle(&m_handle); }

  inline int query(struct riomp_mgmt_mport_properties* qresp) {
    if (qresp == NULL) return -(errno = EINVAL);
    return riomp_mgmt_query(m_handle, qresp);
  }

  static inline std::string toString(struct riomp_mgmt_mport_properties& attr) {
    std::string out;
    char tmp[129] = {0};
    snprintf(tmp, 128, "mport: hdid=%d, id=%d, idx=%d, flags=0x%x, sys_size=%s\n",
             attr.hdid, attr.id, attr.index, attr.flags,
             attr.sys_size?"large":"small");
    out.append(tmp);

    snprintf(tmp, 128, "link: speed=%s width=%s\n", speed_to_string(attr.link_speed),
             width_to_string(attr.link_width));
    out.append(tmp);

    if (attr.flags & RIO_MPORT_DMA) {
       snprintf(tmp, 128, "DMA: max_sge=%d max_size=%d alignment=%d (%s)\n",
                attr.dma_max_sge, attr.dma_max_size, attr.dma_align,
                (attr.flags & RIO_MPORT_DMA_SG)?"HW SG":"no HW SG");
    } else
       snprintf(tmp, 128, "No DMA support\n");
    out.append(tmp);
    return out;
  }

  static inline bool get_mport_list(std::vector<uint32_t>& devids) {
    uint8_t   np = 0;
    uint32_t* dev_ids = NULL;
    if (0 != riomp_mgmt_get_mport_list(&dev_ids, &np)) return false;
    for (int i = 0; i < np; i++) devids.push_back(dev_ids[i]);
    riomp_mgmt_free_mport_list(&dev_ids);
    return true;
  }

  static inline bool get_ep_list(int mport_id, std::vector<uint32_t>& epids) {
    uint32_t  nep = 0;
    uint32_t* ep_ids = NULL;
    if (0 != riomp_mgmt_get_ep_list(mport_id, &ep_ids, &nep)) return false;
    for (int i = 0; i < (int)nep; i++) epids.push_back(ep_ids[i]);
    riomp_mgmt_free_ep_list(&ep_ids);
    return true;
  }

  inline bool get_ep_list(std::vector<uint32_t>& epids) { return get_ep_list(m_mportid, epids); }

  inline int destid_set(uint16_t destid) { return riomp_mgmt_destid_set(m_handle, destid); }

  inline int lcfg_read(uint32_t offset, uint32_t size, uint32_t* data) {
    return riomp_mgmt_lcfg_read(m_handle, offset, size, data);
  }
  inline int lcfg_write(uint32_t offset, uint32_t size, uint32_t data) {
    return riomp_mgmt_lcfg_write(m_handle, offset, size, data);
  }

  inline int rcfg_read(uint32_t destid, uint32_t hc, uint32_t offset, uint32_t size, uint32_t* data) {
    return riomp_mgmt_rcfg_read(m_handle, destid, hc, offset, size, data);
  }
  inline int rcfg_write(uint32_t destid, uint32_t hc, uint32_t offset, uint32_t size, uint32_t data) {
    return riomp_mgmt_rcfg_write(m_handle, destid, hc, offset, size, data);
  }

  // FMD specials
  inline int device_add(uint16_t destid, uint8_t hc, uint32_t ctag, const char *name) {
    return riomp_mgmt_device_add(m_handle, destid, hc, ctag, name);
  }
  inline int device_del(uint16_t destid, uint8_t hc, uint32_t ctag, const char *name) {
    return riomp_mgmt_device_del(m_handle, destid, hc, ctag, name);
  }

private:
  int           m_mportid;
  riomp_mport_t m_handle;
};

#endif // __MPORTMGMT_H__
