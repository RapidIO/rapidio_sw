/*
 * Default, mport based, register read/write functions
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
#include "regrw_log.h"
#include "rapidio_mport_mgmt.h"
#include "linux.h"

#ifdef __cplusplus
extern "C" {
#endif

riomp_mport_t *mport_handle = NULL;
bool mport_init = false;

#define SZ32 sizeof(uint32_t)

struct regrw_driver regrw_dflt_drv = 
{
	mport_reg_rd,
	mport_reg_wr,
	regrw_delay,
	NULL
};

int mport_init_driver(void) {
	uint32_t *mp_list;
	uint8_t mp_cnt = 1;
	uint8_t mp_num;
	int ret;

	if (mport_init)
		return 0;

        /** - request from driver list of available local mport devices */
        if (riomp_mgmt_get_mport_list(&mp_list, &mp_cnt)) {
		goto fail;
	};

	if (!mp_cnt) {
                errno = ENXIO;
		goto fail;
	};

	mp_num = mp_list[0];

	if (riomp_mgmt_free_mport_list(&mp_list)) {
		goto fail;
	};

	mport_handle = (riomp_mport_t *)calloc(1, sizeof(riomp_mport_t *));
        if (riomp_mgmt_mport_create_handle(mp_num, 0, mport_handle)) {
		goto fail;
	};

	regrw_dflt_drv.rd = mport_reg_rd;
	regrw_dflt_drv.wr = mport_reg_wr;
	regrw_dflt_drv.dly = mport_delay;
	regrw_dflt_drv.drv_data = (void *)mport_handle;
	mport_init = true;
	return 0;
}

int mport_preamble(regrw_h *h, riomp_mport_t **mph)
{
	regrw_i *i; 

	if (mport_init_driver()) {
		goto fail;
	};

	if (NULL == h) {
		errno = EINVAL;
		goto fail;
	};

	i = H_TO_I(h);
	*mph = (riomp_mport_t *)i->drv.drv_data;
	if (NULL == *mph) {
		errno = EINVAL;
		goto fail;
	};
	return 0;
fail:
	return -1;
};
	
int mport_reg_rd(regrw_h *h, uint32_t did, uint16_t hc, uint32_t addr,
								uint32_t *val)
{
	riomp_mport_t *mph;
	uint32_t temp;

	if (mport_preamble(h, &mph)) {
		goto fail;
	};

	if (HC_LOCAL == hc) {
 		if (riomp_mgmt_lcfg_read(*mph, offset, SZ32, &temp)) {
			goto fail;
		};
	} else {
 		if (riomp_mgmt_rcfg_read(*mph, did, hc, offset, SZ32, &temp)) {
			goto fail;
		};
	};

	*val = temp;

	return 0;
fail:
	*val = 0xDEADBEEF;
	return -1;
};

int mport_reg_wr(regrw_h *h, uint32_t did, uint16_t hc,
						uint32_t addr, uint32_t val)
{
	riomp_mport_t *mph;
	uint32_t temp;

	if (mport_preamble(h, &mph)) {
		goto fail;
	};

	if (HC_LOCAL == hc) {
 		if (riomp_mgmt_lcfg_write(*mph, offset, SZ32, &temp)) {
			goto fail;
		};
	} else {
 		if (riomp_mgmt_rcfg_write(*mph, did, hc, offset, SZ32, val)) {
			goto fail;
		};
	};

	return 0;
fail:
	return -1;
};

#ifdef __cplusplus
}
#endif
