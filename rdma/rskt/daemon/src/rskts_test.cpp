#include <semaphore.h>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <cstring>

#include "ts_vector.h"
#include "rapidio_mport_mgmt.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"
#include "libcli.h"
#include "liblog.h"
#include "rskts_info.h"

#include "rskt_sock.h"


#ifdef __cplusplus
extern "C" {
#endif

struct rskt_ti
{
	rskt_ti(rskt_h accept_socket) : accept_socket(accept_socket)
	{}
	rskt_h accept_socket;
	pthread_t tid;
};

static console_globals	rskts_test_cli;
static bool all_must_die = false;
static int cons_alive;

static rskt_server *prov_server = nullptr;
static ts_vector<pthread_t> worker_threads;

void rskts_test_shutdown(void)
{
	/* Kill all worker threads */
	DBG("Killing %u active worker threads\n", worker_threads.size());
	for (unsigned i = 0; i < worker_threads.size(); i++) {
		pthread_kill(worker_threads[i], SIGUSR1);
		pthread_join(worker_threads[i], NULL);
	}
	worker_threads.clear();

	if (prov_server != nullptr)
		delete prov_server;

	librskt_finish();
	puts("Goodbye!");
}

void sig_handler(int sig)
{
	switch (sig) {

	case SIGQUIT:	/* ctrl-\ */
		puts("SIGQUIT - CTRL-\\ signal");
	break;

	case SIGINT:	/* ctrl-c */
		puts("SIGINT - CTRL-C signal");
	break;

	case SIGABRT:	/* abort() */
		puts("SIGABRT - abort() signal");
	break;

	case SIGTERM:	/* kill <pid> */
		puts("SIGTERM - kill <pid> signal");
	break;

	case SIGSEGV:	/* Segmentation fault */
		puts("SIGSEGV: Segmentation fault");
	break;

	case SIGUSR1:	/* pthread_kill() */
	/* Ignore signal */
	return;

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
		return;
	}

	puts("Shutting down...");
	rskts_test_shutdown();
	puts("Exiting...");
	exit(0);
} /* sig_handler() */

void *rskt_thread_f(void *arg)
{
	if (!arg) {
		pthread_exit(0);
	}

	rskt_ti *ti = (rskt_ti *)arg;

	INFO("Creating other server object...\n");
	rskt_server *other_server;
	try {
		other_server = new rskt_server("other_server",
				ti->accept_socket,
				RSKT_MAX_SEND_BUF_SIZE,
				RSKT_MAX_RECV_BUF_SIZE);
	}
	catch(rskt_exception& e) {
		CRIT(":%s\n", e.err);
		pthread_exit(0);
	}

	while (1) {
		/* Wait for data from clients */
		INFO("Waiting to receive from client...\n");

		int received_len = other_server->receive(RSKT_MAX_RECV_BUF_SIZE);
		if ( received_len < 0) {
			if (errno == ETIMEDOUT) {
				/* It is not an error since the client may not
				 * be sending anymore data. Just go back and
				 * try again.
				 */
				continue;
			} else {
				ERR("Failed to receive, rc = %d\n", received_len);
				goto exit_rskt_thread_f;
			}
		}

		if (received_len > 0) {
			DBG("received_len = %d\n", received_len);

			void *recv_buf;
			other_server->get_recv_buffer(&recv_buf);

			/* Check for the 'disconnect' message */
			if (*(uint8_t *)recv_buf == 0xFD) {
				puts("Disconnect message received. DISCONNECTING");
				goto exit_rskt_thread_f;
			}
			/* Echo data back to client */
			void *send_buf;
			other_server->get_send_buffer(&send_buf);
			memcpy(send_buf, recv_buf, received_len);

			if (other_server->send(received_len) < 0) {
				ERR("Failed to send back\n");
				goto exit_rskt_thread_f;
			}
		}
	}

exit_rskt_thread_f:
//	sleep(1);	/* Keep disconnection thread alive to process disc_ms_h */
	delete other_server;
	worker_threads.remove(ti->tid);
	delete ti;
	pthread_exit(0);
} /* rskt_thread_f() */

int run_server(int socket_number)
{
	int rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		CRIT("failed in librskt_init, rc = %d\n", rc);
		goto exit_run_server;
	}

	try {
		prov_server = new rskt_server("prov_server", socket_number);
	}
	catch(rskt_exception& e) {
		ERR("Failed to create prov_server: %s\n", e.err);
		goto exit_run_server;
	}

	puts("Provisioning server created..");
	rskt_h acc_socket;
	do {
		puts("Accepting connections...");
		if (prov_server->accept(&acc_socket)) {
			ERR("Failed to accept. Dying!\n");
			goto exit_run_server;
		}
		puts("Connected with client");

		/* Create struct for passing info to thread */
		rskt_ti *ti;
		try {
			ti = new rskt_ti(acc_socket);
		}
		catch(...) {
			ERR("Failed to create rskt_ti\n");
			goto exit_run_server;
		}

		/* Create thread for handling further Tx/Rx on the accept socket */
		int ret = pthread_create(&ti->tid,
					 NULL,
					 rskt_thread_f,
					 ti);
		if (ret) {
			puts("Failed to create request thread\n");
			delete ti;
			goto exit_run_server;
		}
		worker_threads.push_back(ti->tid);
		DBG("Now %u threads in action\n", worker_threads.size());
		pthread_join(ti->tid, NULL);
	} while(1);

exit_run_server:
	rskts_test_shutdown();
	return 0;
} /* run_server */



void show_help()
{
	puts("Usage: rskts_test -s<socket_number>\n");
} /* show_help() */

void rskts_console_cleanup(struct cli_env *env);

void set_prompt(struct cli_env *e)
{
        if (e != NULL) {
                strncpy(e->prompt, "RSKTSvr> ", PROMPTLEN);
        };
};

int RSKTShutdownCmd(struct cli_env *env, int argc, char **argv)
{
	/* FIXME: Missing */
	sprintf(env->output, "Shutdown initiated...\n");
	logMsg(env);

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
        uint8_t  number_of_mports = 8;
        uint32_t ep = 0;
        int i;
        int mport_id;
        int ret = 0;

	if (argc) {
                sprintf(env->output,
			"FAILED: Extra parameters ignored: %s\n", argv[0]);
		logMsg(env);
	};

        ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
        if (ret) {
                sprintf(env->output,
			"ERR: riomp_mgmt_get_mport_list() ERR %d\n", ret);
		logMsg(env);
                return 0;
       }

        printf("\nAvailable %d local mport(s):\n", number_of_mports);
        list_ptr = mport_list;
        for (i = 0; i < number_of_mports; i++, list_ptr++) {
                mport_id = *list_ptr >> 16;
                sprintf(env->output, "+++ mport_id: %u dest_id: %u\n",
                                mport_id, *list_ptr & 0xffff);
		logMsg(env);

                /* Display EPs for this MPORT */

                ret = riomp_mgmt_get_ep_list(mport_id, &ep_list,
						&number_of_eps);
                if (ret) {
                	sprintf(env->output,
                        	"ERR: riodp_ep_get_list() ERR %d\n", ret);
			logMsg(env);
                        break;
                }

                sprintf(env->output, "\t%u Endpoints (dest_ID): ",
			number_of_eps);
		logMsg(env);
                for (ep = 0; ep < number_of_eps; ep++) {
                	sprintf(env->output, "%u ", *(ep_list + ep));
			logMsg(env);
		};

                sprintf(env->output, "\n");
		logMsg(env);

                ret = riomp_mgmt_free_ep_list(&ep_list);
                if (ret) {
                	sprintf(env->output,
                        	"ERR: riodp_ep_free_list() ERR %d\n", ret);
			logMsg(env);
		};

        }

	sprintf(env->output, "\n");
	logMsg(env);

        ret = riomp_mgmt_free_mport_list(&mport_list);
        if (ret) {
		sprintf(env->output,
                	"ERR: riodp_ep_free_list() ERR %d\n", ret);
		logMsg(env);
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
	sprintf(env->output, "\nStatus unimplemented.\n");
	logMsg(env);

};

extern struct cli_cmd RSKTSStatus;

int RSKTSStatusCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;
#if 0
	sprintf(env->output, "        Alive Socket # BkLg MP Pse Max Reqs\n");
        logMsg(env);
        sprintf(env->output, "Lib Lp %5d %8d %4d %2d %3d %3d %3d\n",
                        rskts.lib.loop_alive,
                        rskts.lib.portno,
                        rskts.lib.bklg,
                        rskts.lib.mpnum,
                        rskts.lib.pause_reqs,
                        rskts.lib.max_reqs,
                        rskts.lib.num_reqs);

        logMsg(env);
	sprintf(env->output, "Cli Lp %5d %8d                 %3d\n",
			rskts.cli.cli_alive,
			rskts.cli.cli_portno,
			rskts.cli.cli_sess_num);

        logMsg(env);
	sprintf(env->output, "Cons   %5d\n\n", rskts.cli.cons_alive);

        logMsg(env);
#endif
	return 0;

show_help:
	sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
		argv[0]);
        logMsg(env);
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

void bind_server_cmds(void)
{
	all_must_die = 0;
        add_commands_to_cmd_db(sizeof(server_cmds)/sizeof(server_cmds[0]),
				server_cmds);
	librskt_bind_cli_cmds();
	liblog_bind_cli_cmds();

        return;
} /* bind_server_cmds() */

void *console(void *cons_parm)
{
	struct cli_env cons_env;
	int rc;

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

	cli_init_base(rskts_console_cleanup);
	bind_server_cmds();

	splashScreen((char *)"RSKT_Server");

	cons_alive = 1;

	rc = cli_terminal(&cons_env);

	/* FIXME: shutdown function here */

	if (NULL == cons_parm)
		cons_parm = malloc(sizeof(int));
	*(int *)(cons_parm) = rc;
	printf("\nConsole EXITING\n");
	cons_alive = 0;
	pthread_exit(cons_parm);
} /* console() */

void *cli_session( void *sock_num )
{
	char buffer[256];
	int one = 1;
	struct console_globals *cli = &rskts_test_cli;

	cli->cli_portno = *(int *)(sock_num);

	free(sock_num);

	cli->cli_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (cli->cli_fd < 0) {
        	perror("RSKTD Remote CLI ERROR opening socket");
		goto fail;
	}
	bzero((char *) &cli->cli_addr, sizeof(cli->cli_addr));
	cli->cli_addr.sin_family = AF_INET;
	cli->cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	cli->cli_addr.sin_port = htons(cli->cli_portno);
	setsockopt (cli->cli_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
	setsockopt (cli->cli_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));
	if (bind(cli->cli_fd, (struct sockaddr *) &cli->cli_addr,
						sizeof(cli->cli_addr)) < 0) {
        	perror("\nRSKTD Remote CLI ERROR on binding");
		goto fail;
	}

	printf("\nRSKTD Remote CLI bound to socket %d\n", cli->cli_portno);

	sem_post(&cli->cons_owner);
	cli->cli_alive = 1;
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

		listen(cli->cli_fd,5);
		cli->sess_addr_len = sizeof(cli->sess_addr);
		env.sess_socket = -1;
		cli->cli_sess_fd = accept(cli->cli_fd,
				(struct sockaddr *) &cli->sess_addr,
				&cli->sess_addr_len);
		if (cli->cli_sess_fd < 0) {
			perror("ERROR on accept");
			goto fail;
		};
		env.sess_socket = cli->cli_sess_fd;
		printf("\nRSKTD Starting session %d\n", cli->cli_sess_num);
		cli_terminal( &env );
		printf("\nRSKTD Finishing session %d\n", cli->cli_sess_num);
		close(cli->cli_sess_fd);
		cli->cli_sess_fd = -1;
		cli->cli_sess_num++;
	};
fail:
	cli->cli_alive = 0;
	printf("\nRSKTD REMOTE CLI Thread Exiting\n");

	if (cli->cli_sess_fd > 0) {
		close(cli->cli_sess_fd);
		cli->cli_sess_fd = 0;
	};
     	if (cli->cli_fd > 0) {
     		close(cli->cli_fd);
		cli->cli_fd = 0;
	};

	sock_num = malloc(sizeof(int));
	*(int *)(sock_num) = cli->cli_portno;
	pthread_exit(sock_num);
}

void rskts_console_cleanup(struct cli_env *env)
{
	(void)env;
	/* FIXME: Call shutdown function here for server */
};

void spawn_threads()
{
        int  cli_ret, console_ret = 0, rc;
        int *pass_sock_num, *pass_console_ret;

        all_must_die = 0;
        sem_init(&rskts_test_cli.cons_owner, 0, 0);

        rskts_test_cli.cli_portno = 0;
        rskts_test_cli.cli_alive = 0;
        rskts_test_cli.cons_alive = 0;

        /* Prepare and start console thread */
	pass_console_ret = (int *)(malloc(sizeof(int)));
	*pass_console_ret = 0;
	console_ret = pthread_create( &rskts_test_cli.cons_thread, NULL,
			console, (void *)(pass_console_ret));
	if(console_ret) {
		printf("\nError console_thread rc: %d\n", console_ret);
		exit(EXIT_FAILURE);
	};

        /* Start cli_session_thread, enabling remote debug over Ethernet */
        pass_sock_num = (int *)(malloc(sizeof(int)));
        *pass_sock_num = 1221;

        cli_ret = pthread_create( &rskts_test_cli.cli_thread, NULL, cli_session,
                                (void *)(pass_sock_num));
        if(cli_ret) {
                fprintf(stderr, "Error - cli_session_thread rc: %d\n",cli_ret);
                exit(EXIT_FAILURE);
        }
#if 0	/* FIXME: Do we need this? */
        librskt_test_init(rskts.ctrls.test);
        rc = librskt_init(rskts.ctrls.rsktlib_portno, rskts.ctrls.rsktlib_mp);
	if (rc) {
        	fprintf(stderr, "Error - libskt_init rc = %d, errno = %d:%s\n",
                        rc, errno, strerror(errno));
                exit(EXIT_FAILURE);
	};
#endif
}
int main(int argc, char *argv[])
{
	char c;
	int socket_number = -1;

	/* Register signal handler */
	struct sigaction sig_action;
	sig_action.sa_handler = sig_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGQUIT, &sig_action, NULL);
	sigaction(SIGABRT, &sig_action, NULL);
	sigaction(SIGUSR1, &sig_action, NULL);
	sigaction(SIGSEGV, &sig_action, NULL);

	/* Must specify at least 1 argument (the socket number) */
	if (argc < 2) {
		puts("Insufficient arguments. Must specify -s<socket_number>");
		show_help();
		exit(1);
	}

	while ((c = getopt(argc, argv, "hs:")) != -1)
		switch (c) {

		case 'h':
			show_help();
			exit(1);
			break;
		case 's':
			socket_number = atoi(optarg);
			break;
		case '?':
			/* Invalid command line option */
			show_help();
			exit(1);
			break;
		default:
			abort();
		}

	if (socket_number == -1) {
		puts("Error. Must specify -s<socket_number>");
		show_help();
		exit(1);
	}

	/* Console & CLI threads */
	spawn_threads();

	return run_server(socket_number);
}

#ifdef __cplusplus
}
#endif
