/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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

#ifndef PCIE_UTILS_H
#define PCIE_UTILS_H

#include <stdint.h>

#include "peer_utils.h"
#include "debug.h"

int pcie_bar0_map(struct peer_info *peer);

void pcie_bar0_unmap(struct peer_info *peer);

int pcie_bar2_map(struct peer_info *peer);

void pcie_bar2_unmap(struct peer_info *peer);


static inline void bar2_write_8(struct peer_info *peer, uint32_t offset, uint8_t data)
{
   *((volatile uint8_t *)((uint8_t *)peer->bar2_base_ptr + offset)) = data;
}

static inline void bar2_write_16(struct peer_info *peer, uint32_t offset, uint16_t data)
{
   *((volatile uint16_t *)((uint8_t *)peer->bar2_base_ptr + offset)) = data;
}

static inline void bar2_write_32(struct peer_info *peer, uint32_t offset, uint32_t data)
{
   *((volatile uint32_t *)((uint8_t *)peer->bar2_base_ptr + offset)) = data;
}

static inline uint8_t bar2_read_8(struct peer_info *peer, uint32_t offset)
{
   return *((volatile uint8_t *)((uint8_t *)peer->bar2_base_ptr + offset));
}

static inline uint16_t bar2_read_16(struct peer_info *peer, uint32_t offset)
{
   return *((volatile uint16_t *)((uint8_t *)peer->bar2_base_ptr + offset));
}

static inline uint32_t bar2_read_32(struct peer_info *peer, uint32_t offset)
{
   return *((volatile uint32_t *)((uint8_t *)peer->bar2_base_ptr + offset));
}


#endif

