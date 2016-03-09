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

/* Types */
typedef uint32_t rdma_msg_cat;
typedef uint32_t rdma_msg_type;
typedef uint32_t rdma_msg_sub_type;
typedef uint32_t rdma_msg_seq_no;

/* Category codes */
constexpr uint32_t RDMA_LIB_DAEMON_CALL = 0x0055;
constexpr uint32_t RDMA_REQ_RESP        = 0x00AA;
constexpr uint32_t RDMAD_DAEMON_TO_DAEMON_CALL = 0x33CC;

inline const char *cat_name(const uint32_t cat) {
	if(cat == RDMA_LIB_DAEMON_CALL)
		return "RDMA_LIB_DAEMON_CALL";
	else if (cat == RDMA_REQ_RESP)
		return "RDMA_REQ_RESP";
	else if (cat == RDMAD_DAEMON_TO_DAEMON_CALL)
		return "RDMAD_DAEMON_TO_DAEMON_CALL";
	else
		return "CRAP";
}

#endif

