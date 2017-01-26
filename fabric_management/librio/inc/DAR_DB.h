/*
 ****************************************************************************
 Copyright (c) 2014, Integrated Device Technology Inc.
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

#ifndef __DAR_DB_H__
#define __DAR_DB_H__

#include <stdbool.h>
#include "RapidIO_Device_Access_Routines_API.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAR_DB_MAX_DRIVERS (0x20)

/* ----->>>>>>>>>>>>>  Start of Host Specific Routines  <<<<<<<<<<--------
 *
 *  All of the Host Specific Routines must be bound in after init_DARDB
 *  is called.
 *
 *  Host Specific routines for reading and writing registers.
 *  If dev_info indicates the host, these routines must access host
 *  registers.  If dev_info indicates another device, these routines must
 *  generate Maintenance Read/Write transactions.
 */

extern uint32_t (*ReadReg)(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t *readdata);

extern uint32_t (*WriteReg)(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t writedata);

/* ----->>>>>>>>>>>>>  End of Host Specific Routines  <<<<<<<<<<-------- */

/* Default routines for the DARDB_Driver_t.ReadReg and .WriteReg routines.
 *
 *  If a device implements hooks for correcting register errata in the ReadReg or
 *  WriteReg routines, then these routines should be invoked to perform the
 *  actual read or write.
 */
extern uint32_t DARDB_WriteReg(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t writedata);

extern uint32_t DARDB_ReadReg(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t *readdata);

/* DARDB_init()
 *
 *  Initializes the DAR DB array of drivers, and binds in default
 *  non-functional Host Specific Routines.
 */
uint32_t DARDB_init(void);

/* DARDB Device Driver Structure
 *
 *  Function names beginning with "rio" are defined in the RapidIO Specification,
 *  Annex 1.  Function names beginning with "dev" are defined in RapidIO_Device_Access_Routines_API.h
 *
 *  Refer to RapidIO_Device_Access_Routines_API.h to the functions used to invoke these functions in
 *  the device driver.
 */
typedef struct DAR_DB_Driver_t_TAG {
	/* Handle into the DARDB, indicates where the routine will be bound. */
	DAR_DB_Handle_t db_h;

	/* Hook to allow register interface corrections when reading/writing
	 *  registers for the device.
	 *  User code should call these routines, instead of calling DARRegRead
	 *  and DARRegWrite or the ReadReg and WriteReg routines above.
	 */
	uint32_t (*ReadReg)(DAR_DEV_INFO_t *dev_info, uint32_t offset,
			uint32_t *readdata);
	uint32_t (*WriteReg)(DAR_DEV_INFO_t *dev_info, uint32_t offset,
			uint32_t writedata);
	void (*WaitSec)(uint32_t delay_nsec, uint32_t delay_sec);

} DAR_DB_Driver_t;

/* DARDB_Init_Device_Info
 *
 * Initialize device info such as handle, feature ptrs
 */
void DARDB_Init_Device_Info(DAR_DEV_INFO_t *dev_info);

/* DARDB_Init_Driver_Info
 *
 *  Function to bind default DARDB routines into DARDB_info structure.
 *  After this is done, it is safe for a device to bind their own routines
 *  into the DARDB_info structure directly.
 */
uint32_t DARDB_Init_Driver_Info(uint32_t VendorID, DAR_DB_Driver_t *DAR_info);

/* DAR_DB_Bind_Driver
 *  Binds generic and implementation specific versions of standard API routines.
 *
 *  Will fail if implementation specific versions of rioDeviceSupported is not bound in.
 */
uint32_t DARDB_Bind_Driver(DAR_DB_Driver_t *dev_info);

#ifdef __cplusplus
}
#endif

#endif /* __DAR_DB_H__ */

