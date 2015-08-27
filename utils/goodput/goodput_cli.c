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

#include "goodput_cli.h"
#include "time_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

char *req_type_str[(int)last_action+1] = {
	(char *)"NO_ACT",
	(char *)"DIO",
	(char *)"ioTlat",
	(char *)"ioRlat",
	(char *)"DMA",
	(char *)"dT_Lat",
	(char *)"dR_Lat",
	(char *)"MSG_Tx",
	(char *)"mT_Lat",
	(char *)"MSG_Rx",
	(char *)"mR_Lat",
	(char *)" IBWIN",
	(char *)"~IBWIN",
	(char *)"SHTDWN",
	(char *)"LAST"
};

#define ACTION_STR(x) (char *)((x < last_action)?req_type_str[x]:"UNKWN!")
#define MODE_STR(x) (char *)((x == kernel_action)?"KRNL":"User")
#define THREAD_STR(x) (char *)((0 == x)?"---":((1 == x)?"Run":"Hlt"))

int StartCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, cpu, new_dma;

	idx = getDecParm(argv[0], 0);
	cpu = getDecParm(argv[1], 0);
	new_dma = getDecParm(argv[2], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if ((cpu < -1) || (cpu >= 4)) {
		sprintf(env->output, "\nCpu must be -1 to 3...\n");
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat) {
		sprintf(env->output, "\nWorker %d already running...\n", idx);
        	logMsg(env);
		goto exit;
	};

	start_worker_thread(&wkr[idx], new_dma, cpu);
exit:
        return 0;
};

struct cli_cmd Start = {
"start",
4,
3,
"Start a thread on a cpu",
"start <idx> <cpu> <new_dma>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<new_dma> If <> 0, open mport again to get a new DMA channel\n",
StartCmd,
ATTR_NONE
};

int KillCmd(struct cli_env *env, int argc, char **argv)
{
	int st_idx = 0, end_idx = MAX_WORKERS-1, i;

	if (strncmp(argv[0], "all", 3)) {
		st_idx = getDecParm(argv[0], 0);
		end_idx = st_idx;

		if ((st_idx < 0) || (st_idx >= MAX_WORKERS)) {
			sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        		logMsg(env);
			goto exit;
		};
	};

	for (i = st_idx; i <= end_idx; i++) {
		shutdown_worker_thread(&wkr[i]);
	};
exit:
        return 0;
};

struct cli_cmd Kill = {
"kill",
4,
1,
"Kill a thread",
"kill <idx>\n"
	"<idx> is a worker index from 0 to 7, or \"all\"\n",
KillCmd,
ATTR_NONE
};

int HaltCmd(struct cli_env *env, int argc, char **argv)
{
	int st_idx = 0, end_idx = MAX_WORKERS-1, i;

	if (strncmp(argv[0], "all", 3)) {
		st_idx = getDecParm(argv[0], 0);
		end_idx = st_idx;

		if ((st_idx < 0) || (st_idx >= MAX_WORKERS)) {
			sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        		logMsg(env);
			goto exit;
		};
	};

	for (i = st_idx; i <= end_idx; i++) {
		wkr[i].stop_req = 2;
	};

exit:
        return 0;
};

struct cli_cmd Halt = {
"halt",
2,
1,
"Halt execution of a thread command",
"halt <idx>\n"
	"<idx> is a worker index from 0 to 7, or \"all\"\n",
HaltCmd,
ATTR_NONE
};

int MoveCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, cpu;

	idx = getDecParm(argv[0], 0);
	cpu = getDecParm(argv[1], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if ((cpu < -1) || (cpu >= 4)) {
		sprintf(env->output, "\nCpu must be -1 to 3...\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].cpu_req = cpu;
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd Move = {
"move",
2,
2,
"Move a thread to a different CPU",
"move <idx> <cpu>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n",
MoveCmd,
ATTR_NONE
};

#define FOUR_KB (4*1024)
#define SIXTEEN_MB (16*1024*1024)

int IBAllocCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	uint64_t ib_size;

	idx = getDecParm(argv[0], 0);
	ib_size = getHex(argv[1], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if ((ib_size < FOUR_KB) || (ib_size > SIXTEEN_MB)) {
		sprintf(env->output, "\nIbwin size range: 0x%x to 0x%x\n",
			FOUR_KB, SIXTEEN_MB);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not halted...\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = alloc_ibwin;
	wkr[idx].ib_byte_cnt = ib_size;
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd IBAlloc = {
"IBAlloc",
3,
2,
"Allocate an inbound window",
"IBAlloc <idx> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<size> is a hexadecimal power of two from 0x1000 to 0x01000000\n",
IBAllocCmd,
ATTR_NONE
};

int IBDeallocCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;

	idx = getDecParm(argv[0], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = free_ibwin;
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd IBDealloc = {
"IBDealloc",
3,
1,
"Deallocate an inbound window",
"IBDealloc <idx>\n"
	"<idx> is a worker index from 0 to 7\n",
IBDeallocCmd,
ATTR_NONE
};

int obdio_cmd(struct cli_env *env, int argc, char **argv, enum req_type action)
{
	int idx;
	int did;
	uint64_t rio_addr;
	uint64_t bytes;
	uint64_t acc_sz;
	int wr = 0;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);
	acc_sz = getHex(argv[4], 0);
	if ( direct_io_rx_lat != action)
		wr = getDecParm(argv[5], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};
	wkr[idx].action = action;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].wr = wr;
	if ( direct_io == action)
		wkr[idx].ob_byte_cnt = bytes;
	else
		wkr[idx].ob_byte_cnt = 0x10000;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

int OBDIOCmd(struct cli_env *env, int argc, char **argv)
{
	return obdio_cmd(env, argc, argv, direct_io);
};

struct cli_cmd OBDIO = {
"OBDIO",
5,
6,
"Perform reads/writes through an outbound window",
"OBDIO <idx> <did> <rio_addr> <bytes> <acc_sz> <wr>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8\n"
	"<wr>  0: Read, <>0: Write\n",
OBDIOCmd,
ATTR_NONE
};

int OBDIOTxLatCmd(struct cli_env *env, int argc, char **argv)
{
	return obdio_cmd(env, argc, argv, direct_io_tx_lat);
};

struct cli_cmd OBDIOTxLat = {
"DIOTxLat",
8,
6,
"Perform reads/writes through an outbound window",
"DIOTxLat <idx> <did> <rio_addr> <bytes> <acc_sz> <wr>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8\n"
	"<wr>  0: Read, <>0: Write\n"
	"NOTE: For <wr>=1, there must be a <did> thread running OBDIORxLat!\n",
OBDIOTxLatCmd,
ATTR_NONE
};

int OBDIORxLatCmd(struct cli_env *env, int argc, char **argv)
{
	return obdio_cmd(env, argc, argv, direct_io_rx_lat);
};

struct cli_cmd OBDIORxLat = {
"DIORxLat",
10,
6,
"Perform reads/writes through an outbound window",
"DIORxLat <idx> <did> <rio_addr> <bytes> <acc_sz>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8\n",
OBDIORxLatCmd,
ATTR_NONE
};

int dmaCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	uint64_t rio_addr;
	uint64_t bytes;
	uint64_t acc_sz;
	int wr;
	int kbuf;
	int trans;
	int sync;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);
	acc_sz = getHex(argv[4], 0);
	wr = getDecParm(argv[5], 0);
	kbuf = getDecParm(argv[6], 0);
	trans = getDecParm(argv[7], 0);
	sync = getDecParm(argv[8], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	if (trans > (int)RIO_DIRECTIO_TYPE_NWRITE_R_ALL)
		trans = RIO_DIRECTIO_TYPE_NWRITE;

	if (sync > RIO_DIRECTIO_TRANSFER_FAF)
		sync = RIO_DIRECTIO_TRANSFER_SYNC;

	wkr[idx].action = dma_tx;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].wr = wr;
	wkr[idx].use_kbuf = kbuf;
	wkr[idx].dma_trans_type = (enum riomp_dma_directio_type)trans;
	wkr[idx].dma_sync_type = (enum riomp_dma_directio_transfer_sync)sync;
	wkr[idx].rdma_buff_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd dma = {
"dma",
3,
9,
"Perform reads/writes with the DMA engines",
"dma <idx> <did> <rio_addr> <bytes> <acc_sz> <wr> <kbuf> <trans> <sync>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size, min 0x1000 max 0x1000000\n"
	"<wr>  0: Read, <>0: Write\n"
	"<kbuf>  0: User memory, <>0: Kernel buffer\n"
	"<trans>  0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL\n"
	"<sync>  0 SYNC 1 ASYNC 2 FAF\n",
dmaCmd,
ATTR_NONE
};

int dmaTxLatCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	uint64_t rio_addr;
	uint64_t bytes;
	int wr;
	int kbuf;
	int trans;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);
	wr = getDecParm(argv[4], 0);
	kbuf = getDecParm(argv[5], 0);
	trans = getDecParm(argv[6], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	if (trans > (int)RIO_DIRECTIO_TYPE_NWRITE_R_ALL)
		trans = RIO_DIRECTIO_TYPE_NWRITE;

	wkr[idx].action = dma_tx_lat;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = bytes;
	wkr[idx].wr = wr;
	wkr[idx].use_kbuf = kbuf;
	wkr[idx].dma_trans_type = (enum riomp_dma_directio_type)trans;
	if (wr)
		wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_ASYNC;
	else
		wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;
	wkr[idx].rdma_buff_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd dmaTxLat = {
"dmaTxLat",
8,
9,
"Perform reads/writes with the DMA engines",
"dmaTxLat <idx> <did> <rio_addr> <bytes> <acc_sz> <wr> <kbuf> <trans> <sync>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<wr>  0: Read, <>0: Write\n"
	"<kbuf>  0: User memory, <>0: Kernel buffer\n"
	"<trans>  0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL\n"
	"NOTE: For <wr>=1, there must be a thread on <did> running dmaRxLat!\n",
dmaTxLatCmd,
ATTR_NONE
};

int dmaRxLatCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	uint64_t rio_addr;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = dma_rx_lat;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = 0x10000;
	wkr[idx].acc_size = 0x1;
	wkr[idx].wr = 1;
	wkr[idx].use_kbuf = 1;
	wkr[idx].dma_trans_type = RIO_DIRECTIO_TYPE_NWRITE;
	wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_FAF;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd dmaRxLat = {
"dmaRxLat",
8,
3,
"Acknowledge DMA write completion for DMA Latency write test.",
"dmaRxLat <idx> <did> <rio_addr>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n",
dmaRxLatCmd,
ATTR_NONE
};

void roundoff_message_size(int *bytes)
{
	if (*bytes > 4096)
		*bytes = 4096;

	if (*bytes < 24)
		*bytes = 24;

	*bytes = (*bytes + 7) & 0x1FF8;
};


int msg_tx_cmd(struct cli_env *env, int argc, char **argv, enum req_type req)
{
	int idx;
	int did;
	int sock_num;
	int bytes;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	sock_num = getDecParm(argv[2], 0);
	bytes = getDecParm(argv[3], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	if (!sock_num) {
		sprintf(env->output, "\nSock_num must not be 0.\n");
        	logMsg(env);
		goto exit;
	};

	roundoff_message_size(&bytes);

	wkr[idx].action = req;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].sock_num = sock_num;
	wkr[idx].msg_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

int msgTxCmd(struct cli_env *env, int argc, char **argv)
{
	msg_tx_cmd(env, argc, argv, message_tx);
        return 0;
};

struct cli_cmd msgTx = {
"msgTx",
4,
4,
"Sends channelized messages as requested",
"msgTx <idx> <did> <sock_num> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n"
	"NOTE: All parameters are decimal numbers.\n",
msgTxCmd,
ATTR_NONE
};

int msgTxLatCmd(struct cli_env *env, int argc, char **argv)
{
	msg_tx_cmd(env, argc, argv, message_tx_lat);
        return 0;
};

struct cli_cmd msgTxLat = {
"mTxLat",
2,
4,
"Measures latency of channelized messages as requested",
"mTxLat <idx> <did> <sock_num> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n"
	"NOTE: All parameters are decimal numbers.\n",
msgTxLatCmd,
ATTR_NONE
};

int msgRxLatCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int sock_num;
	int bytes;

	idx = getDecParm(argv[0], 0);
	sock_num = getDecParm(argv[1], 0);
	bytes = getDecParm(argv[2], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	if (!sock_num) {
		sprintf(env->output, "\nSock_num must not be 0.\n");
        	logMsg(env);
		goto exit;
	};

	roundoff_message_size(&bytes);

	wkr[idx].action = message_rx_lat;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = 0;
	wkr[idx].sock_num = sock_num;
	wkr[idx].msg_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd msgRxLat = {
"mRxLat",
2,
3,
"Loops back received messages to sender",
"mRxLat <idx> <sock_num> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<sock_num> RapidIO Channelized Messaging channel number to accept\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n"
	"NOTE: All parameters are decimal numbers.\n",
msgRxLatCmd,
ATTR_NONE
};

int msgRxCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int sock_num;

	idx = getDecParm(argv[0], 0);
	sock_num = getDecParm(argv[1], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat != 2) {
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	if (!sock_num) {
		sprintf(env->output, "\nSock_num must not be 0.\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = message_rx;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = 0;
	wkr[idx].sock_num = sock_num;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd msgRx = {
"msgRx",
4,
2,
"Receives channelized messages as requested",
"msgRx <idx> <sock_num>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<sock_num> Target socket number for connections from msgTx command\n",
msgRxCmd,
ATTR_NONE
};

#define FLOAT_STR_SIZE 20

int PerfCmd(struct cli_env *env, int argc, char **argv)
{
	int i;
	float MBps, Gbps, Msgpersec; 
	uint64_t byte_cnt = 0;
	float tot_MBps = 0, tot_Gbps = 0, tot_Msgpersec = 0;
	uint64_t tot_byte_cnt = 0;
	char MBps_str[FLOAT_STR_SIZE],  Gbps_str[FLOAT_STR_SIZE];

	sprintf(env->output,
        "\nW STS <<<<--Data-->>>> --MBps-- -Gbps- Messages\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		struct timespec elapsed;
		uint64_t nsec;

		MBps = Gbps = Msgpersec = 0;
		byte_cnt = 0;

		Msgpersec = wkr[i].perf_msg_cnt;
		byte_cnt = wkr[i].perf_byte_cnt;

		elapsed = time_difference(wkr[i].st_time, wkr[i].end_time);
		nsec = elapsed.tv_nsec + (elapsed.tv_sec * 1000000000);

		MBps = (float)(byte_cnt / (1024*1024)) / 
			((float)nsec / 1000000000.0);
		Gbps = (MBps * 8.0) / 1000.0;

		memset(MBps_str, 0, FLOAT_STR_SIZE);
		memset(Gbps_str, 0, FLOAT_STR_SIZE);
		sprintf(MBps_str, "%4.3f", MBps);
		sprintf(Gbps_str, "%2.3f", Gbps);

		sprintf(env->output, "%1d %3s %16lx %8s %6s %9.0f\n",
			i,  THREAD_STR(wkr[i].stat),
			byte_cnt, MBps_str, Gbps_str, Msgpersec);
        	logMsg(env);

		if (byte_cnt) {
			tot_byte_cnt += byte_cnt;
			tot_MBps += MBps;
			tot_Gbps += Gbps;
		};
		tot_Msgpersec += Msgpersec;

		if (argc) {
			wkr[i].perf_byte_cnt = 0;
			wkr[i].perf_msg_cnt = 0;
			clock_gettime(CLOCK_MONOTONIC, &wkr[i].st_time);
		};
	};
	memset(MBps_str, 0, FLOAT_STR_SIZE);
	memset(Gbps_str, 0, FLOAT_STR_SIZE);
	sprintf(MBps_str, "%4.3f", tot_MBps);
	sprintf(Gbps_str, "%2.3f", tot_Gbps);

	sprintf(env->output, "T     %16lx %8s %6s %9.0f\n",
		tot_byte_cnt, MBps_str, Gbps_str, tot_Msgpersec);
        logMsg(env);

        return 0;
};

struct cli_cmd Perf = {
"perf",
4,
0,
"Print current performance for threads.",
"perf {<optional>}\n"
	"Any parameter to perf causes the byte and message counts of all\n"
	"   running threads to be zeroed after they are displayed.\n",
PerfCmd,
ATTR_RPT
};

int LatCmd(struct cli_env *env, int argc, char **argv)
{
	int i;
	char min_lat_str[FLOAT_STR_SIZE];
	char avg_lat_str[FLOAT_STR_SIZE];
	char max_lat_str[FLOAT_STR_SIZE];


	sprintf(env->output,
        "\nW STS <<<<-Count-->>>> <<<<Min uSec>>>> <<<<Avg uSec>>>> <<<<Max uSec>>>>\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		uint64_t tot_nsec;
		uint64_t avg_nsec;

		tot_nsec = wkr[i].tot_iter_time.tv_nsec +
				(wkr[i].tot_iter_time.tv_sec * 1000000000);

		/* Note: divide by 2 to account for round trip latency. */
		if (wkr[i].perf_iter_cnt)
			avg_nsec = tot_nsec/2/wkr[i].perf_iter_cnt;
		else
			avg_nsec = 0;

		memset(min_lat_str, 0, FLOAT_STR_SIZE);
		memset(avg_lat_str, 0, FLOAT_STR_SIZE);
		memset(max_lat_str, 0, FLOAT_STR_SIZE);
		sprintf(min_lat_str, "%4.3f",
			(float)(wkr[i].min_iter_time.tv_nsec/2)/1000.0); 
		sprintf(avg_lat_str, "%4.3f", (float)avg_nsec/1000.0);
		sprintf(max_lat_str, "%4.3f",
			(float)(wkr[i].max_iter_time.tv_nsec/2)/1000.0); 

		sprintf(env->output, "%1d %3s %16ld %16s %16s %16s\n",
			i,  THREAD_STR(wkr[i].stat),
			wkr[i].perf_iter_cnt,
			min_lat_str, avg_lat_str, max_lat_str);
        	logMsg(env);


		if (argc) {
			wkr[i].perf_iter_cnt = 0;
			wkr[i].min_iter_time.tv_nsec = 0;
			wkr[i].max_iter_time.tv_nsec = 0;
			wkr[i].tot_iter_time.tv_nsec = 0;
			wkr[i].tot_iter_time.tv_sec = 0;
		};
	};

        return 0;
};

struct cli_cmd Lat = {
"Lat",
3,
0,
"Print current latency for threads.",
"lat {<optional>}\n"
	"Any parameter to lat causes the latency information for all\n"
	"   running threads to be zeroed after they are displayed.\n",
LatCmd,
ATTR_RPT
};


void display_cpu(struct cli_env *env, int cpu)
{
	if (-1 == cpu)
		sprintf(env->output, "Any ");
	else
		sprintf(env->output, "%3d ", cpu);
        logMsg(env);
};
		

void display_gen_status(struct cli_env *env)
{
	int i;

	sprintf(env->output,
        "\nW STS CPU RUN ACTION MODE <<<<--ADDR-->>>> ByteCnt AccSize W OB IB MB\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%1d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].cpu_req);
		display_cpu(env, wkr[i].cpu_run);
		sprintf(env->output,
			"%6s %4s %16lx %7lx %7lx %1d %2d %2d %2d\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), 
			wkr[i].rio_addr, wkr[i].byte_cnt, wkr[i].acc_size, 
			wkr[i].wr, wkr[i].ob_valid, wkr[i].ib_valid, 
			wkr[i].mb_valid);
        	logMsg(env);
	};
};

void display_ibwin_status(struct cli_env *env)
{
	int i;

	sprintf(env->output,
	"\nW STS CPU RUN ACTION MODE IB <<<< HANDLE >>>> <<<<RIO ADDR>>>> <<<<  SIZE  >>>>\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%1d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].cpu_req);
		display_cpu(env, wkr[i].cpu_run);
		sprintf(env->output,
			"%6s %4s %2d %16lx %16lx %16lx\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), 
			wkr[i].ib_valid, wkr[i].ib_handle, wkr[i].ib_rio_addr, 
			wkr[i].ib_byte_cnt);
        	logMsg(env);
	};
};

void display_msg_status(struct cli_env *env)
{
	int i;

	sprintf(env->output,
	"\nW STS CPU RUN ACTION MODE MB ACC CON Msg_Size SockNum TX RX\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%1d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].cpu_req);
		display_cpu(env, wkr[i].cpu_run);
		sprintf(env->output,
			"%6s %4s %2d %3d %3d %8d %7d %2d %2d\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), 
			wkr[i].mb_valid, wkr[i].acc_skt_valid,
			wkr[i].con_skt_valid, wkr[i].msg_size,
			wkr[i].sock_num, (NULL != wkr[i].sock_tx_buf),
			(NULL != wkr[i].sock_rx_buf)
		);
        	logMsg(env);
	};
};

int StatusCmd(struct cli_env *env, int argc, char **argv)
{
	char sel_stat = 'g';

	if (argc)
		sel_stat = argv[0][0];
	
	switch (sel_stat) {
		case 'i':
		case 'I': 
			display_ibwin_status(env);
			break;
		case 'm':
		case 'M': 
			display_msg_status(env);
			break;
		case 'g':
		case 'G': 
			display_gen_status(env);
			break;
		default: sprintf(env->output, "Unknown option \"%c\"\n", 
				argv[0][0]);
        		logMsg(env);
			return 0;
	};

        return 0;
};

struct cli_cmd Status = {
"status",
2,
0,
"Display status of all threads",
"status {i|o|s}\n"
        "Optionally enter a character to select the status type:\n"
        "i : IBWIN status\n"
        "m : Messaging status\n"
        "s : General status\n"
        "Default is general status.\n",
StatusCmd,
ATTR_RPT
};

int dump_idx;
uint64_t dump_base_offset;
uint64_t dump_size;

int DumpCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	uint64_t offset, base_offset;
	uint64_t size;

	if (argc) {
		idx = getDecParm(argv[0], 0);
		base_offset = getHex(argv[1], 0);
		size = getHex(argv[2], 0);
	} else {
		idx = dump_idx;
		base_offset = dump_base_offset;
		size = dump_size;
	};

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
							MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr)) {
		sprintf(env->output, "\nNo mapped inbound window present\n");
        	logMsg(env);
		goto exit;
	};

	if ((base_offset + size) > wkr[idx].ib_byte_cnt) {
		sprintf(env->output, "\nOffset + size exceeds window bytes\n");
        	logMsg(env);
		goto exit;
	}

	dump_idx = idx;
	dump_base_offset = base_offset;
	dump_size = size;

        sprintf(env->output,
                "  Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
        logMsg(env);
        for (offset = 0; offset < size; offset++) {
                if (!(offset & 0xF)) {
                        sprintf(env->output,"\n%8lx", offset);
                        logMsg(env);
                };
                sprintf(env->output, " %2x", 
			*(volatile uint8_t * volatile)(
			(uint8_t *)wkr[idx].ib_ptr + base_offset + offset));
                logMsg(env);
        };
        sprintf(env->output, "\n");
        logMsg(env);
exit:
        return 0;
};

struct cli_cmd Dump = {
"dump",
2,
3,
"Dump inbound memory area",
"Dump <idx> <offset> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<offset> is the hexadecimal offset, in bytes, from the window start\n"
	"<size> is the number of bytes to display, starting at <offset>\n",
DumpCmd,
ATTR_RPT
};

int MpdevsCmd(struct cli_env *env, int argc, char **argv)
{
        uint32_t *mport_list = NULL;
        uint32_t *ep_list = NULL;
        uint32_t *list_ptr;
        uint32_t number_of_eps = 0;
        uint8_t  number_of_mports = RIODP_MAX_MPORTS;
        uint32_t ep = 0;
        int i;
        int mport_id;
        int ret = 0;

        ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
        if (ret) {
                sprintf(env->output, "riomp_mgmt_get_mport_list ERR %d:%s\n",
			ret, strerror(ret));
        	logMsg(env);
		goto exit;
        }

        sprintf(env->output, "\nAvailable %d local mport(s):\n",
			number_of_mports);
        logMsg(env);

        if (number_of_mports > RIODP_MAX_MPORTS) {
                sprintf(env->output, 
			"WARNING: Only %d out of %d have been retrieved\n",
                        RIODP_MAX_MPORTS, number_of_mports);
        	logMsg(env);
        }

        list_ptr = mport_list;
        for (i = 0; i < number_of_mports; i++, list_ptr++) {
                mport_id = *list_ptr >> 16;
                sprintf(env->output, "+++ mport_id: %u dest_id: %u\n",
                                mport_id, *list_ptr & 0xffff);
        	logMsg(env);

                /* Display EPs for this MPORT */

                ret = riomp_mgmt_get_ep_list(mport_id, &ep_list, &number_of_eps);
                if (ret) {
                        sprintf(env->output, 
				"ERR: riodp_ep_get_list() ERR %d: %s\n",
				ret, strerror(ret));
        		logMsg(env);
                        break;
                }

                printf("\t%u Endpoints (dest_ID): ", number_of_eps);
                for (ep = 0; ep < number_of_eps; ep++) {
                        sprintf(env->output, "%u ", *(ep_list + ep));
        		logMsg(env);
		}
                sprintf(env->output, "\n");
        	logMsg(env);

                ret = riomp_mgmt_free_ep_list(&ep_list);
                if (ret) {
                        sprintf(env->output, 
				"ERR: riodp_ep_free_list() ERR %d: %s\n",
				ret, strerror(ret));
        		logMsg(env);
		};

        }

	sprintf(env->output, "\n");
        logMsg(env);

        ret = riomp_mgmt_free_mport_list(&mport_list);
        if (ret) {
                sprintf(env->output,
			"ERR: riodp_ep_free_list() ERR %d: %s\n",
			ret, strerror(ret));
        	logMsg(env);
	};
exit:
        return 0;
};

struct cli_cmd Mpdevs = {
"mpdevs",
2,
0,
"Display mports and devices",
"mpdevs <No Parameters>\n",
MpdevsCmd,
ATTR_NONE
};

struct cli_cmd *goodput_cmds[] = {
	&IBAlloc,
	&IBDealloc,
	&OBDIO,
	&OBDIOTxLat,
	&OBDIORxLat,
	&dma,
	&dmaTxLat,
	&dmaRxLat,
	&msgTx,
	&msgTxLat,
	&msgRx,
	&msgRxLat,
	&Perf,
	&Lat,
	&Status,
	&Start,
	&Kill,
	&Halt,
	&Move,
	&Dump,
	&Mpdevs
};

void bind_goodput_cmds(void)
{
	dump_idx = 0;
	dump_base_offset = 0;
	dump_size = 0x100;

        add_commands_to_cmd_db(sizeof(goodput_cmds)/sizeof(goodput_cmds[0]),
                                goodput_cmds);
};

#ifdef __cplusplus
}
#endif
