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
#include <fcntl.h>
#include <signal.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include "compile_constants.h"
#include "dev_db_sm.h"
#include "DAR_DevDriver.h"
#include "DAR_RegDefs.h"

#include "IDT_DSF_DB_Private.h"
#include "DAR_Utilities.h"

#include "cli_cmd_line.h"
#include "cli_base_init.h"
#include "riocp_pe.h"
#include "riocp_pe_internal.h"
#include "librio_maint.h"
#include "DAR_DevDriver.h"
#include "comptag.h"
#include "librio_fmd_internal.h"
#include "dev_db.h"

riocp_pe_handle mport_pe;
rio_maint_handle reg_acc_h;
DAR_DEV_INFO_t *dev_h;

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		exit(EXIT_SUCCESS);
		kill(getpid(), SIGKILL);
	};
};

void print_help(void)
{
	printf("\nrio_demon manages a RapidIO subsystem.  Options are:\n");
	printf("-c, -C: rio_daemon has a console CLI.\n");
	printf("-d, -D: Discover a previously enumerated RapidIO network.\n");
	printf("-e, -E: Enumerate and initialize a RapidIO network.\n");
	printf("-iX, -IX: Polling interval, in seconds.\n");
	printf("-Mp   : Connect to the RapidIO network using port p.\n");
	printf("-p, -P: POSIX Sockets port number for remote connection.\n");
	printf("-x, -X: Initialize and then immediately exit.\n");
	printf("-h, -H, -?: Print this message.\n");
};

pthread_t poll_thread, cli_session_thread, console_thread;

sem_t cons_owner;

void set_prompt(struct cli_env *e)
{
        riocp_pe_handle pe_h;
        uint32_t comptag = 0;
        const char *name = NULL;
        uint16_t pe_uid = 0;

        if (NULL == e) {
                strncpy(e->prompt, "UNINIT>", PROMPTLEN);
                return;
        };

	pe_h = (riocp_pe_handle)(e->h);

	if (riocp_pe_get_comptag(pe_h, &comptag))
		comptag = 0xFFFFFFFF;
	pe_uid = RIOCP_PE_COMPTAG_GET_NR(comptag);

	name = riocp_pe_handle_get_device_str(pe_h);

	snprintf(e->prompt, PROMPTLEN,  "%4x_%s>", pe_uid, name);
};

void *console(void *cons_parm)
{
	struct cli_env cons_env;

	cons_env.script = NULL;
	cons_env.fout = NULL;
	bzero(cons_env.output, BUFLEN);
	bzero(cons_env.input, BUFLEN);
	cons_env.DebugLevel = 0;
	cons_env.progressState = 0;
	cons_env.sess_socket = -1;
	cons_env.h = mport_pe;
	bzero(cons_env.prompt, PROMPTLEN+1);
	set_prompt( &cons_env );

	sem_wait(&cons_owner);
	sem_wait(&cons_owner);

	splashScreen(&cons_env);
	*(int *)(cons_parm) = cli_terminal(&cons_env);

	exit(EXIT_SUCCESS);

	return cons_parm;
};

void *poll_loop( void *poll_interval ) 
{
	int wait_time = ((int *)(poll_interval))[0];
	int console = ((int*)(poll_interval))[1];
	free(poll_interval);

	printf("RIO_DEMON: Poll interval %d seconds\n", wait_time);
	sem_post(&cons_owner);
	while(TRUE) {
		sm_db_incr_chg_idx();
		sleep(wait_time);
		if (!console)
			printf("\nTick!");
	}
	return poll_interval;
}

void *cli_session( void *sock_num )
{
     int sockfd, newsockfd = -1, portno;
     socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     int one = 1;
	int session_num = 0;

     portno = *(int *)(sock_num);
	free(sock_num);

     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     	if (sockfd < 0) {
        	perror("ERROR opening socket");
		goto fail;
	}
     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
     serv_addr.sin_port = htons(portno);
     setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
     setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        	perror("ERROR on binding");
		goto fail;
	}

	printf("\nRIO_DEMON bound to socket %d\n", portno);
	sem_post(&cons_owner);
	while (strncmp(buffer, "done", 4)) {
		struct cli_env env;

		env.script = NULL;
		env.fout = NULL;
		bzero(env.output, BUFLEN);
		bzero(env.input, BUFLEN);
		env.DebugLevel = 0;
		env.progressState = 0;
		env.sess_socket = -1;
		env.h = mport_pe;
		bzero(env.prompt, PROMPTLEN+1);
		set_prompt( &env );

		listen(sockfd,5);
		clilen = sizeof(cli_addr);
		env.sess_socket = accept(sockfd, 
				(struct sockaddr *) &cli_addr, 
				&clilen);
		if (env.sess_socket < 0) {
			perror("ERROR on accept");
			goto fail;
		};
		printf("\nStarting session %d\n", session_num);
		cli_terminal( &env );
		printf("\nFinishing session %d\n", session_num);
		close(env.sess_socket);
		session_num++;
	};
fail:
	printf("\nRIO_DEMON CLI Thread Exiting\n");
	if (newsockfd >=0)
		close(newsockfd);
     	close(sockfd);
	pthread_exit( (void *)(&n) );
}

void spawn_threads(struct fmd_cfg_parms *cfg)
//int poll_interval, int sock_num, int run_cons)
{
	int  poll_ret, cli_ret, cons_ret;
	int *pass_sock_num, *pass_poll_interval, *pass_cons_ret;

	sem_init(&cons_owner, 0, 0);
	pass_sock_num = (int *)(malloc(sizeof(int)));
	pass_poll_interval = (int *)(malloc(2*sizeof(int)));
	pass_cons_ret = (int *)(malloc(sizeof(int)));
	*pass_sock_num = cfg->cli_port_num;
	pass_poll_interval[0] = cfg->mast_interval;
	pass_poll_interval[1] = cfg->run_cons;
	*pass_cons_ret = cfg->run_cons;

	/* Create independent threads each of which will execute function */
	poll_ret = pthread_create( &poll_thread, NULL, poll_loop, 
				(void*)(pass_poll_interval));
	if(poll_ret) {
		fprintf(stderr,"Error - poll_thread rc: %d\n",poll_ret);
		exit(EXIT_FAILURE);
	}
 
	cli_ret = pthread_create( &cli_session_thread, NULL, 
		cli_session, (void*)(pass_sock_num));
	if(cli_ret) {
		fprintf(stderr,"Error - cli_session_thread rc: %d\n",cli_ret);
		exit(EXIT_FAILURE);
	}

	if (cfg->run_cons) {
		cons_ret = pthread_create( &console_thread, NULL, 
				console, (void*)(pass_cons_ret));
		if(cons_ret) {
			fprintf(stderr,"Error - cons_thread rc: %d\n",cli_ret);
			exit(EXIT_FAILURE);
		}
	};
	printf("pthread_create() for poll_loop returns: %d\n",poll_ret);
	printf("pthread_create() for cli_session_thread returns: %d\n",cli_ret);
	if (cfg->run_cons) 
		printf("pthread_create() for console returns: %d\n",
			cons_ret);
 
}
 
/*
void init_librio_maint(void)
{
	riocp_pe_mport_handles.data = NULL;
	riocp_pe_mport_handles.next = NULL;
	riocp_pe_mport_handles.prev = NULL;

	_riocp_pe_mport_head.data = NULL;
	_riocp_pe_mport_head.next = NULL;
	_riocp_pe_mport_head.prev = NULL;
}
*/

void setup_mport(uint8_t mport_num)
{
	dev_h = (DAR_DEV_INFO_t *)(malloc(sizeof(DAR_DEV_INFO_t)));

	if (riocp_pe_create_host_handle(&mport_pe, mport_num, 0)) {
		printf("\nCannot create host handle, exiting...\n");
		exit(EXIT_FAILURE);
	};

	if (!(RIOCP_PE_IS_MPORT(mport_pe))) {
		printf("\nHost port is not an MPORT, wazzup?...\n");
		exit(EXIT_FAILURE);
	};
	
	dev_h->privateData = (void *)(mport_pe);
	if (riocp_pe_handle_set_private(mport_pe, (void *)dev_h))
		printf("\nFailed setting private data for mport...\n");

	if (RIO_SUCCESS != DAR_Find_Driver_for_Device( FALSE, dev_h )) {
		printf("\nNo Driver for MPORT, wazzup?...\n");
		exit(EXIT_FAILURE);
	};
}

int init_srio_api(uint8_t mport_id);

int main(int argc, char *argv[])
{
	struct fmd_cfg_parms *cfg;
	struct fmd_state *fmd;
	int mport;

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	cfg = fmd_parse_options(argc, argv);
	fmd_process_cfg_file(cfg);
	
	if ((NULL == cfg) || (cfg->init_err))
		goto fail;

	fmd_dd_init(cfg, &fmd);
	if ((NULL == fmd) || (cfg->init_err))
		goto fail;


	if (FMD_SLAVE == cfg->mast_idx)
		mport = cfg->mport_info[0].num;
	else
		mport = cfg->mport_info[cfg->mast_idx].num;
	init_srio_api(mport);
	bind_dd_cmds(fmd);
	setup_mport(mport);

	if (!cfg->init_and_quit) {
		spawn_threads(cfg);

		pthread_join(poll_thread, NULL);
		pthread_join(cli_session_thread, NULL);
		if (cfg->run_cons)
			pthread_join(console_thread, NULL);
	};
 
fail:
	exit(EXIT_SUCCESS);
}

STATUS SRIO_API_ReadRegFunc(DAR_DEV_INFO_t *d_info,
				UINT32 offset, UINT32 *readdata)
{
	STATUS rc = RIO_ERR_INVALID_PARAMETER;
	UINT32 x;
        riocp_pe_handle pe_h;

	if ((d_info == NULL) || (offset >= 0x01000000))
		goto exit;

	pe_h = (riocp_pe_handle)(d_info->privateData);


	if (RIOCP_PE_IS_MPORT(pe_h))
		rc = rio_maint_read_local(reg_acc_h, offset,  &x)?
						RIO_ERR_ACCESS:RIO_SUCCESS;
	else
		rc = rio_maint_read_remote(reg_acc_h, pe_h->destid, 
			pe_h->hopcount, offset, &x, 1)?
						RIO_ERR_ACCESS:RIO_SUCCESS;
	if (RIO_SUCCESS == rc)
		*readdata = x;
exit:
	return rc;
};

STATUS SRIO_API_WriteRegFunc(DAR_DEV_INFO_t *d_info,
				UINT32  offset,
				UINT32  writedata)
{
	STATUS rc = RIO_ERR_INVALID_PARAMETER;
	riocp_pe_handle pe_h;
	

	if ((d_info == NULL) || (offset >= 0x01000000))
		goto exit;

	pe_h = (riocp_pe_handle)(d_info->privateData);


	if (RIOCP_PE_IS_MPORT(pe_h))
		rc = rio_maint_write_local(reg_acc_h, offset,  writedata)?
						RIO_ERR_ACCESS:RIO_SUCCESS;
	else
		rc = rio_maint_write_remote(reg_acc_h, pe_h->destid, 
			pe_h->hopcount, offset, &writedata, 4)?
						RIO_ERR_ACCESS:RIO_SUCCESS;
exit:
	return rc;
};

void SRIO_API_DelayFunc(UINT32 delay_nsec, UINT32 delay_sec)
{
	return;
};

int init_srio_api(uint8_t mport_num)
{
	cli_init_base();
	if (rio_maint_init( mport_num, &reg_acc_h)) {
		printf("\nCannot open mport %d, exiting...\n", mport_num);
		exit(EXIT_FAILURE);
	};

	IDT_DSF_bind_DAR_routines(SRIO_API_ReadRegFunc,
				  SRIO_API_WriteRegFunc,
				  SRIO_API_DelayFunc);

	return 0;
};
