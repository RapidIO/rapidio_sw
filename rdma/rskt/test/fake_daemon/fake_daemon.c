/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * Header file for RapidIO mport device library.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License(GPL) Version 2, or the BSD-3 Clause license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>
#include "librskt_private.h"
#include "librsktd_sn.h"
#include "librsktd_msg_proc.h"
#include "librsktd_dmn.h"
#include "librskt.h"

#ifdef __cplusplus
extern "C" {
#endif

void enqueue_speer_msg(struct librsktd_unified_msg *msg)
{
	if (0)
		*msg = *msg;
};

void enqueue_wpeer_msg(struct librsktd_unified_msg *msg)
{
	if (0)
		*msg = *msg;
};

void rsktd_sn_init(uint16_t max_skt_num)
{
	if (max_skt_num) {};
};

enum rskt_state rsktd_sn_get(uint32_t skt_num)
{
	if (skt_num)
		return rskt_uninit;
		
	return rskt_uninit;
}

void rsktd_sn_set(uint32_t skt_num, enum rskt_state st)
{
	if (skt_num || (st == rskt_uninit))
		return;
	return;
}

uint32_t  rsktd_sn_find_free(uint32_t skt_num)
{
	if (skt_num)
		return 0;

	return 0;
};

extern int start_fm_thread(void)
{
	return 0;
};

void *wpeer_tx_loop(void *unused)
{
	return unused;
};

void start_new_speer(riomp_sock_t new_socket)
{
	if (NULL == new_socket)
		return;

	return;
};

void close_all_speers(void) {};

void close_all_wpeers(void) {};

void rskt_daemon_shutdown(void) {};

int open_wpeers_for_requests(int num_peers, struct peer_rsktd_addr *peers)
{
	if (0) {
		if (NULL == peers)
			return 0;
	};
	return 0 * num_peers;
};

void halt_fm_thread(void) {};

void *speer_tx_loop(void *unused)
{
	return unused;	
};

void librsktd_bind_sn_cli_cmds(void) {}

#ifdef __cplusplus
}
#endif

