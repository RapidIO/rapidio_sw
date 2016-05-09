#ifndef __DMASHMPDATA_H__
#define __DMASHMPDATA_H__

#include <stdint.h>

#include "pshm.h"

class DMAShmPendingData {
public:
  static const int DMA_MAX_CHAN = 8;

  /** \brief Track in-flight pending bytes for all channels. This lives in SHM. */
  typedef struct {
    volatile uint64_t data[DMA_MAX_CHAN];
  } DmaShmPendingData_t;

private:
  static constexpr char const* DMA_SHM_PENDINGDATA_NAME = "DMAChannelSHM-pendingdata:%d";

protected:
  DMAShmPendingData(int mportid) : m_mportid(mportid)
  {
    memset(m_shm_pendingdata_name, 0, sizeof(m_shm_pendingdata_name));
    snprintf(m_shm_pendingdata_name, 128, DMA_SHM_PENDINGDATA_NAME, m_mportid);
    bool first_opener_pdata = true;
    m_shm_pendingdata = new POSIXShm(m_shm_pendingdata_name, sizeof(DmaShmPendingData_t), first_opener_pdata);
    m_pendingdata_tally = (DmaShmPendingData_t*)m_shm_pendingdata->getMem();
  }

protected:
  POSIXShm*           m_shm_pendingdata;
  char                m_shm_pendingdata_name[129];
  DmaShmPendingData_t*m_pendingdata_tally;

private:
  int                 m_mportid;
};

#endif // __DMASHMPDATA_H__
