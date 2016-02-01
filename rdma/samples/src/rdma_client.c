/* RDMA Test App */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

#include "test_macros.h"

#include "librdma.h"

#define MSO_NAME	"client1"
#define MS1_SIZE	1024*1024 /* 1MB */
#define MSUB1_SIZE	4096
#define MS1_FLAGS	0
#define MSUB1_FLAGS	0

uint8_t circ_buf[8][8] ={ {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
			  {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},
			  {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33},
			  {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
			  {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
			  {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66},
			  {0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77},
			  {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88}
			};
#define CIRC_BUF_LEN	8

/**
 * Given two timespec structs, subtract them and return a timespec containing
 * the difference
 *
 * @start     start time
 * @end       end time
 *
 * @returns         difference
 */
struct timespec time_difference( struct timespec start, struct timespec end )
{
	struct timespec temp;

	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
} /* time_difference() */

void test_case1(uint16_t destid,
		uint32_t msub_ofs,
		uint32_t ofs_in_loc_msub,
		uint32_t ofs_in_rem_msub,
		unsigned reps,
		rdma_sync_type_t sync_type)
{
	mso_h 	msoh;
	ms_h	msh1;
	msub_h	msubh1;
	void *vaddr1;
	ms_h	rem_msh;
	msub_h  rem_msubh;
	uint32_t rem_msub_len;
	conn_h	connh;
	unsigned i, j;
	int status;
	struct rdma_xfer_ms_in	 in;
	struct rdma_xfer_ms_out out;
	uint8_t	*p;
	struct timespec before, after, diff;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	/* Create memory space1 */
	status = rdma_create_ms_h("cspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create subspace for writing to / reading from server */
	status = rdma_create_msub_h(msh1, msub_ofs, MSUB1_SIZE, MSUB1_FLAGS, &msubh1);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msh1);

	/* Memory map memory sub-space 1 */
	status = rdma_mmap_msub(msubh1, &vaddr1);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msubh1);

	/* Connect to server */
	clock_gettime(CLOCK_MONOTONIC, &before);
	status = rdma_conn_ms_h(16, destid, "sspace1", msubh1, &connh,
			&rem_msubh, &rem_msub_len, &rem_msh, 0);
	clock_gettime(CLOCK_MONOTONIC, &after);
	CHECK_AND_GOTO(status, "rdma_conn_ms_h", unmap_subspace1);
	diff = time_difference(before, after);
	printf("Time to connect = %ld seconds, %ld nanoseconds\n",
			diff.tv_sec, diff.tv_nsec);

	/* Pause, and get user input to sync with server */
	puts("Press ENTER to send DMA data to server...");
	getchar();

	printf("Sync type is: ");
	switch(sync_type) {
	case rdma_no_wait:
		printf("FAF\n");
		break;
	case rdma_async_chk:
		printf("ASYNC\n");
		break;
	case rdma_sync_chk:
		printf("SYNC\n");
	}

	/* Prepare DMA data */
	p = (uint8_t *)vaddr1;
	for (j = 0; j < reps; j = (j + 1) & (CIRC_BUF_LEN-1)) {
		for (i = 0; i < CIRC_BUF_LEN; i++) {
			p[i] = circ_buf[j][i];
		}

		/* Prepare DMA transfer parameters */
		in.loc_msubh  = msubh1;
		in.loc_offset = ofs_in_loc_msub;
		in.num_bytes  = MSUB1_SIZE;	/* Arbitrary */
		in.rem_msubh  = rem_msubh;
		in.rem_offset = ofs_in_rem_msub;
		in.priority   = 1;
		in.sync_type = sync_type;

		/* Transfer DMA data */
		clock_gettime(CLOCK_MONOTONIC, &before);
		status = rdma_push_msub(&in, &out);
		CHECK_AND_GOTO(status, "rdma_push_msub", disconnect_rem_msh);

		/* If async mode, must call rdma_sync_chk_push_pull() */
		if (sync_type == rdma_async_chk) {
			if (rdma_sync_chk_push_pull(out.chk_handle, NULL)) {
				puts("TEST FAILED in rdma_sync_chk_push_pull after push");
				return;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &after);
		diff = time_difference(before, after);
		printf("Time to push %d bytes = %ld seconds, %ld nanoseconds\n",
				in.num_bytes, diff.tv_sec, diff.tv_nsec);

		/* Pause, and get user input to sync with server */
		puts("Press ENTER to recieve DMA data from server...");
		getchar();

		/* Overwrite data in msubh1 */
		for (i = 0; i < CIRC_BUF_LEN; i++) {
			p[i] = 0x55;
		}

		/* Prepare DMA transfer parameters */
		in.loc_msubh  = msubh1;	/* Destination for read data */
		in.loc_offset = ofs_in_loc_msub;
		in.num_bytes  = MSUB1_SIZE;
		in.rem_msubh  = rem_msubh;
		in.rem_offset = ofs_in_rem_msub;
		in.priority   = 1;
		in.sync_type  = sync_type; /* Blocking */

		/* Read DMA data */
		clock_gettime(CLOCK_MONOTONIC, &before);
		status = rdma_pull_msub(&in, &out);
		CHECK_AND_GOTO(status, "rdma_pull_msub", disconnect_rem_msh);

		/* If async mode, must call rdma_sync_chk_push_pull() */
		if (sync_type == rdma_async_chk) {
			if (rdma_sync_chk_push_pull(out.chk_handle, NULL)) {
				puts("TEST FAILED in rdma_sync_chk_push_pull after pull");
				return;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &after);
		diff = time_difference(before, after);
		printf("Time to pull %d bytes = %ld seconds, %ld nanoseconds\n",
				in.num_bytes, diff.tv_sec, diff.tv_nsec);

		/* Display received data */
		for (i = 0; i < CIRC_BUF_LEN; i++) {
			fprintf(stderr, "0x%02X-", p[i]);
		}
	}

	puts("Press ENTER to exit");
	getchar();

disconnect_rem_msh:
	clock_gettime(CLOCK_MONOTONIC, &before);
	status = rdma_disc_ms_h(rem_msh, msubh1);
	clock_gettime(CLOCK_MONOTONIC, &after);
	diff = time_difference(before, after);
	printf("Send disconnect: %ld seconds, %ld nanoseconds\n",
			diff.tv_sec, diff.tv_nsec);
	CHECK(status, "rdma_disc_ms_h");

unmap_subspace1:
	status = rdma_munmap_msub(msubh1, vaddr1);
	CHECK(status, "rdma_munmap_msub");

destroy_msubh1:
	status = rdma_destroy_msub_h(msh1, msubh1);
	CHECK(status, "rdma_destroy_msub");

destroy_msh1:
	status = rdma_destroy_ms_h(msoh, msh1);
	CHECK(status, "rdma_destroy_ms_h");

destroy_msoh:
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case1() */

void test_case5(uint16_t destid)
{
	mso_h 	msoh;
	ms_h	msh1;
	ms_h	rem_msh;
	msub_h	loc_msubh;
	msub_h	rem_msubh;
	conn_h	connh;
	uint32_t rem_msub_len;
	int	status;

	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	status = rdma_create_ms_h("cspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	status = rdma_create_msub_h(msh1, 0x00, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	puts("Press ENTER to connect to server");
	getchar();

	status = rdma_conn_ms_h(16, destid, "sspace1", loc_msubh,
			&connh, &rem_msubh, &rem_msub_len, &rem_msh, 0);
	CHECK_AND_GOTO(status, "rdma_conn_ms_h", destroy_msoh);
	puts("CONNECTED");

	printf("Press ENTER ** AFTER ** SERVER destroys %s\n", "sspace1");
	getchar();

destroy_msoh:
	status = rdma_destroy_mso_h(msoh);
	CHECK(status, "rdma_destroy_mso_h");
} /* test_case5() */

void test_case_h(uint16_t destid)
{
	mso_h 	msoh;
	ms_h	msh1;
	ms_h	rem_msh;
	msub_h	loc_msubh;
	msub_h	rem_msubh;
	conn_h	connh;
	uint32_t rem_msub_len;
	int	status;

	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	status = rdma_create_ms_h("cspace1", msoh, MS1_SIZE, MS1_FLAGS, &msh1, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", exit);

	status = rdma_create_msub_h(msh1, 0x00, 4096, 0, &loc_msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", exit);

	puts("Press ENTER to connect to server");
	getchar();

	status = rdma_conn_ms_h(16, destid, "sspace1", loc_msubh,
			&connh, &rem_msubh, &rem_msub_len, &rem_msh, 0);
	CHECK_AND_GOTO(status, "rdma_conn_ms_h", exit);
	puts("CONNECTED");

	puts("Shutdown the computer running the server. Press ENTER when done");
	getchar();

	status = rdma_disc_ms_h(rem_msh, loc_msubh);
exit:
	return;
} /* test_case_h() */

void sig_handler(int sig)
{
	switch (sig) {
	case SIGQUIT:	/* ctrl-\ */
	case SIGINT:	/* ctrl-c */
	case SIGABRT:	/* abort() */
	case SIGTERM:	/* kill <pid> */
	break;

	break;

	default:
		puts("UNKNOWN SIGNAL");
	}
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	uint16_t destid = 0xFFFF;

	/* Parse command-line parameters */
	if (argc < 2) {
		puts("rdmad -h -d<destid>");
		exit(1);
	}

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

	while ((c = getopt(argc, argv, "hd:")) != -1)
		switch (c) {
		case 'd':
			destid = atoi(optarg);
		break;
		case 'h':
			puts("rdmad -h -d<destid>");
			exit(1);
		break;
		case '?':
			/* Invalid command line option */
			exit(1);
		break;

		default:
			abort();
		}

	while (1) {
		char ch;

		puts("Select test case:");
		puts("h. Simple connect then disconnect (after remote daemon dies)");
		puts("1 Simple connect/DMA transfer/disconnect");
		puts("2 Simple connect/DMA transfer/disconnect with msub offset in ms");
		puts("3 Simple connect/DMA transfer/disconnect with offset in loc msub");
		puts("4 Simple connect/DMA transfer/disconnect with offset in rem msub");
		puts("5 Connect then wait for server to destroy ms");
		puts("6 Simple connect/DMA transfer/disconnect multi buffer");
		puts("7 Same as 1 but Async mode");
		puts("x Exit RMDA server test app");

		ch = getchar();
		getchar();

		switch (ch) {

		case 'h':
			test_case_h(destid);
		break;

		case '1':
			test_case1(destid, 0, 0, 0, 1, rdma_sync_chk);
		break;

		case '2':
			test_case1(destid, 0x1000, 0, 0, 1, rdma_sync_chk);
		break;

		case '3':
			test_case1(destid, 0x0, 0x50, 0, 1, rdma_sync_chk);
		break;

		case '4':
			test_case1(destid, 0x0, 0x0, 0x50, 1, rdma_sync_chk);
		break;

		case '5':
			test_case5(destid);
		break;

		case '6':
			test_case1(destid, 0, 0, 0, CIRC_BUF_LEN, rdma_sync_chk);
		break;

		case '7':
			test_case1(destid, 0, 0, 0, 1, rdma_async_chk);
		break;

		case 'x':
			exit(0);
		break;

		default:
			printf("Invalid option: %c\n", ch);
		};

	}

	return 0;
}
