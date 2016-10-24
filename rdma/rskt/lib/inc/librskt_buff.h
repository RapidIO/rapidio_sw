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

#ifndef __LIBRSKT_BUFF_H__
#define __LIBRSKT_BUFF_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "librskt_socket.h"

#define RSKT_BUF_HDR_FLAG_INIT		0x01
#define RSKT_BUF_HDR_FLAG_ZEROED	0x02
#define RSKT_BUF_HDR_FLAG_INIT_DONE	0x04

#define RSKT_BUF_HDR_FLAG_CLOSING	0x10
#define RSKT_BUF_HDR_FLAG_LP_EXIT 0x00DEAD00

#define RSKT_BUF_HDR_FLAG_ERROR	0x80000000

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

#define INC_PTR(x,y,z) x=htonl((ntohl(x)+y)%z)

int send_bytes(volatile struct rskt_socket_t *skt, void *data, int byte_cnt,
	                struct rdma_xfer_ms_in *hdr_in, int inited);
int update_remote_hdr(struct rskt_socket_t * volatile skt,
	                struct rdma_xfer_ms_in *hdr_in);
uint32_t get_free_bytes(volatile struct rskt_buf_hdr *hdr, uint32_t buf_sz);
#define AVAIL_BYTES_END -1
#define AVAIL_BYTES_ERROR -2
int  get_avail_bytes(struct rskt_buf_hdr volatile *hdr, uint32_t buf_sz);
void read_bytes(struct rskt_socket_t *skt, void *data, uint32_t byte_cnt);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_BUFF_H__ */

