/* Implementation for libriocp_pe register read/write driver  and         */
/* libriocp_pe PE driver based on librio_switch and libmport.              */
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
// #include <cinttypes>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>


#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "liblog.h"
#include "pe_mpdrv_private.h"
#include "riocp_pe_internal.h"
#include "comptag.h"
#include "IDT_Statistics_Counter_API.h"

#ifdef __cplusplus
extern "C" {
#endif

int mpsw_drv_reg_rd(struct riocp_pe  *pe, uint32_t offset, uint32_t *val)
{
	int ret;
	struct mpsw_drv_private_data *priv_ptr;

	DBG("ENTRY: offset 0x%x val_ptr 0x%lx\n", offset, val);

	ret = riocp_pe_handle_get_private(pe, (void **)&priv_ptr);
	if (ret) {
		ERR("Could not get private data: rc %d:%s\n",
							ret, strerror(ret));
		return ret;
	};

	ret = DARRegRead(&priv_ptr->dev_h, offset, val);
	if (ret) {
		ERR("Read Failed: offset 0x%x rc 0x%x\n", offset, ret);
		ret = EIO;
	};

	DBG("EXIT\n");
	return ret;
};

int mpsw_drv_reg_wr(struct riocp_pe  *pe, uint32_t offset, uint32_t val)
{
	int ret;
	struct mpsw_drv_private_data *priv_ptr;
	

	DBG("ENTRY: offset 0x%x val 0x%x\n", offset, val);

	ret = riocp_pe_handle_get_private(pe, (void **)&priv_ptr);
	if (ret) {
		ERR("Could not get private data: rc %d:%s\n",
							-ret, strerror(-ret));
		return -ret;
	};

	if (RIO_BASE_DEVICE_ID_CSR == offset) {
        	if (RIOCP_PE_IS_MPORT(pe)) {
			struct mpsw_drv_pe_acc_info *p_acc;
			p_acc = (struct mpsw_drv_pe_acc_info *)
					priv_ptr->dev_h.accessInfo;
                	ret = riomp_mgmt_destid_set(p_acc->maint, val);
		};
	};

	ret = DARRegWrite(&priv_ptr->dev_h, offset, val);
	if (ret) {
		ERR("Write Failed: offset 0x%x rc 0x%x\n", offset, ret);
		ret = EIO;
	};

	DBG("EXIT\n");
	return 0;
};

int mpsw_drv_raw_reg_wr(struct riocp_pe *pe, uint32_t did, uint8_t hc,
				uint32_t addr, uint32_t val)
{
	int rc;
	struct mpsw_drv_pe_acc_info *p_acc;

	if (!RIOCP_PE_IS_HOST(pe))
		return -ENOSYS;

	p_acc = (struct mpsw_drv_pe_acc_info *)pe->mport->minfo->private_data;

	if (!p_acc->maint_valid)
		return -EINVAL;

	// if (RIOCP_PE_IS_MPORT(pe) && (did == pe->destid))
	if (RIOCP_PE_IS_MPORT(pe))
                rc = riomp_mgmt_lcfg_write(p_acc->maint, addr, 4, val);
        else
                rc = riomp_mgmt_rcfg_write(p_acc->maint, did, hc, addr, 4, val);
	return rc;
};

int mpsw_drv_raw_reg_rd(struct riocp_pe *pe, uint32_t did, uint8_t hc,
				uint32_t addr, uint32_t *val)
{
	int rc;
	struct mpsw_drv_pe_acc_info *p_acc;

	p_acc = (struct mpsw_drv_pe_acc_info *)
			pe->mport->minfo->private_data;

	if (!p_acc->maint_valid)
		return -EINVAL;

	// if (RIOCP_PE_IS_MPORT(pe) && (did == pe->destid))
	if (RIOCP_PE_IS_MPORT(pe)) 
                rc = riomp_mgmt_lcfg_read(p_acc->maint, addr, 4, val);
        else
                rc = riomp_mgmt_rcfg_read(p_acc->maint, did, hc, addr, 4, val);
	return rc;
};

#define MPSW_MAX_DEV_FN 255
#define MPSW_DFLT_DEV_DIR "/sys/bus/rapidio/devices/"

int mpsw_mport_dev_add(struct riocp_pe *pe, char *name)
{
	int rc = 0;
	struct mpsw_drv_pe_acc_info *p_acc;
	char dev_fn[MPSW_MAX_DEV_FN];

	p_acc = (struct mpsw_drv_pe_acc_info *)
			pe->mport->minfo->private_data;

	if ((!p_acc->maint_valid) || (NULL == name))
		return -EINVAL;

	pe->name = (const char *)name;

	if (RIOCP_PE_IS_MPORT(pe))
		return 0;

	if (!RIOCP_PE_IS_HOST(pe))
		return -ENOSYS;

	memset(dev_fn, 0, MPSW_MAX_DEV_FN);
	snprintf(dev_fn, MPSW_MAX_DEV_FN-1, "%s%s", MPSW_DFLT_DEV_DIR, name);

	if (access(dev_fn, F_OK) != -1) {
		INFO("\nFMD: device \"%s\" exists...\n", name);
	} else {
		rc = riomp_mgmt_device_add(p_acc->maint, pe->destid,
			pe->hopcount, pe->comptag, pe->name);
		if (rc) {
			CRIT("riomp_mgmt_device_add, rc %d errno %d\n",
				rc, errno);
		};
	};

	return rc;
};

int mpsw_destroy_priv_data(struct riocp_pe *pe);

int mpsw_alloc_priv_data(struct riocp_pe *pe, void **p_dat,
			struct riocp_pe *peer)
{
	struct mpsw_drv_private_data *priv_ptr;
	int ret = 1;

	DBG("ENTRY\n");

	*p_dat = malloc(sizeof(struct mpsw_drv_private_data));
	if (NULL == *p_dat) {
		CRIT("Unable to allocate mpsw_drv_private_data\n");
		ret = -ENOMEM;
		goto err;
	};

	pe->private_data = *p_dat;
	priv_ptr = (struct mpsw_drv_private_data *)*p_dat;

	priv_ptr->is_mport = RIOCP_PE_IS_MPORT(pe)?1:0;
	priv_ptr->dev_h_valid = 0;
	priv_ptr->dev_h.privateData = (void *)pe;
	priv_ptr->dev_h.accessInfo  = NULL;
	
	if (priv_ptr->is_mport) {
		struct mpsw_drv_pe_acc_info *acc_p;

		priv_ptr->dev_h.accessInfo =
			malloc(sizeof(struct mpsw_drv_pe_acc_info));
		if (NULL == priv_ptr->dev_h.accessInfo) {
			CRIT("Unable to allocate mpsw_drv_pe_acc_info\n");
			ret = ENOMEM;
			goto err;
		};
	
		acc_p = (struct mpsw_drv_pe_acc_info *)priv_ptr->dev_h.accessInfo;
		acc_p->local = 1;
		acc_p->maint_valid = 0;

        	ret = riomp_mgmt_mport_create_handle(pe->minfo->id, 0, &acc_p->maint);
        	if (ret) {
			CRIT("Unable to open mport %d %d:%s\n", pe->minfo->id,
				errno, strerror(errno));
                	goto exit;
		};
		acc_p->maint_valid = 1;
		DBG("Successfully openned mport did %d ct %x\n",
			pe->destid, pe->comptag);

        	ret = riomp_mgmt_query(acc_p->maint, &acc_p->props);
        	if (ret < 0) {
			CRIT("Unable to query mport %d properties %d:%s\n",
				 pe->mport, errno, strerror(errno));
			goto err;
		};
		pe->mport->minfo->private_data = (void *)acc_p;
		ret = 0;
		goto exit;
	};

	if (NULL == peer)
		goto err;

	pe->mport = peer->mport;

	ret = 0;
	goto exit;
err: 
	mpsw_destroy_priv_data(pe);
exit:
	DBG("EXIT\n");
	return ret;
};

int mpsw_destroy_priv_data(struct riocp_pe *pe)
{
	struct mpsw_drv_private_data *priv_ptr;

	if (NULL == pe->private_data)
		return 0;
		
	priv_ptr = (struct mpsw_drv_private_data *)pe->private_data;
	if (NULL != priv_ptr->dev_h.accessInfo) {
		struct mpsw_drv_pe_acc_info *acc_p;

		acc_p = (struct mpsw_drv_pe_acc_info *)
					(priv_ptr->dev_h.accessInfo);
		if (acc_p->maint_valid) {
        		if (riomp_mgmt_mport_destroy_handle(&acc_p->maint)) 
				CRIT("Unable to close mport %d %d:%s\n",
					pe->mport, errno, strerror(errno));
		};
		free(priv_ptr->dev_h.accessInfo);
	};
	free(pe->private_data);

	return 0;
};

int generic_device_init(struct riocp_pe *pe, uint32_t *ct)
{
	struct mpsw_drv_private_data *priv;
	DAR_DEV_INFO_t *dev_h;
        struct DAR_ptl ptl;
        idt_pc_dev_reset_config_in_t    rst_in = {idt_pc_rst_port};
        idt_pc_dev_reset_config_out_t   rst_out;
        idt_pc_get_status_in_t          ps_in;
        idt_pc_get_config_in_t          pc_in;
        idt_rt_probe_all_in_t           rt_in;
        idt_rt_probe_all_out_t          rt_out;
	/* FIXME: Commented out temporarily to avoid build error. */
        // idt_sc_init_dev_ctrs_in_t       sc_in;
        // idt_sc_init_dev_ctrs_out_t      sc_out;
        idt_em_dev_rpt_ctl_in_t         rpt_in;
	int rc = 1;

	DBG("ENTRY\n");
	priv = (struct mpsw_drv_private_data *)(pe->private_data);

	if (NULL == priv) {
		ERR("Private Data is NULL, exiting\n");
		goto exit;
	};

	pe->name = (const char *)priv->dev_h.name;
	dev_h = &priv->dev_h;

	if (RIOCP_PE_IS_HOST(pe)) {
		/* ensure destID/comptag is configured correctly */
		/* FIXME: Assumes comptag and destID is passed in, not
 		* administered by riocp_pe.
 		*/
		uint32_t did = RIOCP_PE_COMPTAG_DESTID(*ct);
		rc = riocp_pe_update_comptag(pe, ct, did, 1);
		if (rc) {
			ERR("Could not update comptag\n");
			goto exit;
		};

		ptl.num_ports = RIO_ALL_PORTS;
                rc = DARrioPortEnable(dev_h, &ptl, TRUE, FALSE, TRUE);
                if (RIO_SUCCESS != rc)
                        goto exit;
		rc = DARrioSetEnumBound(dev_h, &ptl, 0);
		if (rc) {
			ERR("Could not clear enumeration indication\n");
			goto exit;
		};
        };

        /* Query port configuration and status */
        pc_in.ptl.num_ports = RIO_ALL_PORTS;
        rc = idt_pc_get_config(dev_h, &pc_in, &priv->st.pc);
        if (RIO_SUCCESS != rc)
                goto exit;

        ps_in.ptl.num_ports = RIO_ALL_PORTS;
        rc = idt_pc_get_status(dev_h, &ps_in, &priv->st.ps);
        if (RIO_SUCCESS != rc)
                goto exit;

        if (SWITCH((&priv->dev_h))) {
                UINT8 port;
                rt_in.probe_on_port = RIO_ALL_PORTS;
                rt_in.rt            = &priv->st.g_rt;
                rc = idt_rt_probe_all(dev_h, &rt_in, &rt_out);
                if (RIO_SUCCESS != rc)
                        goto exit;
                for (port = 0; port < NUM_PORTS((&priv->dev_h)); port++) {
                        rt_in.probe_on_port = port;
                        rt_in.rt            = &priv->st.pprt[port];
                        rc = idt_rt_probe_all(dev_h, &rt_in, &rt_out);
                        if (RIO_SUCCESS != rc)
                                goto exit;
                };
        };

        /* Initialize performance counter structure */
	/* FIXME: Commented out temporarily to avoid build error. */
	/*
        sc_in.ptl.num_ports = RIO_ALL_PORTS;
        sc_in.dev_ctrs = &priv->st.sc_dev;
        priv->st.sc_dev.num_p_ctrs   = IDT_MAX_PORTS;
        priv->st.sc_dev.valid_p_ctrs = 0;
        priv->st.sc_dev.p_ctrs       = priv->st.sc;

        rc = idt_sc_init_dev_ctrs(dev_h, &sc_in, &sc_out);
        if (RIO_SUCCESS != rc)
                goto exit;
	*/

	if (!RIOCP_PE_IS_HOST(pe))
                goto exit;

        /* Set device reset handling to "per port" if possible */
        rc = idt_pc_dev_reset_config(dev_h, &rst_in, &rst_out);
        if (RIO_SUCCESS != rc)
                goto exit;

        rc = idt_em_cfg_pw(dev_h, &priv->st.em_pw_cfg, &priv->st.em_pw_cfg);
        if (RIO_SUCCESS != rc)
                goto exit;

        rpt_in.ptl.num_ports = RIO_ALL_PORTS;
        rpt_in.notfn = idt_em_notfn_none;
        rc = idt_em_dev_rpt_ctl(dev_h, &rpt_in, &priv->st.em_notfn);
exit:
	return rc;
};
	
int mpsw_drv_reg_rd(struct riocp_pe  *pe, uint32_t offset, uint32_t *val);

int RIOCP_WU mpsw_drv_init_pe(struct riocp_pe *pe, uint32_t *ct, 
				struct riocp_pe *peer, char *name)
{
	struct mpsw_drv_private_data *p_dat;
	int ret = 1;
	uint32_t temp_devid;
	uint32_t gen_ctl;

	DBG("ENTRY\n");

	p_dat = (struct mpsw_drv_private_data *)pe->private_data;

	/* If private data has been allocated, device was found previously */
	if (NULL != p_dat) {
		DBG("Private Data exists!\n");
		goto exit;
	};

	ret = mpsw_alloc_priv_data(pe, (void **)&p_dat, peer);
	if (ret) {
		CRIT("Unable to allocate private data.\n");
		goto exit;
	};

	/* Select a driver for the device */
	ret = mpsw_drv_raw_reg_rd(pe, pe->destid, pe->hopcount, RIO_DEV_ID_CAR,
				&temp_devid);
	if (ret) {
		CRIT("Unable to read device ID %d:%s\n", ret, strerror(ret));
		goto error;
	};
	
	p_dat->dev_h.devID = temp_devid;
	ret = DAR_Find_Driver_for_Device(1, &p_dat->dev_h);
	if (RIO_SUCCESS != ret) {
		CRIT("Unable to find driver for device, type 0x%x\n, ret 0x%x",
			ret, p_dat->dev_h.devID);
		ret = errno = EOPNOTSUPP;
		goto error;
	};
	p_dat->dev_h_valid = 1;

	ret = generic_device_init(pe, ct);
	if (ret) {
		CRIT("Generic device init failed: %d (0x%x)\n", ret, ret);
		goto error;
	};

	ret = mpsw_mport_dev_add(pe, name);
	if (ret) {
		CRIT("Adding device to mport failed: %d (0x%x)\n", ret, ret);
		goto error;
	};

	if (!p_dat->dev_h.extFPtrForPort)
		goto exit;

	ret = mpsw_drv_reg_rd(pe, 
		RIO_PORT_GEN_CTRL_CSR(p_dat->dev_h.extFPtrForPort), &gen_ctl);
	if (ret) {
		CRIT("Adding device to mport failed: %d (0x%x)\n", ret, ret);
		goto error;
	};

	gen_ctl |= RIO_STD_SW_GEN_CTL_MAST_EN | RIO_STD_SW_GEN_CTL_DISC;

	ret = mpsw_drv_reg_wr(pe, 
		RIO_PORT_GEN_CTRL_CSR(p_dat->dev_h.extFPtrForPort), gen_ctl);
	if (ret) {
		CRIT("Adding device to mport failed: %d (0x%x)\n", ret, ret);
		goto error;
	};

	goto exit;
error: 
	mpsw_destroy_priv_data(pe);
exit:
	DBG("EXIT\n");
	return ret;
};

int RIOCP_WU mpsw_drv_destroy_pe(struct riocp_pe *pe)
{
	DBG("ENTRY\n");
	mpsw_destroy_priv_data(pe);
	DBG("EXIT\n");
	return 0;
};

int RIOCP_WU mpsw_drv_recover_port(struct riocp_pe *pe, uint8_t port)
{
	struct mpsw_drv_private_data *p_dat;
	int ret;
	idt_pc_clr_errs_in_t clr_errs_in;
	idt_pc_clr_errs_out_t clr_errs_out;

	DBG("ENTRY\n");

	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	clr_errs_in.port_num = port;
	clr_errs_in.clr_lp_port_err = 0;
	clr_errs_in.lp_dev_info = NULL;
	clr_errs_in.num_lp_ports = 0;

	ret = idt_pc_clr_errs(&p_dat->dev_h, &clr_errs_in, &clr_errs_out);
	if (ret) {
		DBG("Failed clearing %s Port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			clr_errs_out.imp_rc);
		goto fail;
	};

	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};


int mpsw_drv_get_route_entry(struct riocp_pe  *pe, uint8_t port,
					uint32_t did, pe_rt_val *rt_val)
{
	struct mpsw_drv_private_data *p_dat;
	int ret;
	idt_rt_probe_in_t probe_in;
	idt_rt_probe_out_t probe_out;

	DBG("ENTRY\n");

	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	probe_in.probe_on_port = port;
	probe_in.tt = tt_dev8;
	probe_in.destID = did;
	probe_in.rt = &p_dat->st.g_rt;
	
	ret = idt_rt_probe(&p_dat->dev_h, &probe_in, &probe_out);
	if (ret) {
		DBG("Failed probing %s port %d did %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, did, ret,
			probe_out.imp_rc);
		goto fail;
	};
	
	*rt_val = probe_out.routing_table_value;

	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_set_route_entry(struct riocp_pe  *pe,
                       uint8_t port, uint32_t did, pe_rt_val rt_val)
{
	struct mpsw_drv_private_data *p_dat;
	int ret;
	idt_rt_change_rte_in_t chg_in;
	idt_rt_change_rte_out_t chg_out;
	idt_rt_set_changed_in_t set_in;
	idt_rt_set_changed_out_t set_out;

	DBG("ENTRY\n");
	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	chg_in.dom_entry = 0;
	chg_in.idx       = did;
	chg_in.rte_value = rt_val;
	if (RIO_ALL_PORTS == port)
		chg_in.rt = &p_dat->st.g_rt;
	else
		chg_in.rt = &p_dat->st.pprt[port];
	
	ret = idt_rt_change_rte(&p_dat->dev_h, &chg_in, &chg_out);
	if (ret) {
		DBG("RT_CHG %s port %d did %d rte %x ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, did, rt_val, ret,
			chg_out.imp_rc);
		goto fail;
	};

	set_in.set_on_port = port;
	set_in.rt = chg_in.rt;

	ret = idt_rt_set_changed(&p_dat->dev_h, &set_in, &set_out);
	if (ret) {
		DBG("RT_SET %s port %d did %d rte %x ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, did, rt_val, ret,
			set_out.imp_rc);
		goto fail;
	};
	
	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_get_port_state(struct riocp_pe  *pe, uint8_t port,
			struct riocp_pe_port_state_t *state)
{
	struct mpsw_drv_private_data *p_dat;
	int ret;
	idt_pc_get_status_in_t st_in;

	DBG("ENTRY\n");
	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if ((NULL == state) || (port >= NUM_PORTS(&p_dat->dev_h))) {
		DBG("State or port illegal, EXITING!\n");
		goto fail;
	};

	st_in.ptl.num_ports = RIO_ALL_PORTS;
	ret = idt_pc_get_status(&p_dat->dev_h, &st_in, &p_dat->st.ps);
	if (ret) {
		DBG("PC_Status %s port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			p_dat->st.ps.imp_rc);
		goto fail;
	};
	
	state->port_ok = p_dat->st.ps.ps[port].port_ok;
	state->port_max_width = PW_TO_LANES(p_dat->st.pc.pc[port].pw);
	if (state->port_ok)
		state->port_cur_width = PW_TO_LANES(p_dat->st.ps.ps[port].pw);
	else
		state->port_cur_width = 0;
	switch (p_dat->st.pc.pc[port].ls) {
	case idt_pc_ls_1p25 : state->port_lane_speed = 1250;
	case idt_pc_ls_2p5  : state->port_lane_speed = 2000;
	case idt_pc_ls_3p125: state->port_lane_speed = 2500;
	case idt_pc_ls_5p0  : state->port_lane_speed = 4000;
	case idt_pc_ls_6p25 : state->port_lane_speed = 5000;
	default: state->port_lane_speed = 0;
	};
	state->link_errs = p_dat->st.ps.ps[port].port_error |
				p_dat->st.ps.ps[port].input_stopped |
				p_dat->st.ps.ps[port].output_stopped;

	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};


struct riocp_pe_driver pe_mpsw_driver = {
	mpsw_drv_init_pe,
	mpsw_drv_destroy_pe,
	mpsw_drv_recover_port,
	mpsw_drv_set_route_entry,
	mpsw_drv_get_route_entry,
	mpsw_drv_get_port_state
};
	
struct riocp_reg_rw_driver pe_mpsw_rw_driver = {
	mpsw_drv_reg_rd,
	mpsw_drv_reg_wr,
	mpsw_drv_raw_reg_rd,
	mpsw_drv_raw_reg_wr
};

#ifdef __cplusplus
}
#endif
