/* Type definitions for RSKTD Message Processor thread */
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

#include "librsktd.h"
#include "librsktd_dmn.h"
#include "librsktd_dmn_info.h"
#include "librsktd_lib_info.h"

#ifndef __LIBRSKTD_MSG_PROC_H__
#define __LIBRSKTD_MSG_PROC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* AREQ - Receive application request, process and send response
 *        **app, a_req and a_resp are used to communicate with the application
 * A2W - Receive application request, send further request to worker peer,
 *       get response from worker peer and send response to application 
 *        **app, a_req and a_resp are used for application communication. 
 *        **wp, dreq and dresp are used for worker peer communication. 
 * SREQ - Receive request as RSKTD slave, process and send response
 *        **sp, dreq and dresp are used to communicate with the RSKTD slave
 * S2A - Receive request as RSKTD slave, process, send request to
 *       application, get response, and send response to RSKTD peer.  
 *        **sp, dreq and dresp are used to communicate with the RSKTD slave
 *        **app, req_a and resp_a are used to communicate with the application
 *
 * NOTE: When a request is received, allocate & init corresponding respose.
 *       When a request is sent, allocate & init corresponding respose.
 */
#define RSKTD_CM_MSG_SIZE 0x1000

#define RSKTD_PROC_AREQ 1
#define RSKTD_AREQ_SEQ_AREQ 0x10
#define RSKTD_AREQ_SEQ_ARESP 0x11

#define RSKTD_PROC_A2W 2
#define RSKTD_A2W_SEQ_AREQ  0x21
#define RSKTD_A2W_SEQ_DREQ  0x22
#define RSKTD_A2W_SEQ_DRESP 0x23
#define RSKTD_A2W_SEQ_ARESP 0x24

#define RSKTD_PROC_SREQ 3
#define RSKTD_SPEER_SEQ_DREQ 0x31
#define RSKTD_SPEER_SEQ_DRESP 0x32

#define RSKTD_PROC_S2A 4
#define RSKTD_S2A_SEQ_DREQ 0x41
#define RSKTD_S2A_SEQ_AREQ 0x42
#define RSKTD_S2A_SEQ_ARESP 0x43
#define RSKTD_S2A_SEQ_DRESP 0x44

struct librsktd_unified_msg {
	uint32_t msg_type; /* LIBRSKT and RSKTD message types */
	uint32_t proc_type; /* RSKTD_PROC_... */
	uint32_t proc_stage; /* Processing stage, specific to proc_type */
	struct rskt_dmn_speer **sp; /* RSKTD slave for peer */
				/* For S2A, receive dreq, send dresp */
	struct rskt_dmn_wpeer **wp; /* RSKTD worker peer */
				/* For A2W, send dreq, receive dresp */
	struct rsktd_req_msg *dreq; 
	struct rsktd_resp_msg *dresp;
	struct librskt_app **app; /* Application connected to the RSKTD */
	struct librskt_app_to_rsktd_msg *rx; /* App request/response received */
	struct librskt_rsktd_to_app_msg *tx; /* Response/reqest sent to app */
	struct ms_info *loc_ms; /* Memory space used for connect request */
	struct con_skts *closing_skt; /* Connected socket being closed */
};

struct librsktd_msg_proc_info {
	uint32_t msg_proc_alive;
	pthread_t msg_q_thread;
	sem_t msg_q_mutex;
	sem_t msg_q_cnt;
	struct l_head_t msg_q; /* Type is librsktd, FIFO */
	sem_t msg_q_started;
};

extern struct librsktd_msg_proc_info mproc;

int start_msg_proc_q_thread(void);
void halt_msg_proc_q_thread(void);
void enqueue_mproc_msg(struct librsktd_unified_msg *msg);
void dealloc_msg(struct librsktd_unified_msg *msg);
struct librsktd_unified_msg *alloc_msg(uint32_t msg_type,
                                        uint32_t proc_type,
                                        uint32_t proc_stage);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_MSG_PROC_H__ */
