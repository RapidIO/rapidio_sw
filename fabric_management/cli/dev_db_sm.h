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

#ifndef __DEV_DB_SM_H__
#define __DEV_DB_SM_H__

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
#include "dev_db.h"

#define RIO_DEV_DB_SM "/RIO_DEV_DB_SM"
#define RIO_DEV_DB_SM_MUTEX "/RIO_DEV_DB_SM_MUTEX"

#define MAX_SM_DEVS 256

struct sm_db_dev_info {
	UINT32 ct;
	UINT32 destID;
	UINT32 hc;
};

struct sm_db_devs {
	UINT32	 chg_idx;
	struct timespec chg_time;
	UINT32   md_ct;  
	UINT32	 num_devs;
	struct sm_db_dev_info devs[MAX_SM_DEVS];
};

struct sm_db_mtx {
	int  ref_cnt;
	int  db_ref_cnt; /* Read write field for reference count to sh_db */
	BOOL init_done;
	sem_t sm_db_mutex;
};

extern struct sm_db_devs *sm_db;
extern struct sm_db_mtx *sm_db_mutex;
extern BOOL sm_db_wr_acc;

void sm_db_init(void);
struct sm_db_devs *sm_db_get_rw(void);
struct sm_db_devs *sm_db_get_ro(void);
struct sm_db_mtx *sm_db_get_mutex(void);
void sm_db_cleanup(void);
void sm_db_update( void );
UINT32 sm_db_atomic_copy(int max_devs, struct sm_db_dev_info *devs);

void   sm_db_incr_chg_idx(void);
UINT32 sm_db_get_chg_idx(void);

void bind_dev_db_sm_cmds(void);

extern struct dev_db_entry md;
			/* Master device.  Allows access to all master ports
                         * on the device. Head of all lists of all devices.
                         * NOTE: When rio_mport_cdev is loaded, the master
                         * device is the current operating node.  When
                         * rio_mport_cdev is not loaded, the master device is
                         * the master devices link partner.
                         */

struct dev_db_entry *db_get_md(void);
struct dev_db_entry *next_dbe(struct dev_db_entry *dev);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_DB_SM_H__ */
