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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <DAR_RegDefs.h>
#include <CPS1848_registers.h>
#include <IDT_Tsi721.h>
#include <tsi721_config.h>



#include "rapidio_mport_lib.h"

#include "tsi721_config.h"

static int r_721(int fd, uint32_t offset, uint32_t *value)
{
		return riodp_lcfg_read(fd, offset, sizeof(uint32_t), value);
};

static int w_721(int fd, uint32_t offset, uint32_t value)
{
		return riodp_lcfg_write(fd, offset, sizeof(uint32_t), value);
};

static int r_mtc(int fd, uint32_t offset, uint32_t *value)
{
		return riodp_maint_read(fd, 0, 0, offset, sizeof(uint32_t), value);
};

static int w_mtc(int fd, uint32_t offset, uint32_t value)
{
		return riodp_maint_write(fd, 0, 0, offset, sizeof(uint32_t), value);
};

int check_port_ok (int fd, int debug)
{
	int limit = 100000;
	uint32_t regval = 0;
		
	if (r_721(fd, TSI721_RIO_SP_ERR_STAT, &regval))
		goto fail;

	while (!(regval & TSI721_RIO_SP_ERR_STAT_PORT_OK) && limit) {
		if (r_721(fd, TSI721_RIO_SP_ERR_STAT, &regval))
			goto fail;
		limit--;
	};
	if (debug)
		printf("\n\tfd %d check_port_ok: 0x%8x", 
		fd, regval & TSI721_RIO_SP_ERR_STAT_PORT_OK);
fail:
	return regval & TSI721_RIO_SP_ERR_STAT_PORT_OK;
};

int check_port_err_free(int fd, int debug)
{
	uint32_t regval;
	uint32_t rc = 1;
		
	// Clear all old status bits, then check current error status
	if (w_721(fd, TSI721_RIO_SP_ERR_STAT, 0xFFFFFFFF))
		goto fail;
	if (r_721(fd, TSI721_RIO_SP_ERR_STAT, &regval))
		goto fail;

	rc = regval & (TSI721_RIO_SP_ERR_STAT_PORT_ERR |
	TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP |
	TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP |
	TSI721_RIO_SP_ERR_STAT_OUTPUT_FAIL);
	if (debug)
		printf("\n\tfd %d check_port_err_free: 0x%8x", 
		fd, rc);
fail:
	return rc;
};

// Clean up link using reset port
uint32_t reset_tsi721_rapidio(int fd, int debug, int lp)
{
	uint32_t rc = 1;
	uint32_t ctl2, ctl2_saved;
	uint32_t devctl, devctl_saved;
	uint32_t plmctl, plmctl_saved;

	if (r_721( fd, TSI721_RIO_SP_CTL2, &ctl2_saved ))
		goto exit;
	if (r_721( fd, TSI721_DEVCTL, &devctl_saved ))
		goto exit;
	if (r_721( fd, TSI721_RIO_PLM_SP_IMP_SPEC_CTL, &plmctl_saved ))
		goto exit;

	ctl2 = ctl2_saved ^ (TSI721_RIO_SP_CTL2_GB_6p25_EN |
				TSI721_RIO_SP_CTL2_GB_5p0_EN |
				TSI721_RIO_SP_CTL2_GB_3p125_EN |
				TSI721_RIO_SP_CTL2_GB_2p5_EN |
				TSI721_RIO_SP_CTL2_GB_1p25_EN);
	plmctl = plmctl_saved & ~(TSI721_RIO_PLM_SP_IMP_SPEC_CTL_SELF_RST |
		TSI721_RIO_PLM_SP_IMP_SPEC_CTL_PORT_SELF_RST);
	devctl = devctl_saved & ~TSI721_DEVCTL_SR_RST_MODE;

	plmctl |= TSI721_RIO_PLM_SP_IMP_SPEC_CTL_PORT_SELF_RST;
	devctl |= TSI721_DEVCTL_SR_RST_MODE_SRIO_ONLY;

	// Configure RapidIO port-only reset
	if (w_721( fd, TSI721_DEVCTL, devctl ))
		goto exit;
	if (w_721( fd, TSI721_RIO_PLM_SP_IMP_SPEC_CTL, plmctl ))
		goto exit;

	/* If should reset the link partner, send the reset now.
	*/
	
	if (lp) {
		uint32_t temp;
		if (r_721(fd, TSI721_RIO_SP_LM_RESP, &temp ))
			goto exit;
		if (w_721(fd, TSI721_RIO_SP_LM_REQ, RIO_SPX_LM_REQ_CMD_RESET))
			goto exit;
		if (r_721(fd, TSI721_RIO_SP_LM_RESP, &temp ))
			goto exit;
		if (!(temp & TSI721_RIO_SP_LM_RESP_RESP_VLD)) {
			printf("\n\tReset switch failed: 0x%8x", temp);
			goto exit;
		};
	};

	/* Trigger a port reset by blipping the enabled baudrates.
	*/

	if (w_721( fd, TSI721_RIO_SP_CTL2, ctl2 ))
		goto exit;       

	if (w_721( fd, TSI721_RIO_SP_CTL2, ctl2_saved ))
		goto exit;       
	rc = 0;
exit:
	return rc;
};

int config_cps_gen2_pre(int fd, int debug)
{
	uint32_t regval;
	int rc = 1;

	// Attempt to set the switch into "reset port only" mode
	if (0 != (rc = r_mtc( fd, CPS1848_DEVICE_CTL_1, &regval)))
		goto exit;
	regval |= CPS1848_DEVICE_CTL_1_PORT_RST_CTL;
	if (0 != (rc = w_mtc( fd, CPS1848_DEVICE_CTL_1, regval)))
		goto exit;
exit:
	return rc;
};

/* Switch port should have been reset, now to configure switch.
*/

int config_cps_gen2(int fd, int debug, uint8_t destid)
{
	uint32_t regval;
	int port;
	int rc = 1;

	// First, learn our port number
	if (0 != (rc = r_mtc( fd, CPS1848_SWITCH_PORT_INF_CAR, &regval)))
		goto exit;
	port = regval & CPS1848_SWITCH_PORT_INF_CAR_PORT;

	// Make sure input/output enabled is set
	if (0 != (rc = r_mtc( fd, CPS1848_PORT_X_CTL_1_CSR(port), &regval)))
		goto exit;

	regval |= CPS1848_PORT_X_CTL_1_CSR_INPUT_PORT_EN |
		CPS1848_PORT_X_CTL_1_CSR_OUTPUT_PORT_EN;

	if (0 != (rc = w_mtc( fd, CPS1848_PORT_X_CTL_1_CSR(port), regval)))
		goto exit;

	// Make sure routing is set up correctly.
	regval = port;
	if (0 != (rc = w_mtc( fd, CPS_BROADCAST_UC_DEVICE_RT_ENTRY(destid), 
				regval)))
		goto exit;

exit:
	return rc;
};

void cleanup_ib_windows( int fd, int debug )
{
	int i;
	int found_one = 0;
	uint32_t regval;

	// Check that inbound window mapping isn't broken...
	for (i = 0; i < 8; i++) {
		if (r_721(fd, TSI721_IBWIN_LBX(i), &regval))
			goto fail;
		if (regval & TSI721_IBWIN_LBX_WIN_EN) {
			uint64_t ib_win;
			uint32_t regval2;

			if (r_721(fd, TSI721_IBWIN_UBX(i), &regval2))
				goto fail;
			ib_win = ((uint64_t)(regval2) << 32) + regval;
			if (debug) {
		 		printf("\nTsi721: IBWIN %d RIOADDR %lx", 
						i, ib_win);
				if (found_one)
					printf("\nTsi721: Multiple ib win!\n");
			};
			found_one++;
		};
	};
	
fail:
	if (found_one > 1) {
		for (i = 0; i < 8; i++)
			w_721(fd, TSI721_IBWIN_LBX(i), 0);
		exit(EXIT_FAILURE);
	};
};

int config_tsi721(int loopback, int got_switch, int fd, int debug, int ls_5g)
{
	uint32_t regval;
	int rc = 1;
	uint32_t fiveg_wa_val = ls_5g?TSI721_WA_VAL_5G:TSI721_WA_VAL_3G;
	int reg;

	// Attempt to configure the CPS Gen2 switch before resetting the
	// port.
	if (check_port_ok(fd, debug) && !check_port_err_free(fd, debug) &&
		got_switch)
		config_cps_gen2_pre(fd, debug);

	cleanup_ib_windows(fd, debug);

	// First, configure loopback correctly on all ports
	// Loopback should be set for LOOPBACK test, cleared for other tests.

	if (r_721(fd, TSI721_RIO_PLM_SP_IMP_SPEC_CTL, &regval))
		goto fail;

	if (loopback && !got_switch)
		regval |= TSI721_RIO_PLM_SP_IMP_SPEC_CTL_DLB_EN;
	else
		regval &= ~TSI721_RIO_PLM_SP_IMP_SPEC_CTL_DLB_EN;

	regval &= ~TSI721_RIO_PLM_SP_IMP_SPEC_CTL_DLT_THRESH;

	if (w_721(fd, TSI721_RIO_PLM_SP_IMP_SPEC_CTL, regval))
		goto fail;

	// Make sure input/output enable are set
	if (r_721(fd, TSI721_RIO_SP_CTL, &regval))
		goto fail;
	regval |= TSI721_RIO_SP_CTL_INP_EN | TSI721_RIO_SP_CTL_OTP_EN;
	if (w_721(fd, TSI721_RIO_SP_CTL, regval))
		goto fail;

	// Make sure master enable is set
	if (r_721(fd, TSI721_RIO_SP_GEN_CTL, &regval))
		goto fail;
	regval |= TSI721_RIO_SP_GEN_CTL_MAST_EN;
	if (w_721(fd, TSI721_RIO_SP_GEN_CTL, regval))
		goto fail;

	// Apply 5G training work around, or remove it, as necessary
	for (reg = 0; reg < TSI721_NUM_WA_REGS; reg++)
		if (w_721(fd,  TSI721_5G_WA_REG0(reg), fiveg_wa_val ))
			goto fail;

	if (ls_5g)
		for (reg = 0; reg < TSI721_NUM_WA_REGS; reg++)
			if (w_721(fd, TSI721_5G_WA_REG1(reg),0))
				goto fail;
	rc = 0;
fail:
	return rc;
};


int cleanup_tsi721(int got_switch, int fd, int debug, 
		int destid, int rst_lp)
{
	uint32_t regval;
	int rc = 1;

	// Clean up link using reset port
	reset_tsi721_rapidio(fd, debug, got_switch || rst_lp);

	// Make sure we get back to port ok after port reset
	if (!check_port_ok(fd, debug))
		goto fail;

	// Make sure port is error free after the reset
	regval = check_port_err_free(fd, debug);
	if (regval) {
		printf("\nTsi721 port error status: 0x%08x", regval);
		goto fail;
	};

	// Clear interrupts
	w_721( fd, TSI721_SR2PC_GEN_INT, 0xFFFFFFFF );

	// Specify timeout value
	w_721( fd, TSI721_RIO_SP_LT_CTL, 0x0000ff00 );

	// Specify timeout value
	w_721( fd, TSI721_RIO_SR_RSP_TO, 0x0000ff00 );

	if (got_switch)
		config_cps_gen2(fd, debug, destid);
	rc = 0;
fail:
	return rc;
};

