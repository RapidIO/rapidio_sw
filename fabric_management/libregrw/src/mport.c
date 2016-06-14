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
#include "rio_car_csr.h"
#include "regrw.h"
#include "regrw_log.h"
#include "rapidio_mport_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

bool mport_init = 0;

riomp_mport_t mport_handle;

#define INIT_MPORT if (mport_init_driver()) \
			return ENXIO;

#define CHECK_RCC(r,m)  \
if (NULL == r) \
	return EINVAL; \
m = (riomp_mport_t *)r->regrw.drv_data; \
if (NULL == m ) \
	return EINVAL;


#define MAX_MPORTS 10

#define SZ32 sizeof(uint32_t)

int mport_init_driver(void);

int mport_reg_rd(struct rio_car_csr *rcc, uint32_t offset, uint32_t *val)
{
	riomp_mport_t *mph = (riomp_mport_t *)rcc->regrw.drv_data;
	uint32_t temp;
	int rc = 0;

	INIT_MPORT
	CHECK_RCC(rcc, mph)

	if (HC_LOCAL == rcc->hc)
 		rc = riomp_mgmt_lcfg_read(*mph, offset, SZ32, &temp);
	else
 		rc = riomp_mgmt_rcfg_read(*mph, rcc->dest_id, rcc->hc,
						offset, SZ32, &temp);
	if (rc)
		temp = 0xDEADBEEF;
	else
		*val = temp;

	if (HC_LOCAL == rcc->hc) {
		INFO("L R 0x%x @ 0x%x", offset, temp);
	} else {
		INFO("R R 0x%x @ 0x%x @ %d %d",
			offset, temp, rcc->dest_id, rcc->hc);
	};

	return rc;
};

int mport_reg_wr(struct rio_car_csr *rcc, uint32_t offset, uint32_t val)
{
	riomp_mport_t *mph;

	INIT_MPORT
	CHECK_RCC(rcc, mph)

	if (HC_LOCAL == rcc->hc) {
		INFO("L W 0x%x @ 0x%x", offset, val);
 		return riomp_mgmt_lcfg_write(*mph, offset, SZ32, val);
	} else {
		INFO("R W 0x%x @ 0x%x @ %d %d", offset, val,
						rcc->dest_id, rcc->hc);
 		return riomp_mgmt_rcfg_write(*mph,
				rcc->dest_id, rcc->hc, offset, SZ32, val);
	};
};

int mport_raw_reg_rd(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
		uint32_t addr, uint32_t *val)
{
	riomp_mport_t *mph;
	uint32_t temp;
	int rc = 0;

	INIT_MPORT
	CHECK_RCC(rcc, mph)

	if (HC_LOCAL == hc)
 		rc = riomp_mgmt_lcfg_read(*mph, addr, SZ32, &temp);
	else
 		rc = riomp_mgmt_rcfg_read(*mph, did, hc, addr, SZ32, &temp);

	if (rc)
		temp = 0xDEADBEEF;
	else
		*val = temp;

	if (HC_LOCAL == hc) {
		INFO("L R 0x%x @ 0x%x", addr, temp);
	} else {
		INFO("R R 0x%x @ 0x%x @ %d %d", addr, temp, did, hc);
	};

	return rc;

};

int mport_raw_reg_wr(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
		uint32_t addr, uint32_t val)
{
	riomp_mport_t *mph = (riomp_mport_t *)rcc->regrw.drv_data;

	INIT_MPORT
	CHECK_RCC(rcc, mph)

	if (HC_LOCAL == hc) {
		INFO("L W 0x%x @ 0x%x", addr, val);
 		return riomp_mgmt_lcfg_write(*mph, addr, SZ32, val);
	} else {
		INFO("R W 0x%x @ 0x%x @ %d %d", addr, val, did, hc);
 		return riomp_mgmt_rcfg_write(*mph, did, hc, addr, SZ32, val);
	};
};

struct regrw_driver regrw_dflt_drv = 
{
	mport_reg_rd,
	mport_reg_wr,
	mport_raw_reg_rd,
	mport_raw_reg_wr,
	(uint64_t)NULL
};

int mport_init_driver(void) {
	uint32_t *mp_list;
	uint8_t mp_cnt = MAX_MPORTS;
	uint8_t mp_num;
	int ret;

	if (mport_init)
		return 0;

        /** - request from driver list of available local mport devices */
        ret = riomp_mgmt_get_mport_list(&mp_list, &mp_cnt);
        if (ret)
                return ENXIO;

	if (!mp_cnt)
		return ENXIO;

	mp_num = mp_list[0];

	ret = riomp_mgmt_free_mport_list(&mp_list);
        if (ret < 0) 
		return ENOMEDIUM;

	INFO("Openning Mport %d", mp_num);

        ret = riomp_mgmt_mport_create_handle(mp_num, 0, &mport_handle);
        if (ret < 0) 
		return ENOENT;

	regrw_dflt_drv.reg_rd = mport_reg_rd;
	regrw_dflt_drv.reg_wr = mport_reg_wr;
	regrw_dflt_drv.raw_reg_rd = mport_raw_reg_rd;
	regrw_dflt_drv.raw_reg_wr = mport_raw_reg_wr;
	regrw_dflt_drv.drv_data = (uint64_t)&mport_handle;
	mport_init = true;
	return 0;
}



#ifdef __cplusplus
}
#endif
