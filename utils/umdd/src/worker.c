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
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>

#include <pthread.h>
#include <sstream>

#include <sched.h>


#include "libcli.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "rapidio_mport_dma.h"
#include "liblog.h"

#include "time_utils.h"
#include "worker.h"
#include "goodput.h"
#include "mhz.h"

#ifdef USER_MODE_DRIVER
#include "dmachan.h"
#include "lockfile.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

void dma_free_ibwin(struct worker *info);

void init_worker_info(struct worker *info, int first_time)
{
	if (first_time) {
        	sem_init(&info->started, 0, 0);
        	sem_init(&info->run, 0, 0);
	};

	info->stat = 0;
	info->stop_req = 0;
	info->wkr_thr.cpu_req = -1;
	info->wkr_thr.cpu_run = -1;
	info->action = no_action;
	info->action_mode = kernel_action;
	info->did = -1;
	

        info->rio_addr = 0;
        info->byte_cnt = 0;
        info->acc_size = 0; /* Bytes per transfer for direct IO and DMA */
	info->max_iter = -1;
        info->wr = 0;
        info->mp_h_is_mine = 0;
        info->mp_h = NULL;

        info->ob_valid = 0;
        info->ob_handle = 0;
        info->ob_byte_cnt = 0;

        info->ib_valid = 0;
	info->ib_handle = 0;
        info->ib_rio_addr = 0;
        info->ib_byte_cnt = 0;
	info->ib_ptr = NULL;

        info->data8_tx = 0x12;
        info->data16_tx= 0x3456;
        info->data32_tx = 0x789abcde;
        info->data64_tx = 0xf123456789abcdef;

        info->data8_rx = 0;
        info->data16_rx = 0;
        info->data32_rx = 0;
        info->data64_rx = 0;

	info->use_kbuf = 0;
	info->dma_trans_type = RIO_DIRECTIO_TYPE_NWRITE;
	info->dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;
	info->rdma_kbuff = 0;
	info->rdma_ptr = NULL;

        info-> mb_valid = 0;
        info->acc_skt = NULL;
        info->acc_skt_valid = 0;
        info->con_skt = NULL;
        info->con_skt_valid = 0;
        info->sock_num = 0; 
        info->sock_tx_buf = NULL;
        info->sock_rx_buf = NULL;

#ifdef USER_MODE_DRIVER
	info->owner_func = NULL;
	info->umd_set_rx_fd = NULL;
	info->my_destid = 0xFFFF;
	info->umd_chan = -1;
	info->umd_dch = NULL;
	info->umd_tx_rtype = NREAD;
	info->umd_tx_buf_cnt = 0;
	info->umd_sts_entries = 0;
	info->umd_tx_iter_cnt = 0;
	info->umd_fifo_thr.cpu_req = -1;
	info->umd_fifo_thr.cpu_run = -1;
	info->umd_fifo_proc_alive = 0;
	info->umd_fifo_proc_must_die = 0;
	info->umd_dma_abort_reason = 0;

	//if (first_time) {
        	sem_init(&info->umd_fifo_proc_started, 0, 0);
	//};
	init_seq_ts(&info->desc_ts);
	init_seq_ts(&info->fifo_ts);
	init_seq_ts(&info->meas_ts);
#endif
};

void shutdown_worker_thread(struct worker *info)
{
	int rc;
	if (info->stat) {
		info->action = shutdown_worker;
		info->stop_req = 2;
		sem_post(&info->run);
#ifdef USER_MODE_DRIVER
		info->umd_fifo_proc_must_die = 1;
		if (info->umd_dch != NULL) info->umd_dch->shutdown();
#endif
		pthread_join(info->wkr_thr.thr, NULL);
	};
		
	dma_free_ibwin(info);

#ifdef USER_MODE_DRIVER
	if (info->umd_fifo_proc_alive) {
		info->umd_fifo_proc_must_die = 1;
		pthread_join(info->umd_fifo_thr.thr, NULL);
		info->umd_fifo_proc_must_die = 0;
		info->umd_fifo_proc_alive = 0;
	};

	if (info->umd_dch) {
		int i;

		for (i = 0; i < MAX_UMD_BUF_COUNT; i++)
			info->umd_dch->free_dmamem(info->dmamem[i]);

		delete info->umd_dch;
		info->umd_dch = NULL;
	};
#endif

	if (info->mp_h_is_mine) {
		rc = riomp_mgmt_mport_destroy_handle(&info->mp_h);
		info->mp_h_is_mine = 0;
		if (rc) {
			ERR("Shutdown riomp_mgmt_mport_destroy_handle rc %d:%s\n",
					rc, strerror(errno));
		};
	};
	
	init_worker_info(info, 0);
};

int getCPUCount()
{
	FILE* f = fopen("/proc/cpuinfo", "rt");

	int count = 0;
	while (! feof(f)) {
		char buf[257] = {0};
		fgets(buf, 256, f);
		if (buf[0] == '\0') break;
		if (strstr(buf, "processor\t:")) count++;
	}

	fclose(f);

	return count;
}

int migrate_thread_to_cpu(struct thread_cpu *info)
{
        cpu_set_t cpuset;
        int chk_cpu_lim = 10;
	int rc;

	const int cpu_count = getCPUCount();

	if (-1 == info->cpu_req) {
        	CPU_ZERO(&cpuset);

		for(int c = 0; c < cpu_count; c++) CPU_SET(c, &cpuset);
	} else {
		if (info->cpu_req >= cpu_count) {
			ERR("\n\tInvalid cpu %d cpu count is %d\n", info->cpu_req, cpu_count);
			return 1;
		}
        	CPU_ZERO(&cpuset);
        	CPU_SET(info->cpu_req, &cpuset);
	};

        rc = pthread_setaffinity_np(info->thr, sizeof(cpu_set_t), &cpuset);
	if (rc) {
		ERR("pthread_setaffinity_np rc %d:%s\n",
					rc, strerror(errno));
                return 1;
	};

	if (-1 == info->cpu_req) {
		info->cpu_run = info->cpu_req;
		return 0;
	};
		
        rc = pthread_getaffinity_np(info->thr, sizeof(cpu_set_t), &cpuset);
	if (rc) {
		ERR("pthread_getaffinity_np rc %d:%s\n",
					rc, strerror(errno));
                return 1;
	};

        info->cpu_run = sched_getcpu();
        while ((info->cpu_run != info->cpu_req) && chk_cpu_lim) {
                usleep(1);
                info->cpu_run = sched_getcpu();
                chk_cpu_lim--;
        };
	rc = info->cpu_run != info->cpu_req;
	if (rc) {
		ERR("Unable to schedule thread on cpu %d\n", info->cpu_req);
                return 1;
	};
	return rc;
};

void zero_stats(struct worker *info)
{
	info->perf_msg_cnt = 0;
	info->perf_byte_cnt = 0;
	info->perf_iter_cnt = 0;
	info->min_iter_time = {0,0};
	info->tot_iter_time = {0,0};
	info->max_iter_time = {0,0};

        info->data8_tx = 0x12;
        info->data16_tx= 0x3456;
        info->data32_tx = 0x789abcde;
        info->data64_tx = 0xf123456789abcdef;

        info->data8_rx = 0;
        info->data16_rx = 0;
        info->data32_rx = 0;
        info->data64_rx = 0;
};

void start_iter_stats(struct worker *info)
{
	clock_gettime(CLOCK_MONOTONIC, &info->iter_st_time);
};

void finish_iter_stats(struct worker *info)
{
	clock_gettime(CLOCK_MONOTONIC, &info->iter_end_time);
	time_track(info->perf_iter_cnt, 
		info->iter_st_time, info->iter_end_time,
		&info->tot_iter_time, &info->min_iter_time,
		&info->max_iter_time);
	info->perf_iter_cnt++;	
};

#define ADDR_L(x,y) ((uint64_t)((uint64_t)x + (uint64_t)y))
#define ADDR_P(x,y) ((void *)((uint64_t)x + (uint64_t)y))


int alloc_dma_tx_buffer(struct worker *info)
{
	int rc = 0;

	if (info->use_kbuf) {
		rc = riomp_dma_dbuf_alloc(info->mp_h, info->rdma_buff_size,
					&info->rdma_kbuff);
		if (rc) {
			ERR("FAILED: riomp_dma_dbuf_alloc rc %d:%s\n",
						rc, strerror(errno));
			goto exit;;
		};
		info->rdma_ptr = NULL;
		rc = riomp_dma_map_memory(info->mp_h, info->rdma_buff_size,
					info->rdma_kbuff, &info->rdma_ptr);
		if (rc) {
			ERR("FAILED: riomp_dma_map_memory rc %d:%s\n",
						rc, strerror(errno));
			goto exit;;
		};
	} else {
		info->rdma_ptr = malloc(info->rdma_buff_size);
		if (NULL == info->rdma_ptr) {
			rc = 1;
			ERR("FAILED: Could not allocate local memory!\n");
			goto exit;
		}
	};

	return 0;
exit:
	return rc;
};

void dealloc_dma_tx_buffer(struct worker *info)
{
	if (info->use_kbuf) {
		if (NULL != info->rdma_ptr) {
			riomp_dma_unmap_memory(info->mp_h, info->rdma_buff_size,
				info->rdma_ptr);
			info->rdma_ptr = NULL;
		};
		if (info->rdma_kbuff) {
			riomp_dma_dbuf_free(info->mp_h, &info->rdma_kbuff);
			info->rdma_kbuff = 0;
		};
	} else {
		if (NULL != info->rdma_ptr) {
			free(info->rdma_ptr);
			info->rdma_ptr = NULL;
		};
	};
};

void dma_alloc_ibwin(struct worker *info)
{
	uint64_t i;
	int rc;

	if (!info->ib_byte_cnt || info->ib_valid) {
		ERR("FAILED: window size of 0 or ibwin already exists\n");
		return; 
	};

	info->ib_rio_addr = (uint64_t)(~((uint64_t) 0)); /* RIO_MAP_ANY_ADDR */
	rc = riomp_dma_ibwin_map(info->mp_h, &info->ib_rio_addr,
					info->ib_byte_cnt, &info->ib_handle);
	if (rc) {
		ERR("FAILED: riomp_dma_ibwin_map rc %d:%s\n",
					rc, strerror(errno));
		return;
	};

	rc = riomp_dma_map_memory(info->mp_h, info->ib_byte_cnt, 
					info->ib_handle, &info->ib_ptr);
	if (rc) {
		ERR("FAILED: riomp_dma_ibwin_map rc %d:%s\n",
					rc, strerror(errno));
		return;
	};

	for (i = 0; i < info->ib_byte_cnt; i += 8) {
		uint64_t *d_ptr;

		d_ptr = (uint64_t *)((uint64_t)info->ib_ptr + i);
		*d_ptr = i + (i << 32);
	};

	info->ib_valid = 1;
};

void dma_free_ibwin(struct worker *info)
{
	int rc;
	if (!info->ib_valid)
		return; 

	if (info->ib_ptr && info->ib_valid) {
		rc = riomp_dma_unmap_memory(info->mp_h, info->ib_byte_cnt, 
								info->ib_ptr);
		info->ib_ptr = NULL;
		if (rc)
			ERR("riomp_dma_unmap_memory ib rc %d: %s\n",
				rc, strerror(errno));
	};

	rc = riomp_dma_ibwin_free(info->mp_h, &info->ib_handle);
	if (rc) {
		ERR("FAILED: riomp_dma_ibwin_free rc %d:%s\n",
					rc, strerror(errno));
		return;
	};

	info->ib_valid = 0;
	info->ib_rio_addr = 0;
	info->ib_byte_cnt = 0;
	info->ib_handle = 0;
};

#ifdef USER_MODE_DRIVER

typedef struct {
	volatile uint64_t fifo_count_scanfifo;
	volatile uint64_t fifo_deltats_scanfifo;
	volatile uint64_t fifo_count_other;
	volatile uint64_t fifo_deltats_other;
	volatile uint64_t fifo_deltats_all;

	volatile uint64_t fifo_thr_iter;
} FifoStats_t;

FifoStats_t g_FifoStats[8];

#ifndef PAGE_4K
  #define PAGE_4K	4096
#endif

extern char* dma_rtype_str[];

void *umd_dma_fifo_proc_thr(void *parm)
{
	struct worker* info = NULL;

	if (NULL == parm)
		goto exit;

	info = (struct worker *)parm;
	if (NULL == info->umd_dch)
		goto exit;
	
	DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; 
	memset(wi, 0, sizeof(wi));

	migrate_thread_to_cpu(&info->umd_fifo_thr);

	if (info->umd_fifo_thr.cpu_req != info->umd_fifo_thr.cpu_req) {
		CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing ou!\n",
		     info->umd_fifo_thr.cpu_req, info->umd_fifo_thr.cpu_req);
		goto exit;
	}

	info->umd_fifo_proc_alive = 1;
	sem_post(&info->umd_fifo_proc_started); 

	while (!info->umd_fifo_proc_must_die) {
		if (info->umd_dch->isSim()) info->umd_dch->simFIFO(0, 0); // no faults injected

		const int cnt = info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8);
		if (!cnt) 
			continue;

		get_seq_ts(&info->fifo_ts);

		for (int i = 0; i < cnt; i++) {
			DMAChannel::WorkItem_t& item = wi[i];

			switch (item.opt.dtype) {
			case DTYPE1:
			case DTYPE2:
				info->perf_byte_cnt += info->acc_size;
				break;
			case DTYPE3:
				break;
			default:
				ERR("\n\tUNKNOWN BD %d bd_wp=%u\n",
					item.opt.dtype, item.opt.bd_wp);
				break;
      			}

			wi[i].valid = 0xdeadabba;
                } // END for WorkItem_t vector
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
	} // END while
	goto no_post;
exit:
	sem_post(&info->umd_fifo_proc_started); 
no_post:
	info->umd_fifo_proc_alive = 0;

	pthread_exit(parm);
};

/** \brief Dump UMD DMA Tun status
 *  * \note This executes within the CLI thread but targets Main Battle Tank thread's data
 *   */
extern "C"
void UMD_DD(struct worker* info)
{
        assert(info->umd_dch);

	//const int MHz = getCPUMHz();
	//const int idx = info->idx;

	DMAChannel::ShmClientCompl_t comp[DMAChannel::DMA_SHM_MAX_CLIENTS];
	memset(&comp, 0, sizeof(comp));

	info->umd_dch->listClients(&comp[0], sizeof(comp));
	for (int i = 0; i < DMAChannel::DMA_SHM_MAX_CLIENTS; i++) {
		if (! comp[i].busy) continue;
		INFO("\n\tpid=%d%s change_cnt=%llu bad_tik.{RP=%llu WP=%llu} NREAD_T2_res.{RP=%llu WP=%llu}\n",
		     comp[i].owner_pid, (kill(comp[i].owner_pid,0)? " DEAD": ""),
		     comp[i].change_cnt,
		     comp[i].bad_tik.RP, comp[i].bad_tik.WP,
		     comp[i].NREAD_T2_results.RP, comp[i].NREAD_T2_results.WP);
	}
}

void UMD_Test(const struct worker* wkr)
{
}

/** \brief Lock other processes out of this UMD module/channel
 * \note Due to POSIX locking semantics this has no effect on the current process
 * \note Using the same channel twice in this process will NOT be prevented
 * \parm[out] info info->umd_lock will be populated on success
 * \param[in] module DMA or Mbox, ASCII string
 * \param instance Channel number
 * \return true if lock was acquited, false if somebody else is using it
 */
bool TakeLock(struct worker* info, const char* module, int instance)
{
	if (info == NULL || module == NULL || module[0] == '\0' || instance < 0) return false;

	char lock_name[81] = {0};
	snprintf(lock_name, 80, "/var/lock/UMD-%s-%d..LCK", module, instance);
	try {
		info->umd_lock = new LockFile(lock_name);
	} catch(std::runtime_error ex) {
		CRIT("\n\tTaking lock %s failed: %s\n", lock_name, ex.what());
		return false;
	}
	// NOT catching std::logic_error
	return true;
}

/** \brief Check that the UMD worker and FIFO threads are not stuck to the same (isolcpu) core
 */
bool umd_check_cpu_allocation(struct worker *info)
{
	assert(info);

	if (GetEnv("IGNORE_CPUALLOC") != NULL) return true;

	if (info->wkr_thr.cpu_req != info->umd_fifo_thr.cpu_req) return true;

	if (info->wkr_thr.cpu_req == -1 /*|| info->umd_fifo_thr.cpu_req == -1*/) return true; // free to be scheduled

	if (getCPUCount() < 2) return true; // does not matter

	CRIT("\n\tWorker thread and FIFO thread request the same cpu (%d). Set env IGNORE_CPUALLOC to disable this check.\n", info->wkr_thr.cpu_req);

	return false;
}

void umd_shm_goodput_demo(struct worker *info)
{
	bool fifo_unwork_ACK = false;

	if (! umd_check_cpu_allocation(info)) return;
	if (! TakeLock(info, "DMA", info->umd_chan)) return;

	info->owner_func = umd_shm_goodput_demo;

        info->umd_dch = new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
        if (NULL == info->umd_dch) {
                CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
                goto exit;
        };

        if(info->umd_dch->getDestId() == info->did && GetEnv("FORCE_DESTID") == NULL) {
                CRIT("\n\tERROR: Testing against own desitd=%d. Set env FORCE_DESTID to disable this check.\n", info->did);
                goto exit;
        }

        if (!info->umd_dch->alloc_dmatxdesc(info->umd_tx_buf_cnt)) {
                CRIT("\n\talloc_dmatxdesc failed: bufs %d",
                                                        info->umd_tx_buf_cnt);
                goto exit;
        };
        if (!info->umd_dch->alloc_dmacompldesc(info->umd_sts_entries)) {
                CRIT("\n\talloc_dmacompldesc failed: entries %d",
                                                        info->umd_sts_entries);
                goto exit;
        };

	// NOT USED by SHM server
        memset(info->dmamem, 0, sizeof(info->dmamem));
        memset(info->dmaopt, 0, sizeof(info->dmaopt));

        info->umd_dch->resetHw();
        if (!info->umd_dch->checkPortOK()) {
                CRIT("\n\tPort is not OK!!! Exiting...");
                goto exit;
        };

	INFO("\n\tUMDd/SHM my_destid=%u destid=%u rioaddr=0x%x bcount=%d #buf=%d #fifo=%d\n",
		info->umd_dch->getDestId(),
		info->did, info->rio_addr, info->acc_size,
		info->umd_tx_buf_cnt, info->umd_sts_entries);

        DMAChannel::WorkItem_t wi[info->umd_sts_entries*8];
        memset(wi, 0, sizeof(wi));

	clock_gettime(CLOCK_MONOTONIC, &info->iter_st_time);
        while (!info->stop_req) {
                const int cnt = info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8);
                if (!cnt)
                        goto next;

		fifo_unwork_ACK = false;
                clock_gettime(CLOCK_MONOTONIC, &info->fifo_work_time);

                for (int i = 0; i < cnt; i++) {
                        DMAChannel::WorkItem_t& item = wi[i];

                        switch (item.opt.dtype) {
                        case DTYPE1:
                        case DTYPE2:
                                info->perf_byte_cnt += info->acc_size;
                                break;
                        case DTYPE3:
                                break;
                        default:
                                ERR("\n\tUNKNOWN BD %d bd_wp=%u\n",
                                        item.opt.dtype, item.opt.bd_wp);
                                break;
                        }

                        wi[i].valid = 0xdeadabba;
                } // END for WorkItem_t vector

	next:
                if (!cnt) clock_gettime(CLOCK_MONOTONIC, &info->iter_end_time);
                else      info->iter_end_time = info->fifo_work_time;

		bool check_abort = false;
		do {
			if (info->umd_dch->queueFull()) { check_abort = true; break; }

			struct timespec dT = time_difference(info->iter_end_time, info->iter_st_time);
			const uint64_t nsec = dT.tv_nsec + (dT.tv_sec * 1000000000);
			if (nsec > 10 * 1000000) { // Every 10 ms
				info->iter_st_time = info->iter_end_time;
				check_abort = true;
				break;
			}

			if (cnt) break;
			if (fifo_unwork_ACK) break; // No sense polling PCIe sensessly

			struct timespec dT_FIFO = time_difference(info->iter_end_time, info->fifo_work_time);
			const uint64_t nsec_FIFO = dT_FIFO.tv_nsec + (dT_FIFO.tv_sec * 1000000000);
			if (nsec_FIFO > 100 * 1000) { // Nothing popped in FIFO in the last 100 microsec
				fifo_unwork_ACK = true;
				check_abort = true;
				break;
			}
		} while(0);

		if (check_abort && info->umd_dch->dmaCheckAbort(info->umd_dma_abort_reason)) {
			CRIT("\n\tDMA abort 0x%x: %s. SOFT RESTART\n", info->umd_dma_abort_reason,
			     DMAChannel::abortReasonToStr(info->umd_dma_abort_reason));
			info->umd_dch->softRestart(true);
		}
        } // END while

        goto exit_nomsg;
exit:
        INFO("\n\tDMA %srunning after %d %dus polls\n",
                info->umd_dch->dmaIsRunning()? "": "NOT ",
                        info->perf_msg_cnt, DMA_RUNPOLL_US);

        INFO("\n\tEXITING (FIFO iter=%lu hw RP=%u WP=%u)\n",
                info->umd_dch->m_fifo_scan_cnt,
                info->umd_dch->getFIFOReadCount(),
                info->umd_dch->getFIFOWriteCount());
exit_nomsg:
        if (info->umd_dch)
                info->umd_dch->shutdown();

        delete info->umd_dch; info->umd_dch = NULL;
        delete info->umd_lock; info->umd_lock = NULL;
}

static const uint8_t PATTERN[] = { 0xa1, 0xa2, 0xa3, 0xa4, 0xa4, 0xa6, 0xaf, 0xa8 };
#define PATTERN_SZ      sizeof(PATTERN)
#define DMA_RUNPOLL_US 10

void umd_dma_goodput_demo(struct worker *info)
{
	int oi = 0, rc;
	uint64_t cnt = 0;
	int iter = 0;

	if (! umd_check_cpu_allocation(info)) return;
	if (! TakeLock(info, "DMA", info->umd_chan)) return;

	info->owner_func = umd_dma_goodput_demo;

	const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

	info->umd_dch = new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

	if(info->umd_dch->getDestId() == info->did && GetEnv("FORCE_DESTID") == NULL) {
		CRIT("\n\tERROR: Testing against own desitd=%d. Set env FORCE_DESTID to disable this check.\n", info->did);
		goto exit;
	}

        if (GetEnv("sim") != NULL) { info->umd_dch->setSim(); INFO("SIMULATION MODE\n"); }

	if (!info->umd_dch->alloc_dmatxdesc(info->umd_tx_buf_cnt)) {
		CRIT("\n\talloc_dmatxdesc failed: bufs %d",
							info->umd_tx_buf_cnt);
		goto exit;
	};
        if (!info->umd_dch->alloc_dmacompldesc(info->umd_sts_entries)) {
		CRIT("\n\talloc_dmacompldesc failed: entries %d",
							info->umd_sts_entries);
		goto exit;
	};

        memset(info->dmamem, 0, sizeof(info->dmamem));
        memset(info->dmaopt, 0, sizeof(info->dmaopt));

	// Reduce number of allocated buffers to 1 to allow
	// more transactions to be sent with a larger ring.
        if (!info->umd_dch->alloc_dmamem(info->acc_size, info->dmamem[0])) {
		CRIT("\n\talloc_dmamem failed: i %d size %x",
							0, info->acc_size);
		goto exit;
	};
        memset(info->dmamem[0].win_ptr, PATTERN[0], info->acc_size);

        for (int i = 1; i < info->umd_tx_buf_cnt; i++) {
		info->dmamem[i] = info->dmamem[0];
        };

        info->tick_data_total = 0;
	info->tick_count = info->tick_total = 0;

        info->umd_dch->resetHw();
        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

        if (GetEnv("verb") != NULL) {
	        INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%x bcount=%d #buf=%d #fifo=%d\n",
			info->umd_dch->getDestId(),
			info->did, info->rio_addr, info->acc_size,
			info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

	init_seq_ts(&info->desc_ts);
	init_seq_ts(&info->fifo_ts);
	init_seq_ts(&info->meas_ts);

        info->umd_fifo_proc_must_die = 0;
        info->umd_fifo_proc_alive = 0;

        rc = pthread_create(&info->umd_fifo_thr.thr, NULL,
			    umd_dma_fifo_proc_thr, (void *)info);
	if (rc) {
		CRIT("\n\tCould not create umd_fifo_proc thread, exiting...");
		goto exit;
	};
	sem_wait(&info->umd_fifo_proc_started);

	if (!info->umd_fifo_proc_alive) {
		CRIT("\n\tumd_fifo_proc thread is dead, exiting..");
		goto exit;
	};

	zero_stats(info);

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		uint64_t iq;
		info->umd_dma_abort_reason = 0;
	
        	for (cnt = 0; (cnt < info->byte_cnt) && !info->stop_req;
							cnt += info->acc_size) {
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = __LINE__; break; }

			info->dmaopt[oi].destid      = info->did;
			info->dmaopt[oi].bcount      = info->acc_size;
			info->dmaopt[oi].raddr.lsb64 = info->rio_addr + cnt;

			bool q_was_full = info->umd_dch->queueFull();
			info->umd_dma_abort_reason = 0;

			if (!q_was_full) {
				if (info->umd_dch->queueDmaOpT1(
					info->umd_tx_rtype,
					info->dmaopt[oi], info->dmamem[oi],
                                        info->umd_dma_abort_reason,
					&info->meas_ts)) {
					get_seq_ts(&info->desc_ts);
				} else {
					q_was_full = true;
				};
			};
			
			if ((info->umd_dch->queueSize() > info->umd_tx_buf_cnt)
			 || (info->umd_dch->queueSize() < 0))
				CRIT("\n\t Cnt=0x%lx Qsize=%d oi=%d\n", 
					cnt, info->umd_dch->queueSize(), oi);

			// Busy-wait for queue to drain
			for (iq = 0; !info->stop_req && q_was_full && 
				(iq < 1000000000) &&
				(info->umd_dch->queueSize() >= Q_THR);
			    	iq++) {
  				if (info->umd_dch->dmaCheckAbort(
					info->umd_dma_abort_reason)) {
					CRIT("\n\tCould not enqueue T1 cnt=%d oi=%d\n", cnt, oi);
					CRIT("\n\tDMA abort %x: %s\n", 
						info->umd_dma_abort_reason,
						DMAChannel::abortReasonToStr(
						info->umd_dma_abort_reason));
					goto exit;
  				}
			}

			// Wrap around, do no overwrite last buffer entry
			oi++;
			if ((info->umd_tx_buf_cnt - 1) == oi) {
				oi = 0;
			}
                } // END for transmit burst

		// RX Check

		info->umd_tx_iter_cnt++;
        } // END while NOT stop requested
	goto exit_nomsg;
exit:
        INFO("\n\tDMA %srunning after %d %dus polls\n",
		info->umd_dch->dmaIsRunning()? "": "NOT ", 
			info->perf_msg_cnt, DMA_RUNPOLL_US);

        INFO("\n\tEXITING (FIFO iter=%lu hw RP=%u WP=%u)\n",
                info->umd_dch->m_fifo_scan_cnt,
		info->umd_dch->getFIFOReadCount(),
                info->umd_dch->getFIFOWriteCount());
exit_nomsg:
        info->umd_fifo_proc_must_die = 1;
	if (info->umd_dch)
		info->umd_dch->shutdown();

        pthread_join(info->umd_fifo_thr.thr, NULL);

	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);

        delete info->umd_dch; info->umd_dch = NULL;
	delete info->umd_lock; info->umd_lock = NULL;
}

static inline bool queueDmaOp(struct worker* info, const int oi, const int cnt, bool& q_was_full)
{
	if(info == NULL || oi < 0 || cnt < 0) return false;

	q_was_full = false;
	info->umd_dma_abort_reason = 0;

	bool rr = false;
	if (info->acc_size <= 16) {
		rr = info->umd_dch->queueDmaOpT2(info->umd_tx_rtype,
					info->dmaopt[oi],
					(uint8_t*)info->dmamem[oi].win_ptr, info->acc_size,
					info->umd_dma_abort_reason,
					&info->meas_ts);
	} else {
		rr = info->umd_dch->queueDmaOpT1(info->umd_tx_rtype,
					info->dmaopt[oi], info->dmamem[oi],
					info->umd_dma_abort_reason,
					&info->meas_ts);
	}
	if (! rr) {
		if(info->umd_dma_abort_reason != 0) {
			CRIT("\n\tCould not enqueue T1 cnt=%d oi=%d\n", cnt, oi);
			CRIT("DMA abort %x: %s\n",
				info->umd_dma_abort_reason,
				DMAChannel::abortReasonToStr(
				info->umd_dma_abort_reason));
			goto exit;
		}
		// Don't barf just yet if queue full
		q_was_full = true;
	};

#ifdef PARANOIA_PORT_CHECKS
	{{
	bool inp_err = false, outp_err = false;

	if (info->umd_dch->checkPortError()) {
		CRIT("\n\tPort Error, exiting");
		goto exit;
	}

	info->umd_dch->checkPortInOutError(inp_err, outp_err);
	if(inp_err || outp_err) {
		CRIT("Tsi721 port error%s%s\n",
			(inp_err? " INPUT": ""),
			(outp_err? " OUTPUT": ""));
		goto exit;
	}
	}}
#endif

	return true;
exit:
	return false;
}

#define DMA_LAT_MASTER_SIG1	0xAE
#define DMA_LAT_SLAVE_SIG1	0xEA

static inline bool umd_dma_goodput_latency_demo_SLAVE(struct worker *info, const int oi, const int cnt)
{
	assert(info);
	assert(info->ib_ptr);
	assert(info->dmamem[oi].win_ptr);

        DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	bool q_was_full = false;

	DBG("\n\tPolling %p for Master transfer\n", info->ib_ptr);
	{{
		volatile uint8_t* datain_ptr8 = (uint8_t*)info->ib_ptr + info->acc_size - sizeof(uint8_t);

		// Wait for Mater to TX
		while (!info->stop_req && datain_ptr8[0] != DMA_LAT_MASTER_SIG1) { ; }
		datain_ptr8[0] = 0;
	}}

	if (info->stop_req) return false;

	{{
		uint8_t* dataout_ptr8 = (uint8_t*)info->dmamem[oi].win_ptr + info->acc_size - sizeof(uint8_t);
		dataout_ptr8[0] = DMA_LAT_SLAVE_SIG1;
	}}

	if(! queueDmaOp(info, oi, cnt, q_was_full)) return false;

	DBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
	while (!q_was_full && !info->stop_req && info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8) == 0) { ; }

	return true;
}

static inline bool umd_dma_goodput_latency_demo_MASTER(struct worker *info, const int oi, const int cnt)
{
	assert(info);
	assert(info->ib_ptr);
	assert(info->dmamem[oi].win_ptr);

        DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	bool q_was_full = false;

	{{
		uint8_t* dataout_ptr8 = (uint8_t*)info->dmamem[oi].win_ptr + info->acc_size - sizeof(uint8_t);
		dataout_ptr8[0] = DMA_LAT_MASTER_SIG1;
	}}

	DBG("\n\tTransfer to Slave destid=%d\n", info->did);
	start_iter_stats(info);

	if(! queueDmaOp(info, oi, cnt, q_was_full)) return false;

	//DBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
	while (!q_was_full && !info->stop_req && info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8) == 0) { ; }

	if(info->stop_req) return false;

	// Wait for Slave to TX
	//DBG("\n\tPolling %p for Slave transfer\n", info->ib_ptr);
	{{
		volatile uint8_t* datain_ptr8 = (uint8_t*)info->ib_ptr + info->acc_size - sizeof(uint8_t);

		while (!info->stop_req && datain_ptr8[0] != DMA_LAT_SLAVE_SIG1) { ; }

		finish_iter_stats(info);
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
		DBG("\n\tGot Slave transfer from destid=%d\n", info->did);

		datain_ptr8[0] = 0;
	}}

	return true;
}

/** \brief Multiplexed UMD Latency Demo function
 * \param[in] info
 * \param op operation: 'T' for Master, 'R' for Slave and 'N' for NREAD
 */
void umd_dma_goodput_latency_demo(struct worker* info, const char op)
{
	int oi = 0;
	uint64_t cnt = 0;
	int iter = 0;
        int FT_TEST = 0;

	//NOT WITH SHM// if (! TakeLock(info, "DMA", info->umd_chan)) return;

	//info->owner_func = (void*)umd_dma_goodput_latency_demo;

	info->umd_dch = new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

        if(info->umd_dch->getDestId() == info->did && GetEnv("FORCE_DESTID") == NULL) {
                CRIT("\n\tERROR: Testing against own desitd=%d. Set env FORCE_DESTID to disable this check.\n", info->did);
                goto exit;
        }

	if (op == 'N') {
		FT_TEST = (int)(GetEnv("sim") != NULL) << 1 | (int)(GetEnv("baddid") != NULL);
		assert(FT_TEST != 0x3);
		if (FT_TEST == 0x2) { info->umd_dch->setSim(); INFO("SIMULATION MODE - NREAD\n"); }
		if (FT_TEST == 0x1) { INFO("Tsi721 FAULT MODE - NREAD\n"); }
	}

	try {
		if (!info->umd_dch->alloc_dmatxdesc(info->umd_tx_buf_cnt)) {
			CRIT("\n\talloc_dmatxdesc failed: bufs %d",
								info->umd_tx_buf_cnt);
			goto exit;
		};
		if (!info->umd_dch->alloc_dmacompldesc(info->umd_sts_entries)) {
			CRIT("\n\talloc_dmacompldesc failed: entries %d",
								info->umd_sts_entries);
			goto exit;
		};
        } catch(std::runtime_error ex) { INFO("\n\tException: %s\n", ex.what()); }

        memset(info->dmamem, 0, sizeof(info->dmamem));
        memset(info->dmaopt, 0, sizeof(info->dmaopt));

	// Reduce number of allocated buffers to 1 to allow
	// more transactions to be sent with a larger ring.
        if (!info->umd_dch->alloc_dmamem(info->acc_size, info->dmamem[0])) {
		CRIT("\n\talloc_dmamem failed: i %d size %x",
							0, info->acc_size);
		goto exit;
	};
        memset(info->dmamem[0].win_ptr, PATTERN[0], info->acc_size);

        for (int i = 1; i < info->umd_tx_buf_cnt; i++) {
		info->dmamem[i] = info->dmamem[0];
        };

        info->tick_data_total = 0;
	info->tick_count = info->tick_total = 0;

        try { 
		info->umd_dch->resetHw();
	} catch(std::runtime_error ex) { INFO("\n\tException: %s\n", ex.what()); }
        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

	zero_stats(info);

	if (GetEnv("verb") != NULL) {
		INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%lx bcount=%d #buf=%d #fifo=%d\n",
		     info->umd_dch->getDestId(),
		     info->did, info->rio_addr, info->acc_size,
		     info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

	if(op == 'T' || op == 'R') // Not used for NREAD
		memset(info->ib_ptr, 0,info->acc_size);

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);


	// TX Loop
	for (cnt = 0; !info->stop_req; cnt++) {
		info->dmaopt[oi].destid      = info->did;
		info->dmaopt[oi].bcount      = info->acc_size;
		info->dmaopt[oi].raddr.lsb64 = info->rio_addr;;

		if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = __LINE__; break; }

		switch(op) {
		case 'T': // TX - Master, it does its own start_iter_stats/finish_iter_stats
			if(! umd_dma_goodput_latency_demo_MASTER(info, oi, cnt)) goto exit;
			break;
		case 'R': // RX - Slave, no stats collected
			if(! umd_dma_goodput_latency_demo_SLAVE(info, oi, cnt)) goto exit;
			break;
		case 'N': // TX - NREAD
			{{
			bool q_was_full = false;
			DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

			int bd_q_cnt = 0;
			int bd_f_cnt = 0;
			if (! FT_TEST) {
				start_iter_stats(info);
                		if (! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;
				bd_q_cnt = 1;
			} else { // XXX This is a Holly Mess of Spagetti code to exercise Fault Tolerance
				// Can we recover/replay BD at sim+1 ?
				const int N = GetEnv("sim") != NULL?
						 GetDecParm("$sim", -1) + 1:
						 GetDecParm("$baddid", -1) + 1;
				assert(N);
				start_iter_stats(info);
				for (int i = 0; !q_was_full && i < N; i++) {
					info->dmaopt[oi].destid      = info->did;
					info->dmaopt[oi].bcount      = info->acc_size;
					info->dmaopt[oi].raddr.lsb64 = info->rio_addr;;

					if (i == (GetDecParm("$baddid", -1) -1))
						info->dmaopt[oi].destid = 0xDEAD;

					if (! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;
					bd_q_cnt++;

					if (info->stop_req) goto exit;

					if (i == (N-1)) continue; // Don't advance oi twice

					// Wrap around, do no overwrite last buffer entry
					oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; };
				}

				std::string s;
      				info->umd_dch->dumpBDs(s);
			        DBG("\n\tAfter fault enq [oi=%d] %s: %s\n", oi, (q_was_full? " queue FULL": ""),  s.c_str());
			}

			if (FT_TEST == 0x2) info->umd_dch->simFIFO(GetDecParm("$sim", 0), GetDecParm("$simf", 0));

			DBG("\n\tPolling FIFO transfer completion destid=%d iter=%llu\n", info->did, cnt);
			try {
			while (!q_was_full && !info->stop_req && (bd_f_cnt = info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8)) == 0) {
				if (FT_TEST == 0x1 && info->umd_dch->dmaCheckAbort(info->umd_dma_abort_reason)) break;
			}
			if (FT_TEST == 0x1 && !q_was_full && !info->stop_req) {
				usleep(500);
				bd_f_cnt += info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8);
			}
			} catch(std::runtime_error ex) { INFO("\n\tException: %s\n", ex.what()); }

			// XXX check for errors, nuke faulting BD, do softRestart
			if (q_was_full || bd_f_cnt < bd_q_cnt) {
				if (q_was_full) {
					CRIT("\n\tBUG: Queue Full!\n");
					//info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8); continue; // No faults after a restart to process all T3 BDs
					goto exit;
				}

                                if (info->umd_dch->dmaCheckAbort(
                                        info->umd_dma_abort_reason)) {
                                        CRIT("\n\tCould not TX/RX NREAD bd_q_cnt=%d bd_f_cnt=%d\n", bd_q_cnt, bd_f_cnt);
                                        CRIT("\n\tDMA abort %x: %s\n",
                                                info->umd_dma_abort_reason,
                                                DMAChannel::abortReasonToStr(
                                                info->umd_dma_abort_reason));
                                }
				{{
					bool inp_err = false;
					bool outp_err = false;
					info->umd_dch->checkPortInOutError(inp_err, outp_err);
					if(inp_err || outp_err) {
						CRIT("Tsi721 port error%s%s bd_q_cnt=%d bd_f_cnt=%d\n",
							(inp_err? " INPUT": ""),
							(outp_err? " OUTPUT": ""),
							bd_q_cnt, bd_f_cnt);
						goto exit;
					}

				}}

				// nuke faulting BD
				try {
					const int pending = info->umd_dch->cleanupBDQueue(false /*multithreaded_fifo*/);

					info->umd_dch->softRestart(pending == 0); // Wipe clean BD queue if no outstanding
					if (FT_TEST == 0x2) info->umd_dch->simFIFO(0, 0); // No faults after a restart to process all T3 BDs
				} catch(std::runtime_error ex) { INFO("\n\tException: %s\n", ex.what()); }
			}

                	finish_iter_stats(info);
#if 0
			if (7 <= g_level) { // DEBUG
				std::stringstream ss;
				for(int i = 0; i < 16; i++) {
					char tmp[9] = {0};
					snprintf(tmp, 8, "%02x ", wi[0].t2_rddata[i]);
					ss << tmp;
				}
				DBG("\n\tNREAD-in data: %s\n", ss.str().c_str());
			}
#endif
			}}
			break;

		default: CRIT("\n\t: Invalid operation '%c'\n", op); goto exit;
			break;
		}

		if (info->stop_req) goto exit;

		// Wrap around, do no overwrite last buffer entry
		oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; };
	} // END for infinite transmit

exit:
	if (info->umd_dch)
		info->umd_dch->shutdown();

	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);

        try { delete info->umd_dch; info->umd_dch = NULL; }
	catch(std::runtime_error ex) {}
	delete info->umd_lock; info->umd_lock = NULL;
}

static inline int MIN(int a, int b) { return a < b? a: b; }

#endif // USER_MODE_DRIVER

void *worker_thread(void *parm)
{
	struct worker *info = (struct worker *)parm;

	sem_post(&info->started);

	info->stat = 1;
	while (info->stat) {
		if (info->wkr_thr.cpu_req != info->wkr_thr.cpu_run)
			migrate_thread_to_cpu(&info->wkr_thr);

		switch (info->action) {
        	case alloc_ibwin:
				dma_alloc_ibwin(info);
				break;
        	case free_ibwin:
				dma_free_ibwin(info);
				break;
#ifdef USER_MODE_DRIVER
		case umd_shm:
				umd_shm_goodput_demo(info);
				break;
		case umd_dma:
				umd_dma_goodput_demo(info);
				break;
		case umd_dmalrx:
				umd_dma_goodput_latency_demo(info, 'R');
				break;
		case umd_dmaltx:
				umd_dma_goodput_latency_demo(info, 'T');
				break;
		case umd_dmalnr: // NREAD
				umd_dma_goodput_latency_demo(info, 'N');
				break;
#endif // USER_MODE_DRIVER
		
        	case shutdown_worker:
				info->stat = 0;
		default:
		case no_action:
		case last_action:
				break;
		};

		if (info->stat) {
			info->stat = 2;
			sem_wait(&info->run);
			info->stat = 1;
		};
	};

	shutdown_worker_thread(info);
	pthread_exit(parm);
};


void start_worker_thread(struct worker *info, int new_mp_h, int cpu)
{
	int rc;

	init_worker_info(info, 0);

	if (new_mp_h) {
        	rc = riomp_mgmt_mport_create_handle(0, 0, &info->mp_h);
        	if (rc)
			return;
		info->mp_h_is_mine = 1;
	} else {
        	info->mp_h = mp_h;
	};
	info->mp_num = mp_h_num;
	info->wkr_thr.cpu_req = cpu;

	rc = pthread_create(&info->wkr_thr.thr, NULL, worker_thread,
								(void *)info);

	if (!rc)
		sem_wait(&info->started);
};

#ifdef __cplusplus
}
#endif
