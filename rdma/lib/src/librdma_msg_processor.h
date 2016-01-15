#ifndef LIBRDMA_MSG_PROCESSOR_H
#define LIBRDMA_MSG_PROCESSOR_H

#include <cassert>

#include "rdma_msg.h"
#include "librdma_tx_engine.h"
#include "msg_processor.h"
#include "unix_sock.h"
#include "liblog.h"

class unix_msg_processor : public msg_processor<unix_client, unix_msg_t>
{
public:
	int process_msg(void *vmsg, void *vtx_eng)
	{
		unix_msg_t *msg = static_cast<unix_msg_t *>(vmsg);
		unix_tx_engine *tx_eng = static_cast<unix_tx_engine *>(vtx_eng);

		(void)tx_eng;

		auto rc = 0;

		switch(msg->type) {

		default:
			assert(!"Unhandled message");
		}
		return rc;
	}
};
#endif


