/* Implementation of the RDMA Socket Daemon fabric management thread */
/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "librsktd.h"
#include "librdma.h"
#include "librsktd.h"
#include "librsktd_sn.h"
#include "librsktd_dmn.h"
#include "librsktd_lib_info.h"
#include "librsktd_msg_proc.h"
#include "librsktd_wpeer.h"
#include "liblist.h"
#include "librsktd_fm.h"
#include "libfmdd.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

sem_t fm_started;
sem_t fm_update;
uint32_t fm_alive;
uint32_t fm_must_die;
int fm_thread_valid;
pthread_t fm_thread;
fmdd_h dd_h;

/* Sends requests and responses to all apps */
void *fm_loop(void *unused)
{
	uint32_t did_list_sz;
	uint32_t *did_list;
	uint32_t rc;

	HIGH("ENTER\n");
	dd_h = fmdd_get_handle((char *)"RSKTD", FMDD_RSKT_FLAG);
	if (dd_h == NULL ) {
		CRIT("Cannot obtain dd_h. Exiting\n");
		sem_post(&fm_started);	/* Don't block main thread */
		pthread_exit(0);
	}

	fmdd_bind_dbg_cmds(dd_h);

	if (NULL != dd_h) {
		fm_alive = 1;
		INFO("FM is alive!\n");
	}
	sem_post(&fm_started);
	
	do {
		rc = fmdd_get_did_list(dd_h, &did_list_sz, &did_list);
		if (rc) {
			CRIT("Failed to get DID list from FM.\n");
			goto exit;
		}
		update_wpeer_list(did_list_sz, did_list);
		fmdd_free_did_list(dd_h, &did_list);

		INFO("Waiting for FM to report change to RSKTD\n");
		if (fmdd_wait_for_dd_change(dd_h)) {
			ERR("fmdd_wait_for_dd_change\n");
			break;
		}
	} while (!fm_must_die && (NULL != dd_h));
exit:
	fm_alive = 0;
	fmdd_destroy_handle(&dd_h);
	pthread_exit(unused);
};

int start_fm_thread(void)
{
	int ret;

        /* Prepare and start library connection handling threads */

        sem_init(&fm_started, 0, 0);
	fm_alive = 0;
	fm_must_die = 0;
	fm_thread_valid = 0;

        ret = pthread_create(&fm_thread, NULL, fm_loop, NULL);
        if (ret)
                printf("Error - fm_thread rc: %d\n", ret);
	else {
		sem_wait(&fm_started);
		fm_thread_valid = 1;
	};

	return ret;
};

void halt_fm_thread(void)
{
	fm_must_die = 1;
	fmdd_destroy_handle(&dd_h);
	if (fm_thread_valid) {
		fm_thread_valid  = 0;
		pthread_kill(fm_thread, SIGHUP);
		pthread_join(fm_thread, NULL);
	};
};

#ifdef __cplusplus
}
#endif
