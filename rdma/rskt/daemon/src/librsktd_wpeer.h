/* Procedures for managing RSKTD Worker Peer connections */
/* A Worker Peer is one that accepts commands and returns responses */
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

#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>

#include "librsktd_msg_proc.h"
#include "librsktd_private.h"
#include "liblist.h"

#ifndef __LIBRSKTD_WPEER_H__
#define __LIBRSKTD_WPEER_H__

#ifdef __cplusplus
extern "C" {
#endif

struct rskt_dmn_wpeer {
	uint32_t ct; /* DestID this WPEER communicates with */
	uint32_t cm_skt; /* Channelized messaging socket on target */

	struct rskt_dmn_wpeer **self_ref;
	struct rskt_dmn_wpeer *self_ref_ref;
	int cm_skt_h_valid;
	riomp_sock_t cm_skt_h;
	int wpeer_alive;
	int i_must_die;
	struct l_item_t *wp_li; /* WPEER entry in dmn.wpeers */

	int tx_buff_used;
	int tx_rc;
	union {
		void *tx_buff;
        	struct rsktd_req_msg *req; /* alias for tx_buff */
	};
	int rx_buff_used;
	union {
		void *rx_buff;
        	struct rsktd_resp_msg *resp; /* alias for tx_buff */
	};
	pthread_t w_rx; /* Thread listening for wpeer responses */
	sem_t started;
	sem_t w_rsp_mutex; /* Mutual exclusion on w_rsp queue */
	struct l_head_t w_rsp; /* List of responses expected from this wpeer */
				/* Item is librsktd_unified_msg, */
				/* ordered by w_seq_num */
	uint32_t w_seq_num; /* Sequence number for requests sent to wpeer */
	uint32_t peer_pid; /* Status in hello response */
};

int open_wpeers_for_requests(int num_peers, struct peer_rsktd_addr *peers);
void enqueue_wpeer_msg(struct librsktd_unified_msg *msg);
void update_wpeer_list(uint32_t destid_cnt, uint32_t *destids);

int start_wpeer_handler(void);
void halt_wpeer_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_WPEER_H__ */
