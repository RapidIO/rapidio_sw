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

#include "IDT_Tsi721.h"
#include "rio_register_utils.h"

#include "debug.h"


/**
 * enable_loopback_mode - Enables RIO loopback mode.
 *
 * Also sets Master Enable, and specifies a timeout.
 */
int enable_loopback_mode(void)
{
    uint32_t    reg;
    unsigned    limit;

    /* Enable loopback mode in PLM_SP_IMP_SPEC_CTL */
    reg = RIORegister(TSI721_RIO_PLM_SP_IMP_SPEC_CTL);
    reg |= TSI721_RIO_PLM_SP_IMP_SPEC_CTL_DLB_EN; 
    WriteRIORegister(TSI721_RIO_PLM_SP_IMP_SPEC_CTL, reg);

    /* Ensure that Master Enable bit is set */
    ReadRIORegister(TSI721_RIO_SP_GEN_CTL, &reg);
    reg |= TSI721_RIO_SP_GEN_CTL_MAST_EN; 
    WriteRIORegister(TSI721_RIO_SP_GEN_CTL, reg);

__sync_synchronize();

    reg = RIORegister(TSI721_RIO_SP_ERR_STAT);
    limit = 100000;
    while ((!(reg & TSI721_RIO_SP_ERR_STAT_PORT_OK) || (reg & (TSI721_RIO_SP_ERR_STAT_INPUT_ERR_STOP|TSI721_RIO_SP_ERR_STAT_OUTPUT_ERR_STOP ))) && limit--) {
        reg = RIORegister(TSI721_RIO_SP_ERR_STAT);
    };
    if (reg & TSI721_RIO_SP_ERR_STAT_PORT_OK) {
    	DPRINT("PORT OK, RIO_SP_ERR_STAT = 0x%08x\n", reg);
    } else {
    	fprintf(stderr,"PORT_UNINIT, loopback FAILED 0x%08x\n", reg);
        return -1;
    }
    
    /* Specify timeout value */
    WriteRIORegister( TSI721_RIO_SR_RSP_TO, 0x0000ff00 );

__sync_synchronize();

    return 1;
} /* enable_loopback_mode() */

