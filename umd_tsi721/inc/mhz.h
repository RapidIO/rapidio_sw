/*
 * ****************************************************************************
 * Copyright (c) 2015, Integrated Device Technology Inc.
 * Copyright (c) 2015, RapidIO Trade Association
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *************************************************************************
 * */

#ifndef __MHZ_H__
#define __MHZ_H__

#include <stdio.h>
#include <string.h>

static inline int getCPUMHz()
{
#ifdef __linux
  FILE* fcpu = fopen("/proc/cpuinfo", "rte");

  if(NULL == fcpu) {
    fprintf(stderr, "Could not open /proc/cpuinfo\n");
    return -1;
  }

  int MHz = 0;
  while(! feof(fcpu)) {
    char buffer[33] = {0};
    if (NULL == fgets(buffer, 32, fcpu))
	break;
    if(buffer[0] == '\0') break;

    if(strncmp(buffer, "cpu MHz", 7)) continue;

    int N = strlen(buffer);
    if(buffer[N-1] == '\n') buffer[--N] = '\0';
    if(buffer[N-1] == '\r') buffer[--N] = '\0';

    const char* colon_separator = strstr(buffer, ": ");
    if(colon_separator == NULL) break;

    if(sscanf(colon_separator+2, "%d", &MHz) != 1 || MHz < 1) break;
    break;
  }
  fclose(fcpu);

  return MHz;
#else
  return -1;
#endif
}

#endif // __MHZ_H__
