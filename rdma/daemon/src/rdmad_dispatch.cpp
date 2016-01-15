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
