#include <iostream>
#include <signal.h>
#include <inttypes.h>

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

/*
 * TODO: Multiple threads for receiving client test signaling commands
 * allowing multiple connections to memory spaces */

cm_server	*bat_server;
bool shutting_down = false;

void sig_handler(int sig)
{
	shutting_down = true;
	printf("Got sig(%d) ..exiting\n", sig);
	delete bat_server;
	exit(0);
}

void show_help()
{
	puts("bat_server -c<channel>|-h");
}

int main(int argc, char *argv[])
{
	int channel = BAT_CM_CHANNEL;
	int c;

	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* Parse command-line parameters */
	if (argc < 2) {
		show_help();
		exit(1);
	}
	while ((c = getopt(argc, argv, "hc:")) != -1) {
		switch(c) {
		case 'c':
			channel = atoi(optarg);
			break;
		case 'h':
			show_help();
			exit(1);
			break;
		case '?':
			/* Invalid command line option */
			exit(1);
			break;
		default:
			abort();
		} /* switch */
	} /* while */

	while (1) {
		try {
			bat_server = new cm_server("bat_server",
					BAT_MPORT_ID,
					BAT_MBOX_ID,
					channel,
					&shutting_down);
		}
		catch(cm_exception& e) {
			fprintf(stderr, "bat_server: %s\n", e.err);
			return 1;
		}

		/* Set up buffers for BAT messages */
		void	*buf_rx, *buf_tx;
		bat_server->get_recv_buffer(&buf_rx);
		bat_server->get_send_buffer(&buf_tx);
		bat_msg_t *bm_rx = (bat_msg_t *)buf_rx;
		bat_msg_t *bm_tx = (bat_msg_t *)buf_tx;

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
							 &dummy_client_msubh,
							 &dummy_client_msub_len,
							 30);
					CHECK_BROKEN_PIPE(ret);
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
				puts("Test case finished...next test case!");
				goto free_bat_server;

			default:
				fprintf(stderr, "Message type 0x%" PRIu64 " ignored\n", bm_rx->type);
				break;
			}
		} /* while .. within test case */
free_bat_server:
		delete bat_server;
	} /* while .. all test cases */

	/* Never reached... exit with ctrl-c */
	return 0;
}
