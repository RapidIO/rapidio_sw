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

#include "riodp_mport_lib.h"
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"
#include "libcli.h"
#include "librdma.h"
#include "librsktd.h"
#include "librsktd_private.h"
#include "liblog.h"

#define DFLT_DMN_E_CLI_SKT 3333

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
							DFLT_DMN_E_CLI_SKT);
	printf("	 Note: There must be a space between \"-c\""
							" and <e_skt>.\n");
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
	printf("	 The default value is %d.\n", DFLT_DMN_LSKT_SKT);
	printf("	 The u_skt and Umport value must be correct for\n");
	printf("	 the rskt library to operate correctly.\n");
	printf("	 Note: There must be a space between -u and"
							" <u_skt>.\n");
	printf("-m<Umport>: The local mport associated with the u_skt.\n");
	printf("	 The default value is %d.\n", DFLT_DMN_LSKT_MPORT);
	printf("	 The u_skt and Umport value must be correct for\n");
	printf("	 the rskt library to operate correctly.\n");
	printf("-L <bklg>: Maximum connect request backlog for uskt.\n");
	printf("	 The default value is %d.\n", DFLT_DMN_LSKT_BACKLOG);
	printf("	 The u_skt and Umport value must be correct for\n");
	printf("	 the rskt library to operate correctly.\n");
	printf("-C <cm_skt>: The RSKT daemon listens for connect requests\n");
	printf("	 on RapidIO Channel Manager socket <cm_skt>.\n");
	printf("	 The default value is %d.\n", DFLT_DMN_CM_CONN_SKT);
	printf("	 The cm_skt and mport value must be correct to\n");
	printf("	 successfully connect.\n");
	printf("	 Note: There must be a space between -X and"
							" <cm_skt>.\n");
	printf("-M<Cmport>: The rskt daemon listens for remote connect requests\n");
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

	ctrls.debug = 0;
	ctrls.print_help = 0;
	ctrls.run_cons = 1;
	ctrls.e_cli_skt = DFLT_DMN_E_CLI_SKT;
	ctrls.num_ms = DFLT_DMN_NUM_MS;
	ctrls.ms_size = DFLT_DMN_MS_SIZE;
	ctrls.rskt_buff_size = DFLT_DMN_SBUF_SIZE;
	ctrls.init_ms = DFLT_DMN_INIT;
	ctrls.rsktd_uskt_tst = 0;
	ctrls.rsktd_uskt = DFLT_DMN_LSKT_SKT;
	ctrls.rsktd_u_mp = DFLT_DMN_LSKT_MPORT;
	ctrls.rsktd_u_bklg = DFLT_DMN_LSKT_BACKLOG;
	ctrls.rsktd_cskt_tst = 0;
	ctrls.rsktd_cskt = DFLT_DMN_CM_CONN_SKT;
	ctrls.rsktd_c_mp = DFLT_DMN_CM_CONN_MPORT;
	ctrls.num_peers = 0;

	for (idx = 0; (idx < argc) && !ctrls.print_help; idx++) {
		if (strnlen(argv[idx], 4) < 2)
			continue;

		if ('-' == argv[idx][0]) {
			switch(argv[idx][1]) {
			case '?': 
			case 'h': 
			case 'H': ctrls.print_help = 1;
				  break;
			case 'd': ctrls.debug = 0;
				break;
			case 'D': ctrls.debug = 1;
				break;
			case 't': ctrls.rsktd_uskt_tst = 1;
				break;
			case 'T': ctrls.rsktd_cskt_tst = 1;
				break;
			case 'N': ctrls.init_ms = 0;
				break;
			case 'B': ctrls.run_cons = 0;
				break;
			case 'e': if (argc < (idx+1)) {
					  printf("\n<e_skt> not specified\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				idx++;
				ctrls.e_cli_skt = atoi(argv[idx]);
				if (!ctrls.e_cli_skt) {
					  printf("\n<e_skt> must not be 0\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				break;
			case 's': if (argc < (idx+1)) {
					  printf("\n<num_spaces> not specified\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				idx++;
				ctrls.num_ms = atoi(argv[idx]);
				if (ctrls.num_ms > MAX_DMN_NUM_MS) {
					  printf("\n<num_spaces> max is %d\n",
						MAX_DMN_NUM_MS);
					  ctrls.print_help = 1;
					  goto exit;
				};
				break;
			case 'S': if (argc < (idx+1)) {
					  printf("\n<size> not specified\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				idx++;
				ctrls.ms_size = atoi(argv[idx]);
				if ((128 != ctrls.ms_size) &&
					(256 != ctrls.ms_size) &&
					(512 != ctrls.ms_size) &&
					(1024 != ctrls.ms_size) &&
					(2048 != ctrls.ms_size)) {
					printf("\nIllegal ms_size\n");
					ctrls.print_help = 1;
					goto exit;
				};
				break;
			case 'k': if (argc < (idx+1)) {
					printf("\n<size> not specified\n");
					ctrls.print_help = 1;
					goto exit;
				};
				idx++;
				ctrls.rskt_buff_size = atoi(argv[idx]);
				if ((2 != ctrls.rskt_buff_size) &&
					(4 != ctrls.rskt_buff_size) &&
					(8 != ctrls.rskt_buff_size) &&
					(16 != ctrls.rskt_buff_size) &&
					(32 != ctrls.rskt_buff_size) &&
					(64 != ctrls.rskt_buff_size) &&
					(128 != ctrls.rskt_buff_size)) {
					printf("\nBad socket buffer size\n");
					ctrls.print_help = 1;
					goto exit;
				};
				break;
			case 'u': if (argc < (idx+1)) {
					  printf("\n<u_skt> not specified\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				idx++;
				ctrls.rsktd_uskt = atoi(argv[idx]);
				if (!ctrls.rsktd_uskt) {
					  printf("\n<u_skt> must not be 0\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				break;
			case 'm': 
				if ((argv[idx][2] >= '0') && 
				    (argv[idx][2] <= ('0'+MAX_DMN_MPORT))) {
					ctrls.rsktd_u_mp = argv[idx][2] - '0';
					break;
				};
				printf("\n<mport> invalid\n");
				ctrls.print_help = 1;
				goto exit;
			case 'L': if (argc < (idx+1)) {
					  printf("\n<bklg> not specified\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				idx++;
				ctrls.rsktd_u_bklg = atoi(argv[idx]);
				if (!ctrls.rsktd_u_bklg) {
					  printf("\n<u_bklg> must not be 0\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				break;
			case 'C': if (argc < (idx+1)) {
					  printf("\n<cm_skt> not specified\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				idx++;
				ctrls.rsktd_cskt = atoi(argv[idx]);
				if (!ctrls.rsktd_cskt) {
					  printf("\n<cm_skt> must not be 0\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				break;
			case 'M': if ((argv[idx][2] >= '0') && 
				    (argv[idx][2] <= ('0'+MAX_DMN_MPORT))) {
					ctrls.rsktd_c_mp = argv[idx][2] - '0';
					break;
				};
				printf("\n<Cmport> invalid\n");
				ctrls.print_help = 1;
				goto exit;
				break;
			case 'P': if (argc < (idx+3)) {
					  printf("\nMissing peer parms\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				ctrls.peers[ctrls.num_peers].ct 
					= atoi(argv[idx+1]);
				ctrls.peers[ctrls.num_peers].cm_skt 
					= atoi(argv[idx+2]);
				idx+=2;
				
				if (!ctrls.peers[ctrls.num_peers].cm_skt) {
					  printf("\nIllegal peer parms\n");
					  ctrls.print_help = 1;
					  goto exit;
				};
				ctrls.num_peers++;
				break;
			default: printf("\nUnknown parm: \"%s\"\n", argv[idx]);
				ctrls.print_help = 1;
			};
		};
	}

	ctrls.ms_size = ctrls.ms_size * 1024;
	ctrls.rskt_buff_size = ctrls.rskt_buff_size * 1024;
exit:
	return;
}

void set_prompt(struct cli_env *e)
{
        if (e != NULL) {
                strncpy(e->prompt, "RSKTDaemon> ", PROMPTLEN);
        };
};

struct console_globals {
	int all_must_die;
        /* Globals for console run by RSKT Daemon */
        pthread_t cons_thread;
        sem_t cons_owner;
        int cons_alive;

        /* Globals for remote CLI sessions */
        int cli_alive;
        pthread_t cli_thread;
        int cli_portno;
        int cli_sess_num;

        int cli_fd;
        struct sockaddr_in cli_addr;
        int cli_sess_fd;
        struct sockaddr_in sess_addr;
        socklen_t sess_addr_len;
};

struct console_globals cli;

void rskt_daemon_shutdown(void);

void quit_command_customization(struct cli_env *env)
{
	rskt_daemon_shutdown();
};
	
void *console(void *cons_parm)
{
	struct cli_env cons_env;
	int rc;
	int *prc;

	cons_env.script = NULL;
	cons_env.fout = NULL;
	bzero(cons_env.output, BUFLEN);
	bzero(cons_env.input, BUFLEN);
	cons_env.DebugLevel = 0;
	cons_env.progressState = 0;
	cons_env.sess_socket = -1;
	cons_env.h = NULL;
	cons_env.cmd_prev = NULL;
	bzero(cons_env.prompt, PROMPTLEN+1);
	set_prompt( &cons_env );

	cli_init_base(quit_command_customization);
	librsktd_bind_cli_cmds();
	liblog_bind_cli_cmds();

	splashScreen((char *)"RDMA Socket Daemon Console");

	cli.cons_alive = 1;
	
	rc = cli_terminal(&cons_env);

	rskt_daemon_shutdown();

	printf("\nConsole EXITING\n");
	cli.cons_alive = 0;

	/* For return code to be checked in pthread_join() */
	prc = (int *)malloc(sizeof(int));
	if (prc)
		*prc = rc;
	pthread_exit(prc);
}; /* console() */

void *cli_session( void *rc_ptr )
{
	char buffer[256];
	int one = 1;
	int *prc;

	cli.cli_portno = ctrls.e_cli_skt;

	cli.cli_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (cli.cli_fd < 0) {
        	perror("RSKTD Remote CLI ERROR opening socket");
		goto fail;
	}
	bzero((char *) &cli.cli_addr, sizeof(cli.cli_addr));
	cli.cli_addr.sin_family = AF_INET;
	cli.cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	cli.cli_addr.sin_port = htons(cli.cli_portno);
	setsockopt (cli.cli_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
	setsockopt (cli.cli_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));
	if (bind(cli.cli_fd, (struct sockaddr *) &cli.cli_addr, 
						sizeof(cli.cli_addr)) < 0) {
        	perror("\nRSKTD Remote CLI ERROR on binding");
		goto fail;
	}

	if (ctrls.debug) {
		printf("\nRSKTD Remote CLI bound to socket %d\n", 
		cli.cli_portno);
	};
	sem_post(&cli.cons_owner);
	cli.cli_alive = 1;
	while (!cli.all_must_die && strncmp(buffer, "done", 4)) {
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

		listen(cli.cli_fd,5);
		cli.sess_addr_len = sizeof(cli.sess_addr);
		env.sess_socket = -1;
		cli.cli_sess_fd = accept(cli.cli_fd, 
				(struct sockaddr *) &cli.sess_addr, 
				&cli.sess_addr_len);
		if (cli.cli_sess_fd < 0) {
			if (cli.cli_fd > 0)
				perror("ERROR on accept");
			goto fail;
		};
		env.sess_socket = cli.cli_sess_fd;
		printf("\nRSKTD Starting session %d\n", cli.cli_sess_num);
		cli_terminal( &env );
		printf("\nRSKTD Finishing session %d\n", cli.cli_sess_num);
		if (cli.cli_sess_fd > 0)
			close(cli.cli_sess_fd);
		cli.cli_sess_fd = -1;
		cli.cli_sess_num++;
	};
fail:
	cli.cli_alive = 0;
	if (ctrls.debug)
		printf("\nRSKTD REMOTE CLI Thread Exiting\n");

	if (cli.cli_sess_fd > 0) {
		close(cli.cli_sess_fd);
		cli.cli_sess_fd = -1;
	};
     	if (cli.cli_fd > 0) {
     		close(cli.cli_fd);
		cli.cli_fd = 0;
	};

	/* For return code to be checked in pthread_join() */
	prc = (int *)malloc(sizeof(int));
	if (prc)
		*prc = cli.cli_portno;
	pthread_exit(prc);
} /* cli_session() */

void spawn_threads(void)
{
	int  cli_ret = 0, console_ret = 0;

	sem_init(&cli.cons_owner, 0, 0);

	cli.cli_portno = 0;
	cli.cli_alive = 0;
	cli.cons_alive = 0;

	/* Prepare and start console thread */
	if (ctrls.run_cons) {
		console_ret = pthread_create( &cli.cons_thread, NULL, 
						console, NULL);
		if(console_ret) {
			CRIT("Failed to create console_thread: %s\n", strerror(console_ret));
			exit(EXIT_FAILURE);
		}
	};
	INFO("Console thread started\n");

	/* Start cli_session_thread, enabling remote debug over Ethernet */

	cli_ret = pthread_create( &cli.cli_thread, NULL, cli_session, 
				NULL);
	if(cli_ret) {
		CRIT("Failed to create cli_thread: %s\n", strerror(cli_ret));
		exit(EXIT_FAILURE);
	}
	INFO("CLI thread started\n");

	spawn_daemon_threads(&ctrls);
}

int init_srio_api(uint8_t mport_id);

void rskt_daemon_shutdown(void) {
	kill_daemon_threads();

	if (!cli.all_must_die && cli.cli_alive)
		pthread_kill(cli.cli_thread, SIGHUP);
	cli.all_must_die = 1;

	if (cli.cli_sess_fd > 0) {
		close(cli.cli_sess_fd);
		cli.cli_sess_fd = 0;
	};
     	if (cli.cli_fd > 0) {
     		close(cli.cli_fd);
		cli.cli_fd = 0;
	};
};

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		printf("Shutting down\n");
		rskt_daemon_shutdown();
	};
};

int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;
	int *prc;

	ctrls.debug = 0;

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	cli.all_must_die = 0;
	cli.cli_alive = 0;

	parse_options(argc, argv);
	
	if (ctrls.print_help) {
		print_daemon_help();
		goto exit;
	};

	DBG("Spawning rsktd threads\n");
	spawn_threads();

	if (daemon_threads_failed() && ctrls.init_ms) {
		CRIT("Failed to create daemon threads\n");
		rskt_daemon_shutdown();
	}

	/* If console thread is running, wait for it to terminate */
	if (ctrls.run_cons && cli.cons_alive) {
		pthread_join(cli.cons_thread, (void **)&prc);
		DBG("console thread exit with rc = %d\n", *prc);
		if (prc)
			free(prc);
	}
 
	/* Ditto for CLI thread */
	if (cli.cli_alive) {
		pthread_join(cli.cli_thread, (void **)&prc);
		DBG("CLI thread exit with rc = %d\n", *prc);
		if (prc)
			free(prc);
	}

	printf("\nRDMA Socket Server EXITING!!!!\n");
	rc = EXIT_SUCCESS;
exit:
	exit(rc);
}

#ifdef __cplusplus
}
#endif

