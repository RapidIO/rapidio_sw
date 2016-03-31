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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>

#include "liblog.h"
#include "mboxmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOT_STS(x) (x->sts_entries * 8)
#define FOUR_KB (4 * 1024)
#define FOUR_KB_1 (FOUR_KB+1)

#define OPTIONS_STR "hm:d:c:C:s:b:f:"

#ifdef MBOX_CLIENT
#define HELP_STR \
	"usage: \n" \
	"sudo ./client -m <mp> -d <did> -c <ch> -C <CH> -s <sz> -b <tx> -f <fin>\n" \
		"mp  - MPORT aka device index -- usually 0\n" \
		"did - RapidIO destid (8-bit) where the server is running\n" \
		"ch  - MBOX channel for mp, use 2 (default) or 3\n" \
		"CH  - MBOX channel for the server, use 2 (default) or 3\n" \
		"sz  - Size of messages exchanged, minimum 8, maximum 4096\n" \
		"tx  - Maximum messages pending for TX/RX, minimum 32.\n" \
		"      Must be a power of 2, maximum is 4096\n" \
		"fin - Maximum number of messages finished TX/RX.\n" \
		"      Minimum 32, Must be a power of 2, maximum is 4096\n"

#define CLIENT 1
#define LOG_FILE_NAME "mbox_client.txt"
#else
#define HELP_STR \
	"usage:\n" \
	"sudo ./server -m <mp> -d <did> -c <ch> -C <CH> -b <tx> -f <fin>\n" \
		"mp  - MPORT aka device index -- usually 0\n" \
		"did - RapidIO destid (8-bit) where the client is running\n" \
		"ch  - MBOX channel for mp, values of 2 (default) or 3\n" \
		"CH  - MBOX channel for the client, use 2 (default) or 3\n" \
		"tx  - Maximum messages pending for TX/RX, minimum 32.\n" \
		"      More messages means more throughput\n" \
		"fin - Maximum number of messages finished TX/RX.\n" \
		"      Minimum 32, must be a power of 2, maximum is 4096\n" \

#define CLIENT 0
#define LOG_FILE_NAME "mbox_server.txt"
#endif

struct worker {
	volatile int stop_req;	/* 0 - continue, 1 - stop */
	int mp_num;   /* Mport index */
	int mbox;     ///< Local mailbox OR DMA channel
	int tgt_did;  /* target destID */
	int tgt_mbox; ///< Local mailbox OR DMA channel
	uint64_t msg_sz;	/* Bytes per transfer for direct IO and DMA */
	MboxChannelMgr* mch;
	uint32_t	abort_reason;
	int		sts_entries;
	int		tx_buf_cnt;
	MboxChannel::MboxOptions_t opt; 
	MboxChannel::MboxOptions_t rx_opt; 
	MboxChannel::WorkItem_t *wi;

	bool q_full;
	uint64_t tx_cnt;
	uint64_t rx_cnt;

	char tx_msg[FOUR_KB_1];
	char rx_msg[FOUR_KB_1];
};

struct worker info;

static void init_worker_info(struct worker *info)
{
	memset(info, 0, sizeof(*info));

	info->stop_req = 0;
	info->mp_num = 0;
	info->mbox = 2;
	info->tgt_did = -1;
	info->tgt_mbox = 2;
	info->msg_sz = 4096;
	info->mch = NULL;
	info->abort_reason = 0;
	info->sts_entries = 0x100;
	info->tx_buf_cnt = 0x100;
	memset(&info->opt, 0, sizeof(info->opt));
	memset(&info->rx_opt, 0, sizeof(info->rx_opt));
	info->wi = NULL;
	info->q_full = false;
	info->tx_cnt = 0;
	info->rx_cnt = 0;
}

static inline int MIN(int a, int b) { return a < b ? a : b; }

bool setup_mailbox(struct worker *info)
{
	info->mch = new MboxChannelMgr(info->mp_num, info->mbox);
	if (NULL == info->mch) {
		CRIT("\n\tCould not create MBox Manager: mp_num %d chan %d",
			info->mbox, info->mp_num);
		return true;
	};

	if (!info->mch->open_mbox(info->tx_buf_cnt, info->sts_entries)){
		CRIT("\n\tCould not open MBOX!");
		delete info->mch;
		return true;
	}

	INFO("\n\t%s my_did=%u my_mbox=%d tgt_did=%u tgt_mbox=%d\n\t"
		" msg_sz=%d #buf=%d #fifo=%d\n",
			CLIENT?"CLIENT":"SERVER",
			 info->mch->getDestId(), info->mbox,
			 info->tgt_did, info->tgt_mbox,
			 info->msg_sz,
			 info->tx_buf_cnt, info->sts_entries);

	info->wi = (MboxChannel::WorkItem_t*)
		malloc(sizeof(MboxChannel::WorkItem_t) * TOT_STS(info));
	memset(info->wi, 0, sizeof(MboxChannel::WorkItem_t) * TOT_STS(info));

	return false;
};

#define MSG_FORMAT "%2d %2d %8x" 

void format_tx_msg(struct worker *info, uint32_t seq)
{
	snprintf(info->tx_msg, 16, MSG_FORMAT, info->mch->getDestId(),
							info->mbox, seq);
	memset(&info->tx_msg[16], (char)seq, FOUR_KB_1-16);
};

bool  check_server_resp(struct worker *info) 
{
	uint8_t rx_did, rx_mbox;
	uint32_t rx_seq;
	bool matched = true;

	sscanf(info->rx_msg, MSG_FORMAT, &rx_did, &rx_mbox, &rx_seq);
	if (info->mch->getDestId() != rx_did)
		matched = false;
	if (info->mbox != rx_mbox)
		matched = false;
	if ((info->rx_cnt) != rx_seq)
		matched = false;

	if (!matched)
		CRIT("\nMismatch: DID %1d %1d MBOX %1d %1d SEQ %8x %8x",
			info->mch->getDestId(), rx_did, info->mbox, rx_mbox,
			info->rx_cnt, rx_seq);
	return !matched;
};

bool send_mbox_msg(struct worker *info, bool msg_1)
{
	MboxChannel::StopTx_t rc = MboxChannel::STOP_OK;
	bool no_space;
	const int Q_THR = (2 * info->tx_buf_cnt) / 3;

	info->q_full = false;

	if (!info->mch->send_message(info->opt, info->tx_msg,
						info->msg_sz, msg_1, rc)) {
		if (rc == MboxChannel::STOP_REG_ERR) {
			ERR("\n\tsend_message FAILED!\n");
			return true;	
		} 
		info->q_full = true;
	};

	/* Busy-wait for queue to drain
	 * There are more efficient alternative implementations.
	 */
	no_space = !info->stop_req && info->q_full &&
		(info->mch->queueTxSize() >= Q_THR);

	for (uint32_t dlay = 0; no_space && (dlay < 1000000000); dlay++) {
		no_space = !info->stop_req && info->q_full &&
			(info->mch->queueTxSize() >= Q_THR);
	};

	/* If a message was added, wait until at least one  message 
	 * has been transmitted, less than 2 microseconds.
	 *
	 * For practical applications, FIFO management can be performed
	 * in a separate thread.
 	 */
	while (!info->q_full && !info->stop_req && 
		info->mch->scanFIFO(info->wi, TOT_STS(info)) == 0) {
		// Do Nothing;
	};

	return false;
}

bool copy_msg_to_tx(struct worker *info)
{
	int did, mbox, seq;

	memcpy(info->tx_msg, info->rx_msg,MIN(info->rx_opt.bcount, FOUR_KB)); 
	sscanf(info->tx_msg, MSG_FORMAT, &did, &mbox, &seq);
	info->opt.bcount = MIN(info->rx_opt.bcount, FOUR_KB);
	info->opt.destid = did;
	info->opt.mbox = mbox;

	return info->rx_opt.destid != did;
};

bool recv_mbox_msg(struct worker *info)
{
	uint64_t rx_ts = 0;
	bool rx_buf = false;
	void *buf = NULL;

	while (!info->stop_req && !info->mch->inb_message_ready(rx_ts)) {
		// Busy wait until a message is received
	}

	if (info->stop_req)
		return false;

	while ((buf = info->mch->get_inb_message(info->rx_opt)) != NULL) {
		info->rx_cnt++;
		rx_buf = true;
		memcpy(info->rx_msg, buf, MIN(info->rx_opt.bcount, FOUR_KB));
		info->rx_msg[FOUR_KB] = '\0';
		// Return the buffer to the receive buffer pool!
		info->mch->add_inb_buffer(buf);	
	}

	if (!rx_buf) {
		ERR("\n\tCould not receive message for MBOX%d! cnt=%llu\n",
			info->mbox, info->tx_cnt);
	}
	return rx_buf;
};

void mbox_client(struct worker *info)
{
	uint64_t rx_ok = 0, tx_ok = 0;

	if (setup_mailbox(info))
		return;

	info->abort_reason = 0;

	// TX Loop
	info->opt.destid = info->tgt_did;
	info->opt.mbox = info->tgt_mbox;
	info->opt.bcount = info->msg_sz;

	for (uint64_t cnt = 0; !info->stop_req; cnt++) {
		bool q_full = false;
		bool msg_1 = !cnt;
		uint64_t dlay;
		bool tx_ok;

		format_tx_msg(info, cnt + 1);
		if (send_mbox_msg(info, msg_1) && !info->q_full)
			goto exit;

		if (info->stop_req)
			break;

		if (info->q_full) {
			sleep(0);
			continue;
		};

		// Wait for echo from Server
		if (!recv_mbox_msg(info))
			goto exit;

		if (check_server_resp(info))
			goto exit;
		if (!(cnt & 0xFFFFF)) {
			INFO("Exchanged 0x%16llx messages\n", cnt);
		}
	}
exit:
	delete info->mch;
	info->mch = NULL;
}

void mbox_server(struct worker *info)
{
	uint64_t rx_ok = 0, tx_ok = 0, big_cnt = 0;

	const int Q_THR = (2 * info->tx_buf_cnt) / 3;

	if (setup_mailbox(info))
		return;

	while (!info->stop_req) {
		bool msg_1 = !big_cnt;
		uint64_t dlay;
		bool tx_ok;

		info->q_full = false;
		info->abort_reason = 0;

		// Poll for a message...
		while (!recv_mbox_msg(info) && !info->stop_req) {
		};

		copy_msg_to_tx(info);

		for (int cnt = 0; !info->stop_req; cnt++) {
			if (send_mbox_msg(info, msg_1) && !info->q_full)
				goto exit;

			if (info->stop_req)
				break;

			if (info->q_full) {
				sleep(0);
				continue;
			};
			break;
		}
		big_cnt++;
	}
exit:
	delete info->mch;
	info->mch = NULL;
}

static void sig_handler(int signo)
{
	info.stop_req = 1; 
	INFO("\n\tQuitting time (sig=%d)\n", signo);
}


static void usage_and_exit()
{
	fprintf(stderr, HELP_STR);
	exit(0);
}


int parse_options(struct worker *info, int argc, char* argv[])
{
	int c;
	opterr = 0;

	while ((c = getopt (argc, argv, OPTIONS_STR)) != -1) {
		switch (c) {
			case 'h': usage_and_exit(); 
				break;
			case 'm': info->mp_num = atoi(optarg);
				if (info->mp_num < 0)
					usage_and_exit();
				break;
			case 'd': info->tgt_did = atoi(optarg);
				if (info->tgt_did < 0)
					usage_and_exit();
				break;
			case 'c': info->mbox = atoi(optarg);
				if ((info->mbox < 2) || (info->mbox > 3)) 
					usage_and_exit();
				break;
			case 'C': info->tgt_mbox = atoi(optarg);
				if ((info->tgt_mbox < 2) || (info->tgt_mbox > 3)) 
					usage_and_exit();
				break;
			case 's': info->msg_sz = atoi(optarg);
				if ((info->msg_sz < 8) || (info->msg_sz > 4096)) 
					usage_and_exit();
				break;
			case 'b': info->tx_buf_cnt = atoi(optarg);
				if ((info->tx_buf_cnt < 32) ||
					(info->tx_buf_cnt > 4096)) 
					usage_and_exit();
				break;
			case 'f': info->sts_entries = atoi(optarg);
				if ((info->sts_entries < 32) ||
					(info->sts_entries > 4096)) 
					usage_and_exit();
				break;
			default: ERR("\n\tUnknown argument '%c'.\n", c);
				usage_and_exit();
				break;
		};
	};
};

int main(int argc, char* argv[])
{
	init_worker_info(&info);

	rdma_log_init(LOG_FILE_NAME, 0);

	parse_options(&info, argc, argv);

	// rdma_log_init("mboxserver.txt", 0);

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	
	if (CLIENT)
		mbox_client(&info);
	else
		mbox_server(&info);

	return 0;
}

#ifdef __cplusplus
}
#endif
