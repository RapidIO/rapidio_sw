#ifndef MSG_PROCESSOR_H
#define MSG_PROCESSOR_H

#include <vector>
#include <algorithm>

#include "rdma_msg.h"
#include "tx_engine.h"

#include "liblog.h"

using std::vector;
using std::find;

/**
 * Dispatch entry correlating a message type and the dispatch
 * function to be called to process the message.
 */
template <typename T, typename M>
struct msg_proc_dispatch_entry {
	rdma_msg_type	type;
	int (*disp_func)(const M *msg, tx_engine<T, M> *tx_eng);
	bool operator==(rdma_msg_type type)
	{
		return this->type == type;
	}
};

template <typename T, typename M>
class msg_processor {

public:
	msg_processor(vector<msg_proc_dispatch_entry<T, M>>& dispatch_table) :
		dispatch_table(dispatch_table)
	{
	}

	int process_msg(M* msg, tx_engine<T, M> *tx_eng)
	{
		auto rc = 0;
		DBG("Processing message: 0x%X\n", msg->type);
		auto it = find(begin(dispatch_table),
				end(dispatch_table),
				msg->type);
		if (it == end(dispatch_table)) {
			ERR("Failed to find dispatch entry!\n");
			rc = -1;
		} else {
			rc = it->disp_func(msg, tx_eng);
		}
		return rc;
	}
private:
	vector<msg_proc_dispatch_entry<T, M>>& dispatch_table;
};
#endif

