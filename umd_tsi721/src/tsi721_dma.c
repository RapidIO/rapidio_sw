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

#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <stdexcept>
#include <string>
#include <sstream>

#include "tsi721_dma.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "local_endian.h"

#ifdef __cplusplus
extern "C" {
#endif

void packT3(tsi721_dma_desc* bd_ptr, uint64_t next_ptr)
{
        memset(bd_ptr, 0, sizeof(*bd_ptr));

        bd_ptr->type_id = le32(DTYPE3 << 29);
        bd_ptr->next_lo = le32(next_ptr & TSI721_DMAC_DPTRL_MASK);
        bd_ptr->next_hi = le32(next_ptr >> 32);
};

void packT1n2 (tsi721_dma_desc* bd_ptr, 
                uint8_t rtype, uint8_t prio, uint8_t crf,
                uint16_t devid, uint8_t tt, uint32_t bcount,
                uint64_t raddr_lsb64, uint8_t raddr_msb2,
                uint8_t *buffer_ptr)

{
	enum dma_dtype dtype = DTYPE1;

        memset(bd_ptr, 0, sizeof(*bd_ptr));

	if (bcount <= 16) {
		dtype = DTYPE2;
                if (rtype != NREAD)
                        memcpy(&bd_ptr->data[0], buffer_ptr, 16);
	} else {
		if (bcount == (TSI721_DMAD_BCOUNT1+1))
			bcount = 0;
		bcount &= TSI721_DMAD_BCOUNT1;
                bd_ptr->t1.bufptr_lo = le32((uint64_t)buffer_ptr & 0xffffffff);
                bd_ptr->t1.bufptr_hi = le32((uint64_t)buffer_ptr >> 32);
	};

        bd_ptr->type_id = le32((dtype << 29) |
                        (rtype << 19) | (prio << 17) | (crf << 16) |
                        (uint32_t)devid);

        bd_ptr->bcount = le32(((raddr_lsb64 & 0x3) << 30) |
                        (tt << 26) | bcount);

        raddr_lsb64 = ((uint64_t)(raddr_msb2 & 0x3) << 62) |
                        (raddr_lsb64 >> 2);

        bd_ptr->raddr_lo = le32(raddr_lsb64 & 0xffffffff);
        bd_ptr->raddr_hi = le32(raddr_lsb64 >> 32);
};

int test_packing(void) {
	struct tsi721_dma_desc desc;
	uint8_t data[16];

	memset(&desc, 0xFF, sizeof(desc));
	packT3(&desc, 0x1f23456789abcdFF);

	if (desc.type_id != 0x60000000)
		goto fail;
	if (desc.bcount != 0)
		goto fail;
	if (desc.next_lo != 0x89abcde0)
		goto fail;
	if (desc.next_hi != 0x1f234567)
		goto fail;
	if (desc.data[0] || desc.data[1] || desc.data[2] || desc.data[3])
		goto fail;
	
	memset(&desc, 0xFF, sizeof(desc));
	for (int i = 0x10; i < 0x20; i++)
		data[i-0x10] = i;

	packT1n2(&desc, NREAD, 2, 1, 0xFFFF, 1, (1024*1024*64) - 1,
		0xF123456789abcdef, 3, data);
	
	if (desc.type_id != 0x2005FFFF)
		goto fail;
	if (desc.bcount != (0xC7000000 | ((1024*1024*64) - 1)))
		goto fail;
	if (desc.raddr_lo != 0xE26AF37B)
		goto fail;
	if (desc.raddr_hi != 0xFC48D159)
		goto fail;
	if (desc.t1.bufptr_lo != ((uint64_t)data & (uint64_t)0xFFFFFFFF))
		goto fail;
	if (desc.t1.bufptr_hi != (((uint64_t)data >> 32) & (uint64_t)0xFFFFFFFF))
		goto fail;
	if (desc.t1.s_dist || desc.t1.s_size)
		goto fail;

	memset(&desc, 0xFF, sizeof(desc));
	for (int i = 0x10; i < 0x20; i++)
		data[i-0x10] = i;

	packT1n2(&desc, ALL_NWRITE_R, 1, 0, 0xFF, 0, 17,
		0xfedcba9876543210, 2, data);
	
	if (desc.type_id != 0x201A00FF)
		goto fail;
	if (desc.bcount != 0x00000011)
		goto fail;
	if (desc.raddr_lo != 0x1d950c84)
		goto fail;
	if (desc.raddr_hi != 0xbfb72ea6)
		goto fail;
	if (desc.t1.bufptr_lo != ((uint64_t)data & (uint64_t)0xFFFFFFFF))
		goto fail;
	if (desc.t1.bufptr_hi != (((uint64_t)data >> 32) & (uint64_t)0xFFFFFFFF))
		goto fail;
	if (desc.t1.s_dist || desc.t1.s_size)
		goto fail;

	memset(&desc, 0xFF, sizeof(desc));
	for (int i = 0x10; i < 0x20; i++)
		data[i-0x10] = i;

	packT1n2(&desc, NREAD, 2, 1, 0xFFFF, 1, 16,
		0xF123456789abcdef, 3, data);
	
	if (desc.type_id != 0x4005FFFF)
		goto fail;
	if (desc.bcount != 0xC4000010)
		goto fail;
	if (desc.raddr_lo != 0xe26af37b)
		goto fail;
	if (desc.raddr_hi != 0xfc48d159)
		goto fail;
	if (desc.data[0] || desc.data[1] || desc.data[2] || desc.data[3])
		goto fail;

	packT1n2(&desc, LAST_NWRITE_R, 2, 1, 0xFFFF, 1, 16,
		0xF123456789abcdef, 3, data);
	
	if (desc.type_id != 0x400DFFFF)
		goto fail;
	if (desc.bcount != 0xC4000010)
		goto fail;
	if (desc.raddr_lo != 0xe26af37b)
		goto fail;
	if (desc.raddr_hi != 0xfc48d159)
		goto fail;
	if (desc.data[0] != 0x13121110)
		goto fail;
	if (desc.data[1] != 0x17161514)
		goto fail;
	if (desc.data[2] != 0x1b1a1918)
		goto fail;
	if (desc.data[3] != 0x1f1e1d1c)
		goto fail;

	return 0;
fail:
	return 1;
}

#ifdef __cplusplus
}
#endif

