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
#ifndef __DAR_DEVDRIVER_H__
#define __DAR_DEVDRIVER_H__ 

#ifdef __cplusplus
extern "C" {
#endif
/* Device Access Routine (DAR) Device Driver Interface
* 
*  DAR_DevDriver.h defines the interface for invoking the device driver routines
*  defined in the RapidIO Specification Annex 1.  Device drivers are bound in 
*  using the DAR_DB.h interface.
* 
*  There are also hooks to extend the use of the DAR to support Device 
*  Specific Functions (DSF)
*/
#include <DAR_Basic_Defs.h>
#include <DAR_RegDefs.h>
#include <IDT_Common.h>

/* Each unique device driver is identified with a handle.
*  The structure of the handle is hidden from the user.
*  0 is always an invalid handle.
*/
#define INVALID_HANDLE 0

typedef UINT32 DAR_DB_Handle_t; /* DARDB Device Driver Handle */
typedef UINT32 DSF_Handle_t;    /* Device Specific Function Handle, not */
                                /*     defined by the DAR. */

/* Structure whiche defines the location of the device being accessed.
*  Also defines a reference to the device driver bound into the DAR DB,
*     and the pointer to the private data structure for the DAR.
*  A destid value of HOST_REGS will cause the local hosts registers to be 
*     accessed.
*/
#define HOST_REGS_DEVID 0xFFFFFFFF
#define MAX_DAR_PORTS   18
#define NAME_SIZE	15
#define MAX_DAR_SCRPAD_IDX  30

typedef struct DAR_DEV_INFO_t_TAG
{
    DAR_DB_Handle_t db_h; /* Handle value used to access DAR routines
                          */
    VOID   *privateData;  /* Pointer to a fabric management private data
			   * for this device.
                           */
    VOID   *accessInfo;   /* Pointer to an access info structure
                          */
    char   name[NAME_SIZE];  /* Text name of this device.
			      */
    DSF_Handle_t  dsf_h;  /* Handle for access to device-specific functions, 
                                if provided. 
                             The following fields are used by the ReadReg and 
                             WriteReg host specific routines to determine where
                             to access registers (Host or RapidIO network) and
                             to specify parameters for routine maintenance 
                             requests in the RapidIO network.
                          */

    /* Address offsets for various register blocks.
       A value of 0x00000000 means that the block does not exist on this device.
    */
    UINT32    extFPtrForPort;    /* Offset of LP-Serial Register Extensions 
                                        block.
                                 */
    UINT32    extFPtrPortType;
                /* RO: ID
                       0x01: Generic End Point Device
                       0x02: Generic End Point Device with Software Assisted 
                                 Error Recovery Opt
                       0x03: Generic End Point Free Device
                       0x09: Generic End Point Free Device with Software 
                                 Assisted Error Recovery Opt
                */
    UINT32    extFPtrForLane; /* RO: ID  0x0D: Lane Status
                              */
    UINT32    extFPtrForErr;  /* RO: ID  0x07: Error Management
                              */
    UINT32    extFPtrForVC;   /* RO: ID  0x0A: Virtual Channel
                              */
    UINT32    extFPtrForVOQ;  /* RO: ID  0x0B: Virtual Output Queueing
                              */
    /* Values of RapidIO Standard Registers, useful for understanding device
           capabilities without reading registers.
    */
    UINT32  devID;       /* Contents of RapidIO Standard register 
                                RIO_DEV_ID_CAR
                         */
    UINT32  devInfo;     /* Contents of RapidIO Standard register 
                                RIO_DEV_INFO_CAR
                         */
    UINT32  assyInfo;    /* Contents of RapidIO Standard register 
                                RIO_ASSY_INF_CAR
                         */
    UINT32  features;    /* Contents of RapidIO Standard register 
                                RIO_PROC_ELEM_FEAT_CAR
                         */ 
    UINT32  swPortInfo;  /* Contents of RapidIO Standard register 
                                RIO_SWITCH_PORT_INF_CAR
                         */
    UINT32  swRtInfo;    /* Contents of RapidIO Standard register 
                                RIO_RT_INFO_CAR
                         */
    UINT32  srcOps;      /* Contents of RapidIO Standard register 
                                RIO_SRC_OPS_CAR
                         */
    UINT32  dstOps;      /* Contents of RapidIO Standard register 
                                RIO_DST_OPS_CAR
                         */
    UINT32  swMcastInfo; /* Contents of RapidIO Standard register 
                                RIO_STD_SW_MC_INFO_CSR
                         */
	/* ctl1_reg values are tracked to implement EmergencyLockout as a 
	 * single write, instead of a read/modify/write, for all devices.
	 */
    UINT32  ctl1_reg[MAX_DAR_PORTS]; // Last register value read from or 
                                         // written to the Port x Control 1 CSR
	/* Scratchpad to be used by different devices as they see fit. */
	UINT32 scratchpad[MAX_DAR_SCRPAD_IDX]; 

} DAR_DEV_INFO_t;

#define NUM_PORTS(x)    (( UINT8)((((x)->swPortInfo ) & RIO_SWITCH_PORT_INF_PORT_TOTAL) >>  8))
#define NUM_MC_MASKS(x) ( (UINT8)((((x)->swMcastInfo) & RIO_STD_SW_MC_INFO_MAX_MASKS  )      ))
#define VEND_CODE(x)    ((UINT16)(((x)->devID      ) & RIO_DEV_ID_DEV_VEN_ID))
#define DEV_CODE(x)     ((UINT16)((((x)->devID      ) & RIO_DEV_ID_DEV_ID             ) >> 16))
#define SWITCH(x)	((BOOL)(((x)->features & RIO_PROC_ELEM_FEAT_SW)?TRUE:FALSE))

/* DAR_Find_Driver_for_Device
* 
*  Function to bind a device instance to a set of DAR routines
*  Updates the device info with a value which allows quick access to DAR 
*  routines.  The fields in dev_info must be set as follows before calling 
*  this routine:
*  dev_info_devID_valid - If this flag set to 1, use dev_info->devID instead of
*                         probing.  Otherwise, the internal routine probe the
*                         SRIO device to get the Device ID
*  On exit, all fields in dev_info will be correctly initialized.
*/
STATUS DAR_Find_Driver_for_Device( BOOL  dev_info_devID_valid,
                                   DAR_DEV_INFO_t *dev_info );

/* DAR_Release_Driver_for_Device
*
*  If a device has been removed from the system, call this routine to
*  free up resources associated with the driver.
*/

STATUS DAR_Release_Driver_for_Device ( DAR_DEV_INFO_t *dev_info );

/* Routines to access DAR device registers.  This is a hook to allow
*      device specific register read/write routines to correct non-compliances
*      with registers.  Could also be used as a hook for debugging.
*  The default definition of these routines is a call to ReadReg/WriteReg.
*/
STATUS DARRegRead ( DAR_DEV_INFO_t *dev_info, UINT32 offset, UINT32 *readdata );
STATUS DARRegWrite( DAR_DEV_INFO_t *dev_info, UINT32 offset, UINT32 writedata );

/* Routines which invoke the associated device driver function bound into the 
*  DAR DB.
* 
*  All of these routines have default DAR implementations which rely on 
*  RapidIO standard registers.
*  For more information, refer to Annex I of the RapidIO Specification, 
*      available from www.rapidio.org
*/
STATUS DARrioGetNumLocalPorts   ( DAR_DEV_INFO_t *dev_info, 
                                    UINT32 *numLocalPorts ); 
STATUS DARrioGetFeatures        ( DAR_DEV_INFO_t *dev_info, 
                                    RIO_FEATURES *features );
STATUS DARrioGetSwitchPortInfo  ( DAR_DEV_INFO_t *dev_info, 
                            RIO_SWITCH_PORT_INFO *portinfo );
STATUS DARrioGetExtFeaturesPtr  ( DAR_DEV_INFO_t *dev_info, 
                                          UINT32 *extfptr );
STATUS DARrioGetNextExtFeaturesPtr 
                                ( DAR_DEV_INFO_t *dev_info, 
                                          UINT32  currfptr, 
                                          UINT32 *extfptr );
STATUS DARrioGetSourceOps       ( DAR_DEV_INFO_t *dev_info, 
                                  RIO_SOURCE_OPS *srcops );
STATUS DARrioGetDestOps         ( DAR_DEV_INFO_t *dev_info, 
                                    RIO_DEST_OPS *dstops );
STATUS DARrioGetAddressMode     ( DAR_DEV_INFO_t *dev_info, 
                                   RIO_ADDR_MODE *amode );
STATUS DARrioGetBaseDeviceId    ( DAR_DEV_INFO_t *dev_info, 
                                          UINT32 *deviceid );
STATUS DARrioSetBaseDeviceId    ( DAR_DEV_INFO_t *dev_info, 
                                          UINT32  newdeviceid );
STATUS DARrioAcquireDeviceLock  ( DAR_DEV_INFO_t *dev_info, 
                                          UINT16  hostdeviceid, 
                                          UINT16 *hostlockid );
STATUS DARrioReleaseDeviceLock  ( DAR_DEV_INFO_t *dev_info, 
                                          UINT16  hostdeviceid, 
                                          UINT16 *hostlockid );
STATUS DARrioGetComponentTag    ( DAR_DEV_INFO_t *dev_info, 
                                          UINT32 *componenttag );
STATUS DARrioSetComponentTag    ( DAR_DEV_INFO_t *dev_info, 
                                          UINT32 componenttag );
STATUS DARrioGetPortErrorStatus ( DAR_DEV_INFO_t *dev_info, 
                                           UINT8  portnum, 
                           RIO_PORT_ERROR_STATUS *err_status );

STATUS DARrioLinkReqNResp ( DAR_DEV_INFO_t *dev_info, 
                                     UINT8  portnum, 
                      RIO_SPX_LM_LINK_STAT *link_stat );

/* Routing table support routines use the default routing table registers.
* 
*  The routing table routines assume that setting the routing table entry to 
*      an invalid port number will cause packets sent to that destID to be 
*      dropped.
*   
*  If the system has multiple hosts, these routines require that the host must
*      have used DARrioAquireDeviceLock() to gain ownership of the device.
*      DARrioReleaseDeviceLock() must be called to release ownership of the 
*      device.  The default implementations do not check for device lock, and 
*      assume that there is only one host in the system.
*  These routines are only valid if the device has switching capability.
*  Because this API is aimed at system bring up, there is no support 
*      for multicast.
*/
/* Use RIO_ALL_PORTS to cause packets with that routedestid to be discarded.
*/
#define RIO_ALL_PORTS ((UINT8)(0xFF))

STATUS DARrioStdRouteAddEntry ( DAR_DEV_INFO_t *dev_info, 
                                        UINT16  routedestid, 
                                         UINT8  routeportno  );
STATUS DARrioStdRouteGetEntry ( DAR_DEV_INFO_t *dev_info, 
                                        UINT16  routedestid, 
                                         UINT8 *routeportno );

/* NEW DAR ROUTINE, initializes all routing table entries to use routeportno.
*/
STATUS DARrioStdRouteInitAll ( DAR_DEV_INFO_t *dev_info, 
                                        UINT8  routeportno );

/* NEW DAR ROUTINE, invalidates the routing for the routedestid
*/
STATUS DARrioStdRouteRemoveEntry ( DAR_DEV_INFO_t *dev_info, 
                                           UINT16  routedestid );

/* NEW DAR ROUTINE, sets the default route
*/
STATUS DARrioStdRouteSetDefault  ( DAR_DEV_INFO_t *dev_info, 
                                            UINT8  routeportno );

/* NEW DAR ROUTINE, sets assembly information
*/
STATUS DARrioSetAssmblyInfo ( DAR_DEV_INFO_t *dev_info, 
                                      UINT32  AsmblyVendID, 
                                      UINT16  AsmblyRev    );

/* NEW DAR ROUTINE, gets assembly information
*/
STATUS DARrioGetAssmblyInfo ( DAR_DEV_INFO_t *dev_info, 
		                      UINT32 *AsmblyVendID,
				      UINT16 *AsmblyRev);

/* NEW DAR ROUTINE, lists of ports on a device.
 * if ptl_in->num_ports == RIO_ALL_PORTS,
		ptl_out contains a count, and list, of valid port numbers for the device.
   else
		ptl_out contains the same values as ptl_in. DARRioGetPortList checks 
		that all of the port numbers are valid for the device, and ensures that
		there are no duplicates.
	Note that the port numbers provided may not have any lanes mapped to them, 
	or have a trained link.
*/

#define DAR_MAX_PORTS 18

struct DAR_ptl {
	UINT8	num_ports; /* Number of valid entries in port */
	UINT8	pnums[DAR_MAX_PORTS]; /* List of port numbers available */
};

#define PTL_ALL_PORTS { RIO_ALL_PORTS, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}
extern const struct DAR_ptl ptl_all_ports;

STATUS DARrioGetPortList ( DAR_DEV_INFO_t  *dev_info ,
		                struct DAR_ptl	*ptl_in,  
						struct DAR_ptl	*ptl_out );

/* NEW DAR ROUTINE, sets enumeration boundary bit for specified port(s)
*/

STATUS DARrioSetEnumBound (DAR_DEV_INFO_t *dev_info, struct DAR_ptl *ptl,
			int enum_bnd_val);

/* NEW DAR ROUTINE, gets status of device reset initialization
   Returns either RIO_SUCCESS or a non-zero failure code.
*/
STATUS DARrioGetDevResetInitStatus ( DAR_DEV_INFO_t *dev_info );

/* NEW DAR ROUTINE, controls Port Enable, Port Lockout and In/Out Port Enable
   for specified list of ports
*/
STATUS DARrioPortEnable ( DAR_DEV_INFO_t *dev_info,
                          struct DAR_ptl *ptl,
                          BOOL port_ena,
                          BOOL port_lkout,
                          BOOL in_out_ena );

/* NEW DAR ROUTINE, ensures that the PORT_LOCKOUT bit is set for
                    the specified port
*/
STATUS DARrioEmergencyLockout  ( DAR_DEV_INFO_t *dev_info,
                                 UINT8          port_no );

/* Definitions to subdivide the STATUS return code for implementation 
       specific routines
*/


/* Success status code for all DAR and DSF driver routines
 * is RIO_SUCCESS
*/
 
/* Warning codes for DSF routines
*/
#define DAR_FIRST_IMP_SPEC_WARNING  ((STATUS)(0x0800))  

/*    Routine has succeeded, but something is a bit fishy.
*/
#define DAR_LAST_IMP_SPEC_WARNING   ((STATUS)(0x0FFF))  

/* Error codes for DSF routines
*/
#define DAR_FIRST_IMP_SPEC_ERROR    ((STATUS)(0x40000000)) 

/*    Routine has failed for an Implementation/device specific reason.
*/
#define DAR_LAST_IMP_SPEC_ERROR     ((STATUS)(0x6FFFFFFF)) 

/* Definitions for DAR standard return codes.
   Success status code
*/
#define RIO_SUCCESS                 ((STATUS)(0x0000)) 

/* DAR Standard warnings
   Used by rioRouteGetEntry, indicates that the routeportno returned
   is not the same for all ports
*/
#define RIO_WARN_INCONSISTENT       ((STATUS)(0x0001))  

/* DAR Device Specific Function warnings
*/
#define RIO_DAR_IMP_SPEC_WARNING  DAR_FIRST_IMP_SPEC_WARNING

/* DAR Standard errors
*/

/* Another host has a higher priority
*/
#define RIO_ERR_SLAVE                ((STATUS)(0x1001))  
/* One or more input parameters had an invalid value
*/
#define RIO_ERR_INVALID_PARAMETER    ((STATUS)(0x1002))  
/* The RapidIO fabric returned a Response Packet with ERROR status reported
*/
#define RIO_ERR_RIO                  ((STATUS)(0x1003))  
/* A device-specific hardware interface was unable to generate a maintenance
   transaction and reported an error
*/
#define RIO_ERR_ACCESS               ((STATUS)(0x1004))  
/* Another host already acquired the specified processor element
*/
#define RIO_ERR_LOCK                 ((STATUS)(0x1005))  
/* Device Access Routine does not provide services for this device
*/
#define RIO_ERR_NO_DEVICE_SUPPORT    ((STATUS)(0x1006))  
/* Insufficient storage available in Device Access Routine private storage area
*/
#define RIO_ERR_INSUFFICIENT_RESOURCES  ((STATUS)(0x1007))  
/* Switch cannot support requested routing
*/
#define RIO_ERR_ROUTE_ERROR          ((STATUS)(0x1008))  
/* Target device is not a switch
*/
#define RIO_ERR_NO_SWITCH            ((STATUS)(0x1009))  
/* Target device is not capable of the feature requested.
*/
#define RIO_ERR_FEATURE_NOT_SUPPORTED   ((STATUS)(0x100A))  
/* Port value is not correct/acceptable to this routine
*/
#define RIO_ERR_BAD_PORT                ((STATUS)(0x100B))
/* Value for default route register illegal/not supported
*/
#define RIO_ERR_BAD_DEFAULT_ROUTE       ((STATUS)(0x100C))
/* Value for routing table initialize illegal/not supported
*/
#define RIO_ERR_BAD_DEFAULT_ROUTE_TABLE_PORT ((STATUS)(0x100D))
/* Routing table values unintelligible/corrupted
*/
#define RIO_ERR_RT_CORRUPTED            ((STATUS)(0x100E))
/* Port value for routing table illegal/not supported
*/
#define RIO_ERR_BAD_ROUTE_PORT          ((STATUS)(0x100F))
/* Multicast mask value illegal/not supported
*/
#define RIO_ERR_BAD_MC_MASK             ((STATUS)(0x1010))
/* Port has error conditions after attempting to clear errors
*/
#define RIO_ERR_ERRS_NOT_CL             ((STATUS)(0x1011))
/* Routine was called with a pointer value of "NULL"
 * when this is not allowed.
*/
#define RIO_ERR_NULL_PARM_PTR           ((STATUS)(0x1012))
/* Requested action cannot be performed due to current     
 * configuration of the device.
*/
#define RIO_ERR_NOT_SUP_BY_CONFIG       ((STATUS)(0x1013))
/* Requested action cannot be performed due to an internal 
 * software error.                 
*/
#define RIO_ERR_SW_FAILURE              ((STATUS)(0x1014))

/* Routine has been stubbed out...
*/
#define RIO_STUBBED                  ((STATUS)(0xffff))  

/* Implementation specific routine has failed.  Check routine outputs for
       more information on failure.
*/
#define RIO_DAR_IMP_SPEC_FAILURE     ((STATUS)(0x40000000))  

/* DAR DB Standard errors
   No devices have been bound to the DAR
*/
#define DAR_DB_NO_DEVICES            ((STATUS)(0x70000001))  
/* The DAR DB cannot accept more device drivers. See DAR_DB.h to increase 
      the size of the database.
*/
#define DAR_DB_NO_HANDLES            ((STATUS)(0x70000002))  
/* DAR DB initialization has been attempted more than once.
*/
#define DAR_DB_MULTI_INIT            ((STATUS)(0x70000003))  
/* dev_info.db_h parameter passed in is invalid.
*/
#define DAR_DB_INVALID_HANDLE        ((STATUS)(0x70000004))  
/* Warning that a previously bound device specific function has 
       been overwritten
*/
#define DAR_DB_OVERWRITE_FUNC        ((STATUS)(0x70000005))  
/* No device driver bound in supports the device.
*/
#define DAR_DB_NO_DRIVER             ((STATUS)(0x70000006))  
/* Device driver could not allocate device specific info
*/
#define DAR_DB_DRIVER_INFO           ((STATUS)(0x70000007))

/* Device driver no exceed maximum device driver bound in supports the device.
*/
#define DAR_DB_NO_EXCEED_MAXIMUM     ((STATUS)(0x70000008))  

/* Standard Register access errors
   Register Access Interface invalid
*/
#define RIO_ERR_REG_ACCESS_IF_UNKNOWN       ((STATUS)(0x80000001))  
/* Register Access I2C File Descriptor is unknown
*/
#define RIO_ERR_REG_ACCESS_I2C_FD_UNKNOWN   ((STATUS)(0x80000002))  
/* Register Access RIO File Descriptor is unknown
*/
#define RIO_ERR_REG_ACCESS_RIO_FD_UNKNOWN   ((STATUS)(0x80000003))  
/* Private Data Structure is not defined
*/
#define RIO_ERR_PRIV_STRUCT_UNDEFINED       ((STATUS)(0x80000004))  
/* Invalid a returned parameter
*/
#define RIO_ERR_INVALID_RETURN_PARAM        ((STATUS)(0x80000005))  
/* The device is not an end-point
*/
#define RIO_ERR_NOT_ENDPOINT                ((STATUS)(0x80000006))  
/* No function is supported
*/
#define RIO_ERR_NO_FUNCTION_SUPPORT         ((STATUS)(0x80000007))  
/* Read Register return an invalid value
*/
#define RIO_ERR_READ_REG_RETURN_INVALID_VAL ((STATUS)(0x80000008))  
/* Detected no Port OK
*/
#define RIO_ERR_PORT_OK_NOT_DETECTED        ((STATUS)(0x80000009))  
/* No result returned
*/
#define RIO_ERR_RETURN_NO_RESULT            ((STATUS)(0x8000000A))  
/* Abort function call
*/
#define RIO_ERR_ABORT_FUNCTION_CALL         ((STATUS)(0x8000000B))
/* Not expected returned value
*/
#define RIO_ERR_NOT_EXPECTED_RETURN_VALUE   ((STATUS)(0x8000000C))
/* No lane is available
*/
#define RIO_ERR_NO_LANE_AVAIL               ((STATUS)(0x8000000D))
/* No port is available
*/
#define RIO_ERR_NO_PORT_AVAIL               ((STATUS)(0x8000000E))
/* Port is unavailable
*/
#define RIO_ERR_PORT_UNAVAIL                ((STATUS)(0x8000000F))
/* Illegal port number at index X in list of ports
 * Used by DARRioGetPortList.
*/
#define RIO_ERR_PORT_ILLEGAL(x)                ((STATUS)(0x80000010+x))
/* No response for register access
*/
#define RIO_ERR_REG_ACCESS_FAIL             ((STATUS)(0xFFFFFFFF))  

#ifdef __cplusplus
}
#endif

#endif /* __DAR_DEVDRIVER_H__ */

