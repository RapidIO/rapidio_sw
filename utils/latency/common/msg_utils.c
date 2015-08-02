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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>

#include "debug.h"

#include "IDT_Tsi721.h"
#include "rio_register_utils.h"
#include "tsi721_dma.h"
#include "tsi721_msg.h"
#include "dma_utils.h"
#include "peer_utils.h"
#include <rapidio_mport_mgmt.h>#include <rapidio_mport_rdma.h>#include <rapidio_mport_sock.h>


/**
 * Given an outbound descriptor, dump it to screen.
 *
 * @desc    pointer to descriptor to dump
 */
static void dump_ob_desc(struct tsi721_omsg_desc *desc)
{
    DPRINT("DEVID=0x%04X, ", desc->type_id & TSI721_OMD_DEVID);
    DPRINT("CRF=%d, ", (desc->type_id & TSI721_OMD_CRF) >> 16);
    DPRINT("PRIO=%d, ", (desc->type_id & TSI721_OMD_PRIO) >> 17);
    DPRINT("IOF=%d, ", (desc->type_id & TSI721_OMD_IOF) >> 27);
    DPRINT("DTYPE=%d\n", (desc->type_id & TSI721_OMD_DTYPE) >> 29);
    DPRINT("BCOUNT=%d, ", desc->msg_info & TSI721_OMD_BCOUNT);
    DPRINT("SSIZE=%d, ", (desc->msg_info & TSI721_OMD_SSIZE) >> 12);
    DPRINT("LETTER=%d, ", (desc->msg_info & TSI721_OMD_LETTER) >> 16);
    DPRINT("XMBOX=%d, ", (desc->msg_info & TSI721_OMD_XMBOX) >> 18);
    DPRINT("MBOX=%d, ", (desc->msg_info & TSI721_OMD_MBOX) >> 22);
    DPRINT("TT=%d\n", (desc->msg_info & TSI721_OMD_TT) >> 26);
    DPRINT("BUFFER_PTR=0x%08X%08X\n", desc->bufptr_hi, desc->bufptr_lo);
} /* dump_ob_desc() */


/**
 * Given an outbound descriptor, dump it to screen.
 *
 * @desc    pointer to descriptor to dump
 */
static void dump_ib_desc(struct tsi721_imsg_desc *desc)
{
    DPRINT("DEVID=0x%04X, ", desc->type_id & TSI721_IMD_DEVID);
    DPRINT("CRF=%d, ", (desc->type_id & TSI721_IMD_CRF) >> 16);
    DPRINT("PRIO=%d, ", (desc->type_id & TSI721_IMD_PRIO) >> 17);
    DPRINT("DTYPE=%d\n", (desc->type_id & TSI721_IMD_DTYPE) >> 29);

    DPRINT("BCOUNT=%d, ", desc->msg_info & TSI721_IMD_BCOUNT);
    DPRINT("SSIZE=%d, ", (desc->msg_info & TSI721_IMD_SSIZE) >> 12);
    DPRINT("LETTER=%d, ", (desc->msg_info & TSI721_IMD_LETER) >> 16);
    DPRINT("XMBOX=%d, ", (desc->msg_info & TSI721_IMD_XMBOX) >> 18);
    DPRINT("CS=%d, ", (desc->msg_info & TSI721_IMD_CS) >> 27);
    DPRINT("HO=%d\n", (desc->msg_info & TSI721_IMD_HO) >> 31);
    DPRINT("BUFFER_PTR=0x%08X%08X\n", desc->bufptr_hi, desc->bufptr_lo);
} /* dump_ib_desc() */


/**
 * Given a channel number, dump relevant registers to screen
 *
 * @peer	Pointer to peer_info struct
 * @ch		Channel number to dump message registers for
 */
static void dump_msg_regs(struct peer_info *peer, int ch)
{
    DPRINT("OUTBOUND MESSAGING REGISTERS:\n");
    DPRINT("DWRCNT = %X\t", RIORegister(peer, TSI721_OBDMAC_DWRCNT(ch)));
    DPRINT("DRDCNT = %X\n", RIORegister(peer, TSI721_OBDMAC_DRDCNT(ch)));
    DPRINT("CTL = %X\t", RIORegister(peer, TSI721_OBDMAC_CTL(ch)));
    DPRINT("INT = %X\n", RIORegister(peer, TSI721_OBDMAC_INT(ch)));
    DPRINT("STS = %X\t", RIORegister(peer, TSI721_OBDMAC_STS(ch)));
    DPRINT("DPTRL = %X\n", RIORegister(peer, TSI721_OBDMAC_DPTRL(ch)));
    DPRINT("DPTRH = %X\t", RIORegister(peer, TSI721_OBDMAC_DPTRH(ch)));
    DPRINT("DSBL = %X\n", RIORegister(peer, TSI721_OBDMAC_DSBL(ch)));
    DPRINT("DSBH = %X\t", RIORegister(peer, TSI721_OBDMAC_DSBH(ch)));
    DPRINT("DSSZ = %X\n", RIORegister(peer, TSI721_OBDMAC_DSSZ(ch)));
    DPRINT("DSRP = %X\t", RIORegister(peer, TSI721_OBDMAC_DSRP(ch)));
    DPRINT("DSWP = %X\n", RIORegister(peer, TSI721_OBDMAC_DSWP(ch)));
} /* dump_msg_regs() */


/**
 * Opens outbound mailbox, including all memory allocation.
 * 
 * @peer    pointer to peer_info struct
 * @mbox    which mailbox to use
 * @entries number of entries in ring
 *
 * @return 	1 if successful  < 0 if unsuccessful
 */
int open_outb_mbox(struct peer_info *peer, int mbox, uint32_t entries)
{
	struct tsi721_omsg_desc *bd_ptr;
	int sts_size, rc = 0;
    unsigned num_desc;
	uint32_t i;

    DPRINT("%s: mbox = %d, entries = %d\n", __FUNCTION__, mbox, entries);

    assert(peer);

   	peer->omsg_ring[mbox].size = entries;
	peer->omsg_ring[mbox].sts_rdptr = 0;
 
   	/* Outbound Msg Buffer allocation */
	for (i = 0; i < entries; i++) {
        DPRINT("%s: Allocating buffer for entry %d\n", __FUNCTION__, i);
        /* 4K for each entry. For the demo it is 1 entry */
		peer->omsg_ring[mbox].omq_base[i] = dmatest_buf_alloc(peer->mport_fd,
                                                       ONE_PAGE_LENGTH,
                                           &peer->omsg_ring[mbox].omq_phys[i]);
        DPRINT("%s: peer->omsg_ring[mbox].omq_phys[%d] = 0x%lX",
                                            __FUNCTION__,
                                            i,
                                            peer->omsg_ring[mbox].omq_phys[i]);
                                        
		if (peer->omsg_ring[mbox].omq_base[i] == NULL) {
            fprintf(stderr,"Unable to allocate OB buffer for MBOX%d\n", mbox);
			rc = -ENOMEM;
			goto out_buf;
		}
	}

	/* Outbound message descriptor allocation */
    DPRINT("%s: Allocating descriptor for MBOX%d\n", __FUNCTION__, mbox);
	peer->omsg_ring[mbox].omd_base =  dmatest_buf_alloc(peer->mport_fd, 
				ONE_PAGE_LENGTH,  /* for all descriptors */
				&peer->omsg_ring[mbox].omd_phys);
	if (peer->omsg_ring[mbox].omd_base == NULL) {
        fprintf(stderr,"Unable to allocate OB descriptors for MBOX%d\n", mbox);
		rc = -ENOMEM;
		goto out_buf;
	}
    /* Number of descriptors */
    peer->num_ob_desc = ONE_PAGE_LENGTH / sizeof(struct tsi721_omsg_desc);
    num_desc = peer->num_ob_desc;
    DPRINT("%s: There are %u outbound descriptors\n", __FUNCTION__, num_desc); 

    /* Outbound message descriptor status FIFO allocation */
	sts_size = ((entries + 1) >= TSI721_DMA_MINSTSSZ) ? (entries + 1) : 
                                                        TSI721_DMA_MINSTSSZ;
	sts_size = roundup_pow_of_two(sts_size);
    /* If the rounded size is less than DMA_STATUS_FIFO_LENGTH, use the latter */
    sts_size = (sts_size < DMA_STATUS_FIFO_LENGTH) ? DMA_STATUS_FIFO_LENGTH : 
                                                     sts_size;
    peer->omsg_ring[mbox].sts_size = sts_size;
    DPRINT("%s: Allocating status FIFO for MBOX%d\n", __FUNCTION__, mbox);
    peer->omsg_ring[mbox].sts_base = dmatest_buf_alloc(
                                            peer->mport_fd,
                                      		peer->omsg_ring[mbox].sts_size,
                                			&peer->omsg_ring[mbox].sts_phys);
    if (peer->omsg_ring[mbox].sts_base == NULL) {
        fprintf(stderr,"Unable to allocate OB MSG status FIFO for MBOX%d\n",
                                                                      mbox); 
		rc = -ENOMEM;
		goto out_desc;
	}
   
	/**
	 * Configure Outbound Messaging Engine
	 */
    DPRINT("%s: Configure outbound messaging engine\n", __FUNCTION__);

	/* Setup Outbound Message descriptor pointer */
    WriteRIORegister(peer,
                     TSI721_OBDMAC_DPTRH(mbox), 
                     peer->omsg_ring[mbox].omd_phys >> 32);
    WriteRIORegister(peer, 
                     TSI721_OBDMAC_DPTRL(mbox), 
                     peer->omsg_ring[mbox].omd_phys & TSI721_OBDMAC_DPTRL_MASK);

	/* Setup Outbound Message descriptor status FIFO */
    WriteRIORegister(peer,
                     TSI721_OBDMAC_DSBH(mbox),
                     peer->omsg_ring[mbox].sts_phys >> 32);
    WriteRIORegister(peer,
                     TSI721_OBDMAC_DSBL(mbox),
                     peer->omsg_ring[mbox].sts_phys & TSI721_OBDMAC_DSBL_MASK);
    WriteRIORegister(peer,
                     (uint32_t)TSI721_OBDMAC_DSSZ(mbox),
                     TSI721_DMAC_DSSZ_SIZE(peer->omsg_ring[mbox].sts_size));

	/* Initialize Outbound Message descriptors ring */
	bd_ptr = (struct tsi721_omsg_desc *)peer->omsg_ring[mbox].omd_base;
	bd_ptr[num_desc - 1].type_id  = DTYPE5 << 29;
	bd_ptr[num_desc - 1].msg_info = 0;
	bd_ptr[num_desc - 1].next_lo  =
		(uint64_t)peer->omsg_ring[mbox].omd_phys & TSI721_OBDMAC_DPTRL_MASK;
	bd_ptr[num_desc - 1].next_hi =
		(uint64_t)peer->omsg_ring[mbox].omd_phys >> 32;
	peer->omsg_ring[mbox].wr_count = 0;

    DPRINT("%s: Last descriptor:\n", __FUNCTION__);
    dump_ob_desc(&bd_ptr[num_desc - 1]);

	/* Initialize Outbound Message engine */
    WriteRIORegister(peer, TSI721_OBDMAC_CTL(mbox), TSI721_OBDMAC_CTL_INIT);
    (void)RIORegister(peer, TSI721_OBDMAC_CTL(mbox));
	usleep(10);

	peer->omsg_init[mbox] = 1;  /* Mark mailbox as being initialized */

    dump_msg_regs(peer, mbox);
    DPRINT("%s: All is good, returning 1\n",__FUNCTION__);

	return 1;

out_desc:
    /* Free allocated descriptors */
    dmatest_buf_free(peer->mport_fd,
                     peer->omsg_ring[mbox].omd_base,
                     ONE_PAGE_LENGTH,
                     &peer->omsg_ring[mbox].omd_phys);
    peer->omsg_ring[mbox].omd_base = NULL;

out_buf:
    /* Free allocated message buffers */
	for (i = 0; i < peer->omsg_ring[mbox].size; i++) {
		if (peer->omsg_ring[mbox].omq_base[i]) {
            dmatest_buf_free( peer->mport_fd,
                              peer->omsg_ring[mbox].omq_base[i],
                              TSI721_MSG_BUFFER_SIZE,
				              &peer->omsg_ring[mbox].omq_phys[i]);
			peer->omsg_ring[mbox].omq_base[i] = NULL;
		}
	}
 
    return rc;
} /* open_outb_mbox() */


/**
 * Closes specified mailbox on specified peer including all memory deallocation
 *
 * @peer    pointer to peer_into struct
 * @mbox    mailbox number to close
 */
void close_outb_mbox(struct peer_info *peer, int mbox)
{
    int i, entries;

    assert(peer);

    entries = peer->omsg_ring[mbox].size;

    DPRINT("%s:ENTER\n",__FUNCTION__);

    /* Check if mbox was not open and initialized */
    if (!peer->omsg_init[mbox]) {
        /* Use DPRINT() since we may close unopened mailboxes */
        DPRINT("%s: mbox %d is not open, returning!\n", __FUNCTION__, mbox);
        return;
    }
    peer->omsg_init[mbox] = 0;  /* Mark as uninitialized */

    /* Free status descriptors */
    dmatest_buf_free(peer->mport_fd,
                     peer->omsg_ring[mbox].sts_base,
               		 peer->omsg_ring[mbox].sts_size,
                     &peer->omsg_ring[mbox].sts_phys);
    peer->omsg_ring[mbox].sts_base = NULL;

    /* Free message descriptors */
    dmatest_buf_free(peer->mport_fd,
                     peer->omsg_ring[mbox].omd_base,
                     ONE_PAGE_LENGTH,
                     &peer->omsg_ring[mbox].omd_phys);
    peer->omsg_ring[mbox].omd_base = NULL;

    /* Free allocated message buffers */
	for (i = 0; i < entries; i++) {
		if (peer->omsg_ring[mbox].omq_base[i]) {
            dmatest_buf_free( peer->mport_fd,
                              peer->omsg_ring[mbox].omq_base[i],
                              TSI721_MSG_BUFFER_SIZE,
				              &peer->omsg_ring[mbox].omq_phys[i]);
			peer->omsg_ring[mbox].omq_base[i] = NULL;
		}
	}
} /* close_outb_mbox() */


/**
 * Add specified message buffer to the outbound message ring
 *
 * @peer    pointer to peer_info struct
 * @mbox    mailbox number to send message to
 * @buffer  pointer to buffer containing message contents
 * @len     length, in bytes, of 'buffer'
 *
 * @return 	1 if successful  < 0 if unsuccessful
 */
int add_outb_message(struct peer_info *peer,
                     int mbox,
                     void *buffer,
                     size_t len)
{
    assert(peer);
    assert(buffer);

    /* Check for uninizialized mailbox */
    if (!peer->omsg_init[mbox]) {
        fprintf(stderr,"%s: OB message engine not initialized!\n",__FUNCTION__);
        return -1;
    }

    /* Check for invalid message length */
    if (len > TSI721_MSG_MAX_SIZE || len < 8) {
        fprintf(stderr,"%s: Invalid length (%d), failing\n", __FUNCTION__,
                                                            (int)len);
        return -2;
    }

    /* Copy the message into the allocated message queue at tx_slot */
	memcpy(peer->omsg_ring[mbox].omq_base[0], buffer, len);

    return 1;
} /* add_outb_message() */


/**
 * Send message, already added to OB ring, to specified destination.
 *
 * @destid  Device ID of the recipient of the message
 * @mbox    Mailbox to receive the message
 * @len     Message length, in bytes
 *
 * @return 	1 if successful  < 0 if unsuccessful
 */
int send_outb_message(struct peer_info *peer,
                      uint16_t destid,
                      int mbox,
                      size_t len)
{
    uint32_t tx_slot;
    uint32_t reg;
    struct tsi721_omsg_desc *desc;
#ifdef DEBUG
    uint32_t rd_count;
    int timeout = 100000;
#endif

    assert(peer);

    tx_slot = peer->omsg_ring[mbox].tx_slot;
    DPRINT("%s: tx_slot = %d\n", __FUNCTION__, tx_slot);

    /* Adjust length (round up to multiple of 8 bytes) */
    if (len & 0x7)
		len += 8;

    /* Build descriptor associated with buffer */
	desc = (struct tsi721_omsg_desc *)peer->omsg_ring[mbox].omd_base;
    assert(desc);

    /* TYPE4 and destination ID */
	desc[tx_slot].type_id = (DTYPE4 << 29) | destid;
    /* 16-bit destid, mailbox number, 0xE means 4K, length */
	desc[tx_slot].msg_info = (1 << 26) | 
                             (mbox << 22) | 
                             (0xe << 12) |
                             (len & 0xff8);
    /* Buffer pointer points to physical address of current tx_slot */
	desc[tx_slot].bufptr_lo = 
                (uint64_t)peer->omsg_ring[mbox].omq_phys[0] & 0xffffffff;
	desc[tx_slot].bufptr_hi =
        		(uint64_t)peer->omsg_ring[mbox].omq_phys[0] >> 32;

    /* Increment WR_COUNT */
	peer->omsg_ring[mbox].wr_count++;

    /* Dump contents of descriptor for debugging only */
    dump_ob_desc(&desc[tx_slot]);

	/* Go to next slot, if only 1 slot, wrap-around to 0 */
	if (++peer->omsg_ring[mbox].tx_slot == peer->num_ob_desc) {
        DPRINT("%s: tx_slot reset to 0\n",__FUNCTION__);
		peer->omsg_ring[mbox].tx_slot = 0;
		/* Move through the ring link descriptor at the end */
		peer->omsg_ring[mbox].wr_count++;
	}

    /* For debugging, print the INT register as well as RD_COUNT & WR_COUNT */
#ifdef DEBUG 
    rd_count = RIORegister(peer,TSI721_OBDMAC_DRDCNT(mbox));
    reg = RIORegister(peer, TSI721_OBDMAC_INT(mbox));
#endif
    DPRINT("%s: Before: OBDMAC_INT = %08X\n", __FUNCTION__, reg);
    DPRINT("%s: Before: rd_count = %08X, wr_count = %08X\n", __FUNCTION__,
                                                     rd_count,
                                                     peer->omsg_ring[mbox].wr_count);

  	/* Set new write count value */
	WriteRIORegister(peer,
                     TSI721_OBDMAC_DWRCNT(mbox),
                     peer->omsg_ring[mbox].wr_count);
	(void)RIORegister(peer,TSI721_OBDMAC_DWRCNT(mbox));

#ifdef DEBUG
    /* Now poll the RDCNT until it is equal to the WRCNT */
    /* This tells us the descriptors were processed */
    do {
        rd_count = RIORegister(peer, TSI721_OBDMAC_DRDCNT(mbox));
    } while(rd_count < peer->omsg_ring[mbox].wr_count);

    DPRINT("%s: After: rd_count = %08X, wr_count = %08X\n", 
                                                __FUNCTION__,
                                                rd_count,
                                                peer->omsg_ring[mbox].wr_count);

    /* Wait until DMA message transfer is finished */
    do {
        reg = RIORegister(peer,TSI721_OBDMAC_STS(mbox));
    } while ((reg & TSI721_OBDMAC_STS_RUN) && timeout--); 

    if (timeout <= 0) {
        fprintf(stderr,"%s: DMA[%d] read timeout CH_STAT = %08X\n",
                __FUNCTION__, mbox, reg);
                return -EIO;
    }
#endif     
    reg = RIORegister(peer, TSI721_OBDMAC_INT(mbox));
    DPRINT("%s: After: OBDMAC_INT = %08X\n", __FUNCTION__, reg);

    if (reg & TSI721_OBDMAC_INT_DONE) {
        /* All is good, DONE bit is set */
        DPRINT("%s: All is good, returning 1\n",__FUNCTION__);
        goto out;

    } else if (reg & TSI721_OBDMAC_INT_ERROR) {
        /* If there is an error, read the status register for more info */
        reg = RIORegister(peer, TSI721_OBDMAC_STS(mbox)); 
        DPRINT("%s: OBDMAC_STS = %08X\n", __FUNCTION__, reg);
        if (reg & TSI721_DMAC_STS_ABORT) {
            fprintf(stderr,"%s: ABORTed on outbound message\n",__FUNCTION__);
            reg >>= 16;
            reg &= 0x1F;

            switch (reg) {

            case 0x04: fprintf(stderr,"%s: S-RIO excessive msg retry occurred\n",
                                                                 __FUNCTION__);
                break;
            case 0x05: fprintf(stderr,"%s: S-RIO response timeout occurred\n",
                                                                 __FUNCTION__);
                break;

            case 0x1F: fprintf(stderr,"%s: S-RIO message ERR response received\n",
                                                                 __FUNCTION__);
                break;

            case 8:
            case 9:
            case 11:
            case 12:
                fprintf(stderr,"%s: PCIE Error (not likely to happen)\n",
                                                                 __FUNCTION__);
                break;
                                                                  
            }
        }
    }

out:
    dump_msg_regs(peer, mbox);
    DPRINT("%s: EXIT\n", __FUNCTION__);
    return 1; 
} /* send_outb_message() */


/**
 * Initialize Tsi721 inbound mailbox
 * 
 * @peer    pointer to peer_info struct
 * @mbox:   Mailbox to open for inbound messages
 * @entries: Number of entries in the inbound mailbox ring
 *
 * @return 	1 if successful  < 0 if unsuccessful
 */
int open_inb_mbox(struct peer_info *peer, int mbox, uint32_t entries)
{
	uint32_t i;
	uint64_t *free_ptr;
	int rc = 0;
	int ch = mbox + 4;  /* For inbound, mbox0 = ch4, mbox1 = ch5, and so on. */

    DPRINT("%s: mbox = %d, entries %d\n", __FUNCTION__, mbox, entries);

	/* Initialize IB Messaging Ring */
	peer->imsg_ring[mbox].size       = entries;
	peer->imsg_ring[mbox].rx_slot    = 0;
	peer->imsg_ring[mbox].desc_rdptr = 0;
	peer->imsg_ring[mbox].fq_wrptr = 0;

	for (i = 0; i < peer->imsg_ring[mbox].size; i++)
		peer->imsg_ring[mbox].imq_base[i] = NULL;
    DPRINT("%s: Cleared %d entries of imq_base\n", __FUNCTION__, i);

  	/* Allocate buffers for incoming messages */
  	DPRINT("%s:Allocate buffers for incoming messages\n", __FUNCTION__); 
    peer->imsg_ring[mbox].buf_base = dmatest_buf_alloc(
                                            peer->mport_fd,
                                            entries * TSI721_MSG_BUFFER_SIZE,
                                            &peer->imsg_ring[mbox].buf_phys);
	if (peer->imsg_ring[mbox].buf_base == NULL) {
	    fprintf(stderr,"%s:Failed to allocate buffers for IB MBOX%d\n",
                                                            __FUNCTION__,
                                                            mbox);
        fflush(stderr);
		return -ENOMEM;
	}

	/* Allocate memory for circular free list */
  	DPRINT("%s:Allocate memory for circular free list\n", __FUNCTION__); 
	peer->imsg_ring[mbox].imfq_base = dmatest_buf_alloc(
                                            peer->mport_fd,
                                            ONE_PAGE_LENGTH,
                                            &peer->imsg_ring[mbox].imfq_phys);
	if (peer->imsg_ring[mbox].imfq_base == NULL) {
	    fprintf(stderr,"%s:Failed to allocate free queue for IB MBOX%d\n",
                                                                __FUNCTION__,
                                                                mbox);
        fflush(stderr);
		rc = -ENOMEM;
		goto out_buf;
	}

	/* Allocate memory for Inbound message descriptors */
  	DPRINT("%s:Allocate memory for inbound message descriptors\n", __FUNCTION__); 
	peer->imsg_ring[mbox].imd_base = dmatest_buf_alloc(
                                            peer->mport_fd,
                                            4*ONE_PAGE_LENGTH,
                                            &peer->imsg_ring[mbox].imd_phys);
	if (peer->imsg_ring[mbox].imd_base == NULL) {
	    fprintf(stderr,"%s:Failed to allocate descriptor memory for IB MBOX%d\n",
                                                                __FUNCTION__,
                                                                mbox);
        fflush(stderr);
		rc = -ENOMEM;
		goto out_dma;
	}

	/* Fill free buffer pointer list */
	free_ptr = (uint64_t *)peer->imsg_ring[mbox].imfq_base;
	for (i = 0; i < entries; i++) {
		free_ptr[i] = 
        (uint64_t)(peer->imsg_ring[mbox].buf_phys) + i * TSI721_MSG_BUFFER_SIZE;
    }

	/*
	 * For mapping of inbound SRIO Messages into appropriate queues we need
	 * to set Inbound Device ID register in the messaging engine. We do it
	 * once when first inbound mailbox is requested.
	 */
	if (!(peer->flags & TSI721_IMSGID_SET)) {
        WriteRIORegister(peer, TSI721_IB_DEVID, peer->device_id);
		peer->flags |= TSI721_IMSGID_SET;
        DPRINT("%s: IB_DEVID = %08X\n", __FUNCTION__,
                                        RIORegister(peer,TSI721_IB_DEVID));
	}

	/*
	 * Configure Inbound Messaging channel (ch = mbox + 4)
	 */
    DPRINT("%s: Configuring channel %d\n", __FUNCTION__, ch);

	/* Setup Inbound Message free queue */
    WriteRIORegister(peer,
                     TSI721_IBDMAC_FQBH(ch),
                     (uint64_t)peer->imsg_ring[mbox].imfq_phys >> 32);

    WriteRIORegister(peer,
                     TSI721_IBDMAC_FQBL(ch),
                     (uint64_t)peer->imsg_ring[mbox].imfq_phys & 
                     TSI721_IBDMAC_FQBL_MASK);

    WriteRIORegister(peer,
                     TSI721_IBDMAC_FQSZ(ch),
                     TSI721_DMAC_DSSZ_SIZE(entries));

	/* Setup Inbound Message descriptor queue */
    WriteRIORegister(peer, 
                     TSI721_IBDMAC_DQBH(ch),
                     (uint64_t)peer->imsg_ring[mbox].imd_phys >> 32);

    WriteRIORegister(peer,
                     TSI721_IBDMAC_DQBL(ch),
                     (uint32_t)peer->imsg_ring[mbox].imd_phys & 
                     (uint32_t)TSI721_IBDMAC_DQBL_MASK);

    WriteRIORegister(peer,
                     TSI721_IBDMAC_DQSZ(ch),
                     TSI721_DMAC_DSSZ_SIZE(entries));


	/* Initialize Inbound Message Engine */
    WriteRIORegister(peer,
                     TSI721_IBDMAC_CTL(ch),
                     TSI721_IBDMAC_CTL_INIT);

	(void)RIORegister(peer, TSI721_IBDMAC_CTL(ch));
	usleep(10);
	peer->imsg_ring[mbox].fq_wrptr = entries - 1;
    WriteRIORegister(peer,
                     TSI721_IBDMAC_FQWP(ch),
                     entries - 1);

	peer->imsg_init[mbox] = 1;
	return 1;


out_dma:
    dmatest_buf_free(peer->mport_fd, peer->imsg_ring[mbox].imfq_base,
                                     ONE_PAGE_LENGTH,
                                     &peer->imsg_ring[mbox].imfq_phys);
	peer->imsg_ring[mbox].imfq_base = NULL;

out_buf:
    dmatest_buf_free(peer->mport_fd, peer->imsg_ring[mbox].buf_base,
                                     peer->imsg_ring[mbox].size * TSI721_MSG_BUFFER_SIZE,
		                             &peer->imsg_ring[mbox].buf_phys);
	peer->imsg_ring[mbox].buf_base = NULL;

	return rc;
} /* open_inb_mbox() */


/**
 * Shut down Tsi721 inbound mailbox
 * 
 * @peer    Pointer to peer_info struct
 * @mboxl   Mailbox to close
 */
void close_inb_mbox(struct peer_info *peer, int mbox)
{
	uint32_t rx_slot;

	if (!peer->imsg_init[mbox]) /* mbox isn't initialized yet */
		return;
	peer->imsg_init[mbox] = 0;  /* Mark as uninitialized */


	/* Clear Inbound Buffer Queue */
	for (rx_slot = 0; rx_slot < peer->imsg_ring[mbox].size; rx_slot++)
		peer->imsg_ring[mbox].imq_base[rx_slot] = NULL;

	/* Free memory allocated for message buffers */
    dmatest_buf_free(peer->mport_fd,
                     peer->imsg_ring[mbox].buf_base,
                     peer->imsg_ring[mbox].size * TSI721_MSG_BUFFER_SIZE,
		             &peer->imsg_ring[mbox].buf_phys);
	peer->imsg_ring[mbox].buf_base = NULL;

	/* Free memory allocated for free pointr list */
    dmatest_buf_free(peer->mport_fd,
                     peer->imsg_ring[mbox].imfq_base,
                     ONE_PAGE_LENGTH,
                     &peer->imsg_ring[mbox].imfq_phys);
	peer->imsg_ring[mbox].imfq_base = NULL;

	/* Free memory allocated for RX descriptors */
    dmatest_buf_free(peer->mport_fd, 
                     peer->imsg_ring[mbox].imd_base,
                     ONE_PAGE_LENGTH,
		             &peer->imsg_ring[mbox].imd_phys);
	peer->imsg_ring[mbox].imd_base = NULL;
} /* close_inb_mbox() */


/**
 * Add buffer to the Tsi721 inbound message queue
 *
 * @peer    Pointer to peer_info struct
 * @mbox    Inbound mailbox number
 * @buf     Buffer to add to inbound queue
 *
 * @return 	1 if successful  < 0 if unsuccessful
 */
int add_inb_buffer(struct peer_info *peer, int mbox, void *buf)
{
	uint32_t rx_slot;

	rx_slot = peer->imsg_ring[mbox].rx_slot;
    
    /* There is something wrong if we are trying to add a buffer to 
     * a slot that is not NULL since:
     * 1. we initialize them all to NULL; and
     * 2. we set them back to NULL after reading from them.
     */
	if (peer->imsg_ring[mbox].imq_base[rx_slot]) {
		fprintf(stderr,"%s: Error adding inbound buffer %d, buffer exists\n",
            __FUNCTION__,
			rx_slot);
		return -EINVAL;
	}

    /* This is where the data will be when we read it */
	peer->imsg_ring[mbox].imq_base[rx_slot] = buf;

    DPRINT("%s: Buffer %p added in slot %d\n", __FUNCTION__, buf, rx_slot);
    DPRINT("%s:rx_slot = %d, size = %d\n", __FUNCTION__, rx_slot, 
                                           peer->imsg_ring[mbox].size);
    
    /* Increment rx_slot, wrapping around if necessary */
	if (++peer->imsg_ring[mbox].rx_slot == peer->imsg_ring[mbox].size) {
        DPRINT("%s: rx_slot = %d, resetting to 0\n", __FUNCTION__, rx_slot);
		peer->imsg_ring[mbox].rx_slot = 0;
    }

    DPRINT("%s: EXITing with rx_slot = %d\n",__FUNCTION__,
                                             peer->imsg_ring[mbox].rx_slot);

	return 1;
} /* add_inb_buffer() */


/**
 * Returns whether a message is ready to be read on the inbound.
 *
 * @peer    Pointer to peer_info struct
 * @return  1 if true, -1 if timeout.
 */
int inb_message_ready(struct peer_info *peer)
{
    int32_t rd_ptr, wr_ptr;
    int ch, timeout = 100000;

    assert(peer);

    ch = peer->ib_mbox + 4;
#ifdef DEBUG
        rd_ptr = (int32_t)RIORegister(peer, TSI721_IBDMAC_DQRP(ch));
        wr_ptr = (int32_t)RIORegister(peer, TSI721_IBDMAC_DQWR(ch));
#endif
    do {
        rd_ptr = (int32_t)RIORegister(peer, TSI721_IBDMAC_DQRP(ch));
        wr_ptr = (int32_t)RIORegister(peer, TSI721_IBDMAC_DQWR(ch));
        if( wr_ptr == 255)
            wr_ptr = -1;
    } while(wr_ptr <= rd_ptr && timeout--);

    if (timeout<=0) { 
        return -1;
    }
    DPRINT("%s:EXIT: DQRP = %X, DQWR = %X\n", __FUNCTION__, rd_ptr, wr_ptr);
    return 1;
} /* inb_message_ready() */


/**
 * Fetch inbound message from the Tsi721 MSG Queue. The fetched message
 * is placed in the buffer provided by add_inb_buffer(). Therefore the
 * return value of this function maybe ignored.
 *
 * @peer    Pointer to peer_info struct
 * @mbox    Inbound mailbox number
 *
 * @return pointer to the message on success or NULL on failure.
 */
void *get_inb_message(struct peer_info *peer, int mbox)
{
	struct tsi721_imsg_desc *desc;
	uint32_t rx_slot;
	void *rx_virt = NULL;
	uint64_t rx_phys;
	void *buf = NULL;
	uint64_t *free_ptr;
	int ch = mbox + 4;
	int msg_size;

	if (!peer->imsg_init[mbox])
		return NULL;

    /* Point to the correct descriptor by adding the desc_rdptr
     * to the base address of the descriptors.
     */
	desc = (struct tsi721_imsg_desc *)peer->imsg_ring[mbox].imd_base;
	desc += peer->imsg_ring[mbox].desc_rdptr;

	rx_slot = peer->imsg_ring[mbox].rx_slot;

    DPRINT("%s: INBOUND DESCRIPTOR CONTENTS:\n", __FUNCTION__);
	dump_ib_desc(desc);

	if (!(desc->msg_info & TSI721_IMD_HO)) {
        fprintf(stderr,"%s:TSI721_IMD_HO not set.\n", __FUNCTION__);
        fprintf(stderr,"%s:mbox = %d, desc address = %p, rx_slot = %d\n",
                                                            __FUNCTION__,
                                                            mbox,
                                                            desc,
                                                            rx_slot);
        return NULL;
    }

    DPRINT("%s:mbox = %d, desc address = %p, initially rx_slot = %d\n",
                                                              __FUNCTION__,
                                                              mbox,
                                                              desc,
                                                              rx_slot);
	while (peer->imsg_ring[mbox].imq_base[rx_slot] == NULL) {
		if (++rx_slot == peer->imsg_ring[mbox].size)
			rx_slot = 0;
	}

    DPRINT("%s: Now rx_slot = %d, imq_base[rx_slot] = %p\n", __FUNCTION__,
                                           rx_slot,
                                           peer->imsg_ring[mbox].imq_base[rx_slot]);

    /* Physical address of this message from the descriptor */
	rx_phys = ((uint64_t)desc->bufptr_hi << 32) | desc->bufptr_lo;

    /* Get the correct virtual address of the buffer containing the message
     * by adding the size (computed as the difference between the message's
     * physical address and the physical address of the entire buffer) to
     * the base virtual address of the entire buffer.
     */
	rx_virt = (uint8_t *)peer->imsg_ring[mbox].buf_base +
		  (rx_phys - (uint64_t)peer->imsg_ring[mbox].buf_phys);

    /* imq_base[rx_slot] was populated in add_inb_buffer() */
	buf = peer->imsg_ring[mbox].imq_base[rx_slot];
	msg_size = desc->msg_info & TSI721_IMD_BCOUNT;
	if (msg_size == 0)
		msg_size = RIO_MAX_MSG_SIZE;

    DPRINT("%s: Copying buffer %p, size = %d from slot %d\n", __FUNCTION__, 
                                                             buf,
                                                             msg_size,
                                                             rx_slot);
    /* Copy the message contents to 'buf' */
	memcpy(buf, rx_virt, msg_size);

    /* Now set this buffer pointer back to NULL */
	peer->imsg_ring[mbox].imq_base[rx_slot] = NULL;

	desc->msg_info &= ~TSI721_IMD_HO;

    /* Increment the rdptr, wrapping around if necessary */
	if (++peer->imsg_ring[mbox].desc_rdptr == peer->imsg_ring[mbox].size)
		peer->imsg_ring[mbox].desc_rdptr = 0;

    /* Update the Descriptor Queue Read Pointer */
    WriteRIORegister(peer,
                     TSI721_IBDMAC_DQRP(ch),
                     peer->imsg_ring[mbox].desc_rdptr);
    DPRINT("%s: Now DQRP = %d\n", __FUNCTION__, 
                                 peer->imsg_ring[mbox].desc_rdptr);

	/* Return free buffer into the pointer list */
	free_ptr = (uint64_t *)peer->imsg_ring[mbox].imfq_base;
	free_ptr[peer->imsg_ring[mbox].fq_wrptr] = rx_phys;

    /* Increment free queue pointer, wrapping around if necessary */
	if (++peer->imsg_ring[mbox].fq_wrptr == peer->imsg_ring[mbox].size)
		peer->imsg_ring[mbox].fq_wrptr = 0;

    /* Update the free queue write pointer in the FQWP register */
    WriteRIORegister(peer,
                     TSI721_IBDMAC_FQWP(ch),
                     peer->imsg_ring[mbox].fq_wrptr);

	return buf;

} /* get_inb_message() */



/**
 * Initialization of Messaging Engine
 *
 * @peer    Pointer to peer_info struct
 */
void init_messaging_engine(struct peer_info *peer)
{
    int	ch;

    WriteRIORegister(peer, TSI721_SMSG_ECC_LOG, 0);
    WriteRIORegister(peer, TSI721_RETRY_GEN_CNT, 0);
    WriteRIORegister(peer, TSI721_RETRY_RX_CNT, 0);
    
    /* Set SRIO Message Request/Response Timeout */
    WriteRIORegister(peer, TSI721_RQRPTO, TSI721_RQRPTO_VAL);

    /* Initialize Inbound Messaging Engine Registers */
    for (ch = 0; ch < TSI721_IMSG_CHNUM; ch++) {
        /* Clear interrupt bits */
        WriteRIORegister(peer, TSI721_IBDMAC_INT(ch),
                               TSI721_IBDMAC_INT_MASK);
		/* Clear Status */
		WriteRIORegister(peer, TSI721_IBDMAC_STS(ch),0);
		WriteRIORegister(peer, TSI721_SMSG_ECC_COR_LOG(ch),
                               TSI721_SMSG_ECC_COR_LOG_MASK);
		WriteRIORegister(peer, TSI721_SMSG_ECC_NCOR(ch),
                               TSI721_SMSG_ECC_NCOR_MASK);
	}
} /* messages_init() */

