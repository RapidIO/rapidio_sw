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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>

#ifdef __WINDOWS__
#include "stdafx.h"
#include <io.h>
#include <windows.h>
#include "tsi721api.h"
#include "IDT_Tsi721.h"
#endif

// #ifdef __LINUX__
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
// #endif

#include "riocp_pe_internal.h"
#include "dev_db_sm.h"
#include "cli_cmd_db.h"
#include "cli_cmd_line.h"
#include "cli_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dev_db_entry md; /* Master device.  Allows access to all master ports
                         * on the device. Head of all lists of all devices.
                         * NOTE: When rio_mport_cdev is loaded, the master
                         * device is the current operating node.  When
                         * rio_mport_cdev is not loaded, the master device is
                         * the master devices link partner.
                         */

struct dev_db_entry *db_get_md(void)
{
        return &md;
};

struct dev_db_entry *next_dbe(struct dev_db_entry *dev)
{
        if (NULL != dev)
                return dev->next;
        return NULL;
};

struct sm_db_devs *sm_db;
struct sm_db_mtx *sm_db_mutex;
BOOL sm_db_wr_acc;

void sm_db_init(void)
{
	int idx;

	if ((NULL == sm_db_mutex) || (NULL == sm_db))
		return;

	if (sm_db->chg_idx && sm_db_mutex->db_ref_cnt) {
		sm_db_mutex->db_ref_cnt++;
		goto exit;
	};

	sm_db_mutex->db_ref_cnt = 1;
	sm_db->chg_idx = 0;
	sm_db->md_ct = -2;
	sm_db->num_devs = 0;
	for (idx = 0; idx <= MAX_SM_DEVS; idx++) {
		sm_db->devs[idx].ct = -2;
		sm_db->devs[idx].destID = -2;
		sm_db->devs[idx].hc = -2;
	};
	sm_db_incr_chg_idx();

exit:
	return;
};
		
struct sm_db_devs *sm_db_get_rw(void)
{
        int sm_fd;
        int rc;
        struct sm_db_devs *sm_ptr = NULL;

        sm_fd = shm_open(RIO_DEV_DB_SM, O_RDWR | O_CREAT | O_EXCL, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == sm_fd) {
		if (EEXIST == errno ) {
        		sm_fd = shm_open(RIO_DEV_DB_SM, O_RDWR, 
                        		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
	};

        if (-1 == sm_fd) {
        	fprintf( stderr, "rw shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
	}


        rc = ftruncate(sm_fd, sizeof(struct sm_db_devs));
        if (-1 == rc) {
        	fprintf( stderr, "rw ftruncate failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                shm_unlink(RIO_DEV_DB_SM);
                goto exit;
        };

        sm_ptr = (struct sm_db_devs *)
		mmap(NULL, sizeof(struct sm_db_devs), PROT_READ|PROT_WRITE,
                MAP_SHARED, sm_fd, 0);

        if (MAP_FAILED == sm_ptr) {
        	fprintf( stderr, "rw mmap failed:0x%x %s\n",
            		errno, strerror( errno ) );
                sm_ptr = NULL;
                shm_unlink(RIO_DEV_DB_SM);
                goto exit;
        };

	sm_db = sm_ptr;
	
	sm_db_init();
exit:
        return sm_ptr;
};

struct sm_db_devs *sm_db_get_ro(void)
{
	int sm_fd;
	struct sm_db_devs *sm_ptr = NULL;

	sm_fd = shm_open(RIO_DEV_DB_SM, O_RDONLY, 0);
        if (-1 == sm_fd) {
        	fprintf( stderr, "RO shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
	}

        sm_ptr = (struct sm_db_devs *)
		mmap(NULL, sizeof(struct sm_db_devs), PROT_READ,
                MAP_SHARED, sm_fd, 0);

        if (MAP_FAILED == sm_ptr) {
        	fprintf( stderr, "RO mmap failed:0x%x %s\n",
            		errno, strerror( errno ) );
                sm_ptr = NULL;
                goto exit;
        };

	if ((NULL != sm_ptr) && (NULL != sm_db_mutex)) {
		if (sm_ptr->chg_idx && sm_db_mutex->db_ref_cnt) {
			sm_db_mutex->db_ref_cnt++;
			goto exit;
		};
	};
exit:
	return sm_ptr;
};

struct sm_db_mtx *sm_db_get_mutex(void)
{
        int mtx_fd;
        int rc;
	struct sm_db_mtx *mtx_ptr = NULL;

        mtx_fd = shm_open(RIO_DEV_DB_SM_MUTEX, O_RDWR | O_CREAT | O_EXCL, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == mtx_fd)
		if (EEXIST == errno)
        		mtx_fd = shm_open(RIO_DEV_DB_SM_MUTEX, O_RDWR, 
                        		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == mtx_fd) {
        	fprintf( stderr, "rw mutex shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
	}

        rc = ftruncate(mtx_fd, sizeof(struct sm_db_mtx));
        if (-1 == rc) {
        	fprintf( stderr, "rw mutex ftruncate failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
        };

        mtx_ptr = (struct sm_db_mtx *)
		mmap(NULL, sizeof(struct sm_db_mtx), 
			PROT_READ|PROT_WRITE, MAP_SHARED, mtx_fd, 0);

        if (MAP_FAILED == mtx_ptr) {
        	fprintf( stderr, "rw mutex mmap failed:0x%x %s\n",
            		errno, strerror( errno ) );
                mtx_ptr = NULL;
                goto exit;
        };

	if (NULL != mtx_ptr) {
		if (mtx_ptr->ref_cnt && mtx_ptr->init_done) {
			mtx_ptr->ref_cnt++;
		} else {
			sem_init(&mtx_ptr->sm_db_mutex, 1, 1);
			mtx_ptr->ref_cnt = 1;
			mtx_ptr->init_done = TRUE;
		};
	};
			
exit:
	return mtx_ptr;
};

void sm_db_cleanup(void) {
	if ((NULL != sm_db) && (NULL != sm_db_mutex)) {
		if (sm_db->chg_idx && sm_db_mutex->ref_cnt) {
			sm_db_mutex->db_ref_cnt--;
			if (!sm_db_mutex->db_ref_cnt) {
				if (sm_db_wr_acc)
					sm_db->chg_idx = 0;
				shm_unlink(RIO_DEV_DB_SM);
			};
		}
		sm_db = NULL;
	};
	if (NULL != sm_db_mutex) {
		if (sm_db_mutex->init_done && sm_db_mutex->ref_cnt) {
			sm_db_mutex->ref_cnt--;
			if (!sm_db_mutex->ref_cnt) {
				sem_destroy(&sm_db_mutex->sm_db_mutex);
				sm_db_mutex->init_done = FALSE;
				shm_unlink(RIO_DEV_DB_SM_MUTEX);
			};
		};
			
		sm_db_mutex = NULL;
	};
};
	
riocp_pe_handle mport;

void sm_db_update(void)
{
	riocp_pe_handle *pe_a[MAX_SM_DEVS];
	size_t sz = sizeof(pe_a);
	int idx;

	if (NULL == sm_db)
		goto exit;

	sem_wait(&sm_db_mutex->sm_db_mutex);

	if (riocp_pe_handle_get_list(mport, (riocp_pe_handle **)&pe_a, &sz))
		goto exit;

	sm_db->num_devs = 0;
	
	for (idx = 0; (idx < MAX_SM_DEVS) && (pe_a[idx] != NULL); idx++) {
		if (riocp_pe_get_destid(*pe_a[idx], 
					&sm_db->devs[sm_db->num_devs].destID))
			goto exit;
		if (riocp_pe_get_comptag(*pe_a[idx], 
					&sm_db->devs[sm_db->num_devs].ct))
			goto exit;

		sm_db->devs[sm_db->num_devs].hc     = -3;
		sm_db->num_devs++;
	};
	sm_db_incr_chg_idx();
	sem_post(&sm_db_mutex->sm_db_mutex);
exit:
	return;
};

void sm_db_incr_chg_idx(void)
{
	if ((NULL != sm_db) && (sm_db_wr_acc)) {
		UINT32 next_idx = sm_db->chg_idx+1;
		if (!next_idx)
			next_idx = 1;
		sm_db->chg_idx = next_idx;
		clock_gettime(CLOCK_REALTIME, &sm_db->chg_time);
	};
};

UINT32 sm_db_get_chg_idx(void)
{
	if (NULL != sm_db)
		return sm_db->chg_idx;

	return 0;
};

/* Note that get_first_dev and get_next_dev will block until
 * enumeration has been completed.
 */

UINT32 sm_db_atomic_copy(int max_devs, struct sm_db_dev_info *devs)
{
	UINT32 idx;
	UINT32 num_devs;

	sem_wait(&sm_db_mutex->sm_db_mutex);
	for (idx = 0; idx < sm_db->num_devs; idx++) 
		devs[idx] = sm_db->devs[idx];
	num_devs = sm_db->num_devs;
	sem_post(&sm_db_mutex->sm_db_mutex);

	return num_devs;
};

extern const struct cli_cmd SMChgCnt;

int SMChgCntCmd(struct cli_env *env, int argc, char **argv)
{

	if (NULL == sm_db) {
		sprintf(env->output, "\nsm_db is NULL.\n");
		logMsg(env);
	} else {
		printf(env->output, "\nTime %lld.%.9ld ChgIdx: 0x%8x\n", 
			(long long)sm_db->chg_time.tv_sec,
			sm_db->chg_time.tv_nsec,  sm_db->chg_idx);
		logMsg(env);
		printf(env->output, "sm_db: md_ct %x num_devs %x\n", 
			sm_db->md_ct, sm_db->num_devs);
		logMsg(env);
	};

	if (NULL == sm_db_mutex)
		sprintf(env->output, "sm_db_mutex is NULL.\n");
	else 
		sprintf(env->output, 
			"sm_db_mutex: ref_cnt %x db_ref_cnt %x init_done %x\n",
			sm_db_mutex->ref_cnt, sm_db_mutex->db_ref_cnt,
			sm_db_mutex->init_done );
	logMsg(env);
	return 0;
};

const struct cli_cmd SMChgCnt = {
(char *)"chg",
3,
0,
(char *)"Change index command, no parameters.",
(char *)"Prints current change index for the device database.",
SMChgCntCmd,
ATTR_NONE
};

extern const struct cli_cmd SMIncCnt;

int SMIncCntCmd(struct cli_env *env, int argc, char **argv)
{
	sm_db_incr_chg_idx();
	sprintf(env->output, "\nIncrement idx value: 0x%8x\n", 
		sm_db_get_chg_idx());
	logMsg(env);
	return 0;
};

const struct cli_cmd SMIncCnt = {
(char *)"inc",
3,
0,
(char *)"Increment change index, no parameters.",
(char *)"Increments and prints current change index for the device database.",
SMIncCntCmd,
ATTR_NONE
};

extern const struct cli_cmd SMDevs;

int SMDevsCmd(struct cli_env *env, int argc, char **argv)
{
	struct sm_db_dev_info devs[MAX_SM_DEVS];
	int num_devs, idx;

	num_devs = sm_db_atomic_copy(MAX_SM_DEVS, devs);

	if (num_devs) {
		sprintf(env->output, "\nIdx   CT    DevID   HC\n");
		logMsg(env);
		for (idx = 0; idx < num_devs; idx++) {
			sprintf(env->output, "%2d %8x  %2x     %2x\n", idx, 
				devs[idx].ct, devs[idx].destID, devs[idx].hc);
			logMsg(env);
		};
	} else {
		sprintf(env->output, "\nNo devices returned.\n");
		logMsg(env);
	};
	return 0;
};

const struct cli_cmd SMDevs = {
(char *)"devs",
2,
0,
(char *)"Skelton device list.",
(char *)"Prints current list of devices in device database.",
SMDevsCmd,
ATTR_NONE
};

extern const struct cli_cmd SMClean;

int SMCleanCmd(struct cli_env *env, int argc, char **argv)
{

	if (argc) {
		sprintf(env->output, "\nFreeing Mutex, current state:\n");
		logMsg(env);
		if (NULL == sm_db_mutex) {
			sprintf(env->output, "\nsm_db_mutex is NULL\n");
			logMsg(env);
		} else {
			sprintf(env->output, "ref_cnt   : %x\n",
				sm_db_mutex->ref_cnt);
			logMsg(env);
			sprintf(env->output, "db_ref_cnt: %x\n",
				sm_db_mutex->db_ref_cnt);
			logMsg(env);
			sprintf(env->output, "init_done : %x\n",
				sm_db_mutex->init_done);
			logMsg(env);
		};
		shm_unlink(RIO_DEV_DB_SM_MUTEX);
	} else {
		sprintf(env->output, "\nFreeing sm_db, current state:\n");
		logMsg(env);
		if (NULL == sm_db) {
			sprintf(env->output, "\nsm_db is NULL\n");
			logMsg(env);
		} else {
			sprintf(env->output, "ref_cnt   : %x\n",
				sm_db_mutex->ref_cnt);
			logMsg(env);
			sprintf(env->output, "db_ref_cnt: %x\n",
				sm_db_mutex->db_ref_cnt);
			logMsg(env);
			sprintf(env->output, "init_done : %x\n",
				sm_db_mutex->init_done);
			logMsg(env);
		};
		shm_unlink(RIO_DEV_DB_SM);
	};
	return 0;
};

const struct cli_cmd SMClean = {
(char *)"clean",
3,
0,
(char *)"Drops shared memory blocks.",
(char *)"No parms drops sm block, any part drops mutex.",
SMCleanCmd,
ATTR_NONE
};

const struct cli_cmd *sm_sm_cmds[4] = 
	{&SMChgCnt, 
	 &SMIncCnt,
	 &SMDevs,
	 &SMClean };

void bind_dev_db_sm_cmds(void)
{
	// add_commands_to_cmd_db(4, &sm_cmds[0]);
}

#ifdef __cplusplus
}
#endif
