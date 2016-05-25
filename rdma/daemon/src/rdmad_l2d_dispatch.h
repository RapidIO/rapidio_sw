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
#ifndef RDMAD_L2D_DISPATCH_H
#define RDMAD_L2D_DISPATCH_H

#include "rdmad_unix_msg.h"
#include "rdmad_tx_engine.h"

int rdmad_is_alive_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int get_mport_id_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int create_mso_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int open_mso_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int close_mso_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int destroy_mso_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int create_ms_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int open_ms_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int close_ms_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int destroy_ms_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int create_msub_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int destroy_msub_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int accept_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int undo_accept_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int send_connect_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int send_disconnect_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int get_ibwin_properties_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int connect_ms_resp_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int undo_connect_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

int server_disconnect_ms_disp(const unix_msg_t *in_msg, tx_engine<unix_server, unix_msg_t> *tx_eng);

#endif
