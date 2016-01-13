#include "tx_engine.h"
#include "rx_engine.h"
#include "rdmad_main.h"
#include "rdmad_rpc.h"
#include "rdma_msg.h"

/**
 * Dispatch function for obtaining mport ID.
 */
int get_mport_id_disp(const unix_msg_t *in_msg, daemon2lib_tx_engine *tx_eng)
{
	unix_msg_t out_msg;
	out_msg.type = GET_MPORT_ID_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no = in_msg->seq_no;
	int rc = rdmad_get_mport_id(&out_msg.get_mport_id_out.mport_id);
	if (rc) {
		ERR("Failed in call rdmad_get_mport_id\n");
	} else {
		tx_eng->send_message(&out_msg);
	}
	return rc;
} /* get_mport_id_disp() */

int create_mso_disp(const unix_msg_t *in_msg, daemon2lib_tx_engine *tx_eng)
{
	unix_msg_t out_msg;
	out_msg.type = CREATE_MSO_ACK;
	out_msg.category = RDMA_LIB_DAEMON_CALL;
	out_msg.seq_no = in_msg->seq_no;

	int rc = rdmad_create_mso(in_msg->create_mso_in.owner_name,
				    &out_msg.create_mso_out.msoid,
				    tx_eng->get_client());
	if (rc) {
		ERR("Failed in call rdmad_get_mport_id\n");
	} else {
		tx_eng->send_message(&out_msg);
	}

	return rc;
} /* create_mso_disp() */

