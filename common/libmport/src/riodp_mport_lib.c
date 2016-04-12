/*
 * Copyright 2014, 2015 Integrated Device Technology, Inc.
 *
 * RapidIO mport device API library
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>

#include <linux/rio_cm_cdev.h>
#define CONFIG_RAPIDIO_DMA_ENGINE
#include <linux/rio_mport_cdev.h>

#include <rapidio_mport_mgmt.h>
#include <rapidio_mport_dma.h>
#include <rapidio_mport_sock.h>

#include <map>

#include "dmachanshm.h"
#include "riodp_mport_lib.h"

#define RIO_MPORT_DEV_PATH "/dev/rio_mport"
#define RIO_CMDEV_PATH "/dev/rio_cm"

#define UMD_SLEEP_NS	1 // Setting this to 0 will compile out nanosleep syscall

#define MAX_MPORT	32

struct rapidio_mport_mailbox {
	int fd;
	uint8_t mport_id;
};

struct rapidio_mport_handle {
        int     fd;                         /**< posix api compatible fd to be used with poll/select */
	uint8_t mport_id;
	uint8_t umd_chan;
        void*   dch;
        volatile uint32_t cookie_cutter; // XXX THIS STINKS but the API wants 32-bit!!
        std::map <uint64_t, DMAChannelSHM::DmaOptions_t> asyncm;
        void* stats;                    /**< Pointer to statistics gathering back door for driver... */
};

/** \brief Tsi721 DMA channel availability bitmask vs kernel driver.
 * \note The kernel uses channel 0. ALWAYS
 * \note The kernel SHOULD publish this [exclusion bitmask] using a sysfs interface.
 * \note WARNING Absent sysfs exclusion bitmask we can ony make educated guesses about what DMA channel the kernel is NOT using.
 */
static uint8_t DMA_CHAN_MASK = 0xFE;

static pthread_mutex_t inuse_bitmask_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile uint8_t inuse_bitmask[MAX_MPORT] = {0};

/** \brief Can we use this channel (i.e. not used by kernel) ?
 * \note In real life this should be obtained per-mport
 */
static inline bool riomp_mgmt_mport_umd_check_chan_mask(const int mport_id, const int chan)
{
	const int chanb = 1 << chan;
	return (chanb & DMA_CHAN_MASK) == chanb;
}

static inline int riomp_mgmt_mport_umd_is_channel_selected(const int mport_id, const int chan)
{
	if (inuse_bitmask[mport_id] == 0) return 0;
	if (! riomp_mgmt_mport_umd_check_chan_mask(mport_id, chan)) return -2; // in use by kernel
	
	const int chanb = 1 << chan;
	return (inuse_bitmask[mport_id] & chanb) == chanb;
}

/** \brief Reserve a UMD hw channel
 * \param chan if -1 pick next available channel, if > 0 pick this channel if available
 * \return -1 no channel is available, >0 the channel
 */
static inline int riomp_mgmt_mport_umd_select_channel(const int mport_id, const int chan)
{
	if (inuse_bitmask[mport_id] == DMA_CHAN_MASK) return -1; // ALL channels are taken

	int sel_chan = -1;
	pthread_mutex_lock(&inuse_bitmask_mutex);
	do {
		if (chan > 0) {
			if (! riomp_mgmt_mport_umd_check_chan_mask(mport_id, chan)) break; // in use by kernel
			inuse_bitmask[mport_id] |= (1 << chan);
			sel_chan = chan;
			break;
		}
		for (int i = 1; i < 8; i++) {
			if (! riomp_mgmt_mport_umd_check_chan_mask(mport_id, i)) continue; // in use by kernel
			if ((inuse_bitmask[mport_id] & (1 << i)) == 0) {
				inuse_bitmask[mport_id] |= (1 << i);
				sel_chan = i;
				break;
			}
		}
	} while(0);
	pthread_mutex_unlock(&inuse_bitmask_mutex);

	return sel_chan;
}

static inline void riomp_mgmt_mport_umd_deselect_channel(const int mport_id, const int chan)
{
	if (chan < 1 || chan > 7) return;
	if (inuse_bitmask[mport_id] == 0) return; // nothing to do
	pthread_mutex_lock(&inuse_bitmask_mutex);
	inuse_bitmask[mport_id] &= ~(1 << chan);
	pthread_mutex_unlock(&inuse_bitmask_mutex);
}


void* riomp_mgmt_mport_get_stats(struct rapidio_mport_handle* hnd)
{
	if (hnd == NULL) return NULL;
	return hnd->stats;
}
void riomp_mgmt_mport_set_stats(struct rapidio_mport_handle* hnd, void* stats)
{
	if (hnd == NULL) return;
	hnd->stats = stats;
}

struct rio_channel {
	uint16_t id;
	uint32_t remote_destid;
	uint32_t remote_mbox;
	uint16_t remote_channel;
	uint8_t mport_id;
};

struct rapidio_mport_socket {
	struct rapidio_mport_mailbox *mbox;
	struct rio_channel ch;
	uint8_t	*rx_buffer;
	uint8_t	*tx_buffer;
};

int riomp_mgmt_mport_create_handle(uint32_t mport_id, int flags, riomp_mport_t *mport_handle)
{
	char path[32];
	int fd, ret;
	struct rapidio_mport_handle *hnd = NULL;

	if (mport_id >= MAX_MPORT) return -(errno = EINVAL);

	// XXX O_SYNC    = 0x101000 will break this scheme
	// XXX O_CLOEXEC =  0x80000 will break this scheme

	const int oflags = flags & 0xFFFF;

	snprintf(path, sizeof(path), RIO_MPORT_DEV_PATH "%d", mport_id);

	fd = open(path, O_RDWR | O_CLOEXEC | oflags);
	if (fd == -1)
		return -errno;

	printf("UMDD %s: mport_id=%d flags=0x%x\n", __func__, mport_id, flags);

	hnd = (struct rapidio_mport_handle *)calloc(1, sizeof(struct rapidio_mport_handle));
	if(!(hnd)) {
		ret = -errno;
		close(fd);
		return ret;
	}

	hnd->fd       = fd;
	hnd->mport_id = mport_id;

	hnd->asyncm.clear();

	const char* chan_s = getenv("UMDD_CHAN");
	if (chan_s == NULL || (flags & 0xFFFF0000) == UMD_RESERVED) {
		*mport_handle = hnd;
		return 0;
	}

	int chan = 0;
	do {
		// Pick next available channel
		if ((flags & 0xFFFF0000) == UMD_SELECT_NEXT_CHANNEL) {
			chan = riomp_mgmt_mport_umd_select_channel(mport_id, 0);
			hnd->umd_chan = chan;
			break;
		}

		// Force sharing of channel $UMDD_CHAN
		int tmp = atoi(chan_s);
		if (tmp > 0) {
			if (riomp_mgmt_mport_umd_is_channel_selected(mport_id, tmp))
				chan = tmp;
			else
				chan = riomp_mgmt_mport_umd_select_channel(mport_id, tmp);
			hnd->umd_chan = -chan;
			break;
		}
	} while(0);

	if (chan == 0) {
		fprintf(stderr, "%s: Ran out of UMD DMA channels.\n", __func__);
		errno = ENOSPC; goto error;
	}

	try { hnd->dch = DMAChannelSHM_create(mport_id, chan); }
	catch(std::runtime_error ex) { // UMDd not running
		fprintf(stderr, "%s: Exception: %s\n", __func__, ex.what());
		errno = ENOTCONN; goto error;
	}

	if (hnd->dch == NULL) { // Cannot load SO
		errno = ENOPKG; goto error;
	}

        if (! DMAChannelSHM_pingMaster(hnd->dch)) { // Ping master process via signal(0)
		fprintf(stderr, "%s: UMDd master process is dead.\n", __func__);
		errno = ENOTCONN; goto error;
	}

	*mport_handle = hnd;
	return 0;

error:
	if (hnd->dch != NULL) {	DMAChannelSHM_destroy(hnd->dch); hnd->dch = NULL; }
	if (hnd->umd_chan > 0) riomp_mgmt_mport_umd_deselect_channel(hnd->mport_id, hnd->umd_chan);
	close(hnd->fd);
	free(hnd);
	return -errno;
}

int riomp_mgmt_mport_destroy_handle(riomp_mport_t *mport_handle)
{
	struct rapidio_mport_handle *hnd = *mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (hnd->dch != NULL) { DMAChannelSHM_destroy(hnd->dch); hnd->dch = NULL; }

	hnd->asyncm.clear();

	close(hnd->fd);

	// Shared channel is not deselected, no ref counting
        if (hnd->umd_chan > 0) riomp_mgmt_mport_umd_deselect_channel(hnd->mport_id, hnd->umd_chan);
	free(hnd);

	return 0;
}

int riomp_mgmt_get_handle_id(riomp_mport_t mport_handle, int *id)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	/*
	 * if the architecture does not support posix file handles return a different identifier.
	 */
	*id = hnd->fd;
	return 0;
}

int riomp_sock_mbox_init(void)
{
	return open(RIO_CMDEV_PATH, O_RDWR);
}


int riomp_mgmt_get_mport_list(uint32_t **dev_ids, uint8_t *number_of_mports)
{
	int fd;
	uint32_t entries = *number_of_mports;
	uint32_t *list;
	int ret = -1;


	/* Open RapidIO Channel Manager */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	list = (uint32_t *)calloc((entries + 1), sizeof(*list));
	if (list == NULL)
		goto outfd;

	/* Request MPORT list from the driver (first entry is list size) */
	list[0] = entries;
	if (ioctl(fd, RIO_CM_MPORT_GET_LIST, list)) {
		ret = errno;
		goto outfd;
	}

	/* Return list information */
	*dev_ids = &list[1]; /* pointer to the list */
	*number_of_mports = *list; /* return real number of mports */
	ret = 0;
outfd:
	close(fd);
	return ret;
}

int riomp_mgmt_free_mport_list(uint32_t **dev_ids)
{
	/* Get head of the list, because we did hide the list size and mport ID
	 * parameters
	 */
	uint32_t *list;

	if(dev_ids == NULL)
		return -1;
	list = (*dev_ids) - 1;
	free(list);
	return 0;
}

int riomp_mgmt_get_ep_list(uint8_t mport_id, uint32_t **destids, uint32_t *number_of_eps)
{
	int fd;
	int ret = 0;
	uint32_t entries;
	uint32_t *list;

	/* Open mport */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	/* Get list size */
	entries = mport_id;
	if (ioctl(fd, RIO_CM_EP_GET_LIST_SIZE, &entries)) {
#ifdef MPORT_DEBUG
		printf("%s ep_get_list_size ioctl failed: %s\n", __func__, strerror(errno));
#endif
		ret = errno;
		goto outfd;
	}
#ifdef MPORT_DEBUG
	printf("RIODP: %s() has %d entries\n", __func__,  entries);
#endif
	/* Get list */
	list = (uint32_t *)calloc((entries + 2), sizeof(*list));
	if (list == NULL) {
		ret = -1;
		goto outfd;
	}

	/* Get list (first entry is list size) */
	list[0] = entries;
	list[1] = mport_id;
	if (ioctl(fd, RIO_CM_EP_GET_LIST, list)) {
		ret = errno;
		goto outfd;
	}

	/* Pass to callee, first entry of list is entries in list */
	*destids = &list[2];
	*number_of_eps = entries;

outfd:
	close(fd);
	return ret;
}

int riomp_mgmt_free_ep_list(uint32_t **destids)
{
	/* Get head of the list, because we did hide the list size and mport ID
	 * parameters
	 */
	uint32_t *list;

	if(destids == NULL)
		return -1;
	list = (*destids) - 2;
	free(list);
	return 0;
}

static inline enum rio_exchange convert_directio_type(enum riomp_dma_directio_type type)
{
	switch(type) {
	case RIO_DIRECTIO_TYPE_NWRITE: return RIO_EXCHANGE_NWRITE;
	case RIO_DIRECTIO_TYPE_NWRITE_R: return RIO_EXCHANGE_NWRITE_R;
	case RIO_DIRECTIO_TYPE_NWRITE_R_ALL: return RIO_EXCHANGE_NWRITE_R_ALL;
	case RIO_DIRECTIO_TYPE_SWRITE: return RIO_EXCHANGE_SWRITE;
	case RIO_DIRECTIO_TYPE_SWRITE_R: return RIO_EXCHANGE_SWRITE_R;
	default: return RIO_EXCHANGE_DEFAULT;
	}
}

static inline enum rio_transfer_sync convert_directio_sync(enum riomp_dma_directio_transfer_sync sync)
{
	switch(sync) {
	default: /* sync as default is the smallest pitfall */
	case RIO_DIRECTIO_TRANSFER_SYNC: return RIO_TRANSFER_SYNC;
	case RIO_DIRECTIO_TRANSFER_ASYNC: return RIO_TRANSFER_ASYNC;
	case RIO_DIRECTIO_TRANSFER_FAF: return RIO_TRANSFER_FAF;
	}
}

/*
 * Perform DMA data write to target transfer using user space source buffer
 */
int riomp_dma_write(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr, void *buf,
		uint32_t size, enum riomp_dma_directio_type wr_mode,
		enum riomp_dma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = buf;
	xfer.length = size;
	xfer.handle = 0;
	xfer.offset = 0;
	xfer.method = convert_directio_type(wr_mode);

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_WRITE;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	return (ret < 0)? -errno: ret;
}

/*
 * Perform DMA data write to target transfer using kernel space source buffer
 */
int riomp_dma_write_d(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr,
		      uint64_t handle, uint32_t offset, uint32_t size,
		      enum riomp_dma_directio_type wr_mode,
		      enum riomp_dma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (hnd->dch != NULL) goto umdd;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = NULL;
	xfer.length = size;
	xfer.handle = handle; // According to drivers/rapidio/devices/rio_mport_cdev.c in rio_dma_transfer()
			      //    baddr = (dma_addr_t)xfer->handle;
	xfer.offset = offset;
	xfer.method = convert_directio_type(wr_mode);

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_WRITE;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	return (ret < 0)? -errno: ret;

   umdd:
        if (! DMAChannelSHM_checkMasterReady(hnd->dch)) return -(errno = ENOTCONN);

	if (DMAChannelSHM_queueFull(hnd->dch)) return -(errno = EBUSY);

	// printf("UMDD %s: destid=%u handle=0x%lx rio_addr=0x%lx+0x%x bcount=%d op=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, wr_mode, sync);

	DMAChannelSHM::DmaOptions_t opt;
	memset(&opt, 0, sizeof(opt));
	opt.destid      = destid;
	opt.bcount      = size;
	opt.raddr.lsb64 = tgt_addr + offset;
	RioMport::DmaMem_t dmamem; memset(&dmamem, 0, sizeof(dmamem));

	dmamem.type       = RioMport::DONOTCHECK;
	dmamem.win_handle = handle;
        dmamem.win_size   = size;

	bool q_was_full = false;
	uint32_t dma_abort_reason = 0;

	if (size > 16) {
		if (! DMAChannelSHM_queueDmaOpT1(hnd->dch,
				DMAChannelSHM::convert_riomp_dma_directio(wr_mode), &opt,
				&dmamem, &dma_abort_reason, (seq_ts *)hnd->stats)) {
			if (q_was_full) return -(errno = ENOSPC);
			return -(errno = EINVAL);
		}
	} else {
		if (! DMAChannelSHM_queueDmaOpT2(hnd->dch,
				DMAChannelSHM::convert_riomp_dma_directio(wr_mode),
				&opt, (uint8_t *)handle + offset, size, 
				&dma_abort_reason, (seq_ts *)hnd->stats)) {
			if (q_was_full) return -(errno = ENOSPC);
			return -(errno = EINVAL);
		}
	}

	if (sync == RIO_DIRECTIO_TRANSFER_FAF) return 0;

	if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
		uint32_t cookie = ++hnd->cookie_cutter;
		hnd->asyncm[cookie] = opt;
		return cookie;
	}

	// Only left RIO_DIRECTIO_TRANSFER_SYNC
	for (int cnt = 0;; cnt++) {
		const DMAChannelSHM::TicketState_t st = (DMAChannelSHM::TicketState_t)DMAChannelSHM_checkTicket(hnd->dch, &opt);
                if (st == DMAChannelSHM::UMDD_DEAD) return -(errno = ENOTCONN);
                if (st == DMAChannelSHM::COMPLETED) break;
                if (st == DMAChannelSHM::INPROGRESS) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
			struct timespec sl = {0, UMD_SLEEP_NS};
			if (cnt == 0) {
				uint64_t total_data_pending = 0;
				DMAChannelSHM_getShmPendingData(hnd->dch, &total_data_pending, NULL);
				if (total_data_pending > UMD_SLEEP_NS) sl.tv_nsec = total_data_pending;
			}
			nanosleep(&sl, NULL);
#endif
			continue;
		}
                if (st == DMAChannelSHM::BORKED) {
                        uint64_t t = 0;
                        const int deq = DMAChannelSHM_dequeueFaultedTicket(hnd->dch, &t);
			if (deq)
			     fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d) dequeued faulted ticket %lu\n", __func__, opt.ticket, st, t);
			else fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d)\n", __func__, opt.ticket, st);
			return -(errno = EIO);
		}
	}

	return 0;
}

/*
 * Perform DMA data read from target transfer using user space destination buffer
 */
int riomp_dma_read(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr, void *buf,
		   uint32_t size, enum riomp_dma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = buf;
	xfer.length = size;
	xfer.handle = 0;
	xfer.offset = 0;

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_READ;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	return (ret < 0)? -errno: ret;
}

/*
 * Perform DMA data read from target transfer using kernel space destination buffer
 */
int riomp_dma_read_d(riomp_mport_t mport_handle, uint16_t destid, uint64_t tgt_addr,
		     uint64_t handle, uint32_t offset, uint32_t size,
		     enum riomp_dma_directio_transfer_sync sync)
{
	struct rio_transaction tran;
	struct rio_transfer_io xfer;
	int ret;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (hnd->dch != NULL) goto umdd;

	xfer.rioid = destid;
	xfer.rio_addr = tgt_addr;
	xfer.loc_addr = NULL;
	xfer.length = size;
	xfer.handle = handle;
	xfer.offset = offset;

	tran.transfer_mode = RIO_TRANSFER_MODE_TRANSFER;
	tran.sync = convert_directio_sync(sync);
	tran.dir = RIO_TRANSFER_DIR_READ;
	tran.count = 1;
	tran.block = &xfer;

	ret = ioctl(hnd->fd, RIO_TRANSFER, &tran);
	return (ret < 0)? -errno: ret;

   umdd:
#ifdef MPORT_DEBUG
	printf("UMDD %s: destid=%u handle=0x%lx  rio_addr=0x%lx+0x%x\n bcount=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, sync);
#endif
        if (! DMAChannelSHM_checkMasterReady(hnd->dch)) return -(errno = ENOTCONN);

	if (DMAChannelSHM_queueFull(hnd->dch)) return -(errno = EBUSY);

	DMAChannelSHM::DmaOptions_t opt;
	memset(&opt, 0, sizeof(opt));
	opt.destid      = destid;
	opt.bcount      = size;
	opt.raddr.lsb64 = tgt_addr + offset;

	RioMport::DmaMem_t dmamem; memset(&dmamem, 0, sizeof(dmamem));

	dmamem.type       = RioMport::DONOTCHECK;
	dmamem.win_handle = handle;
        dmamem.win_size   = size;

	bool q_was_full = false;
	uint32_t dma_abort_reason = 0;

	if (! DMAChannelSHM_queueDmaOpT1(hnd->dch, NREAD, &opt, &dmamem,
			&dma_abort_reason, (seq_ts *)hnd->stats)) {
		if (q_was_full) return -(errno = ENOSPC);
		return -(errno = EINVAL);
	}

	if (sync == RIO_DIRECTIO_TRANSFER_FAF) return 0;

	if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
		uint32_t cookie = ++hnd->cookie_cutter;
		hnd->asyncm[cookie] = opt;
		return cookie;
	}

	// Only left RIO_DIRECTIO_TRANSFER_SYNC
	for (int cnt = 0;; cnt++) {
		const DMAChannelSHM::TicketState_t st = (DMAChannelSHM::TicketState_t)DMAChannelSHM_checkTicket(hnd->dch, &opt);
                if (st == DMAChannelSHM::UMDD_DEAD) return -(errno = ENOTCONN);
                if (st == DMAChannelSHM::COMPLETED) break;
                if (st == DMAChannelSHM::INPROGRESS) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
			struct timespec sl = {0, UMD_SLEEP_NS};
			if (cnt == 0) {
				uint64_t total_data_pending = 0;
				DMAChannelSHM_getShmPendingData(hnd->dch, &total_data_pending, NULL);
				if (total_data_pending > UMD_SLEEP_NS) sl.tv_nsec = total_data_pending;
			}
			nanosleep(&sl, NULL);
#endif
			continue;
		}
                if (st == DMAChannelSHM::BORKED) {
                        uint64_t t = 0;
                        const int deq = DMAChannelSHM_dequeueFaultedTicket(hnd->dch, &t);
			if (deq)
			     fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d) dequeued faulted ticket %lu\n", __func__, opt.ticket, st, t);
			else fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d)\n", __func__, opt.ticket, st);
			return -(errno = EIO);
		}
	}

	return 0;
}

/*
 * Wait for DMA transfer completion
 */
int riomp_dma_wait_async(riomp_mport_t mport_handle, uint32_t cookie, uint32_t tmo)
{
	struct rio_async_tx_wait wparam;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (hnd->dch != NULL) goto umdd;

	wparam.token = cookie;
	wparam.timeout = tmo;

	if (ioctl(hnd->fd, RIO_WAIT_FOR_ASYNC, &wparam))
		return -errno;

   umdd:
	// printf("UMDD %s: cookie=%u\n", __func__, cookie);

        if (! DMAChannelSHM_checkMasterReady(hnd->dch)) return -(errno = ENOTCONN);

	// Kernel's rio_mport_wait_for_async_dma wants miliseconds of timeout

	assert(cookie <= hnd->cookie_cutter);

        DMAChannelSHM::DmaOptions_t opt; memset(&opt, 0, sizeof(opt));
	std::map<uint64_t, DMAChannelSHM::DmaOptions_t>::iterator it = hnd->asyncm.find(cookie);

	if (it == hnd->asyncm.end()) // Tough luck
		throw std::runtime_error("riomp_dma_wait_async: Requested cookie not found in internal database");

	opt = it->second;

	hnd->asyncm.erase(it); // XXX This takes a lot of time :()

	uint64_t now = 0;
	while ((now = rdtsc()) < opt.not_before) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
		struct timespec sl = {0, UMD_SLEEP_NS};
		nanosleep(&sl, NULL);
#endif
	}

        for (int cnt = 0 ;; cnt++) {
                const DMAChannelSHM::TicketState_t st = (DMAChannelSHM::TicketState_t)DMAChannelSHM_checkTicket(hnd->dch, &opt);
                if (st == DMAChannelSHM::UMDD_DEAD) return -(errno = ENOTCONN);
                if (st == DMAChannelSHM::COMPLETED) return 0;
                if (st == DMAChannelSHM::BORKED) {
                        uint64_t t = 0;
                        DMAChannelSHM_dequeueFaultedTicket(hnd->dch, &t);
                        fprintf(stderr, "UMDD %s: Ticket %lu status BORKED (%d) dequeued faulted ticket %lu\n", __func__, opt.ticket, st, t);
                        return -(errno = EIO);
                }
	
		// Last-ditch wait ??
                if (st == DMAChannelSHM::INPROGRESS) {
#if defined(UMD_SLEEP_NS) && UMD_SLEEP_NS > 0
			struct timespec sl = {0, UMD_SLEEP_NS};
			sl.tv_nsec = opt.not_before_dns;
			nanosleep(&sl, NULL);
#endif
			continue;
		}
	}

	// Should not reach here

	return -(errno = EINVAL);
}


/*
 * Allocate and map into RapidIO space a local kernel space data buffer
 * (for inbound RapidIO data read/write requests)
 */
int riomp_dma_ibwin_map(riomp_mport_t mport_handle, uint64_t *rio_base, uint32_t size, uint64_t *handle)
{
	struct rio_mmap ib;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ib.rio_addr = *rio_base;
	ib.length = size;

	if (ioctl(hnd->fd, RIO_MAP_INBOUND, &ib))
		return -errno;
	*handle = ib.handle;
	*rio_base = ib.rio_addr;
	return 0;
}

/*
 * Free and unmap from RapidIO space a local kernel space data buffer
 */
int riomp_dma_ibwin_free(riomp_mport_t mport_handle, uint64_t *handle)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_UNMAP_INBOUND, handle))
		return -errno;
	return 0;
}

int riomp_dma_obwin_map(riomp_mport_t mport_handle, uint16_t destid, uint64_t rio_base, uint32_t size,
		    uint64_t *handle)
{
	struct rio_mmap ob;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	ob.rioid = destid;
	ob.rio_addr = rio_base;
	ob.length = size;

	if (ioctl(hnd->fd, RIO_MAP_OUTBOUND, &ob))
		return -errno;
	*handle = ob.handle;
	return 0;
}

int riomp_dma_obwin_free(riomp_mport_t mport_handle, uint64_t *handle)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_UNMAP_OUTBOUND, handle))
		return -errno;
	return 0;
}

/*
 * Allocate a local kernel space data buffer for DMA data transfers
 */
int riomp_dma_dbuf_alloc(riomp_mport_t mport_handle, uint32_t size, uint64_t *handle)
{
	struct rio_dma_mem db;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	db.length = size;

	if (ioctl(hnd->fd, RIO_ALLOC_DMA, &db))
		return -errno;
	*handle = db.dma_handle;
	return 0;
}

/*
 * Free previously allocated local kernel space data buffer
 */
int riomp_dma_dbuf_free(riomp_mport_t mport_handle, uint64_t *handle)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_FREE_DMA, handle))
		return -errno;

	return 0;
}

/*
 * map phys address range to process virtual memory
 */
int riomp_dma_map_memory(riomp_mport_t mport_handle, size_t size, off_t paddr, void **vaddr)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL || !size || !vaddr)
		return -EINVAL;

	*vaddr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, hnd->fd, paddr);

	if (*vaddr == MAP_FAILED) {
		return -errno;
	}

	return 0;
}

/*
 * unmap phys address range to process virtual memory
 */
int riomp_dma_unmap_memory(riomp_mport_t mport_handle, size_t size, void *vaddr)
{
	return munmap(vaddr, size);
}

/*
 * Query mport status/capabilities
 */
int riomp_mgmt_query(riomp_mport_t mport_handle, struct riomp_mgmt_mport_properties *qresp)
{
	struct rapidio_mport_handle *hnd = mport_handle;
	struct rio_mport_properties prop;
	if (!qresp || !hnd)
		return -EINVAL;

	memset(&prop, 0, sizeof(prop));
	if (ioctl(hnd->fd, RIO_MPORT_GET_PROPERTIES, &prop))
		return -errno;

	qresp->hdid               = prop.hdid;
	qresp->id                 = prop.id;
	qresp->index              = prop.index;
	qresp->flags              = prop.flags;
	qresp->sys_size           = prop.sys_size;
	qresp->port_ok            = prop.port_ok;
	qresp->link_speed         = prop.link_speed;
	qresp->link_width         = prop.link_width;
	qresp->dma_max_sge        = prop.dma_max_sge;
	qresp->dma_max_size       = prop.dma_max_size;
	qresp->dma_align          = prop.dma_align;
	qresp->transfer_mode      = prop.transfer_mode;
	qresp->cap_sys_size       = prop.cap_sys_size;
	qresp->cap_addr_size      = prop.cap_addr_size;
	qresp->cap_transfer_mode  = prop.cap_transfer_mode;
	qresp->cap_mport          = prop.cap_mport;

	return 0;
}

/*
 * Read from local (mport) device register
 */
int riomp_mgmt_lcfg_read(riomp_mport_t mport_handle, uint32_t offset, uint32_t size, uint32_t *data)
{
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = data;

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_READ_LOCAL, &mt))
		return -errno;
	return 0;
}

/*
 * Write to local (mport) device register
 */
int riomp_mgmt_lcfg_write(riomp_mport_t mport_handle, uint32_t offset, uint32_t size, uint32_t data)
{
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.offset = offset;
	mt.length = size;
//		uint32_t __user value; /* when length == 0 */
	mt.u.buffer = &data;   /* when length != 0 */

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_WRITE_LOCAL, &mt))
		return -errno;
	return 0;
}

/*
 * Maintenance read from target RapidIO device register
 */
int riomp_mgmt_rcfg_read(riomp_mport_t mport_handle, uint32_t destid, uint32_t hc, uint32_t offset,
		     uint32_t size, uint32_t *data)
{
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.rioid = destid;
	mt.hopcount = hc;
	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = data;   /* when length != 0 */

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_READ_REMOTE, &mt))
		return -errno;
	return 0;
}


/*
 * Maintenance write to target RapidIO device register
 */
int riomp_mgmt_rcfg_write(riomp_mport_t mport_handle, uint32_t destid, uint32_t hc, uint32_t offset,
		      uint32_t size, uint32_t data)
{
	struct rio_mport_maint_io mt;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	mt.rioid = destid;
	mt.hopcount = hc;
	mt.offset = offset;
	mt.length = size;
	mt.u.buffer = &data;   /* when length != 0 */

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_WRITE_REMOTE, &mt))
		return -errno;
	return 0;
}

/*
 * Enable (register) receiving range of RapidIO doorbell events
 */
int riomp_mgmt_dbrange_enable(riomp_mport_t mport_handle, uint32_t rioid, uint16_t start, uint16_t end)
{
	struct rio_doorbell_filter dbf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dbf.rioid = rioid;
	dbf.low = start;
	dbf.high = end;

	if (ioctl(hnd->fd, RIO_ENABLE_DOORBELL_RANGE, &dbf))
		return -errno;
	return 0;
}

/*
 * Disable (unregister) range of inbound RapidIO doorbell events
 */
int riomp_mgmt_dbrange_disable(riomp_mport_t mport_handle, uint32_t rioid, uint16_t start, uint16_t end)
{
	struct rio_doorbell_filter dbf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dbf.rioid = rioid;
	dbf.low = start;
	dbf.high = end;

	if (ioctl(hnd->fd, RIO_DISABLE_DOORBELL_RANGE, &dbf))
		return -errno;
	return 0;
}

/*
 * Enable (register) filter for RapidIO port-write events
 */
int riomp_mgmt_pwrange_enable(riomp_mport_t mport_handle, uint32_t mask, uint32_t low, uint32_t high)
{
	struct rio_pw_filter pwf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	pwf.mask = mask;
	pwf.low = low;
	pwf.high = high;

	if (ioctl(hnd->fd, RIO_ENABLE_PORTWRITE_RANGE, &pwf))
		return -errno;
	return 0;
}

/*
 * Disable (unregister) filter for RapidIO port-write events
 */
int riomp_mgmt_pwrange_disable(riomp_mport_t mport_handle, uint32_t mask, uint32_t low, uint32_t high)
{
	struct rio_pw_filter pwf;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	pwf.mask = mask;
	pwf.low = low;
	pwf.high = high;

	if (ioctl(hnd->fd, RIO_DISABLE_PORTWRITE_RANGE, &pwf))
		return -errno;
	return 0;
}

/*
 * Set event notification mask
 */
int riomp_mgmt_set_event_mask(riomp_mport_t mport_handle, unsigned int mask)
{
	unsigned int evt_mask = 0;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (mask & RIO_EVENT_DOORBELL) evt_mask |= RIO_DOORBELL;
	if (mask & RIO_EVENT_PORTWRITE) evt_mask |= RIO_PORTWRITE;
	if (ioctl(hnd->fd, RIO_SET_EVENT_MASK, evt_mask))
		return -errno;
	return 0;
}

/*
 * Get current value of event mask
 */
int riomp_mgmt_get_event_mask(riomp_mport_t mport_handle, unsigned int *mask)
{
	int evt_mask = 0;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (!mask) return -EINVAL;
	if (ioctl(hnd->fd, RIO_GET_EVENT_MASK, &evt_mask))
		return -errno;
	*mask = 0;
	if (evt_mask & RIO_DOORBELL) *mask |= RIO_EVENT_DOORBELL;
	if (evt_mask & RIO_PORTWRITE) *mask |= RIO_EVENT_PORTWRITE;
	return 0;
}

/*
 * Get current event data
 */
int riomp_mgmt_get_event(riomp_mport_t mport_handle, struct riomp_mgmt_event *evt)
{
	struct rio_event revent;
	ssize_t bytes = 0;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (!evt) return -EINVAL;

	bytes = read(hnd->fd, &revent, sizeof(revent));
	if (bytes == -1)
		return -errno;
	if (bytes != sizeof(revent)) {
		return -EIO;
	}

	if (revent.header == RIO_EVENT_DOORBELL) {
		evt->u.doorbell.payload = revent.u.doorbell.payload;
		evt->u.doorbell.rioid = revent.u.doorbell.rioid;
	} else if (revent.header == RIO_EVENT_PORTWRITE) {
		memcpy(&evt->u.portwrite.payload, &revent.u.portwrite.payload, sizeof(evt->u.portwrite.payload));
	} else {
		return -EIO;
	}
	evt->header = revent.header;

	return 0;
}

/*
 * send an event (only doorbell supported)
 */
int riomp_mgmt_send_event(riomp_mport_t mport_handle, struct riomp_mgmt_event *evt)
{
	struct rapidio_mport_handle *hnd = mport_handle;
	struct rio_event sevent;
	char *p = (char*)&sevent;
	ssize_t ret;
	unsigned int len=0;

	if(hnd == NULL)
		return -EINVAL;

	if (!evt) return -EINVAL;

	if (evt->header != RIO_EVENT_DOORBELL) return -EOPNOTSUPP;

	sevent.header = RIO_DOORBELL;
	sevent.u.doorbell.rioid = evt->u.doorbell.rioid;
	sevent.u.doorbell.payload = evt->u.doorbell.payload;

	while (len < sizeof(sevent)) {
		ret = write(hnd->fd, p+len, sizeof(sevent)-len);
		if (ret == -1) {
			return -errno;
		} else {
			len += ret;
		}
	}

	return 0;
}

/*
 * Set destination ID of local mport device
 */
int riomp_mgmt_destid_set(riomp_mport_t mport_handle, uint16_t destid)
{
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	if (ioctl(hnd->fd, RIO_MPORT_MAINT_HDID_SET, &destid))
		return errno;
	return 0;
}

/*
 * Create a new kernel device object
 */
int riomp_mgmt_device_add(riomp_mport_t mport_handle, uint16_t destid, uint8_t hc, uint32_t ctag,
		    const char *name)
{
	struct rio_rdev_info dev;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dev.destid = destid;
	dev.hopcount = hc;
	dev.comptag = ctag;
	if (name)
		strncpy(dev.name, name, RIO_MAX_DEVNAME_SZ);
	else
		*dev.name = '\0';

	if (ioctl(hnd->fd, RIO_DEV_ADD, &dev))
		return errno;
	return 0;
}

/*
 * Delete existing kernel device object
 */
int riomp_mgmt_device_del(riomp_mport_t mport_handle, uint16_t destid, uint8_t hc, uint32_t ctag,
			  const char *name)
{
	struct rio_rdev_info dev;
	struct rapidio_mport_handle *hnd = mport_handle;

	if(hnd == NULL)
		return -EINVAL;

	dev.destid = destid;
	dev.hopcount = hc;
	dev.comptag = ctag;
	if (name)
		strncpy(dev.name, name, RIO_MAX_DEVNAME_SZ);
	else
		*dev.name = '\0';

	if (ioctl(hnd->fd, RIO_DEV_DEL, &dev))
		return errno;
	return 0;
}

/* Mailbox functions */
int riomp_sock_mbox_create_handle(uint8_t mport_id, uint8_t mbox_id,
			     riomp_mailbox_t *mailbox)
{
	int fd;
	struct rapidio_mport_mailbox *lhandle = NULL;

	/* Open mport */
	fd = riomp_sock_mbox_init();
	if (fd < 0)
		return -1;

	/* TODO claim mbox_id */

	/* Create handle */
	lhandle = (struct rapidio_mport_mailbox *)malloc(sizeof(struct rapidio_mport_mailbox));
	if(!(lhandle)) {
		close(fd);
		return -2;
	}

	lhandle->fd = fd;
	lhandle->mport_id = mport_id;
	*mailbox = lhandle;
	return 0;
}

int riomp_sock_socket(riomp_mailbox_t mailbox, riomp_sock_t *socket_handle)
{
	struct rapidio_mport_socket *handle = NULL;

	/* Create handle */
	handle = (struct rapidio_mport_socket *)calloc(1, sizeof(struct rapidio_mport_socket));
	if(!handle) {
		printf("error in calloc\n");
		return -1;
	}

	handle->mbox = mailbox;
	handle->ch.id = 0;
	*socket_handle = handle;
	return 0;
}

int riomp_sock_send(riomp_sock_t socket_handle, void *buf, uint32_t size)
{
	int ret;
	struct rapidio_mport_socket *handle = socket_handle;
	struct rio_cm_msg msg;

	msg.ch_num = handle->ch.id;
	msg.size = size;
	msg.msg = buf;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_SEND, &msg);
	if (ret)
		return errno;

	return 0;
}

int riomp_sock_receive(riomp_sock_t socket_handle, void **buf,
			 uint32_t size, uint32_t timeout)
{
	int ret;
	struct rapidio_mport_socket *handle = socket_handle;
	struct rio_cm_msg msg;

	msg.ch_num = handle->ch.id;
	msg.size = size;
	msg.msg = *buf;
	msg.rxto = timeout;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_RECEIVE, &msg);
	if (ret)
		return errno;

	return 0;
}

int riomp_sock_release_receive_buffer(riomp_sock_t socket_handle,
					void *buf) /* always 4k aligned buffers */
{
	free(buf);
	return 0;
}

int riomp_sock_close(riomp_sock_t *socket_handle)
{
	int ret;
	struct rapidio_mport_socket *handle = *socket_handle;
	uint16_t ch_num;

	if(!handle)
		return -1;

	ch_num = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CLOSE, &ch_num);
	if (ret < 0) {
		printf("CLOSE IOCTL: returned %d for ch_num=%d (errno=%d)\n", ret, (*socket_handle)->ch.id, errno);
		ret = errno;
	}

	free(handle);
	return ret;
}

int riomp_sock_mbox_destroy_handle(riomp_mailbox_t *mailbox)
{
	struct rapidio_mport_mailbox *mbox = *mailbox;

	if(mbox != NULL) {
		close(mbox->fd);
		free(mbox);
		return 0;
	}

	return -1;

}

int riomp_sock_bind(riomp_sock_t socket_handle, uint16_t local_channel)
{
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num;
	int ret;
	struct rio_cm_channel cdev;

	ch_num = local_channel;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CREATE, &ch_num);
	if (ret < 0)
		return errno;

	cdev.id = ch_num;
	cdev.mport_id = handle->mbox->mport_id;
	handle->ch.id = cdev.id;
	handle->ch.mport_id = cdev.mport_id;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_BIND, &cdev);
	if (ret < 0)
		return errno;

	return 0;
}

int riomp_sock_listen(riomp_sock_t socket_handle)
{
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num;
	int ret;

	ch_num = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_LISTEN, &ch_num);
	if (ret)
		return errno;

	return 0;
}

int riomp_sock_accept(riomp_sock_t socket_handle, riomp_sock_t *conn,
			uint32_t timeout)
{
	struct rapidio_mport_socket *handle = socket_handle;
	struct rapidio_mport_socket *new_handle = *conn;
	struct rio_cm_accept param;
	int ret;

	if(!handle || !conn)
		return -1;

	param.ch_num = handle->ch.id;
	param.wait_to = timeout;

	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_ACCEPT, &param);
	if (ret)
		return errno;

	if (new_handle)
		new_handle->ch.id = param.ch_num;

	return 0;
}

int riomp_sock_connect(riomp_sock_t socket_handle, uint32_t remote_destid,
			 uint8_t remote_mbox, uint16_t remote_channel)
{
	struct rapidio_mport_socket *handle = socket_handle;
	uint16_t ch_num = 0;
	struct rio_cm_channel cdev;
	int ret;

	if (handle->ch.id == 0) {
		if (ioctl(handle->mbox->fd, RIO_CM_CHAN_CREATE, &ch_num))
			return errno;
		handle->ch.id = ch_num;
	}

	/* Configure and Send Connect IOCTL */
	handle->ch.remote_destid  = remote_destid;
	handle->ch.remote_mbox    = remote_mbox;
	handle->ch.remote_channel = remote_channel;
	handle->ch.mport_id = handle->mbox->mport_id;
	cdev.remote_destid  = remote_destid;
	cdev.remote_mbox    = remote_mbox;
	cdev.remote_channel = remote_channel;
	cdev.mport_id = handle->mbox->mport_id;
	cdev.id = handle->ch.id;
	ret = ioctl(handle->mbox->fd, RIO_CM_CHAN_CONNECT, &cdev);
	if (ret) {
		return errno;
	}

	return 0;
}

int riomp_sock_request_send_buffer(riomp_sock_t socket_handle,
				     void **buf) //always 4k aligned buffers
{
	/* socket_handle won't be used for now */

	*buf = malloc(0x1000); /* Always allocate maximum size buffers */
	if (*buf == NULL)
		return -1;

	return 0;
}

int riomp_sock_release_send_buffer(riomp_sock_t socket_handle,
				     void *buf) /* always 4k aligned buffers */
{
	free(buf);
	return 0;
}


const char *speed_to_string(int speed)
{
	switch(speed){
		case RIO_LINK_DOWN:
			return "LINK DOWN";
		case RIO_LINK_125:
			return "1.25Gb";
		case RIO_LINK_250:
			return "2.5Gb";
		case RIO_LINK_312:
			return "3.125Gb";
		case RIO_LINK_500:
			return "5.0Gb";
		case RIO_LINK_625:
			return "6.25Gb";
		default:
			return "ERROR";
	}
}

const char *width_to_string(int width)
{
	switch(width){
		case RIO_LINK_1X:
			return "1x";
		case RIO_LINK_1XR:
			return "1xR";
		case RIO_LINK_2X:
			return "2x";
		case RIO_LINK_4X:
			return "4x";
		case RIO_LINK_8X:
			return "8x";
		case RIO_LINK_16X:
			return "16x";
		default:
			return "ERROR";
	}
}

void riomp_mgmt_display_info(struct riomp_mgmt_mport_properties *attr)
{
	printf("\n+++ SRIO mport configuration +++\n");
	printf("mport: hdid=%d, id=%d, idx=%d, flags=0x%x, sys_size=%s\n",
		attr->hdid, attr->id, attr->index, attr->flags,
		attr->sys_size?"large":"small");

	printf("link: speed=%s width=%s\n", speed_to_string(attr->link_speed),
		width_to_string(attr->link_width));

	if (attr->flags & RIO_MPORT_DMA) {
		printf("DMA: max_sge=%d max_size=%d alignment=%d (%s)\n",
			attr->dma_max_sge, attr->dma_max_size, attr->dma_align,
			(attr->flags & RIO_MPORT_DMA_SG)?"HW SG":"no HW SG");
	} else
		printf("No DMA support\n");
	printf("\n");
}

#ifdef TEST_UMDD

int main()
{
        const int mport_id = 0;

	int chan = 0;

	chan = riomp_mgmt_mport_umd_select_channel(mport_id, 7);
	assert(chan == 7);
	assert(riomp_mgmt_mport_umd_is_channel_selected(mport_id, 7) > 0);
	riomp_mgmt_mport_umd_deselect_channel(mport_id, 7);
	assert(riomp_mgmt_mport_umd_is_channel_selected(mport_id, 7) == 0);

	assert(inuse_bitmask[mport_id] == 0);

	for (chan = 1; chan < 8; chan++) {
		int chan2 = riomp_mgmt_mport_umd_select_channel(mport_id, 0);
		assert(chan2 > 0);
	}

	// Full??
	chan = riomp_mgmt_mport_umd_select_channel(mport_id, 0);
	assert(chan == -1);

	for (chan = 1; chan < 8; chan++) {
		riomp_mgmt_mport_umd_deselect_channel(mport_id, chan);
	}

	assert(inuse_bitmask[mport_id] == 0);

	if (getenv("UMDD_LIB") == NULL) {
		printf("Cannot find $UMDD_LIB in environment. Test ending.\n");
		return 0;
	}
	if (access(getenv("UMDD_LIB"), R_OK)) {
		printf("Cannot find file $UMDD_LIB=%s. Test aborted.\n", getenv("UMDD_LIB"));
                return 1;
        }

	riomp_mport_t h1 = NULL, h2 = NULL;

	const char* chan_s = getenv("UMDD_CHAN");
	if (chan_s != NULL) {
		const int chan = atoi(chan_s);
		int r = riomp_mgmt_mport_create_handle(mport_id, /*flags*/ 0, &h1);
		assert(r == 0);
		assert(h1->umd_chan != -chan);
		assert(riomp_mgmt_mport_umd_is_channel_selected(mport_id, chan) > 0);
		riomp_mgmt_mport_destroy_handle(&h1);
		riomp_mgmt_mport_umd_deselect_channel(mport_id, chan);
	}

	int r1 = riomp_mgmt_mport_create_handle(mport_id, UMD_SELECT_NEXT_CHANNEL, &h1);
	assert(r1 == 0);
	int r2 = riomp_mgmt_mport_create_handle(mport_id, UMD_SELECT_NEXT_CHANNEL, &h2);
	assert(r2 == 0);

	const int ch1 = h1->umd_chan;
	const int ch2 = h2->umd_chan;
	assert(ch1 != ch2);
	riomp_mgmt_mport_destroy_handle(&h2);
	riomp_mgmt_mport_destroy_handle(&h1);

	assert(riomp_mgmt_mport_umd_is_channel_selected(mport_id, ch1) == 0);
	assert(riomp_mgmt_mport_umd_is_channel_selected(mport_id, ch2) == 0);

	return 0;
}
#endif
