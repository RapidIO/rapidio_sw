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

#include "time_utils.h"

#ifdef USER_MODE_DRIVER
#include <string>
#include <map>

#include "dmachan.h"
#include "debug.h"
#include "dmadesc.h"
#include "local_endian.h"
#include "mapfile.h"
#include "mport.h"
#include "pciebar.h"
#include "psem.h"
#include "pshm.h"
#include "rdtsc.h"
#include "lockfile.h"
#endif

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
	alloc_ibwin,
	free_ibwin,
	shutdown_worker,
#ifdef USER_MODE_DRIVER
        umd_shm,
	umd_dma,
	umd_dmaltx,
	umd_dmalrx,
	umd_dmalnr,
#endif
	last_action
};

enum req_mode {
	kernel_action,
	user_mode_action
};

#define MIN_RDMA_BUFF_SIZE 0x10000

#ifdef USER_MODE_DRIVER
#define MAX_UMD_BUF_COUNT 0x4000
#endif

struct thread_cpu {
	int cpu_req; /* Requested CPU, -1 means no CPU affinity */
	int cpu_run; /* Currently running on this CPU */
	pthread_t thr; /* Thread being migrated... */
};

#ifdef USER_MODE_DRIVER

#define SOFT_RESTART	69

#endif // USER_MODE_DRIVER

struct worker {
	int idx; /* index of this worker thread -- needed by UMD */
	struct thread_cpu wkr_thr;
	sem_t started;
	int stat; /* 0 - dead, 1 - running, 2 stopped */
	volatile int stop_req; /* 0 - continue, 1 - stop 2 - shutdown, SOFT_RESTART */
	sem_t run;  /* Managed by controller, post this sem to start a stopped woker */
	req_type action;
	req_mode action_mode;
	int did; /* destID */

	uint64_t rio_addr; /* Target RapidIO address for direct IO and DMA */
	uint64_t byte_cnt; /* Number of bytes to access for direct IO and DMA */
	uint64_t acc_size; /* Bytes per transfer for direct IO and DMA */
	int      max_iter; /* For \inft loop tests make this the upper bound of loops*/

	int wr; 
	int mp_num;	/* Mport index */
	int mp_h_is_mine; /* 0 - common mp_h, 1 - worker specific mp_h */
	riomp_mport_t mp_h;

	int ib_valid;
	uint64_t ib_handle; /* Inbound window RapidIO handle */
	uint64_t ib_rio_addr; /* Inbound window RapidIO address */
	uint64_t ib_byte_cnt; /* Inbound window size */
	void *ib_ptr; /* Pointer to mapped ib_handle */

	uint8_t data8_tx;
	uint16_t data16_tx;
	uint32_t data32_tx;
	uint64_t data64_tx;

	uint8_t data8_rx;
	uint16_t data16_rx;
	uint32_t data32_rx;
	uint64_t data64_rx;

	int use_kbuf; 

	uint64_t perf_msg_cnt; /* Messages read/written */
	uint64_t perf_byte_cnt; /* bytes read/written */

	struct timespec st_time; /* Start of the run, for throughput */
	struct timespec end_time; /* End of the run, for throughput*/

	uint64_t perf_iter_cnt; /* Number of repititions */
	struct timespec iter_st_time; /* Start of the iteration, latency */
	struct timespec iter_end_time; /* End of the iteration, latency */
	struct timespec tot_iter_time; /* Total time for all iterations */
	struct timespec min_iter_time; /* Minimum time over all iterations */
	struct timespec max_iter_time; /* Maximum time over all iterations */

	struct timespec fifo_work_time; /* Last time FIFO had work to do */

#ifdef USER_MODE_DRIVER
	void            (*owner_func)(struct worker*);     ///< Who is the owner of this
	void            (*umd_set_rx_fd)(struct worker*, const int);     ///< Who is the owner of this
	uint16_t	my_destid;
	LockFile*	umd_lock;
	int		umd_chan; ///< Local mailbox OR DMA channel
	DMAChannel 	*umd_dch; ///< Used for anything but DMA Tun
	enum dma_rtype	umd_tx_rtype;
	int 		umd_tx_buf_cnt;
	int		umd_sts_entries;
	int		umd_tx_iter_cnt;
	struct thread_cpu umd_fifo_thr;
	sem_t		umd_fifo_proc_started;
	volatile int	umd_fifo_proc_alive;
	volatile int	umd_fifo_proc_must_die;
        uint32_t        umd_dma_abort_reason;
        volatile uint64_t tick_count, tick_total;
        volatile uint64_t tick_data_total;

	volatile uint64_t umd_fifo_total_ticks;
	volatile uint64_t umd_fifo_total_ticks_count;

	void		(*umd_dma_fifo_callback)(struct worker* info);

        RioMport::DmaMem_t dmamem[MAX_UMD_BUF_COUNT];
        DMAChannel::DmaOptions_t dmaopt[MAX_UMD_BUF_COUNT];

	struct seq_ts desc_ts;
	struct seq_ts fifo_ts;
	struct seq_ts meas_ts;

	uint64_t check_abort_stats[256];
#endif
};

/**
 * @brief Returns number of CPUs as reported in /proc/cpuinfo
 */
int getCPUCount();

/**
 * @brief Initializes worker structure
 *
 * @param[in] info Pointer to worker info
 * @param[in] first_time If non-zero, performs one-time initialization
 * @return None, always succeeds
 */

void init_worker_info(struct worker *info, int first_time);

/**
 * @brief Starts a worker thread that performs Direct IO, DMA, and/or messaging
 *
 * @param[in] info Pointer to worker info, must have been initialized by 
 *		init_worker_info prior to this call.
 * @param[in] new_mp_h If <> 0, open mport again to get new DMA channel
 * @param[in] cpu The cpu that should run the thread, -1 for all cpus
 * @return Null pointer, no return status
 */

void start_worker_thread(struct worker *info, int new_mp_h, int cpu);

/**
 * @brief Stops a worker thread and cleans up all resources.
 *
 * @param[in] info Pointer to worker info
 * @return None, always succeeds.
 */

void shutdown_worker_thread(struct worker *info);

#ifdef __cplusplus
}
#endif

#endif /* __WORKER_H__ */