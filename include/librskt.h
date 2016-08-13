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

/** @brief Value used to test an rskt_h for validity */
#define LIBRSKT_H_INVALID NULL
/** @brief RDMA Socket Handle */
typedef struct rskt_handle_t *rskt_h;

/** @brief Default rapidio_mport_sock.h socket number for connect requests */
#define DFLT_LIBRSKTD_PORT 2222

/** @brief Default mport number for connect requests */
#define DFLT_LIBRSKTD_MPNUM 0

/** @brief Initialize RDMA sockets library to use port and mport numbers
 *
 * @param[in] rsktd_port Channelized messaging port used by RSKT Daemons
 * @param[in] rsktd_mpnum Identifying number for the MPORT used by this RSKTD
 * 
 * @return status of the function call
 * @retval 0 on success
 * @retval -errno on error
 *
 * Typical use: librskt_init(DFLT_LIBRSKTD_PORT, DFLT_LIBRSKTD_MPNUM)
 */
int librskt_init(int rsktd_port, int rsktd_mpnum);

/** @brief Cleanup RDMA sockets library when done using it
 *
 */
void librskt_finish(void);

/** @brief Allocate an RDMA socket data structure 
 *
 * @return pointer to RDMA socket data structure
 * @retval NULL on failure
 */
rskt_h rskt_create_socket(void);

/** @brief Deallocate an RDMA socket data structure and free resources
 *
 * @param[inout] sock pointer to RDMA socket handle, updated to NULL on return
 */
void rskt_destroy_socket(rskt_h *sock);

/** @brief Address for an RDMA socket: device ID and socket number
 *
 * Note that the "ct" will eventually be used as a component tag value,
 * but for now the implementation assumes a device ID.
 */
struct rskt_sockaddr {
        uint32_t ct; /* Component tag for target device */
        uint32_t sn; /* Socket number */
};

/** @brief Bind a socket handle to a given socket address
 *
 * @param[in] skt_h RDMA socket handle
 * @param[in] sock_addr Local socket number, "ct" field not used
 * @return 0 if successful, -1 if not, errno is set appropriately
 */
int rskt_bind(rskt_h skt_h, struct rskt_sockaddr *sock_addr);

/** @brief Use the socket to handle "connect" requests
 *
 * @param[in] skt_h RDMA socket handle
 * @param[in] max_backlog Maximum allowed unanswered connect requests
 * @return 0 if successful, -1 if not, errno is set appropriately
 */
int rskt_listen(rskt_h skt_h, int max_backlog);

/** @brief Accept and process connect requests for the socket
 *
 * @param[in] l_skt_h Socket than has been bound and is listening
 * @param[out] skt_h New connected socket
 * @param[inout] new_sockaddr Address of other end of connection
 * @return 0 if successful, -1 if not, errno is set appropriately
 */
int rskt_accept(rskt_h l_skt_h, rskt_h *skt_h, 
			struct rskt_sockaddr *new_sockaddr);

/** @brief Connect to a socket which is accepting connect requests
 *
 * @param[in] skt_h RDMA socket handle
 * @param[in] sock_addr Target device ID and socket number
 * @return 0 if successful, -1 if not, errno is set appropriately
 */
int rskt_connect(rskt_h skt_h, struct rskt_sockaddr *sock_addr);


/** @brief Send data to other end of connected socket
 *
 * @param[in] skt_h RDMA socket handle
 * @param[in] data Pointer to data buffer to be sent
 * @param[in] byte_cnt Number of bytes to send
 * @return 0 if successful, -1 if not, errno is set appropriately
 */
int rskt_write(rskt_h skt_h, void *data, uint32_t byte_cnt);

/** @brief Bind a socket handle to a given socket address
 *
 * @param[in] skt_h RDMA socket handle
 * @param[in] data Pointer to data buffer for received data
 * @param[in] max_byte_cnt Maximum number of bytes to be read into data
 * @return 0 if successful, -1 if not, errno is set appropriately
 *
 * Reads up to max_byte_cnt bytes, regardless of how may writes were
 * performed to put the data into the buffer.
 */
int rskt_read(rskt_h skt_h, void *data, uint32_t max_byte_cnt); /* Stream */

/** @brief Close the socket immediately, data in flight may be lost
 *
 * @param[in] skt_h RDMA socket handle
 * @return 0 if successful, -1 if not, errno is set appropriately
 */
int rskt_close(rskt_h skt_h);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_H__ */

