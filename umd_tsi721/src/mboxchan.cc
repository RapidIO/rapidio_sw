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

#ifndef RIO_PORT_N_XERR_STATUS_OUTPUT_XERR_STOP
  #define  RIO_PORT_N_XERR_STATUS_OUTPUT_XERR_STOP        0x00010000
#endif
#ifndef RIO_PORT_N_XERR_STATUS_INPUT_XERR_STOP
  #define  RIO_PORT_N_XERR_STATUS_INPUT_XERR_STOP        0x00000100
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
  char tmp[129] = {0};
  std::stringstream ss;
  ss << "OUTBOUND DESCRIPTOR CONTENTS:\n";
  snprintf(tmp, 128, "  DEVID=0x%04X, ", desc->type_id & TSI721_OMD_DEVID); ss << tmp;
  snprintf(tmp, 128, "  CRF=%d, ", (desc->type_id & TSI721_OMD_CRF) >> 16); ss << tmp;
  snprintf(tmp, 128, "  PRIO=%d, ", (desc->type_id & TSI721_OMD_PRIO) >> 17); ss << tmp;
  snprintf(tmp, 128, "  IOF=%d, ", (desc->type_id & TSI721_OMD_IOF) >> 27); ss << tmp;
  snprintf(tmp, 128, "  DTYPE=%d\n", (desc->type_id & TSI721_OMD_DTYPE) >> 29); ss << tmp;
  snprintf(tmp, 128, "  BCOUNT=%d, ", desc->msg_info & TSI721_OMD_BCOUNT); ss << tmp;
  snprintf(tmp, 128, "  SSIZE=%d, ", (desc->msg_info & TSI721_OMD_SSIZE) >> 12); ss << tmp;
  snprintf(tmp, 128, "  LETTER=%d, ", (desc->msg_info & TSI721_OMD_LETTER) >> 16); ss << tmp;
  snprintf(tmp, 128, "  XMBOX=%d, ", (desc->msg_info & TSI721_OMD_XMBOX) >> 18); ss << tmp;
  snprintf(tmp, 128, "  MBOX=%d, ", (desc->msg_info & TSI721_OMD_MBOX) >> 22); ss << tmp;
  snprintf(tmp, 128, "  TT=%d\n", (desc->msg_info & TSI721_OMD_TT) >> 26); ss << tmp;
  snprintf(tmp, 128, "  MSG_XINFO = 0x%08x\n", desc->msg_info); ss << tmp;
  snprintf(tmp, 128, "  BUFFER_PTR=0x%08X%08X\n", desc->bufptr_hi, desc->bufptr_lo); ss << tmp;
  XDBG("\n%s", ss.str().c_str());
}

/**
 * Given an outbound descriptor, dump it to screen.
 *
 * @desc    pointer to descriptor to dump
 */
static inline void
dump_ib_desc(hw_imsg_desc* desc)
{
  char tmp[129] = {0};
  std::stringstream ss;
  ss << "INBOUND DESCRIPTOR CONTENTS:\n";
  snprintf(tmp, 128, "  DEVID=0x%04X, ", desc->type_id & TSI721_IMD_DEVID); ss << tmp;
  snprintf(tmp, 128, "  CRF=%d, ", (desc->type_id & TSI721_IMD_CRF) >> 16); ss << tmp;
  snprintf(tmp, 128, "  PRIO=%d, ", (desc->type_id & TSI721_IMD_PRIO) >> 17); ss << tmp;
  snprintf(tmp, 128, "  DTYPE=%d\n", (desc->type_id & TSI721_IMD_DTYPE) >> 29); ss << tmp;
  snprintf(tmp, 128, "  BCOUNT=%d, ", desc->msg_info & TSI721_IMD_BCOUNT); ss << tmp;
  snprintf(tmp, 128, "  SSIZE=%d, ", (desc->msg_info & TSI721_IMD_SSIZE) >> 12); ss << tmp;
  snprintf(tmp, 128, "  LETTER=%d, ", (desc->msg_info & TSI721_IMD_LETER) >> 16); ss << tmp;
  snprintf(tmp, 128, "  XMBOX=%d, ", (desc->msg_info & TSI721_IMD_XMBOX) >> 18); ss << tmp;
  snprintf(tmp, 128, "  MBOX=%d, ", (desc->msg_info & TSI721_IMD_MBOX) >> 22); ss << tmp;
  snprintf(tmp, 128, "  CS=%d, ", (desc->msg_info & TSI721_IMD_CS) >> 27); ss << tmp;
  snprintf(tmp, 128, "  HO=%d\n", (desc->msg_info & TSI721_IMD_HO) >> 31); ss << tmp;
  snprintf(tmp, 128, "  BUFFER_PTR=0x%08X%08X\n", desc->bufptr_hi, desc->bufptr_lo); ss << tmp;
  XDBG("\n%s", ss.str().c_str());
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

  char tmp[129] = {0};
  std::stringstream ss;
  ss << "OUTBOUND MESSAGING REGISTERS:\n";
  snprintf(tmp, 128, "  DWRCNT = %X\t", mport->rd32(TSI721_OBDMACXDWRCNT(ch))); ss << tmp;
  snprintf(tmp, 128, "  DRDCNT = %X\n", mport->rd32(TSI721_OBDMACXDRDCNT(ch))); ss << tmp;
  snprintf(tmp, 128, "  CTL = %X\t", mport->rd32(TSI721_OBDMACXCTL(ch))); ss << tmp;
  snprintf(tmp, 128, "  INT = %X\n", mport->rd32(TSI721_OBDMACXINT(ch))); ss << tmp;
  snprintf(tmp, 128, "  STS = %X\n", mport->rd32(TSI721_OBDMACXSTS(ch))); ss << tmp;
  snprintf(tmp, 128, "  DPTRL = %X ", mport->rd32(TSI721_OBDMACXDPTRL(ch))); ss << tmp;
  snprintf(tmp, 128, "  DPTRH = %X\n", mport->rd32(TSI721_OBDMACXDPTRH(ch))); ss << tmp;
  snprintf(tmp, 128, "  DSBL = %X ", mport->rd32(TSI721_OBDMACXDSBL(ch))); ss << tmp;
  snprintf(tmp, 128, "  DSBH = %X\n", mport->rd32(TSI721_OBDMACXDSBH(ch))); ss << tmp;
  snprintf(tmp, 128, "  DSSZ = %X\n", mport->rd32(TSI721_OBDMACXDSSZ(ch))); ss << tmp;
  snprintf(tmp, 128, "  DSRP = %X\t", mport->rd32(TSI721_OBDMACXDSRP(ch))); ss << tmp;
  snprintf(tmp, 128, "  DSWP = %X\n", mport->rd32(TSI721_OBDMACXDSWP(ch))); ss << tmp;
  XDBG("\n%s", ss.str().c_str());
}

MboxChannel::MboxChannel(const uint32_t mportid, const uint32_t mbox) :
  m_mbox(mbox), m_ib_mbox(mbox)
{
  if(mbox > 3)
    throw std::runtime_error("MboxChannel: Invalid mbox!");

  m_mport = new RioMport(mportid);
  init();
}

MboxChannel::MboxChannel(const uint32_t mportid, const uint32_t mbox, riomp_mport_t mp_hd) :
  m_mbox(mbox), m_ib_mbox(mbox)
{
  if(mbox > 3)
    throw std::runtime_error("MboxChannel: Invalid mbox!");

  m_mport = new RioMport(mportid, mp_hd);
  init();
}

void MboxChannel::init()
{
  pthread_spin_init(&m_hw_splock, PTHREAD_PROCESS_PRIVATE);

  pthread_spin_init(&m_rx_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_tx_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_bltx_splock, PTHREAD_PROCESS_PRIVATE);

  memset(&m_num_ob_desc, 0, sizeof(m_num_ob_desc));
  memset(&m_imsg_init,   0, sizeof(m_imsg_init));
  memset(&m_imsg_ring,   0, sizeof(m_imsg_ring));
  memset(&m_omsg_init,   0, sizeof(m_omsg_init));
  memset(&m_omsg_ring,   0, sizeof(m_omsg_ring));

  m_restart_pending = 0;
}

/* FIXME: There is a defect in setInitState which is corrected by
 * softRestart.  This is why code calls softRestart after setInitState.
 */
void MboxChannel::setInitState()
{
  assert(this);

  m_mport->wr32(TSI721_SMSG_ECC_LOG, 0);
  m_mport->wr32(TSI721_RETRY_GEN_CNT, 0);
  m_mport->wr32(TSI721_RETRY_RX_CNT, 0);
    
  /* Set SRIO Message Request/Response Timeout */
  //m_mport->wr32(TSI721_RQRPTO, TSI721_RQRPTO_VAL);
  m_mport->wr32(TSI721_RQRPTO, 0x140); // ~10 uS

  const int ch = m_mbox + 4;

  /* Clear interrupt bits */
  m_mport->wr32(TSI721_IBDMACXINT(ch), TSI721_IBDMACXINT_MASK);

  /* Clear Status */
  m_mport->wr32(TSI721_IBDMACXSTS(ch),0);
  m_mport->wr32(TSI721_SMSG_ECC_CORRXLOG(ch),
               TSI721_SMSG_ECC_CORRXLOG_ECC_CORR_MEM);
  m_mport->wr32(TSI721_SMSG_ECC_UNCORRXLOG(ch),
                TSI721_SMSG_ECC_UNCORRXLOG_ECC_UNCORR_MEM);
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
 * @param[in] word: the word to search
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

int MboxChannel::open_inb_mbox(const uint32_t entries)
{
  uint32_t i;
  int rc = 0;
  uint64_t* free_ptr = NULL;

  if(m_imsg_init) return -EAGAIN;

  /* Initialize IB Messaging Ring */
  m_imsg_ring.size = entries;
  m_imsg_ring.rx_slot = 0;
  m_imsg_ring.desc_rdptr = 0;
  m_imsg_ring.fq_wrptr = 0;

  m_imsg_ring.imq_base.reserve(m_imsg_ring.size);
  m_imsg_ring.imq_ts.reserve(m_imsg_ring.size);

  imq_ts_t tmp = {false, 0};
  for (i = 0; i < m_imsg_ring.size; i++) {
    m_imsg_ring.imq_base[i] = NULL;
    m_imsg_ring.imq_ts[i] = tmp; // mark as invalid TS at startup
  }

  /* Allocate buffers for incoming messages */
  if(! m_mport->map_dma_buf(entries * TSI721_MSG_BUFFER_SIZE, m_imsg_ring.buf)) {
    XERR("%s: Failed to allocate buffers for IB MBOX%d\n", __FUNCTION__, m_mbox);
    return -ENOMEM;
  }
  memset(m_imsg_ring.buf.win_ptr, 0, m_imsg_ring.buf.win_size);
  {
    RioMport::DmaMem_t& mem = m_imsg_ring.buf;
    XDBG("\n\t%s: Allocated buffers for incoming messages #buf"
	"\n\t- IB MBOX%d size=%d phys=0x%lx virt=%p\n", __FUNCTION__,
            m_mbox, mem.win_size, mem.win_handle, mem.win_ptr);
  }

  /* Allocate memory for circular free list -- rounded up to 4K by RioMport */
  if(! m_mport->map_dma_buf(entries * sizeof(uint64_t), m_imsg_ring.imfq)) {
    XERR("%s: Failed to allocate free queue for IB MBOX%d\n", __FUNCTION__, m_mbox);
    rc = -ENOMEM;
    goto out_buf;
  }
  memset(m_imsg_ring.imfq.win_ptr, 0, m_imsg_ring.imfq.win_size);
  {
    RioMport::DmaMem_t& mem = m_imsg_ring.imfq;
    XDBG("\n\t%s: Allocated memory for circular free list #imfq"
	"\n\t- IB MBOX%d size=%d phys=0x%lx virt=%p\n", __FUNCTION__,
            m_mbox, mem.win_size, mem.win_handle, mem.win_ptr);
  }

  /* Allocate memory for Inbound message descriptors -- rounded up to 4K by RioMport */
  if(! m_mport->map_dma_buf(entries * MBOX_IB_BUFF_DESCR_SIZE, m_imsg_ring.imd)) {
    XERR("%s: Failed to allocate descriptor memory for IB MBOX%d\n", __FUNCTION__, m_mbox);
    fflush(stderr);
    rc = -ENOMEM;
    goto out_dma;
  }
  memset(m_imsg_ring.imd.win_ptr, 0, m_imsg_ring.imd.win_size);
  {
    RioMport::DmaMem_t& mem = m_imsg_ring.imd;
    XDBG("\n\t%s: Allocated memory for inbound message descriptors #imd"
	"\n\t- IB MBOX%d size=%d phys=0x%lx virt=%p\n", __FUNCTION__,
            m_mbox, mem.win_size, mem.win_handle, mem.win_ptr);
  }

  /* Fill free buffer pointer list */
  free_ptr = (uint64_t *) m_imsg_ring.imfq.win_ptr;
  for (i = 0; i < entries; i++) {
    free_ptr[i] = (uint64_t) (m_imsg_ring.buf.win_handle) + i * TSI721_MSG_BUFFER_SIZE;
  }

  set_inb_mbox_hwregs(entries - 1);

  m_imsg_init = true;
  return 0;

out_dma:
  m_mport->unmap_dma_buf(m_imsg_ring.imfq);
  memset(&m_imsg_ring.imfq, 0, sizeof(m_imsg_ring.imfq));

out_buf:
  m_mport->unmap_dma_buf(m_imsg_ring.buf);
  memset(&m_imsg_ring.buf, 0, sizeof(m_imsg_ring.buf));

  return rc;
}

void MboxChannel::set_inb_mbox_hwregs(const uint32_t fq_wrptr)
{
  /*
   * Configure Inbound Messaging channel (ch = mbox + 4)
   */
  const int ch = m_mbox + 4;    /* For inbound, mbox0 = ch4, mbox1 = ch5, and so on. */

  /*
   * For mapping of inbound SRIO Messages into appropriate queues we need
   * to set Inbound Device ID register in the messaging engine. 
   */

  /* Setup Inbound Message free queue */
  m_mport->wr32(TSI721_IBDMACXFQBH(ch), (uint64_t) m_imsg_ring.imfq.win_handle >> 32);
  m_mport->wr32(TSI721_IBDMACXFQBL(ch), (uint64_t) m_imsg_ring.imfq.win_handle & TSI721_IBDMACXFQBL_ADD);

  m_mport->wr32(TSI721_IBDMACXFQSZ(ch), TSI721_DMAC_DSSZ_SIZE(m_imsg_ring.size));

  /* Setup Inbound Message descriptor queue */
  m_mport->wr32(TSI721_IBDMACXDQBH(ch), (uint64_t) m_imsg_ring.imd.win_handle >> 32);
  m_mport->wr32(TSI721_IBDMACXDQBL(ch), (uint32_t) m_imsg_ring.imd.win_handle & (uint32_t) TSI721_IBDMACXDQBL_ADD);

  m_mport->wr32(TSI721_IBDMACXDQSZ(ch), TSI721_DMAC_DSSZ_SIZE(m_imsg_ring.size));

  /* Initialize Inbound Message Engine */
  m_mport->wr32(TSI721_IBDMACXCTL(ch), TSI721_IBDMACXCTL_INIT);

  (void)m_mport->rd32(TSI721_IBDMACXCTL(ch));
  usleep(10);

  m_imsg_ring.fq_wrptr = fq_wrptr;
  m_mport->wr32(TSI721_IBDMACXFQWP(ch), fq_wrptr);
}

#define CHECK_END_BD() \
  {{ \
    const hw_omsg_desc* bd_ptr = (hw_omsg_desc*)m_omsg_ring.omd.win_ptr; \
    assert(bd_ptr[m_num_ob_desc-1].type_id == (DTYPE5 << 29)); \
  }}

int MboxChannel::open_outb_mbox(const uint32_t entries, const uint32_t sts_entries)
{
  int rc = 0;
  uint32_t num_desc = 0;

  if(m_omsg_init) return -EAGAIN;

  uint32_t reg_size = 0; // what we put in FIFO size register
  uint32_t mem_size = 0; // actual size of FIFO memory

  XDBG("\n\t%s: mbox = %d, entries = %d\n", __FUNCTION__, m_mbox, entries);

  m_omsg_trk.bl_busy = (int*)calloc(entries+1, sizeof(int));

  m_omsg_trk.bltx_busy = (WorkItem_t*)calloc(entries+1, sizeof(WorkItem_t)); // +1 to have a guard, NOT used
  m_omsg_trk.bltx_busy_size = 0;

  m_omsg_ring.size = entries;
  m_omsg_ring.sts_rdptr = 0;

  /* Outbound Msg Buffer allocation */
  for (int i = 0; i < entries; i++) {
    RioMport::DmaMem_t tmp; memset(&tmp, 0, sizeof(RioMport::DmaMem_t));

    /* 4K for each entry. For the demo it is 1 entry */
    if(! m_mport->map_dma_buf(PAGE_4K, tmp)) {
      XERR("Unable to allocate OB buffer for MBOX%d\n", m_mbox);
      rc = -ENOMEM;
      goto out_buf;
    }
    m_omsg_ring.omq.push_back(tmp);
    memset(m_omsg_ring.omq[i].win_ptr, 0, m_omsg_ring.omq[i].win_size);
  }

  {{ // F*** g++
    const uint32_t obdesc_size = (entries + 1) * MBOX_OB_BUFF_DESCR_SIZE;
    uint32_t obdesc_pages = obdesc_size / PAGE_4K;
    if(obdesc_size % PAGE_4K) obdesc_pages++;

    /* Outbound message descriptor allocation */
    XDBG("\n\t%s: Allocating descriptor for MBOX%d as %d 4K pages\n", __FUNCTION__, m_mbox, obdesc_pages);
    if(! m_mport->map_dma_buf(obdesc_pages * PAGE_4K, m_omsg_ring.omd)) {
      XERR("Unable to allocate OB descriptors for MBOX%d\n", m_mbox);
      rc = -ENOMEM;
      goto out_buf;
    }
    memset(m_omsg_ring.omd.win_ptr, 0, m_omsg_ring.omd.win_size);
  }}

  /* Number of descriptors */
  m_num_ob_desc = entries;
  num_desc      = entries;
  XDBG("\n\t%s: There are %u outbound descriptors\n", __FUNCTION__, num_desc);

  m_omsg_ring.sts_size = sts_entries;

  SizeTsi721Fifo(m_omsg_ring.sts_size, reg_size, mem_size);

  XDBG("\n\t%s: Allocating status FIFO for MBOX%d - sts_size=%d\n", __FUNCTION__, m_mbox, entries);
  if(! m_mport->map_dma_buf(mem_size, m_omsg_ring.sts)) {
    XERR("Unable to allocate OB MSG status FIFO for MBOX%d\n", m_mbox);
    rc = -ENOMEM;
    goto out_desc;
  }
  memset(m_omsg_ring.sts.win_ptr, 0, m_omsg_ring.sts.win_size);

  set_outb_mbox_hwregs(0);

  m_omsg_init = true;
  CHECK_END_BD();
  return 0;

out_desc:
  /* Free allocated descriptors */
  m_mport->unmap_dma_buf(m_omsg_ring.omd);
  memset(&m_omsg_ring.omd, 0, sizeof(m_omsg_ring.omd));

out_buf:
  /* Free allocated message buffers */
  for (int i = 0; i < m_omsg_ring.size; i++) {
    m_mport->unmap_dma_buf(m_omsg_ring.omq[i]);
    memset(&m_omsg_ring.omq[i], sizeof(m_omsg_ring.omq[i]), 0);
  }

  return rc;
}

void MboxChannel::set_outb_mbox_hwregs(const uint32_t wr_count)
{
  uint32_t reg_size = 0; // what we put in FIFO size register
  uint32_t mem_size = 0; // actual size of FIFO memory

  SizeTsi721Fifo(m_omsg_ring.sts_size, reg_size, mem_size);

  /**
   * Configure Outbound Messaging Engine
   */

  /* Setup Outbound Message descriptor pointer */
  m_mport->wr32(TSI721_OBDMACXDPTRH(m_mbox), m_omsg_ring.omd.win_handle >> 32);
  m_mport->wr32(TSI721_OBDMACXDPTRL(m_mbox), m_omsg_ring.omd.win_handle & TSI721_OBDMACXDPTRL_DPTRL);

  /* Setup Outbound Message descriptor status FIFO */
  m_mport->wr32(TSI721_OBDMACXDSBH(m_mbox), m_omsg_ring.sts.win_handle >> 32);
  m_mport->wr32(TSI721_OBDMACXDSBL(m_mbox), m_omsg_ring.sts.win_handle & TSI721_OBDMACXDSBL_ADD);
  m_mport->wr32((uint32_t)TSI721_OBDMACXDSSZ(m_mbox), reg_size);

  /* Initialize Outbound Message descriptors ring */
  hw_omsg_desc* bd_ptr = (hw_omsg_desc*)m_omsg_ring.omd.win_ptr;
  {{
    hw_omsg_desc& end_bd = bd_ptr[m_num_ob_desc-1];
    end_bd.type_id = DTYPE5 << 29;
    end_bd.msg_info = 0;
    end_bd.next_lo = (uint64_t) m_omsg_ring.omd.win_handle & TSI721_OBDMACXDPTRL_DPTRL;
    end_bd.next_hi = (uint64_t) m_omsg_ring.omd.win_handle >> 32;
  }}

#ifdef MBOXDEBUG
  XDBG("\n\t%s: Last descriptor index %d:\n", __FUNCTION__, m_num_ob_desc);
  dump_ob_desc(&bd_ptr[m_num_ob_desc]);
#endif

  m_omsg_ring.wr_count      = wr_count;
  m_omsg_ring.rd_count_soft = wr_count;

#ifdef MBOXDEBUG
  dump_msg_regs(m_mport, m_mbox);
#endif
}

void MboxChannel::cleanup()
{
  if(m_imsg_init) {
    m_mport->unmap_dma_buf(m_imsg_ring.imfq);
    m_mport->unmap_dma_buf(m_imsg_ring.buf);
  }

  if(m_omsg_init) {
    m_mport->unmap_dma_buf(m_omsg_ring.omd);
    for (int i = 0; i < m_omsg_ring.size; i++)
      m_mport->unmap_dma_buf(m_omsg_ring.omq[i]);
  }
  for(int i = 0; i < m_imsg_ring.imq_base.size(); i++) {
    if(m_imsg_ring.imq_base[i] == NULL) continue;
    free(m_imsg_ring.imq_base[i]);
  }
  m_imsg_ring.imq_base.clear();

  m_imsg_ring.imq_ts.clear();

  if (m_omsg_trk.bltx_busy != NULL) {
    assert(m_omsg_trk.bltx_busy[m_num_ob_desc].valid == 0);

    free(m_omsg_trk.bltx_busy);
    m_omsg_trk.bltx_busy = NULL;
    m_omsg_trk.bltx_busy_size = 0;
  }

  free(m_omsg_trk.bl_busy);
  m_omsg_trk.bl_busy = NULL;
}

bool MboxChannel::open_mbox(const uint32_t entries, const uint32_t sts_entries)
{
  bool fail = false;

  do {
    if(open_outb_mbox(entries, sts_entries) < 0) { fail = true; break; }
    if(open_inb_mbox(entries) < 0) { fail = true; break; }
  } while (0);

  if(fail) {
    cleanup();
    return false;
  }

  return true;
}


/** \brief Send message to specified destination.
 * \note If check_reg is NOT set the the only way to find out if TX was successful is to do an audit of what came out in the TX FIFO.
 * \param[in, out] opt
 * \param[in] data Data to be sent, mas 4K
 * \param[in] len Size of data
 * \param[in] check_reg Check status reg after TX? (SLOW)
 * \param[out] fail_reason Check status reg after TX? (SLOW)
 *
 * @return true if TX operation was enqueued OK, false otherwise
 * \retval q_was_full Set if the TX queue was full
 */
bool MboxChannel::send_message(MboxOptions_t& opt, const void* data, const size_t len, const bool check_reg, StopTx_t& fail_reason)
{
  uint32_t regi = 0, regs = 0;
  bool sts_abort = false;
#ifdef MBOXDEBUG
  int timeout = 0;
  uint32_t reg = 0, rd_count = 0;
#endif

  const uint16_t mbox = opt.mbox;

  if (mbox >= RIO_MAX_MBOX)
    throw std::runtime_error("send_message: Invalid mbox!");

  bool ret = true;
  fail_reason = STOP_OK;

  if (data == NULL || len < 0 || len > 4096) return false;
 
  opt.dtype = DTYPE4;

  bool queued_T5 = false;

  MboxOptions_t opt_end; memset(&opt_end, 0, sizeof(opt_end));
  opt_end.dtype = DTYPE5;
  opt_end.mbox  = m_mbox;

  pthread_spin_lock(&m_tx_splock);
  if (queueTxFull()) {
    pthread_spin_unlock(&m_tx_splock);
    //XERR("\n\tQueue full for MBOX%d!\n", m_mbox);
    fail_reason = STOP_Q_FULL;
    return false;
  }

  const uint32_t tx_slot = m_omsg_ring.tx_slot;
  DDBG("\n\t%s: tx_slot = %d\n", __FUNCTION__, tx_slot);

  if (m_omsg_trk.bl_busy[tx_slot] != 0) {
    pthread_spin_unlock(&m_tx_splock);
    XERR("\n\tBD%d busy for MBOX%d! -- pending %d\n", tx_slot, m_mbox, m_omsg_trk.bltx_busy_size);
    fail_reason = STOP_BD_BUSY;
    return false;
  }
 
  /* Adjust length (round up to multiple of 8 bytes) */
  int len8 = 0;
  {
    int k = len/8;
    if(len%8 != 0) k++; 
    len8 = k * 8;
  }

  uint8_t* dest = (uint8_t*)m_omsg_ring.omq[tx_slot].win_ptr;
  memcpy(dest, data, len); // ONLY copy original length else SEGFAULT
  if (len8 > len) memset(dest + len, 0, len8 - len); // do not TX junk padding
  CHECK_END_BD();

  /* Build descriptor associated with buffer */
  hw_omsg_desc* desc = (hw_omsg_desc*)m_omsg_ring.omd.win_ptr;
  assert(desc);

  hw_omsg_desc* ldesc = &desc[tx_slot];

  /* TYPE4 and destination ID */
  ldesc->type_id = (DTYPE4 << 29) | opt.destid;
  /* 16-bit destid, mailbox number, 0xE means 4K, length */
  // ldesc->msg_info = (1 << 26) | (opt.mbox << 22) | ((opt.letter & 0x3) << 16) | (0xe << 12) | (len8 & 0xff8);
  ldesc->msg_info = (opt.mbox << 22) | ((opt.letter & 0x3) << 16) | (0xe << 12) | (len8 & 0xff8);
  /* Buffer pointer points to physical address of current tx_slot */
  ldesc->bufptr_lo = (uint64_t)m_omsg_ring.omq[tx_slot].win_handle & 0xffffffff;
  ldesc->bufptr_hi = (uint64_t)m_omsg_ring.omq[tx_slot].win_handle >> 32;

  CHECK_END_BD();

#ifdef MBOXDEBUG
  dump_ob_desc(ldesc);
#endif

  /* Increment WR_COUNT */
  m_omsg_ring.wr_count++;
  m_omsg_ring.tx_slot++;

  WorkItem_t wi;     memset(&wi, 0, sizeof(wi));
  WorkItem_t wi_end; memset(&wi_end, 0, sizeof(wi_end));

  wi.bd_idx = tx_slot;
  wi.bd_wp  = m_omsg_ring.wr_count;

  pthread_spin_lock(&m_bltx_splock);
  {{
    m_omsg_trk.bl_busy[tx_slot] = tx_slot + 1;

    /* Go to next slot, if only 1 slot, wrap-around to 0 */
    if ((m_omsg_ring.wr_count+1) % m_num_ob_desc == 0) {
      queued_T5 = true;

      DDBG("\n\t%s: tx_slot reset to 0 from %d\n", __FUNCTION__, m_omsg_ring.tx_slot);
      CHECK_END_BD();
      /* Move through the ring link descriptor at the end */
      m_omsg_ring.wr_count++;

      wi_end.bd_idx = tx_slot;
      wi_end.bd_wp  = m_omsg_ring.wr_count;

      m_omsg_ring.tx_slot = 0;

      m_omsg_trk.bl_busy[m_num_ob_desc-1]++;
    }

    wi.ts_start = rdtsc();
    /* Set new write count value */
    wr32mboxchan(TSI721_OBDMACXDWRCNT(m_mbox), m_omsg_ring.wr_count);

    wi.opt = opt;
    wi.valid = WI_SIG;
    m_omsg_trk.bltx_busy[wi.bd_idx] = wi;
    m_omsg_trk.bltx_busy_size++;

    if(queued_T5) {
      wi_end.ts_start = wi.ts_start;
      wi_end.opt = opt_end;
      wi_end.valid = WI_SIG;
      m_omsg_trk.bltx_busy[m_num_ob_desc-1] = wi_end;
      m_omsg_trk.bltx_busy_size++;
    }
  }}
  pthread_spin_unlock(&m_bltx_splock);

  if (! check_reg) goto out;
  
  usleep(11); // XXX this must be coupled with value of RQRPTO

#ifdef MBOXDEBUG
  /* Now poll the RDCNT until it is equal to the WRCNT */
  /* This tells us the descriptors were processed */
  do {
    rd_count = rd32mboxchan(TSI721_OBDMACXDRDCNT(m_mbox));
  } while (rd_count < m_omsg_ring.wr_count);

  DDBG("\n\t%s: After: rd_count = %08X, wr_count = %08X\n", __FUNCTION__, rd_count, m_omsg_ring.wr_count);

  /* Wait until DMA message transfer is finished */
  do {
    reg = rd32mboxchan(TSI721_OBDMACXSTS(m_mbox));
  } while ((reg & TSI721_OBDMACXSTS_RUN) && timeout--);

  if (timeout <= 0) {
    XERR("\n\tDMA[%d] read timeout CH_STAT = %08X\n", m_mbox, reg);
    ret = false; goto out;
  }
#endif

  regi = rd32mboxchan(TSI721_OBDMACXINT(m_mbox));
  regs = rd32mboxchan(TSI721_OBDMACXSTS(m_mbox));

  DDBG("\n\tOBDMACXSTS = %08X\n\tOBDMACXINT_DONE = %08X\n", regs, regi)
  //XDBG("\n\tOBDMACXSTS abort ?= %08X\n\t", regs >> 20)

/*
  if (regi & TSI721_OBDMACXINT_DONE) { // All is good, DONE bit is set
    DDBG("\n\t%s: After: OBDMACXINT = %08X OK\n", __FUNCTION__, regi);
    goto out;
  }
*/

  sts_abort = regs & TSI721_OBDMACXSTS_ABORT;

  if ((! (regi & TSI721_OBDMACXINT_DONE) && (regi & TSI721_OBDMACXINT_ERROR)) || sts_abort) {
    fail_reason = STOP_REG_ERR;
    pthread_spin_lock(&m_bltx_splock);
    m_omsg_trk.bl_busy[wi.bd_idx] = 0;
    m_omsg_trk.bltx_busy[wi.bd_idx].valid = 0; m_omsg_trk.bltx_busy_size--;
    if (queued_T5) {
      m_omsg_trk.bl_busy[wi_end.bd_idx] = 0;
      m_omsg_trk.bltx_busy[m_num_ob_desc-1].valid = 0; m_omsg_trk.bltx_busy_size--;
    }
    pthread_spin_unlock(&m_bltx_splock);

    DDBG("\n\tAfter: OBDMACXINT = %08X ERROR\n", regi);
    /* If there is an error, read the status register for more info */
    volatile uint32_t reg_out_err_stop = rd32mboxchan(RIO_PORT_N_XERR_STATUS_OUTPUT_XERR_STOP);
    volatile uint32_t reg_inp_err_stop = rd32mboxchan(RIO_PORT_N_XERR_STATUS_INPUT_XERR_STOP);

    reg_out_err_stop += 0; // silence g++ warnings
    reg_inp_err_stop += 0;
    DDBG("\n\tOBDMACXSTS = %08X\n\tOUTPUT_XERR_STOP = %08X\n\tINPUT_XERR_STOP  = %08X\n",
        regs, reg_out_err_stop, reg_inp_err_stop)

    if (sts_abort) {
      pthread_spin_unlock(&m_tx_splock); // unlock here to print the blurb lock-free

      XERR("\n\tABORTed on outbound message to mbox=%d destid=%d regs=0x%x regi=0x%xn", m_mbox, opt.destid, regs, regi);
      regs >>= 16;
      regs &= 0x1F;

      switch (regs) {
      case 0x04:
        XERR("\n\tS-RIO excessive msg retry occurred, code 0x%x\n", regs);
        break;
      case 0x05:
        XERR("\n\tS-RIO response timeout occurred, code 0x%x\n", regs);
        break;
      case 0x1F:
        XERR("\n\tS-RIO message XERR response received, code 0x%x\n", regs);
        break;
      case 8:
      case 9:
      case 11:
      case 12:
        XERR("\n\tPCIE Error (not likely to happen), code 0x%x\n", regs);
        break;
      }
    }

    return false;
  }

out:
  pthread_spin_unlock(&m_tx_splock);

  //dump_msg_regs(m_mport, m_mbox);
  return ret;
}

/** \brief Returns whether a message is ready to be read on the inbound.
 * \note This is polling RP/WP registers
 * \param[out] rx_ts rdtsc timestamp when message was ready
 */
bool MboxChannel::inb_message_ready_REG(uint64_t& rx_ts)
{
  uint32_t rd_ptr = 0, wr_ptr = 0;
  int32_t timeout = 100000;

  const int ch = m_ib_mbox + 4;

#if 0//def MBOXDEBUG
  rd_ptr = (int32_t)rd32mboxchan(TSI721_IBDMACXDQRP(ch));
  wr_ptr = (int32_t)rd32mboxchan(TSI721_IBDMACXDQWR(ch));

  if (rd_ptr == wr_ptr) return false;
#endif

  uint64_t tmp_ts = 0;
  do {
    rd_ptr = (int32_t)rd32mboxchan(TSI721_IBDMACXDQRP(ch));
    wr_ptr = (int32_t)rd32mboxchan(TSI721_IBDMACXDQWP(ch));
    tmp_ts = rdtsc();
    //if (wr_ptr == (m_imsg_ring.size -1)) break; // XXX why?
    if (rd_ptr == (m_imsg_ring.size - 1) && wr_ptr == 0) break; // XXX why?
  } while (wr_ptr <= rd_ptr && timeout--);

  if (timeout <= 0)
    return false;
 
  DDBG("\n\tEXIT: DQRP = %X, DQWR = %X\n", rd_ptr, wr_ptr);

  rx_ts = tmp_ts;

  return true;
}

/**
 * Fetch inbound message from the Tsi721 MSG Queue. The fetched message
 * is placed in the buffer provided by add_inb_buffer(). Therefore the
 * return value of this function maybe ignored.
 *
 * @return pointer to the message on success or NULL on failure.
 */
void* MboxChannel::get_inb_message(MboxOptions_t& opt)
{
  void *rx_virt = NULL;
  void *buf = NULL;

  const int ch = m_ib_mbox + 4;

  if (!m_imsg_init) return NULL;

  pthread_spin_lock(&m_rx_splock);

  /* Point to the correct descriptor by adding the desc_rdptr
   * to the base address of the descriptors.
   */
  hw_imsg_desc* desc = (hw_imsg_desc*)m_imsg_ring.imd.win_ptr + m_imsg_ring.desc_rdptr;

  uint32_t rx_slot = m_imsg_ring.rx_slot; // This is an index into user-provided buffer[], NOT HW

#ifdef MBOXDEBUG
  dump_ib_desc(desc);
#endif

  if (!(desc->msg_info & TSI721_IMD_HO)) {
    pthread_spin_unlock(&m_rx_splock);
#ifdef MBOXDEBUG
    XDBG("\n\tTSI721_IMD_HO not set mbox = %d, desc address = %p, rx_slot = %d/0x%x\n", m_ib_mbox, desc, rx_slot,rx_slot);
#endif
    return NULL;
  }

  DDBG("\n\tmbox = %d, desc address = %p, initially rx_slot = %d\n", m_ib_mbox, desc, rx_slot);
  while (m_imsg_ring.imq_base[rx_slot] == NULL) { // Hunt for a buffer
    if (++rx_slot == m_imsg_ring.size)
      rx_slot = 0;
  }

  DDBG("\n\tNow rx_slot = %d, imq_base[rx_slot] = %p\n", rx_slot, m_imsg_ring.imq_base[rx_slot]);

  /* Physical address of this message from the descriptor populated by HW */
  const uint64_t rx_phys = ((uint64_t) desc->bufptr_hi << 32) | desc->bufptr_lo;

  /* Get the correct virtual address of the buffer containing the message
   * by adding the size (computed as the difference between the message's
   * physical address and the physical address of the entire buffer) to
   * the base virtual address of the entire buffer.
   */
  const uint32_t offset = rx_phys - (uint64_t)m_imsg_ring.buf.win_handle;
  rx_virt = (uint8_t *)m_imsg_ring.buf.win_ptr + offset;

  /* imq_base[rx_slot] was populated in add_inb_buffer() */
  buf = m_imsg_ring.imq_base[rx_slot];
  uint32_t msg_size = desc->msg_info & TSI721_IMD_BCOUNT;
  if (msg_size == 0)
    msg_size = RIO_MAX_MSG_SIZE;

  opt.dtype = DTYPE6;
  opt.bcount = msg_size;

  uint64_t enq_ts = m_imsg_ring.imq_ts[rx_slot].valid?
             m_imsg_ring.imq_ts[rx_slot].enq_ts:
             0;
  //opt.ts_start = enq_ts;
  m_imsg_ring.imq_ts[rx_slot].valid = false;

  DDBG("\n\tCopying buffer %p, size = %d from slot %d\n", buf, msg_size, rx_slot);
  /* Copy the message contents to 'buf' */
  memcpy(buf, rx_virt, msg_size);

  /* Now set this buffer pointer back to NULL */
  m_imsg_ring.imq_base[rx_slot] = NULL;

  desc->msg_info &= ~TSI721_IMD_HO;

  opt.destid = desc->type_id & 0xFFFF;
  opt.tt_16b = !! ((desc->type_id >> 19) % 0x3);
  opt.mbox   = (desc->msg_info >> 22) & 0x3;
  opt.letter = (desc->msg_info >> 16) & 0x3;

  /* Increment the rdptr, wrapping around if necessary */
  if (++m_imsg_ring.desc_rdptr == m_imsg_ring.size)
    m_imsg_ring.desc_rdptr = 0;

  /* Update the Descriptor Queue Read Pointer */
  wr32mboxchan(TSI721_IBDMACXDQRP(ch), m_imsg_ring.desc_rdptr);
  //usleep(1);
  DDBG("\n\tNow DQRP := %X -- HW DQRP = %X HW DQWR = %X\n",
      m_imsg_ring.desc_rdptr,
      rd32mboxchan(TSI721_IBDMACXDQRP(ch)),
      rd32mboxchan(TSI721_IBDMACXDQWR(ch)));

  /* Return free buffer into the pointer list */
  uint64_t* free_ptr = (uint64_t *) m_imsg_ring.imfq.win_ptr;
  free_ptr[m_imsg_ring.fq_wrptr] = rx_phys;
  const uint64_t fq_ts = rdtsc();

  m_imsg_ring.imq_ts[rx_slot].enq_ts = fq_ts;
  m_imsg_ring.imq_ts[rx_slot].valid  = true;

  /* Increment free queue pointer, wrapping around if necessary */
  if (++m_imsg_ring.fq_wrptr == m_imsg_ring.size)
    m_imsg_ring.fq_wrptr = 0;

  /* Update the free queue write pointer in the FQWP register */
  wr32mboxchan(TSI721_IBDMACXFQWP(ch), m_imsg_ring.fq_wrptr);

  pthread_spin_unlock(&m_rx_splock);

  return buf;
}

/**
 * Add buffer to the Tsi721 inbound message queue
 *
 * @param[in] buf Buffer to add to inbound queue
 *
 * @return      tx_slot >= 0 if successful  < 0 if unsuccessful
 */
int MboxChannel::add_inb_buffer(void* buf)
{
  pthread_spin_lock(&m_rx_splock);

  uint32_t rx_slot = m_imsg_ring.rx_slot;

  /* There is something wrong if we are trying to add a buffer to
   * a slot that is not NULL since:
   * 1. we initialize them all to NULL; and
   * 2. we set them back to NULL after reading from them.
   */
  if (m_imsg_ring.imq_base[rx_slot]) {
    pthread_spin_unlock(&m_rx_splock);
    XERR("\n\tError adding inbound buffer %d, buffer exists\n", rx_slot);
    return -EINVAL;
  }

  /* This is where the data will be when we read it */
  m_imsg_ring.imq_base[rx_slot] = buf;

  DDBG("\n\t%s: Buffer %p added in slot %d\n", __FUNCTION__, buf, rx_slot);

  /* Increment rx_slot, wrapping around if necessary */
  if (++m_imsg_ring.rx_slot == m_imsg_ring.size) {
    DDBG("\n\trx_slot = %d, resetting to 0 (size=%d)\n", rx_slot,  m_imsg_ring.size);
    m_imsg_ring.rx_slot = 0;
  }

  pthread_spin_unlock(&m_rx_splock);

  return m_imsg_ring.rx_slot;
}

/** \brief Scan MBOX TX completion FIFO and return number of items completed
 * \param[out] completed_work Completed work items, count of which is the return val of this function
 * \param max_work max size of completed_work
 * \return -1 on error, >= 0 count of completed work items
 */
int MboxChannel::scanFIFO(WorkItem_t* completed_work, const int max_work)
{
  if(completed_work == NULL || max_work < 1) return -1;

  if(m_restart_pending) return 0;

  int fifo_count = 0;

  /* Check and clear descriptor status FIFO entries */
  const uint32_t old_rdptr = m_omsg_ring.sts_rdptr;

  uint64_t* sts_ptr = (uint64_t*)m_omsg_ring.sts.win_ptr;
  assert(sts_ptr);

  int j = m_omsg_ring.sts_rdptr * 8;

  const uint64_t HW_START = m_omsg_ring.omd.win_handle;
  const uint64_t HW_END   = HW_START + m_omsg_ring.omd.win_size;

  while (sts_ptr[j]) {
      for (int i = j; i < (j+8) && sts_ptr[i]; i++) {
        if(m_restart_pending) return 0;

        const uint64_t hwptr = sts_ptr[i];
        const uint64_t ts_end = rdtsc();
        sts_ptr[i] = 0;
        if (hwptr >= HW_START && hwptr < HW_END) {
          const uint32_t bd_idx = (hwptr-HW_START)/MBOX_OB_BUFF_DESCR_SIZE;

          if (bd_idx < 0 || bd_idx >= m_num_ob_desc) {
            XCRIT("\n\tInvalid bd_idx = %d @ HW 0x%llx\n", bd_idx, hwptr);
            continue;
          }

          bool found = false;
          uint32_t l_wr_count = 0;

          if(m_restart_pending) return 0;

          pthread_spin_lock(&m_bltx_splock);
            if (m_omsg_trk.bltx_busy[bd_idx].valid == WI_SIG) {
              l_wr_count = m_omsg_trk.bltx_busy[bd_idx].bd_wp;
              m_omsg_trk.bltx_busy[bd_idx].ts_end = ts_end;
              found = true;

              completed_work[fifo_count++] = m_omsg_trk.bltx_busy[bd_idx];

              m_omsg_trk.bltx_busy[bd_idx].valid = 0xdeadbeef;
              m_omsg_trk.bltx_busy_size--;
            }
            if (found) {
              if(!m_restart_pending) { assert(m_omsg_trk.bl_busy[bd_idx] != 0); } // XXX Huh?
              m_omsg_trk.bl_busy[bd_idx] = 0;
            }
          pthread_spin_unlock(&m_bltx_splock);

#ifdef MBOXDEBUG
          uint32_t soft_rp = m_omsg_ring.rd_count_soft;
#endif

          if(m_restart_pending) return 0;

          if (found /*&& bd_idx != m_num_ob_desc*/) { // ignore DTYPE5
            pthread_spin_lock(&m_tx_splock);
            m_omsg_ring.rd_count_soft = l_wr_count;
            pthread_spin_unlock(&m_tx_splock);
#ifdef MBOXDEBUG
            soft_rp = l_wr_count;
#endif
          }

#ifdef MBOXDEBUG
          XDBG("\n\tFIFO line=%d off=%d 0x%llx => BD idx %d %sfound -- TX HW RP=%d soft RP=%d WP=%d -- pending %d\n",
                   j, i, hwptr, bd_idx, (found? "": "NOT "),
                   rd32mboxchan(TSI721_OBDMACXDRDCNT(m_mbox)), soft_rp, m_omsg_ring.wr_count,
                   m_omsg_trk.bltx_busy_size);
#endif
        } else {
          XCRIT("\n\tFIFO line=%d off=%d 0x%llx JUNK\n", j, i, hwptr);
        }
      } // END for line-of-8

      ++m_omsg_ring.sts_rdptr;
      m_omsg_ring.sts_rdptr %= m_omsg_ring.sts_size; // Barry hack
      j = m_omsg_ring.sts_rdptr * 8;
  } // END for scan full FIFO every line-of-8

  if(old_rdptr != m_omsg_ring.sts_rdptr)
    wr32mboxchan(TSI721_OBDMACXDSRP(m_mbox), m_omsg_ring.sts_rdptr);

#ifdef FIFODEBUG
  if(fifo_count > 0 && queueTxSize(m_mbox) > (m_num_ob_desc/2))
    dumpBL(mbox);
#endif

  return fifo_count;
}

void MboxChannel::softRestart(const bool nuke_bds)
{
  const uint64_t ts_s = rdtsc();
  m_restart_pending = 1;

/* Initialize mbox channel */

  m_omsg_ring.tx_slot = 0;
  m_omsg_ring.sts_rdptr = 0;

  if (nuke_bds) {
    memset(m_omsg_ring.omd.win_ptr, 0, m_omsg_ring.omd.win_size);

    memset(m_omsg_trk.bl_busy, 0, (m_omsg_ring.size+1)*sizeof(int));

    m_omsg_trk.bltx_busy_size = 0;
    memset(m_omsg_trk.bltx_busy, 0, (m_omsg_ring.size+1)*sizeof(WorkItem_t));
  }

  set_outb_mbox_hwregs(0);

/* Clear error status from MBOX channel */
  m_mport->wr32(TSI721_OBDMACXINT(m_mbox), 0xFFFFFFFF);

  uint32_t ctl = m_mport->rd32(TSI721_OBDMACXCTL(m_mbox));
  if (ctl & TSI721_OBDMACXCTL_SUSPEND)
    m_mport->wr32(TSI721_OBDMACXCTL(m_mbox), ctl & ~TSI721_OBDMACXCTL_SUSPEND);

  m_mport->wr32(TSI721_OBDMACXCTL(m_mbox), TSI721_OBDMACXCTL_INIT |
						TSI721_OBDMACXCTL_RETRY_THR);
  pthread_spin_init(&m_tx_splock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&m_bltx_splock, PTHREAD_PROCESS_PRIVATE);

  m_restart_pending = 0;
  const uint64_t ts_e = rdtsc();

  XINFO("dT = %llu TICKS\n", (ts_e - ts_s));
}

void MboxChannel::dumpBL()
{
}
