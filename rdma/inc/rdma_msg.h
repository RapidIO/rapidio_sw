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
#ifndef RDMA_MSG_H
#define RDMA_MSG_H

#include <stdint.h>

constexpr uint32_t NULL_MSUBID = 0xFFFFFFFF;
constexpr uint32_t NULL_MSID   = 0xFFFFFFFF;
constexpr uint32_t NULL_DESTID = 0xFFFFFFFF;

/* Types */
typedef uint64_t rdma_msg_cat;
typedef uint64_t rdma_msg_type;
typedef uint32_t rdma_msg_sub_type;
typedef uint64_t rdma_msg_seq_no;

/* Category codes */
/**
 * RDMA_CALL blocks waiting for results
 * RDMA_REQ_RESP is asynchronous. A request doesn't wait and
 * a response arrives asynchronously without any notification
 * configured for it; it is handled by the message processor
 */
constexpr rdma_msg_cat RDMA_CALL 		= 0xDDDDDDDDDDDDDDDD;
constexpr rdma_msg_cat RDMA_REQ_RESP        = 0xAAAAAAAAAAAAAAAA;

inline const char *cat_name(const rdma_msg_cat cat) {
	if(cat == RDMA_CALL)
		return "RDMA_CALL";
	else if (cat == RDMA_REQ_RESP)
		return "RDMA_REQ_RESP";
	else
		return "CRAP";
} /* cat_name() */

#endif

