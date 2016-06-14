/*
 * Logging facility definitions for register read/write functions
 */
/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
All rights reserved.
Based on code contributed by Prodrive Technologies

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
#ifndef __REGRW_LOG_H__
#define __REGRW_LOG_H__

#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "regrw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* If 'level' is not specified in the build generate an error */
#ifndef REGRW_LL
#error REGRW_LL not defined. Please specify in Makefile
#endif

/** RapidIO control plane loglevels */
enum regrw_log_level {
        REGRW_LL_NONE  = 0,
        REGRW_LL_CRIT  = 1,
        REGRW_LL_ERROR = 2,
        REGRW_LL_WARN  = 3,
        REGRW_LL_HIGH  = 4,
        REGRW_LL_INFO  = 5,
        REGRW_LL_DEBUG = 6,
        REGRW_LL_TRACE = 7
};

extern enum regrw_log_level regrw_log_level;
extern enum regrw_log_level regrw_disp_level;

#define LOG_VALUE(x) ( \
                (x < REGRW_LL_CRIT)?REGRW_LL_NONE: \
                (x > REGRW_LL_TRACE)?REGRW_LL_TRACE:(enum regrw_log_level)x)

#if REGRW_LL >= REGRW_LL_TRACE
#define TRACE(format, ...) \
	if (REGRW_LL_TRACE <= regrw_log_level) { \
		__regrw_log(REGRW_LL_TRACE, "TRACE", format, ## __VA_ARGS__); \
	}
#else
#define TRACE(format, ...) \
	if (0) { \
		__regrw_log(REGRW_LL_TRACE, "TRACE", format, ## __VA_ARGS__); \
	}
#endif

#if REGRW_LL >= REGRW_LL_DBG
#define DBG(format, ...) \
	if (REGRW_LL_DBG <= regrw_log_level) { \
		__regrw_log(REGRW_LL_DBG, "DBG", format, ## __VA_ARGS__); \
	}
#else
#define DBG(format, ...) \
	if (REGRW_LL_DBG <= regrw_log_level) { \
		__regrw_log(REGRW_LL_DBG, "DBG", format, ## __VA_ARGS__); \
	}
#endif

#if REGRW_LL >= REGRW_LL_INFO
#define INFO(format, ...) \
	if (REGRW_LL_INFO <= regrw_log_level) { \
		__regrw_log(REGRW_LL_INFO, "INFO", format, ## __VA_ARGS__); \
	}
#else
#define INFO(format, ...) \
	if (0) { \
		__regrw_log(REGRW_LL_INFO, "INFO", format, ## __VA_ARGS__); \
	}
#endif

#if REGRW_LL >= REGRW_LL_HIGH
#define HIGH(format, ...) \
	if (REGRW_LL_HIGH <= regrw_log_level) { \
		__regrw_log(REGRW_LL_HIGH, "HIGH", format, ## __VA_ARGS__); \
	}
#else
#define HIGH(format, ...) \
	if (0) { \
		__regrw_log(REGRW_LL_HIGH, "HIGH", format, ## __VA_ARGS__); \
	}
#endif

#if REGRW_LL >= REGRW_LL_WARN
#define WARN(format, ...) \
	if (REGRW_LL_WARN <= regrw_log_level) { \
		__regrw_log(REGRW_LL_WARN, "WARN", format, ## __VA_ARGS__); \
	}
#else
#define WARN(format, ...) \
	if (0) { \
		__regrw_log(REGRW_LL_WARN, "WARN", format, ## __VA_ARGS__); \
	}
#endif

#if REGRW_LL >= REGRW_LL_ERROR
#define ERR(format, ...) \
	if (REGRW_LL_ERROR <= regrw_log_level) { \
		__regrw_log(REGRW_LL_ERROR, "ERR", format, ## __VA_ARGS__); \
	}
#else
#define ERR(format, ...) \
	if (0) { \
		__regrw_log(REGRW_LL_ERROR, "ERR", format, ## __VA_ARGS__); \
	}
#endif

#if REGRW_LL >= REGRW_LL_CRITICAL
#define CRIT(format, ...) \
	if (REGRW_LL_CRIT <= regrw_log_level) { \
		__regrw_log(REGRW_LL_CRIT, "CRIT", format, ## __VA_ARGS__); \
}
#else
#define CRIT(format, ...) \
	if (0) { \
		__regrw_log(REGRW_LL_CRIT, "CRIT", format, ## __VA_ARGS__); \
	}
#endif

#define __regrw_log(level, level_str, format, ...) \
	regrw_log(level, level_str, __FILE__, __LINE__, __func__, \
		format, ## __VA_ARGS__)

int regrw_log(enum regrw_log_level level, const char *level_str,
	const char *file, const unsigned int line_num,
	const char *func, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
