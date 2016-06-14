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

#ifndef __REGRW_H__
#define __REGRW_H__

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HC_LOCAL 0x100

int reg_rd(struct rio_car_csr *rcc, uint32_t offset, uint32_t *val);
int reg_wr(struct rio_car_csr *rcc, uint32_t offset, uint32_t val);
int raw_reg_rd(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
		uint32_t addr, uint32_t *val);
int raw_reg_wr(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
		uint32_t addr, uint32_t val);
int init_rcc_driver(struct rio_car_csr *rcc);
int override_rcc_drvr(struct rio_car_csr *rcc, struct regrw_driver *drv);

struct regrw_driver {
        int (* reg_rd)(struct rio_car_csr *rcc,
                        uint32_t offset, uint32_t *val);
        int (* reg_wr)(struct rio_car_csr *rcc,
			uint32_t offset, uint32_t val);
	int (* raw_reg_rd)(struct rio_car_csr *rcc,
                        uint32_t did, uint16_t hc,
                        uint32_t offset, uint32_t *val);
        int (* raw_reg_wr)(struct rio_car_csr *rcc,
                        uint32_t did, uint16_t hc,
                        uint32_t offset, uint32_t val);
	uint64_t drv_data;
};

/* Initialize default register read/write functions to use specified MPORT
 * value,
 */
int override_regrw_drv(struct regrw_driver *drv);

/* RapidIO control plane logging facility */
int regrw_set_log_level(int level);
int regrw_get_log_level();

/* RapidIO Register Read/Write CLI commands */
void regrw_bind_cli_cmds(void);

#ifdef __cplusplus
}
#endif

#endif /* __REGRW_H__ */
