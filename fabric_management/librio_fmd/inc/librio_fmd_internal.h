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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include "IDT_Routing_Table_Config_API.h"
#include "IDT_Port_Config_API.h"
#include "riocp_pe_internal.h"

#ifndef _LIBRIO_FMD_INTERNAL_H_
#define _LIBRIO_FMD_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FMD_OP_MODE_SLAVE 0
#define FMD_OP_MODE_MASTER 1
#define FMD_DEV08 1
#define FMD_DEV16 2
#define FMD_DEV32 4

struct fmd_mport_info {
	int num;
	int op_mode;
	int devid_sizes;
};

#define FMD_MAX_MPORTS 4
#define FMD_DFLT_INIT_DD 0
#define FMD_DFLT_CLI_PORT_NUM 2222
#define FMD_DFLT_RUN_CONS 1
#define FMD_DFLT_MAST_DEVID_SZ FMD_DEV08
#define FMD_DFLT_MAST_DEVID 1
#define FMD_DFLT_MAST_CM_PORT 4545
#define FMD_DFLT_MAST_INTERVAL 5
#define FMD_DFLT_CFG_FN "cfg/fmd.cfg"
#define FMD_DFLT_DD_FN "/RIO_DEV_DIR_SM"
#define FMD_DFLT_DD_MTX_FN "/RIO_DEV_DIR_MTX_SM"
#define FMD_INVALID_CT 0
#define FMD_MAX_EP 8
#define FMD_MAX_EP_PORT 4
#define FMD_MAX_SW 2
#define FMD_MAX_SW_PORT 18
#define FMD_MAX_CONN 10
#define FMD_MAX_CONN_PORT 18
#define FMD_MAX_DEVS 20

#define FMD_SLAVE -1

struct dev_id {
	int devid;
	int hc;
	int valid;
};

struct fmd_cfg_rapidio {
	idt_pc_pw_t max_pw;
	idt_pc_pw_t op_pw;
	idt_pc_ls_t ls;
	int idle2; /* 0 for idle1, 1 for idle2 */
	int em; /* 0 for no error management, 1 to enable erro management */
};

struct fmd_cfg_ep_port {
	int valid;
	int port;
	int ct;
	struct fmd_cfg_rapidio rio;
	struct dev_id devids[3];
};

struct fmd_cfg_ep {
	int valid;
	char *name;
	int port_cnt;
	struct fmd_cfg_ep_port ports[FMD_MAX_EP_PORT];
};

struct fmd_cfg_sw_port {
	int valid;
	int port;
	struct fmd_cfg_rapidio rio;
};

struct fmd_cfg_sw {
	int valid;
	char *name;
	char *dev_type;
	int destid_sz;
	int destid;
	int hc;
	int ct;
	struct fmd_cfg_sw_port ports[FMD_MAX_SW_PORT];
	idt_rt_state_t rt[3]; // One routing table for each devID size
};

struct fmd_cfg_conn_pe {
	int port_num;
	int ep; /* 1 - Endpoint, 0 - Switch */
	union {
		struct fmd_cfg_sw *sw_h;
		struct fmd_cfg_ep *ep_h;
	};
};

struct fmd_cfg_conn {
	struct fmd_cfg_conn_pe ends[2];
};

struct fmd_cfg_parms {
	int init_err;		/* If asserted, abort initialization */
	int init_and_quit;	/* If asserted, exit after completing init */
	int simple_init;	/* If asserted, do not init device directory */
	int cli_port_num;	/* POSIX Socket for remote CLI session */
	int run_cons;		/* Run a console on this daemon. */
	int mast_idx;		/* Idx of the mport_info that is master */
	int max_mport_info_idx; /* Maximum number of mports */
	struct fmd_mport_info mport_info[FMD_MAX_MPORTS]; 
	int mast_devid_sz;	/* Master FMD location information */
	int mast_devid;		/* Master FMD location information */
	int mast_cm_port; 	/* Master FMD location information */
	int mast_interval;	/* Master FMD location information */
	char *fmd_cfg; /* FMD configuration file */
	FILE *fmd_cfg_fd; /* FMD configuration file file descriptor */
	char *dd_fn; /* Device directory file name */
	char *dd_mtx_fn; /* Device directory mutext file name */
	int ep_cnt;
	struct fmd_cfg_ep eps[FMD_MAX_EP];
	int sw_cnt;
	struct fmd_cfg_sw sws[FMD_MAX_SW];
	int conn_cnt;
	struct fmd_cfg_conn cons[FMD_MAX_CONN];
};

struct fmd_dd_dev_info {
	uint32_t ct;
	uint32_t destID;
	uint32_t hc;
};

struct fmd_dd {
	uint32_t chg_idx;
	struct timespec chg_time;
	uint32_t md_ct;  
	uint32_t num_devs;
	struct fmd_dd_dev_info devs[FMD_MAX_DEVS+1];
};

struct fmd_dd_events {
	int in_use; /* 0 - Unallocated, 1 - Allocated */
	pthread_t dd_ev_thr; /* Identifier for thread waiting on dd_event */
	sem_t dd_event; /* sem_post() whenever the dd changes */
};

#define FMD_MAX_EVENTS 10

struct fmd_dd_mtx {
	int mtx_ref_cnt;
	int dd_ref_cnt; /* Read write field for reference count to fmd_dd */
	int init_done;
	sem_t sem;
	struct fmd_dd_events dd_ev[FMD_MAX_EVENTS];
};

struct fmd_state {
	riocp_pe_handle *mp_h;
	struct fmd_cfg_parms *cfg;
	int fmd_rw;
	char *app_name;
	char *dd_fn;
	int dd_fd;
	struct fmd_dd *dd;
	char *dd_mtx_fn;
	int dd_mtx_fd;
	struct fmd_dd_mtx *dd_mtx;
};

extern struct fmd_cfg_parms *fmd_parse_options(int argc, char *argv[]);
extern void fmd_process_cfg_file(struct fmd_cfg_parms *cfg);

extern int fmd_dd_init(struct fmd_cfg_parms *cfg,
				struct fmd_state **st);
extern void fmd_dd_cleanup(struct fmd_state *st);
extern void fmd_dd_update(struct fmd_state *st);
extern uint32_t fmd_dd_atomic_copy(struct fmd_state *st,
			uint32_t *num_devs,
			struct fmd_dd_dev_info *devs);

extern void fmd_dd_incr_chg_idx(struct fmd_state *st);
extern uint32_t fmd_dd_get_chg_idx(struct fmd_state *st);
extern int fmd_dd_change_notfn(struct fmd_state *st);

extern void bind_dd_cmds(struct fmd_state *st);

#ifdef __cplusplus
}
#endif

#endif /* _LIBRIO_FMD_INTERNAL_H_ */
