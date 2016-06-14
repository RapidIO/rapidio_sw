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

int reg_rd(struct rio_car_csr *rcc, uint32_t offset, uint32_t *val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.reg_rd)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	return rcc->regrw.reg_rd(rcc, offset, val);
};

int reg_wr(struct rio_car_csr *rcc, uint32_t offset, uint32_t val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.reg_wr)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};

	return rcc->regrw.reg_wr(rcc, offset, val);
};

int raw_reg_rd(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
		uint32_t addr, uint32_t *val)
{
	if ((NULL == rcc) || (NULL == rcc->regrw.raw_reg_rd)) {
		ERR("Driver is NULL!");
		return EINVAL;
	};
	return rcc->regrw.raw_reg_rd(rcc, did, hc, addr, val);
};

int raw_reg_wr(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
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
