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

#include "IDT_Tsi721.h"
#include "pcie_utils.h"
#include "rio_register_utils.h"
#include "inbound_utils.h"
#include "tsi721_dma.h"
#include "tsi721_config.h"
#include "dma_utils.h"
#include "msg_utils.h"
#include "time_utils.h"
#include "riodp_mport_lib.h"


/* Number of TSI721 devices detected */
static int num_devices;

/* Peer data structures, one for each device */
struct peer_info peers[MAX_PEERS];


/**
 * Parse command line options
 *
 * @argc		number of arguments
 * @argv		array of argument strings
 * @mode		pointer to mode
 * @repeat		pointer to repeat count
 * @has_switch	pointer to switch flag
 * @num_devices	number of rapidio devices detected on the systekm
 * @destid		pointer to destination ID
 * @mportid		pointer to master port ID
 */
static void parse_cmd_line(int argc,
                           char *argv[],
                           struct peer_info peers[],
                           int *mode,
                           int *repeat,
                           int *has_switch,
                           int num_devices,
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
    
    DPRINT("mode=%d,", *mode);
    DPRINT("repeat=%d,has_switch=%d,destid=%04X,mportid=%d,num_devices=%d\n",
            *repeat,
            *has_switch,
            *destid,
            *mportid,
            num_devices);
 
    /* Populate peer data with parameters from the command line */
    for (i = 0; i < num_devices; i++) {
        /* Data length */
        peers[i].msg_data_length = atoi(argv[6]);

        /* Copy and prepare "resource0" full name */
        strcpy(peers[i].bar0_filename, argv[8+i*2]);
        strcat(peers[i].bar0_filename, "/resource0");

        /* BAR0 size */
        peers[i].bar0_size = atoi(argv[8+i*2+1]);

        /* DEBUG-ONLY */
        DPRINT("%s: BAR0: resource=%s, size=0x%06X\n", __FUNCTION__,
                                                      peers[i].bar0_filename,
                                                      peers[i].bar0_size);
    }
} /* parse_command_line_parameters() */


/**
 * Cleanup allocated buffers, free BARs, and close open files
 *
 * @num_devices		Number of detected devices
 * @peers			Address of array of peer_info structures
 */
static void cleanup_msg_test(int num_devices, struct peer_info peers[])
{
    int i;

    for (i = 0; i < num_devices; i++ ) {

        /* Close outbound mailbox */
        close_outb_mbox(&peers[i], peers[i].ob_mbox);

        /* Close inbound mailbox */
        close_inb_mbox(&peers[i], peers[i].ib_mbox);
        
        /* Free data buffers */
        if (peers[i].src) {
            free((void *)peers[i].src);
        }
        if (peers[i].dest) {
            free((void *)peers[i].dest);
        }
        
        /* Unmap BAR0 */
        if (peers[i].bar0_base_ptr) 
            pcie_bar0_unmap(&peers[i]);

        /* Close mport driver device */
        if( peers[i].mport_fd > 0)
            close(peers[i].mport_fd);

    } /* for */

    DPRINT("%s: Completed.\n", __FUNCTION__);
} /* cleanup_msg_test */


/**
 * Initialize master port
 *
 * @demo_mode	Selected configuration (e.g. LOOPBACK..etc.)
 * @destid		Device ID of destination for messages
 * @has_switch	Flag indicating whether configuration has a switch
 * @mportid		Master port identifier (0 for mport0, 1 for mport1..etc.)
 * @peer		Pointer to peer_info struct to use for mport
 *
 * @return 1 if successful, -1 if unsuccessful
 */
static int init_mport(int demo_mode,
                      uint16_t destid,
                      int has_switch,
                      int num_devices,
                      int mportid,
                      struct peer_info *peer)
{
    int ret;
	uint32_t idx;
	volatile uint8_t *src_ptr, *dst_ptr;

    /* Map BAR0 */
    if( pcie_bar0_map(peer) < 0) {
        cleanup_msg_test(num_devices,peers);
        return -1;
    }

    /* Open mport device */
    peer->mport_fd = riodp_mport_open(mportid, 0);
    if (peer->mport_fd <= 0) {
        perror("riodp_mport_open()");
        fprintf(stderr,"Failed to open mport%d. Aborting!\n", mportid);
        cleanup_msg_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peer->mport_fd = %d\n",__FUNCTION__,peer->mport_fd);
        
    /* Query device information, and store device_id */
    ret = riodp_query_mport(peer->mport_fd, &peer->props);
    if (ret != 0) {
        perror("riodp_query_mport()");
        fprintf(stderr,"Failed to query mport%d. Aborting!\n", mportid);
        cleanup_msg_test(num_devices,peers);
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
        cleanup_msg_test(num_devices,peers);
        return -1;
    }

    /* --- Allocate src buffer --- */
    /* Must round up to nearest page size */
    peer->msg_alloc_data_length = dma_get_alloc_data_length(
                                                        peer->msg_data_length);
    peer->src = (uint8_t *)malloc(peer->msg_alloc_data_length);
    if (!peer->src) {
        fprintf(stderr,"%s: Failed to allocate peer->src\n", __FUNCTION__);
        cleanup_msg_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peer->src = %p\n",__FUNCTION__,peer->src);

    /* --- Allocate dest buffer --- */
    peer->dest = (uint8_t *)malloc(peer->msg_alloc_data_length);
    if (!peer->dest) {
        fprintf(stderr,"%s: Failed to allocate peer->dest\n", __FUNCTION__);
        cleanup_msg_test(num_devices,peers);
        return -1;
    }
    DPRINT("%s:peer->dest = %p\n",__FUNCTION__,peer->dest);

    /* Source data == 0x80 through 0xFF */
    /* Dest data == 0x00 through 0x7F */
    src_ptr  = (uint8_t *)(peer->src);
    dst_ptr  = (uint8_t *)(peer->dest);
    for (idx = 0; idx < peer->data_length; idx++) {
  	    src_ptr[idx]  = (uint8_t)(idx | 0x80);
  	    dst_ptr[idx]  = (uint8_t)(idx & 0x7F);
    }
  	
    /* Initialize messaging engine */
    init_messaging_engine(peer);

    /* Open outbound mailbox */
    ret = open_outb_mbox(peer, peer->ob_mbox, 1);
    DPRINT("%s:open_outb_mbox() returned %d\n", __FUNCTION__,ret);
    fflush(stdout);
    if (ret < 0) {
        fprintf(stderr,"%s: Failed to open ob mailbox %d\n", __FUNCTION__, 
                                                          peer->ob_mbox);
        cleanup_msg_test(num_devices,peers);
        return -1;
    }

    /* Add src buffer to be sent */
    ret = add_outb_message(peer,
                           peer->ob_mbox,
                           (void *)peer->src,
                           peer->msg_data_length);
    if (ret < 0) {
        fprintf(stderr,"%s:Cannot add OB message.\n", __FUNCTION__);
        return -1;
    }

    /* Open inbound mailbox */
    ret = open_inb_mbox(peer, peer->ib_mbox, 256);
    if (ret < 0) {
        fprintf(stderr,"%s: Failed to open ib mailbox %d\n", __FUNCTION__, 
                                                          peer->ib_mbox);
        cleanup_msg_test(num_devices,peers);
        return -1;
    }


    DPRINT( "%s: Completed!\n", __FUNCTION__);

    return 1;
} /* init_mport() */


/**
 * Initialize messaging test, including mapping BARs, opening files,
 * allocating buffers, and initializging the TSI721.
 *
 * @demo_mode	Selected configuration (e.g. LOOPBACK..etc.)
 * @destid		Device ID of destination for messages
 * @has_switch	Flag indicating whether configuration has a switch
 * @num_devices	Number of detected devices
 * @mportid		Master port identifier (0 for mport0, 1 for mport1..etc.)
 * @peers		Address of array of peer_info structures
 *
 * @return 1 if successful, -1 if unsuccessful
 */
static int init_msg_test(int demo_mode,
                         uint16_t destid,
                         int has_switch,
                         int num_devices,
                         int mportid,
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
                       destid,
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
            cleanup_msg_test(num_devices,peers);
            return -1;
        }
        break;

    case DUAL_CARD:
        /* Initialize peer for each mport */
        if (init_mport(demo_mode, 
                       destid,
                       has_switch,
                       num_devices,
                       0,
                       &peers[0]) < 0) {
            return -1;
        }
        if (init_mport(demo_mode, 
                       destid,
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
            cleanup_msg_test(num_devices,peers);
            return 1;
        }
        if (EXIT_FAILURE == cleanup_tsi721(has_switch,
                                           peers[1].mport_fd,
                                           0,  /* debug */
                                           peers[1].props.hdid,
                                           do_reset)
           ) {
            fprintf(stderr,"Failed to reset/cleanup TSI721, failing\n");
            cleanup_msg_test(num_devices,peers);
            return -1;
        }
        break;
    } /* switch */

    return 1;

} /* init_msg_test() */


/**
 * Runs current test using specified parameters.
 * 
 * @demo_mode   Selected demo mode (e.g. LOOPBACK)
 * @destid		Device ID of destination for messages
 * @peer_src	Pointer to peer to use as source
 * @peer_dst	Pointer to peer to use as destination
 * @before		Pointer to time variable for storing time before running test
 * @after		Pointer to time variable for storing time after running test
 * @limit		Pointer to variable specifying timeout value
 *
 * @return 1 if successful, -1 if unsuccessful
 */
static int run_test(int demo_mode,
             uint16_t destid,
             struct peer_info *peer_src,
             struct peer_info *peer_dst, 
		     struct timespec *before,
             struct timespec *after, 
		     uint32_t *limit)
{
    int ret;

    /* LOOPBACK and TESTBOARD obtain their destid from peer_src->device_id */
    if (demo_mode == LOOPBACK || demo_mode == TESTBOARD_WITH_ENUM) {
        destid = peer_src->device_id;
    } else if (destid == 0xFFFF) {
        fprintf(stderr,"%s: Invalid destid 0xFFFF\n", __FUNCTION__);
        return -1;
    }

    /* Add dst buffer to the inbound message queue*/
    ret = add_inb_buffer(peer_dst, peer_dst->ib_mbox, (void *)peer_dst->dest);
    if (ret < 0) {
        fprintf(stderr,"%s:Cannot add dest buffer to IB free list.\n", __FUNCTION__);
        return -1;
    }

    /* Start the timing */
__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, before );

    /* Send message */
    ret = send_outb_message(peer_src,
                            destid,
                            peer_src->ob_mbox,
                            peer_src->msg_data_length);
    if (ret < 0) {
        fprintf(stderr,"%s:Cannot send OB message. Test failed\n", __FUNCTION__);
        return -1;
    } else {
        DPRINT("%s:OB message sent successfuly, apparently!\n", __FUNCTION__);
    }

    /* Check if a reply has arrived */
    ret = inb_message_ready(peer_dst);
    if (ret < 0) {
        fprintf(stderr,"%s: Inbound message timeout!\n", __FUNCTION__);
        return -1;
    }

    /* Get message reply */
    (void)get_inb_message(peer_dst,DEFAULT_IB_MBOX);

    /* Finish the timing */
__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, after );

    /* Compare src and dest */
    /* Note: get_inb_message copies the data to peer_dst->dest */
    ret = memcmp((void *)peer_src->src,
                 (void *)peer_dst->dest,
                 peer_src->msg_data_length);
    if (ret == 0) {
        DPRINT("Buffers compare OK. SUCCESS!\n");
    } else {
        fprintf(stderr,"%s:Compare FAILED\n", __FUNCTION__);
    }

    return 1;
} /* run_test() */


/**
 * Run test when using Slave mode.
 *
 * @destid		Device ID of destination for messages
 * @peer_src	Pointer to peer to use as source
 *
 * @return	1 if successful, -1 if unsuccessful
 */
static int run_slave(uint16_t destid, struct peer_info *peer_src)
{
    int ret;

   /* Add dst buffer to the inbound message queue*/
    ret = add_inb_buffer(peer_src, peer_src->ib_mbox, (void *)peer_src->dest);
    if (ret < 0) {
        fprintf(stderr,"%s:Cannot add dest buffer to IB free list.\n",
                                                                __FUNCTION__);
        return -1;
    }

    /* Loop indefinitely until Master sends message */
    do {
        ret = inb_message_ready(peer_src);
    } while (ret < 0);

    /* Get message from Master */
    (void)get_inb_message(peer_src,DEFAULT_IB_MBOX);

    /* get_inb_message copies the data to peer_dst->dest */
    /* Now we need to send that data back to the Master */
     ret = add_outb_message(peer_src,
                            peer_src->ob_mbox,
                            (void *)peer_src->dest,
                            peer_src->msg_data_length);
    if (ret < 0)
        return -1;

	/* Send received message back to Master */
    ret = send_outb_message(peer_src, 
                            destid,
                            peer_src->ob_mbox,
                            peer_src->msg_data_length);
    if (ret < 0) {
        fprintf(stderr,"%s:Cannot send OB message. Test failed\n", __FUNCTION__);
        return -1;
    } else {
        DPRINT("%s:OB message sent successfuly, apparently!\n", __FUNCTION__);
    }

    return 1;
} /* run_slave() */


/**
 * Ctrl-C handler, invoked when the program hangs and user hits Ctrl-C
 *
 * @param sig   unused
 */
static void ctrl_c_handler( int sig )
{
    puts("Ctrl-C hit, cleaning up and exiting...");
    cleanup_msg_test(num_devices,peers);
    exit(1);
} /* ctrl_c_handler() */


#define LIM_START 10000
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

    /* Register ctrl-c handler */
    signal(SIGQUIT, ctrl_c_handler);
    signal(SIGINT, ctrl_c_handler);

    /* Get number of devices */
    num_devices = atoi(argv[7]);

    init_peer_info(num_devices, peers);

    /* Parse command-line parameters */
    parse_cmd_line(argc,
                   argv,
                   peers,
                   &demo_mode,
                   &repeat,
                   &has_switch,
                   num_devices,
                   &destid,
                   &mportid);

    /* Initialization based on demo_mode */
    ret = init_msg_test(demo_mode,
                        destid,
                        has_switch,
                        num_devices,
                        mportid,
                        peers);
    if (ret < 0) {
        fprintf (stderr, "%s: Failed during initialization.\n", __FUNCTION__);
        return 1;
    }


    /* Run the tests */
    for (i = 0; i < repeat; i++) {
        switch (demo_mode) {
    
        case LOOPBACK:
        case TESTBOARD_WITH_ENUM:
        case DUAL_HOST_MASTER:
            ret = run_test(demo_mode,
                           destid,
                           &peers[mportid],   /* Source peer */
                           &peers[mportid],   /* Dest peer */
                           &before, 
                           &after,
                           &limit);
            break;

        case DUAL_CARD:
            ret = run_test(demo_mode,
                           destid,
                           (mportid == 0) ? &peers[0] : &peers[1], /* Source */
                           (mportid == 0) ? &peers[1] : &peers[0], /* Dest   */
                           &before, 
                           &after,
                           &limit);
            break;

        case DUAL_HOST_SLAVE:
            run_slave(destid,&peers[mportid]);
            break;

        } /* switch */

        /* Keep track of the time for each iteration */
        time_track(i, before, after, &total_time, &min_time, &max_time);
    } /* for */

    /* If we fail, or it is the Slave just quit. No need to compute the time! */
    if (ret != 1) {
        cleanup_msg_test(num_devices,peers);
        return 1;
    }

	/* Compute and display times */
    if (demo_mode != DUAL_HOST_SLAVE) {
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
    }

    /* Free up resources */
    cleanup_msg_test(num_devices,peers);

    puts("Message latency test completed.");

    DPRINT("%s: exit normally\n", __FUNCTION__);

    return 0;
} /* main() */
