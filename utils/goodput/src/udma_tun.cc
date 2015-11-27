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

bool send_icmp_host_unreachable(struct worker* info, uint8_t* l3_in, const int l3_in_size);
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

/** \brief Thread that services Tun TX, sends L3 frames to peer and does RIO TX throttling
 * Update RP via NREAD every 8 buffers; when local q is 2/3 full do it after each send
 */
void* umd_dma_tun_proc_thr(void *parm)
{
        if (NULL == parm) pthread_exit(NULL);

        struct worker* info = (struct worker *)parm;
        if (NULL == info->umd_dch_list[info->umd_chan]) goto exit;
        if (NULL == info->umd_dch2) goto exit;

	DBG("\n\tReady to receive from %s!\n", info->umd_tun_name);

        {{
	DmaChannelInfo_t* dch_list[6] = {0};

        const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;
	
	int dch_cnt = 0;
	int dch_cur = 0;
	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) dch_list[dch_cnt++] = info->umd_dch_list[ch];

	int outstanding = 0; // This is our guess of what's not consumed at other end, per-destid
	uint64_t tx_cnt = 0;
        const int tun_fd = info->umd_tun_fd; // XXX bollocks
        const int net_fd = info->umd_sockp[1];
        const int maxfd = (tun_fd > net_fd)?tun_fd:net_fd;

        info->umd_dma_tap_proc_alive = 1;
        sem_post(&info->umd_dma_tap_proc_started);

	uint16_t destid = 0x69; // apparently all are valid so pick one
        const uint16_t my_destid = info->umd_dch2->getDestId();

        bad_destid[my_destid] = true;

again:
        while(! info->stop_req) {
		DmaChannelInfo_t* dci = dch_list[dch_cur++];
		if (dch_cur >= dch_cnt) dch_cur = 0; // Wrap-around

                fd_set rd_set;

// XXX tun fd is in epoll set!!!
//
                FD_ZERO(&rd_set);
                FD_SET(tun_fd, &rd_set); FD_SET(net_fd, &rd_set);

                const int ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);
                if (info->stop_req) goto exit;

                if (ret < 0 && errno == EINTR) continue;

                if (ret < 0) { CRIT("\n\tselect(): %s\n", strerror(errno)); goto exit; }

                if(FD_ISSET(net_fd, &rd_set)) { goto exit; }

                if (! FD_ISSET(tun_fd, &rd_set)) continue; // XXX Whoa! Why?

                // Data from tun/tap: read it, decode destid from 169.254.x.y and write it to RIO

		// XXX change this to read directly into CMA memory!!

		uint8_t* buffer = (uint8_t*)dci->dmamem[dci->oi].win_ptr;

		DMA_L2_t* pL2 = (DMA_L2_t*)buffer;
                const int nread = cread(tun_fd, buffer+DMA_L2_SIZE, info->umd_tun_MTU);

                {{
                uint32_t* pkt = (uint32_t*)(buffer+DMA_L2_SIZE);
                const uint32_t dest_ip_v4 = ntohl(pkt[4]); // XXX IPv6 will stink big here

                destid = (dest_ip_v4 & 0xFFFF) - DESTID_TRANSLATE;
                }}

                bool is_bad_destid = bad_destid.find(destid) != bad_destid.end();

		// Do I know about this destid (yet?)
		if (info->umd_dma_did_peer.find(destid) == info->umd_dma_did_peer.end()) is_bad_destid = true;

#ifdef UDMA_TUN_DEBUG
                const uint32_t crc = crc32(0, buffer+DMA_L2_SIZE, nread);
                DBG("\n\tGot from tun? %d+%d bytes (L7 CRC32 0x%x) to RIO destid %u%s\n",
                         nread, DMA_L2_SIZE,
                         crc, destid,
                         is_bad_destid? " BLACKLISTED": "");
#endif

                if (is_bad_destid) {
                        send_icmp_host_unreachable(info, buffer+DMA_L2_SIZE, nread); // XXX which tun fd??
                        continue;
                }

		DmaTunDestid_t* peer = info->umd_dma_did_peer[destid];
                const bool first_message = good_destid.find(destid) == good_destid.end();

		// We force reading RP from a "new" destid as a RIO ping as
		// NWRITE does not barf  on bad destids

		if (first_message || ((tx_cnt+1) % 8) == 0 || dci->dch->queueSize() > Q_THR) { // This must be done per-destid
			uint32_t newRP = ~0;
			if (udma_nread_mem(info, destid, peer->rio_addr, sizeof(newRP), (uint8_t*)&newRP)) {
				DBG("\n\tPulled RP from destid %u old RP=%d actual RP=%d\n", destid, peer->RP, newRP);
				peer->RP = newRP;
				// XXX if first_message the WP := newRP+1 ?? Test
			} else {
                                send_icmp_host_unreachable(info, buffer+DMA_L2_SIZE, nread); // XXX which tun fd??
				DBG("\n\tHW error, something is FOBAR with Chan %u\n", info->umd_chan2);

                                bad_destid[destid] = true;
				continue;
			}
		}

		do {{
		  if (peer->WP == peer->RP) { break; }
		  if (peer->WP > peer->RP)  { outstanding = peer->WP-peer->RP; break; }
		  //if (WP == (info->umd_tx_buf_cnt-2)) { outstanding = RP; break; }
		  outstanding = peer->WP + dci->tx_buf_cnt-2 - peer->RP;
		}} while(0);

		DBG("\n\tWP=%d guessed { RP=%d outstanding=%d } %s\n", peer->WP, peer->RP, outstanding, (outstanding==(dci->tx_buf_cnt-1))? "FULL": "");

		if (outstanding == (dci->tx_buf_cnt-1)) {
			CRIT("\n\tPeer destid=%u is FULL, dropping frame!\n", destid);
			// XXX Maybe send back ICMP Host Unreachable? TODO: Study RFCs
			continue;
		}

        	pL2->destid = htons(my_destid);
		pL2->len    = htonl(DMA_L2_SIZE + nread);
		pL2->RO     = 1;

                if (info->stop_req) goto exit;

		// Barry dixit "If full sleep for (queue size / 2) nanoseconds"
		for (int i = 0; i < (dci->tx_buf_cnt-1)/2; i++) {
			if (! dci->dch->queueFull()) break;
			struct timespec tv = { 0, 1};
			nanosleep(&tv, NULL);
		}

		if (dci->dch->queueFull()) {
			continue; // Drop L3 frame
			DBG("\n\tQueue full #1!\n");
		}
                if (info->stop_req) goto exit;

		dci->dmaopt[dci->oi].destid      = destid;
		dci->dmaopt[dci->oi].bcount      = ntohl(pL2->len);
		dci->dmaopt[dci->oi].raddr.lsb64 = peer->rio_addr + sizeof(uint32_t) + peer->WP * BD_PAYLOAD_SIZE(info);

                DBG("\n\tSending to RIO %d+%d bytes to RIO destid %u addr 0x%llx WP=%d oi=%d\n",
                    nread, DMA_L2_SIZE, destid, dci->dmaopt[dci->oi].raddr.lsb64, peer->WP, dci->oi);

	        dci->dch->setCheckHwReg(first_message);

		info->umd_dma_abort_reason = 0;
		if (! dci->dch->queueDmaOpT1(ALL_NWRITE, dci->dmaopt[dci->oi], dci->dmamem[dci->oi],
			                        info->umd_dma_abort_reason, &info->meas_ts)) {
			if(info->umd_dma_abort_reason != 0) { // HW error
                                // ICMPv4 dest unreachable id bad destid 
                                send_icmp_host_unreachable(info, buffer+DMA_L2_SIZE, nread); // XXX which tun
				DBG("\n\tHW error, triggering soft restart\n");

                                bad_destid[destid] = true;

                                info->stop_req = SOFT_RESTART; // XXX of which channel?
				break;
			} else { // queue really full
				DBG("\n\tQueue full #2!\n");
				continue; // Drop L3 frame
			}
		}
                if (first_message) good_destid[destid] = true;

		dci->oi++; if (dci->oi == (dci->tx_buf_cnt-1)) dci->oi = 0; // Account for T3

		// For just one destdid WP==oi but we keep them separate

		peer->WP++; // This must be done per-destid
		if (peer->WP == (dci->tx_buf_cnt-1)) peer->WP = 0; // Account for T3 missing IBwin cell, ALL must have the same bufc!

		tx_cnt++;
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
        info->umd_dma_tap_proc_alive = 0;

        pthread_exit(parm);
} // END umd_dma_tun_proc_thr


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

	// XXX this shit map must be protected by a mutex.

	std::map <uint16_t, DmaTunDestid_t*>::iterator itp = info->umd_dma_did_peer.begin();
	for (; itp != info->umd_dma_did_peer.end(); itp++) {
		DmaTunDestid_t* peer = itp->second;
		assert(peer);

		volatile uint32_t* pRP = (uint32_t*)peer->ib_ptr;

		int k = *pRP;
		assert(k >= 0);
		assert(k < (info->umd_tx_buf_cnt-1));

		if (peer->rio_rx_bd_ready_size >= (info->umd_tx_buf_cnt-1)) continue; // Receiver too slow, inc slow count and wait!

 		pthread_spin_lock(&peer->rio_rx_bd_ready_splock);

		int cnt = 0;
		int idx = peer->rio_rx_bd_ready_size; // If not zero then RIO RX thr is sloow
		for (int i = 0; i < (info->umd_tx_buf_cnt-1); i++) {
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

#ifdef UDMA_TUN_DEBUG_IB
		DBG("\n\tFound %d ready buffers at iter %llu\n", cnt, cbk_iter);
#endif

		sem_post(&peer->rio_rx_work);

	} // END for each peer
}

/** \brief This helper thread wakes up the main thread at quitting time */
void* umd_dma_wakeup_proc_thr(void* arg)
{
	if(arg == NULL) return NULL;
	struct worker* info = (struct worker*)arg;

        while (!info->stop_req) // every 100 mS which is about HZ. stupid
		usleep(100 * 1000);
	
	std::map <uint16_t, DmaTunDestid_t*>::iterator itp = info->umd_dma_did_peer.begin();
	for (; itp != info->umd_dma_did_peer.end(); itp++) {
		DmaTunDestid_t* peer = itp->second;
		assert(peer);
		sem_post(&peer->rio_rx_work);
	}

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

bool umd_dma_goodput_tun_setup_TUN(struct worker *info, uint16_t my_destid)
{
        char if_name[IFNAMSIZ] = {0};
        int flags = IFF_TUN | IFF_NO_PI;

	if (info == NULL) return false;

        memset(info->umd_tun_name, 0, sizeof(info->umd_tun_name));

        // Initialize tun/tap interface
        if ((info->umd_tun_fd = tun_alloc(if_name, flags)) < 0) {
                CRIT("Error connecting to tun/tap interface %s!\n", if_name);
                return false;
        }
        strncpy(info->umd_tun_name, if_name, sizeof(info->umd_tun_name)-1);

        // Configure tun/tap interface for IPv4, L2, no ARP, no multicast

        char TapIPv4Addr[17] = {0};
        const uint16_t my_destid_tun = my_destid + DESTID_TRANSLATE;
        snprintf(TapIPv4Addr, 16, "169.254.%d.%d", (my_destid_tun >> 8) & 0xFF, my_destid_tun & 0xFF);

        char ifconfig_cmd[257] = {0};
        snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s %s netmask 0xffff0000 mtu %d up",
                                    if_name, TapIPv4Addr, info->umd_tun_MTU);
        const int rr = system(ifconfig_cmd);
        if(rr >> 8) {
                info->umd_tun_name[0] = '\0';
                close(info->umd_tun_fd); info->umd_tun_fd = -1;
                return false;
        }

        snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
        system(ifconfig_cmd);

        INFO("\n\t%s %s mtu %d on DMA Chan=%d Chan_n=%d Chan2=%d my_destid=%u #buf=%d #fifo=%d\n",
             if_name, TapIPv4Addr, info->umd_tun_MTU,
             info->umd_chan, info->umd_chan_n, info->umd_chan2,
             my_destid,
             info->umd_tx_buf_cnt, info->umd_sts_entries);

	return true;
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
	uint64_t rx_ok = 0;
	DmaTunDestid_t* peer = NULL;
	
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

	{{
	  DmaTunDestid_t* tmp = (DmaTunDestid_t*)calloc(1, sizeof(DmaTunDestid_t));
	  if (tmp == NULL) goto exit;
	  info->umd_dma_did_peer[info->did] = tmp;
	}}

	// Set up array of pointers to IB L2 headers
	if (! DmaTunDestidInit(info, info->umd_dma_did_peer[info->did], info->ib_ptr)) goto exit;

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

        socketpair(PF_LOCAL, SOCK_STREAM, 0, info->umd_sockp);

	// XXX TODO
	// Phase 1
	// setup up per-peer pointopoint tun
	// spawn one RIO RX thr per destid
	// make an epoll set for tun fds
	// protect info->umd_dma_did_peer with mutex (*)
	// write thread for watching rio destid which bangs info->umd_dma_did_peer, makes new tunnel
	// also kill stale RIO RX threads, purge good_destid, bad_destid under mutex (*)
	// Phase 2
	// write thread that uses mbox/kernel to "broadcast" my ibwin + ALLOCATIONS every 60s
	// write thread that uses mbox/kernel to listen to ibwin bcasts
	if (!umd_dma_goodput_tun_setup_TUN(info, info->umd_dch2->getDestId())) goto exit;

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
                            umd_dma_tun_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_tun_proc_thr thread, exiting...");
                goto exit;
        }
        sem_wait(&info->umd_dma_tap_proc_started);

        if (!info->umd_dma_tap_proc_alive) {
                CRIT("\n\tumd_dma_tun_proc_thr thread is dead, exiting..");
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

	// Receiver (from RIO)
	DBG("\n\tReady to receive from RIO!\n");

again: // Receiver (from RIO), TUN TX: Ingest L3 frames into Tun (zero-copy), update RP, set RO=0

	/*DmaTunDestid_t*/ peer = info->umd_dma_did_peer[info->did];

        while (!info->stop_req && !peer->stop_req) {
	        volatile uint32_t* pRP = (uint32_t*)peer->ib_ptr;

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

	if (info->stop_req == SOFT_RESTART) {
		INFO("\n\tSoft restart requested, nuking MBOX hardware!\n");
		//info->umd_dch->softRestart(); // XXX which one?!?!?
		info->stop_req = 0;
		sem_post(&info->umd_fifo_proc_started);
		sem_post(&info->umd_dma_tap_proc_started);
		goto again;
	}

exit:
	write(info->umd_sockp[0], "X", 1); // Signal Tun/Tap thread to eXit

        info->umd_fifo_proc_must_die = 1;

        pthread_join(info->umd_fifo_thr.thr, NULL);
        pthread_join(info->umd_dma_tap_thr.thr, NULL);

        DmaTunDestidDestroy(info->umd_dma_did_peer[info->did]);
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

        close(info->umd_sockp[0]); close(info->umd_sockp[1]);
        info->umd_sockp[0] = info->umd_sockp[1] = -1;

	std::map <uint16_t, DmaTunDestid_t*>::iterator itp = info->umd_dma_did_peer.begin();
	for (; itp != info->umd_dma_did_peer.end(); itp++) {
		DmaTunDestid_t* peer = itp->second;
		assert(peer);

		DmaTunDestidDestroy(peer); free(peer);
	}
	info->umd_dma_did_peer.clear();

	for (int ch = info->umd_chan; ch <= info->umd_chan_n; ch++) {
		if (info->umd_dch_list[ch] == NULL) continue;
		delete info->umd_dch_list[ch]->dch;
		free(info->umd_dch_list[ch]); info->umd_dch_list[ch] = NULL;
	}
        delete info->umd_dch2; info->umd_dch2 = NULL;
        delete info->umd_lock; info->umd_lock = NULL;
        info->umd_tun_name[0] = '\0';
}

