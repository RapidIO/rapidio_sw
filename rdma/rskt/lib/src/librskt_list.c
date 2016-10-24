/* Implementation of librskt library integrated into applications */
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

#include <errno.h>
#include "memops.h"
#include "memops_umd.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "librskt_info.h"
#include "librskt_list.h"
#include "librskt_private.h"

#define VALID_IDX(x) (SKT_IDX(x) && (SKT_IDX(x) < RSKTS_PER_BLOCK))
#define VALID_H(x,l) ((SKT_IDX(x) && (SKT_IDX(x) < RSKTS_PER_BLOCK) \
		&& (NULL != l) && (x == l->skts[SKT_IDX(x)].handle))?true:false)

int rsktl_init(struct rskt_block *h)
{
	uint64_t i;

	if (NULL == h) {
		errno = EINVAL;
		goto fail;
	};

	memset(h, 0, sizeof(struct rskt_block));
	l_init(&h->free_idx);
	for (i = 1; i < RSKTS_PER_BLOCK; i++) {
		l_push_tail(&h->free_idx, (void *)i);
	};
	h->handle_num = 1;
	sem_init(&h->mtx, 0, 1);

	return 0;
fail:
	return -1;
};

void rskt_clear_skt(volatile struct rskt_socket_t *skt)
{
	skt->sai.rtID = 0xFFFFFFFF;
	skt->connector = skt_rmda_uninit;
	skt->memops = NULL;
}; /* rskt_clear_skt() */

rskt_h rsktl_get_socket(struct rskt_block *h)
{
	uint64_t entry = 0;
	rskt_h h_rc;
	struct rskt_socket_t *temp = (struct rskt_socket_t *)
					calloc(1, sizeof(struct rskt_socket_t));

	if (NULL == h) {
		errno = EINVAL;
		goto fail;
	};
	if (NULL == temp) {
		errno = ENOMEM;
		goto fail;
	};

	if (librskt_wait_for_sem(&h->mtx, 0x4344)) {
		goto fail;
	};
	entry = (uint64_t)l_pop_head(&h->free_idx);

	if (!VALID_IDX(entry)) {
		errno = ENFILE;
		goto fail_locked;
	};

	if (h->skts[entry].handle != LIBRSKT_H_INVALID) {
                errno = ENOTUNIQ;
                goto fail_locked;
        };
	h_rc = (uint64_t)entry + (h->handle_num++ << 16);

        h->skts[entry].sock = temp;
        rskt_clear_skt(h->skts[entry].sock);
        h->skts[entry].st = rskt_alloced; 
	h->skts[entry].wip = false;
	h->skts[entry].rip = false;
        h->skts[entry].handle = h_rc; 

	sem_post(&h->mtx);
	return h_rc;

fail_locked:
	sem_post(&h->mtx);
fail:
	if (NULL != temp) {
		free(temp);
	};
	return LIBRSKT_H_INVALID;
};

int rsktl_put_socket(struct rskt_block *h, rskt_h skt_h)
{
	uint64_t entry = SKT_IDX(skt_h);

	if (librskt_wait_for_sem(&h->mtx, 0x9933)) {
		goto fail_locked;
	};

 	if (VALID_H(skt_h, h)) {
		if (h->skts[entry].wip || h->skts[entry].rip) {
			errno = EBUSY;
			goto fail_locked;
		}

		if (h->skts[entry].sock) {
       			if (NULL != h->skts[entry].sock->memops) {
               			delete h->skts[entry].sock->memops;
               			h->skts[entry].sock->memops = NULL;
       			}
			free(h->skts[entry].sock);
			h->skts[entry].sock = NULL;
       		}

		memset((void *)&h->skts[entry], 0,
					sizeof(struct rskt_entry));
		h->skts[entry].handle = LIBRSKT_H_INVALID;
		l_push_tail(&h->free_idx, (void *)(entry));
	};
	sem_post(&h->mtx);
	return 0;
fail_locked:
	sem_post(&h->mtx);
	return -1;
};

rskt_h rsktl_find_skt_h(struct rskt_block *h, uint32_t sn)
{
	rskt_h skt_h = LIBRSKT_H_INVALID;
	int i;

	if (NULL == h) {
		return skt_h;
	};

	librskt_wait_for_sem(&h->mtx, 0x9933);
 		if (RSKTS_PER_BLOCK == l_size(&h->free_idx)) {
			goto done;
		};

		for (i = 0; i < RSKTS_PER_BLOCK; i++) {
			if (rskt_uninit == h->skts[i].st) {
				continue;
			};

			if (NULL == h->skts[i].sock) {
				continue;
			};
			if (sn == h->skts[i].sock->sa.sn) {
				skt_h = h->skts[i].handle;
				break;
			};
		}
done:
	sem_post(&h->mtx);
	return skt_h;
};
struct rskt_socket_t *rsktl_sock_ptr(struct rskt_block *h, rskt_h skt_h)
{
 	if (!VALID_H(skt_h, h)) {
		goto fail;
	};

	return h->skts[SKT_IDX(skt_h)].sock;
fail:
	return NULL;
}

enum rskt_state rsktl_get_st(struct rskt_block *h, rskt_h skt_h)
{
 	if (!VALID_H(skt_h, h)) {
		goto fail;
	};

	return h->skts[SKT_IDX(skt_h)].st;
fail:
	return rskt_max_state;
}

int rsktl_set_st(struct rskt_block *h, rskt_h skt_h, enum rskt_state st)
{
	if (st >= rskt_max_state) {
		errno = EINVAL;
		goto fail;
	};

	if (librskt_wait_for_sem(&h->mtx, 0x4388)) {
		goto fail;
	};
	if (!VALID_H(skt_h, h)) {
		goto fail_locked;
	};
	h->skts[SKT_IDX(skt_h)].st = st;
	sem_post(&h->mtx);
	return 0;
fail_locked:
	sem_post(&h->mtx);
fail:
	return -1;
};

enum rskt_state rsktl_atomic_set_st(struct rskt_block *h, rskt_h skt_h,
				enum rskt_state old, enum rskt_state st)
{
	enum rskt_state rc_st = rskt_max_state;

	if ((old >= rskt_max_state) || (st >= rskt_max_state)) {
		errno = EINVAL;
		goto fail;
	};

	if (librskt_wait_for_sem(&h->mtx, 0x4388)) {
		goto fail;
	};
	if (!VALID_H(skt_h, h)) {
		goto fail_locked;
	};
	if (h->skts[SKT_IDX(skt_h)].st == old) {
		h->skts[SKT_IDX(skt_h)].st = st;
	};
	rc_st = h->skts[SKT_IDX(skt_h)].st;
fail_locked:
	sem_post(&h->mtx);
fail:
	return rc_st;
};

bool rsktl_atomic_set_wip(struct rskt_block *h, rskt_h skt_h, bool old, bool st)
{
	bool rc = st;
	uint64_t idx;

	if (NULL == h) {
		errno = EINVAL;
		goto fail;
	};
	errno = EINVAL;
	if (librskt_wait_for_sem(&h->mtx, 0x4399)) {
		goto fail;
	};
	if (!VALID_H(skt_h, h)) {
		errno = EINVAL;
		goto fail_locked;
	};

	idx = SKT_IDX(skt_h);
	if (st) {
		rc = (old == h->skts[idx].wip);
		if (rc) {
			h->skts[idx].wip = (rskt_connected == h->skts[idx].st);
		};
	} else {
		h->skts[idx].wip = false;
	};
		
	sem_post(&h->mtx);
	return rc;

fail_locked:
	sem_post(&h->mtx);
fail:
	return false;
};

bool rsktl_atomic_set_rip(struct rskt_block *h, rskt_h skt_h, bool old, bool st)
{
	bool rc = st;
	uint64_t idx;

	if (librskt_wait_for_sem(&h->mtx, 0x43AA)) {
		goto fail;
	};

	if (!VALID_H(skt_h, h)) {
		errno = EINVAL;
		goto fail_locked;
	};

	idx = SKT_IDX(skt_h);
	if (st) {
		rc = (old == h->skts[idx].rip);
		if (rc) {
			h->skts[idx].rip =
				(rskt_connected == h->skts[idx].st) ||
				(rskt_close_by_remote == h->skts[idx].st) ||
				(rskt_closing == h->skts[idx].st);
		};
	} else {
		h->skts[idx].rip = false;
	};
		
	sem_post(&h->mtx);
	return rc;

fail_locked:
	sem_post(&h->mtx);
fail:
	return false;
};

int rsktl_atomic_chk_flush(struct rskt_block *h, rskt_h skt_h)
{
	int rc = -1;
	struct rskt_socket_t *skt;

	if (librskt_wait_for_sem(&h->mtx, 0x43BB)) {
		goto fail;
	};

	skt = rsktl_sock_ptr(h, skt_h);
	if (NULL == skt) {
		rc = -1;
		goto fail_locked;
	};
	if (!skt->msub_sz || (NULL == skt->msub_p)) {
		rc = -1;
		goto fail_locked;
	};
	rc = DMA_FLUSHED(skt)?1:0;
		
fail_locked:
	sem_post(&h->mtx);
fail:
	return rc;
};

#ifdef __cplusplus
}
#endif

