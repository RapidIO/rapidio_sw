/*
 * Unit test register read/write functions
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
#include "regrw.h"
#include "regrw_log.h"
#include "rapidio_mport_mgmt.h"
#include "regrw_mport.h"

#ifdef __cplusplus
extern "C" {
#endif

int test_reg_rd(struct rio_car_csr *rcc, uint32_t offset, uint32_t *val)
{
	rcc->hc = offset;
	*val = offset;
	return 0; 
};

int test_reg_wr(struct rio_car_csr *rcc, uint32_t offset, uint32_t val)
{
	rcc->hc = offset;
	rcc->dest_id = val;
	return 0; 
};

int test_raw_reg_rd(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
                uint32_t addr, uint32_t *val)
{
	rcc->hc = hc;
	rcc->dest_id = did;
	*val = addr;
	return 0; 
};

int test_raw_reg_wr(struct rio_car_csr *rcc, uint32_t did, uint16_t hc,
                uint32_t addr, uint32_t val)
{
	rcc->dest_id = did;
	rcc->hc = hc;
	rcc->src_ops = addr;
	rcc->dst_ops = val;
	return 0;
};

/* Test initialization and override routines. */
int test_case_1(void) {
	struct regrw_driver test_rwdrv, saved_driver;
	struct rio_car_csr rcc;
	int rc;

	/* Verify parameter checking */
	if (EINVAL != init_rcc_driver(NULL))
		goto fail;

	/* Verify regrw default driver initialization */
	if (init_rcc_driver(&rcc))
		goto fail;

	if (mport_reg_rd != rcc.regrw.reg_rd)
		goto fail;
	if (mport_reg_wr != rcc.regrw.reg_wr)
		goto fail;
	if (mport_raw_reg_rd != rcc.regrw.raw_reg_rd)
		goto fail;
	if (mport_raw_reg_wr != rcc.regrw.raw_reg_wr)
		goto fail;
	if ((uint64_t)NULL != rcc.regrw.drv_data)
		goto fail;

	memcpy(&saved_driver, &regrw_dflt_drv, sizeof(struct regrw_driver));

	/* Verify regrw driver override */
	test_rwdrv.reg_rd = test_reg_rd;
	test_rwdrv.reg_wr = test_reg_wr;
	test_rwdrv.raw_reg_rd = test_raw_reg_rd;
	test_rwdrv.raw_reg_wr = test_raw_reg_wr;
	test_rwdrv.drv_data = 0x123456789ABCDEF0;
	
	rc = override_regrw_drv(NULL);
	if (EINVAL != rc)
		goto fail;

	rc = override_regrw_drv(&test_rwdrv);
	if (EINVAL != rc)
		goto fail;

	if (regrw_dflt_drv.reg_rd != test_rwdrv.reg_rd)
		goto fail;
	
	if (regrw_dflt_drv.reg_wr != test_rwdrv.reg_wr)
		goto fail;
	
	if (regrw_dflt_drv.raw_reg_rd != test_rwdrv.raw_reg_rd)
		goto fail;
	
	if (regrw_dflt_drv.raw_reg_wr != test_rwdrv.raw_reg_wr)
		goto fail;
	
	if (regrw_dflt_drv.drv_data != test_rwdrv.drv_data)
		goto fail;

	/* Verify rcc criver override */
	if (EINVAL != override_rcc_drvr(NULL, &test_rwdrv))
		goto fail;

	if (EINVAL != override_rcc_drvr(&rcc, NULL))
		goto fail;

	if (override_rcc_drvr(&rcc, &test_rwdrv))
		goto fail;

	if (rcc.regrw.reg_rd != test_rwdrv.reg_rd)
		goto fail;
	
	if (rcc.regrw.reg_wr != test_rwdrv.reg_wr)
		goto fail;
	
	if (rcc.regrw.raw_reg_rd != test_rwdrv.raw_reg_rd)
		goto fail;
	
	if (rcc.regrw.raw_reg_wr != test_rwdrv.raw_reg_wr)
		goto fail;
	
	if (rcc.regrw.drv_data != test_rwdrv.drv_data)
		goto fail;

	memcpy(&regrw_dflt_drv, &saved_driver, sizeof(struct regrw_driver));

	return 0;
fail:
	return 1;
};
	
/* Test logging interface */
int test_case_2(void) {
	int i;

	for (i = REGRW_LL_NONE; i <= REGRW_LL_TRACE; i++) {
		if (i != regrw_set_log_level(i))
			goto fail;
		if (i != regrw_get_log_level())
			goto fail;
		if (regrw_log_level != i)
			goto fail;
	};

	if (0 != regrw_set_log_level(-1))
		goto fail;
	if (regrw_log_level != 0)
		goto fail;
	if (0 != regrw_get_log_level())
		goto fail;

	if (REGRW_LL_TRACE != regrw_set_log_level(REGRW_LL_TRACE+1))
		goto fail;
	if (regrw_log_level != REGRW_LL_TRACE)
		goto fail;
	if (REGRW_LL_TRACE != regrw_get_log_level())
		goto fail;

	return 0;
fail:
	return 1;
};

/* Default MPORT driver test.
 * Test reads/writes local and remote, raw and cooked
 */

#define BAD_FOOD 0xBAD0F00D
#define FOOD_FAD 0xF00D0FAD

int test_case_3(void) {
	struct rio_car_csr rcc;
	uint32_t loc_vals[4] = {BAD_FOOD};
	uint32_t rem_vals[4] = {FOOD_FAD};
	uint32_t start_addr = 0x000000;
	uint32_t end_addr = 0xFFFFFC;

	/* Verify regrw default driver initialization */
	if (init_rcc_driver(&rcc))
		goto fail;

	rcc.dest_id = 0x1234;
	rcc.tt = (tt_t)1;
	rcc.hc = HC_LOCAL;

	/* Verify parameter checking */
	if (EINVAL != reg_rd(NULL, 0, &loc_vals[0]))
		goto fail;

	if (EINVAL != reg_rd(&rcc, 0, NULL))
		goto fail;

	if (EINVAL != raw_reg_rd(NULL, 0, 0, 0, &loc_vals[0]))
		goto fail;

	if (EINVAL != raw_reg_rd(&rcc, 0, 0, 0, NULL))
		goto fail;

	if (EINVAL != reg_wr(NULL, 0, loc_vals[0]))
		goto fail;

	if (EINVAL != raw_reg_wr(NULL, 0, 0, 0, loc_vals[0]))
		goto fail;

	/* Verify local access */
	if (reg_rd(&rcc, start_addr, &loc_vals[0]))
		goto fail;
	if (BAD_FOOD == loc_vals[0])
		goto fail;

	if (reg_rd(&rcc, end_addr, &loc_vals[1]))
		goto fail;
	if (BAD_FOOD == loc_vals[1])
		goto fail;

	if (reg_wr(&rcc, start_addr, loc_vals[0]))
		goto fail;

	if (reg_wr(&rcc, end_addr, loc_vals[1]))
		goto fail;

	if (raw_reg_rd(&rcc, 0, HC_LOCAL, start_addr, &loc_vals[2]))
		goto fail;
	if (FOOD_FAD == loc_vals[2])
		goto fail;

	if (raw_reg_rd(&rcc, 0, HC_LOCAL, end_addr, &loc_vals[3]))
		goto fail;
	if (0 != loc_vals[3])
		goto fail;

	if (raw_reg_wr(&rcc, 0, HC_LOCAL, start_addr, loc_vals[2]))
		goto fail;

	if (raw_reg_wr(&rcc, 0, HC_LOCAL, end_addr, loc_vals[3]))
		goto fail;

	/* Verify maintenance access to link partner */
	rcc.hc = 0;
	rcc.dest_id = 0x01;

	if (reg_rd(&rcc, start_addr, &rem_vals[0]))
		goto fail;
	if ((FOOD_FAD == rem_vals[0]) || (loc_vals[0] == rem_vals[0]))
		goto fail;

	if (reg_rd(&rcc, end_addr, &rem_vals[1]))
		goto fail;
	if (0 != rem_vals[1])
		goto fail;

	if (reg_wr(&rcc, start_addr, rem_vals[0]))
		goto fail;

	if (reg_wr(&rcc, end_addr, rem_vals[1]))
		goto fail;

	if (raw_reg_rd(&rcc, 0, HC_LOCAL, start_addr, &rem_vals[2]))
		goto fail;
	if ((FOOD_FAD == rem_vals[2]) || (loc_vals[2] == rem_vals[2]))
		goto fail;

	if (raw_reg_rd(&rcc, 0, HC_LOCAL, end_addr, &rem_vals[3]))
		goto fail;
	if (0 != rem_vals[3])
		goto fail;

	if (raw_reg_wr(&rcc, 0, HC_LOCAL, start_addr, rem_vals[2]))
		goto fail;

	if (raw_reg_wr(&rcc, 0, HC_LOCAL, end_addr, rem_vals[3]))
		goto fail;

	return 0;
fail:
	return 1;
};

int main(int argc, char *argv[])
{
        int rc = EXIT_FAILURE;

        if (argc) {
                regrw_log_level = LOG_VALUE(atoi(argv[0]));
		regrw_disp_level = regrw_log_level;
	};

        if (test_case_1()) {
                printf("\nTest_case_1 FAILED.");
                goto fail;
        };
        printf("\nTest_case_1 passed.");


        if (test_case_2()) {
                printf("\nTest_case_2 FAILED.");
                goto fail;
        };
        printf("\nTest_case_2 passed.");

        if (test_case_3()) {
                printf("\nTest_case_3 FAILED.\n");
                goto fail;
        };
        printf("\nTest_case_3 passed.\n");
        exit(0);

fail:
        exit(rc);
};

#ifdef __cplusplus
}
#endif
