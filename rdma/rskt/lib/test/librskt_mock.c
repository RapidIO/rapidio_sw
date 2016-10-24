/* Mock procedures for librskt tests.  No threaded routines are tested */
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

#include "librskt_states.h"
#include "librskt_buff.h"
#include "librskt_socket.h"
#include "librskt_list.h"
#include "librskt_info.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "libcli.h"
#include "librskt.h"
#include "rapidio_mport_mgmt.h"

int test_fail; /* Global "test fail" indication set by mock routines */

/* Routine to make it easy to catch when test_fail is set...
 */
void set_test_fail(int i)
{ 
	test_fail = i;
};

int socket_rc;
int socket_errno;
int socket(int family, int family_type, int rpt)
{
	if (0) {
		socket_rc = family + family_type + rpt;
	};
	errno = socket_errno;
	return socket_rc;
};

int connect_rc;
int connect_errno;
int connect (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
{
	if (0) {
		connect_rc = __fd + __len;
		if (NULL == __addr) {
			connect_rc++;
		};
	};
	errno = connect_errno;
	return connect_rc;
};

int close_rc;
int close_errno;

int close(int fd)
{
	if (0) {
		if (fd) {
			return -1;
		};
	};
	errno = close_errno;
	return close_rc;
};

int free_app2d_errno;
int free_app2d_rc;
int free_app2d_ptr_idx;
struct librskt_app_to_rsktd_msg  *free_app2d_ptrs[5];
int free_app2d(struct librskt_app_to_rsktd_msg *ptr)
{
	int i;
	if (0) {
		if (NULL == ptr) {
			return 0;
		};
	};
	if (free_app2d_ptr_idx < 5) {
		free_app2d_ptrs[free_app2d_ptr_idx] = ptr;
		for (i = 0; i < free_app2d_ptr_idx; i++) {
			if (free_app2d_ptrs[i] == ptr) {
				set_test_fail(1);
			};
		};
		free_app2d_ptr_idx++;
	} else {
		set_test_fail(2);
	};
	errno = free_app2d_errno;
	return free_app2d_rc;
};

struct librskt_rsktd_to_app_msg *alloc_d2app_rc;
int alloc_d2app_errno;
struct librskt_rsktd_to_app_msg *alloc_d2app(void)
{
	errno = alloc_d2app_errno;
	return alloc_d2app_rc;
};

struct librskt_app_to_rsktd_msg *alloc_app2d_rc;
int alloc_app2d_errno;
struct librskt_app_to_rsktd_msg *alloc_app2d(void)
{
	errno = alloc_app2d_errno;
	return alloc_app2d_rc;
};

int free_d2app_errno;
int free_d2app_rc;
int free_d2app_ptr_idx;
struct librskt_rsktd_to_app_msg  *free_d2app_ptrs[5];

int free_d2app(struct librskt_rsktd_to_app_msg *ptr)
{
	int i;
	if (0) {
		if (NULL == ptr) {
			return 0;
		};
	};
	if (free_d2app_ptr_idx < 5) {
		free_d2app_ptrs[free_d2app_ptr_idx] = ptr;
		for (i = 0; i < free_d2app_ptr_idx; i++) {
			if (free_d2app_ptrs[i] == ptr) {
				set_test_fail(1);
			};
		};
		free_d2app_ptr_idx++;
	} else {
		set_test_fail(2);
	};
	errno = free_d2app_errno;
	return free_d2app_rc;
}

int librskt_init_threads_rc;
int librskt_init_threads(void)
{
	return librskt_init_threads_rc;
};


void librskt_finish_threads(void)
{
};

int librskt_wait_for_sem_rc;
int librskt_wait_for_sem_errno;
int librskt_wait_for_sem(sem_t *sema, int err_code)
{
	if (0) {
		if (NULL == sema) {
			return -1;
		};
		return err_code;
	};
	errno = librskt_wait_for_sem_errno;
	return librskt_wait_for_sem_rc;
};

int riomp_mgmt_mport_create_handle_rc;
int riomp_mgmt_mport_create_handle_errno;
int riomp_mgmt_mport_create_handle(uint32_t mport_id,
					int flags, riomp_mport_t *mport_handle)
{
	if (0) {
		if (flags || (NULL == mport_handle)) {
			return 0;
		};
		return mport_id;
	};
	errno = riomp_mgmt_mport_create_handle_errno;
	return riomp_mgmt_mport_create_handle_rc;
};

int librskt_dmsg_req_resp_rc;
int librskt_dmsg_req_resp_errno;
struct librskt_rsktd_to_app_msg librskt_rsktd_to_app_msg_rx;

int librskt_dmsg_req_resp(struct librskt_app_to_rsktd_msg *tx,
                        struct librskt_rsktd_to_app_msg *rx,
                        bool chk_rsp_to)
{
	if (0) {
		if ((NULL == tx) || chk_rsp_to) {
			return 0;
		}
	}
	if (NULL != rx) {
		memcpy((void *)rx, (void *)&librskt_rsktd_to_app_msg_rx,
			sizeof(struct librskt_rsktd_to_app_msg));
	};
	errno = librskt_dmsg_req_resp_errno;
	return librskt_dmsg_req_resp_rc;
};

int librskt_dmsg_tx_resp_rc;
int librskt_dmsg_tx_resp_errno;
int librskt_dmsg_tx_resp(struct librskt_app_to_rsktd_msg *tx)
{
	if (0) {
		if (NULL == tx) {
			return -1;
		};
	};
	errno = librskt_dmsg_tx_resp_rc;
	return librskt_dmsg_tx_resp_errno;
};

int riomp_dma_write_d_idx;
struct riomp_dma_write_d_rc_type {
	int rc;
	int err_no;
	uint32_t loc_tx_wr_flags;
	uint32_t loc_rx_rd_flags;
	uint32_t rem_rx_wr_flags;
	uint32_t rem_tx_rd_flags;
};
struct riomp_dma_write_d_rc_type *riomp_dma_write_d_rc;

int riomp_dma_write_d(riomp_mport_t mport_handle,
		uint16_t destid,
		uint64_t tgt_addr,
		uint64_t handle,
		uint32_t offset,
		uint32_t size,
		enum riomp_dma_directio_type wr_mode,
		enum riomp_dma_directio_transfer_sync sync)
{
	struct rskt_buf_hdr *hdr = (struct rskt_buf_hdr *)handle;
	int idx = riomp_dma_write_d_idx;

	if (0) {
		if (wr_mode || sync) {
			return -1;
		};
		return destid + tgt_addr + offset + size;
	};

	riomp_dma_write_d_idx++;
	hdr->loc_tx_wr_flags |= riomp_dma_write_d_rc[idx].loc_tx_wr_flags;
	hdr->loc_rx_rd_flags |= riomp_dma_write_d_rc[idx].loc_rx_rd_flags;
	hdr->rem_rx_wr_flags |= riomp_dma_write_d_rc[idx].rem_rx_wr_flags;
	hdr->rem_tx_rd_flags |= riomp_dma_write_d_rc[idx].rem_tx_rd_flags;
	
	errno = riomp_dma_write_d_rc[idx].err_no;
	return riomp_dma_write_d_rc[idx].rc;
};

int rdma_open_mso_h_rc;
int rdma_open_mso_h_errno;
int rdma_open_mso_h(const char *owner_name, mso_h *msoh)
{
	if (0) {
		if ((NULL == owner_name) || (NULL == msoh)) {
			return -1;
		};
	};
	errno = rdma_open_mso_h_errno;
	return rdma_open_mso_h_rc;
};

int rdma_open_ms_h_rc;
int rdma_open_ms_h_errno;
int rdma_open_ms_h(const char *ms_name,
                   mso_h msoh,
                   uint32_t flags,
                   uint32_t *bytes,
                   ms_h *msh)
{
	if (0) {
		if ((NULL == ms_name) || (NULL == bytes) || (NULL == msh)) {
			return -1;
		};
		return msoh + flags;
	};
	errno = rdma_open_mso_h_errno;
	return rdma_open_mso_h_rc;
};

int rdma_create_msub_h_rc;
int rdma_create_msub_h_errno;
int rdma_create_msub_h(ms_h msh,
                       uint32_t offset,
                       uint32_t req_bytes,
                       uint32_t flags,
                       msub_h *msubh)
{
	if (0) {
		if (NULL == msubh) {
			return -1;
		};
		return msh + offset + req_bytes + flags;
	};
	errno = rdma_create_msub_h_errno;
	return rdma_create_msub_h_rc;
};

int rdma_mmap_msub_rc;
int rdma_mmap_msub_errno;
int rdma_mmap_msub(msub_h msubh, void **vaddr)
{
	if (0) {
		if (NULL == vaddr) {
			return -1;
		};
		return msubh;
	};
	errno = rdma_mmap_msub_errno;
	return rdma_mmap_msub_rc;
};

int rdma_munmap_msub_rc;
int rdma_munmap_msub_errno;
int rdma_munmap_msub(msub_h msubh, void *vaddr)
{
	if (0) {
		if (NULL == vaddr) {
			return -1;
		};
		return msubh;
	};
	errno = rdma_munmap_msub_errno;
	return rdma_munmap_msub_rc;
};

int rdma_accept_ms_h_rc;
int rdma_accept_ms_h_errno;
int rdma_accept_ms_h(ms_h msh,
                     msub_h loc_msubh,
                     conn_h *connh,
                     msub_h *rem_msubh,
                     uint32_t *rem_msub_len,
                     uint64_t timeout_secs)
{
	if (0) {
		if (NULL == connh) {
			return -1;
		};
		if ((NULL == rem_msubh) || (NULL == rem_msub_len)) {
			return -1;
		};
		return msh + loc_msubh + timeout_secs;
	};
	errno = rdma_accept_ms_h_errno;
	return rdma_accept_ms_h_rc;
};

int rdma_conn_ms_h_rc;
int rdma_conn_ms_h_errno;
int rdma_conn_ms_h(uint8_t destid_len,
                   uint32_t destid,
                   const char *rem_msname,
                   msub_h loc_msubh,
                   conn_h *connh,
                   msub_h *rem_msubh,
                   uint32_t *rem_msub_len,
                   ms_h   *rem_msh,
                   uint64_t timeout_secs)
{
	if (0) {
		if ((NULL == rem_msname) || (NULL == connh)) {
			return -1;
		};
		if ((NULL == rem_msubh) || (NULL == rem_msub_len)) {
			return -1;
		};
		if (NULL == rem_msh) {
			return -1;
		};
		return destid_len + destid + loc_msubh + timeout_secs;
	};
	errno = rdma_conn_ms_h_errno;
	return rdma_conn_ms_h_rc;
};

int rdma_push_msub_rc;
int rdma_push_msub_errno;
int rdma_push_msub(const struct rdma_xfer_ms_in *in,
						struct rdma_xfer_ms_out *out)
{
	if (0) {
		if ((NULL == in) || (NULL == out)) {
			return -1;
		};
	};
	errno = rdma_push_msub_errno;
	return rdma_push_msub_rc;
};

int rdma_disc_ms_h_rc;
int rdma_disc_ms_h_errno;
int rdma_disc_ms_h(conn_h connh, ms_h server_msh, msub_h client_msubh)
{
	if (0) {
		if (connh || server_msh || client_msubh) {
			return -1;
		};
	};
	errno = rdma_disc_ms_h_errno;
	return rdma_disc_ms_h_rc;
};

int rdma_destroy_msub_h_rc;
int rdma_destroy_msub_h_errno;
int rdma_destroy_msub_h(ms_h msh, msub_h msubh)
{
	if (0) {
		if (msh || msubh) {
			return -1;
		};
	};
	errno = rdma_destroy_msub_h_errno;
	return rdma_destroy_msub_h_rc;
};

int rdma_close_ms_h_rc;
int rdma_close_ms_h_errno;
int rdma_close_ms_h(mso_h msoh, ms_h msh)
{
	if (0) {
		if (msh || msoh) {
			return -1;
		};
	};
	errno = rdma_close_ms_h_errno;
	return rdma_close_ms_h_rc;
};

int riomp_dma_map_memory_rc;
int riomp_dma_map_memory_errno;
void *riomp_dma_map_memory_vaddr;
int riomp_dma_map_memory(riomp_mport_t mport_handle, size_t size, off_t paddr, void **vaddr)
{
	if (0) {
		if (NULL == vaddr) {
			return size + paddr;
		};
	};
	*vaddr = riomp_dma_map_memory_vaddr;
	errno = riomp_dma_map_memory_errno;
	return riomp_dma_map_memory_rc;
};

int riomp_dma_unmap_memory_rc;
int riomp_dma_unmap_memory_errno;
int riomp_dma_unmap_memory(riomp_mport_t mport_handle, size_t size, void *vaddr)
{
	if (0) {
		if (mport_handle || size || (NULL != vaddr)) {
			return -1;
		};
	};
	errno = riomp_dma_unmap_memory_errno;
	return riomp_dma_unmap_memory_rc;
};

int rsktl_init_rc;
int rsktl_init_errno;
int rsktl_init(struct rskt_block *h)
{
	if (0) {
		if (NULL == h) {
			return -1;
		};
	};
	errno = rsktl_init_errno;
	return rsktl_init_rc;
};

rskt_h rsktl_get_socket_rc;
int rsktl_get_socket_errno;
rskt_h rsktl_get_socket(struct rskt_block *h)
{
	if (0) {
		if (NULL == h) {
			return -1;
		};
	};
	errno = rsktl_get_socket_errno;
	return rsktl_get_socket_rc;
}

rskt_h rsktl_put_socket_rc;
int rsktl_put_socket_errno;
int rsktl_put_socket(struct rskt_block *h, rskt_h skt_h)
{
	if (0) {
		if (NULL == h) {
			return skt_h;
		};
	};
	errno = rsktl_put_socket_errno;
	return rsktl_put_socket_rc;
};

rskt_h rsktl_find_skt_h_rc;
int rsktl_find_skt_h_errno;
rskt_h rsktl_find_skt_h(struct rskt_block *h, uint32_t sn)
{
	if (0) {
		if (NULL == h) {
			return sn;
		};
	};
	errno = rsktl_find_skt_h_errno;
	return rsktl_find_skt_h_rc;
};

struct rskt_socket_t *rsktl_sock_ptr_rc;
int rsktl_sock_ptr_errno;
struct rskt_socket_t *rsktl_sock_ptr(struct rskt_block *h, rskt_h skt_h)
{
	if (0) {
		if ((LIBRSKT_H_INVALID == skt_h) || (NULL == h)) {
			return NULL;
		};
	};
	errno = rsktl_sock_ptr_errno;
	return rsktl_sock_ptr_rc;
};

enum rskt_state rsktl_get_st_rc;
int rsktl_get_st_errno;

enum rskt_state rsktl_get_st(struct rskt_block *h, rskt_h skt_h)
{
	if (0) {
		if ((LIBRSKT_H_INVALID == skt_h) || (NULL == h)) {
			return rskt_max_state;
		};
	};
	errno = rsktl_get_st_errno;
	return rsktl_get_st_rc;
};

int rsktl_set_st_rc;
int rsktl_set_st_errno;
int rsktl_set_st(struct rskt_block *h, rskt_h skt_h, enum rskt_state st)
{
	if (0) {
		if ((LIBRSKT_H_INVALID == skt_h) || (NULL == h)) {
			return (int)st;
		};
	};
	errno = rsktl_find_skt_h_errno;
	return rsktl_find_skt_h_rc;
};

struct rc_entry {
	int rc;
	int errnum;
	uint32_t old;
	uint32_t st;
};

int rsktl_atomic_set_st_rc_idx;
struct rc_entry *rsktl_atomic_set_st_rcs;

enum rskt_state rsktl_atomic_set_st(struct rskt_block *h, rskt_h skt_h,
				enum rskt_state old, enum rskt_state st)
{
	int idx = rsktl_atomic_set_st_rc_idx;
	rsktl_atomic_set_st_rc_idx++;

	if (0) {
		if ((LIBRSKT_H_INVALID == skt_h) || (NULL == h)) {
			return st;
		};
		return old;
	};
	if (((int)old != rsktl_atomic_set_st_rcs[idx].old) ||
				((int)st != rsktl_atomic_set_st_rcs[idx].st)) {
		set_test_fail(1);
	};
	errno = rsktl_atomic_set_st_rcs[idx].errnum;
	return (enum rskt_state)rsktl_atomic_set_st_rcs[idx].rc;
};

int rsktl_atomic_set_wip_rc_idx;
struct rc_entry *rsktl_atomic_set_wip_rcs;
bool rsktl_atomic_set_wip(struct rskt_block *h, rskt_h skt_h,
				bool old, bool st)
{
	int idx = rsktl_atomic_set_wip_rc_idx;
	rsktl_atomic_set_wip_rc_idx++;
	if (0) {
		if ((NULL == h) || old || st) {
			return LIBRSKT_H_INVALID == skt_h;
		};
	};
	if (((uint32_t)old != rsktl_atomic_set_wip_rcs[idx].old) ||
		((uint32_t)st != rsktl_atomic_set_wip_rcs[idx].st)) {
		set_test_fail(1);
	};
	errno = rsktl_atomic_set_wip_rcs[idx].errnum;
	return rsktl_atomic_set_wip_rcs[idx].rc;
};

int rsktl_atomic_set_rip_rc_idx;
struct rc_entry *rsktl_atomic_set_rip_rcs;
bool rsktl_atomic_set_rip(struct rskt_block *h, rskt_h skt_h,
				bool old, bool st)
{
	int idx = rsktl_atomic_set_rip_rc_idx;
	rsktl_atomic_set_rip_rc_idx++;
	if (0) {
		if ((NULL == h) || old || st) {
			return LIBRSKT_H_INVALID == skt_h;
		};
	};
	if (((uint32_t)old != rsktl_atomic_set_rip_rcs[idx].old) ||
		((uint32_t)st != rsktl_atomic_set_rip_rcs[idx].st)) {
		set_test_fail(1);
	};
	errno = rsktl_atomic_set_rip_rcs[idx].errnum;
	return rsktl_atomic_set_rip_rcs[idx].rc;
};

int rsktl_atomic_chk_flush_rc_idx;
struct rc_entry *rsktl_atomic_chk_flush_rcs;
int rsktl_atomic_chk_flush(struct rskt_block *h, rskt_h skt_h)
{
	int idx = rsktl_atomic_chk_flush_rc_idx;
	rsktl_atomic_chk_flush_rc_idx++;
	if (0) {
		if (NULL == h) {
			return LIBRSKT_H_INVALID == skt_h;
		};
	};
	errno = rsktl_atomic_chk_flush_rcs[idx].errnum;
	return rsktl_atomic_chk_flush_rcs[idx].rc;
};

int rskt_accept_msg_rc_idx;
struct rc_entry *rskt_accept_msg_rcs;
int rskt_accept_msg(rskt_h l_skt_h, rskt_h *skt_h, struct rskt_sockaddr *sktaddr)
{
	int idx = rskt_accept_msg_rc_idx;
	rskt_accept_msg_rc_idx++;
	if (0) {
		if ((NULL == skt_h) || (NULL == sktaddr)) {
			return l_skt_h;
		};
	};
	*skt_h = rskt_accept_msg_rcs[idx].old;
	errno = rskt_accept_msg_rcs[idx].errnum;
	return rskt_accept_msg_rcs[idx].rc;
};

int rskt_accept_init_rc_idx;
struct rc_entry *rskt_accept_init_rcs;
int rskt_accept_init(rskt_h l_skt_h, rskt_h skt_h)
{
	int idx = rskt_accept_init_rc_idx;
	rskt_accept_init_rc_idx++;
	if (0) {
		if (LIBRSKT_H_INVALID == l_skt_h) {
			return skt_h;
		};
	};
	errno = rskt_accept_init_rcs[idx].errnum;
	return rskt_accept_init_rcs[idx].rc;
};

int rskt_close_locked_rc_idx;
struct rc_entry *rskt_close_locked_rcs;
int rskt_close_locked(rskt_h skt_h)
{
	int idx = rskt_close_locked_rc_idx;
	rskt_close_locked_rc_idx++;
	if (0) {
		return skt_h;
	};
	errno = rskt_close_locked_rcs[idx].errnum;
	return rskt_close_locked_rcs[idx].rc;
};

int rskt_connect_rdma_open_errnum;
int rskt_connect_rdma_open_rc;
int rskt_connect_rdma_open(struct rskt_socket_t * volatile skt)
{
	if (0) {
		if (NULL == skt) {
			return 1;
		};
	};
	errno = rskt_connect_rdma_open_errnum;
	return rskt_connect_rdma_open_rc;
};

int setup_skt_ptrs_errnum;
int setup_skt_ptrs_rc;
int setup_skt_ptrs(struct rskt_socket_t * volatile skt)
{
	if (0) {
		if (NULL == skt) {
			return 1;
		};
	};
	errno = setup_skt_ptrs_errnum;
	return setup_skt_ptrs_rc;
};

int get_free_bytes_idx;
struct rc_entry *get_free_bytes_rcs;
uint32_t get_free_bytes(volatile struct rskt_buf_hdr *hdr, uint32_t buf_sz)
{
	int idx = get_free_bytes_idx;
	get_free_bytes_idx++;
	if (0) {
		if (NULL == hdr) {
			return buf_sz;
		};
	};
	hdr->loc_tx_wr_flags |= get_free_bytes_rcs[idx].old;
	errno = get_free_bytes_rcs[idx].errnum;
	return get_free_bytes_rcs[idx].rc;
};

int send_bytes_idx;
struct rc_entry *send_bytes_rcs;
int send_bytes(volatile struct rskt_socket_t *skt, void *data, int byte_cnt,
                        struct rdma_xfer_ms_in *hdr_in, int inited)
{
	int idx = send_bytes_idx;
	send_bytes_idx++;
	if (0) {
		if ((NULL == skt) || (NULL == data) || (NULL == hdr_in)) {
			return byte_cnt + inited;
		};
	};
	errno = send_bytes_rcs[idx].errnum;
	return send_bytes_rcs[idx].rc;
};

int update_remote_hdr_errnum;
int update_remote_hdr_rc;
int update_remote_hdr(struct rskt_socket_t * volatile skt,
                        struct rdma_xfer_ms_in *hdr_in)
{
	if (0) {
		if ((NULL == skt) || (NULL == hdr_in)) {
			return 0;
		};
	};
	errno = update_remote_hdr_errnum;
	return update_remote_hdr_rc;
};

int get_avail_bytes_idx;
struct rc_entry *get_avail_bytes_rcs;
int get_avail_bytes(struct rskt_buf_hdr volatile *hdr, uint32_t buf_sz)
{
	int idx = get_avail_bytes_idx;
	get_avail_bytes_idx++;
	if (0) {
		if (NULL == hdr) {
			return buf_sz;
		};
	};
	errno = get_avail_bytes_rcs[idx].errnum;
	return get_avail_bytes_rcs[idx].rc;
};

int read_bytes_idx;
struct rc_entry *read_bytes_rcs;
void read_bytes(struct rskt_socket_t *skt, void *data, uint32_t byte_cnt)
{
	int idx = read_bytes_idx;
	read_bytes_idx++;
	if (0) {
		if ((NULL == skt) || (NULL == data) || (!byte_cnt)) {
			return;
		};
	};
	if (byte_cnt != (uint32_t)read_bytes_rcs[idx].rc) {
		set_test_fail(1);
	};
};

void rskt_init_memops(struct rskt_socket_t * volatile skt)
{
	if (0) {
		if (NULL == skt)
			return;
	};
	return;
};


#ifdef __cplusplus
}
#endif

