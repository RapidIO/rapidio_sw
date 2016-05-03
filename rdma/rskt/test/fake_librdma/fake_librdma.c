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

#include "librdma.h"
#include "rdma_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void rdma_set_fmd_handle(fmdd_h dd_h)
{
	if (0)
		*(int *)dd_h = 0;

};

int rdma_create_mso_h(const char *owner_name, mso_h *msoh)
{
	if (0) {
		*msoh = 0;
		if (NULL == owner_name)
			return 0;
	};

	return 0;
}

int rdma_open_mso_h(const char *owner_name, mso_h *msoh)
{
	if (0) {
		if ((NULL == owner_name) || (NULL == msoh))
			return 0;
	};

	return 0;
}

int rdma_close_mso_h(mso_h msoh)
{
	if (0) 
		return msoh * 0;
	return 0;
};

int rdma_destroy_mso_h(mso_h msoh)
{
	if (0)
		return msoh * 0;
	return 0;
};

struct loc_ms {
        char *name;
        uint32_t   bytes;
        void *addr;
};

int rdma_create_ms_h(const char *ms_name,
		     mso_h msoh,
		     uint32_t req_bytes,
		     uint32_t flags,
		     ms_h *msh,
		     uint32_t *bytes)
{
	struct loc_ms *lms;

	if (0) {
		if (NULL == ms_name)
			return msoh * 0;
		*bytes = req_bytes + flags;
		*msh = 0;
	};

	lms = (struct loc_ms *)malloc(sizeof(struct loc_ms));
	lms->name = (char *)ms_name;
	lms->bytes = *bytes;
	lms->addr = malloc(*bytes);

	*msh = (uint64_t)lms;

	return 0;
};

int rdma_open_ms_h(const char *ms_name,
		   mso_h msoh,
		   uint32_t flags,
		   uint32_t *bytes,
		   ms_h *msh)
{
	struct loc_ms *lms;

	if (0) {
		if (NULL == ms_name)
			*bytes = msoh + flags;
		*msh = 0;
	};

	lms = (struct loc_ms *)malloc(sizeof(struct loc_ms));
	lms->name = (char *)ms_name;
	lms->bytes = *bytes;
	lms->addr = malloc(*bytes);

	*msh = (uint64_t)lms;

	return 0;
};


int rdma_close_ms_h(mso_h msoh, ms_h msh)
{
	struct loc_ms *lms = (struct loc_ms *)msh;

	free(lms->addr);
	return 0 * msoh;
};

int rdma_destroy_ms_h(mso_h msoh, ms_h msh)
{
	return msoh * msh * 0;
};

int rdma_create_msub_h(ms_h msh,
		       uint32_t offset,
		       uint32_t req_bytes,
		       uint32_t flags,
		       msub_h *msubh)
{
	if (0) {
		*msubh = msh + offset + req_bytes + flags;
	};

	*msubh = msh;
	return 0;
};

int rdma_destroy_msub_h(ms_h msh, msub_h msubh)
{
	return msh * msubh * 0;
};

int rdma_mmap_msub(msub_h msubh, void **vaddr)
{
	if (msubh)
		*vaddr = ((struct loc_ms *)msubh)->addr;
	else 
		*vaddr = NULL;

	return msubh * 0;
};

int rdma_munmap_msub(msub_h msubh, void *vaddr)
{
	if (vaddr) 
		return 0;
	return msubh * 0;
};

int rdma_accept_ms_h(ms_h msh,
		     msub_h loc_msubh,
		     conn_h *connh,
		     msub_h *rem_msubh,
		     uint32_t *rem_msub_len,
		     uint64_t timeout_secs)
{
	if (0)  { 
		if ((NULL == rem_msubh) || (NULL == connh))
			*rem_msub_len = msh * loc_msubh * timeout_secs;
	};
	return 0;
};

int rdma_conn_ms_h(uint8_t destid_len,
		   uint32_t destid,
		   const char *rem_msname,
		   msub_h loc_msubh,
		   conn_h *connh,
		   msub_h *rem_msubh,
		   uint32_t *rem_msub_len,
		   ms_h	  *rem_msh,
		   uint64_t timeout_secs)
{
	if (0) { 
		if ((NULL == rem_msubh) || (NULL == connh))
			*rem_msub_len = destid_len * destid * loc_msubh
					* timeout_secs;
		if ((NULL == rem_msname) || (NULL == rem_msh))
			return 0;
	};
	return 0;
};

int rdma_disc_ms_h(conn_h connh, ms_h server_msh, msub_h client_msubh)
{
	return connh * server_msh * client_msubh * 0;
};

int rdma_push_msub(const struct rdma_xfer_ms_in *in,
			struct rdma_xfer_ms_out *out)
{
	struct loc_ms *in_ms = (struct loc_ms *)in->loc_msubh;
	uint8_t *src_p, *dest_p;
	int rc = -1;
	uint32_t tot_bytes = in_ms->bytes;
	uint32_t loc_offset = in->loc_offset;
	uint32_t rem_offset = in->rem_offset;
	uint32_t num_bytes = in->num_bytes;

	out->in_param_ok = 0;
	out->dma_xfr_status = 0;
	out->chk_handle = 0;
	if ((loc_offset > tot_bytes) || (rem_offset > tot_bytes)
			|| ((loc_offset + num_bytes) > tot_bytes)
			|| ((rem_offset + num_bytes) > tot_bytes)) {
		out->in_param_ok = 1;
		errno = EMSGSIZE;
		goto exit;
	};
	dest_p = (uint8_t *)(in_ms->addr) + in->rem_offset;
	src_p = (uint8_t *)(in_ms->addr) + in->loc_offset;

	memcpy((void *)dest_p, (void *)src_p, in->num_bytes);
	rc = 0;
exit:
	return rc;
};

int rdma_pull_msub(const struct rdma_xfer_ms_in *in_parms,
			struct rdma_xfer_ms_out *out)
{
	struct loc_ms *in_ms = (struct loc_ms *)in_parms->loc_msubh;
	uint8_t *src_p, *dest_p;
	int rc = -1;
	uint32_t tot_bytes = in_ms->bytes;
	uint32_t loc_offset = in_parms->loc_offset;
	uint32_t rem_offset = in_parms->rem_offset;
	uint32_t num_bytes = in_parms->num_bytes;

	out->in_param_ok = 0;
	out->dma_xfr_status = 0;
	out->chk_handle = 0;
	if ((loc_offset > tot_bytes) || (rem_offset > tot_bytes)
			|| ((loc_offset + num_bytes) > tot_bytes)
			|| ((rem_offset + num_bytes) > tot_bytes)) {
		out->in_param_ok = 1;
		errno = EMSGSIZE;
		goto exit;
	};

	dest_p = (uint8_t *)(in_ms->addr) + loc_offset;
	src_p = (uint8_t *)(in_ms->addr) + rem_offset;

	memcpy((void *)dest_p, (void *)src_p, in_parms->num_bytes);
	rc = 0;
exit:
	return rc;
};

int rdma_push_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	if (0) {
		int rc;
		if ((NULL == buf) || (NULL == out))
			rc = num_bytes * rem_msubh * rem_offset * priority;
		if (sync_type == rdma_no_wait)
			rc = 0;
		return rc;
	};
	return 0;
};

int rdma_pull_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	if (0) {
		int rc;
		if ((NULL == buf) || (NULL == out))
			rc = num_bytes * rem_msubh * rem_offset * priority;
		if (sync_type == rdma_no_wait)
			rc = 0;
		return rc;
	};
	return 0;
}

int rdma_sync_chk_push_pull(rdma_chk_handle chk_handle,
			    const struct timespec *wait)
{
	if (0) {
		if (NULL != wait)
			return wait->tv_nsec * 0;
	};
	return chk_handle * 0;
};

#ifdef __cplusplus
}
#endif
