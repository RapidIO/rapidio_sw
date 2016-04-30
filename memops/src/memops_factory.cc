#include <memops.h>
#include <memops_mport.h>
#include <memops_umdd.h>
#include <memops_umd.h>

RIOMemOpsIntf* RIOMemOps_classFactory(const MEMOPSAccess_t type, const int mport, const int channel)
{
  if (mport < 0) throw std::runtime_error("RIOMemOps_classFactory: Invalid mport!");

  switch (type) {
    case MEMOPS_MPORT: return new RIOMemOpsMport(mport); break;
    case MEMOPS_UMDD: if (channel < 2 || channel > 7) // Chan 0 used by kern
                      throw std::runtime_error("RIOMemOps_classFactory: Invalid channel!");
                    return new RIOMemOpsUMDd(mport, channel);
                    break;
    case MEMOPS_UMD: return new RIOMemOpsUMD(mport, channel); break;
    default: throw std::runtime_error("RIOMemOps_classFactory: Invalid access type!"); break;
  }

  throw std::runtime_error("RIOMemOps_classFactory: BUG");

  /*NOTREACHED*/
  return NULL;
}

