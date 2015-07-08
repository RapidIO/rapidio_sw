/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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
#include <errno.h>
#include <stdlib.h>
#include <stdint.h> /* For size_t */
#include <unistd.h>
#include <sched.h>
#include <pthread.h>

static void print_err(const char *thrd_name, const char *proc, const char *fail_rtn)
{
	printf("%s: %s %s failed, errno(%d): %s\n", thrd_name, proc, fail_rtn, 
		errno, strerror(errno));
}

int migrate_thread_to_cpu(pthread_t *thrd, const char *thrd_name, 
			int cpu, int debug)
{
	cpu_set_t cpuset;
	int rc;
	int curr_cpu;
	int chk_cpu_lim = 10;

	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	rc = pthread_setaffinity_np(*thrd, sizeof(cpu_set_t), &cpuset);
	if (rc) {
		print_err(thrd_name, __func__, "setaffinity");
		goto exit;
	};
               
	rc = pthread_getaffinity_np(*thrd, sizeof(cpu_set_t), &cpuset);
	if (rc) {
		print_err(thrd_name, __func__, "getaffinity");
		goto exit;
	};

	if (debug) {
		int j;
		printf("%s() affinity is ", thrd_name);
		for (j = 0; j < CPU_SETSIZE; j++)
			if (CPU_ISSET(j, &cpuset))
				printf("CPU %d\n", j);
	};

	curr_cpu = sched_getcpu(); 
	if (debug)
		printf("%s() running on CPU %d\n", thrd_name, curr_cpu);

	while ((curr_cpu != cpu) && chk_cpu_lim) {
		usleep(1);
		curr_cpu = sched_getcpu(); 
		chk_cpu_lim--;
		if (debug)
			printf("%s() running on CPU %d\n", thrd_name, curr_cpu);
	}

	rc = (curr_cpu != cpu);
exit:
	return rc;
};
