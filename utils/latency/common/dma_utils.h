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

#ifndef DMA_UTILS_H
#define DMA_UTILS_H

#include <stdint.h>

#include "debug.h"
#include "tsi721_dma.h"
#include "peer_utils.h"
#include "rio_register_utils.h"
#include "time.h"

#define ONE_PAGE_LENGTH (4096)

/* DMA Descriptors */
#define DMA_DESCRIPTORS_LENGTH    (4096)    

/* DMA Status FIFO */
#define DMA_STATUS_FIFO_LENGTH (4096)

/* DMA Data */
#define DMA_DATA_LENGTH_MULTIPLE (4096)

static inline uint32_t is_pow_of_two( uint32_t n )
{
    return ((n & (n - 1)) == 0) ? 1 : 0;
} /* is_pow_of_two() */

/**
 * __fls - find last (most-significant) set bit in a long word
 * @word: the word to search
 *
 * Undefined if no set bit exists, so code should check against 0 first.
 */
#define BITS_PER_LONG (32)
static __always_inline unsigned long __fls(unsigned long word)
{
	int num = BITS_PER_LONG - 1;

#if BITS_PER_LONG == 64
	if (!(word & (~0ul << 32))) {
		num -= 32;
		word <<= 32;
	}
#endif
	if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
		num -= 16;
		word <<= 16;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
		num -= 8;
		word <<= 8;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
		num -= 4;
		word <<= 4;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
		num -= 2;
		word <<= 2;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-1))))
		num -= 1;
	return num;
}

int roundup_pow_of_two( int n );

uint32_t dma_get_alloc_data_length( uint32_t length );

void *dmatest_buf_alloc(riomp_mport_t mp_h, uint32_t size, uint64_t *handle);

void dmatest_buf_free(riomp_mport_t mp_h, void *buf, uint32_t size, uint64_t *handle);

int dma_alloc_chan_resources(struct peer_info *peer);

void dma_free_chan_resources(struct peer_info *peer);

int dma_transmit(int demo_mode,
                 enum dma_dtype dtype,
                 uint16_t destid,
                 struct peer_info *peer_src,
                 struct peer_info *peer_dst,
		struct timespec *before,
		struct timespec *after,
		uint32_t	*limit);

int dma_slave_transmit_prep(struct peer_info *peer_src,
                            enum dma_dtype *dtype,
                            uint32_t *rd_count);

int dma_slave_transmit_exec(uint16_t destid,
                            enum dma_dtype dtype,
                            uint32_t rd_count,
                            struct peer_info *peer_src,
                            struct timespec *before,
		                    struct timespec *after
);

static inline uint32_t DMARegister(struct peer_info *peer,
                                   uint32_t offset)
{
    uint32_t value = RIORegister(peer,TSI721_DMAC_BASE(peer->channel_num)+offset);
    DPRINT("%s:offset=0x%08X, value=0x%08X\n", 
                                    __FUNCTION__, 
                                    TSI721_DMAC_BASE(peer->channel_num) + offset, 
                                    value);
    return value;
}

static inline void WriteDMARegister(struct peer_info *peer,
                                    uint32_t offset,
                                    uint32_t data)
{
    DPRINT("%s:offset=0x%08X, value=0x%08X\n",
                                    __FUNCTION__, 
                                    TSI721_DMAC_BASE(peer->channel_num)+offset,
                                    data);
    WriteRIORegister(peer,TSI721_DMAC_BASE(peer->channel_num)+offset,data);
}

#endif

