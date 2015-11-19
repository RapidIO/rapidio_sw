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
#endif

extern "C" {
void zero_stats(struct worker *info);
bool umd_check_cpu_allocation(struct worker *info);
bool TakeLock(struct worker* info, const char* module, int instance);

void* umd_dma_fifo_proc_thr(void *parm);

uint32_t crc32(uint32_t crc, const void *buf, size_t size);

int tun_alloc(char *dev, int flags);
int cread(int fd, uint8_t* buf, int n);
int cwrite(int fd, uint8_t* buf, int n);

bool send_icmp_host_unreachable(struct worker* info, uint8_t* l3_in, const int l3_in_size);
};

bool udma_nread_mem(struct worker *info, const uint16_t did, const uint64_t rio_addr, const int size, uint8_t* data_out)
{
	if(info == NULL || size < 1 || size > 16 || data_out == NULL) return false;

	DMAChannel::DmaOptions_t dmaopt; memset(&dmaopt, 0, sizeof(dmaopt));
	dmaopt.destid      = did;
	dmaopt.bcount      = size;
	dmaopt.raddr.lsb64 = rio_addr;

	struct seq_ts tx_ts;
	DMAChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

	if(! info->umd_dch2->queueDmaOpT2((int)NREAD, dmaopt, data_out, size, info->umd_dma_abort_reason, &tx_ts)) return false;

	const bool q_was_full = (info->umd_dma_abort_reason == 0);

	DBG("\n\tPolling FIFO transfer completion destid=%d\n", did);
	while (!q_was_full && !info->stop_req && info->umd_dch2->scanFIFO(wi, info->umd_sts_entries*8) == 0) { ; }

	memcpy(data_out, wi[0].t2_rddata, size);

	return true;
}

const int DESTID_TRANSLATE = 1;

static std::map <uint16_t, bool> bad_destid;
static std::map <uint16_t, bool> good_destid;

// Update RP via NREAD every 8 buffers; when local q is 2/3 full do it after each send

void* umd_dma_tun_proc_thr(void *parm)
{
        if (NULL == parm) pthread_exit(NULL);

        const int BUFSIZE = 8192;
        uint8_t buffer[BUFSIZE];

        struct worker* info = (struct worker *)parm;
        if (NULL == info->umd_mch) goto exit;

        {{
        const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;
	
	int RP = 0, WP = 0;
	uint64_t tx_cnt = 0;
        const int tun_fd = info->umd_tun_fd;
        const int net_fd = info->umd_sockp[1];
        const int maxfd = (tun_fd > net_fd)?tun_fd:net_fd;

        info->umd_dma_tap_proc_alive = 1;
        sem_post(&info->umd_dma_tap_proc_started);

        DMAChannel::DmaOptions_t opt; memset(&opt, 0, sizeof(opt));

        const uint16_t my_destid = info->umd_mch->getDestId();

        bad_destid[my_destid] = true;

again:
        while(! info->stop_req) {
                fd_set rd_set;

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

		DMA_L2_t* pL2 = (DMA_L2_t*)buffer;
                const int nread = cread(tun_fd, buffer+DMA_L2_SIZE, BUFSIZE-DMA_L2_SIZE);

                {{
                uint32_t* pkt = (uint32_t*)(buffer+DMA_L2_SIZE);
                const uint32_t dest_ip_v4 = ntohl(pkt[4]); // XXX IPv6 will stink big here

                opt.destid = (dest_ip_v4 & 0xFFFF) - DESTID_TRANSLATE;
                }}

                const bool is_bad_destid = bad_destid.find(opt.destid) != bad_destid.end();

#ifdef UDMA_TUN_DEBUG
                const uint32_t crc = crc32(0, buffer, nread+DMA_L2_SIZE);
                DBG("\n\tGot from %s %d+%d bytes (L2 CRC32 0x%x) to RIO destid %u%s\n",
                         info->umd_tun_name, nread, DMA_L2_SIZE,
                         crc, opt.destid,
                         is_bad_destid? " BLACKLISTED": "");
#endif

                if (is_bad_destid) {
                        send_icmp_host_unreachable(info, buffer+DMA_L2_SIZE, nread);
                        continue;
                }

                DBG("\n\tSending to RIO %d+%d bytes to RIO destid %u\n", nread, DMA_L2_SIZE, opt.destid);

        	pL2->destid = htons(my_destid);
		pL2->len    = htonl(DMA_L2_SIZE + nread);
		pL2->RO     = 1;

        send_again:
                if (info->stop_req) goto exit;

                const bool first_message = good_destid.find(opt.destid) == good_destid.end();

                MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;
                if (0 /*! info->umd_mch->send_message(opt, buffer, nread+DMA_L2_SIZE, first_message, fail_reason)*/) {
                        if (fail_reason == MboxChannel::STOP_REG_ERR) {
                                ERR("\n\tsend_message FAILED! TX q size = %d\n", info->umd_mch->queueTxSize());

                                // XXX ICMPv4 dest unreachable id bad destid - TBI

                                bad_destid[opt.destid] = true;

                                info->stop_req = SOFT_RESTART;
                                send_icmp_host_unreachable(info, buffer+DMA_L2_SIZE, nread);
                                break;
                        } else { goto send_again; } // Succeed or die trying
                } else {
                        if (first_message) good_destid[opt.destid] = true;
                }

		WP++; // This must be done per-destid
		if (WP == info->umd_tx_buf_cnt) WP = 0;

		tx_cnt++;
		if (((tx_cnt+1) % 8) == 0) { // This must be done per-destid
			uint32_t newRP = 0;
			if (udma_nread_mem(info, opt.destid, info->rio_addr, sizeof(newRP), (uint8_t*)&newRP)) {
				DBG("\n\tPulled RP from destid %u old RP=%d actual RP=%d\n", opt.destid, RP, newRP);
				RP = newRP;
			}
		}

		// XXX compute if q full, look at Q_THR
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


// Look quickly at IB buffers, signal to main thread if there is work
void umd_dma_goodput_tun_callback(struct worker *info)
{
	if(info == NULL) return;
        assert(info->ib_ptr);

	memset(info->umd_dma_rio_rx_bd_ready, 0xff, sizeof(uint32_t));

	int cnt = 0;
	volatile uint32_t* pRP = (uint32_t*)info->ib_ptr;
	assert(*pRP < info->umd_tx_buf_cnt);

	int k = *pRP;
	for (int i = 0; i < info->umd_tx_buf_cnt; i++) {
		k++;
		if(k == info->umd_tx_buf_cnt) k = 0;

		// XXX Using a C array to signal is a BAD idea as there will be race conditions
		// Better use a std::vector and a spinlock!!!
		if(0 != info->umd_dma_rio_rx_bd_L2_ptr[k]->RO) info->umd_dma_rio_rx_bd_ready[cnt++] = k;
	}
	if (cnt == 0) return;

	sem_post(&info->umd_dma_rio_rx_work);
}

void* umd_dma_wakeup_proc_thr(void* arg)
{
	if(arg == NULL) return NULL;
	struct worker* info = (struct worker*)arg;

        while (!info->stop_req) // every 100 mS which is about HZ. stupid
		usleep(100 * 1000);
	
	sem_post(&info->umd_dma_rio_rx_work);

	return NULL;
}

extern "C"
void umd_dma_goodput_tun_demo(struct worker *info)
{
        int rc = 0;
	uint64_t rx_ok = 0;
        char if_name[IFNAMSIZ] = {0};
        int flags = IFF_TUN | IFF_NO_PI;

	const int BD_PAYLOAD_SIZE = DMA_L2_SIZE + info->umd_tun_MTU; // L2 header is 6 bytes

	// Note: There's no reason to link info->umd_tx_buf_cnt other than
	// convenience. However the IB ring should never be smaller than
	// info->umd_tx_buf_cnt members
	const int IBWIN_SIZE = sizeof(uint32_t) + BD_PAYLOAD_SIZE * info->umd_tx_buf_cnt;

	assert(info->ib_ptr);
	assert(info->ib_byte_cnt >= IBWIN_SIZE);

        if (! umd_check_cpu_allocation(info)) return;
        if (! TakeLock(info, "DMA", info->umd_chan)) return;

        memset(info->umd_tun_name, 0, sizeof(info->umd_tun_name));

        // Initialize tun/tap interface
        if ((info->umd_tun_fd = tun_alloc(if_name, flags)) < 0) {
                CRIT("Error connecting to tun/tap interface %s!\n", if_name);
                delete info->umd_lock; info->umd_lock = NULL;
                return;
        }

        strncpy(info->umd_tun_name, if_name, sizeof(info->umd_tun_name)-1);

	info->umd_dma_fifo_callback = umd_dma_goodput_tun_callback;

        info->umd_dch = new DMAChannel(info->mp_num, info->umd_chan, info->mp_h);
        if (NULL == info->umd_dch) {
                CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan, info->mp_num, info->mp_h);
        	info->umd_tun_name[0] = '\0';
                close(info->umd_tun_fd); info->umd_tun_fd = -1;
                delete info->umd_lock; info->umd_lock = NULL;
                return;
        };
        info->umd_dch2 = new DMAChannel(info->mp_num, info->umd_chan2, info->mp_h);
        if (NULL == info->umd_dch2) {
                CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
                        info->umd_chan2, info->mp_num, info->mp_h);
        	info->umd_tun_name[0] = '\0';
                close(info->umd_tun_fd); info->umd_tun_fd = -1;
                delete info->umd_dch; info->umd_dch   = NULL;
                delete info->umd_lock; info->umd_lock = NULL;
                return;
        };

        char TapIPv4Addr[17] = {0};
        const uint16_t my_destid = info->umd_mch->getDestId() + DESTID_TRANSLATE;
        snprintf(TapIPv4Addr, 16, "169.254.%d.%d", (my_destid >> 8) & 0xFF, my_destid & 0xFF);

        char ifconfig_cmd[257] = {0};
        snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s %s netmask 0xffff0000 mtu %d up",
                                    if_name, TapIPv4Addr, info->umd_tun_MTU);
        const int rr = system(ifconfig_cmd);
        if(rr >> 8) {
        	info->umd_tun_name[0] = '\0';
                close(info->umd_tun_fd); info->umd_tun_fd = -1;
                delete info->umd_dch; info->umd_dch   = NULL;
                delete info->umd_dch2; info->umd_dch2 = NULL;
                delete info->umd_lock; info->umd_lock = NULL;
                return;
        }

        snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
        system(ifconfig_cmd);

	info->umd_dma_rio_rx_bd_L2_ptr = (DMA_L2_t**)calloc(1 + info->umd_tx_buf_cnt, sizeof(uint32_t));
	if (info->umd_dma_rio_rx_bd_L2_ptr == NULL) goto exit;

	info->umd_dma_rio_rx_bd_ready = (uint32_t*)calloc(1 + info->umd_tx_buf_cnt, sizeof(uint32_t));
	if (info->umd_dma_rio_rx_bd_ready == NULL) goto exit;

	info->umd_dma_rio_rx_bd_ready[info->umd_tx_buf_cnt] = 0xdeadbeefL;

	{{ // Pre-populate the locations of the RO bit in IB BDs
	  uint8_t* p = sizeof(uint32_t) + (uint8_t*)info->ib_ptr;
	  for (int i = 0; i < info->umd_tx_buf_cnt; i++, p += BD_PAYLOAD_SIZE) {
		info->umd_dma_rio_rx_bd_L2_ptr[i] = (DMA_L2_t*)p;
	  }
	}}

        socketpair(PF_LOCAL, SOCK_STREAM, 0, info->umd_sockp);

	good_destid[info->did] = true; // FMD should push updates into this

        INFO("\n\t%s %s mtu %d on DMA Chan=%d Chan2=%d my_destid=%u #buf=%d #fifo=%d\n",
             if_name, TapIPv4Addr, info->umd_tun_MTU,
             info->umd_chan, info->umd_chan2,
             info->umd_mch->getDestId(),
             info->umd_tx_buf_cnt, info->umd_sts_entries);

        if (!info->umd_dch->alloc_dmatxdesc(info->umd_tx_buf_cnt)) {
                CRIT("\n\talloc_dmatxdesc failed: bufs %d",
                                                        info->umd_tx_buf_cnt);
                goto exit;
        };
        if (!info->umd_dch->alloc_dmacompldesc(info->umd_sts_entries)) {
                CRIT("\n\talloc_dmacompldesc failed: entries %d",
                                                        info->umd_sts_entries);
                goto exit;
        };

        memset(info->dmamem, 0, sizeof(info->dmamem));
        memset(info->dmaopt, 0, sizeof(info->dmaopt));

        for (int i = 0; i < info->umd_tx_buf_cnt; i++) {
		if (!info->umd_dch->alloc_dmamem(BD_PAYLOAD_SIZE, info->dmamem[i])) {
			CRIT("\n\talloc_dmamem failed: i %d size %x",
								i, BD_PAYLOAD_SIZE);
			goto exit;
		};
		memset(info->dmamem[i].win_ptr, 0, info->dmamem[i].win_size);
	}

        info->tick_data_total = 0;
        info->tick_count = info->tick_total = 0;

        info->umd_dch->resetHw();
        info->umd_dch2->resetHw();
        if (!info->umd_dch->checkPortOK()) {
                CRIT("\n\tPort %d is not OK!!! Exiting...", info->umd_chan);
                goto exit;
        };
        if (!info->umd_dch2->checkPortOK()) {
                CRIT("\n\tPort %d is not OK!!! Exiting...", info->umd_chan2);
                goto exit;
        };

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
        };
        sem_wait(&info->umd_fifo_proc_started);

        if (!info->umd_fifo_proc_alive) {
                CRIT("\n\tumd_fifo_proc thread is dead, exiting..");
                goto exit;
        };

        // Spawn Tap Transmitter Thread
        rc = pthread_create(&info->umd_mbox_tap_thr.thr, NULL,
                            umd_dma_tun_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_tun_proc_thr thread, exiting...");
                goto exit;
        };
        sem_wait(&info->umd_mbox_tap_proc_started);

        if (!info->umd_mbox_tap_proc_alive) {
                CRIT("\n\tumd_mbox_tun_proc_thr thread is dead, exiting..");
                goto exit;
        };

        sem_wait(&info->umd_mbox_tap_proc_started);

	// We need to wake up the RX thread upon quitting so we spawn
	// this lightweight helper thread
	pthread_t wakeup_thr;
        rc = pthread_create(&wakeup_thr, NULL, umd_dma_wakeup_proc_thr, (void *)info);
        if (rc) {
                CRIT("\n\tCould not create umd_dma_wakeup_proc_thr thread, exiting...");
                goto exit;
        };

        zero_stats(info);
        info->evlog.clear();

        clock_gettime(CLOCK_MONOTONIC, &info->st_time);

	// Receiver (from RIO)

again:
	// We could double-lock a spinlock here but experimentally we use
	// a sema here.
	// To BEW: how does this impact RX latecy?
        while (!info->stop_req) {
	        volatile uint32_t* pRP = (uint32_t*)info->ib_ptr;
	        sem_wait(&info->umd_dma_rio_rx_work);

        	if (!info->stop_req) break;

		// Ingest L2 frames into Tun, update RP, set RO=0
		for (int i = 0; i < info->umd_tx_buf_cnt; i++) {
			if (info->umd_dma_rio_rx_bd_ready[i] == ~0) break;

			const int rp = info->umd_dma_rio_rx_bd_ready[i];

			DMA_L2_t* pL2 = info->umd_dma_rio_rx_bd_L2_ptr[rp];
			if(pL2->RO == 0) continue; // already processed?

			rx_ok++;
			const int payload_size = ntohl(pL2->len) - DMA_L2_SIZE;

			uint8_t* payload = (uint8_t*)pL2 + DMA_L2_SIZE;
                        const int nwrite = cwrite(info->umd_tun_fd, payload, payload_size);
#ifdef DMA_TUN_DEBUG
                        const uint32_t crc = crc32(0, payload, payload_size);
                        DBG("\n\tGot a message of size %d from RIO destid %u (L2 CRC32 0x%x) cnt=%llu, wrote %d to %s\n",
                                 ntohl(pL2->len), ntohs(pL2->destid), crc, rx_ok, nwrite, if_name);
#endif

			pL2->RO = 0;
			*pRP = rp;
		}

        } // END while NOT stop requested

	if (info->stop_req == SOFT_RESTART) {
		INFO("\n\tSoft restart requested, nuking MBOX hardware!\n");
		info->umd_mch->softRestart();
		info->stop_req = 0;
		sem_post(&info->umd_fifo_proc_started);
		sem_post(&info->umd_mbox_tap_proc_started);
		goto again;
	}

        goto exit_nomsg;
exit:
        INFO("\n\tDMA %srunning after %d %dus polls\n",
                info->umd_dch->dmaIsRunning()? "": "NOT ",
                        info->perf_msg_cnt, DMA_RUNPOLL_US);

        INFO("\n\tEXITING (FIFO iter=%lu hw RP=%u WP=%u)\n",
                info->umd_dch->m_fifo_scan_cnt,
                info->umd_dch->getFIFOReadCount(),
                info->umd_dch->getFIFOWriteCount());
exit_nomsg:
	write(info->umd_sockp[0], "X", 1); // Signal Tun/Tap thread to eXit

        info->umd_fifo_proc_must_die = 1;
        if (info->umd_dch)
                info->umd_dch->shutdown();

        pthread_join(info->umd_fifo_thr.thr, NULL);
        pthread_join(info->umd_dma_tap_thr.thr, NULL);

        info->umd_dch->get_evlog(info->evlog);
        info->umd_dch->cleanup();

        for (int i = 0; i < info->umd_tx_buf_cnt; i++) {
		if(info->dmamem[i].type != 0)
			info->umd_dch->free_dmamem(info->dmamem[i]);
	}

	info->umd_dma_fifo_callback = NULL;

        close(info->umd_sockp[0]); close(info->umd_sockp[1]);
        close(info->umd_tun_fd);
        info->umd_sockp[0] = info->umd_sockp[1] = -1;
        info->umd_tun_fd = -1;

	free(info->umd_dma_rio_rx_bd_ready);  info->umd_dma_rio_rx_bd_ready = NULL;
	free(info->umd_dma_rio_rx_bd_L2_ptr); info->umd_dma_rio_rx_bd_L2_ptr = NULL;

        delete info->umd_dch;  info->umd_dch = NULL;
        delete info->umd_dch2; info->umd_dch2 = NULL;
        delete info->umd_lock; info->umd_lock = NULL;
        info->umd_tun_name[0] = '\0';
}
