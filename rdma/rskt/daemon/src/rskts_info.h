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

#ifndef __RSKTD_INFO_H__
#define __RSKTD_INFO_H__

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

#include <rapidio_mport_sock.h>

#include "libcli.h"
#include "librskt_private.h"
#include "librdma.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rsktd_peer {
	ct_t ct;
	uint32_t cm_skt;
	uint32_t cm_mp;
};

struct rskts_control_list {
	int print_help; /* -H, -h, ? If true, display help and exit */
	int run_cons; /* -B If true, run a local console thread */
	int e_cli_skt; /* -e AF_INET socket for Ethernet remote console */
	int num_ms; /* -s Number of memory spaces to allocate */
	int ms_size; /* -S Size of each memory space */
	int rskt_buff_size; /* -k Size of each rskt buffer */
	int init_ms; /* -N If true, try to allocate memory spaces */
	int rsktd_uskt; /* -u RSKTD AF_LOCAL socket for rskt library conns */
	int rsktd_u_mp; /* -m Local mport of RSKTD for rskt library conns */
	int rsktd_u_bklg; /* -L Maximum backlog of connect requests */
	int rsktd_cskt; /* -C CM socket for RSKTD peer connections */
	int rsktd_c_mp; /* -M Local mport of RSKTD peer connections */
	int num_peers; /* Max valid index of peer rskt daemons */
	struct rsktd_peer peers[MAX_DMN_PEERS]; /* -P */
};

/* For now, one msub per ms */

struct rskt_ms_info {
	int	valid;
	char	ms_name[MAX_MS_NAME+1];
	ms_h	ms;
	int	ms_size;
	struct rskt_socket_t msub;
};

struct rskt_info {
	mso_h	rskt_mso;
	char	msoh_name[MAX_MS_NAME+1];
	int	num_ms;
	struct rskt_ms_info ms[MAX_DMN_NUM_MS];
};

struct list_t {
	struct list_t *next;
	void *info;
};

struct list_head_t {
	struct list_t *head;
	struct list_t *tail;
};

struct rskts_conn_globals {
	int skt_num;
	uint8_t mpnum;
	int mpfd;
	int bklg;
	struct riomp_mgmt_mport_properties qresp;

	/* CM connection request handler from other RSKT Daemons */
	pthread_t thread; 
	riomp_mailbox_t mb;
	riomp_sock_t skt;

	sem_t loop_started;
	int loop_alive;

	sem_t reqs_mutex;
	struct list_head_t reqs;
	uint16_t max_reqs;
	uint16_t num_reqs;
	uint8_t pause_reqs;

	/* RDMA Socket Database for this RSKT Daemon */
	struct rskt_info rskt;
};

struct console_globals {
	/* Globals for console run by RSKT Daemon */
	pthread_t cons_thread;
	sem_t cons_owner;
	int cons_alive;

	/* Globals for remote CLI sessions */
	int cli_alive;
	int cli_portno;
};

struct lib_accepting {
	int sock_num;
	pthread_t thread;
	sem_t got_a_connect;
};

struct req_list_t {
	struct req_list_t *next;
	riomp_sock_t *skt;
};

/* NOTE: list is empty when HEAD == NULL.
 * When HEAD == NULL, tail is invalid.
 */

struct req_list_head_t {
	struct req_list_t *head;
	struct req_list_t *tail;
};

struct library_globals {
	/* Connection request handler for librskt users */
	int portno;
	int mpnum;
	int bklg;

	pthread_t thread; 

	sem_t loop_started;
	int loop_alive;

	int fd;
	struct sockaddr_un addr;

	int peer_fd;
	socklen_t peer_addr_size;
	struct sockaddr_un peer_addr;

	sem_t reqs_mutex;
	struct req_list_head_t reqs;
	uint16_t num_reqs;
	uint8_t pause_reqs;
	uint8_t max_reqs;
};

struct all_globals {
	int all_must_die;
	uint8_t debug;
	struct control_list ctrls;
	struct console_globals cli;
	struct rskts_conn_globals conn;
	struct library_globals lib;
};

extern struct all_globals rsktd;

#ifdef __cplusplus
}
#endif

#endif /* __RSKTD_INFO_H__ */
