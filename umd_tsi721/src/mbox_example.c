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

/**
 * \file mbox_sample.c
 * \brief Example code for the Tsi721 Mailbox User Mode Driver 
 *
 * This file implements an example client and server.  
 * The client and server have exclusive use of Tsi721 mailbox 
 * transmit and receive queues.
 *
 * The client sends
 * messages of a defined format and user specified length to the server.
 * The server 
 * parses the contents of the message and sends the message back to the
 * client.  The client then checks the message contents.  This loop 
 * repeats forever.  The client display status every 0x100000
 * (1M) messages.  
 *
 * To halt the server or client, use <CTRL>C.
 *
 * See the definitions of HELP_STR below for the parameters accepted
 * by the client and server.
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
#define MIN_MSG_SIZE 24

#define HACK(x) #x
#define STR(x) HACK(x)

#ifdef MBOX_CLIENT
#define OPTIONS_STR "hm:d:c:C:s:b:f:"
#define HELP_STR \
	"usage: \n" \
	"sudo ./client -m <mp> -d <did> -c <ch> -C <CH> -s <sz> -b <tx> -f <fin>\n" \
		"mp  - MPORT aka device index -- usually 0\n" \
		"did - RapidIO destid (8-bit) where the server is running\n" \
		"ch  - MBOX channel for mp, use 2 (default) or 3\n" \
		"CH  - MBOX channel for the server, use 2 (default) or 3\n" \
		"sz  - Size of messages exchanged, minimum " STR(MIN_MSG_SIZE) \
			", maximum 4096\n" \
		"tx  - Maximum messages pending for TX/RX, minimum 32.\n" \
		"      Must be a power of 2, maximum is 4096\n" \
		"fin - Maximum number of messages finished TX/RX.\n" \
		"      Minimum 32, Must be a power of 2, maximum is 4096\n"

#define CLIENT 1
#define LOG_FILE_NAME "mbox_client.txt"
#define MAX_RX_MSG 1
#else
#define OPTIONS_STR "hm:c:b:f:"
#define HELP_STR \
	"usage:\n" \
	"sudo ./server -m <mp> -c <ch> -b <tx> -f <fin>\n" \
		"mp  - MPORT aka device index -- usually 0\n" \
		"ch  - MBOX channel for mp, values of 2 (default) or 3\n" \
		"tx  - Maximum messages pending for TX/RX, minimum 32.\n" \
		"      More messages means more throughput\n" \
		"fin - Maximum number of messages finished TX/RX.\n" \
		"      Minimum 32, must be a power of 2, maximum is 4096\n" \

#define CLIENT 0
#define LOG_FILE_NAME "mbox_server.txt"
#define MAX_RX_MSG 25
#endif


struct worker {
	volatile int stop_req;	///< Set by signal handler to halt execution
	int mp_num;   ///< Mport index to use, usually 0 i.e. /dev/mport0
	int mbox;     ///< Local mailbox 
	int tgt_did;  ///< Server Destination ID 
	int tgt_mbox; ///< Server Mailbox number, either 2 or 3
	uint64_t msg_sz; ///< Message size, multiple of 8, 24 <= msg_sz <= 4096
	MboxChannelMgr* mch; ///< User Mode Driver Mailbox object
	uint32_t	abort_reason; ///< Reason for hardware failure
	int		sts_entries; ///< Number mailbox completions
	int		tx_buf_cnt; ///< Number of mailbox transmit buffers
	MboxChannel::MboxOptions_t opt;  ///< Parameters for message TX
	MboxChannel::MboxOptions_t rx_opt; ///< Parameters from message RX
	MboxChannel::WorkItem_t *wi; ///< User Mode Driver work items
	void * buf[MAX_RX_MSG]; ///< Pointers to received message buffers
	int  buf_cnt; ///< Number of valid buf[] entries

	bool q_full; ///< Indicate that the transmission queue is full.
	uint64_t tx_cnt; ///< Count of transmitted messages
	uint64_t rx_cnt; ///< Count of received messages

	char tx_msg[FOUR_KB_1]; ///< Message to be sent.
};

/** \brief Global variable containing all driver state information
 */
struct worker info;

/**
 * \brief Called by main() to initialize the "info" global.
 *
 * \param[out] info State information to be initialized.
 *
 * \retval None
 */
static void init_worker_info(struct worker *info)
{
	memset(info, 0, sizeof(*info));

	info->mbox = 2;
	info->tgt_did = -1;
	info->tgt_mbox = 2;
	info->msg_sz = 4096;
	info->mch = NULL;
	info->sts_entries = 0x100;
	info->tx_buf_cnt = 0x100;
	info->wi = NULL;
	info->q_full = false;
}

static inline int MIN(int a, int b) { return a < b ? a : b; }

/**
 * \brief Called by main() to create the User Mode Driver Mailbox object
 *
 * \param[inout] info info->mbox determines the mailbox channel owned by
 *                    the object
 *
 * \retval false on success
 */
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

#define MSG_FORMAT "%2d %2d %8x %4x" 

/**
 * \brief Called by mbox_client() to format the next message for transmission
 *
 * \param[in] seq indicates the sequence number for this message
 * \param[inout] info info->tx_msg is updated with the new message contents.  
 *
 * \retval None
 */
void format_tx_msg(struct worker *info, uint32_t seq)
{
	snprintf(info->tx_msg, MIN_MSG_SIZE, MSG_FORMAT, info->mch->getDestId(),
		info->mbox, seq, info->msg_sz);
	memset(&info->tx_msg[MIN_MSG_SIZE], (char)seq, FOUR_KB_1-MIN_MSG_SIZE);
};

/**
 * \brief Called by mbox_client() to check the response message is correct
 *
 * \param[inout] info info->buf[0] is checked to confirm it matches the last 
 * message transmitted.
 *
 * \retval false means the message was correct, true means there was an issue
 *
 * Each message sent by the client is checked to confirm correctness.
 * After the buffer is checked, it is returned to the User Mode Driver object
 * to be used to receive future messages.
 */

bool  check_server_resp(struct worker *info) 
{
	uint8_t rx_did, rx_mbox;
	uint32_t rx_seq, msg_sz;
	int parms;
	bool matched = true;
	int i;

	for (i = 0; i < info->buf_cnt; i++) {
		info->rx_cnt++;
		parms = sscanf((char *)info->buf[i], MSG_FORMAT,
					&rx_did, &rx_mbox, &rx_seq, &msg_sz);
		if (4 != parms)
			matched = false;
		if (info->mch->getDestId() != rx_did)
			matched = false;
		if (info->mbox != rx_mbox)
			matched = false;
		if ((info->rx_cnt) != rx_seq)
			matched = false;
		if ((info->msg_sz) != msg_sz)
			matched = false;

		if (!matched)
			CRIT("\nMismatch: DID %1d %1d MBOX %1d %1d SEQ %8x %8x SZ %4x %4x\n",
				info->mch->getDestId(), rx_did,
				info->mbox, rx_mbox,
				info->rx_cnt, rx_seq,
				info->msg_sz, msg_sz);

		/* RX buffer processing complete, return it to the queue. */
		info->mch->add_inb_buffer(info->buf[i]);	
	};

	return !matched;
};

/**
 * \brief Called to send a message.
 *
 * \param[in] info info->tx_msg is sent to the device specified by info->opt
 * \param[in] msg_1 used to ensure hardware state is checked after the first
 *                 attempt to send a message.
 *
 * \retval false means the message was sent, true means hardware failure   
 *
 * This routine currently requires each message sent to complete transmission
 * before another message is sent.  This unnecessarily limits throughput,
 * but is easy example code.
 */

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

/**
 * \brief Called by mbox_server to copy the next received message into
 *        info->tx_msg.
 *
 * \param[in] i Index of info->buf[] to process
 * \param[inout] info info->tx_msg is updated with a copy of the received 
 *        message in info->buf[i].  info->opt is updated with the coordinates 
 *        of the client which sent the message.
 *
 * \retval false means the message was sent, true means bad message format/vals
 *
 */

bool copy_buf_to_tx(struct worker *info, int i)
{
	int did = 0, mbox = 0, seq = 0, msg_sz = 0, parms;

	memcpy(info->tx_msg, info->buf[i], FOUR_KB); 

	parms = sscanf(info->tx_msg, MSG_FORMAT, &did, &mbox, &seq, &msg_sz);
	if (4 != parms) {
		ERR("\n\tParsed %d parms, expected 4. FAIL.\n", parms);
		goto fail;
	}
	if ((2 != mbox) && (3 != mbox)) {
		ERR("\n\tIllegal mbox value of %d. FAIL.\n", mbox);
		goto fail;
	}

	info->opt.bcount = MIN(msg_sz, FOUR_KB);
	info->opt.destid = did;
	info->opt.mbox = mbox;

	/* RX buffer processing complete, return it to the queue. */
	info->mch->add_inb_buffer(info->buf[i]);	

	return false;
fail:
	return true;
};

/**
 * \brief Called to receive a message.
 *
 * \param[inout] info info->buf[] is updated with pointers to the 
 *        User Mode Driver object buffers which contain received messages. 
 *        info->buf_cnt is updated with the number of received messages.
 *
 * \retval The number of messages received.
 *
 */

bool recv_mbox_msg(struct worker *info)
{
	uint64_t rx_ts = 0;
	int  rx_cnt = 0;
	void *buf = NULL;

	// Busy wait until at least one message is ready
	while (!info->stop_req && !info->mch->inb_message_ready(rx_ts)) {
	}

	if (info->stop_req)
		return false;

	while (rx_cnt < MAX_RX_MSG)  {
		info->buf[rx_cnt] = info->mch->get_inb_message(info->rx_opt);
		if (info->buf[rx_cnt] == NULL) 
			break;
		rx_cnt++;
	}

	if (!rx_cnt) {
		ERR("\n\tCould not receive message for MBOX%d! cnt=%llu\n",
			info->mbox, info->tx_cnt);
	}
	info->buf_cnt = rx_cnt;
	return rx_cnt;
};

/**
 * \brief Mailbox client implementation.
 *
 * \param[in] info Information used by client to send/receive messages.
 *
 * \retval None.
 *
 * The client implements an endless loop sending and receiving messages.
 *
 * Performs the following steps:
 *
 */

void mbox_client(struct worker *info)
{
	uint64_t rx_ok = 0, tx_ok = 0;

	/** - Initialize User Mode Driver Mailbox object */
	if (setup_mailbox(info))
		return;

	info->abort_reason = 0;

	/** - Configure coordinates of the server for message transmission */
	info->opt.destid = info->tgt_did;
	info->opt.mbox = info->tgt_mbox;
	info->opt.bcount = info->msg_sz;

	/** - Loop forever */
	for (uint64_t cnt = 0; !info->stop_req; cnt++) {
		bool q_full = false;
		bool msg_1 = !cnt;
		uint64_t dlay;
		bool tx_ok;
	/** - Format and send the next message in sequence. */

		format_tx_msg(info, cnt + 1);
		if (send_mbox_msg(info, msg_1) && !info->q_full)
			goto exit;

		if (info->stop_req)
			break;

		if (info->q_full) {
			sleep(0);
			continue;
		};

	/** - Receive and check the server response message */
		if (!recv_mbox_msg(info))
			goto exit;

		if (check_server_resp(info))
			goto exit;
		if (!(cnt & 0xFFFFF)) {
			INFO("Exchanged 0x%16llx messages\n", cnt);
		}
	}
exit:
	/** - On exit, cleanup the User Mode Driver Mailbox object */
	delete info->mch;
	info->mch = NULL;
}

/**
 * \brief Mailbox server implementation.
 *
 * \param[in] info Information used by server to send/receive messages.
 *
 * \retval None.
 *
 * The server implements an endless loop receiving and sending messages.
 *
 * Performs the following steps:
 *
 */

void mbox_server(struct worker *info)
{
	uint64_t rx_ok = 0, tx_ok = 0, big_cnt = 0;

	const int Q_THR = (2 * info->tx_buf_cnt) / 3;

	/** - Initialize User Mode Driver Mailbox object */
	if (setup_mailbox(info))
		return;

	/** - Loop forever */
	while (!info->stop_req) {
		bool msg_1 = !big_cnt;
		uint64_t dlay;
		bool tx_ok;

		info->q_full = false;
		info->abort_reason = 0;

	/** - Receive one or more messages */
		while (!recv_mbox_msg(info) && !info->stop_req) {
		};

	/** - For each message received */
		for (int i = 0; i < info->buf_cnt; i++) {
	/** - Copy the message to the transmit buffer and check the format */
			copy_buf_to_tx(info, i);
			for (int cnt = 0; !info->stop_req; cnt++) {
	/** - Send the message back to the client that sent it */
				if (send_mbox_msg(info, msg_1) && !info->q_full)
					goto exit;

				if (info->stop_req)
					break;

				if (info->q_full) {
					sched_yield();
					continue;
				};
				break;
			}
			big_cnt++;
		}
	}
exit:
	/** - On exit, cleanup the User Mode Driver Mailbox object */
	delete info->mch;
	info->mch = NULL;
}

/**
 * \brief Signal handler
 *
 * \param[in] Signal number received.
 *
 * \retval None.
 *
 * The signal handler is invoked whenever a signal (ie control C) is received
 * by the client or server.  This should cause the client and server to exit. 
 *
 */

static void sig_handler(int signo)
{
	info.stop_req = 1; 
	INFO("\n\tQuitting time (sig=%d)\n", signo);
}


/**
 * \brief Called by main to display parameters/usage information and exit
 *
 */

static void usage_and_exit()
{
	fprintf(stderr, HELP_STR);
	exit(0);
}

/**
 * \brief Parse command line parameters and initialize info structure.
 *
 * \param[in] info Structure containing configuration information.
 * \param[in] argc Command line parameter count
 * \param[in] argv Array of pointers to command line parameter null terminated
                   strings
 *
 * \retval 0 means success
 *
 * Has logic for both server and client command line parameters, but only
 * the parameters defined in the server/client specific OPTIONS_STR 
 * constant ares upported.
 *
 * Displays usage information when -h is entered.
 *
 */

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

/**
 * \brief Starting poitn for both server and client
 *
 * \param[in] argc Command line parameter count
 * \param[in] argv Array of pointers to command line parameter null terminated
                   strings
 *
 * \retval 0 means success
 *
 *  Performs the following steps:
 */

int main(int argc, char* argv[])
{
	/** - Initializes all variables */
	init_worker_info(&info);

	/** - Opens log file for record keeping */
	rdma_log_init(LOG_FILE_NAME, 0);

	/** - Parses command line options */
	parse_options(&info, argc, argv);

	/** - Binds signal handlers */
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	
	/** - Invokes client or server loops */
	if (CLIENT)
		mbox_client(&info);
	else
		mbox_server(&info);

	return 0;
}

#ifdef __cplusplus
}
#endif
