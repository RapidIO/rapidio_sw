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

#include "rapidio_mport_dma.h"

constexpr uint16_t DEFAULT_PROV_CHANNEL = 10;
constexpr uint8_t  DEFAULT_PROV_MBOX_ID =  0;
constexpr auto	   DEFAULT_CONSOLE_SKT  = 4444;
constexpr auto	   DEFAULT_RUN_CONS	= 1;

struct peer_info
{
	/**
	 * @brief Constructor
	 */
	peer_info(uint8_t destid_len, uint32_t destid, int mport_id,
		  riomp_mport_t mport_hnd, uint16_t prov_channel,
		  uint8_t prov_mbox_id, int cons_skt, int run_cons)
	: destid_len(destid_len), destid(destid), mport_id(mport_id),
	  mport_hnd(mport_hnd), prov_channel(prov_channel),
	  prov_mbox_id(prov_mbox_id), cons_skt(cons_skt), run_cons(run_cons)
	{}

	/* Device ID */
	uint8_t	destid_len;
	uint32_t destid;

	/* MPORT */
	int mport_id;
	riomp_mport_t mport_hnd;

	/* Provisioning channel and mailbox */
	uint16_t	prov_channel;
	uint8_t		prov_mbox_id;

	/* Daemon CLI connection */
	int	cons_skt;	/* Console socket number, default 4444 */
	int	run_cons;	/* Run console on Daemon? */
};

#endif

