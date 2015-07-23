#ifndef _RAPIDIO_MPORT_DMA_H_
#define _RAPIDIO_MPORT_DMA_H_
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

#ifdef __cplusplus
extern "C" {
#endif

enum riomp_dma_directio_type {
	//RIO_DIRECTIO_TYPE_DEFAULT,	/* Default method */
	RIO_DIRECTIO_TYPE_NWRITE,		/* All packets using NWRITE */
	RIO_DIRECTIO_TYPE_SWRITE,		/* All packets using SWRITE */
	RIO_DIRECTIO_TYPE_NWRITE_R,		/* Last packet NWRITE_R, others NWRITE */
	RIO_DIRECTIO_TYPE_SWRITE_R,		/* Last packet NWRITE_R, others SWRITE */
	RIO_DIRECTIO_TYPE_NWRITE_R_ALL,	/* All packets using NWRITE_R */
};

enum riomp_dma_directio_transfer_sync {
	RIO_DIRECTIO_TRANSFER_SYNC,		/* synchronous transfer */
	RIO_DIRECTIO_TRANSFER_ASYNC,	/* asynchronous transfer */
	RIO_DIRECTIO_TRANSFER_FAF,		/* fire-and-forget transfer only for write transactions */
};

int riomp_dma_write(int fd, uint16_t destid, uint64_t tgt_addr, void *buf, uint32_t size, enum riomp_dma_directio_type wr_mode, enum riomp_dma_directio_transfer_sync sync);
int riomp_dma_write_d(int fd, uint16_t destid, uint64_t tgt_addr, uint64_t handle, uint32_t offset, uint32_t size, enum riomp_dma_directio_type wr_mode, enum riomp_dma_directio_transfer_sync sync);
int riomp_dma_read(int fd, uint16_t destid, uint64_t tgt_addr, void *buf, uint32_t size, enum riomp_dma_directio_transfer_sync sync);
int riomp_dma_read_d(int fd, uint16_t destid, uint64_t tgt_addr, uint64_t handle, uint32_t offset, uint32_t size, enum riomp_dma_directio_transfer_sync sync);
int riomp_dma_wait_async(int fd, uint32_t cookie, uint32_t tmo);
int riomp_dma_ibwin_map(int fd, uint64_t *rio_base, uint32_t size, uint64_t *handle);
int riomp_dma_ibwin_free(int fd, uint64_t *handle);
int riomp_dma_obwin_map(int fd, uint16_t destid, uint64_t rio_base, uint32_t size, uint64_t *handle);
int riomp_dma_obwin_free(int fd, uint64_t *handle);
int riomp_dma_dbuf_alloc(int fd, uint32_t size, uint64_t *handle);
int riomp_dma_dbuf_free(int fd, uint64_t *handle);

#ifdef __cplusplus
}
#endif
#endif /* _RIODP_MPORT_LIB_H_ */
