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
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "rrmap_config.h"
#include "cfg.h"
#include "liblog.h"

#ifndef _FMD_OPTS_H_
#define _FMD_OPTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FMD_MAX_DEV_FN 256

struct fmd_opt_vals {
	int init_and_quit;	/* If asserted, exit after completing init */
	int init_err;
	int simple_init;	/* If asserted, do not init device directory */
	int print_help;		/* If asserted, print help and exit */
	uint16_t cli_port_num;	/* POSIX Socket for remote CLI session */
	uint16_t app_port_num;	/* POSIX Socket for applications to connect */
	int run_cons;		/* Run a console on this daemon. */
	uint32_t log_level;	/* Starting log level */
	uint32_t mast_mode;	/* 0 - FMD slave, 1 - FMD master */
	uint32_t mast_interval;	/* Master FMD location information */
	uint32_t mast_devid_sz;	/* Master FMD location information */
	uint32_t mast_devid;	/* Master FMD location information */
	uint32_t mast_cm_port;	/* Master FMD location information */
	char *fmd_cfg; /* FMD configuration file */
	char *dd_fn; /* Device directory file name */
	char *dd_mtx_fn; /* Device directory mutext file name */
};

extern struct fmd_opt_vals *fmd_parse_options(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* _FMD_OPTS_H_ */
