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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include <sstream>

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
#include "hash.cc"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void init_worker_info(struct worker *info, int first_time)
{
#ifdef USER_MODE_DRIVER
	int i;
#endif

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
	info->umd_chan = 0;
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
	info->desc_ts_idx = 0;
	info->fifo_ts_idx = 0;
	for (i = 0; i < MAX_TIMESTAMPS; i++) {
		info->desc_ts[i].tv_sec = info->desc_ts[i].tv_nsec = 0;
		info->fifo_ts[i].tv_sec = info->fifo_ts[i].tv_nsec = 0;
	};
#endif
};

void msg_cleanup_con_skt(struct worker *info);
void msg_cleanup_acc_skt(struct worker *info);
void msg_cleanup_mb(struct worker *info);
void direct_io_obwin_unmap(struct worker *info);
void dma_free_ibwin(struct worker *info);
void dealloc_dma_tx_buffer(struct worker *info);

void shutdown_worker_thread(struct worker *info)
{
	int rc;
	if (info->stat) {
		info->action = shutdown_worker;
		info->stop_req = 2;
		sem_post(&info->run);
#ifdef USER_MODE_DRIVER
		if (info->umd_dch) {
			info->umd_fifo_proc_must_die = 1;
			info->umd_dch->shutdown();
		};
#endif
		pthread_join(info->wkr_thr.thr, NULL);
	};
		
	dma_free_ibwin(info);
	direct_io_obwin_unmap(info);
	dealloc_dma_tx_buffer(info);

	msg_cleanup_con_skt(info);
	msg_cleanup_acc_skt(info);
	msg_cleanup_mb(info);

#ifdef USER_MODE_DRIVER
	if (info->umd_fifo_proc_alive) {
		info->umd_fifo_proc_must_die = 1;
		pthread_join(info->umd_fifo_thr.thr, NULL);
		info->umd_fifo_proc_must_die = 0;
		info->umd_fifo_proc_alive = 0;
	};

	if (info->umd_dch) {
		int i;

		info->umd_dch->cleanup();

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

void direct_io_tx(struct worker *info, volatile void * volatile ptr)
{
	switch (info->acc_size) {
	case 1: if (info->wr)
			*(uint8_t *)ptr = info->data8_tx;
		else
			info->data8_rx = *(uint8_t * volatile)ptr;
		break;

	case 2: if (info->wr)
			*(uint16_t *)ptr = info->data16_tx;
		else
			info->data16_rx = *(uint16_t * volatile)ptr;
		break;
	case 3:
	case 4: if (info->wr)
			*(uint32_t *)ptr = info->data32_tx;
		else
			info->data32_rx = *(uint32_t * volatile)ptr;
		break;

	case 8: if (info->wr)
			*(uint64_t *)ptr = info->data64_tx;
		else
			info->data64_rx = *(uint64_t * volatile)ptr;
		break;
	default:
		break;
	};
};

void init_direct_io_rx_data(struct worker *info, void * volatile ptr )
{
	switch (info->acc_size) {
	case 1: *(uint8_t *)ptr = ~info->data8_tx;
		break;

	case 2: *(uint16_t *)ptr = ~info->data16_tx;
		break;
	case 3:
	case 4: *(uint32_t *)ptr = ~info->data32_tx;
		break;

	case 8: *(uint64_t *)ptr = ~info->data64_tx;
		break;
	default:
		break;
	};
};

int direct_io_wait_for_change(struct worker *info)
{
	volatile void * volatile ptr = info->ib_ptr;
	uint64_t no_change = 1;
	uint64_t limit = 0x100000;

	if ((info->action == direct_io_tx_lat) && !info->wr)
		return 0;

	while (!info->stop_req && no_change) {
		limit = 0x100000;
		do {
			switch (info->acc_size) {
			case 1: info->data8_rx = *(uint8_t * volatile)ptr;
				no_change = (info->data8_rx != info->data8_tx);
				break;

			case 2: info->data16_rx = *(uint16_t * volatile)ptr;
				no_change = (info->data16_rx != info->data16_tx);
				break;
			case 3:
			case 4: info->data32_rx = *(uint32_t * volatile)ptr;
				no_change = (info->data32_rx != info->data32_tx);
				break;

			case 8: info->data64_rx = *(uint64_t * volatile)ptr;
				no_change = (info->data64_rx != info->data64_tx);
				break;
			default:
				no_change = 0;
				break;
			};
		} while (no_change && --limit);
	};
	return 0;
};

void incr_direct_io_data(struct worker *info)
{
        info->data8_tx++;
        info->data16_tx++;
        info->data32_tx++;
        info->data64_tx++;
};

int direct_io_obwin_map(struct worker *info)
{
	int rc;

	rc = riomp_dma_obwin_map(info->mp_h, info->did, info->rio_addr, 
					info->ob_byte_cnt, &info->ob_handle);
	if (rc) {
		ERR("FAILED: riomp_dma_obwin_map rc %d:%s\n",
					rc, strerror(errno));
		goto exit;
	};

	info->ob_valid = 1;

	rc = riomp_dma_map_memory(info->mp_h, info->ob_byte_cnt, 
					info->ob_handle, &info->ob_ptr);
	if (rc)
		ERR("FAILED: riomp_dma_map_memory rc %d:%s\n",
					rc, strerror(errno));
exit:
	return rc;
};

void direct_io_obwin_unmap(struct worker *info)
{
	int rc = 0;

	if (info->ob_ptr && info->ob_valid) {
		rc = riomp_dma_unmap_memory(info->mp_h, info->ob_byte_cnt, 
								info->ob_ptr);
		info->ob_ptr = NULL;
		if (rc)
			ERR("FAILED: riomp_dma_unmap_memory rc %d:%s\n",
						rc, strerror(errno));
	};

	if (info->ob_handle) {
		rc = riomp_dma_obwin_free(info->mp_h, &info->ob_handle);
		if (rc)
			ERR("FAILED: riomp_dma_obwin_free rc %d:%s\n",
						rc, strerror(errno));
	};

	info->ob_valid = 0;
	info->ob_handle = 0;
	info->ob_byte_cnt = 0;
	info->ob_ptr = NULL;
};

void direct_io_goodput(struct worker *info)
{
	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, window size or acc_size is 0\n");
		return;
	};

	if (!info->ob_byte_cnt) {
		ERR("FAILED: ob_byte_cnt is 0\n");
		return;
	};

	if (direct_io_obwin_map(info))
		goto exit;

	zero_stats(info);
	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		
		uint64_t cnt;

		for (cnt = 0; (cnt < info->byte_cnt) && !info->stop_req;
							cnt += info->acc_size) {
			volatile void * volatile ptr;

			ptr = (void *)((uint64_t)info->ob_ptr + cnt);

			direct_io_tx(info, ptr);
		};

		info->perf_byte_cnt += info->byte_cnt;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
		incr_direct_io_data(info);
	};
exit:
	direct_io_obwin_unmap(info);
};
					
void direct_io_tx_latency(struct worker *info)
{
	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, window size or acc_size is 0\n");
		return;
	};

	if (!info->ob_byte_cnt) {
		ERR("FAILED: ob_byte_cnt is 0\n");
		return;
	};

	if (info->byte_cnt != info->acc_size) {
		ERR("WARNING: access size == byte count for latency\n");
		info->byte_cnt = info->acc_size;
	};

	if (!info->ib_valid && info->wr) {
		ERR("FAILED: ibwin is not valid!\n");
		return;
	};
		
	if (direct_io_obwin_map(info))
		goto exit;

	zero_stats(info);
	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		if (info->wr) {
			init_direct_io_rx_data(info, info->ob_ptr);
			init_direct_io_rx_data(info, info->ib_ptr);
		};

		start_iter_stats(info);

		direct_io_tx(info, info->ob_ptr);
		if (direct_io_wait_for_change(info))
			goto exit;
		finish_iter_stats(info);

		info->perf_byte_cnt += info->byte_cnt;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
		incr_direct_io_data(info);
	};
exit:
	direct_io_obwin_unmap(info);
};
					
void direct_io_rx_latency(struct worker *info)
{
	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, window size or acc_size is 0\n");
		return;
	};

	if (!info->ob_byte_cnt) {
		ERR("FAILED: ob_byte_cnt is 0\n");
		return;
	};

	if (info->byte_cnt != info->acc_size) {
		ERR("WARNING: access size == byte count for latency\n");
		info->byte_cnt = info->acc_size;
	};

	if (!info->ib_valid) {
		ERR("FAILED: ibwin is not valid!\n");
		return;
	};

	if (direct_io_obwin_map(info))
		goto exit;

	zero_stats(info);
	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		if (direct_io_wait_for_change(info))
			goto exit;
		direct_io_tx(info, info->ob_ptr);
		incr_direct_io_data(info);
	};
exit:
	direct_io_obwin_unmap(info);

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

int single_dma_access(struct worker *info, uint64_t offset)
{
	int dma_rc = 0;

	do {
		if (info->use_kbuf && info->wr)
			dma_rc = riomp_dma_write_d(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						info->rdma_kbuff,
						offset,
						info->acc_size,
						info->dma_trans_type,
						info->dma_sync_type);

		if (info->use_kbuf && !info->wr)
			dma_rc = riomp_dma_read_d(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						info->rdma_kbuff,
						offset,
						info->acc_size,
						info->dma_sync_type);

		if (!info->use_kbuf && info->wr)
			dma_rc = riomp_dma_write(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						ADDR_P(info->rdma_ptr, offset),
						info->acc_size,
						info->dma_trans_type,
						info->dma_sync_type);

		if (!info->use_kbuf && info->wr)
			dma_rc = riomp_dma_read(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						ADDR_P(info->rdma_ptr, offset),
						info->acc_size,
						info->dma_sync_type);
	} while ((EINTR == dma_rc) || (EBUSY == dma_rc) || (EAGAIN == dma_rc));

	return dma_rc;
};

void dma_goodput(struct worker *info)
{
	int dma_rc;
	uint8_t * volatile rx_flag;
	uint8_t * volatile tx_flag;

	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, byte_cnd or access size is 0!\n");
		return;
	};

	if (!info->rdma_buff_size) {
		ERR("FAILED: rdma_buff_size is 0!\n");
		return;
	};

	if (dma_tx_lat == info->action) {
		if ((!info->ib_valid || (NULL == info->ib_ptr)) && info->wr) {
			ERR("FAILED: Must do IBA before measuring latency.\n");
			return;
		};
		if (info->byte_cnt != info->acc_size)  {
			ERR("WARNING: For latency, acc_size = byte count.\n");
		};
		info->acc_size = info->byte_cnt;
	};


	if (alloc_dma_tx_buffer(info))
		goto exit;

	zero_stats(info);

	rx_flag = (uint8_t * volatile)info->ib_ptr + info->byte_cnt - 1;
	tx_flag = (uint8_t * volatile)info->rdma_ptr +
			info->byte_cnt - 1;

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		uint64_t cnt;

		if (dma_tx_lat == info->action) {
			start_iter_stats(info);
			*rx_flag = info->perf_iter_cnt + 1;
		};
		*tx_flag = info->perf_iter_cnt;

		/* Note: when info->action == dma_tx_lat, the loop below
		* will go through one iteration.
		*/
		for (cnt = 0; (cnt < info->byte_cnt) && !info->stop_req;
							cnt += info->acc_size) {
			dma_rc = single_dma_access(info, cnt);

			if (RIO_DIRECTIO_TRANSFER_ASYNC == info->dma_sync_type)
			{
				do {
					dma_rc = riomp_dma_wait_async(
						info->mp_h, dma_rc, 0);
				} while ((EINTR == dma_rc) || (EBUSY == dma_rc)
					|| (EAGAIN == dma_rc));
			};
			if (dma_rc) {
				ERR("FAILED: dma transfer rc %d:%s\n",
						dma_rc, strerror(errno));
				goto exit;
			};
		};
		if (dma_tx_lat == info->action) {
			uint64_t dly = 1000000000;
			uint64_t st_dlay = dly;
			uint8_t iter_cnt_as_byte = info->perf_iter_cnt;

			if (info->wr) {
				while (dly && (*rx_flag != iter_cnt_as_byte))
					dly--;
			};
			
			finish_iter_stats(info);

			if (dly) {
				INFO("Response after %d iterations.\n", 
					st_dlay - dly);
			} else {
				ERR("FAILED: No response in %d checks\n",
					st_dlay);
				break;
			};
		} else {
			info->perf_iter_cnt++;
		};
		info->perf_byte_cnt += info->byte_cnt;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);

	};
exit:
	dealloc_dma_tx_buffer(info);
};
	
void dma_rx_latency(struct worker *info)
{
	uint8_t * volatile rx_flag;
	uint8_t * volatile tx_flag;

	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, byte_cnd or access size is 0!\n");
		return;
	};

	if (!info->rdma_buff_size) {
		ERR("FAILED: rdma_buff_size is 0!\n");
		return;
	};

	if (!info->ib_valid || (NULL == info->ib_ptr))  {
		ERR("FAILED: Must do IBA before measuring latency.\n");
		return;
	};

	if (info->byte_cnt != info->acc_size)  {
		ERR("WARNING: For latency, acc_size = byte count.\n");
	};

	if (alloc_dma_tx_buffer(info))
		goto exit;

	rx_flag = (uint8_t * volatile)info->ib_ptr + info->byte_cnt - 1;

	tx_flag = (uint8_t * volatile)info->rdma_ptr +
			info->byte_cnt - 1;
	info->perf_iter_cnt = 0;
	*tx_flag = info->perf_iter_cnt;
	*rx_flag = info->perf_iter_cnt + 1;

	while (!info->stop_req) {
		int dma_rc;
		uint8_t iter_cnt_as_byte;

		iter_cnt_as_byte = info->perf_iter_cnt;
		*tx_flag = iter_cnt_as_byte;
		while ((*rx_flag != iter_cnt_as_byte) && !info->stop_req)
			{};
		*rx_flag = info->perf_iter_cnt + 2;

		dma_rc = single_dma_access(info, 0);
		info->perf_iter_cnt ++;
		*tx_flag = info->perf_iter_cnt;
		if (dma_rc) {
			ERR("FAILED: dma transfer rc %d:%s\n",
					dma_rc, strerror(errno));
			goto exit;
		};
	};
exit:
	dealloc_dma_tx_buffer(info);
};

#define FOUR_KB 4096

int alloc_msg_tx_rx_buffs(struct worker *info)
{
	int rc;

	rc = riomp_sock_request_send_buffer(info->con_skt, &info->sock_tx_buf);
	if (rc) {
		ERR("FAILED: riomp_sock_request_send_buffer rc %d:%s\n",
			rc, strerror(errno));
		return rc;
	};

	if (NULL == info->sock_rx_buf)
		info->sock_rx_buf = malloc(FOUR_KB);

	return (NULL == info->sock_rx_buf);
};

void msg_cleanup_con_skt(struct worker *info)
{
	int rc;

	if (info->sock_rx_buf) {
		rc = riomp_sock_release_receive_buffer(info->con_skt, 
					info->sock_rx_buf);
		info->sock_rx_buf = NULL;
		if (rc)
			ERR("riomp_sock_release_receive_buffer rc con_skt %d:%s\n",
				rc, strerror(errno));
	};

	if (info->sock_tx_buf) {
		rc = riomp_sock_release_send_buffer(info->con_skt, 
						info->sock_tx_buf);
		info->sock_tx_buf = NULL;
		if (rc) {
			ERR("riomp_sock_release_send_buffer rc con_skt %d:%s\n",
				rc, strerror(errno));
		};
	};

	if (2 == info->con_skt_valid) {
		rc = riomp_sock_close(&info->con_skt);
		info->con_skt_valid = 0;
		if (rc) {
			ERR("riomp_sock_close rc con_skt %d:%s\n",
					rc, strerror(errno));
		};
	};
	info->con_skt_valid = 0;

};

void msg_cleanup_acc_skt(struct worker *info) {

	if (info->acc_skt_valid) {
		int rc = riomp_sock_close(&info->acc_skt);
		if (rc)
			ERR("riomp_sock_close acc_skt rc %d:%s\n",
					rc, strerror(errno));
		info->acc_skt_valid = 0;
	};
};

void msg_cleanup_mb(struct worker *info)
{
	if (info->mb_valid) {
        	int rc = riomp_sock_mbox_destroy_handle(&info->mb);
		if (rc)
			ERR("FAILED: riomp_sock_mbox_destroy_handle rc %d:%s\n",
					rc, strerror(errno));
		info->mb_valid = 0;
	};
};

int send_resp_msg(struct worker *info)
{
	int rc;
	const struct timespec ten_usec = {0, 10 * 1000};
	nanosleep(&ten_usec, NULL);
	errno = 0;

	do {
		rc = riomp_sock_send(info->con_skt, info->sock_tx_buf,
					info->msg_size);

		if (rc) {
			if ((errno == ETIME) || (errno == EINTR))
				continue;
			if (errno == EBUSY) {
				nanosleep(&ten_usec, NULL);
				break;
			};
			ERR("FAILED: riomp_sock_send rc %d:%s\n",
							rc, strerror(errno));
                        break;
		};
	} while (((errno == ETIME) || (errno == EINTR) || (errno == EBUSY)) &&
		rc && !info->stop_req);

	return rc;
};

void msg_rx_goodput(struct worker *info)
{
	int rc;

	if (info->mb_valid || info->acc_skt_valid || info->con_skt_valid) {
		ERR("FAILED: mailbox, access socket, or con socket in use.\n");
		return;
	};

	if (!info->sock_num) {
		ERR("FAILED: Socket number cannot be 0.\n");
		return;
	};

        rc = riomp_sock_mbox_create_handle(mp_h_num, 0, &info->mb);
	if (rc) {
		ERR("FAILED: riomp_sock_mbox_create_handle rc %d:%s\n",
			rc, strerror(errno));
		return;
	};

	info->mb_valid = 1;

        rc = riomp_sock_socket(info->mb, &info->acc_skt);
	if (rc) {
		ERR("FAILED: riomp_sock_socket acc_skt rc %d:%s\n",
			rc, strerror(errno));
		return;
	};

	info->acc_skt_valid = 1;

        rc = riomp_sock_bind(info->acc_skt, info->sock_num);
	if (rc) {
		ERR("FAILED: riomp_sock_bind rc %d:%s\n",
			rc, strerror(errno));
		return;
	};

        rc = riomp_sock_listen(info->acc_skt);
	if (rc) {
		ERR("FAILED: riomp_sock_listen rc %d:%s\n", rc, strerror(errno));
		return;
	};

	while (!info->stop_req) {
		int rc;

		if (!info->con_skt_valid) {
                        rc = riomp_sock_socket(info->mb, &info->con_skt);
			if (rc) {
				ERR("FAILED: riomp_sock_socket con_skt rc %d:%s\n",
					rc, strerror(errno));
				goto exit;
			};
			info->con_skt_valid = 1;
		};

		rc = alloc_msg_tx_rx_buffs(info);
		if (rc)
			break;

                rc = riomp_sock_accept(info->acc_skt, &info->con_skt, 1000);
                if (rc) {
                        if ((errno == ETIME) || (errno == EINTR))
                                continue;
			ERR("FAILED: riomp_sock_accept rc %d:%s\n",
				rc, strerror(errno));
                        break;
                };

		info->con_skt_valid = 2;

		zero_stats(info);
		clock_gettime(CLOCK_MONOTONIC, &info->st_time);

		while (!rc && !info->stop_req) {
			rc = riomp_sock_receive(info->con_skt,
				&info->sock_rx_buf, FOUR_KB, 1000);

                	if (rc) {
                        	if ((errno == ETIME) || (errno == EINTR)) {
					rc = 0;
                                	continue;
				};
                        	break;
                	};
			info->perf_msg_cnt++;
			if (message_rx_lat == info->action)
				if (send_resp_msg(info))
					break;

			clock_gettime(CLOCK_MONOTONIC, &info->end_time);
		};
		msg_cleanup_con_skt(info);
        };
exit:
	msg_cleanup_con_skt(info);
	msg_cleanup_acc_skt(info);
	msg_cleanup_mb(info);

};

void msg_tx_goodput(struct worker *info)
{
	int rc;

	if (info->mb_valid || info->acc_skt_valid || info->con_skt_valid) {
		ERR("FAILED: mailbox, access socket, or con socket in use.\n");
		return;
	};

	if (!info->sock_num) {
		ERR("FAILED: Socket number cannot be 0.\n");
		return;
	};

        rc = riomp_sock_mbox_create_handle(mp_h_num, 0, &info->mb);
	if (rc) {
		ERR("FAILED: riomp_sock_mbox_create_handle rc %d:%s\n",
			rc, strerror(errno));
		return;
	};

	info->mb_valid = 1;

        rc = riomp_sock_socket(info->mb, &info->con_skt);
	if (rc) {
		ERR("FAILED: riomp_sock_socket rc %d:%s\n",
			rc, strerror(errno));
		return;
	};

	info->con_skt_valid = 1;

        rc = riomp_sock_connect(info->con_skt, info->did, 0, info->sock_num);
	if (rc) {
		ERR("FAILED: riomp_sock_connect rc %d:%s\n",
			rc, strerror(errno));
		return;
	};

	info->con_skt_valid = 2;

	rc = alloc_msg_tx_rx_buffs(info);

	zero_stats(info);
	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		const struct timespec ten_usec = {0, 10 * 1000};
		nanosleep(&ten_usec, NULL);
		if (message_tx_lat == info->action)
			start_iter_stats(info);

		rc = riomp_sock_send(info->con_skt,
				info->sock_tx_buf, info->msg_size);

                if (rc) {
                        if ((errno == ETIME) || (errno == EINTR))
                                continue;
                        if (errno == EBUSY) {
				nanosleep(&ten_usec, NULL);
                                continue;
			};
			ERR("FAILED: riomp_sock_send rc %d:%s\n",
				rc, strerror(errno));
                        goto exit;
                };

		if (message_tx_lat == info->action) {
			rc = 1;
			while (rc && !info->stop_req) {
				rc = riomp_sock_receive(info->con_skt,
					&info->sock_rx_buf, FOUR_KB, 1000);

                		if (rc) {
                        		if ((errno == ETIME) ||
							(errno == EINTR)) {
                                		continue;
					};
					ERR(
					"FAILED: riomp_sock_receive rc %d:%s\n",
						rc, strerror(errno));
                        		goto exit;
                		};
                	};
			finish_iter_stats(info);
		};

		info->perf_msg_cnt++;
		info->perf_byte_cnt += info->msg_size;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
	};
exit:
	msg_cleanup_con_skt(info);
	msg_cleanup_mb(info);

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
	struct worker *info;

	if (NULL == parm)
		goto exit;

	info = (struct worker *)parm;
	if (NULL == info->umd_dch)
		goto exit;
	
	DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	migrate_thread_to_cpu(&info->umd_fifo_thr);

	if (info->umd_fifo_thr.cpu_req != info->umd_fifo_thr.cpu_req)
		goto exit;

	info->umd_fifo_proc_alive = 1;
	sem_post(&info->umd_fifo_proc_started); 

	while (!info->umd_fifo_proc_must_die) {
		const int cnt = info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8);

		if (!cnt) 
			continue;

		if (info->fifo_ts_idx < MAX_TIMESTAMPS) {
			clock_gettime(CLOCK_MONOTONIC,
					&info->fifo_ts[info->fifo_ts_idx]);
			info->fifo_ts_idx++;
		};

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
exit:
	sem_post(&info->umd_fifo_proc_started); 
	info->umd_fifo_proc_alive = 0;

	pthread_exit(parm);
};

void *umd_mbox_fifo_proc_thr(void *parm)
{
        struct worker *info;

	int idx = -1;
	uint64_t tsF1 = 0, tsF2 = 0;
        const int MHz = getCPUMHz();

        if (NULL == parm) return NULL;

        info = (struct worker *)parm;
        if (NULL == info->umd_mch) return parm;

        migrate_thread_to_cpu(&info->umd_fifo_thr);

        if (info->umd_fifo_thr.cpu_req != info->umd_fifo_thr.cpu_req)
		return parm;

	idx = info->idx;
	memset(&g_FifoStats[idx], 0, sizeof(g_FifoStats[idx]));

	MboxChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

        info->umd_fifo_proc_alive = 1;
        sem_post(&info->umd_fifo_proc_started);

	tsF1 = rdtsc();
	while (!info->umd_fifo_proc_must_die) {
		g_FifoStats[idx].fifo_thr_iter++;

		const uint64_t tss1 = rdtsc();
		const int cnt = info->umd_mch->scanFIFO(info->umd_chan, wi, info->umd_sts_entries*8);
		const uint64_t tss2 = rdtsc();
		if (tss2 > tss1) { g_FifoStats[idx].fifo_deltats_scanfifo += (tss2-tss1); g_FifoStats[idx].fifo_count_scanfifo++; }
		if (0 == cnt) {
			for(int i = 0; i < 20000; i++) {;}
			continue;
		}

                if (info->fifo_ts_idx < MAX_TIMESTAMPS) {
                        clock_gettime(CLOCK_MONOTONIC,
                                        &info->fifo_ts[info->fifo_ts_idx]);
                        info->fifo_ts_idx++;
                }

		const uint64_t tsm1 = rdtsc();
		for (int i = 0; i < cnt; i++) {
                        MboxChannel::WorkItem_t& item = wi[i];

                        uint64_t dT  = 0;
                        float    dTf = 0;
                        if(item.opt.ts_end > item.opt.ts_start) { // Ignore rdtsc wrap-arounds
                                dT = item.opt.ts_end - item.opt.ts_start;
                                dTf = (float)dT / MHz;
                        }
                        switch (item.opt.dtype) {
                        case DTYPE4:
                                info->perf_byte_cnt += info->acc_size;
                                clock_gettime(CLOCK_MONOTONIC, &info->end_time);
                                if(dT > 0) { info->tick_count++; info->tick_total += dT; info->tick_data_total += info->acc_size; }
                                INFO("\n\tFIFO D4 did=%d bd_wp=%u FIFO iter %llu dTick %llu (%f uS)\n",
                                        item.opt.destid,
                                        item.opt.bd_wp, g_FifoStats[idx].fifo_thr_iter, dT, dTf);
                                break;
                        case DTYPE5:
                                INFO("\n\tFinished D5 bd_wp=%u -- FIFO iter %llu dTick %llu (%f uS)u\n",
                                         item.opt.bd_wp, g_FifoStats[idx].fifo_thr_iter, dT, dTf);
                                break;
                        default:
                                INFO("\n\tUNKNOWN BD %d bd_wp=%u, FIFO iter %llu\n",
                                         item.opt.dtype, item.opt.bd_wp,
                                        g_FifoStats[idx].fifo_thr_iter);
                                break;
                        }
			wi[i].valid = 0xdeadabba;
                } // END for WorkItem_t vector

		const uint64_t tsm2 = rdtsc();
		if (tsm2 > tsm1) { g_FifoStats[idx].fifo_deltats_other += (tsm2-tsm1); g_FifoStats[idx].fifo_count_other++; }
//next:
		for(int i = 0; i < 10000; i++) {;}
	}
//exit:
	if (tsF2 > tsF1) { g_FifoStats[idx].fifo_deltats_all = tsF2 - tsF1; }

	sem_post(&info->umd_fifo_proc_started); 
	info->umd_fifo_proc_alive = 0;

        DBG("\n\t%s: EXITING iter=%llu must die? %d\n", __func__, g_FifoStats[idx].fifo_thr_iter, info->umd_fifo_proc_must_die);

	pthread_exit(parm);
}

void UMD_DD(const struct worker* info)
{
	const int MHz = getCPUMHz();

	const int idx = info->idx;

	float    avgTf_scanfifo = 0;
	uint64_t cnt_scanfifo = 0;
	if (g_FifoStats[idx].fifo_count_scanfifo > 0) {
		float avg_tick = (float)g_FifoStats[idx].fifo_deltats_scanfifo / (cnt_scanfifo = g_FifoStats[idx].fifo_count_scanfifo);
		avgTf_scanfifo = (float)avg_tick / MHz;
	}

	float    avgTf_other = 0;
	uint64_t cnt_other = 0;
	if (g_FifoStats[idx].fifo_count_other > 0) {
		float avg_tick = (float)g_FifoStats[idx].fifo_deltats_other / (cnt_other = g_FifoStats[idx].fifo_count_other);
		avgTf_other = (float)avg_tick / MHz;
	}

	char tmp[257] = {0};
	std::stringstream ss; ss << "\n";
	snprintf(tmp, 256, "scanFIFO       avg %fuS total %fuS %llu times\n",
		 avgTf_scanfifo, (float)g_FifoStats[idx].fifo_deltats_scanfifo/MHz, cnt_scanfifo);
	ss<<"\t"<<tmp;
	snprintf(tmp, 256, "other FIFO thr avg %fuS total %fuS %llu times\n",
		 avgTf_other, (float)g_FifoStats[idx].fifo_deltats_other/MHz, cnt_other);
	ss<<"\t"<<tmp;
	if (g_FifoStats[idx].fifo_deltats_all > 0) {
		snprintf(tmp, 256, "FIFO thread total %fuS\n", (float)g_FifoStats[idx].fifo_deltats_all/MHz);
		ss<<"\t"<<tmp;
	}
	CRIT("%s", ss.str().c_str());

	if (info->evlog.size() == 0) return;

	CRIT("\n\tEvlog:\n", NULL);
	write(STDOUT_FILENO, info->evlog.c_str(), info->evlog.size());
}

void calibrate_map_performance(struct worker *info)
{
	int i, j, max = info->umd_tx_buf_cnt;
	std::map<uint32_t, bool> m_bl_busy;
	std::map<uint32_t, uint32_t> m_bl_outstanding;
	std::map<uint64_t, DMAChannel::WorkItem_t> m_pending_work;
	DMAChannel::WorkItem_t wk;

	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec ts_min, ts_max, ts_tot;
	uint64_t fake_win_handle = 0x00000040ff800000;

	memset(&wk, 0, sizeof(wk));

	CRIT("\n\nCalibrating MAP performance for %d runs, %d entries\n",
		info->umd_sts_entries, max);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);

		for  (j = 0; j < max; j++) {
			m_bl_busy[j] = true;
			m_pending_work[fake_win_handle + (i * 0x20)] = wk;
			m_bl_outstanding[j] = m_pending_work.size();
		};
			
		memset(&wk, 0x11, sizeof(wk));

		for  (j = 0; j < max; j++) {
			std::map<uint64_t, DMAChannel::WorkItem_t>::iterator itm;
			m_bl_busy[j] = false;
			m_bl_outstanding[j] = 0 - m_pending_work.size();
			itm = m_pending_work.find(fake_win_handle + (i * 0x20));
			if (itm != m_pending_work.end())
				m_pending_work[fake_win_handle + (i * 0x20)]
					= wk;
			else
				goto fail;
		};

/*
		for  (j = 0; j < max; j++) {
			m_bl_busy.erase(j);
			m_bl_outstanding.erase(j);
			m_pending_work.erase(fake_win_handle + (i * 0x20));
		}
*/
        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		time_track(i, st_time, end_time, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nMAP: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nMAP: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nMAP: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nMAP: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max);
	CRIT("\nMAP: Avg  per iter %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	return;

fail:
	CRIT("\nMAP: Failed, could not find expected m_bl_outstanding");
	
};

void calibrate_array_performance(struct worker *info)
{
	int i, j, max = info->umd_tx_buf_cnt;
	uint8_t *m_bl_busy;
	uint32_t *m_bl_outstanding;
	DMAChannel::WorkItem_t *m_pending_work;
	DMAChannel::WorkItem_t wk;

	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec ts_min, ts_max, ts_tot;

	m_bl_busy = (uint8_t *)malloc(max*sizeof(uint8_t)); 
	m_bl_outstanding =
		(uint32_t *)malloc(max*sizeof(uint32_t)); 
	m_pending_work =
		(DMAChannel::WorkItem_t *)malloc(
			max*sizeof(DMAChannel::WorkItem_t)); 

	memset(&wk, 0, sizeof(wk));

	CRIT("\n\nCalibrating ARRAY performance for %d runs, %d entries\n",
		info->umd_sts_entries, max);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);

		for  (j = 0; j < max; j++) {
			m_bl_busy[j] = true;
			m_pending_work[j] = wk;
			m_bl_outstanding[j] = j;
		};
			
		memset(&wk, 0x11, sizeof(wk));

		for  (j = 0; j < max; j++) {
			m_bl_busy[j] = false;
			m_bl_outstanding[j] = max - j;
			m_pending_work[j] = wk;
		};

        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		time_track(i, st_time, end_time, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nARRAY: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nARRAY: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nARRAY: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nARRAY: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max);
	CRIT("\nARRAY: Avg  per iter %10d %10d\n",
					ts_tot.tv_sec, ts_tot.tv_nsec);
	return;
};

void calibrate_hash_performance(struct worker *info)
{
	int i, j, max = info->umd_tx_buf_cnt;
	ShmHashMap<uint64_t, DMAChannel::WorkItem_t> m_pending_work("DMA Completion Work", max);

	ShmHashMap<int, bool> m_bl_busy("DMA Busy", max);
	ShmHashMap<int, int> m_bl_outstanding("DMA Outstanding", max);
	DMAChannel::WorkItem_t wk;
	bool is_owner = true, parm = true;

	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec ts_min, ts_max, ts_tot;
	uint64_t fake_win_handle = 0x00000040ff800000;

	memset(&wk, 0, sizeof(wk));

	CRIT("\n\nCalibrating HASH performance for %d runs, %d entries\n",
		info->umd_sts_entries, max);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);

		for  (j = 0; j < max; j++) {
			m_bl_busy.insert(j, parm);
			m_pending_work.insert((uint64_t)(fake_win_handle + (i * 0x20)),  wk);
			m_bl_outstanding.insert(j, j);
		};
			
		memset(&wk, 0x11, sizeof(wk));

		for  (j = 0; j < max; j++) {
			DMAChannel::WorkItem_t *m_pending_work_p;
			bool *m_bl_busy_p;
			int *m_bl_outstanding_p;

			m_bl_busy_p = m_bl_busy.find(j, is_owner);
			m_pending_work_p = m_pending_work.find((uint64_t)(fake_win_handle + (i * 0x20)), is_owner);
			m_bl_outstanding_p = m_bl_outstanding.find(j, is_owner);

			*m_bl_busy_p = false;
			*m_bl_outstanding_p = max - j;
			*m_pending_work_p = wk;
		};

        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		time_track(i, st_time, end_time, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nHASH: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nHASH: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nHASH: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nHASH: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max);
	CRIT("\nHASH: Avg  per iter %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	return;
};

void calibrate_pthread_spinlock_performance(struct worker *info)
{
	int i, j, max = 1000000;
	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec ts_min, ts_max, ts_tot;

	pthread_spinlock_t  m_bl_splock;
	
	pthread_spin_init(&m_bl_splock, PTHREAD_PROCESS_PRIVATE);

	CRIT("\n\nCalibrating SPINLOCK performance for %d runs, %d lk/unlks\n"
		, info->umd_sts_entries, max);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max; j++) {
			pthread_spin_lock(&m_bl_splock);
			pthread_spin_unlock(&m_bl_splock);
		};

        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		time_track(i, st_time, end_time, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nSPLOCK: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nSPLOCK: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nSPLOCK: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nSPLOCK: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max);
	CRIT("\nSPLOCK: Avg  per iter %10d %10d\n",
		ts_tot.tv_sec, ts_tot.tv_nsec);
};

struct timespec gt_ts_min, gt_ts_max, gt_ts_tot;

void calibrate_gettime_performance(struct worker *info)
{
	int i, j, max = 1000000;
	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec temp;

	CRIT("\n\nCalibrating GETTIME performance for %d runs, %d gettime()s\n",
		info->umd_sts_entries, max);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max; j++) {
        		clock_gettime(CLOCK_MONOTONIC, &end_time);
		};

        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		time_track(i, st_time, end_time,
			&gt_ts_tot, &gt_ts_min, &gt_ts_max);
	};

	CRIT("\nGETTIME: Min %10d %10d\n", gt_ts_min.tv_sec, gt_ts_min.tv_nsec);
	CRIT("\nGETTIME: Tot %10d %10d\n", gt_ts_tot.tv_sec, gt_ts_tot.tv_nsec);
	gt_ts_tot = time_div(gt_ts_tot, info->umd_sts_entries);
	CRIT("\nGETTIME: Avg %10d %10d\n", gt_ts_tot.tv_sec, gt_ts_tot.tv_nsec);
	CRIT("\nGETTIME: Max %10d %10d\n", gt_ts_max.tv_sec, gt_ts_max.tv_nsec);
	temp = time_div(gt_ts_tot, max);
	CRIT("\nGETTIME: Avg per iter %10d %10d\n",
		temp.tv_sec, temp.tv_nsec);
};

void calibrate_rdtsc_performance(struct worker *info)
{
	int i, j, max = 1000000;
	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec ts_min, ts_max, ts_tot;
	uint64_t ts_start, ts_end, total, total_sec;

	CRIT("\n\nCalibrating RDTSC performance for %d runs, %d gettime()s\n",
		info->umd_sts_entries, max);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		ts_start = rdtsc();
		for (j = 0; j < max; j++) {
			end_time.tv_nsec = rdtsc();
        		clock_gettime(CLOCK_MONOTONIC, &end_time);
		};

        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		ts_end = rdtsc();
		time_track(i, st_time, end_time, &ts_tot, &ts_min, &ts_max);
		total += ts_end - ts_start;
		if (total > 1000000000) {
			total_sec++;
			total -= 1000000000;
		};
	};

	CRIT("\nRDTSC: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nRDTSC: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nRDTSC: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nRDTSC: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	CRIT("\nRDTSC: RDTSC Tot %d %20d \n", total_sec, total);
	CRIT("\nRDTSC: RDTSC Avg %10d \n", total/info->umd_sts_entries); 

	CRIT("\nRDTSC: gettime Min %10d %10d\n",
		gt_ts_min.tv_sec, gt_ts_min.tv_nsec);
	CRIT("\nRDTSC: gettime Avg %10d %10d\n",
		gt_ts_tot.tv_sec, gt_ts_tot.tv_nsec);
	CRIT("\nRDTSC: gettime Max %10d %10d\n",
		gt_ts_max.tv_sec, gt_ts_max.tv_nsec);
	gt_ts_min = time_difference(ts_min, gt_ts_min);
	gt_ts_min = time_difference(ts_tot, gt_ts_tot);
	gt_ts_min = time_difference(ts_max, gt_ts_max);
	CRIT("\nRDTSC: rdtsc impact Min %10d %10d\n",
		gt_ts_min.tv_sec, gt_ts_min.tv_nsec);
	CRIT("\nRDTSC: rdtsc impact Avg %10d %10d\n",
		gt_ts_tot.tv_sec, gt_ts_tot.tv_nsec);
	CRIT("\nRDTSC: rdtsc impact Avg per iter %10d %10d\n",
		gt_ts_tot.tv_sec, gt_ts_tot.tv_nsec/max);
	CRIT("\nRDTSC: rdtsc impact Max %10d %10d\n",
		gt_ts_max.tv_sec, gt_ts_max.tv_nsec);
};

void calibrate_reg_rw_performance(struct worker *info)
{
	int i, j;
	int max_rw = 1000000;
	struct timespec st_time, e_t; 
	struct timespec *end_time = &e_t;
	struct timespec ts_min, ts_max, ts_tot;

	CRIT("\n\nCalibrating RD32DMACHANperformance for %d runs, %d acc\n",
		info->umd_sts_entries, max_rw);
	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max_rw; j++) {
			end_time->tv_nsec =
				info->umd_dch->rd32dmachan(TSI721_DMAC_STS);
		};

        	clock_gettime(CLOCK_MONOTONIC, end_time);
		time_track(i, st_time, e_t, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nRD32DMACHAN: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nRD32DMACHAN: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nRD32DMACHAN: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nRD32DMACHAN: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max_rw);
	CRIT("\nRD32DMACHAN: Each Acc Avg %10d %10d\n",
						ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\n\nCalibrating WR32DMACHANperformance for %d runs, %d acc\n",
		info->umd_sts_entries, max_rw);

	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max_rw; j++) {
			info->umd_dch->wr32dmachan(TSI721_DMAC_STS,
					TSI721_DMAC_STS - TSI721_DMAC_STS);
		};

        	clock_gettime(CLOCK_MONOTONIC, end_time);
		time_track(i, st_time, e_t, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nWR32DMACHAN: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nWR32DMACHAN: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nWR32DMACHAN: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nWR32DMACHAN: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max_rw);
	CRIT("\nWR32DMACHAN: Each Acc Avg %10d %10d\n",
						ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\n\nCalibrating RD32nolock for %d runs, %d acc\n",
		info->umd_sts_entries, max_rw);


	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max_rw; j++) {
			end_time->tv_nsec =
			info->umd_dch->rd32dmachan_nolock(TSI721_DMAC_STS);
		};

        	clock_gettime(CLOCK_MONOTONIC, end_time);
		time_track(i, st_time, e_t, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nRD32nolock: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nRD32nolock: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nRD32nolock: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nRD32nolock: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max_rw);
	CRIT("\nRD32nolock: Each Acc Avg %10d %10d\n",
						ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\n\nCalibrating WR32nolock for %d runs, %d acc\n",
		info->umd_sts_entries, max_rw);


	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max_rw; j++) {
			info->umd_dch->wr32dmachan_nolock(TSI721_DMAC_STS,
					TSI721_DMAC_STS - TSI721_DMAC_STS);
		};

        	clock_gettime(CLOCK_MONOTONIC, end_time);
		time_track(i, st_time, e_t, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nWR32nolock: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nWR32nolock: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nWR32nolock: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nWR32nolock: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max_rw);
	CRIT("\nRD32nolock: Each Acc Avg %10d %10d\n",
						ts_tot.tv_sec, ts_tot.tv_nsec);
};

void calibrate_sched_yield(struct worker *info)
{
	int i, j, max = 10000;
	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/
	struct timespec ts_min, ts_max, ts_tot;

	CRIT("\n\nCalibrating sched_yield for %d runs, %d acc\n",
		info->umd_sts_entries, max);

	for (i = 0; !info->stop_req && (i < info->umd_sts_entries); i++) {
        	clock_gettime(CLOCK_MONOTONIC, &st_time);
		for (j = 0; j < max; j++)
			sched_yield();

        	clock_gettime(CLOCK_MONOTONIC, &end_time);
		time_track(i, st_time, end_time, &ts_tot, &ts_min, &ts_max);
	};

	CRIT("\nSCH_YLD: Min %10d %10d\n", ts_min.tv_sec, ts_min.tv_nsec);
	CRIT("\nSCH_YLD: Tot %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	ts_tot = time_div(ts_tot, info->umd_sts_entries);
	CRIT("\nSCH_YLD: Avg %10d %10d\n", ts_tot.tv_sec, ts_tot.tv_nsec);
	CRIT("\nSCH_YLD: Max %10d %10d\n", ts_max.tv_sec, ts_max.tv_nsec);
	ts_tot = time_div(ts_tot, max);
	CRIT("\nSCH_YLD: Avg per call %10d %10d\n",
		ts_max.tv_sec, ts_max.tv_nsec);
};

void umd_dma_calibrate(struct worker *info)
{
	info->umd_dch =
		new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
								
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto fail;
	};

	calibrate_map_performance(info);
	if (info->stop_req)
		goto exit;
	calibrate_array_performance(info);
	if (info->stop_req)
		goto exit;

	if (info->wr)
		calibrate_hash_performance(info);

	if (info->stop_req)
		goto exit;
	calibrate_gettime_performance(info);
	if (info->stop_req)
		goto exit;
	calibrate_rdtsc_performance(info);
	if (info->stop_req)
		goto exit;
	calibrate_reg_rw_performance(info);
	if (info->stop_req)
		goto exit;
	calibrate_sched_yield(info);

exit:
        info->umd_dch->cleanup();
        delete info->umd_dch;
fail:
	info->umd_dch = NULL;
};

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

static const uint8_t PATTERN[] = { 0xa1, 0xa2, 0xa3, 0xa4, 0xa4, 0xa6, 0xaf, 0xa8 };
#define PATTERN_SZ      sizeof(PATTERN)
#define DMA_RUNPOLL_US 10

void umd_dma_goodput_demo(struct worker *info)
{
	int oi = 0, rc;
	uint64_t cnt;

	if (! umd_check_cpu_allocation(info)) return;

	const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

	info->umd_dch = new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
								
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

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

        info->umd_dch->setInitState();
        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

        info->umd_fifo_proc_must_die = 0;
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
	info->evlog.clear();
/* FIXME: Had a double free in or corruption error on this ??? */
#if 0
        // info->umd_dch->switch_evlog(true);
#endif
	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

        INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%x bcount=%d #buf=%d #fifo=%d\n",
             info->umd_dch->getDestId(),
             info->did, info->rio_addr, info->acc_size,
             info->umd_tx_buf_cnt, info->umd_sts_entries);

	info->umd_dch->trace_dmachan(0x500, 0x12345678);
	while (!info->stop_req) {
		uint64_t iq;
		info->umd_dma_abort_reason = 0;
	
		info->umd_dch->trace_dmachan(0x100, 1);
        	for (cnt = 0; (cnt < info->byte_cnt) && !info->stop_req;
							cnt += info->acc_size) {
			info->dmaopt[oi].destid      = info->did;
			info->dmaopt[oi].bcount      = info->acc_size;
			info->dmaopt[oi].raddr.lsb64 = info->rio_addr;;
			info->dmaopt[oi].raddr.lsb64 += oi * info->acc_size;

			bool q_was_full = false;
			info->umd_dma_abort_reason = 0;
			if (info->umd_dch->queueDmaOpT1(info->umd_tx_rtype,
					info->dmaopt[oi], info->dmamem[oi],
                                        info->umd_dma_abort_reason)) {
				if (info->desc_ts_idx < MAX_TIMESTAMPS) {
					clock_gettime(CLOCK_MONOTONIC,
					&info->desc_ts[info->desc_ts_idx]);
					info->desc_ts_idx ++;
				};
			} else {
				if(info->umd_dma_abort_reason != 0) {
					CRIT("\n\tCould not enqueue T1 cnt=%d oi=%d\n", cnt, oi);
					CRIT("DMA abort %x: %s\n", 
						info->umd_dma_abort_reason,
						DMAChannel::abortReasonToStr(
						info->umd_dma_abort_reason));
					goto exit;
				}
				q_was_full = true;
			};
			
			// Busy-wait for queue to drain
			for (iq = 0; !info->stop_req && q_was_full && 
				(iq < 1000000000) &&
				(info->umd_dch->queueSize() >= Q_THR);
			    	iq++) {
			}

			// Wrap around, do no overwrite last buffer entry
			oi++;
			if ((info->umd_tx_buf_cnt - 1) == oi) {
				oi = 0;
			}
                } // END for transmit burst

		// RX Check

		info->umd_dch->trace_dmachan(0x100, 0x60);
		info->umd_tx_iter_cnt++;
        } // END while NOT stop requested
exit:
        INFO("\n\tDMA %srunning after %d %dus polls\n",
		info->umd_dch->dmaIsRunning()? "": "NOT ", 
			info->perf_msg_cnt, DMA_RUNPOLL_US);

        INFO("\n\tEXITING (FIFO iter=%lu hw RP=%u WP=%u)\n",
                info->umd_dch->m_fifo_scan_cnt,
		info->umd_dch->getFIFOReadCount(),
                info->umd_dch->getFIFOWriteCount());
        info->umd_fifo_proc_must_die = 1;
	if (info->umd_dch)
		info->umd_dch->shutdown();

        pthread_join(info->umd_fifo_thr.thr, NULL);

	info->umd_dch->get_evlog(info->evlog);
        info->umd_dch->cleanup();

	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);
        delete info->umd_dch;

	info->umd_dch = NULL;
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
					info->umd_dma_abort_reason);
	} else {
		rr = info->umd_dch->queueDmaOpT1(info->umd_tx_rtype,
					info->dmaopt[oi], info->dmamem[oi],
					info->umd_dma_abort_reason);
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
	uint64_t cnt;

	info->umd_dch = new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
								
	if (NULL == info->umd_dch) {
		CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
			info->umd_chan, info->mp_num, info->mp_h);
		goto exit;
	};

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

        info->umd_dch->setInitState();
        if (!info->umd_dch->checkPortOK()) {
		CRIT("\n\tPort is not OK!!! Exiting...");
		goto exit;
	};

	zero_stats(info);
	info->evlog.clear();
        //info->umd_dch->switch_evlog(true);

        INFO("\n\tUDMA my_destid=%u destid=%u rioaddr=0x%x bcount=%d #buf=%d #fifo=%d\n",
             info->umd_dch->getDestId(),
             info->did, info->rio_addr, info->acc_size,
             info->umd_tx_buf_cnt, info->umd_sts_entries);

	if(op == 'T' || op == 'R') // Not used for NREAD
		memset(info->ib_ptr, 0,info->acc_size);
	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	// TX Loop
	for (cnt = 0; !info->stop_req; cnt++) {
		info->dmaopt[oi].destid      = info->did;
		info->dmaopt[oi].bcount      = info->acc_size;
		info->dmaopt[oi].raddr.lsb64 = info->rio_addr;;

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
                	if(! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;
                	finish_iter_stats(info);
			}}
			break;
		default: CRIT("\n\t: Invalid operation '%c'\n", op); goto exit;
			break;
		}

		if (info->stop_req) goto exit;

		// Wrap around, do no overwrite last buffer entry
		oi++;
		if ((info->umd_tx_buf_cnt - 1) == oi) {
			oi = 0;
		};
	} // END for infinite transmit

exit:
	if (info->umd_dch)
		info->umd_dch->shutdown();

	info->umd_dch->get_evlog(info->evlog);
        info->umd_dch->cleanup();

	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);
        delete info->umd_dch;

	info->umd_dch = NULL;
}

void umd_mbox_goodput_demo(struct worker *info)
{
	int rc = 0;

	if (! umd_check_cpu_allocation(info)) return;

        info->umd_mch = new MboxChannel(info->mp_num, 1<<info->umd_chan, info->mp_h);

        if (NULL == info->umd_mch) {
                CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
                return;
        };

	if (! info->umd_mch->open_mbox(info->umd_tx_buf_cnt, info->umd_sts_entries)) {
                CRIT("\n\tMboxChannel: Failed to open mbox!");
		delete info->umd_mch;
		return;
	}

	uint64_t tx_ok = 0;
	uint64_t rx_ok = 0;

	const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

        info->umd_mch->setInitState();

        zero_stats(info);
        clock_gettime(CLOCK_MONOTONIC, &info->st_time);
        INFO("\n\tMBOX=%d my_destid=%u destid=%u (dest MBOX=%d) acc_size=%d #buf=%d #fifo=%d\n",
             info->umd_chan,
             info->umd_mch->getDestId(),
             info->did, info->umd_chan_to,
	     info->acc_size,
             info->umd_tx_buf_cnt, info->umd_sts_entries);

 	// Receiver
	if(info->wr == 0) {
		info->umd_mch->set_rx_destid(info->umd_mch->getDeviceId());

		for(int i = 0; i < info->umd_tx_buf_cnt; i++) {
			void* b = calloc(1, PAGE_4K);
      			info->umd_mch->add_inb_buffer(info->umd_chan, b);
		}

        	while (!info->stop_req) {
                	info->umd_dma_abort_reason = 0;
			uint64_t rx_ts = 0;
			while (!info->stop_req && ! info->umd_mch->inb_message_ready(info->umd_chan, rx_ts))
				usleep(1);
			if (info->stop_req) break;

			bool rx_buf = false;
			void* buf = NULL;
			int msg_size = 0;
			uint64_t enq_ts = 0;
			while ((buf = info->umd_mch->get_inb_message(info->umd_chan, msg_size, enq_ts)) != NULL) {
			      rx_ok++; rx_buf = true;
			      DBG("\n\tGot a message of size %d [%s] cnt=%llu\n", msg_size, buf, rx_ok);
			      info->umd_mch->add_inb_buffer(info->umd_chan, buf); // recycle
			}
			if (! rx_buf) {
				ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, rx_ok);
				goto exit_rx;
			}
			if (rx_ts && enq_ts && rx_ts > enq_ts && msg_size > 0) { // not overflown
				const uint64_t dT = rx_ts - enq_ts;
				if (dT > 0) { 
					info->tick_count++;
					info->tick_total += dT;
					info->tick_data_total += msg_size;
				}
			}
		} // END infinite loop


		// Inbound buffers freed in MboxChannel::cleanup

		goto exit_rx;
	} // END Receiver

	// Transmitter
        rc = pthread_create(&info->umd_fifo_thr.thr, NULL,
                            umd_mbox_fifo_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_fifo_proc thread, exiting...");
                goto exit;
        };
        sem_wait(&info->umd_fifo_proc_started);

        if (!info->umd_fifo_proc_alive) {
                CRIT("\n\tumd_fifo_proc thread is dead, exiting..");
                goto exit;
        };

        while (!info->stop_req) {
                info->umd_dma_abort_reason = 0;

                // TX Loop
		MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
		opt.destid = info->did;
		opt.mbox   = info->umd_chan_to;
		char str[PAGE_4K+1] = {0};
                for (int cnt = 0; !info->stop_req; cnt++) {
			bool q_was_full = false;

			snprintf(str, 128, "Mary had a little lamb iter %d\x0", cnt);
		      	if (! info->umd_mch->send_message(opt, str, info->acc_size, q_was_full)) {
				if (! q_was_full) {
					ERR("\n\tsend_message FAILED! TX q size = %d\n", info->umd_mch->queueTxSize(info->umd_chan));
					goto exit;
				}
		      	} else {
				tx_ok++;
                                if (info->desc_ts_idx < MAX_TIMESTAMPS) {
                                        clock_gettime(CLOCK_MONOTONIC,
                                        &info->desc_ts[info->desc_ts_idx]);
                                        info->desc_ts_idx++;
                                }
			}
			if (info->stop_req) break;

			if (q_was_full) ERR("\n\tQueue full for MBOX%d! tx_ok=%llu\n", info->umd_chan, tx_ok);

                        // Busy-wait for queue to drain
                        for (uint64_t iq = 0; !info->stop_req && q_was_full &&
                                (iq < 1000000000) &&
                                (info->umd_mch->queueTxSize(info->umd_chan) >= Q_THR);
                                iq++) {
                        }
		}

                info->umd_tx_iter_cnt++;
        } // END while NOT stop requested

exit:
        info->umd_fifo_proc_must_die = 1;

        pthread_join(info->umd_fifo_thr.thr, NULL);

exit_rx:
        delete info->umd_mch;

        info->umd_mch = NULL;
}

static inline int MIN(int a, int b) { return a < b? a: b; }

void umd_mbox_goodput_latency_demo(struct worker *info)
{
        info->umd_mch = new MboxChannel(info->mp_num, 1<<info->umd_chan, info->mp_h);

        if (NULL == info->umd_mch) {
                CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
                return;
        };

	if (! info->umd_mch->open_mbox(info->umd_tx_buf_cnt, info->umd_sts_entries)) {
                CRIT("\n\tMboxChannel: Failed to open mbox!");
		delete info->umd_mch;
		return;
	}

	uint64_t tx_ok = 0;
	uint64_t rx_ok = 0;

	const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

        info->umd_mch->setInitState();

        zero_stats(info);
        clock_gettime(CLOCK_MONOTONIC, &info->st_time);
        INFO("\n\tMBOX my_destid=%u destid=%u acc_size=%d #buf=%d #fifo=%d\n",
             info->umd_mch->getDestId(),
             info->did, info->acc_size,
             info->umd_tx_buf_cnt, info->umd_sts_entries);

	MboxChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	for(int i = 0; i < info->umd_tx_buf_cnt; i++) {
		void* b = calloc(1, PAGE_4K);
		info->umd_mch->add_inb_buffer(info->umd_chan, b);
	}

 	// Slave/Receiver
	if(info->wr == 0) {
		char msg_buf[4097] = {0};
		info->umd_mch->set_rx_destid(info->umd_mch->getDeviceId());

		MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
		opt.destid = info->did;
		opt.mbox   = info->umd_chan;

        	while (!info->stop_req) {
			bool q_was_full = false;
                	info->umd_dma_abort_reason = 0;
			uint64_t rx_ts = 0;

			while (!info->stop_req && ! info->umd_mch->inb_message_ready(info->umd_chan, rx_ts)) { ; }
			if (info->stop_req) break;

			bool rx_buf = false;
			void* buf = NULL;
			int msg_size = 0;
			uint64_t enq_ts = 0;
			while ((buf = info->umd_mch->get_inb_message(info->umd_chan, msg_size, enq_ts)) != NULL) {
			      rx_ok++; rx_buf = true;
			      memcpy(msg_buf, buf, MIN(PAGE_4K, msg_size));
			      info->umd_mch->add_inb_buffer(info->umd_chan, buf); // recycle
			      DBG("\n\tGot a message of size %d [%s] cnt=%llu\n", msg_size, msg_buf, rx_ok);
			}
			if (! rx_buf) {
				ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, rx_ok);
				if (GetEnv("DEBUG_MBOX")) { for (;;) { ; } } // WEDGE CPU
				goto exit_rx;
			}

			if (info->stop_req) break;

			// Echo message back

		      	if (! info->umd_mch->send_message(opt, msg_buf, msg_size, q_was_full)) {
				if (! q_was_full) {
					ERR("\n\tsend_message FAILED!\n");
					goto exit_rx;
				}
		      	} else { tx_ok++; }

			if (info->stop_req) break;

			if (q_was_full) ERR("\n\tQueue full for MBOX%d! rx_ok=%llu\n", info->umd_chan, rx_ok);

                        // Busy-wait for queue to drain
                        for (uint64_t iq = 0; !info->stop_req && q_was_full &&
                                (iq < 1000000000) &&
                                (info->umd_mch->queueTxSize(info->umd_chan) >= Q_THR);
                                iq++) {
                        }

			DBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
			while (!q_was_full && !info->stop_req && info->umd_mch->scanFIFO(info->umd_chan, wi, info->umd_sts_entries*8) == 0) { ; }
		} // END infinite loop


		// Inbound buffers freed in MboxChannel::cleanup

		goto exit_rx;
	} // END Receiver

	// Master/Transmitter

        while (!info->stop_req) {
                info->umd_dma_abort_reason = 0;

                // TX Loop
		MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
		opt.destid = info->did;
		opt.mbox   = info->umd_chan;
		char str[PAGE_4K+1] = {0};
                for (int cnt = 0; !info->stop_req; cnt++) {
			bool q_was_full = false;

			snprintf(str, 128, "Mary had a little lamb iter %d\x0", cnt);

			start_iter_stats(info);
		      	if (! info->umd_mch->send_message(opt, str, info->acc_size, q_was_full)) {
				if (! q_was_full) {
					ERR("\n\tsend_message FAILED!\n");
					goto exit;
				}
		      	} else { tx_ok++; }
			if (info->stop_req) break;

			if (q_was_full) ERR("\n\tQueue full for MBOX%d! tx_ok=%llu\n", info->umd_chan, tx_ok);

                        // Busy-wait for queue to drain
                        for (uint64_t iq = 0; !info->stop_req && q_was_full &&
                                (iq < 1000000000) &&
                                (info->umd_mch->queueTxSize(info->umd_chan) >= Q_THR);
                                iq++) {
                        }

			DBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
			while (!q_was_full && !info->stop_req && info->umd_mch->scanFIFO(info->umd_chan, wi, info->umd_sts_entries*8) == 0) { ; }

			// Wait from echo from Slave
			uint64_t rx_ts = 0;
			while (!info->stop_req && ! info->umd_mch->inb_message_ready(info->umd_chan, rx_ts)) { ; }
                        if (info->stop_req) break;

                        bool rx_buf = false;
                        void* buf = NULL;
                        int msg_size = 0;
                        uint64_t enq_ts = 0;
                        while ((buf = info->umd_mch->get_inb_message(info->umd_chan, msg_size, enq_ts)) != NULL) {
                              rx_ok++; rx_buf = true;
                              DBG("\n\tGot a message of size %d [%s] cnt=%llu\n", msg_size, buf, tx_ok);
                              info->umd_mch->add_inb_buffer(info->umd_chan, buf); // recycle
                        }
                        if (! rx_buf) {
                                ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, tx_ok);
				if (GetEnv("DEBUG_MBOX")) { for (;;) { ; } } // WEDGE CPU
                                goto exit_rx;
                        }
			finish_iter_stats(info);
		}

                info->umd_tx_iter_cnt++;
        } // END while NOT stop requested

exit:
exit_rx:
        delete info->umd_mch;

        info->umd_mch = NULL;
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
        	case direct_io: direct_io_goodput(info);
				break;
		case direct_io_tx_lat:
				direct_io_tx_latency(info);
				break;
		case direct_io_rx_lat:
				direct_io_rx_latency(info);
				break;
        	case dma_tx:	
			dma_goodput(info);
			break;
        	case dma_tx_lat:	
			dma_goodput(info);
			break;
        	case dma_rx_lat:	
			dma_rx_latency(info);
			break;
        	case message_tx:
        	case message_tx_lat:
				msg_tx_goodput(info);
				break;
        	case message_rx:
        	case message_rx_lat:
				msg_rx_goodput(info);
				break;
        	case alloc_ibwin:
				dma_alloc_ibwin(info);
				break;
        	case free_ibwin:
				dma_free_ibwin(info);
				break;
#ifdef USER_MODE_DRIVER
		case umd_calibrate:
				umd_dma_calibrate(info);
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
		case umd_mbox:
				umd_mbox_goodput_demo(info);
				break;
		case umd_mboxl:
				umd_mbox_goodput_latency_demo(info);
				break;
#endif
		
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
