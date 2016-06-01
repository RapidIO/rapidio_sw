/*
 * Standard register functions, stubbed for the moment.
 */
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

#include <stdint.h>
#include <errno.h>
#include "rio_car_csr.h"
#include "regrw.h"

#ifdef __cplusplus
extern "C" {
#endif

int rcc_set_access(struct rio_car_csr *rcc, uint32_t dest_id, tt_t tt,
	uint16_t hc)
{
	rcc->dest_id = dest_id;
	rcc->tt = tt;
	rcc->hc = hc;

	return 0;
};

int rcc_write_destid(struct rio_car_csr *rcc, uint32_t dest_id, tt_t tt)
{
	uint32_t oset = 0;
	uint32_t val = 0;
	uint32_t mask = 0;
	int rc = 0;

	switch (tt) {
	case tt_dev8: oset = RIO_DEVID;
			val = (dest_id << 16) & RIO_DEVID_DEV8;
			mask = ~RIO_DEVID_DEV8;
			break;
	case tt_dev16: oset = RIO_DEVID;
			val = dest_id & RIO_DEVID_DEV16;
			mask = ~RIO_DEVID_DEV16;
			break;
        case tt_dev32: return EINVAL;
			break;	
	default: return EINVAL;
	}

	val = (rcc->did & mask) | val;
	rc = raw_reg_wr(rcc, rcc->tt, rcc->dest_id, rcc->hc, oset, val);

	if (!rc)
		rcc->did = val;
	return rc;
};

int rcc_read_destids(struct rio_car_csr *rcc)
{
	uint32_t val;
	int rc;
	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc, RIO_DEVID, &val);

	if (!rc)
		rcc->did = val;
	return rc;
};

int rcc_write_comptag(struct rio_car_csr *rcc, uint32_t ct)
{
	int rc;
	rc = raw_reg_wr(rcc, rcc->tt, rcc->dest_id, rcc->hc, RIO_COMPTAG, ct);

	if (!rc)
		rcc->comptag = ct;
	return rc;
};

int rcc_read_comptag(struct rio_car_csr *rcc)
{
	uint32_t val;
	int rc;

	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc, RIO_COMPTAG, &val);
	if (!rc)
		rcc->comptag = val;
	return rc;
};

int rcc_lock(struct rio_car_csr *rcc, uint32_t lock_val)
{
	uint32_t val;
	int rc;

	if (RIO_HOST_LOCK_UNLOCKED == lock_val)
		return EINVAL;

	rc = raw_reg_wr(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_HOST_LOCK, lock_val);
	if (rc)
		return rc;

	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_HOST_LOCK, &val);
	if (!rc)
		rcc->host_lock = val;
	return rc;
};

int rcc_unlock(struct rio_car_csr *rcc, uint32_t unlock_val)
{
	return rcc_lock(rcc, unlock_val);
};

int rcc_set_addrsz(struct rio_car_csr *rcc, RIO_PE_ADDR_T addrsz )
{
	uint32_t val;
	int rc;

	addrsz &= rcc->pe_feat & RIO_PE_FEAT_EXT_ADDR;
	if ((addrsz != RIO_PE_LL_CTL_34BIT) &&
	(addrsz != RIO_PE_LL_CTL_50BIT) &&
	(addrsz != RIO_PE_LL_CTL_66BIT))
		return EINVAL;

	rc = raw_reg_wr(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_PE_LL_CTL, addrsz);
	if (rc)
		return rc;

	rc = raw_reg_rd(rcc, rcc->tt, rcc->dest_id, rcc->hc,
						RIO_PE_LL_CTL, &val);
	if (!rc)
		rcc->pe_ll_ctl = val & RIO_PE_FEAT_EXT_ADDR;
	return rc;
};


int rcc_set_enumb(struct rio_car_csr *rcc, rio_port_t pt, bool enumb)
{
	rcc->global_scratch = enumb?pt:0;
	return ENOSYS;
};

int rcc_get_errstat(struct rio_car_csr *rcc, rio_port_t pt)
{
	rcc->global_scratch = pt;
	return ENOSYS;
};

int rcc_lreq_n_resp(struct rio_car_csr *rcc, rio_port_t pt, 
		RIO_SPX_LM_RESP_STAT_T resp)
{
	rcc->global_scratch = pt + resp;
	return ENOSYS;
};

int rcc_port_enable(struct rio_car_csr *rcc, rio_port_t pt, 
		bool port_enable, bool port_lkout, bool in_out_en)
{
	rcc->global_scratch =(port_enable & port_lkout && in_out_en)?pt:0;
	return ENOSYS;
};

int rcc_emerg_lkout(struct rio_car_csr *rcc, rio_port_t pt)
{
	rcc->global_scratch = pt;
	return ENOSYS;
};

int rcc_set_rte(struct rio_car_csr *rcc, rio_port_t pt,
		tt_t tt, uint32_t did, pe_rt_val rte)
{
	rcc->global_scratch = (tt == tt_dev8)?pt + did + rte:0;
	return ENOSYS;
};

int rcc_get_rte(struct rio_car_csr *rcc, rio_port_t pt,
		tt_t tt, uint32_t did, pe_rt_val *rte)
{
	*rte = (pe_rt_val)pt;
	rcc->global_scratch = (tt == tt_dev8)?(did):0;
	return ENOSYS;
};

int rcc_init_rte(struct rio_car_csr *rcc, rio_port_t pt,
		tt_t tt, pe_rt_val *rte)
{
	rcc->global_scratch = (tt == tt_dev8)?pt:0;
	*rte = 0;
	return ENOSYS;
};

int rcc_set_dflt_rte(struct rio_car_csr *rcc, pe_rt_val *rte)
{
	rcc->global_scratch = 0;
	*rte = 0;
	return ENOSYS;
};


#ifdef __cplusplus
}
#endif
