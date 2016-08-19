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

/* System includes */
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>

/* C++ standard library */
#include <string>

#include <cstdio>
#include <cstring>
#include <cstdarg>

#include "regrw_log.h"

using std::string;

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_LINE_SIZE 512

enum regrw_log_level regrw_log_level = (enum regrw_log_level)REGRW_LL;
enum regrw_log_level regrw_disp_level = (enum regrw_log_level)REGRW_LL;

int regrw_set_log_level(int level)
{
	regrw_log_level = LOG_VALUE(level);
	return regrw_log_level;
};
		
int regrw_set_log_dlevel(int level)
{
	regrw_disp_level = LOG_VALUE(level);
	return regrw_disp_level;
};
		
int regrw_get_log_level(void)
{
	return regrw_log_level;
};

int regrw_get_log_dlevel(void)
{
	return regrw_disp_level;
};

int regrw_log(enum regrw_log_level level,
	     const char *level_str,
	     const char *file,
	     const unsigned int line_num,
	     const char *func,
	     const char *format,
	     ...)
{
	char buffer[LOG_LINE_SIZE];
	va_list	args;
	int	n, p, fmt_len;
	time_t	cur_time;
	struct timeval tv;
	char	asc_time[26];

	char *oneline_fmt = (char *)"%4s %s.%06ldus tid=%ld %s:%4d %s(): ";
	
	/* Prefix with level_str, timestamp, filename, line no., and func */
	time(&cur_time);
	ctime_r(&cur_time, asc_time);
	asc_time[strlen(asc_time) - 1] = '\0';
	gettimeofday(&tv, NULL);
	n = sprintf(buffer, (const char *)oneline_fmt,
		level_str,
		asc_time,
		tv.tv_usec,
		syscall(SYS_gettid),
		file,
		line_num,
		func);
	
	/* Handle format and variable arguments */
	va_start(args, format);
	p = vsnprintf(buffer + n, sizeof(buffer)-n, format, args);
	va_end(args);
	fmt_len = strlen(format);
	if (!fmt_len || (fmt_len && ('\n' != format[fmt_len - 1])))
		snprintf(buffer + n + p, sizeof(buffer)-n-p, "\n");

	/* Push log line into circular log buffer and log file */
	string log_line(buffer);
	fputs(log_line.c_str(), stderr);
#ifdef DEBUG
	if (level <= regrw_disp_level)
		printf("%s", log_line.c_str());
#endif
	/* Return 0 if there is no error */
	return (n < 0) ? n : 0;
} /* regrw_log() */

#ifdef __cplusplus
}
#endif

