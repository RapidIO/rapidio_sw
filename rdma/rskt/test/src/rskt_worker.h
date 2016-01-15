/* Definition of messages exchanged between the RSKT Daemon and the library */
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

#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include "libunit_test.h"
#include "librsktd_dmn.h"

#ifndef __RSKT_WORKER_H__
#define __RSKT_WORKER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LIB_BIND_TEST 1
#define LIB_LISTEN_TEST 2
#define LIB_ACCEPT_TEST 3
#define LIB_ACCEPT_BUDDY 4

#define SPEER_CONN 5
#define WPEER_ACC 6

#define SPEER_WPEER_DRIVER 7

#define LAST_TEST     8

struct rskt_test_info {
	int num_wkrs;
	int start_sn;
	int end_sn;
	int max_iter;
	int rc;
	volatile int new_req;
	volatile int new_resp;
	rskt_h *skts;
	struct worker *buddy;
	sem_t accepting;
	sem_t done_sema;
/* SPEER TEST FIELDS */
	sem_t speer_acc; /* Sema is init to 0, connect() posts this sema */
			/* SPEER CONN LOOP waits on this sema, in numeric				order from wkr[0] up o wkr[5]. */
	sem_t speer_con; /* Sema is init to 0, accept() posts this sema */
			/* SPEER CONN LOOP waits on this sema, in numeric				order from wkr[0] up o wkr[5]. */
	struct l_head_t speer_req;  /* List of rsktd_req_msg */
	sem_t speer_req_cnt; /* Posts this every time a message is
				sent to speer */
	sem_t speer_req_mtx; /* Use this for mutex on speer_req */
	int speer_resp_err;    /* riomp_sock_receive returns this error */
	struct l_head_t speer_resp;  /* List of rsktd_rsp_msg */
	sem_t speer_resp_cnt; /* Posts this every time a message is
				sent by speer */
	sem_t speer_resp_mtx; /* Use this for mutex on speer_rsp */
	
	struct rsktd_req_msg req;
	struct rsktd_resp_msg resp;
	struct rsktd_resp_msg act_resp;
	int acc_sent;
	int acc_received;
	int con_sent;
	int con_received;
	int sp_idx; /* test_speer_wpeer_driver configuration */
	int wp_idx; 
	uint32_t speer_acc_sn;
	uint32_t speer_seq_no;
	uint32_t speer_sn; 
	uint32_t speer_cm_skt_num;
	uint32_t wpeer_cm_skt;
	sem_t speer_wpeer_init_complete;
	sem_t all_workers_ready;
};
	
#ifdef __cplusplus
}
#endif

#endif /* __RSKT_WORKER_H__ */

