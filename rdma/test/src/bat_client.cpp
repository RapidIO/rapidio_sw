#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>

#include <cstdlib>
#include <cstdio>
#include <inttypes.h>

#include "librdma.h"
#include "cm_sock.h"
#include "bat_common.h"

#define MS1_SIZE	1024*1024 /* 1MB */
#define MS2_SIZE	512*1024  /* 512 KB */
#define MS1_FLAGS	0
#define MS2_FLAGS	0

#define MAX_NAME	80

#define BAT_SEND(bat_client) if (bat_client->send()) { \
			fprintf(stderr, "bat_client->send() failed\n"); \
			fprintf(fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			return -1; \
		   }

#define BAT_RECEIVE(bat_client) if (bat_client->receive()) { \
			fprintf(stderr, "bat_client->receive() failed\n"); \
			fprintf(fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			return -2; \
		   }

#define BAT_CHECK_RX_TYPE(bm_rx, bat_type) if (bm_rx->type != bat_type) { \
			fprintf(stderr, "Receive message with wrong type 0x%" PRIu64 "\n", bm_rx->type); \
			fprintf(fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			return -3; \
		   }

#define BAT_EXPECT_RET(ret, value, label) if (ret != value) { \
			fprintf(fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			goto label; \
		   }

#define BAT_EXPECT_FAIL(ret) if (ret) { \
				fprintf(fp, "%s PASSED\n", __func__); \
			     } else { \
				fprintf(fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			     }

#define BAT_EXPECT_PASS(ret) if (!ret) { \
				fprintf(fp, "%s PASSED\n", __func__); \
			     } else { \
				fprintf(fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			     }

/* Signal end-of-test to server */
#define BAT_EOT() { \
	bm_first_tx->type = BAT_END; \
	bat_first_client->send(); \
}

using namespace std;

/* Log file, name and handle */
static char log_filename[PATH_MAX];
static FILE *fp;

/* First client, buffers, message structs..etc. */
static cm_client *bat_first_client;
static bat_msg_t *bm_first_tx;
static bat_msg_t *bm_first_rx;

/* Second client, buffers, message structs..etc. */
static cm_client *bat_second_client;
static bat_msg_t *bm_second_tx;
static bat_msg_t *bm_second_rx;

/* Connection information */
static uint32_t destid;
static int first_channel;
static int second_channel;
char first_channel_str[4];	/* 000 to 999 + '\0' */

static unsigned repetitions = 1;	/* Default is once */

static char loc_mso_name[MAX_NAME];
static char loc_ms_name[MAX_NAME];
static char rem_mso_name[MAX_NAME];
static char rem_ms_name1[MAX_NAME];
static char rem_ms_name2[MAX_NAME];
static char rem_ms_name3[MAX_NAME];

/* ------------------------------- Remote Functions -------------------------------*/

/**
 * Create an mso on the server.
 */
static int create_mso_f(cm_client *bat_client,
			bat_msg_t *bm_tx,
			bat_msg_t *bm_rx,
			const char *name,
			mso_h *msoh)
{
	/* Populate the create message with the name */
	bm_tx->type = CREATE_MSO;
	strcpy(bm_tx->create_mso.name , name);

	/* Send message, wait for ACK and verify it is an ACK */
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_rx, CREATE_MSO_ACK);

	/* Return handle. Only valid if ret == 0 */
	*msoh = bm_rx->create_mso_ack.msoh;

	/* Return ret value from the ACK message */
	return bm_rx->create_mso_ack.ret;
} /* create_mso_f() */

/**
 * Destroy an mso on the server.
 */
static int destroy_mso_f(cm_client *bat_client,
			 bat_msg_t *bm_tx,
			 bat_msg_t *bm_rx,
			 mso_h msoh)
{
	bm_tx->type = DESTROY_MSO;
	bm_tx->destroy_mso.msoh = msoh;
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_rx, DESTROY_MSO_ACK);

	return bm_rx->destroy_mso_ack.ret;
} /* destroy_mso_f() */

/**
 * Open an mso on the 'user' app
 */
static int open_mso_f(cm_client *bat_client,
		      bat_msg_t *bm_tx,
		      bat_msg_t *bm_rx,
		      const char *name,
		      mso_h *msoh)
{
	/* Populate the create message with the name */
	bm_tx->type = OPEN_MSO;
	strcpy(bm_tx->open_mso.name , name);

	/* Send message, wait for ACK and verify it is an ACK */
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_rx, OPEN_MSO_ACK);

	/* Return handle. Only valid if ret == 0 */
	*msoh = bm_rx->open_mso_ack.msoh;

	/* Return ret value from the ACK message */
	return bm_rx->open_mso_ack.ret;
} /* open_mso_f() */

/**
 * Close an mso on the server.
 */
static int close_mso_f(cm_client *bat_client,
		       bat_msg_t *bm_tx,
		       bat_msg_t *bm_rx,
		       mso_h msoh)
{
	bm_tx->type = CLOSE_MSO;
	bm_tx->close_mso.msoh = msoh;
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_rx, CLOSE_MSO_ACK);

	return bm_rx->destroy_mso_ack.ret;
} /* destroy_mso_f() */

/**
 * Create an ms on the server.
 */
static int create_ms_f(cm_client *bat_client,
		       bat_msg_t *bm_tx,
		       bat_msg_t *bm_rx,
		       const char *name, mso_h msoh, uint32_t req_size,
		       uint32_t flags, ms_h *msh, uint32_t *size)
{
	/* Populate the create ms message */
	bm_tx->type = CREATE_MS;
	strcpy(bm_tx->create_ms.name , name);
	bm_tx->create_ms.msoh =  msoh;
	bm_tx->create_ms.req_size = req_size;
	bm_tx->create_ms.flags = flags;

	/* Send message and wait for ack */
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_rx, CREATE_MS_ACK);

	*msh = bm_rx->create_ms_ack.msh;

	/* Size maybe NULL */
	if (size)
		*size = bm_rx->create_ms_ack.size;

	return bm_rx->create_ms_ack.ret;
} /* create_ms_f() */

/* NOTE: No destroy_ms_f() for now since destroying the mso in turn
 * destroys the ms.
 */

/**
 * Open an ms on the server.
 */
static int open_ms_f(cm_client *bat_client,
		     bat_msg_t *bm_tx,
		     bat_msg_t *bm_rx,
		     const char *name, mso_h msoh,
		     uint32_t flags, uint32_t *size, ms_h *msh)
{
	/* Populate the create ms message */
	bm_tx->type = OPEN_MS;
	strcpy(bm_tx->open_ms.name , name);
	bm_tx->open_ms.msoh =  msoh;
	bm_tx->open_ms.flags = flags;

	/* Send message and wait for ack */
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_rx, OPEN_MS_ACK);

	*msh = bm_rx->open_ms_ack.msh;

	/* Size maybe NULL */
	if (size)
		*size = bm_rx->open_ms_ack.size;

	return bm_rx->open_ms_ack.ret;
} /* open_ms_f() */

/* NOTE: No close_ms_f() for now since destroying the mso in turn
 * closes the ms.
 */

/**
 * Create msub on server/user
 */
int create_msub_f(cm_client *bat_client,
		  bat_msg_t *bm_tx,
		  bat_msg_t *bm_rx,
		  ms_h msh, uint32_t offset, int32_t req_size,
		  uint32_t flags, msub_h *msubh)
{
	/* Populate the create msub message */
	bm_tx->type = CREATE_MSUB;
	bm_tx->create_msub.msh =  msh;
	bm_tx->create_msub.offset = offset;
	bm_tx->create_msub.req_size = req_size;
	bm_tx->create_msub.flags = flags;

	/* Send message and wait for ack */
	BAT_SEND(bat_client);
	BAT_RECEIVE(bat_client);
	BAT_CHECK_RX_TYPE(bm_first_rx, CREATE_MSUB_ACK);

	*msubh = bm_rx->create_msub_ack.msubh;

	return bm_rx->create_msub_ack.ret;

} /* create_msub_f() */

static int accept_ms_f(cm_client *bat_client,
		       bat_msg_t *bm_tx,
		       ms_h server_msh, msub_h server_msubh)
{
	/* Populate the accept on ms message */
	bm_tx->type = ACCEPT_MS;
	bm_tx->accept_ms.server_msh = server_msh;
	bm_tx->accept_ms.server_msubh = server_msubh;
	/* client_msubh, and client_msubh_len are dummy. Don't populate. */

	/* Send message. No ACK excepted since accept_ms_h() is blocking */
	BAT_SEND(bat_client);

	return 0;
} /* accept_ms_f() */

/* ------------------------------- Test Cases -------------------------------*/
static int test_case_a(void)
{
	int	ret;
	mso_h	msoh1;
	mso_h	msoh2;

	/* Create first mso */
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh1);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create second mso and make sure it fails */
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh2);
	BAT_EXPECT_FAIL(ret);

	/* Either way, we must delete the mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msoh1);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_a() */

static int test_case_b(void)
{
	int	ret;

	/* Create mso */
	mso_h	msoh1;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh1);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create first ms */
	ms_h	msh1;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, msoh1, MS1_SIZE, 0, &msh1, NULL);
	BAT_EXPECT_RET(ret, 0, destroy_mso);

	/* Create second ms with same name */
	ms_h	msh2;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, msoh1, MS2_SIZE, 0, &msh2, NULL);

	/* Creating the second ms should fail */
	BAT_EXPECT_FAIL(ret);

destroy_mso:
	/* Either way, we must delete the mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msoh1);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_b() */

static int test_case_c(void)
{
	int	ret;

	/* Create mso */
	mso_h	msoh1;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh1);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create first ms */
	ms_h	msh1;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, msoh1, 1024*1024, 0, &msh1, NULL);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create second ms */
	ms_h	msh2;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name2, msoh1, 1024*1024, 0, &msh2, NULL);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create third ms */
	ms_h	msh3;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name3, msoh1, 1024*1024, 0, &msh3, NULL);
	BAT_EXPECT_PASS(ret);

free_mso:
	/* Delete the mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msoh1);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_c() */

/**
 * Create a number of msubs, some overlapping.
 */
static int test_case_g(void)
{
	int ret;

	/* Create mso */
	mso_h	msoh1;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &msoh1);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create ms */
	ms_h	msh1;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, msoh1, MS1_SIZE, 0, &msh1, NULL);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create 1st msub */
	msub_h  msubh1;
	ret = create_msub_f(bat_first_client,
		 	    bm_first_tx,
			    bm_first_rx,
			    msh1, 0, 4096, 0, &msubh1);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create 2nd msub */
	msub_h  msubh2;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msh1, 4096, 8192, 0, &msubh2);
	BAT_EXPECT_RET(ret, 0, free_mso);

	/* Create 3rd msub overlapping with 2nd msub */
	msub_h  msubh3;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    msh1, 8192, 8192, 0, &msubh3);
	BAT_EXPECT_PASS(ret);

free_mso:
	/* Delete the mso */
	ret = destroy_mso_f(bat_first_client, bm_first_tx, bm_first_rx, msoh1);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return 0;
} /* test_case_g() */

/**
 * Test accept_ms_h()/conn_ms_h()/disc_ms_h()..etc.
 *
 * @ch	if ch is 'h' run test case 'h', else run test case 'i'
 */
static int test_case_h_i(char ch)
{
	int ret;

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name,
			   &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1,
			  server_msoh,
			  MS1_SIZE,
			  0,
			  &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msh, 0, 4096, 0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0, &client_msh,
									NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, 4096, 0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_first_client,
			  bm_first_tx,
			  server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     30);	/* 30 second-timeout */
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	sleep(1);

	/* Test case 'h' disconnects first. Test case 'i' destroys
	 * the ms on the server and processes the incoming destroy message.
	 */
	if (ch == 'h') {
		/* Now disconnect from server */
		ret = rdma_disc_ms_h(server_msh_rb, client_msubh);
		BAT_EXPECT_RET(ret, 0, free_client_mso);
	}


free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	/* If we reach till here without errors, then we have passed */
	BAT_EXPECT_PASS(ret);

	return 0;
} /* test_case_h_i() */

#define DMA_DATA_SIZE	64
#define DMA_DATA_SECTION_SIZE	8

uint8_t dma_data_copy[DMA_DATA_SIZE];

/* Prepare DMA data */
static void prep_dma_data(uint8_t *dma_data)
{
	/* 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 3 3 3 3 3 3 3 3 .. etc. */
	for(unsigned i = 0; i < DMA_DATA_SIZE; i += DMA_DATA_SECTION_SIZE) {
		memset(&dma_data[i], i + 1, DMA_DATA_SECTION_SIZE);
	}
	/* Make a copy of the data in the 'copy' buffer */
	memcpy(dma_data_copy, dma_data, sizeof(dma_data_copy));

} /* Prepare DMA data */

/* dump specified data starting at specified offset */
static void dump_data(uint8_t *dma_data, unsigned offset)
{
	for(unsigned i = offset; i < DMA_DATA_SIZE; i++) {
		printf("0x%02X - ", dma_data[i]);
	}
	printf("\n");
} /* dump_data() */

static int do_dma(msub_h client_msubh,
		  msub_h server_msubh,
		  uint32_t ofs_in_loc_msub,
		  uint32_t ofs_in_rem_msub,
		  rdma_sync_type_t sync_type)
{
	/* Map client msub to virtual memory pointer */
	void *vaddr;
	uint8_t *dma_data;

	int ret = rdma_mmap_msub(client_msubh, &vaddr);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Prep DMA data */
	dma_data = (uint8_t *)vaddr + ofs_in_loc_msub;
	prep_dma_data(dma_data);

	/* Prep RDMA xfer structs */
	struct rdma_xfer_ms_in	 in;
	struct rdma_xfer_ms_out out;
	in.loc_msubh = client_msubh;
	in.loc_offset = ofs_in_loc_msub;
	in.num_bytes = DMA_DATA_SIZE;
	in.rem_msubh = server_msubh;
	in.rem_offset = ofs_in_rem_msub;
	in.priority = 1;
	in.sync_type = sync_type;

	/* Push the RDMA data */
	ret = rdma_push_msub(&in, &out);
	BAT_EXPECT_RET(ret, 0, unmap_msubh);

	/* If async mode, must call rdma_sync_chk_push_pull() */
	if (sync_type == rdma_async_chk) {
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, unmap_msubh);
	}

	/* Flush rdma_data */
	memset(dma_data, 0xDD, DMA_DATA_SIZE);

	/* Pull the DMA data back */
	ret = rdma_pull_msub(&in, &out);
	BAT_EXPECT_RET(ret, 0, unmap_msubh);

	/* If async mode, must call rdma_sync_chk_push_pull() */
	if (sync_type == rdma_async_chk) {
		ret = rdma_sync_chk_push_pull(out.chk_handle, NULL);
		BAT_EXPECT_RET(ret, 0, unmap_msubh);
	}

	/* Dump the data out for debugging */
	dump_data(dma_data, ofs_in_loc_msub);

	/* Now compare recieved data with the copy */
	ret = memcmp(dma_data, dma_data_copy, DMA_DATA_SIZE);
	BAT_EXPECT_PASS(ret);

unmap_msubh:
	ret = rdma_munmap_msub(client_msubh, vaddr);
	BAT_EXPECT_RET(ret, 0, exit);

exit:
	return ret;
} /* do_dma() */

static int test_case_dma(uint32_t loc_msub_ofs_in_ms,
			 uint32_t ofs_in_loc_msub,
			 uint32_t ofs_in_rem_msub,
			 rdma_sync_type_t sync_type)
{
	int ret;
	const unsigned MSUB_SIZE = 4096;

	/* Create server mso */
	mso_h	server_msoh;
	ret = create_mso_f(bat_first_client,
			   bm_first_tx,
			   bm_first_rx,
			   rem_mso_name, &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, server_msoh, MS1_SIZE, 0, &server_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create msub on server */
	msub_h  server_msubh;
	ret = create_msub_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msh, 0, MSUB_SIZE, 0, &server_msubh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client mso */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0, &client_msh,
									NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, loc_msub_ofs_in_ms, MSUB_SIZE,
							      0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Accept on ms on the server */
	ret = accept_ms_f(bat_first_client,
			  bm_first_tx,
			  server_msh, server_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);
	sleep(1);

	/* Connect to server */
	msub_h	server_msubh_rb;
	uint32_t  server_msub_len_rb;
	ms_h	server_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &server_msubh_rb, &server_msub_len_rb,
			     &server_msh_rb,
			     0);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Do the DMA transfer and comparison */
	ret = do_dma(client_msubh,
		     server_msubh_rb,
		     ofs_in_loc_msub,
		     ofs_in_rem_msub,
		     sync_type);
	BAT_EXPECT_RET(ret, 0, disconnect);

disconnect:
	/* Now disconnect from server */
	ret = rdma_disc_ms_h(server_msh_rb, client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_1() */


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
					    channel);
	}
	catch(cm_exception e) {
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

/* NOTE: test_case_6() is BROKEN. Need to review ms names ..etc. */
int test_case_6()
{
	const unsigned MSUB_SIZE = 4096;

	/* First create mso, and ms on server */
	/* Create server mso */
	mso_h	server_msoh;
	int ret = create_mso_f(bat_first_client,
			      bm_first_tx,
			      bm_first_rx,
			      rem_mso_name,
			      &server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);

	/* Create server ms */
	ms_h	server_msh;
	ret = create_ms_f(bat_first_client,
			  bm_first_tx,
			  bm_first_rx,
			  rem_ms_name1, server_msoh, MS1_SIZE, 0, &server_msh, NULL);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* Now connect to the 'user' application on the second channel */
	connect_to_channel(second_channel,
	                   "second_channel",
			   &bat_second_client,
			   &bm_second_tx,
			   &bm_second_rx);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

	/* On the 'user' application, open the mso & ms */
	mso_h	user_msoh;
	ret = open_mso_f(bat_second_client,
			 bm_second_tx,
			 bm_second_rx,
			 rem_mso_name, &user_msoh);
	BAT_EXPECT_RET(ret, 0, free_second_bat_client);

	ms_h	user_msh;
	uint32_t user_msh_size;
	ret = open_ms_f(bat_second_client,
			bm_second_tx,
			bm_second_rx,
			rem_ms_name1, user_msoh, 0, &user_msh_size, &user_msh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	/* On the 'user' create an msub, and then do accept on the ms */
	msub_h  user_msubh;
	ret = create_msub_f(bat_second_client,
			    bm_second_tx,
			    bm_second_rx,
			    user_msh, 0, MSUB_SIZE, 0, &user_msubh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	ret = accept_ms_f(bat_second_client,
			  bm_second_tx,
			  user_msh, user_msubh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);
	sleep(1);

	/* Now create client mso, ms, and msub */
	mso_h	client_msoh;
	ret = rdma_create_mso_h(loc_mso_name, &client_msoh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

	/* Create a client ms */
	ms_h	client_msh;
	ret = rdma_create_ms_h(loc_ms_name, client_msoh, MS2_SIZE, 0,
							&client_msh,NULL);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Create a client msub */
	msub_h	client_msubh;
	ret = rdma_create_msub_h(client_msh, 0, MSUB_SIZE,
							      0, &client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Connect to user */
	msub_h	user_msubh_rb;
	uint32_t  user_msub_len_rb;
	ms_h	user_msh_rb;
	ret = rdma_conn_ms_h(16, destid, rem_ms_name1,
			     client_msubh,
			     &user_msubh_rb, &user_msub_len_rb,
			     &user_msh_rb,
			     0);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

	/* Do the DMA transfer and comparison */
	ret = do_dma(client_msubh,
		     user_msubh_rb,
		     0,
		     0,
		     rdma_sync_chk);
	BAT_EXPECT_RET(ret, 0, disconnect);

disconnect:
	/* Now disconnect from server */
	ret = rdma_disc_ms_h(user_msh_rb, client_msubh);
	BAT_EXPECT_RET(ret, 0, free_client_mso);

free_client_mso:
	/* Delete the client mso */
	ret = rdma_destroy_mso_h(client_msoh);
	BAT_EXPECT_RET(ret, 0, free_user_mso);

free_user_mso:
	ret = close_mso_f(bat_second_client,
		          bm_second_tx,
		          bm_second_rx,
		          user_msoh);
	BAT_EXPECT_RET(ret, 0, free_server_mso);

free_second_bat_client:
	delete bat_second_client;

free_server_mso:
	/* Delete the server mso */
	ret = destroy_mso_f(bat_first_client,
			    bm_first_tx,
			    bm_first_rx,
			    server_msoh);
	BAT_EXPECT_RET(ret, 0, exit);
exit:
	return ret;
} /* test_case_6() */

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
			puts("'a' Cannot create two ms owners with same name");
			puts("'b' Cannot create two memory spaces same name");
			puts("'c' Create 3 1MB memory spaces in ibwin0 and ibwin1");
			puts("'g' Create a number of overlapping msubs");
			puts("'h' Accept/Connect/Disconnect test");
			puts("'i' Accept/Connect/Destroy test");
			puts("'1' Simple DMA transfer - 0 offsets, sync mode");
			puts("'2' As '1' but loc_msub_of_in_ms is 4K");
			puts("'3' As '1' but data offset in loc_msub");
			puts("'4' As '1' but data offset in rem_msub");
			puts("'5' As '1' but async mode");
			puts("'6' Create mso+ms on one, open and DMA on another");
			puts("'z' RUN ALL TESTS");
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
			if (first_channel < 9 || first_channel > 255 ){
				printf("Invalid channel number: %s. ", optarg);
				printf("Enter a value between 9 and 254\n");
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

	int ret = connect_to_channel(first_channel,
			             "first_channel",
				     &bat_first_client,
				     &bm_first_tx,
				     &bm_first_rx);
	if (ret)
		return 1;

	/* Open log file for 'append'. Create if non-existent. */
	fp = fopen(log_filename, "a");
	if (!fp) {
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
	case 'g':
		test_case_g();
		BAT_EOT();
		break;
	case 'h':
		fprintf(fp, "test_caseh ");
	case 'i':
		if (tc == 'i')
			fprintf(fp, "test_casei ");
		test_case_h_i(tc);
		BAT_EOT();
		break;
	case '1':
		fprintf(fp, "test_case%c ", tc);
		test_case_dma(0x00, 0x00, 0x00, rdma_sync_chk);
		BAT_EOT();
		break;
	case '2':
		fprintf(fp, "test_case%c ", tc);
		test_case_dma(4*1024, 0x00, 0x00, rdma_sync_chk);
		BAT_EOT();
		break;
	case '3':
		fprintf(fp, "test_case%c ", tc);
		test_case_dma(0x00, 0x80, 0x00, rdma_sync_chk);
		BAT_EOT();
		break;
	case '4':
		fprintf(fp, "test_case%c ", tc);
		test_case_dma(0x00, 0x00, 0x40, rdma_sync_chk);
		BAT_EOT();
		break;
	case '5':
		fprintf(fp, "test_case%c ", tc);
		test_case_dma(0x00, 0x00, 0x00, rdma_async_chk);
		BAT_EOT();
		break;
	case '6':
		fprintf(fp, "test_case%c ", tc);
		test_case_6();
		BAT_EOT();
		break;
	case 'z':
		for (unsigned i = 0; i < repetitions; i++) {
			test_case_a();
			test_case_b();
			test_case_c();
			test_case_g();
			test_case_h_i('h');
			test_case_h_i('i');
			test_case_dma(0x00, 0x00, 0x00, rdma_sync_chk);
			test_case_dma(4*1024, 0x00, 0x00, rdma_sync_chk);
			test_case_dma(0x00, 0x80, 0x00, rdma_sync_chk);
			test_case_dma(0x00, 0x00, 0x40, rdma_sync_chk);
			test_case_dma(0x00, 0x00, 0x00, rdma_async_chk);
			test_case_b();
			test_case_dma(4*1024, 0x00, 0x00, rdma_sync_chk);
			test_case_h_i('h');
			test_case_dma(0x00, 0x00, 0x00, rdma_async_chk);
			test_case_dma(0x00, 0x80, 0x00, rdma_sync_chk);
			test_case_c();
			test_case_dma(0x00, 0x00, 0x40, rdma_sync_chk);
			test_case_h_i('i');
			test_case_dma(4*1024, 0x00, 0x00, rdma_sync_chk);
			BAT_EOT();
		}
		break;
	default:
		fprintf(stderr, "Invalid test case '%c'\n", tc);
		break;
	}


	delete bat_first_client;

	fclose(fp);
	puts("Goodbye!");
	return 0;
}

