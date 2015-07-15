#ifndef PROV_DAEMON_INFO_H_
#define PROV_DAEMON_INFO_H_

#include <stdint.h>
#include <pthread.h>

#include "cm_sock.h"


/**
 * Info for remote daemons provisined by the provisioning thread
 * (i.e. by receiving a HELLO message).
 */
struct prov_daemon_info {
	uint32_t 	destid;
	cm_server	*conn_disc_server;
	pthread_t	tid;
	bool operator==(uint32_t destid) { return this->destid == destid; }
};

#endif
