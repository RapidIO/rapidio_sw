#include <stdio.h> // snprintf

#include "memops.h"
#include "memops_mport.h"
#include "memops_umd.h"

#include "dmachanshm.h"

RIOMemOpsUMD::RIOMemOpsUMD(const int mport_id, const int chan) : RIOMemOpsMport(mport_id)
{
  static char tmp[129] = {0};

  m_errno = 0;

  memset(&m_stats, 0, sizeof(m_stats));

  m_dch = new DMAChannel(mport_id, chan, m_mp_h); // Will throw on error
}

RIOMemOpsUMD::~RIOMemOpsUMD()
{
  delete m_dch;
  // Note: m_mp_h destroyed by ~RIOMemOpsMport
}

bool RIOMemOpsUMD::nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  if (dmaopt.mem.type != DMAMEM) 
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Unsupported memory type!");

  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC || dmaopt.sync == RIO_DIRECTIO_TRANSFER_SYNC)
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Operation not supported! Use FAF.");

  m_errno = 0;
  dmaopt.ticket = 0;

  if (m_dch->queueFull()) { m_errno = ENOSPC; return false; }

  // printf("UMDD %s: destid=%u handle=0x%lx rio_addr=0x%lx+0x%x bcount=%d op=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, wr_mode, sync);

  DMAChannel::DmaOptions_t opt; memset(&opt, 0, sizeof(opt));
  opt.destid      = dmaopt.destid;
  opt.tt_16b      = (bool)dmaopt.tt_16b;
  opt.prio        = dmaopt.prio;
  opt.crf         = dmaopt.crf;
  opt.bcount      = dmaopt.bcount;
  opt.raddr.lsb64 = dmaopt.raddr.lsb64;
  opt.raddr.msb2  = dmaopt.raddr.msb2;

  RioMport::DmaMem_t dmamem; memset(&dmamem, 0, sizeof(dmamem));

  dmamem.type       = RioMport::DONOTCHECK;
  dmamem.win_handle = dmaopt.mem.win_handle + dmaopt.mem.offset;
  dmamem.win_size   = dmaopt.mem.win_size;

  bool q_was_full = false;
  uint32_t dma_abort_reason = 0;

  if (dmaopt.bcount > 16) {
    if (! m_dch->queueDmaOpT1((int)DMAChannelSHM::convert_riomp_dma_directio(dmaopt.wr_mode),
                              opt, dmamem, dma_abort_reason, &m_stats)) {
      if (q_was_full) { m_errno = ENOSPC; return false; }
      m_errno = EINVAL; return false;
    }
  } else {
    if (! m_dch->queueDmaOpT2((int)DMAChannelSHM::convert_riomp_dma_directio(dmaopt.wr_mode),
                              opt, (uint8_t *)dmaopt.mem.win_handle + dmaopt.mem.offset, dmaopt.bcount, dma_abort_reason, &m_stats)) {
      if (q_was_full) { m_errno = ENOSPC; return false; }
      m_errno = EINVAL; return false;
    }
  }

  return true;
}

bool RIOMemOpsUMD::nread_mem(MEMOPSRequest_t& dmaopt /*inout*/)
{
  if (dmaopt.sync == RIO_DIRECTIO_TRANSFER_ASYNC || dmaopt.sync == RIO_DIRECTIO_TRANSFER_SYNC)
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Operation not supported! Use FAF.");

  if (dmaopt.mem.type != DMAMEM) 
    throw std::runtime_error("RIOMemOpsUMD::nwrite_mem: Unsupported memory type!");

  m_errno = 0;
  dmaopt.ticket = 0;

#ifdef MEMOPS_DEBUG
  printf("UMDD %s: destid=%u handle=0x%lx  rio_addr=0x%lx+0x%x\n bcount=%d sync=%d\n", __func__, destid, handle, tgt_addr, offset, size, sync);
#endif

  if (m_dch->queueFull()) { m_errno = ENOSPC; return false; }

  DMAChannel::DmaOptions_t opt;
  memset(&opt, 0, sizeof(opt));
  opt.destid      = dmaopt.destid;
  opt.tt_16b      = (bool)dmaopt.tt_16b;
  opt.prio        = dmaopt.prio;
  opt.crf         = dmaopt.crf;
  opt.bcount      = dmaopt.bcount;
  opt.raddr.lsb64 = dmaopt.raddr.lsb64;
  opt.raddr.msb2  = dmaopt.raddr.msb2;

  RioMport::DmaMem_t dmamem; memset(&dmamem, 0, sizeof(dmamem));

  dmamem.type       = RioMport::DONOTCHECK;
  dmamem.win_handle = dmaopt.mem.win_handle + dmaopt.mem.offset;
  dmamem.win_size   = dmaopt.bcount;

  bool q_was_full = false;
  uint32_t dma_abort_reason = 0;

  if (! m_dch->queueDmaOpT1(NREAD, opt, dmamem, dma_abort_reason, &m_stats)) {
    if (q_was_full) { m_errno = ENOSPC; return false; }
    m_errno = EINVAL; return false;
  }

  return true;
}

bool RIOMemOpsUMD::wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/)
{
  throw std::runtime_error("RIOMemOpsUMD::wait_async: Operation not supported! Use FAF.");
}

const char* RIOMemOpsUMD::abortReasonToStr(int abort_reason)
{
  return strerror(m_errno);
}

int RIOMemOpsUMD::getAbortReason() { return m_errno; }
