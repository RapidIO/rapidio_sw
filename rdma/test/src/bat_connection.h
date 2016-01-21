#ifndef BAT_CONNECTION_H
#define BAT_CONNECTION_H

#include <stdint.h>

#include <exception>
#include <memory>

#include "memory_supp.h"
#include "bat_common.h"

using std::exception;
using std::unique_ptr;

class bat_connection {
public:
	bat_connection(uint32_t destid, uint16_t channel, const char *name, bool *shutting_down)
	{
		try {
			client = make_unique<cm_client>(name, BAT_MPORT_ID, BAT_MBOX_ID,
					channel, shutting_down);	
			if (client->connect(destid))
				throw -1;
			client->get_send_buffer((void **)&bm_tx);
			client->get_recv_buffer((void **)&bm_rx);
		}
		catch(exception& e) {
			fprintf(stderr, "%s: %s\n", name, e.what());
		}
		catch(int e) {
			fprintf(stderr, "'%s' failed to connect to destid(0x%X)\n",
								name, destid);
		}
	}

	int send()
	{
		return client->send();
	}

	int receive()
	{
		return client->receive();
	}

	bat_msg_t	*get_rx_buf() const { return bm_rx; }
	bat_msg_t	*get_tx_buf() const { return bm_tx; }

	void send_eot()
	{
		bm_tx->type = BAT_END;
		client->send();
	}
private:
	unique_ptr<cm_client>	client;
	bat_msg_t 	*bm_tx;		// Default tx message buffer
	bat_msg_t	*bm_rx;		// Default rx message buffer
};
#endif

