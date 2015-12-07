#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>

#include <cstdlib>
#include <cstdio>
#include <inttypes.h>

#include "librdma.h"
#include "cm_sock.h"
#include "bat_common.h"
#include "bat_client_private.h"
#include "bat_client_test_cases.h"


/* Signal end-of-test to server */
#define BAT_EOT() { \
	bm_first_tx->type = BAT_END; \
	bat_first_client->send(); \
}

static bool shutting_down = false;

/* Log file, name and handle */
static char log_filename[PATH_MAX];
FILE *log_fp;

/* First client, buffers, message structs..etc. */
cm_client *bat_first_client;
bat_msg_t *bm_first_tx;
bat_msg_t *bm_first_rx;

/* Second client, buffers, message structs..etc. */
/* FIXME: Probably not needed. We can spawn off users from the server */
#if 0
static cm_client *bat_second_client;
static bat_msg_t *bm_second_tx;
static bat_msg_t *bm_second_rx;
#endif
/* Connection information */
static uint32_t destid;
static int first_channel;
static int second_channel;
char first_channel_str[5];	/* 0001 to 9999 + '\0' */

static unsigned repetitions = 1;	/* Default is once */

static void show_help(void)
{
	// TODO: mport_id should be a command-line parameter
	puts("bat_client -c<channel> -d<destid> -t<test_case> -n<repetitions> -o<output-file> [-l] [-h]");
	puts("-l List all test cases");
	puts("-h Help");
	puts("if <test_case> is 'z', all tests are run");
	puts("if <test_case> is 'z', <repetitions> is the number of times the tests are run");
	puts("<repetitions> is ignored for all other cases");
}

int connect_to_channel(int channel,
		       const char *name,
		       cm_client **bat_client,
		       bat_msg_t **bm_tx,
		       bat_msg_t **bm_rx)
{
	void *buf_rx, *buf_tx;

	printf("%s: Creating client on channel %d\n", __func__, channel);
	try {
		*bat_client = new cm_client(name,
					    BAT_MPORT_ID,
					    BAT_MBOX_ID,
					    channel,
					    &shutting_down);
	}
	catch(cm_exception& e) {
		fprintf(stderr, "%s: %s\n", name, e.err);
		return -1;
	}

	/* Set up buffers for BAT messages */
	(*bat_client)->get_recv_buffer(&buf_rx);
	(*bat_client)->get_send_buffer(&buf_tx);
	*bm_rx = (bat_msg_t *)buf_rx;
	*bm_tx = (bat_msg_t *)buf_tx;

	if ((*bat_client)->connect(destid)) {
		fprintf(stderr, "bat_client->connect() failed\n");
		delete *bat_client;
		return -2;
	}
	printf("Connected to channel %d\n", channel);

	return 0;
} /* connect_to_channel() */

void init_names(void)
{
	/* For the local names, they are not shared so any randomness
	 * would work. Let's use the PID */
	int my_pid;
	char pid_str[10];

	memset(pid_str, 0, sizeof(pid_str));

	my_pid = getpid();
	sprintf(pid_str, "%d", my_pid);

	/* Local names */
	strcpy(loc_mso_name, "MSO_NAME");
	strcat(loc_mso_name, pid_str);
	strcpy(loc_ms_name, "MS_NAME");
	strcat(loc_ms_name, pid_str);

	/* For remote names we append the channel number */
	strcpy(rem_mso_name, "MSO_NAME");
	strcat(rem_mso_name, first_channel_str);
	strcpy(rem_ms_name1, "MS_NAME1");
	strcat(rem_ms_name1, first_channel_str);
	strcpy(rem_ms_name2, "MS_NAME2");
	strcat(rem_ms_name2, first_channel_str);
	strcpy(rem_ms_name3, "MS_NAME3");
	strcat(rem_ms_name3, first_channel_str);
} /* init_names() */

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


	while ((c = getopt(argc, argv, "hlc:d:o:t:n:")) != -1)
		switch (c) {
		case 'c':
			first_channel = atoi(optarg);
			if (strlen(optarg) <= (sizeof(first_channel_str)-1)) {
				strcpy(first_channel_str, optarg);
			}
			if (first_channel < 1 || first_channel > 9999 ){
				printf("Invalid channel number: %s. ", optarg);
				printf("Enter a value between 1 and 9999\n");
				exit(1);
			}
			second_channel = first_channel + 1;
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

	int ret = connect_to_channel(first_channel, "first_channel",
				&bat_first_client, &bm_first_tx, &bm_first_rx);
	if (ret)
		return 1;

	/* Open log file for 'append'. Create if non-existent. */
	log_fp = fopen(log_filename, "a");
	if (!log_fp) {
		fprintf(stderr, "Failed to open log file '%s'\n", log_filename);
		return 2;
	}

	/* Prep the memory space and owner names */
	init_names();

	switch(tc) {

	case 'a':
		test_case_a();
		BAT_EOT();
		break;
	case 'b':
		test_case_b();
		BAT_EOT();
		break;
	case 'c':
		test_case_c();
		BAT_EOT();
		break;
	case 'd':
		test_case_d();
		/* Local test. No need for BAT_EOT */
		break;
	case 'e':
		test_case_e();
		/* Local test. No need for BAT_EOT */
		break;
	case 'f':
		test_case_f();
		/* Local test. No need for BAT_EOT */
		break;
	case 'g':
		test_case_g();
		/* Local test. No need for BAT_EOT */
		break;

		/* Old test cases */
	case 't':
	case 'u':
		test_case_t_u(tc, destid);
		BAT_EOT();
		break;
	case 'v':
	case 'w':
		test_case_v_w(tc, destid);
		break;
	case 'x':
		test_case_x();
		/* No BAT_EOT(). This is a local test */
		break;
	case 'y':
		test_case_y();
		/* No BAT_EOT(). This is a local test */
		break;
	case '1':
		test_case_dma(tc, destid, 0x00, 0x00, 0x00, rdma_sync_chk);
		BAT_EOT();
		break;
	case '2':
		test_case_dma(tc, destid, 4*1024, 0x00, 0x00, rdma_sync_chk);
		BAT_EOT();
		break;
	case '3':
		test_case_dma(tc, destid, 0x00, 0x80, 0x00, rdma_sync_chk);
		BAT_EOT();
		break;
	case '4':
		test_case_dma(tc, destid, 0x00, 0x00, 0x40, rdma_sync_chk);
		BAT_EOT();
		break;
	case '5':
		test_case_dma(tc, destid, 0x00, 0x00, 0x00, rdma_async_chk);
		BAT_EOT();
		break;
	case '6':
		test_case_6();
		BAT_EOT();
		break;
	case 'z':
		for (unsigned i = 0; i < repetitions; i++) {
			test_case_a();
			test_case_b();
			test_case_c();
			test_case_d();
			test_case_e();
			test_case_f();
			test_case_g();
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
		}
		break;
	default:
		fprintf(stderr, "Invalid test case '%c'\n", tc);
		break;
	}

	/* For test cases that kill the bat_server, no point in sending BAT_EOT */
	if(tc != 'j') {
		BAT_EOT();
	}

	shutting_down = true;
	delete bat_first_client;

	fclose(log_fp);
	puts("Goodbye!");
	return 0;
}

