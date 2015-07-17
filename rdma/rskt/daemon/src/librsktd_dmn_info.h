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

struct ms_info {
	int	valid;
	int	state; /* 0 - free, 1 - used, 2 - temp reserved */
	char	ms_name[MAX_MS_NAME+1];
	ms_h	ms;
	int	ms_size;
	struct rskt_socket_t skt;
};

struct mso_info {
	int	valid;
	mso_h	rskt_mso;
	char	msoh_name[MAX_MS_NAME+1];
	int	num_ms;
	struct ms_info ms[MAX_DMN_NUM_MS];
};

struct dmn_globals {
	int cm_skt;
	int cm_skt_tst;
	uint8_t mpnum;
	uint32_t skip_ms;
	uint32_t num_ms;
	uint32_t ms_size;

	int mpfd;
	struct riodp_mport_properties qresp;
	int all_must_die;

	sem_t loop_started;
	volatile int loop_alive;

	/* CM connection request handler from other RSKT Daemons */
	int speer_conn_alive;
	int thread_valid;
	pthread_t thread; 
	int mb_valid;
	sem_t mb_mtx;
	riomp_mailbox_t mb;
	int skt_valid;
	riomp_sock_t cm_acc_h;

	/* RDMA Memory Space Database for this RSKT Daemon */
	struct mso_info mso;

	/* Transmit thread sending to all wpeers */
	int wpeer_tx_alive;
	pthread_t wpeer_tx_thread;
	sem_t wpeer_tx_mutex;
	sem_t wpeer_tx_cnt;
	struct l_head_t wpeer_tx_q;

	/* Transmit thread sending to all speers */
	int speer_tx_alive;
	pthread_t speer_tx_thread;
	sem_t speer_tx_mutex;
	sem_t speer_tx_cnt;
	struct l_head_t speer_tx_q;

	/* Transmit thread sending to all apps */
	int app_tx_alive;
	pthread_t app_tx_thread;
	sem_t app_tx_mutex;
	sem_t app_tx_cnt;
	struct l_head_t app_tx_q;

	/* RSKTD peers that have connected to this RSKT Daemon */
	sem_t speers_mtx;
	struct l_head_t speers; /* Item is ** struct rskt_dmn_speer */

	/* RSKTD peers list this RSKT Daemon has connected to */
	sem_t wpeers_mtx;
	struct l_head_t wpeers; /* Item is ** struct rskt_dmn_wpeer */
				/* ordered by component tag (CT) */
				/* Use ** to allow one write to clear all */
				/* references to a wpeer */

};

extern struct dmn_globals dmn;

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_DMN_INFO_H__ */
