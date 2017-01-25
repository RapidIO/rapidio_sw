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

/* NOTE: TSI721_PW_CTL_PWC_MODE (Reliable port-write reception) is
* always disabled by this routine.
*/
uint32_t idt_tsi721_em_cfg_pw (DAR_DEV_INFO_t *dev_info, 
				rio_em_cfg_pw_in_t   *in_parms, 
				rio_em_cfg_pw_out_t  *out_parms) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t regData;
	uint32_t pw_mask;
	uint32_t retx;

	out_parms->imp_rc = RIO_SUCCESS;

	if (in_parms->priority > 3) {
		out_parms->imp_rc = EM_CFG_PW(1);
		goto exit;
	};
			
	// Set all port-write configuration information.
	// For Tsi721, this is limitted to port-write 
	// destination ID and retransmission rate.

	// Configure 8 or 16 bit destination ID for port writes.
	pw_mask = TSI721_PW_TGT_ID_PW_TGT_ID;
	if (tt_dev16 == in_parms->deviceID_tt) {
		pw_mask |= TSI721_PW_TGT_ID_MSB_PW_ID;
	}
	regData = ((uint32_t)(in_parms->port_write_destID)) << 16;
	regData &= pw_mask;
	if (tt_dev16 == in_parms->deviceID_tt) {
		regData |= TSI721_PW_TGT_ID_LRG_TRANS;
	};

	rc = DARRegWrite( dev_info, TSI721_PW_TGT_ID, regData );
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CFG_PW(2);
		 goto exit;
	};

	// Configure port-write re-transmission rate.
	// Assumption: it is better to choose a longer retransmission time
	// than the value requested.
 
	regData = 0;
	retx = in_parms->port_write_re_tx;

	if (retx) {
		if (retx <= RIO_EM_TSI721_PW_RE_TX_103us) {
			regData = TSI721_PW_CTL_PW_TIMER_103us;
		} else if (retx <= RIO_EM_TSI721_PW_RE_TX_205us) {
			regData = TSI721_PW_CTL_PW_TIMER_205us;
		} else if (retx <= RIO_EM_TSI721_PW_RE_TX_410us) {
			regData = TSI721_PW_CTL_PW_TIMER_410us;
		} else {
			regData = TSI721_PW_CTL_PW_TIMER_820us;
		};
	};

	rc = DARRegWrite(dev_info, TSI721_PW_CTL, regData);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CFG_PW(3);
		 goto exit;
	};

	// Port writes are sent out the RapidIO Port... Always...
	// Note that if a system master sends a port-write,
	// the port write might get routed right back to the master...
	rc = DARRegWrite(dev_info, TSI721_PW_ROUTE,
						TSI721_PW_ROUTE_PORT);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CFG_PW(3);
		 goto exit;
	};

	// Read back current port write configuration data
	rc = DARRegRead(dev_info, TSI721_PW_TGT_ID, &regData);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CFG_PW(4);
		 goto exit;
	};

	out_parms->deviceID_tt = (regData & TSI721_PW_TGT_ID_LRG_TRANS) ?
				tt_dev16 : tt_dev8;
	pw_mask = TSI721_PW_TGT_ID_PW_TGT_ID;
	if (tt_dev16 == out_parms->deviceID_tt) {
		pw_mask |= TSI721_PW_TGT_ID_MSB_PW_ID;
	}
	out_parms->port_write_destID = (uint16_t)((regData & pw_mask) >> 16);

	// Source ID for port writes is found in the
	// TSI721_BASE_ID.  Source ID for port-writes
	// cannot be set separately.

	rc = DARRegRead( dev_info, TSI721_BASE_ID, &regData );
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CFG_PW(3);
		 goto exit;
	};

	out_parms->srcID_valid = true;
	if (tt_dev8 == out_parms->deviceID_tt) {
		out_parms->port_write_srcID =
				(regData & TSI721_BASE_ID_BASE_ID)>> 16;
	} else {
		out_parms->port_write_srcID =
				regData & TSI721_BASE_ID_LAR_BASE_ID;
	};
	// Cannot configure port-write priority or CRF.
	out_parms->priority = 3;
	out_parms->CRF			= true;

	// Figure out retransmission value.
	rc = DARRegRead( dev_info, TSI721_PW_CTL, &regData );
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CFG_PW(3);
		 goto exit;
	};

	switch (regData) {
		 case 0: out_parms->port_write_re_tx = 0;
			 break;
		 case TSI721_PW_CTL_PW_TIMER_103us:
			out_parms->port_write_re_tx =
						RIO_EM_TSI721_PW_RE_TX_103us;
			break;
		 case TSI721_PW_CTL_PW_TIMER_205us:
			out_parms->port_write_re_tx =
						RIO_EM_TSI721_PW_RE_TX_205us;
			break;
		 case TSI721_PW_CTL_PW_TIMER_410us:
			out_parms->port_write_re_tx = 
						RIO_EM_TSI721_PW_RE_TX_410us;
				 break;
		 case TSI721_PW_CTL_PW_TIMER_820us:
			out_parms->port_write_re_tx =
						RIO_EM_TSI721_PW_RE_TX_820us;
			break;
		 default:
			out_parms->port_write_re_tx = regData;
			rc = RIO_ERR_READ_REG_RETURN_INVALID_VAL;
			out_parms->imp_rc = EM_CFG_PW(9);
	}
exit:
	return rc; 
};

uint32_t tsi721_em_determine_notfn( DAR_DEV_INFO_t       *dev_info  , 
                                  rio_em_notfn_ctl_t   *notfn      ,
                                  uint8_t                 pnum      ,
                                  uint32_t               *imp_rc    ) 
{
	if (NULL != dev_info)
		*imp_rc = pnum + *(int*)notfn;

    return RIO_ERR_FEATURE_NOT_SUPPORTED;
} 

typedef struct tsi721_event_cfg_reg_vals_t_TAG {
	uint32_t plm_imp_spec_ctl;
	uint32_t plm_int_enable;
	uint32_t plm_pw_enable;
	uint32_t plm_denial_ctl;
	uint32_t sp_rate_en;
	uint32_t sp_err_rate;
	uint32_t sp_err_thresh;
	uint32_t sp_ctl;
	uint32_t i2c_int_enable;
	uint32_t em_rst_int_en;
	uint32_t em_rst_pw_en;
	uint32_t devctl; // Read Only!!!
	uint32_t local_err_en;
} tsi721_event_cfg_reg_vals_t;

uint32_t tsi721_event_get_regs(DAR_DEV_INFO_t *dev_info,
			tsi721_event_cfg_reg_vals_t *regs)
{
	uint32_t rc;

	rc = DARRegRead(dev_info, TSI721_PLM_IMP_SPEC_CTL,
			&regs->plm_imp_spec_ctl);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_PLM_INT_ENABLE,
			&regs->plm_int_enable);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_PLM_PW_ENABLE,
			&regs->plm_pw_enable);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_PLM_DENIAL_CTL,
			&regs->plm_denial_ctl);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_SP_RATE_EN,
			&regs->sp_rate_en);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_SP_ERR_RATE,
			&regs->sp_err_rate);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_SP_ERR_THRESH,
			&regs->sp_err_thresh);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_SP_CTL,
			&regs->sp_ctl);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_I2C_INT_ENABLE,
			&regs->i2c_int_enable);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_EM_RST_INT_EN,
			&regs->em_rst_int_en);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_EM_RST_PW_EN,
			&regs->em_rst_pw_en);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_DEVCTL,
			&regs->devctl);
	if (rc) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_LOCAL_ERR_EN,
			&regs->local_err_en);
	if (rc) {
		goto fail;
	}
fail:
	return rc;
};

uint32_t tsi721_event_write_regs(DAR_DEV_INFO_t *dev_info,
			tsi721_event_cfg_reg_vals_t *regs)
{
	uint32_t rc;

	rc = DARRegWrite(dev_info, TSI721_PLM_IMP_SPEC_CTL,
			regs->plm_imp_spec_ctl);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_PLM_INT_ENABLE,
			regs->plm_int_enable);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_PLM_PW_ENABLE,
			regs->plm_pw_enable);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_PLM_DENIAL_CTL,
			regs->plm_denial_ctl);
	if (rc) {
		goto fail;
	}
	rc = DARRegWrite(dev_info, TSI721_SP_RATE_EN,
			regs->sp_rate_en);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_SP_ERR_RATE,
			regs->sp_err_rate);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_SP_ERR_THRESH,
			regs->sp_err_thresh);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_SP_CTL,
			regs->sp_ctl);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_I2C_INT_ENABLE,
			regs->i2c_int_enable);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_EM_RST_INT_EN,
			regs->em_rst_int_en);
	if (rc) {
		goto fail;
	}

	rc = DARRegWrite(dev_info, TSI721_EM_RST_PW_EN,
			regs->em_rst_pw_en);
	if (rc) {
		goto fail;
	}

	// DEVCTL is a read only register!
	// rc = DARRegWrite(dev_info, TSI721_DEVCTL,
	// 		&regs->tsi721_devctl);
	// if (rc) {
	// 	goto fail;
	// }

	rc = DARRegWrite(dev_info, TSI721_LOCAL_ERR_EN,
			regs->local_err_en);
	if (rc) {
		goto fail;
	}
fail:
	return rc;
}

uint32_t tsi721_plm_set_notifn(tsi721_event_cfg_reg_vals_t *regs,
				uint32_t mask,
				rio_em_notfn_ctl_t notfn)
{
	uint32_t rc = RIO_SUCCESS;

	switch(notfn) {
	case rio_em_notfn_none:
			regs->plm_int_enable &= ~mask;
			regs->plm_pw_enable &= ~mask;
			break;
	case rio_em_notfn_int:
			regs->plm_int_enable |= mask;
			regs->plm_pw_enable &= ~mask;
			break;
	case rio_em_notfn_pw:
			regs->plm_int_enable &= ~mask;
			regs->plm_pw_enable |= mask;
			break;
	case rio_em_notfn_both:
			regs->plm_int_enable |= mask;
			regs->plm_pw_enable |= mask;
			break;
	case rio_em_notfn_0delta:
			break;
	default: rc = RIO_ERR_INVALID_PARAMETER;
		break;
	}
	return rc;
}

#define TSI721_ERR_RATE_EVENT_EXCLUSIONS \
		(TSI721_SP_ERR_DET_IMP_SPEC | TSI721_SP_ERR_DET_CS_NOT_ACC)
	
uint32_t tsi721_set_event_cfg(tsi721_event_cfg_reg_vals_t *regs,
				rio_em_cfg_t *event,
				rio_em_notfn_ctl_t nfn,
				uint32_t *imp_rc)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t mask;

	if (( event->em_detect >= rio_em_detect_last) ||
			(event->em_event >= rio_em_last)) {
		*imp_rc = EM_CFG_SET(0x20);
		goto fail;
	};

	if (rio_em_detect_0delta == event->em_detect) {
		rc = RIO_SUCCESS;
		goto fail;
	}

	switch (event->em_event) {
	case rio_em_f_los:
		regs->plm_imp_spec_ctl &=
			~TSI721_PLM_IMP_SPEC_CTL_DLT_THRESH;

		if (rio_em_detect_on == event->em_detect) {
			// TODO: Could have a more exact figure than 1000
			// based on link speed/prescaler value
			uint32_t time = event->em_info / 1000 / 256;

			if (!event->em_info) {
				*imp_rc = EM_CFG_SET(0x28);
				goto fail;
			}
			if (!time) {
				time = 1;
			}
			if (time > TSI721_PLM_IMP_SPEC_CTL_DLT_THRESH) {
				time = TSI721_PLM_IMP_SPEC_CTL_DLT_THRESH;
			};
			regs->plm_imp_spec_ctl &=
				~TSI721_PLM_IMP_SPEC_CTL_DLT_THRESH;
			regs->plm_imp_spec_ctl |= time;
		}
		rc = tsi721_plm_set_notifn(regs, TSI721_PLM_STATUS_DLT, nfn);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x29);
			goto fail;
		};
		break;

	case rio_em_f_port_err:
		rc = tsi721_plm_set_notifn(regs, TSI721_PLM_STATUS_PORT_ERR,
								nfn);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x30);
			goto fail;
		};
		break;
	case rio_em_f_2many_retx:
		// Note: only PNA with a reason of 
		// "Packet not accepted due to lack of resources"
		// count towards this threshold.  All other reasons are
		// tracked through the standard Error Management thresholds.
		regs->plm_denial_ctl =
			TSI721_PLM_DENIAL_CTL_CNT_RTY |
			TSI721_PLM_DENIAL_CTL_CNT_PNA;

		if (rio_em_detect_on == event->em_detect) {
			if (!event->em_info) {
				*imp_rc = EM_CFG_SET(0x38);
				goto fail;
			};
			if (event->em_info >=
				TSI721_PLM_DENIAL_CTL_DENIAL_THRESH) {
				regs->plm_denial_ctl |=
				   TSI721_PLM_DENIAL_CTL_DENIAL_THRESH;
			} else {
				regs->plm_denial_ctl |= event->em_info;
			}
		}
		rc = tsi721_plm_set_notifn(regs,
					TSI721_PLM_STATUS_MAX_DENIAL, nfn);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x39);
			goto fail;
		};
		break;

	case rio_em_f_2many_pna:
		// Setting a rate specific to PNA will overwrite the rates
		// set for rio_em_f_err_rate.
		if (regs->sp_rate_en & ~TSI721_SP_RATE_EN_CS_NOT_ACC_EN)
		{
			break;
		}
		if (rio_em_detect_off == event->em_detect) {
			regs->sp_rate_en &= 
					~TSI721_SP_RATE_EN_CS_NOT_ACC_EN;
		} else {
			uint32_t thresh = event->em_info;
			regs->sp_rate_en |= 
					TSI721_SP_RATE_EN_CS_NOT_ACC_EN;
			if (thresh > 0xFF) {
				thresh = 0xFF;
			};
			thresh = thresh << 24 & TSI721_SP_ERR_THRESH_ERR_RFT;
			regs->sp_rate_en &=
					~TSI721_SP_ERR_THRESH_ERR_RFT;
			regs->sp_rate_en |= thresh;
			regs->sp_ctl |= TSI721_SP_CTL_DROP_EN |
						TSI721_SP_CTL_STOP_FAIL_EN;
		}
		rc = tsi721_plm_set_notifn(regs,
					TSI721_PLM_STATUS_OUTPUT_FAIL, nfn);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x39);
			goto fail;
		};
		break;

	case rio_em_f_err_rate:
		{
		uint32_t rate_en, err_thresh, err_rate;

		rc = rio_em_get_f_err_rate_info(event->em_info,
			&rate_en, &err_rate, &err_thresh);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x40);
			goto fail;
		}

		if (rio_em_detect_off == event->em_detect) {
			regs->sp_rate_en &= 
				TSI721_ERR_RATE_EVENT_EXCLUSIONS;
			regs->sp_err_thresh &=
					~TSI721_SP_ERR_THRESH_ERR_RFT;
			regs->sp_err_rate = 0;
		} else {
			if (!(err_thresh & TSI721_SP_ERR_THRESH_ERR_RFT)) {
				*imp_rc = EM_CFG_SET(0x41);
				goto fail;
			}
			regs->sp_rate_en =
				(rate_en & ~TSI721_ERR_RATE_EVENT_EXCLUSIONS) |
				(regs->sp_rate_en &
					TSI721_ERR_RATE_EVENT_EXCLUSIONS);
			regs->sp_err_thresh =
				(err_thresh & TSI721_SP_ERR_THRESH_ERR_RFT) |
				(regs->sp_err_thresh  &
					TSI721_SP_ERR_THRESH_ERR_RFT);
			regs->sp_err_rate = err_rate;
			regs->sp_ctl |= TSI721_SP_CTL_DROP_EN |
						TSI721_SP_CTL_STOP_FAIL_EN;
		}
			
		rc = tsi721_plm_set_notifn(regs,
					TSI721_PLM_STATUS_OUTPUT_FAIL, nfn);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x42);
			goto fail;
		}
		}
		break;

	case rio_em_d_ttl:
		// Time to live is only supported by switches, not Tsi721
		break;

	case rio_em_d_rte:
		// Routing table events are only supported by switches,
		// not Tsi721
		break;

	case rio_em_d_log:
		// Tsi721 logical layer errors are always detected.
		// There is no support for standard logical layer errors,
		//    only implementation specific ones.
		// Notification is limited to port writes.
		mask = TSI721_LOCAL_ERR_EN_ILL_TYPE_EN |
			TSI721_LOCAL_ERR_EN_ILL_ID_EN;
		switch (event->em_detect) {
		case rio_em_detect_on:
			regs->local_err_en &= !mask;
			regs->local_err_en |= event->em_info & mask;
			break;
		case rio_em_detect_off:
			regs->local_err_en &= !mask;
			break;
		case rio_em_detect_0delta:
			break;
		default:
			*imp_rc = EM_CFG_SET(0x44);
			goto fail;
		}
		break;
		
	case rio_em_i_sig_det:
		switch (event->em_detect) {
		case rio_em_detect_on:
			regs->sp_ctl |= TSI721_SP_CTL_PORT_LOCKOUT;
			break;
		case rio_em_detect_off:
			regs->sp_ctl &= ~TSI721_SP_CTL_PORT_LOCKOUT;
			break;
		case rio_em_detect_0delta:
			break;
		default:
			*imp_rc = EM_CFG_SET(0x48);
			goto fail;
		}
		break;
		rc = tsi721_plm_set_notifn(regs,
					TSI721_PLM_STATUS_LINK_INIT, nfn);
		if (RIO_SUCCESS != rc) {
			*imp_rc = EM_CFG_SET(0x49);
			goto fail;
		}
		break;

	case rio_em_i_rst_req:
		// Note: Control of PLM detection and notification for 
		// this event is supported by the
		// rio_pc_dev_reset_config/idt_tsi721_pc_dev_reset_config
		// routine.
		//
		// This routine is responsible for device notification control
		// for the reset event.
		switch (nfn) {
		case rio_em_notfn_none:
			regs->em_rst_int_en = 0;
			regs->em_rst_pw_en = 0;
			break;
		case rio_em_notfn_int:
			regs->em_rst_int_en = TSI721_EM_RST_INT_EN_RST_INT_EN;
			regs->em_rst_pw_en = 0;
			break;
		case rio_em_notfn_pw:
			regs->em_rst_int_en = TSI721_EM_RST_PW_EN_RST_PW_EN;
			regs->em_rst_pw_en = 0;
			break;
		case rio_em_notfn_both:
			regs->em_rst_int_en = TSI721_EM_RST_INT_EN_RST_INT_EN;
			regs->em_rst_pw_en = TSI721_EM_RST_PW_EN_RST_PW_EN;
			break;
		case rio_em_notfn_0delta:
			break;
		default:
			*imp_rc = EM_CFG_SET(0x50);
			goto fail;
		}
		break;

	case rio_em_i_init_fail:
		switch (nfn) {
		case rio_em_notfn_none:
			regs->i2c_int_enable &= ~TSI721_I2C_INT_ENABLE_BL_FAIL;
			break;
		case rio_em_notfn_both:
		case rio_em_notfn_int:
			regs->i2c_int_enable |= TSI721_I2C_INT_ENABLE_BL_FAIL;
			break;
		default:
			break;
		}
		break;

	case rio_em_a_clr_pwpnd:
		break;

	case rio_em_a_no_event:
		break;
		
	default:
		rc = RIO_ERR_INVALID_PARAMETER;
		*imp_rc = EM_CFG_SET(0x80);
		goto fail;
	}

	rc = RIO_SUCCESS;
fail:
	return rc;
}

uint32_t idt_tsi721_em_dev_rpt_ctl (DAR_DEV_INFO_t *dev_info, 
                                    rio_em_dev_rpt_ctl_in_t *in_parms, 
                                    rio_em_dev_rpt_ctl_out_t *out_parms);

uint32_t idt_tsi721_em_cfg_set  (DAR_DEV_INFO_t        *dev_info, 
                                rio_em_cfg_set_in_t   *in_parms, 
                                rio_em_cfg_set_out_t  *out_parms ) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t idx;
	struct DAR_ptl good_ptl;
	rio_pc_get_config_in_t cfg_in;
	rio_pc_get_config_out_t cfg_out;
	tsi721_event_cfg_reg_vals_t regs;
	rio_em_dev_rpt_ctl_in_t rpt_ctl_in; 
	rio_em_dev_rpt_ctl_out_t rpt_ctl_out;

	// Set output info
	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->fail_port_num = RIO_ALL_PORTS;
	out_parms->fail_idx = rio_em_last;
	out_parms->notfn = rio_em_notfn_0delta;

	// Parameter checks.
	if ((in_parms->num_events > (uint8_t)(rio_em_last)) ||
		(NULL == in_parms->events) ||
		(rio_em_notfn_last <= in_parms->notfn)) {
		out_parms->imp_rc = EM_CFG_SET(0x1);
		goto fail;
	};

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_CFG_SET(0x2);
		goto fail;
	};

	// Get current port configuration.
	cfg_in.ptl = good_ptl;
	rc = idt_tsi721_pc_get_config(dev_info, &cfg_in, &cfg_out);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = cfg_out.imp_rc;
		goto fail;
	}
	
	// Nothing to set if the port is unavailable or powered down.
	if (!cfg_out.pc[0].port_available || !cfg_out.pc[0].powered_up) {
		goto fail;
	}
	
	rc = tsi721_event_get_regs(dev_info, &regs);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_CFG_SET(0x8);
		goto fail;
	}
	// First, perform all event disables & validate events
	for (idx = 0; idx < in_parms->num_events; idx++) {
		if (rio_em_detect_on != in_parms->events[idx].em_detect) {
			rc = tsi721_set_event_cfg( &regs,
					&in_parms->events[idx],
					in_parms->notfn,
					&out_parms->imp_rc);
			if (RIO_SUCCESS != rc) {
				out_parms->fail_idx = idx;
				goto fail;
			}
		}
	}

	// Next, perform all event enables.
	for (idx = 0; idx < in_parms->num_events; idx++) {
		if (rio_em_detect_on == in_parms->events[idx].em_detect) {
			rc = tsi721_set_event_cfg(&regs, 
					&in_parms->events[idx],
					in_parms->notfn,
					&out_parms->imp_rc);
			if (RIO_SUCCESS != rc) {
				out_parms->fail_idx = idx;
				goto fail;
			}
		}
	}

	rc = tsi721_event_write_regs(dev_info, &regs);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_CFG_SET(0x9F);
		goto fail;
	}

	rpt_ctl_in.ptl.num_ports = RIO_ALL_PORTS;
	rpt_ctl_in.notfn = in_parms->notfn;
	rc = idt_tsi721_em_dev_rpt_ctl(dev_info, &rpt_ctl_in, &rpt_ctl_out);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = rpt_ctl_out.imp_rc;
		goto fail;
	}
	out_parms->notfn = rpt_ctl_out.notfn;
fail:
	return rc;
};
			
uint32_t tsi721_get_event_cfg(rio_em_cfg_t *event,
				tsi721_event_cfg_reg_vals_t *regs)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t mask;

	event->em_detect = rio_em_detect_0delta;
	event->em_info = 0;

	switch (event->em_event) {
	case rio_em_f_los:
		event->em_info = regs->plm_imp_spec_ctl &
			~TSI721_PLM_IMP_SPEC_CTL_DLT_THRESH;
		event->em_info = event->em_info * 256 * 1000;
		if (event->em_info) {
			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;

	case rio_em_f_port_err:
		if ((regs->plm_int_enable & TSI721_PLM_INT_ENABLE_PORT_ERR) |
		    (regs->plm_pw_enable & TSI721_PLM_PW_ENABLE_PORT_ERR)) {
			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;

	case rio_em_f_2many_retx:
		event->em_info = regs->plm_denial_ctl &
				TSI721_PLM_DENIAL_CTL_DENIAL_THRESH;
		if (event->em_info) {
			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;

	case rio_em_f_2many_pna:
		if (regs->sp_rate_en & TSI721_SP_RATE_EN_CS_NOT_ACC_EN) {
			event->em_detect = rio_em_detect_on;
			event->em_info = regs->sp_rate_en &
					TSI721_SP_ERR_THRESH_ERR_RFT;
			event->em_info = event->em_info >> 24;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;

	case rio_em_f_err_rate:
		rc = rio_em_compute_f_err_rate_info(
			regs->sp_rate_en,
			regs->sp_err_thresh,
			regs->sp_err_rate,
			&event->em_info);

		if ((regs->sp_rate_en &
					~TSI721_ERR_RATE_EVENT_EXCLUSIONS)
			&& (regs->sp_err_thresh &
					TSI721_SP_ERR_THRESH_ERR_RFT)) {

			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;
	case rio_em_d_ttl:
		break;
	case rio_em_d_rte:
		break;
	case rio_em_d_log:
		// Tsi721 logical layer errors are always detected.
		// There is no support for standard logical layer errors,
		//    only implementation specific ones.
		// Notification is limited to port writes.
		mask = TSI721_LOCAL_ERR_EN_ILL_TYPE_EN |
			TSI721_LOCAL_ERR_EN_ILL_ID_EN;
		event->em_info = (regs->local_err_en & mask);
		if (event->em_info) {
			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;
	case rio_em_i_sig_det:
		if (regs->sp_ctl & TSI721_SP_CTL_PORT_LOCKOUT) {
			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;
	case rio_em_i_rst_req:
		if ((regs->devctl & TSI721_PLM_IMP_SPEC_CTL_SELF_RST) ||
		(regs->devctl & TSI721_PLM_IMP_SPEC_CTL_PORT_SELF_RST)) {
			event->em_detect = rio_em_detect_off;
		} else {
			event->em_detect = rio_em_detect_on;
		}
		break;
	case rio_em_i_init_fail:
		if (regs->i2c_int_enable & TSI721_I2C_INT_ENABLE_BL_FAIL) {
			event->em_detect = rio_em_detect_on;
		} else {
			event->em_detect = rio_em_detect_off;
		}
		break;
	case rio_em_a_clr_pwpnd:
	case rio_em_a_no_event:
	default: goto fail;
		break;
	}
	rc = RIO_SUCCESS;
fail:
	return rc;
}

uint32_t idt_tsi721_em_cfg_get(DAR_DEV_INFO_t *dev_info,
				rio_em_cfg_get_in_t *in_parms,
				rio_em_cfg_get_out_t *out_parms) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t idx;
	rio_pc_get_config_in_t cfg_in;
	rio_pc_get_config_out_t cfg_out;
	tsi721_event_cfg_reg_vals_t regs;
	rio_em_dev_rpt_ctl_in_t rpt_ctl_in; 
	rio_em_dev_rpt_ctl_out_t rpt_ctl_out;

	// Set output info
	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->fail_idx = rio_em_last;
	out_parms->notfn = rio_em_notfn_0delta;

	// Parameter checks.
	if ((in_parms->num_events > (uint8_t)(rio_em_last)) ||
			(in_parms->port_num) ||
			(NULL == in_parms->events) ||
			(NULL == in_parms->event_list)) {
		out_parms->imp_rc = EM_CFG_SET(0x1);
		goto fail;
	};

	// Get current port configuration.
	cfg_in.ptl.num_ports = 1;
	cfg_in.ptl.pnums[0] = 0;
	rc = idt_tsi721_pc_get_config(dev_info, &cfg_in, &cfg_out);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = cfg_out.imp_rc;
		goto fail;
	}
	
	// Nothing to set if the port is unavailable or powered down.
	if (!cfg_out.pc[0].port_available || !cfg_out.pc[0].powered_up) {
		goto fail;
	}

	rc = tsi721_event_get_regs(dev_info, &regs);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_CFG_GET(0x8);
		goto fail;
	}

	for (idx = 0; idx < in_parms->num_events; idx++) {
		in_parms->events[idx].em_event = in_parms->event_list[idx];
		rc = tsi721_get_event_cfg(&in_parms->events[idx], &regs);
		if (RIO_SUCCESS != rc) {
			out_parms->fail_idx = idx;
			goto fail;
		}
	}

	rpt_ctl_in.ptl.num_ports = RIO_ALL_PORTS;
	rpt_ctl_in.notfn = rio_em_notfn_0delta;
	rc = idt_tsi721_em_dev_rpt_ctl (dev_info, &rpt_ctl_in, &rpt_ctl_out);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = rpt_ctl_out.imp_rc;
		goto fail;
	}
	out_parms->notfn = rpt_ctl_out.notfn;
	rc = RIO_SUCCESS;
fail:
	return rc;
};

uint32_t idt_tsi721_em_dev_rpt_ctl (DAR_DEV_INFO_t *dev_info, 
                                    rio_em_dev_rpt_ctl_in_t *in_parms, 
                                    rio_em_dev_rpt_ctl_out_t *out_parms)
{
	struct DAR_ptl good_ptl;
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	uint32_t em_dev_pw_en = 0;
	uint32_t em_dev_int_en = 0;
	uint32_t plm_all_pw_en = 0;
	uint32_t plm_all_int_en = 0;
	uint32_t em_pw_enable = 0;
	uint32_t em_int_enable = 0;

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x1);
		goto fail;
	};

	if (rio_em_notfn_0delta == in_parms->notfn) {
		goto get_dev_rpt_ctl;
	};

	if (rio_em_notfn_last <= in_parms->notfn) {
		goto fail;
	};

	// TODO: Keep this routine stubbed until testing is completed.
	if (NULL != in_parms) {
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_EM_INT_ENABLE, &em_int_enable);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x10);
		goto fail;
	}
	rc = DARRegRead(dev_info, TSI721_EM_PW_ENABLE, &em_pw_enable);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x18);
		goto fail;
	}

	em_int_enable &= ~TSI721_EM_INT_ENABLE_LOCALOG;
	em_pw_enable &= ~TSI721_EM_PW_ENABLE_LOCALOG;
	if ((rio_em_notfn_int == in_parms->notfn) ||
			(rio_em_notfn_both == in_parms->notfn)) {
		em_dev_int_en = TSI721_EM_DEV_INT_EN_INT_EN;
		plm_all_int_en = TSI721_PLM_ALL_INT_EN_IRQ_EN;
		em_int_enable |= TSI721_EM_INT_ENABLE_LOCALOG;
	}

	if ((rio_em_notfn_pw == in_parms->notfn) ||
			(rio_em_notfn_both == in_parms->notfn)) {
		em_dev_pw_en = TSI721_EM_DEV_PW_EN_PW_EN;
		plm_all_pw_en = TSI721_PLM_ALL_PW_EN_PW_EN;
		em_pw_enable = TSI721_EM_PW_ENABLE_LOCALOG;
	}
	rc = DARRegWrite(dev_info, TSI721_PLM_ALL_INT_EN, plm_all_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x20);
		goto fail;
	}
	rc = DARRegWrite(dev_info, TSI721_PLM_ALL_PW_EN, plm_all_pw_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x24);
		goto fail;
	}
	rc = DARRegWrite(dev_info, TSI721_EM_INT_ENABLE, em_int_enable);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x28);
		goto fail;
	}
	rc = DARRegWrite(dev_info, TSI721_EM_PW_ENABLE, em_pw_enable);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x2C);
		goto fail;
	}
	rc = DARRegWrite(dev_info, TSI721_EM_DEV_INT_EN, em_dev_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x30);
		goto fail;
	}
	rc = DARRegWrite(dev_info, TSI721_EM_DEV_PW_EN, em_dev_pw_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x34);
		goto fail;
	}

get_dev_rpt_ctl:
	rc = DARRegRead(dev_info, TSI721_PLM_ALL_INT_EN, &plm_all_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x40);
		goto fail;
	}
	rc = DARRegRead(dev_info, TSI721_PLM_ALL_PW_EN, &plm_all_pw_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x44);
		goto fail;
	}
	rc = DARRegRead(dev_info, TSI721_EM_INT_ENABLE, &em_int_enable);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x48);
		goto fail;
	}
	rc = DARRegRead(dev_info, TSI721_EM_PW_ENABLE, &em_pw_enable);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x4C);
		goto fail;
	}
	rc = DARRegRead(dev_info, TSI721_EM_DEV_INT_EN, &em_dev_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x50);
		goto fail;
	}
	rc = DARRegRead(dev_info, TSI721_EM_DEV_PW_EN, &em_dev_pw_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_DEV_RPT_CTL(0x54);
		goto fail;
	}

	em_int_enable &= TSI721_EM_INT_ENABLE_LOCALOG;
	em_pw_enable &= TSI721_EM_PW_ENABLE_LOCALOG;

	out_parms->notfn = rio_em_notfn_none;
	if (em_int_enable && em_dev_int_en && plm_all_int_en) {
		if (em_pw_enable && em_dev_pw_en && plm_all_pw_en) {
			out_parms->notfn = rio_em_notfn_both;
		} else {
			out_parms->notfn = rio_em_notfn_int;
		}
	} else {
		if (em_pw_enable && em_dev_pw_en && plm_all_pw_en) {
			out_parms->notfn = rio_em_notfn_pw;
		}
	}
	rc = RIO_SUCCESS;
fail:
	return rc;
}

#define SAFE_ADD_EVENT_N_LOC(event_in) \
	if (out_parms->num_events < in_parms->num_events) { \
		in_parms->events[out_parms->num_events++].event = event_in; \
		in_parms->events[out_parms->num_events++].port_num = 0; \
	} else { \
		out_parms->too_many = true; \
	}

uint32_t idt_tsi721_em_parse_pw (DAR_DEV_INFO_t *dev_info, 
                                 rio_em_parse_pw_in_t *in_parms, 
                                 rio_em_parse_pw_out_t *out_parms) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	const int IMP_SPEC_IDX = RIO_EMHS_PW_IMP_SPEC_IDX;
	const int ERR_DET_IDX = RIO_EMHS_PW_P_ERR_DET_IDX;

	out_parms->imp_rc = 0;
	out_parms->num_events = 0;
	out_parms->too_many = false;
	out_parms->other_events = false;

	if (!(in_parms->num_events) || (NULL == in_parms->events) ||
							(NULL == dev_info)) {
		out_parms->imp_rc = EM_PARSE_PW(0x01);
		goto fail;
	}

	if (in_parms->pw[IMP_SPEC_IDX] & RIO_EM_PW_IMP_SPEC_PORT_MASK) {
		out_parms->imp_rc = EM_PARSE_PW(0x02);
		rc = RIO_ERR_NO_PORT_AVAIL;
		goto fail;
	}

	if (in_parms->pw[IMP_SPEC_IDX] & TSI721_PW_DLT) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_los);
	}

	if (in_parms->pw[IMP_SPEC_IDX] & TSI721_PW_PORT_ERR) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_port_err);
	}

	if (in_parms->pw[IMP_SPEC_IDX] & TSI721_PW_MAX_DENIAL) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_2many_retx);
	}

	if (in_parms->pw[IMP_SPEC_IDX] & TSI721_PW_OUTPUT_FAIL) {
		if (in_parms->pw[ERR_DET_IDX] & TSI721_SP_ERR_DET_CS_NOT_ACC) {
			SAFE_ADD_EVENT_N_LOC(rio_em_f_2many_pna);
		}
		SAFE_ADD_EVENT_N_LOC(rio_em_f_err_rate);
	}

	// Note: Tsi721 does not support rio_em_d_ttl or rio_em_d_rte events.

	if (in_parms->pw[RIO_EM_PW_L_ERR_DET_IDX] || 
			(in_parms->pw[IMP_SPEC_IDX] & TSI721_PW_LOCALOG)) {
		SAFE_ADD_EVENT_N_LOC(rio_em_d_log);
	}

	if (in_parms->pw[IMP_SPEC_IDX] & TSI721_PW_LINK_INIT) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_sig_det);
	}

	if (in_parms->pw[IMP_SPEC_IDX] & (TSI721_PW_RST_REQ | TSI721_PW_RCS)) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_rst_req);
	}

	// Note: No way to learn of a rio_em_i_init_fail event from a Tsi721
	// port-write

	if ((in_parms->pw[IMP_SPEC_IDX] &
		(TSI721_PW_TLM_PW | TSI721_PW_PBM_PW | TSI721_PW_OUTPUT_DEGR))
		|| (in_parms->pw[ERR_DET_IDX] &
					TSI721_ERR_RATE_EVENT_EXCLUSIONS)) {
		out_parms->other_events = true;
	}
	rc = RIO_SUCCESS;
fail:
	return rc;
}

uint32_t idt_tsi721_em_get_int_stat  ( DAR_DEV_INFO_t             *dev_info, 
                                     rio_em_get_int_stat_in_t   *in_parms, 
                                     rio_em_get_int_stat_out_t  *out_parms ) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	struct DAR_ptl good_ptl;
	uint32_t plm_ints;
	uint32_t plm_int_en, plm_int_stat;
	uint32_t em_ints;
	uint32_t em_int_en, em_int_stat;
	uint32_t i2c_ints;
	uint32_t i2c_int_en, i2c_int_stat;

	if (!(in_parms->num_events) || (NULL == in_parms->events)) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x01);
		goto fail;
	}

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x02);
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_PLM_STATUS, &plm_ints);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x10);
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_PLM_INT_ENABLE, &plm_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x11);
		goto fail;
	}

	plm_int_stat = plm_ints & plm_int_en;
	if (plm_ints & ~plm_int_en) {
		out_parms->other_events = true;
	}
	if (plm_int_stat & TSI721_PLM_STATUS_DLT) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_los);
	}
	if (plm_int_stat & TSI721_PLM_STATUS_PORT_ERR) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_port_err);
	}
	if (plm_int_stat & TSI721_PLM_STATUS_MAX_DENIAL) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_2many_retx);
	}
	if (plm_int_stat & TSI721_PLM_STATUS_OUTPUT_FAIL) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_err_rate);
	}

	rc = DARRegRead(dev_info, TSI721_EM_INT_STAT, &em_ints);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x20);
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_EM_INT_ENABLE, &em_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x21);
		goto fail;
	}

	em_int_stat = em_ints & em_int_en;
	if (em_int_stat & ~em_int_en) {
		out_parms->other_events = true;
	}

	if (em_int_stat & TSI721_EM_INT_STAT_LOCALOG) {
		SAFE_ADD_EVENT_N_LOC(rio_em_d_log);
	}

	if (plm_int_stat & TSI721_PLM_STATUS_LINK_INIT) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_sig_det);
	}

	if (em_int_stat & TSI721_EM_INT_STAT_RCS) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_rst_req);
	}

	rc = DARRegRead(dev_info, TSI721_I2C_INT_STAT, &i2c_ints);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x30);
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_I2C_INT_ENABLE, &i2c_int_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_INT_STAT(0x31);
		goto fail;
	}

	i2c_int_stat = i2c_ints & i2c_int_en;

	if (i2c_int_stat & TSI721_I2C_INT_ENABLE_BL_FAIL) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_init_fail);
	}
fail:
	return rc;
};

uint32_t idt_tsi721_em_get_pw_stat  ( DAR_DEV_INFO_t            *dev_info, 
                                    rio_em_get_pw_stat_in_t   *in_parms, 
                                    rio_em_get_pw_stat_out_t  *out_parms )
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	struct DAR_ptl good_ptl;
	uint32_t plm_pws;
	uint32_t plm_pw_en, plm_pw_stat;
	uint32_t em_pws;
	uint32_t em_pw_en, em_pw_stat;

	if (!(in_parms->num_events) || (NULL == in_parms->events)) {
		out_parms->imp_rc = EM_GET_PW_STAT(0x01);
		goto fail;
	}

	rc = DARrioGetPortList(dev_info, &in_parms->ptl, &good_ptl);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_PW_STAT(0x02);
		goto fail;
	}
	
	rc = DARRegRead(dev_info, TSI721_PLM_STATUS, &plm_pws);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_PW_STAT(0x10);
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_PLM_PW_ENABLE, &plm_pw_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_PW_STAT(0x11);
		goto fail;
	}

	plm_pw_stat = plm_pws & plm_pw_en;
	if (plm_pws & ~plm_pw_en) {
		out_parms->other_events = true;
	}
	if (plm_pw_stat & TSI721_PLM_STATUS_DLT) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_los);
	}
	if (plm_pw_stat & TSI721_PLM_STATUS_PORT_ERR) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_port_err);
	}
	if (plm_pw_stat & TSI721_PLM_STATUS_MAX_DENIAL) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_2many_retx);
	}
	if (plm_pw_stat & TSI721_PLM_STATUS_OUTPUT_FAIL) {
		SAFE_ADD_EVENT_N_LOC(rio_em_f_err_rate);
	}

	rc = DARRegRead(dev_info, TSI721_EM_PW_STAT, &em_pws);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_PW_STAT(0x20);
		goto fail;
	}

	rc = DARRegRead(dev_info, TSI721_EM_PW_ENABLE, &em_pw_en);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_GET_PW_STAT(0x21);
		goto fail;
	}

	em_pw_stat = em_pws & em_pw_en;
	if (em_pw_stat & ~em_pw_en) {
		out_parms->other_events = true;
	}

	if (em_pw_stat & TSI721_EM_PW_STAT_LOCALOG) {
		SAFE_ADD_EVENT_N_LOC(rio_em_d_log);
	}

	if (plm_pw_stat & TSI721_PLM_STATUS_LINK_INIT) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_sig_det);
	}

	if (em_pw_stat & TSI721_EM_PW_STAT_RCS) {
		SAFE_ADD_EVENT_N_LOC(rio_em_i_rst_req);
	}

	// Note: It is not possible to generate a port-write for an I2C event.

	// Clear port-write pending status after all events
	// that contribute to the port-write have been cleared.
	if (out_parms->num_events) {
		SAFE_ADD_EVENT_N_LOC(rio_em_a_clr_pwpnd);
	}
fail:
	return rc;
};

// To cleanup DLT, PORT_ERR, 2MANY_PNA, PORT_FAIL, PORT_DIS, PORT_LOCKOUT,
// need to:
// - Lockout the port to drop all outstanding packets and 
// - Place port in soft reset
// - Clear the bit(s) that disabled the port
// - Remove port from soft reset
//
typedef struct soft_rst_saved_regs_t_TAG {
	uint32_t sp_ctl;
	uint32_t devctl;
	uint32_t plm_ctl;
} soft_rst_saved_regs_t;

uint32_t tsi721_em_soft_rst_prep(DAR_DEV_INFO_t *dev_info, 
				rio_em_clr_events_out_t *out_parms,
				soft_rst_saved_regs_t *regs) 
{

	uint32_t rc = RIO_SUCCESS;

	rc = DARRegRead(dev_info, TSI721_SP_CTL, &regs->sp_ctl);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0x10);
		 goto exit;
	};

	rc = DARRegWrite(dev_info, TSI721_SP_CTL,
				regs->sp_ctl | TSI721_SP_CTL_PORT_LOCKOUT);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0x11);
		 goto exit;
	};

	rc = DARRegRead(dev_info, TSI721_DEVCTL, &regs->devctl);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0x12);
		 goto exit;
	};

	rc = DARRegWrite(dev_info, TSI721_DEVCTL,
			(regs->devctl & ~TSI721_DEVCTL_SR_RST_MODE) |
			TSI721_DEVCTL_SR_RST_MODE_SRIO_ONLY);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0x13);
		 goto exit;
	};

	rc = DARRegRead(dev_info, TSI721_PLM_IMP_SPEC_CTL, &regs->plm_ctl);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0x14);
		 goto exit;
	};

	rc = DARRegWrite(dev_info, TSI721_PLM_IMP_SPEC_CTL, 
		regs->plm_ctl | TSI721_PLM_IMP_SPEC_CTL_SOFT_RST);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0x15);
		 goto exit;
	};

exit:
	return rc;
}

uint32_t tsi721_em_soft_rst_reg_restore(DAR_DEV_INFO_t *dev_info, 
					rio_em_clr_events_out_t *out_parms,
					soft_rst_saved_regs_t *regs) 
{

	uint32_t rc = RIO_SUCCESS;

	rc = DARRegWrite(dev_info, TSI721_PLM_IMP_SPEC_CTL, regs->plm_ctl);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0xF0);
		 goto exit;
	};

	rc = DARRegWrite(dev_info, TSI721_DEVCTL, regs->devctl);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0xF1);
		 goto exit;
	};

	rc = DARRegWrite(dev_info, TSI721_SP_CTL, regs->sp_ctl);
	if (RIO_SUCCESS != rc) {
		 out_parms->imp_rc = EM_CLR_EVENTS(0xF2);
		 goto exit;
	};

exit:
	return rc;
}

uint32_t idt_tsi721_em_clr_events_need_soft_rst(
					rio_em_clr_events_in_t *in_parms,
					rio_em_clr_events_out_t *out_parms,
					bool *need_soft_rst)
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	unsigned int i;

	*need_soft_rst = false;
	// Do any events require a soft reset to clear them?  Check!
	// Also validate port numbers and events while we're at it.
	for (i = 0; i < in_parms->num_events; i++) {
		if (in_parms->events[i].port_num) {
			out_parms->failure_idx = i;
			goto fail;
		}

		switch (in_parms->events[i].event) {
		case rio_em_f_los:
		case rio_em_f_port_err:
		case rio_em_f_2many_pna:
		case rio_em_f_err_rate:
				*need_soft_rst = true;
				break;
		case rio_em_f_2many_retx:
		case rio_em_d_ttl:
		case rio_em_d_rte:
		case rio_em_d_log:
		case rio_em_i_sig_det:
		case rio_em_i_rst_req:
		case rio_em_i_init_fail:
		case rio_em_a_clr_pwpnd:
		case rio_em_a_no_event:
				break;
		default:
			out_parms->failure_idx = i;
			goto fail;
		}
	}
	rc = RIO_SUCCESS;
fail:
	return rc;
}

uint32_t idt_tsi721_em_clr_events_soft_rst(DAR_DEV_INFO_t *dev_info, 
                                    rio_em_clr_events_in_t   *in_parms, 
                                    rio_em_clr_events_out_t  *out_parms)
{
	uint32_t rc;
	soft_rst_saved_regs_t regs;
	uint32_t temp;
	unsigned int i;

	rc = tsi721_em_soft_rst_prep(dev_info, out_parms, &regs);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}

	for (i = 0; i < in_parms->num_events; i++) {
		out_parms->failure_idx = i;
		switch (in_parms->events[i].event) {
		case rio_em_f_los:
			rc = DARRegWrite(dev_info, TSI721_PLM_STATUS,
					TSI721_PLM_STATUS_DLT);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x20);
				goto fail;
			}
			break;
		case rio_em_f_port_err:
			rc = DARRegWrite(dev_info, TSI721_PLM_STATUS_PORT_ERR,
					TSI721_PLM_STATUS_DLT);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x22);
				goto fail;
			}
			break;
		case rio_em_f_2many_pna:
		case rio_em_f_err_rate:
			rc = DARRegRead(dev_info, TSI721_SP_ERR_RATE, &temp);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x24);
				goto fail;
			}
			temp &= ~TSI721_SP_ERR_RATE_ERR_RATE_CNT;
			rc = DARRegWrite(dev_info, TSI721_SP_ERR_RATE, temp);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x26);
				goto fail;
			}

			rc = DARRegWrite(dev_info, TSI721_SP_ERR_DET, 0);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x28);
				goto fail;
			}

			rc = DARRegWrite(dev_info, TSI721_SP_ERR_ATTR_CAPT, 0);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x2A);
				goto fail;
			}

			rc = DARRegWrite(dev_info, TSI721_SP_ERR_STAT,
					TSI721_SP_ERR_STAT_OUTPUT_ERR_ENCTR |
					TSI721_SP_ERR_STAT_INPUT_ERR_ENCTR);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x2C);
				goto fail;
			}

			rc = DARRegWrite(dev_info, TSI721_PLM_STATUS,
					TSI721_PLM_STATUS_OUTPUT_FAIL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x2E);
				goto fail;
			}
			break;
		default: continue;
		}
	}

	out_parms->failure_idx = 0;
	rc = tsi721_em_soft_rst_reg_restore(dev_info, out_parms, &regs);
	if (RIO_SUCCESS != rc) {
		goto fail;
	}
	rc = RIO_SUCCESS;
fail:
	return rc;
}

uint32_t idt_tsi721_em_clr_events(DAR_DEV_INFO_t *dev_info, 
				rio_em_clr_events_in_t *in_parms, 
				rio_em_clr_events_out_t *out_parms) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	bool need_soft_rst;
	unsigned int i;
	uint32_t temp;

	if (!(in_parms->num_events) || (NULL == in_parms->events)) {
		out_parms->imp_rc = EM_CLR_EVENTS(0x01);
		goto fail;
	}

	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->failure_idx = 0;
	out_parms->pw_events_remain = false;
	out_parms->int_events_remain = false;

	rc = idt_tsi721_em_clr_events_need_soft_rst(in_parms, out_parms,
								&need_soft_rst);
	if (RIO_SUCCESS != rc) {
		out_parms->imp_rc = EM_CLR_EVENTS(0x03);
		goto fail;
	}

	if (need_soft_rst) {
		rc = idt_tsi721_em_clr_events_soft_rst(dev_info, in_parms,
								out_parms); 
		if (RIO_SUCCESS != rc) {
			goto fail;
		}
	}

	for (i = 0; i < in_parms->num_events; i++) {
		if (in_parms->events[i].port_num) {
			out_parms->failure_idx = i;
			rc = RIO_ERR_INVALID_PARAMETER;
			goto fail;
		}

		switch (in_parms->events[i].event) {
		case rio_em_f_2many_retx:
			rc = DARRegWrite(dev_info, TSI721_PLM_STATUS,
					TSI721_PLM_STATUS_MAX_DENIAL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x40);
				goto fail;
			}
			break;
		case rio_em_d_log:
			rc = DARRegWrite(dev_info, TSI721_LOCAL_ERR_DET, 0);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x48);
				goto fail;
			}
			break;
			
		case rio_em_i_sig_det:
			rc = DARRegWrite(dev_info, TSI721_PLM_STATUS,
						TSI721_PLM_STATUS_LINK_INIT);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x50);
				goto fail;
			}
			break;
		case rio_em_i_rst_req:
			rc = DARRegWrite(dev_info, TSI721_PLM_STATUS,
						TSI721_PLM_STATUS_RST_REQ);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x58);
				goto fail;
			}

			rc = DARRegWrite(dev_info, TSI721_EM_RST_PORT_STAT,
					TSI721_EM_RST_PORT_STAT_RST_REQ);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x59);
				goto fail;
			}
			break;
		case rio_em_i_init_fail:
			rc = DARRegWrite(dev_info, TSI721_I2C_INT_STAT,
					TSI721_I2C_INT_STAT_BL_FAIL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x60);
				goto fail;
			}
			break;
		case rio_em_a_clr_pwpnd:
			rc = DARRegRead(dev_info, TSI721_SP_ERR_STAT, &temp);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x68);
				goto fail;
			}
			temp |= TSI721_SP_ERR_STAT_PORT_W_P;
			rc = DARRegWrite(dev_info, TSI721_SP_ERR_STAT, temp);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CLR_EVENTS(0x70);
				goto fail;
			}
			break;

		case rio_em_d_ttl: // Event not possible
		case rio_em_d_rte: // Event not possible
		case rio_em_a_no_event: // No event, nothing to do
			break;

		case rio_em_f_los: // Already handled by
		case rio_em_f_port_err: // idt_tsi721_em_clr_events_soft_rst
		case rio_em_f_2many_pna:
		case rio_em_f_err_rate:
		default: // Bad event values are detected by
			// idt_tsi721_em_clr_events_need_soft_rst
			break;
		}
	}

	rc = RIO_SUCCESS;
fail:
	return rc;
};

uint32_t idt_tsi721_em_create_events( DAR_DEV_INFO_t              *dev_info, 
                                    rio_em_create_events_in_t   *in_parms, 
                                    rio_em_create_events_out_t  *out_parms ) 
{
	uint32_t rc = RIO_ERR_INVALID_PARAMETER;
	unsigned int i;

	if (!(in_parms->num_events) || (NULL == in_parms->events)) {
		out_parms->imp_rc = EM_CREATE_EVENTS(0x01);
		goto fail;
	}

	out_parms->imp_rc = RIO_SUCCESS;
	out_parms->failure_idx = 0;

	for (i = 0; i < in_parms->num_events; i++) {
		if (in_parms->events[i].port_num) {
			out_parms->imp_rc = EM_CREATE_EVENTS(0x02);
			out_parms->failure_idx = i;
			rc = RIO_ERR_INVALID_PARAMETER;
			goto fail;
		}

		switch (in_parms->events[i].event) {
		case rio_em_f_los:
			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_DLT);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x10);
				goto fail;
			}
			break;
			
		case rio_em_f_port_err:
			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_PORT_ERR);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x18);
				goto fail;
			}
			break;

		case rio_em_f_2many_pna:
			rc = DARRegWrite(dev_info, TSI721_SP_ERR_DET,
				TSI721_SP_ERR_DET_CS_NOT_ACC);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x20);
				goto fail;
			}

			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_OUTPUT_FAIL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x28);
				goto fail;
			}
			break;

		case rio_em_f_err_rate:
			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_OUTPUT_FAIL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x30);
				goto fail;
			}
			break;

		case rio_em_f_2many_retx:
			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_MAX_DENIAL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x38);
				goto fail;
			}
			break;

		case rio_em_d_ttl: // Nothing to do.
		case rio_em_d_rte: // Nothing to do.
			break;
		case rio_em_d_log:
			rc = DARRegWrite(dev_info, TSI721_LOCAL_ERR_DET,
				TSI721_LOCAL_ERR_DET_ILL_TYPE |
				TSI721_LOCAL_ERR_DET_ILL_ID);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x40);
				goto fail;
			}
			break;

		case rio_em_i_sig_det:
			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_LINK_INIT);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x48);
				goto fail;
			}
			break;

		case rio_em_i_rst_req:
			rc = DARRegWrite(dev_info, TSI721_PLM_EVENT_GEN,
				TSI721_PLM_EVENT_GEN_RST_REQ);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x50);
				goto fail;
			}
			break;

		case rio_em_i_init_fail:
			rc = DARRegWrite(dev_info, TSI721_I2C_INT_SET,
				TSI721_I2C_INT_SET_BL_FAIL);
			if (RIO_SUCCESS != rc) {
				out_parms->imp_rc = EM_CREATE_EVENTS(0x58);
				goto fail;
			}
			break;

		case rio_em_a_clr_pwpnd:
		case rio_em_a_no_event:
				break;
		default:
			out_parms->failure_idx = i;
			rc = RIO_ERR_INVALID_PARAMETER;
			goto fail;
		}
	}
	rc = RIO_SUCCESS;
fail:
	return rc;
};

#ifdef __cplusplus
}
#endif
