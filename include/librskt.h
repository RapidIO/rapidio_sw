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

#include <sys/socket.h>
#include <time.h>

#ifndef __LIBRSKT_H__
#define __LIBRSKT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LIBRSKT_H_INVALID NULL
typedef struct rskt_handle_t *rskt_h;

#define DFLT_LIBRSKTD_PORT 2222
#define DFLT_LIBRSKTD_MPNUM 0

int librskt_init(int rsktd_port, int rsktd_mpnum);

rskt_h rskt_create_socket(void);
void rskt_destroy_socket(rskt_h *sock);

struct rskt_sockaddr {
        uint32_t ct; /* Component tag for target device */
        uint32_t sn; /* Socket number */
};

int rskt_bind(rskt_h skt_h, struct rskt_sockaddr *sock_addr);
int rskt_listen(rskt_h skt_h, int max_backlog);
int rskt_accept(rskt_h l_skt_h, rskt_h skt_h, 
			struct rskt_sockaddr *new_sockaddr);

int rskt_connect(rskt_h skt_h, struct rskt_sockaddr *sock_addr);

int rskt_write(rskt_h skt_h, void *data, uint32_t byte_cnt);
int rskt_read(rskt_h skt_h, void *data, uint32_t max_byte_cnt); /* Stream */
int rskt_recv(rskt_h skt_h, void *data, uint32_t max_byte_cnt); /* Record */
int rskt_flush(rskt_h skt_h, struct timespec timeout);

int rskt_shutdown(rskt_h skt_h);
int rskt_close(rskt_h skt_h);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_H__ */

