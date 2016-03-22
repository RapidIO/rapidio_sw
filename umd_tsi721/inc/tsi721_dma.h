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

#ifndef TSI721_DMA_H
#define TSI721_DMA_H

/* Note: Definitions adapted from drivers/rapidio/devices/tsi721.h
 */
#define  RIO_TT_CODE_8		0x00000000
#define  RIO_TT_CODE_16		0x00000001

#define TSI721_DMA_MAXCH	8
#define TSI721_DMA_MINSTSSZ	32
#define TSI721_DMA_STSBLKSZ	8


/*
 * Event Management Registers
 */
/*
#define TSI721_RIO_EM_INT_STAT		0x10910
#define TSI721_RIO_EM_INT_STAT_PW_RX	0x00010000

#define TSI721_RIO_EM_INT_ENABLE	0x10914
#define TSI721_RIO_EM_INT_ENABLE_PW_RX	0x00010000

#define TSI721_RIO_EM_DEV_INT_EN	0x10930
#define TSI721_RIO_EM_DEV_INT_EN_INT	0x00000001

*/
/*
 * Block DMA Engine Registers
 *   x = 0..7
 */

#define TSI721_DMAC_BASE(x)	(0x51000 + (x) * 0x1000)

#define TSI721_DMAC_DWRCNT	0x000
#define TSI721_DMAC_DRDCNT	0x004

#define TSI721_DMAC_CTL		0x008
#define TSI721_DMAC_CTL_SUSP	0x00000002
#define TSI721_DMAC_CTL_INIT	0x00000001

#define TSI721_DMAC_INT		0x00c
#define TSI721_DMAC_INT_STFULL	0x00000010
#define TSI721_DMAC_INT_DONE	0x00000008
#define TSI721_DMAC_INT_SUSP	0x00000004
#define TSI721_DMAC_INT_ERR	0x00000002
#define TSI721_DMAC_INT_IOFDONE	0x00000001
#define TSI721_DMAC_INT_ALL	0x0000001f

#define TSI721_DMAC_INTSET	0x010

#define TSI721_DMAC_STS		0x014
#define TSI721_DMAC_STS_ABORT	0x00400000
#define TSI721_DMAC_STS_RUN	0x00200000
#define TSI721_DMAC_STS_CS	0x001f0000

#define TSI721_DMAC_INTE	0x018

#define TSI721_DMAC_DPTRL	0x024
#define TSI721_DMAC_DPTRL_MASK	0xffffffe0

#define TSI721_DMAC_DPTRH	0x028

#define TSI721_DMAC_DSBL	0x02c
#define TSI721_DMAC_DSBL_MASK	0xffffffc0

#define TSI721_DMAC_DSBH	0x030

#define TSI721_DMAC_DSSZ	0x034
#define TSI721_DMAC_DSSZ_SIZE_M	0x0000000f
#define TSI721_DMAC_DSSZ_SIZE(size)	(__fls(size) - 4)

#define TSI721_DMAC_DSRP	0x038
#define TSI721_DMAC_DSRP_MASK	0x0007ffff

#define TSI721_DMAC_DSWP	0x03c
#define TSI721_DMAC_DSWP_MASK	0x0007ffff


/*
 * Block DMA Descriptors
 */

struct tsi721_dma_desc {
	uint32_t type_id;

#define TSI721_DMAD_DEVID	0x0000ffff
#define TSI721_DMAD_CRF		0x00010000
#define TSI721_DMAD_PRIO	0x00060000
#define TSI721_DMAD_RTYPE	0x00780000
#define TSI721_DMAD_IOF		0x08000000
#define TSI721_DMAD_DTYPE	0xe0000000

	uint32_t bcount;

#define TSI721_DMAD_BCOUNT1	0x03ffffff /* if DTYPE == 1 */
#define TSI721_DMAD_BCOUNT2	0x0000000f /* if DTYPE == 2 */
#define TSI721_DMAD_TT		0x0c000000
#define TSI721_DMAD_RADDR0	0xc0000000

	union {
		uint32_t raddr_lo;	   /* if DTYPE == (1 || 2) */
		uint32_t next_lo;		   /* if DTYPE == 3 */
	};

#define TSI721_DMAD_CFGOFF	0x00ffffff
#define TSI721_DMAD_HOPCNT	0xff000000

	union {
		uint32_t raddr_hi;	   /* if DTYPE == (1 || 2) */
		uint32_t next_hi;		   /* if DTYPE == 3 */
	};

	union {
		struct {		   /* if DTYPE == 1 */
			uint32_t bufptr_lo;
			uint32_t bufptr_hi;
			uint32_t s_dist;
			uint32_t s_size;
		} t1;
		uint32_t data[4];		   /* if DTYPE == 2 */
		uint32_t    reserved[4];	   /* if DTYPE == 3 */
	};
} __attribute__ ((aligned(32)));


/* Descriptor types for BDMA and Messaging blocks */
enum dma_dtype {
	DTYPE1 = 1, /* Data Transfer DMA Descriptor */
	DTYPE2 = 2, /* Immediate Data Transfer DMA Descriptor */
	DTYPE3 = 3, /* Block Pointer DMA Descriptor */
	DTYPE4 = 4, /* Outbound Msg DMA Descriptor */
	DTYPE5 = 5, /* OB Messaging Block Pointer Descriptor */
	DTYPE6 = 6  /* Inbound Messaging Descriptor */
};

enum dma_rtype {
	NREAD = 0,
	LAST_NWRITE_R = 1,
	ALL_NWRITE = 2,
	ALL_NWRITE_R = 3,
	MAINT_RD = 4,
	MAINT_WR = 5
};

#define TSI721_BDMA_MAX_BCOUNT	(TSI721_DMAD_BCOUNT1 + 1)

typedef uint64_t    dma_addr_t; /* FIXME: Double-check */

struct tsi721_tx_desc {
//	struct dma_async_tx_descriptor	txd;
	uint16_t				destid;
	/* low 64-bits of 66-bit RIO address */
	uint64_t				rio_addr;
	/* upper 2-bits of 66-bit RIO address */
	uint8_t				    rio_addr_u;
	enum dma_rtype			rtype;
//	struct list_head		desc_node;
//	struct scatterlist		*sg;
//	unsigned int			sg_len;
//	enum dma_status			status;
};

struct tsi721_bdma_chan {
	int		id;
	void /*__iomem*/	*regs;
	int		bd_num;		/* number of HW buffer descriptors */
	void		*bd_base;	/* start of DMA descriptors */
	dma_addr_t	bd_phys;
	void		*sts_base;	/* start of DMA BD status FIFO */
	dma_addr_t	sts_phys;
	int		sts_size;
	uint32_t		sts_rdptr;
	uint32_t		wr_count;
	uint32_t		wr_count_next;

//	struct dma_chan		dchan;
	struct tsi721_tx_desc	*tx_desc;
//	spinlock_t		lock;
//	struct list_head	active_list;
//	struct list_head	queue;
//	struct list_head	free_list;
//	struct tasklet_struct	tasklet;
//	bool			active;
};

#ifdef __cplusplus
extern "C" {
#endif

void packT3(tsi721_dma_desc* bd_ptr, uint64_t next_ptr);

void packT1n2(tsi721_dma_desc* bd_ptr, 
                uint8_t rtype, uint8_t prio, uint8_t crf,
                uint16_t devid, uint8_t tt, uint32_t bcount,
                uint64_t raddr_lsb64, uint8_t raddr_msb2,
                uint8_t *buffer_ptr);

// 0 - pass, 1 fail
int test_packing(void);

#ifdef __cplusplus
}
#endif

#endif

