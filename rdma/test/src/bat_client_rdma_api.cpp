#include <stdint.h>

#include "cm_sock.h"

#include "bat_common.h"	/* BAT message format */
#include "bat_client_private.h"
/* ------------------------------- Remote Functions -------------------------------*/

/**
 * Create an mso on the server.
 */
int create_mso_f(cm_client *bat_client,
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
int destroy_mso_f(cm_client *bat_client,
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
int open_mso_f(cm_client *bat_client,
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
int close_mso_f(cm_client *bat_client,
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
int create_ms_f(cm_client *bat_client,
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
int open_ms_f(cm_client *bat_client,
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

int accept_ms_f(cm_client *bat_client,
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

int kill_remote_app(cm_client *bat_client, bat_msg_t *bm_tx)
{
	bm_tx->type = KILL_REMOTE_APP;

	/* Send message. No ACK excepted since remote app will die! */
	BAT_SEND(bat_client);

	return 0;
} /* kill_remote_app() */

int kill_remote_daemon(cm_client *bat_client, bat_msg_t *bm_tx)
{
	bm_tx->type = KILL_REMOTE_DAEMON;

	/* Send message. No ACK excepted since remote app will die! */
	BAT_SEND(bat_client);

	return 0;
} /* kill_remote_daemon() */
