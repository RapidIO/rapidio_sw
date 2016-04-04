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
#include "riodp_mport_lib.h"
#include "liblog.h"

#include "libtime_utils.h"
#include "worker.h"
#include "goodput.h"
#include "mhz.h"

#ifdef USER_MODE_DRIVER
#include "dmachan.h"
#include "lockfile.h"
#include "tun_ipv4.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32(uint32_t crc, const void *buf, size_t size);

void umd_dma_goodput_tun_demo(struct worker *info);
void umd_epwatch_demo(struct worker *info);
void umd_mbox_watch_demo(struct worker *info);
void umd_afu_watch_demo(struct worker *info);

#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
	info->num_trans = 0;

        info-> mb_valid = 0;
        info->acc_skt = NULL;
        info->acc_skt_valid = 0;
        info->con_skt = NULL;
        info->con_skt_valid = 0;
        info->sock_num = 0; 
        info->sock_tx_buf = NULL;
        info->sock_rx_buf = NULL;

	init_seq_ts(&info->desc_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->fifo_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->meas_ts, MAX_TIMESTAMPS);
#ifdef USER_MODE_DRIVER
	info->dma_method = ACCESS_UMD;
	info->owner_func = NULL;
	info->umd_set_rx_fd = NULL;
	info->my_destid = 0xFFFF;
	info->umd_chan = -1;
	info->umd_chan_n = -1;
	info->umd_chan2 = -1;
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
	info->umd_sockp_quit[0] = info->umd_sockp_quit[1] = -1;
	info->umd_epollfd = -1;
	info->umd_mbox_rx_fd = -1;
	info->umd_mbox_tx_fd = -1;
	info->umd_ticks_total_chan2 = 0;
        info->umd_nread_threshold = 0;

	pthread_mutex_init(&info->umd_dma_did_peer_mutex, NULL);

	memset(&info->umd_dci_list, 0, sizeof(info->umd_dci_list));

	info->umd_peer_ibmap = NULL;

	info->umd_dma_did_peer_list_high_wm = 0;
	memset(info->umd_dma_did_peer_list, 0, sizeof(info->umd_dma_did_peer_list));

	info->umd_dma_did_peer.clear();
	info->umd_dma_did_enum_list.clear();

	info->umd_fifo_total_ticks = 0;
	info->umd_fifo_total_ticks_count = 0;

	//if (first_time) {
        	sem_init(&info->umd_fifo_proc_started, 0, 0);
	//};

	info->umd_disable_nread = 0;
	info->umd_push_rp_thr   = 0;
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
		info->umd_fifo_proc_must_die = 1;
		if (info->umd_dch != NULL) info->umd_dch->shutdown();
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
	} while ((EINTR == -dma_rc) || (EBUSY == -dma_rc) || (EAGAIN == -dma_rc));

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
			// FAF transactions go so fast they can overwhelm the
			// small kernel transmit buffer.  Attempt the 
			// transaction until the resource is not busy.
			ts_now_mark(&info->meas_ts, 5);
			do {
				dma_rc = single_dma_access(info, cnt);
				if (dma_rc < 0)
					sleep(0);
// FIXME: when libmport is fixed to return negative return codes,
// this statement should be fixed to check -dma_rc
			} while  ((EBUSY == dma_rc) && 
                        (RIO_DIRECTIO_TRANSFER_FAF == info->dma_sync_type));

			if ((RIO_DIRECTIO_TRANSFER_ASYNC == info->dma_sync_type)
				&& (dma_rc > 0))
			{
				do {
					dma_rc = riomp_dma_wait_async(
						info->mp_h, dma_rc, 0);
				} while ((EINTR == -dma_rc)
					|| (EBUSY == -dma_rc)
					|| (EAGAIN == -dma_rc));
			};
			ts_now_mark(&info->meas_ts, 555);
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

void dma_tx_num_cmd(struct worker *info)
{
	int dma_rc;
	int trans_count;

	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, byte_cnd or access size is 0!\n");
		return;
	};

	if (!info->rdma_buff_size) {
		ERR("FAILED: rdma_buff_size is 0!\n");
		return;
	};

	if (alloc_dma_tx_buffer(info))
		goto exit;

	riomp_mgmt_mport_set_stats(info->mp_h, &info->meas_ts);

	zero_stats(info);

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	for (trans_count = 0; trans_count < info->num_trans; trans_count++) {
		ts_now_mark(&info->meas_ts, 5);
		dma_rc = single_dma_access(info, 0);
		if (dma_rc < 0) {
			ERR("FAILED: dma_rc %d on trans %d!\n",
				dma_rc, trans_count);
			return;
		}
		ts_now_mark(&info->meas_ts, 555);
	};

	clock_gettime(CLOCK_MONOTONIC, &info->end_time);

	while (!info->stop_req) {
		sleep(0);
	}
exit:
	riomp_mgmt_mport_set_stats(info->mp_h, NULL);
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
		if (dma_rc < 0) {
			ERR("FAILED: dma transfer rc %d:%s\n",
					-dma_rc, strerror(errno));
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

		if (rc && (errno == EBUSY)) {
			nanosleep(&ten_usec, NULL);
			break;
		};
	} while (((errno == ETIME) || (errno == EINTR) || (errno == EBUSY) ||
		(errno == EAGAIN)) && rc && !info->stop_req);

	if (rc)
		ERR("FAILED: riomp_sock_send rc %d:%s\n", rc, strerror(errno));
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
		const int cnt = info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8);
		if (!cnt) 
			continue;

		ts_now(&info->fifo_ts);

		for (int i = 0; i < cnt; i++) {
			DMAChannel::WorkItem_t& item = wi[i];

			switch (item.opt.dtype) {
			case DTYPE1:
			case DTYPE2:
				info->perf_byte_cnt += info->acc_size;
				if (item.opt.ts_end > item.opt.ts_start) {
				       info->umd_fifo_total_ticks += item.opt.ts_end - item.opt.ts_start;
				       info->umd_fifo_total_ticks_count++;
				} 
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

void* umd_mbox_fifo_proc_thr(void *parm)
{
        struct worker* info = NULL;

	int idx = -1;
	uint64_t tsF1 = 0, tsF2 = 0;
        const int MHz = getCPUMHz();

        if (NULL == parm) goto exit;

        info = (struct worker *)parm;
        if (NULL == info->umd_mch) goto exit;

        migrate_thread_to_cpu(&info->umd_fifo_thr);

        if (info->umd_fifo_thr.cpu_req != info->umd_fifo_thr.cpu_req) {
		CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing ou!\n",
		     info->umd_fifo_thr.cpu_req, info->umd_fifo_thr.cpu_req);
		goto exit;
	}

	idx = info->idx;
	memset(&g_FifoStats[idx], 0, sizeof(g_FifoStats[idx]));

	MboxChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

        info->umd_fifo_proc_alive = 1;
        sem_post(&info->umd_fifo_proc_started);

again:
	tsF1 = rdtsc();
	while (!info->umd_fifo_proc_must_die && ! info->stop_req) {
		g_FifoStats[idx].fifo_thr_iter++;

		const uint64_t tss1 = rdtsc();
		const int cnt = info->umd_mch->scanFIFO(wi, info->umd_sts_entries*8);
		const uint64_t tss2 = rdtsc();
		if (tss2 > tss1) { g_FifoStats[idx].fifo_deltats_scanfifo += (tss2-tss1); g_FifoStats[idx].fifo_count_scanfifo++; }
		if (0 == cnt) {
			for(int i = 0; i < 1000; i++) {;}
			continue;
		}

		ts_now(&info->fifo_ts);

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
                                DBG("\n\tFIFO D4 did=%d bd_wp=%u FIFO iter %llu dTick %llu (%f uS)\n",
                                        item.opt.destid,
                                        item.opt.bd_wp, g_FifoStats[idx].fifo_thr_iter, dT, dTf);
                                break;
                        case DTYPE5:
                                DBG("\n\tFinished D5 bd_wp=%u -- FIFO iter %llu dTick %llu (%f uS)u\n",
                                         item.opt.bd_wp, g_FifoStats[idx].fifo_thr_iter, dT, dTf);
                                break;
                        default:
                                CRIT("\n\tUNKNOWN BD %d bd_wp=%u, FIFO iter %llu\n",
                                         item.opt.dtype, item.opt.bd_wp,
                                        g_FifoStats[idx].fifo_thr_iter);
                                break;
                        }
			wi[i].valid = 0xdeadabba;
                } // END for WorkItem_t vector

		const uint64_t tsm2 = rdtsc();
		if (tsm2 > tsm1) { g_FifoStats[idx].fifo_deltats_other += (tsm2-tsm1); g_FifoStats[idx].fifo_count_other++; }

		for(int i = 0; i < 1000; i++) {;}
	}

	if (info->stop_req == SOFT_RESTART) {
		DBG("\n\tSoft restart requested, sleeping on semaphore\n");
		sem_wait(&info->umd_fifo_proc_started); 
		DBG("\n\tAwakened after Soft restart!\n");
		goto again;
	}

	if (tsF2 > tsF1) { g_FifoStats[idx].fifo_deltats_all = tsF2 - tsF1; }
        goto no_post;

exit:
	sem_post(&info->umd_fifo_proc_started); 

no_post:
	info->umd_fifo_proc_alive = 0;

        DBG("\n\t%s: EXITING iter=%llu must die? %d\n", __func__, g_FifoStats[idx].fifo_thr_iter, info->umd_fifo_proc_must_die);

	pthread_exit(parm);
}

void UMD_DDD(const struct worker* info)
{
	const int MHz = getCPUMHz();

	if (info->umd_fifo_total_ticks_count > 0) {
	       float avgTick_uS = ((float)info->umd_fifo_total_ticks / info->umd_fifo_total_ticks_count) / MHz;
	       INFO("\n\tFIFO Avg TX %f uS cnt=%llu\n", avgTick_uS, info->umd_fifo_total_ticks_count);
	}
#if 0
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
#endif
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
	uint64_t ts_start, ts_end, total = 0, total_sec = 0;

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

void umd_dma_calibrate(struct worker *info)
{
	info->umd_dch =
		new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
								
	calibrate_map_performance(info);
	if (info->stop_req)
		goto exit;
	calibrate_array_performance(info);
	if (info->stop_req)
		goto exit;

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
	delete info->umd_lock; info->umd_lock = NULL;
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
	uint64_t cnt = 0;
	int iter = 0;

	if (! umd_check_cpu_allocation(info)) return;
	if (! TakeLock(info, "DMA", info->mp_num, info->umd_chan)) return;

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

	init_seq_ts(&info->desc_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->fifo_ts, MAX_TIMESTAMPS);
	init_seq_ts(&info->meas_ts, MAX_TIMESTAMPS);

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
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = 1; break; }

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
					ts_now(&info->desc_ts);
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

        info->umd_dch->cleanup();

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

	if (! TakeLock(info, "DMA", info->mp_num, info->umd_chan)) return;

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

	if (info->umd_dch->isMaster()) {
		if (!info->umd_dch->alloc_dmatxdesc(info->umd_tx_buf_cnt)) {
			CRIT("\n\talloc_dmatxdesc failed: bufs %d",
								info->umd_tx_buf_cnt);
			goto exit;
		}
		if (!info->umd_dch->alloc_dmacompldesc(info->umd_sts_entries)) {
			CRIT("\n\talloc_dmacompldesc failed: entries %d",
								info->umd_sts_entries);
			goto exit;
		}
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

		if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = 1; break; }

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

			const int N = GetDecParm("$sim", 0) + 1;
			const int M = GetDecParm("$simw", 0);

                	start_iter_stats(info);
			if (GetEnv("sim") == NULL) {
                		if (! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;
			} else {
				// Can we recover/replay BD at sim+1 ?
				for (int i = 0; !q_was_full && i < N; i++) {
					if (! queueDmaOp(info, oi, cnt, q_was_full)) goto exit;

					if (info->stop_req) goto exit;

					for (int j = 0; j < M; j++) {
						if (info->stop_req) goto exit;

						info->dmaopt[oi].bcount = 0x20;

						info->umd_dch->queueDmaOpT1(LAST_NWRITE_R, info->dmaopt[oi], info->dmamem[oi],
						                            info->umd_dma_abort_reason, &info->meas_ts);

						// Wrap around, do no overwrite last buffer entry
						oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; };
					}

					if (i != (N-1)) { // Don't advance oi twice
						// Wrap around, do no overwrite last buffer entry
						oi++; if ((info->umd_tx_buf_cnt - 1) == oi) { oi = 0; };
					}
				}
			}

			if (info->umd_dch->isMaster()) {
				DBG("\n\tPolling FIFO transfer completion destid=%d iter=%llu\n", info->did, cnt);
				while (!q_was_full && !info->stop_req && info->umd_dch->scanFIFO(wi, info->umd_sts_entries*8) == 0) { ; }
			}

			// XXX check for errors, nuke faulting BD, do softRestart

                	finish_iter_stats(info);

			if (7 <= g_level) { // DEBUG
				std::stringstream ss;
				for(int i = 0; i < 16; i++) {
					char tmp[9] = {0};
					snprintf(tmp, 8, "%02x ", wi[0].t2_rddata[i]);
					ss << tmp;
				}
				DBG("\n\tNREAD-in data: %s\n", ss.str().c_str());
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
	if (info->umd_dch)
		info->umd_dch->shutdown();

        info->umd_dch->cleanup();

	// Only allocatd one DMA buffer for performance reasons
	if(info->dmamem[0].type != 0) 
                info->umd_dch->free_dmamem(info->dmamem[0]);

        delete info->umd_dch; info->umd_dch = NULL;
	delete info->umd_lock; info->umd_lock = NULL;
}

void umd_mbox_goodput_demo(struct worker *info)
{
	int rc = 0;
	int iter = 0;

	if (! umd_check_cpu_allocation(info)) return;
	if (! TakeLock(info, "MBOX", info->mp_num, info->umd_chan)) return;

	info->owner_func = umd_mbox_goodput_demo;

        info->umd_mch = new MboxChannel(info->mp_num, info->umd_chan, info->mp_h);
        if (NULL == info->umd_mch) {
                CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
		delete info->umd_lock; info->umd_lock = NULL;
                return;
        };

	if (! info->umd_mch->open_mbox(info->umd_tx_buf_cnt, info->umd_sts_entries)) {
                CRIT("\n\tMboxChannel: Failed to open mbox!");
		delete info->umd_mch;
		delete info->umd_lock; info->umd_lock = NULL;
		return;
	}

	uint64_t tx_ok = 0;
	uint64_t rx_ok = 0;

	const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

        info->umd_mch->setInitState();
	info->umd_mch->softRestart();

	if (GetEnv("verb") != NULL) {
		INFO("\n\tMBOX=%d my_destid=%u destid=%u (dest MBOX=%d letter=%d) acc_size=%d #buf=%d #fifo=%d\n",
		     info->umd_chan,
		     info->umd_mch->getDestId(),
		     info->did, info->umd_chan_to, info->umd_letter,
		     info->acc_size,
		     info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

        zero_stats(info);
        clock_gettime(CLOCK_MONOTONIC, &info->st_time);

 	// Receiver
	if(info->wr == 0) {
		for(int i = 0; i < info->umd_tx_buf_cnt; i++) {
			void* b = calloc(1, PAGE_4K);
      			info->umd_mch->add_inb_buffer(b);
		}

		MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
        	while (!info->stop_req) {
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = 1; break; }

                	info->umd_dma_abort_reason = 0;
			uint64_t rx_ts = 0;
			while (!info->stop_req && ! info->umd_mch->inb_message_ready(rx_ts))
				usleep(1);
			if (info->stop_req) break;

			opt.ts_end = rx_ts;

			bool rx_buf = false;
			void* buf = NULL;
			while ((buf = info->umd_mch->get_inb_message(opt)) != NULL) {
			      rx_ok++; rx_buf = true;
			      DBG("\n\tGot a message of size %d [%s] from destid %u mbox %u cnt=%llu\n", opt.bcount, buf, opt.destid, opt.mbox, rx_ok);
			      info->umd_mch->add_inb_buffer(buf); // recycle
			}
			if (! rx_buf) {
				ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, rx_ok);
				goto exit_rx;
			}
			if (rx_ts && opt.ts_start && rx_ts > opt.ts_start && opt.bcount > 0) { // not overflown
				const uint64_t dT = rx_ts - opt.ts_start;
				if (dT > 0) { 
					info->tick_count++;
					info->tick_total += dT;
					info->tick_data_total += opt.bcount;
				}
			}
		} // END infinite loop


		// Inbound buffers freed in MboxChannel::cleanup

		goto exit_rx;
	} // END Receiver

	// Transmitter
	info->umd_fifo_proc_must_die = 0;
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
		opt.letter = info->umd_letter;
		char str[PAGE_4K+1] = {0};
                for (int cnt = 0; !info->stop_req; cnt++) {
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = 1; break; }

			bool q_was_full = false;
			MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;

			snprintf(str, 128, "Mary had a little lamb iter %d\x0", cnt);
		      	if (! info->umd_mch->send_message(opt, str, info->acc_size, !cnt, fail_reason)) {
				if (fail_reason == MboxChannel::STOP_REG_ERR) {
					ERR("\n\tsend_message FAILED! TX q size = %d\n", info->umd_mch->queueTxSize());
					goto exit;
				} else { q_was_full = true; }
		      	} else {
				tx_ok++;
				ts_now(&info->desc_ts);
			}
			if (info->stop_req) break;

			if (q_was_full) INFO("\n\tQueue full for MBOX%d! tx_ok=%llu\n", info->umd_chan, tx_ok);

                        // Busy-wait for queue to drain
                        for (uint64_t iq = 0; !info->stop_req && q_was_full &&
                                (iq < 1000000000) &&
                                (info->umd_mch->queueTxSize() >= Q_THR);
                                iq++) {
                        }
		}

                info->umd_tx_iter_cnt++;
        } // END while NOT stop requested

exit:
        info->umd_fifo_proc_must_die = 1;

        pthread_join(info->umd_fifo_thr.thr, NULL);

exit_rx:
        delete info->umd_mch; info->umd_mch = NULL;
	delete info->umd_lock; info->umd_lock = NULL;
}

static inline int MIN(int a, int b) { return a < b? a: b; }

void umd_mbox_goodput_latency_demo(struct worker *info)
{
	int iter = 0;

	if (! TakeLock(info, "MBOX", info->mp_num, info->umd_chan)) return;

	info->owner_func = umd_mbox_goodput_latency_demo;

        info->umd_mch = new MboxChannel(info->mp_num, info->umd_chan, info->mp_h);
        if (NULL == info->umd_mch) {
                CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
		delete info->umd_lock; info->umd_lock = NULL;
                return;
        };

	if (! info->umd_mch->open_mbox(info->umd_tx_buf_cnt, info->umd_sts_entries)) {
                CRIT("\n\tMboxChannel: Failed to open mbox!");
		delete info->umd_mch;
		delete info->umd_lock; info->umd_lock = NULL;
		return;
	}

	uint64_t tx_ok = 0;
	uint64_t rx_ok = 0;
	uint64_t big_cnt = 0; // how may attempts to TX a packet

	const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

        info->umd_mch->setInitState();
	info->umd_mch->softRestart();

	if (GetEnv("verb") != NULL) {
		INFO("\n\tMBOX my_destid=%u destid=%u acc_size=%d #buf=%d #fifo=%d\n",
		     info->umd_mch->getDestId(),
		     info->did, info->acc_size,
		     info->umd_tx_buf_cnt, info->umd_sts_entries);
	}

        zero_stats(info);
        clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	MboxChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	for(int i = 0; i < info->umd_tx_buf_cnt; i++) {
		void* b = calloc(1, PAGE_4K);
		info->umd_mch->add_inb_buffer(b);
	}

 	// Slave/Receiver
	if(info->wr == 0) {
		char msg_buf[PAGE_4K+1] = {0};
#ifndef MBOXDEBUG
		strncpy(msg_buf, "Generic pingback - Mary had a little lamb", PAGE_4K);
#endif

		MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
		MboxChannel::MboxOptions_t opt_in; memset(&opt, 0, sizeof(opt_in));
		opt.mbox   = info->umd_chan;
		opt.destid = info->did;

        	while (!info->stop_req) {
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = 1; break; }

			bool q_was_full = false;
                	info->umd_dma_abort_reason = 0;
			uint64_t rx_ts = 0;

			while (!info->stop_req && ! info->umd_mch->inb_message_ready(rx_ts)) { ; }
			if (info->stop_req) break;

			opt.ts_end = rx_ts; 

			bool rx_buf = false;
			void* buf = NULL;
			if ((buf = info->umd_mch->get_inb_message(opt_in)) != NULL) {
			      rx_ok++; rx_buf = true;
#ifdef MBOXDEBUG
			      memcpy(msg_buf, buf, MIN(PAGE_4K, opt_in.bcount));
#endif
			      info->umd_mch->add_inb_buffer(buf); // recycle
#ifdef MBOXDEBUG
			      DBG("\n\tGot a message of size %d [%s] from destid %u mbox %u cnt=%llu\n", opt_in.bcount, msg_buf, opt_in.destid, opt_in.mbox, rx_ok);
#endif
			}
			if (! rx_buf) {
				ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, rx_ok);
				if (GetEnv("DEBUG_MBOX")) { for (;;) { ; } } // WEDGE CPU
				goto exit_rx;
			}

			if (info->stop_req) break;

			// Echo message back
			MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;
		      	if (! info->umd_mch->send_message(opt, msg_buf, opt_in.bcount, false, fail_reason)) {
				if (fail_reason == MboxChannel::STOP_REG_ERR) {
					ERR("\n\tsend_message FAILED!\n");
					goto exit_rx;
				} else { q_was_full = true; }
		      	} else { tx_ok++; }

			if (info->stop_req) break;

			if (q_was_full) INFO("\n\tQueue full for MBOX%d! rx_ok=%llu\n", info->umd_chan, rx_ok);

                        // Busy-wait for queue to drain
                        for (uint64_t iq = 0; !info->stop_req && q_was_full &&
                                (iq < 1000000000) &&
                                (info->umd_mch->queueTxSize() >= Q_THR);
                                iq++) {
                        }

			DDBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
			while (!q_was_full && !info->stop_req && info->umd_mch->scanFIFO(wi, info->umd_sts_entries*8) == 0) { ; }
		} // END infinite loop

		// Note: Inbound buffers freed in MboxChannel::cleanup

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
			if (info->max_iter > 0 && ++iter > info->max_iter) { info->stop_req = 1; break; }

			bool q_was_full = false;
			MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;

			snprintf(str, 128, "Mary had a little lamb iter %d\x0", cnt);

			const bool first_message = !big_cnt;

			if (! first_message) start_iter_stats(info); // We check status reg for 1st ever message to make
                                                                     // sure destid is sane, etc. Do not count
                                                                     // latency and this will take a while
		      	if (! info->umd_mch->send_message(opt, str, info->acc_size, first_message, fail_reason)) {
				if (fail_reason == MboxChannel::STOP_REG_ERR) {
					ERR("\n\tsend_message FAILED!\n");
					goto exit;
				} else { q_was_full = true; }
		      	} else { tx_ok++; }
			if (info->stop_req) break;

			if (q_was_full) INFO("\n\tQueue full for MBOX%d! tx_ok=%llu\n", info->umd_chan, tx_ok);

                        // Busy-wait for queue to drain
                        for (uint64_t iq = 0; !info->stop_req && q_was_full &&
                                (iq < 1000000000) &&
                                (info->umd_mch->queueTxSize() >= Q_THR);
                                iq++) {
                        }

			DDBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
			while (!q_was_full && !info->stop_req && info->umd_mch->scanFIFO(wi, info->umd_sts_entries*8) == 0) { ; }

			// Wait from echo from Slave
			uint64_t rx_ts = 0;
			while (!info->stop_req && ! info->umd_mch->inb_message_ready(rx_ts)) { ; }
                        if (info->stop_req) break;

                        bool rx_buf = false;
                        void* buf = NULL;
                        while ((buf = info->umd_mch->get_inb_message(opt)) != NULL) {
                              rx_ok++; rx_buf = true;
                              DDBG("\n\tGot a message of size %d [%s] cnt=%llu\n", opt.bcount, buf, tx_ok);
                              info->umd_mch->add_inb_buffer(buf); // recycle
                        }
                        if (! rx_buf) {
                                ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, tx_ok);
				if (GetEnv("DEBUG_MBOX")) { for (;;) { ; } } // WEDGE CPU
                                goto exit_rx;
                        }
			if (! first_message) finish_iter_stats(info);
			big_cnt++;
		}

                info->umd_tx_iter_cnt++;
        } // END while NOT stop requested

exit:
exit_rx:
        delete info->umd_mch; info->umd_mch = NULL;
	delete info->umd_lock; info->umd_lock = NULL;
}

const int DESTID_TRANSLATE = 1;

std::map <uint16_t, bool> bad_destid;
std::map <uint16_t, bool> good_destid;

void* umd_mbox_tun_proc_thr(void *parm)
{
        if (NULL == parm) pthread_exit(NULL);

	const int BUFSIZE = 8192;
	uint8_t buffer[BUFSIZE];

        struct worker* info = (struct worker *)parm;
        if (NULL == info->umd_mch) goto exit;

	{{ 
	const int tun_fd = info->umd_tun_fd;
	const int net_fd = info->umd_sockp_quit[1];
	const int maxfd = (tun_fd > net_fd)?tun_fd:net_fd;

        info->umd_mbox_tap_proc_alive = 1;
        sem_post(&info->umd_mbox_tap_proc_started);

	MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));

	const uint16_t my_destid = info->umd_mch->getDestId();
	{{
		uint16_t* p = (uint16_t*)buffer;
		p[0] = htons(my_destid);
		p[1] = htons(info->umd_chan);
	}}

	bad_destid[my_destid] = true;

	opt.mbox = info->umd_chan;

again:
	while(! info->stop_req) {
    		fd_set rd_set;

    		FD_ZERO(&rd_set);
    		FD_SET(tun_fd, &rd_set); FD_SET(net_fd, &rd_set);

		const int ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);
		if (info->stop_req) goto exit;

    		if (ret < 0 && errno == EINTR) continue;

		if (ret < 0) { CRIT("\n\tselect(): %s\n", strerror(errno)); goto exit; }

    		if(FD_ISSET(net_fd, &rd_set)) { goto exit; }

    		if (! FD_ISSET(tun_fd, &rd_set)) continue; // XXX Whoa! Why?

      		// Data from tun/tap: read it, decode destid from 169.254.x.y and write it to RIO
      
      		const int nread = cread(tun_fd, buffer+4, BUFSIZE-4);

		{{
		uint32_t* pkt = (uint32_t*)(buffer+4);
		const uint32_t dest_ip_v4 = ntohl(pkt[4]); // XXX IPv6 will stink big here

		opt.destid = (dest_ip_v4 & 0xFFFF) - DESTID_TRANSLATE;
		}}

		const bool is_bad_destid = bad_destid.find(opt.destid) != bad_destid.end();

#ifdef MBOX_TUN_DEBUG
		const uint32_t crc = crc32(0, buffer, nread+4);
		DBG("\n\tGot from %s %d+4 bytes (L2 CRC32 0x%x) to RIO destid %u%s\n",
		         info->umd_tun_name, nread, 
		         crc, opt.destid,
		         is_bad_destid? " BLACKLISTED": "");
#endif

		if (is_bad_destid) {
			send_icmp_host_unreachable(tun_fd, buffer+4, nread);
			continue;
		}

		DBG("\n\tSending to RIO %d+4 bytes to RIO destid %u\n", nread, opt.destid);

	send_again:
		if (info->stop_req) goto exit;

		const bool first_message = good_destid.find(opt.destid) == good_destid.end();

		MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;
                if (! info->umd_mch->send_message(opt, buffer, nread+4, first_message, fail_reason)) {
                	if (fail_reason == MboxChannel::STOP_REG_ERR) {
                        	ERR("\n\tsend_message FAILED! TX q size = %d\n", info->umd_mch->queueTxSize());

				// XXX ICMPv4 dest unreachable id bad destid - TBI
				
				bad_destid[opt.destid] = true;

				info->stop_req = SOFT_RESTART;
				send_icmp_host_unreachable(tun_fd, buffer+4, nread);
				break;
                        } else { goto send_again; } // Succeed or die trying
                } else {
			if (first_message) good_destid[opt.destid] = true;
		}
	}

	if (info->stop_req == SOFT_RESTART) {
                DBG("\n\tSoft restart requested, sleeping on semaphore\n");
		sem_wait(&info->umd_mbox_tap_proc_started); 
                DBG("\n\tAwakened after Soft restart!\n");
		goto again;
	}

	goto no_post;
	}}
exit:
        sem_post(&info->umd_mbox_tap_proc_started);

no_post:
	info->umd_mbox_tap_proc_alive = 0;

	pthread_exit(parm);
} // END umd_mbox_tun_proc_thr

void umd_mbox_goodput_tun_demo(struct worker *info)
{
	int rc = 0;
	char if_name[IFNAMSIZ] = {0};
	int flags = IFF_TUN | IFF_NO_PI;

	if (! umd_check_cpu_allocation(info)) return;
	if (! TakeLock(info, "MBOX", info->mp_num, info->umd_chan)) return;

	info->owner_func = umd_mbox_goodput_tun_demo;

	memset(info->umd_tun_name, 0, sizeof(info->umd_tun_name));

	// Initialize tun/tap interface
	if ((info->umd_tun_fd = tun_alloc(if_name, flags)) < 0) {
		CRIT("Error connecting to tun/tap interface %s!\n", if_name);
		delete info->umd_lock; info->umd_lock = NULL;
		return;
	}

	strncpy(info->umd_tun_name, if_name, sizeof(info->umd_tun_name)-1);

        info->umd_mch = new MboxChannel(info->mp_num, info->umd_chan, info->mp_h);

        if (NULL == info->umd_mch) {
                CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
        	info->umd_tun_name[0] = '\0';
		close(info->umd_tun_fd); info->umd_tun_fd = -1;
		delete info->umd_lock; info->umd_lock = NULL;
                return;
        };

	if (! info->umd_mch->open_mbox(info->umd_tx_buf_cnt, info->umd_sts_entries)) {
                CRIT("\n\tMboxChannel: Failed to open mbox!");
        	info->umd_tun_name[0] = '\0';
		close(info->umd_tun_fd); info->umd_tun_fd = -1;
		delete info->umd_mch; info->umd_mch = NULL;
		delete info->umd_lock; info->umd_lock = NULL;
		return;
	}

	const int MTU = 4092;
	info->umd_tun_MTU = MTU; // Fixed for MBOX

	char TapIPv4Addr[17] = {0};
	const uint16_t my_destid = info->umd_mch->getDestId() + DESTID_TRANSLATE;
	snprintf(TapIPv4Addr, 16, "169.254.%d.%d", (my_destid >> 8) & 0xFF, my_destid & 0xFF);
	
	char ifconfig_cmd[257] = {0};
	snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s %s netmask 0xffff0000 mtu %d up", if_name, TapIPv4Addr, MTU);
	const int rr = system(ifconfig_cmd);
	if(rr >> 8) {
        	info->umd_tun_name[0] = '\0';
		close(info->umd_tun_fd); info->umd_tun_fd = -1;
		delete info->umd_mch; info->umd_mch = NULL;
		delete info->umd_lock; info->umd_lock = NULL;
		return;
	}

	snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
	system(ifconfig_cmd);

	socketpair(PF_LOCAL, SOCK_STREAM, 0, info->umd_sockp_quit);

	uint64_t rx_ok = 0;

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

        info->umd_mch->setInitState();
	info->umd_mch->softRestart();

	INFO("\n\t%s %s mtu %d on MBOX=%d my_destid=%u #buf=%d #fifo=%d\n",
	     if_name, TapIPv4Addr, MTU,
	     info->umd_chan,
	     info->umd_mch->getDestId(),
	     info->umd_tx_buf_cnt, info->umd_sts_entries);

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

	// Spawn Tap Transmitter Thread
        rc = pthread_create(&info->umd_mbox_tap_thr.thr, NULL,
                            umd_mbox_tun_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_mbox_tun_proc_thr thread, exiting...");
                goto exit;
        };
        sem_wait(&info->umd_mbox_tap_proc_started);

        if (!info->umd_mbox_tap_proc_alive) {
                CRIT("\n\tumd_mbox_tun_proc_thr thread is dead, exiting..");
                goto exit;
        };

        zero_stats(info);
        clock_gettime(CLOCK_MONOTONIC, &info->st_time);

 	// Receiver
	{
		for(int i = 0; i < info->umd_tx_buf_cnt; i++) {
			void* b = calloc(1, PAGE_4K);
      			info->umd_mch->add_inb_buffer(b);
		}

		MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
	again:
        	while (!info->stop_req) {
                	info->umd_dma_abort_reason = 0;
			uint64_t rx_ts = 0;
			while (!info->stop_req && ! info->umd_mch->inb_message_ready(rx_ts))
				usleep(1);
			if (info->stop_req) break;

			opt.ts_end = rx_ts;

			bool rx_buf = false;
			uint8_t* buf = NULL;
			while ((buf = (uint8_t*)info->umd_mch->get_inb_message(opt)) != NULL) {
			      rx_ok++; rx_buf = true;
			      uint16_t* p = (uint16_t*)buf;
			      const uint16_t src_devid = htons(p[0]);
			      good_destid[src_devid] = true;
			      int nwrite = cwrite(info->umd_tun_fd, buf+4, opt.bcount-4); nwrite += 0;
			      info->umd_mch->add_inb_buffer(buf); // recycle
#ifdef MBOX_TUN_DEBUG
			      const uint16_t src_mbox  = htons(p[1]);
			      const uint32_t crc = crc32(0, buf, opt.bcount);
			      DBG("\n\tGot a message of size %d from RIO destid %u mbox %u (L2 CRC32 0x%x) cnt=%llu, wrote %d to %s\n",
				       opt.bcount, src_devid, src_mbox, crc, rx_ok, nwrite, if_name);
#endif
			}
			if (! rx_buf) {
				ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, rx_ok);
				goto exit;
			}
			if (rx_ts && opt.ts_start && rx_ts > opt.ts_start && opt.bcount > 0) { // not overflown
				const uint64_t dT = rx_ts - opt.ts_start;
				if (dT > 0) { 
					info->tick_count++;
					info->tick_total += dT;
					info->tick_data_total += opt.bcount;
				}
			}
		} // END infinite loop

		// Inbound buffers freed in MboxChannel::cleanup

		if (info->stop_req == SOFT_RESTART) {
                	INFO("\n\tSoft restart requested, nuking MBOX hardware!\n");
			info->umd_mch->softRestart();
			info->stop_req = 0;
			sem_post(&info->umd_fifo_proc_started);
			sem_post(&info->umd_mbox_tap_proc_started);
			goto again;
		}		

		goto exit;
	} // END Receiver

exit:
	write(info->umd_sockp_quit[0], "X", 1); // Signal Tun/Tap thread to eXit
        info->umd_fifo_proc_must_die = 1;

        pthread_join(info->umd_fifo_thr.thr, NULL);
        pthread_join(info->umd_mbox_tap_thr.thr, NULL);

	close(info->umd_sockp_quit[0]); close(info->umd_sockp_quit[1]);
	close(info->umd_tun_fd);
	info->umd_sockp_quit[0] = info->umd_sockp_quit[1] = -1;
	info->umd_tun_fd = -1;

        delete info->umd_mch; info->umd_mch = NULL;
	delete info->umd_lock; info->umd_lock = NULL;
	info->umd_tun_MTU = 0;
        info->umd_tun_name[0] = '\0';
} // END umd_mbox_goodput_tun_demo

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
        	case dma_tx_num:	
			dma_tx_num_cmd(info);
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
		case umd_dma_tap:
				umd_dma_goodput_tun_demo(info);
				break;
		case umd_mbox:
				umd_mbox_goodput_demo(info);
				break;
		case umd_mboxl:
				umd_mbox_goodput_latency_demo(info);
				break;
		case umd_mbox_tap:
				umd_mbox_goodput_tun_demo(info);
				break;
		case umd_epwatch:
				umd_epwatch_demo(info);
				break;
		case umd_mbox_watch:
				umd_mbox_watch_demo(info);
				break;
		case umd_afu_watch:
				umd_afu_watch_demo(info);
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
