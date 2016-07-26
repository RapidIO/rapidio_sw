/* LIBRSKT internal rdma and socket structure definitions */
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

#ifndef __LIBRSKT_PRIVATE_H__
#define __LIBRSKT_PRIVATE_H__

#include <stdbool.h>
#include <sys/un.h>
#include <semaphore.h>
#include "rdma_types.h"
#include "librdma.h"
#include "librskt.h"
#include "libcli.h"
#include "liblist.h"
#include "rapidio_mport_mgmt.h"

#include "memops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RSKT_FLAG_INIT 0x01
#define RSKT_FLAG_CLOSING 0x04
#define RSKT_FLAG_CLOSED 0x08
#define RSKT_FLAG_ERROR  (RSKT_FLAG_CLOSED | 0x10)

#define RSKT_FLAG_CLOS_CHK (RSKT_FLAG_CLOSING | RSKT_FLAG_CLOSED)

/* Do not change the order/size of these fields without changing the
 * corresponding code and assumptions in librsktrdma.c!!!
 */

#define RSKT_LOC_TX_WR_PTR_OFFSET 0
#define RSKT_LOC_TX_WR_FLAG_OFFSET 4
#define RSKT_LOC_RX_RD_PTR_OFFSET 8
#define RSKT_LOC_RX_RD_FLAG_OFFSET 12
#define RSKT_REM_RX_WR_PTR_OFFSET 16
#define RSKT_REM_RX_WR_FLAG_OFFSET 20
#define RSKT_REM_TX_RD_PTR_OFFSET 24
#define RSKT_REM_TX_RD_FLAG_OFFSET 28

#define RSKT_LOC_HDR_SIZE 16
#define RSKT_TOT_HDR_SIZE 32

#define RSKT_BUF_HDR_FLAG_INIT		1
#define RSKT_BUF_HDR_FLAG_ZEROED    	2
#define RSKT_BUF_HDR_FLAG_INIT_DONE 	4

#define RSKT_BUF_HDR_FLAG_CLOSING 0x10
#define RSKT_BUF_HDR_FLAG_CLOSED  0x20
#define RSKT_BUF_HDR_CLOSE_CHK  (RSKT_BUF_HDR_FLAG_CLOSING | \
				RSKT_BUF_HDR_FLAG_CLOSED)
#define RSKT_BUF_HDR_FLAG_ERROR 0x80000000

struct rskt_buf_hdr {
	uint32_t loc_tx_wr_ptr; /* Transmit buffer write pointer */
	uint32_t loc_tx_wr_flags; /* Transmit buffer write flags */
	uint32_t loc_rx_rd_ptr; /* Receive buffer read pointer */
	uint32_t loc_rx_rd_flags; /* Receive buffer read flags */
	uint32_t rem_rx_wr_ptr; /* Receive buffer write pointer */
				/* Peers loc_tx_wr_ptr */
	uint32_t rem_rx_wr_flags; /* Receive buffer write flags */
				/* Peers loc_tx_wr_flags */
	uint32_t rem_tx_rd_ptr;  /* Transmit buffer read pointer */
				/* Peers loc_rx_rd_ptr */
	uint32_t rem_tx_rd_flags; /* Transmit buffer read flags */
				/* Peers loc_rx_rd_flags */
	uint8_t tx_buffer[0]; /* Transmit buffer starting address */
};

#define RSKT_RD_IDX 0
#define RSKT_WR_IDX 1
#define RSKT_LF_IDX 2
#define RSKT_RF_IDX 3

#define RSKT_TX_IDX 0
#define RSKT_RX_IDX 1
#define RSKT_HDR_MAX_IDX 2

#define RSKT_ANY_MPORT 0xFF
#define RSKT_ANY_ROUTE 0xFFFFFFFF

#define RSKT_DEVANY 0
#define RSKT_DEV8 1
#define RSKT_DEV16 2
#define RSKT_DEV32 3

struct rskt_sockaddr_internal {
	struct rskt_sockaddr sa;
	uint32_t rtID; /* Device ID to be used for routing */
	uint32_t rtstat;  /* Status of device route */
};

enum rskt_state {
	rskt_uninit = 0,
	rskt_alloced= 1,
	rskt_bound  = 2,
	rskt_noconn = 3, /* Not used */
	rskt_listening = 4,
	rskt_accepting = 5,
	rskt_connecting = 6,
	rskt_connected = 7,
	rskt_shutting_down = 8,
	rskt_close_by_local = 9,
	rskt_close_by_remote = 10,
	rskt_closing = 11,
	rskt_shut_down = 12,
	rskt_closed = 13,
	rskt_max_state = 14
};

extern char *rskt_state_strs[rskt_max_state];

#define SKT_STATE_STR(x) ((x<=rskt_closed)?rskt_state_strs[x]:"Invlid")

#define MAX_MS_NAME 40

struct rskt_socket_stats_t {
	uint32_t tx_bytes; /* total bytes sent */
	uint32_t rx_bytes; /* total bytes received */
	uint32_t tx_trans; /* number of "writes" */
	uint32_t rx_trans; /* number of "reads/recvs" */
};

enum skt_cleanup_ctl {
	skt_rmda_uninit,
	skt_rdma_connector,
	skt_rdma_acceptor
};

struct rskt_socket_t {
	uint8_t debug; /* controls debug for write/read/recv/flush */
	uint32_t max_backlog;
	struct rskt_sockaddr_internal sai; /* Remote socket connection */
	enum skt_cleanup_ctl connector; /* 0 if acceptor, 1 if connector */
	/* Local MSOH */
	char msoh_name[MAX_MS_NAME];
	int msoh_valid;
	mso_h msoh;
	/* Local MSH */
	char msh_name[MAX_MS_NAME];
	int msh_valid;
	ms_h msh;
	/* Local MSUB */
	int msubh_valid;
	msub_h msubh;
	uint32_t msub_sz; /* Total size of msub */
	/* Pointers into locally mapped RDMA msub */
	union {
		volatile uint8_t *msub_p; /* Start of buffer */
		volatile struct rskt_buf_hdr *hdr; /* Tx/Rx pointers Header */
	};
	volatile uint8_t *tx_buf; /* TX data buffer pointer */
	volatile uint8_t *rx_buf; /* RX data buffer pointer */
	uint32_t buf_sz; /* Size of TX and RX data buffers */
	uint64_t rio_addr; /* RapidIO address of remote buffer,
				valid if lib.use_mport is asserted */
	uint64_t phy_addr; /* Physical address of local buffer,
				valid if lib.use_mport is asserted */

	/* Memops stuff -- in lieu of RDMA and libmport/DMA */
	RIOMemOpsIntf* memops;
	DmaMem_t memops_ibwin; /* Managed by RSKTd, we just map a portion of a phy addr IBwin into our address space */

	/* Connected MS */
	char con_msh_name[MAX_MS_NAME];
	conn_h connh;	/* Connection handle - for use in disconnection */
	ms_h con_msh;
	msub_h con_msubh;
	uint32_t con_sz; 
	struct rskt_socket_stats_t stats;
};

struct rskt_handle_t {
	enum rskt_state st; /* Must own mtx to change st */
	struct rskt_sockaddr sa; /* address of local socket,
				  *must own mtx before changing */
	volatile struct rskt_socket_t * volatile skt;
};

extern void rskt_clear_skt(volatile struct rskt_socket_t * volatile skt);

void librskt_bind_cli_cmds(void);

#define RSKT_NUM_SKTS 0x10000

struct librskt_globals {
        int portno;	/* RSKTD port number to connect to */
        int mpnum;	/* RSKTD mport number to connect to */
        int init_ok;	/* Equal to portno when initialization is successful */

	uint32_t ct;	/* Component Tag/destID of RSKTD */
        struct sockaddr_un addr; /* RSKTD Linux socket address */
        int addr_sz;	/* size of addr */
        int fd;		/* Connection to RSKTD */
	int use_mport; /* TRUE if sockets library & daemon use mport, 0x666 if using memops,
			* FALSE if they use rdma.
			* Set by HELLO response from RSKT daemon.
			*/
	riomp_mport_t mp_h;

	int all_must_die; /* When non-zero, all threads exit immediately */
			/* RSKTD must cleanup */

	sem_t msg_tx_mtx;	/* Mutex for access to msg_tx list */
	sem_t msg_tx_cnt;	/* Count of messages to send */
	struct l_head_t msg_tx; /* List of messages waiting for transmission*/
	pthread_t tx_thr;	/* Thread transmitting messages to fd */

	sem_t rsvp_mtx;		/* Mutex for access to rsvp list */
	struct l_head_t rsvp;	/* List of requests waiting for responses */
	pthread_t rsvp_thr;	/* Thread receiving messages from fd */
        uint32_t lib_req_seq;	/* Sequence number for requests to send */

	sem_t req_mtx;		/* Mutex for access to req list */
	sem_t req_cnt;		/* Mutex for access to req list */
	struct l_head_t req;	/* List of messages waiting for processing */
	pthread_t req_thr;	/* Thread for processing requests */

	uint32_t test;		/* Messaging in test mode */

	sem_t skts_mtx;		/* Mutex for access to skts list */
	struct l_head_t skts;   /* List of sockets in use by this app */
				/* Data is **rskt_socket_t */
};

extern struct librskt_globals lib;

/* Control messaging test mode */
void librskt_test_init(uint32_t test);
void librskt_dump_skt(struct cli_env *env, struct rskt_socket_t *skt, 
			int row, int header);
void librskt_setup_ptrs(rskt_h skt);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_PRIVATE_H__ */

