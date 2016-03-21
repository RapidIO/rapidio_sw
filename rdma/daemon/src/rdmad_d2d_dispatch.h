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
#ifndef RDMAD_D2D_DISPATCH_H
#define RDMAD_D2D_DISPATCH_H

#include "rdmad_tx_engine.h"

struct cm_msg_t;

/**
 * Server Dispatch Functions.
 */
void cm_hello_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng);

void cm_connect_ms_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng);

void cm_disconnect_ms_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng);

void cm_force_disconnect_ms_ack_disp(cm_msg_t *msg, cm_server_tx_engine *tx_eng);


/**
 * Client Dispatch Functions.
 */
void cm_hello_ack_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng);

void cm_accept_ms_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng);

void cm_force_disconnect_ms_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng);

void cm_disconnect_ms_ack_disp(cm_msg_t *msg, cm_client_tx_engine *tx_eng);


#endif
