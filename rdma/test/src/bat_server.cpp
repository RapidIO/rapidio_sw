#include <signal.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>

#include "tok_parse.h"
#include "librdma.h"
#include "cm_sock.h"
#include "bat_common.h"

#define BAT_SEND()	if (bat_server->send()) { \
				fprintf(stderr, "bat_server->send() failed\n"); \
				goto free_bat_server; \
			}

#define CHECK_BROKEN_PIPE(ret) 	if ((ret) == RDMA_DAEMON_UNREACHABLE) { \
					printf("DAEMON has died/restarted..Exiting\n"); \
					delete bat_server; \
					exit(1); \
				}

/* Internal function used only by test harness. This is why it is NOT defined
 * in librdma.h which is exposed to applications.
 */
#ifdef __cplusplus
extern "C" {
#endif

extern int rdmad_kill_daemon();

#ifdef __cplusplus
}
#endif

typedef std::vector<std::thread> thread_list;

static thread_list	accept_thread_list;

cm_server	*bat_server;
bool shutting_down = false;

void sig_handler(int sig)
{
	shutting_down = true;
	printf("Got sig(%d) ..exiting\n", sig);
	delete bat_server;
	exit(0);
}

void usage(char *program)
{
	printf("%s -c<channel>|-h\n", program);
}

/**
 * Thread function for handling accepts on memory spaces.
 */
void accept_thread_f(uint64_t server_msh, uint64_t server_msubh)
{
	uint32_t dummy_client_msub_len;
	msub_h	 dummy_client_msubh;
	conn_h	 connh;

	rdma_accept_ms_h(server_msh, server_msubh, &connh,
			&dummy_client_msubh, &dummy_client_msub_len, 30);
} /* accept_thread_f() */

int main(int argc, char *argv[])
{
	int c;
	char *program = argv[0];

	uint16_t channel;

	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* Parse command-line parameters */
	if (argc < 2) {
		usage(program);
		exit(EXIT_FAILURE);
	}
	while (-1 != (c = getopt(argc, argv, "hc:"))) {
		switch(c) {
		case 'c':
			if (tok_parse_socket(optarg, &channel, 0)) {
				printf(TOK_ERR_SOCKET_MSG_FMT, "Channel");
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage(program);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			/* Invalid command line option */
			if (isprint(optopt)) {
				printf("Unknown option '-%c\n", optopt);
			}
			usage(program);
			exit(EXIT_FAILURE);
		}
	} /* while */

	while (1) {
		try {
			bat_server = new cm_server("bat_server",
					BAT_MPORT_ID,
					channel,
					&shutting_down);
		}
		catch(exception& e) {
			fprintf(stderr, "bat_server: %s\n", e.what());
			return 1;
		}

		/* Set up buffers for BAT messages */
		void	*buf_rx, *buf_tx;
		bat_server->get_recv_buffer(&buf_rx);
		bat_server->get_send_buffer(&buf_tx);
		bat_msg_t *bm_rx = (bat_msg_t *)buf_rx;
		bat_msg_t *bm_tx = (bat_msg_t *)buf_tx;

		conn_h   connh;	/* LAST connection from an RDMA client */

		puts("Accepting connections...");
		if (bat_server->accept()) {
			fprintf(stderr, "bat_server->accept() failed\n");
			delete bat_server;
			return 2;
		}
		puts("Connected!");

		while (1) {
			/* Fresh start every time */
			bat_server->flush_recv_buffer();
			bat_server->flush_send_buffer();

			/* Receive BAT command from client */
			if (bat_server->receive()) {
				fprintf(stderr, "bat_server->receive() failed\n");
				goto free_bat_server;
			}

			switch (bm_rx->type) {

			case CREATE_MSO:
				bm_tx->create_mso_ack.ret = rdma_create_mso_h(
						bm_rx->create_mso.name,
						&bm_tx->create_mso_ack.msoh);
				CHECK_BROKEN_PIPE(bm_tx->create_mso_ack.ret);
				bm_tx->type = CREATE_MSO_ACK;
				BAT_SEND();
			break;

			case DESTROY_MSO:
				bm_tx->destroy_mso_ack.ret = rdma_destroy_mso_h(
							bm_rx->destroy_mso.msoh);
				CHECK_BROKEN_PIPE(bm_tx->destroy_mso_ack.ret);
				bm_tx->type = DESTROY_MSO_ACK;
				BAT_SEND();
			break;

			case OPEN_MSO:
				bm_tx->open_mso_ack.ret = rdma_open_mso_h(
							bm_rx->open_mso.name,
							&bm_tx->open_mso_ack.msoh);
				CHECK_BROKEN_PIPE(bm_tx->open_mso_ack.ret);
				bm_tx->type = OPEN_MSO_ACK;
				BAT_SEND();
			break;

			case CLOSE_MSO:
				bm_tx->close_mso_ack.ret = rdma_close_mso_h(
							bm_rx->close_mso.msoh);
				CHECK_BROKEN_PIPE(bm_tx->close_mso_ack.ret);
				bm_tx->type = CLOSE_MSO_ACK;
				BAT_SEND();
			break;

			case CREATE_MS:
				bm_tx->create_ms_ack.ret = rdma_create_ms_h(
							bm_rx->create_ms.name,
							bm_rx->create_ms.msoh,
							bm_rx->create_ms.req_size,
							bm_rx->create_ms.flags,
							&bm_tx->create_ms_ack.msh,
							NULL);
				CHECK_BROKEN_PIPE(bm_tx->create_ms_ack.ret);
				bm_tx->type = CREATE_MS_ACK;
				BAT_SEND();
			break;

			case OPEN_MS:
				bm_tx->open_ms_ack.ret = rdma_open_ms_h(
							bm_rx->open_ms.name,
							bm_rx->open_ms.msoh,
							bm_rx->open_ms.flags,
							(uint32_t *)&bm_tx->open_ms_ack.size,
							&bm_tx->open_ms_ack.msh);
				CHECK_BROKEN_PIPE(bm_tx->open_ms_ack.ret);
				bm_tx->type = OPEN_MS_ACK;
				BAT_SEND();
			break;

			case CLOSE_MS:
				bm_tx->close_ms_ack.ret = rdma_close_ms_h(
							bm_rx->close_ms.msoh,
							bm_rx->close_ms.msh);
				CHECK_BROKEN_PIPE(bm_tx->close_ms_ack.ret);
				bm_tx->type = CLOSE_MS_ACK;
				BAT_SEND();
			break;

			case CREATE_MSUB:
				bm_tx->create_msub_ack.ret = rdma_create_msub_h(
							bm_rx->create_msub.msh,
							bm_rx->create_msub.offset,
							bm_rx->create_msub.req_size,
							bm_rx->create_msub.flags,
							&bm_tx->create_msub_ack.msubh);
				CHECK_BROKEN_PIPE(bm_tx->create_msub_ack.ret);
				bm_tx->type = CREATE_MSUB_ACK;
				BAT_SEND();
			break;

			case ACCEPT_MS:
			{
				uint32_t dummy_client_msub_len;
				msub_h	 dummy_client_msubh;
				/* This call is blocking. */
				int ret = rdma_accept_ms_h(bm_rx->accept_ms.server_msh,
							 bm_rx->accept_ms.server_msubh,
							 &connh,
							 &dummy_client_msubh,
							 &dummy_client_msub_len,
							 30);
				printf("connh = %" PRIx64 "\n", connh);
				CHECK_BROKEN_PIPE(ret);
			}
			break;

			case ACCEPT_MS_THREAD:
			{
				/* Create thread for handling accepts */
				auto accept_thread = std::thread(
						&accept_thread_f,
						bm_rx->accept_ms.server_msh,
						bm_rx->accept_ms.server_msubh);
				printf("*** bm_rx->accept_ms>server_msh = %" PRIx64 "\n",
						bm_rx->accept_ms.server_msh);
				/* Store handle so we can join at the end of the test case */
				accept_thread_list.push_back(std::move(accept_thread));
			}
			break;

			case SERVER_DISCONNECT_MS:
			{
				bm_tx->server_disconnect_ms_ack.ret =
					rdma_disc_ms_h(
					connh,	/* Written in ACCEPT_MS */
					bm_rx->server_disconnect_ms.server_msh,
					bm_rx->server_disconnect_ms.client_msubh);
				printf("*** bm_rx->server_disconnect_ms.server_msh = %" PRIx64 "\n",
						bm_rx->server_disconnect_ms.server_msh);
				CHECK_BROKEN_PIPE(bm_tx->server_disconnect_ms_ack.ret);
				bm_tx->type = SERVER_DISCONNECT_MS_ACK;
				BAT_SEND();
			}
			break;

			case KILL_REMOTE_APP:
				puts("App told to die. Committing suicide!");
				sleep(1);
				raise(SIGTERM);	/* Simulate 'kill' */
				break;

			case KILL_REMOTE_DAEMON:
				{
					int ret = rdmad_kill_daemon();
					CHECK_BROKEN_PIPE(ret);
				}
				break;

			case BAT_END:
				/* If there are threads still running,join */
				for_each(begin(accept_thread_list),
					 end(accept_thread_list),
					 [](std::thread& th)
					 {
						th.join();
					 });
				accept_thread_list.clear();
				puts("Test case finished...next test case!");
				goto free_bat_server;

			default:
				fprintf(stderr, "Message type 0x%" PRIu64
						" ignored\n", bm_rx->type);
				break;
			}
		} /* while .. within test case */
free_bat_server:
		delete bat_server;
	} /* while .. all test cases */

	/* Never reached... exit with ctrl-c */
	return 0;
}
