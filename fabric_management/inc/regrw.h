/* Register read/write interface
 */
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
#include "rio_standard.h"
#include "rio_ecosystem.h"

#ifndef __REGRW_H__
#define __REGRW_H__


#ifdef __cplusplus
extern "C" {
#endif

int reg_rd(struct rio_car_csr *rcc, uint32_t offset, uint32_t *val);
int reg_wr(struct rio_car_csr *rcc, uint32_t offset, uint32_t val);
int raw_reg_rd(struct rio_car_csr *rcc, tt_t tt, uint32_t did, uint8_t hc,
		uint32_t addr, uint32_t *val);
int raw_reg_wr(struct rio_car_csr *rcc, tt_t tt, uint32_t did, uint8_t hc,
		uint32_t addr, uint32_t val);

struct riocp_reg_rw_driver {
        int (* reg_rd)(struct rio_car_csr *rcc,
                        uint32_t offset, uint32_t *val);
        int (* reg_wr)(struct rio_car_csr *rcc,
                        uint32_t offset, uint32_t val);
        int (* raw_reg_rd)(struct rio_car_csr *rcc,
                        tt_t tt, uint32_t did, uint8_t hc,
                        uint32_t offset, uint32_t *val);
        int (* raw_reg_wr)(struct rio_car_csr *rcc,
                        tt_t tt, uint32_t did, uint8_t hc,
                        uint32_t offset, uint32_t val);
};

/* To override functions above, pass in structure with new function.
 * If the existing function should be unchanged, pass in NULL.
 */
int override_regrw(struct regwr_drv *drv);

#ifdef __cplusplus
}
#endif

#endif /* __REGRW_H__ */
