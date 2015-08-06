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

#include <stdlib.h>
#include <string.h>

#include "peer_utils.h"
#include "debug.h"

void init_peer_info(int num_peers,struct peer_info peers[])
{
    int i;

    for (i = 0; i < num_peers; i++) {
        DPRINT("%s: Initializing peer[%d]\n", __FUNCTION__, i);

        /* Device ID */
        peers[i].device_id = 0;

        /* MPORT */
        peers[i].mport_num = i;
        peers[i].mp_h = NULL;
        memset(&peers[i].props, 0, sizeof(peers[i].props));

        /* BAR0 */
        peers[i].bar0_fd = 0;
        peers[i].bar0_address = 0;
        peers[i].bar0_size = 0;
        peers[i].bar0_base_ptr = NULL;

        /* BAR2 */
        peers[i].bar2_fd       = 0;
        peers[i].bar2_address = 0;
        peers[i].bar2_size     = 0;
        peers[i].bar2_base_ptr = NULL;

        /* Target */
        peers[i].data_length = 0;
        peers[i].data_size   = 0;

        /* DMA */
        peers[i].dma_data_p = 0;
        peers[i].dma_data_v = NULL;
        peers[i].bd_num = DEFAULT_DESC_NUM;
        peers[i].channel_num = DEFAULT_BDMA_CHANNEL + i;
        peers[i].dma_desc_per_channel = 1;
        peers[i].dma_data_length = 0;
        peers[i].dma_alloc_data_length = 0;
        peers[i].src = NULL;
        peers[i].dest = NULL;
        memset(&peers[i].the_channel,0,sizeof(peers[i].the_channel));

        /* Messaging */
        peers[i].msg_data_length        = 0;
        peers[i].msg_alloc_data_length  = 0;
        peers[i].ob_mbox = DEFAULT_OB_MBOX;
        peers[i].ib_mbox = DEFAULT_IB_MBOX;

        /* RIO */
        peers[i].rio_address = DEFAULT_RIO_ADDRESS;

        /* Inbound */
        peers[i].inbound_ptr = NULL;
        peers[i].inbound_handle = 0;
        peers[i].inbound_window_size = DEFAULT_INBOUND_WINDOW_SIZE;
    }

} /* init_peer_info() */
