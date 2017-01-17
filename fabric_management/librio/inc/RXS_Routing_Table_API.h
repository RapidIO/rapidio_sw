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

#ifndef __RXS_ROUTING_TABLE_API_H__
#define __RXS_ROUTING_TABLE_API_H__


#ifdef __cplusplus
extern "C" {
#endif

#define RXS_MAX_MC_MASKS                      0xFF

#define RXS_RTE_SET_COMMON_0                  (RT_FIRST_SUBROUTINE_0+0x0100)
#define RXS_PROGRAM_RTE_ENTRIES_0             (RT_FIRST_SUBROUTINE_0+0x1900)
#define RXS_PROGRAM_MC_MASKS_0                (RT_FIRST_SUBROUTINE_0+0x1A00)
#define RXS_READ_MC_MASKS_0                   (RT_FIRST_SUBROUTINE_0+0x1B00)
#define RXS_READ_RTE_ENTRIES_0                (RT_FIRST_SUBROUTINE_0+0x1C00)

#define RXS_PROGRAM_MC_MASKS(x)               (RXS_PROGRAM_MC_MASKS_0+x)
#define RXS_PROGRAM_RTE_ENTRIES(x)            (RXS_PROGRAM_RTE_ENTRIES_0+x)
#define RXS_RTE_SET_COMMON(x)                 (RXS_RTE_SET_COMMON_0+x)
#define RXS_READ_MC_MASKS(x)                  (RXS_READ_MC_MASKS_0+x)
#define RXS_READ_RTE_ENTRIES(x)               (RXS_READ_RTE_ENTRIES_0+x)

#define RXS_SET_ALL                           true
#define RXS_SET_CHANGED                       false

#ifdef __cplusplus
}
#endif

#endif /* __RXS_ROUTING_TABLE_API_H__ */
