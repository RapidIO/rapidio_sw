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

#ifndef __LIBRSKT_INFO_H__
#define __LIBRSKT_INFO_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "librskt.h"
#include "librskt_list.h"

#define RSKT_NUM_SKTS 0x10000

struct librskt_globals {
        int portno;	/* RSKTD port number to connect to */
        int mpnum;	/* RSKTD mport number to connect to */
        int init_ok;	/* Equal to portno when initialization is successful */

	uint32_t ct;	/* Component Tag/destID of RSKTD */
        struct sockaddr_un addr; /* RSKTD Linux socket address */
        int addr_sz;	/* size of addr */
        int fd;		/* Connection to RSKTD */
	int use_mport; /*   1 if sockets library & daemon use mport directly,
			* 666 if using memops,
			*   0 if they use rdma.
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

	struct rskt_block skts; /* Data structure for tracking sockets */
};

extern struct librskt_globals lib;

/* Control messaging test mode */
void librskt_bind_cli_cmds(void);

void librskt_dump_skt(struct cli_env *env, struct rskt_socket_t *skt, 
			int row, int header);
void librskt_setup_ptrs(rskt_h skt);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_INFO_H__ */

