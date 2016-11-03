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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "cm_sock.h"
#include "liblog.h"

#include "rdma_types.h"
#include "rdmad_actions.h"
#include "rdma_msg.h"
#include "rdmad_unix_msg.h"
#include "rdmad_cm.h"
#include "rdmad_srvr_threads.h"
#include "rdmad_clnt_threads.h"
#include "rdmad_peer_utils.h"
#include "rdmad_main.h"
#include "rdmad_tx_engine.h"

/**
 * @brief Handles a HELLO message sent from client daemon and received
 * 	  by server daemon.
 */
void cm_hello_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng)
{
	cm_hello_msg_t *hello_msg = &msg->cm_hello;

	uint32_t remote_destid = htobe64(hello_msg->destid);
	DBG("Received HELLO message from destid(0x%X)\n", remote_destid);

	/* Note that if we have recieved a HELLO message then we have
	 * already created a daemon entry for the remote daemon, except
	 * that the entry didn't contain a destid. So find that entry
	 * and add the 'remote_destid' to it. Failing that, don't
	 * ACK the HELLO as we won't have full connectivity with the remote
	 * host. This should not be a possible scenario.
	 *
	 * Also note that setting the destid also sets the entry to
	 * 'provisioned' so no need to explicitly do that.
	 */
	auto rc = prov_daemon_info_list.set_destid(remote_destid, tx_eng);
	if (rc < 0) {
		CRIT("Failed to set destid for tx_eng (%p)\n", tx_eng);
	} else {
		/* Send HELLO ACK with our own destid */
		auto hello_ack_msg = make_unique<cm_msg_t>();
		hello_ack_msg->type = htobe64(CM_HELLO_ACK);
		hello_ack_msg->category = htobe64(RDMA_REQ_RESP);
		hello_ack_msg->seq_no = htobe64(msg->seq_no);
		hello_ack_msg->cm_hello.destid =
				htobe64(the_inbound->get_peer().destid);

		tx_eng->send_message(move(hello_ack_msg));
		INFO("CM_HELLO_ACK sent to destid(0x%X)\n", remote_destid);
	}
} /* cm_hello_disp() */

void cm_hello_ack_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng)
{
	uint32_t destid = be64toh(msg->cm_hello_ack.destid);
	auto rc = hello_daemon_info_list.set_provisioned(destid, tx_eng);
	if (rc < 0) {
		ERR("Failed to provision destid(0x%X)\n", destid);
	}
} /* cm_hello_ack_disp() */

/**
 * @brief Send indication to remote daemon that the connect request to
 * 	  the specified memory space was declined, most likley since
 * 	  the memory space was no in accept mode in the first place
 */
static void send_accept_nack(cm_msg_t *msg,
			     cm_server_tx_engine *tx_eng)
{
	auto cm_msg = make_unique<cm_msg_t>();

	DBG("Sending CM_ACCEPT_MS_NACK\n");
	/* Prepare negative accept message common fields */
	cm_msg->type = htobe64(CM_ACCEPT_MS);
	cm_msg->category = htobe64(RDMA_REQ_RESP);
	cm_msg->seq_no = msg->seq_no; /* Both are BE */

	/* Populate accept-specific fields */
	cm_accept_ms_msg	*cmnam = &cm_msg->cm_accept_ms;
	cmnam->sub_type = htobe64(CM_ACCEPT_MS_NACK);
	cm_connect_ms_msg	*cm_conn_msg = &msg->cm_connect_ms;
	strcpy(cmnam->server_ms_name, cm_conn_msg->server_msname);
	cmnam->client_to_lib_tx_eng_h = cm_conn_msg->client_to_lib_tx_eng_h;

	/* Send! */
	tx_eng->send_message(move(cm_msg));
} /* send_accept_nack() */

/**
 * @brief Runs on server daemon. Handles incoming CM_CONNECT_MS.
 * 	  If the requested MS was NOT in accept mode, then
 * 	  a NACK is sent back to the client daemon. If all goes well
 * 	  then a CONNECT_MS_REQ is sent to the library thus
 * 	  unblocking the call to rdma_accept_ms_h(). rdma_accept_ms_h()
 * 	  in turn sends a CONNECT_MS_RESP to the server daemon.
 */
void cm_connect_ms_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng)
{
	cm_connect_ms_msg	*conn_msg = &msg->cm_connect_ms;
	HIGH("Received CONNECT_MS for '%s'. Contents:\n",
						conn_msg->server_msname);
	conn_msg->dump();

	/* Find the relevant memory space */
	mspace *ms = the_inbound->get_mspace(
				conn_msg->server_msname);
	if (ms == nullptr) {
		WARN("'%s' not found. Ignore CM_CONNECT_MS\n",
				conn_msg->server_msname);
		send_accept_nack(msg, tx_eng);
		return;
	}
	DBG("mspace '%s' found\n", conn_msg->server_msname);

	/* Find the tx engine to use to relay connect to apps/librdma */
	tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng;
	to_lib_tx_eng = ms->get_accepting_tx_eng();
	if (to_lib_tx_eng == nullptr) {
		WARN("'%s' not accepting by owner or users\n",
					conn_msg->server_msname);
		WARN("Ignoring CM_CONNECT_MS\n");
		send_accept_nack(msg, tx_eng);
		return;
	}
	DBG("Found Tx engine in accepting mode\n");

	/* Send 'connect' Unix message contents to the RDMA library */
	auto in_msg = make_unique<unix_msg_t>();
	connect_to_ms_req_input *cm = &in_msg->connect_to_ms_req;

	in_msg->type = CONNECT_MS_REQ;
	in_msg->category = RDMA_CALL;
	in_msg->seq_no = 0;	/* Important as librdma also expects 0 */
	cm->client_msid = be64toh(conn_msg->client_msid);
	cm->client_msubid = be64toh(conn_msg->client_msubid);
	cm->client_msub_bytes = be64toh(conn_msg->client_bytes);
	cm->client_rio_addr_len = be64toh(conn_msg->client_rio_addr_len);
	cm->client_rio_addr_lo	= be64toh(conn_msg->client_rio_addr_lo);
	cm->client_rio_addr_hi	= be64toh(conn_msg->client_rio_addr_hi);
	cm->client_destid_len = be64toh(conn_msg->client_destid_len);
	cm->client_destid = be64toh(conn_msg->client_destid);
	cm->seq_num = be64toh(conn_msg->seq_num);
	cm->connh = be64toh(conn_msg->connh);
	cm->client_to_lib_tx_eng_h = be64toh(conn_msg->client_to_lib_tx_eng_h);

	to_lib_tx_eng->send_message(move(in_msg));

	DBG("Sent CONNECT_MS_REQ to Server RDMA library. Contents:\n");
	cm->dump();
} /* cm_connect_ms_disp() */

/**
 * @brief Runs on server daemon:
 * 	  - Locates memory space
 * 	  - Executes 'disconnect' method in memory space which removes
 * 	    connection parameters from the memory space AND notifies the
 * 	    library.
 * 	  - Acknowledges disconnection by sending CM_DISCONNECT_ACK
 * 	    to the client daemon.
 */
void cm_disconnect_ms_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng)
{
	cm_disconnect_ms_msg	*disc_msg = &msg->cm_disconnect_ms;

	uint32_t server_msid = static_cast<uint32_t>(be64toh(disc_msg->server_msid));
	HIGH("Received DISCONNECT_MS for msid(0x%X)\n", server_msid);

	/* Find the memory space specified in the CM_DISCONNECT_MS message */
	mspace *ms = the_inbound->get_mspace(server_msid);
	if (ms == nullptr) {
		ERR("Failed to find ms(0x%X). Was it destroyed?\n", server_msid);
		return ;	/* Can't do much without the ms */
	}

	/* Relay disconnection request to the RDMA library */
	auto rc = ms->client_disconnect(
			     be64toh(disc_msg->client_msubid),
			     be64toh(disc_msg->client_to_lib_tx_eng_h));
	if (rc < 0) {
		ERR("Failed to relay disconnect ms('%s') to lib\n",
			ms->get_name());
	} else {
		HIGH("'Disconnect' for ms('%s') relayed to 'server'\n",
			ms->get_name());

		/* Acknowledge disconnection */
		auto out_msg = make_unique<cm_msg_t>();
		out_msg->category = htobe64(RDMA_REQ_RESP);
		out_msg->type	 = htobe64(CM_DISCONNECT_MS_ACK);
		out_msg->seq_no	 = 0;
		cm_disconnect_ms_ack_msg *disc_ack = &out_msg->cm_disconnect_ms_ack;

		/* The rest of the fields are copied from the CM_DISCONNECT_MS
		 * message and are already in Big Endian (network) order. */
		disc_ack->client_msubid = disc_msg->client_msubid;
		disc_ack->client_destid = disc_msg->client_destid;
		disc_ack->client_destid_len = disc_msg->client_destid_len;
		disc_ack->client_to_lib_tx_eng_h = disc_msg->client_to_lib_tx_eng_h;
		disc_ack->server_msid = disc_msg->server_msid;
		tx_eng->send_message(move(out_msg));
	}
} /* cm_disconnect_ms_disp() */

/**
 * @brief Runs on client daemon, and is dispatched when CM_DISCONNECT_MS_ACK
 * 	  is received. Relays disconnection to the library as a DISCONNECT_MS_ACK
 * 	  RDMA_CALL
 */
void cm_disconnect_ms_ack_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng)
{
	(void)tx_eng;

	/* Obtain the to-library Tx engine from the message */
	tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng =
			(tx_engine<unix_server, unix_msg_t>*)
			be64toh(msg->cm_disconnect_ms_ack.client_to_lib_tx_eng_h);

	/* Compose the proper to-library message which unblocks the call
	 * rdma_disc_ms_h() waiting for confirmation of disconnection */
	auto	in_msg = make_unique<unix_msg_t>();
	in_msg->type = DISCONNECT_MS_ACK;
	in_msg->category = RDMA_CALL;
	in_msg->seq_no = 0;
	in_msg->disconnect_from_ms_ack.server_msid =
				be64toh(msg->cm_disconnect_ms_ack.server_msid);
	in_msg->disconnect_from_ms_ack.client_msubid =
				be64toh(msg->cm_disconnect_ms_ack.client_msubid);

	to_lib_tx_eng->send_message(move(in_msg));
} /* cm_disconnect_ms_ack_disp() */


void cm_accept_ms_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng)
{
	(void)tx_eng;
	cm_accept_ms_msg	*cm_accept_ms = &msg->cm_accept_ms;

	HIGH("Received CM_ACCEPT_MS from %s\n",	cm_accept_ms->server_ms_name);

	/* Find the entry matching the memory space and tx_eng
	 * and also make sure it is not already marked as 'connected'.
	 * If not found, there is nothing to do and the CM_ACCEPT_MS
	 * is ignored. */
	lock_guard<mutex> conn_lock(connected_to_ms_info_list_mutex);
	auto it = find_if(begin(connected_to_ms_info_list),
			  end(connected_to_ms_info_list),
		[cm_accept_ms](connected_to_ms_info& info)
		{
			bool match = (info.server_msname ==
					cm_accept_ms->server_ms_name);
			match &= !info.connected;
			match &= (be64toh((uint64_t)info.to_lib_tx_eng)
					==
				  cm_accept_ms->client_to_lib_tx_eng_h);
			return match;
		});
	if (it == end(connected_to_ms_info_list)) {
		WARN("Ignoring CM_ACCEPT_MS from ms('%s')\n",
				cm_accept_ms->server_ms_name);
		return;
	}

	/* If this is a rejection (NACK), then handle it */
	if (be64toh(cm_accept_ms->sub_type) == CM_ACCEPT_MS_NACK) {
		/* Compose an ACCEPT_FROM_MS_REQ message but with a
		 * sub_type of ACCEPT_FROM_MS_REQ_NACK  */
		auto in_msg = make_unique<unix_msg_t>();

		in_msg->category = RDMA_CALL;
		in_msg->type	= ACCEPT_FROM_MS_REQ;
		in_msg->sub_type = ACCEPT_FROM_MS_REQ_NACK;
		/* Send the ACCEPT_FROM_MS_REQ message to the blocked
		 * rdma_conn_ms_h() via the tx engine */
		it->to_lib_tx_eng->send_message(move(in_msg));
		return;
	} else if (be64toh(cm_accept_ms->sub_type) == CM_ACCEPT_MS_ACK) {
		/* Compose the ACCEPT_FROM_MS_REQ that is to be sent
		 * over to the BLOCKED rdma_conn_ms_h(). */
		auto in_msg = make_unique<unix_msg_t>();
		accept_from_ms_req_input *am = /* short form */
					&in_msg->accept_from_ms_req_in;

		in_msg->category = RDMA_CALL;
		in_msg->type	= ACCEPT_FROM_MS_REQ;
		in_msg->sub_type = ACCEPT_FROM_MS_REQ_ACK;
		am->server_msid	= be64toh(cm_accept_ms->server_msid);
		am->server_msubid = be64toh(cm_accept_ms->server_msubid);
		am->server_msub_bytes = be64toh(cm_accept_ms->server_msub_bytes);
		am->server_rio_addr_len = be64toh(cm_accept_ms->server_rio_addr_len);
		am->server_rio_addr_lo = be64toh(cm_accept_ms->server_rio_addr_lo);
		am->server_rio_addr_hi = be64toh(cm_accept_ms->server_rio_addr_hi);
		am->server_destid_len = be64toh(cm_accept_ms->server_destid_len);
		am->server_destid = be64toh(cm_accept_ms->server_destid);

		DBG("Accept: msubid=0x%X msid= 0x%X destid=0x%X destid_len=0x%X, rio=0x%"
									PRIx64 "\n",
					am->server_msubid,
					am->server_msid,
					am->server_destid,
					am->server_destid_len,
					am->server_rio_addr_lo);
		DBG("Accept: msub_bytes = %u, rio_addr_len = %u\n",
					am->server_msub_bytes,
					am->server_rio_addr_len);

		/* Send the ACCEPT_FROM_MS_REQ message to the blocked
		 * rdma_conn_ms_h() via the tx engine */
		it->to_lib_tx_eng->send_message(move(in_msg));

		/* Update the corresponding element of connected_to_ms_info_list */
		/* By setting this entry to 'connected' it is ignored if there
		 * is an ACCEPT_MS destined for another client. */
		DBG("Setting '%s' to 'connected\n", cm_accept_ms->server_ms_name);
		it->connected = true;
		it->server_msid = be64toh(cm_accept_ms->server_msid);
		it->server_msubid = be64toh(cm_accept_ms->server_msubid);
	}
} /* cm_accept_ms_disp() */

/**
 * @brief Runs at the Client daemon.
 *
 */
void cm_force_disconnect_ms_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng)
{
	cm_force_disconnect_ms_msg	*cm_force_disconnect_ms =
						&msg->cm_force_disconnect_ms;
	/* Receive CM_FORCE_DISCONNECT_MS */
	HIGH("Received CM_FORCE_DISCONNECT_MS containing '%s'\n",
					cm_force_disconnect_ms->server_msname);

	uint32_t server_msid = static_cast<uint32_t>(
				be64toh(cm_force_disconnect_ms->server_msid));
	uint32_t server_msubid = static_cast<uint32_t>(
				be64toh(cm_force_disconnect_ms->server_msubid));
	uint64_t client_to_lib_tx_eng_h =
			be64toh(cm_force_disconnect_ms->client_to_lib_tx_eng_h);

	/* Relay to library and get ACK back */
	int rc = send_force_disconnect_ms_to_lib(server_msid, server_msubid,
						client_to_lib_tx_eng_h);
	if (rc) {
		ERR("Failed to send destroy message to library or get ack\n");
		return;
	}

	/* Remove the entry relating to the destroyed ms. The entry fields must
	 * match the 'CM_FORCE_DISCONNECT_MS */
	lock_guard<mutex> conn_lock(connected_to_ms_info_list_mutex);
	connected_to_ms_info_list.erase (
		remove_if(begin(connected_to_ms_info_list),
		  end(connected_to_ms_info_list),
		  [=](connected_to_ms_info& info) {
			return (info.server_msid == server_msid)
			&&     (info.server_msubid == server_msubid)
			&&     ((uint64_t)info.to_lib_tx_eng == client_to_lib_tx_eng_h);
			})
			, end(connected_to_ms_info_list));

	/* That's ALL. There is no acknowledgement for force disconnection
	 * as there is generally no application on the other side awaiting that
	 * acknowledgement. A force disconnection may happen if the server
	 * destroys the memory space. However the rdma_destroy_ms_h() API
	 * does not wait for any confirmations of remote disconnections.
	 * Another reason for forced disconnection would be if the application
	 * dies and obviously in that case there would be no one to receive
	 * the forced disconnection acknowledgement.
	 */
	(void)tx_eng;	/* We're not sending anything back! */
} /* cm_force_disconnect_ms_disp() */

/**
 * @brief Called when a client daemon receives CM_SERVER_DISCONNECT_MS
 * from a server daemon instructing it to notify the client app that
 * has connected to a memory space that the connection is no longer valid.
 */
void cm_server_disconnect_ms_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng)
{
	cm_server_disconnect_ms_msg	*cm_server_disconnect_ms =
						&msg->cm_server_disconnect_ms;
	/* Receive CM_SERVER_DISCONNECT_MS */
	HIGH("Received CM_SERVER_DISCONNECT_MS containing '%s'\n",
					cm_server_disconnect_ms->server_msname);

	uint32_t server_msid = static_cast<uint32_t>(
				be64toh(cm_server_disconnect_ms->server_msid));
	uint32_t server_msubid = static_cast<uint32_t>(
				be64toh(cm_server_disconnect_ms->server_msubid));
	uint64_t client_to_lib_tx_eng_h =
			be64toh(cm_server_disconnect_ms->client_to_lib_tx_eng_h);

	/* Relay to library and get ACK back */
	int rc = send_force_disconnect_ms_to_lib(server_msid, server_msubid,
						client_to_lib_tx_eng_h);
	if (rc) {
		ERR("Failed to send destroy message to library or get ack\n");
		return;
	}

	/* Remove the entry relating to the destroyed ms. The entry fields must
	 * match the 'CM_FORCE_DISCONNECT_MS */
	lock_guard<mutex> conn_lock(connected_to_ms_info_list_mutex);
	connected_to_ms_info_list.erase (
		remove_if(begin(connected_to_ms_info_list),
		  end(connected_to_ms_info_list),
		  [&](connected_to_ms_info& info) {
			return (info.server_msid == server_msid)
			&&     (info.server_msubid == server_msubid)
			&&     ((uint64_t)info.to_lib_tx_eng == client_to_lib_tx_eng_h);
			})
			, end(connected_to_ms_info_list));

	/**
	 * Send back CM_FORCE_DISCONNECT_MS_ACK to the remote daemon on which
	 * the ms was either destroyed or self-disconnected by its server. */

	/* Header */
	auto in_msg = make_unique<cm_msg_t>();
	in_msg->type = htobe64(CM_SERVER_DISCONNECT_MS_ACK);
	in_msg->category = htobe64(RDMA_REQ_RESP);
	in_msg->seq_no = 0;

	/* CM_SERVER_DISCONNECT_MS_ACK contents */
	cm_server_disconnect_ms_ack_msg *dam = &in_msg->cm_server_disconnect_ms_ack;
	strcpy(dam->server_msname, cm_server_disconnect_ms->server_msname);

	/* NOTE: All Big Endian below. No conversion needed */
	dam->server_msid = cm_server_disconnect_ms->server_msid;
	dam->client_to_lib_tx_eng_h = cm_server_disconnect_ms->client_to_lib_tx_eng_h;
	dam->server_to_lib_tx_eng_h = cm_server_disconnect_ms->server_to_lib_tx_eng_h;
	/* Send to remote server daemon */
	tx_eng->send_message(move(in_msg));
} /* cm_server_disconnect_ms_disp() */

/**
 * @brief At the server daemon, in response to receiving CM_SERVER_DISCONNECT_MS_ACK.
 * 	  Sends SERVER_DISCONNECT_MS_ACK to the library.
 */
void cm_server_disconnect_ms_ack_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng)
{
	(void)tx_eng;
	cm_server_disconnect_ms_ack_msg *server_disconnect_ms_ack_msg =
						&msg->cm_server_disconnect_ms_ack;

	HIGH("Received CM_FORCE_DISCONNECT_MS_ACK for msid(0x%" PRIX64 "), '%s'\n",
			be64toh(server_disconnect_ms_ack_msg->server_msid),
			server_disconnect_ms_ack_msg->server_msname);

	/* Obtain the to-library Tx engine from the message */
	tx_engine<unix_server, unix_msg_t> *to_lib_tx_eng =
			(tx_engine<unix_server, unix_msg_t>*)be64toh(
			server_disconnect_ms_ack_msg->server_to_lib_tx_eng_h);

	/* Reply to library indicating success */
	auto out_msg = make_unique<unix_msg_t>();
	out_msg->category = RDMA_CALL;
	out_msg->type	 = SERVER_DISCONNECT_MS_ACK;
	out_msg->seq_no	 = 0;
	out_msg->server_disconnect_ms_out.status = 0; /* success */

	to_lib_tx_eng->send_message(move(out_msg));
} /* force_disconnect_ms_ack_disp() */
