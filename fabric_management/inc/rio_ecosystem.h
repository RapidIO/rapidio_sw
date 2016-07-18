/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, Prodrive Technologies
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

THIS SOFTWARE IS PROVENDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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
/* Definitions for the RapidIO ecosystem, including vendors/devices and 
 * device port/lane limits.
 */

#include <stdint.h>
#include "rio_standard.h"

#ifndef __RIO_ECOSYSTEM_H__
#define __RIO_ECOSYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t rio_port_t;
typedef uint8_t rio_lane_t;
/** \brief Hopcount type for maintenance transactions */
typedef uint16_t rio_hc_t;
/** \brief Use HC_LOCAL to identify registers on the local RapidIO interface */
#define HC_LOCAL (rio_hc_t)(0x100)

typedef uint16_t rio_dev16_t; /* Also supports dev8 */
typedef uint32_t rio_dev32_t; /* Also supports dev8 & dev16 */

typedef uint32_t rio_mc_mask_t; /* Biggest mask available */

#define RIO_SW_PORT_INF_PORT_MAX ((RIO_SW_PORT_INF_T)(24))
#define RIO_SW_PORT_INF_LANE_MAX ((RIO_SW_PORT_INF_T)(48))

#define RIO_BAD_PORT_NUM(x) (x >= RIO_SW_PORT_INF_PORT_MAX)

#define RIO_ALL_PORTS       ((rio_port_t)(0xFF))
#define RIO_MAX_DEV_PORT    ((rio_port_t)(RIO_SW_PORT_INF_PORT_MAX))
#define RIO_MAX_PORT_LANES  ((rio_lane_t)(4))
#define RIO_MAX_DEV_LANES   ((rio_lane_t)(RIO_SW_PORT_INF_LANE_MAX))
#define RIO_MAX_MC_MASKS    RIO_RT_GRP_SIZE

/* Maximum values for number of routing table groups at particular level. */
#define RIO_MAX_RT_L0 8
#define RIO_MAX_RT_L1 3
#define RIO_MAX_RT_L2 4

#define RIO_VEND_RESERVED        0xffff
#define RIO_DEVI_RESERVED        0xffff
#define RIO_BAD_OFFSET 0xFFFFFF

#define RIO_VEND_FREESCALE      0x0002
#define RIO_VEND_TUNDRA         0x000d
#define RIO_DEVI_TSI500         0x0500
#define RIO_DEVI_TSI568         0x0568
#define RIO_DEVI_TSI572         0x0572
#define RIO_DEVI_TSI574         0x0574
#define RIO_DEVI_TSI576         0x0578 /* Same ID as Tsi578 */
#define RIO_DEVI_TSI577         0x0577
#define RIO_DEVI_TSI578         0x0578

#define RIO_VEND_TI             0x0030

#define RIO_VEND_IDT            0x0038
#define RIO_DEVI_IDT_CPS1848    0x0374
#define RIO_DEVI_IDT_CPS1432    0x0375
#define RIO_DEVI_IDT_CPS1616    0x0379
#define RIO_DEVI_IDT_SPS1616    0x0378

#define RIO_DEVI_IDT_TSI721     0x80ab
#define RIO_DEVI_IDT_RXS2448    0x80E6
#define RIO_DEVI_IDT_RXS1632    0x80E5

#define RIO_VEND_PRODRIVE       0x00a4

#ifdef __cplusplus
}
#endif

#endif /* __RIO_ECOSYSTEM_H__ */

