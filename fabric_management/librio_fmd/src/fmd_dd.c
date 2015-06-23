/* Fabric Management Daemon Device Directory Implementation */
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

#include "librio_fmd.h"
#include "librio_fmd_internal.h"
#include "liblog.h"
#include "dev_db.h"
#include "cli_cmd_db.h"
#include "cli_cmd_line.h"
#include "cli_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

int fmd_dd_open_rw(struct fmd_cfg_parms *cfg, struct fmd_state *st)
{
	int rc;
	int sz = strlen(cfg->dd_fn)+1;

	st->dd_fn = (char *)malloc(sz);
	memset(st->dd_fn, 0, sz);
	strcpy(st->dd_fn, cfg->dd_fn);

        st->dd_fd = shm_open(st->dd_fn, O_RDWR | O_CREAT | O_EXCL, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == st->dd_fd) {
		if (EEXIST == errno ) {
        		st->dd_fd = shm_open(st->dd_fn, O_RDWR, 
                        		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
	};

        if (-1 == st->dd_fd) {
        	fprintf( stderr, "rw shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
	}

        rc = ftruncate(st->dd_fd, sizeof(struct fmd_dd));
        if (-1 == rc) {
        	fprintf(stderr, "rw ftruncate failed: 0x%x %s\n",
            		errno, strerror(errno));
                shm_unlink(st->dd_fn);
                goto exit;
        };

        st->dd = (fmd_dd *)
		mmap(NULL, sizeof(struct fmd_dd), PROT_READ|PROT_WRITE,
                MAP_SHARED, st->dd_fd, 0);

        if (MAP_FAILED == st->dd) {
        	fprintf(stderr, "rw mmap failed:0x%x %s\n",
            		errno, strerror(errno));
                st->dd = NULL;
                shm_unlink(st->dd_fn);
                goto exit;
        };
	st->fmd_rw = 1;

	return 0;
exit:
        return -1;
};

int fmd_dd_open_ro(char *dd_fn, int *dd_fd, struct fmd_dd **dd, 
					struct fmd_dd_mtx *dd_mtx)
{
	*dd_fd = shm_open(dd_fn, O_RDONLY, 0);
        if (-1 == *dd_fd) {
        	fprintf( stderr, "RO shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
	}

        *dd = (fmd_dd *)mmap(NULL, sizeof(struct fmd_dd), PROT_READ,
                MAP_SHARED, *dd_fd, 0);

        if (MAP_FAILED == *dd) {
        	fprintf(stderr, "RO mmap failed:0x%x %s\n",
            		errno, strerror(errno));
                *dd = NULL;
                goto exit;
        };

	if ((NULL != *dd) && (NULL != dd_mtx))
		if ((*dd)->chg_idx && dd_mtx->dd_ref_cnt)
			dd_mtx->dd_ref_cnt++;
	return 0;
exit:
	return -1;
};

int fmd_dd_mtx_open(char *dd_mtx_fn, int *dd_mtx_fd, struct fmd_dd_mtx **dd_mtx)
{
        int rc;

	if ((NULL == dd_mtx_fn) || (NULL == dd_mtx_fd) || (NULL == dd_mtx)) {
		errno = -EINVAL;
		goto fail;
	};

        *dd_mtx_fd = shm_open(dd_mtx_fn, O_RDWR | O_CREAT | O_EXCL, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == *dd_mtx_fd)
		if (EEXIST == errno)
        		*dd_mtx_fd = shm_open(dd_mtx_fn, O_RDWR, 
                        		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == *dd_mtx_fd) {
        	fprintf( stderr, "rw mutex shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto fail;
	}

        rc = ftruncate(*dd_mtx_fd, sizeof(struct fmd_dd_mtx));
        if (-1 == rc) {
        	fprintf( stderr, "rw mutex ftruncate failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto fail;
        };

        *dd_mtx = (fmd_dd_mtx *)mmap(NULL, sizeof(struct fmd_dd_mtx), 
			PROT_READ|PROT_WRITE, MAP_SHARED, *dd_mtx_fd, 0);

        if (MAP_FAILED == *dd_mtx) {
        	fprintf( stderr, "rw mutex mmap failed:0x%x %s\n",
            		errno, strerror( errno ) );
                *dd_mtx = NULL;
                goto fail;
        };

	if (dd_mtx->dd_ref_cnt && dd_mtx->init_done) {
		dd_mtx->mtx_ref_cnt++;
	} else {
		sem_init(&dd_mtx->sem, 1, 1);
		dd_mtx->mtx_ref_cnt = 1;
		dd_mtx->init_done = TRUE;
	};
	return 0;
fail:
	return -1;
};

int fmd_dd_init(struct fmd_cfg_parms *cfg, struct fmd_state **st)
{
	int rc;
	int idx;
	int sz = strlen(cfg->dd_mtx_fn)+1;

	*st = (fmd_state *)malloc(sizeof(struct fmd_state));
	(*st)->cfg = cfg;
	(*st)->fmd_rw = 1;


	st->dd_mtx_fn = (char *)malloc(sz);
	memset(st->dd_mtx_fn, 0, sz);
	strncpy(st->dd_mtx_fn, cfg->dd_mtx_fn, sz);

       	rc = fmd_dd_mtx_open(st->dd_fn, &st->dd_mtx_fd, &st->dd_mtx);
	rc |= fmd_dd_open_rw(cfg, *st);
	if (rc)
		goto fail;

	/* Previously created dd, add reference, do not initialize */
	if ((*st)->dd->chg_idx && (*st)->dd_mtx->dd_ref_cnt) {
		(*st)->dd_mtx->dd_ref_cnt++;
		goto exit;
	};

	if ((*st)->fmd_rw) {
		(*st)->dd_mtx->dd_ref_cnt = 1;
		(*st)->dd->chg_idx = 0;
		(*st)->dd->md_ct = FMD_INVALID_CT;
		(*st)->dd->num_devs = 0;
		for (idx = 0; idx <= FMD_MAX_DEVS+1; idx++) {
			(*st)->dd->devs[idx].ct = FMD_INVALID_CT;
			(*st)->dd->devs[idx].destID = FMD_INVALID_CT;
			(*st)->dd->devs[idx].hc = 0xFF;
		};
		fmd_dd_incr_chg_idx(*st);
	};
exit:
	return 0;
fail:
	return -1;
};

void fmd_dd_cleanup(struct fmd_state *st)
{
	if ((NULL != st->dd) && (NULL != st->dd_mtx)) {
		if (st->dd->chg_idx && st->dd_mtx->dd_ref_cnt) {
			if (!--st->dd_mtx->dd_ref_cnt) {
				if (st->fmd_rw)
					st->dd->chg_idx = 0;
				shm_unlink(st->dd_fn);
			};
		}
		st->dd = NULL;
	};

	if (NULL != st->dd_mtx) {
		if (st->dd_mtx->init_done && st->dd_mtx->mtx_ref_cnt) {
			if (!--st->dd_mtx->mtx_ref_cnt) {
				sem_destroy(&st->dd_mtx->sem);
				st->dd_mtx->init_done = FALSE;
				shm_unlink(st->dd_mtx_fn);
			};
		};
		st->dd_mtx = NULL;
	};
};
	
void fmd_dd_update(struct fmd_state *st)
{
        size_t pe_cnt;
        riocp_pe_handle *pe = NULL;
        int rc, idx;
        uint32_t comptag, destid;
        uint8_t hopcount;

	if (NULL == st->mp_h) {
                WARN("\nMaster port is NULL, device directory not updated\n");
		goto exit;
	};

        rc = riocp_mport_get_pe_list(*st->mp_h, &pe_cnt, &pe);
        if (rc) {
                CRIT("\nCannot get pe list rc %d...\n", rc);
		goto exit;
        };

	if (pe_cnt > FMD_MAX_DEVS) {
                WARN("\nToo many PEs for DD %d %d...\n", pe_cnt, FMD_MAX_DEVS);
		pe_cnt = FMD_MAX_DEVS;
	};

	sem_wait(&st->dd_mtx->sem);
	st->dd->num_devs = 0;

        for (idx = 0; idx < (int) pe_cnt; idx++) {
                rc = riocp_pe_get_comptag(pe[idx], &comptag);
                if (rc) {
                        WARN("\nCannot get comptag rc %d...\n", rc);
                        comptag = 0xFFFFFFFF;
			continue;
                };
                rc = riocp_pe_get_destid(pe[idx], &destid);
                if (rc) {
                        WARN("\nCannot get comptag rc %d...\n", rc);
                        destid = 0xFFFFFFFF;
			continue;
                };
                hopcount = pe[idx]->hopcount;
	
		st->dd->devs[st->dd->num_devs].ct     = comptag;
		st->dd->devs[st->dd->num_devs].destID = destid;
		st->dd->devs[st->dd->num_devs].hc     = hopcount;
		st->dd->num_devs++;
	};
	fmd_dd_incr_chg_idx(st);
	sem_post(&st->dd_mtx->sem);
exit:
	return;
};

void fmd_dd_incr_chg_idx(struct fmd_state *st)
{
	if ((NULL != st->dd) && st->fmd_rw) {
		uint32_t next_idx = st->dd->chg_idx+1;
		if (!next_idx)
			next_idx = 1;
		st->dd->chg_idx = next_idx;
		clock_gettime(CLOCK_REALTIME, &st->dd->chg_time);
	};
};

uint32_t fmd_dd_get_chg_idx(struct fmd_state *st)
{
	if (NULL != st->dd)
		return st->dd->chg_idx;

	return 0;
};

/* Note that get_first_dev and get_next_dev will block until
 * enumeration has been completed.
 */

uint32_t fmd_dd_atomic_copy(struct fmd_state *st,
			uint32_t *num_devs,
			struct fmd_dd_dev_info *devs)
{
	uint32_t idx;

	sem_wait(&st->dd_mtx->sem);
	for (idx = 0; idx < st->dd->num_devs; idx++) 
		devs[idx] = st->dd->devs[idx];
	*num_devs = st->dd->num_devs;
	sem_post(&st->dd_mtx->sem);

	return *num_devs;
};
struct fmd_state *cli_st;

extern const struct cli_cmd CLIChgCnt;

int CLIChgCntCmd(struct cli_env *env, int argc, char **argv)
{
	if (NULL == cli_st) {
		sprintf(env->output, "\nState pointer is null.\n");
		goto exit;
	};
	if (0)
		argv[0][0] = argc;

	if (NULL == cli_st->dd) {
		sprintf(env->output, "\nDevice Directory not available.\n");
		goto exit;
	} else {
		printf(env->output, "\nTime %lld.%.9ld ChgIdx: 0x%8x\n", 
			(long long)cli_st->dd->chg_time.tv_sec,
			cli_st->dd->chg_time.tv_nsec,  cli_st->dd->chg_idx);
		logMsg(env);
		printf(env->output, "fmd_dd: md_ct %x num_devs %x\n", 
			cli_st->dd->md_ct, cli_st->dd->num_devs);
		logMsg(env);
	};

	if (NULL == cli_st->dd_mtx)
		sprintf(env->output, 
			"\nDevice Directory Mutex not available.\n");
	else 
		sprintf(env->output, 
			"Mutex: mtx_ref_cnt %x dd_ref_cnt %x init_done %x\n",
			cli_st->dd_mtx->mtx_ref_cnt, cli_st->dd_mtx->dd_ref_cnt,
			cli_st->dd_mtx->init_done );
exit:
	logMsg(env);
	return 0;
};

const struct cli_cmd CLIChgCnt = {
(char *)"chg",
3,
0,
(char *)"Change index command, no parameters.",
(char *)"Prints current change index for the device database.",
CLIChgCntCmd,
ATTR_NONE
};

extern const struct cli_cmd CLIIncCnt;

int CLIIncCntCmd(struct cli_env *env, int argc, char **argv)
{
	if (NULL == cli_st) {
		sprintf(env->output, "\nState pointer is null.\n");
		goto exit;
	};
	if (0)
		argv[0][0] = argc;

	fmd_dd_incr_chg_idx(cli_st);
	sprintf(env->output, "\nIncrement idx value: 0x%8x\n", 
		fmd_dd_get_chg_idx(cli_st));
exit:
	logMsg(env);
	return 0;
};

const struct cli_cmd CLIIncCnt = {
(char *)"inc",
3,
0,
(char *)"Increment change index, no parameters.",
(char *)"Increments and prints current change index for the device database.",
CLIIncCntCmd,
ATTR_NONE
};

extern const struct cli_cmd CLIDevDir;

int CLIDevDirCmd(struct cli_env *env, int argc, char **argv)
{
	struct fmd_dd_dev_info devs[FMD_MAX_DEVS+1];
	uint32_t num_devs, idx;

	if (0)
		argv[0][0] = argc;
	if (NULL == cli_st) {
		sprintf(env->output, "\nState pointer is null.\n");
		goto exit;
	};
	num_devs = fmd_dd_atomic_copy(cli_st, &num_devs, devs);

	if (num_devs) {
		sprintf(env->output, "\nIdx   CT    DevID   HC\n");
		for (idx = 0; idx < num_devs; idx++) {
			logMsg(env);
			sprintf(env->output, "%2d %8x  %2x     %2x\n", idx, 
				devs[idx].ct, devs[idx].destID, devs[idx].hc);
		};
	} else {
		sprintf(env->output, "\nNo devices returned.\n");
	};
exit:
	logMsg(env);
	return 0;
};

const struct cli_cmd CLIDevDir = {
(char *)"dd",
2,
0,
(char *)"Device directory display",
(char *)"Prints current list of devices in device directory.",
CLIDevDirCmd,
ATTR_NONE
};

extern const struct cli_cmd CLIClean;

int CLICleanCmd(struct cli_env *env, int argc, char **argv)
{

	if (NULL == cli_st) {
		sprintf(env->output, "\nState pointer is null.\n");
		goto exit;
	};  
	argv[0] = NULL;
	if (argc) {
		sprintf(env->output, "\nFreeing Mutex, current state:\n");
		logMsg(env);
		if (NULL == cli_st->dd_mtx) {
			sprintf(env->output, "\ndd_mtx is NULL\n");
		} else {
			sprintf(env->output, "dd_ref_cnt   : %x\n",
				cli_st->dd_mtx->dd_ref_cnt);
			logMsg(env);
			sprintf(env->output, "mtx_ref_cnt: %x\n",
				cli_st->dd_mtx->mtx_ref_cnt);
			logMsg(env);
			sprintf(env->output, "init_done : %x\n",
				cli_st->dd_mtx->init_done);
		};
		shm_unlink(cli_st->dd_mtx_fn);
	} else {
		sprintf(env->output, "\nFreeing dd, current state:\n");
		logMsg(env);
		if (NULL == cli_st->dd) {
			sprintf(env->output, "\ndd is NULL\n");
		} else {
			sprintf(env->output, "dd_ref_cnt   : %x\n",
				cli_st->dd_mtx->dd_ref_cnt);
			logMsg(env);
			sprintf(env->output, "mtx_ref_cnt: %x\n",
				cli_st->dd_mtx->mtx_ref_cnt);
			logMsg(env);
			sprintf(env->output, "init_done : %x\n",
				cli_st->dd_mtx->init_done);
		};
		shm_unlink(cli_st->dd_fn);
	};
exit:
	logMsg(env);
	return 0;
};

const struct cli_cmd CLIClean = {
(char *)"clean",
3,
0,
(char *)"Drops shared memory blocks.",
(char *)"No parms drops sm block, any part drops mutex.",
CLICleanCmd,
ATTR_NONE
};

const struct cli_cmd *sm_cmds[4] = 
	{&CLIChgCnt, 
	 &CLIIncCnt,
	 &CLIDevDir,
	 &CLIClean };

void bind_dd_cmds(struct fmd_state *st)
{
	cli_st = st;
	add_commands_to_cmd_db(4, &sm_cmds[0]);
}

#ifdef __cplusplus
}
#endif
