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
 * Latency test for TSI721
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
//#include <linux/types.h>
#include <sys/ioctl.h>



// TSI721 includes
#include "CPS1848_registers.h"
#include "IDT_Tsi721.h"
#include "tsi721_config.h"

/* Utility functions */
#include "peer_utils.h"
#include "pcie_utils.h"
#include "inbound_utils.h"
#include "obwin_utils.h"
#include "rio_register_utils.h"
#include "time_utils.h"
#include "debug.h"
#include <rapidio_mport_mgmt.h>#include <rapidio_mport_rdma.h>#include <rapidio_mport_sock.h>

// PCIE offset (to be added to distinguish RIO from PCIE accesses
#define PCIE_ADDRESS_OFFSET 0x70000

// Outbound window default settings 
#define DEFAULT_OUTBOUND_WINDOW  0
#define DEFAULT_OUTBOUND_ZONE    0

// RIO address to use for outbound/inbound
#define DEFAULT_RIO_ADDRESS 0x00000000 
                                        
/* Number of TSI721 devices detected */
static int num_devices;

/* Peer data structures, one for each device */
struct peer_info peers[MAX_PEERS];
	


void cleanup_latency_test(int num_devices, struct peer_info peers[])
{
    int i;

    fflush(stdout);
    fflush(stderr);

    for (i = 0; i < num_devices; i++ ) {

        /* Unmap inbound window */
        if (peers[i].inbound_ptr)
            unmap_inbound_window(&peers[i]);

        /* Unmap BAR0 */
        if (peers[i].bar0_base_ptr) 
            pcie_bar0_unmap(&peers[i]);

        /* Unmap BAR2 */
        if (peers[i].bar2_base_ptr) 
            pcie_bar2_unmap(&peers[i]);

        /* Close mport driver device */
        if( peers[i].mport_fd > 0)
            close(peers[i].mport_fd);

    } /* for */

    DPRINT("%s: Completed.\n", __FUNCTION__);
} /* cleanup_latency_test() */


int init_mport(int demo_mode,
               uint16_t destid,
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
        cleanup_latency_test(num_devices,peers);
        return -1;
    }

    /* Map BAR2 */
    if( pcie_bar2_map(peer) < 0) {
        cleanup_latency_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s: Now peer->.bar2_base_ptr = %p\n", __FUNCTION__,
                                                  peer->bar2_base_ptr);

    /* Open mport device */
    peer->mport_fd = riomp_mgmt_mport_open(mportid, 0);
    if (peer->mport_fd <= 0) {
        perror("riomp_mgmt_mport_open()");
        fprintf(stderr,"Failed to open mport%d. Aborting!\n", mportid);
        cleanup_latency_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peer->mport_fd = %d\n",__FUNCTION__,peer->mport_fd);
        

    /* Query device information, and store device_id */
    ret = riodp_query_mport(peer->mport_fd, &peer->props);
    if (ret != 0) {
        perror("riodp_query_mport()");
        fprintf(stderr,"Failed to query mport%d. Aborting!\n", mportid);
        cleanup_latency_test(num_devices,peers);
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
                        peer->props.link_speed >= RIO_LINK_500
                       );
    if (ret) {
        fprintf(stderr,"Failed to initialize TSI721, failing\n");
        cleanup_latency_test(num_devices,peers);
        return -1;
    }

    /* Map inbound window */
    ret = map_inbound_window(peer); 
    if (ret < 0) {
        fprintf(stderr, "Failed to map inbound window, exiting.\n" );
        cleanup_latency_test(num_devices,peers);
        return -1;
    }

    /* Inbound/overwritten data == 0x00 through 0x7F */
    t_ptr = (uint8_t *)(peer->inbound_ptr);
    for (idx = 0; idx < peer->data_length; idx++) {
	    t_ptr[idx] = (uint8_t)(idx & 0x7F);
    }

    /* Clearn inbound window LUT registers */
    obwin_clear_all(peer);

    /* If loopback mode, then destid is the device_id */
    if (demo_mode == LOOPBACK || demo_mode == TESTBOARD_WITH_ENUM) {
        destid = peer->device_id;
    }

    /* If dual-card mode, we should only configure the outbound window
     * for the peer which has a (device_id != destid), otherwise the device
     * ends up sending to itself and the data never gets to the other one
     */
    if (demo_mode == DUAL_CARD && destid == peer->device_id) {
        ; /* Don't configure an outbound window for the destination device */
    } else {
        /* Configure an outbound zone */
        ret = obwin_config_zone(peer,
                                destid,
                                DEFAULT_OUTBOUND_WINDOW,
                                DEFAULT_OUTBOUND_ZONE,
                                DEFAULT_RIO_ADDRESS);
        if(ret < 0)	{
            fprintf(stderr, "Failed to set outbound zone, exiting.\n" );
            cleanup_latency_test(num_devices,peers);
            return -1;
        }
        
        /* Configure an outbound window to be the full size of BAR2
         * and starting at the starting address of BAR2 */
        obwin_config(peer,
                     DEFAULT_OUTBOUND_WINDOW,
                     peer->bar2_address, 
                     peer->bar2_size);
    } /* else */

    /* Allocate src buffer */
    peer->src = (uint8_t *)malloc(peer->data_length);
    if (!peer->src) {
        perror("Failed to malloc() src buffer");
        cleanup_latency_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peer->src = %p\n",__FUNCTION__,peer->src);

    /* Allocate dest buffer */
    peer->dest = (uint8_t *)malloc(peer->data_length);
    if (!peer->dest) {
        perror("Failed to malloc() dest buffer");
        cleanup_latency_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peers->dest = %p\n",__FUNCTION__,peer->dest);

    /* dest_ptr is only used in slave mode to compare with inbound
     * data sent by the Master and received on the Slave */
    dest_ptr = (uint8_t *)(peer->dest);
    t_ptr    = (uint8_t *)(peer->src);

    /* Source data == 0x80 through 0xFF */
    /* Same goes for Dest data in Slave mode */
    for (idx = 0; idx < peer->data_length; idx++) {
  	    t_ptr[idx]    = (uint8_t)(idx | 0x80);
  	    dest_ptr[idx] = (uint8_t)(idx | 0x80);
    }

  	
    DPRINT( "%s: Completed!\n", __FUNCTION__);

    return 1;
} /* init_mport() */


void parse_cmd_line(int argc,
                    char *argv[],
                    struct peer_info peers[],
                    int *mode,
                    int *repeat,
                    int *has_switch,
                    int *num_devices,
                    uint16_t *destid,
                    int *mportid)
{
    int i;

    /* Demo mode */
    *mode = atoi(argv[1]);

    /* Repeat count */
    *repeat = atoi(argv[2]);

    /* Connection between peers has a switch or not? */
    *has_switch = atoi(argv[3]);

    /* Get destination ID */
    *destid = atoi(argv[4]);

    /* Get mport ID */
    *mportid = atoi(argv[5]);

    /* Get number of devices */
    *num_devices = atoi(argv[8]);
    
    DPRINT("%s:mode=%d,has_switch=%d,destid=%04X,mportid=%d,num_devices=%d\n",
            __FUNCTION__,
            *mode,
            *has_switch,
            *destid,
            *mportid,
            *num_devices);
 
    /* Populate peer data with parameters from the command line */
    for (i = 0; i < *num_devices; i++) {
        /* Data size & length */
        peers[i].data_size   = atoi(argv[6]);
        peers[i].data_length = atoi(argv[7]);

        DPRINT("%s:peers[%d].data_size = %d\n", __FUNCTION__, 
                                                i,
                                                peers[i].data_size);
        DPRINT("%s:peers[%d].data_length = %d\n", __FUNCTION__,
                                                  i,
                                                  peers[i].data_length);

        /* Copy and prepare "resource0" & "resource2" full names */
        strcpy(peers[i].bar0_filename, argv[9+i*5]);
        strcat(peers[i].bar0_filename, "/resource0");
        strcpy(peers[i].bar2_filename, argv[9+i*5]);
        strcat(peers[i].bar2_filename, "/resource2");

        /* BAR0 address & size */
        peers[i].bar0_address = atoi(argv[9+i*5+1]);
        peers[i].bar0_size = atoi(argv[9+i*5+2]);

        /* BAR2 address & size */
        peers[i].bar2_address = atoi(argv[9+i*5+3]);
        peers[i].bar2_size = atoi(argv[9+i*5+4]);

        /* DEBUG-ONLY */
        DPRINT("%s: BAR0: %s 0x%08X 0x%06X\n", __FUNCTION__,
                                         peers[i].bar0_filename,
                                         peers[i].bar0_address,
                                         peers[i].bar0_size);
        DPRINT("%s:BAR2: %s 0x%08X 0x%06X\n", __FUNCTION__,
                                         peers[i].bar2_filename,
                                         peers[i].bar2_address,
                                         peers[i].bar2_size);
    }
} /* parse_cmd_line() */


// Ctrl-C handler, invoked when the program hangs and user hits Ctrl-C
// @param sig   unused
void CtrlCHandler( int sig )
{
    puts( "Ctrl-C hit, cleaning up and exiting..." );
    cleanup_latency_test(num_devices,peers);
    exit( 1 );
}


/**
 * @peer_src   Pointer to peer object for mport0 on slave machine
 */
void run_slave(struct peer_info *peer_src)
{
    uint32_t i;
    uint8_t  value_8;
    uint16_t value_16;
    uint32_t value_32;
    volatile uint8_t *t_ptr;

    DPRINT("%s:Entered\n",__FUNCTION__);
    
    /* Inbound/overwritten data == 0x00 through 0x7F */
    t_ptr = (volatile uint8_t *)peer_src->inbound_ptr;
    for (i = 0; i < peer_src->data_length; i++) {
	    t_ptr[i] = (uint8_t)(i & 0x7F);
    }

    /* This is the slave so it waits indefinitely for each datum
     * then sends it back vit the outbound window. To quit, you can
     * hit ctrl-c
     */
    for (i = 0; i < peer_src->data_length; i += peer_src->data_size) {

        if (peer_src->data_size == 1) {

            while (1) {
                value_8 = inbound_read_8(peer_src->inbound_ptr, i );
                if (value_8 == peer_src->dest[i]) { 
                    bar2_write_8(peer_src, i, value_8);
                    break;
                }
            }
        } else if (peer_src->data_size == 2) {

            while (1) {
                value_16 = inbound_read_16(peer_src->inbound_ptr, i);
                if (value_16 == *((uint16_t *)&peer_src->dest[i])) {
                    bar2_write_16(peer_src, i,value_16);
                    break;
                }
            }
        } else if (peer_src->data_size == 4) {

            while (1) {
                value_32 = inbound_read_32(peer_src->inbound_ptr, i);
                if (value_32 == *((uint32_t *)&peer_src->dest[i])) {
                    bar2_write_32(peer_src, i, value_32);
                    break;
                }
            }

        }     
    } /* for */
    
    DPRINT("%s:Exit\n",__FUNCTION__);
} /* run_slave() */


int static run_test(int demo_mode,
                    struct peer_info *peer_src,
                    struct peer_info *peer_dst,
                    struct timespec *before,
                    struct timespec *after,
                    uint32_t *limit)
{
    volatile uint8_t *pSrc8, *pDest8, *pSrc8Start, *pDest8Start;
    uint32_t i;
	uint32_t idx;
	volatile uint8_t *t_ptr;

    volatile void *inbound_ptr;

    assert( peer_src );

    /* In Master mode, the peer_dst is NULL, otherwise check! */
    if (demo_mode != DUAL_HOST_MASTER) {
        assert( peer_dst );
    }

    /* In Master mode, we expect the data to be received by the Master
     * on the Master's inbound, i.e. the src peer's inbound window.
     * Similarly, we want to compare the data with the src peer's
     * 'src' buffer. */
    if (demo_mode== DUAL_HOST_MASTER) {
        inbound_ptr = peer_src->inbound_ptr;
        pDest8Start = peer_src->dest;
    } else {
        /* For dual-card peer_dst is a different mport
         * For loopback peer_dest is the same as peer_src */
        inbound_ptr = peer_dst->inbound_ptr;
        pDest8Start = peer_dst->dest;
    }

    /* Save start location */
    pSrc8Start  = peer_src->src;

    /* Inbound/overwritten data == 0x00 through 0x7F */
    t_ptr = (volatile uint8_t *)inbound_ptr;
    for (idx = 0; idx < peer_src->data_length; idx++) {
	    t_ptr[idx] = (uint8_t)(idx & 0x7F);
    }

__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, before );
__sync_synchronize();

    /* Write outbound and read back from inbound */
    for (i = 0, pSrc8 = pSrc8Start, pDest8 = pDest8Start;
	 i < peer_src->data_length;
	 i += peer_src->data_size, pSrc8 += peer_src->data_size, pDest8 +=peer_src->data_size) {

        *limit = 100000; 

        if( peer_src->data_size == 1) {
            bar2_write_8(peer_src, i, *pSrc8);

            while( *limit >= 0 ) {
                *pDest8 = inbound_read_8(inbound_ptr, i );
                if( *pDest8 == *pSrc8 ) break;
                *limit = *limit - 1;
            }
        } else if( peer_src->data_size == 2 ) {
            bar2_write_16(peer_src, i, *((uint16_t *)pSrc8));

            while( (*limit) >=0 ) {
                *((uint16_t *)pDest8) = inbound_read_16(inbound_ptr, i);
                if( *((uint16_t *)pDest8) == *((uint16_t *)pSrc8 ) ) break;
                *limit = *limit - 1;
            }
        } else if( peer_src->data_size == 4 ) {
            bar2_write_32(peer_src, i, *((uint32_t *)pSrc8));

            while( (*limit) >= 0 ) {
                *((uint32_t *)pDest8) = inbound_read_32(inbound_ptr, i);
                if( *((uint32_t *)pDest8) == *((uint32_t *)pSrc8 ) ) break;
                *limit = *limit - 1;
            }

        }     
        // Check for timeout condition
        if( *limit <= 0 ) {
            fprintf( stderr, "limit = %d, %s:Inbound data != Sent data at i = %d\n", 
                                                            *limit, __FUNCTION__, i );
        }
    } /* for */

    DPRINT("%s:Now computing timing...\n",__FUNCTION__);

__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, after );
__sync_synchronize();

    if( memcmp( (void *)pSrc8Start, (void *)pDest8Start, peer_src->data_length ) == 0 ) {
        DPRINT( "RapidIO transfer compare OK\n" );
    } else {
        for (i = 0; i < peer_src->data_length; i++ ) {
            DPRINT("%s: pSrc8Start[%d] = 0x%02X, pDest8Start[%d] = 0x%02X\n",
                                                        __FUNCTION__,
                                                        i,
                                                        pSrc8Start[i],
                                                        i,
                                                        pDest8Start[i]);
        }
        puts( "RapidIO transfer compare FAILED" );
        return -1;
    }

    return 1;
} /* run_test() */


#define LIM_START 100000

// --------------------------------- MAIN ------------------------------------//
int main( int argc, char *argv[] )
{
    struct  timespec before, after, min_time, max_time, total_time;
    int     demo_mode;
    int     has_switch;
    uint16_t destid;
    int     mportid;
    int     ret = -1;
    int     i;
    int     repeat;
    uint32_t    limit = LIM_START;
    int do_reset = 1;

    puts("Latency test started...");

    signal( SIGQUIT, CtrlCHandler );
    signal( SIGINT, CtrlCHandler );

    /* Initialize the peer data structures */
    init_peer_info(MAX_PEERS, peers);

    /* Parse command-line parameters */
    parse_cmd_line(argc,
                   argv,
                   peers,
                   &demo_mode,
                   &repeat,
                   &has_switch,
                   &num_devices,
                   &destid,
                   &mportid);

    do {
        switch (demo_mode) {
    
        case LOOPBACK:
        case TESTBOARD_WITH_ENUM:
            /* The only difference between loopback and testboard is the switch
             * configuration, which was taken care of in init_mport()
             */
            if (init_mport(demo_mode, 
                           destid,
                           has_switch,
                           num_devices,
                           mportid,
                           &peers[mportid]) < 0) {
                return 1;
            }

            /* Clean-up the TSI721 state */
            if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                               peers[mportid].mport_fd,
                                               0,  /* debug */
                                               peers[mportid].props.hdid,
                                               do_reset)
               ) {
                fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
                cleanup_latency_test(num_devices,peers);
                return 1;
            }
            break;

        case DUAL_CARD:
            if (init_mport(demo_mode, 
                           destid,
                           has_switch,
                           num_devices,
                           0,
                           &peers[0]) < 0) {
                return 1;
            }

            if (init_mport(demo_mode, 
                           destid,
                           has_switch,
                           num_devices,
                           1,
                           &peers[1]) < 0) {
                return 1;
            }

            /* Clean-up the TSI721 state */
            if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                               peers[0].mport_fd,
                                               0,  /* debug */
                                               peers[0].props.hdid,
                                               0)
               ) {
                fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
                cleanup_latency_test(num_devices,peers);
                return 1;
            }
            if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                               peers[1].mport_fd,
                                               0,  /* debug */
                                               peers[1].props.hdid,
                                               do_reset)
               ) {
                fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
                cleanup_latency_test(num_devices,peers);
                return 1;
            }
            break;

        case DUAL_HOST_MASTER:
            if (init_mport(demo_mode, 
                           destid,
                           has_switch,
                           num_devices,
                           mportid,
                           &peers[mportid]) < 0) {
                return 1;
            }
            /* Clean-up the TSI721 state */
            if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                               peers[mportid].mport_fd,
                                               0,  /* debug */
                                               peers[mportid].props.hdid,
                                               do_reset)
               ) {
                fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
                cleanup_latency_test(num_devices,peers);
                return 1;
            }
            break;

        case DUAL_HOST_SLAVE:
            if (init_mport(demo_mode, 
                           destid,
                           has_switch,
                           num_devices,
                           mportid,
                           &peers[mportid]) < 0) {
                return 1;
            }
            /* Clean-up the TSI721 state */
            if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                               peers[mportid].mport_fd,
                                               0,  /* debug */
                                               peers[mportid].props.hdid,
                                               0)
               ) {
                fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
                cleanup_latency_test(num_devices,peers);
                return 1;
            }
            break;

        } /* switch */
    } while(0);

    /* Run the test */

    
    for (i = 0; i < repeat; i++) {
        switch (demo_mode) {
    
        case LOOPBACK:
        case TESTBOARD_WITH_ENUM:
            ret = run_test(demo_mode,
                           &peers[mportid],   /* Source peer */
                           &peers[mportid],   /* Dest peer */
                           &before, 
                           &after,
                           &limit);
            break;

        case DUAL_CARD:
            ret = run_test(demo_mode,
                           (mportid == 0) ? &peers[0] : &peers[1],   /* Source peer */
                           (mportid == 0) ? &peers[1] : &peers[0],   /* Dest peer */
                           &before, 
                           &after,
                           &limit);
            break;

        case DUAL_HOST_MASTER:
           ret = run_test(demo_mode,
                           &peers[mportid],   
                           NULL,        /* Dest is on another host */
                           &before,
                           &after,
                           &limit);
            break;

        case DUAL_HOST_SLAVE:
            run_slave(&peers[mportid]);
            break;

        } /* switch */

        /* Keep track of the time for each iteration */
        time_track(i, before, after, &total_time, &min_time, &max_time);
    } /* for */

    /* If we fail, or it is the Slave just quit. No need to compute the time! */
    if (ret != 1) {
        cleanup_latency_test(num_devices,peers);
        return 1;
    }

    printf("Total time  :\t %9d sec %9d nsec\n", 
	    	(int)total_time.tv_sec, (int)total_time.tv_nsec);
	printf("Minimum time:\t %9d sec %9d nsec\n",
	    	(int)min_time.tv_sec, (int)min_time.tv_nsec);
	total_time = time_div(total_time, repeat);
	printf("Average time:\t %9d sec %9d nsec\n",
	   		(int)total_time.tv_sec, (int)total_time.tv_nsec);
	printf("Maximum time:\t %9d sec %9d nsec\n",
	    	(int)max_time.tv_sec, (int)max_time.tv_nsec);

    /* Compute the time difference */
    printf("Polled %u times\n", LIM_START - limit);

    /* Free up resources */
    cleanup_latency_test(num_devices,peers);

    puts("Latency test completed.");

	return 0;
}
