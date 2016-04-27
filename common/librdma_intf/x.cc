#include "rdma_mport.h"

RIORdmaOpsMport::RIORdmaOpsMport(const int mport)
{
}
RIORdmaOpsMport::~RIORdmaOpsMport()
{
}
bool RIORdmaOpsMport::nread_mem(RDMARequest_t& dmaopt /*inout*/)
{
}
bool RIORdmaOpsMport::nwrite_mem(RDMARequest_t& dmaopt /*inout*/)
{
}
bool RIORdmaOpsMport::wait_async(RDMARequest_t& dmaopt /*only if async flagged*/, int timeout /*0=blocking*/)
{
}

bool RIORdmaOpsMport::alloc_cmawin(DmaMem_t& mem /*out*/, const int size)
{
}
bool RIORdmaOpsMport::alloc_ibwin(DmaMem_t& mem /*out*/, const int size)
{
}
bool RIORdmaOpsMport::alloc_obwin(DmaMem_t& mem /*out*/, const int size)
{
}

int RIORdmaOpsMport::getAbortReason()
{
}
const char* RIORdmaOpsMport::abortReasonToStr(const int dma_abort_reason)
{
}

bool RIORdmaOpsMport::free_cmawin(DmaMem_t& mem /*out*/)
{
}
bool RIORdmaOpsMport::free_ibwin(DmaMem_t& mem /*out*/)
{
}
bool RIORdmaOpsMport::free_obwin(DmaMem_t& mem /*out*/)
{
}
bool RIORdmaOpsMport::freelloc_obwin(DmaMem_t& mem /*out*/)
{
}
