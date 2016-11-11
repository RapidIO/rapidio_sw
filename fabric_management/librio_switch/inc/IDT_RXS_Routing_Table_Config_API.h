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

#ifndef __IDT_RXS_ROUTING_TABLE_CONFIG_API_H__
#define __IDT_RXS_ROUTING_TABLE_CONFIG_API_H__


#ifdef __cplusplus
extern "C" {
#endif

#define IDT_RXS_DSF_RT_NO_ROUTE                         0x300
#define IDT_RXS_DSF_RT_USE_PACKET_ROUTE                 0x301

#define IDT_RXS_DSF_FIRST_MC_MASK                       0x0100
#define IDT_RXS_DSF_MAX_MC_MASK                         0x00FF
#define IDT_RXS_DSF_BAD_MC_MASK                         (IDT_RXS_DSF_FIRST_MC_MASK+IDT_RXS_DSF_MAX_MC_MASK)

#define IDT_RXS_MC_MASK_IDX_FROM_ROUTE(x) (uint32_t)(((x >= IDT_RXS_DSF_FIRST_MC_MASK) && (x < IDT_RXS_DSF_BAD_MC_MASK))?(x - IDT_RXS_DSF_FIRST_MC_MASK):IDT_RXS_DSF_BAD_MC_MASK)
#define IDT_RXS_MC_MASK_ROUTE_FROM_IDX(x) (uint32_t)((x < IDT_RXS_DSF_MAX_MC_MASK)?(IDT_RXS_DSF_FIRST_MC_MASK + x):IDT_RXS_DSF_BAD_MC_MASK)


#ifdef __cplusplus
}
#endif

#endif /* __IDT_RXS_ROUTING_TABLE_CONFIG_API_H__ */
