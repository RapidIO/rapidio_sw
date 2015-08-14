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
#ifndef __WORKER_H__
#define __WORKER_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
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
#include <sys/stat.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include "rapidio_mport_dma.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct worker
 *
 * @brief Structure for each worker thread in the goodput environment
 *
 * Every worker thread is passed one of these structures to control what
 * it is doing.
 * 
 */

enum req_type {
	no_action,
	direct_io,
	dma_tx,
	message_tx,
	message_rx,
	alloc_ibwin,
	free_ibwin,
	shutdown_worker,
	last_action
};

enum req_mode {
	kernel_action,
	user_mode_action
};

struct worker {
	pthread_t thr;
	sem_t started;
	int stat; /* 0 - dead, 1 - running, 2 stopped */
	int stop_req; /* 0 - continue, 1 - stop 2 - shutdown */
	sem_t run;  /* Managed by controller, post this sem to start a stopped woker */
	int cpu_req; /* Requested CPU, -1 means no CPU affinity */
	int cpu_run; /* Currently running on this CPU */
	req_type action;
	req_mode action_mode;
	int did; /* destID */

	uint64_t rio_addr; /* Target RapidIO address for direct IO and DMA */
	uint64_t byte_cnt; /* Number of bytes to access for direct IO and DMA */
	uint64_t acc_size; /* Bytes per transfer for direct IO and DMA */
	int wr; 
	int mp_h_is_mine; /* 0 - common mp_h, 1 - worker specific mp_h */
	riomp_mport_t mp_h;

	int ob_valid;
	uint64_t ob_handle; /* Outbound window RapidIO address */
	uint64_t ob_byte_cnt; /* Outbound window size */
	void *ob_ptr; /* Pointer to mapped ob_handle */

	int ib_valid;
	uint64_t ib_handle; /* Inbound window RapidIO handle */
	uint64_t ib_rio_addr; /* Inbound window RapidIO address */
	uint64_t ib_byte_cnt; /* Inbound window size */

	int use_kbuf; 
	enum riomp_dma_directio_type dma_trans_type;
	enum riomp_dma_directio_transfer_sync dma_sync_type;
	uint64_t rdma_kbuff; 
	uint64_t rdma_buff_size; 
	void *rdma_ptr; 

	int mb_valid;
	riomp_mailbox_t mb;
	riomp_sock_t acc_skt;
	int acc_skt_valid;
	riomp_sock_t con_skt;
	int con_skt_valid;
	int msg_size;  /* Minimum 20 bytes for CM messaging!!! */
	uint16_t sock_num; /* RIO CM socket to connect to */
	void *sock_tx_buf; 
	void *sock_rx_buf; 

	uint64_t perf_byte_cnt; /* bytes read/written */
	struct timespec st_time;
	struct timespec end_time;
};

/**
 * @brief Initializes worker structure
 *
 * @param[in] parm Pointer to worker info
 * @param[in] first_time If non-zero, performs one-time initialization
 * @return None, always succeeds
 */

void init_worker_info(struct worker *info, int first_time);

/**
 * @brief Starts a worker thread that performs Direct IO, DMA, and/or messaging
 *
 * @param[in] parm Pointer to worker info, must have been initialized by 
 *		init_worker_info prior to this call.
 * @param[in] new_mp_h If <> 0, open mport again to get new DMA channel
 * @param[in] cpu The cpu that should run the thread, -1 for all cpus
 * @return Null pointer, no return status
 */

void start_worker_thread(struct worker *info, int new_mp_h, int cpu);

/**
 * @brief Stops a worker thread and cleans up all resources.
 *
 * @param[in] parm Pointer to worker info
 * @return None, always succeeds.
 */

void shutdown_worker_thread(struct worker *info);

#ifdef __cplusplus
}
#endif

#endif /* __WORKER_H__ */
