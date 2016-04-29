#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include "chanlock.h"

/** \brief Tsi721 DMA channel availability bitmask vs kernel driver.
 * \note The kernel uses channel 0. ALWAYS
 * \note The kernel SHOULD publish this [exclusion bitmask] using a sysfs interface.
 * \note WARNING Absent sysfs exclusion bitmask we can ony make educated guesses about what DMA channel the kernel is NOT using.
 */
static uint8_t DMA_CHAN_MASK = 0xFE;

class DMAChanMgr {
public:
  DMAChanMgr(int mport_id);

  int is_channel_selected(const int chan);
  int select_channel(const int chan);
  void deselect_channel(const int chan);

private:
  inline bool check_chan_mask(const int chan);

private:
  int              m_mport_id;
  pthread_mutex_t  m_inuse_bitmask_mutex;
  volatile uint8_t m_inuse_bitmask;
};

DMAChanMgr::DMAChanMgr(int mport_id) : m_mport_id(mport_id)
{
  pthread_mutex_init(&m_inuse_bitmask_mutex, NULL);
  //memset(&m_inuse_bitmask, 0, sizeof(m_inuse_bitmask));
  m_inuse_bitmask = 0;
}

/** \brief Can we use this channel (i.e. not used by kernel) ?
 * \note In real life this should be obtained per-mport
 */
bool DMAChanMgr::check_chan_mask(int chan)
{
  const int chanb = 1 << chan;
  return (chanb & DMA_CHAN_MASK) == chanb;
}

int DMAChanMgr::is_channel_selected(const int chan)
{
  if (m_inuse_bitmask == 0) return 0;
  if (! check_chan_mask(chan)) return -2; // in use by kernel

  const int chanb = 1 << chan;
  return (m_inuse_bitmask & chanb) == chanb;
}

/** \brief Reserve a UMD hw channel
 * \param chan if -1 pick next available channel, if > 0 pick this channel if available
 * \return -1 no channel is available, >0 the channel
 */
int DMAChanMgr::select_channel(const int chan)
{
  if (m_inuse_bitmask == DMA_CHAN_MASK) return -1; // ALL channels are taken

  int sel_chan = -1;
  pthread_mutex_lock(&m_inuse_bitmask_mutex);
  do {
    if (chan > 0) {
      if (! check_chan_mask(chan)) break; // in use by kernel
      m_inuse_bitmask |= (1 << chan);
      sel_chan = chan;
      break;
    }
    for (int i = 1; i < 8; i++) {
      if (! check_chan_mask(i)) continue; // in use by kernel
      if ((m_inuse_bitmask & (1 << i)) == 0) {
        m_inuse_bitmask |= (1 << i);
        sel_chan = i;
        break;
      }
    }
  } while(0);
  pthread_mutex_unlock(&m_inuse_bitmask_mutex);

  return sel_chan;
}

void DMAChanMgr::deselect_channel(const int chan)
{
  if (chan < 1 || chan > 7) return;
  if (m_inuse_bitmask == 0) return; // nothing to do
  pthread_mutex_lock(&m_inuse_bitmask_mutex);
  m_inuse_bitmask &= ~(1 << chan);
  pthread_mutex_unlock(&m_inuse_bitmask_mutex);
}

#ifdef TEST_UMDD

int main()
{
  const int mport_id = 0;

  int chan = 0;

  DMAChanMgr cm(mport_id);

  chan = cm.select_channel(7);
  assert(chan == 7);
  assert(cm.is_channel_selected(7) > 0);
  cm.deselect_channel(mport_id, 7);
  assert(cm.is_channel_selected(7) == 0);

  for (chan = 1; chan < 8; chan++) {
    int chan2 = cm.select_channel(0);
    assert(chan2 > 0);
  }

  // Full??
  chan = cm.select_channel(0);
  assert(chan == -1);

  for (chan = 1; chan < 8; chan++) {
    cm.deselect_channel(chan);
  }

  return 0;
}
#endif
