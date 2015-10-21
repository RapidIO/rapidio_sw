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

#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include "goodput_cli.h"
#include "time_utils.h"
#include "mhz.h"
#include "liblog.h"
#include "assert.h"

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
#ifdef USER_MODE_DRIVER
        (char*)"UCal",
        (char*)"UDMA",
        (char*)"ltudma",
        (char*)"lrudma",
        (char*)"nrudma",
        (char*)"UMSG",
        (char*)"UMSGLat",
#endif
	(char *)"LAST"
};

std::map<std::string, std::string> SET_VARS;

extern "C" const char* GetEnv(const char* var)
{
	if (var == NULL || var[0] == '\0') return NULL;
	
	std::map<std::string, std::string>::iterator it = SET_VARS.find(var);
	if (it == SET_VARS.end()) return NULL;

	return it->second.c_str();
}

extern "C" void SetEnvVar(const char* arg)
{
	if(arg == NULL || arg[0] == '\0') return;

	char* tmp = strdup(arg);

	char* sep = strstr(tmp, "=");
	if (sep == NULL) goto exit;
	sep[0] = '\0';
	SET_VARS[tmp] = (sep+1);

exit:
	free(tmp);
}


static inline const char* SubstituteParam(const char* arg)
{
	if (arg == NULL || arg[0] == '\0') return arg;
	if (arg[0] != '$') return arg;

	std::map<std::string, std::string>::iterator it = SET_VARS.find(arg+1);
	if (it == SET_VARS.end()) return arg;

	return it->second.c_str();
}

inline int GetDecParm(const char* arg, int dflt)       { return getDecParm((char*)SubstituteParam(arg), dflt); }
inline int GetHex(const char* arg, int dflt)           { return getHex((char*)SubstituteParam(arg), dflt); }
inline float GetFloatParm(const char* arg, float dflt) { return getFloatParm((char*)SubstituteParam(arg), dflt); }

int check_idx(struct cli_env *env, int idx, int want_halted)
{
	int rc = 1;

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (want_halted && (2 != wkr[idx].stat)) {
		sprintf(env->output, "\nWorker not halted...\n");
        	logMsg(env);
		goto exit;
	};

	if (!want_halted && (2 == wkr[idx].stat)) {
		sprintf(env->output, "\nWorker halted...\n");
        	logMsg(env);
		goto exit;
	};
	rc = 0;
exit:
	return rc;
};

#define MAX_GOODPUT_CPU 7

int get_cpu(struct cli_env *env, char *dec_parm, int *cpu)
{
	int rc = 1;

	*cpu = GetDecParm(dec_parm, 0);

	if ((*cpu  < -1) || (*cpu > MAX_GOODPUT_CPU)) {
		sprintf(env->output, "\nCPU must be 0 to %d...\n",
			MAX_GOODPUT_CPU);
        	logMsg(env);
		goto exit;
	};

	rc = 0;
exit:
	return rc;
};

#define ACTION_STR(x) (char *)((x < last_action)?req_type_str[x]:"UNKWN!")
#define MODE_STR(x) (char *)((x == kernel_action)?"KRNL":"User")
#define THREAD_STR(x) (char *)((0 == x)?"---":((1 == x)?"Run":"Hlt"))

int ThreadCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, cpu, new_dma;

	idx = GetDecParm(argv[0], 0);
	if (get_cpu(env, argv[1], &cpu))
		goto exit;
	new_dma = GetDecParm(argv[2], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if (wkr[idx].stat) {
		sprintf(env->output, "\nWorker %d already running...\n", idx);
        	logMsg(env);
		goto exit;
	};

	wkr[idx].idx = idx;
	start_worker_thread(&wkr[idx], new_dma, cpu);
exit:
        return 0;
};

struct cli_cmd Thread = {
"thread",
1,
3,
"Start a thread on a cpu",
"start <idx> <cpu> <new_dma>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<new_dma> If <> 0, open mport again to get a new DMA channel\n",
ThreadCmd,
ATTR_NONE
};

int KillCmd(struct cli_env *env, int argc, char **argv)
{
	int st_idx = 0, end_idx = MAX_WORKERS-1, i;

	if (strncmp(argv[0], "all", 3)) {
		st_idx = GetDecParm(argv[0], 0);
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
	unsigned int st_idx = 0, end_idx = MAX_WORKERS-1, i;

	if (strncmp(argv[0], "all", 3)) {
		st_idx = GetDecParm(argv[0], 0);
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
#ifdef USER_MODE_DRIVER
		wkr[i].umd_fifo_proc_must_die = 1;
		if (wkr[i].umd_dch)
			wkr[i].umd_dch->shutdown();
#endif
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

	idx = GetDecParm(argv[0], 0);
	if (get_cpu(env, argv[1], &cpu))
		goto exit;

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	wkr[idx].wkr_thr.cpu_req = cpu;
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

int WaitCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, state = -1, limit = 10000;
	const struct timespec ten_usec = {0, 10 * 1000};

	idx = GetDecParm(argv[0], 0);
	switch (argv[1][0]) {
	case '0':
	case 'd':
	case 'D': state = 0;
		break;
	case '1':
	case 'r':
	case 'R': state = 1;
		break;
	case '2':
	case 'h':
	case 'H': state = 2;
		break;
	default: state = -1;
		break;
	};

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		sprintf(env->output, "\nIndex must be 0 to %d...\n",
								MAX_WORKERS);
        	logMsg(env);
		goto exit;
	};

	if ((state < 0) || (state > 2)) {
		sprintf(env->output,
			"\nState must be 0|d|D, 1|r|R , or 2|h|H\n");
        	logMsg(env);
		goto exit;
	};

	while ((wkr[idx].stat != state) && limit--)
        	nanosleep(&ten_usec, NULL);

	if (wkr[idx].stat == state)
		sprintf(env->output, "\nPassed, Worker %d is now %s\n",
			idx, THREAD_STR(wkr[idx].stat));
	else
		sprintf(env->output, "\nFAILED, Worker %d is now %s\n",
			idx, THREAD_STR(wkr[idx].stat));
        logMsg(env);

exit:
        return 0;
};

struct cli_cmd Wait = {
"wait",
2,
2,
"Wait until a thread reaches a particular state",
"wait <idx> <state>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<state> 0|d|D - Dead, 1|r|R - Run, 2|h|H - Halted\n",
WaitCmd,
ATTR_NONE
};

int SleepCmd(struct cli_env *env, int argc, char **argv)
{
	float sec = GetFloatParm(argv[0], 0);
	if(sec > 0) {
		sprintf(env->output, "\nSleeping %f sec...\n", sec);
        	logMsg(env);
		const long usec = sec * 1000000;
		usleep(usec);
	}
	return 0;
}

struct cli_cmd Sleep = {
"sleep",
2,
1,
"Sleep for a number of seconds (fractional allowed)",
"sleep <sec>\n",
SleepCmd,
ATTR_NONE
};

#define FOUR_KB (4*1024)
#define SIXTEEN_MB (16*1024*1024)

int IBAllocCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	uint64_t ib_size;

	idx = GetDecParm(argv[0], 0);
	ib_size = GetHex(argv[1], 0);

	if (check_idx(env, idx, 1))
		goto exit;

	if ((ib_size < FOUR_KB) || (ib_size > SIXTEEN_MB)) {
		sprintf(env->output, "\nIbwin size range: 0x%x to 0x%x\n",
			FOUR_KB, SIXTEEN_MB);
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

	idx = GetDecParm(argv[0], 0);

	if (check_idx(env, idx, 1))
		goto exit;

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

	idx = GetDecParm(argv[0], 0);
	did = GetDecParm(argv[1], 0);
	rio_addr = GetHex(argv[2], 0);
	if (direct_io == action) {
		bytes = GetHex(argv[3], 0);
		acc_sz = GetHex(argv[4], 0);
		wr = GetDecParm(argv[5], 0);
	} else {
		acc_sz = GetHex(argv[3], 0);
		bytes = acc_sz;
		if (direct_io_tx_lat == action) 
			wr = GetDecParm(argv[4], 0);
		else
			wr = 1;
	};
		

	if (check_idx(env, idx, 1))
		goto exit;
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
"Measure goodput of reads/writes through an outbound window",
"OBDIO <idx> <did> <rio_addr> <bytes> <acc_sz> <wr>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8, 16\n"
	"<wr>  0: Read, <>0: Write\n"
	"NOTE: <acc_sz> == 16 is used to calibrate\n"
	"       the software contribution to latency...\n",
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
5,
"Measure latency of reads/writes through an outbound window",
"DIOTxLat <idx> <did> <rio_addr> <acc_sz> <wr>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8, 16\n"
	"<wr>  0: Read, <>0: Write\n"
	"NOTE: <acc_sz> == 16 is used to calibrate\n"
	"       the software contribution to latency...\n"
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
4,
4,
"Loop back DIOTxLat writes through an outbound window",
"DIORxLat <idx> <did> <rio_addr> <acc_sz>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<acc_sz> Access size, values: 1, 2, 4, 8\n"
	"NOTE: DIORxLat must be run before OBDIOTxLat!\n",
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

	idx = GetDecParm(argv[0], 0);
	did = GetDecParm(argv[1], 0);
	rio_addr = GetHex(argv[2], 0);
	bytes = GetHex(argv[3], 0);
	acc_sz = GetHex(argv[4], 0);
	wr = GetDecParm(argv[5], 0);
	kbuf = GetDecParm(argv[6], 0);
	trans = GetDecParm(argv[7], 0);
	sync = GetDecParm(argv[8], 0);

	if (check_idx(env, idx, 1))
		goto exit;

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
"Measure goodput of DMA reads/writes",
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

	idx = GetDecParm(argv[0], 0);
	did = GetDecParm(argv[1], 0);
	rio_addr = GetHex(argv[2], 0);
	bytes = GetHex(argv[3], 0);
	wr = GetDecParm(argv[4], 0);
	kbuf = GetDecParm(argv[5], 0);
	trans = GetDecParm(argv[6], 0);

	if (check_idx(env, idx, 1))
		goto exit;

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
		wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;
	else
		wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;
	wkr[idx].rdma_buff_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd dmaTxLat = {
"dTxLat",
2,
7,
"Measure lantecy of DMA reads/writes",
"dTxLat <idx> <did> <rio_addr> <bytes> <wr> <kbuf> <trans>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<wr>  0: Read, <>0: Write\n"
	"<kbuf>  0: User memory, <>0: Kernel buffer\n"
	"<trans>  0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL\n"
	"NOTE: For <wr>=1, there must be a thread on <did> running dRxLat!\n",
dmaTxLatCmd,
ATTR_NONE
};

int dmaRxLatCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int did;
	uint64_t rio_addr;
	uint64_t bytes;

	idx = GetDecParm(argv[0], 0);
	did = GetDecParm(argv[1], 0);
	rio_addr = GetHex(argv[2], 0);
	bytes = GetHex(argv[3], 0);

	if (check_idx(env, idx, 1))
		goto exit;

	if (!rio_addr || !bytes) {
		sprintf(env->output, "\nrio_addr and bytes cannot be 0.\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = dma_rx_lat;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = bytes;
	wkr[idx].wr = 1;
	wkr[idx].use_kbuf = 1;
	wkr[idx].dma_trans_type = RIO_DIRECTIO_TYPE_NWRITE;
	wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;
	if (bytes < MIN_RDMA_BUFF_SIZE) 
		wkr[idx].rdma_buff_size = MIN_RDMA_BUFF_SIZE;
	else
		wkr[idx].rdma_buff_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd dmaRxLat = {
"dRxLat",
8,
4,
"Loop back DMA writes for dTxLat command.",
"dRxLat <idx> <did> <rio_addr> <bytes>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"NOTE: The dRxLat command must be run before dTxLat!\n",
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

	idx = GetDecParm(argv[0], 0);
	did = GetDecParm(argv[1], 0);
	sock_num = GetDecParm(argv[2], 0);
	bytes = GetDecParm(argv[3], 0);

	if (check_idx(env, idx, 1))
		goto exit;

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
"Measure goodput of channelized messages",
"msgTx <idx> <did> <sock_num> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n"
	"NOTE: All parameters are decimal numbers.\n"
	"NOTE: msgTx must send to a corresponding msgRx!\n",
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
"Measures latency of channelized messages",
"mTxLat <idx> <did> <sock_num> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n"
	"NOTE: All parameters are decimal numbers.\n"
	"NOTE: mTxLat must be sending to a node running mRxLat!\n"
	"NOTE: mRxLat must be run before mTxLat!\n",
msgTxLatCmd,
ATTR_NONE
};

int msgRxLatCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int sock_num;
	int bytes;

	idx = GetDecParm(argv[0], 0);
	sock_num = GetDecParm(argv[1], 0);
	bytes = GetDecParm(argv[2], 0);

	if (check_idx(env, idx, 1))
		goto exit;

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
"Loops back received messages to mTxLat sender",
"mRxLat <idx> <sock_num> <size>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<sock_num> RapidIO Channelized Messaging channel number to accept\n"
	"<size> bytes per message, multiple of 8 minimum 24 up to 4096\n"
	"NOTE: All parameters are decimal numbers.\n"
	"NOTE: mRxLat must be run before mTxLat!\n",
msgRxLatCmd,
ATTR_NONE
};

int msgRxCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int sock_num;

	idx = GetDecParm(argv[0], 0);
	sock_num = GetDecParm(argv[1], 0);

	if (check_idx(env, idx, 1))
		goto exit;

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
	"<sock_num> Target socket number for connections from msgTx command\n"
	"NOTE: msgRx must be running before msgTx!\n",
msgRxCmd,
ATTR_NONE
};

#define FLOAT_STR_SIZE 20

int GoodputCmd(struct cli_env *env, int argc, char **argv)
{
	int i;
	float MBps, Gbps, Msgpersec; 
	uint64_t byte_cnt = 0;
	float tot_MBps = 0, tot_Gbps = 0, tot_Msgpersec = 0;
	uint64_t tot_byte_cnt = 0;
	char MBps_str[FLOAT_STR_SIZE],  Gbps_str[FLOAT_STR_SIZE];

#ifdef USER_MODE_DRIVER
#if 0
	sprintf(env->output,
        "\ncsv,Worker#,STS size,DMA Write Size (hex),Ticks/Packet,uS/Pkt,Total Pkts,Thruput (Mbyte/s)\n");
        logMsg(env);

        const int MHz = getCPUMHz();

	for (i = 0; i < MAX_WORKERS; i++) {
		if(wkr[i].tick_count == 0) continue;

		const double TOTAL_SIZE_MEG = wkr[i].tick_data_total / 1048576.0;

		double dT = (double)wkr[i].tick_total / wkr[i].tick_count;
		double dTus = dT / MHz;

		double dTtotalSec = wkr[i].tick_total / (1000000.0 * MHz); // how many S it took to suffle ALL data

		double thruput = TOTAL_SIZE_MEG / dTtotalSec;

		sprintf(env->output, "csv,%d,0x%x,%x,%lf,%lf,%llu,%lf\n",
                        i, wkr[i].umd_sts_entries, wkr[i].acc_size,
			dT, dTus, wkr[i].tick_count, thruput);
        	logMsg(env);
	}
#endif
#endif
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
#ifdef USER_MODE_DRIVER
			wkr[i].tick_data_total = 0;
			wkr[i].tick_total = 0;
			wkr[i].tick_count = 0;
#endif
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

struct cli_cmd Goodput = {
"goodput",
1,
0,
"Print current performance for threads.",
"goodput {<optional>}\n"
	"Any parameter to goodput causes the byte and message counts of all\n"
	"   running threads to be zeroed after they are displayed.\n",
GoodputCmd,
ATTR_RPT
};

int LatCmd(struct cli_env *env, int argc, char **argv)
{
	int i;
	char min_lat_str[FLOAT_STR_SIZE];
	char avg_lat_str[FLOAT_STR_SIZE];
	char max_lat_str[FLOAT_STR_SIZE];

	if (0)
		argv[0][0] = argc;

	sprintf(env->output,
        "\nW STS <<<<-Count-->>>> <<<<Min uSec>>>> <<<<Avg uSec>>>> <<<<Max uSec>>>>\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		uint64_t tot_nsec;
		uint64_t avg_nsec;
		uint64_t divisor;

		divisor = (wkr[i].wr)?2:1;

		tot_nsec = wkr[i].tot_iter_time.tv_nsec +
				(wkr[i].tot_iter_time.tv_sec * 1000000000);

		/* Note: divide by 2 to account for round trip latency. */
		if (wkr[i].perf_iter_cnt)
			avg_nsec = tot_nsec/divisor/wkr[i].perf_iter_cnt;
		else
			avg_nsec = 0;

		memset(min_lat_str, 0, FLOAT_STR_SIZE);
		memset(avg_lat_str, 0, FLOAT_STR_SIZE);
		memset(max_lat_str, 0, FLOAT_STR_SIZE);
		sprintf(min_lat_str, "%4.3f",
			(float)(wkr[i].min_iter_time.tv_nsec/divisor)/1000.0); 
		sprintf(avg_lat_str, "%4.3f", (float)avg_nsec/1000.0);
		sprintf(max_lat_str, "%4.3f",
			(float)(wkr[i].max_iter_time.tv_nsec/divisor)/1000.0); 

		sprintf(env->output, "%1d %3s %16ld %16s %16s %16s\n",
			i,  THREAD_STR(wkr[i].stat),
			wkr[i].perf_iter_cnt,
			min_lat_str, avg_lat_str, max_lat_str);
        	logMsg(env);
	};

        return 0;
};

struct cli_cmd Lat = {
"lat",
3,
0,
"Print current latency for threads.",
"lat {No Parms}\n",
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
        "\nW STS CPU RUN ACTION MODE DID <<<<--ADDR-->>>> ByteCnt AccSize W H OB IB MB\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%1d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		sprintf(env->output,
			"%6s %4s %3d %16lx %7lx %7lx %1d %1d %2d %2d %2d\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), wkr[i].did,
			wkr[i].rio_addr, wkr[i].byte_cnt, wkr[i].acc_size, 
			wkr[i].wr, wkr[i].mp_h_is_mine,
			wkr[i].ob_valid, wkr[i].ib_valid, 
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
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
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
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
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

static inline int MIN(int a, int b) { return a < b? a: b; }

int SetCmd(struct cli_env *env, int argc, char **argv)
{
	if(argc == 0) {
		if (SET_VARS.size() == 0) { INFO("\n\tNo env vars\n"); return 0; }

		std::stringstream ss;
		std::map<std::string, std::string>::iterator it = SET_VARS.begin();
		for(; it != SET_VARS.end(); it++) {
			ss << "\n\t" << it->first << "=" << it->second;
		}

		CRIT("\nEnv vars: %s\n", ss.str().c_str());
		goto exit;
	}

	assert(argv[0]);
	if (argc == 1) { // Delete var
		std::map<std::string, std::string>::iterator it = SET_VARS.find(argv[0]);
		if (it == SET_VARS.end()) { INFO("\n\tNo such env var '%s'\n", argv[0]); return 0; }
		SET_VARS.erase(it);
		goto exit;
	}

	// Set var
	assert(argv[1]);

	do {{
	  int start = 1;
	  if (! strcmp(argv[1], "?")) { // "set key ? val" do not assign val if var "key" exists
		if (GetEnv(argv[0])) break;
		start = 2;	
	  }

	  char buf[4097] = {0};
	  for (int i = start; i < argc; i++) { strncat(buf, argv[i], 4096);  strncat(buf, " ", 4096); }

	  const int N = strlen(buf);
	  buf[N - 1] = '\0';
	  SET_VARS[ argv[0] ] = buf;
	}} while(0);
exit:
	return 0;
}

struct cli_cmd Set = {
"set",
3,
0,
"Set/display environment vars",
"set {var {val}}\n"
        "\"set key\" deletes the variable from env\n"
        "\"set key val ...\" sets key:=val\n"
        "\"set key ? val ...\" sets key:=val iff key does not exist\n"
        "Note: val can be multiple words\n"
        "Default is display env vars.\n",
SetCmd,
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
		idx = GetDecParm(argv[0], 0);
		base_offset = GetHex(argv[1], 0);
		size = GetHex(argv[2], 0);
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
                        sprintf(env->output,"\n%8lx", base_offset + offset);
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

#ifdef USER_MODE_DRIVER

int UCalCmd(struct cli_env *env, int argc, char **argv)
{
	int n = 0, idx, chan, map_sz, sy_iter, hash = 0;

	idx = GetDecParm(argv[n++], 0);
	if (check_idx(env, idx, 1))
		goto exit;

	chan = GetDecParm(argv[n++], 0);
	map_sz = GetHex(argv[n++], 0);
	sy_iter = GetHex(argv[n++], 0);
	hash = GetDecParm(argv[n++], 0);
	
	if ((chan < 1) || (chan > 7)) {
                sprintf(env->output, "Chan %d illegal, must be 1 to 7\n", chan);
        	logMsg(env);
		goto exit;
	};

	if ((map_sz < 32) || (map_sz > 0x800000) || (map_sz & (map_sz-1))) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
			map_sz, MAX_UMD_BUF_COUNT);
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = umd_calibrate;
	wkr[idx].action_mode = user_mode_action;
	wkr[idx].umd_chan = chan;
	wkr[idx].umd_tx_buf_cnt = map_sz;
	wkr[idx].umd_sts_entries = sy_iter;
	wkr[idx].wr = hash;
	
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
	return 0;
};

struct cli_cmd UCal = {
"ucal",
4,
5,
"Calibrate performance of various facilities.",
"<idx> <chan> <map_sz> <sy_iter> <hash>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<chan> is a DMA channel number from 1 through 7\n"
	"<map_sz> is number of entries in a map to test\n"
	"<sy_iter> is the number of times to perform sched_yield and other\n"
	"       operating system related testing.\n"
	"<hash> If non-zero, attempt hash calibration.\n",

UCalCmd,
ATTR_NONE
};


int UTimeCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, st_i = 0, end_i = MAX_TIMESTAMPS-1;
	struct timespec *ts_p = NULL;
	int *ts_idx = NULL;
	uint64_t lim = 0;
	int got_one = 0;
	struct timespec diff, min, max, tot;

	idx = GetDecParm(argv[0], 0);
	if (check_idx(env, idx, 0))
		goto exit;

	switch (argv[1][0]) {
	case 'd':
	case 'D':
		ts_p = wkr[idx].desc_ts;
		ts_idx = &wkr[idx].desc_ts_idx;
		break;
	case 'f':
	case 'F':
		ts_p = wkr[idx].fifo_ts;
		ts_idx = &wkr[idx].fifo_ts_idx;
		break;
	default:
                sprintf(env->output, "FAILED: <type> not 'd' or 'f'.\n");
        	logMsg(env);
		goto exit;
	};
		
	switch (argv[2][0]) {
	case 's':
	case 'S':
		for (idx = 0; idx < MAX_TIMESTAMPS; idx++)
			ts_p[idx].tv_nsec = ts_p[idx].tv_sec = 0;
		*ts_idx = 0;
		break;
	case '-':
		if (argc > 4) {
			st_i = GetDecParm(argv[3], 0);
			end_i = GetDecParm(argv[4], 0);
		} else {
                	sprintf(env->output,
				"\nFAILED: Must enter two idexes\n");
        		logMsg(env);
			goto exit;
		};

		if ((end_i < st_i) || (st_i < 0) || (end_i >= MAX_TIMESTAMPS)) {
                	sprintf(env->output, "FAILED: Index range 0 to %d.\n",
				MAX_TIMESTAMPS-1);
        		logMsg(env);
			goto exit;
		};

		if (*ts_idx < MAX_TIMESTAMPS - 1) {
                	sprintf(env->output,
				"\nWARNING: Last valid timestamp is %d\n",
				*ts_idx);
        		logMsg(env);
		};
		diff = time_difference(ts_p[st_i], ts_p[end_i]);
                sprintf(env->output, "\n---->> Sec<<---- Nsec---m--u--n--\n");
        	logMsg(env);
                sprintf(env->output, "%16ld %16ld\n",
				diff.tv_sec, diff.tv_nsec);
        	logMsg(env);
		break;

	case 'p':
	case 'P':
		if (argc > 3)
			st_i = GetDecParm(argv[3], 0);
		if (argc > 4)
			end_i = GetDecParm(argv[4], 0);

		if ((end_i < st_i) || (st_i < 0) || (end_i >= MAX_TIMESTAMPS)) {
                	sprintf(env->output, "FAILED: Index range 0 to %d.\n",
				MAX_TIMESTAMPS-1);
        		logMsg(env);
			goto exit;
		};

		if (*ts_idx < MAX_TIMESTAMPS - 1) {
                	sprintf(env->output,
				"\nWARNING: Last valid timestamp is %d\n",
				*ts_idx);
        		logMsg(env);
		};

                sprintf(env->output,
			"\n Idx ---->> Sec<<---- Nsec---m--u--n--\n");
        	logMsg(env);
		for (idx = st_i; idx <= end_i; idx++) {
                	sprintf(env->output, "%4d %16ld %16ld\n", idx,
				ts_p[idx].tv_sec, ts_p[idx].tv_nsec);
        		logMsg(env);
		};
		break;
			
	case 'l':
	case 'L':
		if (argc > 3)
			lim = GetDecParm(argv[3], 0);
		else
               		lim = 0;

		for (idx = st_i; idx < end_i; idx++) {
			time_track(idx, ts_p[idx], ts_p[idx+1],
				&tot, &min, &max);
			diff = time_difference(ts_p[idx], ts_p[idx+1]);
			if (diff.tv_nsec < lim)
				continue;
			if (!got_one) {
                		sprintf(env->output,
				"\n Idx ---->> Sec<<---- Nsec---m--u--n--\n");
        			logMsg(env);
				got_one = 1;
			};
                	sprintf(env->output, "%4d %16ld %16ld\n", idx,
				diff.tv_sec, diff.tv_nsec);
        		logMsg(env);
		};

		if (!got_one) {
                	sprintf(env->output,
				"\nNo delays found bigger than %d\n", lim);
        		logMsg(env);
		};
                sprintf(env->output,
			"\n==== ---->> Sec<<---- Nsec---m--u--n--\n");
        	logMsg(env);
                sprintf(env->output, "Min: %16ld %16ld\n", 
				min.tv_sec, min.tv_nsec);
        	logMsg(env);
		diff = time_div(tot, end_i - st_i);
                sprintf(env->output, "Avg: %16ld %16ld\n",
				diff.tv_sec, diff.tv_nsec);
        	logMsg(env);
                sprintf(env->output, "Max: %16ld %16ld\n",
				max.tv_sec, max.tv_nsec);
        	logMsg(env);
		break;
	default:
                sprintf(env->output, "FAILED: <cmd> not 's','p' or 'l'.\n");
        	logMsg(env);
	};
exit:
        return 0;
};

struct cli_cmd UTime = {
"utime",
2,
3,
"UMD Timestamp buffer command",
"<idx> <type> <cmd> <parms>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<type> is 'd' for descriptor timestamps, 'f' for fifo.\n"
	"<cmd> is the command to perform on the buffer, one of:\n"
	"      's' - sample timestamps again\n"
	"      '-' - return difference in two timestamp idices\n"
	"            Note: Must enter two timestamp indexes\n"
	"      'p' - print the existing counter values\n"
	"            Note: optionally enter start and end indexes.\n"
	"      'l' - locate differences greater than x nsec\n"
	"            Note: Must enter the number of nanoseconds in decimal.\n",
UTimeCmd,
ATTR_NONE
};

int UDMACmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int chan;
	int cpu;
	uint32_t buff;
	uint32_t sts;
	uint32_t did;
	uint64_t rio_addr;
	uint32_t bytes;
	uint32_t acc_sz;
	int trans;

        int n = 0; // this be a trick from X11 source tree ;)

	idx      = GetDecParm(argv[n++], 0);
	if (get_cpu(env, argv[n++], &cpu))
		goto exit;

	chan     = GetDecParm(argv[n++], 0);
	buff     = GetHex(argv[n++], 0);
	sts      = GetHex(argv[n++], 0);
	did      = GetDecParm(argv[n++], 0);
	rio_addr = GetHex(argv[n++], 0);
	bytes    = GetHex(argv[n++], 0);
	acc_sz   = GetHex(argv[n++], 0);
	trans    = GetDecParm(argv[n++], 0);

	if (check_idx(env, idx, 1))
		goto exit;

	if ((chan < 1) || (chan > 7)) {
                sprintf(env->output, "Chan %d illegal, must be 1 to 7\n", chan);
        	logMsg(env);
		goto exit;
	};

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff-1)) ||
			(buff > MAX_UMD_BUF_COUNT)) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
			buff, MAX_UMD_BUF_COUNT);
        	logMsg(env);
		goto exit;
	};

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts-1))) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
			sts);
        	logMsg(env);
		goto exit;
	};

	if (!rio_addr || !acc_sz || !bytes) {
                sprintf(env->output,
			"Addr, bytes and acc_size must be non-zero\n");
        	logMsg(env);
		goto exit;
	};

	if ((trans < 0) || (trans > 5)) {
                sprintf(env->output,
			"Illegal trans %d, must be between 0 and 5\n", trans);
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = umd_dma;
	wkr[idx].action_mode = user_mode_action;
	wkr[idx].umd_chan = chan;
	wkr[idx].umd_fifo_thr.cpu_req = cpu;
	wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
	wkr[idx].umd_tx_buf_cnt = buff;
	wkr[idx].umd_sts_entries = sts;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].umd_tx_rtype = (enum dma_rtype)trans;
	wkr[idx].wr = (NREAD != wkr[idx].umd_tx_rtype);
	wkr[idx].use_kbuf = 1;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
        return 0;
};

struct cli_cmd UDMA = {
"udma",
4,
10,
"Transmit DMA requests with User-Mode demo driver",
"<idx> <cpu> <chan> <buff> <sts> <did> <rio_addr> <bytes> <acc_sz> <trans>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<chan> is a DMA channel number from 1 through 7\n"
	"<buff> is the number of transmit descriptors/buffers to allocate\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<sts> is the number of status entries for completed descriptors\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes> total bytes to transfer\n"
	"<acc_sz> Access size\n"
	"<trans>  0 NREAD, 1 LAST_NWR, 2 NW, 3 NW_R\n",
UDMACmd,
ATTR_NONE
};

int UDMALatTxRxCmd(const char cmd, struct cli_env *env, int argc, char **argv)
{
	int idx;
	int chan;
	int cpu;
	uint32_t buff;
	uint32_t sts;
	uint32_t did;
	uint64_t rio_addr;
	uint32_t acc_sz;
	int trans;

        int n = 0; // this be a trick from X11 source tree ;)

	idx      = GetDecParm(argv[n++], 0);
	if (get_cpu(env, argv[n++], &cpu))
		goto exit;
	chan     = GetDecParm(argv[n++], 0);
	buff     = GetHex(argv[n++], 0);
	sts      = GetHex(argv[n++], 0);
	did      = GetDecParm(argv[n++], 0);
	rio_addr = GetHex(argv[n++], 0);
	acc_sz   = GetHex(argv[n++], 0);
	trans    = GetDecParm(argv[n++], 0);

	if (cmd != 'R' && cmd != 'T') {
                sprintf(env->output, "Command '%c' illegal, this should never happen\n", cmd);
        	logMsg(env);
		goto exit;
	};

	if (check_idx(env, idx, 1))
		goto exit;

	if ((chan < 1) || (chan > 7)) {
                sprintf(env->output, "Chan %d illegal, must be 1 to 7\n", chan);
        	logMsg(env);
		goto exit;
	};

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff-1)) ||
			(buff > MAX_UMD_BUF_COUNT)) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
			buff, MAX_UMD_BUF_COUNT);
        	logMsg(env);
		goto exit;
	};

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts-1))) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
			sts);
        	logMsg(env);
		goto exit;
	};

	if (!rio_addr || !acc_sz) {
                sprintf(env->output,
			"Addr and acc_size must be non-zero\n");
        	logMsg(env);
		goto exit;
	};

	if ((trans < 1) || (trans > 5)) {
                sprintf(env->output,
			"Illegal trans %d, must be between 1 and 5 (NREAD=0 disallowed)\n", trans);
        	logMsg(env);
		goto exit;
	};

	if (! wkr[idx].ib_valid) {
		sprintf(env->output, "IBwin not allocated for this worker thread!\n");
		logMsg(env);
		goto exit;
	}
	if (wkr[idx].ib_byte_cnt < acc_sz) {
		sprintf(env->output, "IBwin too small (0x%x) must be at least 0x%x\n", wkr[idx].ib_byte_cnt, acc_sz);
		logMsg(env);
		goto exit;
	}

	wkr[idx].action = (cmd == 'T') ? umd_dmaltx: umd_dmalrx;
	wkr[idx].action_mode = user_mode_action;
	wkr[idx].umd_chan = chan;
	wkr[idx].umd_fifo_thr.cpu_req = cpu;
	wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
	wkr[idx].umd_tx_buf_cnt = buff;
	wkr[idx].umd_sts_entries = sts;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = 0;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].umd_tx_rtype = (enum dma_rtype)trans;
	wkr[idx].wr = 1;
	wkr[idx].use_kbuf = 1;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
	return 0;
}
static int UDMALatTxCmd(struct cli_env *env, int argc, char **argv) { return UDMALatTxRxCmd('T', env, argc, argv); }
static int UDMALatRxCmd(struct cli_env *env, int argc, char **argv) { return UDMALatTxRxCmd('R', env, argc, argv); }

struct cli_cmd UDMALTX = {
"ltudma",
6,
9,
"Latency of DMA requests with User-Mode demo driver - Master",
"<idx> <cpu> <chan> <buff> <sts> <did> <rio_addr> <acc_sz> <trans>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<chan> is a DMA channel number from 1 through 7\n"
	"<buff> is the number of transmit descriptors/buffers to allocate\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<sts> is the number of status entries for completed descriptors\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<acc_sz> Access size\n"
	"<trans>  1 LAST_NWR, 2 NW, 3 NW_R\n"
	"NOTE:  IBAlloc of size >= acc_sz needed before running this command\n",
UDMALatTxCmd,
ATTR_NONE
};
struct cli_cmd UDMALRX = {
"lrudma",
6,
9,
"Latency of DMA requests with User-Mode demo driver - Slave",
"<idx> <cpu> <chan> <buff> <sts> <did> <rio_addr> <acc_sz> <trans>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<chan> is a DMA channel number from 1 through 7\n"
	"<buff> is the number of transmit descriptors/buffers to allocate\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<sts> is the number of status entries for completed descriptors\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<did> target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<acc_sz> Access size\n"
	"<trans>  1 LAST_NWR, 2 NW, 3 NW_R\n"
	"NOTE:  IBAlloc of size >= acc_sz needed before running this command\n",
UDMALatRxCmd,
ATTR_NONE
};

int UDMALatNREAD(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int chan;
	int cpu;
	uint32_t buff;
	uint32_t sts;
	uint32_t did;
	uint64_t rio_addr;
	uint32_t acc_sz;

        int n = 0; // this be a trick from X11 source tree ;)

	idx      = GetDecParm(argv[n++], 0);
	if (get_cpu(env, argv[n++], &cpu))
		goto exit;
	chan     = GetDecParm(argv[n++], 0);
	buff     = GetHex(argv[n++], 0);
	sts      = GetHex(argv[n++], 0);
	did      = GetDecParm(argv[n++], 0);
	rio_addr = GetHex(argv[n++], 0);
	acc_sz   = GetHex(argv[n++], 0);

	if (check_idx(env, idx, 1))
		goto exit;

	if ((chan < 1) || (chan > 7)) {
                sprintf(env->output, "Chan %d illegal, must be 1 to 7\n", chan);
        	logMsg(env);
		goto exit;
	};

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff-1)) ||
			(buff > MAX_UMD_BUF_COUNT)) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
			buff, MAX_UMD_BUF_COUNT);
        	logMsg(env);
		goto exit;
	};

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts-1))) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
			sts);
        	logMsg(env);
		goto exit;
	};

	if (!rio_addr || !acc_sz) {
                sprintf(env->output,
			"Addr and acc_size must be non-zero\n");
        	logMsg(env);
		goto exit;
	};

	wkr[idx].action = umd_dmalnr;
	wkr[idx].action_mode = user_mode_action;
	wkr[idx].umd_chan = chan;
	wkr[idx].umd_fifo_thr.cpu_req = cpu;
	wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
	wkr[idx].umd_tx_buf_cnt = buff;
	wkr[idx].umd_sts_entries = sts;
	wkr[idx].did = did;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = 0;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].umd_tx_rtype = NREAD;
	wkr[idx].wr = 0;
	wkr[idx].use_kbuf = 1;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);
exit:
	return 0;
}

struct cli_cmd UDMALRR = {
"nrudma",
6,
8,
"Latency of DMA requests with User-Mode demo driver - NREAD",
"<idx> <cpu> <chan> <buff> <sts> <did> <rio_addr> <acc_sz>\n"
        "<idx> is a worker index from 0 to 7\n"
        "<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
        "<chan> is a DMA channel number from 1 through 7\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts> is the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<did> target device ID\n"
        "<rio_addr> RapidIO memory address to access\n"
        "<acc_sz> Access size\n"
        "NOTE:  IBAlloc on <did> of size >= acc_sz needed before running this command\n",
UDMALatNREAD,
ATTR_NONE
};

extern void UMD_DD(const struct worker* wkr);

int UMDDDDCmd(struct cli_env *env, int argc, char **argv)
{
	int idx = GetDecParm(argv[0], 0);
	if (idx < 0 || idx >= MAX_WORKERS) {
                sprintf(env->output, "Bad idx %d\n", idx);
        	logMsg(env);
		goto exit;
	}
	UMD_DD(&wkr[idx]);

exit:
	return 0;
}
struct cli_cmd UMDDD = {
"udd",
1,
1,
"Dump UMD misc counters",
"<idx>\n"
	"<idx> is a worker index from 0 to 7\n",
UMDDDDCmd,
ATTR_NONE
};

int UMSGCmd(const char cmd, struct cli_env *env, int argc, char **argv)
{
        int idx;
        int chan;
        int chan_to;
        int cpu;
        uint32_t buff;
        uint32_t sts;
        uint32_t did;
        uint32_t acc_sz;
        int txrx = 0;

        int n = 0; // this be a trick from X11 source tree ;)

        idx      = GetDecParm(argv[n++], 0);
	if (get_cpu(env, argv[n++], &cpu))
		goto exit;
        chan     = GetDecParm(argv[n++], 0);
        chan_to  = GetDecParm(argv[n++], 0);
        buff     = GetHex(argv[n++], 0);
        sts      = GetHex(argv[n++], 0);
        did      = GetDecParm(argv[n++], 0);
        acc_sz   = GetHex(argv[n++], 0);
        txrx     = GetDecParm(argv[n++], 0);

        if (cmd != 'L' && cmd != 'T') {
                sprintf(env->output, "Command '%c' illegal, this should never happen\n", cmd);
                logMsg(env);
                goto exit;
        };

        if (check_idx(env, idx, 1))
                goto exit;

        if ((chan < 2) || (chan > 3)) {
                sprintf(env->output, "Chan %d illegal, must be 2 to 3\n", chan);
                logMsg(env);
                goto exit;
        };
        if ((chan_to < 2) || (chan_to > 3)) {
                sprintf(env->output, "Chan_to %d illegal, must be 2 to 3\n", chan);
                logMsg(env);
                goto exit;
        };


	if ((buff < 32) || (buff > 0x800000) || (buff & (buff-1)) ||
			(buff > MAX_UMD_BUF_COUNT)) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
			buff, MAX_UMD_BUF_COUNT);
        	logMsg(env);
		goto exit;
	};

        if ((sts < 32) || (sts > 0x800000) || (sts & (sts-1))) {
                sprintf(env->output,
                        "Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
                        sts);
                logMsg(env);
                goto exit;
        };
	if (acc_sz < 1 || acc_sz > 4096) {
                sprintf(env->output,
                        "Bad acc_sz %x, must be 1..4096\n",
                        sts);
                logMsg(env);
                goto exit;
	}

        wkr[idx].action = (cmd == 'T')? umd_mbox: umd_mboxl;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan    = chan;
        wkr[idx].umd_chan_to = chan_to;
        wkr[idx].umd_fifo_thr.cpu_req = cpu;
        wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
        wkr[idx].umd_tx_buf_cnt = buff;
        wkr[idx].umd_sts_entries = sts;
        wkr[idx].did = did;
        wkr[idx].rio_addr = 0;
        wkr[idx].byte_cnt = 0;
        wkr[idx].acc_size = acc_sz;
        wkr[idx].umd_tx_rtype = (enum dma_rtype)-1;
        wkr[idx].wr = !!txrx;
        wkr[idx].use_kbuf = 1;

        wkr[idx].stop_req = 0;
        sem_post(&wkr[idx].run);
exit:
        return 0;
}

int UMSGCmdThruput(struct cli_env *env, int argc, char **argv)
{
	return UMSGCmd('T', env, argc, argv);
}
int UMSGCmdLat(struct cli_env *env, int argc, char **argv)
{
	return UMSGCmd('L', env, argc, argv);
}

struct cli_cmd UMSG = {
"umsg",
2,
9,
"Transmit/Receive MBOX requests with User-Mode demo driver",
"<idx> <cpu> <chan> <buff> <sts> <did> <acc_sz> <txrx>\n"
	"<idx> is a worker index from 0 to 7\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<chan> is a Local MBOX channel number from 2 through 3\n"
	"<chan_to> is a Remote MBOX channel number from 2 through 3\n"
	"<buff> is the number of transmit descriptors/buffers to allocate\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<sts> is the number of status entries for completed descriptors\n"
	"       Must be a power of two from 0x20 up to 0x80000\n"
	"<did> target device ID (if Transmitting) -- ignored for RX\n"
	"<acc_sz> Access size (if Transmitting)\n"
	"<txrx>  0 RX, 1 TX\n",
UMSGCmdThruput,
ATTR_NONE
};

struct cli_cmd UMSGL = {
"lumsg",
5,
8,
"Latency of MBOX requests with User-Mode demo driver",
"<idx> <cpu> <chan> <buff> <sts> <did> <acc_sz> <txrx>\n"
        "<idx> is a worker index from 0 to 7\n"
        "<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
        "<chan> is a MBOX channel number from 2 through 3\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts> is the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<did> target device ID (if Transmitting) -- ignored for RX\n"
        "<acc_sz> Access size (if Transmitting)\n"
        "<txrx>  0 Slave, 1 Master\n",
UMSGCmdLat,
ATTR_NONE
};

#endif

struct cli_cmd *goodput_cmds[] = {
	&IBAlloc,
	&IBDealloc,
	&Dump,
	&OBDIO,
	&OBDIOTxLat,
	&OBDIORxLat,
	&dmaTxLat,
	&dmaRxLat,
	&dma,
#ifdef USER_MODE_DRIVER
	&UCal,
	&UDMA,
	&UDMALRR,
	&UDMALTX,
	&UDMALRX,
	&UMSG,
	&UMSGL,
	&UMDDD,
	&UTime,
#endif
	&msgTx,
	&msgRx,
	&msgTxLat,
	&msgRxLat,
	&Goodput,
	&Lat,
	&Status,
	&Set,
	&Thread,
	&Kill,
	&Halt,
	&Move,
	&Wait,
	&Sleep,
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
