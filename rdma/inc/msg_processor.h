#ifndef MSG_PROCESSOR_H
#define MSG_PROCESSOR_H

#include <vector>
#include <algorithm>
#include <string>

#include "rdma_msg.h"
#include "tx_engine.h"

#include "liblog.h"


template <typename T, typename M>
class msg_processor {

public:
	virtual ~msg_processor() {}

	virtual int process_msg(void* msg, void *tx_eng) = 0;
};
#endif

