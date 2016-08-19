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
#include "regrw.h"
#include "regrw_private.h"
#include "CPS1848.h"

#ifdef __cplusplus
extern "C" {
#endif

int read_cps_rte_to_std_rte(regrw_i *i, uint32_t offset, uint32_t *val)
{
	uint32_t temp = 0;
	if (read_cps_rte_to_std_rte(i, offset, &temp)) {
		goto fail;
	};

	if (temp < PE_PORT_COUNT(i)) {
		*val = RIO_RTV_GET_PORT(*val);
	} else if ((temp >= CPS_MC_FIRST) && (temp <= CPS_MC_LAST)) {
		*val = RIO_RTV_MC_MSK(temp - CPS_MC_FIRST);
	} else if (temp == CPS_RTE_USE_L2) {
		*val = RIO_RTV_LVL_G0;
	} else if (temp == CPS_RTE_DROP) {
		*val = RIO_RTV_DFLT_PORT;
	} else if (temp == CPS_RTE_DFLT) {
		*val = RIO_RTV_DFLT_PORT;
	} else {
		*val = RIO_RTV_BAD;
	};
	return 0;
fail:
	return -1;

};

int idt_cps_fill_in_handle(regrw_i *i)
{
	uint idx;
	rio_port_t pt;

	// FAKE STANDARD ROUTING TABLE REGISTERS FOR CPS FAMILY!
	i->cregs.rt_oset = 0xE0800; // Bogus address, within routing table range
	i->rt->bc.ctl = RIO_RT_BC_CTL_MC_MASK_SZ8 |
			RIO_RT_BC_CTL_DEV32_RT_CTRL |
			RIO_RT_BC_CTL_THREE_LEVELS;
	i->rt->bc.mc_i = CPS_BROADCAST_MC_MASK_BASE;
	i->rt->bc.lvl0_i = 0;
	i->rt->bc.lvl1_i = 0x01000000 | CPS_BROADCAST_DEVICE_ROUTE_TABLE;
	i->rt->bc.lvl2_i = 0x01000000 | CPS_BROADCAST_DOMAIN_ROUTE_TABLE;
	for (idx = 0; idx < RIO_LVL_GRP_SZ; idx++) {
		if (idx < RIO_MC_MASK_COUNT(i)) {
			if (regrw_raw_rd(i, i->dest_id, i->hc,
					CPS_BROADCAST_MC_MASK_ENTRY(idx),
					&i->rt->bc.mc[idx])) {
				goto fail;
			};
		};
		if (read_cps_rte_to_std_rte(i,
			CPS_BROADCAST_MC_DOMAIN_RT_ENTRY(idx),
			&i->rt->bc.rt1[0][idx])) {
			goto fail;
		};
		if (read_cps_rte_to_std_rte(i,
				CPS_BROADCAST_UC_DEVICE_RT_ENTRY(idx),
				&i->rt->bc.rt2[0][idx])) {
			goto fail;
		};
	};

	for (pt = 0; pt < PE_PORT_COUNT(i); pt++) {
		i->rt->bc.ctl = RIO_RT_BC_CTL_MC_MASK_SZ8 |
				RIO_RT_BC_CTL_DEV32_RT_CTRL |
				RIO_RT_BC_CTL_THREE_LEVELS;
		i->rt->bc.mc_i = (0x01000000 | CPS_PORT_N_MC_MASK_BASE)
				+ (pt * CPS_PORT_N_MC_MASK_STRIDE);
		i->rt->bc.lvl1_i = (0x01000000 | CPS_PORT_DOMAIN_ROUTE_TABLE)
				+ (pt * CPS_PORT_ROUTE_TABLE_PORT_STRIDE);
		i->rt->bc.lvl2_i = (0x01000000 | CPS_PORT_DEVICE_ROUTE_TABLE)
				+ (pt * CPS_PORT_ROUTE_TABLE_PORT_STRIDE);
		for (idx = 0; idx < RIO_LVL_GRP_SZ; idx++) {
			if (idx < RIO_MC_MASK_COUNT(i)) {
				if (regrw_raw_rd(i, i->dest_id, i->hc,
					CPS_PORT_BASE_MC_MASK_ENTRY(pt, idx),
						&i->rt->pt[pt].mc[idx])) {
					goto fail;
				};
			};
			if (read_cps_rte_to_std_rte(i,
					CPS_BROADCAST_MC_DOMAIN_RT_ENTRY(idx),
					&i->rt->pt[pt].rt1[0][idx])) {
				goto fail;
			}
			if (read_cps_rte_to_std_rte(i,
					CPS_BROADCAST_UC_DEVICE_RT_ENTRY(idx),
					&i->rt->pt[pt].rt2[0][idx])) {
				goto fail;
			}
		}
	};
	return 0;
fail:
	return -1;
};

#ifdef __cplusplus
}
#endif
