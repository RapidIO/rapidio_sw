#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include "chanlock.h"
#include "memops.h"
#include "dmachanshm.h"

/** \brief Tsi721 DMA channel availability bitmask vs kernel driver.
 * \note The kernel uses channel 0. ALWAYS
 * \note The kernel SHOULD publish this [exclusion bitmask] using a sysfs interface.
 * \note WARNING Absent sysfs exclusion bitmask we can ony make educated guesses about what DMA channel the kernel is NOT using.
 */
static uint8_t DMA_CHAN_MASK = 0xFE;

static bool check_chan_mask(int chan)
{
  const int chanb = 1 << chan;
  return (chanb & DMA_CHAN_MASK) == chanb;
}

RIOMemOpsIntf* RIOMemOpsChanMgr(uint32_t mport_id, bool shared, int channel)
{
  if (channel == 0)
    throw std::runtime_error("RIOMemOpsChanMgr: Channel 0 is used by kernel!");

// MPORT
  if (shared && channel == ANY_CHANNEL)
    return RIOMemOps_classFactory(MEMOPS_MPORT, mport_id, channel);

  if (!check_chan_mask(channel))
    throw std::runtime_error("RIOMemOpsChanMgr: Channel is in use by kernel!");

  if (channel < 1 || channel > 7) 
    throw std::runtime_error("RIOMemOpsChanMgr: Invalid channel!");

// UMD monolithic
  if (! shared) {
    try { return RIOMemOps_classFactory(MEMOPS_UMD, mport_id, channel); }
    catch(std::logic_error ex) {
      return NULL;
    }
  }

// UMDd/SHM
  if (! DMAChannelSHM_has_state(mport_id, channel)) return NULL; // UMDd/SHM not running on this mport:channel

  return RIOMemOps_classFactory(MEMOPS_UMDD, mport_id, channel);
}
