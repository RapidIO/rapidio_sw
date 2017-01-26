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

#include <stddef.h>

#include "RapidIO_Source_Config.h"
#include "rio_ecosystem.h"

#include "CPS_DeviceDriver.h"
#include "RXS_DeviceDriver.h"
#include "Tsi721_DeviceDriver.h"
#include "Tsi57x_DeviceDriver.h"

#include "DAR_DB.h"
#include "DAR_DB_Private.h"

#ifdef __cplusplus
extern "C" {
#endif

const struct DAR_ptl ptl_all_ports = PTL_ALL_PORTS;

/* Device Access Routine (DAR) Device Driver routines
  
   This file contains the implementation of all of the device driver
   routines for the DAR.  These routines all have the same form:
   - Validate the dev_info parameter passed in
   - Invoke the device driver routine
*/

rio_driver_family_t rio_get_driver_family(uint32_t devID)
{
	uint16_t vend_code = (uint16_t)(devID & RIO_DEV_IDENT_VEND);
	uint16_t dev_code = (uint16_t)((devID & RIO_DEV_IDENT_DEVI) >> 16);

	switch (vend_code) {
	case RIO_VEND_IDT:
		switch (dev_code) {
		case RIO_DEVI_IDT_CPS1848:
		case RIO_DEVI_IDT_CPS1432:
		case RIO_DEVI_IDT_CPS1616:
		case RIO_DEVI_IDT_SPS1616:
#ifdef CPS_DAR_WANTED
			return RIO_CPS_DEVICE;
#endif
			break;

		case RIO_DEVI_IDT_RXS2448:
		case RIO_DEVI_IDT_RXS1632:
#ifdef RXSx_DAR_WANTED
			return RIO_RXS_DEVICE;
#endif
			break;

		case RIO_DEVI_IDT_TSI721:
#ifdef TSI721_DAR_WANTED
			return RIO_TSI721_DEVICE;
#endif
			break;

		default:
			break;
		}
		break;
	case RIO_VEND_TUNDRA:
		switch (dev_code) {
		case RIO_DEVI_TSI572:
		case RIO_DEVI_TSI574:
		case RIO_DEVI_TSI577:
		case RIO_DEVI_TSI578:
#ifdef TSI57X_DAR_WANTED
			return RIO_TSI57X_DEVICE;
#endif
			break;

		default:
			break;
		}
		break;
	default:
		break;
	}
	return RIO_UNKNOWN_DEVICE;
}


uint32_t update_dev_info_regvals(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t reg_val)
{

	uint32_t rc = RIO_SUCCESS;

	if (dev_info->extFPtrForPort && RIO_SP_VLD(dev_info->extFPtrPortType)) {
		if ((offset
				>= RIO_SPX_CTL(dev_info->extFPtrForPort,
						dev_info->extFPtrPortType, 0))
				&& (offset
						<= RIO_SPX_CTL(
								dev_info->extFPtrForPort,
								dev_info->extFPtrPortType,
								(NUM_PORTS(dev_info) - 1)))) {
			if ((0x1C == (offset & 0x1C)
					&& !RIO_SP3_VLD(
							dev_info->extFPtrPortType))
					|| (0x3C == (offset & 0x3C)
							&& RIO_SP3_VLD(
									dev_info->extFPtrPortType))) {
				uint8_t idx;

				idx =
						(offset
								- RIO_SPX_CTL(
										dev_info->extFPtrForPort,
										dev_info->extFPtrPortType,
										0))
								/ RIO_SP_STEP(
										dev_info->extFPtrPortType);
				if (idx >= NUM_PORTS(dev_info)) {
					rc = RIO_ERR_SW_FAILURE;
				} else {
					dev_info->ctl1_reg[idx] = reg_val;
				};
			};
		};
	};

	return rc;
}


uint32_t DARRegRead(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t *readdata)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	return driver_db[DAR_DB_INDEX(dev_info)].ReadReg(dev_info, offset,
			readdata);
	/* Note: update_dev_info_regvals, as the registers managed should not change
	 * after being written.  On some devices (i.e. Tsi57x), reading these registers
	 * under certain conditions (port 0 powerdown) will not return the correct value.
	 */
}


uint32_t DARRegWrite(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t writedata)
{
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	rc = driver_db[DAR_DB_INDEX(dev_info)].WriteReg(dev_info, offset,
			writedata);
	if (RIO_SUCCESS == rc) {
		rc = update_dev_info_regvals(dev_info, offset, writedata);
	}
	return rc;
}


uint32_t DARrioGetNumLocalPorts(DAR_DEV_INFO_t *dev_info,
		uint32_t *numLocalPorts)
{
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	rc = DARRegRead(dev_info, RIO_SW_PORT_INF, numLocalPorts);
	if (RIO_SUCCESS == rc) {
		*numLocalPorts = RIO_AVAIL_PORTS(*numLocalPorts);
	} else {
		*numLocalPorts = 0;
	}
	return rc;
}


uint32_t DARrioGetFeatures(DAR_DEV_INFO_t *dev_info, RIO_PE_FEAT_T *features)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegRead(dev_info, RIO_PE_FEAT, features);
}


uint32_t DARrioGetSwitchPortInfo(DAR_DEV_INFO_t *dev_info,
		RIO_SW_PORT_INF_T *portinfo)
{
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	rc = DARRegRead(dev_info, RIO_SW_PORT_INF, portinfo);
	if (RIO_SUCCESS == rc) {
		/* If this is not a switch or a multiport-endpoint, portinfo
		 should be 0.  Fake the existence of the switch port info register
		 by supplying 1 for the number of ports, and  0 as the port that
		 we're connected to.

		 Otherwise, leave portinfo alone.
		 */
		if (!(dev_info->features & (RIO_PE_FEAT_SW | RIO_PE_FEAT_MULTIP))
				&& (!*portinfo)) {
			*portinfo = 0x00000100;
		}
	}
	return rc;
}


uint32_t DARrioGetExtFeaturesPtr(DAR_DEV_INFO_t *dev_info, uint32_t *extfptr)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		*extfptr = 0x00000000;
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & RIO_PE_FEAT_EFB_VALID) {
		*extfptr = dev_info->assyInfo & RIO_ASSY_INF_EFB_PTR;
		return RIO_SUCCESS;
	}
	return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARrioGetNextExtFeaturesPtr(DAR_DEV_INFO_t *dev_info,
		uint32_t currfptr, uint32_t *extfptr)
{
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		*extfptr = 0x00000000;
		return DAR_DB_INVALID_HANDLE;
	}

	if (currfptr & ( RIO_ASSY_INF_EFB_PTR >> 16)) {
		rc = DARRegRead(dev_info, currfptr, extfptr);
		*extfptr = (*extfptr & RIO_ASSY_INF_EFB_PTR) >> 16;
	} else {
		rc = RIO_ERR_FEATURE_NOT_SUPPORTED;
	}
	return rc;
}


uint32_t DARrioGetSourceOps(DAR_DEV_INFO_t *dev_info, RIO_SRC_OPS_T *srcops)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegRead(dev_info, RIO_SRC_OPS, srcops);
}


uint32_t DARrioGetDestOps(DAR_DEV_INFO_t *dev_info, RIO_DST_OPS_T *dstops)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegRead(dev_info, RIO_DST_OPS, dstops);
}


uint32_t DARrioGetAddressMode(DAR_DEV_INFO_t *dev_info, RIO_PE_ADDR_T *amode)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & RIO_PE_FEAT_EXT_ADDR) {
		return DARRegRead(dev_info, RIO_PE_LL_CTL, amode);
	}
	return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARrioGetBaseDeviceId(DAR_DEV_INFO_t *dev_info, uint32_t *deviceid)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	if (dev_info->features & (RIO_PE_FEAT_PROC |
			RIO_PE_FEAT_MEM |
			RIO_PE_FEAT_BRDG)) {
		return DARRegRead(dev_info, RIO_DEVID, deviceid);
	}
	return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARrioSetBaseDeviceId(DAR_DEV_INFO_t *dev_info, uint32_t newdeviceid)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & (RIO_PE_FEAT_PROC |
			RIO_PE_FEAT_MEM |
			RIO_PE_FEAT_BRDG)) {
		return DARRegWrite(dev_info, RIO_DEVID, newdeviceid);
	}
	return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARrioAcquireDeviceLock(DAR_DEV_INFO_t *dev_info,
		uint16_t hostdeviceid, uint16_t *hostlockid)
{
	uint32_t regVal;
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	// Reset value is 0xFFFF.
	// Write value 0x1234, it becomes 0x1234
	// Write value 0x1234 again, it becomes 0xFFFF.
	// Ignore writes of any other value when <> 0xFFFF
	//
	if ((hostdeviceid == RIO_HOST_LOCK_DEVID) || (!hostlockid)) {
		return RIO_ERR_INVALID_PARAMETER;
	}

	rc = DARRegRead(dev_info, RIO_HOST_LOCK, &regVal);
	if (RIO_SUCCESS != rc) {
		return rc;
	}

	*hostlockid = hostdeviceid;
	if ((uint16_t)(regVal) == hostdeviceid) {
		// Lock already held by this entity, return success
		return rc;
	}

	rc = DARRegWrite(dev_info, RIO_HOST_LOCK, hostdeviceid);
	if (RIO_SUCCESS != rc) {
		return rc;
	}

	rc = DARRegRead(dev_info, RIO_HOST_LOCK, &regVal);
	if (RIO_SUCCESS != rc) {
		return rc;
	}

	regVal &= RIO_HOST_LOCK_DEVID;
	*hostlockid = (uint16_t)(regVal);
	if (regVal != hostdeviceid) {
		return RIO_ERR_LOCK;
	}
	return RIO_SUCCESS;
}


uint32_t DARrioReleaseDeviceLock(DAR_DEV_INFO_t *dev_info,
		uint16_t hostdeviceid, uint16_t *hostlockid)
{
	uint32_t regVal;
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	rc = DARRegWrite(dev_info, RIO_HOST_LOCK, hostdeviceid);
	if (RIO_SUCCESS == rc) {
		rc = DARRegRead(dev_info, RIO_HOST_LOCK, &regVal);
		if (RIO_SUCCESS == rc) {
			*hostlockid = (uint16_t)(regVal & RIO_HOST_LOCK_DEVID);
			if (0xFFFF != *hostlockid) {
				rc = RIO_ERR_LOCK;
			}
		}
	}
	return rc;
}


uint32_t DARrioGetComponentTag(DAR_DEV_INFO_t *dev_info, uint32_t *componenttag)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegRead(dev_info, RIO_COMPTAG, componenttag);
}


uint32_t DARrioSetComponentTag(DAR_DEV_INFO_t *dev_info, uint32_t componenttag)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegWrite(dev_info, RIO_COMPTAG, componenttag);
}


uint32_t DARrioGetAddrMode(DAR_DEV_INFO_t *dev_info, RIO_PE_ADDR_T *addr_mode)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegRead(dev_info, RIO_PE_LL_CTL, addr_mode);
}


uint32_t DARrioSetAddrMode(DAR_DEV_INFO_t *dev_info, RIO_PE_ADDR_T addr_mode)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return DARRegWrite(dev_info, RIO_PE_LL_CTL, addr_mode);
}


uint32_t DARrioGetPortErrorStatus(DAR_DEV_INFO_t *dev_info, uint8_t portnum,
		RIO_SPX_ERR_STAT_T *err_status)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->extFPtrForPort) {
		if ((!portnum)
				|| (portnum
						< RIO_AVAIL_PORTS(
								dev_info->swPortInfo))) {
			return DARRegRead(dev_info,
					RIO_SPX_ERR_STAT(
							dev_info->extFPtrForPort,
							dev_info->extFPtrPortType,
							portnum), err_status);
		}
		return RIO_ERR_INVALID_PARAMETER;
	}
	return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARrioLinkReqNResp(DAR_DEV_INFO_t *dev_info, uint8_t portnum,
		uint32_t *link_stat)
{
	uint32_t rc;
	uint8_t attempts;
	uint32_t err_n_stat;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	if (dev_info->extFPtrForPort) {
		if ((!portnum)
				|| (portnum
						< RIO_AVAIL_PORTS(
								dev_info->swPortInfo))) {
			rc =
					DARRegRead(dev_info,
							RIO_SPX_ERR_STAT(
									dev_info->extFPtrForPort,
									dev_info->extFPtrPortType,
									portnum),
							&err_n_stat);
			if (RIO_SUCCESS != rc) {
				return rc;
			}

			if (!RIO_PORT_OK(err_n_stat)) {
				return RIO_ERR_PORT_OK_NOT_DETECTED;
			}

			rc =
					DARRegWrite(dev_info,
							RIO_SPX_LM_REQ(
									dev_info->extFPtrForPort,
									dev_info->extFPtrPortType,
									portnum),
							RIO_SPX_LM_REQ_CMD_LR_IS);
			if (RIO_SUCCESS != rc) {
				return rc;
			}

			/* Note that typically a link-response will be received faster than another
			 * maintenance packet can be issued.  Nevertheless, the routine polls 10 times
			 * to check for a valid response, where 10 is a small number assumed to give
			 * enough time for even the slowest device to respond.
			 */
			for (attempts = 0; attempts < 10; attempts++) {
				rc =
						DARRegRead(dev_info,
								RIO_SPX_LM_RESP(
										dev_info->extFPtrForPort,
										dev_info->extFPtrPortType,
										portnum),
								link_stat);

				if (RIO_SUCCESS != rc) {
					return rc;
				}

				if (RIO_SPX_LM_RESP_IS_VALID(*link_stat)) {
					return rc;
				}
			}
			return RIO_ERR_NOT_EXPECTED_RETURN_VALUE;
		}
		return RIO_ERR_INVALID_PARAMETER;
	}
	return RIO_ERR_FEATURE_NOT_SUPPORTED;
}


uint32_t DARrioStdRouteAddEntry(DAR_DEV_INFO_t *dev_info, uint16_t routedestid,
		uint8_t routeportno)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & RIO_PE_FEAT_SW) {
		if (RIO_SUCCESS
				== DARRegWrite(dev_info, RIO_DEVID_RTE,
						routedestid)) {
			return DARRegWrite(dev_info, RIO_RTE, routeportno);
		}
		return RIO_ERR_NO_SWITCH;
	}
	return RIO_ERR_NO_SWITCH;
}


uint32_t DARrioStdRouteGetEntry(DAR_DEV_INFO_t *dev_info, uint16_t routedestid,
		uint8_t *routeportno)
{
	uint32_t rc;
	uint32_t regVal;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & RIO_PE_FEAT_SW) {
		rc = DARRegWrite(dev_info, RIO_DEVID_RTE, routedestid);
		if (RIO_SUCCESS == rc) {
			rc = DARRegRead(dev_info, RIO_RTE, &regVal);
			*routeportno = (uint8_t)(regVal & RIO_RTE_PORT);
		}
		return rc;
	}
	return RIO_ERR_NO_SWITCH;
}


uint32_t DARrioStdRouteInitAll(DAR_DEV_INFO_t *dev_info, uint8_t routeportno)
{
	uint32_t rc;
	int32_t num_dests;
	int32_t dest_idx;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->swRtInfo && (dev_info->features & RIO_PE_FEAT_SW)) {
		num_dests = (dev_info->swRtInfo & RIO_SW_RT_TBL_LIM_MAX_DESTID);
		rc = DARrioStdRouteSetDefault(dev_info, routeportno);
		for (dest_idx = 0;
				((dest_idx <= num_dests) && (RIO_SUCCESS == rc));
				dest_idx++) {
			rc = DARrioStdRouteAddEntry(dev_info, dest_idx,
					routeportno);
		}
		return rc;
	}
	return RIO_ERR_NO_SWITCH;
}


uint32_t DARrioStdRouteRemoveEntry(DAR_DEV_INFO_t *dev_info,
		uint16_t routedestid)
{
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & RIO_PE_FEAT_SW) {
		rc = DARRegWrite(dev_info, RIO_DEVID_RTE, routedestid);
		if (RIO_SUCCESS == rc) {
			return DARRegWrite(dev_info, RIO_RTE,
			RIO_ALL_PORTS);
		}
		return rc;
	}
	return RIO_ERR_NO_SWITCH;
}


uint32_t DARrioStdRouteSetDefault(DAR_DEV_INFO_t *dev_info, uint8_t routeportno)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->features & RIO_PE_FEAT_SW) {
		return DARRegWrite(dev_info, RIO_DFLT_RTE, routeportno);
	}
	return RIO_ERR_NO_SWITCH;
}


uint32_t DARrioSetAssmblyInfo(DAR_DEV_INFO_t *dev_info, uint32_t asmblyVendID,
		uint16_t asmblyRev)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (RIO_CPS_DEVICE == dev_info->driver_family) {
		return CPS_rioSetAssmblyInfo(dev_info, asmblyVendID, asmblyRev);
	}
	return DARDB_rioSetAssmblyInfo(dev_info, asmblyVendID, asmblyRev);
}


uint32_t DARrioGetAssmblyInfo(DAR_DEV_INFO_t *dev_info, uint32_t *AsmblyVendID,
		uint16_t *AsmblyRev)
{
	uint32_t temp;
	uint32_t rc;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	rc = DARRegRead(dev_info, RIO_ASSY_ID, AsmblyVendID);
	if (RIO_SUCCESS == rc) {
		rc = DARRegRead(dev_info, RIO_ASSY_INF, &temp);
		if (RIO_SUCCESS == rc) {
			temp = (temp & RIO_ASSY_INF_ASSY_REV) >> 16;
			*AsmblyRev = (uint16_t)(temp);
		}
	}
	return rc;
}


uint32_t DARrioGetPortList(DAR_DEV_INFO_t *dev_info, struct DAR_ptl *ptl_in,
		struct DAR_ptl *ptl_out)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (!ptl_in || !ptl_out) {
		return RIO_ERR_NULL_PARM_PTR;
	}

	if (RIO_CPS_DEVICE == dev_info->driver_family) {
		return CPS_rioGetPortList(dev_info, ptl_in, ptl_out);
	}
	return DARDB_rioGetPortList(dev_info, ptl_in, ptl_out);
}


uint32_t DARrioSetEnumBound(DAR_DEV_INFO_t *dev_info, struct DAR_ptl *ptl,
		int enum_bnd_val)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (!ptl) {
		return RIO_ERR_NULL_PARM_PTR;
	}

	if (RIO_RXS_DEVICE == dev_info->driver_family) {
		return rxs_rioSetEnumBound(dev_info, ptl, enum_bnd_val);
	}
	return DARDB_rioSetEnumBound(dev_info, ptl, enum_bnd_val);
}


uint32_t DARrioGetDevResetInitStatus(DAR_DEV_INFO_t *dev_info)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}
	return RIO_SUCCESS;
}


uint32_t DARrioPortEnable(DAR_DEV_INFO_t *dev_info, struct DAR_ptl *ptl,
		bool port_ena, bool port_lkout, bool in_out_ena)
{
	uint32_t rc;
	uint32_t port_n_ctrl1_addr;
	uint32_t port_n_ctrl1_reg;
	struct DAR_ptl good_ptl;
	uint8_t idx;

	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (!ptl) {
		return RIO_ERR_NULL_PARM_PTR;
	}

	/* Check whether 'portno' is assigned to a valid port value or not
	 */
	rc = DARrioGetPortList(dev_info, ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		return rc;
	}

	for (idx = 0; idx < good_ptl.num_ports; idx++) {
		port_n_ctrl1_addr = RIO_SPX_CTL(dev_info->extFPtrForPort,
				dev_info->extFPtrPortType, good_ptl.pnums[idx]);

		rc = DARRegRead(dev_info, port_n_ctrl1_addr, &port_n_ctrl1_reg);
		if (RIO_SUCCESS != rc) {
			return rc;
		}

		if (port_ena) {
			port_n_ctrl1_reg &= ~RIO_SPX_CTL_PORT_DIS;
		} else {
			port_n_ctrl1_reg |= RIO_SPX_CTL_PORT_DIS;
		}

		if (port_lkout) {
			port_n_ctrl1_reg |= RIO_SPX_CTL_LOCKOUT;
		} else {
			port_n_ctrl1_reg &= ~RIO_SPX_CTL_LOCKOUT;
		}

		if (in_out_ena) {
			port_n_ctrl1_reg |= RIO_SPX_CTL_INP_EN
					| RIO_SPX_CTL_OTP_EN;
		} else {
			port_n_ctrl1_reg &= ~(RIO_SPX_CTL_INP_EN
					| RIO_SPX_CTL_OTP_EN);
		}

		rc = DARRegWrite(dev_info, port_n_ctrl1_addr, port_n_ctrl1_reg);
	}
	return rc;
}


uint32_t DARrioEmergencyLockout(DAR_DEV_INFO_t *dev_info, uint8_t port_num)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if (dev_info->extFPtrForPort && RIO_SP_VLD(dev_info->extFPtrPortType)
			&& (port_num < NUM_PORTS(dev_info))) {
		return DARRegWrite(dev_info,
				RIO_SPX_CTL(dev_info->extFPtrForPort,
						dev_info->extFPtrPortType,
						port_num),
				dev_info->ctl1_reg[port_num]
						| RIO_SPX_CTL_LOCKOUT);
	}
	return RIO_ERR_INVALID_PARAMETER;
}


uint32_t DARrioDeviceRemoved(DAR_DEV_INFO_t *dev_info)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	if ( NULL != dev_info->privateData) {
		return DAR_DB_DRIVER_INFO;
	}
	return RIO_SUCCESS;
}

uint32_t DARrioDeviceSupported(DAR_DEV_INFO_t *dev_info)
{
	if (!VALIDATE_DEV_INFO(dev_info)) {
		return DAR_DB_INVALID_HANDLE;
	}

	switch(dev_info->driver_family)
	{
	case RIO_CPS_DEVICE:
		return CPS_rioDeviceSupported(dev_info);
	case RIO_RXS_DEVICE:
		return rxs_rioDeviceSupported(dev_info);
	case RIO_TSI721_DEVICE:
		return tsi721_rioDeviceSupported(dev_info);
	case RIO_TSI57X_DEVICE:
		return tsi57x_rioDeviceSupported(dev_info);
	case RIO_UNKNOWN_DEVICE:
		return DARDB_rioDeviceSupported(dev_info);
	case RIO_UNITIALIZED_DEVICE:
	default:
		return DARDB_rioDeviceSupportedStub(dev_info);
	}
}

#ifdef __cplusplus
}
#endif
