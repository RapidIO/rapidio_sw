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

#ifndef __LIBRSKT_LIST_H__
#define __LIBRSKT_LIST_H__

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "librskt_socket.h"

typedef struct rskt_list *rskt_list_h;

#define RSKTS_PER_BLOCK 256

#define SKT_IDX(x) (x & (rskt_h)(RSKTS_PER_BLOCK - 1))

struct rskt_entry {
        struct rskt_socket_t *sock;
        enum rskt_state st;
        bool wip; /* Write in progress */
        bool rip; /* REad in progress */;
	uint64_t handle;
};

struct rskt_block {
	sem_t mtx; /* Mutex for access to remainder of structure fields */
        volatile struct rskt_entry skts[RSKTS_PER_BLOCK];
	struct l_head_t free_idx;
	uint64_t handle_num;
};

/** @brief Initialize list of rskts.
 *
 * @param[in] h Pointer to block of rskt info
 * 
 * @return status of the function call
 * @retval 0 on success
 * @retval -1, with errno on failure   
 */
int rsktl_init(struct rskt_block *h);

/** @brief Allocate and initialize an RSKT handle entry
 *
 * @return Success/failure indication
 * @retval 0 on success   
 * @retval -1, with errno on failure   
 */

rskt_h rsktl_get_socket(struct rskt_block *h);

/** @brief Free and release an RSKT handle entry, invalidate handle
 *
 * @return Success/failure indication
 * @retval 0 on success   
 * @retval -1, with errno on failure   
 */
int rsktl_put_socket(struct rskt_block *h, rskt_h skt_h);

/** @brief Find a rskt_h based on the socket number
 *
 * @param[in] rskt_h handle for the socket
 *
 * @return Pointer to socket data structure
 * @retval Non-NULL on success
 * @retval NULL, with errno on error
 *
 */
rskt_h rsktl_find_skt_h(struct rskt_block *h, uint32_t sn);

/** @brief Translate from rskt_h to * struct rskt_socket_t
 *
 * @param[in] rskt_h handle for the socket
 *
 * @return Pointer to socket data structure
 * @retval Non-NULL on success
 * @retval NULL, with errno on error
 *
 */
struct rskt_socket_t *rsktl_sock_ptr(struct rskt_block *h, rskt_h skt_h);

/** @brief Get current socket state
 *
 * @param[in] rskt_h handle for the socket
 *
 * @return Pointer to socket data structure
 * @retval Non-NULL on success
 * @retval NULL, with errno on error
 *
 */
enum rskt_state rsktl_get_st(struct rskt_block *h, rskt_h skt_h);

/** @brief Set current socket state
 *
 * @param[in] rskt_h handle for the socket
 *
 * @return Integeter
 * @retval 0 on success
 * @retval -1, with errno on error
 *
 */
int rsktl_set_st(struct rskt_block *h, rskt_h skt_h, enum rskt_state st);

/** @brief Atomically change socket state from one value to another
 *
 * @param[in] rskt_h handle for the socket
 * @param[in] old Expected old state of the socket
 * @param[in] st Requested new state of the socket
 *
 * @return Current socket state
 * @retval Matches st on success
 * @retval rskt_max_state, with errno on error
 *
 */
enum rskt_state rsktl_atomic_set_st(struct rskt_block *h, rskt_h skt_h,
				enum rskt_state old, enum rskt_state st);

/** @brief Atomically change write_in_progress state from one value to another.
 * - write_in_progress can only be set to "true" if the socket is in a valid
 * state to allow the connection...
 * - Write_in_progress can be set to "false" whatever the state of the socket
 *
 * @param[in] rskt_h handle for the socket
 * @param[in] old Expected old write_in_progress state
 * @param[in] st Requested new write_in_progress state
 *
 * @return Current write_in_progress state
 * @retval Matches st on success
 * @retval Matches old on failure
 */
bool rsktl_atomic_set_wip(struct rskt_block *h, rskt_h skt_h,
				bool old, bool st);

/** @brief Atomically change read_in_progress state from one to another
 *
 * @param[in] rskt_h handle for the socket
 * @param[in] old Expected old read_in_progress state
 * @param[in] st Requested new read_in_progress state
 *
 * @return Current read_in_progress state
 * @retval Matches st on success
 * @retval Matches old on failure
 */
bool rsktl_atomic_set_rip(struct rskt_block *h, rskt_h skt_h,
				bool old, bool st);

/** @brief Atomically check if remote writes to the socket buffers have
 *  been flushed.
 *
 * @param[in] h handle for rskt state block
 * @param[in] rskt_h handle for the socket
 *
 * @return Current flush status
 * @retval 0 not flushed
 * @retval > 0 means flushed
 * @retval < 0 means error, handle is no longer valid (closed by another thread)
 */
int rsktl_atomic_chk_flush(struct rskt_block *h, rskt_h skt_h);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_LIST_H__ */

