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

#ifndef PEER_UTILS_H
#define PEER_UTILS_H

#include <stdint.h>
#include <semaphore.h>

//#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_rdma.h>
//#include <rapidio_mport_sock.h>

struct peer_info {
	/* Device ID */
	uint8_t	destid_len;
	uint32_t destid;

	/* MPORT */
	int mport_id;
	int mport_fd;

	/* RIO */
	#define DEFAULT_RIO_ADDRESS     0x00000000 

	/* INBOUND WINDOW */
	#define DEFAULT_INBOUND_WINDOW_SIZE     0x00200000

	#define DEFAULT_PROV_CHANNEL		10
	uint16_t	prov_channel;

	#define DEFAULT_PROV_MBOX_ID 0
	uint8_t		prov_mbox_id;

	sem_t	cm_wait_connect_sem;

	/* Daemon CLI connection */
	int	cons_skt;	/* Console socket number, default 4444 */
	int	run_cons;	/* Run console on Daemon? */
};

void init_peer_info(int num_peers,struct peer_info peers[]);

extern struct peer_info peer;

#endif

