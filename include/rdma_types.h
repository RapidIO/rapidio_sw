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

#ifndef __RDMA_TYPES_H__
#define __RDMA_TYPES_H__

#include <stdint.h>

/**
 * Error codes
 */
#define RDMA_DAEMON_UNREACHABLE		0x1000
#define RDMA_MALLOC_FAIL		0x1001
#define RDMA_MPORT_OPEN_FAIL		0x1002
#define RDMA_ERRNO			0x1003 /* Caller to check errno */
#define RDMA_NOT_SUPPORTED		0x1004
#define RDMA_DUPLICATE_MSO		0x1005
#define RDMA_NAME_TOO_LONG		0x1006
#define RDMA_DB_ADD_FAIL		0x1007
#define RDMA_DB_REM_FAIL		0x1008
#define RDMA_ALREADY_OPEN		0x1009
#define RDMA_PTHREAD_FAIL		0x100A
#define RDMA_NULL_PARAM			0x100B
#define RDMA_INVALID_MSO		0x100C
#define RDMA_MS_CLOSE_FAIL		0x100D
#define RDMA_MS_DESTROY_FAIL		0x100E
#define RDMA_DUPLICATE_MS		0x100F
#define RDMA_MSUB_DESTROY_FAIL		0x1010
#define RDMA_INVALID_MS			0x1011
#define RDMA_ALIGN_ERROR		0x1012
#define RDMA_UNMAP_ERROR		0x1013
#define RDMA_DUPLICATE_ACCEPT		0x1014
#define RDMA_ACCEPT_TIMEOUT		0x1015
#define RDMA_ACCEPT_FAIL		0x1016
#define RDMA_INVALID_DESTID		0x1017
#define RDMA_CONNECT_TIMEOUT		0x1018
#define RDMA_CONNECT_FAIL		0x1019
#define RDMA_INVALID_RIO_ADDR		0x101A
#define RDMA_INVALID_SYNC_TYPE		0x101B
#define RDMA_REMOTE_UNREACHABLE		0x101C
#define RDMA_LIB_INIT_FAIL		0x101D

/*
* mso_h - A memory space owner handle. This is a globally unique value within
* the RDMA system. User software must be designed to return all memory spaces
* owned by this handle to the RDMA system if the owner process terminates or
* no longer needs the memory space.
*/
typedef uint64_t mso_h;

/*
* ms_h - memory space identifier. This is a globally unique identifier within
* the RDMA system. It identifies a memory space within the RDMA system.
*/
typedef uint64_t ms_h;

/*
* msub_h - memory space handle. This is a globally unique identifier within
* the RDMA system. It identifies a subspace within a memory space within the
* RDMA system.
* A memory sub-space is identified by a starting virtual address and a number of
* bytes. The memory sub-space can be treated by the application as if it was
* contiguous, even if physically the memory is not contiguous.
*/
typedef uint64_t msub_h;


#endif /* __RDMA_TYPES_H__ */
