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
#ifdef __cplusplus
extern "C" {
#endif

#define SCRPAD_EOF_OFFSET 0xFFFFFFFF
#define SCRPAD_FLAGS_IDX    0
#define SCRPAD_FIRST_IDX    0
#define SCRPAD_MASK_IDX     (SCRPAD_FIRST_IDX+Tsi578_MAX_MC_MASKS)

#define ALL_BITS ((uint32_t)(0xFFFFFFFF))
#define MC_IDX_MASK (Tsi578_RIO_MC_IDX_MC_ID| \
		Tsi578_RIO_MC_IDX_LARGE_SYS| \
		Tsi578_RIO_MC_IDX_MC_EN)
#define PW_MASK (Tsi578_RIO_PW_DESTID_LARGE_DESTID | \
		Tsi578_RIO_PW_DESTID_DESTID_LSB | \
		Tsi578_RIO_PW_DESTID_DESTID_MSB)
#define ERR_DET_MASK (Tsi578_RIO_LOG_ERR_DET_EN_UNSUP_TRANS_EN | \
		Tsi578_RIO_LOG_ERR_DET_EN_ILL_RESP_EN | \
		Tsi578_RIO_LOG_ERR_DET_EN_ILL_TRANS_EN)
#define MC_MASK_CFG_MASK ((uint32_t)(Tsi578_RIO_MC_MASK_CFG_PORT_PRESENT | \
			  Tsi578_RIO_MC_MASK_CFG_MASK_CMD | \
			Tsi578_RIO_MC_MASK_CFG_EG_PORT_NUM | \
			Tsi578_RIO_MC_MASK_CFG_MC_MASK_NUM))
#define MC_DESTID_MASK ((uint32_t)(Tsi578_RIO_MC_DESTID_CFG_DESTID_BASE | \
			Tsi578_RIO_MC_DESTID_CFG_DESTID_BASE_LT | \
			Tsi578_RIO_MC_DESTID_CFG_MASK_NUM_BASE))
#define MC_ASSOC_MASK ((uint32_t)(Tsi578_RIO_MC_DESTID_ASSOC_ASSOC_PRESENT | \
			Tsi578_RIO_MC_DESTID_ASSOC_CMD | \
			Tsi578_RIO_MC_DESTID_ASSOC_LARGE))

const struct scrpad_info scratchpad_const[MAX_DAR_SCRPAD_IDX] = {
	{Tsi578_RIO_MC_IDX(0) , MC_IDX_MASK},  
	{Tsi578_RIO_MC_IDX(1) , MC_IDX_MASK}, 
	{Tsi578_RIO_MC_IDX(2) , MC_IDX_MASK},
	{Tsi578_RIO_MC_IDX(3) , MC_IDX_MASK},
	{Tsi578_RIO_MC_IDX(4) , MC_IDX_MASK},
	{Tsi578_RIO_MC_IDX(5) , MC_IDX_MASK},
	{Tsi578_RIO_MC_IDX(6) , MC_IDX_MASK},
	{Tsi578_RIO_MC_IDX(7) , MC_IDX_MASK},
	{Tsi578_RIO_MC_MSKX(0), Tsi578_RIO_MC_MSKX_MC_MSK},/* SCRPAD_MASK_IDX for offsets to preserve/track */
	{Tsi578_RIO_MC_MSKX(1), Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_MC_MSKX(2), Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_MC_MSKX(3),Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_MC_MSKX(4), Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_MC_MSKX(5), Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_MC_MSKX(6), Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_MC_MSKX(7),Tsi578_RIO_MC_MSKX_MC_MSK},
	{Tsi578_RIO_COMP_TAG   , Tsi578_RIO_COMP_TAG_CTAG },
	{Tsi575_RIO_LUT_ATTR   , Tsi578_RIO_LUT_ATTR_DEFAULT_PORT},
	{Tsi578_RIO_SW_LT_CTL  , Tsi578_RIO_SW_LT_CTL_TVAL},
	{Tsi578_RIO_PW_DESTID  , PW_MASK},
	{Tsi578_RIO_LOG_ERR_DET_EN, ERR_DET_MASK},
	{Tsi578_RIO_PKT_TTL   ,  Tsi578_RIO_PKT_TTL_TTL },
	{Tsi578_RIO_MC_MASK_CFG, MC_MASK_CFG_MASK},
/* Code expects that Tsi578_RIO_MC_DESTID_CFG is the register
 * immediately before Tsi578_RIO_MC_DESTID_ASSOC.
 */
	{Tsi578_RIO_MC_DESTID_CFG,   MC_DESTID_MASK},
	{Tsi578_RIO_MC_DESTID_ASSOC, MC_ASSOC_MASK},
	{SCRPAD_EOF_OFFSET, ALL_BITS}
};

const struct scrpad_info *get_scrpad_info( void ) 
{
	return scratchpad_const;
};


uint32_t regrw_update_h_info_on_write(
	regrw_i *h, uint32_t  offset, uint32_t  data)
{
	uint8_t idx = 0;

	scratchpad = 
	for (idx = SCRPAD_FIRST_IDX; idx < MAX_DAR_SCRPAD_IDX; idx++) {
		if (scratchpad_const[idx].offset == offset) {
			writedata &= scratchpad_const[idx].rw_mask;
			dev_info->scratchpad[idx] = writedata;

			switch (offset) {
			case Tsi578_RIO_MC_MASK_CFG    : 
			{
				uint32_t mask = (writedata & Tsi578_RIO_MC_MASK_CFG_MC_MASK_NUM) >> 16;
				uint8_t port = (writedata & RIO_MC_MSK_CFG_PT_NUM) >> 8;
				uint32_t cmd  = (writedata & RIO_MC_MSK_CFG_CMD);
				/* Write to Tsi578_RIO_MC_MASK_CFG can update mask registers.
				* Emulate effect on mask registers, as we can't trust reading the
				* global mask registers if Port 0 is powered down.
				*/

				switch (cmd) {
				case RIO_MC_MSK_CFG_CMD_ADD:
					dev_info->scratchpad[mask+SCRPAD_MASK_IDX] |= ((uint32_t)(1) << (port + 16));
					break;
				case RIO_MC_MSK_CFG_CMD_DEL:
					dev_info->scratchpad[mask+SCRPAD_MASK_IDX] &= ~((uint32_t)(1) << (port + 16));
					break;
				case RIO_MC_MSK_CFG_CMD_DEL_ALL:
					dev_info->scratchpad[mask+SCRPAD_MASK_IDX] &= ~Tsi578_RIO_MC_MSKX_MC_MSK;
					break;
				case RIO_MC_MSK_CFG_CMD_ADD_ALL:
					dev_info->scratchpad[mask+SCRPAD_MASK_IDX] |= Tsi578_RIO_MC_MSKX_MC_MSK;
					break;
				default:
					break;
				};
				break;
			}

				case Tsi578_RIO_MC_DESTID_ASSOC:
				{
					uint8_t mask = (dev_info->scratchpad[idx - 1] & RIO_MC_CON_SEL_MASK);
					bool large = (dev_info->scratchpad[idx] & RIO_MC_CON_OP_DEV16M);
					uint32_t destid = dev_info->scratchpad[idx - 1] & 
						(RIO_MC_CON_SEL_DEV8 | RIO_MC_CON_SEL_DEV16);
					uint32_t cmd = (dev_info->scratchpad[idx] & RIO_MC_CON_OP_CMD);
					
					/* Write to Tsi578_RIO_MC_DESTID_ASSOC can update destID registers.
					 * Must emulate the effect, as it is not possible to trust the value
					 * of the destID register selected when port 0 is powered down.
					 */
					switch (cmd) {
					case RIO_MC_CON_OP_CMD_DEL:
						dev_info->scratchpad[mask] = 0;
						break;
					case RIO_MC_CON_OP_CMD_ADD:
						dev_info->scratchpad[mask] = (destid >> 16) |
							Tsi578_RIO_MC_IDX_MC_EN | ((large)?(Tsi578_RIO_MC_IDX_LARGE_SYS):0);
						break;
					default:
						break;
					};
					break;
				};
				default: break;
				};
				break;
			};
		};
	}
	return rc;
}

uint32_t IDT_tsi57xReadReg( DAR_DEV_INFO_t *dev_info,
                                uint32_t  offset,
                                uint32_t  *readdata )
{
	uint32_t rc = RIO_SUCCESS;
	bool found_one = false;
	uint8_t idx = 0;

	for (idx = SCRPAD_FIRST_IDX; idx < MAX_DAR_SCRPAD_IDX; idx++) {
		if (scratchpad_const[idx].offset == offset) {
			switch (offset) {
			case Tsi578_RIO_MC_DESTID_ASSOC:
			case Tsi578_RIO_MC_MASK_CFG    : 
			case Tsi578_RIO_MC_DESTID_CFG  : 
				continue;
			default:
				*readdata = dev_info->scratchpad[idx];
				found_one = true;
				continue;
			};
		};
	};

	if (!found_one)
		rc = ReadReg( dev_info, offset, readdata );

	return rc;
}


uint32_t init_scratchpad( DAR_DEV_INFO_t *DAR_info )
{
	uint32_t rc;
	uint8_t idx;

	for (idx = SCRPAD_FIRST_IDX; idx < MAX_DAR_SCRPAD_IDX; idx++) {
		if (SCRPAD_EOF_OFFSET == scratchpad_const[idx].offset)
			break;

		rc = ReadReg( DAR_info, scratchpad_const[idx].offset, &DAR_info->scratchpad[idx]);
		if (RIO_SUCCESS != rc)
			break;
	};
	return rc;
};

uint32_t IDT_tsi57xDeviceSupported( DAR_DEV_INFO_t *DAR_info )
{
    uint32_t rc = DAR_DB_NO_DRIVER;

    if ( IDT_TSI_VENDOR_ID ==  ( DAR_info->devID & RIO_DEV_IDENT_VEND ) )
    {
        if ( (IDT_TSI57x_DEV_ID >> 4) == ( (DAR_info->devID & RIO_DEV_IDENT_DEVI) >> 20) )
        {
            /* Now fill out the DAR_info structure... */
            rc = DARDB_rioDeviceSupportedDefault( DAR_info );

            /* Index and information for DSF is the same as the DAR handle */
            DAR_info->dsf_h = Tsi57x_driver_handle;
		rc = init_scratchpad( DAR_info );

            if ( rc == RIO_SUCCESS ) {
                num_Tsi57x_driver_instances++ ;
                getTsiName( DAR_info );
            };
        }
    }
    return rc;
}

uint32_t bind_tsi57x_DAR_support( void )
{
    DAR_DB_Driver_t DAR_info;

    DARDB_Init_Driver_Info( IDT_TSI_VENDOR_ID, &DAR_info );

	DAR_info.WriteReg = IDT_tsi57xWriteReg;
	DAR_info.ReadReg = IDT_tsi57xReadReg;
    DAR_info.rioDeviceSupported = IDT_tsi57xDeviceSupported;

    DARDB_Bind_Driver( &DAR_info );
    
    return RIO_SUCCESS;
}

