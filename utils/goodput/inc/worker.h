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

#include "libtime_utils.h"

#ifdef USER_MODE_DRIVER
#include <string>
#include <map>

#include "dmachan.h"
#include "rdmaops.h"
#include "mboxchan.h"
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
#include "udma_tun.h"
#include "ibmap.h"
#include "umbox_afu.h"
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
	direct_io,
	direct_io_tx_lat,
	direct_io_rx_lat,
	dma_tx,
	dma_tx_num,
	dma_tx_lat,
	dma_rx_lat,
	message_tx,
	message_tx_lat,
	message_rx,
	message_rx_lat,
	alloc_ibwin,
	free_ibwin,
	shutdown_worker,
#ifdef USER_MODE_DRIVER
	umd_calibrate,
	umd_dma,
	umd_dmaltx,
	umd_dmalrx,
	umd_dmalnr,
	umd_dma_tap,
	umd_mbox,
	umd_mboxl,
	umd_mbox_tap,
	umd_epwatch,
	umd_mbox_watch,
	umd_afu_watch,
#endif
	last_action
};

enum req_mode {
	kernel_action,
	user_mode_action
};

enum {
	ACCESS_UMD   = 42,
	ACCESS_MPORT = -42
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

#define MAX_PEERS		64 ///< Maximum size of cluster

#define MAX_EPOLL_EVENTS	64

typedef struct {
	int			 chan;
	int                      oi;
	int                      tx_buf_cnt; ///< Powers of 2, min 0x20, only (n-2) usable, 1st & last one for T3
	int			 sts_entries;

	volatile uint64_t        ticks_total;
	volatile uint64_t        total_ticks_tx; ///< How many ticks (total) between read from Tun and NWRITE FIFO completion

	RdmaOpsIntf*    rdma;

	RioMport::DmaMem_t       dmamem[MAX_UMD_BUF_COUNT];
	DMAChannel::DmaOptions_t dmaopt[MAX_UMD_BUF_COUNT];
} DmaChannelInfo_t;

/** \brief Stats for the RDMAD-redux */
typedef struct {
	uint16_t destid;        ///< Remote peer's destid
	time_t   on_time;       ///< 1st time it was enumerated by kernel/FMD
	time_t   ls_time;       ///< last time it sent us stuff
	uint64_t my_rio_addr;   ///< My RIO address broadcast to this peer
	int      bcast_cnt_out; ///< How many time we broadcast IBwin mapping to it
	int      bcast_cnt_in;  ///< How many time we received his broadcasts of IBwin mapping
} DmaPeerCommsStats_t;

#endif // USER_MODE_DRIVER

#ifdef USER_MODE_DRIVER
class DmaPeer;
#endif

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

	int ob_valid;
	uint64_t ob_handle; /* Outbound window RapidIO address */
	uint64_t ob_byte_cnt; /* Outbound window size */
	void *ob_ptr; /* Pointer to mapped ob_handle */

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
	enum riomp_dma_directio_type dma_trans_type;
	enum riomp_dma_directio_transfer_sync dma_sync_type;
	uint64_t rdma_kbuff; 
	uint64_t rdma_buff_size; 
	void *rdma_ptr; 
	int num_trans;

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

	struct seq_ts desc_ts;
	struct seq_ts fifo_ts;
	struct seq_ts meas_ts;
#ifdef USER_MODE_DRIVER
	int dma_method; // Only for DMA Tun 0=DMAChannel 1=libmport

	void            (*owner_func)(struct worker*);     ///< Who is the owner of this
	void            (*umd_set_rx_fd)(struct worker*, const int);     ///< Who is the owner of this

	uint16_t	my_destid;
	LockFile*	umd_lock;
	int		umd_chan; ///< Local mailbox OR DMA channel
	int		umd_chan_n; ///< Local mailbox OR DMA channel, forming a range {chan,...,chan_n}
	int		umd_chan2; ///< Local mailbox OR DMA channel
	int		umd_chan_to; ///< Remote mailbox
	int		umd_letter; ///< Remote mailbox letter
	DMAChannel      *umd_dch; ///< Used for anything but DMA Tun
	MboxChannel 	*umd_mch;
	enum dma_rtype	umd_tx_rtype;
	int 		umd_tx_buf_cnt;
	int		umd_sts_entries;
	int		umd_tx_iter_cnt;
	struct thread_cpu umd_fifo_thr;
	sem_t		umd_fifo_proc_started;
	volatile int	umd_fifo_proc_alive;
	volatile int	umd_fifo_proc_must_die;

	struct thread_cpu umd_mbox_tap_thr;

	volatile uint64_t umd_fifo_total_ticks;
	volatile uint64_t umd_fifo_total_ticks_count;

	void		(*umd_dma_fifo_callback)(struct worker* info);

	// Used only for MBOX Tun
        int             umd_tun_fd;
        char            umd_tun_name[33];
        int             umd_tun_MTU;
        int             umd_tun_thruput;

	// Used only for DMA Tun
	int             umd_mbox_tx_fd; // socketpair(2) server for MBOX TX; safer than sharing a MboxChannel instance
	int             umd_mbox_rx_fd; // socketpair(2) server for MBOX RX; sharing a MboxChannel instance is out of question!

	volatile uint64_t umd_ticks_total_chan2;

	DmaChannelInfo_t* umd_dci_nread; ///< Used for NREAD in DMA Tun
	DmaChannelInfo_t* umd_dci_list[8]; // Used for round-robin TX. Only 6 usable!

	int		umd_sockp_quit[2]; ///< Used to signal Tun RX thread to quit
	int             umd_epollfd; ///< Epoll set
	struct thread_cpu umd_dma_tap_thr;

	pthread_mutex_t umd_dma_did_peer_mutex;

        std::map<uint16_t, DmaPeerCommsStats_t> umd_dma_did_enum_list; ///< This is just a list of destids we broadcast to -- populated by EpWatch

	int             umd_dma_did_peer_list_high_wm; ///< High water mark of list below -- maintained by Main Battle Tank thread
	DmaPeer*        umd_dma_did_peer_list[MAX_PEERS]; ///< List of currently up peers -- maintained by Main Battle Tank thread
	std::map<uint16_t, int> umd_dma_did_peer; ///< These are slot into \ref umd_dma_did_peer_list -- maintained by Main Battle Tank thread

	IBwinMap*       umd_peer_ibmap;

	sem_t		umd_mbox_tap_proc_started;
	volatile int	umd_mbox_tap_proc_alive;
	sem_t		umd_dma_tap_proc_started;
	volatile int	umd_dma_tap_proc_alive;
	uint32_t	umd_dma_abort_reason;
	volatile uint64_t tick_count, tick_total;
	volatile uint64_t tick_data_total;

	uint64_t        umd_nread_threshold; ///< Force NREADs (per-peer) if last was warlier than this many rdtsc ticks

	int             umd_dma_bcast_min;	///< Receive "N" bcasts from peer via MBOX before putting up Tun
	int             umd_dma_bcast_interval; ///< Minimum interval in seconds before our broadcasts on MBOX

	// NOT used for DMA Tun
        RioMport::DmaMem_t dmamem[MAX_UMD_BUF_COUNT];
        DMAChannel::DmaOptions_t dmaopt[MAX_UMD_BUF_COUNT];


	volatile int umd_disable_nread;
	volatile int umd_push_rp_thr; ///< Push RP via NWRITE every N packets; 0=after each packet
	volatile int umd_chann_reserve; ///< Reserve chann (if more than 1) for push RP NWRITE
#endif
};

#ifdef USER_MODE_DRIVER
static inline int BD_PAYLOAD_SIZE(const struct worker* info)
{
	if (info == NULL) return -1;
	return DMA_L2_SIZE + info->umd_tun_MTU;
}

#include "dmapeer.h"

#endif // USER_MODE_DRIVER

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
