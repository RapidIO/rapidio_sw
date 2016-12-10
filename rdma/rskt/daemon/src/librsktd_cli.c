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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <rapidio_mport_dma.h>

#include "rio_ecosystem.h"
#include "libcli.h"
#include "librskt_private.h"
#include "librdma.h"
#include "librsktd_lib.h"
#include "librsktd_lib_info.h"
#include "librsktd_dmn_info.h"
#include "librsktd_dmn.h"
#include "librsktd_dmn_test.h"
#include "librsktd_msg_proc.h"
#include "librsktd_private.h"
#include "librsktd.h"
#include "librsktd_sn.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct cli_cmd RSKTStatus;

void print_ms_status(struct cli_env *env, int start_ms, int end_ms)
{
	int ms_idx;
	struct mso_info *mso = &dmn.mso;
	int got_one = 0;

	for (ms_idx = start_ms; ms_idx < end_ms; ms_idx++) {
		if (mso->ms[ms_idx].valid) {
			if (!got_one) {
				LOGMSG(env,
						"\nMS V   Name                     MSSize   State LocSN RemSN   RemCt\n");
				got_one = 1;
			};
			int state = mso->ms[ms_idx].state;
			LOGMSG(env, "%2d %1d %26s %8x %5s %5d %5d 0x%8x\n",
					ms_idx, mso->ms[ms_idx].valid,
					mso->ms[ms_idx].ms_name,
					mso->ms[ms_idx].ms_size,
					RSKTD_MS_STATE_TO_STR(state),
					mso->ms[ms_idx].loc_sn,
					mso->ms[ms_idx].rem_sn,
					mso->ms[ms_idx].rem_ct);
		}
	}

	if (!got_one) {
		LOGMSG(env, "\nNo Memory Spaces!");
	}
}

void print_loop_status(struct cli_env *env)
{
	LOGMSG(env, "        Alive Socket # BkLg MP Pse Max Reqs Name\n");
	LOGMSG(env, "RLConn %5d 0d%8d\n", cli.cli_alive,
			ctrls.e_cli_skt);
	LOGMSG(env, "SpConn %5d 0d%8d     %2d\n", dmn.speer_conn_alive,
			dmn.cm_skt, dmn.mpnum);

	LOGMSG(env, "LibCon %5d 0d%8d %4d %2d %s\n", lib_st.lib_conn_loop_alive,
			lib_st.port, lib_st.bklg, lib_st.mpnum,
			lib_st.addr.sun_path);

	LOGMSG(env, "WP Tx  %5d\n", dmn.wpeer_tx_alive);
	LOGMSG(env, "SP Tx  %5d\n", dmn.speer_tx_alive);
	LOGMSG(env, "APP Tx %5d\n", dmn.app_tx_alive);
	LOGMSG(env, "DAEMON all_must_die : %d\n", dmn.all_must_die);
}

struct cli_cmd DMNStatus;

void display_app_list(struct cli_env *env);
void display_acc_list(struct cli_env *env);
void display_con_list(struct cli_env *env);
void display_speers_list(struct cli_env *env);
void display_wpeers_list(struct cli_env *env);

int DMNStatusCmd(struct cli_env *env, int argc, char **argv)
{
        int st_ms_idx = 0, max_ms_idx = MAX_DMN_NUM_MS;

	if (argc) {
		st_ms_idx = getDecParm(argv[0], 0);
		max_ms_idx = st_ms_idx+1;
		if (st_ms_idx >= MAX_DMN_NUM_MS)
			goto show_help;
	};
	
	print_loop_status(env);
	print_ms_status(env, st_ms_idx, max_ms_idx);
	display_app_list(env);
	display_acc_list(env);
	display_con_list(env);
	display_speers_list(env);
	display_wpeers_list(env);

	LOGMSG(env, "\n");

	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values\n");
	cli_print_help(env, &DMNStatus);

	return 0;
};

struct cli_cmd RSKTStatus = {
"status",
2,
0,
"RSKT Status command.",
"{MS_IDX {MSUB_IDX}}\n" 
        "Dumps the status of the memory space & peer connection database.\n"
	"<MS_IDX> : 0-ff, Optionally limits display to one memory space.\n",
DMNStatusCmd,
ATTR_RPT
};

void rskt_daemon_shutdown(void);

int RSKTShutdownCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc) {
		LOGMSG(env, "Extra parameters ignored: %s\n", argv[0]);
	}
	LOGMSG(env, "Shutdown initiated...\n");

	rskt_daemon_shutdown();

	LOGMSG(env, "Shutdown complete...\n");

	return 0;
}

struct cli_cmd RSKTShutdown = {
"shutdown",
8,
0,
"Shutdown daemon.",
"No Parameters\n" 
        "Shuts down all threads, including CLI.\n",
RSKTShutdownCmd,
ATTR_NONE
};

#define MAX_MPORTS 8

int RSKTMpdevsCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t *mport_list = NULL;
	uint32_t *ep_list = NULL;
	uint32_t *list_ptr;
	uint32_t number_of_eps = 0;
	uint8_t number_of_mports = 8;
	uint32_t ep = 0;
	int i;
	int mport_id;
	int ret = 0;

	if (argc) {
		LOGMSG(env, "FAILED: Extra parameters ignored: %s\n", argv[0]);
	}

	ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
	if (ret) {
		LOGMSG(env, "ERR: riomp_mgmt_get_mport_list() ERR %d\n", ret);
		return 0;
	}

	printf("\nAvailable %d local mport(s):\n", number_of_mports);
	if (number_of_mports > MAX_DMN_MPORT) {
		LOGMSG(env, "WARNING: Only %d out of %d have been retrieved\n",
				MAX_DMN_MPORT, number_of_mports);
	}

	list_ptr = mport_list;
	for (i = 0; i < number_of_mports; i++, list_ptr++) {
		mport_id = *list_ptr >> 16;
		LOGMSG(env, "+++ mport_id: %u dest_id: %u\n", mport_id,
				*list_ptr & 0xffff);

		/* Display EPs for this MPORT */

		ret = riomp_mgmt_get_ep_list(mport_id, &ep_list,
				&number_of_eps);
		if (ret) {
			LOGMSG(env, "ERR: riodp_ep_get_list() ERR %d\n", ret);
			break;
		}

		LOGMSG(env, "\t%u Endpoints (dest_ID): ", number_of_eps);
		for (ep = 0; ep < number_of_eps; ep++) {
			LOGMSG(env, "%u ", *(ep_list + ep));
		}

		LOGMSG(env, "\n");

		ret = riomp_mgmt_free_ep_list(&ep_list);
		if (ret) {
			LOGMSG(env, "ERR: riodp_ep_free_list() ERR %d\n", ret);
		}
	}

	LOGMSG(env, "\n");

	ret = riomp_mgmt_free_mport_list(&mport_list);
	if (ret) {
		LOGMSG(env, "ERR: riodp_ep_free_list() ERR %d\n", ret);
	}

	return 0;
}

struct cli_cmd RSKTMpdevs = {
"mpdevs",
2,
0,
"Query mport info.",
"No Parameters\n" 
        "Displays available mports, and associated target destigation IDs.\n",
RSKTMpdevsCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTMsoh;

int RSKTMsohCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	struct mso_info *mso = &dmn.mso;

	if (argc) {
		switch( argv[0][0]) {
		case 'd':
			rc = d_rdma_drop_mso_h(&mso->rskt_mso);
			LOGMSG(env, "rdma_drop_mso_h rc: %d\n", rc);
			break;
		case 'g':
			if (argc < 2) {
				LOGMSG(env, "Missing <name>\n");
				goto syntax_error;

			}
			rc = d_rdma_get_mso_h(argv[1], &mso->rskt_mso);
			LOGMSG(env, "rdma_get_mso_h rc: %d\n", rc);
			break;
		default:
			LOGMSG(env, "Unknown option %s\n", argv[0]);
			goto syntax_error;
		}
	}

	LOGMSG(env, "Current mso_h name: \"%s\"\n", mso->msoh_name);
	LOGMSG(env, "Current mso_h value: %16" PRIu64 "\n",
			(uint64_t )(mso->rskt_mso));

	return 0;

syntax_error:
	cli_print_help(env, &RSKTMsoh);
	return 0;
}

struct cli_cmd RSKTMsoh = {
"msoh",
4,
0,
"Allocate or drop memory space owner handle",
"msoh {\"d\"|<\"g\" <name>>\n"
	"No parameters dislays the memory space owner\n"
	"d     : Drop current memory space owner handle\n"
	"g     : Get a memory space owner handle\n"
	"<name>: Name of memory space owner\n"
	" Note: Only one memory space owner is tracked.\n"
	" It is possible to overwrite and lose an existing msoh\n",
RSKTMsohCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTMs;

int RSKTMsCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	int ms_size;
	int st_ms_idx = 0;
	int end_ms_idx = MAX_DMN_NUM_MS;
	struct mso_info *mso = &dmn.mso;

	if (argc) {
		LOGMSG(env, "\n");

		st_ms_idx = getDecParm(argv[0], 0);
		if (st_ms_idx >= MAX_DMN_NUM_MS) {
			LOGMSG(env, "ms_idx must be 0 to %d\n",
					MAX_DMN_NUM_MS-1);
			goto syntax_error;
		};
		end_ms_idx = st_ms_idx + 1;

		if (argc > 1) {
			switch( argv[1][0]) {
			case 'd':
				rc = d_rdma_drop_ms_h(&mso->ms[st_ms_idx],
						mso->rskt_mso);

				LOGMSG(env, "rdma_drop_ms_h rc: %d\n", rc);
				break;
			case 'g':
				if (argc < 3) {
					LOGMSG(env, "Missing size\n");
					goto syntax_error;
				}

				ms_size = getDecParm(argv[2], 128);
				if ((ms_size != 128) && (ms_size != 256)
						&& (ms_size != 512)
						&& (ms_size != 1024)
						&& (ms_size != 2048)) {
					LOGMSG(env, "WARNING: Invalid size\n");
				} else {
					ms_size = ms_size * 1024;
				}

				if (argc < 4) {
					LOGMSG(env, "Missing name\n");
					goto syntax_error;
				}

				if (mso->ms[st_ms_idx].valid) {
					rc = d_rdma_drop_ms_h(
							&mso->ms[st_ms_idx],
							mso->rskt_mso);

					LOGMSG(env, "rdma_drop_ms_h rc: %d\n",
							rc);
				}

				rc = d_rdma_get_ms_h(&mso->ms[st_ms_idx],
						argv[3], mso->rskt_mso, ms_size,
						0);
				LOGMSG(env, "rdma_get_ms_h rc: %d\n", rc);
				if (end_ms_idx > dmn.mso.num_ms) {
					dmn.mso.num_ms = end_ms_idx;
				}
				break;
			default:
				LOGMSG(env, "Unknown option %s\n", argv[0]);
				goto syntax_error;
			}
		}
	}

	print_ms_status(env, st_ms_idx, end_ms_idx);
	
	return 0;

syntax_error:
	cli_print_help(env, &RSKTMs);
	return 0;
}

struct cli_cmd RSKTMs = {
"MS",
2,
0,
"Display, allocate or drop memory space",
"ms {<ms#> {\"d\"|<\"g\" <size> <name>>}}\n"
	"No parameters dislays all memory spaces\n"
	"<ms#  : Index of ms to display/get/drop\n"
	"d     : Drop memory space handle\n"
	"g     : Get a memory space handle\n"
	"<size>: Size of memory space in kilobytes, valid values are\n"
	"	128, 256, 512, 1024, 2048.\n"
	"<name>: Name of the memory subspace\n",
RSKTMsCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTMsub;

int RSKTMsubCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	int ms_size;
	uint32_t st_ms_idx = 0;
	uint32_t end_ms_idx = MAX_DMN_NUM_MS;
	uint32_t flags = 0;
	struct mso_info *mso = &dmn.mso;

	if (argc) {
		st_ms_idx = getDecParm(argv[0], 0);
		if (st_ms_idx >= MAX_DMN_NUM_MS) {
			LOGMSG(env, "ms_idx must be 0 to %d\n",
					MAX_DMN_NUM_MS-1);
			goto syntax_error;
		};
		end_ms_idx = st_ms_idx + 1;

		if (1 == argc)
			goto display;

		switch( argv[1][0]) {
		case 'd':
			rc = d_rdma_drop_msub_h(&mso->ms[st_ms_idx]);
			LOGMSG(env, "rdma_d_msub_h rc: %d\n", rc);
			break;
		case 'g':
			if (argc < 4) {
				LOGMSG(env, "Missing parms\n");
				goto syntax_error;
			}

			ms_size = getDecParm(argv[2], 128);
			if ((ms_size != 128) && (ms_size != 4) && (ms_size != 8)
					&& (ms_size != 16) && (ms_size != 32)
					&& (ms_size != 64)
					&& (ms_size != 128)) {
				LOGMSG(env, "Invalid size\n");
			} else {
				ms_size = ms_size * 1024;
			}

			if (ms_size > mso->ms[st_ms_idx].ms_size) {
				LOGMSG(env, "Size invalid\n");
			}

			rc = d_rdma_get_msub_h(&mso->ms[st_ms_idx], 0, ms_size,
					flags);
			LOGMSG(env, "rdma_create_msub_h rc: %d\n", rc);

			break;
		default:
			LOGMSG(env, "Unknown option %s\n", argv[0]);
			goto syntax_error;
		}
	}
display:

	print_ms_status(env, st_ms_idx, end_ms_idx);
	
	return 0;

syntax_error:
	cli_print_help(env, &RSKTMsub);
	return 0;
}

struct cli_cmd RSKTMsub = {
"msub",
3,
0,
"Display, get or drop memory subspace handle",
"msub {<ms#> {\"d\"|<\"g\" <size>>}\n"
	"<ms#>  : Index of memory space to display/get/drop\n"
	"d      : Drop memory subspace\n"
	"g      : Get a memory subspace\n"
	"<size> : Size of memory subspace in kilobytes, valid values are\n"
	"         4, 8, 16, 32, 64, 128.\n",
RSKTMsubCmd,
ATTR_NONE
};

void display_app_list(struct cli_env *env)
{
	int i, found_one = 0;

	for (i = 0; i < MAX_APPS; i++) {
		if ((lib_st.apps[i].app_fd <= 0) || !lib_st.apps[i].alive
				|| lib_st.apps[i].i_must_die)
			continue;
		if (!found_one) {
			LOGMSG(env,
					"\nAPP\nFd RxSeqNum DmnSqNum Alive I_Die Process App\n");
			found_one = 1;
		}
		LOGMSG(env, "%2d %8d %8d %5d  %3d  %5d %30s\n",
				lib_st.apps[i].app_fd,
				lib_st.apps[i].rx_req_num,
				lib_st.apps[i].dmn_req_num,
				lib_st.apps[i].alive, lib_st.apps[i].i_must_die,
				lib_st.apps[i].proc_num,
				lib_st.apps[i].app_name);
	}
	if (!found_one) {
		LOGMSG(env, "\nNo applications connected");
	}
}

void display_acc_list(struct cli_env *env)
{
	struct l_item_t *li;
	struct acc_skts *acc;
	struct librskt_app *app;
	int found_one = 0;

	acc = (struct acc_skts *)l_head(&lib_st.acc, &li);
	while (NULL != li) {
		if (NULL == acc) {
			continue;
		}

		app = *(acc->app);
		if (NULL != app) {
			if (!found_one) {
				LOGMSG(env,
						"\nACC Skts\nFd AppNamePN  SockNum BkLg  State Acc ConReq\n");
				found_one = 1;
			}
			LOGMSG(env, "%2d %10s %7d %4d %6s  %1d  %6d\n",
					app->app_fd, app->app_name,
					acc->skt_num, acc->max_backlog,
					SKT_STATE_STR(rsktd_sn_get(acc->skt_num)),
					(acc->acc_req==NULL)?1:0,
					l_size(&acc->conn_req));
		}
		acc = (struct acc_skts *)l_next(&li);

	}
	if (!found_one) {
		LOGMSG(env, "\nNo sockets listening");
	}
}

void display_con_list(struct cli_env *env)
{
	struct l_item_t *li;
	struct con_skts *con;
	struct librskt_app *app;
	int found_one = 0;

	con = (struct con_skts *)l_head(&lib_st.con, &li);
	while (NULL != li) {
		if (NULL == con) {
			continue;
		}

		app = *(con->app);
		if (NULL != app) {
			if (!found_one) {
				LOGMSG(env,
						"\nCON Skts\nFd AppNamePN  SockNum  State RemSkNm Rem_CT\n");
				found_one = 1;
			}
			LOGMSG(env, "%2d %10s %7d %6s %7d %4d\n", app->app_fd,
					app->app_name, con->loc_sn,
					SKT_STATE_STR(rsktd_sn_get(con->loc_sn)),
					con->rem_sn, con->rem_ct);
		}
		con = (struct con_skts *)l_next(&li);

	}
	if (!found_one) {
		LOGMSG(env, "\nNo connected sockets");
	}
}
		

void display_speers_list(struct cli_env *env)
{
	int i, found_one = 0;

	for (i = 0; i < MAX_PEER; i++) {
		if (!dmn.speers[i].alive || dmn.speers[i].i_must_die)
			continue;

		if (!found_one) {
			LOGMSG(env,
					"\nSPEERS\nComp_Tag CM_Sockt A   Req   Seq   Resp    Seq\n");
			found_one = 1;
		}

		LOGMSG(env, "%8d %8d %1d %8s %3d %8s %3d\n", dmn.speers[i].ct,
				dmn.speers[i].cm_skt_num,
				dmn.speers[i].alive && dmn.speers[i].got_hello,
				RSKTD_REQ_STR(ntohl(dmn.speers[i].req->msg_type)),
				ntohl(dmn.speers[i].req->msg_seq),
				RSKTD_RESP_STR(ntohl(dmn.speers[i].resp->msg_type)),
				ntohl(dmn.speers[i].resp->msg_seq));
	}

	if (!found_one) {
		LOGMSG(env, "\nRDMN Socket Peers: None\n");
	}
}

void display_wpeers_list(struct cli_env *env)
{
	int i, found_one = 0;

	for (i = 0; i < MAX_PEER; i++) {
		if ((!dmn.wpeers[i].wpeer_alive) || dmn.wpeers[i].i_must_die)
			continue;

		if (!found_one) {
			LOGMSG(env,
					"\nWPEERS\nComp_Tag CM_Sockt PeerPid  A D  SeqNum  Rx_Buff\n");
			found_one = 1;
		}

		LOGMSG(env, "%8d %8d %8d %1d %1d %8d %p\n", dmn.wpeers[i].ct,
				dmn.wpeers[i].cm_skt, dmn.wpeers[i].peer_pid,
				dmn.wpeers[i].wpeer_alive,
				dmn.wpeers[i].i_must_die,
				dmn.wpeers[i].w_seq_num, dmn.wpeers[i].rx_buff);
	}

	if (!found_one) {
		LOGMSG(env, "\nRDMN Socket WPeers: None\n");
	}
}
		
extern struct cli_cmd DMNLibStatus;

int DMNLibStatusCmd(struct cli_env *env, int argc, char **argv)
{
	int fd_in = -1, i;

	if (argc) {
		if (argc > 1)
			goto syntax_error;

		fd_in = getHex(argv[0], 0);

		if (fd_in == lib_st.fd) {
			LOGMSG(env, "Aborting library threads\n");
			halt_lib_handler();
			goto display;
		}

		for (i = 0; i < MAX_APPS; i++) {
			if (lib_st.apps[i].app_fd == fd_in) {
				LOGMSG(env,
						"Aborting app rx thread fd %d idx %d\n",
						fd_in, i);
				lib_st.apps[i].i_must_die = 1;
				pthread_kill(lib_st.apps[i].thread, SIGUSR1);
			}
			goto display;
		}
	}

display:
	print_loop_status(env);

	display_app_list(env);
	display_acc_list(env);
	display_con_list(env);

	return 0;

syntax_error:
	cli_print_help(env, &DMNLibStatus);
	return 0;
}

struct cli_cmd DMNLibStatus = {
"lstatus",
3,
0,
"Display status of daemon library/application connection handling thread\n",
"{<fd>}\n"
	"<fd>: Optionally, shutdown an specified file descriptor\n"
	"      If the fd matches the threads file descriptor, kills all.\n", 
DMNLibStatusCmd,
ATTR_NONE
};

void display_lib_req_msg(struct cli_env *env,
		struct librskt_app_to_rsktd_msg *rq)
{
	LOGMSG(env, "MsgType  :   0x%8x   0x%8x\n", rq->msg_type,
			ntohl(rq->msg_type));

	LOGMSG(env, "MsgSeq   : %12d %12d\n", rq->a_rq.app_seq_num,
			ntohl(rq->a_rq.app_seq_num));

	switch( ntohl(rq->msg_type)) {
	case LIBRSKTD_BIND:
		LOGMSG(env, "Bind Sn  :   0x%8x   0x%8x %12d\n",
				rq->a_rq.msg.bind.sn,
				ntohl(rq->a_rq.msg.bind.sn),
				ntohl(rq->a_rq.msg.bind.sn));
		break;

	case LIBRSKTD_LISTEN:
		LOGMSG(env, "Listen Sn:   0x%8x   0x%8x %12d\n",
				rq->a_rq.msg.listen.sn,
				ntohl(rq->a_rq.msg.listen.sn),
				ntohl(rq->a_rq.msg.listen.sn));
		LOGMSG(env, "List Bklg:   0x%8x   0x%8x %12d\n",
				rq->a_rq.msg.listen.max_bklog,
				ntohl(rq->a_rq.msg.listen.max_bklog),
				ntohl(rq->a_rq.msg.listen.max_bklog));
		break;

	case LIBRSKTD_ACCEPT:
		LOGMSG(env, "Accept Sn:   0x%8x   0x%8x %12d\n",
				rq->a_rq.msg.accept.sn,
				ntohl(rq->a_rq.msg.accept.sn),
				ntohl(rq->a_rq.msg.accept.sn));
		break;

	case LIBRSKTD_CONN:
		LOGMSG(env, "ConnSn   :   0x%8d   0x%8d %12d\n",
				rq->a_rq.msg.conn.sn,
				ntohl(rq->a_rq.msg.conn.sn),
				ntohl(rq->a_rq.msg.conn.sn));
		break;

	case LIBRSKTD_CLOSE:
		LOGMSG(env, "Close  Sn:   0x%8x   0x%8x %12d\n",
				rq->a_rq.msg.close.sn,
				ntohl(rq->a_rq.msg.close.sn),
				ntohl(rq->a_rq.msg.close.sn));
		break;

	case LIBRSKTD_HELLO:
		LOGMSG(env, "Hello Nm:  %s\n", rq->a_rq.msg.hello.app_name);
		LOGMSG(env, "Hello Pn:   0x%8d   0x%8d\n",
				rq->a_rq.msg.hello.proc_num,
				ntohl(rq->a_rq.msg.hello.proc_num));
		break;

	default:
		LOGMSG(env, "UNKNOWN MESSAGE!\n");
	}
}

void display_lib_resp_msg(struct cli_env *env,
		struct librskt_rsktd_to_app_msg *rsp)
{
	LOGMSG(env, "MsgType  :   0x%8x   0x%8x\n", rsp->msg_type,
			ntohl(rsp->msg_type));

	LOGMSG(env, "MsgSeq   : %12d %12d\n", rsp->a_rsp.req.app_seq_num,
			ntohl(rsp->a_rsp.req.app_seq_num));
	LOGMSG(env, "Err      :   0x%8x   0x%8x %12d %s\n", rsp->a_rsp.err,
			ntohl(rsp->a_rsp.err), ntohl(rsp->a_rsp.err),
			strerror(ntohl(rsp->a_rsp.err)));

	switch( ntohl(rsp->msg_type)) {
	case LIBRSKTD_BIND_RESP:
		LOGMSG(env, "Bind Sn  :   0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.bind.sn,
				ntohl(rsp->a_rsp.req.msg.bind.sn),
				ntohl(rsp->a_rsp.req.msg.bind.sn));
		break;

	case LIBRSKTD_LISTEN_RESP:
		LOGMSG(env, "Listen Sn:   0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.listen.sn,
				ntohl(rsp->a_rsp.req.msg.listen.sn),
				ntohl(rsp->a_rsp.req.msg.listen.sn));
		LOGMSG(env, "Listen Bklg: 0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.listen.max_bklog,
				ntohl(rsp->a_rsp.req.msg.listen.max_bklog),
				ntohl(rsp->a_rsp.req.msg.listen.max_bklog));
		break;

	case LIBRSKTD_ACCEPT_RESP:
		LOGMSG(env, "Accept Sn:   0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.accept.sn,
				ntohl(rsp->a_rsp.req.msg.accept.sn),
				ntohl(rsp->a_rsp.req.msg.accept.sn));
		LOGMSG(env, "R new_sn : %12d %12d\n",
				rsp->a_rsp.msg.accept.new_sn,
				ntohl(rsp->a_rsp.msg.accept.new_sn));
		LOGMSG(env, "R sa.sn  : %12d %12d\n",
				rsp->a_rsp.msg.accept.peer_sa.sn,
				ntohl(rsp->a_rsp.msg.accept.peer_sa.sn));
		LOGMSG(env, "R sa.ct  : %12d %12d\n",
				rsp->a_rsp.msg.accept.peer_sa.ct,
				ntohl(rsp->a_rsp.msg.accept.peer_sa.ct));
		LOGMSG(env, "R mso_nm : \"%s\"\n",
				rsp->a_rsp.msg.accept.mso_name);
		LOGMSG(env, "R ms_nm  : \"%s\"\n",
				rsp->a_rsp.msg.accept.ms_name);
		break;

	case LIBRSKTD_CONN_RESP:
		LOGMSG(env, "ConnectSn:   0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.conn.sn,
				ntohl(rsp->a_rsp.req.msg.conn.sn),
				ntohl(rsp->a_rsp.req.msg.conn.sn));
		LOGMSG(env, "R new_sn : %12d %12d\n",
				rsp->a_rsp.msg.conn.new_sn,
				ntohl(rsp->a_rsp.msg.conn.new_sn));
		LOGMSG(env, "R mso    : \"%s\"\n", rsp->a_rsp.msg.conn.mso);
		LOGMSG(env, "R ms     : \"%s\"\n", rsp->a_rsp.msg.conn.ms);
		LOGMSG(env, "R msub_s :   0x%8x   0x%8x %12d\n",
				rsp->a_rsp.msg.conn.msub_sz,
				ntohl(rsp->a_rsp.msg.conn.msub_sz),
				ntohl(rsp->a_rsp.msg.conn.msub_sz));
		LOGMSG(env, "R rem_ms : \"%s\"\n", rsp->a_rsp.msg.conn.rem_ms);
		LOGMSG(env, "R rem_sn :    0x%8x   0x%8x %12d\n",
				rsp->a_rsp.msg.conn.rem_sn,
				ntohl(rsp->a_rsp.msg.conn.rem_sn),
				ntohl(rsp->a_rsp.msg.conn.rem_sn));
		break;

	case LIBRSKTD_CLOSE_RESP:
		LOGMSG(env, "Close  Sn:    0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.close.sn,
				ntohl(rsp->a_rsp.req.msg.close.sn),
				ntohl(rsp->a_rsp.req.msg.close.sn));
		break;

	case LIBRSKTD_HELLO_RESP:
		LOGMSG(env, "Hello Nm:  \"%s\"\n",
				rsp->a_rsp.req.msg.hello.app_name);
		LOGMSG(env, "Hello Pn:    0x%8x   0x%8x %12d\n",
				rsp->a_rsp.req.msg.hello.proc_num,
				ntohl(rsp->a_rsp.req.msg.hello.proc_num),
				ntohl(rsp->a_rsp.req.msg.hello.proc_num));
		LOGMSG(env, "R CT     :   0x%8x   0x%8x %12d\n",
				rsp->a_rsp.msg.hello.ct,
				ntohl(rsp->a_rsp.msg.hello.ct),
				ntohl(rsp->a_rsp.msg.hello.ct));
		break;

	default:
		LOGMSG(env, "UNKNOWN MESSAGE! 0x%8x 0x%8x %12d\n",
				rsp->msg_type, ntohl(rsp->msg_type),
				ntohl(rsp->msg_type));
	}
}

extern struct cli_cmd RSKTReq;

int RSKTReqCmd(struct cli_env *env, int argc, char **argv)
{
	struct librskt_app *app = &lib_st.apps[0];

	if ((app->app_fd <= 0) || !app->alive || app->i_must_die) {
		LOGMSG(env, "Command not available\n");
		return 0;
	}

	app->test_rq.msg_type = htonl(getHex(argv[0], 1));
	app->test_rq.a_rq.app_seq_num = htonl(getDecParm(argv[1], 1));
	
	switch (ntohl(app->test_rq.msg_type)) {
	case LIBRSKTD_BIND:
		if (argc != 3)
			goto show_help;
		app->test_rq.a_rq.msg.bind.sn = htonl(getDecParm(argv[2], 1));
		break;
	case LIBRSKTD_LISTEN:
		if (argc != 4)
			goto show_help;
		app->test_rq.a_rq.msg.listen.sn = htonl(getDecParm(argv[2], 1));
		app->test_rq.a_rq.msg.listen.max_bklog = htonl(getDecParm(argv[3], 1));
		break;
	case LIBRSKTD_ACCEPT:
		if (argc != 3)
			goto show_help;
		app->test_rq.a_rq.msg.accept.sn = htonl(getDecParm(argv[2], 1));
		break;
	case LIBRSKTD_CONN:
		if (argc != 4)
			goto show_help;
		app->test_rq.a_rq.msg.conn.sn = htonl(getDecParm(argv[2], 1));
		app->test_rq.a_rq.msg.conn.ct = htonl(getDecParm(argv[3], 1));
		break;
	case LIBRSKTD_CLOSE:
		if (argc != 3)
			goto show_help;
		app->test_rq.a_rq.msg.close.sn = htonl(getDecParm(argv[2], 1));
		break;
	case LIBRSKTD_HELLO:
		if (argc != 4)
			goto show_help;
		strcpy(app->test_rq.a_rq.msg.hello.app_name, argv[2]);
		app->test_rq.a_rq.msg.hello.proc_num = htonl(getDecParm(argv[3], 1));
		break;
	default: 
		break;
	};

	display_lib_req_msg(env, &app->test_rq);
	LOGMSG(env, "Sending message...\n");
	sem_post(&app->test_msg_rx);
	LOGMSG(env, "Awaiting response...\n");
	sem_wait(&app->test_msg_tx);
	display_lib_resp_msg(env, &app->test_rsp);

	return 0;
show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values\n");
	cli_print_help(env, &RSKTReq);

	return 0;
};

struct cli_cmd RSKTReq = {
"lreq",
4,
2,
"Test library req message to RDMA Daemon",
"<type> <seq> <msgparms>\n"
	"<type> bind 1 listen 2 ac 3 conn 4 close 5 hello 1111\n"
	"<seq> random number to match req with response\n"
	"bind  : sn\n"
	"listen: sn bklog\n"
	"accept: sn\n"
	"conn  : sn ct\n"
	"close : sn\n"
	"hello : name proc_num\n",
RSKTReqCmd,
ATTR_NONE
};

void display_dmn_req_msg(struct cli_env *env, struct rsktd_req_msg *rq)
{
	LOGMSG(env, "MsgType     :    0x%8x   0x%8x %12d\n", rq->msg_type,
			ntohl(rq->msg_type), ntohl(rq->msg_type));

	LOGMSG(env, "MsgSeq      :    0x%8x   0x%8x %12d\n", rq->msg_seq,
			ntohl(rq->msg_seq), ntohl(rq->msg_seq));

	switch( ntohl(rq->msg_type)) {
	case RSKTD_HELLO_REQ:
		LOGMSG(env, "Hello Ct    :    0x%8x   0x%8x %12d\n",
				rq->msg.hello.ct, ntohl(rq->msg.hello.ct),
				ntohl(rq->msg.hello.ct));
		LOGMSG(env, "Hello Cm    :    0x%8x   0x%8x %12d\n",
				rq->msg.hello.cm_skt,
				ntohl(rq->msg.hello.cm_skt),
				ntohl(rq->msg.hello.cm_skt));
		LOGMSG(env, "Hello MP    :    0x%8x   0x%8x %12d\n",
				rq->msg.hello.cm_mp, ntohl(rq->msg.hello.cm_mp),
				ntohl(rq->msg.hello.cm_mp));
		break;

	case RSKTD_CONNECT_REQ:
		LOGMSG(env, "Conn Dst Sn :    0x%8x   0x%8x %12d\n",
				rq->msg.con.dst_sn, ntohl(rq->msg.con.dst_sn),
				ntohl(rq->msg.con.dst_sn));
		LOGMSG(env, "Conn Dst Ct :    0x%8x   0x%8x %12d\n",
				rq->msg.con.dst_ct, ntohl(rq->msg.con.dst_ct),
				ntohl(rq->msg.con.dst_ct));
		LOGMSG(env, "Conn Src Sn :    0x%8x   0x%8x %12d\n",
				rq->msg.con.src_sn, ntohl(rq->msg.con.src_sn),
				ntohl(rq->msg.con.src_sn));
		LOGMSG(env, "Conn SrcMso : \"%s\"\n", rq->msg.con.src_mso);
		LOGMSG(env, "Conn SrcMs  : \"%s\"\n", rq->msg.con.src_ms);
		LOGMSG(env, "Conn Msub O :    0x%8x   0x%8x %12d\n",
				rq->msg.con.src_msub_o,
				ntohl(rq->msg.con.src_msub_o),
				ntohl(rq->msg.con.src_msub_o));
		LOGMSG(env, "Conn Msub S :    0x%8x   0x%8x %12d\n",
				rq->msg.con.src_msub_s,
				ntohl(rq->msg.con.src_msub_s),
				ntohl(rq->msg.con.src_msub_s));
		break;

	case RSKTD_CLOSE_REQ:
		LOGMSG(env, "Close Rem Sn:    0x%8x   0x%8x %12d\n",
				rq->msg.clos.rem_sn, ntohl(rq->msg.clos.rem_sn),
				ntohl(rq->msg.clos.rem_sn));
		LOGMSG(env, "Close Loc Sn:    0x%8x   0x%8x %12d\n",
				rq->msg.clos.loc_sn, ntohl(rq->msg.clos.loc_sn),
				ntohl(rq->msg.clos.loc_sn));
		LOGMSG(env, "Close Force :    0x%8x   0x%8x %12d\n",
				rq->msg.clos.force, ntohl(rq->msg.clos.force),
				ntohl(rq->msg.clos.force));
		break;

	default:
		LOGMSG(env, "UNKNOWN MESSAGE! 0x%8x   0x%8x %12d\n",
				rq->msg_type, ntohl(rq->msg_type),
				ntohl(rq->msg_type));
	}
}

void display_dmn_resp_msg(struct cli_env *env, struct rsktd_resp_msg *rsp)
{
	LOGMSG(env, "MsgType   :    0x%8x   0x%8x %12d\n", rsp->msg_type,
			ntohl(rsp->msg_type), ntohl(rsp->msg_type));
	LOGMSG(env, "MsgSeq    :    0x%8x   0x%8x %12d\n", rsp->msg_seq,
			ntohl(rsp->msg_seq), ntohl(rsp->msg_seq));
	LOGMSG(env, "Err       :    0x%8x   0x%8x %12d %s\n", rsp->err,
			ntohl(rsp->err), ntohl(rsp->err),
			strerror(ntohl(rsp->err)));

	switch( ntohl(rsp->msg_type)) {
	case RSKTD_HELLO_RESP:
		LOGMSG(env, "Hello Ct  :    0x%8x   0x%8x %12d\n",
				rsp->req.hello.ct, ntohl(rsp->req.hello.ct),
				ntohl(rsp->req.hello.ct));
		LOGMSG(env, "Hello Cm  :    0x%8x   0x%8x %12d\n",
				rsp->req.hello.cm_skt,
				ntohl(rsp->req.hello.cm_skt),
				ntohl(rsp->req.hello.cm_skt));
		LOGMSG(env, "Hello MP  :    0x%8x   0x%8x %12d\n",
				rsp->req.hello.cm_mp,
				ntohl(rsp->req.hello.cm_mp),
				ntohl(rsp->req.hello.cm_mp));
		LOGMSG(env, "R     PPid:    0x%8d   0x%dx\n",
				rsp->msg.hello.peer_pid,
				ntohl(rsp->msg.hello.peer_pid));
		break;

	case RSKTD_CONNECT_RESP:
		LOGMSG(env, "Conn Dst Sn:    0x%8x   0x%8x %12d\n",
				rsp->req.con.dst_sn, ntohl(rsp->req.con.dst_sn),
				ntohl(rsp->req.con.dst_sn));
		LOGMSG(env, "Conn Dst Ct:    0x%8x   0x%8x %12d\n",
				rsp->req.con.dst_ct, ntohl(rsp->req.con.dst_ct),
				ntohl(rsp->req.con.dst_ct));
		LOGMSG(env, "Conn Src Sn:    0x%8x   0x%8x %12d\n",
				rsp->req.con.src_sn, ntohl(rsp->req.con.src_sn),
				ntohl(rsp->req.con.src_sn));
		LOGMSG(env, "Conn SrcMso: \"%s\"\n", rsp->req.con.src_mso);
		LOGMSG(env, "Conn SrcMs : \"%s\"\n", rsp->req.con.src_ms);
		LOGMSG(env, "Conn Msub O:   0x%8x   0x%8x %12d\n",
				rsp->req.con.src_msub_o,
				ntohl(rsp->req.con.src_msub_o),
				ntohl(rsp->req.con.src_msub_o));
		LOGMSG(env, "Conn Msub S:   0x%8x   0x%8x %12d\n",
				rsp->req.con.src_msub_s,
				ntohl(rsp->req.con.src_msub_s),
				ntohl(rsp->req.con.src_msub_s));
		LOGMSG(env, "R Acc SN   :   0x%8x   0x%8x %12d\n",
				rsp->msg.con.acc_sn, ntohl(rsp->msg.con.acc_sn),
				ntohl(rsp->msg.con.acc_sn));
		LOGMSG(env, "R Dst SN   :   0x%8x   0x%8x %12d\n",
				rsp->msg.con.dst_sn, ntohl(rsp->msg.con.dst_sn),
				ntohl(rsp->msg.con.dst_sn));
		LOGMSG(env, "R Dst CT   :   0x%8x   0x%8x %12d\n",
				rsp->msg.con.dst_ct, ntohl(rsp->msg.con.dst_ct),
				ntohl(rsp->msg.con.dst_ct));
		LOGMSG(env, "R Dst CMSN :   0x%8x   0x%8x %12d\n",
				rsp->msg.con.dst_dmn_cm_skt,
				ntohl(rsp->msg.con.dst_dmn_cm_skt),
				ntohl(rsp->msg.con.dst_dmn_cm_skt));
		LOGMSG(env, "R Dst MS   :  \"%s\"\n", rsp->msg.con.dst_ms);
		break;

	case RSKTD_CLOSE_RESP:
		LOGMSG(env, "Close Rem Sn:   0x%8x   0x%8x %12d\n",
				rsp->req.clos.rem_sn,
				ntohl(rsp->req.clos.rem_sn),
				ntohl(rsp->req.clos.rem_sn));
		LOGMSG(env, "Close Loc Sn:    0x%8x   0x%8x %12d\n",
				rsp->req.clos.loc_sn,
				ntohl(rsp->req.clos.loc_sn),
				ntohl(rsp->req.clos.loc_sn));
		LOGMSG(env, "Close Force :   0x%8x   0x%8x %12d\n",
				rsp->req.clos.force, ntohl(rsp->req.clos.force),
				ntohl(rsp->req.clos.force));
		LOGMSG(env, "R     Stat:   0x%8x   0x%8x %12d\n",
				rsp->msg.clos.status,
				ntohl(rsp->msg.clos.status),
				ntohl(rsp->msg.clos.status));
		break;

	default:
		LOGMSG(env, "UNKNOWN MESSAGE!\n");
	}
}

extern struct cli_cmd RSKTDReq;

int RSKTDReqCmd(struct cli_env *env, int argc, char **argv)
{
	struct rskt_dmn_speer *speer = &dmn.speers[0];

	if (!speer->alive || speer->i_must_die) {
		LOGMSG(env, "Command not available\n");
		return 0;
	}

	speer->req->msg_type = htonl(getHex(argv[0], 1));
	speer->req->msg_seq = htonl(getDecParm(argv[1], 1));
	
	switch (ntohl(speer->req->msg_type)) {
	case RSKTD_HELLO_REQ:
		if (argc != 5)
			goto show_help;
		speer->req->msg.hello.ct = htonl(getDecParm(argv[2], 1));
		speer->req->msg.hello.cm_skt = htonl(getDecParm(argv[3], 1));
		speer->req->msg.hello.cm_mp = htonl(getDecParm(argv[4], 1));
		break;
	case RSKTD_CONNECT_REQ:
		if (argc != 9)
			goto show_help;
		speer->req->msg.con.dst_sn = htonl(getDecParm(argv[2], 1));
		speer->req->msg.con.dst_ct = htonl(getDecParm(argv[3], 1));
		speer->req->msg.con.src_sn = htonl(getDecParm(argv[4], 1));
		strcpy(speer->req->msg.con.src_mso, argv[5]);
		strcpy(speer->req->msg.con.src_ms, argv[6]);
		speer->req->msg.con.src_msub_o = htonl(getDecParm(argv[7], 1));
		speer->req->msg.con.src_msub_s = htonl(getDecParm(argv[8], 1));
		break;
	case RSKTD_CLOSE_REQ:
		if (argc != 5)
			goto show_help;
		speer->req->msg.clos.rem_sn = htonl(getDecParm(argv[2], 1));
		speer->req->msg.clos.loc_sn = htonl(getDecParm(argv[3], 1));
		speer->req->msg.clos.force = htonl(getDecParm(argv[4], 1));
		break;
	default: 
		break;
	};

	display_dmn_req_msg(env, speer->req);
	LOGMSG(env, "Sending message...\n");
	sem_post(&speer->req_ready);
	LOGMSG(env, "Awaiting response...\n");
	sem_wait(&speer->resp_ready);
	display_dmn_resp_msg(env, speer->resp);

	return 0;
show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values\n");
	cli_print_help(env, &RSKTDReq);

	return 0;
};

struct cli_cmd RSKTDReq = {
"dreq",
4,
2,
"Test RSKT Daemon req message to RSKT Daemon",
"<type> <seq> <msgparms>\n"
	"<type> hello 0x21 connect 0x42 close 0x84\n"
	"<seq> random number to match req with response\n"
	"hello : ct cm_skt cm_mp\n"
	"conn  : dst_sn dst_ct src_sn src_mso src_ms src_msub_o src_msub_s\n"
	"close : rem_sn loc_sn force\n"
	"NOTE: CONN will block when a library accept request is not pending\n",
RSKTDReqCmd,
ATTR_NONE
};

extern struct cli_cmd DMNDmnStatus;

int DMNDmnStatusCmd(struct cli_env *env, int argc, char **argv)
{
	if (0)
		argv[0][0] = argc;

	if (argc) {
		cli_print_help(env, &DMNDmnStatus);
		return 0;
	};

	print_loop_status(env);
	print_ms_status(env, 0, MAX_DMN_NUM_MS);
	display_acc_list(env);
	display_con_list(env);
	display_speers_list(env);
	display_wpeers_list(env);

	return 0;
}

struct cli_cmd DMNDmnStatus = {
"dstatus",
3,
0,
"Display status of daemon library/application connection handling thread\n",
"{<ct>}\n"
	"<ct>: Optionally, shutdown an specified peer connection handler\n",
DMNDmnStatusCmd,
ATTR_NONE
};

extern struct cli_cmd DMNWpeer;

int DMNWpeerCmd(struct cli_env *env, int argc, char **argv)
{
	struct peer_rsktd_addr wpeer;
        uint32_t *mport_list = NULL;
        uint32_t *ep_list = NULL;
        uint32_t *list_ptr;
        uint32_t number_of_eps = 0;
        uint8_t  number_of_mports = RIO_MAX_MPORTS;
        uint32_t ep = 0;
        int i;
        int mport_id;
        int ret = 0;

        ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
        if (ret) {
                printf("ERR: riomp_mgmt_get_mport_list() ERR %d\n", ret);
                return 0;
        }

        printf("\nAvailable %d local mport(s):\n", number_of_mports);
        if (number_of_mports > RIO_MAX_MPORTS) {
                printf("WARNING: Only %d out of %d have been retrieved\n",
                		RIO_MAX_MPORTS, number_of_mports);
        }

        list_ptr = mport_list;
        for (i = 0; i < number_of_mports; i++, list_ptr++) {
                mport_id = *list_ptr >> 16;
                printf("+++ mport_id: %u dest_id: %u\n",
                                mport_id, *list_ptr & 0xffff);

                /* Display EPs for this MPORT */

                ret = riomp_mgmt_get_ep_list(mport_id, &ep_list, &number_of_eps);
                if (ret) {
                        printf("ERR: riodp_ep_get_list() ERR %d\n", ret);
                        break;
                }

                printf("\t%u Endpoints (dest_ID): ", number_of_eps);
                for (ep = 0; ep < number_of_eps; ep++)
                        printf("%u ", *(ep_list + ep));
                printf("\n");

                ret = riomp_mgmt_free_ep_list(&ep_list);
                if (ret)
                        printf("ERR: riodp_ep_free_list() ERR %d\n", ret);

        }

        printf("\n");

        ret = riomp_mgmt_free_mport_list(&mport_list);
        if (ret)
                printf("ERR: riodp_ep_free_list() ERR %d\n", ret);
 
	if (!argc)
		return 0;

	if (argc != 2)
		goto syntax_error;

	wpeer.ct = getDecParm(argv[0], 0);
	wpeer.cm_skt = getDecParm(argv[1], 0);

	LOGMSG(env, "Openning Peer CT %d CM_SKT %d\n", wpeer.ct, wpeer.cm_skt);
	ret = open_wpeers_for_requests(1, &wpeer);
	LOGMSG(env, "Return code %d:%s\n", ret, strerror(ret));
	display_wpeers_list(env);

	return 0;

syntax_error:
	cli_print_help(env, &DMNWpeer);
	return 0;
}

struct cli_cmd DMNWpeer = {
"wpeer",
2,
0,
"Display current peers, or attempts to connect to peer.\n",
"{<ct> <cmskt>}\n"
	"<ct>: Destination ID of peer.\n"
	"<cmskt>: CM socket of the peer.\n"
	"Note: If not parms are entered, display available peers.\n",
DMNWpeerCmd,
ATTR_NONE
};

struct cli_cmd *daemon_cmds[] = 
	{ &RSKTMs,
	  &RSKTMsoh,
	  &RSKTMsub,
	  &RSKTStatus,
	  &RSKTShutdown,
	  &RSKTMpdevs,
	  &RSKTReq,
	  &DMNLibStatus,
	  &RSKTDReq,
	  &DMNDmnStatus,
	  &DMNWpeer
	};

void librsktd_bind_cli_cmds(void)
{
        add_commands_to_cmd_db(sizeof(daemon_cmds)/sizeof(daemon_cmds[0]), 
				daemon_cmds);
	librsktd_bind_sn_cli_cmds();

        return;
};

#ifdef __cplusplus
}
#endif
