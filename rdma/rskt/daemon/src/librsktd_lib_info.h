/* Global state information for RSKTD threads handling library connections */
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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

//#include <rapidio_mport_mgmt.h>
//#include <rapidio_mport_rdma.h>

#include "libcli.h"
#include "librskt_private.h"
#include "librsktd.h"
#include "librdma.h"
#include "liblist.h"
#include "librsktd_dmn_info.h"

#ifndef __LIBRSKTD_LIB_INFO_H__
#define __LIBRSKTD_LIB_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif

struct librskt_app;

/* Information about who to respond to for a request sent to a local library */
struct rsktd_to_app_req {
	struct rskt_dmn_speer *speer;
	struct rsktd_rx_msg *rsp;
};

struct acc_skts {
	struct librskt_app **app; /* Only one accept request per skt */
	uint32_t skt_num; /* Socket number */
	int max_backlog;
	struct l_head_t conn_req; /* FIFO of struct librsktd_unified_msg * */
				/* SPEER CONNECT requests */
	struct librsktd_unified_msg *acc_req; /* Only one accept req per sn */
				/* Application CONNECT requests */
};

struct con_skts {
	struct librskt_app **app; /* Only one accept request per skt */
	uint32_t loc_sn;
	struct rskt_dmn_wpeer **w;
	struct ms_info *loc_ms;
	uint32_t rem_ct; /* Component tag of remote peer */
	uint32_t rem_sn;
};

struct librskt_app {
        int app_fd;
	struct librskt_app **self_ptr; 
        socklen_t addr_size;
        struct sockaddr_un addr;
        pthread_t thread;
        int alive;
        sem_t started;
        volatile int i_must_die;
	sem_t test_msg_tx;
	sem_t test_msg_rx;
	struct librskt_app_to_rsktd_msg test_rq;
	struct librskt_rsktd_to_app_msg test_rsp;
	sem_t app_resp_mutex;
	uint32_t dmn_req_num;
	uint32_t rx_req_num; /* Sequence number for last received app req */
	struct l_head_t app_resp_q; /* List of responses for requests sent
					* to the APP.  Ordered by rsktd_seq_num
					* Struct librsktd_unified_msg.
					*/
	char app_name[MAX_APP_NAME];
	int32_t proc_num;
};

struct lib_msg_t {
	struct librskt_app **app_ptr;
	int msg_is_rq;
	union {
		struct librskt_req_msg *rq;
		struct librskt_resp_msg *rsp;
	};
};

struct librsktd_connect_globals {
        int port;
        int mpnum;
        int bklg;
	int tst; /* 0 - use real skt connection, 1 - fake skt connection*/

        pthread_t conn_thread;
        int loop_alive;
        sem_t loop_started;
        volatile int all_must_die;
	uint32_t ct; /* Component tag of RSKTD mport */

        int fd; /* File number library instance connect to */
        struct sockaddr_un addr;
	struct librskt_app *new_app;

	struct l_head_t app; /* List of connections to applications/libraries */
				/* Items are struct librskt_app */
				/* Key is file descriptor (app_fd) */

        pthread_t tx_thread; /* Thread responsible for sending messages to
				* all apps.
				*/
	sem_t tx_thread_alive;
	sem_t tx_msg_mutex; /* Must have this sem to access msg_tx queue */
	sem_t tx_msg_cnt; /* Posted for each entry in tx_msg_q */
	struct l_head_t tx_msg_q; /* List of messages to transmit */
				/* Items are struct lib_msg_t */
				/* FIFO, for all apps! */

	struct l_head_t acc; /* List of sockets listening/accepting */
				/* Items are struct acc_skts */
				/* Key is *local* socket number */
	struct l_head_t con; /* List of connected sockets */
				/* Items are struct con_skts */
				/* Key is local socket number */
	struct l_head_t creq; /* List of connect requests waiting */
				/* for socket to accept. */
				/* Items are struct librsktd_unified_msg */
				/* Key is local socket number */
};

extern struct librsktd_connect_globals lib_st;

/* NONBLOCKING Deal with library accept message and rsktd connect message */
extern void rsktd_connect_accept(struct acc_skts *acc);

extern void enqueue_app_msg(struct librsktd_unified_msg *msg);
extern void *app_tx_loop(void *unused);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_LIB_INFO_H__ */
