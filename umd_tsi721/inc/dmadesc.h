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

#ifndef __DMADESC_H__
#define __DMADESC_H__

#include <stdint.h>
#include <string.h>

#include <stdexcept>

#include "local_endian.h"

#define SIXTYFOURMEG 64*1024*1024

/**
 * \verbatim
RADDR Spiel:

The RapidIO protocol is defined as big endian, which means bit 0 is the most significant bit in the address.  The layout of the descriptors is little endian.

A 66 bit address defined in big endian as addr[0:65] becomes addr [0:1], addr [2:33], and addr [34:65] when divided into the bit fields of the descriptor.

But to show the big endian values in the little endian descriptor, it's necessary to swap the bit ordering, hence addr[1:0], addr [33:2] and addr [65:34].
 * \endverbatim
*/

#if 0
enum dma_dtype {
  DTYPE1 = 1, /* Data Transfer DMA Descriptor */
  DTYPE2 = 2, /* Immediate Data Transfer DMA Descriptor */
  DTYPE3 = 3, /* Block Pointer DMA Descriptor */
  DTYPE4 = 4, /* Outbound Msg DMA Descriptor */
  DTYPE5 = 5, /* OB Messaging Block Pointer Descriptor */
  DTYPE6 = 6  /* Inbound Messaging Descriptor */
}

enum dma_rtype {
  NREAD = 0,
  LAST_NWRITE_R = 1,
  ALL_NWRITE = 2,
  ALL_NWRITE_R = 3,
  MAINT_RD = 4,
  MAINT_WR = 5
}
#endif

struct hw_dma_desc {
  uint32_t type_id;
  uint32_t bcount;
  union {
    uint32_t raddr_lo;     /* if DTYPE == (1 || 2) */
    uint32_t next_lo;      /* if DTYPE == 3 */
  };
  union {
    uint32_t raddr_hi;     /* if DTYPE == (1 || 2) */
    uint32_t next_hi;      /* if DTYPE == 3 */
  };
  union {
    struct {               /* if DTYPE == 1 */
      uint32_t bufptr_lo;
      uint32_t bufptr_hi;
      uint32_t s_dist;
      uint32_t s_size;
    } t1;
    uint32_t data[4];      /* if DTYPE == 2 */
    uint32_t reserved[4];  /* if DTYPE == 3 */
  };
} __attribute__((aligned(32)));

struct dmadesc {
  dmadesc() { memset(this, 0, sizeof(*this)); }

  inline int pack(hw_dma_desc* bd_ptr)
  {
    uint64_t rio_addr;
    uint32_t flags;

    if (NULL == bd_ptr) return -1;

    struct dmadesc* desc = this;
    memset(bd_ptr, 0, sizeof(*bd_ptr));

    switch (dtype) {
      case 3:
        bd_ptr->type_id = le32(desc->dtype << 29);
        bd_ptr->next_lo = le32(desc->next_ptr & TSI721_DMAC_DPTRL_MASK);
        bd_ptr->next_hi = le32(desc->next_ptr >> 32);
        break;

      case 1:
        flags = (desc->crf << 16) | (desc->prio << 17) | (desc->iof << 27);

        bd_ptr->type_id = le32((desc->dtype << 29) |
                          (desc->rtype << 19) | desc->devid | flags);

        bd_ptr->bcount = le32(((desc->raddr_lsb64 & 0x3) << 30) |
                         (desc->tt << 26) | desc->bcount);

        rio_addr = (desc->raddr_lsb64 >> 2) |
                   ((uint64_t)(desc->raddr_msb2 & 0x3) << 62);
      
        bd_ptr->raddr_lo = le32(rio_addr & 0xffffffff);
        bd_ptr->raddr_hi = le32(rio_addr >> 32);
        bd_ptr->t1.bufptr_lo = le32(desc->buffer_ptr & 0xffffffff);
        bd_ptr->t1.bufptr_hi = le32(desc->buffer_ptr >> 32);
        break;

      case 2:
        flags = (desc->crf << 16) | (desc->prio << 17) | (desc->iof << 27);

        bd_ptr->type_id = le32((desc->dtype << 29) |
                          (desc->rtype << 19) | desc->devid | flags);

        bd_ptr->bcount = le32(((desc->raddr_lsb64 & 0x3) << 30) |
                         (desc->tt << 26) | (desc->bcount & 0xf));

        rio_addr = (desc->raddr_lsb64 >> 2) |
                   ((uint64_t)(desc->raddr_msb2 & 0x3) << 62);

        bd_ptr->raddr_lo = le32(rio_addr & 0xffffffff);
        bd_ptr->raddr_hi = le32(rio_addr >> 32);

        if (desc->rtype != 0 /*NREAD*/ && desc->rtype != 4 /*MAINT_RD*/)
          memcpy(bd_ptr->data, desc->t2_data, 16);
        break;

      default: assert(0); break;
    }

    return sizeof(*bd_ptr);
  }
  
  uint16_t devid;
  uint8_t  crf, prio, rtype, dtype, iof, tt;
  uint32_t bcount;
  uint8_t  raddr_msb2;
  uint64_t raddr_lsb64;
  
  // T1
  uint64_t buffer_ptr;

  // T2  
  uint32_t t2_data_len;
  uint8_t  t2_data[16];

  // T3
  uint64_t next_ptr;
};

#ifndef TSI721_DMAC_DPTRL_MASK
  #define TSI721_DMAC_DPTRL_MASK  0xffffffe0
#endif

static inline void dmadesc_setdevid(struct dmadesc &desc, const uint16_t devid)
{
  desc.devid = devid;
}

static inline void dmadesc_setdtype(struct dmadesc &desc, const uint8_t type)
{
  desc.dtype = type & 0x7;
}

static inline void dmadesc_setiof(struct dmadesc &desc, const uint8_t iof)
{
  desc.iof = !! iof;
}
static inline void dmadesc_setcrf(struct dmadesc &desc, const uint8_t crf)
{
  desc.crf = !! crf;
}
static inline void dmadesc_setprio(struct dmadesc &desc, const uint8_t prio)
{
  desc.prio = prio & 0x3;
}
static inline void dmadesc_setrtype(struct dmadesc &desc, const uint8_t rtype)
{
  desc.rtype = rtype & 0xf;
}

static inline void dmadesc_set_raddr(struct dmadesc &desc, const uint8_t raddr_msb2, const uint64_t raddr_lsb64)
{
  desc.raddr_msb2  = raddr_msb2 & 0x3;
  desc.raddr_lsb64 = raddr_lsb64;
}

static inline void dmadesc_set_tt(struct dmadesc &desc, const uint8_t tt)
{
  desc.tt = tt & 0x3;
}

/// T1
static inline void dmadesc_setT1_bufptr(struct dmadesc &desc, const uint64_t bufptr)
{
  if(bufptr == 0)
    throw std::runtime_error("dmadesc_setT1_bufptr: NULL pointer!");

  desc.buffer_ptr = bufptr;
}
static inline void dmadesc_setT1_buflen(struct dmadesc &desc, const uint32_t buflen)
{
  if(buflen > SIXTYFOURMEG)
    throw std::runtime_error("dmadesc_setT1_buflen: data are too large!");

  desc.bcount = (buflen == SIXTYFOURMEG)? 0: buflen;
}

/// T2
static inline void dmadesc_setT2_data(struct dmadesc &desc, const uint8_t* data, const uint32_t data_len)
{
  if(data_len > 16)
    throw std::runtime_error("dmadesc_setT2_data: data are too large!");

  desc.t2_data_len = data_len;
  desc.bcount = (data_len == 16)? 0: data_len;
  memcpy(desc.t2_data, data, data_len);
}

/// T3
static inline void dmadesc_setT3_nextptr(struct dmadesc &desc, const uint64_t next)
{
  if (!next)
        throw std::runtime_error("dmadesc_setT3_nextptr: 0 pointer!");

  desc.next_ptr = next;
}

#endif // __DMADESC_H__
