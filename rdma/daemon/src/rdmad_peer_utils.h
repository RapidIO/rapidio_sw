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

#define CONFIG_RAPIDIO_DMA_ENGINE
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"

#include "riodp_mport_lib.h"

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
#if 0
	/* Messaging */
	#define DEFAULT_LOC_CHANNEL	2
	uint16_t	loc_channel;

	#define DEFAULT_AUX_CHANNEL	3
	uint16_t	aux_channel;

	#define DEFAULT_DESTROY_CHANNEL	4
	uint16_t	destroy_channel;
#endif
	#define DEFAULT_PROV_CHANNEL		10
	uint16_t	prov_channel;
#if 0
	#define DEFAULT_MAILBOX_ID	0
	uint8_t		mbox_id;

	#define DEFAULT_AUX_MAILBOX_ID  0
	uint8_t		aux_mbox_id;
#endif
	#define DEFAULT_PROV_MBOX_ID 0
	uint8_t		prov_mbox_id;
#if 0
	#define DEFAULT_DESTROY_MAILBOX_ID  0
	uint8_t		destroy_mbox_id;
#endif
	sem_t	cm_wait_connect_sem;

	/* Daemon CLI connection */
	int	cons_skt;	/* Console socket number, default 4444 */
	int	run_cons;	/* Run console on Daemon? */
};

void init_peer_info(int num_peers,struct peer_info peers[]);

extern struct peer_info peer;

#endif

