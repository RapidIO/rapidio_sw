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

#ifndef LIBRDMA_H
#define LIBRDMA_H

#include <time.h>
#include <errno.h>

#include "rdma_types.h"
#include "libfmdd.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Set fabric management handle
 *
 * @param[in] dd_h Sets the FMD handle for use by all code in this process.
 */
void rdma_set_fmd_handle(fmdd_h dd_h);

/********** Interface  functions **********************/

/**
 * rdma_create_mso_h
 *
 * Create a memory space owner and return a handle thereto
 *
 * @param[in] owner_name Name of the memory space owner
 * @param[out] msoh      Unique handle which is used to group ownership
 *                       of memory spaces
 *
 * @return 0 if successful
 */
int rdma_create_mso_h(const char *owner_name, mso_h *msoh);

/**
 * rdma_open_mso_h
 *
 * Open an existing memory space owner and return a handle thereto
 *
 * @param[in]  owner_name: Name of the memory space owner
 * @param[out] msoh      : Unique handle which is used to group
 *                         ownership of memory spaces
 *
 * @return 0 if successful, < 0 if unsuccessful
 */
int rdma_open_mso_h(const char *owner_name, mso_h *msoh);

/**
 * rdma_close_mso_h
 *
 * Close previously-opened memory space owner
 *
 * @param[in] msoh: Memory space owner handle
 *
 * @return 0 if successful
 */
int rdma_close_mso_h(mso_h msoh);

/**
 * rdma_destroy_mso_h
 *
 * Destroys the memory space owner
 *
 * @param[in] msoh: Memory space owner handle
 *
 * @return 0 if successful
 */
int rdma_destroy_mso_h(mso_h msoh);

/**
 * Create memory space
 *
 * Creates a memory space representing a memory space of specified size
 * owned by specified owner.
 *
 * @param[in] ms_name Memory space name
 * @param[in] msoh: Memory space owner handle
 * @param[in] req_bytes: Length, in bytes, of desired memory space
 * @param[in] flags: Flags. TBD.
 * @param[in] msh: Unique handle used to identify the created memory space
 * @param[in] bytes: Actual number of bytes allocated for memory space
 *
 * @return 0 if successful
 */
int rdma_create_ms_h(const char *ms_name,
		     mso_h msoh,
		     uint32_t req_bytes,
		     uint32_t flags,
		     ms_h *msh,
		     uint32_t *bytes);

/**
 * rdma_open_ms_h
 *
 * Opens an existing memory space owned by specified owner
 *
 * @param[in] ms_name Memory space name
 * @param[in] msoh: Memory space owner handle
 * @param[in] flags: Flags. TBD.
 * @param[in] bytes: [OUT] Length, in bytes, of memory space to be opened
 * @param[in] msh: [OUT] Unique handle used to identify the created memory space
 *
 * @return 0 if successful
 */
int rdma_open_ms_h(const char *ms_name,
		   mso_h msoh,
		   uint32_t flags,
		   uint32_t *bytes,
		   ms_h *msh);

/**
 * rdma_close_ms_h
 *
 * Close an open memory space
 *
 * @param[in] msoh: Memory space owner handle
 * @param[in] msh: Memory space handle
 *
 * @return  0 if successful
*/
int rdma_close_ms_h(mso_h msoh, ms_h msh);

/**
 * Destroy memory space
 *
 * Requests that the memory space owned by “mso” and identified by “msh”
 * be returned to the RDMA memory pool.
 *
 * @param[in] msoh: Memory space owner handle
 * @param[in] msh: Memory space handle
 *
 * @return  0 if successful
*/
int rdma_destroy_ms_h(mso_h msoh, ms_h msh);

/**
 *rdma_create_msub_h
 *
 * Creates a memory sub-space of specified length within the specified memory
 * space. Then maps the address to a virtual address and returns a pointer to 
 * that space.
 *
 * @param[in] msh:    Handle for memory space in which sub-space is to be created
 * @param[in] offset  Offset from start of memory space
 * @param[in] req_bytes: Length, in bytes, of desired memory sub-space
 * @param[in] flags: Flags.
 * @param[in] msubh: [OUT] Unique handle used to identify the created memory sub-space
 *
 * @return 0 if successful
 */
int rdma_create_msub_h(ms_h msh,
		       uint32_t offset,
		       uint32_t req_bytes,
		       uint32_t flags,
		       msub_h *msubh);

/**
 * rdma_destroy_msub_h
 *
 * Destroys memory sub-space specified by specified memory space &
 * memory sub-space handles.
 * The space is returned to the memory space containing the sub-space.
 * The virtual space corresponding to the sub-space is unmapped.
 *
 * @param[in] msh: Handle for memory space 
 * @param[in] msubh: Handle representing the memory sub-space to be destroyed
 *
 * @return 0 if successful
 */
int rdma_destroy_msub_h(ms_h msh, msub_h msubh);

/**
 * Memory map a memory sub-handle
 *
 * Memory maps the physical space of a memory sub-space into
 * virtual space and provides a pointer thereto.
 *
 * @param[in] msubh: Memory sub-space handle
 * @param[in] vaddr: [OUT] Pointer for holding virtual address
 *
 * @return 0 if successful
 */
int rdma_mmap_msub(msub_h msubh, void **vaddr);

/**
 * Memory un-map a memory sub-handle's virtual address
 *
 * @param[in] msubh: Memory sub-space handle
 * @param[in] vaddr: Pointer to virtual address mapping memory subspace
 *
 * @return 0 if successful
 */
int rdma_munmap_msub(msub_h msubh, void *vaddr);

/**
 * Accept a connection to memory space
 *
 * @param[in] msh: Handle for memory space 
 * @param[in] loc_msubh: Handle to created local memory subspace in database
 * @param[out] connh: Connection handle
 * @param[out] rem_msubh: Handle to received remote memory subspace in database
 * @param[out] rem_msub_len: Length in bytes of remote memory subspace
 * @param[in] timeout_secs: timeout in seconds after which function returns
 * regardless of success
 *
 * @return 0 if successful
 */
int rdma_accept_ms_h(ms_h msh,
		     msub_h loc_msubh,
		     conn_h *connh,
		     msub_h *rem_msubh,
		     uint32_t *rem_msub_len,
		     uint64_t timeout_secs);

/**
 * Connect to a remote memory space
 *
 * Requests that this process/thread be given access to the memory space
 * identified by name/destid. Sends a CM message to server including
 * info such as rio_addr, destid, local msubh, and local msub length.
 * Receives remote msubh information including length & rio_addr, 
 * also via a CM, as well as the remote memory space handle.
 *
 * @param[in] destid_len: Size of destination ID of node hosting the memory space
 * @param[in] destid: Destination ID of node hosting the memory space msh
 * @param[out] connh: Connection handle
 * @param[in] rem_msname: Remote memory space name
 * @param[in] loc_msubh: Handle to created local memory subspace,
 * 			 0 if not provided
 * @param[out] rem_msubh: Handle to remote memory subspace provided by server
 * @param[out] rem_msub_len: Remote memory subspace length in bytes
 * @param[out] rem_msh: Handle to remote memory space provided by server
 * @param[in] timeout_secs: Timeout in seconds, after which connection has failed
 *
 * @return 0 if successful
 */
int rdma_conn_ms_h(uint8_t destid_len,
		   uint32_t destid,
		   const char *rem_msname,
		   msub_h loc_msubh,
		   conn_h *connh,
		   msub_h *rem_msubh,
		   uint32_t *rem_msub_len,
		   ms_h	  *rem_msh,
		   uint64_t timeout_secs);

/**
 * Disconnect [from] a remote memory space
 *
 * Requests that this process/thread, should no longer have access to 
 * the server memory space identified by server_msh.
 * The client database is cleared of all the server subspaces
 * belonging to server_msh, and further transactions to those subspaces
 * will fail.
 *
 * @param[in] connh		Connection handle
 * @param[in] server_msh	Server memory space to disconnect from
 * @param[in] client_msubh	Client memory subspace provided during
 * 				connection request, 0 if not provided.
 *
 * @return 0 if successful
 */
int rdma_disc_ms_h(conn_h connh, ms_h server_msh, msub_h client_msubh);

/* DMA transfer synchronization types */
typedef enum {
      rdma_no_wait,   /* Return immediately. No completion indication (FAF) */
      rdma_async_chk, /* Return immediately, request completion indication handle */
      rdma_sync_chk   /* Return only when transfer has completed (blocking). */
} rdma_sync_type_t;

/* DMA transfer check handle */
typedef uint32_t rdma_chk_handle;

struct rdma_xfer_ms_in {
	msub_h loc_msubh;	/* Local subspace handle */
	uint32_t loc_offset;	/* Offset within local subspace */
	int num_bytes;		/* Number of bytes to transfer */
	msub_h rem_msubh;	/* Remote subspace handle */
	int rem_offset;		/* Offset within remote subspace */
	int priority;		/* Rapid I/O priority */
	rdma_sync_type_t sync_type; /* Sync type */
};

struct rdma_xfer_ms_out {
	int in_param_ok;	/* Result of input parameter checking, 0==good */
	int dma_xfr_status;	/* Result of DMA transfer ('rdma_sync_chk') */
	rdma_chk_handle   chk_handle; /* Handle to check completion of async xfers */
};

/**
 * rdma_push_msub
 *
 * Transfer data from local msub to remote msub in accordance with in_params
 *
 * @param[in] in	Transfer parameters
 * @param[in] out	Transfer result
 *
 * @return 0 if successful
 */
int rdma_push_msub(const struct rdma_xfer_ms_in *in, struct rdma_xfer_ms_out *out);

/**
 * rdma_pull_msub
 *
 * Transfer data from remote msub to local msub in accordance with in_params
 *
 * @param[in] in_parms	Transfer parameters
 * @param[out] out	Transfer result
 *
 * @return 0 if successful
 */
int rdma_pull_msub(const struct rdma_xfer_ms_in *in_parms, struct rdma_xfer_ms_out *out);

/**
 * rdma_push_buf
 *
 * Transfer data from local buffer to remote msub
 *
 * @param[in] buf		Local buffer containing outgoing data
 * @param[in] num_bytes	Number of bytes to transfer
 * @param[in] rem_msubh	Remote subspace handle
 * @param[in] rem_offset	Offset within remote subspace
 * @param[in] priority	Rapid I/O priority
 * @param[in] sync_type	Sync type
 * @param[out] out		Transfer result
 *
 * @return 0 if successful
 */
int rdma_push_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out);

/**
 * rdma_pull_buf
 *
 * Transfer data from remote msub to local buffer
 *
 * @param[in] buf		Local buffer for storing incoming data
 * @param[in] num_bytes	Number of bytes to transfer
 * @param[in] rem_msubh	Remote subspace handle
 * @param[in] rem_offset	Offset within remote subspace
 * @param[in] priority	Rapid I/O priority
 * @param[in] sync_type	Sync type
 * @param[in] out		Transfer result
 *
 * @return 0 if successful
 */
int rdma_pull_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out);

/**
 * rdma_sync_chk_push_pull
 *
 * Synchronizes application to completion of a push or pull operation
 * associated with the chk_handle.
 *
 * rdma_sync_chk_push_pull returns when there is a timeout or when
 * the transfer  has completed.
 *
 * Note: completion of a transfer could be unsuccessful.
 *
 * @param[in] chk_handle	Handle for checking completion of last async transfer
 * @param[in] wait	Timeout value
 *
 * @return 0 if successful
 */
int rdma_sync_chk_push_pull(rdma_chk_handle chk_handle,
			    const struct timespec *wait);

#ifdef __cplusplus
}
#endif

#endif /* LIBRDMA_H */
