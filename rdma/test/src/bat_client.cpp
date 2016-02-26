#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>

#include <memory>
#include <string>
#include <sstream>
#include <vector>

#include <cstdlib>
#include <cstdio>
#include <inttypes.h>

#include "memory_supp.h"
#include "librdma.h"
#include "bat_common.h"
#include "bat_connection.h"
#include "bat_client_private.h"
#include "bat_client_test_cases.h"

using namespace std;

static inline void bat_eot(int num_channels)
{
	for (auto i = 0; i < num_channels; i++) {
		bat_connections[i]->send_eot();
	}
}

/* Static -- local to this module only */
static bool shutting_down = false;
static char log_filename[PATH_MAX];

/* Globals - referred to by the test cases */
FILE *log_fp;	/* Log file */
vector<bat_connection *>	bat_connections; /* Connections to BAT servers */

static void show_help(void)
{
	puts("bat_client -c<first_channel> -n<num of channels> -d<destid> -t<test_case> -o<output-file> [-r<repetitions>] [-l] [-h]");
	puts("-c First CM channel number by server app (creates/destroys..etc.)");
	puts("-n Total number of CM channels used by server+user apps (opens/closes/accepts..etc.)");
	puts("-d RapidIO destination ID for the node running bot the 'server' and 'user'");
	puts("-t Test case to run");
	puts("-r Repetitions to run the tests for (Optional. Default is 1)");
	puts("\t If <test_case> is 'z', all tests are run");
	puts("\t If <test_case> is 'z', <repetitions> is the number of times the tests are run");
	puts("\t <repetitions> is ignored for all other cases");
	puts("-l List all test cases");
	puts("-m mport_id");
	puts("-h Help");
} /* show_help() */

int main(int argc, char *argv[])
{
	auto tc = 'a';
	int c;
	auto first_channel = 2224;
	auto num_channels  = 1;
	auto repetitions   = 1;
	auto mport_id = BAT_MPORT_ID;
	uint32_t destid;
	vector<unique_ptr<bat_connection> > connections;

	/* List and help are special cases */
	if (argc == 2) {
		if (argv[1][1] == 'l') {
			puts("List of test cases:");
			/* New test cases */
			puts("'a' Test duplicate mso creation");
			puts("'b' Test duplicate ms creation");
			puts("'c' Test memory space allocation");
			puts("'d' Test memory subspace allocation");
			puts("'e' Test memory subspace allocation/mapping");
			puts("'f' Test data stored in overlapping msubs");
			puts("'g' Test optimized memory space allocation");
			puts("'h' Repeated connect/disconnect with multiple mem spaces");
			puts("'i' Connect/disconnect with 3 memory spaces");
			puts("'j' Connect/destroy(local first) with 3 memory spaces");
			puts("'k' Connect/destroy(remote first) with 3 memory spaces");
			puts("'l' Test coalescing of random equal memory spaces");
			puts("'m' Test concurrent creation of ms, msub, accept, connect, disconnect");
			puts("'n' Create mso/ms on server, open mso/ms on user, close, then destroy");
			puts("'o' Test server-side disconnection with 0 client msub");
			puts("'p' Connect without Accept returns connection failure (not timeout)");

			puts("'t' Accept/Connect/Disconnect test with 0 client msub");
			puts("'u' Accept/Connect/Destroy test with 0 client msub");
			puts("'v' Accept/Connect then kill remote app");
			puts("'w' Accept/Connect then kill remote daemon");
			puts("'x' Create local mso, die, then try to open");
			puts("'y' Restart daemon and create the same mso");
			puts("'1' Simple DMA transfer - 0 offsets, sync mode");
			puts("'2' As '1' but loc_msub_of_in_ms is 4K");
			puts("'3' As '1' but data offset in loc_msub");
			puts("'4' As '1' but data offset in rem_msub");
			puts("'5' As '1' but async mode");
			puts("'6' As '1' but FAF (rdma_no-wait) mode");
			puts("'7' Create mso+ms on one, open and DMA on another");
			puts("'8' As 1 but use a buffer instead of loc_msub");
			puts("'z' RUN ALL TESTS (with some exceptions)");
			exit(1);
		}
		show_help();
		exit(1);
	}

	/* Parse command-line parameters */
	if (argc < 6) {
		show_help();
		exit(1);
	}

	while ((c = getopt(argc, argv, "hld:o:c:t:n:r:")) != -1)
		switch (c) {
		case 'c':
			first_channel = atoi(optarg);
			break;
		case 'd':
			destid = atoi(optarg);
			break;
		case 'h':
			show_help();
			exit(1);
			break;
		case 'l':
			break;
		case 'm':
			mport_id = atoi(optarg);
			break;
		case 'n':
			num_channels = atoi(optarg);
			break;
		case 'o':
			strcpy(log_filename, optarg);
			break;
		case 'r':
			repetitions = atoi(optarg);
			printf("Tests will be run %d times!\n", repetitions);
			break;
		case 't':	/* test case */
			tc = optarg[0];
			break;
		case '?':
			/* Invalid command line option */
			exit(1);
			break;
		default:
			abort();
		}

	/* Connect to servers */
	try {
		for (auto i = 0 ; i < num_channels; i++ ) {
			stringstream conn_name;
			conn_name << "conn_" << i;

			connections.push_back(make_unique<bat_connection>(
							mport_id,
							destid,
							first_channel + i,
							conn_name.str().c_str(),
							&shutting_down));
			bat_connections.push_back(connections[i].get());
		}
	}
	catch(...) {
		fprintf(stderr, "Failed to connect to CM channel\n");
		return 1;
	}

	/* Open log file for 'append'. Create if non-existent. */
	log_fp = fopen(log_filename, "a");
	if (!log_fp) {
		fprintf(stderr, "Failed to open log file '%s'\n", log_filename);
		return 2;
	}

	switch(tc) {

	case 'a':
		test_case_a();
		bat_eot(1);
		break;
	case 'b':
		test_case_b();
		bat_eot(1);
		break;
	case 'c':
		test_case_c();
		bat_eot(1);
		break;
	case 'd':
		test_case_d();
		/* Local test. No need for bat_eot */
		break;
	case 'e':
		test_case_e();
		/* Local test. No need for bat_eot */
		break;
	case 'f':
		test_case_f();
		/* Local test. No need for bat_eot */
		break;
	case 'g':
		test_case_g();
		/* Local test. No need for bat_eot */
		break;

	case 'h':
		test_case_h(destid);
		bat_eot(1);
		break;

	case 'i':
	case 'j':
	case 'k':
		test_case_i_j_k(tc, destid);
		bat_eot(1);
		break;

	case 'l':
		test_case_l();
		/* Local test. No need for bat_eot */
		break;

	case 'm':
		test_case_m(destid);
		bat_eot(3);
		break;

	case 'n':
		test_case_n();
		bat_eot(num_channels);
		break;
	case 'o':
		test_case_o(destid);
		bat_eot(1);
		break;
	case 'p':
		test_case_p(destid);
		/* Local test. No need for bat_eot */
		break;

	case 't':
	case 'u':
		test_case_t_u(tc, destid);
		bat_eot(1);
		break;
	case 'v':
	case 'w':
		test_case_v_w(tc, destid);
		break;
	case 'x':
		test_case_x();
		/* No BAT_EOT1(). This is a local test */
		break;
	case 'y':
		test_case_y();
		/* No BAT_EOT1(). This is a local test */
		break;
	case '1':
		test_case_dma(tc, destid, 0x00, 0x00, 0x00, rdma_sync_chk);
		bat_eot(1);
		break;
	case '2':
		test_case_dma(tc, destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
		bat_eot(1);
		break;
	case '3':
		test_case_dma(tc, destid, 0x00, 0x80, 0x00, rdma_sync_chk);
		bat_eot(1);
		break;
	case '4':
		test_case_dma(tc, destid, 0x00, 0x00, 0x40, rdma_sync_chk);
		bat_eot(1);
		break;
	case '5':
		test_case_dma(tc, destid, 0x00, 0x00, 0x00, rdma_async_chk);
		bat_eot(1);
		break;
	case '6':
		test_case_dma(tc, destid, 0x00, 0x00, 0x00, rdma_no_wait);
		bat_eot(1);
		break;
	case '7':
		test_case_7(destid);
		bat_eot(2);
		break;
	case '8':
		test_case_dma_buf(tc, destid, 0x00, rdma_sync_chk);
		bat_eot(1);
		break;
	case '9':
		test_case_dma_buf(tc, destid, 0x20, rdma_sync_chk);
		bat_eot(1);
		break;

	case 'A':
		test_case_dma_buf(tc, destid, 0x00, rdma_no_wait);
		bat_eot(1);
		break;
	case 'z':
		for (auto i = 0; i < repetitions; i++) {
			/* Sequential test cases */
			test_case_a();
			test_case_b();
			test_case_c();
			test_case_d();
			test_case_e();
			test_case_f();
			test_case_g();
			test_case_h(destid);
			test_case_i_j_k('i', destid);
			test_case_i_j_k('j', destid);
			test_case_i_j_k('k', destid);
			test_case_l();
			test_case_m(destid);
			test_case_n();
			test_case_o(destid);
			test_case_p(destid);

			test_case_t_u('t', destid);
			test_case_t_u('u', destid);

			test_case_dma('1', destid, 0x00, 0x00, 0x00, rdma_sync_chk);
			test_case_dma('2', destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
			test_case_dma('3', destid, 0x00, 0x80, 0x00, rdma_sync_chk);
			test_case_dma('4', destid, 0x00, 0x00, 0x40, rdma_sync_chk);
			test_case_dma('5', destid, 0x00, 0x00, 0x00, rdma_async_chk);
			test_case_dma('6', destid, 0x00, 0x00, 0x00, rdma_no_wait);
			test_case_7(destid);
			test_case_dma_buf('8', destid, 0x00, rdma_sync_chk);
			test_case_dma_buf('9', destid, 0x20, rdma_sync_chk);
			test_case_dma_buf('A', destid, 0x00, rdma_no_wait);

			/* Randomized test cases */
			test_case_b();
			test_case_dma('2', destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
			test_case_t_u('t', destid);
			test_case_dma('5', destid, 0x00, 0x00, 0x00, rdma_async_chk);
			test_case_dma('3', destid, 0x00, 0x80, 0x00, rdma_sync_chk);
			test_case_c();
			test_case_dma('4', destid, 0x00, 0x00, 0x40, rdma_sync_chk);
			test_case_t_u('u', destid);
			test_case_dma('2', destid, 4*1024, 0x00, 0x00, rdma_sync_chk);

			bat_eot(3);
		}
		break;
	default:
		fprintf(stderr, "Invalid test case '%c'\n", tc);
		break;
	}

	shutting_down = true;
	sleep(1);	/* Let sockets process the 'shutting_down=true' */

	/* Close log file */
	fclose(log_fp);

	puts("Goodbye!");
	return 0;
}

