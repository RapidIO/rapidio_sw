/* LIBRSKT internal rdma and socket structure definitions */
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

#ifndef __LIBRSKT_STATES_H__
#define __LIBRSKT_STATES_H__

#ifdef __cplusplus
extern "C" {
#endif

enum rskt_state {
        rskt_uninit = 0,
        rskt_alloced= 1,
        rskt_reqbound = 2,
        rskt_bound  = 3,
        rskt_reqlisten = 4,
        rskt_listening = 5,
        rskt_accepting = 6,
        rskt_reqconnect = 7,
        rskt_connecting = 8,
        rskt_connected = 9,
        rskt_shutting_down = 10,
        rskt_close_by_local = 11,
        rskt_close_by_remote = 12,
        rskt_closing = 13,
        rskt_shut_down = 14,
        rskt_closed = 15,
        rskt_max_state = 16
};

#define SKT_STATE_STR(x) ((char *)(\
	(x == rskt_uninit)?"UNINIT": \
        (x == rskt_alloced)?"Allocd": \
        (x == rskt_reqbound)?"ReqBnd": \
        (x == rskt_bound)?"Bound ": \
        (x == rskt_reqlisten)?"ReqLst": \
        (x == rskt_listening)?"Listen": \
        (x == rskt_accepting)?"Accept": \
        (x == rskt_reqconnect)?"ReqCon": \
        (x == rskt_connecting)?"Coning": \
        (x == rskt_connected)?"CONNED": \
        (x == rskt_shutting_down)?"Shutng": \
        (x == rskt_close_by_local)?"ClsLoc": \
        (x == rskt_close_by_remote)?"ClsRem": \
        (x == rskt_closing)?"Closin": \
        (x == rskt_shut_down)?"SHTDWN": \
        (x == rskt_closed)?"CLOSED":"Invlid"))

#ifdef __cplusplus
}
#endif

#endif /* __LIBRSKT_STATES_H__ */

