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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include "libcli.h"
#include "liblog.h"
#include "worker.h"
#include "goodput.h"
#include "goodput_cli.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"
#include "rapidio_mport_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

riomp_mport_t mp_h;
int mp_h_valid;
int mp_h_num;

struct worker wkr[MAX_WORKERS];

void goodput_thread_shutdown(struct cli_env *env)
{
	int i;

	if (0)
		env = env + 1;

	for (i = 0; i < MAX_WORKERS; i++)
		shutdown_worker_thread(&wkr[i]);

	if (mp_h_valid) {
		riomp_mgmt_mport_destroy_handle(&mp_h);
		mp_h_valid = 0;
	};
};

int setup_mport(int mport_num)
{
	int rc;

	if (mp_h_valid) {
		riomp_mgmt_mport_destroy_handle(&mp_h);
		mp_h_valid = 0;
	};

	mp_h_num = mport_num;
	rc = riomp_mgmt_mport_create_handle(mport_num, 0, &mp_h);
	if (!rc)
		mp_h_valid = 1;

	return rc;
};

void sig_handler(int signo)
{
	printf("\nRx Signal %x\n", signo);
	if ((signo == SIGINT) || (signo == SIGHUP) || (signo == SIGTERM)) {
		printf("Shutting down\n");
		goodput_thread_shutdown(NULL);
	};
};

int main(int argc, char *argv[])
{
	uint8_t mport_num = 0;
	int rc = EXIT_FAILURE;
	int i;

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);

	rdma_log_init("goodput_log.txt", 1);
	if (setup_mport(mport_num)) {
		printf("\nCould not open mport %d, exiting\n", mport_num);
		exit(EXIT_FAILURE);
	};

	for (i = 0; i < MAX_WORKERS; i++)
		init_worker_info(&wkr[i], 1);

	riomp_sock_mbox_init();
        cli_init_base(goodput_thread_shutdown);
        bind_goodput_cmds();
	liblog_bind_cli_cmds();
	splashScreen((char *)"Goodput Evaluation Application");

	console((void *)((char *)"GoodPut> "));

	goodput_thread_shutdown(NULL);

        if (mp_h_valid) {
                riomp_mgmt_mport_destroy_handle(&mp_h);
                mp_h_valid = 0;
        };

	printf("\nGoodput Evaluation Application EXITING!!!!\n");
	rc = EXIT_SUCCESS;

	exit(rc);
}

#ifdef __cplusplus
}
#endif
