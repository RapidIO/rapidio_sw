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
#include "DAR_DevDriver.h"
#include "DAR_RegDefs.h"

#include "IDT_DSF_DB_Private.h"
#include "DAR_Utilities.h"

#include "libcli.h"
#include "riocp_pe.h"
#include "riocp_pe_internal.h"
#include "librio_maint.h"
#include "DAR_DevDriver.h"
#include "fmd_dd.h"
#include "fmd_app_msg.h"
#include "liblist.h"
#include "liblog.h"
#include "fmd_cfg.h"
#include "fmd_cfg_cli.h"
#include "fmd_state.h"
#include "fmd_app_mgmt.h"
#include "riodp_mport_lib.h"
#include "fmd_mgmt_cli.h"
#include "fmd_mgmt_master.h"
#include "fmd_dev_rw_cli.h"
#include "libfmdd.h"

#ifdef __cplusplus
extern "C" {
#endif

riocp_pe_handle mport_pe;
rio_maint_handle reg_acc_h;
DAR_DEV_INFO_t *dev_h;

struct fmd_cfg_parms *cfg;
struct fmd_state *fmd;

void custom_quit(cli_env *env)
{
	env = env;
	shutdown_mgmt();
	halt_app_handler();
	cleanup_app_handler();
	fmd_dd_cleanup(fmd->dd_mtx_fn, &fmd->dd_mtx_fd, &fmd->dd_mtx,
			fmd->dd_fn, &fmd->dd_fd, &fmd->dd, fmd->fmd_rw);
};

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		custom_quit(NULL);
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
        uint16_t pe_did = 0;

        if (NULL == e) {
                strncpy(e->prompt, "UNINIT>", PROMPTLEN);
                return;
        };

        if (NULL == e->h) {
                strncpy(e->prompt, "HUNINIT>", PROMPTLEN);
                return;
        };

	pe_h = (riocp_pe_handle)(e->h);

	if (riocp_pe_get_comptag(pe_h, &comptag))
		comptag = 0xFFFFFFFF;
	pe_did = comptag & 0x0000FFFF;

	name = riocp_pe_handle_get_device_str(pe_h);

	snprintf(e->prompt, PROMPTLEN,  "%4x_%s>", pe_did, name);
};

void *poll_loop( void *poll_interval ) 
{
	int wait_time = ((int *)(poll_interval))[0];
	int console = ((int*)(poll_interval))[1];
	free(poll_interval);

	printf("RIO_DEMON: Poll interval %d seconds\n", wait_time);
	sem_post(&cons_owner);
	while(TRUE) {
		fmd_dd_incr_chg_idx(fmd->dd, 1);
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
	int ret;

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
		cli_init_base(custom_quit);
		bind_dd_cmds(fmd->dd, fmd->dd_mtx, fmd->dd_fn, fmd->dd_mtx_fn);
		liblog_bind_cli_cmds();
		fmd_bind_dbg_cmds();
		fmd_bind_mgmt_dbg_cmds();
		fmd_bind_dev_rw_cmds();

		splashScreen((char *)"FMD Daemon Command Line Interface");
		cons_ret = pthread_create( &console_thread, NULL, 
			console, (void *)((char *)"FMD > "));
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
 
	ret = start_fmd_app_handler(cfg->app_port_num, 50, 0, 
					cfg->dd_fn, cfg->dd_mtx_fn); 
	if (ret) {
		fprintf(stderr,"Error - start_fmd_app_handler rc: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	ret = start_peer_mgmt(cfg->mast_cm_port, 0, cfg->mast_devid, 
			FMD_SLAVE != cfg->mast_idx);
	if (ret) {
		fprintf(stderr,"Error - start_fmd_app_handler rc: %d\n", ret);
		exit(EXIT_FAILURE);
	}
}
 
int fmd_init_switch(riocp_pe_handle pe, struct fmd_cfg_sw *sw)
{
	enum riocp_sw_default_route_action dft_act = 
						RIOCP_SW_DEFAULT_ROUTE_DROP;
	idt_rt_state_t *rt;
	uint32_t ret, did;

	if (NULL == sw)
		return 0;

	sw->sw_h = pe;
	rt = &sw->rt[FMD_DEV08];

	if (rt->default_route < RIOCP_SW_DEFAULT_ROUTE_DROP)
		dft_act = RIOCP_SW_DEFAULT_ROUTE_UNICAST;
	
	ret = riocp_sw_set_default_route_action(pe, dft_act, rt->default_route);
	if (ret < 0) {
		CRIT("Could not set default route for sw\n");
		goto exit;
	};

	for (did = 0; did < ANY_ID; did++) {
		ret = riocp_sw_set_route_entry(pe, RIOCP_PE_ANY_PORT, did, 
						rt->dev_table[did].rte_val);
		if (ret) {
			CRIT("Could not set route for did %d\n", did);
			goto exit;
		};
	};
	ret = 0;
	sw->valid = 1;
	
exit:
	return ret;
};

/* FIXME: Currently limited to supporting probe of mport, one switch, and
 * connected endpoints.
 */

int fmd_traverse_network(riocp_pe_handle mport_pe, int port_num, 
			struct fmd_cfg_parms *cfg)
{
	riocp_pe_handle new_pe, curr_pe, swtch;
	int port_cnt, rc, pnum;
	struct riocp_pe_capabilities capabilities;
	uint32_t comptag, ep_ct, con_end, conn_did, conn_hc;
	struct fmd_cfg_ep *conn_ep; 

	/* Initialize master port */
	curr_pe = mport_pe;

	rc = riocp_pe_probe(curr_pe, 0, &swtch);
	if (rc) {
		if ((-ENODEV != rc) && (-EIO != rc)) {
			CRIT("Mport probe failed %d\n", rc);
			goto exit;
		};
		INFO("Mport %x Probe NO DEVICE\n", comptag);
		goto exit;
	};

	rc = riocp_pe_get_comptag(swtch, &comptag);
	if (rc) {
		CRIT("Get switch comptag failed, rc %d\n", rc);
		goto exit;
	};
	INFO("Updating SwitchCompTag %x\n", comptag);

	if (riocp_pe_update_comptag(swtch, &comptag, fmd->cfg->sws[0].did, 0)) {
		CRIT("\nFailed to update switch component tag\n");
		exit(EXIT_FAILURE);
	};

	fmd->cfg->sws[0].ct = comptag;
	INFO("Initializing Switch %x\n", comptag);
	if (fmd_init_switch(swtch, &cfg->sws[0])) {
		CRIT("\nFailed to initialize switch\n");
		exit(EXIT_FAILURE);
	};

	rc = riocp_pe_get_capabilities(swtch, &capabilities);
	if (rc) {
		CRIT("Get switch capabilities failed, rc %d\n", rc);
		goto exit;
	};

	port_cnt = RIOCP_PE_PORT_COUNT(capabilities);

	for (pnum = 0; pnum < port_cnt; pnum++) {
		new_pe = NULL;
		rc = riocp_pe_probe(swtch, pnum, &new_pe);

		if (rc) {
			if ((-ENODEV != rc) && (-EIO != rc)) {
				CRIT("Port %d probe failed %d\n", pnum, rc);
				goto exit;
			};
			INFO("Switch Port %d NO DEVICE\n", pnum);
			continue;
		};

		if (NULL == new_pe)
			continue;

		rc = riocp_pe_get_comptag(new_pe, &ep_ct);
		if (rc) {
			CRIT("Get new comptag failed, rc %d\n", rc);
			goto exit;
		};
		INFO("Switch Port %d Found %x \n", pnum, ep_ct);
		if (NULL == fmd->cfg->sws[0].ports[pnum].conn) {
			INFO("Switch Port %d Found Unexpected Endpoint %x \n",
				pnum, ep_ct);
			INFO("Switch Port %d Unconfigured!!!\n", pnum);
			continue;
		};
			
		con_end = OTHER_END(fmd->cfg->sws[0].ports[pnum].conn_end);
		conn_ep = fmd->cfg->sws[0].ports[pnum].conn->ends[con_end].ep_h;
		conn_did = conn_ep->ports[0].devids[FMD_DEV08].devid;
		conn_hc = conn_ep->ports[0].devids[FMD_DEV08].hc;

		rc = riocp_pe_update_comptag(new_pe, &ep_ct, conn_did, 1);
		if (rc) {
			CRIT("\nFailed to update EP component tag\n");
			exit(EXIT_FAILURE);
		};
		conn_ep->ep_h = new_pe;

		rc = riocp_pe_get_comptag(new_pe, &ep_ct);
		if (rc) {
			CRIT("Get updated comptag failed, rc %d\n", rc);
			goto exit;
		};

		rc = riodp_device_add(new_pe->mport->minfo->maint->fd, 
			conn_did, conn_hc, ep_ct, conn_ep->name);
		if (rc && (EEXIST != rc)) {
			CRIT("riodp_device_add, rc %d\n", rc);
			goto exit;
		};
		conn_ep->ports[0].ct = ep_ct;	
	};
	return 0;
exit:
	return -1;
};

int setup_mport_master(int mport)
{
	/* FIXME: Change this to support other master ports etc... */
	struct fmd_cfg_ep *ep;
	uint32_t comptag;

	if (riocp_pe_create_host_handle(&mport_pe, mport, 0)) {
		CRIT("\nCannot create host handle, exiting...\n");
		exit(EXIT_FAILURE);
	};

	fmd->fd = mport_pe->fd;

	if (!(RIOCP_PE_IS_MPORT(mport_pe))) {
		CRIT("\nHost port is not an MPORT, wazzup?...\n");
		exit(EXIT_FAILURE);
	};

	if (riocp_pe_get_comptag(mport_pe, &comptag)) {
		CRIT("\nCannot read mport0 comptag\n");
		exit(EXIT_FAILURE);
	};
	if (riocp_pe_update_comptag(mport_pe, &comptag, 
		fmd->cfg->mport_info[0].devids[FMD_DEV08].devid, 1)) {
		CRIT("\nCannot update mport0 comptag\n");
		exit(EXIT_FAILURE);
	};
	if (riocp_pe_get_comptag(mport_pe, &comptag)) {
		CRIT("\nCannot read mport0 comptag\n");
		exit(EXIT_FAILURE);
	};

	fmd->cfg->mport_info[0].mp_h = mport_pe;
	fmd->cfg->mport_info[0].ct = comptag;

		
	ep = fmd->cfg->mport_info[0].ep;
	if (NULL == ep) {
		CRIT("\nNo endpoint defined for master port.\n");
		exit(EXIT_FAILURE);
	};

	return fmd_traverse_network(mport_pe, 0, fmd->cfg);
};

int setup_mport_slave(int mport)
{
	uint32_t comptag;
	struct fmd_cfg_ep *ep;
	int rc;
	struct timespec dly = {5, 0}; /* 5 seconds */

	if (riocp_pe_create_agent_handle(&mport_pe, mport, 0)) {
		CRIT("\nCannot create agent handle, exiting...\n");
		exit(EXIT_FAILURE);
	};

	fmd->fd = mport_pe->fd;
	fmd->cfg->mport_info[0].mp_h = mport_pe;
		
	ep = fmd->cfg->mport_info[0].ep;
	if (NULL == ep) {
		CRIT("\nNo endpoint defined for master port.\n");
		exit(EXIT_FAILURE);
	};
	ep->valid = 1;
	ep->ep_h = mport_pe;

	/* Write MPORT device ID */
	rc = riodp_destid_set(reg_acc_h->fd, 
		(uint16_t)fmd->cfg->mport_info[0].devids[FMD_DEV08].devid); 

	if (rc) {
		CRIT("\nriodp_destid_set rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
	};
		
	/* Write MPORT Component Tag */
	rc = riodp_lcfg_read(reg_acc_h->fd, 0x6c, 4, &comptag);
	if (rc) {
		CRIT("\nriodp_lcfg_read 1 rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
	};

	comptag = (comptag & 0xFFFF0000) | 
		(uint32_t)fmd->cfg->mport_info[0].devids[FMD_DEV08].devid; 

	rc = riodp_lcfg_write(reg_acc_h->fd, 0x6c, 4, comptag);
	if (rc) {
		CRIT("\nriodp_lcfg_write 1 rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
	};

	fmd->cfg->mport_info[0].ct = comptag;
	ep->ports[0].ct = comptag;

	/* Poll to add the mport  and FMD master devices until the master
	* completes network initialization.
	*/
	do {
		rc = riodp_device_add(reg_acc_h->fd, 
		(uint16_t)fmd->cfg->mport_info[0].devids[FMD_DEV08].devid, 
			(uint8_t)0xFF, comptag, FMD_SLAVE_MPORT_NAME);
		if (EEXIST == rc)
			rc = 0;
		if (rc) {
			INFO("\nCannot add mport0 object %d %d: %s\n", 
				rc, errno, strerror(errno));
			nanosleep(&dly, NULL);
			continue;
		};

		rc = riodp_device_add(reg_acc_h->fd,
		(uint16_t)cfg->mast_devid, 1, cfg->mast_devid,
			FMD_SLAVE_MASTER_NAME);
		if (EEXIST == rc)
			rc = 0;
		if (rc) {
			CRIT("\nCannot add FMD Master device %d %d: %s\n", 
				rc, errno, strerror(errno));
		};
	} while (EIO == rc);
	return rc;
};

int do_mport_fixups(void)
{
	int rc;
	uint32_t port_ctl;

	rc = riodp_lcfg_write(reg_acc_h->fd, 0x13c, 4, 0xE0000000);
	if (rc) {
		CRIT("\nSet MAST_EN failed rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
		goto exit;
	};
	
	rc = riodp_lcfg_write(reg_acc_h->fd, 0x120, 4, 0x0000FF00);
	if (rc) {
		CRIT("\nSet Link response timeout failed rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
		goto exit;
	};
	
	rc = riodp_lcfg_write(reg_acc_h->fd, 0x124, 4, 0x0000FF00);
	if (rc) {
		CRIT("\nSet Packet response timeout failed rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
		goto exit;
	};
	
	rc = riodp_lcfg_write(reg_acc_h->fd, 0x10a04, 4, 0x00000000);
	if (rc) {
		CRIT("\nSet Port-Write handling mode failed rc: %d %d: %s\n", 
			rc, errno, strerror(errno));
	};

	rc = riodp_lcfg_read(reg_acc_h->fd, 0x15C, 4, &port_ctl);
	if (rc) {
		CRIT("\nCannot read Port 0 control CSR: %d %d: %s\n", 
			rc, errno, strerror(errno));
	};
	port_ctl |= 0x00600000;
	rc = riodp_lcfg_write(reg_acc_h->fd, 0x15C, 4, port_ctl);
	if (rc) {
		CRIT("\nCannot write Port 0 control CSR: %d %d: %s\n", 
			rc, errno, strerror(errno));
	};
exit:
	return rc;
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

	rc = do_mport_fixups();
	if (rc) {
		CRIT("\nCannot do mport %d fixups, exiting...\n", mport);
		exit(EXIT_FAILURE);
	};

	
	if (FMD_SLAVE == fmd->cfg->mast_idx)
		rc = setup_mport_slave(mport);
	else
		rc = setup_mport_master(mport);

	if (rc) {
		CRIT("\nNetwork initialization failed...\n");
	};
}

void fmd_dd_update(riocp_pe_handle mp_h, struct fmd_dd *dd,
			struct fmd_dd_mtx *dd_mtx)
{
        int rc;
        uint32_t comptag;
	struct fmd_cfg_ep *cfg_ep;

        if (NULL == mp_h) {
                WARN("\nMaster port is NULL, device directory not updated\n");
                goto exit;
        };

	rc = riocp_pe_get_comptag(mp_h, &comptag);
	if (rc) {
		WARN("Cannot get mport comptag rc %d...\n", rc);
		comptag = 0xFFFFFFFF;
	};

	cfg_ep = find_cfg_ep_by_ct(comptag, fmd->cfg);

	add_device_to_dd(cfg_ep->ports[0].ct, 
			cfg_ep->ports[0].devids[FMD_DEV08].devid, 
			FMD_DEV08, cfg_ep->ports[0].devids[FMD_DEV08].hc,
			1,
			FMDD_FLAG_OK_MP,
			cfg_ep->name); 

        fmd_dd_incr_chg_idx(dd, 1);
        sem_post(&dd_mtx->sem);
exit:
        return;
};

int main(int argc, char *argv[])
{

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	rdma_log_init("fmd.log", 1);
	cfg = fmd_parse_options(argc, argv);
	if ((cfg->init_and_quit) && (cfg->print_help))
		goto fail;
	fmd_process_cfg_file(cfg);
	
	if ((NULL == cfg) || (cfg->init_err))
		goto fail;

        fmd = (fmd_state *)malloc(sizeof(struct fmd_state));
        fmd->cfg = cfg;
        fmd->fmd_rw = 1;


        fmd->dd_mtx_fn = (char *)malloc(strlen(cfg->dd_mtx_fn)+1);
        memset(fmd->dd_mtx_fn, 0, strlen(cfg->dd_mtx_fn)+1);
        strncpy(fmd->dd_mtx_fn, cfg->dd_mtx_fn, strlen(cfg->dd_mtx_fn));

        fmd->dd_fn = (char *)malloc(strlen(cfg->dd_fn)+1);
        memset(fmd->dd_fn, 0, strlen(cfg->dd_fn)+1);
        strncpy(fmd->dd_fn, cfg->dd_fn, strlen(cfg->dd_fn));

	fmd_dd_init(fmd->dd_mtx_fn, &fmd->dd_mtx_fd, &fmd->dd_mtx,
		fmd->dd_fn, &fmd->dd_fd, &fmd->dd);
	if ((NULL == fmd) || (cfg->init_err))
		goto fail;

	setup_mport(fmd);
	if (!fmd->cfg->simple_init)
		fmd_dd_update(*fmd->mp_h, fmd->dd, fmd->dd_mtx);

	if (!cfg->init_and_quit) {
		spawn_threads(cfg);

		pthread_join(poll_thread, NULL);
		pthread_join(cli_session_thread, NULL);
		if (cfg->run_cons)
			pthread_join(console_thread, NULL);
	};
	shutdown_mgmt();
	halt_app_handler();
	cleanup_app_handler();
 
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
