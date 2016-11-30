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

#ifndef __LIBRSKTD_DMN_INFO_H__
#define __LIBRSKTD_DMN_INFO_H__

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

#include <rapidio_mport_mgmt.h>

#include "librsktd_dmn.h"
#include "librsktd_private.h"
#include "liblist.h"
#include "librsktd_speer.h"
#include "librsktd_wpeer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	rsktd_ms_free = 0,
	rsktd_ms_used,
	rsktd_ms_rsvd,
	rsktd_ms_flux
} rsktd_ms_state;

#define RSKTD_MS_STATE_TO_STR(x) ( \
	(rsktd_ms_free == x)?"FREE": \
	(rsktd_ms_used == x)?"USED": \
	(rsktd_ms_rsvd == x)?"RSVD": \
	(rsktd_ms_flux == x)?"FLUX": "UNKN")

struct ms_info {
	int	valid;
	rsktd_ms_state	state; 
	char	ms_name[MAX_MS_NAME+1];
	ms_h	ms;
	int	ms_size;
	struct rskt_socket_t skt;
	int	loc_sn;
	int	rem_sn;
	int	rem_ct;
};

struct mso_info {
	int	valid;
	mso_h	rskt_mso;
	char	msoh_name[MAX_MS_NAME+1];
	int	next_ms;
	int	num_ms;
	struct ms_info ms[MAX_DMN_NUM_MS];
};

#define MAX_PEER 10

struct dmn_globals {
	int cm_skt;
	uint8_t mpnum;
	uint32_t skip_ms;
	uint32_t num_ms;
	uint32_t ms_size;

	riomp_mport_t mp_hnd;
	struct riomp_mgmt_mport_properties qresp;
	volatile int all_must_die;
	sem_t graceful_exit;

	/* CM connection request handler from other RSKT Daemons */
	sem_t speer_conn_thr_started;
	volatile int speer_conn_alive;
	int speer_conn_thr_valid;
	pthread_t speer_conn_thread; 
	int mb_valid;
	sem_t mb_mtx;
	riomp_mailbox_t mb;
	int skt_valid;
	riomp_sock_t cm_acc_h;

	/* RDMA Memory Space Database for this RSKT Daemon */
	struct mso_info mso;

	/* Transmit thread sending to all wpeers */
	sem_t wpeer_tx_loop_started;
	volatile int wpeer_tx_alive;
	pthread_t wpeer_tx_thread;
	sem_t wpeer_tx_mutex;
	sem_t wpeer_tx_cnt;
	struct l_head_t wpeer_tx_q;

	/* Transmit thread sending to all speers */
	sem_t speer_tx_loop_started;
	volatile int speer_tx_alive;
	pthread_t speer_tx_thread;
	sem_t speer_tx_mutex;
	sem_t speer_tx_cnt;
	struct l_head_t speer_tx_q;

	/* Transmit thread sending to all apps */
	sem_t app_tx_loop_started;
	volatile int app_tx_alive;
	pthread_t app_tx_thread;
	sem_t app_tx_mutex;
	sem_t app_tx_cnt;
	struct l_head_t app_tx_q;

	/* RSKTD peers that have connected to this RSKT Daemon */
#ifdef NOT_DEFINED
	sem_t speers_mtx;
	struct l_head_t speers; /* Item is ** struct rskt_dmn_speer */

	/* RSKTD peers list this RSKT Daemon has connected to */
	sem_t wpeers_mtx;
	struct l_head_t wpeers; /* Item is ** struct rskt_dmn_wpeer */
				/* ordered by component tag (CT) */
				/* Use ** to allow one write to clear all */
				/* references to a wpeer */
#endif
	struct rskt_dmn_speer speers[MAX_PEER];
	struct rskt_dmn_wpeer wpeers[MAX_PEER];

};

extern struct dmn_globals dmn;

struct console_globals {
        int all_must_die;
        /* Globals for console run by RSKT Daemon */
        pthread_t cons_thread;
        sem_t cons_owner;
        int cons_alive;

        /* Globals for remote CLI sessions */
        int cli_alive;
        int cli_portno;
};

extern struct console_globals cli;
#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_DMN_INFO_H__ */
