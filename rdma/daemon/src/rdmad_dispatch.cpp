#include "tx_engine.h"
#include "rx_engine.h"
#include "rdmad_main.h"
#include "rdmad_rpc.h"
#include "rdma_msg.h"

/**
 * Dispatch function for obtaining mport ID.
 */
int get_mport_id_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = GET_MPORT_ID_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.get_mport_id_out.status =
		rdmad_get_mport_id(&out_msg.get_mport_id_out.mport_id);

	tx_eng->send_message(&out_msg);

	return out_msg.get_mport_id_out.status;
} /* get_mport_id_disp() */

int create_mso_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = CREATE_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;
	out_msg.create_mso_out.status =
		rdmad_create_mso(in_msg->create_mso_in.owner_name,
				 &out_msg.create_mso_out.msoid,
				 tx_eng->get_client());
	if (out_msg.create_mso_out.status) {
		ERR("Failed in call rdmad_create_mso\n");
	}
	tx_eng->send_message(&out_msg);

	return out_msg.create_mso_out.status;
} /* create_mso_disp() */

int open_mso_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type 	 = OPEN_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no   = in_msg->seq_no;
	out_msg.open_mso_out.status =
		rdmad_open_mso(in_msg->open_mso_in.owner_name,
				&out_msg.open_mso_out.msoid,
				&out_msg.open_mso_out.mso_conn_id,
				tx_eng->get_client());

	if (out_msg.open_mso_out.status) {
		ERR("Failed in call rdmad_open_mso\n");
	}
	tx_eng->send_message(&out_msg);

	return out_msg.open_mso_out.status;
} /* open_mso_disp() */

int close_mso_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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

int destroy_mso_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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


int create_ms_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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
				&out_msg.create_ms_out.rio_addr);

	if (out_msg.create_ms_out.status) {
		ERR("Failed in call rdmad_create_ms\n");
	}
	tx_eng->send_message(&out_msg);

	return out_msg.create_ms_out.status;
} /* create_msg_disp() */

int open_ms_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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
			      tx_eng->get_client());

	if (out_msg.open_ms_out.status) {
		ERR("Failed in call rdmad_open_ms\n");
	}
	tx_eng->send_message(&out_msg);

	return out_msg.open_ms_out.status;
} /* open_ms_disp() */

int close_ms_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

	DBG("seq_no = 0x%X\n", in_msg->seq_no);

	out_msg.type	 = CLOSE_MS_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no 	 = in_msg->seq_no;

	out_msg.close_ms_out.status =
		rdmad_close_ms(in_msg->close_ms_in.msid,
			       in_msg->close_ms_in.ms_conn_id);

	if (out_msg.close_ms_out.status) {
		ERR("Failed in call rdmad_close_ms\n");
	}
	tx_eng->send_message(&out_msg);

	return out_msg.close_ms_out.status;
} /* close_ms_disp() */

int destroy_ms_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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

int create_msub_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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
				  &out_msg.create_msub_out.phys_addr);

	if (out_msg.create_msub_out.status) {
		ERR("Failed in call rdmad_create_msub\n");
	}
	tx_eng->send_message(&out_msg);

	return out_msg.destroy_ms_out.status;
} /* create_msub_disp() */

int destroy_msub_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{
	unix_msg_t out_msg;

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

#if 0
int accept_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{

}

int undo_accept_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{

}

int send_connect_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{

}

int undo_connect_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{

}

int send_disconnect_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{

}

int get_ibwin_properties_disp(const unix_msg_t *in_msg, unix_tx_engine *tx_eng)
{

}
#endif

