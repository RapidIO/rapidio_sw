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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include "debug.h"

#include "pcie_utils.h"

/**
 * map_pcie_bar - Maps a PCIE BAR to a virtual pointer
 * @bar_num: PCIE BAR number
 * @bar_size: Size of BAR space to map, in bytes.
 * @bar_sys_filename: Full pathname of the BAR's resource file under /sys
 * @fd_bar: Address of variable to hold the resource file decriptor
 * @bar_ptr: Address of a pointer variable for storing pointer to mapped BAR
 *
 * Returns %1 on success or %-1 on failure.
 */
int map_pcie_bar(uint8_t bar_num, 
                 uint32_t bar_size,
                 const char *bar_sys_filename, 
                 int *fd_bar,
                 volatile void **bar_ptr)
{
    /* Open resource file */
    if ((*fd_bar = open(bar_sys_filename, O_RDWR | O_SYNC)) == -1) {
        perror( "Failed to open the BAR0 resource" );
        return -1;
    }    
    DPRINT("BAR%d opened via %s\n", bar_num, bar_sys_filename);

    *bar_ptr = mmap( NULL,                     /* Kernel picks starting addr */
                     bar_size,                 /* Length */
                     PROT_READ | PROT_WRITE,   /* For reading & writing */
                     MAP_SHARED,               /* Must be MAP_SHARED */
                     *fd_bar,                  /* File descriptor */
                     0                         /* Offset within BAR space */
                    );

    if (*bar_ptr == (void *) -1) {
        /* Mapping failed, close the file and return with error code */
        perror("Failed to MMAP");
        close(*fd_bar);
        return -1;
    }

    DPRINT("BAR%d mapped to %p with size = 0x%X\n", bar_num, *bar_ptr, bar_size);

    return 1;
} /* map_pcie_bar() */


int pcie_bar0_map(struct peer_info *peer)
{
    return map_pcie_bar(0, 
                        peer->bar0_size, 
                        peer->bar0_filename, 
                        &peer->bar0_fd, 
                        &peer->bar0_base_ptr);
} /* pcie_bar0_map() */


int pcie_bar2_map(struct peer_info *peer)
{
    return map_pcie_bar(2, 
                        peer->bar2_size, 
                        peer->bar2_filename, 
                        &peer->bar2_fd, 
                        &peer->bar2_base_ptr);
} /* pcie_bar0_map() */


/**
 * unmap_pcie_bar - Unmaps previously mapped PCIE BAR
 * @bar_ptr: Pointer variable to mapped BAR
 * @bar_size: Size of BAR space to unmap, in bytes.
 * @fd_bar: Resource file decriptor
 */
void unmap_pcie_bar( volatile void *bar_ptr, 
                            uint32_t bar_size, 
                            int fd_bar )
{
    if (munmap((void *)bar_ptr, bar_size) == -1) {
        perror("Failed to unmap PCIE BAR");
    }
    close(fd_bar);
} /* unmap_pcie_bar() */


void pcie_bar0_unmap(struct peer_info *peer)
{
    unmap_pcie_bar(peer->bar0_base_ptr,
                   peer->bar0_size,
                   peer->bar0_fd);
}


void pcie_bar2_unmap(struct peer_info *peer)
{
    unmap_pcie_bar(peer->bar2_base_ptr,
                   peer->bar2_size,
                   peer->bar2_fd);
}

