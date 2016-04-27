#include <rdma.h>
#include <rdma_mport.h>

RIORdmaOpsIntf* RIORdmaOps_classFactory(const RDMAAccess_t type, const int mport, const int channel)
{
  if (mport < 0) throw std::runtime_error("RIORdmaOps_classFactory: Invalid mport!");

  switch (type) {
    case RDMA_MPORT: return new RIORdmaOpsMport(mport); break;
    case RDMA_UMDD: /*return new RIORdmaOpsUMDD(mport, channel);*/ break;
    case RDMA_UMD: /*return new RIORdmaOpsUMD(mport, channel);*/ break;
    default: throw std::runtime_error("RIORdmaOps_classFactory: Invalid access type!"); break;
  }

  throw std::runtime_error("RIORdmaOps_classFactory: BUG");

  /*NOTREACHED*/
  return NULL;
}

