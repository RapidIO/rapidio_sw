/*
 ************************************************************************ 
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
l of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this l of conditions and the following disclaimer in the documentation
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <stdarg.h>
#include <setjmp.h>
#include "cmocka.h"

#include "DAR_DB.h"
#include "DAR_DevDriver.h"
#include "IDT_RXS_API.h"
#include "IDT_RXS2448.h"
#include "IDT_RXS_Routing_Table_Config_API.h"
#include "IDT_RXS_Test.h"
#include "src/IDT_RXS_API.c"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mock_dar_reg_t_TAG
{
	uint32_t offset;
	uint32_t data;
} mock_dar_reg_t;

#define NUM_DAR_REG ((((RXS2448_MAX_PORTS+1)*(RXS_NUM_PERF_CTRS))*2)+ \
                     (RXS2448_MAX_PORTS+1)+1 ) //409 = ((((24*8) * 2) + 24) + 1)
#define MAX_DAR_REG (NUM_DAR_REG+1)

mock_dar_reg_t mock_dar_reg[MAX_DAR_REG+1];

#define ERR_NULL_PARM_PTR           ((uint32_t)(0x1012))
#define ERR_ACCESS                  ((uint32_t)(0x1004)) 
#define SUCCESS                     ((uint32_t)(0x0000))


static DAR_DEV_INFO_t mock_dev_info;
static idt_sc_dev_ctrs_t *mock_dev_ctrs = (idt_sc_dev_ctrs_t *)malloc(sizeof(idt_sc_dev_ctrs_t));
static idt_sc_p_ctrs_val_t *pp_ctrs = (idt_sc_p_ctrs_val_t *)malloc((MAX_DAR_PORTS+1) * sizeof(idt_sc_p_ctrs_val_t));

/* Create a mock dev_info.
 */
void rxs_test_setup(void)
{
        uint8_t idx, pnum;

	mock_dev_info.db_h = 3670020;
	mock_dev_info.privateData = 0x0;
        mock_dev_info.accessInfo = 0x0;
	strcpy(mock_dev_info.name, "RXS2448");
	mock_dev_info.dsf_h = 0x80E50005;
        mock_dev_info.extFPtrForPort = 0;
	mock_dev_info.extFPtrPortType = 0;
	mock_dev_info.extFPtrForLane = 12288;
	mock_dev_info.extFPtrForErr = 0;
	mock_dev_info.extFPtrForVC = 0;
	mock_dev_info.extFPtrForVOQ = 0;
	mock_dev_info.devID = 0x80E60038;
	mock_dev_info.devInfo = 0;
	mock_dev_info.assyInfo = 256;
	mock_dev_info.features = 402658623;
	mock_dev_info.swPortInfo = 6146;
	mock_dev_info.swRtInfo = 255;
	mock_dev_info.srcOps = 4;
	mock_dev_info.dstOps = 0;
	mock_dev_info.swMcastInfo = 0;
	for (idx = 0; idx < MAX_DAR_PORTS; idx++) //24
		mock_dev_info.ctl1_reg[idx] = 0;
	for (idx = 0; idx < MAX_DAR_SCRPAD_IDX; idx++) //30
		mock_dev_info.scratchpad[idx] = 0;

	mock_dev_ctrs->num_p_ctrs = MAX_DAR_PORTS; //24
	mock_dev_ctrs->valid_p_ctrs = MAX_DAR_PORTS; //24

        for (pnum = 0; pnum < MAX_DAR_PORTS+1; pnum++)
        { 
            pp_ctrs[pnum].pnum = pnum;
            pp_ctrs[pnum].ctrs_cnt = RXS_NUM_PERF_CTRS; //8
            for (idx = 0; idx < IDT_MAX_SC; idx++) //32
            {
                pp_ctrs[pnum].ctrs[idx].total = 0;
                pp_ctrs[pnum].ctrs[idx].last_inc = 0;
                pp_ctrs[pnum].ctrs[idx].sc = idt_sc_disabled;
                pp_ctrs[pnum].ctrs[idx].tx = false;
                pp_ctrs[pnum].ctrs[idx].srio = true;
           }
       }
       mock_dev_ctrs->p_ctrs = pp_ctrs;
}

/* Initialize the mock register structure for different registers.
 */
void init_mock_rxs_reg(void) 
{
        // idx is always should be less tahn MAX_DAR_REG.
        uint32_t idx = 0, port, cntr;

        // initialize RXS_RIO_SPX_PCNTR_CTL
        for (port = 0; port < RXS2448_MAX_PORTS+1; port++)
        {
            for (cntr = 0; cntr < RXS_NUM_PERF_CTRS; cntr++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_SPX_PCNTR_CTL(port, cntr);
                mock_dar_reg[idx].data = 0x02;
                idx++;
            }
        }

        // Initialize RXS_RIO_SPX_PCNTR_CNTR
        for (port = 0; port < RXS2448_MAX_PORTS+1; port++)
        {
            for (cntr = 0; cntr < RXS_NUM_PERF_CTRS; cntr++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_SPX_PCNTR_CNT(port, cntr);
                mock_dar_reg[idx].data = 0x10;
                idx++;
            }
        }

        // Initialize RXS_RIO_SPX_PCNTR_CTL
        for (port = 0; port < RXS2448_MAX_PORTS+1; port++)
        {
            mock_dar_reg[idx].offset = RXS_RIO_SPX_PCNTR_EN(port);
            mock_dar_reg[idx].data = 0x00;
            idx++;
        }

        // Initialize RXS_RIO_PCNTR_CTL
        mock_dar_reg[idx].offset = RXS_RIO_PCNTR_CTL;
        mock_dar_reg[idx].data = 0x00;
        idx++;
}

/* The function tries to find the index of the offset in the dar_reg array and returns the idx,
 * otherwise it returns MAX_DAR_REG.
 */
uint32_t find_offset(uint32_t offset)
{
	uint32_t idx;
	for (idx = 0; idx < NUM_DAR_REG; idx++)
	{
		if (mock_dar_reg[idx].offset == offset)
			return idx;
	}
	return MAX_DAR_REG;
}

/* The function reads the value data of offset from the dar_reg array.
 * If the function finds the offset, it returns SUCCESS otherwise ERR_ACCESS.
 */
uint32_t __wrap_DARRegRead(DAR_DEV_INFO_t *dev_info, uint32_t offset, uint32_t *readdata)
{
	uint32_t idx = MAX_DAR_REG;

        if (NULL != dev_info && *readdata)
            mock_dev_info = *dev_info;

	idx = find_offset(offset);
	if (idx == MAX_DAR_REG)
		return ERR_ACCESS;

	*readdata = mock_dar_reg[idx].data;
	return SUCCESS;
}

/* The function updates the value data of offset in the dar_reg array.
 * If the function finds the offset, it returns SUCCESS otherwise ERR_ACCESS.
 */
uint32_t __wrap_DARRegWrite(DAR_DEV_INFO_t *dev_info, uint32_t offset, uint32_t writedata)
{
	uint32_t idx = MAX_DAR_REG;

        if (NULL != dev_info)
            mock_dev_info = *dev_info;

	idx = find_offset(offset);
	if (idx == MAX_DAR_REG)
		return ERR_ACCESS;

	mock_dar_reg[idx].data = writedata;
        return SUCCESS;
}

/* The setup function which should be called before any unit tests that need to be executed.
 */
static int setup(void **state)
{
        DARDB_init();
	rxs_test_setup();
        init_mock_rxs_reg();

        (void)state; // unused
        return 0;
}

/* The teardown function to be called after any tests have finished.
 */
static int teardown(void **state) 
{
        free(mock_dev_ctrs);

        free(*state);
        return 0;
}

static void assumptions(void **state)
{
	// verify constants
	assert_int_equal(0xFF, RIO_ALL_PORTS);
        assert_int_equal(8, RXS_NUM_PERF_CTRS);
        assert_int_equal(23, RXS2448_MAX_PORTS);
        assert_int_equal(47, RXS2448_MAX_LANES);

	(void)state; // unused
}

static void macros_test(void **state)
{
	// the macro doesn't care if you give it values out of range
	assert_int_equal(0x1C100, RXS_RIO_SPX_PCNTR_EN(0x00));
        assert_int_equal(0x1D800, RXS_RIO_SPX_PCNTR_EN(0x17));
        assert_int_equal(0x1C110, RXS_RIO_SPX_PCNTR_CTL(0x00, 0x00));
        assert_int_equal(0x1D82C, RXS_RIO_SPX_PCNTR_CTL(0x17, 0x07));
        assert_int_equal(0x1C130, RXS_RIO_SPX_PCNTR_CNT(0x00, 0x00));
        assert_int_equal(0x1D84C, RXS_RIO_SPX_PCNTR_CNT(0x17, 0x07));
        assert_int_equal(0x30000, RXS_RIO_BC_L0_G0_ENTRYX_CSR(0x00));
        assert_int_equal(0x30028, RXS_RIO_BC_L0_G0_ENTRYX_CSR(0x0A));
        assert_int_equal(0x30400, RXS_RIO_BC_L1_GX_ENTRYY_CSR(0x00, 0x00));
        assert_int_equal(0x30428, RXS_RIO_BC_L1_GX_ENTRYY_CSR(0x00, 0x0A));
        assert_int_equal(0x31000, RXS_RIO_BC_L2_GX_ENTRYY_CSR(0x00, 0x00));
        assert_int_equal(0x31028, RXS_RIO_BC_L2_GX_ENTRYY_CSR(0x00, 0x0A));
        assert_int_equal(0x32000, RXS_RIO_BC_MC_X_S_CSR(0x00));
        assert_int_equal(0x32050, RXS_RIO_BC_MC_X_S_CSR(0x0A));
        assert_int_equal(0x50000, RXS_RIO_SPX_L0_G0_ENTRYY_CSR(0x00, 0x00));
        assert_int_equal(0x64028, RXS_RIO_SPX_L0_G0_ENTRYY_CSR(0x0A, 0x0A));
        assert_int_equal(0x50400, RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(0x00, 0x00, 0x00));
        assert_int_equal(0x64428, RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(0x0A, 0x00, 0x0A));
        assert_int_equal(0x51000, RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(0x00, 0x00, 0x00));
        assert_int_equal(0x65028, RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(0x0A, 0x00, 0x0A));
        assert_int_equal(0x80000, RXS_RIO_SPX_MC_Y_S_CSR(0x00, 0x00));
        assert_int_equal(0x8A050, RXS_RIO_SPX_MC_Y_S_CSR(0x0A, 0x0A));

        assert_int_equal(0x001FF, IDT_RXS_DSF_BAD_MC_MASK);

	(void)state; // unused
}

static void rxs_init_ctrs(idt_sc_init_dev_ctrs_in_t *parms_in)
{
        uint8_t pnum;

        parms_in->ptl.num_ports = RIO_ALL_PORTS;
        for (pnum = 0; pnum < RXS2448_MAX_PORTS+1; pnum++) {
            parms_in->ptl.pnums[pnum] = 0x00;
        }

        parms_in->dev_ctrs = mock_dev_ctrs;
}

static void rxs_init_dev_ctrs_test(void **state)
{
        idt_sc_init_dev_ctrs_in_t      mock_sc_in;
        idt_sc_init_dev_ctrs_out_t     mock_sc_out;

        rxs_init_ctrs(&mock_sc_in);

        assert_int_equal(SUCCESS, idt_rxs_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_equal(SUCCESS, mock_sc_out.imp_rc);

        (void)state; // unused
}

static void rxs_init_read_ctrs(idt_sc_read_ctrs_in_t *parms_in)
{
	uint8_t srch_i;
	uint32_t cntr;

	parms_in->ptl.num_ports = RIO_ALL_PORTS;
	parms_in->dev_ctrs = mock_dev_ctrs;
        for (srch_i = 0; srch_i < parms_in->dev_ctrs->valid_p_ctrs; srch_i++) {
		for (cntr = 0; cntr < RXS_NUM_PERF_CTRS; cntr++) {
			parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = true;
			switch (cntr) {
			case 0:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pkt;
				break;
			case 1:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pkt;
				break;
			case 2:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pcntr;
				break;
			case 3:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pcntr;
				break;
			case 4:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_ttl_pcntr;
				break;
			case 5:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_retries;
				break;
			case 6:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pna;
				break;
			case 7:
				parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pkt_drop;
				break;
			}
		}
	}
}

static void rxs_read_dev_ctrs_test(void **state)
{
	idt_sc_read_ctrs_in_t      mock_sc_in;
	idt_sc_read_ctrs_out_t     mock_sc_out;

	rxs_init_read_ctrs(&mock_sc_in);

	assert_int_equal(SUCCESS, idt_rxs_sc_read_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_equal(SUCCESS, mock_sc_out.imp_rc);

	(void)state; // unused
}

static void rxs_cfg_dev_ctrs_test(void **state)
{
        bool tx = true;
	idt_sc_cfg_rxs_ctr_in_t      mock_sc_in;
	idt_sc_cfg_rxs_ctr_out_t     mock_sc_out;

        mock_sc_in.ptl.num_ports = RIO_ALL_PORTS;
        mock_sc_in.dev_ctrs = mock_dev_ctrs;
        mock_sc_in.ctr_en = RXS_RIO_PCNTR_CTL_CNTR_FRZ;
        mock_sc_in.prio_mask = FIRST_BYTE_MASK;
        for (mock_sc_in.ctr_idx = 0; mock_sc_in.ctr_idx < RXS_NUM_PERF_CTRS; ++mock_sc_in.ctr_idx) {
                mock_sc_in.tx = tx;
                switch (mock_sc_in.ctr_idx) {
                case 0:
                        mock_sc_in.ctr_type = idt_sc_rio_pkt;
                        break;
                case 1:
                        mock_sc_in.ctr_type = idt_sc_rio_pkt;
                        mock_sc_in.tx = !tx;
                        break;
                case 2:
                        mock_sc_in.ctr_type = idt_sc_rio_pcntr;
                        break;
                case 3:
                        mock_sc_in.ctr_type = idt_sc_rio_pcntr;
                        mock_sc_in.tx = !tx;
                        break;
                case 4:
                        mock_sc_in.ctr_type = idt_sc_retries;
                        break;
                case 5:
                        mock_sc_in.ctr_type = idt_sc_retries;
                        mock_sc_in.tx = !tx;
                        break;
                case 6:
                        mock_sc_in.ctr_type = idt_sc_rio_ttl_pcntr;
                        break;
                case 7:
                        mock_sc_in.ctr_type = idt_sc_pna;
                        break;
                }
                assert_int_equal(SUCCESS, idt_rxs_sc_cfg_ctr(&mock_dev_info, &mock_sc_in, &mock_sc_out));
                assert_int_equal(SUCCESS, mock_sc_out.imp_rc);
        }

	(void)state; // unused
}

static void rxs_cfg_read_dev_ctrs_test(void **state)
{
        rxs_cfg_dev_ctrs_test(state);
        rxs_read_dev_ctrs_test(state);

        (void)state; // unused
}

static void rxs_init_cfg_read_dev_ctrs_test(void **state)
{
        rxs_init_dev_ctrs_test(state);
        rxs_cfg_dev_ctrs_test(state);
        rxs_read_dev_ctrs_test(state);

        (void)state; // unused
}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++; // not used

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(assumptions),
                cmocka_unit_test(macros_test),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_cfg_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_cfg_read_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_cfg_read_dev_ctrs_test, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_read_dev_ctrs_test, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}

#ifdef __cplusplus
}
#endif
	
