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

#include "fmd_dd.h"
#include "liblog.h"
#include "dev_db.h"
#include "riocp_pe_internal.h"
#include "cli_cmd_db.h"
#include "cli_cmd_line.h"
#include "cli_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

int fmd_dd_open_rw(char *dd_fn, int *dd_fd, struct fmd_dd **dd, 
					struct fmd_dd_mtx *dd_mtx)
{
	int rc;

        *dd_fd = shm_open(dd_fn, O_RDWR | O_CREAT | O_EXCL, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (-1 == *dd_fd) {
		if (EEXIST == errno ) {
        		*dd_fd = shm_open(dd_fn, O_RDWR, 
                        		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
	};

        if (-1 == *dd_fd) {
        	fprintf( stderr, "rw shm_open failed: 0x%x %s\n",
            		errno, strerror( errno ) );
                goto exit;
	}

        rc = ftruncate(*dd_fd, sizeof(struct fmd_dd));
        if (-1 == rc) {
        	fprintf(stderr, "rw ftruncate failed: 0x%x %s\n",
            		errno, strerror(errno));
                shm_unlink(dd_fn);
                goto exit;
        };

        *dd = (fmd_dd *)
		mmap(NULL, sizeof(struct fmd_dd), PROT_READ|PROT_WRITE,
                MAP_SHARED, *dd_fd, 0);

        if (MAP_FAILED == *dd) {
        	fprintf(stderr, "rw mmap failed:0x%x %s\n",
            		errno, strerror(errno));
                *dd = NULL;
                shm_unlink(dd_fn);
                goto exit;
        };

	if ((NULL != *dd) && (NULL != dd_mtx))
		if ((*dd)->chg_idx && dd_mtx->dd_ref_cnt)
			dd_mtx->dd_ref_cnt++;
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

	if ((*dd_mtx)->dd_ref_cnt && (*dd_mtx)->init_done) {
		(*dd_mtx)->mtx_ref_cnt++;
	} else {
		sem_init(&(*dd_mtx)->sem, 1, 1);
		(*dd_mtx)->mtx_ref_cnt = 1;
		(*dd_mtx)->init_done = TRUE;
	};
	return 0;
fail:
	return -1;
};

int fmd_dd_init(char *dd_mtx_fn, int *dd_mtx_fd, struct fmd_dd_mtx **dd_mtx,
		char *dd_fn, int *dd_fd, struct fmd_dd **dd)
{
	int rc;
	int idx;


       	rc = fmd_dd_mtx_open(dd_mtx_fn, dd_mtx_fd, dd_mtx);
	if (rc)
		goto fail;
	rc = fmd_dd_open_rw(dd_fn, dd_fd, dd, *dd_mtx);
	if (rc)
		goto fail;

	/* Previously created dd, add reference, do not initialize */
	if ((*dd)->chg_idx && (*dd_mtx)->dd_ref_cnt) {
		(*dd_mtx)->dd_ref_cnt++;
		goto exit;
	};

	(*dd_mtx)->dd_ref_cnt = 1;
	(*dd)->chg_idx = 0;
	(*dd)->md_ct = 0;
	(*dd)->num_devs = 0;
	for (idx = 0; idx < FMD_MAX_DEVS; idx++) {
		(*dd)->devs[idx].ct = 0;
		(*dd)->devs[idx].destID = 0;
		(*dd)->devs[idx].hc = 0xFF;
	};
	fmd_dd_incr_chg_idx(*dd, 1);
exit:
	return 0;
fail:
	return -1;
};

void fmd_dd_cleanup(char *dd_mtx_fn, int *dd_mtx_fd,
                        struct fmd_dd_mtx **dd_mtx_p,
                        char *dd_fn, int *dd_fd, struct fmd_dd **dd_p,
			int dd_rw)
{
	struct fmd_dd_mtx *dd_mtx = (NULL == dd_mtx_p)?NULL:*dd_mtx_p;
	struct fmd_dd *dd = (NULL == dd_p)?NULL:*dd_p;

	if ((NULL != dd) && (NULL != dd_mtx)) {
		if (dd->chg_idx && dd_mtx->dd_ref_cnt) {
			if (!--dd_mtx->dd_ref_cnt) {
				if (dd_rw)
					dd->chg_idx = 0;
				shm_unlink(dd_fn);
			};
		}
		if (*dd_fd) {
			close(*dd_fd);
			*dd_fd = 0;
		};
		*dd_p = NULL;
	};

	if (NULL != dd_mtx) {
		if (dd_mtx->init_done && dd_mtx->mtx_ref_cnt) {
			if (!--dd_mtx->mtx_ref_cnt) {
				sem_destroy(&dd_mtx->sem);
				dd_mtx->init_done = FALSE;
				shm_unlink(dd_mtx_fn);
			};
		};
		if (*dd_mtx_fd) {
			close(*dd_mtx_fd);
			*dd_mtx_fd = 0;
		};
		*dd_mtx_p = NULL;
	};
};
	
void fmd_dd_update(riocp_pe_handle mp_h, struct fmd_dd *dd,
                        struct fmd_dd_mtx *dd_mtx)
{
        size_t pe_cnt;
        riocp_pe_handle *pe;
        int rc, idx;
        uint32_t comptag, destid;
        uint8_t hopcount;

	if (NULL == mp_h) {
                WARN("\nMaster port is NULL, device directory not updated\n");
		goto exit;
	};

        rc = riocp_mport_get_pe_list(mp_h, &pe_cnt, &pe);
        if (rc) {
                CRIT("\nCannot get pe list rc %d...\n", rc);
		goto exit;
        };

	if (pe_cnt > FMD_MAX_DEVS) {
                WARN("\nToo many PEs for DD %d %d...\n", pe_cnt, FMD_MAX_DEVS);
		pe_cnt = FMD_MAX_DEVS;
	};

	sem_wait(&dd_mtx->sem);
	dd->num_devs = 0;

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
	
		dd->devs[dd->num_devs].ct     = comptag;
		dd->devs[dd->num_devs].destID = destid;
		dd->devs[dd->num_devs].hc     = hopcount;
		dd->num_devs++;
	};
	fmd_dd_incr_chg_idx(dd, 1);
	sem_post(&dd_mtx->sem);
exit:
	return;
};

void fmd_dd_incr_chg_idx(struct fmd_dd *dd, int dd_rw)
{
	if ((NULL != dd) && dd_rw) {
		uint32_t next_idx = dd->chg_idx+1;
		if (!next_idx)
			next_idx = 1;
		dd->chg_idx = next_idx;
		clock_gettime(CLOCK_REALTIME, &dd->chg_time);
	};
};

uint32_t fmd_dd_get_chg_idx(struct fmd_dd *dd)
{
	if (NULL != dd)
		return dd->chg_idx;

	return 0;
};

/* Note that get_first_dev and get_next_dev will block until
 * enumeration has been completed.
 */

uint32_t fmd_dd_atomic_copy(struct fmd_dd *dd,
                        struct fmd_dd_mtx *dd_mtx,
			uint32_t *num_devs,
			struct fmd_dd_dev_info *devs,
			uint32_t max_devs)
{
	uint32_t idx;

	sem_wait(&dd_mtx->sem);
	*num_devs = dd->num_devs;
	if (*num_devs > max_devs)
		*num_devs = max_devs;
	for (idx = 0; idx < *num_devs; idx++) 
		devs[idx] = dd->devs[idx];
	sem_post(&dd_mtx->sem);

	return *num_devs;
};

#ifdef __cplusplus
}
#endif