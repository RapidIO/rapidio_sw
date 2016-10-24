/* LIBRSKT internal support for multithreaded messaging */
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

#ifndef __LIBRSKT_THREADS_H__
#define __LIBRSKT_THREADS_H__

#include <semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

struct librskt_app_to_rsktd_msg *alloc_app2d(void);
int free_app2d(struct librskt_app_to_rsktd_msg *ptr);
struct librskt_rsktd_to_app_msg *alloc_d2app(void);
int free_d2app(struct librskt_rsktd_to_app_msg *ptr);

int librskt_init_threads(void);
void librskt_finish_threads(void);

#define TIMEOUT true
#define NO_TIMEOUT false

int librskt_dmsg_req_resp(struct librskt_app_to_rsktd_msg *tx,
                        struct librskt_rsktd_to_app_msg *rx,
                        bool chk_rsp_to);
int librskt_dmsg_tx_resp(struct librskt_app_to_rsktd_msg *tx);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_THREADS_H__ */

