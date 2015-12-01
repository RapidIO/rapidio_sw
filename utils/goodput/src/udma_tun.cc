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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/inotify.h>

#include <pthread.h>
#include <sstream>

#include <sched.h>

#include "libcli.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "rapidio_mport_dma.h"
#include "liblog.h"

#include "time_utils.h"
#include "worker.h"
#include "goodput.h"
#include "mhz.h"

#ifdef USER_MODE_DRIVER
#include "dmachan.h"
#include "hash.cc"
#include "lockfile.h"
#include "tun_ipv4.h"
#endif

extern "C" {
void zero_stats(struct worker *info);
bool umd_check_cpu_allocation(struct worker *info);
bool TakeLock(struct worker* info, const char* module, int instance);

void* umd_dma_fifo_proc_thr(void *parm);

uint32_t crc32(uint32_t crc, const void *buf, size_t size);
};

/** \bried NREAD data from peer at high priority, all-in-one, blocking
 * \param[in] info C-like this
 * \param destid RIO destination id of peer
 * \param rio_addr RIO mem address into peer's IBwin, not 50-but compatible
 * \param size How much data to read, up to 16 bytes
 * \param[out] Points to where data will be deposited
 * \retuen true if NREAD completed OK
 */
static inline bool udma_nread_mem(struct worker *info, const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out)
{
	if(info == NULL || size < 1 || size > 16 || data_out == NULL) return false;

#ifdef UDMA_TUN_DEBUG_NREAD
	DBG("\n\tNREAD from RIO %d bytes destid %u addr 0x%llx\n", size, destid, rio_addr);
#endif

	DMAChannel* dmac = info->umd_dch2;

	DMAChannel::DmaOptions_t dmaopt; memset(&dmaopt, 0, sizeof(dmaopt));
	dmaopt.destid      = destid;
	dmaopt.prio        = 2; // We want to get in front all pending ALL_WRITEs in 721 silicon
	dmaopt.bcount      = size;
	dmaopt.raddr.lsb64 = rio_addr;

	struct seq_ts tx_ts;
	uint32_t umd_dma_abort_reason = 0;
	DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	int q_was_full = !dmac->queueDmaOpT2((int)NREAD, dmaopt, data_out, size, umd_dma_abort_reason, &tx_ts);

	if (!umd_dma_abort_reason) DBG("\n\tPolling FIFO transfer completion destid=%d\n", destid);

	for(int i = 0;
	    !q_was_full && !info->stop_req && (i < 100) && !dmac->scanFIFO(wi, info->umd_sts_entries*8);
	    i++) {
		usleep(1);
	}

	if (umd_dma_abort_reason || (dmac->queueSize() > 0)) { // Boooya!! Peer not responding
		CRIT("\n\tChan %u stalled with full %d stop %d Qsize %d abort reason %x %s\n",
		      info->umd_chan2, q_was_full, info->stop_req, dmac->queueSize(),
		      umd_dma_abort_reason, DMAChannel::abortReasonToStr(umd_dma_abort_reason));
		// XXX Cleanup, nuke the one BD
		return false;
	}

#ifdef UDMA_TUN_DEBUG_NREAD
	std::stringstream ss;
	for(int i = 0; i < 16; i++) {
		char tmp[9] = {0};
		snprintf(tmp, 8, "%02x ", wi[0].t2_rddata[i]);
		ss << tmp;
	}
	DBG("\n\tNREAD-in data: %s\n", ss.str().c_str());
#endif

	memcpy(data_out, wi[0].t2_rddata, size);

	return true;
}

const int DESTID_TRANSLATE = 1;

static std::map <uint16_t, bool> bad_destid;
static std::map <uint16_t, bool> good_destid;

static bool inline umd_dma_tun_process_tun_RX(struct worker *info, DmaChannelInfo_t* dci, const int tun_fd, const uint16_t my_destid);

/** \brief Thread that services Tun TX, sends L3 frames to peer and does RIO TX throttling
 * Update RP via NREAD every 8 buffers; when local q is 2/3 full do it after each send
 */
void* umd_dma_tun_RX_proc_thr(void *parm)
{
	struct epoll_event* events = NULL;

        if (NULL == parm) pthread_exit(NULL);

        struct worker* info = (struct worker *)parm;
        if (NULL == info->umd_dch_list[info->umd_chan]) goto exit;
        if (NULL == info->umd_dch2) goto exit;

	if ((events = (struct epoll_event*)calloc(MAX_EPOLL_EVENTS, sizeof(struct epoll_event))) == NULL) goto exit;

	DBG("\n\tReady to receive from multuplexed Tun devices!\n");

        {{
	DmaChannelInfo_t* dch_list[6] = {0};

	int dch_cnt = 0;
	int dch_cur = 0;
	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) dch_list[dch_cnt++] = info->umd_dch_list[ch];

	uint64_t tx_cnt = 0;

        info->umd_dma_tap_proc_alive = 1;
        sem_post(&info->umd_dma_tap_proc_started);

        const uint16_t my_destid = info->umd_dch2->getDestId();

        bad_destid[my_destid] = true;

again:
        while(! info->stop_req) {
		// XXX Maybe pick the one with the least full queue??
		DmaChannelInfo_t* dci = dch_list[dch_cur++];
		if (dch_cur >= dch_cnt) dch_cur = 0; // Wrap-around

		const int epoll_cnt = epoll_wait (info->umd_epollfd, events, MAX_EPOLL_EVENTS, -1);
		if (epoll_cnt < 0) {
			CRIT("\n\tepoll_wait failed: %s\n", strerror(errno))
			break;
		}

		for (int epi = 0; epi < epoll_cnt; epi++) {
			if (info->stop_req) break;
			if (events[epi].data.fd == info->umd_sockp[1]) goto exit; // Time to quit!

			const int tun_fd = events[epi].data.fd;

			// We do NOT use edge-triggered epoll for Tun.
			// We want to process one L3 frame per incoming Tun in round-robin
			// fashion. If more frames are available epoll_wait will not block.

			if ((events[epi].events & EPOLLERR) || (events[epi].events & EPOLLHUP) || (!(events[epi].events & EPOLLIN))) {
				CRIT("\n\tepoll error for fd=%d: %s\n", tun_fd, strerror(errno));
				//close (tun_fd); // really? destroy peer as well????
				continue;
			}

			if (umd_dma_tun_process_tun_RX(info, dci, tun_fd, my_destid)) tx_cnt++;
		}
        }

        if (info->stop_req == SOFT_RESTART) {
                DBG("\n\tSoft restart requested, sleeping on semaphore\n");
                sem_wait(&info->umd_dma_tap_proc_started);
                DBG("\n\tAwakened after Soft restart!\n");
                goto again;
        }

        goto no_post;
        }}
exit:
        sem_post(&info->umd_dma_tap_proc_started);

no_post:
	free(events);
        info->umd_dma_tap_proc_alive = 0;

        pthread_exit(parm);
} // END umd_dma_tun_RX_proc_thr

static bool inline umd_dma_tun_process_tun_RX(struct worker *info, DmaChannelInfo_t* dci, const int tun_fd, const uint16_t my_destid)
{
	if (info == NULL || dci == NULL || tun_fd < 0 || my_destid == 0xffff) return false;

	int destid_fd = -1;
	int destid_dpi = -1;
	int is_bad_destid = 0;
	DmaPeerDestid_t* peer = NULL;
	bool first_message;
	int outstanding = 0; // This is our guess of what's not consumed at other end, per-destid

        const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;
	
	uint8_t* buffer = (uint8_t*)dci->dmamem[dci->oi].win_ptr;

	DMA_L2_t* pL2 = (DMA_L2_t*)buffer;
	const int nread = cread(tun_fd, buffer+DMA_L2_SIZE, info->umd_tun_MTU);

	{{
	uint32_t* pkt = (uint32_t*)(buffer+DMA_L2_SIZE);
	const uint32_t dest_ip_v4 = ntohl(pkt[4]); // XXX IPv6 will stink big here

	destid_dpi = (dest_ip_v4 & 0xFFFF) - DESTID_TRANSLATE;
	}}

	is_bad_destid += bad_destid.find(destid_dpi) != bad_destid.end();

	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	do {{
	  std::map <int, uint16_t>::iterator itfd = info->umd_dma_did_peer_fd2did.find(tun_fd);
	  if (itfd == info->umd_dma_did_peer_fd2did.end()) { is_bad_destid++; break; }
	  destid_fd = itfd->second;
	  std::map <uint16_t, DmaPeerDestid_t*>::iterator itp = info->umd_dma_did_peer.find(destid_fd);
	  if (itp == info->umd_dma_did_peer.end()) { is_bad_destid++; destid_fd = -1; break; }
	  peer = itp->second;
	}} while(0);
	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	assert(destid_fd == destid_dpi);

	if (info->stop_req || peer->stop_req) goto error;

#ifdef UDMA_TUN_DEBUG
	{{
	const uint32_t crc = crc32(0, buffer+DMA_L2_SIZE, nread);
	DBG("\n\tGot from tun? %d+%d bytes (L7 CRC32 0x%x) to RIO destid %u%s\n",
		 nread, DMA_L2_SIZE,
		 crc, destid_dpi,
		 is_bad_destid? " BLACKLISTED": "");
	}};
#endif

	if (is_bad_destid) {
		send_icmp_host_unreachable(tun_fd, buffer+DMA_L2_SIZE, nread);
		goto error;
	}

	first_message = peer->tx_cnt == 0 || good_destid.find(destid_dpi) == good_destid.end();

	// We force reading RP from a "new" destid as a RIO ping as
	// NWRITE does not barf  on bad destids

	if (first_message || (peer->tx_cnt % 8) == 0 || dci->dch->queueSize() > Q_THR) { // This must be done per-destid
		uint32_t newRP = ~0;
		if (udma_nread_mem(info, destid_dpi, peer->rio_addr, sizeof(newRP), (uint8_t*)&newRP)) {
			DBG("\n\tPulled RP from destid %u old RP=%d actual RP=%d\n", destid_dpi, peer->RP, newRP);
			peer->RP = newRP;
			if (first_message) peer->WP = newRP; // XXX maybe newRP+1 ?? Test
		} else {
			send_icmp_host_unreachable(tun_fd, buffer+DMA_L2_SIZE, nread); // XXX which tun fd??
			DBG("\n\tHW error, something is FOBAR with Chan %u\n", info->umd_chan2);

			bad_destid[destid_dpi] = true;
			peer->stop_req = 1; // XXX Who reaps the dead peers?
			goto error;
		}
	}

	do {{
	  if (peer->WP == peer->RP) { break; }
	  if (peer->WP > peer->RP)  { outstanding = peer->WP-peer->RP; break; }
	  //if (WP == (info->umd_tx_buf_cnt-2)) { outstanding = RP; break; }
	  outstanding = peer->WP + dci->tx_buf_cnt-2 - peer->RP;
	}} while(0);

	DBG("\n\tWP=%d guessed { RP=%d outstanding=%d } %s\n", peer->WP, peer->RP, outstanding, (outstanding==(dci->tx_buf_cnt-1))? "FULL": "");

	if (info->stop_req || peer->stop_req) goto error;

	if (outstanding == (dci->tx_buf_cnt-1)) {
		CRIT("\n\tPeer destid=%u is FULL, dropping frame!\n", destid_dpi);
		// XXX Maybe send back ICMP Host Unreachable? TODO: Study RFCs
		goto error;
	}

	pL2->destid = htons(my_destid);
	pL2->len    = htonl(DMA_L2_SIZE + nread);
	pL2->RO     = 1;

	if (info->stop_req) goto error;

	// Barry dixit "If full sleep for (queue size / 2) nanoseconds"
	for (int i = 0; i < (dci->tx_buf_cnt-1)/2; i++) {
		if (! dci->dch->queueFull()) break;
		struct timespec tv = { 0, 1};
		nanosleep(&tv, NULL);
	}

	if (dci->dch->queueFull()) {
		DBG("\n\tQueue full #1!\n");
		goto error; // Drop L3 frame
	}
	if (info->stop_req) goto error;

	dci->dmaopt[dci->oi].destid      = destid_dpi;
	dci->dmaopt[dci->oi].bcount      = ntohl(pL2->len);
	dci->dmaopt[dci->oi].raddr.lsb64 = peer->rio_addr + sizeof(uint32_t) + peer->WP * BD_PAYLOAD_SIZE(info);

	DBG("\n\tSending to RIO %d+%d bytes to RIO destid %u addr 0x%llx WP=%d oi=%d\n",
	    nread, DMA_L2_SIZE, destid_dpi, dci->dmaopt[dci->oi].raddr.lsb64, peer->WP, dci->oi);

	dci->dch->setCheckHwReg(first_message);

	info->umd_dma_abort_reason = 0;
	if (! dci->dch->queueDmaOpT1(ALL_NWRITE, dci->dmaopt[dci->oi], dci->dmamem[dci->oi],
					info->umd_dma_abort_reason, &info->meas_ts)) {
		if(info->umd_dma_abort_reason != 0) { // HW error
			// ICMPv4 dest unreachable id bad destid 
			send_icmp_host_unreachable(tun_fd, buffer+DMA_L2_SIZE, nread); // XXX which tun
			DBG("\n\tHW error, triggering soft restart\n");

			bad_destid[destid_dpi] = true;

			info->stop_req = SOFT_RESTART; // XXX of which channel?
			goto error;
		} else { // queue really full
			DBG("\n\tQueue full #2!\n");
			goto error; // Drop L3 frame
		}
	}
	if (first_message) good_destid[destid_dpi] = true;

	dci->oi++; if (dci->oi == (dci->tx_buf_cnt-1)) dci->oi = 0; // Account for T3

	// For just one destdid WP==oi but we keep them separate

	peer->WP++; // This must be done per-destid
	if (peer->WP == (dci->tx_buf_cnt-1)) peer->WP = 0; // Account for T3 missing IBwin cell, ALL must have the same bufc!

	peer->tx_cnt++;

	return true;

error:
	return false;
}


/** \brief Callback from FIFO isolcpu thread
 * Look quickly at L2 headers of IB buffers, signal to main thread if there is work to do
 */
void umd_dma_goodput_tun_callback(struct worker *info)
{
	if(info == NULL) return;
        assert(info->ib_ptr);

	static uint64_t cbk_iter = 0;

	cbk_iter++;

	if (info->umd_dma_did_peer.size() == 0) return;

	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);

	std::map <uint16_t, DmaPeerDestid_t*>::iterator itp = info->umd_dma_did_peer.begin();
	for (; !info->stop_req && itp != info->umd_dma_did_peer.end(); itp++) {
		DmaPeerDestid_t* peer = itp->second;
		assert(peer);

		if (peer->stop_req) continue;

		volatile uint32_t* pRP = (uint32_t*)peer->ib_ptr;

		int k = *pRP;
		assert(k >= 0);
		assert(k < (info->umd_tx_buf_cnt-1));

		if (peer->rio_rx_bd_ready_size >= (info->umd_tx_buf_cnt-1)) continue; // Receiver too slow, inc slow count and wait!

 		pthread_spin_lock(&peer->rio_rx_bd_ready_splock);

		int cnt = 0;
		int idx = peer->rio_rx_bd_ready_size; // If not zero then RIO RX thr is sloow
		for (int i = 0; i < (info->umd_tx_buf_cnt-1); i++) {
			if (info->stop_req) break;
			if (peer->stop_req) continue;

			if(0 != peer->rio_rx_bd_L2_ptr[k]->RO) {
#ifdef UDMA_TUN_DEBUG_IB
				DBG("\n\tFound ready buffer at RP=%d -- iter %llu\n", k, cbk_iter);
#endif

				assert(k < (info->umd_tx_buf_cnt-1));

				peer->rio_rx_bd_ready[idx++] = k;
				peer->rio_rx_bd_L2_ptr[k]->RO = 0; // So we won't revisit
				cnt++;
			} else {
				break; // Stop at 1st non-ready IB bd??
			}

			k++; if(k == (info->umd_tx_buf_cnt-1)) k = 0; // RP wrap-around

		}

		if (cnt > 0) {
			peer->rio_rx_bd_ready_size += cnt;
			assert(peer->rio_rx_bd_ready_size <= (info->umd_tx_buf_cnt-1));
		}

		pthread_spin_unlock(&peer->rio_rx_bd_ready_splock);

		if (cnt == 0) continue;

		if (info->stop_req) break;

#ifdef UDMA_TUN_DEBUG_IB
		DBG("\n\tFound %d ready buffers at iter %llu\n", cnt, cbk_iter);
#endif

		sem_post(&peer->rio_rx_work);
	} // END for each peer

	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
}

/** \brief This helper thread wakes up the main thread at quitting time */
void* umd_dma_wakeup_proc_thr(void* arg)
{
	if(arg == NULL) return NULL;
	struct worker* info = (struct worker*)arg;

        while (!info->stop_req) // every 100 mS which is about HZ. stupid
		usleep(100 * 1000);
	
	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);

	std::map <uint16_t, DmaPeerDestid_t*>::iterator itp = info->umd_dma_did_peer.begin();
	for (; itp != info->umd_dma_did_peer.end(); itp++) {
		DmaPeerDestid_t* peer = itp->second;
		assert(peer);
		sem_post(&peer->rio_rx_work);
	}

	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	return NULL;
}

uint32_t Test_NREAD(struct worker* info)
{
  uint32_t newRP = 0;
  if (udma_nread_mem(info, info->did, info->rio_addr, sizeof(newRP), (uint8_t*)&newRP)) {
	DBG("\n\tAt start on destid %u RP=%u\n", info->did, newRP);	
  } else {
	return ~0;
  }
  return newRP;
}

const int CH2_BUFC = 0x20;

bool umd_dma_goodput_tun_setup_chan2(struct worker *info)
{
	if (info == NULL) return false;

        info->umd_dch2 = new DMAChannel(info->mp_num, info->umd_chan2, info->mp_h);
        if (NULL == info->umd_dch2) {
                CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan2, info->mp_num, info->mp_h);
                return false;
        }

        // TX - Chan 2
        info->umd_dch2->setCheckHwReg(true);
        if (!info->umd_dch2->alloc_dmatxdesc(CH2_BUFC)) {
                CRIT("\n\talloc_dmatxdesc failed: bufs %d", CH2_BUFC);
                return false;
        }
        if (!info->umd_dch2->alloc_dmacompldesc(info->umd_sts_entries)) { // Same as for Chan 1
                CRIT("\n\talloc_dmacompldesc failed: entries %d", info->umd_sts_entries);
                return false;
        }

        info->umd_dch2->resetHw();
        if (!info->umd_dch2->checkPortOK()) {
                CRIT("\n\tPort %d is not OK!!! Exiting...", info->umd_chan2);
                return false;
        }

	return true;
}

bool umd_dma_goodput_tun_setup_chanN(struct worker *info, const int n)
{
	if (info == NULL) return false;

	if (n < 0 || n > 7) return false;
	if (n < info->umd_chan || n > info->umd_chan_n) return false;

	if (info->umd_dch_list[n] != NULL) return false; // already setup

	DmaChannelInfo_t* dci = (DmaChannelInfo_t*)calloc(1, sizeof(DmaChannelInfo_t));
	if (dci == NULL) return false;

	info->umd_dch_list[n] = dci;

	dci->chan        = n;
	dci->tx_buf_cnt  = info->umd_tx_buf_cnt;
	dci->sts_entries = info->umd_sts_entries;

        dci->dch = new DMAChannel(info->mp_num, n, info->mp_h);
        if (NULL == dci->dch) {
                CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
                     dci->chan, info->mp_num, info->mp_h);
		goto error;
        }

        // TX
        if (! dci->dch->alloc_dmatxdesc(dci->tx_buf_cnt)) {
                CRIT("\n\talloc_dmatxdesc failed: bufs %d", dci->tx_buf_cnt);
		goto error;
        }
        if (! dci->dch->alloc_dmacompldesc(dci->sts_entries)) {
                CRIT("\n\talloc_dmacompldesc failed: entries %d", dci->sts_entries);
		goto error;
        }

        memset(dci->dmamem, 0, sizeof(dci->dmamem));
        memset(dci->dmaopt, 0, sizeof(dci->dmaopt));

        // TX - Chan 1
        for (int i = 0; i < dci->tx_buf_cnt; i++) {
                if (! dci->dch->alloc_dmamem(BD_PAYLOAD_SIZE(info), dci->dmamem[i])) {
                        CRIT("\n\talloc_dmamem failed: i %d size %x", i, BD_PAYLOAD_SIZE(info));
			goto error;
                };
                memset(dci->dmamem[i].win_ptr, 0, dci->dmamem[i].win_size);
        }

        dci->dch->resetHw();
        if (! dci->dch->checkPortOK()) {
                CRIT("\n\tPort %d is not OK!!! Exiting...", dci->chan);
		goto error;
        }

	return true;

error:
	if (dci->dch != NULL) delete dci->dch;
	info->umd_dch_list[n] = NULL;
	free(dci);
	return false;
}

bool umd_dma_goodput_tun_setup_TUN(struct worker *info, DmaPeerDestid_t* peer, uint16_t my_destid)
{
        char if_name[IFNAMSIZ] = {0};
        int flags = IFF_TUN | IFF_NO_PI;

	if (info == NULL || peer == NULL) return false;
	if (my_destid == peer->destid) return false;

        memset(peer->tun_name, 0, sizeof(peer->tun_name));

        // Initialize tun/tap interface
        if ((peer->tun_fd = tun_alloc(if_name, flags)) < 0) {
                CRIT("Error connecting to tun/tap interface %s!\n", if_name);
                return false;
        }
        strncpy(peer->tun_name, if_name, sizeof(peer->tun_name)-1);

	{{
	  struct epoll_event event;
	  event.data.fd = peer->tun_fd;
	  event.events = EPOLLIN; // | EPOLLET;
          if (epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, peer->tun_fd, &event) < 0) {
		CRIT("\n\tFailed to add tun_fd %d for peer destid %u to epoll set %d\n",
		     peer->tun_fd, peer->destid, info->umd_epollfd);
                close(peer->tun_fd); peer->tun_fd = -1;
	  }
	}}

        // Configure tun/tap interface for pointo-to-point IPv4, L2, no ARP, no multicast

        char TapIPv4Addr[17] = {0};
        const uint16_t my_destid_tun = my_destid + DESTID_TRANSLATE;
        const uint16_t peer_destid_tun = peer->destid + DESTID_TRANSLATE;

        snprintf(TapIPv4Addr, 16, "169.254.%d.%d pointopoint 169.254.%d.%d",
                 (my_destid_tun >> 8) & 0xFF,   my_destid_tun & 0xFF,
                 (peer_destid_tun >> 8) & 0xFF, peer_destid_tun & 0xFF);

        char ifconfig_cmd[257] = {0};
        snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s %s mtu %d up",
                                    if_name, TapIPv4Addr, info->umd_tun_MTU);
        const int rr = system(ifconfig_cmd);
        if(rr >> 8) {
                peer->tun_name[0] = '\0';
		// No need to remove from epoll set, close does that as it isn't dup(2)'ed
                close(peer->tun_fd); peer->tun_fd = -1;
                return false;
        }

        snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
        system(ifconfig_cmd);

        INFO("\n\t%s %s mtu %d on DMA Chan=%d,...,Chan_n=%d Chan2=%d my_destid=%u peer_destid=%u #buf=%d #fifo=%d\n",
             if_name, TapIPv4Addr, info->umd_tun_MTU,
             info->umd_chan, info->umd_chan_n, info->umd_chan2,
             my_destid, peer->destid,
             info->umd_tx_buf_cnt, info->umd_sts_entries);

	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	info->umd_dma_did_peer_fd2did[peer->tun_fd] = peer->destid;
	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	return true;
}

void* umd_dma_tun_TX_proc_thr(void* arg)
{
	if (arg == NULL) return NULL;

	DmaPeerDestid_t* peer = (DmaPeerDestid_t*)arg;
	struct worker* info = peer->info;

	uint64_t rx_ok = 0; ///< TX'ed into Tun device

	volatile uint32_t* pRP = (uint32_t*)peer->ib_ptr;

	DBG("\n\tReady to receive from RIO from destid %u!\n", peer->destid);

again: // Receiver (from RIO), TUN TX: Ingest L3 frames into Tun (zero-copy), update RP, set RO=0
	while (!info->stop_req && !peer->stop_req) {

		// We could double-lock a spinlock here but experimentally we use a sema here.
	        sem_wait(&peer->rio_rx_work); // To BEW: how does this impact RX latecy?

        	if (info->stop_req || peer->stop_req) break;

		//DBG("\n\tInbound %d buffers(s) ready.\n", info->umd_dma_rio_rx_bd_ready_size);

		assert(peer->rio_rx_bd_ready_size <= (info->umd_tx_buf_cnt-1));

		// Make this quick & sweet so we don't hose the FIFO thread for long
		int cnt = 0;
		int ready_bd_list[info->umd_tx_buf_cnt]; memset(ready_bd_list, 0xff, sizeof(ready_bd_list));
		pthread_spin_lock(&peer->rio_rx_bd_ready_splock);
		for (int i = 0; i < peer->rio_rx_bd_ready_size; i++) {
                        assert(peer->rio_rx_bd_ready[i] < (info->umd_tx_buf_cnt-1));
			ready_bd_list[cnt++] = peer->rio_rx_bd_ready[i];
		}
		peer->rio_rx_bd_ready_size = 0;
		pthread_spin_unlock(&peer->rio_rx_bd_ready_splock);

#ifdef UDMA_TUN_DEBUG
		DBG("\n\tInbound %d buffers(s) will be processed.\n", cnt);
#endif

        	if (info->stop_req || peer->stop_req) break;

		for (int i = 0; i < cnt && !info->stop_req; i++) {
			const int rp = ready_bd_list[i];
			assert(rp >= 0);
			assert(rp < (info->umd_tx_buf_cnt-1));

			DMA_L2_t* pL2 = peer->rio_rx_bd_L2_ptr[rp];

			rx_ok++;
			const int payload_size = ntohl(pL2->len) - DMA_L2_SIZE;

			assert(payload_size > 0);
			assert(payload_size <= info->umd_tun_MTU);

			uint8_t* payload = (uint8_t*)pL2 + DMA_L2_SIZE;
                        const int nwrite = cwrite(peer->tun_fd, payload, payload_size);
#ifdef UDMA_TUN_DEBUG
                        const uint32_t crc = crc32(0, payload, payload_size);
                        DBG("\n\tGot a msg of size %d from RIO destid %u (L7 CRC32 0x%x) cnt=%llu, wrote %d to %s\n",
                                 ntohl(pL2->len), ntohs(pL2->destid), crc, rx_ok, nwrite, peer->tun_name);
#endif

			//pL2->RO = 0;
			*pRP = rp;
		}
        } // END while NOT stop requested

	if (info->stop_req == SOFT_RESTART && !peer->stop_req) {
                DBG("\n\tSoft restart requested, sleeping in a 10uS loop\n");
		while(info->stop_req != 0) usleep(10);
                DBG("\n\tAwakened after Soft restart!\n");
		goto again;
	}

	// XXX clean up this dead peer

	return arg;
}

/** \brief Setup a peer and spawn its associated thread
 * \param[in] info this-like
 * \param     destid peer's RIO destid
 * \param     rio_addr address into peer's IBwin
 * \param[in] ib_ptr address allocated for peer into LOCAL IBwin
 * \return true if everything is setup, false otherwise
 */
bool umd_dma_goodput_tun_setup_peer(struct worker* info, const uint16_t destid, const uint64_t rio_addr, const void* ib_ptr)
{
	int rc = 0;
	pthread_t tun_TX_thr;

	if (info == NULL || rio_addr == 0) return false;

        DmaPeerDestid_t* tmppeer = (DmaPeerDestid_t*)calloc(1, sizeof(DmaPeerDestid_t));
        if (tmppeer == NULL) return false;

	tmppeer->destid    = destid;
	tmppeer->rio_addr  = rio_addr;
	//tmppeer->ib_ptr  = ib_ptr; // Done in DmaPeerDestidInit

        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
          info->umd_dma_did_peer[destid] = tmppeer;
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
        
        // Set up array of pointers to IB L2 headers
        if (! DmaPeerDestidInit(info, tmppeer, ib_ptr)) goto exit;
        if (! umd_dma_goodput_tun_setup_TUN(info, tmppeer, info->umd_dch2->getDestId())) goto exit;

        rc = pthread_create(&tun_TX_thr, NULL,
                            umd_dma_tun_TX_proc_thr, (void *)tmppeer);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_tun_TX_proc_thr thread, exiting...");
                goto exit;
        }

	return true;

exit:
        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
          info->umd_dma_did_peer.erase(destid);
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	DmaPeerDestidDestroy(tmppeer); free(tmppeer);

	return false;
}

/** \brief Man battle tank thread -- Pobeda Nasha!
 * \verbatim
We maintain the (per-peer destid) IB window (or
sub-section thereof) in the following format:
+-4b-+---L2 size+MTU----+------------------+
| RP | L2 | L3  payload | ... repeat L2+L3 |
+----+------------------+------------------+
We keep (bufc-1) IB L2+L3 combos
 * \endverbatim
 */
extern "C"
void umd_dma_goodput_tun_demo(struct worker *info)
{
        int rc = 0;
	
	// Note: There's no reason to link info->umd_tx_buf_cnt other than
	// convenience. However the IB ring should never be smaller than
	// info->umd_tx_buf_cnt-1 members -- (dis)counting T3 BD on TX side
	const int IBWIN_SIZE = sizeof(uint32_t) + BD_PAYLOAD_SIZE(info) * (info->umd_tx_buf_cnt-1);

	assert(info->ib_ptr);
	assert(info->ib_byte_cnt >= IBWIN_SIZE);

	memset(info->ib_ptr, 0, info->ib_byte_cnt);

        if (! umd_check_cpu_allocation(info)) return;
        if (! TakeLock(info, "DMA", info->umd_chan2)) return;

	info->umd_dma_fifo_callback = umd_dma_goodput_tun_callback;

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (!umd_dma_goodput_tun_setup_chanN(info, ch)) goto exit;
	}

	if (!umd_dma_goodput_tun_setup_chan2(info)) goto exit;
	//Test_NREAD(info);

	if (info->umd_dch2->getDestId() == 0xffff) {
		CRIT("\n\tMy_destid=0x%x which is BORKED -- bad enumeration?\n", info->umd_dch2->getDestId());
		goto exit;
	}

	info->umd_epollfd = epoll_create1 (0);
	if (info->umd_epollfd < 0) goto exit;

        socketpair(PF_LOCAL, SOCK_STREAM, 0, info->umd_sockp);

	{{
	  struct epoll_event event;
	  event.data.fd = info->umd_sockp[1];
	  event.events = EPOLLIN | EPOLLET;
          if(epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, info->umd_sockp[1], &event) < 0) goto exit;
	}}

	// XXX TODO
	// Phase 1
	// write thread for watching rio destid which bangs info->umd_dma_did_peer, makes new tunnel
	// also kill stale RIO RX threads, purge good_destid, bad_destid under mutex (*)
	// Phase 2
	// write thread that uses mbox/kernel to "broadcast" my ibwin + ALLOCATIONS every 60s
	// write thread that uses mbox/kernel to listen to ibwin bcasts

        init_seq_ts(&info->desc_ts);
        init_seq_ts(&info->fifo_ts);
        init_seq_ts(&info->meas_ts);

        info->umd_fifo_proc_must_die = 0;
        info->umd_fifo_proc_alive = 0;

        rc = pthread_create(&info->umd_fifo_thr.thr, NULL,
                            umd_dma_fifo_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_fifo_proc thread, exiting...");
                goto exit;
        }
        sem_wait(&info->umd_fifo_proc_started);

        if (!info->umd_fifo_proc_alive) {
                CRIT("\n\tumd_fifo_proc thread is dead, exiting..");
                goto exit;
        }

        // Spawn Tap Transmitter Thread
        rc = pthread_create(&info->umd_dma_tap_thr.thr, NULL,
                            umd_dma_tun_RX_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_tun_RX_proc_thr thread, exiting...");
                goto exit;
        }
        sem_wait(&info->umd_dma_tap_proc_started);

        if (!info->umd_dma_tap_proc_alive) {
                CRIT("\n\tumd_dma_tun_RX_proc_thr thread is dead, exiting..");
                goto exit;
        }

	// We need to wake up the RX thread upon quitting so we spawn
	// this lightweight helper thread
	pthread_t wakeup_thr;
        rc = pthread_create(&wakeup_thr, NULL, umd_dma_wakeup_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_wakeup_proc_thr thread, exiting...");
                goto exit;
        }

        zero_stats(info);
        info->evlog.clear();

        clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	// These should be spanned elsewhere
	if (! umd_dma_goodput_tun_setup_peer(info, info->did, info->rio_addr, info->ib_ptr)) goto exit;

again:
        while (!info->stop_req) {
		usleep(10 * 1000); // Have anything better to do?
        }

	if (info->stop_req == SOFT_RESTART) {
		INFO("\n\tSoft restart requested, nuking MBOX hardware!\n");
		//info->umd_dch->softRestart(); // XXX which one?!?!?
		info->stop_req = 0;
		sem_post(&info->umd_fifo_proc_started);
		sem_post(&info->umd_dma_tap_proc_started);
		goto again;
	}

exit:
	write(info->umd_sockp[0], "X", 1); // Signal Tun/Tap RX thread to eXit

        info->umd_fifo_proc_must_die = 1;

        pthread_join(info->umd_fifo_thr.thr, NULL);
        pthread_join(info->umd_dma_tap_thr.thr, NULL);


        info->umd_dma_did_peer[info->did] = NULL;
        info->umd_dma_did_peer.erase(info->did);

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (info->umd_dch_list[ch] == NULL) continue;
		DmaChannelInfo_t* dci = info->umd_dch_list[ch];

		for (int i = 0; i < dci->tx_buf_cnt; i++) {
			if(dci->dmamem[i].type == 0) continue;
			dci->dch->free_dmamem(info->dmamem[i]);
		}
	}

	info->umd_dma_fifo_callback = NULL;

        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	{{
	  std::map <uint16_t, DmaPeerDestid_t*>::iterator itp = info->umd_dma_did_peer.begin();
	  for (; itp != info->umd_dma_did_peer.end(); itp++) {
		DmaPeerDestid_t* peer = itp->second;
		assert(peer);

		{{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
		  struct epoll_event event;
		  event.data.fd = peer->tun_fd;
		  event.events = EPOLLIN;
		  epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
		}}

		info->umd_dma_did_peer_fd2did.erase(peer->tun_fd);

		DmaPeerDestidDestroy(peer); free(peer);
	  }
	  info->umd_dma_did_peer.clear();
	  info->umd_dma_did_peer_fd2did.clear();
	}}
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

        close(info->umd_sockp[0]); close(info->umd_sockp[1]);
        info->umd_sockp[0] = info->umd_sockp[1] = -1;
	close(info->umd_epollfd); info->umd_epollfd = -1;

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (info->umd_dch_list[ch] == NULL) continue;
		delete info->umd_dch_list[ch]->dch;
		free(info->umd_dch_list[ch]); info->umd_dch_list[ch] = NULL;
	}
        delete info->umd_dch2; info->umd_dch2 = NULL;
        delete info->umd_lock; info->umd_lock = NULL;
        info->umd_tun_name[0] = '\0';

	good_destid.clear();
	bad_destid.clear();
}

void umd_dma_goodput_tun_add_ep(struct worker* info, const uint32_t destid)
{
}

void umd_dma_goodput_tun_del_ep(struct worker* info, const uint32_t destid)
{
}

#ifdef UDMA_TUN_DEBUG_IN
static void decodeInotifyEvent(const struct inotify_event* i, std::string& str)
{
     if (i == NULL) return;

     std::stringstream ss;
     ss << "wd = " << i->wd << " ";
     if (i->cookie > 0)
         ss << "cookie = " << i->cookie;
 
     ss << "mask = ";
     if (i->mask & IN_ACCESS)        ss << " IN_ACCESS ";
     if (i->mask & IN_ATTRIB)        ss << " IN_ATTRIB ";
     if (i->mask & IN_CLOSE_NOWRITE) ss << " IN_CLOSE_NOWRITE ";
     if (i->mask & IN_CLOSE_WRITE)   ss << " IN_CLOSE_WRITE ";
     if (i->mask & IN_CREATE)        ss << " IN_CREATE ";
     if (i->mask & IN_DELETE)        ss << " IN_DELETE ";
     if (i->mask & IN_DELETE_SELF)   ss << " IN_DELETE_SELF ";
     if (i->mask & IN_IGNORED)       ss << " IN_IGNORED ";
     if (i->mask & IN_ISDIR)         ss << " IN_ISDIR ";
     if (i->mask & IN_MODIFY)        ss << " IN_MODIFY ";
     if (i->mask & IN_MOVE_SELF)     ss << " IN_MOVE_SELF ";
     if (i->mask & IN_MOVED_FROM)    ss << " IN_MOVED_FROM ";
     if (i->mask & IN_MOVED_TO)      ss << " IN_MOVED_TO ";
     if (i->mask & IN_OPEN)          ss << " IN_OPEN ";
     if (i->mask & IN_Q_OVERFLOW)    ss << " IN_Q_OVERFLOW ";
     if (i->mask & IN_UNMOUNT)       ss << " IN_UNMOUNT ";
 
     if (i->len > 0)
         ss << " name = " << i->name;

     str = ss.str();
}
#endif // UDMA_TUN_DEBUG_IN

static inline bool umd_check_dma_tun_thr_running(struct worker* info)
{
	if (info == NULL) return false;

	const int tundmathreadindex = info->umd_chan; // FUDGE
	if (tundmathreadindex < 0) return false;

	if (1 != wkr[tundmathreadindex].stat) return false;

	return true;
}

extern "C"
void umd_epwatch_demo(struct worker* info)
{
	if (info == NULL) return;

	const int tundmathreadindex = info->umd_chan; // FUDGE
	if (tundmathreadindex < 0) return;

	const char* SYS_RAPIDIO_DEVICES = "/sys/bus/rapidio/devices";

	int inf_fd = inotify_init1(IN_CLOEXEC);

	inotify_add_watch(inf_fd, SYS_RAPIDIO_DEVICES, IN_CREATE|IN_DELETE);

	bool first_time = true;
	std::map <uint32_t, bool> ep_map;

	if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

	INFO("\n\tWatching %s for change. Fetching RIO EPs from mport %d\n", SYS_RAPIDIO_DEVICES, info->mp_num);

	while (! info->stop_req) {
		bool inotify_change = false;

		for (int i = 0; i < 10 && !info->stop_req; i++) {
		 	uint32_t ep_count = 0;
			uint32_t* ep_list = NULL;

			if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

			if (i != 0) {{ // We use select(2) as a fancy sleep and looking at what FMD is doing to /sys/bus/rapidio/devices
			  struct timeval to = { 0, 100 * 1000 }; // 100 ms
			  fd_set rfds; FD_ZERO (&rfds); FD_SET(inf_fd, &rfds);

			  int ret = select(inf_fd+1, &rfds, NULL, NULL, &to);
			  if (ret < 0) {
				CRIT("\n\tselect failed on inotify fd: %s\n", strerror(errno));
				goto exit;
			  }

			  if (FD_ISSET(inf_fd, &rfds)) { // consume inotify event but we use the riomp_mgmt_get_ep_list method to learn ep changes
				uint8_t buf[8192] = {0};;
				int nread = read(inf_fd, buf, 8192); nread += 0;
				inotify_change = true;
#ifdef UDMA_TUN_DEBUG_IN
				const int N = nread / sizeof(struct inotify_event);
				struct inotify_event* in = (struct inotify_event*)buf;
				for (int i = 0; 7 <= g_level && i < N; i++, in++) {
					std::string s;
					decodeInotifyEvent(in, s);
					DBG("\n\tInotify event %d: %s\n", i, s.c_str());
				}
#endif
			  }
			}}

			if (info->stop_req) goto exit;
			if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

			if (i == 0 || inotify_change) { // We force reading EP list once a second
				inotify_change = false;
				if (0 != riomp_mgmt_get_ep_list(info->mp_num, &ep_list, &ep_count) || ep_list == NULL) {
					CRIT("\n\triomp_mgmt_get_ep_list failed. All your base are belong to us.\n");
					goto exit;
				}

				if (first_time && 7 <= g_level && ep_count > 0) {
					std::stringstream ss;
					for (int i = 0; i < ep_count; i++) ss << ep_list[i] << " ";
					DBG("\n\tMport %d EPs%s: %s\n", info->mp_num, (first_time? "(1st time)": ""), ss.str().c_str());
				}

				if (first_time) {
					first_time = false;
					for (int i = 0; i < ep_count; i++) {
						const uint32_t destid = ep_list[i];
						ep_map[destid] = true;
						umd_dma_goodput_tun_add_ep(&wkr[tundmathreadindex], destid);
					}
				} else {
					std::map <uint32_t, bool> ep_map_now;
					for (int i = 0; i < ep_count; i++) {
						const uint32_t destid = ep_list[i];
						ep_map_now[destid] = true;
						if (ep_map.find(destid) == ep_map.end()) {
							ep_map[destid] = true;
							DBG("\n\tMport %d EP %u ADD\n", info->mp_num, destid);
							umd_dma_goodput_tun_add_ep(&wkr[tundmathreadindex], destid);
						}
					}
					// Set diff -- find out which destids have gone
					for (std::map <uint32_t, bool>::iterator itm = ep_map.begin(); itm != ep_map.end(); itm++) {
						const uint32_t destid = itm->first;
						if (ep_map_now.find(destid) == ep_map_now.end()) {
							ep_map.erase(destid);
							DBG("\n\tMport %d EP %u DEL\n", info->mp_num, destid);
							umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], destid);
						}
					}
				}
				riomp_mgmt_free_ep_list(&ep_list);
			}
		}
	}
exit:
	close(inf_fd);
	return;

exit_bomb:
	CRIT("\n\tWorker thread %d (DMA Tun Thr) is not running. Bye!\n", tundmathreadindex);
	goto exit;
}
