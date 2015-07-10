#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#include "liblog.h"
#include "libfmdd.h"
#include "rdmad_clnt_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

static sem_t fm_started;
//static csem_t fm_update;
uint32_t fm_alive;
static uint32_t fm_must_die;
static pthread_t fm_thread;
static fmdd_h dd_h;

/* Sends requests and responses to all apps */
void *fm_loop(void *unused)
{
	uint32_t did_list_sz;
	uint32_t *did_list;
	uint32_t rc;

	dd_h = fmdd_get_handle((char *)"RDMAD", FMDD_RDMA_FLAG);
	fmdd_bind_dbg_cmds(dd_h);

	if (dd_h != NULL) {
		fm_alive = 1;
		HIGH("FM is alive\n");
	}

	/* FM thread is up and running */
	sem_post(&fm_started);

	do {
		rc = fmdd_get_did_list(dd_h, &did_list_sz, &did_list);
		if (rc) {
			CRIT("Failed to get device ID list from FM. Exiting.\n");
			goto exit;
		}
		INFO("Fabman reported %u remote endpoints\n", did_list_sz);
		for (unsigned i = 0; i < did_list_sz; i++) {
			INFO("Provisioning 0x%X\n", did_list[i]);
			/* Check if daemon is running */
			if (fmdd_check_did(dd_h, did_list[i], FMDD_RDMA_FLAG)) {
				/* SEND HELLO */
				if (provision_rdaemon(did_list[i])) {
					CRIT("Failed to provision daemon at destid(0x%X)\n",
							did_list[i]);
				} else {
					HIGH("Provisioned desitd(0x%X)\n", did_list[i]);
				}
			}
		}
		fmdd_free_did_list(dd_h, &did_list);

		if (fmdd_wait_for_dd_change(dd_h)) {
			ERR("Failed in fmdd_wait_for_dd_change()\n");
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
