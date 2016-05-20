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

/** \brief Wrapper class for Mport management functions
 * \note Some functions in this class are static and cand be use without class instantiation.
 */
class MportMgmt {
public:
	/** \brief Create a handle for the /dev/rio_mportX device. A list of available
	* device indexes is returned by get_mport_list.
	*
	* @param[in] mport_id : Index of /devs/rio_mportX device to use.  For a list
	*					of available mports, use mportmgmt.h::get_mport_list.
	*
	* @return MportMgmt object, or thrown exception.
	*/
  MportMgmt(int mport_id) : m_mportid(mport_id)
  {
    if (riomp_mgmt_mport_create_handle(m_mportid, 0, &m_handle))
      throw std::runtime_error("MportMgmt: Failed to create mport handle!");
  }
  ~MportMgmt() { riomp_mgmt_mport_destroy_handle(&m_handle); }

  /** \brief Query Mport properties */
  inline int query(struct riomp_mgmt_mport_properties& qresp) {
    return riomp_mgmt_query(m_handle, &qresp);
  }

  /** \brief Convert Mport properties to human readable string */
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

  /** \brief Enumerate installed Mports (aka Tsi721 PCIe cards for x86). Static. */
  static inline bool get_mport_list(std::vector<uint32_t>& devids) {
    uint8_t   np = 0;
    uint32_t* dev_ids = NULL;
    if (0 != riomp_mgmt_get_mport_list(&dev_ids, &np)) return false;
    for (int i = 0; i < np; i++)
      devids.push_back(dev_ids[i] >> 16); /// \todo UNDOCUMENTED: Lower nibble has destid, ignored
    riomp_mgmt_free_mport_list(&dev_ids);
    return true;
  }

  /** \brief Enumerate RapidIO destination IDs known to the RapidIO kernel driver and
   * associated with the specific mport_id. A list of available
  * device indexes is returned by get_mport_list.
   * 
   * All destination IDs are dev8 (8 bit) in size.
   *
   * Static.
   */
  static inline bool get_ep_list(int mport_id, std::vector<uint32_t>& epids) {
    uint32_t  nep = 0;
    uint32_t* ep_ids = NULL;
    if (0 != riomp_mgmt_get_ep_list(mport_id, &ep_ids, &nep)) return false;
    for (int i = 0; i < (int)nep; i++) epids.push_back(ep_ids[i]);
    riomp_mgmt_free_ep_list(&ep_ids);
    return true;
  }

  /** \brief Enumerate RapidIO destination IDs known to the RapidIO kernel driver and
  * associated with this handle.
  *
  * All destination IDs are dev8 (8 bit) in size.
  */
  inline bool get_ep_list(std::vector<uint32_t>& epids) { return get_ep_list(m_mportid, epids); }

private:
  int           m_mportid;
  riomp_mport_t m_handle;
};

#endif // __MPORTMGMT_H__
