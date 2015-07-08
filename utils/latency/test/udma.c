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

/*
 *   DMA Latency Test
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/time.h>

#define CONFIG_RAPIDIO_DMA_ENGINE
#include "linux/rio_cm_cdev.h"
#include "linux/rio_mport_cdev.h"

#include "pcie_utils.h"
#include "rio_register_utils.h"
#include "inbound_utils.h"
#include "tsi721_dma.h"
#include "tsi721_config.h"
#include "dma_utils.h"
#include "time_utils.h"
#include "riodp_mport_lib.h"

#include "debug.h"

/* Number of TSI721 devices detected */
static int num_devices;

/* Peer data structures, one for each device */
struct peer_info peers[MAX_PEERS];


void parse_command_line_parameters(int argc,
                                   char *argv[],
                                   struct peer_info peers[],
                                   int *mode,
                                   int *has_switch,
                                   int *num_devices,
                                   uint16_t *destid,
                                   int *mportid)
{
    int i;

    /* Demo mode */
    *mode = atoi(argv[1]);

    /* Connection between peers has a switch or not? */
    *has_switch = atoi(argv[2]);

    /* Get destination ID */
    *destid = atoi(argv[3]);

    /* Get mport ID */
    *mportid = atoi(argv[4]);

    /* Get number of devices */
    *num_devices = atoi(argv[6]);
    
    DPRINT("mode=%d,has_switch=%d,destid=%04X,mportid=%d,num_devices=%d\n",
            *mode,
            *has_switch,
            *destid,
            *mportid,
            *num_devices);
 
    /* Populate peer data with parameters from the command line */
    for (i = 0; i < *num_devices; i++) {
        /* Data length */
        peers[i].dma_data_length = atoi(argv[5]);

        /* Copy and prepare "resource0" full name */
        strcpy(peers[i].bar0_filename, argv[7+i*2]);
        strcat(peers[i].bar0_filename, "/resource0");

        /* BAR0 size */
        peers[i].bar0_size = atoi(argv[7+i*2+1]);
    }
} /* parse_command_line_parameters() */


void cleanup_dma_test(int num_devices, struct peer_info peers[])
{
    int i;

    for (i = 0; i < num_devices; i++ ) {

        /* Fee DMA channel resources */ 
        dma_free_chan_resources(&peers[i]); 
    
        /* Free data buffers */
        if (peers[i].dma_data_length <= MAX_BDMA_INLINE_DATA_LEN) {
            if (peers[i].src)
                free((void *)peers[i].src);
        } else {
            if (peers[i].dma_data_v) 
                dmatest_buf_free(peers[i].mport_fd,
                                 (void *)peers[i].dma_data_v,
                                 peers[i].dma_alloc_data_length,
                                 &peers[i].dma_data_p);
        }
        if (peers[i].dest)
            free((void *)peers[i].dest);

        /* Unmap inbound window */
        if (peers[i].inbound_ptr)
            unmap_inbound_window(&peers[i]);
        /* Unmap BAR0 */
        if (peers[i].bar0_base_ptr) 
            pcie_bar0_unmap(&peers[i]);

        /* Close mport driver device */
        if( peers[i].mport_fd > 0)
            close(peers[i].mport_fd);

    } /* for */
} /* cleanup_dma_test() */


int init_mport(int demo_mode,
               int has_switch,
               int num_devices,
               int mportid,
               struct peer_info *peer)
{
    int ret;
	uint32_t idx;
	volatile uint8_t *t_ptr;
	volatile uint8_t *dest_ptr;

    /* Map BAR0 */
    if( pcie_bar0_map(peer) < 0) {
        cleanup_dma_test(num_devices,peers);
        return -1;
    }

    /* Open mport device */
    peer->mport_fd = riodp_mport_open(mportid, 0);
    if (peer->mport_fd <= 0) {
        perror("riodp_mport_open()");
        fprintf(stderr,"Failed to open mport%d. Aborting!\n", mportid);
        cleanup_dma_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s: peer->mport_fd = %d\n", __FUNCTION__, peer->mport_fd);
        

    /* Query device information, and store device_id */
    ret = riodp_query_mport(peer->mport_fd, &peer->props);
    if (ret != 0) {
        perror("riodp_query_mport()");
        fprintf(stderr,"Failed to query mport%d. Aborting!\n", mportid);
        cleanup_dma_test(num_devices,peers);
        return -1;
    }
    peer->device_id = peer->props.hdid;
    DPRINT("%s:Device ID for mport(%d) = %08X\n", __FUNCTION__,
                                                  mportid,  
                                                  peer->device_id);

    /* Configure TSI721 */
    ret = config_tsi721(demo_mode == LOOPBACK,
                        has_switch,
                        peer->mport_fd,
                        0,  /* debug */
                        peer->props.link_speed >= RIO_LINK_500); 
    if (ret) {
        fprintf(stderr,"Failed to initialize TSI721, failing\n");
        return -1;
    }

    /* Map inbound window */
    ret = map_inbound_window(peer); 
    if (ret < 0) {
        fprintf(stderr, "Failed to map inbound window, exiting.\n" );
        cleanup_dma_test(num_devices,peers);
        return -1;
    }

  	/* Inbound/overwritten data == 0x00 through 0x7F */
    t_ptr = (uint8_t *)(peer->inbound_ptr);
    for (idx = 0; idx < peer->dma_data_length; idx++) {
        t_ptr[idx] = (uint8_t)(idx & 0x7F);
    }

    /* Allocate DMA channel resources */
    ret = dma_alloc_chan_resources(peer);
    if (ret < 0) {
        fprintf(stderr, "Failed to alloc DMA resources for peer, exiting.\n");
        cleanup_dma_test(num_devices,peers);
        return -1;
    }
    
    /* Allocate source and destination DMA data buffers */ 
    /*    Allocate src buffer */
    if (peer->dma_data_length > MAX_BDMA_INLINE_DATA_LEN) {

        /* Must round up to nearest page size */
        peer->dma_alloc_data_length = dma_get_alloc_data_length(
                                                         peer->dma_data_length);
        /* Allocate coherent memory via rio_mport_cdev call */
        peer->dma_data_v = dmatest_buf_alloc(peer->mport_fd,
                                             peer->dma_alloc_data_length,
                                            &peer->dma_data_p);
        if (!peer->dma_data_v) {
            cleanup_dma_test(num_devices,peers);
            return -1;
        }
        peer->src = (uint8_t *)peer->dma_data_v;
    } else {
        peer->src = (uint8_t *)malloc(peer->dma_data_length);
        if (!peer->src) {
            perror("Failed to malloc() src buffer");
            cleanup_dma_test(num_devices,peers);
            return -1;
        }
        DPRINT("%s:peer->src = %p\n",__FUNCTION__,peer->src);
    }

    /*    Allocate dest buffer */
    peer->dest = (uint8_t *)malloc(peer->dma_data_length);
    if (!peer->dest) {
        perror("Failed to malloc() dest buffer");
        cleanup_dma_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peer->dest = %p\n",__FUNCTION__,peer->dest);

    /* dest_ptr is only used in slave mode to compare with inbound
     * data sent by the Master and received on the Slave */
    dest_ptr = (uint8_t *)(peer->dest);
  	t_ptr    = (uint8_t *)(peer->src);
	/* Source data == 0x80 through 0xFF */
    for (idx = 0; idx < peer->dma_data_length; idx++) {
        t_ptr[idx] = (uint8_t)(idx | 0x80);
    	dest_ptr[idx] = (uint8_t)(idx | 0x80);
    }

  	if (t_ptr[peer->dma_data_length - 1] == 
   		inbound_read_8(peer->inbound_ptr, peer->dma_data_length - 1)
       ) {
        perror("Memory values the same, bad init");
        cleanup_dma_test(num_devices,peers);
        return -1;
    }
   
    return 1;
 
} /* init_mport() */


int init_dma_test(int demo_mode,
                        int mportid,
                        int has_switch,
                        int num_devices,
                        struct peer_info peers[])
{
    int do_reset = 1;


    switch (demo_mode) {
    
    case LOOPBACK:
    case TESTBOARD_WITH_ENUM:
    case DUAL_HOST_MASTER:
    case DUAL_HOST_SLAVE:
        /* Initialize peer for specified mport */
        if (init_mport(demo_mode, 
                       has_switch,
                       num_devices,
                       mportid,
                       &peers[mportid]) < 0) {
            return -1;
        }

        /* Clean-up the TSI721 state */
        if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                           peers[mportid].mport_fd,
                                           0,  /* debug */
                                           peers[mportid].props.hdid,
                                           do_reset)
           ) {
            fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
            cleanup_dma_test(num_devices,peers);
            return -1;
        }
        break;

    case DUAL_CARD:
        /* Initialize peer for each mport */
        if (init_mport(demo_mode, 
                       has_switch,
                       num_devices,
                       0,
                       &peers[0]) < 0) {
            return -1;
        }
        if (init_mport(demo_mode, 
                       has_switch,
                       num_devices,
                       1,
                       &peers[1]) < 0) {
            return -1;
        }

        /* Clean-up the TSI721 state */
        if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                           peers[0].mport_fd,
                                           0,  /* debug */
                                           peers[0].props.hdid,
                                           do_reset)
           ) {
            fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
            cleanup_dma_test(num_devices,peers);
            return 1;
        }
        if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                           peers[1].mport_fd,
                                           0,  /* debug */
                                           peers[1].props.hdid,
                                           do_reset)
           ) {
            fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
            cleanup_dma_test(num_devices,peers);
            return -1;
        }
        break;
    } /* switch */
	
    DPRINT( "%s: Completed!\n", __FUNCTION__);

    return 1;
} /* init_dma_test() */


/**
 * @destid     Device ID of the Master
 * @peer_src   Pointer to peer object for mport0 on slave machine
 * @before     Before time
 * @after      After time
 * @limit      Timeout limit for DMA
 */
int run_slave(uint16_t destid,
              struct peer_info *peer_src,
              struct timespec *before,
              struct timespec *after, 
		      uint32_t *limit)
{
    int i, ret;
    uint8_t value;
    uint32_t rd_count;
    enum dma_dtype dtype;

    DPRINT("%s:Entered\n",__FUNCTION__);
    
    /* DMA transmission preparation */
    ret = dma_slave_transmit_prep(peer_src, &dtype, &rd_count);

    /* Wait for last datum */
    i = peer_src->dma_data_length - 1; 
    while (1) {
        value = inbound_read_8(peer_src->inbound_ptr,i); 
        /* If data compares OK, store it in the 'src' buffer so we can
         * send it back to the Master via DMA */
        if (value == peer_src->dest[i]) {
            break;
        }
    }

    DPRINT("%s: Received all inb ound data successfully\n",__FUNCTION__);

    ret = dma_slave_transmit_exec(destid,
                                  dtype,
                                  rd_count,
                                  peer_src,
                                  before,
                                  after);
    if (ret != 1) {
        fprintf(stderr,"%s:DMA transmit failed, exiting\n",__FUNCTION__);
        cleanup_dma_test(num_devices,peers);
        return -1;
    }
 
    DPRINT("%s: Sent all data back via DMA successfully\n",__FUNCTION__);

    return 1;
} /* run_slave() */


int run_test(int demo_mode,
             uint16_t destid,
             struct peer_info *peer_src,
             struct peer_info *peer_dst, 
		     struct timespec *before,
             struct timespec *after, 
		     uint32_t *limit)
{
    uint8_t value;
    int i, ret, errors, llimit;
    volatile void *inbound_ptr;
    volatile uint8_t *src;
    uint16_t ldestid;

    /* In Master mode, we expect the data to be received by the Master
     * on the Master's inbound, i.e. the src peer's inbound window.
     * Similarly, we want to compare the data with the src peer's
     * 'src' buffer. */
    if (demo_mode == DUAL_HOST_MASTER) {
        inbound_ptr = peer_src->inbound_ptr;
        src = peer_src->src;
        ldestid = destid;
    } else {
        inbound_ptr = peer_dst->inbound_ptr;
        src = peer_dst->src;
        ldestid = peer_dst->device_id;
    }

    if (peer_src->dma_data_length <= MAX_BDMA_INLINE_DATA_LEN) {
        ret = dma_transmit(demo_mode,
                           DTYPE2,
                           ldestid,
                           peer_src, peer_dst, before, after, limit);
    } else {
        ret = dma_transmit(demo_mode,
                           DTYPE1,
                           ldestid,
                           peer_src, peer_dst, before, after, limit);
    }
    if (ret != 1) {
        fprintf(stderr,"DMA transmit failed, exiting\n");
        cleanup_dma_test(num_devices,peers);
        return -1;
    }

    errors = 0;
    for (i = peer_src->dma_data_length - 1; i >= 0; i--) {
        llimit = 10000;
        while (llimit) {
            value = inbound_read_8(inbound_ptr,i); 
            if (value == src[i]) break;
            llimit--;
        }
        if (!llimit) {
            fprintf(stderr,"Inbound timeout:.value=0x%02X, src[%d]=0x%02X\n",
                                                value, i, src[i]);
            errors++;
        } else {
            DPRINT("llimit = %d,value = 0x%02X\n",llimit,value);
        }
    }
    if (!errors) {
        printf( "RapidIO DMA data compares OK...SUCCESS!\n");
    } else {
        fprintf(stderr,"%s:RapidIO DMA data failed comparison\n",__FUNCTION__);
        return -1;
    }

    return 1;
} /* run_test() */


#define LIM_START 10000

/* Ctrl-C handler, invoked when the program hangs and user hits Ctrl-C
 * @param sig   unused
 */
void ctrl_c_handler( int sig )
{
    puts("Ctrl-C hit, cleaning up and exiting...");
    cleanup_dma_test(num_devices,peers);
    exit(1);
} /* ctrl_c_handler() */


int main(int argc, char *argv[])
{
    struct timespec before, after, difference;
    int     demo_mode;
    int     has_switch;
    int     mportid;
    uint16_t destid;
    uint32_t limit = LIM_START;
    int ret = -1; 

    /* Register ctrl-c handler */
    signal(SIGQUIT, ctrl_c_handler);
    signal(SIGINT, ctrl_c_handler);

    /* Initialize the peer data structures */
    init_peer_info(MAX_PEERS, peers);

    /* Parse command-line parameters */
    parse_command_line_parameters(argc,
                                  argv,
                                  peers,
                                  &demo_mode,
                                  &has_switch,
                                  &num_devices,
                                  &destid,
                                  &mportid);

    /* Quit if we fail to initialize */
    /* Note: Includes init of the source and inbound memory buffers. */
    if (init_dma_test(demo_mode, mportid, has_switch, num_devices, peers) < 0) {
        return 1;
    }

    /* Display clock resolution -- SKIP for Slave mode */
    ret = clock_getres(CLOCK_MONOTONIC, &difference);
    if (demo_mode != DUAL_HOST_SLAVE) {
        if (ret == 0) {
    	    printf("\nResolution of clock: %d seconds %d nseconds\n",
    		    (uint32_t)difference.tv_sec, (uint32_t)difference.tv_nsec);
        } else {
	        printf("Resolution of clock is UNKNOWN.\n");
        }
    }

    /* Run the test */
    switch (demo_mode) {
    
    case LOOPBACK:
        ret = run_test(demo_mode,
                       0,           /* NOT used */
                       &peers[mportid],   /* Source peer */
                       &peers[mportid],   /* Dest peer */
                       &before, 
                       &after,
                       &limit);
        break;

    case DUAL_CARD:
        ret = run_test(demo_mode,
                       0,           /* NOT used */
                       (mportid == 0) ? &peers[0] : &peers[1],   /* Source peer */
                       (mportid == 0) ? &peers[1] : &peers[0],   /* Dest peer */
                       &before, 
                       &after,
                       &limit);
        break;

    case DUAL_HOST_MASTER:
        ret = run_test(demo_mode,
                       destid,
                       &peers[mportid],   
                       NULL,        /* Dest is on another host */
                       &before,
                       &after,
                       &limit);
        break;

    case DUAL_HOST_SLAVE:
        ret = run_slave(destid,
                        &peers[mportid],
                        &before, 
                        &after,
                        &limit);
        break;

    case TESTBOARD_WITH_ENUM:
        ret = run_test(demo_mode,
                       peers[mportid].device_id, 
                       &peers[mportid],   /* Source peer */
                       &peers[mportid],   /* Dest peer */
                       &before, 
                       &after,
                       &limit);
        break;
    }

    /* If we fail, just quit. No need to compute the time! */
    if (ret != 1) {
        fprintf(stderr,"Skipping timing stats due to failure.\n");
        cleanup_dma_test(num_devices,peers);
        return 1;
    }

    /* If Slave, skip the timing; that is only done on the Master */
    if (demo_mode == DUAL_HOST_SLAVE) {
        cleanup_dma_test(num_devices,peers);
        return 0;   /* normal exit */
    }

    /* Compute the time difference */
    printf("Polled %d times\n", LIM_START - limit);
    difference = time_difference( before, after );
    printf("RapidIO DMA copy time = %ld seconds and %ld nanoseconds\n", 
                                                        difference.tv_sec,
                                                        difference.tv_nsec );
    /* Normalize to nano seconds */
    if( difference.tv_sec ) {
        difference.tv_nsec += (difference.tv_sec * 1000000000);
        difference.tv_sec = 0;
    }
       
    /* Free up resources */
    cleanup_dma_test(num_devices,peers);

    return 0;
} /* main() */

