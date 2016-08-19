/*
 * Query.c: Quick "macro" tests of register values.
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

uint32_t regrw_dev_vendor(regrw_h h)
{
	return GET_DEV_VENDOR(h);
};

uint32_t regrw_dev_ident(regrw_h h)
{
	return GET_DEV_IDENT(h);
};

uint32_t regrw_assy_vendor(regrw_h h)
{
	return GET_ASSY_VENDOR(h);
};

uint32_t regrw_assy_ident(regrw_h h)
{
	return GET_ASSY_IDENT(h);
};

uint32_t regrw_assy_version(regrw_h h)
{
	return GET_ASSY_VERSION(h);
};

uint32_t regrw_dev8(regrw_h h)
{
	return GET_GET_DEV8(h);
};

uint32_t regrw_new_dev8(regrw_h h,uint8_t dev8)
{
	return GET_CHG_DEV8(h, dev8);
};

uint32_t regrw_dev16(regrw_h h)
{
	if (H_TO_I(h)->cregs.pe_feat & RIO_PE_FEAT_DEV16) {
		return GET_GET_DEV16(h);
	};
	return 0;
};

uint32_t regrw_new_dev16(regrw_h h, uint16_t dev16)
{
	if (H_TO_I(h)->cregs.pe_feat & RIO_PE_FEAT_DEV16) {
		return GET_CHG_DEV16(h, dev16);
	};
	return 0;
};

uint32_t regrw_dev32(regrw_h h)
{
	if (H_TO_I(h)->cregs.pe_feat & RIO_PE_FEAT_DEV32) {
		return GET_GET_DEV32(h);
	};
	return 0;
};
uint32_t regrw_ct(regrw_h h)
{
	return GET_CT(h);
};

uint32_t regrw_scratch(regrw_h h)
{
	return GET_CT(h);
};

uint32_t regrw_ext_feat_oset(regrw_h h)
{
	return GET_EXT_FEAT_OSET(h);
};

uint32_t regrw_enumb(regrw_h h, rio_port_t p)
{
	return GET_ENUMB(h, p);
};

RIO_PE_ADDR_T regrw_addrsz_supp(regrw_h h)
{
	return RIO_ADDRSZ_SUPP(h);
};

bool regrw_addr34_supp(regrw_h h)
{
	return RIO_ADDR34_SUPP(h);
};

bool regrw_addr50_supp(regrw_h h)
{
	return RIO_ADDR50_SUPP(h);
};

bool regrw_addr66_supp(regrw_h h)
{
	return RIO_ADDR66_SUPP(h);
};

RIO_PE_ADDR_T regrw_addrsz_now(regrw_h h)
{
	return RIO_ADDRSZ_NOW(h);
};

bool regrw_addr34_now(regrw_h h)
{
	return RIO_ADDR34_NOW(h);
};

bool regrw_addr50_now(regrw_h h)
{
	return RIO_ADDR50_NOW(h);
};

bool regrw_addr66_now(regrw_h h)
{
	return RIO_ADDR66_NOW(h);
};

/* Check for processing element features */
bool regrw_std_rte(regrw_h h)
{
	return RIO_STD_RTE(h);
};

bool regrw_ext_rte(regrw_h h)
{
	return RIO_EXT_RTE(h);
};

uint32_t regrw_is_MULTP(regrw_h h)
{
	return PE_IS_MULTP(h);
};

uint32_t regrw_is_SW(regrw_h h)
{
	return PE_IS_SW(h);
};

uint32_t regrw_is_PROC(regrw_h h)
{
	return PE_IS_PROC(h);
};

uint32_t regrw_is_MEM(regrw_h h)
{
	return PE_IS_MEM(h);
};

uint32_t regrw_is_BRIDGE(regrw_h h)
{
	return PE_IS_BRIDGE(h);
};

rio_port_t regrw_port_count(regrw_h h)
{
	return PE_PORT_COUNT(h);
};

rio_port_t regrw_sw_acc_port(regrw_h h)
{
	return RIO_SW_ACC_PORT(h);
};

#ifdef __cplusplus
}
#endif
