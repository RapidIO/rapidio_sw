/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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

#include <stdint.h>
#include <unistd.h>
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
#include <sys/time.h>

#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "string_util.h"
#include "tok_parse.h"
#include "rio_misc.h"
#include <rapidio_mport_mgmt.h>

#include "libcli.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librdma.h"
#include "liblog.h"
#include "rskts_info.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RSKT_SVR_DFLT_MP_NUM 0
#define RSKT_SVR_MAX_MP_NUM 7
#define RSKT_SVR_DFLT_CM_SKT 8376
#define RSKT_SVR_DFLT_NUM_MS 1
#define RSKT_SVR_MAX_NUM_MS 8
#define RSKT_SVR_DFLT_MS_LEN 2048
#define RSKT_SVR_DFLT_MSUB_LEN 64
#define RSKT_SVR_DFLT_DBG_SKT 1123
#define RSKT_SVR_DFLT_DEBUG   0

struct server_controls {
	int test;
	int rsktlib_mp;
	int rsktlib_portno;
	int remcli_portno;
	int print_help;
};

struct rskt_server_info {
	int all_must_die;
	int debug;
	struct server_controls ctrls;
	struct console_globals cli;
	struct library_globals lib;
};

struct rskt_server_info rskts;

void print_server_help(void)
{
	printf("rskt_server is a test application which loops back messages\n");
	printf("sent by the rskt_client. Parameters documented below:\n");
	printf("-h, -H, ?  : Print this help message and exit.\n");
        printf("-d       : Disables display of debug messages. This is the \n");
        printf("           default operating mode for the server.\n");
        printf("-D       : Displays debug/trace/error messages\n");
        printf("-t       : Run the library in test mode - no connections\n");
	printf("-u <u_num>: AF_LOCAL <u_skt> socket for rskt library calls\n");
        printf("           The default value is %d.\n", RSKT_DFLT_APP_PORT_NUM);
        printf("           The u_skt and Umport value must be correct for\n");
        printf("           the rskt library to operate correctly.\n");
	printf("-m <Umpnum>: Local mport associatedi wht the u_skt.\n");
	printf("                Default is %d\n", RSKT_SVR_DFLT_MP_NUM);
	printf("                Maximum is %d\n", RSKT_SVR_MAX_MP_NUM);
        printf("           The u_skt and Umport value must be correct for\n");
        printf("           the rskt library to operate correctly.\n");
	printf("-e <e_skt>  : Remote console connectivity over Ethernet"
                                			" uses <e_skt>.\n");
        printf("         The default <e_skt> value is %d.\n",
                                                        RSKTS_DFLT_CLI_SKT);
};

int parse_options(int argc, char *argv[], struct server_controls *ctrls)
{
	int idx;
	uint32_t tmp32;
	uint16_t tmp16;

	ctrls->test = 0;
	ctrls->rsktlib_mp = DFLT_DMN_LSKT_MPORT;
	ctrls->rsktlib_portno = RSKT_DFLT_APP_PORT_NUM;
	ctrls->remcli_portno = RSKTS_DFLT_CLI_SKT;
	rskts.debug = 1;
	ctrls->print_help = 0;

        for (idx = 0; (idx < argc) && !ctrls->print_help; idx++) {
                if (strnlen(argv[idx], 4) < 2)
                        continue;

                if ('-' == argv[idx][0]) {
                        switch(argv[idx][1]) {
                        case '?':
                        case 'h':
                        case 'H':
                        	ctrls->print_help = 1;
				break;
			case 'd':
				rskts.debug = 0;
				break;
			case 't':
				ctrls->test = 1;
				break;
			case 'D':
				rskts.debug = 1;
				break;
			case 'm':
				idx++;
				if (argc < idx) {
					printf("\n<mport> not specified\n");
					ctrls->print_help = 1;
					goto exit;
				}
				if (tok_parse_mport_id(argv[idx], &tmp32, 0)) {
					printf("\n");
					printf(TOK_ERR_MPORT_MSG_FMT);
					ctrls->print_help = 1;
					goto exit;
				}
				ctrls->rsktlib_mp = (int)tmp32;
				break;
			case 'u':
				idx++;
				if (argc < idx) {
					printf("\n<u_skt> not specified\n");
					ctrls->print_help = 1;
					goto exit;
				}
				if (tok_parse_socket(argv[idx], &tmp16, 0)) {
					printf("\n");
					printf(TOK_ERR_SOCKET_MSG_FMT, "<u_skt>");
					ctrls->print_help = 1;
					goto exit;
				}
				ctrls->rsktlib_portno = (int)tmp16;
				break;
			case 'e':
				idx++;
				if (argc < idx) {
					printf("\n<e_skt> not specified\n");
					ctrls->print_help = 1;
					goto exit;
				}
				if (tok_parse_socket(argv[idx], &tmp16, 0)) {
					printf("\n");
					printf(TOK_ERR_SOCKET_MSG_FMT, "<e_skt>");
					ctrls->print_help = 1;
					goto exit;
				}
				ctrls->remcli_portno = (int)tmp16;
				break;
                        default:
                        	printf("\nUnknown parm: \"%s\"\n", argv[idx]);
                                ctrls->print_help = 1;
                        };
                };
        }
exit:

	return 0;
}

void set_prompt(struct cli_env *e)
{
        if (e != NULL) {
                SAFE_STRNCPY(e->prompt, "RSKTSvr> ", sizeof(e->prompt));
        };
};

void rskt_server_shutdown(void);

int RSKTShutdownCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{
	rskt_server_shutdown();
	LOGMSG(env, "Shutdown initiated...\n");

	return 0;
};

struct cli_cmd RSKTShutdown = {
"shutdown",
8,
0,
"Shutdown server.",
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
	if (number_of_mports > RSKT_SVR_MAX_MP_NUM) {
		LOGMSG(env, "WARNING: Only %d out of %d have been retrieved\n",
				RSKT_SVR_MAX_MP_NUM, number_of_mports);
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
	};

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

void print_skt_conn_status(struct cli_env *env)
{
	LOGMSG(env, "\nStatus unimplemented.\n");
};
	
extern struct cli_cmd RSKTSStatus;

int RSKTSStatusCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;

	LOGMSG(env, "        Alive Socket # BkLg MP Pse Max Reqs\n");
	LOGMSG(env, "Lib Lp %5d %8d %4d %2d %3d %3d %3d\n",
			rskts.lib.loop_alive, rskts.lib.portno, rskts.lib.bklg,
			rskts.lib.mpnum, rskts.lib.pause_reqs,
			rskts.lib.max_reqs, rskts.lib.num_reqs);

	LOGMSG(env, "Cli Lp %5d %8d\n", rskts.cli.cli_alive,
			rskts.cli.cli_portno);

	LOGMSG(env, "Cons   %5d\n\n", rskts.cli.cons_alive);

	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTSStatus);

	return 0;
};

struct cli_cmd RSKTSStatus = {
"status",
2,
0,
"Status command.",
"No parameters\n"
        "Dumps the status of the CLI and remote CLI threads.\n",
RSKTSStatusCmd,
ATTR_RPT
};

struct cli_cmd *server_cmds[] = 
	{ &RSKTShutdown,
	  &RSKTMpdevs
	};

int all_must_die;

void bind_server_cmds(void)
{
	all_must_die = 0;
        add_commands_to_cmd_db(sizeof(server_cmds)/sizeof(server_cmds[0]), 
				server_cmds);
	librskt_bind_cli_cmds();
	liblog_bind_cli_cmds();

        return;
};

void rskts_console_cleanup(struct cli_env *env);

void *console(void *cons_parm)
{
	struct cli_env cons_env;
	int rc;

	init_cli_env(&cons_env);
	set_prompt( &cons_env );

	cli_init_base(rskts_console_cleanup);
	bind_server_cmds();

	splashScreen(&cons_env, (char *)"RSKT_Server");

	rskts.cli.cons_alive = 1;
	
	rc = cli_terminal(&cons_env);

	rskt_server_shutdown();
	if (NULL == cons_parm) {
		cons_parm = malloc(sizeof(int));
	}

	if (NULL != cons_parm) {
		*(int *)(cons_parm) = rc;
	}
	printf("\nConsole EXITING\n");
	rskts.cli.cons_alive = 0;
	pthread_exit(cons_parm);
};

void spawn_threads()
{
        int  cli_ret, console_ret = 0, rc;
        int *pass_console_ret;
	struct remote_login_parms *rlp = (struct remote_login_parms *)
			malloc(sizeof(struct remote_login_parms));

        rskts.all_must_die = 0;
        sem_init(&rskts.cli.cons_owner, 0, 0);

        rskts.cli.cli_portno = 0;
        rskts.cli.cons_alive = 0;

        /* Prepare and start console thread */
	pass_console_ret = (int *)(malloc(sizeof(int)));
	if (NULL == pass_console_ret) {
		fprintf(stderr, "Error - console_thread creation\n");
		exit(EXIT_FAILURE);
	}
	*pass_console_ret = 0;
	console_ret = pthread_create( &rskts.cli.cons_thread, NULL,
			console, (void *)(pass_console_ret));
	if(console_ret) {
		printf("\nError console_thread rc: %d\n", console_ret);
		exit(EXIT_FAILURE);
	};

        /* Start remote_login_thread, enabling remote debug over Ethernet */
	rlp->portno = rskts.ctrls.remcli_portno;
	SAFE_STRNCPY(rlp->thr_name, "RSKTS_RCLI", sizeof(rlp->thr_name));
	rlp->status = &rskts.cli.cli_alive;

        cli_ret = pthread_create( &remote_login_thread, NULL, remote_login,
                                (void *)(rlp));
        if(cli_ret) {
                fprintf(stderr, "Error - remote_login_thread rc: %d\n",cli_ret);
                exit(EXIT_FAILURE);
        }
        librskt_test_init(rskts.ctrls.test);
        rc = librskt_init(rskts.ctrls.rsktlib_portno, rskts.ctrls.rsktlib_mp);
	if (rc) {
        	fprintf(stderr, "Error - libskt_init rc = %d, errno = %d:%s\n",
                        rc, errno, strerror(errno));
                exit(EXIT_FAILURE);
	};
}
 
void rskt_server_shutdown(void)
{
	all_must_die = 1;
	
        if (rskts.lib.fd > 0) {
                close(rskts.lib.fd);
                rskts.lib.fd = 0;
        };

        if (rskts.lib.addr.sun_path[0]) {
                if (-1 == unlink(rskts.lib.addr.sun_path))
                        perror("ERROR on l_conn unlink");
                rskts.lib.addr.sun_path[0] = 0;
        };
};

void rskts_console_cleanup(struct cli_env *env)
{
	(void)env;
	rskt_server_shutdown();
};

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	CRIT("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		printf("Shutting down\n");
		rskt_server_shutdown();
        	exit(0);
	};
	return;
};

int main(int argc, char *argv[])
{
	if (parse_options(argc, argv, &rskts.ctrls))
		goto exit;

	if (rskts.debug) {
		printf("rskts.ctrls.rsktlib_portno : %d\n", rskts.ctrls.rsktlib_portno);
		printf("rskts.ctrls.rsktlib_mp: %d\n", rskts.ctrls.rsktlib_mp);
		printf("rskts.ctrls.remcli_portno : %d\n", rskts.ctrls.remcli_portno);
		printf("rskts.debug : %d\n", rskts.debug);
	};

	if (rskts.ctrls.print_help) {
		print_server_help();
		goto exit;
	};

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGPIPE, sig_handler);

	spawn_threads();

	pthread_join(rskts.cli.cons_thread, NULL);
 
	printf("\nRDMA SOCKET SERVER SERVER EXITING!!!!\n");
exit:
	exit(EXIT_SUCCESS);
}

#ifdef __cplusplus
}
#endif
