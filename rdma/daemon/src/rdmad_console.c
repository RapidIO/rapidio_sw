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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <cstdio>
#include <cstring>

#include <signal.h>
#include <pthread.h>

#include "liblog.h"

#include "cm_sock.h"

#include "rdmad_main.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_cm.h"
#include "libcli.h"

int ibwin_info_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)argc;
	(void)argv;

	the_inbound->dump_info(env);
	return 0;
} /* ibwin_info_cmd_f() */

int all_ms_info_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)argc;
	(void)argv;
	the_inbound->dump_all_mspace_info(env);
	return 0;
} /* all_ms_info_cmd_f() */

int all_ms_msub_info_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)argc;
	(void)argv;
	the_inbound->dump_all_mspace_with_msubs_info(env);
	return 0;
} /* all_ms_msub_info_cmd_f() */

int owners_cmd_f(struct cli_env *env, int argc, char **argv)
{
	(void)argc;
	(void)argv;
	the_inbound->get_owners().dump_info(env);
	return 0;
}

extern struct cli_cmd hello_rdaemon_cmd ;

int hello_rdaemon_cmd_f(struct cli_env *env, int argc, char **argv)
{
        uint32_t *mport_list = NULL;
        uint32_t *ep_list = NULL;
        uint32_t *list_ptr;
        uint32_t number_of_eps = 0;
        uint8_t  number_of_mports = RIODP_MAX_MPORTS;
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
        if (number_of_mports > RIODP_MAX_MPORTS) {
                printf("WARNING: Only %d out of %d have been retrieved\n",
                        RIODP_MAX_MPORTS, number_of_mports);
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

	if (argc != 1) {
		cli_print_help(env, &hello_rdaemon_cmd);
		return 0;
	}

	/* Extract parameters from command */
	uint32_t destid   = getDecParm(argv[0], 0);

	/* Call provisioning command to send HELLO to remote daemon */
	ret = provision_rdaemon(destid);
	if (ret == -7) {
		sprintf(env->output, "destid(0x%X) already provisioned\n", destid);
		logMsg(env);
		return 0;
	}

	sprintf(env->output, "Return code %d:%s\n", ret, strerror(ret));
	logMsg(env);

	return 0;
} /* hello_rdaemon_cmd_f() */

struct cli_cmd ibwin_info_cmd = {
	"ibinfo",
	1,
	0,
	"Inbound Window Info.",
	"{None}\n"
	"Displays info about all inbound windows.\n",
	ibwin_info_cmd_f,
	ATTR_NONE
};

struct cli_cmd all_ms_info_cmd = {
	"allmsinfo",
	1,
	0,
	"Memory Space Info.",
	"{None}\n"
	"Displays info about all memory spaces.\n",
	all_ms_info_cmd_f,
	ATTR_NONE
};

struct cli_cmd all_ms_msub_info_cmd = {
	"msmsub",
	1,
	0,
	"Memory Space & Subspace Info.",
	"{None}\n"
	"Displays info about all memory spaces and subspaces.\n",
	all_ms_msub_info_cmd_f,
	ATTR_NONE
};


struct cli_cmd hello_rdaemon_cmd = {
	"hello",
	1,
	0,
	"Display known remote daemons, or attempts to connect to remote daemon.\n",
	"{<destid>}\n"
	"<destid>: Destination ID of remote daemon.\n"
	"Note: If not parms are entered, display available peers.\n",
	hello_rdaemon_cmd_f,
	ATTR_NONE
};

struct cli_cmd list_owners_cmd = {
	"owners",
	1,
	0,
	"Memory space owner information.",
	"{None}\n"
	"Displays info about memory space owners.\n",
	owners_cmd_f,
	ATTR_NONE
};

struct cli_cmd *rdmad_cmds[] = {
	&ibwin_info_cmd,
	&all_ms_info_cmd,
	&all_ms_msub_info_cmd,
	&hello_rdaemon_cmd,
	&list_owners_cmd
};

unsigned rdmad_cmds_size(void)
{
	return sizeof(rdmad_cmds);
}

void custom_quit(struct cli_env *e)
{
	(void)e;
	shutdown(&peer);
}

/**
 * Server for remote debug (remdbg) application.
 */
void *cli_session(void *arg)
{
	int sockfd;
	int portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	int one = 1;
	int session_num = 0;

	/* Check for NULL */
	if (arg == NULL) {
		CRIT("Argument is NULL. Exiting\n");
		pthread_exit(0);
	}

	/* TCP port number */
	portno = *((int *)arg);

	/* Create listen socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		CRIT("ERROR opening socket. Exiting\n");
		pthread_exit(0);
	}

	/* Prepare the family, address, and port */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(portno);

	/* Enable reuse of addresses as long as there is no active accept() */
	setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

	/* For socket to send data in buffer right away */
	setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));

	/* Bind socket to address */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		CRIT("ERROR on binding. Exiting\n");
		close(sockfd);
		pthread_exit(0);
	}

	INFO("RDMAD bound to socket on port number %d\n", portno);

	while (strncmp(buffer, "done", 4)) {
		struct cli_env env;

		/* Initialize the environment */
		env.script = NULL;
		env.fout = NULL;
		bzero(env.output, BUFLEN);
		bzero(env.input, BUFLEN);
		env.DebugLevel = 0;
		env.progressState = 0;
		env.sess_socket = -1;

		/* Set the prompt for the CLI */
		bzero(env.prompt, PROMPTLEN+1);
		strcpy(env.prompt, "RRDMAD> ");

		/* Prepare socket for listening */
		listen(sockfd,5);

		/* Accept connections from remdbg apps */
		clilen = sizeof(cli_addr);
		env.sess_socket = accept(sockfd,
				(struct sockaddr *) &cli_addr,
				&clilen);
		if (env.sess_socket < 0) {
			CRIT("ERROR on accept\n");
			close(sockfd);
			pthread_exit(0);
		}

		/* Start the session */
		INFO("\nStarting session %d\n", session_num);
		cli_terminal(&env);
		INFO("\nFinishing session %d\n", session_num);
		close(env.sess_socket);
		session_num++;
	}

	pthread_exit(0);
} /* cli_session() */

