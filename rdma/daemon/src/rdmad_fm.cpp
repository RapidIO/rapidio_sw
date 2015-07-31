#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#include <algorithm>
#include <vector>
#include <set>

using namespace std;
#include "liblog.h"
#include "libfmdd.h"
#include "rdmad_clnt_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

static sem_t fm_started;
uint32_t fm_alive;
static uint32_t fm_must_die;
static pthread_t fm_thread;
static fmdd_h dd_h;

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
		pthread_exit(0);
	}

	/* FM thread is up and running */
	sem_post(&fm_started);

	do {
		/* Get fresh list of dids */
		if (fmdd_get_did_list(dd_h, &new_did_list_size, &new_did_list)) {
			CRIT("Failed to get device ID list from FM. Exiting.\n");
			break;
		}
		INFO("Fabman reported %u remote endpoints\n", new_did_list_size);
		sort(new_did_list, new_did_list + new_did_list_size);

		/* Determine which dids are new since last time */
		vector<uint32_t> result(new_did_list_size);
		vector<uint32_t>::iterator end_result = set_difference(
				new_did_list, new_did_list + new_did_list_size,
				old_did_list, old_did_list + old_did_list_size,
				begin(result));
		result.resize(end_result - begin(result));
		INFO("%d new endpoints detected\n", result.size());
		fmdd_free_did_list(dd_h, &new_did_list);

		/* Save a copy of the current list for comparison next time */
		if (old_did_list == NULL)
			fmdd_free_did_list(dd_h, &old_did_list);
		if (fmdd_get_did_list(dd_h, &old_did_list_size, &old_did_list)) {
			CRIT("Failed to get device ID list from FM. Exiting.\n");
			break;
		}
		sort(old_did_list, old_did_list + old_did_list_size);

		/* Provision new dids */
		for (uint32_t& did : result) {
			INFO("Provisioning 0x%X\n", did);
			/* Check if daemon is running */
			if (fmdd_check_did(dd_h, did, FMDD_RDMA_FLAG)) {
				/* SEND HELLO */
				if (provision_rdaemon(did)) {
					CRIT("Fail to provision destid(0x%X)\n",
									did);
				} else {
					HIGH("Provisioned desitd(0x%X)\n", did);
				}
			} else {
				/* If the daemon isn't running and we can't provision the
				 * did, then it must NOT be in the old did list so that next time
				 * around it gets provisioned.
				 */
				WARN("FM daemon is not running on did(0x%X)\n", did);
				remove(old_did_list, old_did_list + old_did_list_size, did);
				old_did_list_size--;
			}
		}

		/* Loop again only if Fabric Management reports change */
		DBG("Waiting for FM to report change...\n");
		if (fmdd_wait_for_dd_change(dd_h)) {
			ERR("Failed in fmdd_wait_for_dd_change(). Exiting\n");
			break;
		}
	} while (!fm_must_die);

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

        ret = pthread_create(&fm_thread, NULL, fm_loop, NULL);
        if (ret) {
                CRIT("Error - fm_thread rc: %d\n", ret);
        } else {
		sem_wait(&fm_started);
        }

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
