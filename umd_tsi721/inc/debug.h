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

#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef RDMA_LL
 #include "liblog.h"
#endif

#ifdef DEBUG
  #define Dprintf(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#else
  #define Dprintf(format, ...) if (0) fprintf (stdout, format, ## __VA_ARGS__)
#endif

#ifdef RDMA_LL
  #define XDBG          DBG
  #define XINFO         INFO
  #define XCRIT         CRIT
  #define XERR          ERR
#else
  #define XDBG(format, ...)   if (0) fprintf(stderr, format, ## __VA_ARGS__)
  #define XINFO(format, ...)  if (0) fprintf(stderr, format, ## __VA_ARGS__)
  #define XCRIT(format, ...)  if (0) fprintf(stderr, format, ## __VA_ARGS__)
  #define XERR(format, ...)   if (0) fprintf(stderr, format, ## __VA_ARGS__)
#endif

#endif // __DEBUG_H__
