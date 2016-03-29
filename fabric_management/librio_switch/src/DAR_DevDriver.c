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
#include<DAR_DB.h>
#include<DAR_DB_Private.h>

#ifdef __cplusplus
extern "C" {
#endif

const struct DAR_ptl ptl_all_ports = PTL_ALL_PORTS;

/* Device Access Routine (DAR) Device Driver routines
  
   This file contains the implementation of all of the device driver
   routines for the DAR.  These routines all have the same form:
   - Validate the dev_info parameter passed in
   - Invoke the device driver routine bound into the DAR DB, based on the
     dev_info device handle information.
*/

STATUS update_dev_info_regvals( DAR_DEV_INFO_t *dev_info, UINT32 offset, UINT32 reg_val ) {

  STATUS rc = RIO_SUCCESS;

  if (dev_info->extFPtrForPort && 
       ((RIO_EXT_FEAT_PHYS_EP           == dev_info->extFPtrPortType) ||
        (RIO_EXT_FEAT_PHYS_EP_SAER      == dev_info->extFPtrPortType) ||
        (RIO_EXT_FEAT_PHYS_EP_FREE      == dev_info->extFPtrPortType) ||
        (RIO_EXT_FEAT_PHYS_EP_FREE_SAER == dev_info->extFPtrPortType)) ) {
     if ((offset >= RIO_PORT_N_CONTROL_CSR(dev_info->extFPtrForPort, 0                      )) && 
         (offset <= RIO_PORT_N_CONTROL_CSR(dev_info->extFPtrForPort, (NUM_PORTS(dev_info) - 1)))) {
        if (0x1C == (offset & 0x1C)) {
	   UINT8 idx;

           idx = (offset - RIO_PORT_N_CONTROL_CSR(dev_info->extFPtrForPort, 0)) / 0x20;
           if (idx >= NUM_PORTS(dev_info)) { 
              rc = RIO_ERR_SW_FAILURE;
           } else {
              dev_info->ctl1_reg[idx] = reg_val;
           };
        };
     };
  };

  return rc;
};

STATUS DARRegRead( DAR_DEV_INFO_t *dev_info, UINT32 offset, UINT32 *readdata )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) ) {
       rc = driver_db[DAR_DB_INDEX(dev_info)].ReadReg( dev_info,
                                                       offset,
                                                       readdata );
	   /* Note: update_dev_info_regvals, as the registers managed should not change
	    * after being written.  On some devices (i.e. Tsi57x), reading these registers
		* under certain conditions (port 0 powerdown) will not return the correct value.
		*/
   };
   return rc;
}



STATUS DARRegWrite( DAR_DEV_INFO_t *dev_info, UINT32 offset, UINT32 writedata )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].WriteReg( dev_info,
                                                        offset,
                                                        writedata );
       if (RIO_SUCCESS == rc) {
          rc = update_dev_info_regvals( dev_info, offset, writedata );
       };
   return rc;
}


STATUS DARrioGetNumLocalPorts( DAR_DEV_INFO_t *dev_info, UINT32 *numLocalPorts )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetNumLocalPorts( dev_info,
                                                               numLocalPorts );
   return rc;
}


STATUS DARrioGetFeatures( DAR_DEV_INFO_t *dev_info, RIO_FEATURES *features )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetFeatures( dev_info,
                                                              features );
   return rc;
}


STATUS DARrioGetSwitchPortInfo( DAR_DEV_INFO_t *dev_info,
                         RIO_SWITCH_PORT_INFO *portinfo  )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetSwitchPortInfo( dev_info,
                                                                    portinfo );
   return rc;
}


STATUS DARrioGetExtFeaturesPtr( DAR_DEV_INFO_t *dev_info, UINT32 *extfptr )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetExtFeaturesPtr( dev_info,
                                                                    extfptr );
   else
       *extfptr = 0x00000000;
   return rc;
}


STATUS DARrioGetNextExtFeaturesPtr( DAR_DEV_INFO_t *dev_info,
                                           UINT32  currfptr,
                                           UINT32 *extfptr )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetNextExtFeaturesPtr(
                                                                     dev_info,
                                                                     currfptr,
                                                                     extfptr );
   else
       *extfptr = 0x00000000;
   return rc;
}


STATUS DARrioGetSourceOps( DAR_DEV_INFO_t *dev_info, RIO_SOURCE_OPS *srcops )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetSourceOps( dev_info,
                                                               srcops );
   return rc;
}


STATUS DARrioGetDestOps( DAR_DEV_INFO_t *dev_info, RIO_DEST_OPS *dstops )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetDestOps( dev_info, dstops);
   return rc;
}


STATUS DARrioGetAddressMode( DAR_DEV_INFO_t *dev_info, RIO_ADDR_MODE *amode )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetAddressMode( dev_info,
                                                                 amode );
   return rc;
}


STATUS DARrioGetBaseDeviceId( DAR_DEV_INFO_t *dev_info, UINT32 *deviceid )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetBaseDeviceId( dev_info,
                                                                  deviceid );
   return rc;
}


STATUS DARrioSetBaseDeviceId( DAR_DEV_INFO_t *dev_info, UINT32 newdeviceid )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioSetBaseDeviceId( dev_info,
                                                                  newdeviceid);
   return rc;
}


STATUS DARrioAcquireDeviceLock( DAR_DEV_INFO_t *dev_info,
                                       UINT16  hostdeviceid,
                                       UINT16 *hostlockid )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioAcquireDeviceLock( dev_info,
                                                                hostdeviceid,
                                                                  hostlockid );
   return rc;
}


STATUS DARrioReleaseDeviceLock( DAR_DEV_INFO_t *dev_info,
                                       UINT16  hostdeviceid,
                                       UINT16 *hostlockid )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioReleaseDeviceLock( dev_info,
                                                                hostdeviceid,
                                                                  hostlockid );
   return rc;
}


STATUS DARrioGetComponentTag( DAR_DEV_INFO_t *dev_info, UINT32 *componenttag )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetComponentTag( dev_info,
                                                              componenttag );
   return rc;
}


STATUS DARrioSetComponentTag( DAR_DEV_INFO_t *dev_info, UINT32 componenttag )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioSetComponentTag( dev_info,
                                                              componenttag );
   return rc;
}

STATUS DARrioGetAddrMode( DAR_DEV_INFO_t *dev_info, RIO_ADDR_MODE *addr_mode )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetAddrMode( dev_info,
                                                              addr_mode );
   return rc;
}

STATUS DARrioSetAddrMode( DAR_DEV_INFO_t *dev_info, RIO_ADDR_MODE addr_mode )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioSetAddrMode( dev_info,
                                                              addr_mode );
   return rc;
}


STATUS DARrioGetPortErrorStatus( DAR_DEV_INFO_t *dev_info,
                                         UINT8  portnum,
                         RIO_PORT_ERROR_STATUS *err_status )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetPortErrorStatus(dev_info,
                                                                     portnum,
                                                                  err_status );
   return rc;
}

STATUS DARrioLinkReqNResp ( DAR_DEV_INFO_t *dev_info, 
                                     UINT8  portnum, 
                                    UINT32 *link_stat )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioLinkReqNResp(dev_info, portnum, link_stat );
   return rc;
}

STATUS DARrioStdRouteAddEntry( DAR_DEV_INFO_t *dev_info,
                                      UINT16  routedestid,
                                       UINT8  routeportno  )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioStdRouteAddEntry( dev_info,
                                                                routedestid,
                                                                routeportno );
   return rc;
}


STATUS DARrioStdRouteGetEntry( DAR_DEV_INFO_t *dev_info,
                                      UINT16  routedestid,
                                       UINT8 *routeportno  )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioStdRouteGetEntry( dev_info,
                                                                routedestid,
                                                                routeportno );
   return rc;
}


STATUS DARrioStdRouteInitAll( DAR_DEV_INFO_t *dev_info, UINT8 routeportno )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioStdRouteInitAll( dev_info,
                                                               routeportno );
   return rc;
}


STATUS DARrioStdRouteRemoveEntry( DAR_DEV_INFO_t *dev_info, UINT16 routedestid )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO(dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioStdRouteRemoveEntry(dev_info,
                                                                 routedestid );
   return rc;
}


STATUS DARrioStdRouteSetDefault( DAR_DEV_INFO_t *dev_info, UINT8 routeportno )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioStdRouteSetDefault( dev_info,
                                                                 routeportno );
   return rc;
}

STATUS DARrioSetAssmblyInfo( DAR_DEV_INFO_t *dev_info,
                                     UINT32  AsmblyVendID,
                                     UINT16  AsmblyRev )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioSetAssmblyInfo( dev_info,
                                                             AsmblyVendID,
                                                                AsmblyRev );
   return rc;
}


STATUS DARrioGetAssmblyInfo ( DAR_DEV_INFO_t *dev_info,
                                      UINT32 *AsmblyVendID,
                                      UINT16 *AsmblyRev )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetAssmblyInfo( dev_info,
                                                                 AsmblyVendID,
                                                                 AsmblyRev );
   return rc;
}

STATUS DARrioGetPortList( DAR_DEV_INFO_t *dev_info, struct DAR_ptl *ptl_in, struct DAR_ptl *ptl_out )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ((!dev_info) || (!ptl_in) || (!ptl_out))
	   return RIO_ERR_NULL_PARM_PTR;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetPortList( dev_info, ptl_in, ptl_out );
   return rc;
}

STATUS DARrioSetEnumBound( DAR_DEV_INFO_t *dev_info, struct DAR_ptl *ptl,
			int enum_bnd_val)
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

     if (!ptl)
	   return RIO_ERR_NULL_PARM_PTR;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioSetEnumBound( dev_info,
                                                          ptl, enum_bnd_val );
   return rc;
}

STATUS DARrioGetDevResetInitStatus( DAR_DEV_INFO_t *dev_info )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioGetDevResetInitStatus(
                                                                  dev_info );
   return rc;
}

STATUS DARrioPortEnable(
    DAR_DEV_INFO_t  *dev_info,
    struct DAR_ptl	*ptl,
    BOOL            port_ena,
    BOOL            port_lkout,
    BOOL            in_out_ena )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if (!ptl)
	   return RIO_ERR_NULL_PARM_PTR;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioPortEnable( dev_info,
                                                             ptl,
                                                             port_ena,
                                                             port_lkout,
                                                             in_out_ena );
   return rc;
}

STATUS DARrioEmergencyLockout( 
    DAR_DEV_INFO_t *dev_info,
    UINT8           port_no  )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioEmergencyLockout ( dev_info,
                                                                    port_no
                                                                  );
   return rc;
}

STATUS DARrioDeviceRemoved( DAR_DEV_INFO_t *dev_info )
{
   STATUS rc = DAR_DB_INVALID_HANDLE;

   if ( VALIDATE_DEV_INFO( dev_info ) )
       rc = driver_db[DAR_DB_INDEX(dev_info)].rioDeviceRemoved( dev_info );

   return rc;
}

#ifdef __cplusplus
}
#endif
