#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "chanlock.h"
#include "memops.h"
#include "dmachanshm.h"

/**
 * \verbatim
In /sys/module/tsi721_mport/parameters/

- 'dma_sel' - DMA channel selection mask. Bitmask that defines which hardware
        DMA channels (0 ... 6) will be registered with DmaEngine core.
        If bit is set to 1, the corresponding DMA channel will be registered.
        DMA channels not selected by this mask will not be used by this device
        driver. Default value is 0x7f (use all channels).


- 'mbox_sel' - RIO messaging MBOX selection mask. This is a bitmask that defines
        messaging MBOXes are managed by this device driver. Mask bits 0 - 3
        correspond to MBOX0 - MBOX3. MBOX is under driver's control if the
        corresponding bit is set to '1'. Default value is 0x0f (= all).
\endverbatim
 */

/** \brief Tsi721 DMA channel availability bitmask vs kernel driver.
 * \note The kernel uses channel 7. ALWAYS
 * \note The kernel SHOULD publish this [exclusion bitmask] using a sysfs interface.
 * \note WARNING Absent sysfs exclusion bitmask we can ony make educated guesses about what DMA channel the kernel is NOT using.
 */
#define DMA_CHAN_MASK_DEFAULT 0x7F

static inline uint8_t DmaChanMask()
{
  char buf[257] = {0};
  uint32_t ret = DMA_CHAN_MASK_DEFAULT;

  FILE* fp = fopen("/sys/module/tsi721_mport/parameters/dma_sel", "r");
  if (fp == NULL)
	goto done;
  if (NULL == fgets(buf, 256, fp))
	goto done;
  fclose(fp);

  if (buf[0] != '\0') {
    if (sscanf(buf, "%x", &ret) != 1) goto done;
    ret &= 0xFF;
    ret = ~ret;
  }

done:
  return (ret & 0xFF);
}

static inline bool check_chan_mask_dma(int chan)
{
  const int chanb = 1 << chan;
  return (chanb & DmaChanMask()) == chanb;
}

RIOMemOpsIntf* RIOMemOpsChanMgr(uint32_t mport_id, bool shared, int channel)
{
  if (channel == 0)
    throw std::runtime_error("RIOMemOpsChanMgr: Channel 0 is used by kernel!");

// MPORT
  if (shared && channel == ANY_CHANNEL)
    return RIOMemOps_classFactory(MEMOPS_MPORT, mport_id, channel);

#if 0
  if (!check_chan_mask_dma(channel))
    throw std::runtime_error("RIOMemOpsChanMgr: Channel is in use by kernel!");
#endif

  if (channel < 0 || channel > 6) 
    throw std::runtime_error("RIOMemOpsChanMgr: Invalid channel!");

// UMD monolithic
  if (! shared) {
    try { return RIOMemOps_classFactory(MEMOPS_UMD, mport_id, channel); }
    catch(std::runtime_error ex) {
      return NULL;
    }
  }

// UMDd/SHM
  if (! DMAChannelSHM_has_state(mport_id, channel)) return NULL; // UMDd/SHM not running on this mport:channel

  return RIOMemOps_classFactory(MEMOPS_UMDD, mport_id, channel);
}
