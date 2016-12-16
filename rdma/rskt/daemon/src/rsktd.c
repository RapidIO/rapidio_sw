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
#include <stdint.h>
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
#include <arpa/inet.h>

#include "string_util.h"
#include "tok_parse.h"
#include "rio_misc.h"
#include "libcli.h"
#include "librdma.h"
#include "librsktd.h"
#include "librsktd_private.h"
#include "liblog.h"
#include "libfmdd.h"
#include "librsktd_fm.h"
#include "librsktd_dmn.h"

#ifdef __cplusplus
extern "C" {
#endif

struct control_list ctrls;

void print_daemon_help(void)
{
	printf("\nRapidIO Socket daemon is the test software for RDMA.\n");
	printf("It requests a number of memory spaces, and manages\n");
	printf("socket style communication using memory subspaces\n");
	printf("The following options are supported:\n");
	printf("-h, -H, ?: Displays this list of parameters and syntax\n");
	printf("-d	 : Disables display of debug messages. This is the \n");
	printf("	   default operating mode for the daemon.\n");
	printf("-D	 : Displays debug/trace/error messages\n");
	printf("-N	 : Do not initialize memory spaces, test manually\n");
	printf("-t	 : Library connection test mode, no socket conn\n");
	printf("-T	 : RSKTD peer conn test mode, no cm socket conn\n");
	printf("-B	 : Run blindly, without a console\n");
	printf("-e <e_skt> : Remote console connectivity over Ethernet"
							" uses <e_skt>.\n");
	printf("	 The default <e_skt> value is %d.\n", 
							RSKT_DFLT_CLI_SKT);
	printf("-s <num_spaces>: Number of memory spaces to use\n");
	printf("	 Default is %d.\n", DFLT_DMN_NUM_MS);
	printf("	 Maximum is %d.\n", MAX_DMN_NUM_MS);
	printf("-S <size>: Size in kilobytes of memory spaces to request.\n");
	printf("	Valid values are 128, 256, 512, 1024, and 2048.\n");
	printf("	The default value is %d.\n", DFLT_DMN_MS_SIZE);
	printf("-k <size>: Size of rskt socket buffers in kilobytes.\n");
	printf("	Valid values are 2, 4, 8, 16, 32, 64, and 128.\n");
	printf("	Default is %d.\n", DFLT_DMN_SBUF_SIZE);
       	printf("	Must be less than -s value\n");
	printf("	The larger the buffer, the faster the transfer.\n");
	printf("-u <u_skt>: AF_LOCAL <u_skt> socket for rskt library calls\n");
	printf("	 The default value is %d.\n", RSKT_DFLT_APP_PORT_NUM);
	printf("	 The u_skt and Umport value must be correct for\n");
	printf("	 the rskt library to operate correctly.\n");
	printf("-m <Umport>: The local mport associated with the u_skt.\n");
	printf("	 The default value is %d.\n", DFLT_DMN_LSKT_MPORT);
	printf("	 The u_skt and Umport value must be correct for\n");
	printf("	 the rskt library to operate correctly.\n");
	printf("-K <bklg>: Maximum connect request backlog for uskt.\n");
	printf("	 The default value is %d.\n", DFLT_DMN_LSKT_BACKLOG);
	printf("	 The u_skt and Umport value must be correct for\n");
	printf("	 the rskt library to operate correctly.\n");
	printf("-l <loglv>: Current logging level to use.\n");
	printf("	   Default is set to match makefile RDMA_LL parm.\n");
	printf("	   Current default value is %d\n", RDMA_LL);
	printf("-C <cm_skt>: The RSKT daemon listens for connect requests\n");
	printf("	 on RapidIO Channel Manager socket <cm_skt>.\n");
	printf("	 The default value is %d.\n", RSKT_DFLT_DMN_CM_PORT);
	printf("	 The cm_skt and mport value must be correct to\n");
	printf("	 successfully connect.\n");
	printf("-M <Cmport>: The rskt daemon listens for remote connect requests\n");
	printf("	 on this RapidIO mport.\n");
	printf("	 Default value is %d.\n", DFLT_DMN_CM_CONN_MPORT);
	printf("	 The cm_skt and Cmport value must be correct to\n");
	printf("	 successfully connect.\n");
	printf("-P <ct> <skt>: The component tag <ct> and CM channel #\n");
	printf("	 <skt>, of a remotely accessible RSKT daemon.\n");
	printf("	 Maximum entries is %d.\n", MAX_DMN_PEERS);
};

void parse_options(int argc, char *argv[])
{
	int idx;
	uint16_t tmp;

	ctrls.debug = 1;	/* For now */
	ctrls.print_help = 0;
	ctrls.run_cons = 1;
	ctrls.log_level = RDMA_LL;
	ctrls.e_cli_skt = RSKT_DFLT_CLI_SKT;
	ctrls.num_ms = DFLT_DMN_NUM_MS;
	ctrls.ms_size = DFLT_DMN_MS_SIZE;
	ctrls.rskt_buff_size = DFLT_DMN_SBUF_SIZE;
	ctrls.init_ms = DFLT_DMN_INIT;
	ctrls.rsktd_uskt_tst = 0;
	ctrls.rsktd_uskt = RSKT_DFLT_APP_PORT_NUM;
	ctrls.rsktd_u_mp = DFLT_DMN_LSKT_MPORT;
	ctrls.rsktd_u_bklg = DFLT_DMN_LSKT_BACKLOG;
	ctrls.rsktd_cskt_tst = 0;
	ctrls.rsktd_cskt = RSKT_DFLT_DMN_CM_PORT;
	ctrls.rsktd_c_mp = DFLT_DMN_CM_CONN_MPORT;
	ctrls.num_peers = 0;

	for (idx = 0; (idx < argc) && !ctrls.print_help; idx++) {
		if (strnlen(argv[idx], 4) < 2)
			continue;

		if ('-' == argv[idx][0]) {
			switch(argv[idx][1]) {
			case '?': 
			case 'h': 
			case 'H':
				ctrls.print_help = 1;
				break;
			case 'd':
				ctrls.debug = 0;
				break;
			case 'D':
				ctrls.debug = 1;
				break;
			case 't':
				ctrls.rsktd_uskt_tst = 1;
				break;
			case 'T':
				ctrls.rsktd_cskt_tst = 1;
				break;
			case 'N':
				ctrls.init_ms = 0;
				break;
			case 'B':
				ctrls.run_cons = 0;
				break;
			case 'e':
				idx++;
				if (argc < idx) {
					printf("\n<e_skt> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_socket(argv[idx], &ctrls.e_cli_skt, 0)) {
					printf("\n");
					printf(TOK_ERR_SOCKET_MSG_FMT, "<e_skt>");
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 's':
				idx++;
				if (argc < idx) {
					printf("\n<num_spaces> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_ulong(argv[idx], &ctrls.num_ms, 1, MAX_DMN_NUM_MS, 0)) {
					printf("\n");
					printf(TOK_ERR_ULONG_MSG_FMT, "<num_spaces>", 1, MAX_DMN_NUM_MS);
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'S':
				idx++;
				if (argc < idx) {
					printf("\n<size> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_ulong(argv[idx], &ctrls.ms_size,
						128, 2048, 0)) {
					if ((128 != ctrls.ms_size)
						&& (256 != ctrls.ms_size)
						&& (512 != ctrls.ms_size)
						&& (1024 != ctrls.ms_size)
						&& (2048 != ctrls.ms_size)) {
						printf("\nMemory space size must be one of 128, 256, 512, 1024 or 2048\n");
						ctrls.print_help = 1;
						goto exit;
					}
				}
				break;
			case 'k':
				idx++;
				if (argc < idx) {
					printf("\n<size> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_ulong(argv[idx], &ctrls.rskt_buff_size, 2, 128, 0)) {
					if ((2 != ctrls.rskt_buff_size) &&
						(4 != ctrls.rskt_buff_size) &&
						(8 != ctrls.rskt_buff_size) &&
						(16 != ctrls.rskt_buff_size) &&
						(32 != ctrls.rskt_buff_size) &&
						(64 != ctrls.rskt_buff_size) &&
						(128 != ctrls.rskt_buff_size)) {
						printf("\nSocket buffer size must be one of 2, 4, 8, 16, 32, 64 or 128\n");
						ctrls.print_help = 1;
						goto exit;
					}
				}
				break;
			case 'u':
				idx++;
				if (argc < idx) {
					printf("\n<u_skt> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_socket(argv[idx], &ctrls.rsktd_uskt, 0)) {
					printf("\n");
					printf(TOK_ERR_SOCKET_MSG_FMT, "<u_skt>");
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'm': 
				idx++;
				if (argc < idx) {
					printf("\n<mport> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_mport_id(argv[idx], &ctrls.rsktd_u_mp, 0)) {
					printf("\n");
					printf(TOK_ERR_MPORT_MSG_FMT);
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'K':
				idx++;
				if (argc < idx) {
					printf("\n<bklg> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_ulong(argv[idx], &ctrls.rsktd_u_bklg, 1, UINT32_MAX, 0)) {
					printf("\n");
					printf(TOK_ERR_ULONG_MSG_FMT, "<u_bklg>", 1, UINT32_MAX);
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'l':
				idx++;
				if (argc < idx) {
					printf("\n<loglvl> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_log_level(argv[idx], &ctrls.log_level, 0)) {
					printf("\n");
					printf(TOK_ERR_LOG_LEVEL_MSG_FMT);
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'C':
				idx++;
				if (argc < idx) {
					printf("\n<cm_skt> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_socket(argv[idx], &ctrls.rsktd_cskt, 0)) {
					printf("\n");
					printf(TOK_ERR_SOCKET_MSG_FMT, "<cm_skt>");
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'M':
				idx++;
				if (argc < idx) {
					printf("\n<Cmport> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				}
				if (tok_parse_mport_id(argv[idx], &ctrls.rsktd_c_mp, 0)) {
					printf("\n");
					printf(TOK_ERR_MPORT_MSG_FMT);
					ctrls.print_help = 1;
					goto exit;
				}
				break;
			case 'P':
				if (argc < (idx + 2)) {
					printf("\nMissing peer parms\n");
					ctrls.print_help = 1;
					goto exit;
				}
				idx++;
				if (tok_parse_ul(argv[idx], &ctrls.peers[ctrls.num_peers].ct, 0)) {
					printf("\n");
					printf(TOK_ERR_UL_HEX_MSG_FMT, "component tag");
					ctrls.print_help = 1;
					goto exit;
				}
				idx++;
				if (tok_parse_socket(argv[idx], &tmp, 0)) {
					printf("\n");
					printf(TOK_ERR_SOCKET_MSG_FMT, "Socket number");
					ctrls.print_help = 1;
					goto exit;
				}
				ctrls.peers[ctrls.num_peers].cm_skt = tmp;
				ctrls.num_peers++;
				break;
			default:
				printf("\nUnknown parm: \"%s\"\n", argv[idx]);
				ctrls.print_help = 1;
			};
		};
	}

exit:
	ctrls.ms_size = ctrls.ms_size * 1024;
	ctrls.rskt_buff_size = ctrls.rskt_buff_size * 1024;
	return;
}

void set_prompt(struct cli_env *e)
{
        if (e != NULL) {
               SAFE_STRNCPY(e->prompt, "RSKTDaemon> ", sizeof(e->prompt));
        };
};

struct console_globals cli;

void rskt_daemon_shutdown(void);

void quit_command_customization(struct cli_env *UNUSED_PARM(env))
{
	rskt_daemon_shutdown();
};
	
void spawn_threads(void)
{
	int  cli_ret = 0, console_ret = 0;
	struct remote_login_parms *rlp;

	rlp = (struct remote_login_parms *)
					malloc(sizeof(struct remote_login_parms));
	if (NULL == rlp) {
		printf("\nCould not allocate memory for login parameters\n");
		exit(EXIT_FAILURE);
	}

	sem_init(&cli.cons_owner, 0, 0);

	cli.cli_portno = 0;
	cli.cli_alive = 0;
	cli.cons_alive = 0;

	DBG("ENTER\n");
	cli_init_base(quit_command_customization);
	librsktd_bind_cli_cmds();
	liblog_bind_cli_cmds();
	/* FIXME: The call to fmdd_bind_dbg_cmds(dd_h);  should go here,
	* but due to the exigencies of the initialization sequence it's
	* actually done in librkstd_fm.c.
	*/

	/* Prepare and start console thread */
	if (ctrls.run_cons) {
		struct cli_env t_env;

		init_cli_env(&t_env);
		splashScreen(&t_env, (char *)"RDMA Socket Daemon Console");
		console_ret = pthread_create( &cli.cons_thread, NULL, 
						console, 
					(void *)((char *)"RSKTD > "));
		if(console_ret) {
			CRIT("Failed to create console_thread: %s\n", strerror(console_ret));
			exit(EXIT_FAILURE);
		};
		INFO("Console thread started\n");
		pthread_detach(cli.cons_thread);
	};


	/* Start remote_login_thread, enabling remote debug over Ethernet */
	rlp->portno = ctrls.e_cli_skt;
	SAFE_STRNCPY(rlp->thr_name, "RSKTD_RCLI", sizeof(rlp->thr_name));
	rlp->status = &cli.cli_alive;

	cli_ret = pthread_create( &remote_login_thread, NULL, remote_login, 
								(void *)(rlp));
	if(cli_ret) {
		CRIT("Failed to create remote_login_thread: %s\n",
							strerror(cli_ret));
		exit(EXIT_FAILURE);
	}
	INFO("CLI thread started\n");

	if (spawn_daemon_threads(&ctrls)) {
		CRIT("spawn_daemon_threads FAILED");
		exit(EXIT_FAILURE);
	};
}

void rskt_daemon_shutdown(void)
{
	DBG("ENTER\n");
	kill_daemon_threads();
     	DBG("EXIT\n");
};

void sig_handler(int signo)
{
	INFO("Rx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		INFO("Shutting down\n");
		rskt_daemon_shutdown();
		exit(1);
	}

	if (signo == SIGUSR1) {
		/* Ignore signal */
		return;
	}

	if (signo == SIGPIPE) {
		/* Ignore signal */
		return;
	}
};

int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;

	ctrls.debug = 1;	/* For now */

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGPIPE, sig_handler);

	rdma_log_init("rsktd_log.txt", 1);

	cli.all_must_die = 0;
	cli.cli_alive = 0;

	parse_options(argc, argv);

	g_level = ctrls.log_level;
	
	if (ctrls.print_help) {
		print_daemon_help();
		goto exit;
	};

	DBG("Spawning rsktd threads\n");
	sem_init(&dmn.graceful_exit, 0, 0);
	spawn_threads();

	if (daemon_threads_failed() && ctrls.init_ms) {
		CRIT("Failed to create daemon threads\n");
		rskt_daemon_shutdown();
	}
	CRIT("RSKTD Running...\n");

	sem_wait(&dmn.graceful_exit);
	rskt_daemon_shutdown();

	printf("\nRDMA Socket Deamon Graceful EXIT!!!!\n");
	rdma_log_close();
	rc = EXIT_SUCCESS;
exit:
	exit(rc);
}

#ifdef __cplusplus
}
#endif

