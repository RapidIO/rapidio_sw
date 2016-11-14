/* Fabric Management Daemon Configuration file and options parsing support */
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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>

#ifdef __WINDOWS__
#include "stdafx.h"
#include <io.h>
#include <windows.h>
#include "tsi721api.h"
#include "IDT_Tsi721.h"
#endif

// #ifdef __LINUX__
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
// #endif

#include "fmd_dd.h"
#include "cfg.h"
#include "libcli.h"
#include "liblog.h"
#include "fmd_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

void fmd_print_help(void)
{
	printf("\nThe RapidIO Fabric Management Daemon (\"FMD\") manages a\n");
	printf("RapidIO fabric defined in a configuration file.\n");
	printf("Options are:\n");
	printf("-a, -A<port>: POSIX Ethernet socket for App connections.\n");
	printf("       Default is %d\n", FMD_DFLT_APP_PORT_NUM);
	printf("-c, -C<filename>: FMD configuration file name.\n");
	printf("       Default is \"%s\"\n", FMD_DFLT_CFG_FN);
	printf("-d, -D<filename>: Device directory Posix SM file name.\n");
	printf("       Default is \"%s\"\n", FMD_DFLT_DD_FN);
	printf("-h, -H, -?: Print this message.\n");
	printf("-i<interval>: Interval between Device Directory updates.\n");
	printf("       Default is %d\n", FMD_DFLT_MAST_INTERVAL);
	printf("-l, -L<level>: Set starting logging level.\n");
	printf("       Default is %x\n",FMD_DFLT_LOG_LEVEL); 
	printf("-m, -M<filename>: Device directory Mutex SM file name.\n");
	printf("       Default is \"%s\"\n", FMD_DFLT_DD_MTX_FN);
	printf("-n, -N: Do not start console CLI.\n");
	printf("-p<port>: POSIX Ethernet socket for remote CLI.\n");
	printf("       Default is %d\n", FMD_DFLT_CLI_SKT);
	printf("-s, -S: Simple initialization, do not populate device dir.\n");
	printf("       Default is %d\n", FMD_DFLT_INIT_DD);
	printf("-x, -X: Initialize and then immediately exit.\n");
};

struct fmd_opt_vals *fmd_parse_options(int argc, char *argv[])
{
	int idx;

	char *dflt_fmd_cfg = (char *)FMD_DFLT_CFG_FN;
	char *dflt_dd_fn = (char *)FMD_DFLT_DD_FN;
	char *dflt_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	struct fmd_opt_vals *opts;

	opts = (struct fmd_opt_vals *)calloc(1, sizeof(struct fmd_opt_vals));
	if (NULL == opts) {
		printf("Unable to allocate memory\n");
		return NULL;
	}
	opts->init_err = 0;
	opts->init_and_quit = 0;
	opts->simple_init = 0;
	opts->print_help = 0;
	opts->cli_port_num = FMD_DFLT_CLI_SKT;
	opts->app_port_num = FMD_DFLT_APP_PORT_NUM;
	opts->run_cons = 1;
	opts->log_level = FMD_DFLT_LOG_LEVEL;
	opts->mast_mode = 0;
	opts->mast_interval = FMD_DFLT_MAST_INTERVAL;
	opts->mast_devid_sz = FMD_DFLT_MAST_DEVID_SZ;
	opts->mast_devid = FMD_DFLT_MAST_DEVID;
	opts->mast_cm_port = FMD_DFLT_MAST_CM_PORT;
	update_string(&opts->fmd_cfg, dflt_fmd_cfg, strlen(dflt_fmd_cfg));
	update_string(&opts->dd_fn, dflt_dd_fn, strlen(dflt_dd_fn));
	update_string(&opts->dd_mtx_fn, dflt_dd_mtx_fn, strlen(dflt_dd_mtx_fn));

	for (idx = 0; idx < argc; idx++) {
		if (strnlen(argv[idx], 4) < 2)
			continue;

		if ('-' == argv[idx][0]) {
			switch(argv[idx][1]) {
			case 'a': 
			case 'A': opts->app_port_num= (int)strtol(&argv[idx][2], NULL, 10);
				  break;
			case 'c': 
			case 'C': if (get_v_str(&opts->fmd_cfg, 
							&argv[idx][2],
							0))
					  goto print_help;
				  break;
			case 'd': 
			case 'D': if (get_v_str(&opts->dd_fn,
							&argv[idx][2],
							1))
					  goto print_help;
				  break;
			case '?': 
			case 'h': 
			case 'H': goto print_help;

			case 'i': 
			case 'I': opts->mast_interval = (int)strtol(&argv[idx][2], NULL, 10);
				  break;
			case 'l': 
			case 'L': opts->log_level = (int)strtol(&argv[idx][2], NULL, 10);
				  break;
			case 'm': 
			case 'M': if (get_v_str(&opts->dd_mtx_fn,
							&argv[idx][2],
							1))
					  goto print_help;
				  break;
			case 'n': 
			case 'N': opts->run_cons = 0;
				  break;

			case 'p': 
			case 'P': opts->cli_port_num= (int)strtol(&argv[idx][2], NULL, 10);
				  break;
			case 's': 
			case 'S': opts->simple_init = 1;
				  break;
			case 'x': 
			case 'X': opts->init_and_quit = 1;
				  break;
			default: printf("\nUnknown parm: \"%s\"\n", argv[idx]);
				 goto print_help;
			};
		};
	}
	return opts;

print_help:
	opts->init_and_quit = 1;
	opts->print_help = 1;
	fmd_print_help();
	return opts;
}

#ifdef __cplusplus
}
#endif

