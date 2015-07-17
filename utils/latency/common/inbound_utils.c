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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "debug.h"
#include "IDT_Tsi721.h"
#include "rio_register_utils.h"
#include "peer_utils.h"
#include <rapidio_mport_mgmt.h>#include <rapidio_mport_rdma.h>#include <rapidio_mport_sock.h>

#ifdef __cplusplus
extern "C" {
#endif

int map_inbound_window(struct peer_info *peer)
{
    uint32_t reg;
    int      ret;

    /* Check for NULL */
    if (!peer) {
        fprintf(stderr, "%s: peer==NULL, failing\n",__FUNCTION__);
        return -1;
    }

    /* Print parameters (for debugging only) */
    DPRINT("%s: mport_fd = %d\n",__FUNCTION__, peer->mport_fd);
    DPRINT("%s: rio_address = 0x%lx\n",__FUNCTION__, peer->rio_address);
    DPRINT("%s: window size = %d\n",__FUNCTION__, peer->inbound_window_size);
    DPRINT("%s: &inbound_handle = %p\n",__FUNCTION__, &peer->inbound_handle);

    /* First, obtain an inbound handle from the mport driver */
    ret = riodp_ibwin_map(peer->mport_fd,
                          &peer->rio_address,
                          peer->inbound_window_size,
                          &peer->inbound_handle);
    if (ret) {
        fprintf(stderr, "Failed to get inbound window handle\n" );
        return -1;
    } 

    /* Enable inbound window and disable PHY error checking */
    reg = RIORegister(peer,TSI721_RIO_SP_CTL); 
    WriteRIORegister(peer,TSI721_RIO_SP_CTL, 
                     reg | TSI721_RIO_SP_CTL_INP_EN | TSI721_RIO_SP_CTL_OTP_EN
                    );

    /* Memory-map the inbound window */
	peer->inbound_ptr = mmap(NULL, 
                        peer->inbound_window_size, 
                        PROT_READ | PROT_WRITE, 
                        MAP_SHARED,
                        peer->mport_fd,
                        peer->inbound_handle);
    if (peer->inbound_ptr == (void *) -1) {
        perror( "Failed to MMAP inbound memory!" );
        return -1;
    }

    DPRINT("%s:Inbound window mapped to %p with size = 0x%X\n", __FUNCTION__,
                                                                peer->inbound_ptr, 
                                                                peer->inbound_window_size);
    return 1;
} /* map_inbound_window() */
                                

int unmap_inbound_window(struct peer_info *peer)
{
    /* Memory-unmap the inbound window's virtual pointer */
    if (munmap((void *)peer->inbound_ptr, peer->inbound_window_size) == -1) {
        perror("Failed to unmap inbound window");
        /* Don't return; still try to free the inbound window */
    } else {
        DPRINT("Inbound memory successfully unmapped\n");
    }

    /* Free the inbound window via the mport driver */
    if (riodp_ibwin_free(peer->mport_fd, &peer->inbound_handle)) {
        perror( "Failed to free inbound window in TSI721" );
        return -1;
    }     
    DPRINT("%s:Success.\n",__FUNCTION__);
    return 1;
} /* unmap_inbound_window() */


/* Inbound window access functions */
uint8_t inbound_read_8(volatile void *inbound_ptr, uint32_t offset)
{
#ifdef DEBUG
    static int once = 0;
    if (!once++) 
        DPRINT("%s:inbound_ptr = %p\n", __FUNCTION__,inbound_ptr); 
#endif 
    return *((volatile uint8_t *)((uint8_t *)(inbound_ptr) + offset));
}

uint16_t inbound_read_16(volatile void *inbound_ptr, uint32_t offset)
{
#ifdef DEBUG
    static int once = 0;
    if (!once++) 
        DPRINT("%s:inbound_ptr = %p\n", __FUNCTION__,inbound_ptr); 
#endif
    return *((volatile uint16_t *)((uint8_t *)(inbound_ptr) + offset));
}

uint32_t inbound_read_32(volatile void *inbound_ptr, uint32_t offset)
{
#ifdef DEBUG
    static int once = 0;
    if (!once++) 
        DPRINT("%s:inbound_ptr = %p\n", __FUNCTION__,inbound_ptr); 
#endif
    return *((volatile uint32_t *)((uint8_t *)inbound_ptr + offset));
}

#ifdef __cplusplus
}
#endif
