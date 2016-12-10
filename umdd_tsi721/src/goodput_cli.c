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
#include <iostream>
#include <sstream>
#include <vector>

#include "rio_ecosystem.h"
#include "string_util.h"
#include "tok_parse.h"
#include "rio_misc.h"
#include "goodput_cli.h"
#include "libtime_utils.h"
#include "mhz.h"
#include "liblog.h"
#include "assert.h"

#ifdef __cplusplus
extern "C" {
#endif

char *req_type_str[(int)last_action+1] = {
	(char *)"NO_ACT",
	(char *)" IBWIN",
	(char *)"~IBWIN",
	(char *)"SHTDWN",
#ifdef USER_MODE_DRIVER
        (char*)"UMDdSHM",
        (char*)"UDMA",
        (char*)"ltudma",
        (char*)"lrudma",
        (char*)"nrudma",
        (char*)"tstudma",
#endif
	(char *)"LAST"
};

int check_idx(struct cli_env *env, int idx, int want_halted)
{
	int rc = 1;

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
		goto exit;
	};

	if (want_halted && (2 != wkr[idx].stat)) {
		LOGMSG(env, "\nWorker not halted...\n");
		goto exit;
	};

	if (!want_halted && (2 == wkr[idx].stat)) {
		LOGMSG(env, "\nWorker halted...\n");
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

	if ((*cpu < -1) || (*cpu > MAX_GOODPUT_CPU)) {
		LOGMSG(env, "\nCPU must be 0 to %d...\n", MAX_GOODPUT_CPU);
		goto exit;
	};

	rc = 0;
exit:
	return rc;
};

#define ACTION_STR(x) (char *)((x < last_action)?req_type_str[x]:"UNKWN!")
#define MODE_STR(x) (char *)((x == kernel_action)?"KRNL":"User")
#define THREAD_STR(x) (char *)((0 == x)?"---":((1 == x)?"Run":"Hlt"))

int ThreadCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	int idx, cpu, new_dma;

	idx = getDecParm(argv[0], 0);
	if (get_cpu(env, argv[1], &cpu))
		goto exit;

	new_dma = getDecParm(argv[2], 0);

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
		goto exit;
	};

	if (wkr[idx].stat) {
		LOGMSG(env, "\nWorker %d already running...\n", idx);
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
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
	"<new_dma> If <> 0, open mport again to get a new DMA channel\n",
ThreadCmd,
ATTR_NONE
};

int KillCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	int st_idx = 0, end_idx = MAX_WORKERS-1, i;

	if (strncmp(argv[0], "all", 3)) {
		st_idx = getDecParm(argv[0], 0);
		end_idx = st_idx;

		if ((st_idx < 0) || (st_idx >= MAX_WORKERS)) {
			LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
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

int HaltCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	unsigned int st_idx = 0, end_idx = MAX_WORKERS-1, i;

	if (strncmp(argv[0], "all", 3)) {
		st_idx = (unsigned int) getDecParm(argv[0], 0);
		end_idx = st_idx;

		if (st_idx >= MAX_WORKERS) {
			LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
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

int MoveCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	int idx, cpu;

	idx = getDecParm(argv[0], 0);
	if (get_cpu(env, argv[1], &cpu))
		goto exit;

	if ((idx < 0) || (idx >= MAX_WORKERS)) {
		LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
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

int WaitCmd(struct cli_env *env, int UNUSED(argc), char **argv)
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
		LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
		goto exit;
	};

	if ((state < 0) || (state > 2)) {
		LOGMSG(env, "\nState must be 0|d|D, 1|r|R , or 2|h|H\n");
		goto exit;
	};

	while ((wkr[idx].stat != state) && limit--)
		nanosleep(&ten_usec, NULL);

	if (wkr[idx].stat == state) {
		LOGMSG(env, "\nPassed, Worker %d is now %s\n", idx,
				THREAD_STR(wkr[idx].stat));
	} else {
		LOGMSG(env, "\nFAILED, Worker %d is now %s\n", idx,
				THREAD_STR(wkr[idx].stat));
	};

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

int SleepCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	float sec = GetFloatParm(argv[0], 0);
	if(sec > 0) {
		LOGMSG(env, "\nSleeping %f sec...\n", sec);
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

extern int setup_mport(int mportid);

int MportCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
        const int mportid = GetDecParm(argv[0], 0);
        if (mportid < 0) {
        	LOGMSG(env, "\nIndex must be > 0\n");
                goto exit;
        }

        if (setup_mport(mportid)) {
        	LOGMSG(env, "\nInvalid mport %d\n", mportid);
		return -1;
	}

   exit:
        return 0;
}

struct cli_cmd Mport = {
"mport",
2,
1,
"Switch Mport instance (if more than one Tsi721 PICe card exits)",
"mport <mportid>\n"
        "<mportid> is a worker index, 0-based\n",
MportCmd,
ATTR_NONE
};
#define FOUR_KB (4*1024)
#define SIXTEEN_MB (16*1024*1024)

int IBAllocCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	int idx;
	uint64_t ib_size;

	idx = getDecParm(argv[0], 0);
	ib_size = getHex(argv[1], 0);

	if (check_idx(env, idx, 1))
		goto exit;

	if ((ib_size < FOUR_KB) || (ib_size > SIXTEEN_MB)) {
		LOGMSG(env, "\nIbwin size range: 0x%x to 0x%x\n", FOUR_KB,
				SIXTEEN_MB);
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

int IBDeallocCmd(struct cli_env *env, int UNUSED(argc), char **argv)
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
	
	SAFE_STRNCPY(fl_cpy, file_line, sizeof(fl_cpy));
	tok = strtok_r(file_line, delim, &saveptr);
	while ((NULL != tok) && (tok_cnt < 13)) {
		tok = strtok_r(NULL, delim, &saveptr);
		tok_cnt++;
	};

	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, proc_new_utime, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, proc_new_stime, 0))
		goto error;

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
	
	SAFE_STRNCPY(fl_cpy, file_line, sizeof(fl_cpy));
	
	/* Throw the first token away. */
	tok = strtok_r(file_line, delim, &saveptr);
	if (NULL == tok)
		goto error;
	
	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_user, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_nice, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_system, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_idle, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_iowait, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_irq, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ll(tok, p_softirq, 0))
		goto error;

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
	FILE *stat_fp = NULL, *cpu_stat_fp = NULL;
	char filename[256] = {0};
	char file_line[CPUOCC_BUFF_SIZE] = {0};
	uint64_t p_user = 1, p_nice = 1, p_system = 1, p_idle = 1;
	uint64_t p_iowait = 1, p_irq = 1, p_softirq = 1;
	int rc;

	pid_t my_pid = getpid();

	snprintf(filename, 255, "/proc/%d/stat", my_pid);

	stat_fp = fopen(filename, "re" );
	if (NULL == stat_fp) {
		ERR( "FAILED: Open proc stat file \"%s\": %d %s\n",
			filename, errno, strerror(errno));
		goto exit;
	};

	cpu_stat_fp = fopen("/proc/stat", "re");
	if (NULL == cpu_stat_fp) {
		ERR("FAILED: Open file \"/proc/stat\": %d %s\n",
			errno, strerror(errno));
		goto exit;
	};

	if (NULL == fgets(file_line, sizeof(file_line), stat_fp)) {
		ERR("Unexpected EOF 1 : %d %s\n", errno, strerror(errno));
		goto exit;
	};

	cpu_occ_parse_proc_line(file_line, proc_user_jifis, proc_kern_jifis);

		
	memset(file_line, 0, sizeof(file_line));
	if (NULL == fgets(file_line, sizeof(file_line), cpu_stat_fp)) {
		ERR("Unexpected EOF 1 : %d %s\n", errno, strerror(errno));
		goto exit;
	};

	cpu_occ_parse_stat_line(file_line, &p_user, &p_nice, &p_system,
			&p_idle, &p_iowait, &p_irq, &p_softirq);

	*tot_jifis = p_user + p_nice + p_system + p_idle +
			p_iowait + p_irq + p_softirq;
	
	rc = 0;
exit:
	if (stat_fp != NULL) fclose(stat_fp);
	if (cpu_stat_fp != NULL) fclose(cpu_stat_fp);
	return rc;
};

int CPUOccSetCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{

	if (cpu_occ_set(&old_tot_jifis, &old_proc_kern_jifis,
			&old_proc_user_jifis)) {
		LOGMSG(env, "\nFAILED: Could not get proc info \n");
		goto exit;
	};
	LOGMSG(env, "\nSet CPU Occ measurement start point\n");

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

int CPUOccDisplayCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{
	char pctg[24];
	int cpus = getCPUCount();

	if (!cpus)
		cpus = 1;

	if (!cpu_occ_valid) {
		LOGMSG(env, "\nFAILED: CPU OCC measurement start not set\n");
		goto exit;
	};

	if (cpu_occ_set(&new_tot_jifis, &new_proc_kern_jifis,
			&new_proc_user_jifis)) {
		LOGMSG(env, "\nFAILED: Could not get proc info \n");
		goto exit;
	};


	cpu_occ_pct = (((float)(new_proc_kern_jifis + new_proc_user_jifis -
				 old_proc_kern_jifis - old_proc_user_jifis)) /
		((float)(new_tot_jifis - old_tot_jifis))) * 100.0 * cpus;
	sprintf(pctg, "%4.2f", cpu_occ_pct);
	LOGMSG(env, "\n-Kernel- ProcUser ProcKern CPU_Occ\n");
	LOGMSG(env, "%8ld %8ld %8ld %7s\n", new_tot_jifis - old_tot_jifis,
			new_proc_user_jifis - old_proc_user_jifis,
			new_proc_kern_jifis - old_proc_kern_jifis, pctg);
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

#define FLOAT_STR_SIZE 20

int GoodputCmd(struct cli_env *env, int argc, char **UNUSED(argv))
{
	int i;
	float MBps, Gbps, Msgpersec, link_occ; 
	uint64_t byte_cnt = 0;
	float tot_MBps = 0, tot_Gbps = 0, tot_Msgpersec = 0;
	uint64_t tot_byte_cnt = 0;
	char MBps_str[FLOAT_STR_SIZE],  Gbps_str[FLOAT_STR_SIZE];
	char link_occ_str[FLOAT_STR_SIZE];

	LOGMSG(env,
			"\n W STS <<<<--Data-->>>> --MBps-- -Gbps- Messages  Link_Occ\n");

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

		LOGMSG(env, "%2d %3s %16lx %8s %6s %9.0f  %6s\n",
			i,  THREAD_STR(wkr[i].stat),
			byte_cnt, MBps_str, Gbps_str, Msgpersec, link_occ_str);

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

	LOGMSG(env, "Total  %16lx %8s %6s %9.0f  %6s\n", tot_byte_cnt, MBps_str,
			Gbps_str, tot_Msgpersec, link_occ_str);
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

int LatCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{
	int i;
	char min_lat_str[FLOAT_STR_SIZE];
	char avg_lat_str[FLOAT_STR_SIZE];
	char max_lat_str[FLOAT_STR_SIZE];

	LOGMSG(env,
			"\n W STS <<<<-Count-->>>> <<<<Min uSec>>>> <<<<Avg uSec>>>> <<<<Max uSec>>>>\n");

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

		LOGMSG(env, "%2d %3s %16ld %16s %16s %16s\n", i,
				THREAD_STR(wkr[i].stat), wkr[i].perf_iter_cnt,
				min_lat_str, avg_lat_str, max_lat_str);
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
	if (-1 == cpu) {
		LOGMSG(env, "Any ");
	} else {
		LOGMSG(env, "%3d ", cpu);
	}
};


void display_gen_status(struct cli_env *env)
{
	int i;

	LOGMSG(env,
		"\n W STS CPU RUN ACTION  MODE DID <<<<--ADDR-->>>> ByteCnt AccSize W H IB\n");

	for (i = 0; i < MAX_WORKERS; i++) {
		LOGMSG(env, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		LOGMSG(env, "%7s %4s %3d %16lx %7lx %7lx %1d %1d %2d\n",
				ACTION_STR(wkr[i].action),
				MODE_STR(wkr[i].action_mode), wkr[i].did,
				wkr[i].rio_addr, wkr[i].byte_cnt,
				wkr[i].acc_size, wkr[i].wr, wkr[i].mp_h_is_mine,
				wkr[i].ib_valid);
	};
};

void display_ibwin_status(struct cli_env *env)
{
	int i;

	LOGMSG(env,
			"\n W STS CPU RUN ACTION  MODE IB <<<< HANDLE >>>> <<<<RIO ADDR>>>> <<<<  SIZE  >>>\n");

	for (i = 0; i < MAX_WORKERS; i++) {
		LOGMSG(env, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		LOGMSG(env, "%7s %4s %2d %16lx %16lx %15lx\n",
				ACTION_STR(wkr[i].action),
				MODE_STR(wkr[i].action_mode), wkr[i].ib_valid,
				wkr[i].ib_handle, wkr[i].ib_rio_addr,
				wkr[i].ib_byte_cnt);
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
		case 'g':
		case 'G': 
			display_gen_status(env);
			break;
		default:
			LOGMSG(env, "Unknown option \"%c\"\n", argv[0][0]);
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
		LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
		goto exit;
	};

	if (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr)) {
		LOGMSG(env, "\nNo mapped inbound window present\n");
		goto exit;
	};

	if ((base_offset + size) > wkr[idx].ib_byte_cnt) {
		LOGMSG(env, "\nOffset + size exceeds window bytes\n");
		goto exit;
	}

	dump_idx = idx;
	dump_base_offset = base_offset;
	dump_size = size;

	LOGMSG(env, "  Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
	for (offset = 0; offset < size; offset++) {
		if (!(offset & 0xF)) {
			LOGMSG(env, "\n%8lx", base_offset + offset);
		};
		LOGMSG(env, " %2x",
				*(volatile uint8_t * volatile )((uint8_t * )wkr[idx].ib_ptr
						+ base_offset + offset));
	};
	LOGMSG(env, "\n");

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

int FillCmd(struct cli_env *env, int UNUSED(argc), char **argv)
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
		LOGMSG(env, "\nIndex must be 0 to %d...\n", MAX_WORKERS);
		goto exit;
	};

	if (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr)) {
		LOGMSG(env, "\nNo mapped inbound window present\n");
		goto exit;
	};

	if ((base_offset + size) > wkr[idx].ib_byte_cnt) {
		LOGMSG(env, "\nOffset + size exceeds window bytes\n");
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

int MpdevsCmd(struct cli_env *env, int UNUSED(argc) , char **UNUSED(argv))
{
	uint32_t *mport_list = NULL;
	uint32_t *ep_list = NULL;
	uint32_t *list_ptr;
	uint32_t number_of_eps = 0;
	uint8_t number_of_mports = RIO_MAX_MPORTS;
	uint32_t ep = 0;
	int i;
	int mport_id;
	int ret = 0;

	ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
	if (ret) {
		LOGMSG(env, "riomp_mgmt_get_mport_list ERR %d:%s\n", ret,
				strerror(ret));
		goto exit;
	}

	LOGMSG(env, "\nAvailable %d local mport(s):\n", number_of_mports);

	if (number_of_mports > RIO_MAX_MPORTS) {
		LOGMSG(env, "WARNING: Only %d out of %d have been retrieved\n",
				RIO_MAX_MPORTS, number_of_mports);
	}

	list_ptr = mport_list;
	for (i = 0; i < number_of_mports; i++, list_ptr++) {
		mport_id = *list_ptr >> 16;
		LOGMSG(env, "+++ mport_id: %u dest_id: %u\n", mport_id,
				*list_ptr & 0xffff);

		/* Display EPs for this MPORT */

		ret = riomp_mgmt_get_ep_list(mport_id, &ep_list,
				&number_of_eps);
		if (ret) {
			LOGMSG(env, "ERR: riodp_ep_get_list() ERR %d: %s\n",
					ret, strerror(ret));
			break;
		}

		printf("\t%u Endpoints (dest_ID): ", number_of_eps);
		for (ep = 0; ep < number_of_eps; ep++) {
			LOGMSG(env, "%u ", *(ep_list + ep));
		}
		LOGMSG(env, "\n");

		ret = riomp_mgmt_free_ep_list(&ep_list);
		if (ret) {
			LOGMSG(env, "ERR: riodp_ep_free_list() ERR %d: %s\n",
					ret, strerror(ret));
		};

	}

	LOGMSG(env, "\n");

	ret = riomp_mgmt_free_mport_list(&mport_list);
	if (ret) {
		LOGMSG(env, "ERR: riodp_ep_free_list() ERR %d: %s\n", ret,
				strerror(ret));
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
		LOGMSG(env, "FAILED: <type> not 'd', 'f' or 'm'.\n");
		goto exit;
	};
		
	switch (argv[2][0]) {
	case 's':
	case 'S':
		init_seq_ts(ts_p, MAX_TIMESTAMPS);
		break;
	case '-':
		if (argc > 4) {
			st_i = GetDecParm(argv[3], 0);
			end_i = GetDecParm(argv[4], 0);
		} else {
			LOGMSG(env, "\nFAILED: Must enter two idexes\n");
			goto exit;
		};

		if ((end_i < st_i) || (st_i < 0) || (end_i >= MAX_TIMESTAMPS)) {
			LOGMSG(env, "FAILED: Index range 0 to %d.\n",
					MAX_TIMESTAMPS-1);
			goto exit;
		}
		;

		if (ts_p->ts_idx < MAX_TIMESTAMPS - 1) {
			LOGMSG(env, "\nWARNING: Last valid timestamp is %d\n",
					ts_p->ts_idx);
		}
		;
		diff = time_difference(ts_p->ts_val[st_i], ts_p->ts_val[end_i]);
		LOGMSG(env, "\n---->> Sec<<---- Nsec---MMMuuuNNN\n");
		LOGMSG(env, "%16ld %16ld\n", diff.tv_sec, diff.tv_nsec);
		break;

	case 'p':
	case 'P':
		if (argc > 3)
			st_i = GetDecParm(argv[3], 0);
		if (argc > 4)
			end_i = GetDecParm(argv[4], 0);

		if ((end_i < st_i) || (st_i < 0) || (end_i >= MAX_TIMESTAMPS)) {
			LOGMSG(env, "FAILED: Index range 0 to %d.\n",
					MAX_TIMESTAMPS-1);
			goto exit;
		};

		if (ts_p->ts_idx < MAX_TIMESTAMPS - 1) {
			LOGMSG(env, "\nWARNING: Last valid timestamp is %d\n",
					ts_p->ts_idx);
		};

		LOGMSG(env, "\n Idx ---->> Sec<<---- Nsec---mmmuuunnn Marker\n")
		;
		for (idx = st_i; idx <= end_i; idx++) {
			LOGMSG(env, "%4d %16ld %16ld %d\n", idx,
					ts_p->ts_val[idx].tv_sec,
					ts_p->ts_val[idx].tv_nsec,
					ts_p->ts_mkr[idx]);
		}
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
			if ((uint64_t)diff.tv_nsec < lim)
				continue;
			if (!got_one) {
				LOGMSG(env,
						"\n Idx ---->> Sec<<---- Nsec---MMMuuuNNN Marker\n");
				got_one = 1;
			};
			LOGMSG(env, "%4d %16ld %16ld %d -> %d\n", idx,
					diff.tv_sec, diff.tv_nsec,
					ts_p->ts_mkr[idx],
					ts_p->ts_mkr[idx + 1]);
		};

		if (!got_one) {
			LOGMSG(env, "\nNo delays found bigger than %ld\n", lim);
		}

		LOGMSG(env, "\n==== ---->> Sec<<---- Nsec---MMMuuuNNN\n");
		LOGMSG(env, "Min: %16ld %16ld\n", min.tv_sec, min.tv_nsec);
		diff = time_div(tot, end_i - st_i);
		LOGMSG(env, "Avg: %16ld %16ld\n", diff.tv_sec, diff.tv_nsec);
		LOGMSG(env, "Max: %16ld %16ld\n", max.tv_sec, max.tv_nsec);
		break;
	default:
		LOGMSG(env, "FAILED: <cmd> not 's','p' or 'l'.\n");
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

int UMDdSHMCmd(struct cli_env *env, int UNUSED(argc), char **argv)
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

	if ((chan < 1) || (chan > 7)) {
		LOGMSG(env, "Chan %d illegal, must be 1 to 7\n", chan);
		goto exit;
	}

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff - 1))
			|| (buff > MAX_UMD_BUF_COUNT)) {
		LOGMSG(env, "Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
				buff, MAX_UMD_BUF_COUNT);
		goto exit;
	}

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts - 1))) {
		LOGMSG(env,
				"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
				sts);
		goto exit;
	}

        wkr[idx].action = umd_shm;
        wkr[idx].action_mode = user_mode_action;
        wkr[idx].umd_chan = chan;
        wkr[idx].umd_fifo_thr.cpu_req = cpu;
        wkr[idx].umd_fifo_thr.cpu_run = wkr[idx].wkr_thr.cpu_run;
        wkr[idx].umd_tx_buf_cnt = buff;
        wkr[idx].umd_sts_entries = sts;
        wkr[idx].did = ~0;
        wkr[idx].rio_addr = 0;
        wkr[idx].byte_cnt = 0;
        wkr[idx].acc_size = 0;
        wkr[idx].wr = 1;
        wkr[idx].use_kbuf = 1;

        wkr[idx].stop_req = 0;

        sem_post(&wkr[idx].run);
exit:
        return 0;
}

struct cli_cmd UMDdSHM = {
"umdd",
4,
5,
"Start SHM server of User-Mode driver (NOTE: one SHM server per DMA channel)",
"<idx> <cpu> <chan> <buff> <sts>\n"
        "<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
        "<cpu> is a cpu number, or -1 to indicate no cpu affinity\n"
        "<chan> is a DMA channel number from 1 through 7\n"
        "<buff> is the number of transmit descriptors/buffers to allocate\n"
        "       Must be a power of two from 0x20 up to 0x80000\n"
        "<sts> is the number of status entries for completed descriptors\n"
        "       Must be a power of two from 0x20 up to 0x80000\n",
UMDdSHMCmd,
ATTR_NONE
};

int UDMACmd(struct cli_env *env, int UNUSED(argc), char **argv)
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
		LOGMSG(env, "Chan %d illegal, must be 1 to 7\n", chan);
		goto exit;
	}

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff - 1))
			|| (buff > MAX_UMD_BUF_COUNT)) {
		LOGMSG(env, "Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
				buff, MAX_UMD_BUF_COUNT);
		goto exit;
	}

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts - 1))) {
		LOGMSG(env,
				"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
				sts);
		goto exit;
	}

	if (!rio_addr || !acc_sz || !bytes) {
		LOGMSG(env, "Addr, bytes and acc_size must be non-zero\n");
		goto exit;
	}

	if ((trans < 0) || (trans > 5)) {
		LOGMSG(env, "Illegal trans %d, must be between 0 and 5\n",
				trans);
		goto exit;
	}

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
	wkr[idx].max_iter = GetDecParm((char *)"$maxit", -1);

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
	"<trans>  0 NREAD, 1 LAST_NWR, 2 NW, 3 NW_R\n"
        "NOTE:  Enter simulation with \"set sim 1\" before running this command\n",
UDMACmd,
ATTR_NONE
};

int UDMALatTxRxCmd(const char cmd, struct cli_env *env, int UNUSED(argc), char **argv)
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
		LOGMSG(env, "Command '%c' illegal, this should never happen\n",
				cmd);
		goto exit;
	}

	if (check_idx(env, idx, 1))
		goto exit;

	if ((chan < 1) || (chan > 7)) {
		LOGMSG(env, "Chan %d illegal, must be 1 to 7\n", chan);
		goto exit;
	}

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff - 1))
			|| (buff > MAX_UMD_BUF_COUNT)) {
		LOGMSG(env, "Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
				buff, MAX_UMD_BUF_COUNT);
		goto exit;
	}

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts - 1))) {
		LOGMSG(env,
				"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
				sts);
		goto exit;
	}

	if (!rio_addr || !acc_sz) {
		LOGMSG(env, "Addr and acc_size must be non-zero\n");
		goto exit;
	}

	if ((trans < 1) || (trans > 5)) {
		LOGMSG(env,
				"Illegal trans %d, must be between 1 and 5 (NREAD=0 disallowed)\n",
				trans);
		goto exit;
	}

	if (!wkr[idx].ib_valid) {
		LOGMSG(env, "IBwin not allocated for this worker thread!\n");
		goto exit;
	}
	if (wkr[idx].ib_byte_cnt < acc_sz) {
		LOGMSG(env, "IBwin too small (0x%lx) must be at least 0x%x\n",
				wkr[idx].ib_byte_cnt, acc_sz);
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
	wkr[idx].max_iter = GetDecParm((char *)"$maxit", -1);

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

int UDMALatNREAD(struct cli_env *env, int UNUSED(argc), char **argv)
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
		LOGMSG(env, "Chan %d illegal, must be 1 to 7\n", chan);
		goto exit;
	}

	if ((buff < 32) || (buff > 0x800000) || (buff & (buff - 1))
			|| (buff > MAX_UMD_BUF_COUNT)) {
		LOGMSG(env, "Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
				buff, MAX_UMD_BUF_COUNT);
		goto exit;
	}

	if ((sts < 32) || (sts > 0x800000) || (sts & (sts - 1))) {
		LOGMSG(env,
				"Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
				sts);
		goto exit;
	}

	if (!rio_addr || !acc_sz) {
		LOGMSG(env, "Addr and acc_size must be non-zero\n");
		goto exit;
	}

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
	wkr[idx].max_iter = GetDecParm((char *)"$maxit", -1);

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
        "NOTE:  IBAlloc on <did> of size >= acc_sz needed before running this command\n"
        "NOTE:  Enter simulation with \"set sim 1\" before running this command\n",
UDMALatNREAD,
ATTR_NONE
};

int UDMATestBed(struct cli_env *env, int UNUSED(argc), char **argv)
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
        	LOGMSG(env, "Chan %d illegal, must be 1 to 7\n", chan);
                goto exit;
        };

        if ((buff < 32) || (buff > 0x800000) || (buff & (buff-1)) ||
                        (buff > MAX_UMD_BUF_COUNT)) {
        	LOGMSG(env,
                        "Bad Buff %x, must be power of 2, 0x20 to 0x%x\n",
                        buff, MAX_UMD_BUF_COUNT);
                goto exit;
	}

        if ((sts < 32) || (sts > 0x800000) || (sts & (sts-1))) {
        	LOGMSG(env,
                        "Bad Buff %x, must be power of 2, 0x20 to 0x80000\n",
                        sts);
                goto exit;
        };

        if (!rio_addr || !acc_sz) {
        	LOGMSG(env,
                        "Addr and acc_size must be non-zero\n");
                goto exit;
        };

        wkr[idx].action = umd_dmatest;
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
        wkr[idx].max_iter = GetDecParm((char *)"$maxit", -1);

        sem_post(&wkr[idx].run);
exit:
        return 0;
}

struct cli_cmd UDMATB = {
"tstudma",
6,
8,
"DMA BD testbed with User-Mode demo driver",
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
        "NOTE:  IBAlloc on <did> of size >= acc_sz needed before running this command\n"
        "NOTE:  Enter simulation with \"set sim 1\" before running this command\n",
UDMATestBed,
ATTR_NONE
};

extern void UMD_DD(const struct worker* wkr);

int UMDDDDCmd(struct cli_env *env, int argc, char **argv)
{
	int idx = argc > 0? GetDecParm(argv[0], 0): 0;
	if (idx < 0 || idx >= MAX_WORKERS) {
		LOGMSG(env, "Bad idx %d\n", idx);
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
"Dump UMD Tun status",
"<idx>\n"
	"<idx> [optional, default=0] is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n",
UMDDDDCmd,
ATTR_RPT
};

extern void UMD_Test(const struct worker* wkr);

int UMDTestCmd(struct cli_env *env, int argc, char **argv)
{
	int idx = argc > 0 ? GetDecParm(argv[0], 0) : 0;
	int did = argc > 1 ? GetDecParm(argv[1], 666) : 666;

	if (idx < 0 || idx >= MAX_WORKERS) {
		LOGMSG(env, "Bad idx %d\n", idx);
		goto exit;
	}

	wkr[idx].did = did;
	UMD_Test(&wkr[idx]);

exit:
	return 0;
}

struct cli_cmd UMDTest = {
"udt",
1,
0,
"UMD Tun misc tests",
"<idx>\n"
        "<idx> [optional, default=0] is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n",
UMDTestCmd,
ATTR_NONE
};

int getIsolCPU(std::vector<std::string>& cpus)
{
  FILE* f = popen("awk '{for(i=1;i<NF;i++){if($i~/isolcpus/){is=$i}}}END{split(is,a,/=/);c=a[2];n=split(c,b,/,/); for(i in b){print b[i]}}' /proc/cmdline", "re");
  if(f == NULL) return -1;

  int count = 0;
  while(! feof(f)) {
    char buf[257] = {0};
    if (NULL == fgets(buf, 256, f))
	break;
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
	for (; it != cpus.end(); it++) {
		char tmp[9] = {0};
		snprintf(tmp, 8, "cpu%d=%s", ++c, it->c_str());
		if (SetEnvVar(tmp)) {
			CRIT("IsolcpuCmd: Out of memory %s\n", tmp);
			return -1;
		}
		strncat(clist, it->c_str(), 128);
		strncat(clist, " ", 128);
	}

	LOGMSG(env, "\nIsolcpus: %s\n", clist);

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

struct cli_cmd *goodput_cmds[] = {
	&IBAlloc,
	&IBDealloc,
	&Dump,
	&Fill,
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
	&Mport,
	&CPUOccSet,
	&CPUOccDisplay,
	&Mpdevs,
#ifdef USER_MODE_DRIVER
	&UMDdSHM,
	&UDMA,
	&UDMALRR,
	&UDMALTX,
	&UDMALRX,
        &UDMATB,
	&UMDDD,
	&UTime,
	&UMDTest,
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
