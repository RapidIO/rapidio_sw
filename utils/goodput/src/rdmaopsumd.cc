/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "libcli.h"
#include "liblog.h"

#include "libtime_utils.h"
#include "worker.h"

#include "dmachan.h"
#include "rdmaops.h"
#include "rdmaopsumd.h"

/** \brief T2 NREAD data from peer at high priority, all-in-one, blocking
 * \param[in] info C-like this
 * \param destid RIO destination id of peer
 * \param rio_addr RIO mem address into peer's IBwin, not 50-but compatible
 * \param size How much data to read, up to 16 bytes
 * \param[out] Points to where data will be deposited
 * \retuen true if NREAD completed OK
 */
bool RdmaOpsUMD::nread_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, uint8_t* data_out)
{
  int i;

  assert(m_dmac);

  if(size < 1 || size > 16 || data_out == NULL) return false;

#ifdef UDMA_TUN_DEBUG_NREAD
  DBG("\n\tNREAD from RIO %d bytes destid %u addr 0x%llx\n", size, destid, rio_addr);
#endif

  DMAChannel::DmaOptions_t dmaopt; memset(&dmaopt, 0, sizeof(dmaopt));
  dmaopt.destid      = destid;
  dmaopt.prio        = 2; // We want to get in front all pending ALL_WRITEs in 721 silicon
  dmaopt.bcount      = size;
  dmaopt.raddr.lsb64 = rio_addr;

  struct seq_ts tx_ts;
  DMAChannel::WorkItem_t wi[DMA_CHAN2_STS*8]; memset(wi, 0, sizeof(wi));

  int q_was_full = !m_dmac->queueDmaOpT2((int)NREAD, dmaopt, data_out, size, m_dma_abort_reason, &tx_ts);

  i = 0;
  if (! m_dma_abort_reason) {
    DBG("\n\tPolling FIFO transfer completion destid=%d\n", destid);

    for(i = 0;
          !q_was_full && (i < 100000)
      && m_dmac->queueSize(); 
        i++) {
      m_dmac->scanFIFO(wi, DMA_CHAN2_STS*8);
      usleep(1);
    }
  }

  if (m_dma_abort_reason || (m_dmac->queueSize() > 0)) { // Boooya!! Peer not responding
    uint32_t RXRSP_BDMA_CNT = 0;
    bool inp_err = false, outp_err = false;
    m_dmac->checkPortInOutError(inp_err, outp_err);

    {{
      RioMport* mport = new RioMport(m_mp_num, m_mp_h);
      RXRSP_BDMA_CNT = mport->rd32(TSI721_RXRSP_BDMA_CNT); // aka 0x29904 Received Response Count for Block DMA Engine Register
      delete mport;
    }}

    CRIT("\n\tChan2 %u stalled with %sq_size=%d WP=%lu FIFO.WP=%llu %s%s%s%sRXRSP_BDMA_CNT=%u abort reason 0x%x %s After %d checks qful %d\n",
          m_dmac->getChannel(),
          (q_was_full? "QUEUE FULL ": ""), m_dmac->queueSize(),
                      m_dmac->getWP(), m_dmac->m_tx_cnt, 
          (m_dmac->checkPortOK()? "Port:OK ": ""),
          (m_dmac->checkPortError()? "Port:ERROR ": ""),
          (inp_err? "Port:OutpERROR ": ""),
          (inp_err? "Port:InpERROR ": ""),
          RXRSP_BDMA_CNT,
          m_dma_abort_reason, DMAChannel::abortReasonToStr(m_dma_abort_reason), i, q_was_full);

    m_dmac->softRestart();

    return false;
  }

#ifdef UDMA_TUN_DEBUG_NREAD
  if (RDMA_LL_DBG <= g_level) {
    std::stringstream ss;
    for(int i = 0; i < size; i++) {
      char tmp[9] = {0};
      snprintf(tmp, 8, "%02x ", wi[0].t2_rddata[i]);
      ss << tmp;
    }
    DBG("\n\tNREAD-in data: %s\n", ss.str().c_str());
  }
#endif

  memcpy(data_out, wi[0].t2_rddata, size);

  return true;
}

/** \brief T2 NWRITE_R data to peer at high priority, all-in-one, blocking
 * \param[in] info C-like this
 * \param destid RIO destination id of peer
 * \param rio_addr RIO mem address into peer's IBwin, not 50-but compatible
 * \param size How much data to write, up to 16 bytes
 * \param[in] Points to where data are
 * \retuen true if NWRITE_R completed OK
 */
bool RdmaOpsUMD::nwrite_mem_T2(const uint16_t destid, const uint64_t rio_addr, const int size, const uint8_t* data)
{
  int i = 0;

  assert(m_dmac);

  if(size < 1 || size > 16 || data == NULL) return false;

#ifdef UDMA_TUN_DEBUG_NWRITE
  DBG("\n\tNREAD from RIO %d bytes destid %u addr 0x%llx\n", size, destid, rio_addr);
#endif

  DMAChannel::DmaOptions_t dmaopt; memset(&dmaopt, 0, sizeof(dmaopt));
  dmaopt.destid      = destid;
  dmaopt.prio        = 2; // We want to get in front all pending ALL_WRITEs in 721 silicon
  dmaopt.bcount      = size;
  dmaopt.raddr.lsb64 = rio_addr;

  struct seq_ts tx_ts;

#ifdef UDMA_TUN_DEBUG_NWRITE_CH2
  DMAChannel::WorkItem_t wi[DMA_CHAN2_STS*8]; memset(wi, 0, sizeof(wi));
#endif

  int q_was_full = !m_dmac->queueDmaOpT2((int)ALL_NWRITE_R, dmaopt, (uint8_t*)data, size, m_dma_abort_reason, &tx_ts);

#ifdef UDMA_TUN_DEBUG_NWRITE_CH2
  i = 0;
  if (! m_dma_abort_reason) {
    DBG("\n\tPolling FIFO transfer completion destid=%d\n", destid);

    for(i = 0; !q_was_full && (i < 100000) && m_dmac->queueSize(); i++) {
      m_dmac->scanFIFO(wi, DMA_CHAN2_STS*8);
      usleep(1);
    }
  }
#endif

  if (m_dma_abort_reason
#ifdef UDMA_TUN_DEBUG_NWRITE_CH2
      || (m_dmac->queueSize() > 0)
#endif
        ) { // Boooya!! Peer not responding
    uint32_t RXRSP_BDMA_CNT = 0;
    bool inp_err = false, outp_err = false;
    m_dmac->checkPortInOutError(inp_err, outp_err);

    {{
      RioMport* mport = new RioMport(m_mp_num, m_mp_h);
      RXRSP_BDMA_CNT = mport->rd32(TSI721_RXRSP_BDMA_CNT); // aka 0x29904 Received Response Count for Block DMA Engine Register
      delete mport;
    }}

    CRIT("\n\tChan2 %u stalled with %sq_size=%d WP=%lu FIFO.WP=%llu %s%s%s%sRXRSP_BDMA_CNT=%u abort reason 0x%x %s After %d checks qful %d\n",
          m_dmac->getChannel(),
          (q_was_full? "QUEUE FULL ": ""), m_dmac->queueSize(),
          m_dmac->getWP(), m_dmac->m_tx_cnt,
          (m_dmac->checkPortOK()? "Port:OK ": ""),
          (m_dmac->checkPortError()? "Port:ERROR ": ""),
          (inp_err? "Port:OutpERROR ": ""),
          (inp_err? "Port:InpERROR ": ""),
          RXRSP_BDMA_CNT,
          m_dma_abort_reason, DMAChannel::abortReasonToStr(m_dma_abort_reason), i, q_was_full);

    m_dmac->softRestart();

    return false;
  } // END if not responding

  return true;
}

bool RdmaOpsUMD::nwrite_mem(DMAChannel::DmaOptions_t& dmaopt, RioMport::DmaMem_t& dmamem)
{
  assert(m_dmac);

  return m_dmac->queueDmaOpT1(LAST_NWRITE_R, dmaopt, dmamem, m_dma_abort_reason, &m_meas_ts);
}

/** \brief Setup the TX/NREAD DMA channel
 * \note This channel will do only one operation at a time
 * \note It will get only the minimum number of BDs \ref DMA_CHAN2_BUFC
 */
DMAChannel* RdmaOpsUMD::setup_chan2(struct worker *info)
{
  assert(info);
  assert(m_dmac == NULL);
  if (info == NULL) return NULL;

  setMportInfo(info->mp_num, info->mp_h);

  DMAChannel* dch = new DMAChannel(info->mp_num, info->umd_chan2, info->mp_h);
  if (NULL == dch) {
    CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
            info->umd_chan2, info->mp_num, info->mp_h);
    goto error;
  }

  // TX - Chan 2
  dch->setCheckHwReg(true);
  if (! dch->alloc_dmatxdesc(DMA_CHAN2_BUFC)) {
    CRIT("\n\talloc_dmatxdesc failed: bufs %d", DMA_CHAN2_BUFC);
    goto error;
  }
  if (! dch->alloc_dmacompldesc(DMA_CHAN2_STS)) {
    CRIT("\n\talloc_dmacompldesc failed: entries %d", DMA_CHAN2_STS);
    goto error;
  }

  dch->resetHw();
  if (!dch->checkPortOK()) {
    CRIT("\n\tPort %d is not OK!!! Exiting...", info->umd_chan2);
    goto error;
  }

  return (m_dmac = dch);

error:
  if (dch != NULL) delete dch;
  return NULL;
}

/** \brief Setup a TX/NWRITE DMA channel
 */
DMAChannel* RdmaOpsUMD::setup_chanN(struct worker* info, int chan, RioMport::DmaMem_t* dmamem)
{
  assert(info);
  assert(m_dmac == NULL);

  if (info == NULL || dmamem == NULL) return NULL;

  setMportInfo(info->mp_num, info->mp_h);

  DMAChannel* dch = new DMAChannel(info->mp_num, chan, info->mp_h);
  if (NULL == dch) {
    CRIT("\n\tDMAChannel alloc FAIL: chan %d mp_num %d hnd %x",
         chan, info->mp_num, info->mp_h);
    goto error;
  }

  // TX
  if (! dch->alloc_dmatxdesc(info->umd_tx_buf_cnt)) {
    CRIT("\n\talloc_dmatxdesc failed: bufs %d", info->umd_tx_buf_cnt);
    goto error;
  }
  if (! dch->alloc_dmacompldesc(info->umd_sts_entries)) {
    CRIT("\n\talloc_dmacompldesc failed: entries %d", info->umd_sts_entries);
    goto error;
  }

  for (int i = 0; i < info->umd_tx_buf_cnt; i++) {
    if (! dch->alloc_dmamem(BD_PAYLOAD_SIZE(info), dmamem[i])) {
      CRIT("\n\talloc_dmamem failed: i %d size %x", i, BD_PAYLOAD_SIZE(info));
      goto error;
    };
    memset(dmamem[i].win_ptr, 0, dmamem[i].win_size);
  }

  dch->resetHw();
  if (! dch->checkPortOK()) {
    CRIT("\n\tPort %d is not OK!!! Exiting...", chan);
    goto error;
  }

  return (m_dmac = dch);

error:
  if (dch != NULL) delete dch;
  return NULL;
}


