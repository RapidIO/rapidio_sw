#include <stdint.h>

#include <cstdio>

#include "rapidio_mport_mgmt.h"

#include "librskt_private.h"
#include "librsktd_private.h"
#include "librdma.h"
#include "liblog.h"
#include "rskts_info.h"

#include "rskt_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

void show_help()
{
	puts("rsktc_test -d<destid> -s<socket_number>\n");
}

int main(int argc, char *argv[])
{
	char c;
	uint16_t destid = 0xFFFF;
	int socket_number = 0;

	while ((c = getopt(argc, argv, "hd:s:")) != -1)
		switch (c) {

		case 'd':
			destid = atoi(optarg);
			break;
		case 's':
			socket_number = atoi(optarg);
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
		}

	int rc = librskt_init(DFLT_DMN_LSKT_SKT, 0);
	if (rc) {
		puts("failed in librskt_init");
		return 1;
	}

	rskt_client *client;

	try {
		client = new rskt_client("client1");
	}
	catch(rskt_exception& e) {
		ERR("Failed to create client: %s\n", e.err);
		return 1;
	}
	puts("Client created.");
	printf("Connecting to server on destid(0x%X) on socket %d\n",
			destid, socket_number);
	if (client->connect(destid, socket_number)) {
		ERR("Failed to connect to destid(0x%X) on socket number(%d)\n",
				destid, socket_number);
		return 2;
	}

	char *in_msg;
	char *out_msg;

	client->get_recv_buffer((void **)&in_msg);
	client->get_send_buffer((void **)&out_msg);

	strcpy(out_msg, "My test string");

	if (client->send(strlen(out_msg))) {
		ERR("Failed to send message\n");
		return 3;
	}
	puts("Test message sent to server");

	if (client->receive(32)) {
		ERR("Failed to receive message\n");
		return 4;
	}
	printf("Reply received:  %s\n", in_msg);

}

#ifdef __cplusplus
}
#endif
