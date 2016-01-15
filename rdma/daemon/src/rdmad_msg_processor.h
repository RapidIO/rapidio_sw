#ifndef RDMAD_MSG_PROCESSOR_H
#define RDMAD_MSG_PROCESSOR_H

#include "rdma_msg.h"
#include "rdmad_tx_engine.h"
#include "msg_processor.h"
#include "unix_sock.h"
#include "liblog.h"
#include "rdmad_dispatch.h"

class unix_msg_processor : public msg_processor<unix_server, unix_msg_t>
{
public:
	int process_msg(void *vmsg, void *vtx_eng)
	{
		auto rc = 0;
		unix_msg_t *msg = static_cast<unix_msg_t *>(vmsg);
		unix_tx_engine *tx_eng = static_cast<unix_tx_engine *>(vtx_eng);

		switch(msg->type) {
		case GET_MPORT_ID:
			rc = get_mport_id_disp(msg, tx_eng);
			break;
		case CREATE_MSO:
			rc = create_mso_disp(msg, tx_eng);
			break;
		case DESTROY_MSO:
			rc = destroy_mso_disp(msg, tx_eng);
			break;
		default:
			assert(!"Unhandled message");
		}
		return rc;
	}
};
#endif

