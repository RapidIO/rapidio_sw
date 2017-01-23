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

#include <stdbool.h>

#include "string_util.h"
#include "DAR_DB.h"
#include "DAR_DB_Private.h"
#include "RapidIO_Utilities_API.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAR_DB_MAX_INDEX (DAR_DB_MAX_DRIVERS-1)

DAR_DB_Driver_t driver_db[DAR_DB_MAX_DRIVERS];
static int first_free_driver;


/* For the local SRIO register access */
uint32_t (*ReadReg ) ( DAR_DEV_INFO_t *dev_info, uint32_t offset,
                                               uint32_t *readdata );
uint32_t (*WriteReg) ( DAR_DEV_INFO_t *dev_info, uint32_t  offset,
                                               uint32_t  writedata );

static void update_db_h( DAR_DEV_INFO_t *dev_info, int32_t idx )
{
    dev_info->db_h = ( VENDOR_ID( dev_info ) << 16 ) + idx ;
}


static uint32_t DARDB_ReadRegStub ( DAR_DEV_INFO_t *dev_info,
                                          uint32_t  offset  ,
                                          uint32_t *readdata )
{
    if ((NULL == dev_info) || (NULL == readdata))
       return RIO_ERR_NULL_PARM_PTR;

    if (offset > 0xFFFFFF)
       return RIO_ERR_INVALID_PARAMETER;

    *readdata = 0xFFFFFFFF;
    return RIO_STUBBED;
}


static uint32_t DARDB_WriteRegStub ( DAR_DEV_INFO_t *dev_info,
                                           uint32_t  offset  ,
                                           uint32_t  writedata )
{
    if (NULL == dev_info)
       return RIO_ERR_NULL_PARM_PTR;

    if ((offset > 0xFFFFFF) || (writedata != 0))
       return RIO_ERR_INVALID_PARAMETER;

    return RIO_STUBBED;
}


uint32_t DARDB_ReadReg( DAR_DEV_INFO_t *dev_info,
                                     uint32_t  offset,
                                     uint32_t *readdata )
{
    return ReadReg( dev_info, offset, readdata );
}


uint32_t DARDB_WriteReg( DAR_DEV_INFO_t *dev_info,
                                      uint32_t  offset,
                                      uint32_t  writedata )
{
    return WriteReg( dev_info, offset, writedata );
}


/* Standard HAL routines for system bringup, none of which use RapidIO
   standard functions/values
  
   Device drivers MUST implement their own versions of these routines.
*/
uint32_t DARDB_rioSetAssmblyInfo( DAR_DEV_INFO_t *dev_info,
                                               uint32_t  AsmblyVendID, 
                                               uint16_t  AsmblyRev    )
{
    if (NULL == dev_info)
       return RIO_ERR_NULL_PARM_PTR;

    if (AsmblyVendID || AsmblyRev)
       return RIO_ERR_FEATURE_NOT_SUPPORTED;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARDB_rioGetPortListDefault ( DAR_DEV_INFO_t  *dev_info ,
									struct DAR_ptl	*ptl_in,
									struct DAR_ptl	*ptl_out )
{
   uint8_t idx;
   uint32_t rc = RIO_ERR_INVALID_PARAMETER;
   bool dup_port_num[RIO_MAX_PORTS] = {false};

   ptl_out->num_ports = 0;

   if ((ptl_in->num_ports > NUM_PORTS(dev_info)) && (ptl_in->num_ports != RIO_ALL_PORTS))
	   goto exit;
   if ((ptl_in->num_ports > RIO_MAX_PORTS) && (ptl_in->num_ports != RIO_ALL_PORTS))
	   goto exit;

   if (!(ptl_in->num_ports))
	   goto exit;

   if (RIO_ALL_PORTS == ptl_in->num_ports) {
      ptl_out->num_ports = NUM_PORTS(dev_info);
      for (idx = 0; idx < NUM_PORTS(dev_info); idx++)
         ptl_out->pnums[idx] = idx;
   } else {
      ptl_out->num_ports = ptl_in->num_ports;
      for (idx = 0; idx < ptl_in->num_ports; idx++) {
         ptl_out->pnums[idx] = ptl_in->pnums[idx];
	     if ((ptl_out->pnums[idx] >= NUM_PORTS(dev_info)) ||
	        (dup_port_num[ptl_out->pnums[idx]]          )) {
			ptl_out->num_ports = 0;
			rc = RIO_ERR_PORT_ILLEGAL(idx);
	        goto exit;
	     };
	     dup_port_num[ptl_out->pnums[idx]] = true;
      };
   };
   rc = RIO_SUCCESS;

exit:
   return rc;
};

static uint32_t DARDB_rioSetEnumBound( DAR_DEV_INFO_t *dev_info,
                                     struct DAR_ptl *ptl,
					int enum_bnd_val )
{
    uint32_t rc = RIO_ERR_FEATURE_NOT_SUPPORTED;
    uint32_t currCSR, tempCSR;
	struct DAR_ptl good_ptl;
	uint8_t idx;
    enum_bnd_val = !!enum_bnd_val;
	
    if ( dev_info->extFPtrForPort )
    {
		rc = DARrioGetPortList( dev_info, ptl, &good_ptl);
		if (rc != RIO_SUCCESS)
			goto exit;

        for ( idx = 0; idx < good_ptl.num_ports; idx++) 
        {
            rc = DARRegRead( dev_info,
                             RIO_SPX_CTL( dev_info->extFPtrForPort,
						dev_info->extFPtrPortType, 
                                                     good_ptl.pnums[idx] ),
                            &currCSR ) ;
            if ( RIO_SUCCESS == rc )
            {
		if (enum_bnd_val) {
                	tempCSR = currCSR | RIO_SPX_CTL_ENUM_B;
		} else {
                	tempCSR = currCSR & ~RIO_SPX_CTL_ENUM_B;
		};
		if (tempCSR == currCSR)
			continue;

                rc = DARRegWrite( dev_info,
                                  RIO_SPX_CTL(
                                                      dev_info->extFPtrForPort,
						dev_info->extFPtrPortType, 
                                                      good_ptl.pnums[idx] ),
                                  tempCSR ) ;
                if ( RIO_SUCCESS == rc )
                {
                    rc = DARRegRead( dev_info,
                                     RIO_SPX_CTL(
                                                      dev_info->extFPtrForPort,
						dev_info->extFPtrPortType, 
                                                      good_ptl.pnums[idx] ),
                                    &tempCSR ) ;
                    if ( tempCSR != currCSR )
                        rc = RIO_ERR_FEATURE_NOT_SUPPORTED ;
                }
            }
        }
    }
exit:
    return rc;
}


static uint32_t DARDB_rioDeviceSupportedStub( DAR_DEV_INFO_t *dev_info )
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
uint32_t DARDB_rioDeviceSupportedDefault( DAR_DEV_INFO_t *dev_info )
{
    uint32_t rc;

    /* The reading devID should be unnecessary,
       but it should not hurt to do so here.
    */
    rc = ReadReg( dev_info, RIO_DEV_IDENT, &dev_info->devID ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_DEV_INF, &dev_info->devInfo ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_ASSY_INF, &dev_info->assyInfo ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_PE_FEAT, &dev_info->features ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    /* swPortInfo can be supported by endpoints.
       May as well read the register...
    */
    rc = ReadReg( dev_info, RIO_SW_PORT_INF, &dev_info->swPortInfo ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    if ( !dev_info->swPortInfo )
        dev_info->swPortInfo = 0x00000100 ; /* Default for endpoints is 1 port
                                            */

    if ( dev_info->features & RIO_PE_FEAT_SW )
    {
        rc = ReadReg( dev_info, RIO_SW_RT_TBL_LIM, &dev_info->swRtInfo ) ;
        if ( RIO_SUCCESS != rc )
            return rc;
    }

    rc = ReadReg( dev_info, RIO_SRC_OPS, &dev_info->srcOps ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    rc = ReadReg( dev_info, RIO_DST_OPS, &dev_info->dstOps ) ;
    if ( RIO_SUCCESS != rc )
        return rc;

    if (dev_info->features & RIO_PE_FEAT_MC)
    {
	rc = ReadReg(dev_info, RIO_SW_MC_INF, &dev_info->swMcastInfo);
        if ( RIO_SUCCESS != rc )
            return rc;
    }

    if ( dev_info->features & RIO_PE_FEAT_EFB_VALID )
    {
        uint32_t curr_ext_feat;
        uint32_t prev_addr;

        rc = DARrioGetExtFeaturesPtr( dev_info, &prev_addr ) ;
        while ( ( RIO_SUCCESS == rc ) && prev_addr )
        {
            rc = ReadReg(dev_info, prev_addr, &curr_ext_feat);
            if (RIO_SUCCESS != rc)
                return rc;

            switch( curr_ext_feat & RIO_EFB_T )
            {
                case RIO_EFB_T_SP_EP           :
                case RIO_EFB_T_SP_EP_SAER      :
                case RIO_EFB_T_SP_NOEP      :
                case RIO_EFB_T_SP_NOEP_SAER :
	            dev_info->extFPtrPortType = curr_ext_feat & 
			                         RIO_EFB_T;
                    dev_info->extFPtrForPort = prev_addr;
                    break;

                case RIO_EFB_T_EMHS :
                    dev_info->extFPtrForErr = prev_addr;
                    break;

                case RIO_EFB_T_HS:
                    dev_info->extFPtrForHS = prev_addr;
                    break;

                case RIO_EFB_T_LANE :
                    dev_info->extFPtrForLane = prev_addr;
                    break;

                case RIO_EFB_T_VC:
                    dev_info->extFPtrForLane = prev_addr;
                    break;

                case RIO_EFB_T_V0Q:
                    dev_info->extFPtrForVOQ = prev_addr;
                    break;

                case RIO_EFB_T_RT:
                    dev_info->extFPtrForRT = prev_addr;
                    break;

                case RIO_EFB_T_TS:
                    dev_info->extFPtrForTS = prev_addr;
                    break;

                case RIO_EFB_T_MISC:
                    dev_info->extFPtrForMISC = prev_addr;
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


uint32_t DAR_Find_Driver_for_Device( bool    dev_info_devID_valid,
                           DAR_DEV_INFO_t *dev_info )
{
    int32_t driverIdx;
    uint32_t rc = RIO_SUCCESS;

    SAFE_STRNCPY(dev_info->name, "UNKNOWN", sizeof(dev_info->name));

    /* If dev_info_devID_valid is true, we are using a static devID instead
         of probing.  Otherwise, we are probing a SRIO device to get a devID
    */
    if ( !dev_info_devID_valid )
    {
        rc = ReadReg( dev_info, RIO_DEV_IDENT, &dev_info->devID ) ;
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
		   uint32_t ctl1;
		   uint8_t  idx;
		   for (idx = 0; idx < NUM_PORTS(dev_info); idx++) {
			  rc = ReadReg(dev_info,
				RIO_SPX_CTL(dev_info->extFPtrForPort, 
					dev_info->extFPtrPortType, idx), &ctl1);
			  if ( RIO_SUCCESS != rc )
				 goto exit;
			   dev_info->ctl1_reg[idx] = ctl1;
			}; 
		}; 
	};
exit:
    return rc;
}


/* Initialize driver_db with defined max number of driver */

void init_DAR_driver( DAR_DB_Driver_t *DAR_info )
{
    DAR_info->ReadReg                  = DARDB_ReadReg;
    DAR_info->WriteReg                 = DARDB_WriteReg;
    DAR_info->WaitSec                  = DAR_WaitSec;

    DAR_info->rioSetAssmblyInfo        = DARDB_rioSetAssmblyInfo;
    DAR_info->rioGetPortList           = DARDB_rioGetPortListDefault;
    DAR_info->rioSetEnumBound          = DARDB_rioSetEnumBound;

    DAR_info->rioDeviceSupported       = DARDB_rioDeviceSupportedStub;
}

uint32_t DARDB_init()
{
    int32_t idx;

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


void DARDB_Init_Device_Info( DAR_DEV_INFO_t *dev_info )
{
    dev_info->db_h           = 0 ;
    dev_info->privateData    = 0 ;
    dev_info->accessInfo     = 0 ;
    dev_info->assyInfo       = 0 ;
    dev_info->extFPtrForPort = 0 ;
    dev_info->extFPtrPortType= 0 ;
    dev_info->extFPtrForLane = 0 ;
    dev_info->extFPtrForErr  = 0 ;
    dev_info->extFPtrForHS   = 0 ;
    dev_info->extFPtrForVC   = 0 ;
    dev_info->extFPtrForVOQ  = 0 ;
    dev_info->extFPtrForRT   = 0 ;
    dev_info->extFPtrForTS   = 0 ;
    dev_info->extFPtrForMISC = 0 ;

}


uint32_t DARDB_Init_Driver_Info( uint32_t VendorID, DAR_DB_Driver_t *DAR_info )
{
    uint32_t rc = DAR_DB_NO_HANDLES;

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


uint32_t DARDB_Bind_Driver( DAR_DB_Driver_t *dev_info )
{

    driver_db[DAR_DB_INDEX(dev_info)] = *dev_info; 

    return RIO_SUCCESS;
}

#ifdef __cplusplus
}
#endif
