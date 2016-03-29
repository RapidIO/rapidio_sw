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

#include <DAR_DevDriver.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Device Access Routine (DAR) Data Base (DB) Interface
*  
*  The DAR DB consists of
*  - Register read and write routines, to be bound in for the
*    processor executing this software.  See Host Specific Routines.
*  - An array of device drivers.  Device drivers consist of   
*    a structure containing pointers to procedures.
* 
*  There are routines defined to 
*  - Initialize the database using routines which are based on
*    RapidIO standard registers
*  - Bind in device drivers
*  
*  If a device driver is not found for a device, then a default device driver 
*  will be used.  The default device driver has not implementation specific 
*  support, and uses the default implementation of each routine.
* 
*  Example code for initializing the DAR DB
* 
*  STATUS example_DARDB_init( void )
*  {
*     DARDB_init();
* 
*     ReadReg  = acmeHostReadRegFunc;
*     WriteReg = acmeHostWriteRegFunc;
*     Delay    = acmeHostDelayFunc;
* 
*     RETURN bind_acme_driver_to_DAR();
*  }
*  
*  What follows is an example of how to bind a Driver into the DAR DB
* 
*  Suppose Acme Co, with an RTA Vendor ID of 0xACDE, has to bind the driver for 
*  Device 0xBEEF, versions 0x00, 0x01 and 0x02 into the DAR.
* 
*  STATUS bind_acme_driver_to_DAR ( void )
*  {
*   STATUS          rc = SUCCESS;
*   UINT32          vendor = 0xACDE;
*   UINT32          device = 0xBEEF;
* 
*   DAR_DB_Handle_t db_h;
*   DAR_DB_Driver_t db_dev_driver; 
* 
*   *  Bind in implementation specific versions of generic routines
*   *  In this example, Acme supplies two implementation specific versions
*   *    of generic DAR routines, as well as the required implementation
*   *    specific version of the generic DAR DB routines.
* 
*   rc = DARDB_init_driver_info( 0xACDE, &db_dev_driver );
* 
*   if (SUCCESS == rc)
*   {
*      *  Implementation specific versions of generic DAR routines
*      db_dev_driver.rioGetFeatures       = acme_rioGetFeatures;
*      db_dev_driver.rioGetSwitchPortInfo = acme_rioGetSwitchPortInfo;
*  
*      *  Required device specific versions of generic DAR routines
*      db_dev_driver.rioStdRouteInitAll      = acme_rioStdRouteInitAll;
*      db_dev_driver.rioStdRouteRemoveEntry  = acme_rioStdRouteRemoveEntry;
*      db_dev_driver.rioStdRouteSetDefault   = acme_rioStdRouteSetDefault;
*      db_dev_driver.rioSetAssmblyInfo       = acme_rioSetAssmblyInfo;
*      db_dev_driver.rioSetEnumBound         = acme_rioSetEnumBound;
*      db_dev_driver.rioGetDevResetInitStatus= acme_DARrioGetDevResetInitStatus;
*      db_dev_driver.rioDeviceSupported      = acme_rioDeviceSupported;
*      db_dev_driver.rioDeviceRemoved        = acme_rioDeviceRemoved;
* 
*      rc = DARDB_Bind_Driver( db_h, &db_dev_driver );
*   };
* 
*   return rc;
*  };

*  DAR_DB_MAX_DRIVERS defines the maximum number of distinct drivers supported
*  by the database.
*/

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

extern STATUS    (*ReadReg ) ( DAR_DEV_INFO_t *dev_info, 
                                       UINT32  offset, 
                                       UINT32 *readdata );

extern STATUS    (*WriteReg) ( DAR_DEV_INFO_t *dev_info, 
                                       UINT32  offset, 
                                       UINT32  writedata );

/* ----->>>>>>>>>>>>>  End of Host Specific Routines  <<<<<<<<<<-------- */

/* Default routines for the DARDB_Driver_t.ReadReg and .WriteReg routines.
*  
*  If a device implements hooks for correcting register errata in the ReadReg or
*  WriteReg routines, then these routines should be invoked to perform the 
*  actual read or write.
*/
extern STATUS DARDB_WriteReg( DAR_DEV_INFO_t *dev_info, 
                                      UINT32  offset, 
                                      UINT32  writedata );
extern STATUS DARDB_ReadReg( DAR_DEV_INFO_t *dev_info, 
                                     UINT32  offset, 
                                     UINT32 *readdata );
/* DARDB_init()
* 
*  Initializes the DAR DB array of drivers, and binds in default 
*  non-functional Host Specific Routines.
*/
STATUS DARDB_init( void ); 

/* DARDB Device Driver Structure
* 
*  Function names beginning with "rio" are defined in the RapidIO Specification,
*  Annex 1.  Function names beginning with "dev" are defined in DAR_DevDriver.h
* 
*  Refer to DAR_DevDriver.h to the functions used to invoke these functions in 
*  the device driver.
*/
typedef struct DAR_DB_Driver_t_TAG
{
    /* Handle into the DARDB, indicates where the routine will be bound. */
    DAR_DB_Handle_t db_h;

    /* Hook to allow register interface corrections when reading/writing 
    *  registers for the device.
    *  User code should call these routines, instead of calling DARRegRead 
    *  and DARRegWrite or the ReadReg and WriteReg routines above.
    */
    STATUS (*ReadReg )( DAR_DEV_INFO_t *dev_info, 
                                UINT32  offset, 
                                UINT32 *readdata );
    STATUS (*WriteReg)( DAR_DEV_INFO_t *dev_info, 
                                UINT32  offset, 
                                UINT32  writedata );
    VOID   (*WaitSec) ( UINT32 delay_nsec,
                        UINT32 delay_sec );

    /* Standard HAL routines for system bringup, all of which use standard 
    *      RapidIO registers.
    *  The default routines will function correctly for compliant devices.
    */
    STATUS (*rioGetNumLocalPorts     )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32 *numLocalPorts);
    STATUS (*rioGetFeatures          )( DAR_DEV_INFO_t *dev_info, 
                                          RIO_FEATURES *features     );
    STATUS (*rioGetSwitchPortInfo    )( DAR_DEV_INFO_t *dev_info, 
                                        RIO_SWITCH_PORT_INFO *portinfo     );
    STATUS (*rioGetExtFeaturesPtr    )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32 *extfptr      );
    STATUS (*rioGetNextExtFeaturesPtr)( DAR_DEV_INFO_t *dev_info, 
                                                UINT32  currfptr, 
                                                UINT32 *extfptr      );
    STATUS (*rioGetSourceOps         )( DAR_DEV_INFO_t *dev_info, 
                                        RIO_SOURCE_OPS *srcops       );
    STATUS (*rioGetDestOps           )( DAR_DEV_INFO_t *dev_info, 
                                        RIO_DEST_OPS   *dstops       );
    STATUS (*rioGetAddressMode       )( DAR_DEV_INFO_t *dev_info, 
                                        RIO_ADDR_MODE  *amode         );
    STATUS (*rioGetBaseDeviceId      )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32 *deviceid     );
    STATUS (*rioSetBaseDeviceId      )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32  newdeviceid   );
    STATUS (*rioAcquireDeviceLock    )( DAR_DEV_INFO_t *dev_info, 
                                                UINT16  hostdeviceid, 
                                                UINT16 *hostlockid   );
    STATUS (*rioReleaseDeviceLock    )( DAR_DEV_INFO_t *dev_info, 
                                                UINT16  hostdeviceid,
                                                UINT16 *hostlockid   );
    STATUS (*rioGetComponentTag      )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32 *componenttag );
    STATUS (*rioSetComponentTag      )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32  componenttag );
    STATUS (*rioGetAddrMode          )( DAR_DEV_INFO_t *dev_info, 
                                         RIO_ADDR_MODE *addr_mode );
    STATUS (*rioSetAddrMode          )( DAR_DEV_INFO_t *dev_info, 
                                         RIO_ADDR_MODE  addr_mode );
    STATUS (*rioGetPortErrorStatus   )( DAR_DEV_INFO_t *dev_info, 
                                                UINT16  portnum,
                                 RIO_PORT_ERROR_STATUS *err_status   );
    STATUS (*rioLinkReqNResp         )( DAR_DEV_INFO_t *dev_info, 
                                                 UINT8  portnum, 
                                  RIO_SPX_LM_LINK_STAT *link_stat );

    STATUS (*rioStdRouteAddEntry     )( DAR_DEV_INFO_t *dev_info, 
                                                UINT16  routedestid, 
                                                 UINT8  routeportno  );
    STATUS (*rioStdRouteGetEntry     )( DAR_DEV_INFO_t *dev_info, 
                                                UINT16  routedestid, 
                                                 UINT8 *routeportno  );

    /* Device Specific routines for system bring up.
    *  Device drivers MUST implement their own versions of these routines.
    */
    STATUS (*rioStdRouteInitAll      )( DAR_DEV_INFO_t *dev_info, 
                                                 UINT8  routeportno );
    STATUS (*rioStdRouteRemoveEntry  )( DAR_DEV_INFO_t *dev_info, 
                                                UINT16  routedestid );
    STATUS (*rioStdRouteSetDefault   )( DAR_DEV_INFO_t *dev_info, 
                                                 UINT8  routeportno );
    STATUS (*rioSetAssmblyInfo       )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32  AsmblyVendID, 
                                                UINT16  AsmblyRev   );
    STATUS (*rioGetAssmblyInfo       )( DAR_DEV_INFO_t *dev_info, 
                                                UINT32 *AsmblyVendID, 
                                                UINT16 *AsmblyRev   );
	STATUS (*rioGetPortList	         )( DAR_DEV_INFO_t  *dev_info ,
										struct DAR_ptl	*ptl_in,
										struct DAR_ptl	*ptl_out );
    STATUS (*rioSetEnumBound         )( DAR_DEV_INFO_t  *dev_info, 
                                        struct DAR_ptl  *ptl_in,
					int             enum_bnd_val);
    STATUS (*rioGetDevResetInitStatus)( DAR_DEV_INFO_t *dev_info    );
    STATUS (*rioPortEnable           )( DAR_DEV_INFO_t *dev_info,
                                        struct DAR_ptl	*ptl_in, 
                                                BOOL    port_ena,
                                                BOOL    port_lkout,
                                                BOOL    in_out_ena );
    STATUS (*rioEmergencyLockout     )( DAR_DEV_INFO_t *dev_info,
                                                 UINT8  port_no  );
   
    /* rioDeviceSupported will be invoked by DAR_Find_Driver_for_Device to see 
    *    if the device type specified by dev_info->devID (device manufacturer,
    *    device number) is supported by this driver.  Note that dev_info->db_h 
    *    is also valid, so all DAR driver routines can be called.
    *
    *  Each device driver may call DARDB_rioDeviceSupportedDefault to 
    *    initialize the device driver information structure.
    *
    *  If this is not a device supported by the driver, rioDeviceSupported
    *    implementations must return DAR_DB_NO_DRIVER.
    *
    *  If this is a device supported by the driver, rioDeviceSupported must 
    *    return RIO_SUCCESS.
    */ 
    STATUS (*rioDeviceSupported)( DAR_DEV_INFO_t *dev_info );
    STATUS (*rioDeviceRemoved  )( DAR_DEV_INFO_t *dev_info );
} DAR_DB_Driver_t;

/* Default implementation of DARRioGetPortList, made available for
 * device specific implementations.
 */
STATUS DARDB_rioGetPortList(DAR_DEV_INFO_t  *dev_info ,
							struct DAR_ptl	*ptl_in,
							struct DAR_ptl	*ptl_out );

/* DARDB_Init_Device_Info
*
* Initialize device info such as handle, feature ptrs
*/
VOID DARDB_Init_Device_Info( DAR_DEV_INFO_t *dev_info );

/* DARDB_Init_Driver_Info
*
*  Function to bind default DARDB routines into DARDB_info structure.
*  After this is done, it is safe for a device to bind their own routines 
*  into the DARDB_info structure directly. 
*/
STATUS DARDB_Init_Driver_Info( UINT32 VendorID, DAR_DB_Driver_t *DAR_info );

/* DAR_DB_Bind_Driver
*  Binds generic and implementation specific versions of standard API routines.
*
*  Will fail if implementation specific versions of rioDeviceSupported and
*  rioDeviceRemoved are not bound in.
*/
STATUS DARDB_Bind_Driver( DAR_DB_Driver_t *dev_info );

/* DARDB_rioDeviceSupporetedDefault 
*  Initializes the dev_info fields by reading RapidIO standard registers from 
*  the device.
*
*  Intended to be called by any implementation of rioDeviceSupported to 
*  initialize dev_info.
*/
STATUS DARDB_rioDeviceSupportedDefault( DAR_DEV_INFO_t *dev_info );

/* DARDB_rioGetPortListDefault
 * Default implementation of rioGetPortList, intended to be called by
 * driver routines that support different devices which do and do not 
 * have contiguous port numbers.
 */
STATUS DARDB_rioGetPortListDefault ( DAR_DEV_INFO_t  *dev_info ,
									struct DAR_ptl	*ptl_in,
									struct DAR_ptl	*ptl_out );

#ifdef __cplusplus
}
#endif

#endif /* __DAR_DB_H__ */

