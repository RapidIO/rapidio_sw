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

#include "RapidIO_Source_Config.h"
#include "RapidIO_Device_Access_Routines_API.h"
#include "RXS2448.h"
#include "src/RXS_DeviceDriver.c"
#include "src/RXS_PC.c"
#include "rio_ecosystem.h"
#include "tok_parse.h"
#include "libcli.h"
#include "rapidio_mport_mgmt.h"

#define DEBUG_PRINTF 0
#define DEBUG_REGTRACE 0

#include "common_src/RXS_cmdline.c"
#include "common_src/RXS_reg_emulation.c"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RXS_DAR_WANTED

static void rxs_not_supported_test(void **state)
{
	(void)state; // not used
}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++;// not used

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(rxs_not_supported_test)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

#endif /* RXS_DAR_WANTED */

#ifdef RXS_DAR_WANTED

static void rxs_rio_pc_macros_test(void **state)
{
	assert_int_equal(RIO_SPX_CTL_PTW_MAX_LANES(0x40000000), 4);
	assert_int_equal(RIO_SPX_CTL_PTW_MAX_LANES(0xC0000000), 4);
	assert_int_equal(RIO_SPX_CTL_PTW_MAX_LANES(0x80000000), 2);
	assert_int_equal(RIO_SPX_CTL_PTW_MAX_LANES(0x00000000), 1);

	(void)state;
}

typedef struct clk_pd_tests_t_TAG {
	uint32_t ps; // Prescalar
	uint32_t cfgsig0; // Ref clock, clock config
	uint32_t clk_pd; // Expected clock period
} clk_pd_tests_t;

#define LO_LAT     RXS_MPM_CFGSIG0_CORECLK_SELECT_LO_LAT
#define LO_RSVD    RXS_MPM_CFGSIG0_CORECLK_SELECT_RSVD
#define LO_PWR_12G RXS_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_12G
#define LO_PWR_10G RXS_MPM_CFGSIG0_CORECLK_SELECT_LO_PWR_10G
#define MHZ_100    RXS_MPM_CFGSIG0_REFCLK_SELECT_100MHZ
#define MHZ_156    RXS_MPM_CFGSIG0_REFCLK_SELECT_156P25MHZ

static clk_pd_tests_t clk_pd_pass[] = {
	{ 42, LO_LAT | MHZ_100, 1001},
	{ 42, LO_LAT | MHZ_156, 1000},
	{ 38, LO_PWR_12G | MHZ_156, 998},
	{ 37, LO_PWR_12G | MHZ_100, 992},
	{ 31, LO_PWR_10G | MHZ_156, 992},
	{ 31, LO_PWR_10G | MHZ_100, 992}
};

static void rxs_rio_pc_clk_pd_success_test(void **state)
{
	RXS_test_state_t *l_st = *(RXS_test_state_t **)state;
	const int num_tests = sizeof(clk_pd_pass) / sizeof(clk_pd_pass[0]);
	uint32_t srv_pd;
	uint32_t i;

	// On real hardware, it is disasterous to mess with clocking
	// parameters, so just check that the current setup passes

	if (l_st->real_hw) {
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));

		return;
	}

	for (i = 0; i < num_tests; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni = %d\n", i);
		}
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_PRESCALAR_SRV_CLK,
			clk_pd_pass[i].ps));
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_MPM_CFGSIG0,
			clk_pd_pass[i].cfgsig0));
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));
		assert_int_equal(srv_pd, clk_pd_pass[i].clk_pd);
	}
}

static clk_pd_tests_t clk_pd_fail[] = {
	{ 0x1FF, 0, 0},
	{ 0, 0, 0},
	{ 42, LO_RSVD | MHZ_156, 0},
	{ 42, LO_RSVD | MHZ_100, 0},
	{ 41, LO_LAT | MHZ_100, 1000},
	{ 41, LO_LAT | MHZ_156, 1001},
	{ 37, LO_PWR_12G | MHZ_156, 998},
	{ 36, LO_PWR_12G | MHZ_100, 992},
	{ 30, LO_PWR_10G | MHZ_156, 992},
	{ 32, LO_PWR_10G | MHZ_100, 992}
};

static void rxs_rio_pc_clk_pd_fail_test(void **state)
{
	RXS_test_state_t *l_st = *(RXS_test_state_t **)state;
	const int num_tests = sizeof(clk_pd_fail) / sizeof(clk_pd_fail[0]);
	uint32_t srv_pd;
	uint32_t i;

	// On real hardware, it is disasterous to mess with clocking
	// parameters, so just check that the current setup passes

	if (l_st->real_hw) {
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));

		return;
	}

	for (i = 0; i < num_tests; i++) {
		if (DEBUG_PRINTF) {
			printf("\ni = %d\n", i);
		}
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_PRESCALAR_SRV_CLK,
			clk_pd_fail[i].ps));
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info, RXS_MPM_CFGSIG0,
			clk_pd_fail[i].cfgsig0));
		assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_clk_pd(&mock_dev_info, &srv_pd));
		assert_int_equal(srv_pd, 0);
	}
}

static void compare_pc(rio_pc_one_port_config_t *pc,
			rio_pc_one_port_config_t *chk_pc)
{
	uint32_t lane;

	assert_int_equal(pc->pnum, chk_pc->pnum);
	assert_int_equal(pc->port_available, chk_pc->port_available);
	assert_int_equal(pc->powered_up, chk_pc->powered_up);
	assert_int_equal(pc->pw, chk_pc->pw);
	assert_int_equal(pc->ls, chk_pc->ls);
	assert_int_equal(pc->fc, chk_pc->fc);
	assert_int_equal(pc->iseq, chk_pc->iseq);
	assert_int_equal(pc->xmitter_disable, chk_pc->xmitter_disable);
	assert_int_equal(pc->port_lockout, chk_pc->port_lockout);
	assert_int_equal(pc->nmtc_xfer_enable, chk_pc->nmtc_xfer_enable);
	assert_int_equal(pc->tx_lswap, chk_pc->tx_lswap);
	assert_int_equal(pc->rx_lswap, chk_pc->rx_lswap);
	for (lane = 0; lane < RIO_MAX_PORT_LANES; lane++) {
		assert_int_equal(pc->tx_linvert[lane],
				chk_pc->tx_linvert[lane]);
		assert_int_equal(pc->rx_linvert[lane],
				chk_pc->rx_linvert[lane]);
	}
}
static void rxs_rio_pc_get_config_success(void **state)
{
	rio_pc_get_config_in_t pc_in;
	rio_pc_get_config_out_t pc_out;
	rio_pc_one_port_config_t curr_pc;

	uint32_t err_stat = RXS_SPX_ERR_STAT_DFLT;
	uint32_t ctl = RXS_SPX_CTL_DFLT;
	uint32_t plm_ctl = RXS_PLM_SPX_IMP_SPEC_CTL_DFLT;
	uint32_t pwdn = RXS_PLM_SPX_PWDN_CTL_DFLT;
	uint32_t pol = RXS_PLM_SPX_POL_CTL_DFLT;
	uint32_t lrto = 0;
	uint32_t nmtc_en_mask = RXS_SPX_CTL_INP_EN | RXS_SPX_CTL_OTP_EN;

	rio_port_t port;
	uint32_t lane;
	uint32_t l_vec;

	// Default register values have all ports unavailable
	curr_pc.port_available = false;
	curr_pc.powered_up = false;
	curr_pc.pw = rio_pc_pw_last;
	curr_pc.ls = rio_pc_ls_last;
	curr_pc.fc = rio_pc_fc_last;
	curr_pc.iseq = rio_pc_is_last;
	curr_pc.xmitter_disable = false;
	curr_pc.port_lockout = false;
	curr_pc.nmtc_xfer_enable = false;
	curr_pc.tx_lswap = rio_lswap_none;
	curr_pc.rx_lswap = rio_lswap_none;
	for (lane = 0; lane < RIO_MAX_PORT_LANES; lane++) {
		curr_pc.tx_linvert[lane] = false;
		curr_pc.rx_linvert[lane] = false;
	}

	// Check that the default register values line up with the above.
	pc_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), pc_out.num_ports);
	assert_int_equal(lrto, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);

	for (port = 0; port < pc_out.num_ports; port++) {
		curr_pc.pnum = port;
		compare_pc(&pc_out.pc[port], &curr_pc);
	}

	if (DEBUG_PRINTF) {
		printf("\nconfig 1\n");
	}
	// Make port available and powered down
	// Doesn't work this way on real hardware
	err_stat &= ~RXS_SPX_ERR_STAT_PORT_UNAVL;
	pwdn |= RXS_PLM_SPX_PWDN_CTL_PWDN_PORT;
	curr_pc.port_available = true;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_PLM_SPX_PWDN_CTL(port), pwdn));
	}
	// Also update LRTO value
	pc_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), pc_out.num_ports);
	assert_int_equal(lrto, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);

	for (port = 0; port < pc_out.num_ports; port++) {
		curr_pc.pnum = port;
		compare_pc(&pc_out.pc[port], &curr_pc);
	}

	if (DEBUG_PRINTF) {
		printf("\nconfig 2\n");
	}
	// Power up the port, with transmitter disabled
	// Doesn't work this way on real hardware
	ctl |= RXS_SPX_CTL_PORT_DIS;
	pwdn &= ~RXS_PLM_SPX_PWDN_CTL_PWDN_PORT;
	curr_pc.powered_up = true;
	curr_pc.xmitter_disable = true;
	curr_pc.pw = rio_pc_pw_4x;
	curr_pc.ls = rio_pc_ls_6p25;
	curr_pc.fc = rio_pc_fc_rx;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_CTL(port), ctl));
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_PLM_SPX_PWDN_CTL(port), pwdn));
	}

	pc_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), pc_out.num_ports);
	assert_int_equal(lrto, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);

	for (port = 0; port < pc_out.num_ports; port++) {
		curr_pc.pnum = port;
		compare_pc(&pc_out.pc[port], &curr_pc);
	}

	if (DEBUG_PRINTF) {
		printf("\nconfig 3\n");
	}
	// Enable transmitter, set lockout, disable non-maintenance
	// Doesn't work this way on real hardware
	ctl &= ~RXS_SPX_CTL_PORT_DIS;
	curr_pc.xmitter_disable = false;

	ctl &= ~nmtc_en_mask;
	ctl |= RXS_SPX_CTL_PORT_LOCKOUT;
	curr_pc.port_lockout = true;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_CTL(port), ctl));
	}

	pc_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), pc_out.num_ports);
	assert_int_equal(lrto, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);

	for (port = 0; port < pc_out.num_ports; port++) {
		curr_pc.pnum = port;
		compare_pc(&pc_out.pc[port], &curr_pc);
	}

	if (DEBUG_PRINTF) {
		printf("\nconfig 4\n");
	}
	// Clear lockout, enable non-maintenance
	// Doesn't work this way on real hardware
	ctl &= ~RXS_SPX_CTL_PORT_LOCKOUT;
	ctl |= nmtc_en_mask;
	curr_pc.port_lockout = false;
	curr_pc.nmtc_xfer_enable = true;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_CTL(port), ctl));
	}

	pc_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), pc_out.num_ports);
	assert_int_equal(lrto, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);

	for (port = 0; port < pc_out.num_ports; port++) {
		curr_pc.pnum = port;
		compare_pc(&pc_out.pc[port], &curr_pc);
	}

	if (DEBUG_PRINTF) {
		printf("\nconfig 5\n");
	}
	// Try various combinations of Tx/Rx swap, and lane inversion
	// l_vec is a bit mask of conditions for tx & rx swap,
	// and various lane inversions...
	for (l_vec = 0; l_vec < 1 << RIO_MAX_PORT_LANES; l_vec++) {
		plm_ctl &= ~RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_RX;
		plm_ctl &= ~RXS_PLM_SPX_IMP_SPEC_CTL_SWAP_TX;
		curr_pc.tx_lswap = rio_lswap_none;
		curr_pc.rx_lswap = rio_lswap_none;

		plm_ctl |= (l_vec & 0x3) << 16;
		curr_pc.rx_lswap = lswap(l_vec & 0x3);
		plm_ctl |= (l_vec & 0xC) << 16;
		curr_pc.tx_lswap =lswap((l_vec & 0xC) >> 2);
		pol = l_vec;
		pol |= ~(l_vec << 16) & RXS_PLM_SPX_POL_CTL_TX_ALL_POL;
		for (lane = 0; lane < RIO_MAX_PORT_LANES; lane++) {
			curr_pc.rx_linvert[lane] = l_vec & (1 << lane);
			curr_pc.tx_linvert[lane] = !(l_vec & (1 << lane));
		}
		for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
					RXS_PLM_SPX_IMP_SPEC_CTL(port),
					plm_ctl));
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
					RXS_PLM_SPX_POL_CTL(port), pol));
		}
		pc_in.ptl.num_ports = RIO_ALL_PORTS;
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
		assert_int_equal(NUM_RXS_PORTS(&mock_dev_info),
					pc_out.num_ports);
		assert_int_equal(lrto, pc_out.lrto);
		assert_int_equal(0, pc_out.log_rto);

		for (port = 0; port < pc_out.num_ports; port++) {
			curr_pc.pnum = port;
			compare_pc(&pc_out.pc[port], &curr_pc);
		}
	}

	(void)state;
}

static void rxs_rio_pc_get_config_bad_parms(void **state)
{
	rio_pc_get_config_in_t pc_in;
	rio_pc_get_config_out_t pc_out;

	// Bad number of ports...
	pc_in.ptl.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;
	pc_out.imp_rc = RIO_SUCCESS;
	pc_out.lrto = 0xFFFF;
	pc_out.log_rto = 0xFFFF;
	pc_out.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(0, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);
	assert_int_equal(0, pc_out.num_ports);

	// Bad port number
	pc_in.ptl.num_ports = 1;
	pc_in.ptl.pnums[0] = NUM_RXS_PORTS(&mock_dev_info);
	pc_out.imp_rc = RIO_SUCCESS;
	pc_out.lrto = 0xFFFF;
	pc_out.log_rto = 0xFFFF;
	pc_out.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_get_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);
	assert_int_equal(0, pc_out.lrto);
	assert_int_equal(0, pc_out.log_rto);
	assert_int_equal(0, pc_out.num_ports);
	(void)state;
}

static void adjust_ps_for_port(rio_pc_one_port_status_t *curr_ps,
				rio_port_t port)
{
	curr_ps->pnum = port;

	if (port & 1) {
		curr_ps->pw = rio_pc_pw_2x;
		curr_ps->num_lanes = 2;
		curr_ps->first_lane = ((port / 2) * 4) + 2;
	} else {
		curr_ps->pw = rio_pc_pw_4x;
		curr_ps->num_lanes = 4;
		curr_ps->first_lane = ((port / 2) * 4);
	}
}

static void compare_ps(rio_pc_one_port_status_t *ps,
			rio_pc_one_port_status_t *chk_ps)
{
	assert_int_equal(ps->pnum, chk_ps->pnum);
	assert_int_equal(ps->pw, chk_ps->pw);
	assert_int_equal(ps->fc, chk_ps->fc);
	assert_int_equal(ps->iseq, chk_ps->iseq);
	assert_int_equal(ps->port_error, chk_ps->port_error);
	assert_int_equal(ps->input_stopped, chk_ps->input_stopped);
	assert_int_equal(ps->output_stopped, chk_ps->output_stopped);
	assert_int_equal(ps->num_lanes, chk_ps->num_lanes);
	assert_int_equal(ps->first_lane, chk_ps->first_lane);
}

static void rxs_rio_pc_get_status_success(void **state)
{
	rio_pc_get_status_in_t ps_in;
	rio_pc_get_status_out_t ps_out;
	rio_pc_one_port_status_t curr_ps;

	rio_port_t port;
	uint32_t pw;
	uint32_t lane_adj = 0;

	uint32_t err_stat = RXS_SPX_ERR_STAT_DFLT;
	uint32_t ctl = RXS_SPX_CTL_DFLT;

	// Default register values have all ports uninitialized
	curr_ps.port_ok = false;
	curr_ps.pw = rio_pc_pw_last;
	curr_ps.fc = rio_pc_fc_last;
	curr_ps.iseq = rio_pc_is_last;
	curr_ps.port_error = false;
	curr_ps.input_stopped = false;
	curr_ps.output_stopped = false;
	curr_ps.num_lanes = 0;
	curr_ps.first_lane = 0;

	// Check that the default register values line up with the above.
	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		curr_ps.pnum = port;
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 1\n");
	}
	// Make link uninitialized
	// Doesn't work this way on real hardware
	// Must read/modify/write err_stat to preserve odd & even
	// port initialization differences
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		err_stat &= ~RXS_SPX_ERR_STAT_PORT_UNAVL;
		err_stat |= RXS_SPX_ERR_STAT_PORT_UNINIT;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
	}

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		curr_ps.pnum = port;
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 2\n");
	}
	// Make port initialized, with a port error
	// Doesn't work this way on real hardware
	curr_ps.port_ok = true;
	curr_ps.port_error = true;
	curr_ps.fc = rio_pc_fc_rx;
	curr_ps.iseq = rio_pc_is_two;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		err_stat &= ~RXS_SPX_ERR_STAT_PORT_UNINIT;
		err_stat |= RXS_SPX_ERR_STAT_PORT_OK |
				RXS_SPX_ERR_STAT_PORT_ERR;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
	}

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		adjust_ps_for_port(&curr_ps, port);
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 3\n");
	}
	// Clear port error, add input error
	// Doesn't work this way on real hardware
	curr_ps.port_error = false;
	curr_ps.input_stopped = true;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		err_stat &= ~RXS_SPX_ERR_STAT_PORT_ERR;
		err_stat |= RXS_SPX_ERR_STAT_INPUT_ERR_STOP;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
	}

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		adjust_ps_for_port(&curr_ps, port);
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 4\n");
	}
	// Clear input error, add output error
	// Doesn't work this way on real hardware
	curr_ps.input_stopped = false;
	curr_ps.output_stopped = true;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		err_stat &= ~RXS_SPX_ERR_STAT_INPUT_ERR_STOP;
		err_stat |= RXS_SPX_ERR_STAT_OUTPUT_ERR_STOP;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
	}

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		adjust_ps_for_port(&curr_ps, port);
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 5\n");
	}
	// Add input error and port fail (port error)
	// Doesn't work this way on real hardware
	curr_ps.input_stopped = true;
	curr_ps.output_stopped = true;
	curr_ps.port_error = true;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		err_stat |= RXS_SPX_ERR_STAT_INPUT_ERR_STOP;
		err_stat |= RXS_SPX_ERR_STAT_OUTPUT_ERR_STOP;
		err_stat |= RXS_SPX_ERR_STAT_OUTPUT_FAIL;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
		// Must read/modify/write ctl to avoid complexity with which
		// port widths are enabled...
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_CTL(port), &ctl));
		ctl |= RXS_SPX_CTL_STOP_FAIL_EN;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_CTL(port), ctl));
	}

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		adjust_ps_for_port(&curr_ps, port);
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 6\n");
	}
	// Remove input/output error and port fail (port error)
	// Doesn't work this way on real hardware
	curr_ps.input_stopped = false;
	curr_ps.output_stopped = false;
	curr_ps.port_error = false;

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), &err_stat));
		err_stat &= ~RXS_SPX_ERR_STAT_INPUT_ERR_STOP;
		err_stat &= ~RXS_SPX_ERR_STAT_OUTPUT_ERR_STOP;
		err_stat &= ~RXS_SPX_ERR_STAT_OUTPUT_FAIL;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_ERR_STAT(port), err_stat));
		// Must read/modify/write ctl to avoid complexity with which
		// port widths are enabled...
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_CTL(port), &ctl));
		ctl |= RXS_SPX_CTL_STOP_FAIL_EN;
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
				RXS_SPX_CTL(port), ctl));
	}

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(NUM_RXS_PORTS(&mock_dev_info), ps_out.num_ports);

	for (port = 0; port < ps_out.num_ports; port++) {
		adjust_ps_for_port(&curr_ps, port);
		compare_ps(&ps_out.ps[port], &curr_ps);
	}

	if (DEBUG_PRINTF) {
		printf("\nstatus 7\n");
	}
	// Try various link initialization values for the even (4x) ports

	for (pw = 0; pw < 6; pw++) {
		for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port += 2)
		{
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
					RXS_SPX_CTL(port), &ctl));

			// Ensure no port width overrides are active.
			ctl &= ~RXS_SPX_CTL_OVER_PWIDTH;
			ctl &= ~RXS_SPX_CTL_INIT_PWIDTH;
			switch (pw) {
			case 0:
				lane_adj = 0;
				curr_ps.pw = rio_pc_pw_2x;
				curr_ps.num_lanes = 2;
				ctl |= RIO_SPX_CTL_PTW_INIT_2X;
				break;
			case 1:
				lane_adj = 2;
				curr_ps.pw = rio_pc_pw_1x_l2;
				curr_ps.num_lanes = 1;
				ctl |= RIO_SPX_CTL_PTW_INIT_1X_LR;
				break;
			case 2:
				lane_adj = 0;
				curr_ps.pw = rio_pc_pw_1x_l0;
				curr_ps.num_lanes = 1;
				ctl |= RIO_SPX_CTL_PTW_INIT_1X_L0;
				break;
			case 3:
				lane_adj = 0;
				curr_ps.pw = rio_pc_pw_2x;
				curr_ps.num_lanes = 2;
				ctl |= RIO_SPX_CTL_PTW_INIT_2X;
				ctl &= ~RIO_SPX_CTL_PTW_MAX_4X;
				break;
			case 4:
				lane_adj = 1;
				curr_ps.pw = rio_pc_pw_1x_l1;
				curr_ps.num_lanes = 1;
				ctl |= RIO_SPX_CTL_PTW_INIT_1X_LR;
				break;
			case 5:
				lane_adj = 0;
				curr_ps.pw = rio_pc_pw_1x_l0;
				curr_ps.num_lanes = 1;
				ctl |= RIO_SPX_CTL_PTW_INIT_1X_L0;
				break;
			default:
				assert_false(true);
			}
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
					RXS_SPX_CTL(port), ctl));
		}
		ps_in.ptl.num_ports = RIO_ALL_PORTS;
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
		assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
		assert_int_equal(NUM_RXS_PORTS(&mock_dev_info),
							ps_out.num_ports);

		// Only check even ports
		for (port = 0; port < ps_out.num_ports; port += 2) {
			curr_ps.pnum = port;
			curr_ps.first_lane = (port * 4) + lane_adj;
			compare_ps(&ps_out.ps[port], &curr_ps);
		}
	}
	// Try various link initialization values for the odd (2x) ports

	if (DEBUG_PRINTF) {
		printf("\nstatus 8\n");
	}
	for (pw = 0; pw < 2; pw++) {
		for (port = 1; port < NUM_RXS_PORTS(&mock_dev_info); port += 2)
		{
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
					RXS_SPX_CTL(port), &ctl));
			ctl &= ~RXS_SPX_CTL_OVER_PWIDTH;
			ctl &= ~RXS_SPX_CTL_INIT_PWIDTH;
			switch (pw) {
			case 0:
				curr_ps.pw = rio_pc_pw_1x_l1;
				curr_ps.num_lanes = 1;
				ctl |= RIO_SPX_CTL_PTW_INIT_1X_LR;
				break;
			case 1:
				curr_ps.pw = rio_pc_pw_1x_l0;
				curr_ps.num_lanes = 1;
				ctl |= RIO_SPX_CTL_PTW_INIT_1X_L0;
				break;
			default:
				assert_false(true);
			}
			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
					RXS_SPX_CTL(port), ctl));
		}
		ps_in.ptl.num_ports = RIO_ALL_PORTS;
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
		assert_int_equal(RIO_SUCCESS, ps_out.imp_rc);
		assert_int_equal(NUM_RXS_PORTS(&mock_dev_info),
							ps_out.num_ports);

		// Only check odd ports
		for (port = 1; port < ps_out.num_ports; port += 2) {
			curr_ps.pnum = port;
			compare_ps(&ps_out.ps[port], &curr_ps);
		}
	}

	(void)state;
}

static void rxs_rio_pc_get_status_bad_parms(void **state)
{
	rio_pc_get_status_in_t ps_in;
	rio_pc_get_status_out_t ps_out;

	// Bad number of ports...
	ps_in.ptl.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;
	ps_out.imp_rc = RIO_SUCCESS;
	ps_out.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_not_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(0, ps_out.num_ports);

	// Bad port number
	ps_in.ptl.num_ports = 1;
	ps_in.ptl.pnums[0] = NUM_RXS_PORTS(&mock_dev_info);
	ps_out.imp_rc = RIO_SUCCESS;
	ps_out.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_get_status(&mock_dev_info, &ps_in, &ps_out));
	assert_int_not_equal(RIO_SUCCESS, ps_out.imp_rc);
	assert_int_equal(0, ps_out.num_ports);
	(void)state;
}

static void rxs_rio_pc_reset_port_bad_parms(void **state)
{
	rio_pc_reset_port_in_t pc_in;
	rio_pc_reset_port_out_t pc_out;

	// Bad port number...
	pc_in.port_num = NUM_RXS_PORTS(&mock_dev_info);
	pc_in.oob_reg_acc = true;
	pc_in.reg_acc_port = 0;
	pc_in.reset_lp = true;
	pc_in.preserve_config = true;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
			rxs_rio_pc_reset_port(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	(void)state;
}

// Test that the register access port is never reset.
static void rxs_rio_pc_reset_port_exclude_reg_acc(void **state)
{
	rio_pc_reset_port_in_t pc_in;
	rio_pc_reset_port_out_t pc_out;
	uint32_t chk_data;
	uint32_t all_1 = 0xFFFFFFFF;
	rio_port_t port, t_pt;

	// Test on one port at a time.
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Power up the port so it can be reset
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);

		// Set some status on the register access port
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
						RXS_SPX_ERR_DET(port), all_1));
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_not_equal(0, chk_data);

		// Clear link management response status
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));

		// Try to reset the register access port
		pc_in.port_num = port;
		pc_in.oob_reg_acc = false;
		pc_in.reg_acc_port = port;
		pc_in.reset_lp = true;
		pc_in.preserve_config = true;
		pc_out.imp_rc = RIO_SUCCESS;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_reset_port(&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		// Check that the port was not reset, by confirming that the
		// status bits set remain set.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_not_equal(0, chk_data);

		// Check that link partner was not reset
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));
		assert_int_equal(0, chk_data);
	}

	// All ports now have err detect bits set.  Try resetting all ports
	// and verify that the register access port is preserved.

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Try to reset the register access port
		pc_in.port_num = RIO_ALL_PORTS;
		pc_in.oob_reg_acc = false;
		pc_in.reg_acc_port = port;
		pc_in.reset_lp = true;
		pc_in.preserve_config = true;
		pc_out.imp_rc = RIO_SUCCESS;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_reset_port(&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		for (t_pt = 0; t_pt < NUM_RXS_PORTS(&mock_dev_info); t_pt++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
					RXS_SPX_ERR_DET(t_pt), &chk_data));
			if (t_pt == port) {
				// Verify status is preserved on register
				// access port
				assert_int_not_equal(0, chk_data);

				// Check that a reset was not sent to
				// the link partner.
				assert_int_equal(RIO_SUCCESS,
					DARRegRead(&mock_dev_info,
						RXS_SPX_LM_RESP(t_pt),
						&chk_data));
				assert_int_equal(0, chk_data);
				continue;
			}
			// Verify status is cleared on all other ports, and that
			// a reset was sent to the link partner,
			// and then make the status non-zero in preparation for
			// the next test loop.
			assert_int_equal(0, chk_data);
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
					RXS_SPX_LM_RESP(t_pt), &chk_data));
			assert_int_equal(RIO_SPX_LM_RESP_VLD, chk_data);

			assert_int_equal(RIO_SUCCESS,
				DARRegWrite(&mock_dev_info,
					RXS_SPX_ERR_DET(t_pt), all_1));
		}
	}

	// All ports still have the status bits set.
	// Try resetting all ports and verify that if oob register access is
	// used, that all ports are reset.
	pc_in.port_num = RIO_ALL_PORTS;
	pc_in.oob_reg_acc = true;
	pc_in.reg_acc_port = 0;
	pc_in.reset_lp = false;
	pc_in.preserve_config = true;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_equal(RIO_SUCCESS,
		rxs_rio_pc_reset_port(&mock_dev_info, &pc_in, &pc_out));
	assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_SPX_ERR_DET(port), &chk_data));
		// Verify status is cleared on all ports,
		// and that a reset was not sent to the link partner.
		assert_int_equal(0, chk_data);
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
					RXS_SPX_LM_RESP(port), &chk_data));
		assert_int_equal(0, chk_data);
	}

	(void)state;
}

static void rxs_rio_pc_reset_link_partner_bad_parms(void **state)
{
	rio_pc_reset_link_partner_in_t pc_in;
	rio_pc_reset_link_partner_out_t pc_out;

	// Bad port number...
	pc_in.port_num = NUM_RXS_PORTS(&mock_dev_info);
	pc_in.resync_ackids = true;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_reset_link_partner(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	(void)state;
}

// Test that the link partner is reset, and the local port is reset,
// according to the input parameters.
static void rxs_rio_pc_reset_link_partner_success(void **state)
{
	rio_pc_reset_link_partner_in_t pc_in;
	rio_pc_reset_link_partner_out_t pc_out;
	uint32_t chk_data;
	uint32_t all_1 = 0xFFFFFFFF;
	rio_port_t port;

	// Test on one port at a time.
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Power up the port so it can be reset
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);

		// Set some status on the port under test
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
						RXS_SPX_ERR_DET(port), all_1));
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_not_equal(0, chk_data);

		// Clear link management response status
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));
		// Reset the link partner and resync_ackids
		pc_in.port_num = port;
		pc_in.resync_ackids = true;
		pc_out.imp_rc = RIO_SUCCESS;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_reset_link_partner(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		// Check that the port was reset,
		// by confirming that the status bits are cleared.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_equal(0, chk_data);

		// Check that link partner was reset
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));
		assert_int_equal(RIO_SPX_LM_RESP_VLD, chk_data);
	}

	// Try again, verifying that the local port was no reset.

	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Set some status on the port under test
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
						RXS_SPX_ERR_DET(port), all_1));
		pc_in.port_num = port;
		pc_in.resync_ackids = false;
		pc_out.imp_rc = RIO_SUCCESS;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_reset_link_partner(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		// Verify the local port was not reset.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
					RXS_SPX_ERR_DET(port), &chk_data));
		assert_int_not_equal(0, chk_data);

		// Check that a reset was sent to the link partner.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
					RXS_SPX_LM_RESP(port), &chk_data));
		assert_int_equal(RIO_SPX_LM_RESP_VLD, chk_data);
	}
	(void)state;
}

static void rxs_rio_pc_clr_errs_bad_parms(void **state)
{
	rio_pc_clr_errs_in_t pc_in;
	rio_pc_clr_errs_out_t pc_out;

	// Bad port number...
	memset(&pc_in, 0, sizeof(pc_in));
	pc_in.port_num = NUM_RXS_PORTS(&mock_dev_info);
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_clr_errs(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	// Clear link partner errors without link partner information
	memset(&pc_in, 0, sizeof(pc_in));
	pc_in.port_num = 0;
	pc_in.clr_lp_port_err = true;
	pc_in.lp_dev_info = NULL;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_clr_errs(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	// Clear link partner errors without link partner port hints
	memset(&pc_in, 0, sizeof(pc_in));
	pc_in.port_num = 0;
	pc_in.clr_lp_port_err = true;
	pc_in.lp_dev_info = &mock_dev_info;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_clr_errs(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	(void)state;
}

// Test that the local port is reset, and no ackID synchronization is performed
static void rxs_rio_pc_clr_errs_success(void **state)
{
	rio_pc_clr_errs_in_t pc_in;
	rio_pc_clr_errs_out_t pc_out;
	uint32_t chk_data;
	uint32_t all_1 = 0xFFFFFFFF;
	rio_port_t port;

	// Test on one port at a time.
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Power up the port
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);

		// Set some status on the port under test
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
						RXS_SPX_ERR_DET(port), all_1));
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_not_equal(0, chk_data);

		// Clear link management response status
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));
		// Reset the this port, do not clear link partner errors
		pc_in.port_num = port;
		pc_in.clr_lp_port_err = false;
		pc_out.imp_rc = RIO_SUCCESS;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_clr_errs(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		// Check that the port was reset,
		// by confirming that the status bits are cleared.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_equal(0, chk_data);

		// Check that link partner was not touched...
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));
		assert_int_equal(0, chk_data);
	}

	(void)state;
}

// Test that the local port is reset, and ackID synchronization is performed
static void rxs_rio_pc_clr_errs_resync_ackids(void **state)
{
	rio_pc_clr_errs_in_t pc_in;
	rio_pc_clr_errs_out_t pc_out;
	uint32_t chk_data;
	uint32_t all_1 = 0xFFFFFFFF;
	rio_port_t port;
	uint32_t rc;

	// Test on one port at a time.
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		if (DEBUG_PRINTF) {
			printf("\nport = %d\n", port);
		}

		// Power up the port
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);

		// Set some status on the port under test
		assert_int_equal(RIO_SUCCESS,
			DARRegWrite(&mock_dev_info,
						RXS_SPX_ERR_DET(port), all_1));
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_not_equal(0, chk_data);

		// Clear link management response status
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_LM_RESP(port),
								&chk_data));
		// Reset the this port, clear link partner errors
		pc_in.port_num = port;
		pc_in.clr_lp_port_err = true;
		pc_in.lp_dev_info = &mock_lp_dev_info;
		pc_in.num_lp_ports = 1;
		pc_in.lp_port_list[0] = 0;
		pc_out.imp_rc = RIO_SUCCESS;

		rc = rxs_rio_pc_clr_errs(&mock_dev_info, &pc_in, &pc_out);
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
		assert_int_equal(RIO_SUCCESS, rc);

		// Check that the port was reset,
		// by confirming that the status bits are cleared.
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_ERR_DET(port),
								&chk_data));
		assert_int_equal(0, chk_data);

		// Check that inbound and outbound ackIDs have been resynced
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_IN_ACKID_CSR(port),
								&chk_data));
		assert_int_equal(ACKID_CAP_BASE + port, chk_data);
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_OUT_ACKID_CSR(port),
								&chk_data));
		assert_int_equal(0x8003f03f, chk_data);
	}

	(void)state;
}

static void rxs_rio_pc_secure_port_bad_parms(void **state)
{
	rio_pc_secure_port_in_t pc_in;
	rio_pc_secure_port_out_t pc_out;

	// Bad number of ports
	pc_in.ptl.num_ports = NUM_RXS_PORTS(&mock_dev_info) + 1;
	pc_in.mtc_pkts_allowed = true;
	pc_in.MECS_participant = true;
	pc_in.MECS_acceptance = true;
	pc_in.rst = rio_pc_rst_device;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_secure_port(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	// Bad port number
	pc_in.ptl.num_ports = 1;
	pc_in.ptl.pnums[0] = NUM_RXS_PORTS(&mock_dev_info);
	pc_in.mtc_pkts_allowed = true;
	pc_in.MECS_participant = true;
	pc_in.MECS_acceptance = true;
	pc_in.rst = rio_pc_rst_device;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_secure_port(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	// Bad device reset configuration
	pc_in.ptl.num_ports = NUM_RXS_PORTS(&mock_dev_info);
	pc_in.mtc_pkts_allowed = true;
	pc_in.MECS_participant = true;
	pc_in.MECS_acceptance = true;
	pc_in.rst = rio_pc_rst_last;
	pc_out.imp_rc = RIO_SUCCESS;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_secure_port(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	(void)state;
}

// Test register settings for each control value.
static void rxs_rio_pc_secure_port_success(void **state)
{
	rio_pc_secure_port_in_t pc_in;
	rio_pc_secure_port_out_t pc_out;
	uint32_t temp;
	rio_port_t port;
	uint32_t plm_ctl_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
	uint32_t ctl_mask = RXS_SPX_CTL_MULT_CS;
	uint32_t filt_mask = RXS_TLM_SPX_FTYPE_FILT_MTC;

	// Test on one port at a time.
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Power up the port
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);

		// Turn all items off

		pc_in.ptl.num_ports = 1;
		pc_in.ptl.pnums[0] = port;
		pc_in.mtc_pkts_allowed = false;
		pc_in.MECS_participant = false;
		pc_in.MECS_acceptance = false;
		pc_in.rst = rio_pc_rst_device;
		pc_out.imp_rc = RIO_SUCCESS;

		if (DEBUG_PRINTF) {
			printf("\nrst = %d %s\n",
				pc_in.rst, rst_to_str[pc_in.rst]);
		}
		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_secure_port(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		assert_false(pc_out.bc_mtc_pkts_allowed);
		assert_false(pc_out.MECS_participant);
		assert_true(pc_out.MECS_acceptance);
		assert_int_equal(rio_pc_rst_device, pc_out.rst);

		// Check register values for MECS, filter, and reset...

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_CTL(port), &temp));
		assert_int_equal(0, temp & ctl_mask);

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_TLM_SPX_FTYPE_FILT(port),
								 &temp));
		assert_int_equal(filt_mask, temp & filt_mask);

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_PLM_SPX_IMP_SPEC_CTL(port), &temp));
		assert_int_equal(plm_ctl_mask, temp & plm_ctl_mask);

		// Turn all items on

		pc_in.ptl.num_ports = 1;
		pc_in.ptl.pnums[0] = port;
		pc_in.mtc_pkts_allowed = true;
		pc_in.MECS_participant = true;
		pc_in.MECS_acceptance = true;
		pc_in.rst = rio_pc_rst_device;
		pc_out.imp_rc = RIO_SUCCESS;

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_secure_port(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		assert_true(pc_out.bc_mtc_pkts_allowed);
		assert_true(pc_out.MECS_participant);
		assert_true(pc_out.MECS_acceptance);
		assert_int_equal(rio_pc_rst_device, pc_out.rst);

		// Check register values for MECS, filter, and reset...

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_SPX_CTL(port), &temp));
		assert_int_equal(ctl_mask, temp & ctl_mask);

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_TLM_SPX_FTYPE_FILT(port), &temp));
		assert_int_equal(0, temp & filt_mask);

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_PLM_SPX_IMP_SPEC_CTL(port), &temp));
		assert_int_equal(plm_ctl_mask, temp & plm_ctl_mask);
	}

	(void)state;
}

// Test register settings for each reset control value
static void rxs_rio_pc_secure_port_rst_cfg(void **state)
{
	rio_pc_secure_port_in_t pc_in;
	rio_pc_secure_port_out_t pc_out;
	uint32_t temp;
	rio_port_t port;
	uint32_t plm_ctl_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
	uint32_t port_mask;
	uint32_t exp_plm_ctl = 0;
	uint32_t exp_em_int = 0;
	uint32_t exp_em_pw = 0;

	// Test on one port at a time.
	// Change reset configuration based on port number
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		// Power up the port
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);

		// Test reset port configuration
		pc_in.ptl.num_ports = 1;
		pc_in.ptl.pnums[0] = port;
		pc_in.mtc_pkts_allowed = false;
		pc_in.MECS_participant = false;
		pc_in.MECS_acceptance = false;
		pc_in.rst = (rio_pc_rst_handling)(port % rio_pc_rst_last);
		pc_out.imp_rc = RIO_SUCCESS;

		if (DEBUG_PRINTF) {
			printf("\nport %d rst = %d %s\n",
				port, pc_in.rst, rst_to_str[pc_in.rst]);
		}
		port_mask = 1 << port;
		switch (pc_in.rst) {
		case rio_pc_rst_device:
			exp_plm_ctl = plm_ctl_mask;
			exp_em_int = 0;
			exp_em_pw = 0;
			break;

		case rio_pc_rst_port:
			exp_plm_ctl = RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
			exp_em_int = 0;
			exp_em_pw = 0;
			break;

		case rio_pc_rst_int:
			exp_plm_ctl = 0;
			exp_em_int = port_mask;
			exp_em_pw = 0;
			break;

		case rio_pc_rst_pw:
			exp_plm_ctl = 0;
			exp_em_int = 0;
			exp_em_pw = port_mask;
			break;

		case rio_pc_rst_ignore:
			exp_plm_ctl = 0;
			exp_em_int = 0;
			exp_em_pw = 0;
			break;

		default:
			assert_true(false);
		}

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_secure_port(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);

		assert_false(pc_out.bc_mtc_pkts_allowed);
		assert_false(pc_out.MECS_participant);
		assert_true(pc_out.MECS_acceptance);
		assert_int_equal(pc_in.rst, pc_out.rst);

		// Check register values for reset...

		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info,
				RXS_PLM_SPX_IMP_SPEC_CTL(port), &temp));
		assert_int_equal(exp_plm_ctl, temp & plm_ctl_mask);
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_EM_RST_INT_EN, &temp));
		assert_int_equal(exp_em_int, temp & port_mask);
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_EM_RST_PW_EN, &temp));
		assert_int_equal(exp_em_pw, temp & port_mask);

	}

	(void)state;
}

static void rxs_rio_pc_dev_reset_config_bad_parms(void **state)
{
	rio_pc_dev_reset_config_in_t pc_in;
	rio_pc_dev_reset_config_out_t pc_out;

	// Bad device reset configuration
	pc_in.rst = rio_pc_rst_last;
	pc_out.imp_rc = RIO_SUCCESS;
	pc_out.rst = rio_pc_rst_last;

	assert_int_not_equal(RIO_SUCCESS,
		rxs_rio_pc_dev_reset_config(&mock_dev_info, &pc_in, &pc_out));
	assert_int_not_equal(RIO_SUCCESS, pc_out.imp_rc);

	(void)state;
}

#define INCR_RST(x) ((rio_pc_rst_handling)((unsigned int)(x) + 1))
// Test register settings for each reset control value
static void rxs_rio_pc_dev_reset_config_rst_cfg(void **state)
{
	rio_pc_dev_reset_config_in_t pc_in;
	rio_pc_dev_reset_config_out_t pc_out;
	uint32_t temp;
	rio_port_t port;
	uint32_t plm_ctl_mask = RXS_PLM_SPX_IMP_SPEC_CTL_SELF_RST |
				RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
	uint32_t port_mask = (1 << NUM_RXS_PORTS(&mock_dev_info)) - 1;
	uint32_t exp_plm_ctl;
	uint32_t exp_em_int;
	uint32_t exp_em_pw;
	rio_pc_rst_handling rst;

	// Power up all ports
	for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
		set_all_port_config(cfg_perfect, NO_TTL, NO_FILT, port);
	}

	// Test each configuration
	for (rst = rio_pc_rst_device; rst < rio_pc_rst_last; rst = INCR_RST(rst)) {
		if (DEBUG_PRINTF) {
			printf("\nrst = %d %s\n", rst, rst_to_str[rst]);
		}
		// Test reset port configuration
		pc_in.rst = rst;
		pc_out.imp_rc = RIO_SUCCESS;
		pc_out.rst = rio_pc_rst_last;

		switch (pc_in.rst) {
		case rio_pc_rst_device:
			exp_plm_ctl = plm_ctl_mask;
			exp_em_int = 0;
			exp_em_pw = 0;
			break;

		case rio_pc_rst_port:
			exp_plm_ctl = RXS_PLM_SPX_IMP_SPEC_CTL_PORT_SELF_RST;
			exp_em_int = 0;
			exp_em_pw = 0;
			break;

		case rio_pc_rst_int:
			exp_plm_ctl = 0;
			exp_em_int = port_mask;
			exp_em_pw = 0;
			break;

		case rio_pc_rst_pw:
			exp_plm_ctl = 0;
			exp_em_int = 0;
			exp_em_pw = port_mask;
			break;

		case rio_pc_rst_ignore:
			exp_plm_ctl = 0;
			exp_em_int = 0;
			exp_em_pw = 0;
			break;

		default:
			assert_true(false);
		}

		assert_int_equal(RIO_SUCCESS,
			rxs_rio_pc_dev_reset_config(
					&mock_dev_info, &pc_in, &pc_out));
		assert_int_equal(RIO_SUCCESS, pc_out.imp_rc);
		assert_int_equal(pc_in.rst, pc_out.rst);

		// Check register values for reset...

		for (port = 0; port < NUM_RXS_PORTS(&mock_dev_info); port++) {
			assert_int_equal(RIO_SUCCESS,
				DARRegRead(&mock_dev_info,
					RXS_PLM_SPX_IMP_SPEC_CTL(port), &temp));
			assert_int_equal(exp_plm_ctl, temp & plm_ctl_mask);
		}
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_EM_RST_INT_EN, &temp));
		assert_int_equal(exp_em_int, temp);
		assert_int_equal(RIO_SUCCESS,
			DARRegRead(&mock_dev_info, RXS_EM_RST_PW_EN, &temp));
		assert_int_equal(exp_em_pw, temp);
	}

	(void)state;
}

int main(int argc, char** argv)
{
	const struct CMUnitTest tests[] = {
			cmocka_unit_test(rxs_rio_pc_macros_test),
			cmocka_unit_test_setup(
				rxs_rio_pc_clk_pd_success_test, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_clk_pd_fail_test, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_get_config_success, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_get_config_bad_parms, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_get_status_success, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_get_status_bad_parms, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_reset_port_bad_parms, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_reset_port_exclude_reg_acc, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_reset_link_partner_bad_parms, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_reset_link_partner_success, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_clr_errs_bad_parms, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_clr_errs_success, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_clr_errs_resync_ackids, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_secure_port_bad_parms, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_secure_port_success, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_secure_port_rst_cfg, setup),

			cmocka_unit_test_setup(
				rxs_rio_pc_dev_reset_config_bad_parms, setup),
			cmocka_unit_test_setup(
				rxs_rio_pc_dev_reset_config_rst_cfg, setup),
			};

	memset(&st, 0, sizeof(st));
	st.argc = argc;
	st.argv = argv;

	return cmocka_run_group_tests(tests, grp_setup, grp_teardown);
}

#endif /* RXS_DAR_WANTED */

#ifdef __cplusplus
}
#endif
