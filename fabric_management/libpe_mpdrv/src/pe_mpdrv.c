/* Implementation for libriocp_pe register read/write driver  and	 */
/* libriocp_pe PE driver based on librio_switch and libmport.	      */
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
#include "cfg.h"

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
	struct mpsw_drv_private_data *priv_ptr = NULL;

	DBG("ENTRY: offset 0x%x val 0x%x\n", offset, val);

	ret = riocp_pe_handle_get_private(pe, (void **)&priv_ptr);
	if (ret) {
		ERR("Could not get private data: rc %d:%s\n",
							-ret, strerror(-ret));
		return -ret;
	};

	if (RIO_DEVID == offset) {
		if (RIOCP_PE_IS_MPORT(pe)) {
			struct mpsw_drv_pe_acc_info *p_acc;
			uint16_t dev8_did = ((val >> 16) & 0xFF);
			p_acc = (struct mpsw_drv_pe_acc_info *)
					priv_ptr->dev_h.accessInfo;
			ret = riomp_mgmt_destid_set(p_acc->maint, dev8_did);
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
	struct mpsw_drv_pe_acc_info *p_acc = NULL;

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
	struct mpsw_drv_pe_acc_info *p_acc = NULL;

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
	struct mpsw_drv_pe_acc_info *p_acc = NULL;
	struct mpsw_drv_private_data *p_dat = NULL;
	char dev_fn[MPSW_MAX_DEV_FN] = {0};

	if (RIOCP_PE_IS_MPORT(pe))
		return 0;

	p_dat = (struct mpsw_drv_private_data *)pe->private_data;

	if (SWITCH(&p_dat->dev_h))
		return 0;

	p_acc = (struct mpsw_drv_pe_acc_info *)
			pe->mport->minfo->private_data;

	if ((!p_acc->maint_valid) || (NULL == name))
		return -EINVAL;

	pe->name = (const char *)name;

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
	struct mpsw_drv_private_data *priv_ptr = NULL;
	int ret = 1;

	DBG("ENTRY\n");

	*p_dat = calloc(1, sizeof(struct mpsw_drv_private_data));
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
			calloc(1, sizeof(struct mpsw_drv_pe_acc_info));
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
	struct mpsw_drv_private_data *priv_ptr = NULL;

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

int mpdrv_init_rt(uint32_t ct, DAR_DEV_INFO_t *dh)
{
        struct cfg_dev sw;
        idt_rt_set_all_in_t set_in;
        idt_rt_set_all_out_t set_out;
        pe_port_t port;
        int rc;

        if (cfg_find_dev_by_ct(ct, &sw)) {
                if (!sw.is_sw)
                        WARN("libriocp_pe libcfg conflict: is CT 0x%x"
                                " a switch?", ct);
        };

        if (NULL != sw.sw_info.rt[CFG_DEV08]) {
                set_in.set_on_port = RIO_ALL_PORTS;
                set_in.rt = sw.sw_info.rt[CFG_DEV08];

                rc = idt_rt_set_all(dh, &set_in, &set_out);
                if (RIO_SUCCESS != rc) {
                        ERR("Error programming global rt on ct 0x%x rc %d\n",
                                sw.ct, rc);
                        goto fail;
                };
        }

        for (port = 0; port < NUM_PORTS(dh); port++) {
                if (NULL == sw.sw_info.sw_pt[port].rt[CFG_DEV08])
                        continue;

                set_in.set_on_port = port;
                set_in.rt = sw.sw_info.sw_pt[port].rt[CFG_DEV08];

                rc = idt_rt_set_all(dh, &set_in, &set_out);
                if (RIO_SUCCESS != rc) {
                        ERR("Error programming port %d rt on ct 0x%x rc %d\n",
                                port, ct, rc);
                        goto fail;
                };
        }
        return 0;
fail:
        return 1;
};

int generic_device_init(struct riocp_pe *pe, uint32_t *ct)
{
	uint32_t port_info;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;
	struct DAR_ptl ptl;
	idt_pc_set_config_in_t set_pc_in;
	idt_pc_dev_reset_config_in_t    rst_in = {idt_pc_rst_port};
	idt_pc_dev_reset_config_out_t   rst_out;
	idt_pc_get_status_in_t	  ps_in;
	idt_pc_get_config_in_t	  pc_in;
	idt_rt_probe_all_in_t	   rt_in;
	idt_rt_probe_all_out_t	  rt_out;
	idt_sc_init_dev_ctrs_in_t       sc_in;
	idt_sc_init_dev_ctrs_out_t      sc_out;
	idt_em_dev_rpt_ctl_in_t	 rpt_in;
	int rc = 1;

	DBG("ENTRY\n");
	priv = (struct mpsw_drv_private_data *)(pe->private_data);

	if (NULL == priv) {
		ERR("Private Data is NULL, exiting\n");
		goto exit;
	};

	pe->name = (const char *)priv->dev_h.name;
	dev_h = &priv->dev_h;

	if (RIOCP_PE_IS_HOST(pe) || RIOCP_PE_IS_MPORT(pe)) {
		/* ensure destID/comptag/addrmode is configured correctly */
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
		rc = DARrioPortEnable(dev_h, &ptl, true, false, true);
		if (RIO_SUCCESS != rc)
			goto exit;
		rc = DARrioSetEnumBound(dev_h, &ptl, 0);
		if (rc) {
			ERR("Could not clear enumeration indication\n");
			goto exit;
		};
		if (RIOCP_PE_IS_MPORT(pe)) {
			uint8_t mem_sz;
			RIO_PE_ADDR_T addr_mode;
			if (!cfg_get_mp_mem_sz(pe->minfo->id, &mem_sz)) {
				switch (mem_sz) {
				case CFG_MEM_SZ_34: 
					addr_mode = RIO_PE_FEAT_EXT_ADDR34;
					break;
				case CFG_MEM_SZ_50:
					addr_mode = RIO_PE_FEAT_EXT_ADDR50;
					break;
				case CFG_MEM_SZ_66:
					addr_mode = RIO_PE_FEAT_EXT_ADDR66;
					break;
				default:
					rc = -1;
					ERR("CT 0x%0x Unknown mem_sz 0x%x\n",
						*ct, mem_sz);
					goto exit;
				};
				if (!(addr_mode & dev_h->features)) {
					rc = -1;
					ERR("CT 0x%0x Addr mode 0x%x Failed\n",
						addr_mode);
					goto exit;
				};
				rc = DARrioSetAddrMode(dev_h, addr_mode);
				if (RIO_SUCCESS != rc) {
					ERR("CT 0x%0x DARrioSetAddrMode rc %x\n",
						*ct, rc);
					goto exit;
				};
			};
		};
	};

	/* Query port configuration and status */
	pc_in.ptl.num_ports = RIO_ALL_PORTS;
	rc = idt_pc_get_config(dev_h, &pc_in, &priv->st.pc);
	if (RIO_SUCCESS != rc)
		goto exit;

	set_pc_in.lrto = 50; /* 5 microsecond link timeout */
	set_pc_in.log_rto = 500; /* 50 microsecond logical response timeout */
	set_pc_in.oob_reg_acc = false;
	set_pc_in.num_ports = priv->st.pc.num_ports;
	memcpy(set_pc_in.pc, priv->st.pc.pc, sizeof(priv->st.pc));

	for (int i = 0; i < priv->st.pc.num_ports; i++) {
		set_pc_in.pc[i].fc = idt_pc_fc_rx;
	};

	rc = mpsw_drv_raw_reg_rd(pe, pe->destid, pe->hopcount, RIO_SW_PORT_INF,
				&port_info);
	if (rc) {
		CRIT("Unable to get port info %d:%s\n", rc, strerror(rc));
		goto exit;
	};
	set_pc_in.reg_acc_port = RIO_ACCESS_PORT(port_info);
	
/*
	rc = idt_pc_set_config(dev_h, &set_pc_in, &priv->st.pc);
	if (RIO_SUCCESS != rc)
		goto exit;
*/

	ps_in.ptl.num_ports = RIO_ALL_PORTS;
	rc = idt_pc_get_status(dev_h, &ps_in, &priv->st.ps);
	if (RIO_SUCCESS != rc)
		goto exit;
	if (SWITCH((&priv->dev_h))) {
		pe_port_t port;
		rt_in.probe_on_port = RIO_ALL_PORTS;
		rt_in.rt	    = &priv->st.g_rt;
		rc = idt_rt_probe_all(dev_h, &rt_in, &rt_out);
		if (RIO_SUCCESS != rc)
			goto exit;
		for (port = 0; port < NUM_PORTS((&priv->dev_h)); port++) {
			rt_in.probe_on_port = port;
			rt_in.rt	    = &priv->st.pprt[port];
			rc = idt_rt_probe_all(dev_h, &rt_in, &rt_out);
			if (RIO_SUCCESS != rc)
				goto exit;
		};
	};

	if (SWITCH((&priv->dev_h))) {
		pe_port_t port;
		rc = mpdrv_init_rt(*ct, dev_h);
		if (rc)
			goto exit;

		rt_in.probe_on_port = RIO_ALL_PORTS;
		rt_in.rt	    = &priv->st.g_rt;
		rc = idt_rt_probe_all(dev_h, &rt_in, &rt_out);
		if (RIO_SUCCESS != rc)
			goto exit;
		for (port = 0; port < NUM_PORTS((&priv->dev_h)); port++) {
			rt_in.probe_on_port = port;
			rt_in.rt	    = &priv->st.pprt[port];
			rc = idt_rt_probe_all(dev_h, &rt_in, &rt_out);
			if (RIO_SUCCESS != rc)
				goto exit;
		};
	};

	/* Initialize performance counter structure */
	sc_in.ptl.num_ports = RIO_ALL_PORTS;
	sc_in.dev_ctrs = &priv->st.sc_dev;
	priv->st.sc_dev.num_p_ctrs   = IDT_MAX_PORTS;
	priv->st.sc_dev.valid_p_ctrs = 0;
	priv->st.sc_dev.p_ctrs       = priv->st.sc;

	rc = idt_sc_init_dev_ctrs(dev_h, &sc_in, &sc_out);
	if (RIO_SUCCESS != rc)
		goto exit;

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
	struct mpsw_drv_private_data *p_dat = NULL;
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
	ret = mpsw_drv_raw_reg_rd(pe, pe->destid, pe->hopcount, RIO_DEV_IDENT,
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
		RIO_SP_GEN_CTL(p_dat->dev_h.extFPtrForPort), &gen_ctl);
	if (ret) {
		CRIT("Adding device to mport failed: %d (0x%x)\n", ret, ret);
		goto error;
	};

	gen_ctl |= RIO_SP_GEN_CTL_MAST_EN | RIO_SP_GEN_CTL_DISC;

	ret = mpsw_drv_reg_wr(pe, 
		RIO_SP_GEN_CTL(p_dat->dev_h.extFPtrForPort), gen_ctl);
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

int RIOCP_WU mpsw_drv_init_pe_em(struct riocp_pe *pe, bool en_em)
{
        idt_em_dev_rpt_ctl_in_t         rpt_in;
	struct mpsw_drv_private_data *priv;
	DAR_DEV_INFO_t *dev_h;

	if (RIOCP_PE_IS_MPORT(pe))
		return 0;

	priv =(struct mpsw_drv_private_data *)(pe->private_data); 

	if (NULL == priv) {
		ERR("Private Data is NULL, exiting\n");
		return -1;
	};

	dev_h = &priv->dev_h;

	if (en_em)
        	rpt_in.notfn = idt_em_notfn_pw;
	else
        	rpt_in.notfn = idt_em_notfn_none;

	rpt_in.ptl.num_ports = RIO_ALL_PORTS;
	return idt_em_dev_rpt_ctl(dev_h, &rpt_in, &priv->st.em_notfn);
};

int RIOCP_WU mpsw_drv_destroy_pe(struct riocp_pe *pe)
{
	DBG("ENTRY\n");
	mpsw_destroy_priv_data(pe);
	DBG("EXIT\n");
	return 0;
};

int RIOCP_WU mpsw_drv_recover_port(struct riocp_pe *pe, pe_port_t port,
				pe_port_t lp_port)
{
	struct mpsw_drv_private_data *p_dat = NULL;
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
	clr_errs_in.clr_lp_port_err = 1;
	clr_errs_in.lp_dev_info = NULL;
	clr_errs_in.num_lp_ports = 1;
	clr_errs_in.lp_port_list[0] = lp_port;

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


int mpsw_drv_get_route_entry(struct riocp_pe  *pe, pe_port_t port,
					uint32_t did, pe_rt_val *rt_val)
{
	struct mpsw_drv_private_data *p_dat = NULL;
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
		       pe_port_t port, uint32_t did, pe_rt_val rt_val)
{
	struct mpsw_drv_private_data *p_dat = NULL;
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

int mpsw_drv_alloc_mcast_mask(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val *rt_val, int32_t port_mask)
{
	struct mpsw_drv_private_data *p_dat = NULL;
	int ret;
	idt_rt_alloc_mc_mask_in_t alloc_in;
	idt_rt_alloc_mc_mask_out_t alloc_out;
	idt_rt_change_mc_mask_in_t chg_in;
	idt_rt_change_mc_mask_out_t chg_out;
	idt_rt_set_changed_in_t set_in;
	idt_rt_set_changed_out_t set_out;

	DBG("ENTRY\n");
	if (!(RIOCP_PE_IS_SWITCH(sw->cap))) {
		DBG("Allocated multicast mask on non-switch, EXITING!\n");
		goto fail;
	};
	if (riocp_pe_handle_get_private(sw, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if ((port != RIOCP_PE_ALL_PORTS) && (port >= RIOCP_PE_PORT_COUNT(sw->cap))) {
		DBG("Illegal Look Up Table Selected - max is %d. EXITING!",
			RIOCP_PE_PORT_COUNT(sw->cap)-1);
		goto fail;
	};

	if (RIOCP_PE_ALL_PORTS == port)
		alloc_in.rt = &p_dat->st.g_rt;
	else
		alloc_in.rt = &p_dat->st.pprt[port];

	ret = idt_rt_alloc_mc_mask(&p_dat->dev_h, &alloc_in, &alloc_out);
	if (ret) {
		DBG("ALLOC_MC %s port %d ret 0x%x imp_rc 0x%x\n",
			sw->name?sw->name:"Unknown", port, ret,
			alloc_out.imp_rc);
		goto fail;
	};

	*rt_val = alloc_out.mc_mask_rte;
	
	chg_in.mc_mask_rte = alloc_out.mc_mask_rte;
	chg_in.mc_info.mc_mask = port_mask;
	chg_in.rt = alloc_in.rt;

	ret = idt_rt_change_mc_mask(&p_dat->dev_h, &chg_in, &chg_out);
	if (ret) {
		DBG("CHG_MC %s port %d mask %x ret 0x%x imp_rc 0x%x\n",
			sw->name?sw->name:"Unknown", port, port_mask, ret,
			alloc_out.imp_rc);
		goto fail;
	};

	set_in.set_on_port = port;
	set_in.rt = alloc_in.rt;

	ret = idt_rt_set_changed(&p_dat->dev_h, &set_in, &set_out);
	if (ret) {
		DBG("RT_SET %s port %d rte %x ret 0x%x imp_rc 0x%x\n",
			sw->name?sw->name:"Unknown", port, rt_val, ret,
			set_out.imp_rc);
		goto fail;
	};
	
	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_free_mcast_mask(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val)
{
	struct mpsw_drv_private_data *p_dat = NULL;
	int ret;
	idt_rt_dealloc_mc_mask_in_t free_in;
	idt_rt_dealloc_mc_mask_out_t free_out;

	DBG("ENTRY\n");
	if (!(RIOCP_PE_IS_SWITCH(sw->cap))) {
		DBG("Allocated multicast mask on non-switch, EXITING!\n");
		goto fail;
	};
	if (riocp_pe_handle_get_private(sw, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if ((port != RIOCP_PE_ALL_PORTS) && (port >= RIOCP_PE_PORT_COUNT(sw->cap))) {
		DBG("Illegal Look Up Table Selected - max is %d. EXITING!",
			RIOCP_PE_PORT_COUNT(sw->cap)-1);
		goto fail;
	};

	if (RIOCP_PE_ALL_PORTS == port)
		free_in.rt = &p_dat->st.g_rt;
	else
		free_in.rt = &p_dat->st.pprt[port];
	free_in.mc_mask_rte = rt_val;

	ret = idt_rt_dealloc_mc_mask(&p_dat->dev_h, &free_in, &free_out);
	if (ret) {
		DBG("FREE_MC %s port %d ret 0x%x imp_rc 0x%x\n",
			sw->name?sw->name:"Unknown", port, ret,
			free_out.imp_rc);
		goto fail;
	};

	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_change_mcast_mask(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val, uint32_t port_mask)
{
	struct mpsw_drv_private_data *p_dat = NULL;
	int ret;
	idt_rt_change_mc_mask_in_t chg_in;
	idt_rt_change_mc_mask_out_t chg_out;
	idt_rt_set_changed_in_t set_in;
	idt_rt_set_changed_out_t set_out;

	DBG("ENTRY\n");
	if (!(RIOCP_PE_IS_SWITCH(sw->cap))) {
		DBG("Allocated multicast mask on non-switch, EXITING!\n");
		goto fail;
	};
	if (riocp_pe_handle_get_private(sw, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if ((port != RIOCP_PE_ALL_PORTS) && (port >= RIOCP_PE_PORT_COUNT(sw->cap))) {
		DBG("Illegal Look Up Table Selected - max is %d. EXITING!",
			RIOCP_PE_PORT_COUNT(sw->cap)-1);
		goto fail;
	};

	if (RIOCP_PE_ALL_PORTS == port)
		chg_in.rt = &p_dat->st.g_rt;
	else
		chg_in.rt = &p_dat->st.pprt[port];

	chg_in.mc_mask_rte = rt_val;
	chg_in.mc_info.mc_mask = port_mask;

	ret = idt_rt_change_mc_mask(&p_dat->dev_h, &chg_in, &chg_out);
	if (ret) {
		DBG("CHG_MC %s port %d mask %x ret 0x%x imp_rc 0x%x\n",
			sw->name?sw->name:"Unknown", port, port_mask, ret,
			chg_out.imp_rc);
		goto fail;
	};

	set_in.set_on_port = port;
	set_in.rt = chg_in.rt;

	ret = idt_rt_set_changed(&p_dat->dev_h, &set_in, &set_out);
	if (ret) {
		DBG("CHG_MC %s port %d rte %x ret 0x%x imp_rc 0x%x\n",
			sw->name?sw->name:"Unknown", port, rt_val, ret,
			set_out.imp_rc);
		goto fail;
	};
	
	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_port_start(struct riocp_pe  *pe, pe_port_t port)
{
	struct mpsw_drv_private_data *p_dat = NULL;
	int ret;
	int recover_ret = 0;
	idt_pc_get_status_in_t st_in;
	idt_pc_set_config_in_t cfg_in;
	idt_pc_probe_in_t probe_in;
	idt_pc_probe_out_t probe_out;

	DBG("ENTRY\n");
	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if (port >= NUM_PORTS(&p_dat->dev_h)) {
		DBG("Port illegal, EXITING!\n");
		goto fail;
	};

	cfg_in.lrto = p_dat->st.pc.lrto;
	cfg_in.oob_reg_acc = false;
	cfg_in.reg_acc_port = RIOCP_PE_SW_PORT(pe->cap);
	cfg_in.num_ports = p_dat->st.pc.num_ports;
	memcpy(&cfg_in.pc, &p_dat->st.pc.pc, sizeof(p_dat->st.pc.pc));

	cfg_in.pc[port].port_available = true;
	cfg_in.pc[port].powered_up = true;
	cfg_in.pc[port].xmitter_disable = false;
	cfg_in.pc[port].port_lockout = false;
	cfg_in.pc[port].nmtc_xfer_enable = true;

	ret = idt_pc_set_config(&p_dat->dev_h, &cfg_in, &p_dat->st.pc);
	if (ret) {
		DBG("PC_SetConfig %s port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			p_dat->st.pc.imp_rc);
		goto fail;
	};
	
	probe_in.port = port;

	ret = idt_pc_probe(&p_dat->dev_h, &probe_in, &probe_out);

	if (ret) {
		DBG("PC_Probes %s port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			probe_out.imp_rc);
		goto fail;
	};

	switch (probe_out.status) {
	case port_ok: break; /* Alls good... */
	case port_degr: break; /* Port is up, but not at maximum width */
	case port_los: goto fail; /* Port could not initialize, fail */
	case port_err:
		// FIXME: Link partner port not set correctly.
		recover_ret = mpsw_drv_recover_port(pe, port, 0);
		if (recover_ret) {
			DBG("Recover Port %s port %d failed.\n",
				pe->name?pe->name:"Unknown", port);
		};

	default: 
		DBG("PC_Probes %s port %d Unknown status 0x%x\n", 
			pe->name?pe->name:"Unknown", port, probe_out.status);
		recover_ret = 1;
	}
	
	/* Update status of all ports on this device. */
	st_in.ptl.num_ports = RIO_ALL_PORTS;
	ret = idt_pc_get_status(&p_dat->dev_h, &st_in, &p_dat->st.ps);
	if (ret) {
		DBG("PC_Status %s port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			p_dat->st.ps.imp_rc);
		goto fail;
	};
	
	DBG("EXIT\n");
	return recover_ret;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_port_stop(struct riocp_pe  *pe, pe_port_t port)
{
	struct mpsw_drv_private_data *p_dat = NULL;
	int ret;
	idt_pc_set_config_in_t cfg_in;

	DBG("ENTRY\n");
	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if (port >= NUM_PORTS(&p_dat->dev_h)) {
		DBG("Port illegal, EXITING!\n");
		goto fail;
	};

	cfg_in.lrto = p_dat->st.pc.lrto;
	cfg_in.oob_reg_acc = false;
	cfg_in.reg_acc_port = RIOCP_PE_SW_PORT(pe->cap);
	cfg_in.num_ports = p_dat->st.pc.num_ports;
	memcpy(cfg_in.pc, p_dat->st.pc.pc, sizeof(p_dat->st.pc.pc));

	cfg_in.pc[port].powered_up = false;

	ret = idt_pc_set_config(&p_dat->dev_h, &cfg_in, &p_dat->st.pc);
	if (ret) {
		DBG("PC_SetConfig %s port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			p_dat->st.pc.imp_rc);
		goto fail;
	};

	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_reset_port(struct riocp_pe  *pe, pe_port_t port, bool reset_lp)
{
	struct mpsw_drv_private_data *p_dat = NULL;
	int ret;
	idt_pc_reset_port_in_t reset_in;
	idt_pc_reset_port_out_t reset_out;

	DBG("ENTRY\n");
	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		goto fail;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		goto fail;
	};

	if (port >= NUM_PORTS(&p_dat->dev_h)) {
		DBG("Port illegal, EXITING!\n");
		goto fail;
	};

	reset_in.port_num = port;
	reset_in.oob_reg_acc = false;
	reset_in.reg_acc_port = RIOCP_PE_SW_PORT(pe->cap);
	reset_in.reset_lp = reset_lp;
	reset_in.preserve_config = true;

	ret = idt_pc_reset_port(&p_dat->dev_h, &reset_in, &reset_out);
	if (ret) {
		DBG("PC_reset_port %s port %d ret 0x%x imp_rc 0x%x\n",
			pe->name?pe->name:"Unknown", port, ret,
			reset_out.imp_rc);
		goto fail;
	};

	DBG("EXIT\n");
	return 0;
fail:
	DBG("FAIL\n");
	return 1;
};

int mpsw_drv_get_port_state(struct riocp_pe  *pe, pe_port_t port,
			struct riocp_pe_port_state_t *state)
{
	struct mpsw_drv_private_data *p_dat = NULL;
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

	if (port >= NUM_PORTS(&p_dat->dev_h)) {
		DBG("Port illegal, EXITING!\n");
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
	case idt_pc_ls_2p5  : state->port_lane_speed = 2500;
	case idt_pc_ls_3p125: state->port_lane_speed = 3125;
	case idt_pc_ls_5p0  : state->port_lane_speed = 5000;
	case idt_pc_ls_6p25 : state->port_lane_speed = 6250;
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

int mpsw_drv_set_port_speed(struct riocp_pe  *pe, pe_port_t port,
			uint32_t lane_speed)
{
	struct mpsw_drv_private_data *p_dat;
	int ret = 1;
	idt_pc_set_config_in_t pc_in;

	DBG("ENTRY\n");
	if (riocp_pe_handle_get_private(pe, (void **)&p_dat)) {
		DBG("Private Data does not exist EXITING!\n");
		return ret;
	};

	if (!p_dat->dev_h_valid) {
		DBG("Device handle not valid EXITING!\n");
		return ret;
	};

	if (port >= NUM_PORTS(&p_dat->dev_h)) {
		DBG("State or port illegal, EXITING!\n");
		return ret;
	};

	pc_in.lrto = p_dat->st.pc.lrto;
	pc_in.oob_reg_acc = 0;
	pc_in.reg_acc_port = RIO_ALL_PORTS;
	pc_in.num_ports = p_dat->st.pc.num_ports;
	memcpy(&pc_in.pc, p_dat->st.pc.pc, sizeof(pc_in.pc));
	switch (lane_speed) {
	case 1250: pc_in.pc[port].ls = idt_pc_ls_1p25;
	case 2500: pc_in.pc[port].ls = idt_pc_ls_2p5;
	case 3125: pc_in.pc[port].ls = idt_pc_ls_3p125;
	case 5000: pc_in.pc[port].ls = idt_pc_ls_5p0;
	case 6250: pc_in.pc[port].ls = idt_pc_ls_6p25; 
	default  : pc_in.pc[port].ls = idt_pc_ls_last;
	};

	return idt_pc_set_config(&p_dat->dev_h, &pc_in, &p_dat->st.pc);
};


struct riocp_pe_driver pe_mpsw_driver = {
	mpsw_drv_init_pe,
	mpsw_drv_init_pe_em,
	mpsw_drv_destroy_pe,
	mpsw_drv_recover_port,
	mpsw_drv_set_port_speed,
	mpsw_drv_get_port_state,
	mpsw_drv_port_start,
	mpsw_drv_port_stop,
	mpsw_drv_reset_port,
	mpsw_drv_set_route_entry,
	mpsw_drv_get_route_entry,
	mpsw_drv_alloc_mcast_mask,
	mpsw_drv_free_mcast_mask,
	mpsw_drv_change_mcast_mask
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
