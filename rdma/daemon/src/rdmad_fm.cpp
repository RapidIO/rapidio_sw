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
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#include <algorithm>
#include <vector>
#include <set>

#include "liblog.h"
#include "libfmdd.h"
#include "rdmad_mspace.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_main.h"
#include "rdmad_clnt_threads.h"

using std::vector;
using std::set;
using std::remove;
using std::remove_if;
using std::sort;
using std::copy;

#ifdef __cplusplus
extern "C" {
#endif

static sem_t fm_started;
uint32_t fm_alive;
static uint32_t fm_must_die;
static pthread_t fm_thread;
static fmdd_h dd_h;

/**
 * @brief	Given an old DID list and a new DID list, do a set difference
 * 		(new - old) to determine new nodes that need provisioning.
 * 		However, only provision a new node if there is a daemon
 * 		running on it. Otherwise, remove the DID from the new list
 * 		(which will become the old list next time around), so it
 * 		re-appears again next time there is a change (daemon started)
 * 		and we provision it then
 */
static int provision_new_dids(uint32_t old_did_list_size,
			      uint32_t *old_did_list,
			      uint32_t *new_did_list_size,
			      uint32_t *new_did_list)
{
	int rc = 0;

	/* Determine which dids are new since last time */
	vector<uint32_t> result(*new_did_list_size);
	vector<uint32_t>::iterator end_result = set_difference(
			new_did_list, new_did_list + *new_did_list_size,
			old_did_list, old_did_list + old_did_list_size,
			begin(result));
	result.resize(end_result - begin(result));
	INFO("%u new endpoints detected\n", result.size());

	/* Provision new dids */
	for (uint32_t& did : result) {
		/* Check if daemon is running */
		if (fmdd_check_did(dd_h, did, FMDD_RDMA_FLAG)) {
			/* SEND HELLO */
			INFO("Provisioning 0x%X\n", did);
			rc = provision_rdaemon(did);
			if (rc) {
				CRIT("Fail to provision destid(0x%X)\n", did);
			} else {
				HIGH("Provisioned destid(0x%X)\n", did);
			}
		} else {
			INFO("NOT provisioning 0x%X since daemon not running\n", did);
			/* Remove it from list since we need to attempt to
			 * provision it again when the daemon is actually
			 * up and running.
			 * 'remove' will put the 'did' at the end of the list,
			 * then by decrementing the size, it is off the list. */
			remove(new_did_list, new_did_list + *new_did_list_size, did);
			(*new_did_list_size)--;
			rc = 0;	/* Not an error; daemon not started */
		}
	}

	return rc;
} /* provision_new_dids() */

/**
 * @brief If daemon isn't running on 'did' return true, otherwise
 *	  return false;
 */
bool daemon_not_running(uint32_t did)
{
 	if (!fmdd_check_did(dd_h, did, FMDD_RDMA_FLAG)) {
		return true;
	} else {
		return false;
	}
} /* daemon_not_running() */

/**
 * @brief Call daemon_not_running on every element of the current 'did'
 * list. If daemon_no_running() returns 'true' then remove the
 * 'did' from the current list.
 */
static void validate_remote_daemons(uint32_t *old_did_list_size,
 		  	  	    uint32_t *old_did_list)
{
	DBG("*old_did_list_size = %u\n", *old_did_list_size);
	uint32_t *old_did_list_end = remove_if(old_did_list,
			      old_did_list + *old_did_list_size,
			      daemon_not_running);
	*old_did_list_size = old_did_list_end - old_did_list;
	DBG("Now *old_did_list_size = %u\n", *old_did_list_size);
} /* validate_remove_daemons() */

/* Sends requests and responses to all apps */
void *fm_loop(void *unused)
{
	uint32_t old_did_list_size = 0;
	uint32_t *old_did_list = NULL;
	uint32_t new_did_list_size = 0;
	uint32_t *new_did_list = NULL;

	dd_h = fmdd_get_handle((char *)"RDMAD", FMDD_RDMA_FLAG);

	if (dd_h != NULL) {
		fmdd_bind_dbg_cmds(dd_h);
		fm_alive = 1;
		HIGH("FM is alive\n");
	} else {
		CRIT("Cannot obtain dd_h. Exiting\n");
		sem_post(&fm_started);	/* Don't block main thread */
		pthread_exit(0);
	}

	/* FM thread is up and running */
	sem_post(&fm_started);

	do {
		/* Get fresh list of dids */
		if (new_did_list != NULL)
			fmdd_free_did_list(dd_h, &new_did_list);
		if (fmdd_get_did_list(dd_h, &new_did_list_size, &new_did_list)) {
			CRIT("Failed to get device ID list from FM. Exiting.\n");
			break;
		}
		INFO("Fabman reported %u remote endpoints\n", new_did_list_size);
		sort(new_did_list, new_did_list + new_did_list_size);

		/* Provision new DIDs, if any */
		provision_new_dids(old_did_list_size,
				   old_did_list,
				   &new_did_list_size,
				   new_did_list);

		/* Need to check for dead daemons only to remove them from
		 * the old_did_list */
		validate_remote_daemons(&old_did_list_size, old_did_list);

		/* Save a copy of the current list for comparison next time */
		if (old_did_list != NULL)	/* Delete if ncessary */
			delete old_did_list;
		old_did_list_size = new_did_list_size;	/* Same size */
		old_did_list = new uint32_t[old_did_list_size];
		copy(new_did_list, new_did_list + new_did_list_size, old_did_list);
		sort(old_did_list, old_did_list + old_did_list_size);
		DBG("old_did_list_size = %u\n", old_did_list_size);

		/* Loop again only if Fabric Management reports change */
		DBG("Waiting for FM to report change...\n");
		if (fmdd_wait_for_dd_change(dd_h)) {
			ERR("Failed in fmdd_wait_for_dd_change(). Exiting\n");
			break;
		}
	} while (!fm_must_die);

	/* Clean up */
	if (old_did_list != NULL)
		delete old_did_list;
        if (new_did_list != NULL)
        	fmdd_free_did_list(dd_h, &new_did_list);

	fm_alive = 0;
	fmdd_destroy_handle(&dd_h);
	pthread_exit(unused);
};

int start_fm_thread(void)
{
	int ret;

        /* Prepare and start library connection handling threads */
	HIGH("ENTER\n");
        sem_init(&fm_started, 0, 0);
	fm_alive = 0;
	fm_must_die = 0;

        ret = pthread_create(&fm_thread, NULL, fm_loop, NULL);
        if (ret) {
                CRIT("Error - fm_thread rc: %d\n", ret);
        } else {
        	HIGH("Waiting for fm_thread to be up and running\n");
		sem_wait(&fm_started);
        }

        HIGH("EXIT\n");
	return ret;
};

void halt_fm_thread(void)
{
	fm_must_die = 1;
	fmdd_destroy_handle(&dd_h);
	pthread_kill(fm_thread, SIGHUP);
	pthread_join(fm_thread, NULL);
};

#ifdef __cplusplus
}
#endif
