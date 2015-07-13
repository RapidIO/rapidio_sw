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

#ifndef TSI721_MSG_H
#define TSI721_MSG_H

#include <stdint.h>

#include "tsi721_dma.h"

#define RIO_MAX_MSG_SIZE	0x1000

/*
 * Messaging definitions
 */
#define TSI721_MSG_BUFFER_SIZE		RIO_MAX_MSG_SIZE
#define TSI721_MSG_MAX_SIZE		RIO_MAX_MSG_SIZE
#define TSI721_IMSG_MAXCH		8
#define TSI721_IMSG_CHNUM		TSI721_IMSG_MAXCH
#define TSI721_IMSGD_MIN_RING_SIZE	32
#define TSI721_IMSGD_RING_SIZE		512

#define TSI721_OMSG_CHNUM		4 /* One channel per MBOX */
#define TSI721_OMSGD_MIN_RING_SIZE	32
#define TSI721_OMSGD_RING_SIZE		512

/*
 * Outbound Messaging Engine Registers
 *   x = 0..7
 */

#define TSI721_OBDMAC_DWRCNT(x)		(0x61000 + (x) * 0x1000)

#define TSI721_OBDMAC_DRDCNT(x)		(0x61004 + (x) * 0x1000)

#define TSI721_OBDMAC_CTL(x)		(0x61008 + (x) * 0x1000)
#define TSI721_OBDMAC_CTL_MASK		0x00000007
#define TSI721_OBDMAC_CTL_RETRY_THR	0x00000004
#define TSI721_OBDMAC_CTL_SUSPEND	0x00000002
#define TSI721_OBDMAC_CTL_INIT		0x00000001

#define TSI721_OBDMAC_INT(x)		(0x6100c + (x) * 0x1000)
#define TSI721_OBDMAC_INTSET(x)		(0x61010 + (x) * 0x1000)
#define TSI721_OBDMAC_INTE(x)		(0x61018 + (x) * 0x1000)
#define TSI721_OBDMAC_INT_MASK		0x0000001F
#define TSI721_OBDMAC_INT_ST_FULL	0x00000010
#define TSI721_OBDMAC_INT_DONE		0x00000008
#define TSI721_OBDMAC_INT_SUSPENDED	0x00000004
#define TSI721_OBDMAC_INT_ERROR		0x00000002
#define TSI721_OBDMAC_INT_IOF_DONE	0x00000001
#define TSI721_OBDMAC_INT_ALL		TSI721_OBDMAC_INT_MASK

#define TSI721_OBDMAC_STS(x)		(0x61014 + (x) * 0x1000)
#define TSI721_OBDMAC_STS_MASK		0x007f0000
#define TSI721_OBDMAC_STS_ABORT		0x00400000
#define TSI721_OBDMAC_STS_RUN		0x00200000
#define TSI721_OBDMAC_STS_CS		0x001f0000

#define TSI721_OBDMAC_PWE(x)		(0x6101c + (x) * 0x1000)
#define TSI721_OBDMAC_PWE_MASK		0x00000002
#define TSI721_OBDMAC_PWE_ERROR_EN	0x00000002

#define TSI721_OBDMAC_DPTRL(x)		(0x61020 + (x) * 0x1000)
#define TSI721_OBDMAC_DPTRL_MASK	0xfffffff0

#define TSI721_OBDMAC_DPTRH(x)		(0x61024 + (x) * 0x1000)
#define TSI721_OBDMAC_DPTRH_MASK	0xffffffff

#define TSI721_OBDMAC_DSBL(x)		(0x61040 + (x) * 0x1000)
#define TSI721_OBDMAC_DSBL_MASK		0xffffffc0

#define TSI721_OBDMAC_DSBH(x)		(0x61044 + (x) * 0x1000)
#define TSI721_OBDMAC_DSBH_MASK		0xffffffff

#define TSI721_OBDMAC_DSSZ(x)		(0x61048 + (x) * 0x1000)
#define TSI721_OBDMAC_DSSZ_MASK		0x0000000f

#define TSI721_OBDMAC_DSRP(x)		(0x6104c + (x) * 0x1000)
#define TSI721_OBDMAC_DSRP_MASK		0x0007ffff

#define TSI721_OBDMAC_DSWP(x)		(0x61050 + (x) * 0x1000)
#define TSI721_OBDMAC_DSWP_MASK		0x0007ffff

//#define TSI721_RQRPTO			0x60010
#define TSI721_RQRPTO_MASK		0x00ffffff
#define TSI721_RQRPTO_VAL		400	/* Response TO value */

/*
 * Inbound Messaging Engine Registers
 *   x = 0..7
 */

#define TSI721_IB_DEVID_GLOBAL		0xffff
#define TSI721_IBDMAC_FQBL(x)		(0x61200 + (x) * 0x1000)
#define TSI721_IBDMAC_FQBL_MASK		0xffffffc0

#define TSI721_IBDMAC_FQBH(x)		(0x61204 + (x) * 0x1000)
#define TSI721_IBDMAC_FQBH_MASK		0xffffffff

#define TSI721_IBDMAC_FQSZ_ENTRY_INX	TSI721_IMSGD_RING_SIZE
#define TSI721_IBDMAC_FQSZ(x)		(0x61208 + (x) * 0x1000)
#define TSI721_IBDMAC_FQSZ_MASK		0x0000000f

#define TSI721_IBDMAC_FQRP(x)		(0x6120c + (x) * 0x1000)
#define TSI721_IBDMAC_FQRP_MASK		0x0007ffff

#define TSI721_IBDMAC_FQWP(x)		(0x61210 + (x) * 0x1000)
#define TSI721_IBDMAC_FQWP_MASK		0x0007ffff

#define TSI721_IBDMAC_FQTH(x)		(0x61214 + (x) * 0x1000)
#define TSI721_IBDMAC_FQTH_MASK		0x0007ffff
#if 0
#define TSI721_IB_DEVID			0x60020
#endif
#define TSI721_IB_DEVID_MASK		0x0000ffff

#define TSI721_IBDMAC_CTL(x)		(0x61240 + (x) * 0x1000)
#define TSI721_IBDMAC_CTL_MASK		0x00000003
#define TSI721_IBDMAC_CTL_SUSPEND	0x00000002
#define TSI721_IBDMAC_CTL_INIT		0x00000001

#define TSI721_IBDMAC_STS(x)		(0x61244 + (x) * 0x1000)
#define TSI721_IBDMAC_STS_MASK		0x007f0000
#define TSI721_IBSMAC_STS_ABORT		0x00400000
#define TSI721_IBSMAC_STS_RUN		0x00200000
#define TSI721_IBSMAC_STS_CS		0x001f0000

#define TSI721_IBDMAC_INT(x)		(0x61248 + (x) * 0x1000)
#define TSI721_IBDMAC_INTSET(x)		(0x6124c + (x) * 0x1000)
#define TSI721_IBDMAC_INTE(x)		(0x61250 + (x) * 0x1000)
#define TSI721_IBDMAC_INT_MASK		0x0000100f
#define TSI721_IBDMAC_INT_SRTO		0x00001000
#define TSI721_IBDMAC_INT_SUSPENDED	0x00000008
#define TSI721_IBDMAC_INT_PC_ERROR	0x00000004
#define TSI721_IBDMAC_INT_FQ_LOW	0x00000002
#define TSI721_IBDMAC_INT_DQ_RCV	0x00000001
#define TSI721_IBDMAC_INT_ALL		TSI721_IBDMAC_INT_MASK

#define TSI721_IBDMAC_PWE(x)		(0x61254 + (x) * 0x1000)
#define TSI721_IBDMAC_PWE_MASK		0x00001700
#define TSI721_IBDMAC_PWE_SRTO		0x00001000
#define TSI721_IBDMAC_PWE_ILL_FMT	0x00000400
#define TSI721_IBDMAC_PWE_ILL_DEC	0x00000200
#define TSI721_IBDMAC_PWE_IMP_SP	0x00000100

#define TSI721_IBDMAC_DQBL(x)		(0x61300 + (x) * 0x1000)
#define TSI721_IBDMAC_DQBL_MASK		0xffffffc0
#define TSI721_IBDMAC_DQBL_ADDR		0xffffffc0

#define TSI721_IBDMAC_DQBH(x)		(0x61304 + (x) * 0x1000)
#define TSI721_IBDMAC_DQBH_MASK		0xffffffff

#define TSI721_IBDMAC_DQRP(x)		(0x61308 + (x) * 0x1000)
#define TSI721_IBDMAC_DQRP_MASK		0x0007ffff

#define TSI721_IBDMAC_DQWR(x)		(0x6130c + (x) * 0x1000)
#define TSI721_IBDMAC_DQWR_MASK		0x0007ffff

#define TSI721_IBDMAC_DQSZ(x)		(0x61314 + (x) * 0x1000)
#define TSI721_IBDMAC_DQSZ_MASK		0x0000000f


/*
 * Messaging Engine Interrupts
 */
#if 0
#define TSI721_SMSG_PWE			0x6a004

#define TSI721_SMSG_INTE		0x6a000
#define TSI721_SMSG_INT			0x6a008
#define TSI721_SMSG_INTSET		0x6a010
#define TSI721_SMSG_INT_MASK		0x0086ffff
#define TSI721_SMSG_INT_UNS_RSP		0x00800000
#define TSI721_SMSG_INT_ECC_NCOR	0x00040000
#define TSI721_SMSG_INT_ECC_COR		0x00020000
#define TSI721_SMSG_INT_ECC_NCOR_CH	0x0000ff00
#define TSI721_SMSG_INT_ECC_COR_CH	0x000000ff
#endif
//#define TSI721_SMSG_ECC_LOG		0x6a014
#define TSI721_SMSG_ECC_LOG_MASK	0x00070007
#define TSI721_SMSG_ECC_LOG_ECC_NCOR_M	0x00070000
#define TSI721_SMSG_ECC_LOG_ECC_COR_M	0x00000007

//#define TSI721_RETRY_GEN_CNT		0x6a100
#define TSI721_RETRY_GEN_CNT_MASK	0xffffffff

//#define TSI721_RETRY_RX_CNT		0x6a104
#define TSI721_RETRY_RX_CNT_MASK	0xffffffff

#define TSI721_SMSG_ECC_COR_LOG(x)	(0x6a300 + (x) * 4)
#define TSI721_SMSG_ECC_COR_LOG_MASK	0x000000ff

#define TSI721_SMSG_ECC_NCOR(x)		(0x6a340 + (x) * 4)
#define TSI721_SMSG_ECC_NCOR_MASK	0x000000ff


/*
 * Inbound Messaging Descriptor
 */
struct tsi721_imsg_desc {
	uint32_t type_id;

#define TSI721_IMD_DEVID	0x0000ffff
#define TSI721_IMD_CRF		0x00010000
#define TSI721_IMD_PRIO		0x00060000
#define TSI721_IMD_TT		0x00180000
#define TSI721_IMD_DTYPE	0xe0000000

	uint32_t msg_info;

#define TSI721_IMD_BCOUNT	0x00000ff8
#define TSI721_IMD_SSIZE	0x0000f000
#define TSI721_IMD_LETER	0x00030000
#define TSI721_IMD_XMBOX	0x003c0000
#define TSI721_IMD_MBOX		0x00c00000
#define TSI721_IMD_CS		0x78000000
#define TSI721_IMD_HO		0x80000000

	uint32_t bufptr_lo;
	uint32_t bufptr_hi;
	uint32_t    reserved[12];

} __attribute__ ((aligned(64)));

/*
 * Outbound Messaging Descriptor
 */
struct tsi721_omsg_desc {
	uint32_t type_id;

#define TSI721_OMD_DEVID	0x0000ffff
#define TSI721_OMD_CRF		0x00010000
#define TSI721_OMD_PRIO		0x00060000
#define TSI721_OMD_IOF		0x08000000
#define TSI721_OMD_DTYPE	0xe0000000
#define TSI721_OMD_RSRVD	0x17f80000

	uint32_t msg_info;

#define TSI721_OMD_BCOUNT	0x00000ff8
#define TSI721_OMD_SSIZE	0x0000f000
#define TSI721_OMD_LETTER	0x00030000
#define TSI721_OMD_XMBOX	0x003c0000
#define TSI721_OMD_MBOX		0x00c00000
#define TSI721_OMD_TT		0x0c000000

	union {
		uint32_t bufptr_lo;	/* if DTYPE == 4 */
		uint32_t next_lo;		/* if DTYPE == 5 */
	};

	union {
		uint32_t bufptr_hi;	/* if DTYPE == 4 */
		uint32_t next_hi;		/* if DTYPE == 5 */
	};

} __attribute__ ((aligned(16)));

struct tsi721_imsg_ring {
	uint32_t		size;
	/* VA/PA of data buffers for incoming messages */
	void		*buf_base;
	dma_addr_t	buf_phys;
	/* VA/PA of circular free buffer list */
	void		*imfq_base;
	dma_addr_t	imfq_phys;
	/* VA/PA of Inbound message descriptors */
	void		*imd_base;
	dma_addr_t	imd_phys;
	 /* Inbound Queue buffer pointers */
	void		*imq_base[TSI721_IMSGD_RING_SIZE];

	uint32_t		rx_slot;
	void		*dev_id;
	uint32_t		fq_wrptr;
	uint32_t		desc_rdptr;
//	spinlock_t	lock;
};

struct tsi721_omsg_ring {
	uint32_t		size;
	/* VA/PA of OB Msg descriptors */
	void		*omd_base;
	dma_addr_t	omd_phys;
	/* VA/PA of OB Msg data buffers */
	void		*omq_base[TSI721_OMSGD_RING_SIZE];
	dma_addr_t	omq_phys[TSI721_OMSGD_RING_SIZE];
	/* VA/PA of OB Msg descriptor status FIFO */
	void		*sts_base;
	dma_addr_t	sts_phys;
	uint32_t		sts_size; /* # of allocated status entries */
	uint32_t		sts_rdptr;

	uint32_t		tx_slot;
	void		*dev_id;
	uint32_t		wr_count;
//	spinlock_t	lock;
};

enum tsi721_flags {
	TSI721_USING_MSI	= (1 << 0),
	TSI721_USING_MSIX	= (1 << 1),
	TSI721_IMSGID_SET	= (1 << 2),
};

#endif


