#ifndef BAT_CLIENT_PRIVATE_H
#define BAT_CLIENT_PRIVATE_H

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>

#include "cm_sock.h"
#include "librdma.h"

#define BAT_SEND(bat_client) if (bat_client->send()) { \
			fprintf(stderr, "bat_client->send() failed\n"); \
			fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			return -1; \
		   }

#define BAT_RECEIVE(bat_client) if (bat_client->receive()) { \
			fprintf(stderr, "bat_client->receive() failed\n"); \
			fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			return -2; \
		   }

#define BAT_CHECK_RX_TYPE(bm_rx, bat_type) if (bm_rx->type != bat_type) { \
			fprintf(stderr, "Receive message with wrong type 0x%" PRIu64 "\n", bm_rx->type); \
			fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			return -3; \
		   }

#define BAT_EXPECT_RET(ret, value, label) if (ret != value) { \
			fprintf(log_fp, "%s FAILED, line %d, ret = %d\n", __func__, __LINE__, ret); \
			goto label; \
		   }

#define BAT_EXPECT_FAIL(ret) if (ret) { \
				fprintf(log_fp, "%s PASSED\n", __func__); \
			     } else { \
				fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			     }

#define BAT_EXPECT_PASS(ret) if (!ret) { \
				fprintf(log_fp, "%s PASSED\n", __func__); \
			     } else { \
				fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
			     }

#define LOG(fmt, args...)    fprintf(log_fp, fmt, ## args)

extern FILE *log_fp;

/* First client, buffers, message structs..etc. */
extern cm_client *bat_first_client;
extern bat_msg_t *bm_first_tx;
extern bat_msg_t *bm_first_rx;

/**
 * Create an mso on the server.
 */
int create_mso_f(cm_client *bat_client,
			bat_msg_t *bm_tx,
			bat_msg_t *bm_rx,
			const char *name,
			mso_h *msoh);

/**
 * Destroy an mso on the server.
 */
int destroy_mso_f(cm_client *bat_client,
			 bat_msg_t *bm_tx,
			 bat_msg_t *bm_rx,
			 mso_h msoh);

/**
 * Open an mso on the 'user' app
 */
int open_mso_f(cm_client *bat_client,
		      bat_msg_t *bm_tx,
		      bat_msg_t *bm_rx,
		      const char *name,
		      mso_h *msoh);

/**
 * Close an mso on the server.
 */
int close_mso_f(cm_client *bat_client,
		       bat_msg_t *bm_tx,
		       bat_msg_t *bm_rx,
		       mso_h msoh);

/**
 * Create an ms on the server.
 */
int create_ms_f(cm_client *bat_client,
		       bat_msg_t *bm_tx,
		       bat_msg_t *bm_rx,
		       const char *name, mso_h msoh, uint32_t req_size,
		       uint32_t flags, ms_h *msh, uint32_t *size);

/**
 * Open an ms on the server.
 */
int open_ms_f(cm_client *bat_client,
		     bat_msg_t *bm_tx,
		     bat_msg_t *bm_rx,
		     const char *name, mso_h msoh,
		     uint32_t flags, uint32_t *size, ms_h *msh);

/**
 * Create msub on server/user
 */
int create_msub_f(cm_client *bat_client,
		  bat_msg_t *bm_tx,
		  bat_msg_t *bm_rx,
		  ms_h msh, uint32_t offset, int32_t req_size,
		  uint32_t flags, msub_h *msubh);

int accept_ms_f(cm_client *bat_client,
		       bat_msg_t *bm_tx,
		       ms_h server_msh, msub_h server_msubh);

int kill_remote_app(cm_client *bat_client, bat_msg_t *bm_tx);

int kill_remote_daemon(cm_client *bat_client, bat_msg_t *bm_tx);

#endif

