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
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "IDT_Tsi721.h"
#include "tsi721_config.h"
#include "rio_register_utils.h"

#include "debug.h"

/** Log base 2 of n
 * @param n  a number that is a power of 2 for which the log is to be computed
 */
static uint16_t Log2( uint32_t n ) 
{
   uint16_t logValue = -1;

   while( n != 0 ) {
       logValue++;
       n >>= 1;
   }

   return logValue;
} /* Log2() */


/**
 * Clear all outbound window zone lookup tables
 * @peer    Pointer to peer_info struct
 */
void obwin_clear_all(struct peer_info *peer)
{
	uint32_t zone, window;
	uint32_t zone_sel;

	/* Disable all outbound windows */
	for( window = 0; window < 8; window++ )
		WriteRIORegister(peer,TSI721_OBWINLBX(window), 0);

	/* Zero out the lookup tables for all outbound windows */
	for( window = 0; window < 8; window++ ) {
		for( zone = 0; zone < 8; zone++ ) {
			/* Ensure the ZONE_GO bit is 0 before programming LUT registers */
			unsigned    limit = 100;
			ReadRIORegister(peer,TSI721_ZONE_SEL, &zone_sel);
			while( (zone_sel & TSI721_ZONE_SEL_ZONE_GO) & (limit-- > 0) ) {
				usleep( 1000 );
				ReadRIORegister(peer, TSI721_ZONE_SEL, &zone_sel );
			}

			if( limit == 0 ) {
				perror("ZONE_GO failed to de-assert..WE HAVE A PROBLEM!!");
			}

			WriteRIORegister(peer,TSI721_LUT_DATA0, 0);
			WriteRIORegister(peer,TSI721_LUT_DATA1, 0);
			WriteRIORegister(peer,TSI721_LUT_DATA2, 0);

			zone_sel = zone + ((window << 3) & TSI721_ZONE_SEL_WIN_SEL) + TSI721_ZONE_SEL_ZONE_GO;
			WriteRIORegister(peer, TSI721_ZONE_SEL, zone_sel );
		}
	}

	// At this point the LUT registers for all windows and zones have been zeroed.
	DPRINT("%s:Outbound windows/zones cleanup completed\n", __FUNCTION__);
} /* obwin_clear_all() */


/** 
 * Configure the outbound window
 * @peer            Pointer to peer_info struct
 * @win_num         the window number
 * @win_start_addr  the physical starting address of the window within BAR2
 * @win_size        the size of the window in bytes
 */
void obwin_config(struct peer_info *peer,
                  uint8_t win_num,
                  uint32_t win_start_addr,
                  uint32_t win_size)
{
    uint32_t reg;
    uint16_t win_size_bits = (Log2(win_size) - 15) << 8;
    
    /* Upper PCIE BAR2 address */
    WriteRIORegister(peer, TSI721_OBWINUBX(win_num), 0x00000000);        

    /*  Window size bits stored in bits 12:8 */
    WriteRIORegister(peer, TSI721_OBWINSZX(win_num), win_size_bits);        

    // Lower PCIE BAR2 address  & enable bit
    reg = (win_start_addr & TSI721_OBWINLBX_ADD) | TSI721_OBWINLBX_WIN_EN;
    WriteRIORegister(peer, TSI721_OBWINLBX(win_num), reg);

    DPRINT("%s:Outbound win %d configured at PCIe addr 0x%08X, size = 0x%X\n",
                                                                __FUNCTION__,
                                                                win_num,
                                                                win_start_addr,
                                                                win_size);
} /* obwin_config() */


// Configures a zone in an outbound window
// @peer          pointer to peer struct
// @destid        device ID of destination
// @win_num       window containing the zone
// @zone_num      the zone to be configured
// @rio_addr      physical RIO address
//
int obwin_config_zone(struct peer_info *peer,
                             uint16_t destid,
                             uint8_t win_num,
                             uint8_t zone_num,
                             uint32_t rio_addr)
{
    uint32_t    zone_sel;
    uint32_t    lut_data0;
    uint32_t    lut_data1;
    uint32_t    lut_data2;
    unsigned    limit = 100;

    /* Ensure the ZONE_GO bit is 0 before programming LUT registers */
    ReadRIORegister(peer, TSI721_ZONE_SEL, &zone_sel);
    limit = 100;
    while( (zone_sel & TSI721_ZONE_SEL_ZONE_GO) & (limit-- > 0) ) {
        usleep( 1000 );
        ReadRIORegister(peer, TSI721_ZONE_SEL, &zone_sel);
    }
    if( limit == 0 ) {
        perror( "ZONE_GO failed to de-assert..WE HAVE A PROBLEM!!" );
    } else {
        DPRINT("%s:ZONE_GO de-asserted, ready to program LUTs\n", __FUNCTION__);
    }

    // Prepare values for LUT for the specified window/zone
    lut_data0 = (0x00000004 & TSI721_LUT_DATA0_WR_TYPE) | // NWRITE_R
                (0x00000100 & TSI721_LUT_DATA0_RD_TYPE) | // NREAD
                (rio_addr & TSI721_LUT_DATA0_ADD);  
                                            
    lut_data1 = 0x00000000; // Address is 32-bits. No upper address bits.
    lut_data2 = 0xFF000000 + destid; // Use 255 hops, device ID = <destid>

    // Write Outbound window Lookup Table Data 0-2 registers
    WriteRIORegister(peer, TSI721_LUT_DATA0, lut_data0 );
    WriteRIORegister(peer, TSI721_LUT_DATA1, lut_data1 );
    WriteRIORegister(peer, TSI721_LUT_DATA2, lut_data2 );

    // Tell the TSI721 what window and what zone those LUT values apply to
    // ZONE_SEL bits 2-0 are the zone, bits 5-3 are the window
    // Toggling ZONE_GO from 0 to 1 so that an address lookup table access is
    // performed
	zone_sel = zone_num
             + ( (win_num << 3) & TSI721_ZONE_SEL_WIN_SEL )
             + TSI721_ZONE_SEL_ZONE_GO;
    WriteRIORegister(peer, TSI721_ZONE_SEL, zone_sel );
    
    DPRINT("%s:Win %d, zone %d configured, destid = %04X, rio_address = %08X\n",
                                                                        __FUNCTION__,
                                                                        win_num,
                                                                        zone_num,
                                                                        destid,
                                                                        rio_addr);
    return 1;
} /* obwin_set_zone() */


