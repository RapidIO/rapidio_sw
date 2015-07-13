/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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

#ifndef MSG_UTILS_H
#define MSG_UTILS_H

#include <stdint.h>

#include "debug.h"
#include "peer_utils.h"

int init_messaging_engine(struct peer_info *peer);

int open_outb_mbox(struct peer_info *peer, int mbox, uint32_t entries);

void close_outb_mbox(struct peer_info *peer, int mbox);

int add_outb_message(struct peer_info *peer,
                     int mbox,
                     void *buffer,
                     size_t len);

int send_outb_message(struct peer_info *peer,
                      uint16_t destid,
                      int mbox,
                      size_t len);

int open_inb_mbox(struct peer_info *peer, int mbox, uint32_t entries);

void close_inb_mbox(struct peer_info *peer, int mbox);

int add_inb_buffer(struct peer_info *peer, int mbox, void *buf);

void *get_inb_message(struct peer_info *peer, int mbox);

int inb_message_ready(struct peer_info *peer);


#endif

