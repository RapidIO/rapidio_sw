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
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef __PEDRV_H__
#define __PEDRV_H__


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Device Driver Functions
 */

#define ALL_PE_PORTS ((uint8_t)(0xFF))

typedef uint32_t pe_rt_val;
#define RT_VAL_FIRST_PORT	((pe_rt_val)(0x00000000))
#define RT_VAL_LAST_PORT	((pe_rt_val)(0x000000FF))
#define RT_VAL_FIRST_MC		((pe_rt_val)(0x00000100))
#define RT_VAL_LAST_MC		((pe_rt_val)(0x000001FF))
#define RT_VAL_FIRST_NEXT_LVL 	((pe_rt_val)(0x00000200))
#define RT_VAL_LAST_NEXT_LVL 	((pe_rt_val)(0x000002FF))
#define RT_VAL_DROP		((pe_rt_val)(0x00000300))
#define RT_VAL_DEFAULT_ROUTE	((pe_rt_val)(0x00000301))
#define RT_VAL_BAD		((pe_rt_val)(0x0FFFFFFF))

/* Routing table definitions */
typedef uint8_t pe_port_t;
#define RIOCP_PE_ALL_PORTS (pe_port_t)0xff /* Use the global LUT */

/* Routing table entry values */
#define RIOCP_PE_EGRESS_PORT(n)	((pe_rt_val)(RT_VAL_FIRST_PORT + ((n) & 0xff)))
#define RIOCP_PE_MULTICAST_MASK(n) ((pe_rt_val)(RT_VAL_FIRST_MC + ((n) & 0xff)))
#define RIOCP_PE_NEXT_LEVEL_GROUP(n) ((pe_rt_val)(RT_VAL_FIRST_NEXT_LVL + ((n) & 0xff)))
#define RIOCP_PE_NO_ROUTE           ((pe_rt_val)(0x300))
#define RIOCP_PE_DEFAULT_ROUTE      ((pe_rt_val)(0x301))

#define RIOCP_PE_IS_EGRESS_PORT(n)       ((n) <= 0xff)
#define RIOCP_PE_IS_MULTICAST_MASK(n)    ((n) >= 0x100 && (n) <= 0x1ff)
#define RIOCP_PE_IS_NEXT_LEVEL_GROUP(n)  ((n) >= 0x200 && (n) <= 0x2ff)

#define RIOCP_PE_GET_EGRESS_PORT(n)      (RIOCP_PE_IS_EGRESS_PORT(n)?(((n) & 0xff)):RT_VAL_BAD)
#define RIOCP_PE_GET_MULTICAST_MASK(n)   (RIOCP_PE_IS_MULTICAST_MASK(n)?(((n) - 0x100) & 0xff):RT_VAL_BAD)
#define RIOCP_PE_GET_NEXT_LEVEL_GROUP(n) (RIOCP_PE_IS_NEXT_LEVEL_GROUP(n)?(((n) - 0x200) & 0xff):RT_VAL_BAD)

int  init_pe(struct riocp_pe *pe, uint32_t *ct,
				struct riocp_pe *peer, char *name);
int  init_pe_em(struct riocp_pe *pe, bool en_em);
int  destroy_pe(struct riocp_pe *pe);

int  recover_port(struct riocp_pe *pe, pe_port_t port,
				pe_port_t lp_port);
int  set_port_speed(struct riocp_pe *pe,
				pe_port_t port, uint32_t lane_speed);
int  get_port_state(struct riocp_pe *pe,
				pe_port_t port,
				struct riocp_pe_port_state_t *state);
int  port_start(struct riocp_pe *pe, uint8_t port);
int  port_stop(struct riocp_pe *pe, uint8_t port);

int  set_route_entry(struct riocp_pe *pe,
			pe_port_t port, uint32_t did, pe_rt_val rt_val);
int  get_route_entry(struct riocp_pe *pe,
			pe_port_t port, uint32_t did, pe_rt_val *rt_val);
int  alloc_mcast_mask(struct riocp_pe *sw, pe_port_t port, 
			pe_rt_val *rt_val, int32_t port_mask);
int  free_mcast_mask(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val);
int  change_mcast_mask(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val, uint32_t port_mask);


struct pedrv_driver {
	int RIOCP_WU (* init_pe)(struct riocp_pe *pe, uint32_t *ct,
				struct riocp_pe *peer, char *name);
	int RIOCP_WU (* init_pe_em)(struct riocp_pe *pe, bool en_em);
	int RIOCP_WU (* destroy_pe)(struct riocp_pe *pe);

	int RIOCP_WU (* recover_port)(struct riocp_pe *pe, pe_port_t port,
				pe_port_t lp_port);
	int RIOCP_WU (* set_port_speed)(struct riocp_pe *pe,
				pe_port_t port, uint32_t lane_speed);
	int RIOCP_WU (* get_port_state)(struct riocp_pe *pe,
				pe_port_t port,
				struct riocp_pe_port_state_t *state);
	int RIOCP_WU (* port_start)(struct riocp_pe *pe, uint8_t port);
	int RIOCP_WU (* port_stop)(struct riocp_pe *pe, uint8_t port);

	int RIOCP_WU (* set_route_entry)(struct riocp_pe *pe,
			pe_port_t port, uint32_t did, pe_rt_val rt_val);
	int RIOCP_WU (* get_route_entry)(struct riocp_pe *pe,
			pe_port_t port, uint32_t did, pe_rt_val *rt_val);
	int RIOCP_WU (* alloc_mcast_mask)(struct riocp_pe *sw, pe_port_t port, 
			pe_rt_val *rt_val, int32_t port_mask);
	int RIOCP_WU (* free_mcast_mask)(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val);
	int RIOCP_WU (* change_mcast_mask)(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val, uint32_t port_mask);
};

/* To override functions above, pass in structure with new function.
 * If the existing function should be unchanged, pass in NULL.
 */

int override_pedrv(struct pedrv_driver *drv);

#ifdef __cplusplus
}
#endif

#endif /* __PEDRV_H__ */
