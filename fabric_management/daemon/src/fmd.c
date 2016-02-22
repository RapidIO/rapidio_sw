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
#include "DAR_DevDriver.h"
#include "fmd_dd.h"
#include "fmd_app_msg.h"
#include "liblist.h"
#include "liblog.h"
#include "cfg.h"
#include "fmd_cfg_cli.h"
#include "fmd_state.h"
#include "fmd_app_mgmt.h"
#include <rapidio_mport_mgmt.h>
#include "fmd_mgmt_cli.h"
#include "fmd_mgmt_master.h"
#include "fmd_dev_rw_cli.h"
#include "fmd_opts.h"
#include "libfmdd.h"
#include "pe_mpdrv_private.h"
#include "IDT_Routing_Table_Config_API.h"

#ifdef __cplusplus
extern "C" {
#endif

riocp_pe_handle mport_pe;
DAR_DEV_INFO_t *dev_h;

struct fmd_opt_vals *opts;
struct fmd_state *fmd;

void custom_quit(cli_env *env)
{
	shutdown_mgmt();
	halt_app_handler();
	cleanup_app_handler();
	fmd_dd_cleanup(fmd->dd_mtx_fn, &fmd->dd_mtx_fd, &fmd->dd_mtx,
			fmd->dd_fn, &fmd->dd_fd, &fmd->dd, fmd->fmd_rw);
};

void sig_handler(int signo)
{
	INFO("\nRx Signal %x\n", signo);
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
                strncpy(e->prompt, "UNINIT> ", PROMPTLEN);
                return;
        };

        if (NULL == e->h) {
                strncpy(e->prompt, "HUNINIT> ", PROMPTLEN);
                return;
        };

	pe_h = (riocp_pe_handle)(e->h);

	if (riocp_pe_get_comptag(pe_h, &comptag))
		comptag = 0xFFFFFFFF;
	pe_did = comptag & 0x0000FFFF;

	name = riocp_pe_handle_get_device_str(pe_h);

	snprintf(e->prompt, PROMPTLEN,  "%4x_%s> ", pe_did, name);
};

void *poll_loop( void *poll_interval ) 
{
	int wait_time = ((int *)(poll_interval))[0];
	int console = ((int*)(poll_interval))[1];
        char my_name[16];
	free(poll_interval);

	INFO("RIO_DEMON: Poll interval %d seconds\n", wait_time);
	sem_post(&cons_owner);

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "FMD_DD_POLL_%02d",wait_time);
        pthread_setname_np(poll_thread, my_name);

        pthread_detach(poll_thread);

	while(TRUE) {
		fmd_dd_incr_chg_idx(fmd->dd, 1);
		sleep(wait_time);
		if (!console)
			INFO("\nTick!");
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
        char my_name[16];

        memset(my_name, 0, 16);
        snprintf(my_name, 15, "ETH_CLI_SESS");
        pthread_setname_np(cli_session_thread, my_name);

        pthread_detach(cli_session_thread);

     portno = *(int *)(sock_num);
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
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        	CRIT("ERROR on binding");
		goto fail;
	}

	INFO("\nFMD bound to socket %d\n", portno);
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
			CRIT("ERROR on accept");
			goto fail;
		};
		INFO("\nStarting session %d\n", session_num);
		cli_terminal( &env );
		INFO("\nFinishing session %d\n", session_num);
		close(env.sess_socket);
		session_num++;
	};
fail:
	INFO("\nRIO_DEMON CLI Thread Exiting\n");
	if (newsockfd >=0)
		close(newsockfd);
     	close(sockfd);
	pthread_exit( (void *)(&n) );
}

void spawn_threads(struct fmd_opt_vals *cfg)
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

	cli_init_base(custom_quit);
	bind_dd_cmds(fmd->dd, fmd->dd_mtx, fmd->dd_fn, fmd->dd_mtx_fn);
	liblog_bind_cli_cmds();
	// fmd_bind_dbg_cmds();
	fmd_bind_mgmt_dbg_cmds();
	fmd_bind_dev_rw_cmds();

	/* Create independent threads each of which will execute function */
	poll_ret = pthread_create( &poll_thread, NULL, poll_loop, 
				(void*)(pass_poll_interval));
	if(poll_ret) {
		CRIT("Error - poll_thread rc: %d\n",poll_ret);
		exit(EXIT_FAILURE);
	}
 
	cli_ret = pthread_create( &cli_session_thread, NULL, 
		cli_session, (void*)(pass_sock_num));
	if(cli_ret) {
		CRIT("Error - cli_session_thread rc: %d\n",cli_ret);
		exit(EXIT_FAILURE);
	}

	if (cfg->run_cons) {

		splashScreen((char *)"FMD Daemon Command Line Interface");
		cons_ret = pthread_create( &console_thread, NULL, 
			console, (void *)((char *)"FMD > "));
		if(cons_ret) {
			CRIT("Error - cons_thread rc: %d\n",cli_ret);
			exit(EXIT_FAILURE);
		}
	};
	INFO("pthread_create() for poll_loop returns: %d\n",poll_ret);
	INFO("pthread_create() for cli_session_thread returns: %d\n",cli_ret);
	if (cfg->run_cons) 
		CRIT("pthread_create() for console returns: %d\n",
			cons_ret);
 
	ret = start_fmd_app_handler(cfg->app_port_num, 50, 0, 
					cfg->dd_fn, cfg->dd_mtx_fn); 
	if (ret) {
		CRIT("Error - start_fmd_app_handler rc: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	ret = start_peer_mgmt(cfg->mast_cm_port, 0, cfg->mast_devid, 
			cfg->mast_mode);
	if (ret) {
		CRIT("Error - start_fmd_app_handler rc: %d\n", ret);
		exit(EXIT_FAILURE);
	}
}
 
/* FIXME: Currently limited to supporting probe of mport, one switch, and
 * connected endpoints.
 */

int config_sw_routing(riocp_pe_handle swtch, struct cfg_dev *conn_sw)
{
	int i, rc= 0;

	for (i = 0; i < IDT_DAR_RT_DEV_TABLE_SIZE && !rc; i++) {
		int rt_val =
			conn_sw->sw_info.rt[CFG_DEV08]->dev_table[i].rte_val;

		if (rt_val >= IDT_DSF_FIRST_MC_MASK)
			rt_val = 0xDE;
		if (ANY_ID != i) {
			rc = riocp_sw_set_route_entry(swtch, RIO_ALL_PORTS, i,
				rt_val);
		};
		DBG("idx %d rc %d\n", i, rc);
	};

	return rc;
};

int fmd_traverse_network(int mport_num, riocp_pe_handle mport_pe, struct cfg_dev *c_dev)
{
	riocp_pe_handle new_pe, curr_pe, swtch;
	int port_cnt, conn_pt, rc, pnum;
	uint32_t comptag, ep_ct;
	struct cfg_dev conn_sw, conn_ep;
	struct riocp_pe_capabilities capabilities;

	/* Initialize master port */
	curr_pe = mport_pe;
	if (cfg_get_conn_dev(c_dev->ct, mport_num, &conn_sw, &conn_pt)) {
		CRIT("No config dev connected to CT 0x%x Mport %d\n",
			c_dev->ct, mport_num);
		goto exit;
	};

	if (!conn_sw.is_sw) {
		CRIT("Switch config not connected to CT 0x%x Mport %d\n",
			c_dev->ct, mport_num);
		goto exit;
	};

	comptag = conn_sw.ct;

	rc = riocp_pe_probe(curr_pe, 0, &swtch, &comptag, (char *)conn_sw.name);
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

	if (comptag != conn_sw.ct) {
		CRIT("Probed switch comptag 0x%x != 0x%x config comptag\n",
			comptag, conn_sw.ct);
		goto exit;
	};

	rc = riocp_pe_get_capabilities(swtch, &capabilities);
	if (rc) {
		CRIT("Get switch capabilities failed, rc %d\n", rc);
		goto exit;
	};

	rc = config_sw_routing(swtch, &conn_sw);
	if (rc) {
		CRIT("Cannot configure switch routing rc %d\n", rc);
		goto exit;
	};

	port_cnt = RIOCP_PE_PORT_COUNT(capabilities);

	for (pnum = port_cnt - 1; pnum >= 0 ; pnum--) {
		new_pe = NULL;
		
		if (cfg_get_conn_dev(conn_sw.ct, pnum, &conn_ep, &conn_pt)) {
			INFO("Switch Port %d NO CONFIG\n", pnum);
			continue;
		};

		rc = riocp_pe_probe(swtch, pnum, &new_pe, &conn_ep.ct,
				(char *)conn_ep.name);

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

		if (ep_ct != conn_ep.ct) {
			DBG("Probed ep ct 0x%x != 0x%x config ct port %d\n",
				comptag, conn_sw.ct, pnum);
			goto exit;
		};

		INFO("Switch Port %d DEVICE %s CT 0x%x DID 0x%x\n", pnum, 
			new_pe->name, new_pe->comptag, new_pe->destid);
	};
	return 0;
exit:
	return -1;
};

int setup_mport_master(int mport)
{
	/* FIXME: Change this to support other master ports etc... */
	uint32_t comptag;
	struct cfg_mport_info mp;
	struct cfg_dev cfg_dev;

	if (cfg_find_mport(mport, &mp)) {
		CRIT("\nCannot find configured mport, exiting...\n");
		exit(EXIT_FAILURE);
	};

	comptag = mp.ct;

	if (cfg_find_dev_by_ct(comptag, &cfg_dev)) {
		CRIT("\nCannot find configured mport device, exiting...\n");
		exit(EXIT_FAILURE);
	};

	if (riocp_pe_create_host_handle(&mport_pe, mport, 0, &pe_mpsw_rw_driver,
			&comptag, (char *)cfg_dev.name)) {
		CRIT("\nCannot create host handle mport %d, exiting...",
			mport);
		exit(EXIT_FAILURE);
	};

	return fmd_traverse_network(mport, mport_pe, &cfg_dev);
};

int setup_mport_slave(int mport, uint32_t m_did, uint32_t m_cm_port)
{
	int rc, ret;
	uint32_t comptag;
	struct cfg_mport_info mp;
	struct cfg_dev cfg_dev;
	char slave_mp_fn[FMD_MAX_DEV_FN];
	char mast_dev_fn[FMD_MAX_DEV_FN];
	struct mpsw_drv_private_data *p_dat;
	struct mpsw_drv_pe_acc_info *acc_p;

	if (cfg_find_mport(mport, &mp)) {
		CRIT("\nCannot find configured mport, exiting...\n");
		exit(EXIT_FAILURE);
	};

	comptag = mp.ct;

	if (cfg_find_dev_by_ct(comptag, &cfg_dev)) {
		CRIT("\nCannot find configured mport device, exiting...\n");
		exit(EXIT_FAILURE);
	};

	if (riocp_pe_create_agent_handle(&mport_pe, mport, 0,
			&pe_mpsw_rw_driver, &comptag, (char *)cfg_dev.name)) {
		CRIT("\nCannot create agent handle, exiting...\n");
		exit(EXIT_FAILURE);
	};

	ret = riocp_pe_handle_get_private(mport_pe, (void **)&p_dat);
	if (ret) {
		CRIT("\nCannot retrieve mport private data, exiting...\n");
		exit(EXIT_FAILURE);
	};

	acc_p = (mpsw_drv_pe_acc_info *)p_dat->dev_h.accessInfo;
	if ((NULL == acc_p) || !acc_p->maint_valid) {
		CRIT("\nMport access info is NULL, exiting...\n");
		exit(EXIT_FAILURE);
	};

	/* Poll to add the mport  and FMD master devices until the master
	* completes network initialization.
	*/
	memset(slave_mp_fn, 0, FMD_MAX_DEV_FN);
	snprintf(slave_mp_fn, FMD_MAX_DEV_FN-1, "%s%s",
                        FMD_DFLT_DEV_DIR, FMD_SLAVE_MPORT_NAME);
	memset(mast_dev_fn, 0, FMD_MAX_DEV_FN);
	snprintf(mast_dev_fn, FMD_MAX_DEV_FN-1, "%s%s",
                        FMD_DFLT_DEV_DIR, FMD_SLAVE_MASTER_NAME);
	do {
		if (access(slave_mp_fn, F_OK) != -1) {
                        rc = 0;
                } else {
			rc = riomp_mgmt_device_add(acc_p->maint,
				mp.devids[CFG_DEV08].devid,
				(uint8_t)0xFF, comptag, FMD_SLAVE_MPORT_NAME);
		};
		if (rc) {
			INFO("\nCannot add mport0 object %d %d: %s\n", 
				rc, errno, strerror(errno));
			sleep(5);
			continue;
		};

		if (access(mast_dev_fn, F_OK) != -1) {
                        rc = 0;
                } else {
			rc = riomp_mgmt_device_add(acc_p->maint,
				(uint16_t)fmd->opts->mast_devid,
				1, fmd->opts->mast_devid,
				FMD_SLAVE_MASTER_NAME);
		};
		if (rc) {
			CRIT("\nCannot add FMD Master device %d %d: %s\n", 
				rc, errno, strerror(errno));
			sleep(5);
		};
	} while (EIO == rc);
	return rc;
};

void setup_mport(struct fmd_state *fmd)
{
	int rc = 0;
	int mport = 0;
	STATUS dsf_rc;

        dsf_rc = IDT_DSF_bind_DAR_routines(SRIO_API_ReadRegFunc,
                                SRIO_API_WriteRegFunc,
                                SRIO_API_DelayFunc);
        if (dsf_rc) {
                CRIT("\nCannot initialize RapidIO APIs...\n");
                exit(EXIT_FAILURE);
        };

        if (riocp_bind_driver(&pe_mpsw_driver)) {
                CRIT("\nFailed to bind riocp driver, exiting...\n");
                exit(EXIT_FAILURE);
        };

	fmd->mp_h = &mport_pe;

	if (fmd->opts->mast_mode)
		rc = setup_mport_master(mport);
	else
		rc = setup_mport_slave(mport, fmd->opts->mast_devid,
						fmd->opts->mast_cm_port);

	if (rc) {
		CRIT("\nNetwork initialization failed...\n");
	};
}

int fmd_dd_update(riocp_pe_handle mp_h, struct fmd_dd *dd,
			struct fmd_dd_mtx *dd_mtx)
{
        int rc = 1;
        uint32_t comptag;
	struct cfg_dev c_dev;

        if (NULL == mp_h) {
                WARN("\nMaster port is NULL, device directory not updated\n");
                goto fail;
        };

	rc = riocp_pe_get_comptag(mp_h, &comptag);
	if (rc) {
		WARN("Cannot get mport comptag rc %d...\n", rc);
		comptag = 0xFFFFFFFF;
	};

	if (cfg_find_dev_by_ct(comptag, &c_dev))
		goto fail;

	add_device_to_dd(c_dev.ct, c_dev.did, FMD_DEV08, c_dev.hc, 1,
			FMDD_FLAG_OK_MP, (char *)c_dev.name); 

        fmd_dd_incr_chg_idx(dd, 1);
        sem_post(&dd_mtx->sem);
	return 0;
fail:
        return 1;
};

int main(int argc, char *argv[])
{

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	rdma_log_init("fmd.log", 1);
	opts = fmd_parse_options(argc, argv);
	g_level = opts->log_level;
	if ((opts->init_and_quit) && (opts->print_help))
		goto fail;
        fmd = (fmd_state *)malloc(sizeof(struct fmd_state));
        fmd->opts = opts;
        fmd->fmd_rw = 1;

	if (cfg_parse_file(opts->fmd_cfg, &fmd->dd_mtx_fn, &fmd->dd_fn, 
			&fmd->opts->mast_devid, &fmd->opts->mast_cm_port,
			&fmd->opts->mast_mode))
		goto fail;

	if (fmd_dd_init(opts->dd_mtx_fn, &fmd->dd_mtx_fd, &fmd->dd_mtx,
			opts->dd_fn, &fmd->dd_fd, &fmd->dd))
		goto dd_cleanup;

	setup_mport(fmd);

	if (!fmd->opts->simple_init)
		if (fmd_dd_update(*fmd->mp_h, fmd->dd, fmd->dd_mtx))
			goto fail;

	if (!fmd->opts->init_and_quit) {
		spawn_threads(fmd->opts);

		pthread_join(cli_session_thread, NULL);
		if (fmd->opts->run_cons)
			pthread_join(console_thread, NULL);
	};
	shutdown_mgmt();
	halt_app_handler();
	cleanup_app_handler();

dd_cleanup:
	fmd_dd_cleanup(opts->dd_mtx_fn, &fmd->dd_mtx_fd, &fmd->dd_mtx,
			opts->dd_fn, &fmd->dd_fd, &fmd->dd, fmd->fmd_rw);
fail:
	exit(EXIT_SUCCESS);
}

#ifdef __cplusplus
}
#endif
