/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/
#include "tx_engine.h"
#include "rdmad_actions.h"
#include "rdma_msg.h"
#include "rdmad_unix_msg.h"
#include "cm_sock.h"
#include "rdmad_srvr_threads.h"

int rdmad_is_alive_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = RDMAD_IS_ALIVE_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no	 = in_msg->seq_no;
	out_msg.rdmad_is_alive_out.dummy = 0x5678;

	tx_eng->send_message(&out_msg);

	return 0;
} /* rdmad_is_alive() */

/**
 * Dispatch function for obtaining mport ID.
 */
int get_mport_id_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = GET_MPORT_ID_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.get_mport_id_out.status =
		rdmad_get_mport_id(&out_msg.get_mport_id_out.mport_id);

	tx_eng->send_message(&out_msg);

	return out_msg.get_mport_id_out.status;
} /* get_mport_id_disp() */

int create_mso_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = CREATE_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.create_mso_out.status =
		rdmad_create_mso(in_msg->create_mso_in.owner_name,
				 &out_msg.create_mso_out.msoid,
				 tx_eng);
	if (out_msg.create_mso_out.status) {
		ERR("Failed in call rdmad_create_mso\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.create_mso_out.status;
} /* create_mso_disp() */

int open_mso_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = OPEN_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no   = in_msg->seq_no;
	out_msg.open_mso_out.status =
		rdmad_open_mso(in_msg->open_mso_in.owner_name,
				&out_msg.open_mso_out.msoid,
				&out_msg.open_mso_out.mso_conn_id,
				tx_eng);

	if (out_msg.open_mso_out.status) {
		ERR("Failed in call rdmad_open_mso\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.open_mso_out.status;
} /* open_mso_disp() */

int close_mso_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = CLOSE_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no   = in_msg->seq_no;
	out_msg.close_mso_out.status =
		rdmad_close_mso(in_msg->close_mso_in.msoid,
				in_msg->close_mso_in.mso_conn_id);

	if (out_msg.close_mso_out.status) {
		ERR("Failed in call rdmad_open_mso\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.close_mso_out.status;
} /* close_mso_disp() */

int destroy_mso_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = DESTROY_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.destroy_mso_out.status =
		rdmad_destroy_mso(in_msg->destroy_mso_in.msoid);
	if (out_msg.destroy_mso_out.status) {
		ERR("Failed in call rdmad_destroy_mso\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.destroy_mso_out.status;
} /* destroy_mso_disp() */


int create_ms_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = CREATE_MS_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.create_ms_out.status =
		rdmad_create_ms(in_msg->create_ms_in.ms_name,
				in_msg->create_ms_in.bytes,
				in_msg->create_ms_in.msoid,
				&out_msg.create_ms_out.msid,
				&out_msg.create_ms_out.phys_addr,
				&out_msg.create_ms_out.rio_addr,
				tx_eng);

	if (out_msg.create_ms_out.status) {
		ERR("Failed in call rdmad_create_ms\n");
	}

	tx_eng->send_message(&out_msg);

	DBG("rc = 0x%X\n", out_msg.create_ms_out.status);
	return out_msg.create_ms_out.status;
} /* create_msg_disp() */

int open_ms_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = OPEN_MS_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.open_ms_out.status =
		rdmad_open_ms(in_msg->open_ms_in.ms_name,
			      &out_msg.open_ms_out.msid,
			      &out_msg.open_ms_out.phys_addr,
			      &out_msg.open_ms_out.rio_addr,
			      &out_msg.open_ms_out.ms_conn_id,
			      &out_msg.open_ms_out.bytes,
			      tx_eng);

	if (out_msg.open_ms_out.status) {
		ERR("Failed in call rdmad_open_ms\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.open_ms_out.status;
} /* open_ms_disp() */

int close_ms_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = CLOSE_MS_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.close_ms_out.status =
		rdmad_close_ms(in_msg->close_ms_in.msid,
			       tx_eng);

	if (out_msg.close_ms_out.status) {
		ERR("Failed in call rdmad_close_ms\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.close_ms_out.status;
} /* close_ms_disp() */

int destroy_ms_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = DESTROY_MS_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.destroy_ms_out.status =
		rdmad_destroy_ms(in_msg->destroy_ms_in.msoid,
			         in_msg->destroy_ms_in.msid);
	if (out_msg.destroy_ms_out.status) {
		ERR("Failed in call rdmad_destroy_ms\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.destroy_ms_out.status;
} /* destroy_ms_disp() */

int create_msub_disp(const unix_msg_t *in_msg,
		     tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = CREATE_MSUB_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.create_msub_out.status =
		rdmad_create_msub(in_msg->create_msub_in.msid,
				  in_msg->create_msub_in.offset,
				  in_msg->create_msub_in.req_bytes,
				  &out_msg.create_msub_out.bytes,
				  &out_msg.create_msub_out.msubid,
				  &out_msg.create_msub_out.rio_addr,
				  &out_msg.create_msub_out.phys_addr,
				  tx_eng);

	if (out_msg.create_msub_out.status) {
		ERR("Failed in call rdmad_create_msub\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.destroy_ms_out.status;
} /* create_msub_disp() */

int destroy_msub_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = DESTROY_MSUB_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.destroy_msub_out.status =
		rdmad_destroy_msub(in_msg->destroy_msub_in.msid,
				   in_msg->destroy_msub_in.msubid);

	if (out_msg.destroy_msub_out.status) {
		ERR("Failed in call rdmad_destroy_msub\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.destroy_msub_out.status;
} /* destroy_msub_disp() */

int get_ibwin_properties_disp(const unix_msg_t *in_msg, tx_engine<unix_server,
				unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = GET_IBWIN_PROPERTIES_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.get_ibwin_properties_out.status =
		rdmad_get_ibwin_properties(
				&out_msg.get_ibwin_properties_out.num_ibwins,
				&out_msg.get_ibwin_properties_out.ibwin_size);
	if (out_msg.get_ibwin_properties_out.status) {
		ERR("Failed in call rdmad_get_ibwin_properties\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.get_ibwin_properties_out.status;
} /* get_ibwin_properties_disp() */

int accept_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = ACCEPT_MS_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.accept_out.status =
		rdmad_accept_ms(in_msg->accept_in.server_msid,
				in_msg->accept_in.server_msubid,
				tx_eng);
	if (out_msg.accept_out.status) {
		ERR("Failed in call rdmad_accept_ms\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.accept_out.status;
} /* accept_disp() */

int undo_accept_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = UNDO_ACCEPT_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.undo_accept_out.status =
		rdmad_undo_accept_ms(in_msg->undo_accept_in.server_msid, tx_eng);
	if (out_msg.undo_accept_out.status) {
		ERR("Failed in call to rdmad_undo_accept()\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.undo_accept_out.status;
} /* undo_accept_disp() */

/**
 * 1. Using client_destid determine the appropriate tx_engine
 * 2. Compose a cm_accept_ms message inside a cm_msg_t (NOT YET).
 * 3. Call the tx_engine from step 1. to send the message.
 */
int connect_ms_resp_disp(const unix_msg_t *in_msg,
		tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;
	auto 	   rc = 0;

	sem_wait(&prov_daemon_info_list_sem);

	try {
		/* Short form */
		const connect_to_ms_resp_input *conn_resp =
				&in_msg->connect_to_ms_resp_in;

		/* Find the right cm_server using the destid */
		cm_server *conn_server;
		auto it = find(begin(prov_daemon_info_list),
				end(prov_daemon_info_list),
				conn_resp->client_destid);
		if (it != end(prov_daemon_info_list)) {
			conn_server = it->conn_disc_server;
		} else {
			CRIT("No server provisioned for destid(0x%X)\n",
					conn_resp->client_destid);
			throw RDMA_REMOTE_UNREACHABLE;
		}

		mspace *ms = the_inbound->get_mspace(conn_resp->server_msid);
		if (ms == nullptr) {
			CRIT("Cannot find memory space msid(0x%X)\n",
					conn_resp->server_msid);
			throw RDMA_INVALID_MS;
		}

		/* Prepare CM_ACCEPT_MS message from CONNECT_MS_RESP params */
		cm_accept_msg	*cmam;
		conn_server->get_send_buffer((void **)&cmam);
		conn_server->flush_send_buffer();

		cmam->type = htobe64(CM_ACCEPT_MS);
		strcpy(cmam->server_ms_name, ms->get_name());
		cmam->server_msid = htobe64(conn_resp->server_msid);
		cmam->server_msubid = htobe64(conn_resp->server_msubid);
		cmam->server_msub_bytes = htobe64(conn_resp->server_msub_bytes);
		cmam->server_rio_addr_len = htobe64(conn_resp->server_rio_addr_len);
		cmam->server_rio_addr_lo = htobe64(conn_resp->server_rio_addr_lo);
		cmam->server_rio_addr_hi = htobe64(conn_resp->server_rio_addr_hi);
		cmam->server_destid_len = htobe64(16);
		cmam->server_destid = htobe64(peer.destid);
		cmam->client_msid = htobe64(conn_resp->client_msid);
		cmam->client_msubid = htobe64(conn_resp->client_msubid);
		cmam->client_to_lib_tx_eng_h = htobe64(conn_resp->client_to_lib_tx_eng_h);

		DBG("cm_accept_msg has server_destid = 0x%X\n",
					be64toh(cmam->server_destid));
		DBG("cm_accept_msg has server_destid_len = 0x%X\n",
					be64toh(cmam->server_destid_len));

		/* Send the CM_ACCEPT_MS message to remote (client) daemon */
		rc = conn_server->send();
		if (rc) {
			ERR("Failed to send CM_ACCEPT_MS\n");
			throw rc;
		}
	} /* try */
	catch(int e) {
		 rc = e;
	} /* catch() */

	out_msg.type	 = CONNECT_MS_RESP_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.connect_to_ms_resp_out.status = rc;
	tx_eng->send_message(&out_msg);

	sem_post(&prov_daemon_info_list_sem);

	return rc;
} /* connect_msg_resp_disp() */

int send_connect_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = SEND_CONNECT_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	const send_connect_input *in = &in_msg->send_connect_in;
	out_msg.send_connect_out.status = rdmad_send_connect(
			in->server_msname,
			in->server_destid,
			in->client_msid,
			in->client_msubid,
			in->client_bytes,
			in->client_rio_addr_len,
			in->client_rio_addr_lo,
			in->client_rio_addr_hi,
			in->seq_num,
			tx_eng);

	if (out_msg.send_connect_out.status) {
		ERR("Failed in call to rdmad_send_connect()\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.send_connect_out.status;
} /* send_connect_disp() */

int undo_connect_disp(const unix_msg_t *in_msg,
			tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = UNDO_CONNECT_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.undo_connect_out.status = rdmad_undo_connect(
					in_msg->undo_connect_in.server_ms_name,
					tx_eng);

	if (out_msg.undo_connect_out.status) {
		ERR("Failed in call to rdmad_undo_connect()\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.undo_connect_out.status;
} /* undo_connect_disp() */

int send_disconnect_disp(const unix_msg_t *in_msg,
			tx_engine<unix_server,unix_msg_t> *tx_eng)
{
	static unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = SEND_DISCONNECT_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	const send_disconnect_input *in = &in_msg->send_disconnect_in;
	out_msg.send_connect_out.status = rdmad_send_disconnect(
			in->server_destid,
			in->server_msid,
			in->client_msubid,
			tx_eng);

	if (out_msg.send_disconnect_out.status) {
		ERR("Failed in call to rdmad_send_disconnect()\n");
	}

	tx_eng->send_message(&out_msg);

	return out_msg.send_disconnect_out.status;
} /* send_disconnect_disp() */

int server_disconnect_ms_disp(const unix_msg_t *in_msg,
				tx_engine<unix_server, unix_msg_t> *tx_eng)
{
	int rc;
	try {
		uint32_t server_msid = in_msg->server_disconnect_ms_in.server_msid;
		mspace *ms = the_inbound->get_mspace(server_msid);

		if (ms == nullptr) {
			ERR("Invalid server_msid(0x%X)\n", server_msid);
			throw RDMA_INVALID_MS;
		}

		/* Disconnect memory space from specified client */
		rc = ms->server_disconnect(
			in_msg->server_disconnect_ms_in.client_msubid,
			in_msg->server_disconnect_ms_in.client_to_lib_tx_eng_h);
		if (rc) {
			ERR("Failed to disconnect MS\n");
			throw rc;
		}

		/**
		 * NOTE: FIXME: TODO:
		 * The above call to ms->server_disconnect does not
		 * block waiting for the CM_FORCE_DISCONNECT_MS_ACK message.
		 * The reason is that we have a thread that is executing
		 * a cm_server->receive(). This can be rectified when we
		 * switch the daemon-to-daemon communications to use
		 * rx_engine and tx_engine. For now the CM_FORCE_DISCONNECT_MS_ACK
		 * is consumed by that thread in rdmad_srvr_threads.cpp and the
		 * SERVER_DISCONNECT_MS_ACK is always sent back to the server
		 * app/library (below).
		 */
		/* Reply to library indicating success */
		static unix_msg_t out_msg;
		out_msg.category = RDMA_LIB_DAEMON_CALL;
		out_msg.type	 = SERVER_DISCONNECT_MS_ACK;
		out_msg.seq_no	 = in_msg->seq_no;
		out_msg.server_disconnect_ms_out.status = rc;

		tx_eng->send_message(&out_msg);
	}
	catch(int e) {
		rc = e;
	}
	return rc;
} /* server_disconnect_ms_disp() */
