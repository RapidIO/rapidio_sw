#include <stdint.h>

#include <cstdio>

#include "rapidio_mport_mgmt.h"

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

void show_help()
{
	puts("rsktc_test -d<destid> -s<socket_number> [-h] [-l<data_length>] [-r<repetitions>] [-t] \n");
	puts("-d<destid>: Specify destination ID of machine running rskts_test. Default is 1234");
	puts("-s<socket_number>: Specity socket number used by rskts_test for listening for connections");
	puts("-h:  This help message.");
	puts("-l<data_length>: Specify length of data to send (0 to 8192). If omitted default is 512 bytes");
	puts("-r<repetitions>: Specify number of repetitions to run for. Default is 1");
	puts("-t:  Use a number of repetitions and varying data lengths (txtest/rxtest) (overrides -r and -l)");
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

	case SIGUSR1:
		return; /* Ignore */

	default:
		printf("UNKNOWN SIGNAL (%d)\n", sig);
		return;
	}

	delete client;
	exit(0);
} /* sig_handler() */

int main(int argc, char *argv[])
{
	char c;
	uint16_t destid = 0xFFFF;
	int socket_number = 1234;
	unsigned repetitions = 1;
	unsigned data_length = 512;
	bool tx_test = false;

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
		show_help();
		exit(1);
	}

	while ((c = getopt(argc, argv, "htd:l:r:s:")) != -1)
		switch (c) {

		case 'd':
			destid = atoi(optarg);
			break;
		case 'h':
			show_help();
			exit(1);
			break;
		case 'l':
			data_length = atoi(optarg);
			break;
		case 'r':
			repetitions = atoi(optarg);
			break;
		case 's':
			socket_number = atoi(optarg);
			break;
		case 't':
			tx_test = true;
			break;
		case '?':
			/* Invalid command line option */
			show_help();
			exit(1);
			break;
		default:
			abort();
		}

	/* Must specify destid */
	if (destid == 0xFFFF) {
		puts("Error. Must specify <destid>");
		show_help();
		exit(1);
	}

	int rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		CRIT("failed in librskt_init, rc = %d\n", rc);
		return 1;
	}


	/* Send data, receive data, and compare */
	for (unsigned i = 0; i < repetitions; i++) {

		/* Create a client and connect to server */
		try {
			client = new rskt_client("client1", data_length, data_length);
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
		DBG("Test data sent to server\n");

		/* Receive data back from server */
		if (client->receive(data_length) < 0) {
			ERR("Failed to receive message\n");
			delete client;
			return 4;
		}
		DBG("Echoed data received from server\n");

		if (memcmp(in_msg, out_msg, data_length)) {
			ERR("Data did not compare. FAILED.\n");
		} else {
			printf("i = %u, ***** Data compares OK *****\n", i);
		}

		/* Disconnect message */
		out_msg[0] = 0xFD;
		if (client->send(data_length)) {
			ERR("Failed to send disconnect message\n");
		}
		puts("Disconnect message sent");

		/* Call destructor to close and destroy socket */
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
