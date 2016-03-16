/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "liblog.h"

#include "mboxmgr.h"

struct worker {
  volatile int stop_req;	/* 0 - continue, 1 - stop */

  int did;			/* destID */

  uint64_t byte_cnt;		/* Number of bytes to access for direct IO and DMA */
  uint64_t acc_size;		/* Bytes per transfer for direct IO and DMA */
  int wr;
  int mp_num;			/* Mport index */

  int             umd_chan; ///< Local mailbox OR DMA channel
  //int             umd_chan_to; ///< Remote mailbox
  //int             umd_letter; ///< Remote mailbox letter
  MboxChannelMgr* umd_mch;

  uint32_t        umd_dma_abort_reason;

  int             umd_sts_entries;
  int             umd_tx_buf_cnt;
};

static void init_worker_info(struct worker *info)
{
  memset(info, 0, sizeof(*info));

  info->did = -1;
  info->umd_chan = -1;
  info->acc_size = 256;
}

static inline int MIN(int a, int b) { return a < b ? a : b; }

void umd_mbox_latency_demo(struct worker *info)
{
  info->umd_mch = new MboxChannelMgr(info->mp_num, info->umd_chan);
  if (NULL == info->umd_mch) {
    CRIT("\n\tMboxChannel alloc FAIL: chan %d mp_num %d", info->umd_chan, info->mp_num);
    return;
  };

  if (info->umd_mch->open_mbox(info->umd_tx_buf_cnt, info->umd_sts_entries)){
    CRIT("\n\tMboxChannel: Failed to open mbox!");
    delete info->umd_mch;
    return;
  }

  uint64_t big_cnt = 0;		// how may attempts to TX a packet
  uint64_t rx_ok = 0, tx_ok = 0;

  const int Q_THR = (2 * info->umd_tx_buf_cnt) / 3;

  INFO("\n\tMBOX %s my_destid=%u destid=%u acc_size=%d #buf=%d #fifo=%d\n",
       (info->wr? "Master": "Slave"),
       info->umd_mch->getDestId(), info->did,
       info->acc_size,
       info->umd_tx_buf_cnt, info->umd_sts_entries);

  MboxChannel::WorkItem_t wi[info->umd_sts_entries*8]; memset(wi, 0, sizeof(wi));

// Slave/Receiver
  if (info->wr == 0) {
    char msg_buf[MboxChannelMgr::PAGE_4K + 1] = { 0 };
#ifndef MBOXDEBUG
    strncpy(msg_buf, "Generic pingback - Mary had a little lamb", MboxChannelMgr::PAGE_4K);
#endif

    MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));
    MboxChannel::MboxOptions_t opt_in; memset(&opt, 0, sizeof(opt_in));

    opt.mbox = info->umd_chan;
    opt.destid = info->did;

    while (!info->stop_req) {
      bool q_was_full = false;
      info->umd_dma_abort_reason = 0;
      uint64_t rx_ts = 0;

      while (!info->stop_req && !info->umd_mch->inb_message_ready(rx_ts)) {;}

      if (info->stop_req) break;

      opt.ts_end = rx_ts;

      bool rx_buf = false;
      void *buf = NULL;
      if ((buf = info->umd_mch->get_inb_message(opt_in)) != NULL) {
	rx_ok++;
	rx_buf = true;
#ifdef MBOXDEBUG
	memcpy(msg_buf, buf, MIN(MboxChannelMgr::PAGE_4K, opt_in.bcount));
#endif
	info->umd_mch->add_inb_buffer(buf);	// recycle
#ifdef MBOXDEBUG
	DBG("\n\tGot a message of size %d [%s] from destid %u mbox %u cnt=%llu\n", opt_in.bcount, msg_buf, opt_in.destid, opt_in.mbox, rx_ok);
#endif
      }
      if (!rx_buf) {
	ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, rx_ok);
	goto exit_rx;
      }

      if (info->stop_req) break;

      // Echo message back
      MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;
      if (!info->umd_mch->send_message(opt, msg_buf, opt_in.bcount, false, fail_reason)) {
	if (fail_reason == MboxChannel::STOP_REG_ERR) {
	  ERR("\n\tsend_message FAILED!\n");
	  goto exit_rx;
	} else { q_was_full = true; }
      } else { tx_ok++; }

      if (info->stop_req) break;

      if (q_was_full) INFO("\n\tQueue full for MBOX%d! rx_ok=%llu\n", info->umd_chan, rx_ok);

      // Busy-wait for queue to drain
      for (uint64_t iq = 0; !info->stop_req && q_was_full && (iq < 1000000000) && (info->umd_mch->queueTxSize() >= Q_THR); iq++) {;}

      DDBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
      while (!q_was_full && !info->stop_req && info->umd_mch->scanFIFO(wi, info->umd_sts_entries * 8) == 0) {;}
    } // END infinite loop

    goto exit_rx;
  }				// END Receiver

// Master/Transmitter
  while (!info->stop_req) {
    info->umd_dma_abort_reason = 0;

    // TX Loop
    MboxChannel::MboxOptions_t opt; memset(&opt, 0, sizeof(opt));

    opt.destid = info->did;
    opt.mbox = info->umd_chan;
    char str[MboxChannelMgr::PAGE_4K + 1] = { 0 };
    for (int cnt = 0; !info->stop_req; cnt++) {
      bool q_was_full = false;
      MboxChannel::StopTx_t fail_reason = MboxChannel::STOP_OK;

      snprintf(str, 128, "Mary had a little lamb iter %d\x0", cnt);

      const bool first_message = !big_cnt;

      // We check status reg for 1st ever message to make sure destid is sane, etc.
      if (!info->umd_mch->send_message(opt, str, info->acc_size, first_message, fail_reason)) {
	if (fail_reason == MboxChannel::STOP_REG_ERR) {
	  ERR("\n\tsend_message FAILED!\n");
	  goto exit;
	} else { q_was_full = true; }
      } else { tx_ok++; }

      if (info->stop_req) break;

      if (q_was_full) INFO("\n\tQueue full for MBOX%d! tx_ok=%llu\n", info->umd_chan, tx_ok);

      // Busy-wait for queue to drain
      for (uint64_t iq = 0; !info->stop_req && q_was_full && (iq < 1000000000) && (info->umd_mch->queueTxSize() >= Q_THR); iq++) {;}

      DDBG("\n\tPolling FIFO transfer completion destid=%d\n", info->did);
      while (!q_was_full && !info->stop_req && info->umd_mch->scanFIFO(wi, info->umd_sts_entries * 8) == 0) {;}

      // Wait from echo from Slave
      uint64_t rx_ts = 0;
      while (!info->stop_req && !info->umd_mch->inb_message_ready(rx_ts)) {;}

      if (info->stop_req) break;

      bool rx_buf = false;
      void *buf = NULL;
      while ((buf = info->umd_mch->get_inb_message(opt)) != NULL) {
	rx_ok++;
	rx_buf = true;
	DDBG("\n\tGot a message of size %d [%s] cnt=%llu\n", opt.bcount, buf, tx_ok);
	info->umd_mch->add_inb_buffer(buf);	// recycle
      }
      if (!rx_buf) {
	ERR("\n\tRX ring in unholy state for MBOX%d! cnt=%llu\n", info->umd_chan, tx_ok);
	goto exit;
      }

      big_cnt++;
    }
  } // END while NOT stop requested

exit:
exit_rx:
  delete info->umd_mch;  info->umd_mch = NULL;
}

static struct worker info;

static void sig_handler(int signo)
{
  info.stop_req = 1; 
  INFO("\n\tQuitting time (sig=%d)\n", signo);
}

static void usage(const char* name)
{
  fprintf(stderr, "usage: %s [-M] -d <destid> -m <mportid> -c <chan> -b <buf> -s <sts> [-A <acc_size>]\n" \
"\t\tM       - Function as Master (TX) [requires -s], not present function as Slave (RX)\n" \
"\t\tdestid  - RapidIO destid (8-bit)\n" \
"\t\tmportid - MPORT aka RapidIO PICe index -- usually 0\n" \
"\t\tchan    - MBOX 2 or MBOX 3\n" \
"\t\tbuf     - buffer descriptor count, min 32, power-of-2, base10\n" \
"\t\tsts     - TX FIFO size, min 32, power-of-2, base10\n",
          name);
  exit(0);
}

int main(int argc, char* argv[])
{
  if(argc == 1) usage(0 [argv]);

  init_worker_info(&info);

  rdma_log_init("mboxsample.txt", 0);

  int c;
  opterr = 0;
  while ((c = getopt (argc, argv, "hMm:d:A:b:s:c:")) != -1)
    switch (c) {
      case 'h': usage(0 [argv]); break;
      case 'M': info.wr = 1; break; // Master
      case 'm': info.mp_num   = atoi(optarg); break;
      case 'd': info.did      = atoi(optarg); break;
      case 'A': info.acc_size = atoi(optarg); break;
      case 'b': info.umd_tx_buf_cnt  = atoi(optarg); break;
      case 's': info.umd_sts_entries = atoi(optarg); break;
      case 'c': info.umd_chan = atoi(optarg); break;
      default: ERR("\n\tUnknown argument '%c'. Bye!", c); return 1; break;
    };

  assert(info.mp_num >= 0); 
  assert(info.umd_chan == 2 || info.umd_chan == 3);
  assert(info.umd_tx_buf_cnt >= 0x20);
  assert(info.umd_sts_entries >= 0x20);

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  
  umd_mbox_latency_demo(&info);

  INFO("\n\tCe ne'est pas un Abandon du Travail. Vive le RapidIO ;-)\n");

  return 0;
}
