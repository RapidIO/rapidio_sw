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
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include "string_util.h"
#include "rio_misc.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "rapidio_mport_dma.h"
#include "mportmgmt.h"
#include "mportcmsock.h"
#include "libcli.h"
#include "liblog.h"
#include "libfxfr_private.h"

#define MAX_IBWIN 8

#ifdef __cplusplus
extern "C" {
#endif

uint8_t debug;

void print_server_help(void)
{
	printf("\nThe file_xfer server accepts requests to send files.\n");
	printf("The following options are supported:\n");
	printf("-h, -H, ?: Displays an explanation of parameters and syntax\n");
	printf("-d	 : Disables display of debug messages. This is the \n");
	printf("	   default operating mode for the server.\n");
	printf("-D	 : Displays debug/trace/error messages\n");
	printf("-c <skt> : Supports remote console connectivity using"
							" <skt>.\n");
	printf("	 The default <skt> value is 4444.\n");
	printf("	 Note: There must be a space between \"-c\""
							" and <skt>.\n");
	printf("-i <rio_ibwin_base>: Use <rio_ibwin_base> as the starting\n");
	printf("	 RapidIO address for inbound RDMA windows.\n");
	
	printf("	 The default value is 0x%x.\n", TOTAL_TX_BUFF_SIZE); 
	printf("	 Note: There must be a space between -i and \n");
	printf("-m<mport>: Accept requests on <mport>. 0-9 is the range of\n");
	printf("	 valid mport values.\n");
	printf("	 The default mport value is 0. \n");
	printf("	Note: There must not be a space between -m and"
							" <mport>.\n");
	printf("-n, -N: Disables the command line interface for this"
							" server.\n");
	printf("	 Note that a server command line can still be\n");
	printf("	 accessed using Ethernet sockets (see the \"-c\""
							" parameter).\n");
	printf("-S <size>: Size in kilobytes of each inbound RDMA window.\n");
	printf("	Valid values: 128, 256, 512, 1024, 2048, and 4096.\n");
	printf("	The default value is %d.\n", TOTAL_TX_BUFF_SIZE/1024);
	printf("	The larger the window, the faster the transfer.\n");

	printf("-W<ibwin_cnt>: Attempts to get <ibwin_cnt> inbound RDMA"
						" windows.\n");
	printf("	 The default value is 1.\n");
	printf("	 The maximum number of parallel file transfers is\n");
	printf("	 equal to the number of inbound RDMA windows. The\n");
	printf("	 server will continue to operate as long as it has\n");
	printf("	 at least one inbound RDMA window.\n");
	printf("-X <cm_skt>: The server listens for file transfer requests\n");
	printf("	 on RapidIO Channel Manager socket <cm_skt>.\n");
	printf("	 The default value is 5555.\n");
	printf("	 The cm_skt and mport value must be correct to\n");
	printf("	 successfully connect .\n");
	printf("	 Note: There must be a space between -X and"
							" <cm_skt>.\n");
};
void parse_options(int argc, char *argv[], 
		int *cons_skt,
		int *print_help,
		uint8_t *mport_num,
		int *run_cons,
		int *win_size,      
		int *num_win, 
		int *xfer_skt,
	       	uint64_t *ibwin_base )   
{
	int idx;

	*cons_skt = 8754;
	*print_help = 0;
	*mport_num = 0;
	*run_cons = 1;
	*win_size = TOTAL_TX_BUFF_SIZE/1024;
	*num_win = 1;
	*xfer_skt = 5555;
	*ibwin_base = TOTAL_TX_BUFF_SIZE;

	for (idx = 0; (idx < argc) && !*print_help; idx++) {
		if (strnlen(argv[idx], 4) < 2)
			continue;

		if ('-' == argv[idx][0]) {
			switch(argv[idx][1]) {
			case 'd': debug = 0;
				break;
			case 'D': debug = 1;
				break;
			case 'c': 
			case 'C': if (argc < (idx+1)) {
					  printf("\n<skt> not specified\n");
					  *print_help = 1;
					  goto exit;
				} else {
					idx++;
					*cons_skt = atoi(argv[idx]);
				};
				break;
			case '?': 
			case 'h': 
			case 'H': *print_help = 1;
				  break;
			case 'i': 
			case 'I': if (argc < (idx+1)) {
					  printf("\n<base> not specified\n");
					  *print_help = 1;
					  goto exit;
				} else {
					idx++;
					*ibwin_base = atoi(argv[idx]);
				};
				break;
			case 'm': 
			case 'M': 
				if ((argv[idx][2] >= '0') && 
				    (argv[idx][2] <= '9')) {
					*mport_num = argv[idx][2] - '0';
				} else {
					printf("\n<mport> invalid\n");
					*print_help = 1;
					goto exit;
				};
				break;
			case 'n': 
			case 'N': *run_cons = 0;
				  break;
			case 's': 
			case 'S': if (argc < (idx+1)) {
					  printf("\n<size> not specified\n");
					  *print_help = 1;
					  goto exit;
				};
				idx++;
				*win_size = atoi(argv[idx]);
				if (*win_size > (TOTAL_TX_BUFF_SIZE/1024)) {
					printf("\n<size> exceeds max\n");
					*print_help = 1;
					goto exit;
				};
				break;
			case 'w':
			case 'W': if ((argv[idx][2] >= '0') && 
				    (argv[idx][2] <= '9')) {
					*num_win = argv[idx][2] - '0';
				} else {
					printf("\n<ibwin_cnt> invalid\n");
					*print_help = 1;
					goto exit;
				};
				break;
			case 'x': 
			case 'X': if (argc < (idx+1)) {
					  printf("\n<skt> not specified\n");
					  *print_help = 1;
					  goto exit;
				} else {
					idx++;
					  *xfer_skt = atoi(argv[idx]);
				};
				  break;
			default: printf("\nUnknown parm: \"%s\"\n", argv[idx]);
				*print_help = 1;
			};
		};
	}
	*win_size = *win_size * 1024;
exit:
	return;
}

pthread_t conn_thread, cli_session_thread, console_thread;
int cli_session_portno;
int cli_session_alive;

sem_t cons_owner;

void set_prompt(struct cli_env *e)
{
        if (e != NULL) {
                SAFE_STRNCPY(e->prompt, "FXServer> ", sizeof(e->prompt));
        };
};

uint8_t mp_h_mport_num;
struct riomp_mgmt_mport_properties qresp;
RIOMemOpsIntf* memops = NULL;
int mp_h_valid;

struct ibwin_info ibwins[MAX_IBWIN];

struct req_list_t {
	struct req_list_t *next;
	MportCMSocket *skt;
};

/* NOTE: list is empty when HEAD == NULL.
 * When HEAD == NULL, tail is invalid.
 */

struct req_list_head_t {
	struct req_list_t *head;
	struct req_list_t *tail;
};

MportCMSocket* pop_conn_req(struct req_list_head_t *list);

int all_must_die;
sem_t conn_reqs_mutex;
struct req_list_head_t conn_reqs;
uint16_t num_conn_reqs;
uint8_t pause_file_xfer_conn;
uint8_t max_file_xfer_queue;

uint8_t last_ibwin; /* 0-7 means next window to be cleared */


sem_t conn_loop_started;
int conn_loop_alive;
int conn_skt_num;
extern struct cli_cmd FXStatus;

int FXStatusCmd(struct cli_env *env, int argc, char **argv)
{
        int        idx, st_idx = 0, max_idx = MAX_IBWIN;

	if (argc) {
		st_idx = getDecParm(argv[0], 0);
		max_idx = st_idx+1;
		if ((st_idx >= MAX_IBWIN) || (argc > 2))
			goto show_help;
		if (argc > 1)
			ibwins[st_idx].debug = getDecParm(argv[1], 0)?1:0;
	};

	
	sprintf(env->output, 
	"\nWin V   RapidIO Addr    Size    Memory Space PHYS TV D C RC\n");
        logMsg(env);
	for (idx = st_idx; idx < max_idx; idx++) {
		sprintf(env->output, 
			"%2d  %1d %16lx %8lx  %16lx  %1d %1d %1d %8x\n",
				idx,
				ibwins[idx].valid,
				(long unsigned int)ibwins[idx].rio_base,
				(long unsigned int)ibwins[idx].length,
				(long unsigned int)ibwins[idx].handle,
				ibwins[idx].thr_valid,
				ibwins[idx].debug,
				ibwins[idx].completed,
				ibwins[idx].rc);
		logMsg(env);
	}
	sprintf(env->output, "\nall_must_die status : %d\n", all_must_die);
        logMsg(env);
	sprintf(env->output, "XFER Socket #       : %d\n", conn_skt_num);
        logMsg(env);
	sprintf(env->output, "XFER conn_loop_alive: %d\n", conn_loop_alive);
        logMsg(env);
	sprintf(env->output, "Pending conn reqs   : %s\n", 
			(conn_reqs.head)?"AVAILABLE":"None");
        logMsg(env);
	sprintf(env->output, "\nCLI Sessions Alive : %d\n", cli_session_alive);
        logMsg(env);
	sprintf(env->output, "CLI Socket #       : %d\n", cli_session_portno);
        logMsg(env);
	sprintf(env->output, "\nServer mport       : %d\n", mp_h_mport_num);
        logMsg(env);
	sprintf(env->output, "Server destID      : %x\n", qresp.hdid);
        logMsg(env);

	return 0;

show_help:
	sprintf(env->output, "\nFAILED: Extra parms or invalid values\n");
        logMsg(env);
	cli_print_help(env, &FXStatus);

	return 0;
};

struct cli_cmd FXStatus = {
"status",
2,
0,
"File transfer status command.",
"{IDX {DBG}}\n" 
        "Dumps the status of the file transfer state database.\n"
	"<IDX> : 0-7, Optionally limits display to a single process.\n"
	"<DBG> : 0|1  Disables (0) or enables (1) debug statements.\n",
FXStatusCmd,
ATTR_RPT
};

void fxfr_server_shutdown(void);

int FXShutdownCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{
	fxfr_server_shutdown();
	sprintf(env->output, "Shutdown initiated...\n"); 
	logMsg(env);

	return 0;
};

struct cli_cmd FXShutdown = {
"shutdown",
8,
0,
"Shutdown server.",
"No Parameters\n" 
        "Shuts down all threads, including CLI.\n",
FXShutdownCmd,
ATTR_NONE
};

int FXMpdevsCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc) {
                sprintf(env->output,
			"FAILED: Extra parameters ignored: %s\n", argv[0]);
		logMsg(env);
	};

        std::vector<uint32_t> mport_list;
        if (! MportMgmt::get_mport_list(mport_list)) {
                sprintf(env->output,
			"ERR: riomp_mport_get_mport_list() ERR\n");
		logMsg(env);
                return 0;
	}

	const int number_of_mports = mport_list.size();

        printf("\nAvailable %d local mport(s):\n", number_of_mports);

        for (int i = 0; i < number_of_mports; i++) {
                const int mport_id = mport_list[i];

                sprintf(env->output, "+++ mport_id: %u\n", mport_id);
		logMsg(env);

                /* Display EPs for this MPORT */

		std::vector<uint32_t> ep_list;
                if (! MportMgmt::get_ep_list(mport_id, ep_list)) {
                	sprintf(env->output,
                        	"ERR: riomp_ep_get_list() ERR\n");
			logMsg(env);
                        break;
                }

		const int number_of_eps = ep_list.size();

                sprintf(env->output, "\t%u Endpoints (dest_ID): ", 
			number_of_eps);
		logMsg(env);
                for (int ep = 0; ep < number_of_eps; ep++) {
                	sprintf(env->output, "%u ", ep_list[i]);
			logMsg(env);
		}

                sprintf(env->output, "\n");
		logMsg(env);
        }

	sprintf(env->output, "\n");
	logMsg(env);

	return 0;
}

struct cli_cmd FXMpdevs = {
"mpdevs",
2,
0,
"Query mport info.",
"No Parameters\n" 
        "Displays available mports, and associated target destigation IDs.\n",
FXMpdevsCmd,
ATTR_NONE
};

void print_file_xfer_status(struct cli_env *env)
{
	sprintf(env->output, "\nFile transfer status: %s\n", 
		pause_file_xfer_conn?"PAUSED":"running");
	logMsg(env);

	if (pause_file_xfer_conn) {
		sprintf(env->output, "Maximum Q length: %d\n",
			max_file_xfer_queue);
		logMsg(env);
		
		sprintf(env->output, "Current Q length: %d\n",
			num_conn_reqs);
		logMsg(env);
	};
};
		
void batch_start_connections(void)
{
	int i;

	pause_file_xfer_conn = 0;

	for (i = 0; (i < MAX_IBWIN) && num_conn_reqs; i++) {
		if (ibwins[i].valid && ibwins[i].completed) {
			ibwins[i].req_skt = pop_conn_req(&conn_reqs);
			ibwins[i].completed = 0;
			sem_post(&ibwins[i].req_avail);
		};
	};
};
	
	
int FXPauseCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc) {
		pause_file_xfer_conn = getDecParm(argv[0], 0)?1:0;
		max_file_xfer_queue = 8;
	};
	if (argc > 1) {
		max_file_xfer_queue = getDecParm(argv[1], 0);
	};

	if ((num_conn_reqs >= max_file_xfer_queue) ||
		(!pause_file_xfer_conn && num_conn_reqs)) {

		sem_wait(&conn_reqs_mutex);
		batch_start_connections();
		sem_post(&conn_reqs_mutex);
	};

	print_file_xfer_status(env);

	return 0;
}

struct cli_cmd FXPause = {
"pause",
5,
0,
"Pause file request transfer processing",
"pause {<PAUSE> {<NUMREQS>}}\n"
	"PAUSE is either 0 (Transfer files) or 1 (Halt file transfers).\n"
	"NUMREQS: 0-255, maximum number of requests pending before PAUSE is\n"
	"         set back to 0.\n"
	"         Default value is 8 when PAUSE is set to 1.\n",
FXPauseCmd,
ATTR_NONE
};

struct cli_cmd *server_cmds[] = 
	{ //&FXIbwin,
	  &FXStatus,
	  &FXShutdown,
	  &FXMpdevs,
	  &FXPause
	};

void bind_server_cmds(void)
{
	all_must_die = 0;
	last_ibwin = MAX_IBWIN;
	num_conn_reqs = 0;
	pause_file_xfer_conn = 0;
	max_file_xfer_queue = 0;
        add_commands_to_cmd_db(sizeof(server_cmds)/sizeof(server_cmds[0]), 
				server_cmds);

        return;
};

void add_conn_req(struct req_list_head_t *list, MportCMSocket* new_socket)
{
	struct req_list_t *new_req;

	new_req = (struct req_list_t *)(malloc(sizeof(struct req_list_t)));
	new_req->skt = new_socket;
	new_req->next = NULL;

	if (NULL == list->head) {
		list->head = list->tail = new_req;
	} else {
		list->tail->next = new_req;
		list->tail = new_req;
	};
	num_conn_reqs++;
};

MportCMSocket* pop_conn_req(struct req_list_head_t *list)
{
	MportCMSocket *skt = NULL;
	struct req_list_t *temp;

	if (NULL != list->head) {
		skt = list->head->skt;
		temp = list->head;
		list->head = list->head->next;
		free(temp);
		num_conn_reqs--;
	};
	return skt;
};

void prep_info_for_xfer(struct ibwin_info *info)
{
	bzero(info->file_name, MAX_FILE_NAME);
	info->rc = 0;
	info->fd = -1;
	info->bytes_rxed = 0;
};

void *xfer_loop(void *ibwin_idx)
{
	struct ibwin_info *info;
	int idx = *(int *)(ibwin_idx);

	info = &ibwins[idx];

	info->req_skt = NULL;
	info->completed = 1;

	if (debug)
		printf("\nxfer_loop idx %d starting\n", idx);

	while(!all_must_die) {
		struct timespec req = {2, 0}, rem;

		prep_info_for_xfer(info);
		if (NULL == info->req_skt)
			sem_wait(&info->req_avail);

		if (info->debug)
			printf("Thread %d processing connect request.\n", idx);
		info->rc = rx_file(info, &all_must_die);

   		nanosleep(&req, &rem);

		if (NULL != info->req_skt) {
			delete info->req_skt; info->req_skt = NULL;
		};

		if (!pause_file_xfer_conn) {
			sem_wait(&conn_reqs_mutex);
			info->req_skt = pop_conn_req(&conn_reqs);
			if (NULL == info->req_skt)
				info->completed = 1;
			sem_post(&conn_reqs_mutex);
		};
	};

	info->thr_valid = 0;
	info->valid = 0;

	if (info->debug || debug)
		printf("\nxfer_loop idx %d EXITING\n", idx);

	*(int *)(ibwin_idx) = EXIT_SUCCESS;
	pthread_exit(ibwin_idx);
};

MportCMSocket* conn_skt = NULL;

void *conn_loop(void *ret)
{
	int rc = 1;
	MportCMSocket* new_socket = NULL;
	uint8_t found_one = 0;
	int i;
	int xfer_skt_num;
	
	conn_loop_alive = 0;

	if (NULL == ret) {
		if (debug)
			printf("conn_loop: Parameter is null, exiting\n");
		goto exit;
	};

	xfer_skt_num = *(int *)(ret);
	conn_skt_num = xfer_skt_num;

	conn_skt = new MportCMSocket(mp_h_mport_num, 0);

	rc = conn_skt->bind(xfer_skt_num);
	if (rc) {
		if (debug)
			printf("conn_loop: riomp_sock_bind() ERR %d (%s)\n", rc, strerror(errno));
		goto close_skt;
	}

	rc = conn_skt->listen();
	if (rc) {
		if (debug)
			printf("conn_loop: riomp_sock_listen() ERR %d (%s)\n", rc, strerror(errno));
		goto close_skt;
	}

	if (debug)
		printf("\nSERVER File Transfer bound to socket %d\n", xfer_skt_num);

	conn_loop_alive = 1;
	sem_post(&conn_loop_started);

	while (!all_must_die) {
		rc = conn_skt->accept(new_socket, 0);
		if (rc) {
			if ((errno == ETIME) || (errno == EINTR))
				continue;
			if (debug)
				printf("conn_loop: riomp_accept() ERR %d (%s)\n", rc, strerror(errno));
			break;
		}

		sem_wait(&conn_reqs_mutex);

		if (all_must_die) {
			delete new_socket; new_socket = NULL;
			sem_post(&conn_reqs_mutex);
			continue;
		};

		found_one = 0;
		for (i = 0; (i < MAX_IBWIN) && !pause_file_xfer_conn; i++) {
			if (ibwins[i].valid && ibwins[i].completed) {
				ibwins[i].req_skt = new_socket;
				ibwins[i].completed = 0;
				sem_post(&ibwins[i].req_avail);
				found_one = 1;
				break;
			};
		};

		if (!found_one)
			add_conn_req( &conn_reqs, new_socket);

		if (pause_file_xfer_conn && 
				(num_conn_reqs >= max_file_xfer_queue))
			batch_start_connections();

		sem_post(&conn_reqs_mutex);
		new_socket = NULL;
	}

	/* Make sure all connection requests are cleaned up */
	if (NULL != new_socket)
		delete new_socket;

	new_socket = pop_conn_req(&conn_reqs);

	while (NULL != new_socket) {
		delete new_socket;
		if (rc && debug)
			printf("conn_loop: nskt riomp_socket_close ERR %d\n", 
				rc);
		new_socket = pop_conn_req(&conn_reqs);
	};
		
close_skt:
	delete conn_skt; conn_skt = NULL;

exit:
	if (debug)
		printf("\nSERVER File Transfer EXITING\n");
	conn_loop_alive = 0;
	sem_post(&conn_loop_started);
	*(int *)(ret) = rc;
	pthread_exit(ret);
};

int setup_mport(uint8_t mport_num, uint8_t num_win, uint32_t win_size, 
		uint64_t UNUSED_PARM(ibwin_base), int UNUSED_PARM(xfer_skt_num))
{
	int rc = -1;
	uint8_t i;

	all_must_die = 0;

	mp_h_mport_num = mport_num;
	memops = RIOMemOpsChanMgr(mport_num, true /*shared*/, ANY_CHANNEL);
	mp_h_valid = 1;

	{{
	  MportMgmt* mp_h = new MportMgmt(mport_num);
	  if (mp_h->query(qresp)) {
		printf("\nUnable to query mport %d...\n", mport_num);
		delete mp_h;
		goto close_mport;
	  }
	  delete mp_h;
	}}

	if (debug) {
		std::string s = MportMgmt::toString(qresp);
		printf("%s", s.c_str());
	}

	if (!(qresp.flags & RIO_MPORT_DMA)) {
		printf("\nMport %d has no DMA support...\n", mport_num);
		goto close_mport;
	};

	for (i = 0; i < MAX_IBWIN; i++) {
		ibwins[i].valid = FALSE;
		ibwins[i].thr_valid = FALSE;
		sem_init(&ibwins[i].req_avail, 0, 0);
		ibwins[i].handle = 0;
		ibwins[i].length = 0;
		ibwins[i].ib_mem = (char *)MAP_FAILED;
		ibwins[i].msg_rx = NULL;
		ibwins[i].msg_tx = NULL;
		ibwins[i].rxed_msg = NULL;
		ibwins[i].tx_msg = NULL;
		ibwins[i].msg_buff_size = 0;
		ibwins[i].debug = 0;
		ibwins[i].bytes_rxed = 0;
	};

	for (i = 0; i < num_win; i++) {
		memset(&ibwins[i].mem, 0, sizeof(ibwins[i].mem));
		ibwins[i].mem.rio_address = RIO_ANY_ADDR;
		bool rc = memops->alloc_ibwin(ibwins[i].mem, win_size);
		if (!rc) goto close_ibwin;

		ibwins[i].handle = ibwins[i].mem.win_handle;
		ibwins[i].rio_base = ibwins[i].handle; // XXX really?
		ibwins[i].ib_mem = (char*)ibwins[i].mem.win_ptr;
		ibwins[i].length = win_size;
		ibwins[i].debug  = debug;

		ibwins[i].valid = 1;
	};

	return 0;

close_ibwin:
	for (i = 0; i < MAX_IBWIN; i++) {
		if (ibwins[i].mem.win_size > 0)
			memops->free_xwin(ibwins[i].mem);
	};
close_mport:
	if (mp_h_valid) {
		delete memops; memops = NULL;
		mp_h_valid = 0;
	};

	return rc;
};

void *cli_session( void *sock_num )
{
	int sockfd, newsockfd = -1, portno;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	char buffer[256];
	int one = 1;
	int session_num = 0;

	portno = *(int *)(sock_num);
	cli_session_portno = portno;

	free(sock_num);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
        	CRIT("ERROR opening socket");
		goto fail;
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(portno);
	setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
	setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))<0) {
        	CRIT("ERROR on binding port %d", cli_session_portno);
		goto fail;
	}

	INFO("\nSERVER Remote CLI bound to socket %d\n", portno);
	sem_post(&cons_owner);
	cli_session_alive = 1;
	while (!all_must_die && strncmp(buffer, "done", 4)) {
		struct cli_env env;

		env.script = NULL;
		env.fout = NULL;
		bzero(env.output, BUFLEN);
		bzero(env.input, BUFLEN);
		env.DebugLevel = 0;
		env.progressState = 0;
		env.sess_socket = -1;
		env.h = NULL;
		bzero(env.prompt, PROMPTLEN+1);
		set_prompt( &env );

		listen(sockfd,5);
		clilen = sizeof(cli_addr);
		env.sess_socket = accept(sockfd, 
				(struct sockaddr *) &cli_addr, 
				&clilen);
		if (env.sess_socket < 0) {
			CRIT("ERROR on accept");
			goto fail;
		};
		printf("\nStarting session %d\n", session_num);
		cli_terminal( &env );
		printf("\nFinishing session %d\n", session_num);
		close(env.sess_socket);
		session_num++;
	};
fail:
	cli_session_alive = 0;
	if (debug)
		printf("\nSERVER REMOTE CLI Thread Exiting\n");
	if (newsockfd >=0)
		close(newsockfd);
     	close(sockfd);

	sock_num = malloc(sizeof(int));
	*(int *)(sock_num) = portno;
	pthread_exit(sock_num);
}

int spawned_threads;
void fxfr_server_shutdown_cli(struct cli_env *env);

void spawn_threads(int cons_skt, int xfer_skt, int run_cons)
{
	int  conn_ret, cli_ret, cons_ret = 0;
	int *pass_sock_num, *pass_cons_ret, *conn_loop_rc;
	int i;

	all_must_die = 0;
	conn_loop_alive = 0;
	cli_session_alive = 0;
	spawned_threads = 1;

	cli_init_base(fxfr_server_shutdown_cli);
	bind_server_cmds();
	liblog_bind_cli_cmds();

	sem_init(&cons_owner, 0, 0);

	cli_session_portno = 0;

	/* Prepare and start console thread */
	if (run_cons) {
		pass_cons_ret = (int *)(malloc(sizeof(int)));
		*pass_cons_ret = 0;
		splashScreen((char *)
			"RTA File Transfer Server Command Line Interface");

		cons_ret = pthread_create( &console_thread, NULL, 
				console, (void *)((char *)"FXServer> "));
		if(cons_ret) {
			printf("\nError cons_thread rc: %d\n", cons_ret);
			exit(EXIT_FAILURE);
		}
	};

	/* Prepare and start CM connection handling thread */
	sem_init(&conn_reqs_mutex, 0, 0);
	conn_reqs.head = conn_reqs.tail = NULL;
	sem_post(&conn_reqs_mutex);

	sem_init(&conn_loop_started, 0, 0);

	conn_loop_rc = (int *)(malloc(sizeof(int)));
	*conn_loop_rc = xfer_skt;
	conn_ret = pthread_create(&conn_thread, NULL, conn_loop, 
				(void *)(conn_loop_rc));
	if (conn_ret) {
		printf("Error - conn_thread rc: %d\n", conn_ret);
		exit(EXIT_FAILURE);
	}
 
	for (i = 0; i < MAX_IBWIN; i++) {
		int *pass_idx;
		int ibwin_ret;

		if (!ibwins[i].valid)
			continue;

		pass_idx = (int *)(malloc(sizeof(int)));
		*pass_idx = i;

		ibwin_ret = pthread_create( &ibwins[i].xfer_thread, NULL, 
				xfer_loop, (void *)(pass_idx));
		if (ibwin_ret) {
			printf("\nCould not create fxfr rx thread %d, rc=%d\n", 
					i, ibwin_ret);
		}
		ibwins[i].thr_valid = 1;
	};

	/* Start cli_session_thread, enabling remote debug over Ethernet */
	pass_sock_num = (int *)(malloc(sizeof(int)));
	*pass_sock_num = cons_skt;

	cli_ret = pthread_create( &cli_session_thread, NULL, cli_session, 
				(void *)(pass_sock_num));
	if(cli_ret) {
		fprintf(stderr,"Error - cli_session_thread rc: %d\n",cli_ret);
		exit(EXIT_FAILURE);
	}

	if (debug) {
		printf("pthread_create() for conn_loop returns: %d\n",
			conn_ret);
		printf("pthread_create() for cli_session_thread returns: %d\n",
			cli_ret);
		if (run_cons) 
			printf("pthread_create() for console returns: %d\n", 
				cons_ret);
	};
}
 
int init_srio_api(uint8_t mport_id);

void fxfr_server_shutdown(void) {
	int idx;

	if (!all_must_die && spawned_threads) {
		if (conn_loop_alive)
			pthread_kill(conn_thread, SIGHUP);
		if (cli_session_alive)
			pthread_kill(cli_session_thread, SIGHUP);
	};

	all_must_die = 1;
	
	/* Make sure all request processing threads die... */
	for (idx = 0; (idx < MAX_IBWIN) && spawned_threads; idx++) {
		sem_post(&conn_reqs_mutex);
		if (ibwins[idx].valid) {
			if (ibwins[idx].thr_valid) {
				sem_post(&ibwins[idx].req_avail);
			} else {
				memops->free_xwin(ibwins[idx].mem);
			};
		};
	};
};

void fxfr_server_shutdown_cli(struct cli_env *UNUSED(env))
{
	fxfr_server_shutdown();
};

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		printf("Shutting down\n");
		fxfr_server_shutdown();
	};
};

int main(int argc, char *argv[])
{
	int cons_skt, print_help;
	uint8_t mport_num = 0;
	int rc = EXIT_FAILURE;
	int i;
	int run_cons, win_size, num_win, xfer_skt;
	uint64_t rio_base;

	debug = 0;

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	parse_options(argc, argv, &cons_skt, &print_help, &mport_num, &run_cons,
		&win_size, &num_win, &xfer_skt, &rio_base);   
	
	if (print_help) {
		print_server_help();
		goto exit;
	};

	rdma_log_init("fxfr_server", 1);
	if (setup_mport(mport_num, num_win, win_size, rio_base, xfer_skt)) {
		console(NULL);
	};

	spawn_threads(cons_skt, xfer_skt, run_cons);

	sem_wait(&conn_loop_started);

	if (!conn_loop_alive)
		fxfr_server_shutdown();

	pthread_join(conn_thread, NULL);
	for (i = 0; i < MAX_IBWIN; i++) {
		if (ibwins[i].thr_valid)
			pthread_join(ibwins[i].xfer_thread, NULL);
	};

	if (run_cons)
		pthread_join(console_thread, NULL);
 
	/* pthread_join(cli_session_thread, NULL); */

	if (mp_h_valid) {
		delete memops; memops = NULL;
		mp_h_valid = 0;
	}

	printf("\nFILE TRANSFER SERVER EXITING!!!!\n");
	rc = EXIT_SUCCESS;
exit:
	rdma_log_close();
	exit(rc);
}

#ifdef __cplusplus
}
#endif
