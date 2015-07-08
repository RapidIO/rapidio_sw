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

#ifndef PEER_UTILS_H
#define PEER_UTILS_H

#include <stdint.h>
#include <limits.h>

#include "tsi721_dma.h"
#include "tsi721_msg.h"

#define CONFIG_RAPIDIO_DMA_ENGINE
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"
#include "riodp_mport_lib.h"

/* DEMO MODE */
#define LOOPBACK           0
#define DUAL_CARD          1
#define DUAL_HOST_MASTER   2
#define DUAL_HOST_SLAVE    3
#define TESTBOARD_WITH_ENUM 4

/* Information about peers in the demo */
#define MAX_PEERS    2
#define MAX_BDMA_INLINE_DATA_LEN  16
#define RIO_MAX_MBOX 4 /* linux/rio.h */

struct peer_info {
    /* Device ID */
    int device_id;
    int flags;

    /* MPORT */
    int mport_num;
    int mport_fd;
	struct rio_mport_properties props;

    /* BAR0 */
    int bar0_fd;
    char bar0_filename[PATH_MAX];
    uint32_t bar0_address;
    uint32_t bar0_size;
    volatile void *bar0_base_ptr;

    /* BAR2 */
    int bar2_fd;
    char bar2_filename[PATH_MAX];
    uint32_t bar2_address;
    uint32_t bar2_size;
    volatile void *bar2_base_ptr;

    /* Bridge */
    uint32_t    data_length;
    uint8_t     data_size;

    /* Sourc and Destination buffers */
    volatile uint8_t *src;
    volatile uint8_t *dest;

    /* DMA */
    uint64_t dma_data_p;     /* Physical address */
    volatile void     *dma_data_v;    /* Virtual address  */
    int     bd_num;     /* Number of descriptors */
    int     channel_num;    /* DMA channel to use */
    uint32_t dma_data_length;/* Actual length    */
    uint32_t dma_alloc_data_length;  /* Allocated length */
    int     dma_desc_per_channel;

    #define DEFAULT_DESC_NUM         1
    #define DEFAULT_BDMA_CHANNEL     2
    struct tsi721_bdma_chan the_channel;

    /* Messaging */
    #define DEFAULT_OB_MBOX   2
    #define DEFAULT_IB_MBOX   2
    int ob_mbox;
    int ib_mbox;
    uint32_t msg_data_length;       /* Actual length     */
    uint32_t msg_alloc_data_length; /* Rounded-up length */
    struct tsi721_imsg_ring imsg_ring[RIO_MAX_MBOX];
    struct tsi721_omsg_ring omsg_ring[RIO_MAX_MBOX];
    int omsg_init[RIO_MAX_MBOX];
    int imsg_init[RIO_MAX_MBOX];
    unsigned num_ob_desc;   /* FIXME: Should be part of the omsg_ring? */

    /* RIO */
    #define DEFAULT_RIO_ADDRESS     0x00000000 
    uint64_t rio_address;

    /* INBOUND WINDOW */
    #define DEFAULT_INBOUND_WINDOW_SIZE     0x00200000
    volatile void *inbound_ptr;
    uint64_t inbound_handle;
    uint32_t inbound_window_size;
};

void init_peer_info(int num_peers,struct peer_info peers[]);

#endif

