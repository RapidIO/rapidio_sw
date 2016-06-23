/* RSKT Daemon library routines, for use of the Daemon */
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

/* Message exchanged between the RSKTD and the RSKTLIB */

#ifndef __LIBRSKTD_PRIVATE_H__
#define __LIBRSKTD_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DFLT_DMN_MPORT 0
#define MAX_DMN_MPORT 7

#define DMN_LSKT_FMT "/var/tmp/RSKTD%04d.%1d"
#define DFLT_DMN_LSKT_SKT 3333
#define DFLT_DMN_LSKT_MPORT DFLT_DMN_MPORT
#define DFLT_DMN_LSKT_BACKLOG 50

#define DFLT_DMN_CM_CONN_SKT 4455
#define DFLT_DMN_CM_CONN_MPORT 0

#define DFLT_DMN_E_CLI_SKT 4343

#define MAX_DMN_PEERS 16

#define DFLT_DMN_INIT 1
#define DFLT_DMN_NUM_MS 8
#define MAX_DMN_NUM_MS 64
#define DFLT_DMN_SBUF_SIZE 64
#define DFLT_DMN_MS_SIZE 64

struct peer_rsktd_addr {
	uint32_t ct;
        uint32_t cm_skt;
};

struct control_list {
        int debug;
        int print_help; /* -H, -h, ? If true, display help and exit */
        int run_cons; /* -B If true, run a local console thread */
	int log_level; /* -l<x> Sets default log level, 0 for silence */
        int e_cli_skt; /* -e AF_INET socket for Ethernet remote console */
        int num_ms; /* -s Number of memory spaces to allocate */
        int ms_size; /* -S Size of each memory space */
        int rskt_buff_size; /* -k Size of each rskt buffer */
        int init_ms; /* -N If true, try to allocate memory spaces */
	int use_mport; /* false use RDMA, true use MPORT.  Default is false */
        int rsktd_uskt_tst; /* -t Test uskt mode */
        int rsktd_uskt; /* -u RSKTD AF_LOCAL socket for rskt library conns */
        int rsktd_u_mp; /* -m Local mport of RSKTD for rskt library conns */
        int rsktd_u_bklg; /* -K Maximum backlog of connect requests */
        int rsktd_cskt_tst; /* -T Test uskt mode */
        int rsktd_cskt; /* -C CM socket for RSKTD peer connections */
        int rsktd_c_mp; /* -M Local mport of RSKTD peer connections */
        int num_peers; /* Max valid index of peer rskt daemons */
        struct peer_rsktd_addr peers[MAX_DMN_PEERS]; /* -P */
};

extern struct control_list ctrls;

void librsktd_bind_cli_cmds(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKTD_PRIVATE_H__ */

