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
#include <vector>
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
        (char*)"UDMATun",
        (char*)"UMSG",
        (char*)"UMSGLat",
        (char*)"UMSGTun",
        (char*)"EPWa",
        (char*)"UMSGWa",
#endif
	(char *)"LAST"
};

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

int get_cpu(struct cli_env *env, char *dec_parm, int *cpu)
{
	int rc = 1;

	*cpu = GetDecParm(dec_parm, 0);

	const int MAX_GOODPUT_CPU = getCPUCount() - 1;

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

	idx = getDecParm(argv[0], 0);
	if (get_cpu(env, argv[1], &cpu))
		goto exit;
	new_dma = getDecParm(argv[2], 0);

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

#define HACK(x) #x
#define STR(x) HACK(x)

struct cli_cmd Thread = {
"thread",
1,
3,
"Start a thread on a cpu",
"start <idx> <cpu> <new_dma>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<new_dma> If <> 0, open mport again to get a new DMA channel\n",
ThreadCmd,
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) ", or \"all\"\n",
KillCmd,
ATTR_NONE
};

int HaltCmd(struct cli_env *env, int argc, char **argv)
{
	unsigned int st_idx = 0, end_idx = MAX_WORKERS-1, i;

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) ", or \"all\"\n",
HaltCmd,
ATTR_NONE
};

int MoveCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, cpu;

	idx = getDecParm(argv[0], 0);
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX)",\n" 
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n",
MoveCmd,
ATTR_NONE
};

int WaitCmd(struct cli_env *env, int argc, char **argv)
{
	int idx, state = -1, limit = 10000;
	const struct timespec ten_usec = {0, 10 * 1000};

	idx = getDecParm(argv[0], 0);
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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

	idx = getDecParm(argv[0], 0);
	ib_size = getHex(argv[1], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<size> is a hexadecimal power of two from 0x1000 to 0x01000000\n",
IBAllocCmd,
ATTR_NONE
};

int IBDeallocCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;

	idx = getDecParm(argv[0], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n",
IBDeallocCmd,
ATTR_NONE
};


#define PROC_STAT_PFMT "\nTot CPU Jiffies %lu %lu %lu %lu %lu %lu %lu\n"

#define CPUOCC_BUFF_SIZE 1024

void cpu_occ_parse_proc_line(char *file_line, 
				uint64_t *proc_new_utime,
				uint64_t *proc_new_stime)
{
	char *tok;
	char *saveptr;
	char *delim = (char *)" ";
	int tok_cnt = 0;
	char fl_cpy[CPUOCC_BUFF_SIZE];
	
	strncpy(fl_cpy, file_line, CPUOCC_BUFF_SIZE-1);
	tok = strtok_r(file_line, delim, &saveptr);
	while ((NULL != tok) && (tok_cnt < 13)) {
		tok = strtok_r(NULL, delim, &saveptr);
		tok_cnt++;
	};

	if (NULL == tok)
		goto error;
	
	*proc_new_utime = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;

	*proc_new_stime = atoll(tok);
	return;
error:
	ERR("\nFAILED: proc_line \"%s\"\n", fl_cpy);
};

void cpu_occ_parse_stat_line(char *file_line,
			uint64_t *p_user,
			uint64_t *p_nice,
			uint64_t *p_system,
			uint64_t *p_idle,
			uint64_t *p_iowait,
			uint64_t *p_irq,
			uint64_t *p_softirq)
{
	char *tok, *saveptr;
	char *delim = (char *)" ";
	char fl_cpy[CPUOCC_BUFF_SIZE];
	
	strncpy(fl_cpy, file_line, CPUOCC_BUFF_SIZE-1);
	
	/* Throw the first token away. */
	tok = strtok_r(file_line, delim, &saveptr);

	if (NULL == tok)
		goto error;
	
	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_user = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_nice = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_system = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_idle = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_iowait = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_irq = atoll(tok);

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	*p_softirq = atoll(tok);
	
	return;
error:
	ERR("\nFAILED: stat_line \"%s\"\n", fl_cpy);
};

int cpu_occ_valid;

uint64_t old_tot_jifis;
uint64_t old_proc_kern_jifis;
uint64_t old_proc_user_jifis;
uint64_t new_tot_jifis;
uint64_t new_proc_kern_jifis;
uint64_t new_proc_user_jifis;

float    cpu_occ_pct;

int cpu_occ_set(uint64_t *tot_jifis, 
		uint64_t *proc_kern_jifis,
		uint64_t *proc_user_jifis)
{
	pid_t my_pid = getpid();
	FILE *stat_fp, *cpu_stat_fp;
	char filename[256];
	char file_line[CPUOCC_BUFF_SIZE];
	uint64_t p_user = 1, p_nice = 1, p_system = 1, p_idle = 1;
	uint64_t p_iowait = 1, p_irq = 1, p_softirq = 1;
	int rc;

	memset(filename, 0, 256);
	snprintf(filename, 255, "/proc/%d/stat", my_pid);

	stat_fp = fopen(filename, "r" );
	if (NULL == stat_fp) {
		ERR( "FAILED: Open proc stat file \"%s\": %d %s\n",
			filename, errno, strerror(errno));
		goto exit;
	};

	cpu_stat_fp = fopen("/proc/stat", "r");
	if (NULL == cpu_stat_fp) {
		ERR("FAILED: Open file \"/proc/stat\": %d %s\n",
			errno, strerror(errno));
		goto exit;
	};

	memset(file_line, 0, 1024);
	fgets(file_line, 1024,  stat_fp);

	cpu_occ_parse_proc_line(file_line, proc_user_jifis, proc_kern_jifis);

		
	memset(file_line, 0, 1024);
	fgets(file_line, 1024,  cpu_stat_fp);

	cpu_occ_parse_stat_line(file_line, &p_user, &p_nice, &p_system,
			&p_idle, &p_iowait, &p_irq, &p_softirq);

	*tot_jifis = p_user + p_nice + p_system + p_idle +
			p_iowait + p_irq + p_softirq;
	fclose(stat_fp);
	fclose(cpu_stat_fp);
	
	rc = 0;
exit:
	return rc;
};

int CPUOccSetCmd(struct cli_env *env, int argc, char **argv)
{

	if (cpu_occ_set(&old_tot_jifis, &old_proc_kern_jifis,
			&old_proc_user_jifis)) {
		sprintf(env->output, "\nFAILED: Could not get proc info \n");
        	logMsg(env);
		goto exit;
	};
	sprintf(env->output, "\nSet CPU Occ measurement start point\n");
        logMsg(env);

	cpu_occ_valid = 1;
exit:
        return 0;
};

struct cli_cmd CPUOccSet = {
"oset",
2,
0,
"Set CPU Occupancy measurement start point.",
"ost\n"
	"No parameters\n", 
CPUOccSetCmd,
ATTR_NONE
};

int cpu_occ_saved_idx;

int CPUOccDisplayCmd(struct cli_env *env, int argc, char **argv)
{
	char pctg[24];
	int cpus = getCPUCount();

	if (!cpus)
		cpus = 1;

	if (!cpu_occ_valid) {
		sprintf(env->output,
			"\nFAILED: CPU OCC measurement start not set\n");
        	logMsg(env);
		goto exit;
	};

	if (cpu_occ_set(&new_tot_jifis, &new_proc_kern_jifis,
			&new_proc_user_jifis)) {
		sprintf(env->output, "\nFAILED: Could not get proc info \n");
        	logMsg(env);
		goto exit;
	};


	cpu_occ_pct = (((float)(new_proc_kern_jifis + new_proc_user_jifis -
				 old_proc_kern_jifis - old_proc_user_jifis)) /
		((float)(new_tot_jifis - old_tot_jifis))) * 100.0 * cpus;
	sprintf(pctg, "%4.2f", cpu_occ_pct);

	sprintf(env->output, "\n-Kernel- ProcUser ProcKern CPU_Occ\n");
        logMsg(env);
	sprintf(env->output, "%8ld %8ld %8ld %7s\n",
		new_tot_jifis - old_tot_jifis,
		new_proc_user_jifis - old_proc_user_jifis,
		new_proc_kern_jifis - old_proc_kern_jifis,
		pctg);
        logMsg(env);
exit:
        return 0;
};

struct cli_cmd CPUOccDisplay = {
"odisp",
3,
0,
"Display cpu occupancy",
"odisp <idx>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n",
CPUOccDisplayCmd,
ATTR_RPT
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
	if (direct_io == action) {
		bytes = getHex(argv[3], 0);
		acc_sz = getHex(argv[4], 0);
		wr = getDecParm(argv[5], 0);
	} else {
		acc_sz = getHex(argv[3], 0);
		bytes = acc_sz;
		if (direct_io_tx_lat == action) 
			wr = getDecParm(argv[4], 0);
		else
			wr = 1;
	};
		
	if (direct_io_tx_lat == action) {
		if (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr)) {
			sprintf(env->output,
				"\nNo mapped inbound window present\n");
        		logMsg(env);
			goto exit;
		};
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);
	acc_sz = getHex(argv[4], 0);
	wr = getDecParm(argv[5], 0);
	kbuf = getDecParm(argv[6], 0);
	trans = getDecParm(argv[7], 0);
	sync = getDecParm(argv[8], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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

	if (check_idx(env, idx, 1))
		goto exit;

	if (wr && (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr))) {
		sprintf(env->output, "\nNo mapped inbound window present\n");
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
		wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;
	else
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

struct cli_cmd dmaTxLat = {
"dTxLat",
2,
7,
"Measure lantecy of DMA reads/writes",
"dTxLat <idx> <did> <rio_addr> <bytes> <wr> <kbuf> <trans>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	rio_addr = getHex(argv[2], 0);
	bytes = getHex(argv[3], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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

	idx = getDecParm(argv[0], 0);
	did = getDecParm(argv[1], 0);
	sock_num = getDecParm(argv[2], 0);
	bytes = getHex(argv[3], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size> bytes per message hex. Must be a multiple of 8.\n"
        "       Minimum 0x18, maximum 0x1000 (24 through 4096).\n"
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did> target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size> bytes per message hex. Must be a multiple of 8."
        "       Minimum 0x18, maximum 0x1000 (24 through 4096).\n"
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

	idx = getDecParm(argv[0], 0);
	sock_num = getDecParm(argv[1], 0);
	bytes = getHex(argv[2], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<sock_num> RapidIO Channelized Messaging channel number to accept\n"
	"<size> bytes per message hex. Must be a multiple of 8."
        "       Minimum 0x18, maximum 0x1000 (24 through 4096).\n"
	"NOTE: All parameters are decimal numbers.\n"
	"NOTE: mRxLat must be run before mTxLat!\n",
msgRxLatCmd,
ATTR_NONE
};

int msgRxCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	int sock_num;

	idx = getDecParm(argv[0], 0);
	sock_num = getDecParm(argv[1], 0);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<sock_num> Target socket number for connections from msgTx command\n"
	"NOTE: msgRx must be running before msgTx!\n",
msgRxCmd,
ATTR_NONE
};

#define FLOAT_STR_SIZE 20

int GoodputCmd(struct cli_env *env, int argc, char **argv)
{
	int i;
	float MBps, Gbps, Msgpersec, link_occ; 
	uint64_t byte_cnt = 0;
	float tot_MBps = 0, tot_Gbps = 0, tot_Msgpersec = 0;
	uint64_t tot_byte_cnt = 0;
	char MBps_str[FLOAT_STR_SIZE],  Gbps_str[FLOAT_STR_SIZE];
	char link_occ_str[FLOAT_STR_SIZE];

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
        "\n W STS <<<<--Data-->>>> --MBps-- -Gbps- Messages  Link_Occ\n");
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
		link_occ = Gbps/0.95;

		memset(MBps_str, 0, FLOAT_STR_SIZE);
		memset(Gbps_str, 0, FLOAT_STR_SIZE);
		memset(link_occ_str, 0, FLOAT_STR_SIZE);
		sprintf(MBps_str, "%4.3f", MBps);
		sprintf(Gbps_str, "%2.3f", Gbps);
		sprintf(link_occ_str, "%2.3f", link_occ);

		sprintf(env->output, "%2d %3s %16lx %8s %6s %9.0f  %6s\n",
			i,  THREAD_STR(wkr[i].stat),
			byte_cnt, MBps_str, Gbps_str, Msgpersec, link_occ_str);
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
	memset(link_occ_str, 0, FLOAT_STR_SIZE);
	sprintf(MBps_str, "%4.3f", tot_MBps);
	sprintf(Gbps_str, "%2.3f", tot_Gbps);
	link_occ = tot_Gbps/0.95;
	sprintf(link_occ_str, "%2.3f", link_occ);

	sprintf(env->output, "Total  %16lx %8s %6s %9.0f  %6s\n",
		tot_byte_cnt, MBps_str, Gbps_str, tot_Msgpersec, link_occ_str);
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
        "\n W STS <<<<-Count-->>>> <<<<Min uSec>>>> <<<<Avg uSec>>>> <<<<Max uSec>>>>\n");
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

		sprintf(env->output, "%2d %3s %16ld %16s %16s %16s\n",
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
        "\n W STS CPU RUN ACTION  MODE DID <<<<--ADDR-->>>> ByteCnt AccSize W H OB IB MB\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		sprintf(env->output,
			"%7s %4s %3d %16lx %7lx %7lx %1d %1d %2d %2d %2d\n",
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
	"\n W STS CPU RUN ACTION  MODE IB <<<< HANDLE >>>> <<<<RIO ADDR>>>> <<<<  SIZE  >>>\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		sprintf(env->output,
			"%7s %4s %2d %16lx %16lx %15lx\n",
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
	"\n W STS CPU RUN ACTION  MODE MB ACC CON Msg_Size SockNum TX RX\n");
        logMsg(env);

	for (i = 0; i < MAX_WORKERS; i++) {
		sprintf(env->output, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
        	logMsg(env);
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		sprintf(env->output,
			"%7s %4s %2d %3d %3d %8d %7d %2d %2d\n",
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
"status {i|m|s}\n"
        "Optionally enter a character to select the status type:\n"
        "i : IBWIN status\n"
        "m : Messaging status\n"
        "s : General status\n"
        "Default is general status.\n",
StatusCmd,
ATTR_RPT
};

static inline int MIN(int a, int b) { return a < b? a: b; }

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<offset> is the hexadecimal offset, in bytes, from the window start\n"
	"<size> is the number of bytes to display, starting at <offset>\n",
DumpCmd,
ATTR_RPT
};

int FillCmd(struct cli_env *env, int argc, char **argv)
{
	int idx;
	uint64_t offset, base_offset;
	uint64_t size;
	uint64_t data;

	idx = getDecParm(argv[0], 0);
	base_offset = getHex(argv[1], 0);
	size = getHex(argv[2], 0);
	data = getHex(argv[3], 0);

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

        for (offset = 0; offset < size; offset++) {
		*(volatile uint8_t * volatile)
		((uint8_t *)wkr[idx].ib_ptr + base_offset + offset) = data;
        };
exit:
        return 0;
};

struct cli_cmd Fill = {
"fill",
4,
4,
"Fill inbound memory area",
"Fill <idx> <offset> <size> <data>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<offset> is the hexadecimal offset, in bytes, from the window start\n"
	"<size> is the number of bytes to display, starting at <offset>\n"
	"<data> is the 8 bit value to write.\n",
FillCmd,
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	struct seq_ts *ts_p = NULL;
	uint64_t lim = 0;
	int got_one = 0;
	struct timespec diff, min, max, tot;

	idx = GetDecParm(argv[0], 0);
	if (check_idx(env, idx, 0))
		goto exit;

	switch (argv[1][0]) {
	case 'd':
	case 'D': 
		ts_p = &wkr[idx].desc_ts;
		break;
	case 'f':
	case 'F':
		ts_p = &wkr[idx].fifo_ts;
		break;
	case 'm':
	case 'M':
		ts_p = &wkr[idx].meas_ts;
		break;
	default:
                sprintf(env->output, "FAILED: <type> not 'd', 'f' or 'm'.\n");
        	logMsg(env);
		goto exit;
	};
		
	switch (argv[2][0]) {
	case 's':
	case 'S':
		init_seq_ts(ts_p);
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

		if (ts_p->ts_idx < MAX_TIMESTAMPS - 1) {
                	sprintf(env->output,
				"\nWARNING: Last valid timestamp is %d\n",
				ts_p->ts_idx);
        		logMsg(env);
		};
		diff = time_difference(ts_p->ts_val[st_i], ts_p->ts_val[end_i]);
                sprintf(env->output, "\n---->> Sec<<---- Nsec---MMMuuuNNN\n");
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

		if (ts_p->ts_idx < MAX_TIMESTAMPS - 1) {
                	sprintf(env->output,
				"\nWARNING: Last valid timestamp is %d\n",
				ts_p->ts_idx);
        		logMsg(env);
		};

                sprintf(env->output,
			"\n Idx ---->> Sec<<---- Nsec---mmmuuunnn Marker\n");
        	logMsg(env);
		for (idx = st_i; idx <= end_i; idx++) {
                	sprintf(env->output, "%4d %16ld %16ld %d\n", idx,
				ts_p->ts_val[idx].tv_sec, 
				ts_p->ts_val[idx].tv_nsec,
				ts_p->ts_mkr[idx]);
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
			time_track(idx, ts_p->ts_val[idx], ts_p->ts_val[idx+1],
				&tot, &min, &max);
			diff = time_difference(ts_p->ts_val[idx],
						ts_p->ts_val[idx+1]);
			if (diff.tv_nsec < lim)
				continue;
			if (!got_one) {
                		sprintf(env->output,
				"\n Idx ---->> Sec<<---- Nsec---MMMuuuNNN Marker\n");
        			logMsg(env);
				got_one = 1;
			};
                	sprintf(env->output, "%4d %16ld %16ld %d -> %d\n", idx,
				diff.tv_sec, diff.tv_nsec, 
				ts_p->ts_mkr[idx], ts_p->ts_mkr[idx+1]);
        		logMsg(env);
		};

		if (!got_one) {
                	sprintf(env->output,
				"\nNo delays found bigger than %d\n", lim);
        		logMsg(env);
		};
                sprintf(env->output,
			"\n==== ---->> Sec<<---- Nsec---MMMuuuNNN\n");
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<type> is:\n"
	"      'd' - descriptor timestamps\n"
	"      'f' - FIFO (descriptor completions)\n"
	"      'm' - measurement (development only)\n"
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
	wkr[idx].max_iter = GetDecParm("$maxit", -1);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	wkr[idx].max_iter = GetDecParm("$maxit", -1);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	wkr[idx].max_iter = GetDecParm("$maxit", -1);

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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
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
	int idx = argc > 0? GetDecParm(argv[0], 0): 0;
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
0,
"Dump UMD misc counters",
"<idx>\n"
	"<idx> [optional, default=0] is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n",
UMDDDDCmd,
ATTR_NONE
};

int UDMACmdTun(struct cli_env *env, int argc, char **argv)
{
        int idx;
        int chan;
        int chan_n;
	int chan2;
        int cpu;
        uint32_t buff;
        uint32_t sts;
	int mtu;

        int n = 0; // this be a trick from X11 source tree ;)

        idx      = GetDecParm(argv[n++], 0);
        if (get_cpu(env, argv[n++], &cpu))
                goto exit;
        chan     = GetDecParm(argv[n++], 0);
        chan_n   = GetDecParm(argv[n++], 0);
        chan2     = GetDecParm(argv[n++], 0);
        buff     = GetHex(argv[n++], 0);
        sts      = GetHex(argv[n++], 0);
        mtu      = GetDecParm(argv[n++], 0);

        if (check_idx(env, idx, 1))
                goto exit;

        if ((chan < 1) || (chan > 7)) {
                sprintf(env->output, "Chan_1 %d illegal, must be 1 to 7\n", chan);
                logMsg(env);
                goto exit;
        };
        if ((chan_n < 1) || (chan_n > 7)) {
                sprintf(env->output, "Chan_n %d illegal, must be 1 to 7\n", chan);
                logMsg(env);
                goto exit;
        };
	if (chan > chan_n) {
                sprintf(env->output, "Chan {%d...%d} range illegal\n", chan, chan_n);
                logMsg(env);
                goto exit;
        };
        if ((chan2 < 1) || (chan2 > 7)) {
                sprintf(env->output, "Chan2 %d illegal, must be 1 to 7\n", chan2);
                logMsg(env);
                goto exit;
        };
	if (chan2 >= chan && chan2 <= chan_n) {
                sprintf(env->output, "Chan2 %d illegal, cannot be in {%d...%d} range\n", chan2, chan, chan_n);
                logMsg(env);
                goto exit;
        };
        if (chan == chan2) {
                sprintf(env->output, "Must use different channels\n");
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

	if (mtu < 580 || mtu > 128*1024) {
                sprintf(env->output, "MTU %d illegal, must be 580 to 128k\n", mtu);
                logMsg(env);
                goto exit;
        };

        wkr[idx].action = umd_dma_tap;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan    = chan;
        wkr[idx].umd_chan_n  = chan_n;
        wkr[idx].umd_chan2   = chan2; // for NREAD
        wkr[idx].umd_tun_MTU = mtu;
        wkr[idx].umd_fifo_thr.cpu_req = cpu;
        wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
        wkr[idx].umd_tx_buf_cnt = buff;
        wkr[idx].umd_sts_entries = sts;
	wkr[idx].did = ~0;
	wkr[idx].rio_addr = 0;
        wkr[idx].byte_cnt = 0;
        wkr[idx].acc_size = mtu+DMA_L2_SIZE;
        wkr[idx].umd_tx_rtype = ALL_NWRITE;
        wkr[idx].wr = 1;
        wkr[idx].use_kbuf = 1;

        wkr[idx].stop_req = 0;
        sem_post(&wkr[idx].run);
exit:
        return 0;
}

struct cli_cmd UDMAT = {
"tundma",
5,
6,
"TUN/TAP (L3) over DMA with User-Mode demo driver -- pointopoint",
"<idx> <cpu> <chan_1> <chan_n> <chan_nread> <buff> <sts> <mtu>\n"
        "<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
        "<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
        "<chan_1> is a DMA channel number from 1 through 7\n"
        "<chan_n> is a DMA channel number from 1 through 7 -- we use chan{1...n} for TX round-robin\n"
        "<chan_nread> is a DMA channel number from 1 through 7 used for NREAD, distinct from range above\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts> is the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<mtu> is interface MTU\n"
        "       Must be between (576+4) and 128k; upper bound depends on kernel\n"
        "Note: tunX device will be configured as 169.254.x.y where x.y is our destid+1\n"
        "Note: IBAlloc size = (8+MTU)*buf+4 needed before running this command\n",
UDMACmdTun,
ATTR_NONE
};

int UMSGCmd(const char cmd, struct cli_env *env, int argc, char **argv)
{
        int idx;
        int chan;
        int chan_to;
        int letter = 0;
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
        if (cmd == 'T') {
		chan_to  = GetDecParm(argv[n++], 0);
		letter   = GetDecParm(argv[n++], 0);
	}
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
        if (cmd == 'T'){
		if ((chan_to < 2) || (chan_to > 3)) {
			sprintf(env->output, "Chan_to %d illegal, must be 2 to 3\n", chan);
			logMsg(env);
			goto exit;
		}
		if ((letter < 0) || (letter > 3)) {
			sprintf(env->output, "Letter %d illegal, must be 0 to 3\n", letter);
			logMsg(env);
			goto exit;
		}
        };


	if ((buff < 32) || (buff > MAX_UMD_BUF_COUNT) || (buff & (buff-1)) ||
			(buff > MAX_UMD_BUF_COUNT)) {
                sprintf(env->output,
			"Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
			buff, MAX_UMD_BUF_COUNT);
        	logMsg(env);
		goto exit;
	};

        if ((sts < 32) || (sts > 0x800000) || (sts & (sts-1))) {
                sprintf(env->output,
                        "Bad STS %x, must be power of 2, 0x20 to 0x80000\n",
                        sts);
                logMsg(env);
                goto exit;
        };
	if ((acc_sz < 1 || acc_sz > 4096) && txrx) {
                sprintf(env->output,
                        "Bad acc_sz %d, must be 1..4096\n", acc_sz);
                logMsg(env);
                goto exit;
	}

        if (mp_h_qresp_valid && (qresp.hdid == did) && txrx &&
			(GetEnv("FORCE_DESTID") == NULL)) {
                sprintf(env->output,
                	"\n\tERROR: Testing against own desitd=%d."
                        "Set env FORCE_DESTID to disable this check.\n",
                        did);
                logMsg(env);
                goto exit;
        }

        wkr[idx].action = (cmd == 'T')? umd_mbox: umd_mboxl;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan    = chan;
        wkr[idx].umd_chan_to = chan_to;
        wkr[idx].umd_letter  = letter;
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
        wkr[idx].max_iter = GetDecParm("$maxit", -1);

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
10,
"Transmit/Receive MBOX requests with User-Mode demo driver",
"<idx> <cpu> <chan> <chan_to> <letter> <buff> <sts> <did> <size> <txrx>\n"
	"<idx>    : worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<cpu>    : cpu number, or -1 to indicate no cpu affinity\n"
	"<chan>   : Local MBOX channel number from 2 through 3\n"
	"<chan_to>: Remote MBOX channel number from 2 through 3\n"
	"<letter> : Remote MBOX letter number from 0 through 3 -- ignored for RX\n"
	"<buff>   : number of transmit descriptors/buffers to allocate\n"
	"           A power of two, 0x20 up to " STR(MAX_UMD_BUF_COUNT) "\n"
	"<sts>    : number of status entries for completed descriptors\n"
	"           A power of two, 0x20 up to 0x80000\n"
	"<did>    : target device ID (if Transmitting) -- ignored for RX\n"
	"<size>   : Message size, hexadecimal multiple of 8\n"
	"           Minimum 8 maximum 0x1000 (8 through 4096)\n"
	"           Note: Only used when txrx <> 0\n"
	"<txrx>   : 0 RX, 1 TX\n",
UMSGCmdThruput,
ATTR_NONE
};

int UMSGCmdTun(struct cli_env *env, int argc, char **argv)
{
        int idx;
        int chan;
        int cpu;
        uint32_t buff;
        uint32_t sts;

        int n = 0; // this be a trick from X11 source tree ;)

        idx      = GetDecParm(argv[n++], 0);
	if (get_cpu(env, argv[n++], &cpu))
		goto exit;
        chan     = GetDecParm(argv[n++], 0);
        buff     = GetHex(argv[n++], 0);
        sts      = GetHex(argv[n++], 0);

        if (check_idx(env, idx, 1))
                goto exit;

        if ((chan < 2) || (chan > 3)) {
                sprintf(env->output, "Chan %d illegal, must be 2 to 3\n", chan);
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

        wkr[idx].action      = umd_mbox_tap;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan    = chan;
        wkr[idx].umd_chan_to = chan; // FUGE
        wkr[idx].umd_fifo_thr.cpu_req = cpu;
        wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
        wkr[idx].umd_tx_buf_cnt = buff;
        wkr[idx].umd_sts_entries = sts;
        wkr[idx].did = 0;
        wkr[idx].rio_addr = 0;
        wkr[idx].byte_cnt = 0;
        wkr[idx].acc_size = 0;
        wkr[idx].umd_tx_rtype = MAINT_WR;
        wkr[idx].wr = 1;
        wkr[idx].use_kbuf = 1;

        wkr[idx].stop_req = 0;
        sem_post(&wkr[idx].run);
exit:
        return 0;
}

struct cli_cmd UMSGL = {
"lumsg",
5,
8,
"Latency of MBOX requests with User-Mode demo driver",
"<idx> <cpu> <chan> <buff> <sts> <did> <size> <txrx>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
        "<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
        "<chan> is a MBOX channel number from 2 through 3\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts>  the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<did>  target device ID (if Transmitting) -- ignored for RX\n"
	"<size> Message size, hexadecimal multiple of 8\n"
	"       Minimum 8 maximum 0x1000 (8 through 4096)\n"
	"       Note: Only used when txrx <> 0\n"
        "<txrx> 0 Slave, 1 Master\n",
UMSGCmdLat,
ATTR_NONE
};

struct cli_cmd UMSGT = {
"tumsg",
2,
5,
"TUN/TAP (L3) over MBOX with User-Mode demo driver",
"<idx> <cpu> <chan> <buff> <sts>\n"
        "<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
        "<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
        "<chan> is a MBOX channel number from 2 through 3\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts> is the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "Note: tunX device will be configured as 169.254.x.y where x.y is our destid+1",
UMSGCmdTun,
ATTR_NONE
};

#endif

int getIsolCPU(std::vector<std::string>& cpus)
{
  FILE* f = popen("awk '{for(i=1;i<NF;i++){if($i~/isolcpus/){is=$i}}}END{split(is,a,/=/);c=a[2];n=split(c,b,/,/); for(i in b){print b[i]}}' /proc/cmdline", "re");
  if(f == NULL) return -1;

  int count = 0;
  while(! feof(f)) {
    char buf[257] = {0};
    fgets(buf, 256, f);
    if(buf[0] == '\0') break;

    int N = strlen(buf);
    if(buf[N-1] == '\n') buf[--N] = '\0';
    if(buf[N-1] == '\r') buf[--N] = '\0';
   
    cpus.push_back(buf);
    count++;
  }
  pclose(f);

  return count;
}

int IsolcpuCmd(struct cli_env *env, int argc, char **argv)
{
        int minisolcpu = 0;

        int n = 0; // this be a trick from X11 source tree ;)

        if (argc > 0)
		minisolcpu = GetDecParm(argv[n++], 0);

	std::vector<std::string> cpus;

	const int NI = getIsolCPU(cpus);

	if(minisolcpu > 0 && NI < minisolcpu) {
		CRIT("\n\tMinimum number of isolcpu cores (%d) not met, got %d. Bailing out!\n", minisolcpu, NI);
		return -1;
	}

	int c = 0;
	char clist[129] = {0};
	std::vector<std::string>::iterator it = cpus.begin();
	for(; it != cpus.end(); it++) {
		char tmp[9] = {0};
		snprintf(tmp, 8, "cpu%d=%s", ++c, it->c_str());
		SetEnvVar(tmp);
		strncat(clist, it->c_str(), 128);
		strncat(clist, " ", 128);
	}

	sprintf(env->output, "\nIsolcpus: %s\n", clist); logMsg(env);

	return 0;
}

struct cli_cmd Isolcpu = {
"isolcpu",
4,
0,
"Returns the number of islcpus and sets the cpu1...cpuN env vars",
"<minisolcpu>\n"
        "<minisolcpu> [optional] STOP execution if minimum number of isolcpus not met\n",
IsolcpuCmd,
ATTR_NONE
};

#ifdef USER_MODE_DRIVER

int EpWatchCmd(struct cli_env *env, int argc, char **argv)
{
	int idx = 0;
        int tundmathreadindex = -1;
        int epdid = ~0;

        int n = 0; // this be a trick from X11 source tree ;)

        idx               = GetDecParm(argv[n++], 0);
	tundmathreadindex = GetDecParm(argv[n++], 0);
	if (argc > 2)
		epdid = GetDecParm(argv[n++], 0);

        if (check_idx(env, idx, 1))
                goto exit;

        if (check_idx(env, tundmathreadindex, 0))
                goto exit;

	if (idx == tundmathreadindex) {
                sprintf(env->output, "Must use different worker threads!\n");
                logMsg(env);
                goto exit;
	}

        wkr[idx].action      = umd_epwatch;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan    = -1; // FUGE
        wkr[idx].umd_chan_to = tundmathreadindex; // FUDGE
        wkr[idx].umd_fifo_thr.cpu_req = -1;
        wkr[idx].umd_fifo_thr.cpu_run = -1;
        wkr[idx].umd_tx_buf_cnt = 0;
        wkr[idx].umd_sts_entries = 0;
	wkr[idx].did = epdid; // FUDGE
        wkr[idx].rio_addr = 0;
        wkr[idx].byte_cnt = 0;
        wkr[idx].acc_size = 0;
        wkr[idx].umd_tx_rtype = MAINT_WR;
        wkr[idx].wr = 0;
        wkr[idx].use_kbuf = 0;

        wkr[idx].stop_req = 0;
        sem_post(&wkr[idx].run);

	return 0;

exit:
	return 0;
}

struct cli_cmd EPWatch = {
"epwatch",
3,
2,
"Watches RIO endpoints coming/going",
"<idx> <tundmathreadindex> <ep-did>\n"
        "<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
        "<tundmathreadindex> Idx of tundma thread which must be started prior to this thread.\n"
	"<ep-did> [optional] EP to delete (simulates EP going away)",
EpWatchCmd,
ATTR_NONE
};

int UMSGCmdWatch(struct cli_env *env, int argc, char **argv)
{
        int idx;
        int tundmathreadindex = -1;
        int chan;
        uint32_t buff;
        uint32_t sts;

        int n = 0; // this be a trick from X11 source tree ;)

        idx      = GetDecParm(argv[n++], 0);
        tundmathreadindex = GetDecParm(argv[n++], 0);

        if (check_idx(env, idx, 1))
                goto exit;

        if (check_idx(env, tundmathreadindex, 0))
                goto exit;

        if (idx == tundmathreadindex) {
                sprintf(env->output, "Must use different worker threads!\n");
                logMsg(env);
                goto exit;
        }

        chan     = GetDecParm(argv[n++], 0);
        buff     = GetHex(argv[n++], 0);
        sts      = GetHex(argv[n++], 0);

        if (check_idx(env, idx, 1))
                goto exit;

        if ((chan < 2) || (chan > 3)) {
                sprintf(env->output, "Chan %d illegal, must be 2 to 3\n", chan);
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

        wkr[idx].action      = umd_mbox_watch;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan    = chan;
        wkr[idx].umd_chan_to    = tundmathreadindex; // FUDGE
        wkr[idx].umd_fifo_thr.cpu_req = -1;
        wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
        wkr[idx].umd_tx_buf_cnt = buff;
        wkr[idx].umd_sts_entries = sts;
	wkr[idx].did = ~0;
        wkr[idx].rio_addr = 0;
        wkr[idx].byte_cnt = 0;
        wkr[idx].acc_size = 0;
        wkr[idx].umd_tx_rtype = MAINT_WR;
        wkr[idx].wr = 1;
        wkr[idx].use_kbuf = 1;

        wkr[idx].stop_req = 0;
        sem_post(&wkr[idx].run);
exit:
        return 0;
}

struct cli_cmd UMSGWATCH = {
"mboxwatch",
5,
5,
"Watches MBOX messages about peers' IBwin allocations",
"<idx> <tundmathreadindex> <chan> <buff> <sts>\n"
        "<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<tundmathreadindex> idx of tundma thread which must be started prior to this thread.\n"
        "<chan> is a MBOX channel number from 2 through 3\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts> is the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n",
UMSGCmdWatch,
ATTR_NONE
};

#endif // USER_MODE_DRIVER

struct cli_cmd *goodput_cmds[] = {
	&IBAlloc,
	&IBDealloc,
	&Dump,
	&Fill,
	&OBDIO,
	&OBDIOTxLat,
	&OBDIORxLat,
	&dmaTxLat,
	&dmaRxLat,
	&dma,
	&msgTx,
	&msgRx,
	&msgTxLat,
	&msgRxLat,
	&Goodput,
	&Lat,
	&Status,
	&Thread,
	&Isolcpu,
	&Kill,
	&Halt,
	&Move,
	&Wait,
	&Sleep,
	&CPUOccSet,
	&CPUOccDisplay,
	&Mpdevs,
#ifdef USER_MODE_DRIVER
	&UCal,
	&UDMA,
	&UDMALRR,
	&UDMALTX,
	&UDMALRX,
	&UDMAT,
	&UMSG,
	&UMSGL,
	&UMSGT,
	&UMDDD,
	&UTime,
	&EPWatch,
	&UMSGWATCH,
#endif
};

void bind_goodput_cmds(void)
{
	dump_idx = 0;
	dump_base_offset = 0;
	dump_size = 0x100;
	cpu_occ_saved_idx = 0;

	cpu_occ_valid = 0;
	new_proc_user_jifis = 0;
	new_proc_kern_jifis = 0;
	old_proc_user_jifis = 0;
	old_proc_kern_jifis = 0;
	old_tot_jifis = 1;
	new_tot_jifis = 2;
	cpu_occ_pct = 0.0;

        add_commands_to_cmd_db(sizeof(goodput_cmds)/sizeof(goodput_cmds[0]),
                                goodput_cmds);
};

#ifdef __cplusplus
}
#endif
