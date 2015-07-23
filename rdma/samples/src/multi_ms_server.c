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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include <vector>

#include "librdma.h"

using std::vector;

#define MSO_NAME	"mso1"

struct ti {
	ti(mso_h msoh, unsigned ms_number) : msoh(msoh), ms_number(ms_number)
	{
		printf("msoh = 0x%lX, ms_number = %u\n", this->msoh, this->ms_number);
		sem_init(&started, 0, 0);
	}
	pthread_t	tid;
	mso_h		msoh;
	unsigned	ms_number;
	ms_h		msh;
	msub_h		msubh;
	sem_t		started;
};

vector<pthread_t>	tid_list;
bool		shutting_down = false;

void *ms_thread_f(void *arg)
{
	if (!arg) {
		printf("NULL parameter passed to ms_thread_f. Exiting\n");
		pthread_exit(0);
	}
	ti*	tio = (ti *)arg;

	printf("ms_thread_f(%u) started\n", tio->ms_number);

	char ms_name[128];
	sprintf(ms_name, "sspace%u", tio->ms_number);

	/* Create memory space */
	puts("Create memory space");
	int ret = rdma_create_ms_h(ms_name,
				   tio->msoh,
				   1024*1024,
				   0,
				   &tio->msh,
				   NULL);
	if (ret) {
		printf("rdma_create_ms_h() for %s failed, ret = %d\n", ms_name, ret);
		delete tio;
		pthread_exit(0);
	}
	puts("Memory space created");

	/* Create subspace for data exchange */
	puts("Create subspace");
	ret = rdma_create_msub_h(tio->msh, 0, 4*1024, 0, &tio->msubh );
	if (ret) {
		printf("rdma_create_msub() failed, ret = %d\n", ret);
		delete tio;
		pthread_exit(0);
	}
	puts("Subspace created");

	/* Push tid in the tid_list so we are able to kill it on exit */
	tid_list.push_back(tio->tid);

	/* Tell calling thread that we have started, before we block accepting! */
	sem_post(&tio->started);

	/* Accept connections */
	unsigned client_msub_len;
	msub_h	client_msubh;
	puts("Accepting connections...");
	ret = rdma_accept_ms_h(tio->msh,
			       tio->msubh,
			       &client_msubh,
			       &client_msub_len,
			       0);
	if (ret) {
		printf("rdma_accept_ms_h() failed, ret = %d\n", ret);
		delete tio;
		pthread_exit(0);
	}

	/* Stay alive until 'shutting down' */
	while (!shutting_down) {
		sleep(1);
	}
	pthread_exit(0);
} /* ms_thread_f() */

void show_help()
{
	puts("multi_ms_server -n<number of memory spaces> | -h");
	puts("-n Creates the specified number of memory spaces,");
	puts("creates a subspace of 4K for each memory space, and");
	puts("puts each of the memory spaces in 'accept' mode.");
	puts("The memory spaces are names 'sspace1..sspacen");
	puts("-h Displays this help message");
} /* show_help() */

void sig_handler(int sig)
{
	puts("sig_handler");

	/* Ignore SIGUSR1 */
	if (sig == SIGUSR1)
		return;

	if (sig == SIGINT)
		puts("ctrl-c hit. Exiting");

	/* Set global flag so threads would exit */
	shutting_down = true;

	/* Kil and wait for threads to terminate before killing process */
	for (auto it = begin(tid_list); it != end(tid_list); it++) {
		pthread_kill(*it, SIGUSR1);
		pthread_join(*it, NULL);
	}

	/* Terminate process */
	exit(1);
} /* sig_handler() */

int main(int argc, char *argv[])
{
	char c;
	unsigned n;

	/* Register signal handler */
	struct sigaction sig_action;
	sig_action.sa_handler = sig_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGQUIT, &sig_action, NULL);
	sigaction(SIGABRT, &sig_action, NULL);
	sigaction(SIGUSR1, &sig_action, NULL);

	while ((c = getopt(argc, argv, "hn:")) != -1)
		switch (c) {

		case 'h':
			show_help();
			exit(1);
			break;
		case 'n':
			n = atoi(optarg);
			printf("Creating %u memory spaces!\n", n);
			break;

		case '?':
			/* Invalid command line option */
			exit(1);
			break;
		default:
			abort();
		}

	/* Create memory space owner */
	mso_h	msoh;
	if (rdma_create_mso_h(MSO_NAME, &msoh)) {
		printf("Failed to create mso('%s')\n", MSO_NAME);
		return 1;
	}
	printf("msoh created\n");

	/* Create threads for the memory spaces */
	for (unsigned i = 1; i <= n; i++) {
		ti *tio = new ti(msoh, i);
		printf("i = %u\n", i);
		if (pthread_create(&tio->tid, NULL, ms_thread_f, tio)) {
			printf("Failed to create thread for ms%u\n", i);
			delete tio;
			rdma_destroy_mso_h(msoh);
			return 1;
		}

		/* Wait for thread to start before creating the next one */
		sem_wait(&tio->started);
	}

	/* Keep the process alive until someone ctrl-c's out of it */
	while (!shutting_down) {
		sleep(1);
	}

	if (rdma_destroy_mso_h(msoh)) {
		puts("Failed to destroy msoh");
	}

	return 0;
} /* main() */

