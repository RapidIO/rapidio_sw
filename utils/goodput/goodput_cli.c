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
	(char *)"DMA",
	(char *)"MSG_Tx",
	(char *)"MSG_Rx",
	(char *)" IBWIN",
	(char *)"~IBWIN",
	(char *)"SHTDWN",
	(char *)"LAST"
};

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
"start <wkr_idx> <cpu> <new_dma>\n"
	"<wkr_idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<new_dma> If <> 0, open mport again to get a new DMA channel\n",
StartCmd,
ATTR_NONE
};

int KillCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;

	idx = getDecParm(argv[0], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	shutdown_worker_thread(&wkr[idx]);
exit:
        return 0;
};

struct cli_cmd Kill = {
"kill",
4,
1,
"Kill a thread",
"kill <idx>\n"
	"<idx> is a worker index from 0 to 7\n",
KillCmd,
ATTR_NONE
};

int HaltCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;

	idx = getDecParm(argv[0], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	wkr[idx].stop_req = 2;
exit:
        return 0;
};

struct cli_cmd Halt = {
"halt",
2,
1,
"Halt execution of a thread command",
"halt <idx>\n"
	"<idx> is a worker index from 0 to 7\n",
HaltCmd,
ATTR_NONE
};

int SwitchCmd(struct cli_env *env, int argc, char **argv)
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
exit:
        return 0;
};

struct cli_cmd Switch = {
"switch",
2,
2,
"Switch a thread to a different CPU\n",
"switch <idx> <cpu>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n",
SwitchCmd,
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
		sprintf(env->output, "\nWorker not stopped...\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = alloc_ibwin;
	wkr[idx].ib_byte_cnt = ib_size;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd IBAlloc = {
"IBAlloc",
3,
2,
"Allocate an inbound window",
"IBAlloc <wkr_idx> <size>\n"
	"<wkr_idx> is a worker index from 0 to 7\n"
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
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd IBDealloc = {
"IBDealloc",
3,
1,
"Deallocate an inbound window",
"IBDealloc <wkr_idx>\n"
	"<wkr_idx> is a worker index from 0 to 7\n",
IBDeallocCmd,
ATTR_NONE
};

int OBDIOCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	uint64_t rio_addr;
	uint64_t bytes;
	uint64_t acc_sz;
	int wr;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);
	acc_sz = getHex(argv[4], 0);
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

	wkr[idx].action = direct_io;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].wr = wr;
	wkr[idx].ob_byte_cnt = bytes;

	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd OBDIO = {
"OBDIO",
5,
6,
"Perform reads/writes through an outbound window",
"OBDIO <wkr_idx> <did> <rio_addr> <bytes> <acc_sz> <wr>\n"
	"<wkr_idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8\n"
	"<wr>  0: Read, <>0: Write\n",
OBDIOCmd,
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

	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd dma = {
"dma",
3,
9,
"Perform reads/writes with the DMA engines",
"dma <wkr_idx> <did> <rio_addr> <bytes> <acc_sz> <wr> <kbuf> <trans> <sync>\n"
	"<wkr_idx> is a worker index from 0 to 7\n"
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

int msgTxCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	int sock_num;
	int bytes;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	sock_num = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);

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

	bytes = (bytes + 7) & 0xFF8;
	if (bytes < 24)
		bytes = 24;

	wkr[idx].action = message_tx;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].sock_num = sock_num;
	wkr[idx].msg_size = bytes;

	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd msgTx = {
"msgTx",
4,
4,
"Sends channelized messages as requested",
"msgTx <wkr_idx> <did> <sock_num> <size>\n"
	"<wkr_idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO memory address to access\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n",
msgTxCmd,
ATTR_NONE
};

int msgRxCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	int sock_num;

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	sock_num = getHex(argv[2], 0);

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
	wkr[idx].did = did;
	wkr[idx].sock_num = sock_num;

	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd msgRx = {
"msgRx",
4,
3,
"Receives channelized messages as requested",
"msgRx <wkr_idx> <did> <sock_num>\n"
	"<wkr_idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO memory address to access\n",
msgRxCmd,
ATTR_NONE
};

int PerfCmd(struct cli_env *env, int argc, char **argv)
{
	int i;
	float MBps, Gbps, Msgpersec; 
	uint64_t byte_cnt = 0;
	float tot_MBps = 0, tot_Gbps = 0, tot_Msgpersec = 0;
	uint64_t tot_byte_cnt = 0;

	sprintf(env->output,
        "\nW <<<<--Data-->>>> --MBps-- -Gbps- Msgs/Sec\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		struct timespec elapsed;
		uint64_t nsec;

		MBps = Gbps = Msgpersec = 0;
		byte_cnt = 0;

		if ((wkr[i].action == message_rx) ||
						(wkr[i].action == message_tx)) {
			Msgpersec = wkr[i].perf_byte_cnt;
			if (wkr[i].action == message_tx)
				byte_cnt = Msgpersec * wkr[i].msg_size;
		};
		if ((wkr[i].action == direct_io) ||
						(wkr[i].action == dma_tx)) {
			byte_cnt = wkr[i].perf_byte_cnt;
		};

		elapsed = time_difference(wkr[i].st_time, wkr[i].end_time);
		nsec = elapsed.tv_nsec + (elapsed.tv_sec * 1000000000);

		MBps = (float)(byte_cnt / (1024*1024)) / 
			((float)nsec / 1000000000.0);
		Gbps = (MBps * 8.0) / 1000.0;

		sprintf(env->output, "%1d %16lx %4.3f %2.3f %9.0f\n",
			i, byte_cnt, MBps, Gbps, Msgpersec);
        	logMsg(env);

		if (byte_cnt) {
			tot_byte_cnt += byte_cnt;
			tot_MBps += MBps;
			tot_Gbps += Gbps;
		};
		tot_Msgpersec += Msgpersec;
	};
	sprintf(env->output, "T %16lx %8.3f %2.3f %9.0f\n",
		tot_byte_cnt, tot_MBps, tot_Gbps, tot_Msgpersec);
        logMsg(env);

        return 0;
};

struct cli_cmd Perf = {
"perf",
4,
0,
"Print current performance for threads.",
"perf <no parameters>",
PerfCmd,
ATTR_NONE
};


#define ACTION_STR(x) (char *)((x < last_action)?req_type_str[x]:"UNKWN!")
#define MODE_STR(x) (char *)((x == kernel_action)?"KRNL":"User")

void display_gen_status(struct cli_env *env)
{
	int i;

	sprintf(env->output,
        "\nW STS CPU RUN ACTION MODE <<<<--ADDR-->>>> ByteCnt AccSize W OB IB MB\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		int stat;
		stat = wkr[i].stat;

		sprintf(env->output,
		"%1d %3s %3d %3d %6s %4s %16lx %7lx %7lx %1d %2d %2d %2d\n",
			i, (0 == stat)?"DED":((1 == stat)?"RUN":"BLK"),
			wkr[i].cpu_req, wkr[i].cpu_run,
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
	"\nW S CPU RUN ACTION MODE IB <<<< HANDLE >>>> <<<<RIO ADDR>>>> <<<<  SIZE  >>>>\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		int stat;
		stat = wkr[i].stat;
		sprintf(env->output,
			"%1d %1s %3d %3d %6s %4s %2d %16lx %16lx %16lx\n",
			i, (0 == stat)?"D":((1 == stat)?"R":"B"),
			wkr[i].cpu_req, wkr[i].cpu_run,
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
	"\nW S CPU RUN ACTION MODE MB ACC CON Msg_Size SockNum TX RX\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		int stat;
		stat = wkr[i].stat;
		sprintf(env->output,
			"%1d %1s %3d %3d %6s %4s %2d %3d %3d %7d %7d %2d %2d\n",
			i,  (0 == stat)?"D":((1 == stat)?"R":"B"),
			wkr[i].cpu_req, wkr[i].cpu_run,
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
		default: sprintf(env->output, "Unknown option \"%c\"", 
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

int DumpCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	uint64_t offset, base_offset;
	uint64_t size;

	idx = getDecParm(argv[0], 0);
	base_offset = getHex(argv[1], 0);
	size = getHex(argv[2], 0);

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

        sprintf(env->output,
                "  Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
        logMsg(env);
        for (offset = 0; offset < size; offset++) {
                if (!(offset & 0xF)) {
                        sprintf(env->output,"\n%8lx", offset);
                        logMsg(env);
                };
                sprintf(env->output, " %2x", 
			*(uint8_t *)(
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
ATTR_NONE
};

struct cli_cmd *goodput_cmds[] = {
	&IBAlloc,
	&IBDealloc,
	&OBDIO,
	&dma,
	&msgTx,
	&msgRx,
	&Perf,
	&Status,
	&Start,
	&Kill,
	&Halt,
	&Dump
};

void bind_goodput_cmds(void)
{
        add_commands_to_cmd_db(sizeof(goodput_cmds)/sizeof(goodput_cmds[0]),
                                goodput_cmds);
};

#ifdef __cplusplus
}
#endif
