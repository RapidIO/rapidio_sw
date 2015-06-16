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
#include "liblist.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

riocp_pe_handle mport_pe;
rio_maint_handle reg_acc_h;
DAR_DEV_INFO_t *dev_h;

struct fmd_cfg_parms *cfg;
struct fmd_state *fmd;

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		exit(EXIT_SUCCESS);
		kill(getpid(), SIGKILL);
	};
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

	fmd_dd_cleanup(fmd);
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
 
int fmd_traverse_network(riocp_pe_handle mport_pe, int port_num, int enumerate)
{
	struct l_head_t sw;
	riocp_pe_handle new_pe, curr_pe;
	int port_cnt, rc, cur_port;
	struct riocp_pe_capabilities capabilities;
	uint32_t comptag, new_comptag;

	l_init(&sw);

	curr_pe = mport_pe;

	while (NULL != curr_pe) {
		rc = riocp_pe_get_comptag(curr_pe, &comptag);
		if (rc) {
			CRIT("Get comptag failed, rc %d\n", rc);
			goto exit;
		};
		INFO("Checking CompTag %x\n", comptag);

		rc = riocp_pe_get_capabilities(curr_pe, &capabilities);
		if (rc) {
			CRIT("Get capabilities failed, rc %d\n", rc);
			goto exit;
		};

		port_cnt = RIOCP_PE_PORT_COUNT(capabilities);
		if (18 < port_cnt) {
			CRIT("Illegal port count of %d\n", port_cnt);
			goto exit;
		};

		for (cur_port = 0; cur_port < port_cnt; cur_port++) {
			new_pe = NULL;
			if (enumerate)
				rc = riocp_pe_probe(curr_pe, cur_port, &new_pe);
			else
				rc = riocp_pe_discover(curr_pe, cur_port,
								&new_pe);

			if (rc) {
				if ((-ENODEV != rc) && (-EIO != rc)) {
					CRIT(
					"Port %d probe or discover failed %d\n",
						cur_port, rc);
					goto exit;
				};
				INFO("Comp tag %x Port %d NO DEVICE\n",
					comptag, cur_port);
				continue;
			};

			if (NULL == new_pe)
				continue;

			rc = riocp_pe_get_comptag(new_pe, &new_comptag);
			if (rc) {
				CRIT("Get new comptag failed, rc %d\n", rc);
				goto exit;
			};
			INFO("Comp tag %x Port %d Found %x \n", 
				comptag, cur_port, new_comptag);

			rc = riocp_pe_get_capabilities(new_pe, &capabilities);
			if (rc) {
				CRIT("new_pe get cap ERR, port %d rc %d\n", 
					cur_port, rc);
				goto exit;
			};
			if (RIOCP_PE_IS_SWITCH(capabilities)) {
				INFO("New device is switch!\n");
				l_push_tail(&sw, new_pe);
			};
		};

		curr_pe = (riocp_pe_handle)l_pop_head(&sw);
	};
	return 0;
exit:
	return -1;
};

void setup_mport(struct fmd_state *fmd)
{
	uint8_t *mport_list;
	int mport, found = 0, rc = 0;
	size_t mport_cnt, i;

	fmd->mp_h = &mport_pe;

	if (riocp_mport_get_port_list(&mport_cnt, &mport_list)) {
		CRIT("\nCannot get mport list, exiting...\n");
		exit(EXIT_FAILURE);
	};

	if (FMD_SLAVE == fmd->cfg->mast_idx)
		mport = fmd->cfg->mport_info[0].num;
	else
		mport = fmd->cfg->mport_info[fmd->cfg->mast_idx].num;

	for (i = 0; !found && (i < mport_cnt); i++) {
		if (mport_list[i] == mport)
			found = 1;
	};

	if (!found) {
		CRIT("\nConfigured mport not present, exiting...\n");
		exit(EXIT_FAILURE);
	};

	if (rio_maint_init(mport, &reg_acc_h)) {
		CRIT("\nCannot open mport %d, exiting...\n", mport);
		exit(EXIT_FAILURE);
	};

	if (riocp_pe_create_host_handle(&mport_pe, mport, 0)) {
		CRIT("\nCannot create host handle, exiting...\n");
		exit(EXIT_FAILURE);
	};

	if (!(RIOCP_PE_IS_MPORT(mport_pe))) {
		CRIT("\nHost port is not an MPORT, wazzup?...\n");
		exit(EXIT_FAILURE);
	};

	if (FMD_SLAVE == fmd->cfg->mast_idx)
		rc = fmd_traverse_network(mport_pe, 0, DISCOVER);
	else
		rc = fmd_traverse_network(mport_pe, 0, ENUMERATE);

	if (rc) {
		CRIT("\nNetwork initialization failed...\n");
		exit(EXIT_FAILURE);
	};
}

int main(int argc, char *argv[])
{

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	rdma_log_init("fmd_log", 0);
	cfg = fmd_parse_options(argc, argv);
	fmd_process_cfg_file(cfg);
	
	if ((NULL == cfg) || (cfg->init_err))
		goto fail;

	fmd_dd_init(cfg, &fmd);
	if ((NULL == fmd) || (cfg->init_err))
		goto fail;


	cli_init_base();
	bind_dd_cmds(fmd);
	liblog_bind_cli_cmds();
	setup_mport(fmd);
	if (!fmd->cfg->simple_init)
		fmd_dd_update(fmd);

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

#ifdef __cplusplus
}
#endif
