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
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>

#include "debug.h"

#include "rio_register_utils.h"
#include "tsi721_dma.h"
#include "dma_utils.h"
#include "inbound_utils.h"
#include <rapidio_mport_mgmt.h>#include <rapidio_mport_rdma.h>#include <rapidio_mport_sock.h>


int roundup_pow_of_two( int n )
{
    uint32_t mask = 0x80000000;

    /* Corner case, n = 0 */
    if (n == 0) return 1;

    /* If already a power of two we're done */
    if (is_pow_of_two( n )) return n;
    
    /* Find the highest '1' bit */
    while (((n & mask) == 0) && (mask != 0)) {
        mask >>= 1;
    }
    
    /* Return next higher mask */
    return mask << 1;
} /* roundup_pow_of_two() */


uint32_t dma_get_alloc_data_length( uint32_t length )
{
    uint32_t q = length / DMA_DATA_LENGTH_MULTIPLE;
    uint32_t r = length % DMA_DATA_LENGTH_MULTIPLE;

    return (r == 0) ? DMA_DATA_LENGTH_MULTIPLE * q : DMA_DATA_LENGTH_MULTIPLE * (q + 1);
} /* dma_get_alloc_data_length() */


void *dmatest_buf_alloc(int fd, uint32_t size, uint64_t *handle)
{
	void *buf_ptr = NULL;
	uint64_t h;
	int ret;

	if (handle) {
		ret = riomp_rdma_dbuf_alloc(fd, size, &h);
		if (ret) {
			fprintf(stderr,"riomp_rdma_dbuf_alloc failed err=%d\n", ret);
			return NULL;
		}

		buf_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, h);
		if (buf_ptr == MAP_FAILED) {
			perror("mmap");
			buf_ptr = NULL;
			ret = riomp_rdma_dbuf_free(fd, handle);
			if (ret)
				fprintf(stderr, "riomp_rdma_dbuf_free failed err=%d\n", ret);
		} else
			*handle = h;
	} else {
        fprintf(stderr,"%s: handle==NULL, failing.\n",__FUNCTION__);
        return NULL;
	}

    DPRINT("%s:Allocated %d bytes at phys=0x%lx & virt=%p\n", __FUNCTION__,
                                                              size,
                                                              *handle,
                                                              buf_ptr);
	return buf_ptr;
} /* dmatest_buf_alloc() */


void dmatest_buf_free(int fd, void *buf, uint32_t size, uint64_t *handle)
{
	if (handle && *handle) { 
		int ret;

		if (munmap(buf, size))
			perror("munmap");

		ret = riomp_rdma_dbuf_free(fd, handle);
		if (ret)
			fprintf(stderr,"%s:riomp_rdma_dbuf_free failed err=%d\n", __FUNCTION__,
                                                                 ret);
	} 
} /* dmatest_buf_free() */


static int dma_ch_init(struct peer_info *peer)
{
    /* Parameters */
    int mport_fd;
    struct tsi721_bdma_chan *bdma_chan;
    int bd_num;
    int channel;

     
    /* Original */
    uint64_t dma_descriptors_p;                 /* Physical address */
    struct tsi721_dma_desc *dma_descriptors_v;  /* Virtual address  */

	uint64_t *sts_ptr           /* Virtual address  */;
	uint64_t sts_phys;          /* Physical address */
	int		 sts_size;         
    
    uint32_t reg;   /* For reading back registers */
    (void) reg;     /* silence compiler warning */


    if (!peer) {
        fprintf(stderr,"%s: peer==NULL, failing.\n", __FUNCTION__);
        return -1;
    }

    /* Extract parameters */
    mport_fd  = peer->mport_fd;
    bdma_chan = &peer->the_channel;
    bd_num    = peer->bd_num;
    channel   = peer->channel_num;
 
    DPRINT("Init Block DMA Engine, CH%d\n", channel);

    bdma_chan->id = channel;

   	/*
	 * Allocate space for DMA descriptors
	 * (add an extra element for link descriptor)
	 */
    dma_descriptors_v = (struct tsi721_dma_desc *)dmatest_buf_alloc(mport_fd, 
                                          DMA_DESCRIPTORS_LENGTH,
                                          &dma_descriptors_p);
    if (dma_descriptors_v == NULL) {
        perror( "Failed to allocate memory for descriptors" );
        return -ENOMEM;
    }
    bdma_chan->bd_num  = bd_num;
    bdma_chan->bd_phys = dma_descriptors_p;
    bdma_chan->bd_base = dma_descriptors_v;

	DPRINT("DMA descriptors @ %p (phys = %llx)\n", dma_descriptors_v, 
                                         (unsigned long long)dma_descriptors_p);

	/* 
     * Allocate space for descriptor status FIFO 
     *
     */
	sts_size = ((bd_num + 1) >= TSI721_DMA_MINSTSSZ) ?
					(bd_num + 1) : TSI721_DMA_MINSTSSZ;
	sts_size = roundup_pow_of_two(sts_size);

    /* If the rounded size is less than DMA_STATUS_LENGTH, use the latter */
    sts_size = (sts_size < DMA_STATUS_FIFO_LENGTH) ? DMA_STATUS_FIFO_LENGTH : sts_size;

    sts_ptr  = (uint64_t *)dmatest_buf_alloc(mport_fd,
                                 sts_size,
                                 &sts_phys);
	if (!sts_ptr) {
    	/* Free space allocated for DMA descriptors */
        dmatest_buf_free(mport_fd,bdma_chan->bd_base,DMA_DESCRIPTORS_LENGTH,
                                                     &bdma_chan->bd_phys);
        bdma_chan->bd_base = NULL;
        perror("Failed to allocate descriptor status FIFO");
		return -ENOMEM;
	}

	bdma_chan->sts_phys = sts_phys;
	bdma_chan->sts_base = sts_ptr;
	bdma_chan->sts_size = sts_size;

	/* 
     * Initialize DMA descriptors ring using added link descriptor 
     */
    dma_descriptors_v[bd_num].type_id = DTYPE3 << 29;
    dma_descriptors_v[bd_num].next_lo = (uint64_t)dma_descriptors_p & TSI721_DMAC_DPTRL_MASK;
    dma_descriptors_v[bd_num].next_hi = (uint64_t)dma_descriptors_p >> 32;

    /*
     *  Setup DMA descriptor pointers
     */
    WriteDMARegister(peer,TSI721_DMAC_DPTRH,(uint64_t)dma_descriptors_p >> 32);
    WriteDMARegister(peer,TSI721_DMAC_DPTRL, (uint64_t)dma_descriptors_p & TSI721_DMAC_DPTRL_MASK); 

    /* Setup descriptor status FIFO */
    WriteDMARegister(peer,TSI721_DMAC_DSBH,(uint64_t)sts_phys >> 32);
    WriteDMARegister(peer,TSI721_DMAC_DSBL,(uint64_t)sts_phys & TSI721_DMAC_DSBL_MASK);
    WriteDMARegister(peer,TSI721_DMAC_DSSZ,TSI721_DMAC_DSSZ_SIZE(sts_size));

    /* Clear interrupt bits */
    WriteDMARegister(peer,TSI721_DMAC_INT,TSI721_DMAC_INT_ALL);
    reg = DMARegister(peer,TSI721_DMAC_INT);
    DPRINT("%s:TSI721_DMAC_INT = %08X\n",__FUNCTION__,reg);

    /* Toggle DMA channel initialization */
    WriteDMARegister(peer,TSI721_DMAC_CTL,TSI721_DMAC_CTL_INIT); 
    reg = DMARegister(peer,TSI721_DMAC_CTL);
    bdma_chan->wr_count      = 0;
    bdma_chan->wr_count_next = 0;
    bdma_chan->sts_rdptr     = 0;
    usleep(10);

    DPRINT("%s:Completed\n",__FUNCTION__);
    return 1;
} /* dma_ch_init() */


static int dma_ch_free(struct peer_info *peer)
{
    uint32_t channel_status;
    struct tsi721_bdma_chan *bdma_chan;

    if (peer == NULL) {
        fprintf(stderr, "%s: peer==NULL, exiting.\n",__FUNCTION__);
        return -1;
    }

    /* Extract parameters from peer struct */
    bdma_chan = &peer->the_channel;

    /* This takes care of trying to free a channel in a call to
     * cleanup_dma_test() before it has been allocated */
    if (bdma_chan->bd_base == NULL)
        return 0;

	/* Check if DMA channel still running */
    channel_status = DMARegister(peer, TSI721_DMAC_STS );
    if (channel_status & TSI721_DMAC_STS_RUN) {
        fprintf(stderr,"dma_ch_free(): DMA channel is still running!");
        return -EFAULT;
    }

    /* Put DMA channel into init state */
    WriteDMARegister(peer,TSI721_DMAC_CTL, TSI721_DMAC_CTL_INIT);

    /* Free space allocated for DMA descriptors */
    dmatest_buf_free(peer->mport_fd, 
                     bdma_chan->bd_base,
                     DMA_DESCRIPTORS_LENGTH,
                     &bdma_chan->bd_phys);
    bdma_chan->bd_base = NULL;

    /* Free space allocated for status FIFO */
    dmatest_buf_free(peer->mport_fd, 
                     bdma_chan->sts_base,
                     bdma_chan->sts_size,
                     &bdma_chan->sts_phys);
    bdma_chan->sts_base = NULL;

    DPRINT("dma_ch_free: done.\n");
    return 1;
} /* dma_ch_free() */


#define TSI721_DMA_TX_QUEUE_SZ	16	/* number of transaction descriptors */

int dma_alloc_chan_resources(struct peer_info *peer)
{
    struct tsi721_tx_desc *desc = NULL;
    struct tsi721_bdma_chan *bdma_chan;

    if (peer == NULL) {
        fprintf(stderr,"peer is NULL, exiting\n");
        return -1;
    }

    bdma_chan = &peer->the_channel;

    /* Is it already initialized? */
    if (bdma_chan->bd_base)
        return TSI721_DMA_TX_QUEUE_SZ;

    /* Initialize BDMA channel */
    if (dma_ch_init(peer) < 0) {
        return -ENODEV;
    }

    /* Allocate queue of transaction descriptors */
    desc = (struct tsi721_tx_desc *)calloc(TSI721_DMA_TX_QUEUE_SZ, sizeof(struct tsi721_tx_desc));
    if (!desc) {
        perror("Failed to allocate transaction descriptors");
        dma_ch_free(peer);
        return -ENOMEM;
    }
    DPRINT("%s:Tx descriptor allocated at %p\n",__FUNCTION__,desc);
    
    bdma_chan->tx_desc = desc;

    DPRINT("%s:Tx descriptor inside the peer is at %p\n",
                                __FUNCTION__,peer->the_channel.tx_desc);

    return TSI721_DMA_TX_QUEUE_SZ;
} /* dma_alloc_chan_resources() */


void dma_free_chan_resources(struct peer_info *peer)
{
    struct tsi721_bdma_chan *bdma_chan;

    if (!peer) {
        fprintf(stderr,"%s: peer==NULL, failing.",__FUNCTION__);
        return;
    }

    bdma_chan = &peer->the_channel;
    DPRINT("%s: for channel %d\n",__FUNCTION__,bdma_chan->id);

	if (bdma_chan->bd_base == NULL)
		return;

   	free(bdma_chan->tx_desc);

	dma_ch_free(peer);

} /* dma_free_chan_resources() */


int dma_slave_transmit_prep(
                            struct peer_info *peer_src,
                            enum dma_dtype *dtype,
                            uint32_t *rd_count)
{
    struct tsi721_bdma_chan *bdma_chan;
    volatile uint8_t *data;
    uint32_t dataLength;
    uint64_t phys_addr;
    struct tsi721_dma_desc *bd_ptr;

    /* Check for NULL pointer */
    if (!peer_src ) {
        fprintf(stderr,"%s: peer_src==NULL, failing.\n", __FUNCTION__);
        return -1;
    }    
    
    /* Determine type based on data length */
    *dtype = (peer_src->dma_data_length <= MAX_BDMA_INLINE_DATA_LEN) ? DTYPE2 : 
                                                                      DTYPE1; 
    /* Extract parameters */
    bdma_chan   = &peer_src->the_channel;
    data        = peer_src->src;
    dataLength  = peer_src->dma_data_length;
    phys_addr   = peer_src->dma_data_p;
    
    /* Echo the parameters for debugging */
    DPRINT("%s: DTYPE = %d\n", __FUNCTION__, *dtype);
    DPRINT("%s: data = %p\n", __FUNCTION__, data);
    DPRINT("%s: dataLength = %d\n", __FUNCTION__, dataLength);
    DPRINT("%s: phys_addr = 0x%lx\n", __FUNCTION__, phys_addr);

    bd_ptr = (struct tsi721_dma_desc *)bdma_chan->bd_base;

    /* Check for NULL buffer descriptor pointer */
    if (!bd_ptr) {
        fprintf(stderr,"%s: bdma_chan->base==NULL,failing.\n",__FUNCTION__);
        return -1;
    }
    DPRINT("%s: bd_ptr = %p\n", __FUNCTION__,bd_ptr);

    /* Handle both types of descriptors */
    if (*dtype == DTYPE2) {
        memcpy(bd_ptr[0].data, (void *)data, dataLength);
        /* Debug only */
        DPRINT("%x %x %x %x\n", bd_ptr[0].data[0],
                                bd_ptr[0].data[1],
                                bd_ptr[0].data[2],
                                bd_ptr[0].data[3]);
    } else if (*dtype == DTYPE1) {
        bd_ptr[0].t1.bufptr_lo = phys_addr & 0xFFFFFFFF;
        bd_ptr[0].t1.bufptr_hi = phys_addr >> 32;
        bd_ptr[0].t1.s_dist = 0;
        bd_ptr[0].t1.s_size = 0;
        DPRINT("%s:bufptr_lo = %08X\n", __FUNCTION__,bd_ptr[0].t1.bufptr_lo);
        DPRINT("%s:bufptr_hi = %08X\n", __FUNCTION__,bd_ptr[0].t1.bufptr_hi);
    } else {
        fprintf(stderr,"%s: Invalid descriptor type..failing\n",__FUNCTION__);
        return -1;
    }
 
    /* Get current read count */
    /* NOTE: do not count this time, as a "real" implementation would 
     * track the write counter without having to read the read counter.
     */
    *rd_count = DMARegister(peer_src,TSI721_DMAC_DRDCNT);
    DPRINT("%s: DMAC_DRDCNT (rd_count) = %08X\n",__FUNCTION__,*rd_count);

    return 1;
} /* dma_slave_transmit_prep() */


int dma_slave_transmit_exec(uint16_t destid,
                            enum dma_dtype dtype,
                            uint32_t rd_count,
                            struct peer_info *peer_src,
                            struct timespec *before,
		                    struct timespec *after
                           )
{
    struct tsi721_bdma_chan *bdma_chan;
    struct tsi721_dma_desc *bd_ptr;
    uint32_t    reg;
    int i;

    bdma_chan = &peer_src->the_channel;
    bd_ptr    = (struct tsi721_dma_desc *)bdma_chan->bd_base;

__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, before );
    /* Initialize DMA descriptor */
    bd_ptr[0].type_id  = (dtype << 29) | (ALL_NWRITE << 19) | destid;
    DPRINT("%s:type_id set to %08X\n", __FUNCTION__, bd_ptr[0].type_id);
    bd_ptr[0].bcount   = peer_src->dma_data_length;;
    bd_ptr[0].raddr_lo = 0;
    bd_ptr[0].raddr_hi = 0;
   
    DPRINT("%s: DMA descriptor initialized\n",__FUNCTION__);

    /* Start DMA operation */
    WriteDMARegister(peer_src,TSI721_DMAC_DWRCNT, rd_count + 2); /* was 2 */
    reg = DMARegister(peer_src,TSI721_DMAC_DWRCNT);
    DPRINT("%s:Now DMAC_DWRCNT set to = %08X\n",__FUNCTION__,reg);

__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, after );

    i = 0;

    /* Wait until DMA transfer is finished */
    DPRINT("%s: Now i = %d\n",__FUNCTION__,i);
    while ((reg = DMARegister(peer_src,TSI721_DMAC_STS)) & TSI721_DMAC_STS_RUN) {
        if (++i >= 500000) {
            fprintf(stderr,"%s: DMA[%d] read timeout CH_STAT = %08X\n",
                __FUNCTION__, bdma_chan->id, reg);
                return -EIO;
        }
    }
    DPRINT("%s:DMA transfer finished, i = %d\n",__FUNCTION__,i);

    if (reg & TSI721_DMAC_STS_ABORT) {
        /* If DMA operation aborted due to error,
		 * reinitialize DMA channel
		 */
        fprintf(stderr,"%s: DMA ABORT ch_stat=%08X\n",__FUNCTION__,reg);
        reg >>= 16;
        reg &= 0x1F;

        switch (reg) {

        case 5: 
            fprintf(stderr,"%s:S-RIO rsponse timeout occurred\n",__FUNCTION__);
            break;

        case 6: 
            fprintf(stderr,"%s:S-RIO I/O ERROR resopnse received\n",__FUNCTION__);
            break;

        case 7: 
            fprintf(stderr,"%s:S-RIO implementation specific error occurred\n",
                                                                    __FUNCTION__);
            break;
        default:
            fprintf(stderr,"%s:PCIE error occurred\n",__FUNCTION__);
        }
        WriteDMARegister(peer_src,TSI721_DMAC_INT,TSI721_DMAC_INT_ALL);
        WriteDMARegister(peer_src,TSI721_DMAC_CTL,TSI721_DMAC_CTL_INIT);
        usleep(10);
        WriteDMARegister(peer_src,TSI721_DMAC_DWRCNT,0);
        usleep(1);
        return -EIO;
    }

    /*
	 * Update descriptor status FIFO RD pointer.
	 * NOTE: Skipping check and clear FIFO entries because we are waiting
	 * for transfer to be completed.
	 */
    reg = DMARegister(peer_src,TSI721_DMAC_DSWP);
    WriteDMARegister(peer_src,TSI721_DMAC_DSRP,reg);

    return 1;
} /* dma_slave_transmit_exec() */


/**
 * dma_transmit: 
 */
int dma_transmit(int demo_mode,
                 enum dma_dtype dtype,
                 uint16_t destid,
                 struct peer_info *peer_src,
                 struct peer_info *peer_dst,
		         struct timespec *before,
		         struct timespec *after,
                 uint32_t *limit )
{
    struct tsi721_bdma_chan *bdma_chan;
    volatile uint8_t *data;
    uint32_t dataLength;
    uint64_t phys_addr;

    uint32_t rd_count;
    struct tsi721_dma_desc *bd_ptr;
    int i;
    uint32_t reg;
    uint32_t value;
    volatile uint8_t *inbound_ptr = NULL;
    int chk_i = peer_src->dma_data_length - 1;
    
    /* Check for NULL for 'src'. 'dst' can be NULL in master/slave mode */
    if (!peer_src ) {
        fprintf(stderr,"%s: peer_src==NULL, failing.\n",
			__FUNCTION__);
        return -1;
    }    
   
    /* 'dst' cannot be NULL for loopback and dual-card modes */
    if (demo_mode == LOOPBACK || demo_mode == DUAL_CARD) {
        if (!peer_dst) {
            fprintf(stderr,"%s: peer_src==NULL, failing.\n",
	    		__FUNCTION__);
            return -1;
        }
        inbound_ptr = (volatile uint8_t *)peer_dst->inbound_ptr;
    } else if (demo_mode == DUAL_HOST_MASTER) {
        inbound_ptr = (volatile uint8_t *)peer_src->inbound_ptr;
    }

    /* Extract parameters */
    bdma_chan   = &peer_src->the_channel;
    data        = peer_src->src;
    dataLength  = peer_src->dma_data_length;
    phys_addr   = peer_src->dma_data_p;
    
    /* Echo the parameters for debugging */
    DPRINT("%s: DTYPE = %d\n", __FUNCTION__, dtype);
    DPRINT("%s: destid = %d\n", __FUNCTION__, destid);
    DPRINT("%s: data = %p\n", __FUNCTION__, data);
    DPRINT("%s: dataLength = %d\n", __FUNCTION__, dataLength);
    DPRINT("%s: phys_addr = 0x%lx\n", __FUNCTION__, phys_addr);

    bd_ptr = (struct tsi721_dma_desc *)bdma_chan->bd_base;

    /* Check for NULL buffer descriptor pointer */
    if (!bd_ptr) {
        fprintf(stderr,"%s: bdma_chan->base==NULL,failing.\n",__FUNCTION__);
        return -1;
    }
    DPRINT("%s: bd_ptr = %p\n", __FUNCTION__,bd_ptr);

    /* Handle both types of descriptors */
    if (dtype == DTYPE2) {
        memcpy(bd_ptr[0].data, (void *)data, dataLength);
        /* Debug only */
        DPRINT("%x %x %x %x\n", bd_ptr[0].data[0],
                                bd_ptr[0].data[1],
                                bd_ptr[0].data[2],
                                bd_ptr[0].data[3]);
    } else if (dtype == DTYPE1) {
        bd_ptr[0].t1.bufptr_lo = phys_addr & 0xFFFFFFFF;
        bd_ptr[0].t1.bufptr_hi = phys_addr >> 32;
        bd_ptr[0].t1.s_dist = 0;
        bd_ptr[0].t1.s_size = 0;
        DPRINT("%s:bufptr_lo = %08X\n", __FUNCTION__,bd_ptr[0].t1.bufptr_lo);
        DPRINT("%s:bufptr_hi = %08X\n", __FUNCTION__,bd_ptr[0].t1.bufptr_hi);
    } else {
        fprintf(stderr,"%s: Invalid descriptor type..failing\n",__FUNCTION__);
        return -1;
    }
 
    /* Get current read count */
/* NOTE: do not count this time, as a "real" implementation would 
* track the write counter without having to read the read counter.
*/
    rd_count = DMARegister(peer_src,TSI721_DMAC_DRDCNT);
    DPRINT("%s: DMAC_DRDCNT (rd_count) = %08X\n",__FUNCTION__,rd_count);

__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, before );
__sync_synchronize();
    /* Initialize DMA descriptor */
    bd_ptr[0].type_id  = (dtype << 29) | (ALL_NWRITE << 19) | destid;
    DPRINT("%s:type_id set to %08X\n", __FUNCTION__, bd_ptr[0].type_id);
    bd_ptr[0].bcount   = dataLength;
    bd_ptr[0].raddr_lo = 0;
    bd_ptr[0].raddr_hi = 0;
   
    DPRINT("%s: DMA descriptor initialized\n",__FUNCTION__);

    /* Start DMA operation */
    WriteDMARegister(peer_src,TSI721_DMAC_DWRCNT, rd_count + 2); /* was 2 */
    reg = DMARegister(peer_src,TSI721_DMAC_DWRCNT);
    DPRINT("%s:Now DMAC_DWRCNT set to = %08X\n",__FUNCTION__,reg);

    /* Wait for data to change in the receive buffer */
    if (demo_mode == LOOPBACK || demo_mode == DUAL_CARD || demo_mode == DUAL_HOST_MASTER) {
        while (*limit) {
            value = inbound_read_8(inbound_ptr, chk_i);
            if (value == peer_src->src[chk_i]) break;
            *limit = *limit - 1;
        }
    }
__sync_synchronize();
    clock_gettime( CLOCK_MONOTONIC, after );
__sync_synchronize();

    i = 0;

    /* Wait until DMA transfer is finished */
    DPRINT("%s: Now i = %d\n",__FUNCTION__,i);
    while ((reg = DMARegister(peer_src,TSI721_DMAC_STS)) & TSI721_DMAC_STS_RUN) {
        if (++i >= 500000) {
            fprintf(stderr,"%s: DMA[%d] read timeout CH_STAT = %08X\n",
                __FUNCTION__, bdma_chan->id, reg);
                return -EIO;
        }
    }
    DPRINT("%s:DMA transfer finished, i = %d\n",__FUNCTION__,i);

    if (reg & TSI721_DMAC_STS_ABORT) {
        /* If DMA operation aborted due to error,
		 * reinitialize DMA channel
		 */
        fprintf(stderr,"%s: DMA ABORT ch_stat=%08X\n",__FUNCTION__,reg);
        reg >>= 16;
        reg &= 0x1F;

        switch (reg) {

        case 5: 
            fprintf(stderr,"%s:S-RIO rsponse timeout occurred\n",__FUNCTION__);
            break;

        case 6: 
            fprintf(stderr,"%s:S-RIO I/O ERROR resopnse received\n",__FUNCTION__);
            break;

        case 7: 
            fprintf(stderr,"%s:S-RIO implementation specific error occurred\n",
                                                                    __FUNCTION__);
            break;
        default:
            fprintf(stderr,"%s:PCIE error occurred\n",__FUNCTION__);
        }
        WriteDMARegister(peer_src,TSI721_DMAC_INT,TSI721_DMAC_INT_ALL);
        WriteDMARegister(peer_src,TSI721_DMAC_CTL,TSI721_DMAC_CTL_INIT);
        usleep(10);
        WriteDMARegister(peer_src,TSI721_DMAC_DWRCNT,0);
        usleep(1);
        return -EIO;
    }

    /*
	 * Update descriptor status FIFO RD pointer.
	 * NOTE: Skipping check and clear FIFO entries because we are waiting
	 * for transfer to be completed.
	 */
    reg = DMARegister(peer_src,TSI721_DMAC_DSWP);
    WriteDMARegister(peer_src,TSI721_DMAC_DSRP,reg);

    return 1;
} /* dma_transmit */

