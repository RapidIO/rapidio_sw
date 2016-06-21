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
#include "rio_car_csr.h"
#include "regrw_log.h"

#ifdef __cplusplus
extern "C" {
#endif

struct regrw_driver dflt_driver;

#ifndef REGRW_USE_MALLOC
struct regrw_i regrw_hndl[REGRW_NUM_HANDLES];
struct regrw_sw regrw_sw_hndl[REGRW_NUM_SWITCHES];

bool hdnl_use[REGRW_NUM_HANDLES] = {false};
bool sw_use[REGRW_NUM_SWITCHES] = {false};
#endif

int regrw_get_handle(regrw_h *h)
{
	regrw_i *hnd;
	errno = 0;
#ifdef REGRW_USE_MALLOC
	hnd = (regrw_h)calloc(1, sizeof(struct regrw_i));
	if (NULL == hnd) {
		return -1;
	*h = (void *)hnd;
	memcpy((*h)->regrw, &dflt_driver, sizeof(struct regrw_driver));
	return 0;
#else
	bool found_one = false;
	*h = NULL;

	for (int srch_i = 0; srch_i < REGRW_NUM_HANDLES; srch_i++) {
		if (!hdnl_use[srch_i]) {
			hdnl_use[srch_i] = true;
			memset(&regrw_hndl[srch_i], 0, sizeof(struct regrw_i));
			memcpy(&regrw_hndl[srch_i], &dflt_driver,
						sizeof(struct regrw_driver));
			*h = &regrw_hndl[srch_i];
			found_one = true;
			break;
		};
	};
	if (found_one)
		return 0;
	
	errno = ENOMEM;
	return -1;
#endif;
};

int regrw_destroy_handle(regrw_h *h)
{
#ifdef REGRW_USE_MALLOC
	errno = 0;
	free(*h);
	*h = NULL;
	return 0;
#else
	bool found_one = false;
	uint idx;
	struct regrw_i *hndl = (struct regrw_i *)h;

	errno = 0;
	if (NULL == h)
		goto fail;

	if (NULL == *h)
		return 0;

 	idx = ((uint64_t)*h - (uint64_t)&regrw_hndl[0]) /
			sizeof(regrw_hndl[0]);
	if (idx >= REGRW_NUM_HANDLES) {
		goto fail;
	};

	if (hndl->sw_info) {
		uint sw_idx = (((uint64_t)hndl->sw_info -
				(uint64_t)&regrw_sw_hndl[0]) /
				sizeof(regrw_sw_hndl[0]);
		if (sw_idx >= REGRW_NUM_SWITCHES)
			goto fail;

		sw_use[sw_idx] = false;
		memset(&regrw_sw_hndl[sw_idx], 0, sizeof(regrw_sw_hndl[0]));
	};

	hndl_use[idx] = false;
	memset(&regrw_hndl[idx], 0, sizeof(regrw_hndl[0]));
		
	*h = NULL;

	return 0;
fail:
	errno = EINVAL;
	return -1;

#endif;
};

int regrw_init_handle(regrw_h h, uint32_t did, rio_hc_t hc)
{
	uint32_t vend_devi;
	int rc;
	struct regrw_i *hnd = (regrw_i *)h;

	errno = 0;
	if (NULL == hnd) {
		errno = EINVAL;
		goto fail;
	}

	if (regrw_raw_read(h, did, hc, RIO_DEV_IDENT, &vend_devi))
		goto fail;

	if (regrw_set_path(h, did, hc))
		goto fail;

/* FIXME: Allocate switch information here */

	if (regrw_fill_in_handle(hnd, vend_devi);
		goto fail;

	return 0;
fail:
	return -1;
};

int regrw_set_path(regrw_h h, uint32_t did, rio_hc_t hc);
{
	struct regrw_i *hnd = (regrw_i *)h;

	errno = 0;
	if ((NULL == hnd) || (hc > HC_LOCAL))
		goto fail;

	hnd->dest_id = did;
	hnd->tt = tt_dev8;
	hnd->hc = hc;

	return 0;
fail:
	errno = EINVAL;
	return -1;
};

int regrw_get_path(regrw_h h, uint32_t *did, rio_hc_t hc);
{
	struct regrw_i *hnd = (regrw_i *)h;

	errno = 0;
	if ((NULL == hnd) || (hnd->tt == tt_uninit) || (hnd->tt >= tt_last) ||
			(hnd->hc > HC_LOCAL))
		goto fail;

	*did = hnd->dest_id;
	*hc = hnd->hc;

	return 0;
fail:
	errno = EINVAL;
	return -1;
};


int regrw_vend_name(regrw_h h, const char **name)
{
	struct regrw_i *hnd = (regrw_i *)h;

	if (NULL == hnd) {
		*name = NULL;
		errno = EINVAL;
		return -1;
	};

	*name = h->vend_name;
	return 0;
};

int regrw_dev_t_name(regrw_h h, const char **name)
{
	struct regrw_i *hnd = (regrw_i *)h;

	if (NULL == hnd) {
		*name = NULL;
		errno = EINVAL;
		return -1;
	};

	*name = h->dev_t_name;
	return 0;
};

int regrw_rd(struct rio_car_csr *rcc, uint32_t offset, uint32_t *val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.reg_rd)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	return rcc->regrw.reg_rd(rcc, offset, val);
};

int regrw_wr(struct rio_car_csr *rcc, uint32_t offset, uint32_t val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.reg_wr)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	return rcc->regrw.reg_wr(rcc, offset, val);
};

int regrw_raw_rd(struct rio_car_csr *rcc, uint32_t did, rio_hc_t hc,
		uint32_t addr, uint32_t *val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.raw_reg_rd)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};
	return rcc->regrw.raw_reg_rd(rcc, did, hc, addr, val);
};

int regrw_raw_wr(struct rio_car_csr *rcc, uint32_t did, rio_hc_t hc,
		uint32_t addr, uint32_t val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.raw_reg_wr)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	return rcc->regrw.raw_reg_wr(rcc, did, hc, addr, val);
};

/* To override functions above, pass in structure with new function.
 * If the existing function should be unchanged, pass in NULL.
 */
extern struct regrw_driver regrw_dflt_drv;

int init_rcc_driver(struct rio_car_csr *rcc)
{
	if (NULL == rcc) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	memcpy(&rcc->regrw, &regrw_dflt_drv, sizeof(struct regrw_driver));
	return 0;
};
	
int override_rcc_drvr(struct rio_car_csr *rcc, struct regrw_driver *drv)
{
	if ((NULL == rcc) || (NULL == drv)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	memcpy(&rcc->regrw, &drv, sizeof(struct regrw_driver));
	return 0;
};

int override_regrw_drv(struct regrw_driver *drv)
{
	if (NULL == drv) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	memcpy(&regrw_dflt_drv, drv, sizeof(struct regrw_driver));
	return 0;
};
#ifdef __cplusplus
}
#endif
