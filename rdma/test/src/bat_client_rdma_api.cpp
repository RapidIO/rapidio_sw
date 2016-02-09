#include <stdint.h>

#include "cm_sock.h"

#include "bat_common.h"	/* BAT message format */
#include "bat_client_private.h"
#include "bat_connection.h"

/* ------------------------------- Remote Functions -------------------------------*/

static inline int remote_call(bat_connection *bat_conn)
{
	auto rc = 0;
	if (bat_conn->send()) {
		fprintf(stderr, "bat_conn->send() failed\n");
		fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__);
		rc = -1;
	} else if (bat_conn->receive()) {
		fprintf(stderr, "bat_conn->receive() failed\n");
		fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__);
		rc = -2;
	}

	return rc;
} /* remote_call() */

/**
 * Create an mso on the server.
 */
int create_mso_f(bat_connection *bat_conn,
		 const char *name,
		 mso_h *msoh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	/* Populate the create message with the name */
	bm_tx->type = CREATE_MSO;
	strcpy(bm_tx->create_mso.name , name);

	/* Send message, wait for ACK and verify it is an ACK */
	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "Failed in remote_call()\n");
	} else if ((rc = (int)bm_rx->create_mso_ack.ret)) {
		fprintf(stderr, "%s: create_mso_h returned 0x%X\n", __func__, rc);
	} else if (bm_rx->type != CREATE_MSO_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	} else {
		/* Return handle. Only valid if no errors */
		*msoh = bm_rx->create_mso_ack.msoh;
	}

	return rc;
} /* create_mso_f() */

/**
 * Destroy an mso on the server.
 */
int destroy_mso_f(bat_connection *bat_conn, mso_h msoh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	bm_tx->type = DESTROY_MSO;
	bm_tx->destroy_mso.msoh = msoh;

	/* Send message, wait for ACK and verify it is an ACK */
	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "%s: Failed in remote_call()\n", __func__);
	} else if ((rc = (int)bm_rx->destroy_mso_ack.ret)) {
		fprintf(stderr, "%s: destroy_mso_h returned 0x%X\n", __func__, rc);
	} else if (bm_rx->type != DESTROY_MSO_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	}

	return rc;
} /* destroy_mso_f() */

/**
 * Open an mso on the 'user' app
 */
int open_mso_f(bat_connection *bat_conn,
		      const char *name,
		      mso_h *msoh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	bm_tx->type = OPEN_MSO;
	strcpy(bm_tx->open_mso.name , name);

	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "Failed in remote_call()\n");
	} else if ((rc = (int)bm_rx->open_mso_ack.ret)) {
		fprintf(stderr, "open_mso_h returned 0x%X\n", rc);
	} else if (bm_rx->type != OPEN_MSO_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	} else {
		/* Return handle if no errors */
		*msoh = bm_rx->open_mso_ack.msoh;
	}

	return rc;
} /* open_mso_f() */

/**
 * Close an mso on the server.
 */
int close_mso_f(bat_connection *bat_conn, mso_h msoh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	bm_tx->type = CLOSE_MSO;
	bm_tx->close_mso.msoh = msoh;

	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "Failed in remote_call()\n");
	} else if ((rc = (int)bm_rx->close_mso_ack.ret)) {
		fprintf(stderr, "close_mso_h returned 0x%X\n", rc);
	} else if (bm_rx->type != CLOSE_MSO_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	}
	return rc;
} /* close_mso_f() */

/**
 * Create an ms on the server.
 */
int create_ms_f(bat_connection *bat_conn,
	       const char *name, mso_h msoh, uint32_t req_size,
	       uint32_t flags, ms_h *msh, uint32_t *size)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	/* Populate the create ms message */
	bm_tx->type = CREATE_MS;
	strcpy(bm_tx->create_ms.name , name);
	bm_tx->create_ms.msoh =  msoh;
	bm_tx->create_ms.req_size = req_size;
	bm_tx->create_ms.flags = flags;

	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "Failed in remote_call()\n");
	} else if ((rc = (int)bm_rx->create_ms_ack.ret)) {
		fprintf(stderr, "create_ms_h returned 0x%X\n", rc);
	} else if (bm_rx->type != CREATE_MS_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	} else {
		*msh = bm_rx->create_ms_ack.msh;
		if (size != nullptr)
			*size = bm_rx->create_ms_ack.size;
	}
	return rc;
} /* create_ms_f() */

/* NOTE: No destroy_ms_f() for now since destroying the mso in turn
 * destroys the ms.
 */

/**
 * Open an ms on the server.
 */
int open_ms_f(bat_connection *bat_conn, const char *name, mso_h msoh,
	      uint32_t flags, uint32_t *size, ms_h *msh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	/* Populate the create ms message */
	bm_tx->type = OPEN_MS;
	strcpy(bm_tx->open_ms.name , name);
	bm_tx->open_ms.msoh =  msoh;
	bm_tx->open_ms.flags = flags;

	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "Failed in remote_call()\n");
	} else if ((rc = (int)bm_rx->open_ms_ack.ret)) {
		fprintf(stderr, "open_ms_h returned 0x%X\n", rc);
	} else if (bm_rx->type != OPEN_MS_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	} else {
		*msh = bm_rx->open_ms_ack.msh;
		/* Size maybe NULL */
		if (size)
			*size = bm_rx->open_ms_ack.size;
	}

	return rc;
} /* open_ms_f() */

/* NOTE: No close_ms_f() for now since closing the mso in turn
 * closes the ms.
 */

/**
 * Create msub on server/user
 */
int create_msub_f(bat_connection *bat_conn,
		  ms_h msh, uint32_t offset, int32_t req_size,
		  uint32_t flags, msub_h *msubh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();
	bat_msg_t *bm_rx = bat_conn->get_rx_buf();

	/* Populate the create msub message */
	bm_tx->type = CREATE_MSUB;
	bm_tx->create_msub.msh =  msh;
	bm_tx->create_msub.offset = offset;
	bm_tx->create_msub.req_size = req_size;
	bm_tx->create_msub.flags = flags;

	rc = remote_call(bat_conn);
	if (rc) {
		fprintf(stderr, "Failed in remote_call()\n");
	} else if ((rc = (int)bm_rx->open_ms_ack.ret)) {
		fprintf(stderr, "open_ms_h returned 0x%X\n", rc);
	} else if (bm_rx->type != CREATE_MSUB_ACK) {
		fprintf(stderr, "%d: Received message with wrong type 0x%X\n",
					__LINE__, (uint32_t)bm_rx->type);
		rc = -1;
	} else {
		*msubh = bm_rx->create_msub_ack.msubh;
	}

	return rc;

} /* create_msub_f() */

int accept_ms_f(bat_connection *bat_conn, ms_h server_msh, msub_h server_msubh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();

	/* Populate the accept on ms message */
	bm_tx->type = ACCEPT_MS;
	bm_tx->accept_ms.server_msh = server_msh;
	bm_tx->accept_ms.server_msubh = server_msubh;
	/* client_msubh, and client_msubh_len are dummy. Don't populate. */

	/* Send message. No ACK excepted since accept_ms_h() is blocking */
	if (bat_conn->send()) {
		fprintf(stderr, "bat_conn->send() failed\n");
		fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__);
		rc = -1;
	}
	return rc;
} /* accept_ms_f() */

int accept_ms_thread_f(bat_connection *bat_conn, ms_h server_msh,
						 msub_h server_msubh)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();

	/* Populate the accept on ms message */
	bm_tx->type = ACCEPT_MS_THREAD;
	bm_tx->accept_ms.server_msh = server_msh;
	bm_tx->accept_ms.server_msubh = server_msubh;
	/* client_msubh, and client_msubh_len are dummy. Don't populate. */

	/* Send message. No ACK excepted */
	if (bat_conn->send()) {
		fprintf(stderr, "bat_conn->send() failed\n");
		fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__);
		rc = -1;
	}
	return rc;
} /* accept_ms_thread_f() */

int kill_remote_app(bat_connection *bat_conn)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();

	bm_tx->type = KILL_REMOTE_APP;

	/* Send message. No ACK excepted since remote app will die! */
	if (bat_conn->send()) {
		fprintf(stderr, "bat_conn->send() failed\n");
		fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__);
		rc = -1;
	}
	return rc;
} /* kill_remote_app() */

int kill_remote_daemon(bat_connection *bat_conn)
{
	int rc = 0;

	bat_msg_t *bm_tx = bat_conn->get_tx_buf();

	bm_tx->type = KILL_REMOTE_DAEMON;

	/* Send message. No ACK excepted since remote app will die! */
	if (bat_conn->send()) {
		fprintf(stderr, "bat_conn->send() failed\n");
		fprintf(log_fp, "%s FAILED, line %d\n", __func__, __LINE__);
		rc = -1;
	}
	return rc;
} /* kill_remote_daemon() */
