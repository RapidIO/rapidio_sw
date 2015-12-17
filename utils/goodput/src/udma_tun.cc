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
#include "liblog.h"

#include "time_utils.h"
#include "worker.h"
#include "goodput.h"
#include "mhz.h"

#include "dmachan.h"
#include "hash.cc"
#include "lockfile.h"
#include "tun_ipv4.h"

extern "C" {
	void zero_stats(struct worker *info);
	int migrate_thread_to_cpu(struct thread_cpu *info);
	bool umd_check_cpu_allocation(struct worker *info);
	bool TakeLock(struct worker* info, const char* module, int instance);
	uint32_t crc32(uint32_t crc, const void *buf, size_t size);
};

#define PAGE_4K    4096

#define DMA_CHAN2_BUFC  0x20
#define DMA_CHAN2_STS   0x20

#define MBOX_BUFC  0x20
#define MBOX_STS   0x20

void umd_dma_goodput_tun_del_ep(struct worker* info, const uint32_t destid, bool signal);
void umd_dma_goodput_tun_del_ep_signal(struct worker* info, const uint32_t destid);
static bool inline umd_dma_tun_process_tun_RX(struct worker *info, DmaChannelInfo_t* dci, DmaPeerDestid_t* peer, const uint16_t my_destid);

///< This be fishy! There's no standard for ntonll :(
static inline uint64_t htonll(uint64_t value)
{
     int num = 42;
     if(*(char *)&num == 42)
          return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32LL) | htonl(value >> 32);
     else 
          return value;
}

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

	DMAChannel* dmac = info->umd_dch_nread;

	DMAChannel::DmaOptions_t dmaopt; memset(&dmaopt, 0, sizeof(dmaopt));
	dmaopt.destid      = destid;
	dmaopt.prio        = 2; // We want to get in front all pending ALL_WRITEs in 721 silicon
	dmaopt.bcount      = size;
	dmaopt.raddr.lsb64 = rio_addr;

	struct seq_ts tx_ts;
	uint32_t umd_dma_abort_reason = 0;
	DMAChannel::WorkItem_t wi[DMA_CHAN2_STS*8]; memset(wi, 0, sizeof(wi));

	int q_was_full = !dmac->queueDmaOpT2((int)NREAD, dmaopt, data_out, size, umd_dma_abort_reason, &tx_ts);

	if (!umd_dma_abort_reason) DBG("\n\tPolling FIFO transfer completion destid=%d\n", destid);

	for(int i = 0;
	    !q_was_full && !info->stop_req && (i < 1000) && !dmac->scanFIFO(wi, DMA_CHAN2_STS*8);
	    i++) {
		usleep(1);
	}

	if (umd_dma_abort_reason || (dmac->queueSize() > 0)) { // Boooya!! Peer not responding
		uint32_t RXRSP_BDMA_CNT = 0;
		bool inp_err = false, outp_err = false;
		info->umd_dch_nread->checkPortInOutError(inp_err, outp_err);
		{{
		  RioMport* mport = new RioMport(info->mp_num, info->mp_h);
		  RXRSP_BDMA_CNT = mport->rd32(TSI721_RXRSP_BDMA_CNT); // aka 0x29904 Received Response Count for Block DMA Engine Register
		  delete mport;
		}}

		CRIT("\n\tChan2 %u stalled with %sq_size=%d WP=%lu FIFO.WP=%llu %s%s%s%sRXRSP_BDMA_CNT=%u abort reason 0x%x %s\n",
		      info->umd_chan2,
		      (q_was_full? "QUEUE FULL ": ""), dmac->queueSize(),
                      info->umd_dch_nread->getWP(), info->umd_dch_nread->m_tx_cnt, 
		      (info->umd_dch_nread->checkPortOK()? "Port:OK ": ""),
		      (info->umd_dch_nread->checkPortError()? "Port:ERROR ": ""),
		      (inp_err? "Port:OutpERROR ": ""),
		      (inp_err? "Port:InpERROR ": ""),
		      RXRSP_BDMA_CNT,
		      umd_dma_abort_reason, DMAChannel::abortReasonToStr(umd_dma_abort_reason));

		dmac->softRestart();

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

        info->umd_ticks_total_chan2 += (wi[0].opt.ts_end - wi[0].opt.ts_start);

	memcpy(data_out, wi[0].t2_rddata, size);

	return true;
}

const int DESTID_TRANSLATE = 1;

/** \brief Thread that services Tun TX, sends L3 frames to peer and does RIO TX throttling
 * Update RP via NREAD every 8 buffers; when local q is 2/3 full do it after each send
 */
void* umd_dma_tun_RX_proc_thr(void *parm)
{
	struct epoll_event* events = NULL;

        if (NULL == parm) return NULL;

        struct worker* info = (struct worker *)parm;
        if (NULL == info->umd_dch_list[info->umd_chan]) goto exit;
        if (NULL == info->umd_dch_nread) goto exit;

	if ((events = (struct epoll_event*)calloc(MAX_EPOLL_EVENTS, sizeof(struct epoll_event))) == NULL) goto exit;

#if 0
	info->umd_dma_tap_thr.cpu_req = GetDecParm("$cpu2", -1);

        migrate_thread_to_cpu(&info->umd_dma_tap_thr);

        if (info->umd_dma_tap_thr.cpu_req != info->umd_dma_tap_thr.cpu_run) {
                CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing out!\n",
                     info->umd_dma_tap_thr.cpu_req, info->umd_dma_tap_thr.cpu_run);
                goto exit;
        }

	INFO("\n\tReady to receive from multiplexed Tun devices {isolcpu $cpu2=%d}. MAX_PEERS=%d\n", info->umd_dma_tap_thr.cpu_req, MAX_PEERS);
#else
	INFO("\n\tReady to receive from multiplexed Tun devices. MAX_PEERS=%d\n", MAX_PEERS);
#endif

        {{
	DmaChannelInfo_t* dch_list[6] = {0};

	int dch_cnt = 0;
	int dch_cur = 0;
	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) dch_list[dch_cnt++] = info->umd_dch_list[ch];

	uint64_t tx_cnt = 0;

        info->umd_dma_tap_proc_alive = 1;
        sem_post(&info->umd_dma_tap_proc_started);

        const uint16_t my_destid = info->my_destid;

again:
        while(! info->stop_req) {
		// XXX Maybe pick the one with the least full queue??
		DmaChannelInfo_t* dci = dch_list[dch_cur++];
		if (dch_cur >= dch_cnt) dch_cur = 0; // Wrap-around

	poll_again:
		const int epoll_cnt = epoll_wait (info->umd_epollfd, events, MAX_EPOLL_EVENTS, -1);
		if (epoll_cnt < 0 && errno == EINTR) goto poll_again;
	
		if (epoll_cnt < 0) {
			CRIT("\n\tepoll_wait failed: %s\n", strerror(errno))
			break;
		}

		for (int epi = 0; epi < epoll_cnt; epi++) {
			if (info->stop_req) break;
			if (events[epi].data.fd == info->umd_sockp_quit[1]) {
				INFO("\n\tSocketpair said quit!\n");
				goto exit; // Time to quit!
			}

			// We do NOT use edge-triggered epoll for Tun.
			// We want to process one L3 frame per incoming Tun in round-robin
			// fashion. If more frames are available epoll_wait will not block.

			if ((events[epi].events & EPOLLERR) || (events[epi].events & EPOLLHUP) || (!(events[epi].events & EPOLLIN))) {
				CRIT("\n\tepoll error for data.ptr=%p: %s\n", events[epi].data.ptr, strerror(errno));
				//close (tun_fd); // really? destroy peer as well????
				continue;
			}

			for (int cnt = 0; cnt < 4; cnt++) { // Maximum per-Tun
				if (umd_dma_tun_process_tun_RX(info, dci, (DmaPeerDestid_t*)events[epi].data.ptr, my_destid)) tx_cnt++;
				else break; // Read all I could from Tun fd
			}
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

        return parm;
} // END umd_dma_tun_RX_proc_thr

/** \brief Process data from Tun and send it over DMA NWRITE
 */
static bool inline umd_dma_tun_process_tun_RX(struct worker *info, DmaChannelInfo_t* dci, DmaPeerDestid_t* peer, const uint16_t my_destid)
{
	if (info == NULL || dci == NULL || peer == NULL || my_destid == 0xffff) return false;

	bool ret = false;
	int destid_dpi = -1;
	int is_bad_destid = 0;
	bool first_message;
	int outstanding = 0; // This is our guess of what's not consumed at other end, per-destid

        const int Q_THR = (8 * (info->umd_tx_buf_cnt-1)) / 10;
	
	DMAChannel::DmaOptions_t& dmaopt = dci->dmaopt[dci->oi];

	uint8_t* buffer = (uint8_t*)dci->dmamem[dci->oi].win_ptr;

	const int nread = read(peer->tun_fd, buffer+DMA_L2_SIZE, info->umd_tun_MTU);
	const uint64_t now = rdtsc();

	if (nread < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false;

	assert(nread > 0);

	DMA_L2_t* pL2 = (DMA_L2_t*)buffer;

	{{
	uint32_t* pkt = (uint32_t*)(buffer+DMA_L2_SIZE);
	const uint32_t dest_ip_v4 = ntohl(pkt[4]); // XXX IPv6 will stink big here

	destid_dpi = (dest_ip_v4 & 0xFFFF) - DESTID_TRANSLATE;
	}}

	is_bad_destid += (destid_dpi == info->my_destid);

	//DmaPeerLock(peer);

	if (info->stop_req || peer->stop_req) goto unlock;

#ifdef UDMA_TUN_DEBUG
	{{
	const uint32_t crc = crc32(0, buffer+DMA_L2_SIZE, nread);
	DBG("\n\tGot from tun? %d+%d bytes (L7 CRC32 0x%x) to RIO destid %u%s\n",
		 nread, DMA_L2_SIZE,
		 crc, destid_dpi,
		 is_bad_destid? " BLACKLISTED": "");
	}};
#endif

	peer->tun_rx_cnt++;

	if (is_bad_destid) {
		ERR("\n\tBad destid %u -- score=%d\n", destid_dpi, is_bad_destid);
		send_icmp_host_unreachable(peer->tun_fd, buffer+DMA_L2_SIZE, nread);
		goto error;
	}

	first_message = peer->tx_cnt == 0;

	do {{
	  if (peer->WP == peer->RP) { break; }
	  if (peer->WP > peer->RP)  { outstanding = peer->WP-peer->RP; break; }
	  //if (WP == (info->umd_tx_buf_cnt-2)) { outstanding = RP; break; }
	  outstanding = peer->WP + dci->tx_buf_cnt-2 - peer->RP;
	}} while(0);

	DBG("\n\tWP=%d guessed { RP=%d outstanding=%d } %s\n", peer->WP, peer->RP, outstanding, (outstanding==(dci->tx_buf_cnt-1))? "FULL": "");

	// We force reading RP from a "new" destid as a RIO ping as
	// NWRITE does not barf  on bad destids

	if (first_message || /*(peer->tx_cnt % Q_HLF) == 0 ||*/ outstanding >= Q_THR || dci->dch->queueSize() > Q_THR) { // This must be done per-destid
		uint32_t newRP = ~0;
		if (udma_nread_mem(info, destid_dpi, peer->rio_addr, sizeof(newRP), (uint8_t*)&newRP)) {
			DBG("\n\tPulled RP from destid %u old RP=%d actual RP=%d\n", destid_dpi, peer->RP, newRP);
			peer->RP = newRP;
			if (first_message) peer->WP = newRP; // XXX maybe newRP+1 ?? Test
		} else {
			send_icmp_host_unreachable(peer->tun_fd, buffer+DMA_L2_SIZE, nread); // XXX which tun fd??
			DBG("\n\tHW error, something is FOOBAR with Chan %u\n", info->umd_chan2);

			peer->stop_req = 1;

			umd_dma_goodput_tun_del_ep(info, destid_dpi, false); // Nuke Tun & Peer

			goto error;
		}
	}

	if (info->stop_req || peer->stop_req) goto error;

	if (outstanding == (dci->tx_buf_cnt-1)) {
		CRIT("\n\tPeer destid=%u is FULL, dropping frame!\n", destid_dpi);
		peer->rio_tx_peer_full++;
		// XXX Maybe send back ICMP Host Unreachable? TODO: Study RFCs
		goto error;
	}

	pL2->destid = htons(my_destid);
	pL2->len    = htonl(DMA_L2_SIZE + nread);
	pL2->RO     = 1;

	if (info->stop_req) goto unlock;

	// Barry dixit "If full sleep for (queue size / 2) nanoseconds"
	for (int i = 0; i < (dci->tx_buf_cnt-1)/2; i++) {
		if (! dci->dch->queueFull()) break;
		struct timespec tv = { 0, 1};
		nanosleep(&tv, NULL);
	}

	if (dci->dch->queueFull()) {
		DBG("\n\tQueue full #1 on chan %d!\n", dci->chan);
		goto error; // Drop L3 frame
	}

	if (info->stop_req) goto unlock;

	dmaopt.destid      = destid_dpi;
	dmaopt.bcount      = ntohl(pL2->len);
	dmaopt.raddr.lsb64 = peer->rio_addr + sizeof(uint32_t) + peer->WP * BD_PAYLOAD_SIZE(info);
	dmaopt.u_data      = now;

	DBG("\n\tSending to RIO %d+%d bytes to RIO destid %u addr 0x%llx WP=%d chan=%d oi=%d\n",
	    nread, DMA_L2_SIZE, destid_dpi, dmaopt.raddr.lsb64, peer->WP, dci->chan, dci->oi);

	dci->dch->setCheckHwReg(first_message);

	info->umd_dma_abort_reason = 0;
	if (! dci->dch->queueDmaOpT1(ALL_NWRITE, dci->dmaopt[dci->oi], dci->dmamem[dci->oi],
					info->umd_dma_abort_reason, &info->meas_ts)) {
		if(info->umd_dma_abort_reason != 0) { // HW error
			// ICMPv4 dest unreachable id bad destid 
			send_icmp_host_unreachable(peer->tun_fd, buffer+DMA_L2_SIZE, nread); // XXX which tun
			DBG("\n\tHW error, triggering soft restart\n");

			peer->stop_req = 1;

			umd_dma_goodput_tun_del_ep(info, destid_dpi, false); // Nuke Tun & Peer

			info->stop_req = SOFT_RESTART; // XXX of which channel?
			goto error;
		} else { // queue really full
			DBG("\n\tQueue full #2 on chan %d!\n", dci->chan);
			goto error; // Drop L3 frame
		}
	}

	dci->oi++; if (dci->oi == (dci->tx_buf_cnt-1)) dci->oi = 0; // Account for T3

	// For just one destdid WP==oi but we keep them separate

	peer->WP++; // This must be done per-destid
	if (peer->WP == (dci->tx_buf_cnt-1)) peer->WP = 0; // Account for T3 missing IBwin cell, ALL must have the same bufc!

	peer->tx_cnt++;
	ret = true;

unlock:
	// if (peer != NULL) DmaPeerUnLock(peer);
	return ret;

error:  goto unlock;
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

	for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
		DmaPeerDestid_t* peer = info->umd_dma_did_peer_list[epi];
		if (peer == NULL) continue; // Just vacated one slot?

		if (peer->sig == PEER_SIG_INIT) continue; // Not UP yet

		assert(peer->sig == PEER_SIG_UP);

		const uint16_t destid = peer->destid;

		// XXX Do I need this paranoia check in production?
		if (info->umd_dma_did_enum_list.find(destid) == info->umd_dma_did_enum_list.end()) {
			CRIT("\n\tBUG Peer for destid %u exists in only one map!\n", destid);
			break; // Better luck next time we're called
		}

		bool pending_work = false;

		int k = ~0;
		int cnt = 0;
		int idx = ~0;
		uint64_t now = 0;
		volatile uint32_t* pRP = NULL;

		//DmaPeerLock(peer);

		if (peer->stop_req) goto unlock;

		pRP = (uint32_t*)peer->ib_ptr;

		k = *pRP;
		assert(k >= 0);
		assert(k < (info->umd_tx_buf_cnt-1));

		now = rdtsc();
		if (peer->rio_rx_bd_ready_size >= (info->umd_tx_buf_cnt-1)) { // Quick peek while unlocked -- Receiver too slow, go to next peer!
			if (peer->rio_rx_peer_full_ts == 0) peer->rio_rx_peer_full_ts = now;
			continue;
		}
		if (peer->rio_rx_peer_full_ts > 0 && now > peer->rio_rx_peer_full_ts) { // We just drained somw of IBwin window for this peer
			peer->rio_rx_peer_full_ticks_total += now - peer->rio_rx_peer_full_ts;
		}
		peer->rio_rx_peer_full_ts = 0;

 		pthread_spin_lock(&peer->rio_rx_bd_ready_splock);

		idx = peer->rio_rx_bd_ready_size; // If not zero then RIO RX thr is sloow
		assert(idx >= 0);

		for (int i = 0; i < (info->umd_tx_buf_cnt-1); i++) {
			if (info->stop_req) break;
			if (peer->stop_req) continue;

			if (idx >= (info->umd_tx_buf_cnt-1)) break;

			if(0 != peer->rio_rx_bd_L2_ptr[k]->RO) {
				const uint64_t now = rdtsc();
#ifdef UDMA_TUN_DEBUG_IB
				DBG("\n\tFound ready buffer at RP=%u -- iter %llu\n", k, cbk_iter);
#endif

				assert(k < (info->umd_tx_buf_cnt-1));
				assert(idx < (info->umd_tx_buf_cnt-1));

				peer->rio_rx_bd_ready[idx] = k;
				peer->rio_rx_bd_ready_ts[idx] = now;
				peer->rio_rx_bd_L2_ptr[k]->RO = 0; // So we won't revisit
				idx++;
				cnt++;
			} else {
				break; // Stop at 1st non-ready IB bd??
			}

			k++; if(k == (info->umd_tx_buf_cnt-1)) k = 0; // RP wrap-around
		}

		if (cnt > 0) {
			assert(cnt <= (info->umd_tx_buf_cnt-1)); // XXX Cannot exceed that!
			peer->rio_rx_bd_ready_size += cnt;
			assert(peer->rio_rx_bd_ready_size <= (info->umd_tx_buf_cnt-1));
		}

		pthread_spin_unlock(&peer->rio_rx_bd_ready_splock);

		if (cnt == 0) goto unlock;

		pending_work = true;

#ifdef UDMA_TUN_DEBUG_IB
		DBG("\n\tFound %d ready buffers at iter %llu\n", cnt, cbk_iter);
#endif

		if (pending_work) sem_post(&peer->rio_rx_work);

	unlock:
		//DmaPeerUnLock(peer);
		if (info->stop_req) break;
	} // END for each peer

	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
}

/** \brief This helper thread wakes up the Peer thread(s) and the main thread at quitting time */
void* umd_dma_wakeup_proc_thr(void* arg)
{
	if(arg == NULL) return NULL;
	struct worker* info = (struct worker*)arg;

        while (!info->stop_req) // every 1 mS which is about HZ/10. stupid
		usleep(1000);
	
	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	do {{
	  if (info->umd_dma_did_peer_list_high_wm == 0) break;

          for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
                DmaPeerDestid_t* peer = info->umd_dma_did_peer_list[epi];
                if (peer == NULL) continue; // Just vacated one slot?

		assert(peer->sig == PEER_SIG_UP);

                //DmaPeerLock(peer);
                peer->stop_req = 1;
		sem_post(&peer->rio_rx_work);
		//DmaPeerUnLock(peer);
	  }
	}} while(0);
	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	return NULL;
}

/** \brief Setup the TX/NREAD DMA channel
 * \note This channel will do only one operation at a time
 * \note It will get only the minimum number of BDs \ref DMA_CHAN2_BUFC
 */
bool umd_dma_goodput_tun_setup_chan2(struct worker *info)
{
	if (info == NULL) return false;

        info->umd_dch_nread = new DMAChannel(info->mp_num, info->umd_chan2, info->mp_h);
        if (NULL == info->umd_dch_nread) {
                CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan2, info->mp_num, info->mp_h);
                return false;
        }

        // TX - Chan 2
        info->umd_dch_nread->setCheckHwReg(true);
        if (!info->umd_dch_nread->alloc_dmatxdesc(DMA_CHAN2_BUFC)) {
                CRIT("\n\talloc_dmatxdesc failed: bufs %d", DMA_CHAN2_BUFC);
                return false;
        }
        if (!info->umd_dch_nread->alloc_dmacompldesc(DMA_CHAN2_STS)) {
                CRIT("\n\talloc_dmacompldesc failed: entries %d", DMA_CHAN2_STS);
                return false;
        }

        info->umd_dch_nread->resetHw();
        if (!info->umd_dch_nread->checkPortOK()) {
                CRIT("\n\tPort %d is not OK!!! Exiting...", info->umd_chan2);
                return false;
        }

	return true;
}

/** \brief Setup a TX/NWRITE DMA channel
 */
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

/** \brief Set up a Tun for a Peer
 */
bool umd_dma_goodput_tun_setup_TUN(struct worker *info, DmaPeerDestid_t* peer, uint16_t my_destid)
{
	bool ret = false;
        char if_name[IFNAMSIZ] = {0};
        char Tap_Ifconfig_Cmd[257] = {0};
        int flags = IFF_TUN | IFF_NO_PI;

	if (info == NULL || peer == NULL) return false;

	DmaPeerLock(peer);

	if (my_destid == peer->destid) goto error;

        memset(peer->tun_name, 0, sizeof(peer->tun_name));

        // Initialize tun/tap interface
        if ((peer->tun_fd = tun_alloc(if_name, flags)) < 0) {
                CRIT("Error connecting to tun/tap interface %s!\n", if_name);
                goto error;
        }
        strncpy(peer->tun_name, if_name, sizeof(peer->tun_name)-1);

	{{
	  const int flags = fcntl(peer->tun_fd, F_GETFL, 0);
	  fcntl(peer->tun_fd, F_SETFL, flags | O_NONBLOCK);
	}}

        // Configure tun/tap interface for pointo-to-point IPv4, L2, no ARP, no multicast

	{{
          const uint16_t my_destid_tun   = my_destid + DESTID_TRANSLATE;
          const uint16_t peer_destid_tun = peer->destid + DESTID_TRANSLATE;

          snprintf(Tap_Ifconfig_Cmd, 256, "169.254.%d.%d pointopoint 169.254.%d.%d",
                 (my_destid_tun >> 8) & 0xFF,   my_destid_tun & 0xFF,
                 (peer_destid_tun >> 8) & 0xFF, peer_destid_tun & 0xFF);

          char ifconfig_cmd[257] = {0};
          snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s %s mtu %d up",
                                    if_name, Tap_Ifconfig_Cmd, info->umd_tun_MTU);
          const int rr = system(ifconfig_cmd);
          if(rr >> 8) {
                peer->tun_name[0] = '\0';
		// No need to remove from epoll set, close does that as it isn't dup(2)'ed
                close(peer->tun_fd); peer->tun_fd = -1;
                goto error;
          }

          snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
          system(ifconfig_cmd);
	}}

	peer->sig = PEER_SIG_UP;

	{{
	  struct epoll_event event;
	  event.data.ptr = peer;
	  event.events = EPOLLIN; // | EPOLLET;
          if (epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, peer->tun_fd, &event) < 0) {
		CRIT("\n\tFailed to add tun_fd %d for peer destid %u to epoll set %d\n",
		     peer->tun_fd, peer->destid, info->umd_epollfd);
                close(peer->tun_fd); peer->tun_fd = -1;
		goto error;
	  }
	}}

	ret = true;

unlock:
	DmaPeerUnLock(peer);

	if (ret) {
		INFO("\n\t%s %s mtu %d on DMA Chan=%d,...,Chan_n=%d Chan2=%d my_destid=%u peer_destid=%u #buf=%d #fifo=%d\n",
		     if_name, Tap_Ifconfig_Cmd, info->umd_tun_MTU,
		     info->umd_chan, info->umd_chan_n, info->umd_chan2,
		     my_destid, peer->destid,
		     info->umd_tx_buf_cnt, info->umd_sts_entries);
	}
	
	return ret;

error:  goto unlock;
}

/** \brief This waits for \ref umd_dma_goodput_tun_callback to signal that IBwin buffers are ready for this peer. Then stuffs L3 frames into Tun
 */
void* umd_dma_tun_TX_proc_thr(void* arg)
{
	if (arg == NULL) return NULL;

	DmaPeerDestid_t* peer = (DmaPeerDestid_t*)arg;
	assert(peer->sig == PEER_SIG_UP);

	struct worker* info = peer->info;

	uint64_t rx_ok = 0; ///< TX'ed into Tun device

	volatile uint32_t* pRP = (uint32_t*)peer->ib_ptr;

	struct thread_cpu myself; memset(&myself, 0, sizeof(myself));

	myself.thr = pthread_self();
        myself.cpu_req = GetDecParm("$cpu2", -1);

        migrate_thread_to_cpu(&myself);

        if (myself.cpu_req != myself.cpu_run) {
                CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing out!\n",
                     myself.cpu_req, myself.cpu_run);
                goto exit;
        }

	INFO("\n\tReady to receive from RIO from destid %u {isolcpu $cpu2=%d}\n", peer->destid, myself.cpu_req);

again: // Receiver (from RIO), TUN TX: Ingest L3 frames into Tun (zero-copy), update RP, set RO=0
	while (!info->stop_req && !peer->stop_req) {
		if (peer->rio_rx_bd_ready_size == 0) // Unlocked op, we take a sneak peek at volatile counter
			sem_wait(&peer->rio_rx_work); // To BEW: how does this impact RX latecy?

        	if (info->stop_req || peer->stop_req) goto stop_req;
		assert(peer->sig == PEER_SIG_UP);

		DBG("\n\tInbound %d buffers(s) ready RP=%u\n", peer->rio_rx_bd_ready_size, *pRP);

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

#ifdef UDMA_TUN_DEBUG_IB
		DBG("\n\tInbound %d buffers(s) will be processed from destid %u RP=%u\n", cnt, peer->destid, *pRP);
#endif

        	if (info->stop_req || peer->stop_req) goto stop_req;
		assert(peer->sig == PEER_SIG_UP);

		for (int i = 0; i < cnt && !info->stop_req; i++) {
			int rp = ready_bd_list[i];
			assert(rp >= 0);
			assert(rp < (info->umd_tx_buf_cnt-1));

			if (info->stop_req || peer->stop_req) goto stop_req;
			assert(peer->sig == PEER_SIG_UP);

			DMA_L2_t* pL2 = peer->rio_rx_bd_L2_ptr[rp];

			rx_ok++;
			const int payload_size = ntohl(pL2->len) - DMA_L2_SIZE;

			assert(payload_size > 0);
			assert(payload_size <= info->umd_tun_MTU);

			peer->rx_cnt++;

			uint8_t* payload = (uint8_t*)pL2 + DMA_L2_SIZE;
			uint64_t tx_ts = rdtsc();
                        const int nwrite = cwrite(peer->tun_fd, payload, payload_size);
#ifdef UDMA_TUN_DEBUG_IB
                        const uint32_t crc = crc32(0, payload, payload_size);
                        DBG("\n\tGot a msg of size %d from RIO destid %u (L7 CRC32 0x%x) cnt=%llu, wrote %d to %s -- rp=%d\n",
                                 ntohl(pL2->len), ntohs(pL2->destid), crc, rx_ok, nwrite, peer->tun_name, rp);
#endif

			if (info->stop_req || peer->stop_req) goto stop_req;
			assert(peer->sig == PEER_SIG_UP);

			if (nwrite == payload_size) {
			     peer->tun_tx_cnt++;
			     if (tx_ts > peer->rio_rx_bd_ready_ts[i]) peer->total_ticks_rx += tx_ts - peer->rio_rx_bd_ready_ts[i];
			} else peer->tun_tx_err++;

			peer->rio_rx_bd_ready_ts[i] = 0;

			rp++; if (rp == (info->umd_tx_buf_cnt-1)) rp = 0;
#ifdef UDMA_TUN_DEBUG_IB
			DBG("\n\tUpdating old RP %d to %d\n", *pRP, rp);
#endif
			*pRP = rp;
		}
        } // END while NOT stop requested

stop_req:
	if (info->stop_req == SOFT_RESTART && !peer->stop_req) {
                DBG("\n\tSoft restart requested, sleeping in a 10uS loop\n");
		while(info->stop_req != 0) usleep(10);
                DBG("\n\tAwakened after Soft restart!\n");
		goto again;
	}

	// XXX clean up this dead peer

exit:
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
	int slot = -1;
	pthread_t tun_TX_thr;
	bool peer_list_full = false;

	if (info == NULL || rio_addr == 0) return false;

        DmaPeerDestid_t* tmppeer = (DmaPeerDestid_t*)calloc(1, sizeof(DmaPeerDestid_t));
        if (tmppeer == NULL) return false;

	tmppeer->sig       = PEER_SIG_INIT;
	tmppeer->destid    = destid;
	tmppeer->rio_addr  = rio_addr;
	//tmppeer->ib_ptr  = ib_ptr; // Done in DmaPeerDestidInit

        // Set up array of pointers to IB L2 headers
        if (! DmaPeerDestidInit(info, tmppeer, ib_ptr)) goto exit;

        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	  for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
                if (info->umd_dma_did_peer_list[epi] != NULL) continue; // No a vacant slot?
		slot = epi;
		break;
	  }
	  if (slot != -1) {
		info->umd_dma_did_peer_list[slot] = tmppeer;
		info->umd_dma_did_peer[destid] = slot;
	  } else if (info->umd_dma_did_peer_list_high_wm < MAX_PEERS) {
		slot = info->umd_dma_did_peer_list_high_wm;
		info->umd_dma_did_peer_list[slot] = tmppeer;
		info->umd_dma_did_peer[destid] = slot;
		info->umd_dma_did_peer_list_high_wm++;
	  } else {
		peer_list_full = true;
	  }
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
        
	if (peer_list_full) {
		CRIT("\n\tPeer list is full -- no room for NEW destid %u\n", destid);
		goto exit;
	}

        if (! umd_dma_goodput_tun_setup_TUN(info, tmppeer, info->my_destid)) goto exit;

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
	  if (slot != -1) info->umd_dma_did_peer_list[slot] = NULL;;
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	DmaPeerDestidDestroy(tmppeer); free(tmppeer);

	return false;
}

/** \brief Thread to service FIFOs for all TX/NWRITE DMA channels
 * \note This must be running on an isolcpu core.
 * \note It has a callback for other code to share isolcpu resource.
 */
void* umd_dma_tun_fifo_proc_thr(void* parm)
{
        struct worker* info = NULL;

        int dch_cnt = 0;
	DmaChannelInfo_t* dch_list[6] = {0};

        if (NULL == parm) goto exit;

        info = (struct worker *)parm;

        if (NULL == info->umd_dch_list[info->umd_chan]) goto exit;
        if (NULL == info->umd_dch_list[info->umd_chan_n]) goto exit;

        for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) dch_list[dch_cnt++] = info->umd_dch_list[ch];
	
	assert(dch_cnt);

        DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

        migrate_thread_to_cpu(&info->umd_fifo_thr);

        if (info->umd_fifo_thr.cpu_req != info->umd_fifo_thr.cpu_run) {
                CRIT("\n\tRequested CPU %d does not match migrated cpu %d, bailing out!\n",
                     info->umd_fifo_thr.cpu_req, info->umd_fifo_thr.cpu_run);
                goto exit;
        }

	INFO("\n\tFIFO Scanner thread ready, optimised for %s\n", info->umd_tun_thruput? "thruput": "latency");

        info->umd_fifo_proc_alive = 1;
        sem_post(&info->umd_fifo_proc_started);

        while (!info->umd_fifo_proc_must_die) {
		get_seq_ts(&info->fifo_ts);
		for (int ch = 0; ch < dch_cnt; ch++) {
			// This is a hook to do stuff for IB buffers in isolcpu thread
			// Note: No relation to TX FIFO/buffers, just CPU sharing
			if (info->umd_dma_fifo_callback != NULL)
				info->umd_dma_fifo_callback(info);

			const int cnt = dch_list[ch]->dch->scanFIFO(wi, info->umd_sts_entries*8);
			if (!cnt) {
				if (info->umd_tun_thruput) {
					struct timespec tv = { 0, 1 };
					nanosleep(&tv, NULL);
				}
				continue;
			}

			for (int i = 0; i < cnt; i++) {
				DMAChannel::WorkItem_t& item = wi[i];

				switch (item.opt.dtype) {
				case DTYPE1:
				case DTYPE2:
					if (item.opt.ts_end > item.opt.ts_start) {
						dch_list[ch]->ticks_total += (item.opt.ts_end - item.opt.ts_start);	
					}
					// These are from read from Tun
					if (item.opt.u_data != 0 && item.opt.ts_end > item.opt.u_data) {
						dch_list[ch]->total_ticks_tx += item.opt.ts_end - item.opt.u_data;
					}
					break;
				case DTYPE3:
					break;
				default:
					ERR("\n\tUNKNOWN BD %d bd_wp=%u\n",
						item.opt.dtype, item.opt.bd_wp);
					break;
				}

				wi[i].valid = 0xdeadabba;
			} // END for WorkItem_t vector
		} // END for each channel
                clock_gettime(CLOCK_MONOTONIC, &info->end_time);
        } // END while
        goto no_post;
exit:
        sem_post(&info->umd_fifo_proc_started);
no_post:
        info->umd_fifo_proc_alive = 0;

        return parm;
}

/* \brief Interact with other peers and set up tun devices as appropriate
 * This code evolved to be RDMAD-redux
 * It is fed by MboxWatch via socketpairs
 * and EpWatch via a (polled) list of enumerated EPs
 * \note MboxWatch may or may not be running; we do not barf if the socketpair is/becomes -1!
 * \param[in] info this replacement
 * \param min_bcasts Put up Tun device only after hearing min_bcasts from peer
 * \param bcast_interval Interval between broadcasts
 */
void umd_dma_goodput_tun_RDMAD(struct worker *info, const int min_bcasts, const int bcast_interval)
{
	time_t next_bcast = time(NULL);

        while (!info->stop_req) {
// Broadcast my IBwin mapping Urbi & Orbi, once per second
		time_t now = 0;
		std::vector<uint32_t> peer_list;

		// Collect peers quickly
		pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
		std::map<uint16_t, DmaPeerCommsStats_t>::iterator itp = info->umd_dma_did_enum_list.begin();
		for (; itp != info->umd_dma_did_enum_list.end(); itp++) {
			if (itp->first == info->my_destid) continue;
			peer_list.push_back(itp->first);
		}
		pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

		now = time(NULL);

		if (peer_list.size() == 0 || info->umd_mbox_tx_fd < 0 || now <= next_bcast) goto receive;

		next_bcast = now + bcast_interval;

		if (7 <= g_level) {
			std::stringstream ss;
			for (int ip = 0; ip < peer_list.size(); ip++) ss << peer_list[ip] << " ";
			DBG("\n\tGot %d peers to broadcast to [tx_fd=%d]: %s\n", peer_list.size(), info->umd_mbox_tx_fd, ss.str().c_str());
		}

		for (int ip = 0; ip < peer_list.size(); ip++) {
			const uint16_t destid = peer_list[ip];

			uint64_t rio_addr = 0; // Where I want the peer to deposit its stuff
			void*    ib_ptr   = NULL;
			if (! info->umd_peer_ibmap->lookup(destid, rio_addr, ib_ptr)) {
				CRIT("\n\tCan't find IBwin mapping for destid %u!\n", destid);
				goto exit;
			}

			assert(rio_addr);
                        if (info->umd_mbox_tx_fd < 0) break;

                        uint8_t buf[sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t)] = {0};

			DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;

			DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));
			payload->rio_addr      = htonll(rio_addr);
			payload->base_rio_addr = htonll(info->umd_peer_ibmap->getBaseRioAddr());
			payload->base_size     = htonl(info->umd_peer_ibmap->getBaseSize());
			payload->MTU           = htonl(info->umd_tun_MTU);
			payload->bufc          = htons(info->umd_tx_buf_cnt-1);
			payload->action        = ACTION_ADD;

			pL2->destid_src = htons(info->my_destid);
			pL2->destid_dst = htons(destid);
                        pL2->len        = htons(sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t));

			DBG("\n\tSignalling [tx_fd=%d] peer destid %u {base_rio_addr=0x%llx base_size=%lu rio_addr=0x%llx bufc=%u MTU=%lu}\n",
			    info->umd_mbox_tx_fd, destid,
			    info->umd_peer_ibmap->getBaseRioAddr(), info->umd_peer_ibmap->getBaseSize(),
			    rio_addr, info->umd_tx_buf_cnt-1, info->umd_tun_MTU);

                        if (info->umd_mbox_tx_fd < 0) break;

			int nsend = send(info->umd_mbox_tx_fd, buf, sizeof(buf), 0);
			if (nsend > 0) {
				pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
				info->umd_dma_did_enum_list[destid].bcast_cnt_out++;
				info->umd_dma_did_enum_list[destid].my_rio_addr = rio_addr;
				pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
			} else {
				ERR("\n\tSend to MboxWatch thread failed [tx_fd=%d]: %s\n", strerror(errno), info->umd_mbox_tx_fd);
			}
		}

// Receive broadcasts from other peers
	receive:
		if (info->umd_mbox_rx_fd < 0) { // Other thread not ready!
			usleep(10 * 1000);
			continue;
		}

		uint8_t buf[PAGE_4K] = {0};

                struct timeval to = { 0, 100 * 1000 }; // 100 ms
                fd_set rfds; FD_ZERO (&rfds); FD_SET(info->umd_mbox_rx_fd, &rfds);

                int ret = select(info->umd_mbox_rx_fd+1, &rfds, NULL, NULL, &to);
                if (ret < 0) {
                        DBG("\n\tselect failed on umd_mbox_rx_fd: %s\n", strerror(errno));
                        continue; // MboxWatch conked or was stopped. Better luck next time.
                }

                if (info->stop_req) goto exit;

                if (FD_ISSET(info->umd_mbox_rx_fd, &rfds)) do {{ // Sumthin to came over on MBOX, read only 1st dgram
                        const int nread = recv(info->umd_mbox_rx_fd, buf, PAGE_4K, 0);

                        DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
                        assert(nread == ntohs(pL2->len));

			uint16_t from_destid = ntohs(pL2->destid_src);

			if (from_destid == info->my_destid) { // Myself?
				DBG("\n\tGot %d bytes on [rx_fd=%d] from myself!\n", nread, info->umd_mbox_rx_fd);
				break;
			}

			time_t now = time(NULL);
			bool peer_setup = false;
			uint64_t peer_rio_addr = 0;
			pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
			{{
			  if (info->umd_dma_did_enum_list[from_destid].on_time == 0) {
				info->umd_dma_did_enum_list[from_destid].destid = from_destid;
				info->umd_dma_did_enum_list[from_destid].on_time = now;
			  }
			  info->umd_dma_did_enum_list[from_destid].ls_time = now;
			  info->umd_dma_did_enum_list[from_destid].bcast_cnt_in++;

			  std::map<uint16_t, int>::iterator itp = info->umd_dma_did_peer.find(from_destid);
			  if (itp != info->umd_dma_did_peer.end()) {
				peer_setup = true;
				peer_rio_addr = info->umd_dma_did_peer_list[itp->second]->rio_addr;
			  }
			}}
			pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

			// We have an IBwin mapping belonging to from_destid waiting in (buf + sizeof(DMA_MBOX_L2_t))
			// So put up a new peer, Tun and start up a new Tun thread!!

			assert(ntohs(pL2->len) >= (sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t)));

			DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));

			if (payload->action == ACTION_DEL) {
			    if (peer_setup) {
				INFO("\n\tPeer destid %u told us to go away. Nuking peer\n", from_destid);
				umd_dma_goodput_tun_del_ep(info, from_destid, false);
				break; // F*ck g++
			    } else {
				ERR("\n\tPeer destid %u told us to go away but we have no peer for it!\n", from_destid);
				break; // F*ck g++
			    }
			}

			// If rio_addr comes and != what we have for peer then nuke peer as it had restarted!!!!!

			const uint64_t from_rio_addr      = htonll(payload->rio_addr);
			assert(from_rio_addr);

			if (peer_setup) {
				assert(peer_rio_addr);
				if (from_rio_addr != peer_rio_addr) {
			  		INFO("\n\tGot info [rx_fd=%d] for STALE destid %u NEW peer.rio_addr=0x%llx changed from stored peer.rio_addr=0x%llx. Nuking peer.\n",
					    info->umd_mbox_rx_fd, from_destid, from_rio_addr, peer_rio_addr);
					umd_dma_goodput_tun_del_ep(info, from_destid, true);
					break;
				}

				break; // Don't put up Tun twice // F*ck g++
			}

			const uint64_t from_base_rio_addr = htonll(payload->base_rio_addr);
			const uint32_t from_base_size     = ntohl(payload->base_size);
			const uint16_t from_bufc          = ntohs(payload->bufc);
			const uint32_t from_MTU           = ntohl(payload->MTU);

			assert(from_base_rio_addr);
			assert(from_base_size);

			do {{
			  if (from_MTU != info->umd_tun_MTU) {
				INFO("\n\tGot a mismatched MTU %lu from peer destid %u (expecting %lu). Ignoring peer.\n",
				     from_MTU, from_destid, info->umd_tun_MTU);
				break;
			  }
			  if (from_bufc != (info->umd_tx_buf_cnt-1)) {
				INFO("\n\tGot a mismatched bufc %u from peer destid %u (expecting %d). Ignoring peer.\n",
				     from_bufc, from_destid, (info->umd_tx_buf_cnt-1));
				break;
			  }
			  if (from_rio_addr == 0) {
				INFO("\n\tGot a 0 rio_addr from peer destid %u. Ignoring peer.\n", from_destid);
				break;
			  }

			  uint64_t rio_addr    = 0; 
			  void*    peer_ib_ptr = NULL;
			  if (! info->umd_peer_ibmap->alloc(from_destid, rio_addr, peer_ib_ptr)) {
				CRIT("\n\tWARNING: Can't alloc IBwin mapping for NEW peer destid %u!\n", from_destid);
				break;
			  } 

			  // If EndpointWatch thread is not started bcast_cnt will stay
			  // at -1 and this thread will never attemp to set up a tun for
			  // peer
			  int bcast_cnt = -1;
			  pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
			  {{
			    std::map<uint16_t, DmaPeerCommsStats_t>::iterator itp = info->umd_dma_did_enum_list.find(from_destid);
			    if (itp != info->umd_dma_did_enum_list.end())
			  	bcast_cnt = itp->second.bcast_cnt_in + itp->second.bcast_cnt_out;
			  }}
			  pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

			  DBG("\n\tGot info [rx_fd=%d] for NEW destid %u peer.{base_rio_addr=0x%llx base_size=%lu rio_addr=0x%llx bufc=%u MTU=%lu} allocated rio_addr=%llu ib_ptr=%p bcast_cnt=%d\n",
			      info->umd_mbox_rx_fd,
			      from_destid, from_base_rio_addr, from_base_size, from_rio_addr, from_bufc, from_MTU,
			       rio_addr, peer_ib_ptr, bcast_cnt);
			  
			  if (bcast_cnt < (min_bcasts * 2)) break; // We want to be doubleplus-sure the peer is ready to receive us once we put the Tun up

			  DBG("\n\tSetting up Tun for NEW peer destid %u\n", from_destid);
			  if (! umd_dma_goodput_tun_setup_peer(info, from_destid, from_rio_addr, peer_ib_ptr)) goto exit;
			}} while(0);
		}} while(0); // END if FD_ISSET
        } // END while !info->stop_req

exit:
	return;
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
        bool dma_fifo_proc_thr_started = false;
        bool dma_tap_thr_started = false;
	
	info->owner_func = umd_dma_goodput_tun_demo;

	// Note: There's no reason to link info->umd_tx_buf_cnt other than
	// convenience. However the IB ring should never be smaller than
	// info->umd_tx_buf_cnt-1 members -- (dis)counting T3 BD on TX side
	const int IBWIN_SIZE = sizeof(uint32_t) + BD_PAYLOAD_SIZE(info) * (info->umd_tx_buf_cnt-1);

	assert(info->ib_ptr);
	assert(info->ib_byte_cnt >= IBWIN_SIZE);

	memset(info->ib_ptr, 0, info->ib_byte_cnt);

        if (! umd_check_cpu_allocation(info)) return;
        if (! TakeLock(info, "DMA", info->umd_chan2)) return;
/* XXX FIX THIS
	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (! TakeLock(info, "DMA", ch)) goto exit;
	}
*/

	{{ // Clear on read
	  RioMport* mport = new RioMport(info->mp_num, info->mp_h);
	  mport->rd32(TSI721_RXRSP_BDMA_CNT); // aka 0x29904 Received Response Count for Block DMA Engine Register
	  delete mport;
	}}

	info->umd_dma_fifo_callback = umd_dma_goodput_tun_callback;

	info->umd_peer_ibmap = new IBwinMap(info->ib_rio_addr, info->ib_ptr, info->ib_byte_cnt, (info->umd_tx_buf_cnt-1), info->umd_tun_MTU);

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (!umd_dma_goodput_tun_setup_chanN(info, ch)) goto exit;
	}

	if (!umd_dma_goodput_tun_setup_chan2(info)) goto exit;

	info->my_destid = info->umd_dch_nread->getDestId();

	if (info->my_destid == 0xffff) {
		CRIT("\n\tMy_destid=0x%x which is BORKED -- bad enumeration?\n", info->my_destid);
		goto exit;
	}

	info->umd_epollfd = epoll_create1 (0);
	if (info->umd_epollfd < 0) goto exit;

        socketpair(PF_LOCAL, SOCK_STREAM, 0, info->umd_sockp_quit);

	{{
	  struct epoll_event event;
	  event.data.fd = info->umd_sockp_quit[1];
	  event.events = EPOLLIN | EPOLLET;
          if(epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, info->umd_sockp_quit[1], &event) < 0) goto exit;
	}}

        init_seq_ts(&info->desc_ts);
        init_seq_ts(&info->fifo_ts);
        init_seq_ts(&info->meas_ts);

        info->umd_fifo_proc_must_die = 0;
        info->umd_fifo_proc_alive = 0;

        rc = pthread_create(&info->umd_fifo_thr.thr, NULL,
                            umd_dma_tun_fifo_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_tun_fifo_proc_thr thread, exiting...");
                goto exit;
        }
        sem_wait(&info->umd_fifo_proc_started);

        if (!info->umd_fifo_proc_alive) {
                CRIT("\n\tumd_fifo_proc thread is dead, exiting..");
                goto exit;
        }
	dma_fifo_proc_thr_started = true;

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
	dma_tap_thr_started = true;

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

again:
	umd_dma_goodput_tun_RDMAD(info,
	                          (info->umd_dma_bcast_min = GetDecParm("$bcast_min", 3)) /* min_bcasts */,
				  (info->umd_dma_bcast_interval = GetDecParm("$bcast_interval", 10)) /* bcast_interval, in sec */);

	if (info->stop_req == SOFT_RESTART) {
		INFO("\n\tSoft restart requested, nuking MBOX hardware!\n");
		//info->umd_dch->softRestart(); // XXX which one?!?!?
		info->stop_req = 0;
		sem_post(&info->umd_fifo_proc_started);
		sem_post(&info->umd_dma_tap_proc_started);
		goto again;
	}

exit:
	if (!info->stop_req) info->stop_req = 1; // Maybe we got here on a local error

	write(info->umd_sockp_quit[0], "X", 1); // Signal Tun/Tap RX thread to eXit
        info->umd_fifo_proc_must_die = 1;

	usleep(500 * 1000); // let detached threads quit

        if (dma_fifo_proc_thr_started) pthread_join(info->umd_fifo_thr.thr, NULL);
        if (dma_tap_thr_started) pthread_join(info->umd_dma_tap_thr.thr, NULL);

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (info->umd_dch_list[ch] == NULL) continue;
		DmaChannelInfo_t* dci = info->umd_dch_list[ch];

		for (int i = 0; i < dci->tx_buf_cnt; i++) {
			if(dci->dmamem[i].type == 0) continue;
			dci->dch->free_dmamem(dci->dmamem[i]);
		}
	}

	delete info->umd_peer_ibmap;

	info->umd_dma_fifo_callback = NULL;

        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	  do {{
	    if (info->umd_dma_did_peer.size() == 0) break;

            for (int epi = 0; epi < info->umd_dma_did_peer_list_high_wm; epi++) {
                DmaPeerDestid_t* peer = info->umd_dma_did_peer_list[epi];
                if (peer == NULL) continue; // Just vacated one slot?

		assert(peer->sig == PEER_SIG_UP);

		{{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
		  struct epoll_event event;
		  event.data.fd = peer->tun_fd;
		  event.events = EPOLLIN;
		  epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
		}}

		// Tell peer we stop?
		umd_dma_goodput_tun_del_ep_signal(info, peer->destid);

		peer->stop_req = 1;
		sem_post(&peer->rio_rx_work);

		info->umd_dma_did_peer.erase(peer->destid);
		info->umd_dma_did_peer_list[epi] = NULL;

		peer->sig = ~0;
		DmaPeerDestidDestroy(peer); free(peer);
	    }
	  }} while(0);
	  info->umd_dma_did_peer.clear();
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

        close(info->umd_sockp_quit[0]); close(info->umd_sockp_quit[1]);
        info->umd_sockp_quit[0] = info->umd_sockp_quit[1] = -1;

	close(info->umd_epollfd); info->umd_epollfd = -1;

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (info->umd_dch_list[ch] == NULL) continue;
		delete info->umd_dch_list[ch]->dch;
		free(info->umd_dch_list[ch]); info->umd_dch_list[ch] = NULL;
	}
        delete info->umd_dch_nread; info->umd_dch_nread = NULL;
        delete info->umd_lock; info->umd_lock = NULL;
        info->umd_tun_name[0] = '\0';

	// Do NOT close these as are manipulated from another thread!
	info->umd_mbox_rx_fd = -1;
        info->umd_mbox_tx_fd = -1;
}

void umd_dma_goodput_tun_add_ep(struct worker* info, const uint32_t destid)
{
	time_t now = time(NULL);

	DmaPeerCommsStats_t tmp; memset(&tmp, 0, sizeof(tmp));
	tmp.destid    = destid;
	tmp.on_time   = now;

        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	{{
	  info->umd_dma_did_enum_list[destid] = tmp;
	}}
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);
}

/** \brief Tell peer (via MBOX) to go away
 */
void umd_dma_goodput_tun_del_ep_signal(struct worker* info, const uint32_t destid)
{
	assert(info);
	assert(info->umd_chan2 >= 0);

	if (info->umd_mbox_tx_fd < 0) return;

	uint8_t buf[sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t)] = {0};

	DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;

	DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));
	payload->action = ACTION_DEL;

	pL2->destid_src = htons(info->my_destid);
	pL2->destid_dst = htons(destid);
	pL2->len        = htons(sizeof(DMA_MBOX_L2_t) + sizeof(DMA_MBOX_PAYLOAD_t));

	DBG("\n\tSignalling [tx_fd=%d] peer destid %u that is deleted!\n", info->umd_mbox_tx_fd, destid);

	if (info->umd_mbox_tx_fd < 0) return;

	int nsend = send(info->umd_mbox_tx_fd, buf, sizeof(buf), 0);
	if (nsend < 0) {
		ERR("\n\tSend to MboxWatch thread failed [tx_fd=%d]: %s\n", strerror(errno), info->umd_mbox_tx_fd);
	}
}

/** \brief Check whether we have an up Tun for an endpoint 
 */
bool umd_dma_goodput_tun_ep_has_peer(struct worker* info, const uint16_t destid)
{
	assert(info);
	assert(info->umd_chan2 >= 0);

	bool found = false;

        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
        {{
          std::map <uint16_t, int>::iterator itp = info->umd_dma_did_peer.find(destid);
	  if (itp != info->umd_dma_did_peer.end()) found = true;
        }}
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	return found;
}

void umd_dma_goodput_tun_del_ep(struct worker* info, const uint32_t destid, bool signal)
{
	assert(info);
	//assert(info->umd_dch_nread);

	const uint16_t my_destid = info->my_destid;
	assert(my_destid != destid);

	bool found = false;
        pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
        {{
	  DDBG("\n\t Enum EPs: %d Peers: %d\n", info->umd_dma_did_enum_list.size(), info->umd_dma_did_peer.size());

	  // Is it sane to do this hoping the peer will rebroadcast to us
	  // when it comes back? This means that unless mport/FMD notifies
	  // use AGAIN of this peer we shall never initiate comms with this
	  // peer?!?!
	  //
	  // NO! If both peers nuke each other they shall never talk again.

	  if (GetDecParm("$ignore_deadbeats", -1) != -1)
	  	info->umd_dma_did_enum_list.erase(destid);
	  else {
		  info->umd_dma_did_enum_list[destid].ls_time       = 0;
		  info->umd_dma_did_enum_list[destid].bcast_cnt_in  = 0;
		  info->umd_dma_did_enum_list[destid].bcast_cnt_out = 0;
	  }

          std::map <uint16_t, int>::iterator itp = info->umd_dma_did_peer.find(destid);
	  if (itp == info->umd_dma_did_peer.end()) { goto done; }

	  found = true;
	  {
		const int slot = itp->second;
                DmaPeerDestid_t* peer = info->umd_dma_did_peer_list[slot];
                assert(peer);

                {{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
                  struct epoll_event event;
                  event.data.fd = peer->tun_fd;
                  event.events = EPOLLIN;
                  epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
                }}

		peer->stop_req = 1;
		sem_post(&peer->rio_rx_work);
		sched_yield(); // Allow peer to wake up and quit... hopefully. And with mutex held. Yay!

		info->umd_dma_did_peer.erase(itp);
		info->umd_dma_did_peer_list[slot] = NULL;

                peer->sig = ~0;
                DmaPeerDestidDestroy(peer); free(peer);

		if (slot == (info->umd_dma_did_peer_list_high_wm-1))
			info->umd_dma_did_peer_list_high_wm--;
          }

	  // Mark IBwin mapping for peer as free and make RP=0
	  info->umd_peer_ibmap->free(destid);
        }}

done:
        pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	if (found) {
		if (signal) umd_dma_goodput_tun_del_ep_signal(info, destid); // Just in case it was forced we tell him for F*ck Off
		INFO("\n\tNuked peer destid %u%s\n", destid, (signal? " with MBOX notification": " without notification")); 
	} else  INFO("\n\tCannot find a peer for destid %u\n", destid);
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

	const int tundmathreadindex = info->umd_chan_to; // FUDGE
	if (tundmathreadindex < 0) return false;

	if (1 != wkr[tundmathreadindex].stat) return false;

	return true;
}

/** \brief Thread which watches RIO endpoint coming and going by querying libmport
 * \note It wants the wkr index of the Main Battle Tank thread as it will notify it OOB about ADD/DEL events
 */
extern "C"
void umd_epwatch_demo(struct worker* info)
{
	if (info == NULL) return;

	info->owner_func = umd_epwatch_demo;

	info->umd_mbox_rx_fd = -1;
        info->umd_mbox_tx_fd = -1;

	const int tundmathreadindex = info->umd_chan_to; // FUDGE
	if (tundmathreadindex < 0) return;

	if (info->did != ~0) {
		if (!umd_check_dma_tun_thr_running(info)) {
			CRIT("\n\tWorker thread %d (DMA Tun Thr) is not running. Bye!\n", tundmathreadindex);
			return;
		}
		DBG("\n\tForce-deleting EP destid %u\n", info->did);
		umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], info->did, true);
		return;
	}

	const char* SYS_RAPIDIO_DEVICES = "/sys/bus/rapidio/devices";

	int inf_fd = inotify_init1(IN_CLOEXEC);

	inotify_add_watch(inf_fd, SYS_RAPIDIO_DEVICES, IN_CREATE|IN_DELETE);

	bool first_time = true;
	std::map <uint16_t, bool> ep_map;

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
						const uint16_t destid = ep_list[i];
						ep_map[destid] = true;
						umd_dma_goodput_tun_add_ep(&wkr[tundmathreadindex], destid);
					}
				} else {
					std::map <uint32_t, bool> ep_map_now;
					for (int i = 0; i < ep_count; i++) {
						const uint16_t destid = ep_list[i];
						ep_map_now[destid] = true;
						if (ep_map.find(destid) == ep_map.end()) {
							ep_map[destid] = true;
							DBG("\n\tMport %d EP %u ADD\n", info->mp_num, destid);
							umd_dma_goodput_tun_add_ep(&wkr[tundmathreadindex], destid);
						}
					}
					// Set diff -- find out which destids have gone
					for (std::map <uint16_t, bool>::iterator itm = ep_map.begin(); itm != ep_map.end(); itm++) {
						const uint16_t destid = itm->first;
						if (ep_map_now.find(destid) == ep_map_now.end()) {
							ep_map.erase(destid);
							DBG("\n\tMport %d EP %u DEL\n", info->mp_num, destid);
							umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], destid, true); // notify just in case FMD is wrong
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

/** \brief Look at destid/EP's Last Seen time stamp and kick out if too old
 * \return true if endpoint exists and has been kicked
 */
bool umd_epwatch_timeout_ep(struct worker *info, const uint16_t destid)
{
	assert(info);

	time_t ls_time = 0;
	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	{{
	  std::map<uint16_t, DmaPeerCommsStats_t>::iterator ite = info->umd_dma_did_enum_list.begin();
	  if (ite != info->umd_dma_did_enum_list.end()) 
		ls_time = ite->second.ls_time;
	}}
	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	do {{
	  if (! ls_time) break; // Never contacted us

	  // No point nuking enumerated peers which haven't 
	  // established a Tun "connection" with us. Perhaps UMD Tun not started yet?
	  if (! umd_dma_goodput_tun_ep_has_peer(info, destid)) break;

	  int dT = time(NULL) - ls_time;
	  if (dT < (info->umd_dma_bcast_min * info->umd_dma_bcast_interval)) break; // Grace period

	  umd_dma_goodput_tun_del_ep(info, destid, false);
	  return true;
	}} while(0);

	return false;
}
/** \brief Thread which minds a MBOX and a socketpair
 * \note It wants the wkr index of the Main Battle Tank thread so it can communicate with it via socketpair(2): reads data from socketpair, sends over MBOX, reads data from MBOX, sends over socketpair
 * \note It modifies Main Battle Tank thread's struct worker { umd_mbox_tx_fd, umd_mbox_rx_fd }
 */
extern "C"
void umd_mbox_watch_demo(struct worker *info)
{
        uint64_t rx_ok = 0;
        uint64_t tx_ok = 0;
	int sockp1[2] = {-1};
	int sockp2[2] = {-1};
	int umd_mbox_tx_fd = -1;
	int umd_mbox_rx_fd = -1;

	info->owner_func = umd_mbox_watch_demo;

	info->umd_mbox_rx_fd = -1;
        info->umd_mbox_tx_fd = -1;

        const int tundmathreadindex = info->umd_chan_to; // FUDGE
        if (tundmathreadindex < 0) return;

	RioMport* mport = new RioMport(info->mp_num, info->mp_h);

	// Clear on read
	mport->rd32(TSI721_TXPKT_SMSG_CNT); // aka 0x41410  Sent Packet Count of Messaging Engine Register
	mport->rd32(TSI721_RXPKT_SMSG_CNT); // aka 0x29900   Received Packet Count for Messaging Engine Register

        if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

        if (! TakeLock(info, "MBOX", info->umd_chan)) goto exit;

        info->umd_mch = new MboxChannel(info->mp_num, info->umd_chan, info->mp_h);

        if (NULL == info->umd_mch) {
                CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
                info->umd_tun_name[0] = '\0';
                close(info->umd_tun_fd); info->umd_tun_fd = -1;
                delete info->umd_lock; info->umd_lock = NULL;
                goto exit_bomb;
        };

	// I only send only 1 MBOX message at a time

        if (! info->umd_mch->open_mbox(MBOX_BUFC, MBOX_STS)) {
                CRIT("\n\tMboxChannel: Failed to open mbox!");
                info->umd_tun_name[0] = '\0';
                close(info->umd_tun_fd); info->umd_tun_fd = -1;
                delete info->umd_mch; info->umd_mch = NULL;
                delete info->umd_lock; info->umd_lock = NULL;
                goto exit_bomb;
        }

        socketpair(PF_LOCAL, SOCK_DGRAM, 0, sockp1); // SOCK_DGRAM so we have a message delineation
        socketpair(PF_LOCAL, SOCK_DGRAM, 0, sockp2); // SOCK_DGRAM so we have a message delineation

	umd_mbox_tx_fd = sockp1[0];
	umd_mbox_rx_fd = sockp2[1];

        if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

	wkr[tundmathreadindex].umd_mbox_tx_fd = sockp2[0];
	if (wkr[tundmathreadindex].umd_set_rx_fd != NULL)
	     wkr[tundmathreadindex].umd_set_rx_fd(&wkr[tundmathreadindex], sockp1[1]);
	else wkr[tundmathreadindex].umd_mbox_rx_fd = sockp1[1];

        INFO("\n\tWatching MBOX %d [tx_fd=%d, rx_fd=%d] sockp (%d,%d) (%d,%d)\n",
	     info->umd_chan, umd_mbox_tx_fd, umd_mbox_rx_fd,
	     sockp1[0], sockp1[1],
	     sockp2[0], sockp2[1]);

	info->umd_mch->setInitState();

	{{
	  uint8_t buf[PAGE_4K] = {0};

	  MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
          opt.mbox   = info->umd_chan;

	  for(int i = 0; i < MBOX_BUFC; i++) {
		void* b = calloc(1, PAGE_4K);
		info->umd_mch->add_inb_buffer(b);
	  }

	  MboxChannel::WorkItem_t wi[MBOX_STS*8]; memset(wi, 0, sizeof(wi));

          while (! info->stop_req) {
		if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

// Receive from socketpair(2), send on MBOX

		struct timeval to = { 0, 1 * 1000 }; // 1 ms
		fd_set rfds; FD_ZERO (&rfds); FD_SET(umd_mbox_rx_fd, &rfds);

		int ret = select(umd_mbox_rx_fd+1, &rfds, NULL, NULL, &to);
		if (ret < 0) {
			CRIT("\n\tselect failed on umd_mbox_rx_fd: %s\n", strerror(errno));
			goto exit;
		}

		if (info->stop_req) goto exit;
		if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

		if (FD_ISSET(umd_mbox_rx_fd, &rfds)) { // Sumthin to send on MBOX, read only 1st dgram
			const int nread = recv(umd_mbox_rx_fd, buf, PAGE_4K, 0);

			DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
			assert(nread == ntohs(pL2->len));
			pL2->mbox_src = info->umd_chan;

			DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));

			bool q_was_full = false;
             		opt.destid = ntohs(pL2->destid_dst);
			DBG("\n\tSending %d bytes to RIO destid %u over MBOX action=0x%x\n", nread, opt.destid, payload->action);
			MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;
			if (! info->umd_mch->send_message(opt, buf, nread, true, fail_reason)) {
				if (fail_reason == MboxChannel::STOP_REG_ERR) {
					DBG("\n\tsend_message FAILED! TX q_size=%d destid=%u. Soft MBOX restart.\n",
					    info->umd_mch->queueTxSize(), opt.destid);

					info->umd_mch->softRestart();

					// No point nuking enumerated peers which haven't 
					// established a Tun "connection" with us. Perhaps UMD Tun not started yet?
					if (umd_check_dma_tun_thr_running(info) /*&& umd_dma_goodput_tun_ep_has_peer(&wkr[tundmathreadindex], opt.destid)*/)
						umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], opt.destid, false);

					goto receive;
				} else { q_was_full = true; }
			} else { tx_ok++; }

			DDBG("\n\tPolling FIFO transfer completion destid=%d TX q_size = %d\n", opt.destid, info->umd_mch->queueTxSize());
			for (int i = 0;
			     !q_was_full && !info->stop_req && (info->umd_mch->scanFIFO(wi, MBOX_STS*8) == 0) && (i < 100000);
			     i++) {
				usleep(1);
			}

			if (info->stop_req) goto exit;
			if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

			if (info->umd_mch->queueTxSize() > 0) {
                                ERR("\n\tTX queue non-empty for destid=%u. Soft MBOX restart.%s tx_ok=%d TXPKT_SMSG_CNT=%u RXPKT_SMSG_CNT=%u\n",
                                    opt.destid, (q_was_full? "Q FULL?": ""), tx_ok,
                                    mport->rd32(TSI721_TXPKT_SMSG_CNT), mport->rd32(TSI721_RXPKT_SMSG_CNT));

				info->umd_mch->softRestart();

				if (umd_check_dma_tun_thr_running(info) /*&& umd_dma_goodput_tun_ep_has_peer(&wkr[tundmathreadindex], opt.destid)*/)
					umd_dma_goodput_tun_del_ep(&wkr[tundmathreadindex], opt.destid, false);

				goto receive;
			}

			if (info->stop_req) goto exit;
			if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

			// Hmm go and lock a mutex unprovoked. Despicable
			umd_epwatch_timeout_ep(&wkr[tundmathreadindex], opt.destid);
		} // END if FD_ISSET

	receive:
		if (info->stop_req) goto exit;
		if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

// Receive from MBOX, send to umd_mbox_rx_fd

		bool message_ready = false;
		for (int i = 0; !info->stop_req && i < 100000; i++) {
			uint64_t rx_ts = 0;
			if (info->umd_mch->inb_message_ready(rx_ts)) { message_ready = true; break; }
			if (info->stop_req) goto exit;
			if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;
			usleep(1);
		}

		if (info->stop_req) goto exit;
		if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

		if (!message_ready) continue;

		uint8_t* buf = NULL;
		MboxChannel::MboxOptions_t opt_rx; memset(&opt_rx, 0, sizeof(opt));
                while ((buf = (uint8_t*)info->umd_mch->get_inb_message(opt_rx)) != NULL) {
			if (info->stop_req) goto exit;
			if (!umd_check_dma_tun_thr_running(info)) goto exit_bomb;

			DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
			assert(opt_rx.bcount >= ntohs(pL2->len));

			DMA_MBOX_PAYLOAD_t* payload = (DMA_MBOX_PAYLOAD_t*)(buf + sizeof(DMA_MBOX_L2_t));
			DBG("\n\tGot %d/%d bytes on RIO MBOX from destid %u [tx_fd=%d] action=0x%x\n", opt_rx.bcount,
			    ntohs(pL2->len), ntohs(pL2->destid_src),
			    umd_mbox_tx_fd, payload->action);

			if (send(umd_mbox_tx_fd, buf, ntohs(pL2->len), MSG_DONTWAIT) < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
				CRIT("\n\tsend failed [tx_fd=%d]: %s\n", umd_mbox_tx_fd, strerror(errno));
				goto exit;
			}
				
			info->umd_mch->add_inb_buffer(buf); // recycle
			rx_ok++;
		}
	  } // END while
	}}

exit:
        if (umd_check_dma_tun_thr_running(info)) {
		wkr[tundmathreadindex].umd_mbox_tx_fd = -1;
        	wkr[tundmathreadindex].umd_mbox_rx_fd = -1;
	}

        close(sockp1[0]); close(sockp1[1]);
        close(sockp2[0]); close(sockp2[1]);

	delete mport;
        delete info->umd_mch; info->umd_mch = NULL;
        delete info->umd_lock; info->umd_lock = NULL;

	return;

exit_bomb:
	CRIT("\n\tWorker thread %d (DMA Tun Thr) is not running. Bye!\n", tundmathreadindex);
	goto exit;
}

/** \bried Dump UMD DMA Tun status
 * \note This executes within the CLI thread but targets Main Battle Tank thread's data
 */
extern "C"
void UMD_DD(struct worker* info)
{
	int q_size[6] = {0};
	bool     port_ok[6] = {0};
	bool     port_err[6] = {0};
	uint32_t port_WP[6] = {0};
	uint64_t port_FIFO_WP[6] = {0};
	uint64_t port_ticks_total[6] = {0};
	uint64_t port_total_ticks_tx[6] = {0};
        DmaChannelInfo_t* dch_list[6] = {0};

	assert(info->umd_dch_nread);

	std::string s;
	if (info->umd_peer_ibmap->toString(s) > 0)
		INFO("\n\tIBwin mappings:\n%s", s.c_str());

        int dch_cnt = 0;
        for (int ch = info->umd_chan; info->umd_chan >= 0 && ch <= info->umd_chan_n; ch++) {
		assert(info->umd_dch_list[ch]);

		dch_list[dch_cnt]         = info->umd_dch_list[ch];
		q_size[dch_cnt]           = info->umd_dch_list[ch]->dch->queueSize();
		port_ok[dch_cnt]          = info->umd_dch_list[ch]->dch->checkPortOK();
		port_err[dch_cnt]         = info->umd_dch_list[ch]->dch->checkPortError();
		port_WP[dch_cnt]          = info->umd_dch_list[ch]->dch->getWP();
		port_FIFO_WP[dch_cnt]     = info->umd_dch_list[ch]->dch->m_tx_cnt;
		port_ticks_total[dch_cnt] = info->umd_dch_list[ch]->ticks_total;

		port_total_ticks_tx[dch_cnt] = info->umd_dch_list[ch]->total_ticks_tx;

		dch_cnt++;
	}

	const int MHz = getCPUMHz();

	std::stringstream ss;
	{
		char tmp[257] = {0};
		snprintf(tmp, 256, "Chan2 %d q_size=%d", info->umd_chan2, info->umd_dch_nread->queueSize());
		ss << "\n\t\t" << tmp;
		ss << "      WP=" << info->umd_dch_nread->getWP() << " FIFO.WP=" << info->umd_dch_nread->m_tx_cnt;
		if (info->umd_dch_nread->m_tx_cnt > 0) {
			float AvgUS = ((float)info->umd_ticks_total_chan2 / info->umd_dch_nread->m_tx_cnt) / MHz;
			ss << " AvgTxRx=" << AvgUS << "uS";
		}
		if (info->umd_dch_nread->checkPortOK()) ss << " ok";
		if (info->umd_dch_nread->checkPortError()) ss << " ERROR";
	}
	for (int ch = 0; ch < dch_cnt; ch++) {
		assert(dch_list[ch]);

		char tmp[257] = {0};
		snprintf(tmp, 256, "Chan  %d q_size=%d oi=%d", dch_list[ch]->chan, q_size[ch], dch_list[ch]->oi);
		ss << "\n\t\t" << tmp;
		ss << " WP=" << port_WP[ch];
		ss << " FIFO.WP=" << port_FIFO_WP[ch];
		if (port_FIFO_WP[ch] > 0) {
			float AvgUS = ((float)port_ticks_total[ch] / port_FIFO_WP[ch]) / MHz;
			ss << " AvgTX=" << AvgUS << "uS";
			float AvgUSTun = ((float)port_total_ticks_tx[ch] / port_FIFO_WP[ch]) / MHz;
			ss << " AvgTxTun=" << AvgUSTun << "uS";
		}
		if (port_ok[ch]) ss << " ok";
		if (port_err[ch]) ss << " ERROR";
	}
	if (dch_cnt > 0)
		INFO("\n\tDMA Channel stats: %s\n", ss.str().c_str());

	std::vector<DmaPeerDestid_t>     peer_list;
	std::vector<DmaPeerCommsStats_t> peer_list_enum;

	time_t now = time(NULL);

	// Collect snapshot of peers quickly

	pthread_mutex_lock(&info->umd_dma_did_peer_mutex);
	{{
	  std::map<uint16_t, DmaPeerCommsStats_t>::iterator ite = info->umd_dma_did_enum_list.begin();
	  for (; ite != info->umd_dma_did_enum_list.end(); ite++) {
		if (ite->first == info->my_destid) continue;
		peer_list_enum.push_back(ite->second);
	  }

          std::map <uint16_t, int>::iterator itp = info->umd_dma_did_peer.begin();
	  for (; itp != info->umd_dma_did_peer.end(); itp++)
		peer_list.push_back(*info->umd_dma_did_peer_list[itp->second]); // Yeehaw! Make a copy of struct
	}}
	pthread_mutex_unlock(&info->umd_dma_did_peer_mutex);

	if (peer_list_enum.size() > 0) {
		std::stringstream ss;
		for (int ip = 0; ip < peer_list_enum.size(); ip++) {
			char tmp[257] = {0};
			snprintf(tmp, 256, "Did %u Age %ds LS %ds bcast_cnt {out=%d in=%d} RIO addr sent 0x%llx",
				 peer_list_enum[ip].destid,
				 (peer_list_enum[ip].on_time > 0)? now-peer_list_enum[ip].on_time: -1,
				 (peer_list_enum[ip].ls_time > 0)? now-peer_list_enum[ip].ls_time: -1,
				 peer_list_enum[ip].bcast_cnt_out,
				 peer_list_enum[ip].bcast_cnt_in,
				 peer_list_enum[ip].my_rio_addr);
			ss << "\n\t\t" << tmp;
		}
		INFO("\n\tGot %d enumerated peer(s): %s\n", peer_list_enum.size(), ss.str().c_str());
	}

	if (peer_list.size() > 0) {
		std::stringstream ss;
		for (int ip = 0; ip < peer_list.size(); ip++) {
			DmaPeerDestid_t& peer = peer_list[ip];
			char tmp[257] = {0};
			snprintf(tmp, 256, "Did %u %s peer.rio_addr=0x%llx tx.WP=%u tx.RP~%u tx.RIO=%llu rx.RIO=%llu\n\t\t\tTun.rx=%llu Tun.tx=%llu Tun.txerr=%llu\n\t\t\ttx.peer_full=%llu", 
				 peer.destid, peer.tun_name, peer.rio_addr,
				 peer.WP, peer.RP,
				 peer.tx_cnt, peer.rx_cnt,
				 peer.tun_rx_cnt, peer.tun_tx_cnt, peer.tun_tx_err,
			         peer.rio_tx_peer_full
				);
			ss << "\n\t\t" << tmp;

			uint32_t* pRP = (uint32_t*)peer.ib_ptr;
			assert(pRP);

			float TotalTimeSpentFull = (float)peer.rio_rx_peer_full_ticks_total / MHz;
			snprintf(tmp, 256, "\n\t\t\trx.RP=%u IBBdReady=%d IBBDFullTotal=%fuS", *pRP, peer.rio_rx_bd_ready_size, TotalTimeSpentFull);
			ss << tmp;

			if (peer.tun_tx_cnt > 0) {
				char tmp[65] = {0};
				float AvgUS = ((float)peer.total_ticks_rx / peer.tun_tx_cnt) / MHz;
				snprintf(tmp, 64, " AvgSoftRX=%fuS", AvgUS);
				ss << tmp;
			}

			if (peer.mutex.__data.__lock) {
				snprintf(tmp, 256, "\n\t\t\tlocker.tid=0x%x", peer.mutex.__data.__owner);
				ss << tmp;
			}
		}
		INFO("\n\tGot %d UP peer(s): %s\n", peer_list.size(), ss.str().c_str());
	}
}
