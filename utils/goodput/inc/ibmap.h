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

#ifndef __IBMAP_H__
#define __IBMAP_H__

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <stdexcept>
#include <map>

#include "udma_tun.h"

class IBwinMap {
public:
  IBwinMap(const uint64_t rio_addr, const void* ib_ptr, const uint32_t ib_size, const int bufc, const int tun_MTU) :
    m_rio_addr(rio_addr), m_ib_ptr((uint8_t*)ib_ptr), m_ib_size(ib_size)
  {
    if (rio_addr == 0) throw std::runtime_error("IBwinMap: Invalid rio_addr!");
    if (ib_ptr == NULL) throw std::runtime_error("IBwinMap: Invalid ib_ptr!");

    if (tun_MTU < 580) throw std::runtime_error("IBwinMap: Invalid tun_MTU!");

    if (ib_size < (sizeof(uint32_t) + sizeof(DMA_L2_SIZE) + tun_MTU))
      throw std::runtime_error("IBwinMap: Invalid ib_size!");

    if (bufc < (0x20-1)) throw std::runtime_error("IBwinMap: Invalid bufc!");

    PEER_IBWIN_SIZE = sizeof(uint32_t) + bufc * (sizeof(DMA_L2_SIZE) + tun_MTU);
    MAX_PEERS = ib_size / PEER_IBWIN_SIZE;

    assert(MAX_PEERS);

    m_slot_allocated = (int*)calloc(MAX_PEERS + 1, sizeof(int));
    if (m_slot_allocated == NULL) throw std::runtime_error("IBwinMap: Out of memory!");

    m_slot_allocated[MAX_PEERS] = 0xdeadbeefL;

    pthread_mutex_init(&m_mutex, NULL);
  } 

  inline bool lookup(const uint16_t destid, uint64_t& rio_addr, void*& ib_ptr)
  {
    return alloc(destid, rio_addr, ib_ptr);
  }

  inline bool alloc(const uint16_t destid, uint64_t& rio_addr, void*& ib_ptr)
  {
    assert(this);

    bool ret = false;
    int slot = -1;

    rio_addr = 0;
    ib_ptr = NULL;

    pthread_mutex_lock(&m_mutex);

    std::map<uint16_t, int>::iterator itm = m_destid_map.find(destid);
    if (itm != m_destid_map.end()) { // already allocated OK, just fish it
      slot = itm->second;
      assert(m_slot_allocated[slot] == 1);
      goto done_ok;
    }

    slot = -2;
    for (int i = 0; i < MAX_PEERS; i++) {
      if (m_slot_allocated[i] > 0) continue;
      slot = i;
      break;
    }

    if (slot == -2) goto done; // IBwin is full

    // NEW
    m_slot_allocated[slot] = 1;
    m_destid_map[destid]   = slot;

done_ok:
    assert(slot >= 0);
    rio_addr = m_rio_addr + slot * PEER_IBWIN_SIZE;
    ib_ptr   = m_ib_ptr   + slot * PEER_IBWIN_SIZE;

    memset(ib_ptr, 0, PEER_IBWIN_SIZE);

    ret = true;

done:
    pthread_mutex_unlock(&m_mutex);

    return ret;
  }

  inline bool free(const uint16_t destid)
  {
    bool ret = false;

    assert(this);

    pthread_mutex_lock(&m_mutex);
    std::map<uint16_t, int>::iterator itm = m_destid_map.find(destid);
    if (itm == m_destid_map.end()) goto done;

    {{
      const int slot = itm->second;
      assert(m_slot_allocated[slot] == 1);
      m_slot_allocated[slot] = -1;

      m_destid_map.erase(itm);

      uint32_t* pRP = (uint32_t*)(m_ib_ptr + slot * PEER_IBWIN_SIZE);
      pRP[0] = ~0;
    }}

done:
    pthread_mutex_unlock(&m_mutex);
    return ret;
  }

  ~IBwinMap() {
    pthread_mutex_lock(&m_mutex);
    assert(m_slot_allocated);
    assert((uint32_t)m_slot_allocated[MAX_PEERS] == 0xdeadbeefL);
    ::free(m_slot_allocated);
  }

  inline uint64_t getBaseRioAddr() { return m_rio_addr; }
  inline uint64_t getBaseSize()    { return m_ib_size; }
  inline void*    getBasePtr()     { return m_ib_ptr; }

  inline int toString(std::string& s)
  {
    int cnt = 0;
    char tmp[257] = {0};

    pthread_mutex_lock(&m_mutex);
    std::map<uint16_t, int>::iterator itm = m_destid_map.begin();
    for (; itm != m_destid_map.end(); itm++) {
      const uint16_t destid = itm->first;
      const int      slot   = itm->second;
      const uint64_t rio_addr = m_rio_addr + slot * PEER_IBWIN_SIZE;
      const void*    ib_ptr   = m_ib_ptr   + slot * PEER_IBWIN_SIZE;
      snprintf(tmp, 256, "\tdestid %u slot=%d rio_addr=0x%llx ib_ptr=%p\n", destid, slot, rio_addr, ib_ptr);
      s.append(tmp);
      cnt++;
    }
    pthread_mutex_unlock(&m_mutex);

    return cnt;
  }

private:
  uint64_t  m_rio_addr;
  uint8_t*  m_ib_ptr;
  uint32_t  m_ib_size;
  uint32_t  PEER_IBWIN_SIZE;
  uint32_t  MAX_PEERS;
  int*      m_slot_allocated; // 0 = never allocated, 1 allocated, -1 freed
  std::map<uint16_t, int> m_destid_map;
  pthread_mutex_t m_mutex;
};

#endif // __IBMAP_H__
