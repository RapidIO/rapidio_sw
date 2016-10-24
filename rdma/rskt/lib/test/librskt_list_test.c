/* Test code for librskt */
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <string.h>
#include <netinet/in.h>
#include "memops.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "librskt_private.h"
#include "librskt_buff.h"
#include "librskt_socket.h"
#include "librskt_list.h"
#include "librskt_info.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "libcli.h"
#include "librskt.h"

int test_librskt_structs(void)
{
	struct dup_rskt_entry {
		struct rskt_socket_t *sock;
		enum rskt_state st;
		bool wip; /* Write in progress */
		bool rip; /* REad in progress */;
		uint64_t handle;
	};

	struct dup_rskt_block {
		sem_t mtx; /* Mutex for access to remainder of structure fields */
		volatile struct rskt_entry skts[RSKTS_PER_BLOCK];
		struct l_head_t free_idx;
		uint64_t handle_num;
	};

	if (sizeof(struct dup_rskt_entry) != sizeof(struct rskt_entry)) {
		goto fail;
	};
	if (sizeof(struct dup_rskt_block) != sizeof(struct rskt_block)) {
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int test_librskt_list_init(void)
{
	struct rskt_block blk;

	errno = 0;
	if (!rsktl_init(NULL)) {
		goto fail;
	};
	if (errno != EINVAL) {
		goto fail;
	};

	if (rsktl_init(&blk)) {
		goto fail;
	};
	if (RSKTS_PER_BLOCK - 1 != l_size(&blk.free_idx)) {
		goto fail;
	}
	if (1 != blk.handle_num) {
		goto fail;
	};
	if (sem_trywait(&blk.mtx)) {
		goto fail;
	};
	if (!sem_trywait(&blk.mtx)) {
		goto fail;
	};
	if (sem_post(&blk.mtx)) {
		goto fail;
	};
	return 0;
fail:
	return 1;
};

/* 
- Test allocation to maximum number of sockets.
- Test that allocating one more than maximum fails.
- Test deallocation, and verify that sockets are
  returned to the correct state.
- Test allocating to the maximum amount again, and
  test deallocation again.
- Test that deallocation of old handles fails...
*/
int test_rsktl_get_socket(void)
{
	struct rskt_block blk;
	rskt_h h[RSKTS_PER_BLOCK * 2];
	uint64_t i;
	int rc = 1;
	
	if (rsktl_init(&blk)) {
		goto fail;
	};
	for (i = 1; i < RSKTS_PER_BLOCK; i++) {
		h[i] = rsktl_get_socket(&blk);
		if (((i << 16) + i) != h[i]) {
			goto fail;
		};
		if (NULL == blk.skts[i].sock) {
			goto fail;
		};
		if (rskt_alloced != blk.skts[i].st) {
			goto fail;
		}
		if (blk.skts[i].wip || blk.skts[i].rip) {
			goto fail;
		}
		if (blk.skts[i].handle != h[i]) {
			goto fail;
		}
		if (i + 1 != blk.handle_num) {
			goto fail;
		};
	};
	rc = 2;
	if (l_size(&blk.free_idx)) {
		goto fail;
	};
	h[0] = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID != h[0]) {
		goto fail;
	};

	rc = 3;
	for (i = 1; i < RSKTS_PER_BLOCK; i++) {
		if (rsktl_put_socket(&blk, h[i])) {
			goto fail;
		};
		if (NULL != blk.skts[i].sock) {
			goto fail;
		};
		if (rskt_uninit != blk.skts[i].st) {
			goto fail;
		};
		if (LIBRSKT_H_INVALID != blk.skts[i].handle) {
			goto fail;
		};
		if (i != (uint64_t)l_size(&blk.free_idx)) {
			goto fail;
		}
		if (blk.free_idx.tail->item != (void *)i) {
			goto fail;
		}
		if (rsktl_put_socket(&blk, h[i])) {
			goto fail;
		}
		if (i != (uint64_t)l_size(&blk.free_idx)) {
			goto fail;
		}
		if (blk.free_idx.tail->item != (void *)i) {
			goto fail;
		}
	};

	rc = 4;
#define H2(x) (x + RSKTS_PER_BLOCK)
	for (i = 1; i < RSKTS_PER_BLOCK; i++) {
		h[H2(i)] = rsktl_get_socket(&blk);
		if (((H2(i-1) << 16) + i) != h[H2(i)]) {
			goto fail;
		};
		if (NULL == blk.skts[i].sock) {
			goto fail;
		};
		if (rskt_alloced != blk.skts[i].st) {
			goto fail;
		}
		if (blk.skts[i].wip || blk.skts[i].rip) {
			goto fail;
		}
		if (blk.skts[i].handle != h[H2(i)]) {
			goto fail;
		}
		if ((H2(i)) != blk.handle_num) {
			goto fail;
		};
	};
	if (l_size(&blk.free_idx)) {
		goto fail;
	};
	h[H2(0)] = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID != h[H2(0)]) {
		goto fail;
	};

	rc = 5;
	for (i = 1; i < RSKTS_PER_BLOCK; i++) {
		if (rsktl_put_socket(&blk, h[H2(i)])) {
			goto fail;
		};
		if (NULL != blk.skts[i].sock) {
			goto fail;
		};
		if (rskt_uninit != blk.skts[i].st) {
			goto fail;
		};
		if (LIBRSKT_H_INVALID != blk.skts[i].handle) {
			goto fail;
		};
		if (i != (uint64_t)l_size(&blk.free_idx)) {
			goto fail;
		}
		if (blk.free_idx.tail->item != (void *)i) {
			goto fail;
		}
		if (rsktl_put_socket(&blk, h[H2(i)])) {
			goto fail;
		}
		if (i != (uint64_t)l_size(&blk.free_idx)) {
			goto fail;
		}
		if (blk.free_idx.tail->item != (void *)i) {
			goto fail;
		}
	};
	rc = 6;
	for (i = 1; i < RSKTS_PER_BLOCK; i++) {
		if (rsktl_put_socket(&blk, h[i])) {
			goto fail;
		};
	};
	return 0;
fail:
	return rc;
};

/* Test that socket is handled correctly for each socket state.*/

struct rsktl_put_skt_test_array_entry {
	enum rskt_state st;
	bool success;
};

struct rsktl_put_skt_test_array_entry rsktl_put_skt_test_array[] = {
	{ rskt_uninit, true},
	{ rskt_alloced, true},
	{ rskt_reqbound, true },
	{ rskt_bound, true },
	{ rskt_reqlisten, true },
	{ rskt_listening, true },
	{ rskt_accepting, true },
	{ rskt_reqconnect, true },
	{ rskt_connecting, true },
	{ rskt_connected, true },
	{ rskt_shutting_down, true },
	{ rskt_close_by_local, true },
	{ rskt_close_by_remote, true },
	{ rskt_closing, true },
	{ rskt_shut_down, true },
	{ rskt_closed, true },
	{ rskt_max_state, true }
};

int test_rsktl_put_socket(void)
{
	struct rskt_block blk;
	rskt_h h;
	uint64_t i;

	if ((sizeof(rsktl_put_skt_test_array) /
				sizeof(struct rsktl_put_skt_test_array_entry))
			!= (rskt_max_state + 1)) {
		goto fail;
	};

	if (rsktl_init(&blk)) {
		goto fail;
	};

	if (rsktl_put_socket(&blk, LIBRSKT_H_INVALID)) {
		goto fail;
	};

	for (i = 0; i <= (uint64_t)rskt_max_state; i++) {
		int rc;
		struct rskt_socket_t *sock_ptr;

		h = rsktl_get_socket(&blk);
		sock_ptr = rsktl_sock_ptr(&blk, h);
		if (NULL == sock_ptr) {
			goto fail;
		};
		blk.skts[SKT_IDX(h)].st = rsktl_put_skt_test_array[i].st;

		rc = rsktl_put_socket(&blk, h);

		if (rc && rsktl_put_skt_test_array[i].success) {
			goto fail;
		}

		if (!rc && !rsktl_put_skt_test_array[i].success) {
			goto fail;
		}

		/* Change state and recover socket */
		if (!rsktl_put_skt_test_array[i].success) {
			blk.skts[SKT_IDX(h)].st = rskt_uninit;
			if (rsktl_put_socket(&blk, h)) {
				goto fail;
			};
		};
	};

	return 0;
fail:
	return 1;
};

#define TEST_IDX(x,y,z) (x + (y * z))
int test_rsktl_find_skt_h(void)
{
	struct rskt_block blk;
	rskt_h h[RSKTS_PER_BLOCK];
	struct rskt_socket_t *sock_ptr[RSKTS_PER_BLOCK];
	uint64_t i, j, k;
	uint64_t max = 0x1000;
	uint64_t test_loops = 10;

	if (rsktl_init(&blk)) {
		goto fail;
	};

	for (j = 0; j < test_loops; j++) {
		for (i = 1; i < RSKTS_PER_BLOCK; i++) {
			h[i] = rsktl_get_socket(&blk);
			if (LIBRSKT_H_INVALID == h[i]) {
				goto fail;
			};
			sock_ptr[i] = rsktl_sock_ptr(&blk, h[i]);
			if (NULL == sock_ptr[i]) {
				goto fail;
			};
			sock_ptr[i]->sa.sn = TEST_IDX(i, j, max);
		};

		/* Check all other socket numbers cannot be found */
		for (k = 0; k < test_loops; k++) {
			if (k == j) {
				continue;
			};
			for (i = 1; i < RSKTS_PER_BLOCK; i++) {
				if (rsktl_find_skt_h(&blk, TEST_IDX(i, k, max))
							 != LIBRSKT_H_INVALID) {
					goto fail;
				};
			};
		};
		for (i = 1; i < RSKTS_PER_BLOCK; i++) {
			rskt_h sock_h;
			struct rskt_socket_t *temp_ptr;

			sock_h = rsktl_find_skt_h(&blk, TEST_IDX(i, j, max));
			if (sock_h != h[i]) {
				goto fail;
			};
			temp_ptr = rsktl_sock_ptr(&blk, sock_h);
			if (temp_ptr != sock_ptr[i]) {
				goto fail;
			};
			if (rsktl_put_socket(&blk, sock_h)) {
				goto fail;
			};
		};
	};
	return 0;
fail:
	return 1;
};

#define SOCK_PTR_IDX(i,j) TEST_IDX(i, j, RSKTS_PER_BLOCK)

int test_rsktl_sock_ptr(void)
{
	struct rskt_block blk;
	uint64_t i, j, k;
	const uint64_t max = 10;
	rskt_h h[RSKTS_PER_BLOCK * max];

	if (rsktl_init(&blk)) {
		goto fail;
	};

	for (j = 0; j < max; j++) {
		for (i = 1; i < RSKTS_PER_BLOCK; i++) {
			h[SOCK_PTR_IDX(i, j)] = SOCK_PTR_IDX(i, j);
		}
	}

	for (j = 0; j < max; j++) {
		for (i = 1; i < RSKTS_PER_BLOCK; i++) {
			h[SOCK_PTR_IDX(i, j)] = rsktl_get_socket(&blk);
			if (LIBRSKT_H_INVALID == h[SOCK_PTR_IDX(i,j)]) {
				goto fail;
			};
		};
		for (k = 0; k < max; k++) {
			if (k == j) {
				continue;
			}
			for (i = 1; i < RSKTS_PER_BLOCK; i++) {
				if (NULL != rsktl_sock_ptr(&blk,
						h[SOCK_PTR_IDX(i, k)])) {
					goto fail;
				};
			};
		};
		for (i = 1; i < RSKTS_PER_BLOCK; i++) {
			if (rsktl_put_socket(&blk, h[SOCK_PTR_IDX(i, j)])) {
				goto fail;
			};
		};
	};

	return 0;
fail:
	return 1;
}

#define INCR_ST(x) x = (rskt_state)(((uint32_t)x) + 1)

int test_rsktl_get_st(void)
{
	struct rskt_block blk;
	enum rskt_state t_st;
	rskt_h h;

	if (rsktl_init(&blk)) {
		goto fail;
	};

	h = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID == h) {
		goto fail;
	};

	for (t_st = rskt_uninit; t_st < (rskt_max_state + 1); INCR_ST(t_st)) {
		blk.skts[SKT_IDX(h)].st = t_st;
		if (t_st != rsktl_get_st(&blk, h)) {
			goto fail;
		};
	};

	return 0;
fail:
	return 1;
};

int test_rsktl_set_st(void)
{
	struct rskt_block blk;
	enum rskt_state t_st;
	rskt_h h;

	if (rsktl_init(&blk)) {
		goto fail;
	};

	h = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID == h) {
		goto fail;
	};

	for (t_st = rskt_uninit; t_st < rskt_max_state; INCR_ST(t_st)) {
		if (rsktl_set_st(&blk, h, t_st)) {
			goto fail;
		};
		if (t_st != blk.skts[SKT_IDX(h)].st) {
			goto fail;
		}
	};

	if (rsktl_set_st(&blk, h, rskt_uninit)) {
		goto fail;
	};
	if (!rsktl_set_st(&blk, h, rskt_max_state)) {
		goto fail;
	};
	if (rskt_uninit != blk.skts[SKT_IDX(h)].st) {
		goto fail;
	};
	if (EINVAL != errno) {
		goto fail;
	};
	if (sem_trywait(&blk.mtx)) {
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int test_rsktl_atomic_set_st(void)
{
	struct rskt_block blk;
	enum rskt_state tst_st, chk_st;
	rskt_h h;
	enum rskt_state rc_st;

	if (rsktl_init(&blk)) {
		goto fail;
	};

	h = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID == h) {
		goto fail;
	};

	/* Full range of success cases */
	for (tst_st = rskt_uninit; tst_st < rskt_max_state; INCR_ST(tst_st)) {
		for (chk_st = rskt_uninit; chk_st < rskt_max_state;
							INCR_ST(chk_st)) {
			if (chk_st == tst_st) {
				continue;
			};
			if (rsktl_set_st(&blk, h, tst_st)) {
				goto fail;
			};
			rc_st = rsktl_atomic_set_st(&blk, h, tst_st, chk_st);
			if (rc_st != chk_st) {
				goto fail;
			};
		}
	};

	/* Full range of failure cases */
	for (tst_st = rskt_uninit; tst_st < rskt_max_state; INCR_ST(tst_st)) {
		if (rsktl_set_st(&blk, h, tst_st)) {
			goto fail;
		};
		for (chk_st = rskt_uninit; chk_st < rskt_max_state;
							INCR_ST(chk_st)) {
			if (chk_st == tst_st) {
				continue;
			};
			rc_st = rsktl_atomic_set_st(&blk, h, chk_st, tst_st);
			if (rc_st != tst_st) {
				goto fail;
			};
		};
	};

	if (rsktl_set_st(&blk, h, rskt_listening)) {
		goto fail;
	};
	rc_st = rsktl_atomic_set_st(&blk, h, rskt_max_state, rskt_uninit);
	if (rskt_max_state != rc_st) {
		goto fail;
	};
	if (rskt_listening != blk.skts[SKT_IDX(h)].st) {
		goto fail;
	};
	if (EINVAL != errno) {
		goto fail;
	};

	errno = 0;

	if (!rsktl_atomic_set_st(&blk, h, rskt_uninit, rskt_max_state)) {
		goto fail;
	};
	if (rskt_listening != blk.skts[SKT_IDX(h)].st) {
		goto fail;
	};
	if (EINVAL != errno) {
		goto fail;
	};
	if (sem_trywait(&blk.mtx)) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_atomic_set_wip(void)
{
	struct rskt_block blk;
	rskt_h h;
	enum rskt_state tst_st;

	if (rsktl_init(&blk)) {
		goto fail;
	};

	h = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID == h) {
		goto fail;
	};

	for (tst_st = rskt_uninit; tst_st < rskt_max_state; INCR_ST(tst_st)) {
		if (rskt_connected == tst_st) {
			continue;
		};
		if (rsktl_set_st(&blk, h, tst_st)) {
			goto fail;
		};

		if (!rsktl_atomic_set_wip(&blk, h, false, true)) {
			goto fail;
		};
		if (blk.skts[SKT_IDX(h)].wip) {
			goto fail;
		};
		if (rsktl_atomic_set_wip(&blk, h, true, false)) {
			goto fail;
		};
		if (blk.skts[SKT_IDX(h)].wip) {
			goto fail;
		};
	};
	if (rsktl_set_st(&blk, h, rskt_connected)) {
		goto fail;
	};

	if (!rsktl_atomic_set_wip(&blk, h, false, true)) {
		goto fail;
	};
	if (!blk.skts[SKT_IDX(h)].wip) {
		goto fail;
	};
	if (!rsktl_atomic_set_wip(&blk, h, true, true)) {
		goto fail;
	};
	if (!blk.skts[SKT_IDX(h)].wip) {
		goto fail;
	};
	if (rsktl_atomic_set_wip(&blk, h, false, true)) {
		goto fail;
	};
	if (!blk.skts[SKT_IDX(h)].wip) {
		goto fail;
	};
	if (rsktl_atomic_set_wip(&blk, h, true, false)) {
		goto fail;
	};
	if (blk.skts[SKT_IDX(h)].wip) {
		goto fail;
	};
	if (sem_trywait(&blk.mtx)) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_atomic_set_rip(void)
{
	struct rskt_block blk;
	rskt_h h;
	uint64_t i;
	enum rskt_state tst_st;

	if (rsktl_init(&blk)) {
		goto fail;
	};

	h = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID == h) {
		goto fail;
	};

	for (tst_st = rskt_uninit; tst_st < rskt_max_state; INCR_ST(tst_st)) {
		if (rskt_connected == tst_st) {
			continue;
		};
		if (rskt_close_by_remote == tst_st) {
			continue;
		};
		if (rskt_closing == tst_st) {
			continue;
		};
		if (rskt_connected == tst_st) {
			continue;
		};
		if (rsktl_set_st(&blk, h, tst_st)) {
			goto fail;
		};

		if (!rsktl_atomic_set_rip(&blk, h, false, true)) {
			goto fail;
		};
		if (blk.skts[SKT_IDX(h)].rip) {
			goto fail;
		};
		if (rsktl_atomic_set_rip(&blk, h, true, false)) {
			goto fail;
		};
		if (blk.skts[SKT_IDX(h)].rip) {
			goto fail;
		};
	};

	for (i = 0; i < 3; i++) {
		tst_st = rskt_uninit;
		switch (i) {
		case 0: tst_st = rskt_connected;
			break;
		case 1: tst_st = rskt_close_by_remote;
			break;
		default:
		case 2: tst_st = rskt_closing;
			break;
		};
		if (rsktl_set_st(&blk, h, tst_st)) {
			goto fail;
		};

		if (!rsktl_atomic_set_rip(&blk, h, false, true)) {
			goto fail;
		};
		if (!blk.skts[SKT_IDX(h)].rip) {
			goto fail;
		};
		if (!rsktl_atomic_set_rip(&blk, h, true, true)) {
			goto fail;
		};
		if (!blk.skts[SKT_IDX(h)].rip) {
			goto fail;
		};
		if (rsktl_atomic_set_rip(&blk, h, false, true)) {
			goto fail;
		};
		if (!blk.skts[SKT_IDX(h)].rip) {
			goto fail;
		};
		if (rsktl_atomic_set_rip(&blk, h, true, false)) {
			goto fail;
		};
		if (blk.skts[SKT_IDX(h)].rip) {
			goto fail;
		};
	};
	if (sem_trywait(&blk.mtx)) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_atomic_chk_flush(void)
{
	struct rskt_block blk;
	rskt_h h;
	struct rskt_socket_t *sock_ptr;

	if (rsktl_init(&blk)) {
		goto fail;
	};
	if (rsktl_atomic_chk_flush(&blk, LIBRSKT_H_INVALID) >= 0) {
		goto fail;
	};

	h = rsktl_get_socket(&blk);
	if (LIBRSKT_H_INVALID == h) {
		goto fail;
	};

	if (rsktl_atomic_chk_flush(&blk, h) >= 0) {
		goto fail;
	};

	sock_ptr = rsktl_sock_ptr(&blk, h);
	if (NULL == sock_ptr) {
		goto fail;
	};

	sock_ptr->msub_sz = 4 * 1024;
	sock_ptr->msub_p = (volatile uint8_t *)calloc(1, 4*1024);
	sock_ptr->hdr->loc_tx_wr_flags = htonl(RSKT_BUF_HDR_FLAG_INIT_DONE);
	sock_ptr->hdr->loc_rx_rd_flags = htonl(RSKT_BUF_HDR_FLAG_INIT_DONE);
	sock_ptr->hdr->rem_rx_wr_flags = htonl(RSKT_BUF_HDR_FLAG_INIT_DONE);
	sock_ptr->hdr->rem_tx_rd_flags = htonl(RSKT_BUF_HDR_FLAG_INIT_DONE);

	if (rsktl_atomic_chk_flush(&blk, h)) {
		goto fail;
	};

	sock_ptr->hdr->rem_rx_wr_flags |= htonl(RSKT_BUF_HDR_FLAG_CLOSING);
	if (rsktl_atomic_chk_flush(&blk, h)) {
		goto fail;
	};

	sock_ptr->hdr->loc_tx_wr_flags |= htonl(RSKT_BUF_HDR_FLAG_CLOSING);
	if (1 != rsktl_atomic_chk_flush(&blk, h)) {
		goto fail;
	};

	if (sem_trywait(&blk.mtx)) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

typedef int (* test_func)(void);

test_func array_of_tests[] = {
	test_librskt_structs,
	test_librskt_list_init,
	test_rsktl_get_socket,
	test_rsktl_put_socket,
	test_rsktl_find_skt_h,
	test_rsktl_sock_ptr,
	test_rsktl_get_st,
	test_rsktl_set_st,
	test_rsktl_atomic_set_st,
	test_atomic_set_wip,
	test_atomic_set_rip,
	test_atomic_chk_flush
};

int main(int argc, char *argv[])
{
	uint32_t i, rc;

	for (i = 0; i < sizeof(array_of_tests)/sizeof(test_func); i++) {
		rc = array_of_tests[i]();
		if (rc) {
			goto fail;
		};
	};
	printf("\nPASSED\n");
	return 0;
fail:
	printf("\nFAILED: %d %d\n", i, rc);
	return 1;
};

#ifdef __cplusplus
}
#endif
