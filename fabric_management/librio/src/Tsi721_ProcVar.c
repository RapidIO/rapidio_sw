/*
****************************************************************************
Copyright (c) 2017, Integrated Device Technology Inc.
Copyright (c) 2017, RapidIO Trade Association
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
#include "Tsi721.h"
#include "Tsi721_API.h"
#include "DAR_DB.h"
#include "DAR_DB_Private.h"
#include "RapidIO_Utilities_API.h"
#include "RapidIO_Port_Config_API.h"
#include "RapidIO_Routing_Table_API.h"
#include "RapidIO_Error_Management_API.h"
#include "IDT_DSF_DB_Private.h"

#include "string_util.h"

#ifdef __cplusplus
extern "C" {
#endif

// CHANGES
//
// Check port width computation for get_config and set_config
// Check set_config with "all ports" as an input parameter.  Endless loop?
//

static DSF_Handle_t Tsi721_driver_handle;
static uint32_t num_Tsi721_driver_instances;

uint32_t IDT_tsi721ReadReg( DAR_DEV_INFO_t *dev_info,
				uint32_t  offset,
				uint32_t  *readdata )
{
	uint32_t rc = RIO_SUCCESS;

	switch(offset) {
	case RIO_SW_PORT_INF: *readdata = 0x00000100;
				break;
	case TSI721_SR_RSP_TO: *readdata = 0x00000100;
		rc = ReadReg(dev_info, offset, readdata);
		*readdata = (*readdata) << 8; 
				break;
	/* Never enable reliable port-write reception.  Ever. */
	case TSI721_PW_CTL:
		rc = ReadReg(dev_info, offset, readdata);
		*readdata &= ~TSI721_PW_CTL_PWC_MODE;
		break;
		
	default:
		rc = ReadReg(dev_info, offset, readdata);
	};

	return rc;
}

uint32_t IDT_tsi721WriteReg( DAR_DEV_INFO_t *dev_info,
				uint32_t  offset,
				uint32_t  writedata )
{
	uint32_t rc = RIO_SUCCESS;
	uint32_t temp_data;

	switch (offset) {
	/* Correct register errata in Tsi721 */
	case TSI721_SR_RSP_TO: writedata = writedata >> 8;
			rc = WriteReg(dev_info, offset, writedata);
			break;
	/* Only support 8 bit device IDs */
	case TSI721_BASE_ID:
			temp_data = (writedata & TSI721_BASE_ID_BASE_ID)
				>> 16;
			rc = WriteReg(dev_info, offset, writedata);
			if (rc)
				break;
			rc = WriteReg(dev_info, TSI721_IB_DEVID, temp_data);
			break;
	case TSI721_PW_CTL:
		/* Never enable reliable port-write reception.  Ever. */
		writedata &= ~TSI721_PW_CTL_PWC_MODE;
		rc = WriteReg(dev_info, offset, writedata);
		break;
	default:
		rc = WriteReg(dev_info, offset, writedata);
	}
	return rc;
};
// Routing table entry value to use when requesting
// default route or packet discard (no route)
#define HW_DFLT_RT 0xFF

uint32_t IDT_tsi721DeviceSupported( DAR_DEV_INFO_t *DAR_info )
{
	uint32_t rc = DAR_DB_NO_DRIVER;

	if (TSI721_DEVICE_VENDOR == (DAR_info->devID & RIO_DEV_IDENT_VEND))
	{
		if ((RIO_DEVI_IDT_TSI721) ==
			((DAR_info->devID & RIO_DEV_IDENT_DEVI) >> 16)) {
			// Now fill out the DAR_info structure...
			rc = DARDB_rioDeviceSupportedDefault( DAR_info );

			// Index and information for DSF is the same
 			// as the DAR handle
			DAR_info->dsf_h = Tsi721_driver_handle;

			if ( rc == RIO_SUCCESS ) {
				num_Tsi721_driver_instances++;
				SAFE_STRNCPY(DAR_info->name, "Tsi721",
							sizeof(DAR_info->name));
			}
		}
	}
	return rc;
}

uint32_t bind_tsi721_DAR_support( void )
{
	DAR_DB_Driver_t DAR_info;

	DARDB_Init_Driver_Info( RIO_VEND_IDT, &DAR_info );
	DAR_info.WriteReg = IDT_tsi721WriteReg;
	DAR_info.ReadReg = IDT_tsi721ReadReg;

	DAR_info.rioDeviceSupported = IDT_tsi721DeviceSupported;

	DARDB_Bind_Driver( &DAR_info );
	
	return RIO_SUCCESS;
}

uint32_t bind_tsi721_DSF_support( void )
{
	IDT_DSF_DB_t idt_driver;
 
	IDT_DSF_init_driver( &idt_driver );

	idt_driver.dev_type = RIO_DEVI_IDT_TSI721;

	idt_driver.rio_pc_clr_errs = idt_tsi721_pc_clr_errs;
	idt_driver.rio_pc_dev_reset_config = idt_tsi721_pc_dev_reset_config;
	idt_driver.rio_pc_get_config = idt_tsi721_pc_get_config;
	idt_driver.rio_pc_get_status = idt_tsi721_pc_get_status;
	idt_driver.rio_pc_reset_link_partner = idt_tsi721_pc_reset_link_partner;
	idt_driver.rio_pc_reset_port = idt_tsi721_pc_reset_port;
	idt_driver.rio_pc_secure_port = idt_tsi721_pc_secure_port;
	idt_driver.rio_pc_set_config = idt_tsi721_pc_set_config;
	idt_driver.rio_pc_probe = default_rio_pc_probe;

	idt_driver.rio_em_cfg_pw	   = idt_tsi721_em_cfg_pw;
	idt_driver.rio_em_cfg_set	  = idt_tsi721_em_cfg_set;
	idt_driver.rio_em_cfg_get	  = idt_tsi721_em_cfg_get;
	idt_driver.rio_em_dev_rpt_ctl  = idt_tsi721_em_dev_rpt_ctl;
	idt_driver.rio_em_parse_pw	 = idt_tsi721_em_parse_pw;
	idt_driver.rio_em_get_int_stat = idt_tsi721_em_get_int_stat;
	idt_driver.rio_em_get_pw_stat  = idt_tsi721_em_get_pw_stat;
	idt_driver.rio_em_clr_events   = idt_tsi721_em_clr_events;
	idt_driver.rio_em_create_events= idt_tsi721_em_create_events;

	idt_driver.rio_sc_init_dev_ctrs = idt_tsi721_sc_init_dev_ctrs;
	idt_driver.rio_sc_read_ctrs	= idt_tsi721_sc_read_ctrs;

	IDT_DSF_bind_driver( &idt_driver, &Tsi721_driver_handle);

	return RIO_SUCCESS;
}
#ifdef __cplusplus
}
#endif
