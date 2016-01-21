#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>

#include <memory>

#include <cstdlib>
#include <cstdio>
#include <inttypes.h>

#include "memory_supp.h"
#include "librdma.h"
#include "bat_common.h"
#include "bat_connection.h"
#include "bat_client_private.h"
#include "bat_client_test_cases.h"

using std::unique_ptr;

/* Signal end-of-test to server */
#define BAT_EOT1() server_conn->send_eot();
#define BAT_EOT2() user_conn->send_eot();

static bool shutting_down = false;

/* Log file, name and handle */
static char log_filename[PATH_MAX];
FILE *log_fp;

static auto num_channels = 0;
static int first_channel;
static unique_ptr<bat_connection>	server_conn_ptr;
bat_connection			*server_conn;
static int second_channel;
static unique_ptr<bat_connection>	user_conn_ptr;
bat_connection			*user_conn;
static uint32_t destid;

static unsigned repetitions = 1;	/* Default is once */

static void show_help(void)
{
	// TODO: mport_id should be a command-line parameter
	puts("bat_client -s<server_channel> -u<user_channel> -d<destid> -t<test_case> -n<repetitions> -o<output-file> [-l] [-h]");
	puts("-s Specify CM channel used by server app (creates/destroys..etc.)");
	puts("-u Specify CM channel used by user app (opens/closes/accepts..etc.)");
	puts("-d Specify RapidIO destination ID for the node running bot the 'server' and 'user'");
	puts("-t Specify which test case to run");
	puts("-n Specify number of repetitions to run the tests for");
	puts("if <test_case> is 'z', all tests are run");
	puts("if <test_case> is 'z', <repetitions> is the number of times the tests are run");
	puts("<repetitions> is ignored for all other cases");
	puts("-l List all test cases");
	puts("-h Help");
} /* show_help() */

int main(int argc, char *argv[])
{
	char tc = 'a';
	int c;

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

			/* Old test cases */
			puts("'t' Accept/Connect/Disconnect test");
			puts("'u' Accept/Connect/Destroy test");
			puts("'v' Accept/Connect then kill remote app");
			puts("'w' Accept/Connect then kill remote daemon");
			puts("'x' Create local mso, die, then try to open");
			puts("'y' Restart daemon and create the same mso");
			puts("'1' Simple DMA transfer - 0 offsets, sync mode");
			puts("'2' As '1' but loc_msub_of_in_ms is 4K");
			puts("'3' As '1' but data offset in loc_msub");
			puts("'4' As '1' but data offset in rem_msub");
			puts("'5' As '1' but async mode");
			puts("'6' Create mso+ms on one, open and DMA on another");
			puts("'z' RUN ALL TESTS (with some exceptions)");
			exit(1);
		}
		show_help();
		exit(1);
	}

	/* Parse command-line parameters */
	if (argc < 4) {
		show_help();
		exit(1);
	}


	while ((c = getopt(argc, argv, "hld:o:s:t:u:n:")) != -1)
		switch (c) {
		case 's':
			first_channel = atoi(optarg);
			num_channels++;
			break;
		case 'u':
			second_channel = atoi(optarg);
			num_channels++;
			break;
		case 'd':
			destid = atoi(optarg);
			break;
		case 'l':
			break;
		case 'o':
			strcpy(log_filename, optarg);
			break;
		case 't':
			tc = optarg[0];
			break;
		case 'h':
			show_help();
			exit(1);
			break;
		case 'n':
			repetitions = atoi(optarg);
			printf("Tests will be run %d times!\n", repetitions);
			break;
		case '?':
			/* Invalid command line option */
			exit(1);
			break;
		default:
			abort();
		}

	/* Connect to 'server' */
	try {
		server_conn_ptr = make_unique<bat_connection>(destid, first_channel,
						"server_conn", &shutting_down);
		server_conn = server_conn_ptr.get();
		if (num_channels == 2) {
			user_conn_ptr = make_unique<bat_connection>(destid, second_channel,
					"user_conn", &shutting_down);
			user_conn = user_conn_ptr.get();
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
		BAT_EOT1();
		break;
	case 'b':
		test_case_b();
		BAT_EOT1();
		break;
	case 'c':
		test_case_c();
		BAT_EOT1();
		break;
	case 'd':
		test_case_d();
		/* Local test. No need for BAT_EOT1 */
		break;
	case 'e':
		test_case_e();
		/* Local test. No need for BAT_EOT1 */
		break;
	case 'f':
		test_case_f();
		/* Local test. No need for BAT_EOT1 */
		break;
	case 'g':
		test_case_g();
		/* Local test. No need for BAT_EOT1 */
		break;

	case 'h':
		test_case_h(destid);
		BAT_EOT1();
		break;

	case 'i':
	case 'j':
	case 'k':
		test_case_i_j_k(tc, destid);
		BAT_EOT1();
		break;

	case 'l':
		test_case_l();
		/* Local test. No need for BAT_EOT1 */
		break;

	case 'm':
		test_case_m(destid);
		BAT_EOT1();
		break;

		/* Old test cases */
	case 't':
	case 'u':
		test_case_t_u(tc, destid);
		BAT_EOT1();
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
		BAT_EOT1();
		break;
	case '2':
		test_case_dma(tc, destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
		BAT_EOT1();
		break;
	case '3':
		test_case_dma(tc, destid, 0x00, 0x80, 0x00, rdma_sync_chk);
		BAT_EOT1();
		break;
	case '4':
		test_case_dma(tc, destid, 0x00, 0x00, 0x40, rdma_sync_chk);
		BAT_EOT1();
		break;
	case '5':
		test_case_dma(tc, destid, 0x00, 0x00, 0x00, rdma_async_chk);
		BAT_EOT1();
		break;
	case '6':
		test_case_6(destid);
		BAT_EOT1();
		break;
	case 'z':
		for (unsigned i = 0; i < repetitions; i++) {
			/* New test cases */
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

			/* Old test cases */
			test_case_t_u('t', destid);
			test_case_t_u('u', destid);

			test_case_dma('1', destid, 0x00, 0x00, 0x00, rdma_sync_chk);
			test_case_dma('2', destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
			test_case_dma('3', destid, 0x00, 0x80, 0x00, rdma_sync_chk);
			test_case_dma('4', destid, 0x00, 0x00, 0x40, rdma_sync_chk);
#if 0
			test_case_dma('5', destid, 0x00, 0x00, 0x00, rdma_async_chk);
#endif
			test_case_b();
			test_case_dma('2', destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
			test_case_t_u('t', destid);
#if 0
			test_case_dma('5', destid, 0x00, 0x00, 0x00, rdma_async_chk);
#endif
			test_case_dma('3', destid, 0x00, 0x80, 0x00, rdma_sync_chk);
			test_case_c();
			test_case_dma('4', destid, 0x00, 0x00, 0x40, rdma_sync_chk);
			test_case_t_u('u', destid);
			test_case_dma('2', destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
			BAT_EOT1();
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

