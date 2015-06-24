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
#include "fmd_dd.h"

#ifndef _FMD_CFG_H_
#define _FMD_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FMD_OP_MODE_SLAVE 0
#define FMD_OP_MODE_MASTER 1
#define FMD_DEV08 0
#define FMD_DEV16 1
#define FMD_DEV32 2
#define FMD_DEVID_MAX (FMD_DEV32+1)

struct dev_id {
	uint32_t devid;
	uint32_t hc;
	uint32_t valid;
};

struct fmd_cfg_ep;

struct fmd_mport_info {
	uint32_t num;
	riocp_pe_handle mp_h;
	int op_mode;
	struct dev_id devids[FMD_DEVID_MAX];
	struct fmd_cfg_ep *ep; /* Link to endpoint definition for this MPORT */
	int ep_pnum; /* EP port number that matches this MPORT */
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
#define FMD_MAX_EP 4
#define FMD_MAX_EP_PORT 1
#define FMD_MAX_SW 1
#define FMD_MAX_SW_PORT 18
#define FMD_MAX_CONN 10
#define FMD_MAX_CONN_PORT 18
#define FMD_MAX_DEVS 20

#define OTHER_END(x) ((1 == x)?0:((0==x)?1:2))

#define FMD_SLAVE -1

struct fmd_cfg_rapidio {
	idt_pc_pw_t max_pw;
	idt_pc_pw_t op_pw;
	idt_pc_ls_t ls;
	int idle2; /* 0 for idle1, 1 for idle2 */
	int em; /* 0 for no error management, 1 to enable erro management */
};

struct fmd_cfg_ep_port {
	int valid;
	uint32_t port;
	uint32_t ct;
	struct fmd_cfg_rapidio rio;
	struct dev_id devids[FMD_DEVID_MAX];
	struct fmd_cfg_conn *conn;
	int conn_end; /* index of *conn for this switch */
};

struct fmd_cfg_ep {
	int valid;
	riocp_pe_handle ep_h;
	char *name;
	int port_cnt;
	struct fmd_cfg_ep_port ports[FMD_MAX_EP_PORT];
};

struct fmd_cfg_sw_port {
	int valid;
	int port;
	struct fmd_cfg_rapidio rio;
	struct fmd_cfg_conn *conn;
	int conn_end; /* index of *conn for this switch */
};

struct fmd_cfg_sw {
	int valid;
	riocp_pe_handle sw_h;
	char *name;
	char *dev_type;
	uint32_t did_sz;
	uint32_t did;
	uint32_t hc;
	uint32_t ct;
	uint32_t traversed;
	struct fmd_cfg_sw_port ports[FMD_MAX_SW_PORT];
	// One routing table for each devID size
	idt_rt_state_t rt[FMD_DEVID_MAX]; 
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
	int valid;
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
	uint32_t mast_devid_sz;	/* Master FMD location information */
	uint32_t mast_devid;		/* Master FMD location information */
	uint32_t mast_cm_port; 	/* Master FMD location information */
	int mast_interval;	/* Master FMD location information */
	char *fmd_cfg; /* FMD configuration file */
	FILE *fmd_cfg_fd; /* FMD configuration file file descriptor */
	char *dd_fn; /* Device directory file name */
	char *dd_mtx_fn; /* Device directory mutext file name */
	uint32_t ep_cnt;
	struct fmd_cfg_ep eps[FMD_MAX_EP];
	uint32_t sw_cnt;
	struct fmd_cfg_sw sws[FMD_MAX_SW];
	uint32_t conn_cnt;
	struct fmd_cfg_conn cons[FMD_MAX_CONN];
};

extern struct fmd_cfg_parms *fmd_parse_options(int argc, char *argv[]);
extern void fmd_process_cfg_file(struct fmd_cfg_parms *cfg);
extern struct fmd_cfg_sw *find_cfg_sw_by_ct(uint32_t ct, 
					struct fmd_cfg_parms *cfg);

#ifdef __cplusplus
}
#endif

#endif /* _FMD_CFG_H_ */
