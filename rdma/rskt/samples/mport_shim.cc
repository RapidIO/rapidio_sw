#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdexcept>

#include "rapidio_mport_mgmt.h"

typedef struct {
  uint8_t   np;
  uint32_t* dev_ids;
} MportList_t;

extern "C"
uint16_t mport_my_destid()
{
  int rc = 0;
  uint16_t my_destid = 0xFFFF;
  
  MportList_t* mpl = (MportList_t*)calloc(1, sizeof(MportList_t));
  assert(mpl);

  mpl->np = 8;
  if ((rc = riomp_mgmt_get_mport_list(&mpl->dev_ids, &mpl->np))) {
    char tmp[129] = {0};
    snprintf(tmp, 128, "Mport Shim: riomp_mgmt_get_mport_list failed %d: %s\n", rc, strerror(errno));
    throw std::runtime_error(tmp);
  }
  /// \todo UNDOCUMENTED: Lower nibble has destid
  if (mpl->np == 0) {
    char tmp[129] = {0};
    snprintf(tmp, 128, "Mport Shim: No mport instances found.\n");
    throw std::runtime_error(tmp);
  }
  my_destid = mpl->dev_ids[0] & 0xFFFF;
  riomp_mgmt_free_mport_list(&mpl->dev_ids);

  cfree(mpl);

  return my_destid;
}

extern "C"
void mport_set_ioctl(void* arg)
{
  riomp_mgmt_set_ioctl(arg);
}

