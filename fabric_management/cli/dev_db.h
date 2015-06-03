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

#ifndef __DEV_DB_H__
#define __DEV_DB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "IDT_Port_Config_API.h"
#include "IDT_Routing_Table_Config_API.h"
#include "IDT_Statistics_Counter_API.h"
#include "IDT_Error_Management_API.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <time.h>
#include "dev_db_path.h"

struct dev_state {
	STATUS			 rc;	  /* RC of last routine call      */
	idt_pc_rst_handling	 dev_rst; /* Device reset handling config */
	idt_pc_get_config_out_t  pc;	  /* Standard config vals */
	idt_pc_get_status_out_t	 ps;	  /* Standard status vals */
	idt_rt_state_t		 g_rt;    /* Global Routing Table for device */
	idt_rt_state_t		 pprt[IDT_MAX_PORTS]; /* Per port RT */
	idt_sc_p_ctrs_val_t	 sc[IDT_MAX_PORTS]; /* Statistics counters */
	idt_sc_dev_ctrs_t	 sc_dev; /* Device info for stats counters */
	idt_em_cfg_pw_t		 em_pw_cfg; /* Event Management Portwrite Cfg */
	idt_em_dev_rpt_ctl_out_t em_notfn; /* Device notification control */
};

struct dev_db_entry {
	UINT32			ct; /* Component tag value for this device. */
	UINT32			ref_cnt; /* Library user reference count */
	BOOL			acc; /* TRUE if device is accessable/avail */
	struct route_w_resp	mpr; /* Maintenace packet route */
	DAR_DEV_INFO_t		dev_h; /* DAR/DSF handle for this device */
	struct dev_db_entry 	*lp[IDT_MAX_PORTS]; /* Port link partners */
	UINT8			lp_p[IDT_MAX_PORTS]; /* Connected lp port */
	struct dev_db_entry 	*next; /* next entry in component list */
	struct dev_db_entry	*prev; /* previous entry in component list */
	struct dev_state	st; /* Device state. */
};

void init_dev_db(BOOL writeable);

/* Routines for searching the database */
struct dev_db_entry *db_get_md(void);
struct dev_db_entry *next_dbe(struct dev_db_entry *dbe);
struct dev_db_entry *next_dbe_p(struct dev_db_entry *dbe, UINT8 port);
struct dev_db_entry *find_dbe_ct(UINT32 ct);

/* Routines for initializing the database of fabric components.
 * Enumerate: Initialize and assign component tags and device IDs.
 * Discover : Learn the component tags and device IDs assigned to devices.
 */
#define ENUMERATE TRUE
#define DISCOVER FALSE
STATUS init_md(BOOL enume);

void probe_port(struct dev_db_entry *st_dbe, UINT8 port,
            struct dev_db_entry **new_dbe, BOOL *prev_found);
void discover_port(struct dev_db_entry *st_dbe, UINT8 port,
            struct dev_db_entry **new_dbe, BOOL *prev_found);

void enumerate(struct dev_db_entry *dbe, UINT8 port);
void discover(struct dev_db_entry *dbe, UINT8 port);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_DB_H__ */
