/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>
#include <sstream>

#include <sched.h>

#include "libcli.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_dma.h"
#include "liblog.h"

#include "libtime_utils.h"
#include "worker.h"
#include "goodput.h"
#include "mhz.h"

#ifdef USER_MODE_DRIVER
#include "dmachanshm.h"
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

	info->umd_fifo_total_ticks = 0;
	info->umd_fifo_total_ticks_count = 0;

	//if (first_time) {
        	sem_init(&info->umd_fifo_proc_started, 0, 0);
	//};
	init_seq_ts(&info->desc_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->fifo_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->meas_ts, MAX_TIMESTAMPS);

	memset(info->check_abort_stats, 0, sizeof(info->check_abort_stats));
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

#ifndef PAGE_4K
  #define PAGE_4K	4096
#endif

extern char* dma_rtype_str[];

/** \brief Dump UMDd Tun status
 *  * \note This executes within the CLI thread but targets Main thread's data
 *   */
extern "C"
void UMD_DD(struct worker* info)
{
        assert(info->umd_dch);

	const int MHz = getCPUMHz();
	//const int idx = info->idx;

	std::string ab;
	for (int i = 0; i < 255; i++) {
		if (0 == info->check_abort_stats[i]) continue;

		char tmp[257] = {0};
		snprintf(tmp, 256, "%c=%lu", i, info->check_abort_stats[i]);
		ab.append(tmp).append(" ");
	}
	if (ab.size() > 0)
		INFO("\n\tcheckAbort stats: %s\n", ab.c_str());

	uint64_t total = 0;
        DMAChannelSHM::DmaShmPendingData_t pdata; memset(&pdata, 0, sizeof(pdata));
	info->umd_dch->getShmPendingData(total, pdata);

	if (total > 0) {
		std::stringstream ss;
		ss << "Total: " << total << "\n";
		for(int i = 1; i < DMAChannelSHM::DMA_MAX_CHAN; i++) {
			if(pdata.data[i] == 0) continue;
			ss << "\tCh" << i <<" pending " << pdata.data[i] << "\n";
		}
		INFO("\n\tIn-flight data %s", ss.str().c_str());
	}

	INFO("\n\tQsize=%d Enqd.WP=%lu HW.{WP=%u RP=%u} FIFOackd=%llu FIFO.{WP=%u RP=%u} AckedSN=%llu\n",
	     info->umd_dch->queueSize(),
	     info->umd_dch->getWP(),
	     info->umd_dch->getWriteCount(), info->umd_dch->getReadCount(),
	     info->umd_dch->m_tx_cnt,
	     info->umd_dch->getFIFOReadCount(), info->umd_dch->getFIFOWriteCount(),
	     info->umd_dch->getAckedSN());

	if (info->umd_fifo_total_ticks_count > 0) {
		float avgTick_uS = ((float)info->umd_fifo_total_ticks / info->umd_fifo_total_ticks_count) / MHz;
		INFO("\n\tFIFO Avg TX %f uS cnt=%llu\n", avgTick_uS, info->umd_fifo_total_ticks_count);
	}

	DMAChannelSHM::ShmClientCompl_t comp[DMAChannelSHM::DMA_SHM_MAX_CLIENTS];
	memset(&comp, 0, sizeof(comp));
	info->umd_dch->listClients(&comp[0], sizeof(comp));

	for (int i = 0; i < DMAChannelSHM::DMA_SHM_MAX_CLIENTS; i++) {
		if (! comp[i].busy) continue;
		INFO("\n\tpid=%d%s change_cnt=%llu bad_tik.{RP=%llu WP=%llu} NREAD_T2_res.{RP=%llu WP=%llu} EnqBy=%llu TXdBy=%llu\n",
		     comp[i].owner_pid, (kill(comp[i].owner_pid,0)? " DEAD": ""),
		     comp[i].change_cnt,
		     comp[i].bad_tik.RP, comp[i].bad_tik.WP,
		     comp[i].NREAD_T2_results.RP, comp[i].NREAD_T2_results.WP,
		     comp[i].bytes_enq, comp[i].bytes_txd);
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
bool TakeLock(struct worker* info, const char* module, const int mport, const int instance)
{
        if (info == NULL || module == NULL || module[0] == '\0' || instance < 0) return false;

        char lock_name[81] = {0};
        snprintf(lock_name, 80, "/var/lock/UMD-%s-%d:%d..LCK", module, mport, instance);
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

	if (GetEnv((char *)"IGNORE_CPUALLOC") != NULL) return true;

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
        if (! TakeLock(info, "DMA", info->mp_num, info->umd_chan)) return;

	info->owner_func = umd_shm_goodput_demo;

        info->umd_dch = new DMAChannelSHM(info->mp_num, info->umd_chan, info->mp_h);
        if (NULL == info->umd_dch) {
                CRIT("\n\tDMAChannelSHM alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
                goto exit;
        };

        if(info->umd_dch->getDestId() == info->did && GetEnv((char *)"FORCE_DESTID") == NULL) {
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

	INFO("\n\tUMDd/SHM my_destid=%u #buf=%d #fifo=%d\n",
		info->umd_dch->getDestId(),
		info->umd_tx_buf_cnt, info->umd_sts_entries);

        DMAChannelSHM::WorkItem_t wi[info->umd_sts_entries*8];
        memset(wi, 0, sizeof(wi));

	clock_gettime(CLOCK_MONOTONIC, &info->iter_st_time);
        while (!info->stop_req) {
                const int cnt = info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8);
                if (!cnt)
                        goto next;

		fifo_unwork_ACK = false;
                clock_gettime(CLOCK_MONOTONIC, &info->fifo_work_time);

                for (int i = 0; i < cnt; i++) {
                        DMAChannelSHM::WorkItem_t& item = wi[i];

                        switch (item.opt.dtype) {
                        case DTYPE1:
                        case DTYPE2:
                                //info->perf_byte_cnt += info->acc_size;
                                if (item.opt.ts_end > item.opt.ts_start) {
					info->umd_fifo_total_ticks += item.opt.ts_end - item.opt.ts_start;
					info->umd_fifo_total_ticks_count++;
				}
                                break;
                        case DTYPE3:
                                break;
                        default:
                                ERR("\n\tUNKNOWN BD %d bd_wp=%u\n",
                                        item.opt.dtype, item.bd_wp);
                                break;
                        }

                        wi[i].valid = 0xdeadabba;
                } // END for WorkItem_t vector

	next:
                if (!cnt) clock_gettime(CLOCK_MONOTONIC, &info->iter_end_time);
                else      info->iter_end_time = info->fifo_work_time;

		char check_abort = 0;
		do {
			if (info->umd_dch->queueFull()) { check_abort = true; break; }

			struct timespec dT = time_difference(info->iter_end_time, info->iter_st_time);
			const uint64_t nsec = dT.tv_nsec + (dT.tv_sec * 1000000000);
			if (nsec > 10 * 1000000) { // Every 10 ms
				info->iter_st_time = info->iter_end_time;
				check_abort = 'T';
				break;
			}

			if (cnt) break;
			if (fifo_unwork_ACK) break; // No sense polling PCIe sensessly

			struct timespec dT_FIFO = time_difference(info->iter_end_time, info->fifo_work_time);
			const uint64_t nsec_FIFO = dT_FIFO.tv_nsec + (dT_FIFO.tv_sec * 1000000000);
			if (nsec_FIFO > 1000 * 1000) { // Nothing popped in FIFO in the last 1000 microsec
				fifo_unwork_ACK = true;
				check_abort = 't';
				break;
			}
		} while(0);

		if (check_abort) info->check_abort_stats[(int)check_abort]++;
//check_abort=0;

		if (check_abort && info->umd_dch->dmaCheckAbort(info->umd_dma_abort_reason)) {
			CRIT("\n\tDMA abort 0x%x: %s. SOFT RESTART\n", info->umd_dma_abort_reason,
			     DMAChannelSHM::abortReasonToStr(info->umd_dma_abort_reason));
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
	uint32_t oi = 0;//, rc;
	uint64_t cnt = 0;
	int iter = 0;

	if (! umd_check_cpu_allocation(info)) return;

	info->owner_func = umd_dma_goodput_demo;

	const uint32_t Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

	info->umd_dch = new DMAChannelSHM(info->mp_num, info->umd_chan, info->mp_h);
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannelSHM alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

	if(info->umd_dch->getDestId() == info->did && GetEnv((char *)"FORCE_DESTID") == NULL) {
		CRIT("\n\tERROR: Testing against own desitd=%d. Set env FORCE_DESTID to disable this check.\n", info->did);
		goto exit;
	}

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

        for (uint32_t i = 1; i < info->umd_tx_buf_cnt; i++) {
		info->dmamem[i] = info->dmamem[0];
        };

        info->tick_data_total = 0;
	info->tick_count = info->tick_total = 0;

        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

        if (GetEnv((char *)"verb") != NULL) {
	        INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%x bcount=%d #buf=%d #fifo=%d\n",
			info->umd_dch->getDestId(),
			info->did, info->rio_addr, info->acc_size,
			info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

	init_seq_ts(&info->desc_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->fifo_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->meas_ts, MAX_TIMESTAMPS);

	zero_stats(info);

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		uint64_t iq;
		info->umd_dma_abort_reason = 0;
	
        	for (cnt = 0; info->stop_req; cnt += info->acc_size) {
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = __LINE__; break; }

			info->dmaopt[oi].destid      = info->did;
			info->dmaopt[oi].bcount      = info->acc_size;
			info->dmaopt[oi].raddr.lsb64 = info->rio_addr;

			bool q_was_full = info->umd_dch->queueFull();
			info->umd_dma_abort_reason = 0;

			if (!q_was_full) {
				if (info->umd_dch->queueDmaOpT1(
					info->umd_tx_rtype,
					info->dmaopt[oi], info->dmamem[oi],
                                        info->umd_dma_abort_reason,
					&info->meas_ts)) {
					ts_now(&info->desc_ts);
				} else {
					q_was_full = true;
				};
			};
			
			if ((info->umd_dch->queueSize() > info->umd_tx_buf_cnt)
			 || (info->umd_dch->queueSize() < 0))
				CRIT("\n\t Cnt=0x%lx Qsize=%d oi=%d\n", 
					cnt, info->umd_dch->queueSize(), oi);

			uint64_t t = 0;
			if (info->umd_dch->dequeueFaultedTicket(t)) {
				CRIT("\n\tFaulted ticket %llu current ticket %llu\n", t, info->dmaopt[oi].ticket);
				goto exit;
			}

			info->perf_byte_cnt = info->umd_dch->getBytesEnqueued();

			// Busy-wait for queue to drain
			for (iq = 0; !info->stop_req && q_was_full && 
				(iq < 1000000000) &&
				(info->umd_dch->queueSize() >= Q_THR);
			    	iq++) {
			}

			// Wrap around, do no overwrite last buffer entry
			oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; }
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
	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);

        delete info->umd_dch; info->umd_dch = NULL;
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
			CRIT("\n\tCould not enqueue T1 cnt=%d oi=%d DMA abort %x: %s\n", cnt, oi,
				info->umd_dma_abort_reason,
				DMAChannelSHM::abortReasonToStr(
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

        if (q_was_full) {
                uint64_t t = 0;
                if (info->umd_dch->dequeueFaultedTicket(t))
                     { CRIT("\n\tQueue FULL faulted ticket %llu current ticket %llu\n", t, info->dmaopt[oi].ticket); }
                else { CRIT("\n\tQueue FULL current ticket %llu\n", t, info->dmaopt[oi].ticket); }

                return false;
        }

        for (; !info->stop_req; ) {
                const DMAChannelSHM::TicketState_t st = info->umd_dch->checkTicket(info->dmaopt[oi]);
                if (st == DMAChannelSHM::COMPLETED) break;
                //if (st == DMAChannelSHM::INPROGRESS) continue;

                if (st == DMAChannelSHM::BORKED) {
                        CRIT("\n\tTicket %llu status BORKED (%d)\n", info->dmaopt[oi].ticket, st);
                        return false;
                }

                uint64_t t = 0;
                if (!info->umd_dch->dequeueFaultedTicket(t)) continue;

                assert(t != info->dmaopt[oi].ticket);
        }

	return true;
}

static inline bool umd_dma_goodput_latency_demo_MASTER(struct worker *info, const int oi, const int cnt)
{
	assert(info);
	assert(info->ib_ptr);
	assert(info->dmamem[oi].win_ptr);

	bool q_was_full = false;

	{{
		uint8_t* dataout_ptr8 = (uint8_t*)info->dmamem[oi].win_ptr + info->acc_size - sizeof(uint8_t);
		dataout_ptr8[0] = DMA_LAT_MASTER_SIG1;
	}}

	DBG("\n\tTransfer to Slave destid=%d\n", info->did);
	start_iter_stats(info);

	if(! queueDmaOp(info, oi, cnt, q_was_full)) return false;

	if (q_was_full) {
		uint64_t t = 0;
		if (info->umd_dch->dequeueFaultedTicket(t))
		     { CRIT("\n\tQueue FULL faulted ticket %llu current ticket %llu\n", t, info->dmaopt[oi].ticket); }
		else { CRIT("\n\tQueue FULL current ticket %llu\n", t, info->dmaopt[oi].ticket); }

		return false;
	}

	for (; !info->stop_req; ) {
		const DMAChannelSHM::TicketState_t st = info->umd_dch->checkTicket(info->dmaopt[oi]);
		if (st == DMAChannelSHM::COMPLETED) break;
		//if (st == DMAChannelSHM::INPROGRESS) continue;
		
		if (st == DMAChannelSHM::BORKED) {
			CRIT("\n\tTicket %llu status BORKED\n", info->dmaopt[oi].ticket);
			return false;
		}

		uint64_t t = 0;
		if (!info->umd_dch->dequeueFaultedTicket(t)) continue;

		assert(t != info->dmaopt[oi].ticket);
	}

	if(info->stop_req) return false;

	// Wait for Slave to TX
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
	uint32_t oi = 0;
	uint64_t cnt = 0;
	int iter = 0;

	//info->owner_func = (void*)umd_dma_goodput_latency_demo;

	info->umd_dch = new DMAChannelSHM(info->mp_num, info->umd_chan, info->mp_h);
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannelSHM alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

        if(info->umd_dch->getDestId() == info->did && GetEnv((char *)"FORCE_DESTID") == NULL) {
                CRIT("\n\tERROR: Testing against own desitd=%d. Set env FORCE_DESTID to disable this check.\n", info->did);
                goto exit;
        }

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

        for (uint32_t i = 1; i < info->umd_tx_buf_cnt; i++) {
		info->dmamem[i] = info->dmamem[0];
        };

        info->tick_data_total = 0;
	info->tick_count = info->tick_total = 0;

        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

	zero_stats(info);

	if (GetEnv((char *)"verb") != NULL) {
		INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%lx bcount=%d #buf=%d #fifo=%d\n",
		     info->umd_dch->getDestId(),
		     info->did, info->rio_addr, info->acc_size,
		     info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

	if(op == 'T' || op == 'R') // Not used for NREAD
		memset(info->ib_ptr, 0, info->acc_size);

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	// TX Loop
	for (cnt = 0; !info->stop_req; cnt++) {
		info->dmaopt[oi].destid      = info->did;
		info->dmaopt[oi].bcount      = info->acc_size;
		info->dmaopt[oi].raddr.lsb64 = info->rio_addr;

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

			start_iter_stats(info);
			if (! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;

			if (q_was_full) {
				CRIT("\n\tBUG: Queue Full!\n");
			
                                if (info->umd_dch->dmaCheckAbort(
                                        info->umd_dma_abort_reason)) {
                                        CRIT("\n\tDMA abort %x: %s\n",
                                                info->umd_dma_abort_reason,
                                                DMAChannelSHM::abortReasonToStr(
                                                info->umd_dma_abort_reason));
                                }
				{{
					bool inp_err = false;
					bool outp_err = false;
					info->umd_dch->checkPortInOutError(inp_err, outp_err);
					if(inp_err || outp_err) {
						CRIT("Tsi721 port error%s%s\n",
							(inp_err? " INPUT": ""),
							(outp_err? " OUTPUT": ""));
						goto exit;
					}

				}}
			}

			std::vector<DMAChannelSHM::NREAD_Result_t> results;
			if (info->acc_size <= 16) {
				for (;;) {
					DMAChannelSHM::NREAD_Result_t res;
					if (!info->umd_dch->dequeueDmaNREADT2(res)) break;
					assert(res.ticket);
					results.push_back(res);
				}
			}

                	finish_iter_stats(info);

			if (7 <= g_level && results.size() > 0) { // DEBUG
				std::vector<DMAChannelSHM::NREAD_Result_t>::iterator it = results.begin();
				for (; it != results.end(); it++) {
					std::stringstream ss;
					for(int i = 0; i < 16; i++) {
						char tmp[9] = {0};
						snprintf(tmp, 8, "%02x ", it->data[i]);
						ss << tmp;
					}
					DBG("\n\tTicket %llu NREAD-in data: %s\n", it->ticket, ss.str().c_str());
				}
			}

			std::vector<uint64_t> faults;
			for (;;) {
				uint64_t t = 0;
				if (!info->umd_dch->dequeueFaultedTicket(t)) break;
				assert(t);
				faults.push_back(t);
			}
			if (7 <= g_level && faults.size() > 0) {
				std::vector<uint64_t>::iterator it = faults.begin();
				for (; it != faults.end(); it++) INFO("Faulted ticket: %llu\n", *it);
			}

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
	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);

        try { delete info->umd_dch; info->umd_dch = NULL; }
	catch(std::runtime_error ex) {}
}

/** \brief UMD SHM Testbed for BD ordering. Possible broken code.
 * \param[in] info
 */
void umd_dma_goodput_testbed(struct worker* info)
{
	uint32_t oi = 0;
	uint64_t cnt = 0;
	int iter = 0;

	info->umd_dch = new DMAChannelSHM(info->mp_num, info->umd_chan, info->mp_h);
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannelSHM alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

        if(info->umd_dch->getDestId() == info->did && GetEnv((char *)"FORCE_DESTID") == NULL) {
                CRIT("\n\tERROR: Testing against own desitd=%d. Set env FORCE_DESTID to disable this check.\n", info->did);
                goto exit;
        }

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

        for (uint32_t i = 1; i < info->umd_tx_buf_cnt; i++) {
		info->dmamem[i] = info->dmamem[0];
        };

        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

	if (GetEnv((char *)"verb") != NULL) {
		INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%lx bcount=%d #buf=%d #fifo=%d\n",
		     info->umd_dch->getDestId(),
		     info->did, info->rio_addr, info->acc_size,
		     info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

	if (info->ib_ptr != NULL) memset(info->ib_ptr, 0, info->acc_size);

	{{
	const int N = GetDecParm((char*)"$simnr", 0) + 1;
	const int M = GetDecParm((char*)"$simnw", 0);

	if (GetDecParm((char*)"$checkreg", 0)) info->umd_dch->setCheckHwReg(true);

	// TX Loop
	for (cnt = 0; !info->stop_req; cnt++) {
		if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = __LINE__; break; }

		bool q_was_full = false;

		// Can we recover/replay BD at sim+1 ?
		for (int i = 0; !q_was_full && i < N; i++) {
			info->dmaopt[oi].destid      = info->did;
			info->dmaopt[oi].bcount      = info->acc_size;
			info->dmaopt[oi].raddr.lsb64 = info->rio_addr;

			DBG("\n\tEnq rtype=%d did=%u acc_size=0x%x rio_addr=0x%llx\n",
			    info->umd_tx_rtype, info->dmaopt[oi].destid,
			    info->dmaopt[oi].bcount, info->dmaopt[oi].raddr.lsb64);

			if (! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;

			// Wrap around, do no overwrite last buffer entry
			oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; };

			if (q_was_full) goto full_test;

			if (info->stop_req) goto exit;

			for (int j = 0; j < M; j++) {
				if (info->stop_req) goto exit;

				info->dmaopt[oi].destid      = info->did;
				info->dmaopt[oi].raddr.lsb64 = info->rio_addr;

				info->dmaopt[oi].bcount = 0x20;

				info->umd_dch->queueDmaOpT1(LAST_NWRITE_R, info->dmaopt[oi], info->dmamem[oi],
							    info->umd_dma_abort_reason, &info->meas_ts);
				if(info->umd_dma_abort_reason != 0) {
					CRIT("\n\tCould not enqueue T1 cnt=%d oi=%d DMA abort %x: %s\n", cnt, oi,
						info->umd_dma_abort_reason,
						DMAChannelSHM::abortReasonToStr(
						info->umd_dma_abort_reason));
					goto exit;
				}

				// Wrap around, do no overwrite last buffer entry
				oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; };

				if (q_was_full) goto full_test;
			}
		}

	full_test:
		if (q_was_full) {
			CRIT("\n\tBUG: Queue Full!\n");
		
			if (info->umd_dch->dmaCheckAbort(
				info->umd_dma_abort_reason)) {
				CRIT("\n\tDMA abort %x: %s\n",
					info->umd_dma_abort_reason,
					DMAChannelSHM::abortReasonToStr(
					info->umd_dma_abort_reason));
			}
			{{
				bool inp_err = false;
				bool outp_err = false;
				info->umd_dch->checkPortInOutError(inp_err, outp_err);
				if(inp_err || outp_err) {
					CRIT("Tsi721 port error%s%s\n",
						(inp_err? " INPUT": ""),
						(outp_err? " OUTPUT": ""));
					goto exit;
				}

			}}
		}

		std::vector<DMAChannelSHM::NREAD_Result_t> results;
		if (info->acc_size <= 16) {
			for (;;) {
				DMAChannelSHM::NREAD_Result_t res;
				if (!info->umd_dch->dequeueDmaNREADT2(res)) break;
				assert(res.ticket);
				results.push_back(res);
			}
		}

		if (7 <= g_level && results.size() > 0) { // DEBUG
			std::vector<DMAChannelSHM::NREAD_Result_t>::iterator it = results.begin();
			for (; it != results.end(); it++) {
				std::stringstream ss;
				for(int i = 0; i < 16; i++) {
					char tmp[9] = {0};
					snprintf(tmp, 8, "%02x ", it->data[i]);
					ss << tmp;
				}
				DBG("\n\tTicket %llu NREAD-in data: %s\n", it->ticket, ss.str().c_str());
			}
		}

		std::vector<uint64_t> faults;
		for (;;) {
			uint64_t t = 0;
			if (!info->umd_dch->dequeueFaultedTicket(t)) break;
			assert(t);
			faults.push_back(t);
		}
		if (7 <= g_level && faults.size() > 0) {
			std::vector<uint64_t>::iterator it = faults.begin();
			for (; it != faults.end(); it++) INFO("Faulted ticket: %llu\n", *it);
		}

		if (info->stop_req) goto exit;
	} // END for infinite transmit
	}} // FKK GCC

exit:
	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);

        try { delete info->umd_dch; info->umd_dch = NULL; }
	catch(std::runtime_error ex) {}
}
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
		case umd_dmatest: 
				umd_dma_goodput_testbed(info);
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
