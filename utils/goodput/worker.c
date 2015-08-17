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

#include "libcli.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "rapidio_mport_dma.h"
#include "liblog.h"

#include "worker.h"
#include "goodput.h"

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
	info->cpu_req = -1;
	info->cpu_run = -1;
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
};

void shutdown_worker_thread(struct worker *info)
{
	int rc;
	if (info->stat) {
		info->action = shutdown_worker;
		info->stop_req = 2;
		sem_post(&info->run);
		pthread_join(info->thr, NULL);
	};
		
	if (info->ib_ptr && info->ib_valid) {
		rc = riomp_dma_unmap_memory(info->mp_h, info->ib_byte_cnt, 
								info->ib_ptr);
		info->ib_ptr = NULL;
		if (rc)
			ERR("Shutdown riomp_dma_unmap_memory ib rc %d: %s\n",
				rc, strerror(errno));
	};

	if (info->ib_valid) {
		info->ib_valid = 0;
		rc = riomp_dma_ibwin_free(info->mp_h, &info->ib_handle);
		if (rc)
			ERR("Shutdown riomp_dma_ibwin_free rc %d: %s\n",
				rc, strerror(errno));
	};

	if (info->ob_ptr && info->ob_valid) {
		rc = riomp_dma_unmap_memory(info->mp_h, info->ob_byte_cnt, 
								info->ob_ptr);
		info->ob_ptr = NULL;
		if (rc)
			ERR("Shutdown riomp_dma_unmap_memory OB rc %d: %s\n",
				rc, strerror(errno));
	};

	if (info->ob_valid) {
		info->ob_valid = 0;
		rc = riomp_dma_obwin_free(info->mp_h, &info->ob_handle);
		if (rc)
			ERR("Shutdown riomp_dma_obwin_free rc %d: %s\n",
				rc, strerror(errno));
	};

	if (info->rdma_kbuff) {
		rc = riomp_dma_dbuf_free(info->mp_h, &info->rdma_kbuff);
		info->rdma_kbuff = 0;
		if (rc)
			ERR("Shutdown riomp_dma_dbuf_free rdma_kbuff rc %d:%s\n",
				rc, strerror(errno));
	};

	if (NULL != info->rdma_ptr) {
		free(info->rdma_ptr);
		info->rdma_ptr = NULL;
	};

	if (info->con_skt_valid) {
		if (info->sock_rx_buf) {
			rc = riomp_sock_release_receive_buffer(info->con_skt, 
						info->sock_rx_buf);
			info->sock_rx_buf = NULL;
			if (rc)
				ERR("Shutdown riomp_sock_release_receive_buffer rc con_skt %d:%s\n",
					rc, strerror(errno));
		};
		if (info->sock_tx_buf) {
			rc = riomp_sock_release_send_buffer(info->con_skt, 
						info->sock_tx_buf);
			info->sock_tx_buf = NULL;
			if (rc) {
				ERR("Shutdown riomp_sock_release_send_buffer rc con_skt %d:%s\n",
					rc, strerror(errno));
			};
		};

		rc = riomp_sock_close(&info->con_skt);
		info->con_skt_valid = 0;
		if (rc) {
			ERR("Shutdown riomp_sock_close rc con_skt %d:%s\n",
					rc, strerror(errno));
		};
	};

	if (info->acc_skt_valid) {
		rc = riomp_sock_close(&info->acc_skt);
		info->acc_skt_valid = 0;
		if (rc)
			ERR("Shutdown riomp_sock_close rc ACC_skt %d:%s\n",
					rc, strerror(errno));
	};

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

int migrate_thread_to_cpu(struct worker *info)
{
        cpu_set_t cpuset;
        int chk_cpu_lim = 10;
	int rc;

	if (-1 == info->cpu_req) {
        	CPU_ZERO(&cpuset);
        	CPU_SET(0, &cpuset);
        	CPU_SET(1, &cpuset);
        	CPU_SET(2, &cpuset);
        	CPU_SET(3, &cpuset);
	} else {
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

void direct_io_goodput(struct worker *info)
{
	uint8_t data8 = 0x12;
	uint16_t data16 = 0x3456;
	uint32_t data32 = 0x789abcde;
	uint64_t data64 = 0xf123456789abcdef;
	int rc;

	if (!info->rio_addr || !info->byte_cnt || !info->acc_size) {
		ERR("FAILED: rio_addr, window size or acc_size is 0\n");
		return;
	};

	if (!info->ob_byte_cnt) {
		ERR("FAILED: ob_byte_cnt is 0\n");
		return;
	};

	rc = riomp_dma_obwin_map(info->mp_h, info->did, info->rio_addr, 
					info->ob_byte_cnt, &info->ob_handle);
	if (rc) {
		ERR("FAILED: riomp_dma_obwin_map rc %d:%s\n",
					rc, strerror(errno));
		return;
	};

	info->ob_valid = 1;
	info->perf_byte_cnt = 0;

	rc = riomp_dma_map_memory(info->mp_h, info->ob_byte_cnt, 
					info->ob_handle, &info->ob_ptr);
	if (rc) {
		ERR("FAILED: riomp_dma_map_memory rc %d:%s\n",
					rc, strerror(errno));
		return;
	};

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		
		uint64_t cnt;

		for (cnt = 0; (cnt < info->byte_cnt) && !info->stop_req;
							cnt += info->acc_size) {
			void *ptr;

			ptr = (void *)((uint64_t)info->ob_ptr + cnt);

			switch (info->acc_size) {
			case 1: if (info->wr)
					*(uint8_t *)ptr = data8;
				else
					data8 = *(uint8_t *)ptr;
				break;

			case 2: if (info->wr)
					*(uint16_t *)ptr = data16;
				else
					data16 = *(uint16_t *)ptr;
				break;
			case 3:
			case 4: if (info->wr)
					*(uint32_t *)ptr = data32;
				else
					data32 = *(uint32_t *)ptr;
				break;

			default: if (info->wr)
					*(uint64_t *)ptr = data64;
				else
					data64 = *(uint64_t *)ptr;
				break;
			};
		};

		info->perf_byte_cnt += info->byte_cnt;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
	};

	if (info->ob_ptr) {
		rc = riomp_dma_unmap_memory(info->mp_h, info->ob_byte_cnt, 
								info->ob_ptr);
		info->ob_ptr = NULL;
		if (rc) {
			ERR("FAILED: riomp_dma_unmap_memory rc %d:%s\n",
						rc, strerror(errno));
			return;
		};
	};


	if (info->ob_valid) {
		rc = riomp_dma_obwin_free(info->mp_h, &info->ob_handle);
		if (rc) {
			ERR("FAILED: riomp_dma_obwin_free rc %d:%s\n",
						rc, strerror(errno));
			return;
		};
	};

	info->ob_valid = 0;
};
					
#define ADDR_L(x,y) ((uint64_t)((uint64_t)x + (uint64_t)y))
#define ADDR_P(x,y) ((void *)((uint64_t)x + (uint64_t)y))

void dma_goodput(struct worker *info)
{
	int dma_rc;

	if (!info->rio_addr || !info->byte_cnt || !info->acc_size)
		return;

	if (!info->rdma_buff_size)
		return;

	if (info->use_kbuf) {
		if (riomp_dma_dbuf_alloc(info->mp_h, info->rdma_buff_size,
					&info->rdma_kbuff))
			return;
	} else {
		info->rdma_ptr = malloc(info->rdma_buff_size);
		if (NULL == info->rdma_ptr)
			return;
	};

	info->perf_byte_cnt = 0;

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	while (!info->stop_req) {
		uint64_t offset = 0;
		uint64_t cnt;

		for (cnt = 0; (cnt < info->byte_cnt) && !info->stop_req;
							cnt += info->acc_size) {
			if (info->use_kbuf) {
				if (info->wr)
					dma_rc = riomp_dma_write_d(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						info->rdma_kbuff,
						offset,
						info->acc_size,
						info->dma_trans_type,
						info->dma_sync_type);
				else 
					dma_rc = riomp_dma_read_d(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						info->rdma_kbuff,
						offset,
						info->acc_size,
						info->dma_sync_type);
			} else {
				if (info->wr)
					dma_rc = riomp_dma_write(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						ADDR_P(info->rdma_ptr, offset),
						info->acc_size,
						info->dma_trans_type,
						info->dma_sync_type);
				else 
					dma_rc = riomp_dma_read(info->mp_h,
						info->did,
						ADDR_L(info->rio_addr, offset),
						ADDR_P(info->rdma_ptr, offset),
						info->acc_size,
						info->dma_sync_type);
			};

			if (RIO_DIRECTIO_TRANSFER_ASYNC == info->dma_sync_type)
				riomp_dma_wait_async(info->mp_h, dma_rc, 0);

			offset += info->acc_size;
		};

		info->perf_byte_cnt += info->byte_cnt;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
	};

	if (info->use_kbuf) {
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
	
#define FOUR_KB 4096

void msg_rx_goodput(struct worker *info)
{
	if (info->mb_valid || info->acc_skt_valid || info->con_skt_valid)
		return;

	if (!info->sock_num)
		return;

        if (riomp_sock_mbox_create_handle(mp_h_num, 0, &info->mb))
		return;

	info->mb_valid = 1;

        if (riomp_sock_socket(info->mb, &info->acc_skt))
		return;

	info->acc_skt_valid = 1;

        if (riomp_sock_bind(info->acc_skt, info->sock_num))
		return;

        if (riomp_sock_listen(info->acc_skt))
		return;

	while (!info->stop_req) {
		int rc;

		if (!info->con_skt_valid) {
                        if (riomp_sock_socket(info->mb, &info->con_skt))
				goto exit;
			info->con_skt_valid = 1;
		};

		if (NULL == info->sock_rx_buf)
			info->sock_rx_buf = malloc(FOUR_KB);

                rc = riomp_sock_accept(info->acc_skt, &info->con_skt, 1000);
                if (rc) {
                        if ((errno == ETIME) || (errno == EINTR))
                                continue;
                        break;
                };

		clock_gettime(CLOCK_MONOTONIC, &info->st_time);
		info->perf_byte_cnt = 0;
		while (!rc && !info->stop_req) {
			int ret = riomp_sock_receive(info->con_skt,
				&info->sock_rx_buf, FOUR_KB, 1000);

                	if (ret) {
                        	if ((errno == ETIME) || (errno == EINTR))
                                	continue;
                        	break;
                	};
			info->perf_byte_cnt++;
			clock_gettime(CLOCK_MONOTONIC, &info->end_time);
		};

		riomp_sock_release_receive_buffer(info->con_skt,
							info->sock_rx_buf);
		info->sock_rx_buf = NULL;
		riomp_sock_close(&info->con_skt);
		info->con_skt_valid = 0;
        };
exit:
	if (info->acc_skt_valid) {
        	riomp_sock_close(&info->acc_skt);
		info->acc_skt_valid = 0;
	};

	if (info->mb_valid) {
        	riomp_sock_mbox_destroy_handle(&info->mb);
		info->mb_valid = 0;
	};
};

void msg_tx_goodput(struct worker *info)
{
	if (info->mb_valid || info->acc_skt_valid || info->con_skt_valid)
		return;

	if (!info->sock_num)
		return;

        if (riomp_sock_mbox_create_handle(mp_h_num, 0, &info->mb))
		return;

	info->mb_valid = 1;

        if (riomp_sock_socket(info->mb, &info->con_skt))
		return;

        if (riomp_sock_connect(info->con_skt, info->did, 0, info->sock_num))
		return;

	info->con_skt_valid = 1;

	if (riomp_sock_request_send_buffer(info->con_skt, &info->sock_tx_buf))
		return;

	clock_gettime(CLOCK_MONOTONIC, &info->st_time);
	info->perf_byte_cnt = 0;

	while (!info->stop_req) {
		int ret = riomp_sock_send(info->con_skt,
				info->sock_tx_buf, info->msg_size);

                if (ret) {
                        if ((errno == ETIME) || (errno == EINTR))
                                continue;
                        break;
                };
		info->perf_byte_cnt += info->msg_size;
		clock_gettime(CLOCK_MONOTONIC, &info->end_time);
	};

	if (NULL != info->sock_tx_buf) {
		riomp_sock_release_send_buffer(info->con_skt,
						info->sock_tx_buf);
		info->sock_tx_buf = NULL;
	};

	riomp_sock_close(&info->con_skt);
	info->con_skt_valid = 0;

	if (info->mb_valid) {
        	riomp_sock_mbox_destroy_handle(&info->mb);
		info->mb_valid = 0;
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

void *worker_thread(void *parm)
{
	struct worker *info = (struct worker *)parm;

	sem_post(&info->started);

	info->stat = 1;
	while (info->stat) {
		if (info->cpu_req != info->cpu_run)
			migrate_thread_to_cpu(info);

		switch (info->action) {
        	case direct_io: direct_io_goodput(info);
				break;
        	case dma_tx:	
			dma_goodput(info);
			break;
        	case message_tx:
				msg_tx_goodput(info);
				break;
        	case message_rx:
				msg_rx_goodput(info);
				break;
        	case alloc_ibwin:
				dma_alloc_ibwin(info);
				break;
        	case free_ibwin:
				dma_free_ibwin(info);
				break;
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

	info->cpu_req = cpu;

	rc = pthread_create(&info->thr, NULL, worker_thread, (void *)info);

	if (!rc)
		sem_wait(&info->started);
};

#ifdef __cplusplus
}
#endif
