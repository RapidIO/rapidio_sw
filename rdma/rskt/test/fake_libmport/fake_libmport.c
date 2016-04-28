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
#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_dma.h>
#include <rapidio_mport_sock.h>
#include <stdint.h>
#include <sched.h>
#include "fake_libmport.h"
#include "DAR_RegDefs.h"
#include "libunit_test.h"
#include "rskt_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

int kill_acc_conn;

/**
 * @brief Perform DMA data write to target transfer using user space source buffer
 *
 * @param[in] mport_handle port handle
 * @param[in] destid destination device ID
 * @param[in] tgt_addr target memory address
 * @param[in] buf pointer to userspace source buffer
 * @param[in] size number of bytes to transfer
 * @param[in] wr_mode DirectIO write mode
 * @param[in] sync transfer synchronization flag
 * @return status of the function call
 * @retval 0 on success
 * @retval -errno on error
 */
int riomp_dma_write(riomp_mport_t mport_handle, uint16_t destid,
		uint64_t tgt_addr, void *buf, uint32_t size,
		enum riomp_dma_directio_type wr_mode, 
		enum riomp_dma_directio_transfer_sync sync)
{
	if (0) {
		if ((NULL == buf) || (NULL == mport_handle) ||
				(RIO_DIRECTIO_TYPE_NWRITE == wr_mode) ||
				(RIO_DIRECTIO_TRANSFER_SYNC == sync))
			return 0;
	};

	return 0 * destid * tgt_addr * size;
};
		
int riomp_dma_write_d(riomp_mport_t mport_handle, uint16_t destid,
	uint64_t tgt_addr, uint64_t handle, uint32_t offset, uint32_t size,
	enum riomp_dma_directio_type wr_mode,
	enum riomp_dma_directio_transfer_sync sync)
{
	if (0) {
		if ((NULL == mport_handle) ||
				(RIO_DIRECTIO_TYPE_NWRITE == wr_mode) ||
				(RIO_DIRECTIO_TRANSFER_SYNC == sync))
			return 0;
	};

	return 0 * destid * tgt_addr * handle * offset * size;
};

int riomp_dma_read(riomp_mport_t mport_handle, uint16_t destid, 
	uint64_t tgt_addr, void *buf, uint32_t size,
	enum riomp_dma_directio_transfer_sync sync)
{
	if (0) {
		if ((NULL == buf) || (NULL == mport_handle) ||
				(RIO_DIRECTIO_TRANSFER_SYNC == sync))
			return 0;
	};

	return 0 * destid * tgt_addr * size;
};

int riomp_dma_read_d(riomp_mport_t mport_handle, uint16_t destid,
		uint64_t tgt_addr, uint64_t handle, uint32_t offset,
		uint32_t size, enum riomp_dma_directio_transfer_sync sync)
{
	if (0) {
		if ((NULL == mport_handle) ||
				(RIO_DIRECTIO_TRANSFER_SYNC == sync))
			return 0;
	};

	return 0 * destid * tgt_addr * handle * offset * size;
};

int riomp_dma_wait_async(riomp_mport_t mport_handle,
		uint32_t cookie, uint32_t tmo)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * cookie * tmo;
};

int riomp_dma_ibwin_map(riomp_mport_t mport_handle, uint64_t *rio_base,
			uint32_t size, uint64_t *handle)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == rio_base)
				|| (NULL == handle))
			return 0;
	};

	return 0 * size;
};

int riomp_dma_ibwin_free(riomp_mport_t mport_handle, uint64_t *handle)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == handle))
			return 0;
	};

	return 0;
};

int riomp_dma_obwin_map(riomp_mport_t mport_handle, uint16_t destid,
			uint64_t rio_base, uint32_t size, uint64_t *handle)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == handle))
			return 0;
	};

	return 0 * destid * rio_base * size;
};

int riomp_dma_obwin_free(riomp_mport_t mport_handle, uint64_t *handle)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == handle))
			return 0;
	};

	return 0;
};

int riomp_dma_dbuf_alloc(riomp_mport_t mport_handle, uint32_t size,
					uint64_t *handle)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == handle))
			return 0;
	};

	return 0 * size;
};

int riomp_dma_dbuf_free(riomp_mport_t mport_handle, uint64_t *handle)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == handle))
			return 0;
	};

	return 0;
};

int riomp_dma_map_memory(riomp_mport_t mport_handle, size_t size, off_t paddr,
				void **vaddr)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == vaddr))
			return 0;
	};

	return 0 * size * paddr;
};


int riomp_dma_unmap_memory(riomp_mport_t mport_handle, size_t size, void *vaddr)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == vaddr))
			return 0;
	};

	return 0 * size;
};

int riomp_mgmt_get_mport_list(uint32_t **dev_ids, uint8_t *number_of_mports)
{
	if (0) {
		if ((NULL == dev_ids) || (NULL == number_of_mports))
			return 0;
	};

	return 0;
};
	
int riomp_mgmt_free_mport_list(uint32_t **dev_ids)
{
	if (0) {
		if (NULL == dev_ids)
			return 0;
	};

	return 0;
};

int riomp_mgmt_get_ep_list(uint8_t mport_id, uint32_t **destids,
							uint32_t *number_of_eps)
{
	if (0) {
		if ((NULL == destids) || (NULL == number_of_eps))
			return 0;
	};

	return 0 * mport_id;
};

int riomp_mgmt_free_ep_list(uint32_t **destids)
{
	if (0) {
		if (NULL == destids)
			return 0;
	};

	return 0;
};

int riomp_mgmt_mport_create_handle(uint32_t mport_id, int flags,
						riomp_mport_t *mport_handle)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * mport_id * flags;
};

int riomp_mgmt_mport_destroy_handle(riomp_mport_t *mport_handle)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0;
};

int riomp_mgmt_get_handle_id(riomp_mport_t mport_handle, int *id)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == id))
			return 0;
	};

	return 0;
};

int riomp_mgmt_query(riomp_mport_t mport_handle,
				struct riomp_mgmt_mport_properties *qresp)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == qresp))
			return 0;
	};

	qresp->hdid = FAKE_LIBMPORT_CT;
	qresp->id = 0;
	qresp->sys_size = 1;
	qresp->port_ok = 1;
	qresp->link_speed = RIO_LINK_625;
	qresp->link_width = RIO_LINK_16X;
	qresp->transfer_mode = 1;
	qresp->cap_sys_size = 3;
	qresp->cap_transfer_mode = 3;
	qresp->flags = RIO_MPORT_DMA | RIO_MPORT_DMA_SG | RIO_MPORT_IBSG;

	return 0;
};

void riomp_mgmt_display_info(struct riomp_mgmt_mport_properties *prop)
{
	if (0) {
		if (NULL == prop)
			prop->hdid = 0;
	};
};

int riomp_mgmt_destid_set(riomp_mport_t mport_handle, uint16_t destid)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * destid;
};

int riomp_mgmt_lcfg_read(riomp_mport_t mport_handle, uint32_t offset,
						uint32_t size, uint32_t *data)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == data))
			return 0;
	};

	switch (offset) {
	case RIO_COMPONENT_TAG_CSR: *data = FAKE_LIBMPORT_CT;
	default: *data = 0;
	};

	return 0 * offset * size;
};

int riomp_mgmt_lcfg_write(riomp_mport_t mport_handle, uint32_t offset,
				uint32_t size, uint32_t data)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * offset * size * data;
};

int riomp_mgmt_rcfg_read(riomp_mport_t mport_handle, uint32_t destid,
			uint32_t hc, uint32_t offset, uint32_t size,
			uint32_t *data)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == data))
			return 0;
	};

	return 0 * destid * hc * offset * size;
};

int riomp_mgmt_rcfg_write(riomp_mport_t mport_handle, uint32_t destid,
			uint32_t hc, uint32_t offset, uint32_t size,
			uint32_t data)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * destid * hc * offset * size * data;
};

int riomp_mgmt_dbrange_enable(riomp_mport_t mport_handle, uint32_t rioid,
			 uint16_t start, uint16_t end)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * rioid * start * end;
};

int riomp_mgmt_dbrange_disable(riomp_mport_t mport_handle, uint32_t rioid,
				uint16_t start, uint16_t end)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * rioid * start * end;
};

int riomp_mgmt_pwrange_enable(riomp_mport_t mport_handle, uint32_t mask,
				uint32_t low, uint32_t high)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * mask * low * high;
};

int riomp_mgmt_pwrange_disable(riomp_mport_t mport_handle, uint32_t mask,
				uint32_t low, uint32_t high)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * mask * low * high;
};

int riomp_mgmt_set_event_mask(riomp_mport_t mport_handle, unsigned int mask)
{
	if (0) {
		if (NULL == mport_handle)
			return 0;
	};

	return 0 * mask;
};

int riomp_mgmt_get_event_mask(riomp_mport_t mport_handle, unsigned int *mask)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == mask))
			return 0;
	};

	return 0;
};

int riomp_mgmt_get_event(riomp_mport_t mport_handle,
			struct riomp_mgmt_event *evt)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == evt))
			return 0;
	};

	return 0;
};

int riomp_mgmt_send_event(riomp_mport_t mport_handle,
			struct riomp_mgmt_event *evt)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == evt))
			return 0;
	};

	return 0;
};

int riomp_mgmt_device_add(riomp_mport_t mport_handle, uint16_t destid,
			uint8_t hc, uint32_t ctag, const char *name)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == name))
			return 0;
	};

	return 0 * destid * hc * ctag;
};

int riomp_mgmt_device_del(riomp_mport_t mport_handle, uint16_t destid,
			uint8_t hc, uint32_t ctag, const char *name)
{
	if (0) {
		if ((NULL == mport_handle) || (NULL == name))
			return 0;
	};

	return 0 * destid * hc * ctag;
};

// struct riomp_mailbox_t * struct worker;

sem_t sock_wkr_idx_mtx;
int sock_wkr_idx = 0;
int sock_mbox_init;

int riomp_sock_mbox_init(void)
{
	sem_init(&sock_wkr_idx_mtx, 0, 1);
	return 0;
}

int riomp_sock_mbox_exit(void)
{
	return 0;
}

int riomp_sock_mbox_create_handle(uint8_t mport_id, uint8_t mbox_id,
				riomp_mailbox_t *mailbox)
{
	int rc = 0;

	if (0) {
		if ((NULL == mailbox) || !mbox_id || mport_id)
			return 0;
	};
	if (!sock_mbox_init) {
		rc = riomp_sock_mbox_init();
		sock_mbox_init = 1;
	};
	return rc;
}

int riomp_sock_mbox_destroy_handle(riomp_mailbox_t *mailbox)
{
	if (0) {
		if (NULL == mailbox)
			return 0;
	};
	sock_mbox_init = 0;
	return 0;
}

int riomp_sock_socket(riomp_mailbox_t mailbox, riomp_sock_t *socket_handle)
{
	if (0) {
		if ((NULL == mailbox) || (NULL == socket_handle))
			return 0;
	};

	*socket_handle = (riomp_sock_t)malloc(
					sizeof(struct rapidio_mport_socket));
	(*socket_handle)->wkr_idx = -1;
	(*socket_handle)->acceptor = TEST_SKT_NO_CONN;

	return 0;
}

int riomp_sock_send(riomp_sock_t socket_handle, void *buf, uint32_t size)
{
	struct rskt_test_info *test;
	struct rsktd_req_msg *req;
        struct rsktd_resp_msg *resp;

	if ((NULL == socket_handle) || (NULL == buf) || !size)
		goto fail;

	if ((socket_handle->wkr_idx < 0) ||
			(socket_handle->wkr_idx > MAX_WORKER_IDX))
		goto fail;

	test = (struct rskt_test_info *)wkr[socket_handle->wkr_idx].priv_info;

	if (NULL == test)
		goto fail;

	switch (socket_handle->acceptor) {
	case TEST_SKT_CONNECT: /* Connector always sends requests */
		req = (struct rsktd_req_msg *)malloc(sizeof(struct rsktd_req_msg));
		memcpy(req, buf, sizeof(struct rsktd_req_msg));
        	sem_wait(&test->speer_req_mtx);
        	l_push_tail(&test->speer_req, (void *)req);
        	sem_post(&test->speer_req_mtx);
        	sem_post(&test->speer_req_cnt);
		test->con_sent++;
		break;

	case TEST_SKT_ACCEPT: /* Acceptor always sends responses */
		resp = (struct rsktd_resp_msg *)malloc(sizeof(struct rsktd_resp_msg));
		memcpy(resp, buf, sizeof(struct rsktd_resp_msg));
        	sem_wait(&test->speer_resp_mtx);
        	l_push_tail(&test->speer_resp, (void *)resp);
        	sem_post(&test->speer_resp_mtx);
        	sem_post(&test->speer_resp_cnt);
		test->acc_sent++;
		break;
	default: goto fail;
		break;
	};

	return 0;

fail:
	return 1;
	
}

int riomp_sock_receive(riomp_sock_t socket_handle, void **buf, uint32_t size,
			uint32_t timeout)
{
	struct rskt_test_info *test;
	struct rsktd_req_msg *req;
        struct rsktd_resp_msg *resp;
	int rc = 1;

	if (0) {
		rc = timeout;
	};

	if ((NULL == socket_handle) || (NULL == buf) || !size)
		goto fail;

	if ((socket_handle->wkr_idx < 0) ||
			(socket_handle->wkr_idx > MAX_WORKER_IDX))
		goto fail;
	test = (struct rskt_test_info *)wkr[socket_handle->wkr_idx].priv_info;

	if (NULL == test)
		goto fail;

	switch (socket_handle->acceptor) {
	case TEST_SKT_CONNECT: /* Connector always receives responses */
        	sem_wait(&test->speer_resp_cnt);
        	sem_wait(&test->speer_resp_mtx);
        	resp = (struct rsktd_resp_msg *)l_pop_head(&test->speer_resp);
        	sem_post(&test->speer_resp_mtx);
		if (NULL == resp)
			goto fail;
		memcpy(*buf, resp, sizeof(struct rsktd_resp_msg));
		//free(resp);
		rc = test->speer_resp_err;
		test->con_received++;
		break;

	case TEST_SKT_ACCEPT: /* Acceptor always receives requests */
        	sem_wait(&test->speer_req_cnt);
        	sem_wait(&test->speer_req_mtx);
        	req = (struct rsktd_req_msg *)l_pop_head(&test->speer_req);
        	sem_post(&test->speer_req_mtx);
		if (NULL == req)
			goto fail;
		memcpy(*buf, req, sizeof(struct rsktd_req_msg));
		//free(req);
		rc = 0;
		test->acc_received++;
		break;
	default: goto fail;
		break;
	};

	return rc;
fail:
	return 1;
}

int riomp_sock_release_receive_buffer(riomp_sock_t socket_handle, void *buf)
{
	if ((NULL == socket_handle) || (NULL == buf))
		goto fail;
	//free(buf);
fail:
	return 0;
}

int riomp_sock_close(riomp_sock_t *socket_handle)
{
	if (NULL == socket_handle)
		goto exit;
	if (NULL == *socket_handle)
		goto exit;

	// free(*socket_handle);
	*socket_handle = NULL;

exit:
	return 0;
}

int riomp_sock_bind(riomp_sock_t socket_handle, uint16_t local_channel)
{
	if (0) {
		if (NULL == socket_handle)
			return 0;
	};
	return 0 * local_channel;
}

int riomp_sock_listen(riomp_sock_t socket_handle)
{
	if (0) {
		if (NULL == socket_handle)
			return 0;
	};
	return 0;
}

int riomp_sock_accept(riomp_sock_t socket_handle, riomp_sock_t *conn,
			uint32_t timeout)
{
	struct rskt_test_info volatile * volatile test;

	if (0)
		sock_wkr_idx = timeout;

	if ((NULL == socket_handle) || (NULL == conn))
		goto fail;

	if (NULL == *conn)
		goto fail;

	/* if the socket is already initialized, this is a call from the
 	* test worker.  No need to hide the connection mechanism.
 	* The info should already be there.
 	*/
	if ((*conn)->wkr_idx == -1) {
		sem_wait(&sock_wkr_idx_mtx);
		(*conn)->wkr_idx = sock_wkr_idx;
		sock_wkr_idx = (sock_wkr_idx + 1) % (MAX_WORKERS/2);
		(*conn)->acceptor = TEST_SKT_ACCEPT;
		sem_post(&sock_wkr_idx_mtx);


		while (((NULL == (volatile void * volatile)
				wkr[(*conn)->wkr_idx].priv_info) 
			|| (wkr[(*conn)->wkr_idx].stop_req != worker_running))
			&& !kill_acc_conn) {
			sleep(0);
		};
	};

	if (kill_acc_conn)
		goto fail;

	if (!wkr[(*conn)->wkr_idx].stop_req == worker_running)
		goto fail;

	test = (struct rskt_test_info *)
		wkr[(*conn)->wkr_idx].priv_info;

	if (test == NULL)
		goto fail;
	sem_post((sem_t *)&test->speer_con);
	sem_wait((sem_t *)&test->speer_acc);
	
	return 0;
fail:
	return 1;
}

int riomp_sock_connect(riomp_sock_t socket_handle, uint32_t remote_destid,
			uint16_t remote_channel)
{
	struct rskt_test_info *test;

	if (0) {
		socket_handle->acceptor = remote_mbox + remote_channel;
	};
	if (NULL == socket_handle)
		goto fail;

	socket_handle->wkr_idx = remote_destid;
	socket_handle->acceptor = TEST_SKT_CONNECT;

	if ((socket_handle->wkr_idx < 0) ||
			(socket_handle->wkr_idx > MAX_WORKER_IDX))
		goto fail;
	test = (struct rskt_test_info *)wkr[socket_handle->wkr_idx].priv_info;

	if (NULL == test)
		goto fail;

	sem_post(&test->speer_acc);
	sem_wait(&test->speer_con);
	
	return 0;
fail:
	return 1;
}

int riomp_sock_request_send_buffer(riomp_sock_t socket_handle, void **buf)
{
	struct rskt_test_info *test;

	if (NULL == socket_handle)
		goto fail;

	if ( (socket_handle->acceptor < TEST_SKT_CONNECT) ||
			(socket_handle->acceptor > TEST_SKT_ACCEPT) ||
			(socket_handle->wkr_idx < 0) ||
			(socket_handle->wkr_idx > MAX_WORKER_IDX))
		goto fail;
	test = (struct rskt_test_info *)wkr[socket_handle->wkr_idx].priv_info;

	if (NULL == test)
		goto fail;
	*buf = malloc(4*1024);
	return 0;
fail:
	return 1;
}

int riomp_sock_release_send_buffer(riomp_sock_t socket_handle, void *buf)
{
	if ( (socket_handle->acceptor < TEST_SKT_CONNECT) ||
			(socket_handle->acceptor > TEST_SKT_ACCEPT) ||
			(socket_handle->wkr_idx < 0) ||
			(socket_handle->wkr_idx > MAX_WORKER_IDX))
		goto fail;
//	if (NULL != buf)
		//free(buf);
fail:
	if (0)
		*(int *)buf = 0;
	return 0;
}

#ifdef __cplusplus
}
#endif
