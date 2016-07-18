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
#ifdef __cplusplus
extern "C" {
#endif

int idt_rxs_fill_in_handle(regrw_i *h)
{
	if (read_rt_regs(h))
		goto fail;

	for (idx = 0; idx < RIO_LVL_GRP_SZ; idx++) {
                if (idx < RIO_MC_MASK_COUNT(h)) {
                        if (regrw_raw_rd(hnd, CPS_BROADCAST_MC_MASK_ENTRY(idx),
                                        &h->rt->bc.mc[0][idx])) {
                                goto fail;
                        };
                };
                if (read_cps_rte_to_std_rte(hnd,
                        CPS_BROADCAST_MC_DOMAIN_RT_ENTRY(idx),
                        &h->rt->bc.rt1[0][idx])) {
                        goto fail;
                };
                if (read_cps_rte_to_std_rte(hnd, CPS_BROADCAST_UC_DEVICE_RT_ENTRY(idx),
                                &h->rt->bc.rt2[0][idx])) {
                        goto fail;
                };
        };
	
fail:
	return -1;
};

#ifdef __cplusplus
}
#endif
