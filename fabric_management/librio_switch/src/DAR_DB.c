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
/* Device Access Routine (DAR) Database (DB) Implementation

   The DAR DB implementation includes
   - Database variables and pointers to procedures
   - Stub and default implementations for device driver routines
   - Routines for DAR DB initialization and binding driver routines
*/

#include <DAR_DB.h>
#include <DAR_DB_Private.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DAR_DB_MAX_INDEX (DAR_DB_MAX_DRIVERS-1)

DAR_DB_Driver_t driver_db[DAR_DB_MAX_DRIVERS];
static int first_free_driver;


/* For the local SRIO register access */
STATUS (*ReadReg ) ( DAR_DEV_INFO_t *dev_info, UINT32 offset,
                                               UINT32 *readdata );
STATUS (*WriteReg) ( DAR_DEV_INFO_t *dev_info, UINT32  offset,
                                               UINT32  writedata );

static VOID update_db_h( DAR_DEV_INFO_t *dev_info, INT32 idx )
{
    dev_info->db_h = ( VENDOR_ID( dev_info ) << 16 ) + idx ;
}


static STATUS DARDB_ReadRegStub ( DAR_DEV_INFO_t *dev_info,
                                          UINT32  offset  ,
                                          UINT32 *readdata )
{
    if ((NULL == dev_info) || (NULL == readdata))
       return RIO_ERR_NULL_PARM_PTR;

    if (offset > 0xFFFFFF)
       return RIO_ERR_INVALID_PARAMETER;

    *readdata = 0xFFFFFFFF;
    return RIO_STUBBED;
}


static STATUS DARDB_WriteRegStub ( DAR_DEV_INFO_t *dev_info,
                                           UINT32  offset  ,
                                           UINT32  writedata )
{
    if (NULL == dev_info)
       return RIO_ERR_NULL_PARM_PTR;

    if ((offset > 0xFFFFFF) || (writedata != 0))
       return RIO_ERR_INVALID_PARAMETER;

    return RIO_STUBBED;
}


STATUS DARDB_ReadReg( DAR_DEV_INFO_t *dev_info,
                                     UINT32  offset,
                                     UINT32 *readdata )
{
    return ReadReg( dev_info, offset, readdata );
}


STATUS DARDB_WriteReg( DAR_DEV_INFO_t *dev_info,
                                      UINT32  offset,
                                      UINT32  writedata )
{
    return WriteReg( dev_info, offset, writedata );
}


/* Standard HAL routines for system bringup, all of which use
   standard RapidIO registers.  The default routines will function
   correctly for devices which are compliant to the RapidIO defined
   register definitions.
*/
static STATUS DARDB_rioGetNumLocalPorts( DAR_DEV_INFO_t *dev_info,
                                                 UINT32 *numLocalPorts )
{
    STATUS rc = DARRegRead( dev_info, RIO_SWITCH_PORT_INF_CAR, numLocalPorts );
	if (RIO_SUCCESS == rc) 
       *numLocalPorts = RIO_AVAIL_PORTS(*numLocalPorts);
	else
	   *numLocalPorts = 0;
	return rc;
}


static STATUS DARDB_rioGetFeatures( DAR_DEV_INFO_t *dev_info,
                                            RIO_FEATURES *features )
{
    return DARRegRead( dev_info, RIO_PROC_ELEM_FEAT_CAR, features );
}


static STATUS DARDB_rioGetSwitchPortInfo(      DAR_DEV_INFO_t *dev_info,
                                         RIO_SWITCH_PORT_INFO *portinfo  )
{
    STATUS rc =  DARRegRead( dev_info, RIO_SWITCH_PORT_INF_CAR, portinfo );

    if ( RIO_SUCCESS == rc )
        /* If this is not a switch or a multiport-endpoint, portinfo
           should be 0.  Fake the existance of the switch port info register
           by supplying 1 for the number of ports, and  0 as the port that
           we're connected to.
          
           Otherwise, leave portinfo alone.
        */
        if (   !(dev_info->features &
                (RIO_PROC_ELEM_FEAT_SW | RIO_PROC_ELEM_FEAT_MULTIP))
            &&  (! *portinfo ) )
              *portinfo = 0x00000100 ;

    return rc;
}


static STATUS DARDB_rioGetExtFeaturesPtr( DAR_DEV_INFO_t *dev_info,
                                                  UINT32 *extfptr )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_EXT_FEA )
    {
        rc = RIO_SUCCESS;
        *extfptr = dev_info->assyInfo & RIO_ASBLY_INFO_EXT_FEAT_PTR;
    };

    return rc;
}


static STATUS DARDB_rioGetNextExtFeaturesPtr( DAR_DEV_INFO_t *dev_info,
                                                      UINT32  currfptr,
                                                      UINT32 *extfptr )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;

    if ( currfptr & ( RIO_STD_EXT_FEATS_PTR >> 16) )
    {
        rc = DARRegRead( dev_info, currfptr, extfptr );
        *extfptr = (*extfptr & RIO_STD_EXT_FEATS_PTR) >> 16;
    };

    return rc;
}


static STATUS DARDB_rioGetSourceOps( DAR_DEV_INFO_t *dev_info,
                                     RIO_SOURCE_OPS *srcops )
{
    return DARRegRead( dev_info, RIO_SRC_OPS_CAR, srcops );
}


static STATUS DARDB_rioGetDestOps( DAR_DEV_INFO_t *dev_info,
                                     RIO_DEST_OPS *dstops )
{
    return DARRegRead( dev_info, RIO_DST_OPS_CAR, dstops );
}


static STATUS DARDB_rioGetAddressMode( DAR_DEV_INFO_t *dev_info,
                                        RIO_ADDR_MODE *amode )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_EXT_AS )
        rc = DARRegRead( dev_info, RIO_PE_LLAYER_CONTROL_CSR, amode ) ;

    return rc;
}


static STATUS DARDB_rioGetBaseDeviceId( DAR_DEV_INFO_t *dev_info,
                                                UINT32 *deviceid )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;

    if ( dev_info->features & (RIO_PROC_ELEM_FEAT_PROC |
                               RIO_PROC_ELEM_FEAT_MEM  |
                               RIO_PROC_ELEM_FEAT_BRDG ) )
        rc = DARRegRead( dev_info, RIO_BASE_DEVICE_ID_CSR, deviceid ) ;

    return rc;
}


static STATUS DARDB_rioSetBaseDeviceID( DAR_DEV_INFO_t *dev_info,
                                                       UINT32  newdeviceid )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;

    if ( dev_info->features & (RIO_PROC_ELEM_FEAT_PROC |
                               RIO_PROC_ELEM_FEAT_MEM  |
                               RIO_PROC_ELEM_FEAT_BRDG) )
        rc = DARRegWrite( dev_info, RIO_BASE_DEVICE_ID_CSR, newdeviceid ) ;

    return rc;
}


static STATUS DARDB_rioAcquireDeviceLock( DAR_DEV_INFO_t *dev_info,
                                                  UINT16  hostdeviceid,
                                                  UINT16 *hostlockid )
{
    UINT32 regVal;

    STATUS rc = DARRegRead( dev_info, RIO_HOST_BASE_DEVICE_ID_LOCK_CSR,
                                     &regVal );

    if ((hostdeviceid == RIO_STD_HOST_BASE_ID_LOCK_HOST_BASE_ID) ||
        ( !hostlockid                                          )) {
        return RIO_ERR_INVALID_PARAMETER;
    };

    *hostlockid = hostdeviceid;

    // Only do the write if the lock is not already taken...
    if ((RIO_SUCCESS == rc) && ((UINT16)(regVal) != hostdeviceid)) {
       rc = DARRegWrite( dev_info, RIO_HOST_BASE_DEVICE_ID_LOCK_CSR,
                                       hostdeviceid );
       if (RIO_SUCCESS == rc)
       {
           rc = DARRegRead(dev_info, RIO_HOST_BASE_DEVICE_ID_LOCK_CSR, &regVal);
           if ( RIO_SUCCESS == rc )
           {
               regVal &= RIO_STD_HOST_BASE_ID_LOCK_HOST_BASE_ID;
               if (regVal != hostdeviceid)
                   rc = RIO_ERR_LOCK;
   
               *hostlockid = (UINT16)(regVal);
           }
       }
    }

    return rc;
}


static STATUS DARDB_rioReleaseDeviceLock( DAR_DEV_INFO_t *dev_info,
                                                  UINT16  hostdeviceid,
                                                  UINT16 *hostlockid )
{
    UINT32 regVal;
    STATUS rc = DARRegWrite( dev_info,
                             RIO_HOST_BASE_DEVICE_ID_LOCK_CSR,
                             hostdeviceid );

    if ( RIO_SUCCESS == rc )
    {
        rc = DARRegRead( dev_info, RIO_HOST_BASE_DEVICE_ID_LOCK_CSR, &regVal ) ;

        if ( RIO_SUCCESS == rc )
        {
            *hostlockid = (UINT16)(regVal & RIO_STD_HOST_BASE_ID_LOCK_HOST_BASE_ID);
            if ( 0xFFFF != *hostlockid )
                rc = RIO_ERR_LOCK ;
        }
    }

    return rc;
}


static STATUS DARDB_rioGetComponentTag( DAR_DEV_INFO_t *dev_info,
                                                UINT32 *componenttag )
{
    return DARRegRead( dev_info, RIO_COMPONENT_TAG_CSR, componenttag );
}


static STATUS DARDB_rioSetComponentTag( DAR_DEV_INFO_t *dev_info,
                                                UINT32  componenttag )
{
    return DARRegWrite( dev_info, RIO_COMPONENT_TAG_CSR, componenttag );
}


static STATUS DARDB_rioGetPortErrorStatus( DAR_DEV_INFO_t *dev_info,
                                                   UINT16  portnum,
                                    RIO_PORT_ERROR_STATUS *err_status )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;

    if ( dev_info->extFPtrForPort )
    {
        if ( ( !portnum ) ||
             (portnum < RIO_AVAIL_PORTS(dev_info->swPortInfo) ) )
            rc = DARRegRead( dev_info,
                             RIO_PORT_N_ERR_STAT_CSR( dev_info->extFPtrForPort,
                                                      portnum),
                             err_status );
        else
            rc = RIO_ERR_INVALID_PARAMETER ;
    }

    return rc;
}

static STATUS DARDB_rioLinkReqNResp( DAR_DEV_INFO_t *dev_info, 
                                              UINT8  portnum, 
                               RIO_SPX_LM_LINK_STAT *link_stat )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;
	UINT8 attempts;
	UINT32 err_n_stat;

    if ( dev_info->extFPtrForPort )
    {
        if ( ( !portnum ) ||
             (portnum < RIO_AVAIL_PORTS(dev_info->swPortInfo) ) ) {
			rc = DARRegRead( dev_info,
							 RIO_PORT_N_ERR_STAT_CSR( dev_info->extFPtrForPort,
													   portnum),
							 &err_n_stat );
			 if (RIO_SUCCESS != rc)
				 goto exit;
			 if (!IS_RIO_PORT_N_ERR_STAT_PORT_OK(err_n_stat)) {
				rc = RIO_ERR_PORT_OK_NOT_DETECTED;
				goto exit;
			}

			 rc = DARRegWrite( dev_info,
                               RIO_PORT_N_LINK_MAINT_REQ_CSR( dev_info->extFPtrForPort,
                                                              portnum),
                               RIO_SPX_LM_REQ_CMD_LR_IS );
			 if (RIO_SUCCESS != rc)
				 goto exit;
		   /* Note that typically a link-response will be received faster than another
		    * maintenance packet can be issued.  Nevertheless, the routine polls 10 times
			* to check for a valid response, where 10 is a small number assumed to give 
			* enough time for even the slowest device to respond.
            */
		   for (attempts = 0; attempts < 10; attempts++) {
				rc = DARRegRead( dev_info,
								 RIO_PORT_N_LINK_MAINT_RESP_CSR( dev_info->extFPtrForPort,
																 portnum),
								 link_stat );

				if (RIO_SUCCESS != rc) 
					goto exit;

			    if (RIO_PORT_N_LINK_RESP_IS_VALID(*link_stat))
					goto exit;
          }
		   rc = RIO_ERR_NOT_EXPECTED_RETURN_VALUE;
        } else {
            rc = RIO_ERR_INVALID_PARAMETER ;
		}
    }
exit:
    return rc;
}

static STATUS DARDB_rioStdRouteAddEntry( DAR_DEV_INFO_t *dev_info,
                                                UINT16  routedestid,
                                                 UINT8  routeportno  )
{
    STATUS rc = RIO_ERR_NO_SWITCH;

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_SW )
    {
        rc = DARRegWrite( dev_info,
                          RIO_STD_RTE_CONF_DESTID_SEL_CSR,
                          routedestid ) ;
        if ( RIO_SUCCESS == rc )
            rc = DARRegWrite( dev_info,
                              RIO_STD_RTE_CONF_PORT_SEL_CSR,
                              routeportno ) ;
    }

    return rc;
}


static STATUS DARDB_rioStdRouteGetEntry( DAR_DEV_INFO_t *dev_info,
                                                 UINT16  routedestid,
                                                  UINT8 *routeportno )
{
    STATUS rc = RIO_ERR_NO_SWITCH;
    UINT32 regVal;

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_SW )
    {
        rc = DARRegWrite( dev_info,
                          RIO_STD_RTE_CONF_DESTID_SEL_CSR,
                          routedestid ) ;
        if ( RIO_SUCCESS == rc )
        {
            rc = DARRegRead( dev_info,
                             RIO_STD_RTE_CONF_PORT_SEL_CSR,
                            &regVal ) ;
            *routeportno = (UINT8)( regVal & RIO_STD_ROUTE_CFG_PORT_PORT0 );
        }
    }

    return rc;
}


/* Standard HAL routines for system bringup, none of which use RapidIO
   standard functions/values
  
   Device drivers MUST implement their own versions of these routines.
*/
static STATUS DARDB_rioStdRouteInitAll( DAR_DEV_INFO_t *dev_info,
                                                 UINT8  routeportno )
{
    STATUS rc = RIO_ERR_NO_SWITCH;
    INT32 num_dests = ( dev_info->swRtInfo & RIO_RT_INFO_SIZE );
    INT32 dest_idx;

    if ( (dev_info->swRtInfo) && (dev_info->features & RIO_PROC_ELEM_FEAT_SW) )
    {
        rc = DARrioStdRouteSetDefault( dev_info, routeportno ) ;
        for ( dest_idx = 0; ( (dest_idx <= num_dests) && (RIO_SUCCESS == rc) );
              dest_idx++ )
            rc = DARrioStdRouteAddEntry( dev_info, dest_idx, routeportno ) ;
    }

    return rc;
}

static STATUS DARDB_rioStdRouteRemoveEntry( DAR_DEV_INFO_t *dev_info,
                                                    UINT16  routedestid )
{
    STATUS rc = RIO_ERR_NO_SWITCH;

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_SW )
    {
        rc = DARRegWrite( dev_info,
                          RIO_STD_RTE_CONF_DESTID_SEL_CSR,
                          routedestid ) ;
        if ( RIO_SUCCESS == rc )
            rc = DARRegWrite( dev_info,
                              RIO_STD_RTE_CONF_PORT_SEL_CSR,
                              RIO_ALL_PORTS ) ;
    }

    return rc;
}


static STATUS DARDB_rioStdRouteSetDefault( DAR_DEV_INFO_t *dev_info,
                                                    UINT8  routeportno )
{
    STATUS rc = RIO_ERR_NO_SWITCH;

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_SW )
        rc = DARRegWrite( dev_info, RIO_STD_RTE_DEF_PORT_CSR, routeportno ) ;

    return rc;
}

static STATUS DARDB_rioSetAssmblyInfo( DAR_DEV_INFO_t *dev_info, 
                                               UINT32  AsmblyVendID, 
                                               UINT16  AsmblyRev    )
{
    if (NULL == dev_info)
       return RIO_ERR_NULL_PARM_PTR;

    if (AsmblyVendID || AsmblyRev)
       return RIO_ERR_FEATURE_NOT_SUPPORTED;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


static STATUS DARDB_rioGetAssmblyInfo ( DAR_DEV_INFO_t *dev_info, 
		                                UINT32 *AsmblyVendID,
				                UINT16 *AsmblyRev)
{
    STATUS rc = DARRegRead( dev_info, RIO_ASBLY_ID_CAR, AsmblyVendID );
    if (RIO_SUCCESS == rc) {
       UINT32 temp;
       rc = DARRegRead( dev_info, RIO_ASBLY_INFO_CAR, &temp );
       if (RIO_SUCCESS == rc)  {
          temp = (temp & RIO_ASBLY_INFO_ASBLY_REV) >> 16;
          *AsmblyRev = (UINT16)(temp);
       };
    };

    return rc;
}

STATUS DARDB_rioGetPortListDefault ( DAR_DEV_INFO_t  *dev_info ,
									struct DAR_ptl	*ptl_in,
									struct DAR_ptl	*ptl_out )
{
   UINT8 idx;
   STATUS rc = RIO_ERR_INVALID_PARAMETER;
   BOOL dup_port_num[DAR_MAX_PORTS];

   ptl_out->num_ports = 0;

   if ((ptl_in->num_ports > NUM_PORTS(dev_info)) && (ptl_in->num_ports != RIO_ALL_PORTS))
	   goto exit;

   if (!(ptl_in->num_ports))
	   goto exit;

   if (RIO_ALL_PORTS == ptl_in->num_ports) {
      ptl_out->num_ports = NUM_PORTS(dev_info);
      for (idx = 0; idx < NUM_PORTS(dev_info); idx++)
         ptl_out->pnums[idx] = idx;
   } else {
      for (idx = 0; idx < DAR_MAX_PORTS; idx++)
         dup_port_num[idx] = FALSE;

      ptl_out->num_ports = ptl_in->num_ports;
      for (idx = 0; idx < ptl_in->num_ports; idx++) {
         ptl_out->pnums[idx] = ptl_in->pnums[idx];
	     if ((ptl_out->pnums[idx] >= NUM_PORTS(dev_info)) ||
	        (dup_port_num[ptl_out->pnums[idx]]          )) {
			ptl_out->num_ports = 0;
			rc = RIO_ERR_PORT_ILLEGAL(idx);
	        goto exit;
	     };
	     dup_port_num[ptl_out->pnums[idx]] = TRUE;
      };
   };
   rc = RIO_SUCCESS;

exit:
   return rc;
};

static STATUS DARDB_rioSetEnumBound( DAR_DEV_INFO_t *dev_info,
                                     struct DAR_ptl *ptl )
{
    STATUS rc = RIO_ERR_FEATURE_NOT_SUPPORTED;
    UINT32 tempCSR;
	struct DAR_ptl good_ptl;
	UINT8 idx;

    if ( dev_info->extFPtrForPort )
    {
		rc = DARrioGetPortList( dev_info, ptl, &good_ptl);
		if (rc != RIO_SUCCESS)
			goto exit;

        for ( idx = 0; idx < good_ptl.num_ports; idx++) 
        {
            rc = DARRegRead( dev_info,
                             RIO_PORT_N_CONTROL_CSR( dev_info->extFPtrForPort,
                                                     good_ptl.pnums[idx] ),
                            &tempCSR ) ;
            if ( RIO_SUCCESS == rc )
            {
                tempCSR |= RIO_SPX_CTL_ENUM_B ;
                rc = DARRegWrite( dev_info,
                                  RIO_PORT_N_CONTROL_CSR(
                                                      dev_info->extFPtrForPort,
                                                      good_ptl.pnums[idx] ),
                                  tempCSR ) ;
                if ( RIO_SUCCESS == rc )
                {
                    rc = DARRegRead( dev_info,
                                     RIO_PORT_N_CONTROL_CSR(
                                                      dev_info->extFPtrForPort,
                                                      good_ptl.pnums[idx] ),
                                    &tempCSR ) ;
                    if ( !(tempCSR & RIO_SPX_CTL_ENUM_B) )
                        rc = RIO_ERR_FEATURE_NOT_SUPPORTED ;
                }
            }
        }
    }
exit:
    return rc;
}


static STATUS DARDB_rioPortEnable( DAR_DEV_INFO_t *dev_info,
                                   struct DAR_ptl *ptl, 
                                            BOOL    port_ena,
                                            BOOL    port_lkout,
                                            BOOL    in_out_ena )
{
    STATUS rc = RIO_SUCCESS;
    UINT32 port_n_ctrl1_addr;
    UINT32 port_n_ctrl1_reg;
	struct DAR_ptl good_ptl;
	UINT8 idx;

    /* Check whether 'portno' is assigned to a valid port value or not
    */
    rc = DARrioGetPortList( dev_info, ptl, &good_ptl);
	if (rc != RIO_SUCCESS)
		goto exit;

	for (idx = 0; idx < good_ptl.num_ports; idx++) {
		port_n_ctrl1_addr = 
			RIO_PORT_N_CONTROL_CSR( dev_info->extFPtrForPort,
									good_ptl.pnums[idx] ) ; 
		rc = DARRegRead( dev_info, port_n_ctrl1_addr, &port_n_ctrl1_reg ) ;
		if ( RIO_SUCCESS != rc )
			goto exit;

		if ( port_ena )
			port_n_ctrl1_reg &= ~RIO_SPX_CTL_PORT_DIS ;
		else
			port_n_ctrl1_reg |= RIO_SPX_CTL_PORT_DIS ;

		if ( port_lkout )
		   port_n_ctrl1_reg |=  RIO_SPX_CTL_PORT_LOCKOUT;
		else
		   port_n_ctrl1_reg &= ~RIO_SPX_CTL_PORT_LOCKOUT;

		if ( in_out_ena )
			port_n_ctrl1_reg |= RIO_SPX_CTL_INPUT_EN | RIO_SPX_CTL_OUTPUT_EN ;
		else
			port_n_ctrl1_reg &= ~(RIO_SPX_CTL_INPUT_EN | RIO_SPX_CTL_OUTPUT_EN);

		rc = DARRegWrite( dev_info, port_n_ctrl1_addr, port_n_ctrl1_reg );
	};
exit:
    return rc;
}

static STATUS DARDB_rioEmergencyLockout( DAR_DEV_INFO_t *dev_info, UINT8 port_num )
{
   STATUS rc = RIO_ERR_INVALID_PARAMETER;

   if (dev_info->extFPtrForPort && 
      ((RIO_EXT_FEAT_PHYS_EP           == dev_info->extFPtrPortType) ||
        (RIO_EXT_FEAT_PHYS_EP_SAER      == dev_info->extFPtrPortType) ||
        (RIO_EXT_FEAT_PHYS_EP_FREE      == dev_info->extFPtrPortType) ||
        (RIO_EXT_FEAT_PHYS_EP_FREE_SAER == dev_info->extFPtrPortType)) &&
      (port_num < NUM_PORTS(dev_info))) {
      rc = DARRegWrite( dev_info, 
		        RIO_PORT_N_CONTROL_CSR(dev_info->extFPtrForPort, port_num), 
		        dev_info->ctl1_reg[port_num] | RIO_SPX_CTL_PORT_LOCKOUT );
   };
   return rc;
} 

static STATUS DARDB_rioDeviceSupportedStub( DAR_DEV_INFO_t *dev_info )
{
    if (NULL == dev_info)
       return RIO_ERR_NULL_PARM_PTR;

    return DAR_DB_NO_DRIVER;
}


/* DARDB_rioDeviceSupportedDefault updates the database handle field,
   db_h, in dev_info to reflect the passed in index.
  
   The dev_info->devID field must be valid when this routine is called.

   Default driver for a device is the DARDB_ routines defined above.
   This routine is the last driver bound in to the DAR DB, so it will
   allow otherwise 'unsupported' devices to get minimal support.
  
   This routine should also be called directly by all devices to ensure that
   their DAR_DEV_INFO_t fields are correctly filled in.  Device specific
   fixups can be added after a call to this routine.
*/
STATUS DARDB_rioDeviceSupportedDefault( DAR_DEV_INFO_t *dev_info )
{
    STATUS rc;

    /* The reading devID should be unnecessary,
       but it should not hurt to do so here.
    */
    rc = ReadReg( dev_info, RIO_DEV_ID_CAR, &dev_info->devID ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_DEV_INFO_CAR, &dev_info->devInfo ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_ASBLY_INFO_CAR, &dev_info->assyInfo ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_PROC_ELEM_FEAT_CAR, &dev_info->features ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    /* swPortInfo can be supported by endpoints.
       May as well read the register...
    */
    rc = ReadReg( dev_info, RIO_SWITCH_PORT_INF_CAR, &dev_info->swPortInfo ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    if ( !dev_info->swPortInfo )
        dev_info->swPortInfo = 0x00000100 ; /* Default for endpoints is 1 port
                                            */

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_SW )
    {
        rc = ReadReg( dev_info, RIO_RT_INFO_CAR, &dev_info->swRtInfo ) ;
        if ( RIO_SUCCESS != rc )
            return rc;
    }

    rc = ReadReg( dev_info, RIO_SRC_OPS_CAR, &dev_info->srcOps ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_DST_OPS_CAR, &dev_info->dstOps ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    if (dev_info->features & RIO_PROC_ELEM_FEAT_MC)
    {
        rc = ReadReg(dev_info, RIO_STD_SW_MC_INFO_CSR, &dev_info->swMcastInfo);
        if ( RIO_SUCCESS != rc )
            return rc;
    }

    if ( dev_info->features & RIO_PROC_ELEM_FEAT_EXT_FEA )
    {
        UINT32 curr_ext_feat;
        UINT32 prev_addr;

        rc = DARDB_rioGetExtFeaturesPtr( dev_info, &prev_addr ) ;
        while ( ( RIO_SUCCESS == rc ) && prev_addr )
        {
            rc = ReadReg(dev_info, prev_addr, &curr_ext_feat);
            if (RIO_SUCCESS != rc)
                return rc;

            switch( curr_ext_feat & RIO_STD_EXT_FEATS_TYPE )
            {
                case RIO_EXT_FEAT_PHYS_EP           :
                case RIO_EXT_FEAT_PHYS_EP_SAER      :
                case RIO_EXT_FEAT_PHYS_EP_FREE      :
                case RIO_EXT_FEAT_PHYS_EP_FREE_SAER :
	            dev_info->extFPtrPortType = curr_ext_feat & 
			                         RIO_STD_EXT_FEATS_TYPE;
                    dev_info->extFPtrForPort = prev_addr;
                    break;

                case RIO_EXT_FEAT_ERR_RPT :
                    dev_info->extFPtrForErr = prev_addr;
                    break;

                case RIO_EXT_FEAT_PER_LANE :
                    dev_info->extFPtrForLane = prev_addr;
                    break;

                case RIO_EXT_FEAT_VC:
                    dev_info->extFPtrForLane = prev_addr;
                    break;

                case RIO_EXT_FEAT_VC_VOQ:
                    dev_info->extFPtrForVOQ = prev_addr;
                    break;

                default:
					if (0xFFFFFFFF == curr_ext_feat) {
						// Register access has failed.
						rc = RIO_ERR_ACCESS;
						goto DARDB_rioDeviceSupportedDefault_fail;
					};
                    break;
            }
            prev_addr = curr_ext_feat >> 16 ;
        }
    }
DARDB_rioDeviceSupportedDefault_fail:
    return rc;
}


static STATUS DARDB_rioGetDevResetInitStatus( DAR_DEV_INFO_t *dev_info )
{
    if (NULL == dev_info)
       return RIO_ERR_NULL_PARM_PTR;

    return RIO_SUCCESS;
}


STATUS DAR_Find_Driver_for_Device( BOOL    dev_info_devID_valid,
                           DAR_DEV_INFO_t *dev_info )
{
    INT32 driverIdx;
    STATUS rc = RIO_SUCCESS;

    strcpy(dev_info->name, "UNKNOWN");
    dev_info->name[NAME_SIZE-1] = '\0';

    /* If dev_info_devID_valid is TRUE, we are using a static devID instead
         of probing.  Otherwise, we are probing a SRIO device to get a devID
    */
    if ( !dev_info_devID_valid )
    {
        rc = ReadReg( dev_info, RIO_DEV_ID_CAR, &dev_info->devID ) ;
        if ( RIO_SUCCESS != rc )
			goto exit;
    };

    rc = DAR_DB_NO_DRIVER ;

    /* Go through drivers until one acknowledges that it can support
       the targetted device.
      
       Worst case, expect that the default driver will be used to support
       the device.
    */
    for ( driverIdx = 0;
         ( (driverIdx < DAR_DB_MAX_DRIVERS) &&
           (DAR_DB_NO_DRIVER == rc        ) );
          driverIdx++ )
    {
        update_db_h( dev_info, driverIdx ) ;
        rc = driver_db[driverIdx].rioDeviceSupported( dev_info ) ;
    }

	if (RIO_SUCCESS == rc) {
		// NOTE: All register manipulations must be done with DARReadReg and DARWriteReg,
		// or the application must manage dev_info->ctl1_reg themselves manually.  Otherwise, the
		// EmergencyLockout call may have unintended consequences.
    
		if ( dev_info->extFPtrForPort ) {
		   UINT32 ctl1;
		   UINT8  idx;
		   for (idx = 0; idx < NUM_PORTS(dev_info); idx++) {
			  rc = ReadReg(dev_info, RIO_PORT_N_CONTROL_CSR(dev_info->extFPtrForPort, idx), &ctl1);
			  if ( RIO_SUCCESS != rc )
				 goto exit;
			   dev_info->ctl1_reg[idx] = ctl1;
			}; 
		}; 
	};
exit:
    return rc;
}


static STATUS DARDB_rioDeviceRemoved( DAR_DEV_INFO_t *dev_info )
{
    STATUS rc = RIO_SUCCESS;

    if ( NULL != dev_info->privateData )
        rc = DAR_DB_DRIVER_INFO ;

    return rc;
}

/* Initialize driver_db with defined max number of driver */

void init_DAR_driver( DAR_DB_Driver_t *DAR_info )
{
    DAR_info->ReadReg                  = DARDB_ReadReg ;
    DAR_info->WriteReg                 = DARDB_WriteReg ;

    DAR_info->rioGetNumLocalPorts      = DARDB_rioGetNumLocalPorts ;
    DAR_info->rioGetFeatures           = DARDB_rioGetFeatures ;
    DAR_info->rioGetSwitchPortInfo     = DARDB_rioGetSwitchPortInfo ;
    DAR_info->rioGetExtFeaturesPtr     = DARDB_rioGetExtFeaturesPtr ;
    DAR_info->rioGetNextExtFeaturesPtr = DARDB_rioGetNextExtFeaturesPtr ;
    DAR_info->rioGetSourceOps          = DARDB_rioGetSourceOps ;
    DAR_info->rioGetDestOps            = DARDB_rioGetDestOps ;
    DAR_info->rioGetAddressMode        = DARDB_rioGetAddressMode ;
    DAR_info->rioGetBaseDeviceId       = DARDB_rioGetBaseDeviceId ;
    DAR_info->rioSetBaseDeviceId       = DARDB_rioSetBaseDeviceID ;
    DAR_info->rioAcquireDeviceLock     = DARDB_rioAcquireDeviceLock ;
    DAR_info->rioReleaseDeviceLock     = DARDB_rioReleaseDeviceLock ;
    DAR_info->rioGetComponentTag       = DARDB_rioGetComponentTag ;
    DAR_info->rioSetComponentTag       = DARDB_rioSetComponentTag ;
    DAR_info->rioGetPortErrorStatus    = DARDB_rioGetPortErrorStatus ;
    DAR_info->rioLinkReqNResp          = DARDB_rioLinkReqNResp;

    DAR_info->rioStdRouteAddEntry      = DARDB_rioStdRouteAddEntry ;
    DAR_info->rioStdRouteGetEntry      = DARDB_rioStdRouteGetEntry ;

    DAR_info->rioStdRouteInitAll       = DARDB_rioStdRouteInitAll ;
    DAR_info->rioStdRouteRemoveEntry   = DARDB_rioStdRouteRemoveEntry ;
    DAR_info->rioStdRouteSetDefault    = DARDB_rioStdRouteSetDefault ;
    DAR_info->rioSetAssmblyInfo        = DARDB_rioSetAssmblyInfo ;
    DAR_info->rioGetAssmblyInfo        = DARDB_rioGetAssmblyInfo ;
	DAR_info->rioGetPortList           = DARDB_rioGetPortListDefault;
    DAR_info->rioSetEnumBound          = DARDB_rioSetEnumBound ;
    DAR_info->rioGetDevResetInitStatus = DARDB_rioGetDevResetInitStatus ;

    DAR_info->rioPortEnable            = DARDB_rioPortEnable;
    DAR_info->rioEmergencyLockout      = DARDB_rioEmergencyLockout;

    DAR_info->rioDeviceSupported       = DARDB_rioDeviceSupportedStub ;
    DAR_info->rioDeviceRemoved         = DARDB_rioDeviceRemoved ;
}

STATUS DARDB_init()
{
    INT32 idx;

    ReadReg            = DARDB_ReadRegStub ;
    WriteReg           = DARDB_WriteRegStub ;

    first_free_driver = 0 ; /* Nothing bound yet. */

    for ( idx = 0; idx < DAR_DB_MAX_DRIVERS; idx++ )
    {
		init_DAR_driver( &driver_db[idx] );
    }

    driver_db[DAR_DB_MAX_INDEX].rioDeviceSupported
                                              = DARDB_rioDeviceSupportedDefault;
    driver_db[DAR_DB_MAX_INDEX].db_h          = DAR_DB_MAX_INDEX ;

    return RIO_SUCCESS;
}


VOID DARDB_Init_Device_Info( DAR_DEV_INFO_t *dev_info )
{
    dev_info->db_h           = 0 ;
    dev_info->privateData    = 0 ;
    dev_info->accessInfo     = 0 ;
    dev_info->assyInfo       = 0 ;
    dev_info->extFPtrForPort = 0 ;
    dev_info->extFPtrPortType= 0 ;
    dev_info->extFPtrForLane = 0 ;
    dev_info->extFPtrForErr  = 0 ;
    dev_info->extFPtrForVC   = 0 ;
    dev_info->extFPtrForVOQ  = 0 ;
}


STATUS DARDB_Init_Driver_Info( UINT32 VendorID, DAR_DB_Driver_t *DAR_info )
{
    STATUS rc = DAR_DB_NO_HANDLES;

    /* Preserve the last index for the default device driver */
    if ( first_free_driver < DAR_DB_MAX_INDEX )
    {
        DAR_info->db_h = ( (VendorID & 0x0000FFFF) << 16 ) + first_free_driver ;
        first_free_driver++ ;
        rc = RIO_SUCCESS ;
    }

    init_DAR_driver( DAR_info );
    return rc;
}


STATUS DARDB_Bind_Driver( DAR_DB_Driver_t *dev_info )
{

    driver_db[DAR_DB_INDEX(dev_info)] = *dev_info; 

    return RIO_SUCCESS;
}

#ifdef __cplusplus
}
#endif
