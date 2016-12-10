#include <stdint.h>

#include <cstdio>

#include "rapidio_mport_mgmt.h"

#include "tok_parse.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt.h"
#include "librdma.h"
#include "liblog.h"
#include "rskts_info.h"

#include "rskt_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

static rskt_client *client;

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

	case SIGUSR1:
		return; /* Ignore */

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
		return;
	}

	delete client;
	exit(0);
} /* sig_handler() */

void usage(char *program)
{
	printf("%s -d<destid> -s<socket_number> [-h] [-l<data_length>] [-r<repetitions>] [-t] \n", program);
	puts("-d<destid>: Specify destination ID of machine running rskts_test.");
	puts("-s<socket_number>: Specity socket number used by rskts_test for listening for connections, Default is 1234");
	puts("-h:  This help message.");
	puts("-l<data_length>: Specify length of data to send (0 to 8192). Default is 512 bytes");
	puts("-r<repetitions>: Specify number of repetitions to run for. Default is 1");
	puts("-t:  Use a number of repetitions and varying data lengths (txtest/rxtest) (overrides -r and -l)");
}

int main(int argc, char *argv[])
{
	int c;
	char *program = argv[0];

	// command line parameters
	uint16_t did = 0;
	bool did_set = false;
	uint16_t socket_number = 1234;
	uint32_t repetitions = 1;
	uint32_t data_length = 512;
	bool tx_test = false;
	int rc;


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

	/* Must specify at least 1 argument (the destid) */
	if (argc < 2) {
		puts("Insufficient arguments. Must specify <destid>");
		usage(program);
		exit(1);
	}

	while (-1 != (c = getopt(argc, argv, "htd:l:r:s:")))
		switch (c) {

		case 'd':
			if (tok_parse_did(optarg, &did, 0)) {
				printf(TOK_ERR_DID_MSG_FMT);
				exit (EXIT_FAILURE);
			}
			did_set = true;
			break;
		case 'l':
			if (tok_parse_long(optarg, &data_length, 0, 8192, 0)) {
				printf(TOK_ERR_LONG_MSG_FMT, "Data length", 0, 8192);
				exit (EXIT_FAILURE);
			}
			break;
		case 'r':
			if (tok_parse_l(optarg, &repetitions, 0)) {
				printf(TOK_ERR_L_MSG_FMT, "Number of repetitions");
				exit (EXIT_FAILURE);
			}
			break;
		case 's':
			if (tok_parse_socket(optarg, &socket_number, 0)) {
				printf(TOK_ERR_SOCKET_MSG_FMT, "Socket number");
				exit (EXIT_FAILURE);
			}
			break;
		case 't':
			tx_test = true;
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

	/* Must specify destid */
	if (!did_set ) {
		usage(program);
		exit(EXIT_FAILURE);
	}

	rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		CRIT("failed in librskt_init, rc = %d\n", rc);
		return 1;
	}


	/* Send data, receive data, and compare */
	for (unsigned i = 0; i < repetitions; i++) {

		/* Create a client and connect to server */
		try {
			client = new rskt_client("client1", RSKT_MAX_SEND_BUF_SIZE, RSKT_MAX_RECV_BUF_SIZE);
		}
		catch(rskt_exception& e) {
			ERR("Failed to create client: %s\n", e.err);
			return 1;
		}
		DBG("Client created.\n");
		DBG("Connecting to server on destid(0x%X) on socket %d\n",
			destid, socket_number);
		if (client->connect(destid, socket_number)) {
			ERR("Failed to connect to destid(0x%X) on socket number(%d)\n",
					destid, socket_number);
			return 2;
		} else {
			puts("Successfully connected");
		}

		uint8_t *out_msg;
		uint8_t *in_msg;

		client->get_recv_buffer((void **)&in_msg);
		client->get_send_buffer((void **)&out_msg);

		/* Data to be sent */
		for (unsigned i = 0; i < data_length; i++)
			out_msg[i] = i;

		/* Fill read buffer with 0xAA */
		memset(in_msg, 0xAA, data_length);

		/* Send data to server */
		if (client->send(data_length)) {
			ERR("Failed to send message\n");
			delete client;
			return 3;
		}
		printf("%d byte of test data sent to server\n", data_length);

		/* Receive data back from server */
		rc = client->receive(data_length);
		if (rc < 0) {
			ERR("Failed to receive message\n");
			delete client;
			return 4;
		}
		printf("Received %d bytes back from server\n", rc);
		if (rc != data_length) {
			printf("Expecting %d bytes but received %d bytes\n",
							data_length, rc);
		}
		if (memcmp(in_msg, out_msg, data_length)) {
			ERR("Data did not compare. FAILED.\n");
		} else {
			printf("i = %u, ***** Data compares OK *****\n", i);
		}

		/* Call destructor to close and destroy socket */
		puts("Deleting client object (closing sockets)");
		delete client;
	} /* for() */

	puts("Press ENTER to quit");
	getchar();
	puts("Goodbye!");

	/* Clean up library */
	DBG("Calling librskt_finish()\n");
	librskt_finish();

} /* main() */

#ifdef __cplusplus
}
#endif
