/* Procedures for managing RSKTD Slave Peer connections */
/* A Slave Peer connection accepts commands for this RSKTD to execute and
 * return responses.
 */
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

#include <rapidio_mport_sock.h>

#include "librsktd_msg_proc.h"
#include "librsktd_private.h"

#ifndef __LIBRSKTD_SPEER_H__
#define __LIBRSKTD_SPEER_H__

#ifdef __cplusplus
extern "C" {
#endif

struct rskt_dmn_speer {
	int cm_skt_h_valid;
	riomp_sock_t cm_skt_h;
	int i_must_die;
	int comm_fail;
	struct rskt_dmn_speer **self_ref;
	struct rskt_dmn_speer *self_ref_ref;

	/* Variables set by HELLO message */
	uint32_t ct;
	uint32_t cm_skt_num;
	uint32_t cm_mp;

	/* speer_loop variables */
	pthread_t s_rx;
	sem_t started;
	int alive;
	int got_hello;
	int rx_buff_used;
	int rx_rc;
	union {
		void *rx_buff;
        	struct rsktd_req_msg *req; /* alias for rx_buff */
	};
	int tx_buff_used;
	int tx_rc;
	union {
		void *tx_buff;
        	struct rsktd_resp_msg *resp; /* alias for tx_buff */
	};
	sem_t connect_completed;

	/* speer_loop messaging test infrastructure */
	sem_t req_ready;
	sem_t resp_ready;
};

#define SPEER_THRD_NM_FMT "SPEERx%x"

void start_new_speer(riomp_sock_t new_socket);
void *speer_tx_loop(void *unused);
void close_all_speers(void);
void close_speer(struct rskt_dmn_speer *speer);
int start_speer_handler(uint32_t cm_skt, uint32_t mpnum,
                                        uint32_t num_ms, uint32_t ms_size,
                                        uint32_t skip_ms, uint32_t tst);
void halt_speer_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_SPEER_H__ */
