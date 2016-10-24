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

#ifndef __LIBRSKT_SOCKET_H__
#define __LIBRSKT_SOCKET_H__

#include <stdbool.h>
#include <sys/un.h>
#include <semaphore.h>

#include "memops.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "rdma_types.h"
#include "librdma.h"
#include "librsktd.h"
#include "librskt.h"
#include "librskt_states.h"
#include "librskt_buff.h"
#include "libcli.h"
#include "liblist.h"
#include "rapidio_mport_mgmt.h"

struct rskt_sockaddr_internal {
	struct rskt_sockaddr sa;
	uint32_t rtID; /* Device ID to be used for routing */
	uint32_t rtstat;  /* Status of device route */
};

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

#define WR_SKT_CLOSED(x) (x->hdr->loc_tx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING))
#define RD_SKT_CLOSING(x) (x->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_CLOSING))
#define RD_SKT_ERROR(x) (x->hdr->rem_rx_wr_flags & htonl(RSKT_BUF_HDR_FLAG_ERROR))
#define SKT_CONNECTED(x) ((rskt_connected == x->st) || (rskt_closing == x->st))
#define DMA_FLUSHED(x) (RD_SKT_CLOSING(x) && WR_SKT_CLOSED(x))

struct rskt_socket_t {
	struct rskt_sockaddr sa; /* address of local socket,
				  *must own mtx before changing */

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
	DmaMem_t memops_ibwin; /* Managed by RSKTd, we just map a portion
				* of a phy addr IBwin into our address space */
	/* Connected MS */
	char con_msh_name[MAX_MS_NAME];
	conn_h connh;	/* Connection handle - for use in socket close */
	ms_h con_msh;
	msub_h con_msubh;
	uint32_t con_sz; 
	struct rskt_socket_stats_t stats;
};

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_SOCKET_H__ */

