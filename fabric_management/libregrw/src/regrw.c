/*
 * Register read/write functions
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
#include "regrw.h"
#include "regrw_private.h"
#include "regrw_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DAR_SCRPAD_IDX 30

struct regrw_driver regrw_dflt_drv;

int regrw_get_handle(regrw_h *h)
{
	struct regrw_i *hnd;
	errno = 0;

	*h = calloc(1, sizeof(struct regrw_i));

	if (NULL == h) {
		return -1;
	};
	if (regrw_override_h_drvr(h, &regrw_dflt_drv)) {
		return -1;
	};
	return 0;
};

int regrw_init_handle(regrw_h h, uint32_t did, rio_hc_t hc)
{
	uint32_t vend_devi;
	int rc;
	struct regrw_i *hnd = H_TO_I(h);

	errno = 0;
	if (NULL == hnd) {
		errno = EINVAL;
		goto fail;
	}

	if (regrw_raw_read(h, did, hc, RIO_DEV_IDENT, &vend_devi)) {
		goto fail;
	}

	if (regrw_set_path(h, did, hc)) {
		goto fail;
	}

/* FIXME: Allocate switch information here */

	if (regrw_fill_in_handle(hnd, vend_devi)) {
		goto fail;
	};

	if (PE_IS_SW(h)) {
	};

	return 0;
fail:
	return -1;
};

int regrw_destroy_handle(regrw_h *h)
{
	struct regrw_i *i;
	errno = 0;

	if (NULL == h) {
		errno = EINVAL;
		return -1;
	};

	i = H_TO_I(*h);

	if (NULL == i) {
		return 0;
	};

	i = H_TO_I(h);

	if (NULL != i->sw_info) {
		free(i->sw_info);
		i->sw_info = NULL;
	};
	if (NULL != i->scratchpad) {
		free(i->scratchpad);
		i->scratchpad = NULL;
	};
	free(*h);
	*h = NULL;
	return 0;
};

int regrw_set_path(regrw_h h, uint32_t did, rio_hc_t hc);
{
	struct regrw_i *i = H_TO_I(h);

	errno = 0;
	if ((NULL == h) || (hc > HC_LOCAL)) {
		goto fail;
	};

	i->dest_id = did;
	i->tt = tt_dev8;
	i->hc = hc;

	return 0;
fail:
	errno = EINVAL;
	return -1;
};

int regrw_get_path(regrw_h h, uint32_t *did, rio_hc_t *hc);
{
	struct regrw_i *i = H_TO_I(h);

	errno = 0;
	if ((NULL == h) || (i->tt == tt_uninit) || (i->tt >= tt_last) ||
			(i->hc > HC_LOCAL)) {
		goto fail;
	};

	*did = hnd->dest_id;
	*hc = hnd->hc;

	return 0;
fail:
	errno = EINVAL;
	return -1;
};

int regrw_rd(regrw_h h, uint32_t offset, uint32_t *val)
{
        struct regrw_i *i = H_TO_I(h);

        uint32_t temp;
        int rc = 0;

	if ((NULL == h) || (NULL == val)) {
		errno = EINVAL;
		goto fail;
	};

        if (regrw_get_info_from_h(h, offset, val)) {
		if (regrw_rare_rd(h, i->dest_id, i->hc, offset, val)) {
			goto fail;
		} else {
			return regrw_update_h_info(h, offset, *val);
		};
        };
	return 0;
fail:
        return -1;
};

int regrw_rare_rd(regrw_h h, uint32_t did, rio_hc_t hc,
						uint32_t offset, uint32_t *val)
{
	if ((NULL == h) || (NULL == val)) {
		errno = EINVAL;
		goto fail;
	};

	if (regrw_raw_rd(h, did, hc, offset, val)) {
		goto fail;
	};

	return regrw_fixup_from_read(h, offset, val);
	
fail:
	return -1;
};

int regrw_raw_rd(regrw_h h, uint32_t did, rio_hc_t hc,
						uint32_t offset, uint32_t *val)
{
        struct regrw_i *i = H_TO_I(h);

	if ((NULL == h) || (NULL == val)) {
		errno = EINVAL;
		goto fail;
	};

	if (NULL == i->drv.rd) {
		errno = EINVAL;
		goto fail;
	};

	return i->drv.rd(h, did, hc, offset, val);
fail:
        return -1;
};

int regrw_wr(regrw_h h, uint32_t offset, uint32_t val)
{
	struct regrw_i *i = H_TO_I(h);

	if (NULL == h) {
		errno = EINVAL;
		goto fail;
	};

	if (regrw_rare_wr(h, i->dest_id, i->hc, offset, fixed_val)) {
		goto fail;
	};

	return regrw_update_h_info(h, offset, val);
fail:
	return -1;
};

int regrw_rare_wr(regrw_h h, uint32_t did, rio_hc_t hc,
		uint32_t addr, uint32_t val)
{
	struct regrw_i *i = H_TO_I(h);
	uint32_t fixed_val = val;

	if (NULL == h) {
		errno = EINVAL;
		goto fail;
	};

	if (regrw_fixup_for_write(h, offset, &fixed_val)) {
		goto fail;
	};

	return regrw_raw_wr(h, did, hc, addr, fixed_val);
fail:
	return -1;
};

int regrw_raw_wr(regrw_h h, uint32_t did, rio_hc_t hc,
		uint32_t addr, uint32_t val)
{
	struct regrw_i *i = H_TO_I(h);
	uint32_t fixed_val = val;

	if ((NULL == h) || (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};

	return i->drv.wr(h, did, hc, addr, val);
fail:
	return -1;
};

/* Device/Vendor name strings.
 */
int regrw_vend_name(regrw_h h, const char **name)
{
	struct regrw_i *i = H_TO_I(h);

	if (NULL == h) {
		*name = NULL;
		errno = EINVAL;
		return -1;
	};

	*name = i->vend_name;
	return 0;
};

int regrw_dev_t_name(regrw_h h, const char **name)
{
	struct regrw_i *i = H_TO_I(h);

	if (NULL == h) {
		*name = NULL;
		errno = EINVAL;
		return -1;
	};

	*name = i->dev_t_name;
	return 0;
};

/* Write and read destination ID register.
 */

int regrw_write_destid(regrw_h h, regrw_tt_t tt, uint32_t dest_id)
{
	struct regrw_i *i = H_TO_I(h);
	uint32_t offset = RIO_DEVID;
	uint32_t value = 0xFFFFFFFF;

	if ((NULL == h) || (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};

	if (RIO_PE_FEAT_SW & i->pe_feat) {
		errno = ENOSYS;
		goto fail;
	};
	
	switch (tt) {
	case regrw_tt_dev8:
		value = GET_CHG_DEV8(destid);
		break;
	case regrw_tt_dev16:
		if (!(RIO_PE_FEAT_DEV16 & i->pe_feat)) {
			errno = ENOSYS;
			goto fail;
		};
		value = GET_CHG_DEV16(destid);
		break;
	case regrw_tt_dev32:
		if (!(RIO_PE_FEAT_DEV32 & i->pe_feat)) {
			errno = ENOSYS;
			goto fail;
		};
		offset = RIO_DEV32;
		value = destid;
		break;
	default: 
		errno = EINVAL;
		goto fail;
	};

	return i->drv.wr(h, offset, value);
fail:
	return -1;
		
};

int regrw_read_destids(regrw_h h)
{
	struct regrw_i *i = H_TO_I(h);
	int rc;

	if ((NULL == h) || (NULL == i->drv.raw_rd)) {
		errno = EINVAL;
		goto fail;
	};
	
	if (RIO_PE_FEAT_SW & i->pe_feat) {
		errno = ENOSYS;
		goto fail;
	};
	
	if (i->drv.raw_rd(h, RIO_DEVID, &i->devid)) {
		goto fail;
	};

	if (RIO_PE_FEAT_DEV32 & i->pe_feat) {
		if (i->drv.raw_rd(h, RIO_DEV32, &i->dev32)) {
			goto fail;
		};
	};
	return 0;
fail:
	return -1;
};

int regrw_write_comptag(regrw_h h, uint32_t ct)
{
	struct regrw_i *i = H_TO_I(h);
	int rc;

	if ((NULL == h) || (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};
	
	if (i->drv.wr(h, RIO_COMPTAG, ct)) {
		goto fail;
	};

	return regrw_read_comptag(h);
fail:
	return -1;
};

int regrw_read_comptag(regrw_h h)
{
	struct regrw_i *i = H_TO_I(h);
	int rc;

	errno = 0;
	if ((NULL == h) || (NULL == i->drv.raw_rd)) {
		errno = EINVAL;
		goto fail;
	};
	
	if (i->drv.raw_rd(h, RIO_COMPTAG, &i->comptag)) {
		goto fail;
	};

	return 0;
fail:
	return -1;
};

int regrw_lock(regrw_h h, uint32_t *lck_val);
{
	struct regrw_i *i = H_TO_I(h);
	uint32_t mask = RIO_HOST_LOCK_DEVID;
	uint32_t value = *lck_val;

	errno = 0;

	if ((NULL == h) || (RIO_HOST_LOCK_UNLOCKED == value)) {
		errno = EINVAL;
		goto fail;
	};
	
	if ((NULL == i->drv.raw_wr) || (NULL == i->drv.raw_rd)) {
		errno = EINVAL;
		goto fail;
	};

	if (DEV32_SUPP(h)) {
		mask = RIO_HOST_LOCK_DEVID32;
	};
	
	value &= mask;

	if (i->drv.raw_wr(h, RIO_HOST_LOCK, value)) {
		goto fail;
	};

	if (i->drv.raw_rd(h, RIO_HOST_LOCK, &i->host_lock)) {
		goto fail;
	};

	if (i->host_lock != value) {
		*lck_val = i->host_lock;
		errno = ENOLCK;
		goto fail;
	};
	return 0;
fail:
	return -1;
};

int regrw_unlock(regrw_h h, uint32_t *unlck_val);
{
	rc = regrw_lock(h, unlock_val);

	if (rc && (ENOLCK == errno) && (RIO_HOST_LOCK_UNLOCKED == *unlck_val)) {
		errno = 0;
		return 0;
	};

	return rc;
};

int regrw_set_addrsz(regrw_h h, RIO_PE_ADDR_T addrsz )
{
	struct regrw_i *i = H_TO_I(h);

	errno = 0;

	if ((NULL == h) (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};

	addrsz &= RIO_PE_LL_CTL_ADDRSZ;
	
	if ((RIO_PE_LL_CTL_34BIT != addrsz) && (RIO_PE_LL_CTL_50BIT != addrsz)
	&& (RIO_PE_LL_CTL_66BIT != addrsz)) {
		errno = EINVAL;
		goto fail;
	};

	if (!(i->pe_feat & addrsz)) {
		errno = ENOSYS;
		goto fail;
	};

	return i->drv.wr(h, RIO_PE_LL_CTL, addrsz);
fail:
	return -1;
};

int get_port_range(regrw_h h, rio_port_t p,
					rio_port_t *st_pt, rio_port_t *end_pt)
{
	*st_pt = *end_pt = -1;

	if (RIO_ALL_PORTS == p) {
		st_pt = 0;
		end_pt = PE_PORT_COUNT(h);
	} else {
		if (PE_PHYS_PORT(h,p)) {
			*st_pt = *end_pt = p;
		} else {
			errno = EINVAL;
			goto fail;
		};
	};
	return 0;
fail:
	return -1;
};


int regrw_set_enumb(regrw_h h, rio_port_t p, bool enumb)
{
	struct regrw_i *i = H_TO_I(h);
	uint32_t offset;
	rio_port_t spx, st_pt, end_pt;

	errno = 0;

	if ((NULL == h) || (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};

	if (get_port_range(h, p, &st_pt, &end_pt)) {
		goto fail;
	};

	for (spx = st_pt; spx <= end_pt; spx++) {
		offset = RIO_SPX_CTL(SPX_BASE(i), SPX_TYPE(i), spx);
		if (RIO_BAD_OFFSET == offset) {
			errno = EINVAL;
			goto fail;
		};

		if (enumb) {
			i->ctl1[spx] |= RIO_SPX_CTL_ENUMB;
		} else {
			i->ctl1[spx] &= ~RIO_SPX_CTL_ENUMB;
		};

		if (i->drv.raw_wr(h, offset, i->ctl1[spx])) {
			goto fail;
		};
	};
fail:
	return -1;
};

int regrw_get_errstat(regrw_h h, rio_port_t p, RIO_SPX_ERR_STAT_T *e_stat)
{
	struct regrw_i *i = H_TO_I(h);
	uint32_t offset;
	uint32_t temp;

	if ((NULL == h) || (NULL == i->drv.rd) || (NULL == e_stat)) {
                errno = EINVAL;
                goto fail;
        };
	
	if (!PE_PHYS_PORT(h,p)) {
		errno = EINVAL;
		goto fail;
	};

	offset = RIO_SPX_ERR_STAT(SPX_BASE(i), SPX_TYPE(i), p);
	if (RIO_BAD_OFFSET == offset) {
		errno = EINVAL;
		goto fail;
	};

	if (i->drv.rd(h, offset, &temp)) {
		goto fail;
	};
	
	*e_stat = temp;
	return 0;
fail:
	return -1;
};

int regrw_lreq_n_resp(regrw_h h, rio_port_t p, RIO_SPX_LM_RESP_STAT_T *lresp)
{
	struct regrw_i *i = H_TO_I(h);
        uint32_t req_offset, resp_offset;
        uint32_t status;
	uint64_t delay_usec = RIO_SP_TO_NSEC_LIM(RIO_SP_LTO_NSEC(i->lto))/1000;

        if ((NULL == h) || (NULL == i->drv.rd) || (NULL == lresp)) {
                errno = EINVAL;
                goto fail;
        };

        if ((NULL == i->drv.rd) || (NULL == i->drv.wr) || (NULL == i->drv.dly))
	{
                errno = EINVAL;
                goto fail;
        };

        if (!PE_PHYS_PORT(h,p)) {
                errno = EINVAL;
                goto fail;
        };
	
	req_offset = RIO_SPX_LM_REQ(SPX_BASE(i), SPX_TYPE(i), p);
	resp_offset = RIO_SPX_LM_RESP(SPX_BASE(i), SPX_TYPE(i), p);

	if ((RIO_BAD_OFFSET == req_offset) || (RIO_BAD_OFFSET == req_offset)) {
		errno = EINVAL;
		goto fail;
	};

	if (i->drv.rd(h, resp_offset, &status)) {
		goto fail;
	};

	/* If can't confirm that previously sent transaction is complete,
	* wait until it must be complete before proceeding.
	*/
	if (!RIO_SPX_LM_RESP_IS_VALID(status)) {
		i->drv.dly(delay_usec);
	};

	if (i->drv.wr(h, req_offset, RIO_SPX_LM_REQ_CMD_LR_PS)) {
		goto fail;
	};

	i->drv.dly(delay_usec);

	if (i->drv.rd(h, resp_offset, &status)) {
		goto fail;
	};
	
	*lresp = status;

	return 0;
fail:
	return -1;
};

int regrw_port_enable(regrw_h h, rio_port_t p,
		bool port_enable, bool port_lkout, bool in_out_en)
{
	struct regrw_i *i = H_TO_I(h);
        uint32_t req_offset, resp_offset;
        uint32_t ctl1;
	uint64_t delay_usec = RIO_SP_TO_NSEC_LIM(RIO_SP_LTO_NSEC(i->lto))/1000;
	rio_port_t spx, st_pt, end_pt;

	errno = 0;

	if ((NULL == h) || (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};

	if (get_port_range(h, p, &st_pt, &end_pt)) {
		goto fail;
	};

	for (spx = st_pt; spx <= end_pt; spx++) {
		ctl1 = i->ctl1[pt];

		if (port_enable) {
			ctl1 &= ~RIO_SPX_CTL_PORT_DIS;
		} else {
			ctl1 |= RIO_SPX_CTL_PORT_DIS;
		};
	
		if (port_lkout) {
			ctl1 |=  RIO_SPX_CTL_LOCKOUT;
		} else {
			ctl1 &= ~RIO_SPX_CTL_LOCKOUT;
		}

		if (in_out_en) {
			ctl1 |= RIO_SPX_CTL_INP_EN | RIO_SPX_CTL_OTP_EN ;
		} else {
			ctl1 &= ~(RIO_SPX_CTL_INP_EN | RIO_SPX_CTL_OTP_EN);
		};

		if (crtl1 == i->ctl1[pt]) {
			continue;
		};

		offset = RIO_SPX_CTL(SPX_BASE(i), SPX_TYPE(i), spx);
		if (RIO_BAD_OFFSET == offset) {
			errno = EINVAL;
			goto fail;
		};

		if (i->drv.wr(h, offset, ctrl1)) {
			goto fail;
		};
	};
	
	return 0;
fail:
	return -1;
};

int regrw_emerg_lkout(regrw_h h, rio_port_t p)
{
	struct regrw_i *i = H_TO_I(h);
        uint32_t offset;
        uint32_t ctl1;

	errno = 0;

	if ((NULL == h) || (NULL == i->drv.wr)) {
		errno = EINVAL;
		goto fail;
	};

	if (!PE_PHYS_PORT(h, p,)) {
		errno = EINVAL;
		goto fail;
	};

	offset = RIO_SPX_CTL(SPX_BASE(i), SPX_TYPE(i), p);
	if (RIO_BAD_OFFSET == offset) {
		errno = EINVAL;
		goto fail;
	};

	if (i->drv.wr(h, offset, i->ctl1[p] | RIO_SPX_CTL_LOCKOUT)) {
		goto fail;
	};

	return 0;
fail:
	return -1;
};

int regrw_wr_rte(regrw_h h, rio_port_t p,
                regrw_tt_t tt, uint32_t did, pe_rt_val rte)
{
	errno = ENOSYS;
	return -1;
};

int regrw_rd_rte(regrw_h h, rio_port_t p, regrw_tt_t tt, uint32_t did,
                                                                pe_rt_val *rte)
{
	errno = ENOSYS;
	return -1;
};

int regrw_init_rte(regrw_h h, rio_port_t p, regrw_tt_t tt, pe_rt_val *rte)
{
	errno = ENOSYS;
	return -1;
};

int regrw_wr_dflt_rte(regrw_h h, pe_rt_val *rte)
{
	errno = ENOSYS;
	return -1;
};

/* To override functions above, pass in structure with new function.
 * If the existing function should be unchanged, pass in NULL.
 */
int regrw_override_h_drvr(regrw_h *h, struct regrw_driver *drv);
{
	struct regrw_i *i;

	if ((NULL == h) || (NULL == drv)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	i = H_TO_I(h);
	
	if (drv->rd) {
		i->drv.rd = drv->rd;
	};

	if (drv->wr) {
		i->drv.wr = drv->wr;
	};

	if (drv->raw_rd) {
		i->drv.raw_rd = drv->raw_rd;
	};

	if (drv->raw_wr) {
		i->drv.raw_wr = drv->raw_wr;
	};

	if (drv->drv_data) {
		i->drv.drv_data = drv->drv_data;
	};

	return 0;
};
	
int regrw_override_regrw_drv(struct regrw_driver *drv);
{
	if (NULL == drv) {
		return EINVAL;
	};

	if (drv->rd) {
		regrw_dflt_drv.rd = drv->rd;
	};

	if (drv->wr) {
		regrw_dflt_drv.wr = drv->wr;
	};

	if (drv->raw_rd) {
		regrw_dflt_drv.raw_rd = drv->raw_rd;
	};

	if (drv->raw_wr) {
		regrw_dflt_drv.raw_wr = drv->raw_wr;
	};

	if (drv->drv_data) {
		regrw_dflt_drv.drv_data = drv->drv_data;
	};

	return 0;
};

#ifdef __cplusplus
}
#endif
