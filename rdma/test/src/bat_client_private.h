#ifndef BAT_CLIENT_PRIVATE_H
#define BAT_CLIENT_PRIVATE_H

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cinttypes>

#include "cm_sock.h"
#include "librdma.h"
#include "bat_connection.h"


#define BAT_EXPECT_RET(ret, value, label) if (ret != value) { \
			fprintf(log_fp, "%s FAILED, line %d, ret = 0x%X\n", __func__, __LINE__, ret); \
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

#define BAT_EXPECT(cond, label) if (!(cond)) { \
				fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
				goto label; \
			     }

#define BAT_EXPECT_NOT(cond, label) if (cond) { \
				fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__); \
				goto label; \
			     }

#define LOG(fmt, args...)    fprintf(log_fp, fmt, ## args)

extern FILE *log_fp;

extern bat_connection *server_conn;
extern bat_connection *user_conn;

/* Functions in LIBRDMA but NOT published in LIBRDMA.H since they are
 * for testing only. */
extern "C" int rdmad_kill_daemon();
extern "C" int rdma_get_ibwin_properties(unsigned *num_ibwins,
					 uint32_t *ibwin_size);
extern "C" int rdma_get_msh_properties(ms_h msh,
				       uint64_t *rio_addr,
				       uint32_t *bytes);

/**
 * Create an mso on the server.
 */
int create_mso_f(bat_connection *bat_conn, const char *name, mso_h *msoh);

/**
 * Destroy an mso on the server.
 */
int destroy_mso_f(bat_connection *bat_conn, mso_h msoh);

/**
 * Open an mso on the 'user' app
 */
int open_mso_f(bat_connection *bat_conn, const char *name, mso_h *msoh);

/**
 * Close an mso on the server.
 */
int close_mso_f(bat_connection *bat_conn, mso_h msoh);

/**
 * Create an ms on the server.
 */
int create_ms_f(bat_connection *bat_conn, const char *name, mso_h msoh,
		uint32_t req_size, uint32_t flags, ms_h *msh, uint32_t *size);

/**
 * Open an ms on the server.
 */
int open_ms_f(bat_connection *bat_conn, const char *name, mso_h msoh,
				uint32_t flags, uint32_t *size, ms_h *msh);

/**
 * Create msub on server/user
 */
int create_msub_f(bat_connection *bat_conn, ms_h msh, uint32_t offset,
			int32_t req_size, uint32_t flags, msub_h *msubh);

int accept_ms_f(bat_connection *bat_conn, ms_h server_msh, msub_h server_msubh);

int accept_ms_thread_f(bat_connection *bat_conn, ms_h server_msh,
							msub_h server_msubh);

int kill_remote_app(bat_connection *bat_conn);

int kill_remote_daemon(bat_connection *bat_conn);

#endif

