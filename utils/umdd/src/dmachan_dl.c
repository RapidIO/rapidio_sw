#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>

#include "dmachan.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIG 0xF00DB00L

typedef struct {
  uint32_t sig;
  void* dlh; ///< dlopen-returned handle
  void* dch;

  void* (*create)(const uint32_t mportid, const uint32_t chan);
  void (*destroy)(void* dch);

  int (*pingMaster)(void* dch);
  int (*checkPortOK)(void* dch);
  int (*dmaCheckAbort)(void* dch, uint32_t* abort_reason);
  uint16_t (*getDestId)(void* dch);
  int (*queueSize)(void* dch);
  int (*queueFull)(void* dch);
  uint64_t (*getBytesEnqueued)(void* dch);
  uint64_t (*getBytesTxed)(void* dch);
  int (*dequeueFaultedTicket)(void* dch, uint64_t* tik);
  int (*dequeueDmaNREADT2)(void* dch, DMAChannel::NREAD_Result_t* res);
  int (*checkTicket)(void* dch, const DMAChannel::DmaOptions_t* opt);

  int (*queueDmaOpT1)(void* dch, int rtype, DMAChannel::DmaOptions_t* opt, RioMport::DmaMem_t* mem, uint32_t* abort_reason, struct seq_ts* ts_p);
  int (*queueDmaOpT2)(void* dch, int rtype, DMAChannel::DmaOptions_t* opt, uint8_t* data, const int data_len, uint32_t* abort_reason, struct seq_ts* ts_p);
} DMAChannelPtr_t;

void* DMAChannel_create(const uint32_t mportid, const uint32_t chan)
{
  const char* LIB_UMDD = getenv("UMDD_LIB");

  assert(LIB_UMDD);
  assert(access(LIB_UMDD, R_OK) != -1);

  DMAChannelPtr_t* libp = (DMAChannelPtr_t*)calloc(1, sizeof(DMAChannelPtr_t));

  dlerror();

  libp->dlh = dlopen(LIB_UMDD, RTLD_LAZY);
  if (libp->dlh == NULL) {
    fprintf(stderr, "%s: Cannot dlopen %s: %s\n", __func__, LIB_UMDD, dlerror());
    assert(libp->dlh);
  }

  libp->create          = (void* (*)(uint32_t, uint32_t))dlsym(libp->dlh, "DMAChannel_create"); assert(libp->create);
  libp->destroy         = (void (*)(void*))dlsym(libp->dlh, "DMAChannel_destroy"); assert(libp->destroy);
  libp->pingMaster      = (int (*)(void*))dlsym(libp->dlh, "DMAChannel_pingMaster"); assert(libp->pingMaster);
  libp->checkPortOK     = (int (*)(void*))dlsym(libp->dlh, "DMAChannel_checkPortOK"); assert(libp->checkPortOK);
  libp->dmaCheckAbort   = (int (*)(void*, uint32_t*))dlsym(libp->dlh, "DMAChannel_dmaCheckAbort"); assert(libp->dmaCheckAbort);
  libp->getDestId       = (uint16_t (*)(void*))dlsym(libp->dlh, "DMAChannel_getDestId"); assert(libp->getDestId);
  libp->queueFull       = (int (*)(void*))dlsym(libp->dlh, "DMAChannel_queueFull"); assert(libp->queueFull);
  libp->getBytesEnqueued= (uint64_t (*)(void*))dlsym(libp->dlh, "DMAChannel_getBytesEnqueued"); assert(libp->getBytesEnqueued);
  libp->getBytesTxed    = (uint64_t (*)(void*))dlsym(libp->dlh, "DMAChannel_getBytesTxed"); assert(libp->getBytesTxed);

  libp->dequeueFaultedTicket= (int (*)(void*, uint64_t*))dlsym(libp->dlh, "DMAChannel_dequeueFaultedTicket");
  assert(libp->dequeueFaultedTicket);
  libp->dequeueDmaNREADT2   = (int (*)(void*, DMAChannel::NREAD_Result_t*))dlsym(libp->dlh, "DMAChannel_dequeueDmaNREADT2");
  assert(libp->dequeueDmaNREADT2);
  libp->checkTicket         = (int (*)(void*, const DMAChannel::DmaOptions_t*))dlsym(libp->dlh, "DMAChannel_checkTicket");
  assert(libp->checkTicket);

  libp->queueDmaOpT1 = (int (*)(void*, int, DMAChannel::DmaOptions_t*, RioMport::DmaMem_t*, uint32_t*, seq_ts*))dlsym(libp->dlh, "DMAChannel_queueDmaOpT1");
  assert(libp->queueDmaOpT1);
  libp->queueDmaOpT2 = (int (*)(void*, int, DMAChannel::DmaOptions_t*, uint8_t*, int, uint32_t*, seq_ts*))dlsym(libp->dlh, "DMAChannel_queueDmaOpT2");
  assert(libp->queueDmaOpT2);

  libp->dch = libp->create(mportid, chan);
  assert(libp->dch);

  libp->sig = SIG;

  return libp;
}

void DMAChannel_destroy(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  ((DMAChannelPtr_t*)dch)->destroy(((DMAChannelPtr_t*)dch)->dch);

  dlclose(((DMAChannelPtr_t*)dch)->dlh);

  ((DMAChannelPtr_t*)dch)->sig = 0xDEADBEEF;
  free(dch);
}
int DMAChannel_pingMaster(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->pingMaster(((DMAChannelPtr_t*)dch)->dch);
}
int DMAChannel_checkPortOK(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->checkPortOK(((DMAChannelPtr_t*)dch)->dch);
}
int DMAChannel_dmaCheckAbort(void* dch, uint32_t* abort_reason)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->dmaCheckAbort(((DMAChannelPtr_t*)dch)->dch, abort_reason);
}
uint16_t DMAChannel_getDestId(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->getDestId(((DMAChannelPtr_t*)dch)->dch);
}
int DMAChannel_queueSize(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->queueSize(((DMAChannelPtr_t*)dch)->dch);
}
int DMAChannel_queueFull(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->queueFull(((DMAChannelPtr_t*)dch)->dch);
}
uint64_t DMAChannel_getBytesEnqueued(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->getBytesEnqueued(((DMAChannelPtr_t*)dch)->dch);
}
uint64_t DMAChannel_getBytesTxed(void* dch)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->getBytesTxed(((DMAChannelPtr_t*)dch)->dch);
}
int DMAChannel_dequeueFaultedTicket(void* dch, uint64_t* tik)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->dequeueFaultedTicket(((DMAChannelPtr_t*)dch)->dch, tik);
}
int DMAChannel_dequeueDmaNREADT2(void* dch, DMAChannel::NREAD_Result_t* res)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->dequeueDmaNREADT2(((DMAChannelPtr_t*)dch)->dch, res);
}
int DMAChannel_checkTicket(void* dch, const DMAChannel::DmaOptions_t* opt)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->checkTicket(((DMAChannelPtr_t*)dch)->dch, opt);
}

int DMAChannel_queueDmaOpT1(void* dch, int rtype, DMAChannel::DmaOptions_t* opt, RioMport::DmaMem_t* mem, uint32_t* abort_reason, struct seq_ts* ts_p)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->queueDmaOpT1(((DMAChannelPtr_t*)dch)->dch, rtype, opt, mem, abort_reason, ts_p);
}
int DMAChannel_queueDmaOpT2(void* dch, int rtype, DMAChannel::DmaOptions_t* opt, uint8_t* data, const int data_len, uint32_t* abort_reason, struct seq_ts* ts_p)
{
  assert(((DMAChannelPtr_t*)dch)->sig == SIG);
  return ((DMAChannelPtr_t*)dch)->queueDmaOpT2(((DMAChannelPtr_t*)dch)->dch, rtype, opt, data, data_len, abort_reason, ts_p);
}
#ifdef __cplusplus
}; // END extern "C"
#endif
