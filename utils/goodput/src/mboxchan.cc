#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <map>
#include <vector>
#include <stdexcept>
#include <string>
#include <sstream>
#include <algorithm> // std::sort

#include "mboxchan.h"
#include "Tsi721_fifo.h"

#ifndef RIO_PORT_N_ERR_STATUS_OUTPUT_ERR_STOP
  #define  RIO_PORT_N_ERR_STATUS_OUTPUT_ERR_STOP        0x00010000
#endif
#ifndef RIO_PORT_N_ERR_STATUS_INPUT_ERR_STOP
  #define  RIO_PORT_N_ERR_STATUS_INPUT_ERR_STOP        0x00000100
#endif

#define PAGE_4K    4096

/**
 * Given an outbound descriptor, dump it to screen.
 *
 * @desc    pointer to descriptor to dump
 */
static inline void
dump_ob_desc(hw_omsg_desc* desc)
{
  DBG("  DEVID=0x%04X, ", desc->type_id & TSI721_OMD_DEVID);
  DBG("  CRF=%d, ", (desc->type_id & TSI721_OMD_CRF) >> 16);
  DBG("  PRIO=%d, ", (desc->type_id & TSI721_OMD_PRIO) >> 17);
  DBG("  IOF=%d, ", (desc->type_id & TSI721_OMD_IOF) >> 27);
  DBG("  DTYPE=%d\n", (desc->type_id & TSI721_OMD_DTYPE) >> 29);
  DBG("  BCOUNT=%d, ", desc->msg_info & TSI721_OMD_BCOUNT);
  DBG("  SSIZE=%d, ", (desc->msg_info & TSI721_OMD_SSIZE) >> 12);
  DBG("  LETTER=%d, ", (desc->msg_info & TSI721_OMD_LETTER) >> 16);
  DBG("  XMBOX=%d, ", (desc->msg_info & TSI721_OMD_XMBOX) >> 18);
  DBG("  MBOX=%d, ", (desc->msg_info & TSI721_OMD_MBOX) >> 22);
  DBG("  TT=%d\n", (desc->msg_info & TSI721_OMD_TT) >> 26);
  DBG("  BUFFER_PTR=0x%08X%08X\n", desc->bufptr_hi, desc->bufptr_lo);
}

/**
 * Given an outbound descriptor, dump it to screen.
 *
 * @desc    pointer to descriptor to dump
 */
static inline void
dump_ib_desc(hw_imsg_desc* desc)
{
  DBG("  DEVID=0x%04X, ", desc->type_id & TSI721_IMD_DEVID);
  DBG("  CRF=%d, ", (desc->type_id & TSI721_IMD_CRF) >> 16);
  DBG("  PRIO=%d, ", (desc->type_id & TSI721_IMD_PRIO) >> 17);
  DBG("  DTYPE=%d\n", (desc->type_id & TSI721_IMD_DTYPE) >> 29);

  DBG("  BCOUNT=%d, ", desc->msg_info & TSI721_IMD_BCOUNT);
  DBG("  SSIZE=%d, ", (desc->msg_info & TSI721_IMD_SSIZE) >> 12);
  DBG("  LETTER=%d, ", (desc->msg_info & TSI721_IMD_LETER) >> 16);
  DBG("  XMBOX=%d, ", (desc->msg_info & TSI721_IMD_XMBOX) >> 18);
  DBG("  CS=%d, ", (desc->msg_info & TSI721_IMD_CS) >> 27);
  DBG("  HO=%d\n", (desc->msg_info & TSI721_IMD_HO) >> 31);
  DBG("  BUFFER_PTR=0x%08X%08X\n", desc->bufptr_hi, desc->bufptr_lo);
}

/**
 * Given a channel number, dump relevant registers to screen
 *
 * @peer        Pointer to peer_info struct
 * @ch                Channel number to dump message registers for
 */
static inline void
dump_msg_regs(RioMport* mport, int ch)
{
  if(mport == NULL || ch < 0 || ch > 8) return;

  DBG("OUTBOUND MESSAGING REGISTERS:\n", NULL);
  DBG("  DWRCNT = %X\t", mport->rd32(TSI721_OBDMAC_DWRCNT(ch)));
  DBG("  DRDCNT = %X\n", mport->rd32(TSI721_OBDMAC_DRDCNT(ch)));
  DBG("  CTL = %X\t", mport->rd32(TSI721_OBDMAC_CTL(ch)));
  DBG("  INT = %X\n", mport->rd32(TSI721_OBDMAC_INT(ch)));
  DBG("  STS = %X\n", mport->rd32(TSI721_OBDMAC_STS(ch)));
  DBG("  DPTRL = %X ", mport->rd32(TSI721_OBDMAC_DPTRL(ch)));
  DBG("  DPTRH = %X\n", mport->rd32(TSI721_OBDMAC_DPTRH(ch)));
  DBG("  DSBL = %X ", mport->rd32(TSI721_OBDMAC_DSBL(ch)));
  DBG("  DSBH = %X\n", mport->rd32(TSI721_OBDMAC_DSBH(ch)));
  DBG("  DSSZ = %X\n", mport->rd32(TSI721_OBDMAC_DSSZ(ch)));
  DBG("  DSRP = %X\t", mport->rd32(TSI721_OBDMAC_DSRP(ch)));
  DBG("  DSWP = %X\n", mport->rd32(TSI721_OBDMAC_DSWP(ch)));
}

MboxChannel::MboxChannel(const uint32_t mportid, const uint32_t mboxes) :
  m_mboxen(mboxes)
{
  if(mboxes > 0xF)
    throw std::runtime_error("DMAChannel: Invalid channel set!");

  m_mport = new RioMport(mportid);
  init();
}

MboxChannel::MboxChannel(const uint32_t mportid, const uint32_t mboxes, riomp_mport_t mp_hd) :
  m_mboxen(mboxes)
{
  if(mboxes > 0xF)
    throw std::runtime_error("DMAChannel: Invalid channel set!");

  m_mport = new RioMport(mportid, mp_hd);
  init();
}

void MboxChannel::init()
{
  pthread_spin_init(&m_hw_splock, PTHREAD_PROCESS_PRIVATE);

  for(int i = 0; i < RIO_MAX_MBOX; i++) {
    pthread_spin_init(&m_rx_splock[i], PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&m_tx_splock[i], PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&m_bltx_splock[i], PTHREAD_PROCESS_PRIVATE);
  }

  memset(m_num_ob_desc, 0, sizeof(m_num_ob_desc));
  memset(m_imsg_init, 0, sizeof(m_imsg_init));
  memset(m_imsg_ring, 0, sizeof(m_imsg_ring));
  memset(m_omsg_init, 0, sizeof(m_omsg_init));
  memset(m_omsg_ring, 0, sizeof(m_omsg_ring));
}

void MboxChannel::setInitState()
{
  assert(this);

  m_mport->wr32(TSI721_SMSG_ECC_LOG, 0);
  m_mport->wr32(TSI721_RETRY_GEN_CNT, 0);
  m_mport->wr32(TSI721_RETRY_RX_CNT, 0);
    
  /* Set SRIO Message Request/Response Timeout */
  m_mport->wr32(TSI721_RQRPTO, TSI721_RQRPTO_VAL);

  for(int i = 0; i < RIO_MAX_MBOX; i++) {
    const int mboxmsk = 1<<i;
 
    if(! (m_mboxen & mboxmsk)) continue;

    const int ch = i + 4;

    /* Clear interrupt bits */
    m_mport->wr32(TSI721_IBDMAC_INT(ch), TSI721_IBDMAC_INT_MASK);

    /* Clear Status */
    m_mport->wr32(TSI721_IBDMAC_STS(ch),0);
    m_mport->wr32(TSI721_SMSG_ECC_COR_LOG(ch), TSI721_SMSG_ECC_COR_LOG_MASK);
    m_mport->wr32(TSI721_SMSG_ECC_NCOR(ch), TSI721_SMSG_ECC_NCOR_MASK);
  }
}

static inline bool is_pow_of_two(const uint32_t n) { return ((n & (n - 1)) == 0) ? 1 : 0; }

static inline uint32_t roundup_pow_of_two(const uint32_t n)
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
}

/**
 * __fls - find last (most-significant) set bit in a long word
 * @word: the word to search
 * 
 * Undefined if no set bit exists, so code should check against 0 first.
 */
#define BITS_PER_LONG 32
static inline unsigned long __fls(unsigned long word)
{
  int num = BITS_PER_LONG - 1;

#if BITS_PER_LONG == 64
  if (!(word & (~0ul << 32))) {
    num -= 32;
    word <<= 32;
  }
#endif
  if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
    num -= 16;
    word <<= 16;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
    num -= 8;
    word <<= 8;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
    num -= 4;
    word <<= 4;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
    num -= 2;
    word <<= 2;
  }
  if (!(word & (~0ul << (BITS_PER_LONG-1))))
    num -= 1;
  return num;
}

int MboxChannel::open_inb_mbox(const int mbox, const uint32_t entries)
{
  uint32_t i;
  int rc = 0;
  const int ch = mbox + 4;    /* For inbound, mbox0 = ch4, mbox1 = ch5, and so on. */
  uint64_t* free_ptr = NULL;

  if(m_imsg_init[mbox]) return -EAGAIN;

  /* Initialize IB Messaging Ring */
  m_imsg_ring[mbox].size = entries;
  m_imsg_ring[mbox].rx_slot = 0;
  m_imsg_ring[mbox].desc_rdptr = 0;
  m_imsg_ring[mbox].fq_wrptr = 0;

  for (i = 0; i < m_imsg_ring[mbox].size; i++)
    m_imsg_ring[mbox].imq_base[i] = NULL;

  /* Allocate buffers for incoming messages */
  if(! m_mport->map_dma_buf(entries * TSI721_MSG_BUFFER_SIZE, m_imsg_ring[mbox].buf)) {
    ERR("%s: Failed to allocate buffers for IB MBOX%d\n", __FUNCTION__, mbox);
    fflush(stderr);
    return -ENOMEM;
  }
  memset(m_imsg_ring[mbox].buf.win_ptr, 0, m_imsg_ring[mbox].buf.win_size);
  {
    RioMport::DmaMem_t& mem = m_imsg_ring[mbox].buf;
    DBG("%s: Allocated buffers for incoming messages #buf          - IB MBOX%d size=%d phys=0x%lx virt=%p\n", __FUNCTION__,
            mbox, mem.win_size, mem.win_handle, mem.win_ptr);
  }

  /* Allocate memory for circular free list */
  if(! m_mport->map_dma_buf(PAGE_4K, m_imsg_ring[mbox].imfq)) {
    ERR("%s: Failed to allocate free queue for IB MBOX%d\n", __FUNCTION__, mbox);
    fflush(stderr);
    rc = -ENOMEM;
    goto out_buf;
  }
  memset(m_imsg_ring[mbox].imfq.win_ptr, 0, m_imsg_ring[mbox].imfq.win_size);
  {
    RioMport::DmaMem_t& mem = m_imsg_ring[mbox].imfq;
    DBG("%s: Allocated memory for circular free list #imfq         - IB MBOX%d size=%d phys=0x%lx virt=%p\n", __FUNCTION__,
            mbox, mem.win_size, mem.win_handle, mem.win_ptr);
  }

  /* Allocate memory for Inbound message descriptors */
  if(! m_mport->map_dma_buf(4 * PAGE_4K, m_imsg_ring[mbox].imd)) {
    ERR("%s: Failed to allocate descriptor memory for IB MBOX%d\n", __FUNCTION__, mbox);
    fflush(stderr);
    rc = -ENOMEM;
    goto out_dma;
  }
  memset(m_imsg_ring[mbox].imd.win_ptr, 0, m_imsg_ring[mbox].imd.win_size);
  {
    RioMport::DmaMem_t& mem = m_imsg_ring[mbox].imd;
    DBG("%s: Allocated memory for inbound message descriptors #imd - IB MBOX%d size=%d phys=0x%lx virt=%p\n", __FUNCTION__,
            mbox, mem.win_size, mem.win_handle, mem.win_ptr);
  }

  /* Fill free buffer pointer list */
  free_ptr = (uint64_t *) m_imsg_ring[mbox].imfq.win_ptr;
  for (i = 0; i < entries; i++) {
    free_ptr[i] = (uint64_t) (m_imsg_ring[mbox].buf.win_handle) + i * TSI721_MSG_BUFFER_SIZE;
  }

  /*
   * For mapping of inbound SRIO Messages into appropriate queues we need
   * to set Inbound Device ID register in the messaging engine. 
   */

  /*
   * Configure Inbound Messaging channel (ch = mbox + 4)
   */

  /* Setup Inbound Message free queue */
  m_mport->wr32(TSI721_IBDMAC_FQBH(ch), (uint64_t) m_imsg_ring[mbox].imfq.win_handle >> 32);
  m_mport->wr32(TSI721_IBDMAC_FQBL(ch), (uint64_t) m_imsg_ring[mbox].imfq.win_handle & TSI721_IBDMAC_FQBL_MASK);

  m_mport->wr32(TSI721_IBDMAC_FQSZ(ch), TSI721_DMAC_DSSZ_SIZE(entries));

  /* Setup Inbound Message descriptor queue */
  m_mport->wr32(TSI721_IBDMAC_DQBH(ch), (uint64_t) m_imsg_ring[mbox].imd.win_handle >> 32);
  m_mport->wr32(TSI721_IBDMAC_DQBL(ch), (uint32_t) m_imsg_ring[mbox].imd.win_handle & (uint32_t) TSI721_IBDMAC_DQBL_MASK);

  m_mport->wr32(TSI721_IBDMAC_DQSZ(ch), TSI721_DMAC_DSSZ_SIZE(entries));

  /* Initialize Inbound Message Engine */
  m_mport->wr32(TSI721_IBDMAC_CTL(ch), TSI721_IBDMAC_CTL_INIT);

  (void)m_mport->rd32(TSI721_IBDMAC_CTL(ch));
  usleep(10);
  m_imsg_ring[mbox].fq_wrptr = entries - 1;
  m_mport->wr32(TSI721_IBDMAC_FQWP(ch), entries - 1);

  m_imsg_init[mbox] = true;
  return 0;

out_dma:
  m_mport->unmap_dma_buf(m_imsg_ring[mbox].imfq);
  memset(&m_imsg_ring[mbox].imfq, 0, sizeof(m_imsg_ring[mbox].imfq));

out_buf:
  m_mport->unmap_dma_buf(m_imsg_ring[mbox].buf);
  memset(&m_imsg_ring[mbox].buf, 0, sizeof(m_imsg_ring[mbox].buf));

  return rc;
}

#define CHECK_END_BD(mbox) \
  {{ \
    const hw_omsg_desc* bd_ptr = (hw_omsg_desc*)m_omsg_ring[(mbox)].omd.win_ptr; \
    assert(bd_ptr[m_num_ob_desc[mbox]].type_id == (DTYPE5 << 29)); \
  }}

int MboxChannel::open_outb_mbox(const int mbox, const uint32_t entries)
{
  hw_omsg_desc* bd_ptr = NULL;
  int rc = 0;
  uint32_t num_desc = 0;

  if(m_omsg_init[mbox]) return -EAGAIN;

  uint32_t reg_size = 0; // what we put in FIFO size register
  uint32_t mem_size = 0; // actual size of FIFO memory

  DBG("%s: mbox = %d, entries = %d\n", __FUNCTION__, mbox, entries);

  m_omsg_ring[mbox].size = entries;
  m_omsg_ring[mbox].sts_rdptr = 0;

  /* Outbound Msg Buffer allocation */
  for (int i = 0; i < entries; i++) {
    RioMport::DmaMem_t tmp; memset(&tmp, 0, sizeof(RioMport::DmaMem_t));

    /* 4K for each entry. For the demo it is 1 entry */
    if(! m_mport->map_dma_buf(PAGE_4K, tmp)) {
      ERR("Unable to allocate OB buffer for MBOX%d\n", mbox);
      rc = -ENOMEM;
      goto out_buf;
    }
    m_omsg_ring[mbox].omq.push_back(tmp);
    memset(m_omsg_ring[mbox].omq[i].win_ptr, 0, m_omsg_ring[mbox].omq[i].win_size);
  }

  {{ // F*** g++
    const uint32_t obdesc_size = (entries + 1) * MBOX_BUFF_DESCR_SIZE;
    uint32_t obdesc_pages = obdesc_size / PAGE_4K;
    if(obdesc_size % PAGE_4K) obdesc_pages++;

    /* Outbound message descriptor allocation */
    DBG("%s: Allocating descriptor for MBOX%d as %d 4K pages\n", __FUNCTION__, mbox, obdesc_pages);
    if(! m_mport->map_dma_buf(PAGE_4K*obdesc_pages, m_omsg_ring[mbox].omd)) {
      ERR("Unable to allocate OB descriptors for MBOX%d\n", mbox);
      rc = -ENOMEM;
      goto out_buf;
    }
    memset(m_omsg_ring[mbox].omd.win_ptr, 0, m_omsg_ring[mbox].omd.win_size);
  }}

  /* Number of descriptors */
  m_num_ob_desc[mbox] = entries;
  num_desc            = entries;
  DBG("%s: There are %u outbound descriptors\n", __FUNCTION__, num_desc);

  SizeTsi721Fifo(entries, reg_size, mem_size);

  m_omsg_ring[mbox].sts_size = entries;

  DBG("%s: Allocating status FIFO for MBOX%d - sts_size=%d\n", __FUNCTION__, mbox, entries);
  if(! m_mport->map_dma_buf(mem_size, m_omsg_ring[mbox].sts)) {
    ERR("Unable to allocate OB MSG status FIFO for MBOX%d\n", mbox);
    rc = -ENOMEM;
    goto out_desc;
  }
  memset(m_omsg_ring[mbox].sts.win_ptr, 0, m_omsg_ring[mbox].sts.win_size);

  /**
   * Configure Outbound Messaging Engine
   */

  /* Setup Outbound Message descriptor pointer */
  m_mport->wr32(TSI721_OBDMAC_DPTRH(mbox), m_omsg_ring[mbox].omd.win_handle >> 32);
  m_mport->wr32(TSI721_OBDMAC_DPTRL(mbox), m_omsg_ring[mbox].omd.win_handle & TSI721_OBDMAC_DPTRL_MASK);

  /* Setup Outbound Message descriptor status FIFO */
  m_mport->wr32(TSI721_OBDMAC_DSBH(mbox), m_omsg_ring[mbox].sts.win_handle >> 32);
  m_mport->wr32(TSI721_OBDMAC_DSBL(mbox), m_omsg_ring[mbox].sts.win_handle & TSI721_OBDMAC_DSBL_MASK);
  m_mport->wr32((uint32_t)TSI721_OBDMAC_DSSZ(mbox), reg_size);

  /* Initialize Outbound Message descriptors ring */
  bd_ptr = (hw_omsg_desc*)m_omsg_ring[mbox].omd.win_ptr;
  bd_ptr[num_desc].type_id = DTYPE5 << 29;
  bd_ptr[num_desc].msg_info = 0;
  bd_ptr[num_desc].next_lo = (uint64_t) m_omsg_ring[mbox].omd.win_handle & TSI721_OBDMAC_DPTRL_MASK;
  bd_ptr[num_desc].next_hi = (uint64_t) m_omsg_ring[mbox].omd.win_handle >> 32;
  m_omsg_ring[mbox].wr_count = 0;
  m_omsg_ring[mbox].rd_count_soft = 0;

  DBG("%s: Last descriptor index %d:\n", __FUNCTION__, num_desc);
  dump_ob_desc(&bd_ptr[num_desc]);

  /* Initialize Outbound Message engine */
  {{
    uint32_t init = TSI721_OBDMAC_CTL_INIT;
    init |= TSI721_OBDMAC_CTL_RETRY_THR;
    m_mport->wr32(TSI721_OBDMAC_CTL(mbox), init);
  }}
  usleep(10);
  (void)m_mport->rd32(TSI721_OBDMAC_CTL(mbox));

  dump_msg_regs(m_mport, mbox);

  m_omsg_init[mbox] = true;
  CHECK_END_BD(mbox);
  return 0;

out_desc:
  /* Free allocated descriptors */
  m_mport->unmap_dma_buf(m_omsg_ring[mbox].omd);
  memset(&m_omsg_ring[mbox].omd, 0, sizeof(m_omsg_ring[mbox].omd));

out_buf:
  /* Free allocated message buffers */
  for (int i = 0; i < m_omsg_ring[mbox].size; i++) {
    m_mport->unmap_dma_buf(m_omsg_ring[mbox].omq[i]);
    memset(&m_omsg_ring[mbox].omq[i], sizeof(m_omsg_ring[mbox].omq[i]), 0);
  }

  return rc;
}

void MboxChannel::cleanup()
{
  for(int mbox = 0; mbox < RIO_MAX_MBOX; mbox++) {
    const int mboxmsk = 1<<mbox;

    if(! (m_mboxen & mboxmsk)) continue;

    if(m_imsg_init[mbox]) {
      m_mport->unmap_dma_buf(m_imsg_ring[mbox].imfq);
      m_mport->unmap_dma_buf(m_imsg_ring[mbox].buf);
    }

    if(m_omsg_init[mbox]) {
      m_mport->unmap_dma_buf(m_omsg_ring[mbox].omd);
      for (int i = 0; i < m_omsg_ring[mbox].size; i++)
        m_mport->unmap_dma_buf(m_omsg_ring[mbox].omq[i]);
    }
    for(int i = 0; i < TSI721_IMSGD_RING_SIZE; i++) {
      if(m_imsg_ring[mbox].imq_base[i] == NULL) continue;
      free(m_imsg_ring[mbox].imq_base[i]);
    }
  } // END for mbox
}

bool MboxChannel::open_mbox(const uint32_t entries)
{
  bool fail = false;

  for(int mbox = 0; mbox < RIO_MAX_MBOX; mbox++) {
    const int mboxmsk = 1<<mbox;

    if(! (m_mboxen & mboxmsk)) continue;

    if(open_outb_mbox(mbox, entries) < 0) { fail = true; break; }
    if(open_inb_mbox(mbox, entries) < 0) { fail = true; break; }
  }

  if(fail) {
    cleanup();
    return false;
  }

  return true;
}


/**
 * Send message, already added to OB ring, to specified destination.
 *
 * @destid  Device ID of the recipient of the message
 * @mbox    Mailbox to receive the message
 * @len     Message length, in bytes
 *
 * @return         1 if successful  < 0 if unsuccessful
 */
bool MboxChannel::send_message(const uint16_t destid, const int mbox, const void* data, size_t len)
{
  volatile uint32_t reg = 0;

  if (mbox >= RIO_MAX_MBOX)
    throw std::runtime_error("send_message: Invalid mbox!");

  bool ret = true;

  const int mboxmsk = 1<<mbox;
  if (! (m_mboxen & mboxmsk)) return false;

  if (data == NULL || len < 0 || len > 4096) return false;

  pthread_spin_lock(&m_tx_splock[mbox]);
  if (m_omsg_trk[mbox].bltx_busy.size() >= m_num_ob_desc[mbox]) {
    pthread_spin_unlock(&m_tx_splock[mbox]);
    ERR("%s: Queue full for MBOX%d!\n", __FUNCTION__, mbox);
    return false;
  }

  const uint32_t tx_slot = m_omsg_ring[mbox].tx_slot;
  DDBG("%s: tx_slot = %d\n", __FUNCTION__, tx_slot);

  if (m_omsg_trk[mbox].bl_busy.find(tx_slot) != m_omsg_trk[mbox].bl_busy.end()) {
    pthread_spin_unlock(&m_tx_splock[mbox]);
    ERR("%s: BD%d busy for MBOX%d!\n", __FUNCTION__, tx_slot, mbox);
    return false;
  }
 
  /* Adjust length (round up to multiple of 8 bytes) */
  if (len & 0x7)
    len += 8;

  memcpy(m_omsg_ring[mbox].omq[tx_slot].win_ptr, data, len);
  CHECK_END_BD(mbox);

  /* Build descriptor associated with buffer */
  hw_omsg_desc* desc = (hw_omsg_desc*)m_omsg_ring[mbox].omd.win_ptr;
  assert(desc);

  hw_omsg_desc ldesc; memset(&ldesc, 0, sizeof(ldesc));

  /* TYPE4 and destination ID */
  ldesc.type_id = (DTYPE4 << 29) | destid;
  /* 16-bit destid, mailbox number, 0xE means 4K, length */
  ldesc.msg_info = (1 << 26) | (mbox << 22) | (0xe << 12) | (len & 0xff8);
  /* Buffer pointer points to physical address of current tx_slot */
  ldesc.bufptr_lo = (uint64_t)m_omsg_ring[mbox].omq[tx_slot].win_handle & 0xffffffff;
  ldesc.bufptr_hi = (uint64_t)m_omsg_ring[mbox].omq[tx_slot].win_handle >> 32;

  memcpy(&desc[tx_slot], &ldesc, sizeof(ldesc));
  CHECK_END_BD(mbox);

  /* Increment WR_COUNT */
  m_omsg_ring[mbox].wr_count++;
  m_omsg_ring[mbox].tx_slot++;

  const uint64_t HW_START = m_omsg_ring[mbox].omd.win_handle;

  pthread_spin_lock(&m_bltx_splock[mbox]);
  m_omsg_trk[mbox].bltx_busy[HW_START + tx_slot*MBOX_BUFF_DESCR_SIZE] = m_omsg_ring[mbox].wr_count;
  m_omsg_trk[mbox].bl_busy[tx_slot]++;
  pthread_spin_unlock(&m_bltx_splock[mbox]);

  /* Go to next slot, if only 1 slot, wrap-around to 0 */
  if (m_omsg_ring[mbox].tx_slot == m_num_ob_desc[mbox]) {
    DBG("%s: tx_slot reset to 0 from %d\n", __FUNCTION__, m_omsg_ring[mbox].tx_slot);
    CHECK_END_BD(mbox);
    /* Move through the ring link descriptor at the end */
    m_omsg_ring[mbox].wr_count++;
    m_omsg_ring[mbox].tx_slot = 0;

    pthread_spin_lock(&m_bltx_splock[mbox]);
    m_omsg_trk[mbox].bltx_busy[HW_START + m_num_ob_desc[mbox]*MBOX_BUFF_DESCR_SIZE] = m_omsg_ring[mbox].wr_count; 
    m_omsg_trk[mbox].bl_busy[m_num_ob_desc[mbox]]++;
    pthread_spin_unlock(&m_bltx_splock[mbox]);
  }

  /* For debugging, print the INT register as well as RD_COUNT & WR_COUNT */
#ifdef MBOXDEBUG
  int timeout = 100000;
  uint32_t rd_count = rd32mboxchan(TSI721_OBDMAC_DRDCNT(mbox));
  reg = rd32mboxchan(TSI721_OBDMAC_INT(mbox));
  DDBG("%s: Before: OBDMAC_INT = %08X\n", __FUNCTION__, reg);
  DDBG("%s: Before: rd_count = %08X, wr_count = %08X\n", __FUNCTION__, rd_count, m_omsg_ring[mbox].wr_count);
#endif

  /* Set new write count value */
  wr32mboxchan(TSI721_OBDMAC_DWRCNT(mbox), m_omsg_ring[mbox].wr_count);
  (void)rd32mboxchan(TSI721_OBDMAC_DWRCNT(mbox));

#ifdef MBOXDEBUG
  /* Now poll the RDCNT until it is equal to the WRCNT */
  /* This tells us the descriptors were processed */
  do {
    rd_count = rd32mboxchan(TSI721_OBDMAC_DRDCNT(mbox));
  } while (rd_count < m_omsg_ring[mbox].wr_count);

  DDBG("%s: After: rd_count = %08X, wr_count = %08X\n", __FUNCTION__, rd_count, m_omsg_ring[mbox].wr_count);

  /* Wait until DMA message transfer is finished */
  do {
    reg = rd32mboxchan(TSI721_OBDMAC_STS(mbox));
  } while ((reg & TSI721_OBDMAC_STS_RUN) && timeout--);

  if (timeout <= 0) {
    ERR("%s: DMA[%d] read timeout CH_STAT = %08X\n", __FUNCTION__, mbox, reg);
    ret = false; goto out;
  }
#endif

  reg = rd32mboxchan(TSI721_OBDMAC_INT(mbox));

  if (reg & TSI721_OBDMAC_INT_DONE) { /* All is good, DONE bit is set */
    DDBG("%s: After: OBDMAC_INT = %08X OK\n", __FUNCTION__, reg);
    goto out;
  }

  if (reg & TSI721_OBDMAC_INT_ERROR) {
    DBG("%s: After: OBDMAC_INT = %08X ERROR\n", __FUNCTION__, reg);
    /* If there is an error, read the status register for more info */
    volatile uint32_t reg = rd32mboxchan(TSI721_OBDMAC_STS(mbox));
    volatile uint32_t reg_out_err_stop = rd32mboxchan(RIO_PORT_N_ERR_STATUS_OUTPUT_ERR_STOP);
    volatile uint32_t reg_inp_err_stop = rd32mboxchan(RIO_PORT_N_ERR_STATUS_INPUT_ERR_STOP);

    reg_out_err_stop += 0; // silence g++ warnings
    reg_inp_err_stop += 0;
    DBG("%s: OBDMAC_STS = %08X\n", __FUNCTION__, reg);
    DBG("%s: OUTPUT_ERR_STOP = %08X\n", __FUNCTION__, reg_out_err_stop);
    DBG("%s: INPUT_ERR_STOP  = %08X\n", __FUNCTION__, reg_inp_err_stop);

    if (reg & TSI721_DMAC_STS_ABORT) {
      pthread_spin_unlock(&m_tx_splock[mbox]); // unlock here to print the blurb lock-free

      ERR("%s: ABORTed on outbound message to mbox=%d destid=%d\n", __FUNCTION__, mbox, destid);
      reg >>= 16;
      reg &= 0x1F;

      switch (reg) {
      case 0x04:
        ERR("%s: S-RIO excessive msg retry occurred, code 0x%x\n", __FUNCTION__, reg);
        break;
      case 0x05:
        ERR("%s: S-RIO response timeout occurred, code 0x%x\n", __FUNCTION__, reg);
        break;

      case 0x1F:
        ERR("%s: S-RIO message ERR response received, code 0x%x\n", __FUNCTION__, reg);
        break;

      case 8:
      case 9:
      case 11:
      case 12:
        ERR("%s: PCIE Error (not likely to happen), code 0x%x\n", __FUNCTION__, reg);
        break;
      }
    }

    return false;
  }

out:
  pthread_spin_unlock(&m_tx_splock[mbox]);

  //dump_msg_regs(m_mport, mbox);
  return ret;
}

/**
 * Returns whether a message is ready to be read on the inbound.
 *
 * @peer    Pointer to peer_info struct
 * @return  1 if true, -1 if timeout.
 */
bool MboxChannel::inb_message_ready(const int ib_mbox)
{
  const int mboxmsk = 1<<ib_mbox;
  if(! (m_mboxen & mboxmsk)) return false;


  uint32_t rd_ptr = 0, wr_ptr = 0;
  int32_t timeout = 100000;

  const int ch = ib_mbox + 4;

#ifdef MBOXDEBUG
  rd_ptr = (int32_t)rd32mboxchan(TSI721_IBDMAC_DQRP(ch));
  wr_ptr = (int32_t)rd32mboxchan(TSI721_IBDMAC_DQWR(ch));
#endif

  do {
    rd_ptr = (int32_t)rd32mboxchan(TSI721_IBDMAC_DQRP(ch));
    wr_ptr = (int32_t)rd32mboxchan(TSI721_IBDMAC_DQWR(ch));
    if (wr_ptr == 255)
      wr_ptr = -1;
  } while (wr_ptr <= rd_ptr && timeout--);

  if (timeout <= 0)
    return false;
 
  DBG("%s:EXIT: DQRP = %X, DQWR = %X\n", __FUNCTION__, rd_ptr, wr_ptr);

  return true;
}

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
void* MboxChannel::get_inb_message(const int ib_mbox, int& msg_size)
{
  const int mboxmsk = 1<<ib_mbox;
  if(! (m_mboxen & mboxmsk)) return NULL;

  void *rx_virt = NULL;
  void *buf = NULL;

  const int ch = ib_mbox + 4;

  if (!m_imsg_init[ib_mbox]) return NULL;

  pthread_spin_lock(&m_rx_splock[ib_mbox]);

  /* Point to the correct descriptor by adding the desc_rdptr
   * to the base address of the descriptors.
   */
  hw_imsg_desc* desc = (hw_imsg_desc*)m_imsg_ring[ib_mbox].imd.win_ptr + m_imsg_ring[ib_mbox].desc_rdptr;

  uint32_t rx_slot = m_imsg_ring[ib_mbox].rx_slot; // This is an index into user-provided buffer[], NOT HW

#ifdef DEBUG
  DBG("%s: INBOUND DESCRIPTOR CONTENTS:\n", __FUNCTION__);
  dump_ib_desc(desc);
#endif

  if (!(desc->msg_info & TSI721_IMD_HO)) {
    pthread_spin_unlock(&m_rx_splock[ib_mbox]);
    ERR("%s:TSI721_IMD_HO not set.\n", __FUNCTION__);
    ERR("%s:mbox = %d, desc address = %p, rx_slot = %d\n", __FUNCTION__, ib_mbox, desc, rx_slot);
    return NULL;
  }

  DBG("%s:mbox = %d, desc address = %p, initially rx_slot = %d\n", __FUNCTION__, ib_mbox, desc, rx_slot);
  while (m_imsg_ring[ib_mbox].imq_base[rx_slot] == NULL) { // Hunt for a buffer
    if (++rx_slot == m_imsg_ring[ib_mbox].size)
      rx_slot = 0;
  }

  DBG("%s: Now rx_slot = %d, imq_base[rx_slot] = %p\n", __FUNCTION__, rx_slot, m_imsg_ring[ib_mbox].imq_base[rx_slot]);

  /* Physical address of this message from the descriptor populated by HW */
  const uint64_t rx_phys = ((uint64_t) desc->bufptr_hi << 32) | desc->bufptr_lo;

  /* Get the correct virtual address of the buffer containing the message
   * by adding the size (computed as the difference between the message's
   * physical address and the physical address of the entire buffer) to
   * the base virtual address of the entire buffer.
   */
  const uint32_t offset = rx_phys - (uint64_t)m_imsg_ring[ib_mbox].buf.win_handle;
  rx_virt = (uint8_t *)m_imsg_ring[ib_mbox].buf.win_ptr + offset;

  /* imq_base[rx_slot] was populated in add_inb_buffer() */
  buf = m_imsg_ring[ib_mbox].imq_base[rx_slot];
  msg_size = desc->msg_info & TSI721_IMD_BCOUNT;
  if (msg_size == 0)
    msg_size = RIO_MAX_MSG_SIZE;

  DBG("%s: Copying buffer %p, size = %d from slot %d\n", __FUNCTION__, buf, msg_size, rx_slot);
  /* Copy the message contents to 'buf' */
  memcpy(buf, rx_virt, msg_size);

  /* Now set this buffer pointer back to NULL */
  m_imsg_ring[ib_mbox].imq_base[rx_slot] = NULL;

  desc->msg_info &= ~TSI721_IMD_HO;

  /* Increment the rdptr, wrapping around if necessary */
  if (++m_imsg_ring[ib_mbox].desc_rdptr == m_imsg_ring[ib_mbox].size)
    m_imsg_ring[ib_mbox].desc_rdptr = 0;

  /* Update the Descriptor Queue Read Pointer */
  wr32mboxchan(TSI721_IBDMAC_DQRP(ch), m_imsg_ring[ib_mbox].desc_rdptr);
  DBG("%s: Now DQRP = %d\n", __FUNCTION__, m_imsg_ring[ib_mbox].desc_rdptr);

  /* Return free buffer into the pointer list */
  uint64_t* free_ptr = (uint64_t *) m_imsg_ring[ib_mbox].imfq.win_ptr;
  free_ptr[m_imsg_ring[ib_mbox].fq_wrptr] = rx_phys;

  /* Increment free queue pointer, wrapping around if necessary */
  if (++m_imsg_ring[ib_mbox].fq_wrptr == m_imsg_ring[ib_mbox].size)
    m_imsg_ring[ib_mbox].fq_wrptr = 0;

  /* Update the free queue write pointer in the FQWP register */
  wr32mboxchan(TSI721_IBDMAC_FQWP(ch), m_imsg_ring[ib_mbox].fq_wrptr);

  pthread_spin_unlock(&m_rx_splock[ib_mbox]);

  return buf;
}

/**
 * Add buffer to the Tsi721 inbound message queue
 *
 * @peer    Pointer to peer_info struct
 * @mbox    Inbound mailbox number
 * @buf     Buffer to add to inbound queue
 *
 * @return      tx_slot >= 0 if successful  < 0 if unsuccessful
 */
int MboxChannel::add_inb_buffer(const int mbox, void* buf)
{
  const int mboxmsk = 1<<mbox;
  if(! (m_mboxen & mboxmsk)) return -1;
  
  pthread_spin_lock(&m_rx_splock[mbox]);

  uint32_t rx_slot = m_imsg_ring[mbox].rx_slot;

  /* There is something wrong if we are trying to add a buffer to
   * a slot that is not NULL since:
   * 1. we initialize them all to NULL; and
   * 2. we set them back to NULL after reading from them.
   */
  if (m_imsg_ring[mbox].imq_base[rx_slot]) {
    pthread_spin_unlock(&m_rx_splock[mbox]);
    ERR("%s: Error adding inbound buffer %d, buffer exists\n", __FUNCTION__, rx_slot);
    return -EINVAL;
  }

  /* This is where the data will be when we read it */
  m_imsg_ring[mbox].imq_base[rx_slot] = buf;

  DDBG("%s: Buffer %p added in slot %d\n", __FUNCTION__, buf, rx_slot);

  /* Increment rx_slot, wrapping around if necessary */
  if (++m_imsg_ring[mbox].rx_slot == m_imsg_ring[mbox].size) {
    DBG("%s: rx_slot = %d, resetting to 0 (size=%d)\n", __FUNCTION__, rx_slot,  m_imsg_ring[mbox].size);
    m_imsg_ring[mbox].rx_slot = 0;
  }

  pthread_spin_unlock(&m_rx_splock[mbox]);

  return m_imsg_ring[mbox].rx_slot;
}

int MboxChannel::scanFIFO(const int mbox)
{
  const int mboxmsk = 1<<mbox;
  if(! (m_mboxen & mboxmsk)) return -1;

#if (defined(DEBUG) && DEBUG > 1) || defined(FIFODEBUG)
  const void*    fifo_ptr   = m_omsg_ring[mbox].sts.win_ptr;
  const uint32_t fifo_size = m_omsg_ring[mbox].sts.win_size;

  if(hexdump64bit(fifo_ptr, fifo_size))
    ; //printf("FIFO hw RP=%u WP=%u\n", getFIFOReadCount(mbox), getFIFOWriteCount(mbox));
#endif

  int fifo_count = 0;

  /* Check and clear descriptor status FIFO entries */
  const uint32_t old_rdptr = m_omsg_ring[mbox].sts_rdptr;

  uint64_t* sts_ptr = (uint64_t*)m_omsg_ring[mbox].sts.win_ptr;
  assert(sts_ptr);

  int j = m_omsg_ring[mbox].sts_rdptr * 8;
DDBG("\n\tFIFO START line=%d size=%d RD=%d\n", j, m_omsg_ring[mbox].sts_size, old_rdptr);

  const uint64_t HW_START = m_omsg_ring[mbox].omd.win_handle;
  const uint64_t HW_END   = HW_START + m_omsg_ring[mbox].omd.win_size;
  while (sts_ptr[j]) {
      for (int i = j; i < (j+8) && sts_ptr[i]; i++) {
        const uint64_t hwptr = sts_ptr[i];
        sts_ptr[i] = 0;
        if(hwptr >= HW_START && hwptr < HW_END) {
          bool found = 0;
          uint32_t l_wr_count = 0;
          pthread_spin_lock(&m_bltx_splock[mbox]);
            std::map<uint64_t, uint32_t>::iterator it = m_omsg_trk[mbox].bltx_busy.find(hwptr);
            if (it != m_omsg_trk[mbox].bltx_busy.end()) {
              l_wr_count = it->second; found = true;
              m_omsg_trk[mbox].bltx_busy.erase(it);
            }
            const uint32_t bd_idx = (hwptr-HW_START)/MBOX_BUFF_DESCR_SIZE;
            if (found) {
              assert(m_omsg_trk[mbox].bl_busy[bd_idx] == 1);
              m_omsg_trk[mbox].bl_busy.erase(bd_idx);
            }
          pthread_spin_unlock(&m_bltx_splock[mbox]);

          uint32_t soft_rp = m_omsg_ring[mbox].rd_count_soft;

          if (found /*&& bd_idx != m_num_ob_desc[mbox]*/) { // ignore DTYPE5
            pthread_spin_lock(&m_tx_splock[mbox]);
            m_omsg_ring[mbox].rd_count_soft = l_wr_count;
            soft_rp = l_wr_count;
            pthread_spin_unlock(&m_tx_splock[mbox]);
          }

          DBG("\n\tFIFO line=%d off=%d 0x%llx => BD idx %d %sfound -- TX HW RP=%d soft RP=%d WP=%d -- pending %d\n",
                   j, i, hwptr, bd_idx, (found? "": "NOT "),
                   rd32mboxchan(TSI721_OBDMAC_DRDCNT(mbox)), soft_rp, m_omsg_ring[mbox].wr_count,
                   m_omsg_trk[mbox].bltx_busy.size());
          ++fifo_count;
        } else {
          CRIT("\n\tFIFO line=%d off=%d 0x%llx JUNK\n", j, i, hwptr);
        }
      } // END for line-of-8

      ++m_omsg_ring[mbox].sts_rdptr;
      m_omsg_ring[mbox].sts_rdptr %= m_omsg_ring[mbox].sts_size; // Barry hack
      j = m_omsg_ring[mbox].sts_rdptr * 8;
  } // END for scan full FIFO every line-of-8

  if(old_rdptr != m_omsg_ring[mbox].sts_rdptr)
    wr32mboxchan(TSI721_OBDMAC_DSRP(mbox), m_omsg_ring[mbox].sts_rdptr);

#ifdef FIFODEBUG
  if(fifo_count > 0 && queueTxSize(mbox) > (m_num_ob_desc[mbox]/2))
    dumpBL(mbox);
#endif

  return fifo_count;
}

void MboxChannel::dumpBL(int mbox)
{
  std::stringstream ss;
  ss << "BL[] = { ";
  int count = 0;
  const uint64_t HW_START = m_omsg_ring[mbox].omd.win_handle;
  pthread_spin_lock(&m_bltx_splock[mbox]);
  for(std::map<uint64_t, uint32_t>::iterator it = m_omsg_trk[mbox].bltx_busy.begin();
      it != m_omsg_trk[mbox].bltx_busy.end(); it++) {
    const uint64_t hw_ptr2 = it->first;
    const uint32_t bd_idx = (hw_ptr2-HW_START)/MBOX_BUFF_DESCR_SIZE;
    ss << bd_idx << ":" << it->second << " "; 
    count++;
  }
  pthread_spin_unlock(&m_bltx_splock[mbox]);
  ss << "} size=" << count << "\n";
  DBG("%s", ss.str().c_str());
}

#ifdef TEST_MBOX

#define RIO_DESTID	7//6

int MBOX = 2;

volatile int canStart = 0;
volatile int mustStop = 0;

volatile uint64_t fifo_thr_iter = 0;

//static inline pid_t gettid() { return syscall(__NR_gettid); }

void* fifo_thr(void* arg)
{
  if(arg == NULL) return NULL;

  cpu_set_t set; CPU_ZERO(&set);
  CPU_SET(3, &set);
  sched_setaffinity(0, sizeof( cpu_set_t ), &set);

  MboxChannel* mch = (MboxChannel*)arg;

  while(! canStart) usleep(1);

  while(! mustStop) {
    fifo_thr_iter++;

    if(mch->scanFIFO(MBOX) > 0) goto next_sleep;

    if(mustStop) break;
    usleep(1);
    continue;

 next_sleep:
    if(mustStop) break;
    sched_yield();
  }

  return NULL;
}

int main(int argc, char* argv[])
{
  if(argc > 1) { int tmp = -1; if(sscanf(argv[1], "%d", &tmp) == 1 && tmp >= 0) { MBOX = tmp; } }

  printf("$RIO_DESTID=%s\n", getenv("RIO_DESTID"));

  MboxChannel* mch = new MboxChannel(0, 1<<MBOX /*| 1<<3*/);

  const int BUFCOUNT = 256;

  mch->setInitState();
  mch->set_rx_destid(mch->getDeviceId());
  if(!mch->open_mbox(BUFCOUNT))
    throw std::runtime_error("Failed to open mbox!");

  DBG("Compiled at %s %s\n", __DATE__, __TIME__);

  if(argc > 2 && tolower(argv[2][0]) == 's') { // RX
    for(int i = 0; i < BUFCOUNT; i++)
      mch->add_inb_buffer(MBOX, calloc(1, PAGE_4K));

again:
    printf("Waiting for msg!\n");
    do { static int c; write(STDOUT_FILENO, &"..........\r"[c++%11], 1); } while(! mch->inb_message_ready(MBOX));

    void* buf = NULL;
    int msg_size = 0;
    while((buf = mch->get_inb_message(MBOX, msg_size)) != NULL) {
      printf("\nGot a message of size %d [%s]\n\n", msg_size, buf);
      mch->add_inb_buffer(MBOX, buf); // recycle
    }
    goto again;
  } else {
    uint16_t destid = RIO_DESTID;
    if(getenv("RIO_DESTID") != NULL) {
      int tmp = -1;
      if (sscanf(getenv("RIO_DESTID"), "%d", &tmp) == 1 && tmp >= 0) { destid = tmp; }
    }
    printf("RIO_DESTID = %u\n", destid);

    pthread_t fifo;
    pthread_create(&fifo, NULL, fifo_thr, mch);
    canStart = 1; sched_yield();

    char str[129] = {0};
    for (int i = 0; i < 6667; i++) {
      snprintf(str, 128, "Mary had a little lamb iter %d\x0", i);
      if(! mch->send_message(destid, MBOX, str, 1+strlen(str))) {
        ERR("send_message FAILED!\n");
        break;
      }
      while (mch->queueTxFull(MBOX)) { usleep(100); } 
    }
    mustStop = 1;
    pthread_join(fifo, NULL);
    write(STDOUT_FILENO, "ENTER:", 6); getchar();
  }

  delete mch;
  return 0;
}
#endif // TEST_MBOX
