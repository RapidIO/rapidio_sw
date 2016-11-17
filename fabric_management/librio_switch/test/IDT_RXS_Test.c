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

#define NUM_DAR_REG ((((RXS2448_MAX_PORTS)*(RXS_NUM_PERF_CTRS))*2)+ \
                     (RXS2448_MAX_PORTS*2)+7+ \
                     ((RXS2448_MAX_PORTS)*IDT_DAR_RT_DEV_TABLE_SIZE)+ \
                     (RXS_MAX_L2_GROUP*IDT_DAR_RT_DEV_TABLE_SIZE)+ \
                     (RXS_MAX_L1_GROUP*IDT_DAR_RT_DEV_TABLE_SIZE)+ \
                     ((RXS2448_MAX_PORTS)*RXS_MAX_L2_GROUP*IDT_DAR_RT_DEV_TABLE_SIZE)+ \
                     ((RXS2448_MAX_PORTS)*RXS_MAX_L1_GROUP*IDT_DAR_RT_DEV_TABLE_SIZE) )

#define UPB_DAR_REG (NUM_DAR_REG+1)

mock_dar_reg_t mock_dar_reg[UPB_DAR_REG];

static DAR_DEV_INFO_t mock_dev_info;
static idt_sc_dev_ctrs_t *mock_dev_ctrs = (idt_sc_dev_ctrs_t *)malloc(sizeof(idt_sc_dev_ctrs_t));
static idt_sc_p_ctrs_val_t *pp_ctrs = (idt_sc_p_ctrs_val_t *)malloc((MAX_DAR_PORTS) * sizeof(idt_sc_p_ctrs_val_t));

/* Create a mock dev_info.
 */
static void rxs_test_setup(void)
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
	for (idx = 0; idx < MAX_DAR_PORTS; idx++)
        {
		mock_dev_info.ctl1_reg[idx] = 0;
        }

	for (idx = 0; idx < MAX_DAR_SCRPAD_IDX; idx++)
        {
		mock_dev_info.scratchpad[idx] = 0;
        }

	mock_dev_ctrs->num_p_ctrs = MAX_DAR_PORTS;
	mock_dev_ctrs->valid_p_ctrs = MAX_DAR_PORTS;

        for (pnum = 0; pnum < MAX_DAR_PORTS; pnum++)
        { 
            pp_ctrs[pnum].pnum = pnum;
            pp_ctrs[pnum].ctrs_cnt = RXS_NUM_PERF_CTRS;
            for (idx = 0; idx < IDT_MAX_SC; idx++)
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
static void init_mock_rxs_reg(void) 
{
        // idx is always should be less tahn UPB_DAR_REG.
        uint32_t idx = 0, port, cntr, idev, grp;

        // initialize RXS_RIO_SPX_PCNTR_CTL
        for (port = 0; port < RXS2448_MAX_PORTS; port++)
        {
            for (cntr = 0; cntr < RXS_NUM_PERF_CTRS; cntr++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_SPX_PCNTR_CTL(port, cntr);
                mock_dar_reg[idx].data = 0x02;
                idx++;
            }
        }

        // Initialize RXS_RIO_SPX_PCNTR_CNTR
        for (port = 0; port < RXS2448_MAX_PORTS; port++)
        {
            for (cntr = 0; cntr < RXS_NUM_PERF_CTRS; cntr++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_SPX_PCNTR_CNT(port, cntr);
                mock_dar_reg[idx].data = 0x10;
                idx++;
            }
        }

        // Initialize RXS_RIO_SPX_PCNTR_CTL and RXS_RIO_SP0_CTL
        for (port = 0; port < RXS2448_MAX_PORTS; port++)
        {
            mock_dar_reg[idx].offset = RXS_RIO_SPX_PCNTR_EN(port);
            mock_dar_reg[idx].data = 0x00;
            idx++;
            mock_dar_reg[idx].offset = RXS_RIO_SP0_CTL(port);
            mock_dar_reg[idx].data = 0x00;
            idx++;
        }

        // Initialize RXS_RIO_PCNTR_CTL
        mock_dar_reg[idx].offset = RXS_RIO_PCNTR_CTL;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_ROUTE_DFLT_PORT
        mock_dar_reg[idx].offset = RXS_RIO_ROUTE_DFLT_PORT;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_PKT_TIME_LIVE
        mock_dar_reg[idx].offset = RXS_RIO_PKT_TIME_LIVE;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_SP_LT_CTL
        mock_dar_reg[idx].offset = RXS_RIO_SP_LT_CTL;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_SR_RSP_TO
        mock_dar_reg[idx].offset = RXS_RIO_SR_RSP_TO;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_SP0_CTL2
        mock_dar_reg[idx].offset = RXS_RIO_SP0_CTL2;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_PLM_SP0_IMP_SPEC_CTL
        mock_dar_reg[idx].offset = RXS_RIO_PLM_SP0_IMP_SPEC_CTL;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_SP0_ERR_STAT
        mock_dar_reg[idx].offset = RXS_RIO_SP0_ERR_STAT;
        mock_dar_reg[idx].data = 0x00;
        idx++;

        // Initialize RXS_RIO_SPX_MC_Y_S_CSR
        for (port = 0; port < RXS2448_MAX_PORTS; port++)
        {
            for (idev = 0; idev < IDT_DAR_RT_DEV_TABLE_SIZE; idev++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_SPX_MC_Y_S_CSR(port, idev);
                mock_dar_reg[idx].data = 0x00;
                idx++;
            }
        }

        // Initialize RXS_RIO_BC_L2_GX_ENTRYY_CSR
        for (grp = 0; grp < RXS_MAX_L2_GROUP; grp++)
        {
            for (idev = 0; idev < IDT_DAR_RT_DEV_TABLE_SIZE; idev++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_BC_L2_GX_ENTRYY_CSR(grp, idev);
                mock_dar_reg[idx].data = 0x00;
                idx++;
            }
        }

        // Initialize RXS_RIO_BC_L1_GX_ENTRYY_CSR
        for (grp = 0; grp < RXS_MAX_L1_GROUP; grp++)
        {
            for (idev = 0; idev < IDT_DAR_RT_DEV_TABLE_SIZE; idev++)
            {
                mock_dar_reg[idx].offset = RXS_RIO_BC_L1_GX_ENTRYY_CSR(grp, idev);
                mock_dar_reg[idx].data = 0x00;
                idx++;
            }
        }

        // Initialize RXS_RIO_SPX_L2_GY_ENTRYZ_CSR
        for (port = 0; port < RXS2448_MAX_PORTS; port++)
        {
            for (grp = 0; grp < RXS_MAX_L2_GROUP; grp++)
            {
                for (idev = 0; idev < IDT_DAR_RT_DEV_TABLE_SIZE; idev++)
                {
                     mock_dar_reg[idx].offset = RXS_RIO_SPX_L2_GY_ENTRYZ_CSR(port, grp, idev);
                     mock_dar_reg[idx].data = 0x00;
                     idx++;
                }
            }
        }

        // Initialize RXS_RIO_SPX_L1_GY_ENTRYZ_CSR
        for (port = 0; port < RXS2448_MAX_PORTS; port++)
        {
            for (grp = 0; grp < RXS_MAX_L1_GROUP; grp++)
            {
                for (idev = 0; idev < IDT_DAR_RT_DEV_TABLE_SIZE; idev++)
                {
                     mock_dar_reg[idx].offset = RXS_RIO_SPX_L1_GY_ENTRYZ_CSR(port, grp, idev);
                     mock_dar_reg[idx].data = 0x00;
                     idx++;
                }
            }
        }
}

/* The function tries to find the index of the offset in the dar_reg array and returns the idx,
 * otherwise it returns UPB_DAR_REG.
 */
uint32_t find_offset(uint32_t offset)
{
	uint32_t idx;
	for (idx = 0; idx < NUM_DAR_REG; idx++)
	{
		if (mock_dar_reg[idx].offset == offset)
                {
			return idx;
                }
	}
	return UPB_DAR_REG;
}

/* The function reads the value data of offset from the dar_reg array.
 * If the function finds the offset, it returns SUCCESS otherwise ERR_ACCESS.
 */
uint32_t __wrap_DARRegRead(DAR_DEV_INFO_t *dev_info, uint32_t offset, uint32_t *readdata)
{
	uint32_t idx = UPB_DAR_REG;

        if (NULL != dev_info && *readdata) 
        {
            mock_dev_info = *dev_info;
        }
        else 
        {
            *dev_info = mock_dev_info;
        }

	idx = find_offset(offset);
	if (idx == UPB_DAR_REG)
        {
                return RIO_ERR_ACCESS;
        }

	*readdata = mock_dar_reg[idx].data;
	return RIO_SUCCESS;
}

/* The function updates the value data of offset in the dar_reg array.
 * If the function finds the offset, it returns SUCCESS otherwise ERR_ACCESS.
 */
uint32_t __wrap_DARRegWrite(DAR_DEV_INFO_t *dev_info, uint32_t offset, uint32_t writedata)
{
	uint32_t idx = UPB_DAR_REG;

        if (NULL != dev_info) 
        {
            mock_dev_info = *dev_info;
        }
        else
        {
            *dev_info = mock_dev_info;
        }

	idx = find_offset(offset);
	if (idx == UPB_DAR_REG)
        {
		return RIO_ERR_ACCESS;
        }

	mock_dar_reg[idx].data = writedata;
        return RIO_SUCCESS;
}

/* The setup function which should be called before any unit tests that need to be executed.
 */
static int setup(void **state)
{
        memset(&mock_dev_info, 0x00, sizeof(idt_sc_dev_ctrs_t));
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

        (void)state; //unused
        return 0;
}

void assumptions_test(void **state)
{
	// verify constants
        assert_int_equal(8, RXS_NUM_PERF_CTRS);
        assert_int_equal(24, RXS2448_MAX_PORTS);
        assert_int_equal(48, RXS2448_MAX_LANES);

	(void)state; // unused
}

void macros_test(void **state)
{
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
        for (pnum = 0; pnum < RXS2448_MAX_PORTS; pnum++) {
            parms_in->ptl.pnums[pnum] = 0x00;
        }

        parms_in->dev_ctrs = mock_dev_ctrs;
}

void rxs_init_dev_ctrs_test(void **state)
{
        idt_sc_init_dev_ctrs_in_t      mock_sc_in;
        idt_sc_init_dev_ctrs_out_t     mock_sc_out;

        rxs_init_ctrs(&mock_sc_in);

        assert_int_equal(RIO_SUCCESS, idt_rxs_sc_init_dev_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

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
                        default:
                                parms_in->dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_disabled;
                                break;
			}
		}
	}
}

void rxs_read_dev_ctrs_test(void **state)
{
	idt_sc_read_ctrs_in_t      mock_sc_in;
	idt_sc_read_ctrs_out_t     mock_sc_out;

	rxs_init_read_ctrs(&mock_sc_in);

	assert_int_equal(RIO_SUCCESS, idt_rxs_sc_read_ctrs(&mock_dev_info, &mock_sc_in, &mock_sc_out));
        assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);

	(void)state; // unused
}

void rxs_cfg_dev_ctrs_test(void **state)
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
                default:
                        mock_sc_in.ctr_type = idt_sc_disabled;
                        break;
                }
                assert_int_equal(RIO_SUCCESS, idt_rxs_sc_cfg_ctr(&mock_dev_info, &mock_sc_in, &mock_sc_out));
                assert_int_equal(RIO_SUCCESS, mock_sc_out.imp_rc);
        }

	(void)state; // unused
}

void rxs_cfg_read_dev_ctrs_test(void **state)
{
        rxs_cfg_dev_ctrs_test(state);
        rxs_read_dev_ctrs_test(state);

        (void)state; // unused
}

void rxs_init_cfg_read_dev_ctrs_test(void **state)
{
        rxs_init_dev_ctrs_test(state);
        rxs_cfg_dev_ctrs_test(state);
        rxs_read_dev_ctrs_test(state);

        (void)state; // unused
}

static void rxs_init_mock_rt(idt_rt_state_t *rt)
{
        uint32_t idx;
        rt->default_route = 2;
        for (idx = 0; idx < IDT_DAR_RT_DEV_TABLE_SIZE; idx++)
        {
            rt->dev_table[idx].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
            rt->dev_table[idx].changed = false;
            rt->dom_table[idx].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
            rt->dom_table[idx].changed = false;
        }
        for (idx = 0; idx < IDT_RXS_DSF_MAX_MC_MASK; idx++) 
        {
            rt->mc_masks[idx].mc_destID = 0xFF;
            rt->mc_masks[idx].tt = tt_dev8;
            rt->mc_masks[idx].mc_mask = 0;
            rt->mc_masks[idx].in_use = false;
            rt->mc_masks[idx].allocd = false;
            rt->mc_masks[idx].changed = false;
        }
}

void rxs_init_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        (void)state; // unused
}

void rxs_init_setport_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint32_t port = RIO_ALL_PORTS;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        (void)state; // unused
}

void rxs_set_all_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
	idt_rt_set_all_out_t        mock_set_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        (void)state; // unused
}

void rxs_set_all_setport_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = RIO_ALL_PORTS;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        (void)state; // unused
}

void rxs_set_all_rt_null_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;

        uint32_t temp;
        uint8_t port = 0;

        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = NULL;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_ERR_INVALID_PARAMETER, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(IDT_RXS_RTE_SET_COMMON(1), mock_set_out.imp_rc);

        (void)state; // unused
}

void rxs_change_rte_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_change_rte_in_t      mock_chg_in;
	idt_rt_change_rte_out_t     mock_chg_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_set_changed_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_changed_in_t     mock_set_chg_in;
        idt_rt_set_changed_out_t    mock_set_chg_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_alloc_mc_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, IDT_DSF_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        (void)state; // unused
}

void rxs_change_mc_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_change_mc_mask_in_t  mock_mc_chg_in;
	idt_rt_change_mc_mask_out_t mock_mc_chg_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, IDT_DSF_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = 1;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = false;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_routing_table_01_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_change_rte_in_t      mock_chg_in;
        idt_rt_change_rte_out_t     mock_chg_out;
        idt_rt_set_changed_in_t     mock_set_chg_in;
        idt_rt_set_changed_out_t    mock_set_chg_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_change_mc_mask_in_t  mock_mc_chg_in;
        idt_rt_change_mc_mask_out_t mock_mc_chg_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, IDT_DSF_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = 1;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = false;
        mock_mc_chg_in.mc_info.allocd = false;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_mc_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(mock_chg_in.rte_value, mock_set_chg_in.rt->dom_table[mock_chg_in.idx].rte_val);
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_init_02_mock_rt(idt_rt_state_t *rt)
{
        uint32_t idx;
        rt->default_route = 2;
        for (idx = 0; idx < IDT_DAR_RT_DEV_TABLE_SIZE; idx++)
        {
            rt->dev_table[idx].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
            rt->dev_table[idx].changed = true;
            rt->dom_table[idx].rte_val = IDT_RXS_DSF_RT_NO_ROUTE;
            rt->dom_table[idx].changed = true;
        }
        for (idx = 0; idx < IDT_RXS_DSF_MAX_MC_MASK; idx++)
        {
            rt->mc_masks[idx].mc_destID = 0xFF;
            rt->mc_masks[idx].tt = tt_dev8;
            rt->mc_masks[idx].mc_mask = 0;
            rt->mc_masks[idx].in_use = true;
            rt->mc_masks[idx].allocd = true;
            rt->mc_masks[idx].changed = true;
        }
}

void rxs_routing_table_02_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_change_rte_in_t      mock_chg_in;
        idt_rt_change_rte_out_t     mock_chg_out;
        idt_rt_set_changed_in_t     mock_set_chg_in;
        idt_rt_set_changed_out_t    mock_set_chg_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_change_mc_mask_in_t  mock_mc_chg_in;
        idt_rt_change_mc_mask_out_t mock_mc_chg_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_02_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = false;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = 2;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = true;
        mock_mc_chg_in.mc_info.changed = true;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_mc_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(mock_chg_in.rte_value, mock_set_chg_in.rt->dev_table[mock_chg_in.idx].rte_val);
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        (void)state; // unused
}

void rxs_probe_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_probe_in_t           mock_probe_in;
        idt_rt_probe_out_t          mock_probe_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 1;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_probe_in.probe_on_port = port;
	mock_probe_in.tt = tt_dev8;
	mock_probe_in.destID = did;
	mock_probe_in.rt = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        (void)state; // unused
}

void rxs_probe_all_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_probe_all_in_t       mock_probe_all_in;
	idt_rt_probe_all_out_t      mock_probe_all_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_probes_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_probe_in_t           mock_probe_in;
        idt_rt_probe_out_t          mock_probe_out;
        idt_rt_probe_all_in_t       mock_probe_all_in;
        idt_rt_probe_all_out_t      mock_probe_all_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 1;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_probe_in.probe_on_port = port;
        mock_probe_in.tt = tt_dev8;
        mock_probe_in.destID = did;
        mock_probe_in.rt = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_probe_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_routing_table_03_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_change_rte_in_t      mock_chg_in;
        idt_rt_change_rte_out_t     mock_chg_out;
        idt_rt_set_changed_in_t     mock_set_chg_in;
        idt_rt_set_changed_out_t    mock_set_chg_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_change_mc_mask_in_t  mock_mc_chg_in;
        idt_rt_change_mc_mask_out_t mock_mc_chg_out;
        idt_rt_probe_in_t           mock_probe_in;
        idt_rt_probe_out_t          mock_probe_out;
        idt_rt_probe_all_in_t       mock_probe_all_in;
        idt_rt_probe_all_out_t      mock_probe_all_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 2;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = did;
        mock_mc_chg_in.mc_info.tt = tt_dev8;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = true;
        mock_mc_chg_in.mc_info.changed = true;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_mc_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_probe_in.probe_on_port = port;
        mock_probe_in.tt = tt_dev8;
        mock_probe_in.destID = did;
        mock_probe_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_probe_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(mock_chg_in.rte_value, mock_set_chg_in.rt->dom_table[mock_chg_in.idx].rte_val);
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_routing_table_04_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_change_rte_in_t      mock_chg_in;
        idt_rt_change_rte_out_t     mock_chg_out;
        idt_rt_set_changed_in_t     mock_set_chg_in;
        idt_rt_set_changed_out_t    mock_set_chg_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_change_mc_mask_in_t  mock_mc_chg_in;
        idt_rt_change_mc_mask_out_t mock_mc_chg_out;
        idt_rt_probe_in_t           mock_probe_in;
        idt_rt_probe_out_t          mock_probe_out;
        idt_rt_probe_all_in_t       mock_probe_all_in;
        idt_rt_probe_all_out_t      mock_probe_all_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint8_t did = 2;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 1;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = did;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = true;
        mock_mc_chg_in.mc_info.changed = true;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_mc_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_probe_in.probe_on_port = port;
        mock_probe_in.tt = tt_dev8;
        mock_probe_in.destID = did;
        mock_probe_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe(&mock_dev_info, &mock_probe_in, &mock_probe_out));
        assert_int_equal(RIO_SUCCESS, mock_probe_out.imp_rc);

        mock_probe_all_in.probe_on_port = RIO_ALL_PORTS;
        mock_probe_all_in.rt = mock_probe_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_probe_all(&mock_dev_info, &mock_probe_all_in, &mock_probe_all_out));
        assert_int_equal(mock_chg_in.rte_value, mock_set_chg_in.rt->dom_table[mock_chg_in.idx].rte_val);
        assert_int_equal(RIO_SUCCESS, mock_probe_all_out.imp_rc);

        (void)state; // unused
}

void rxs_change_mc_mask_rt_test(void **state)
{
        idt_rt_initialize_in_t      mock_init_in;
        idt_rt_initialize_out_t     mock_init_out;
        idt_rt_set_all_in_t         mock_set_in;
        idt_rt_set_all_out_t        mock_set_out;
        idt_rt_change_rte_in_t      mock_chg_in;
        idt_rt_change_rte_out_t     mock_chg_out;
        idt_rt_set_changed_in_t     mock_set_chg_in;
        idt_rt_set_changed_out_t    mock_set_chg_out;
        idt_rt_alloc_mc_mask_in_t   mock_alloc_in;
        idt_rt_alloc_mc_mask_out_t  mock_alloc_out;
        idt_rt_change_mc_mask_in_t  mock_mc_chg_in;
        idt_rt_change_mc_mask_out_t mock_mc_chg_out;
        idt_rt_state_t              rt;
        uint32_t temp;
        uint8_t port = 0;
        uint32_t did = 0x200;

        rxs_init_mock_rt(&rt);
        assert_int_equal(RIO_SUCCESS, DARRegRead(&mock_dev_info, RXS_RIO_ROUTE_DFLT_PORT, &temp));
        mock_init_in.set_on_port = port;
        mock_init_in.default_route = (uint8_t)(temp & RXS_RIO_ROUTE_DFLT_PORT_DEFAULT_OUT_PORT);
        mock_init_in.default_route_table_port = IDT_RXS_DSF_RT_NO_ROUTE;
        mock_init_in.update_hw = false;
        mock_init_in.rt        = &rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_initialize(&mock_dev_info, &mock_init_in, &mock_init_out));
        assert_int_equal(RIO_SUCCESS, mock_init_out.imp_rc);

        mock_set_in.set_on_port = mock_init_in.set_on_port;
        mock_set_in.rt = mock_init_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_all(&mock_dev_info, &mock_set_in, &mock_set_out));
        assert_int_equal(RIO_SUCCESS, mock_set_out.imp_rc);

        mock_chg_in.dom_entry = true;
        mock_chg_in.idx       = 2;
        mock_chg_in.rte_value = 1;
        mock_chg_in.rt        = mock_set_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_rte(&mock_dev_info, &mock_chg_in, &mock_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_chg_out.imp_rc);

        mock_set_chg_in.set_on_port = mock_init_in.set_on_port;
        mock_set_chg_in.rt = mock_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_set_changed(&mock_dev_info, &mock_set_chg_in, &mock_set_chg_out));
        assert_int_equal(RIO_SUCCESS, mock_set_chg_out.imp_rc);

        mock_dev_info.swMcastInfo = 1;
        mock_alloc_in.rt = mock_set_chg_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_alloc_mc_mask(&mock_dev_info, &mock_alloc_in, &mock_alloc_out));
        assert_int_equal(RIO_SUCCESS, mock_alloc_out.imp_rc);

        mock_mc_chg_in.mc_mask_rte = mock_alloc_out.mc_mask_rte;
        mock_mc_chg_in.mc_info.mc_mask = 1;
        mock_mc_chg_in.mc_info.mc_destID = did;
        mock_mc_chg_in.mc_info.tt = tt_dev16;
        mock_mc_chg_in.mc_info.in_use = true;
        mock_mc_chg_in.mc_info.allocd = true;
        mock_mc_chg_in.mc_info.changed = false;
        mock_mc_chg_in.rt = mock_alloc_in.rt;

        assert_int_equal(RIO_SUCCESS, idt_rxs_rt_change_mc_mask(&mock_dev_info, &mock_mc_chg_in, &mock_mc_chg_out));
        assert_int_equal(IDT_RXS_DSF_RT_USE_PACKET_ROUTE, mock_mc_chg_in.rt->dom_table[mock_chg_in.idx].rte_val);
        assert_int_equal(IDT_RXS_DSF_FIRST_MC_MASK, mock_mc_chg_in.rt->dev_table[0].rte_val);
        assert_int_equal(IDT_RXS_DSF_RT_NO_ROUTE, mock_mc_chg_in.rt->dev_table[mock_chg_in.idx].rte_val);
        assert_true(mock_mc_chg_in.rt->dom_table[mock_chg_in.idx].changed);
        assert_int_equal(RIO_SUCCESS, mock_mc_chg_out.imp_rc);

        (void)state; // unused
}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++; // not used

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(assumptions_test),
                cmocka_unit_test(macros_test),
                cmocka_unit_test_setup_teardown(rxs_init_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_cfg_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_cfg_read_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_cfg_read_dev_ctrs_test, setup, NULL),
		cmocka_unit_test_setup_teardown(rxs_read_dev_ctrs_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_init_setport_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_all_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_all_setport_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_all_rt_null_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_rte_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_mask_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_set_changed_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_alloc_mc_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_change_mc_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_01_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_02_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_probe_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_probe_all_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_probes_rt_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_03_test, setup, NULL),
                cmocka_unit_test_setup_teardown(rxs_routing_table_04_test, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}

#ifdef __cplusplus
}
#endif

