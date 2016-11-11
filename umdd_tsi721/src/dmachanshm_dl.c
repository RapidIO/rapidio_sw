/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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

#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <dlfcn.h>

#include "dmachanshm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIG 0xF00DB00L

static const char* UMDD_LIB_ENV = "UMDD_LIB";

typedef struct {
  uint32_t sig;
  void* dlh; ///< dlopen-returned handle
  void* dch;

  void* (*create)(const uint32_t mportid, const uint32_t chan);
  void (*destroy)(void* dch);

  int (*pingMaster)(void* dch);
  int (*checkMasterReady)(void* dch);

  int (*checkPortOK)(void* dch);
  int (*dmaCheckAbort)(void* dch, uint32_t* abort_reason);
  uint16_t (*getDestId)(void* dch);
  int (*queueSize)(void* dch);
  int (*queueFull)(void* dch);
  uint64_t (*getBytesEnqueued)(void* dch);
  uint64_t (*getBytesTxed)(void* dch);
  int (*dequeueFaultedTicket)(void* dch, uint64_t* tik);
  int (*dequeueDmaNREADT2)(void* dch, DMAChannelSHM::NREAD_Result_t* res);
  int (*checkTicket)(void* dch, const DMAChannelSHM::DmaOptions_t* opt);

  int (*queueDmaOpT1)(void* dch, enum dma_rtype rtype, DMAChannelSHM::DmaOptions_t* opt, RioMport::DmaMem_t* mem, uint32_t* abort_reason, struct seq_ts* ts_p);
  int (*queueDmaOpT2)(void* dch, enum dma_rtype rtype, DMAChannelSHM::DmaOptions_t* opt, uint8_t* data, const int data_len, uint32_t* abort_reason, struct seq_ts* ts_p);

  void (*getShmPendingData)(void* dch, uint64_t* total, DMAShmPendingData::DmaShmPendingData_t* per_client);

  bool (*has_state)(uint32_t mport_it, uint32_t channel);
  bool (*has_logging)();

} DMAChannelSHMPtr_t;

bool DMAChannelSHM_has_state(uint32_t mport_id, uint32_t channel)
{
  const char* UMDD_LIB;
  DMAChannelSHMPtr_t* libp;
  char* err;

  UMDD_LIB = getenv(UMDD_LIB_ENV);
  if (NULL == UMDD_LIB) {
    return NULL;
  }

  if (access(UMDD_LIB, R_OK)) {
    return NULL;
  }

  libp = (DMAChannelSHMPtr_t*)calloc(1, sizeof(DMAChannelSHMPtr_t));
  if (NULL == libp) {
    return NULL;
  }

  dlerror();
  libp->dlh = dlopen(UMDD_LIB, RTLD_LAZY);
  if (libp->dlh == NULL) {
    err = dlerror();
    fprintf(stderr, "%s: Cannot dlopen %s: %s\n", __func__, UMDD_LIB, err ? err : "NULL");
    assert(libp->dlh);
  }

  libp->has_state = (bool (*)(uint32_t, uint32_t))dlsym(libp->dlh, "DMAChannelSHM_has_state");
  assert(libp->has_state);

  bool ret = libp->has_state(mport_id, channel);

  dlclose(libp->dlh);
  free(libp);

  return ret;
}

bool DMAChannelSHM_has_logging()
{
  const char* UMDD_LIB;
  DMAChannelSHMPtr_t* libp;
  char* err;

  UMDD_LIB = getenv(UMDD_LIB_ENV);
  if (NULL == UMDD_LIB) {
    return NULL;
  }

  if (access(UMDD_LIB, R_OK)) {
    return NULL;
  }

  libp = (DMAChannelSHMPtr_t*)calloc(1, sizeof(DMAChannelSHMPtr_t));
  if (NULL == libp) {
    return NULL;
  }

  dlerror();
  libp->dlh = dlopen(UMDD_LIB, RTLD_LAZY);
  if (libp->dlh == NULL) {
    err = dlerror();
    fprintf(stderr, "%s: Cannot dlopen %s: %s\n", __func__, UMDD_LIB, err ? err : "NULL");
    assert(libp->dlh);
  }

  libp->has_logging = (bool (*)())dlsym(libp->dlh, "DMAChannelSHM_has_logging");
  assert(libp->has_logging);

  bool ret = libp->has_logging();

  dlclose(libp->dlh);
  free(libp);

  return ret;
}

void* DMAChannelSHM_create(const uint32_t mportid, const uint32_t chan)
{
  const char* UMDD_LIB;
  DMAChannelSHMPtr_t* libp;
  char* err;

  UMDD_LIB = getenv(UMDD_LIB_ENV);
  if (NULL == UMDD_LIB) {
    return NULL;
  }

  if (access(UMDD_LIB, R_OK)) {
    return NULL;
  }

  libp = (DMAChannelSHMPtr_t*)calloc(1, sizeof(DMAChannelSHMPtr_t));
  if (NULL == libp) {
    return NULL;
  }

  dlerror();
  libp->dlh = dlopen(UMDD_LIB, RTLD_LAZY);
  if (NULL == libp->dlh) {
    err = dlerror();
    fprintf(stderr, "%s: Cannot dlopen %s: %s\n", __func__, UMDD_LIB, err ? err : "NULL");
    free(libp);
    return NULL;
  }

  libp->create          = (void* (*)(uint32_t, uint32_t))dlsym(libp->dlh, "DMAChannelSHM_create"); assert(libp->create);
  libp->destroy         = (void (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_destroy"); assert(libp->destroy);
  libp->pingMaster      = (int (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_pingMaster"); assert(libp->pingMaster);
  libp->checkMasterReady= (int (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_checkMasterReady"); assert(libp->pingMaster);
  libp->checkPortOK     = (int (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_checkPortOK"); assert(libp->checkPortOK);
  libp->dmaCheckAbort   = (int (*)(void*, uint32_t*))dlsym(libp->dlh, "DMAChannelSHM_dmaCheckAbort"); assert(libp->dmaCheckAbort);
  libp->getDestId       = (uint16_t (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_getDestId"); assert(libp->getDestId);
  libp->queueFull       = (int (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_queueFull"); assert(libp->queueFull);
  libp->getBytesEnqueued= (uint64_t (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_getBytesEnqueued"); assert(libp->getBytesEnqueued);
  libp->getBytesTxed    = (uint64_t (*)(void*))dlsym(libp->dlh, "DMAChannelSHM_getBytesTxed"); assert(libp->getBytesTxed);

  libp->dequeueFaultedTicket= (int (*)(void*, uint64_t*))dlsym(libp->dlh, "DMAChannelSHM_dequeueFaultedTicket");
  assert(libp->dequeueFaultedTicket);
  libp->dequeueDmaNREADT2   = (int (*)(void*, DMAChannelSHM::NREAD_Result_t*))dlsym(libp->dlh, "DMAChannelSHM_dequeueDmaNREADT2");
  assert(libp->dequeueDmaNREADT2);
  libp->checkTicket         = (int (*)(void*, const DMAChannelSHM::DmaOptions_t*))dlsym(libp->dlh, "DMAChannelSHM_checkTicket");
  assert(libp->checkTicket);

  libp->queueDmaOpT1 = (int (*)(void*, enum dma_rtype, DMAChannelSHM::DmaOptions_t*, RioMport::DmaMem_t*, uint32_t*, seq_ts*))dlsym(libp->dlh, "DMAChannelSHM_queueDmaOpT1");
  assert(libp->queueDmaOpT1);
  libp->queueDmaOpT2 = (int (*)(void*, enum dma_rtype, DMAChannelSHM::DmaOptions_t*, uint8_t*, int, uint32_t*, seq_ts*))dlsym(libp->dlh, "DMAChannelSHM_queueDmaOpT2");
  assert(libp->queueDmaOpT2);

  libp->getShmPendingData = (void (*)(void*, uint64_t*, DMAShmPendingData::DmaShmPendingData_t*))dlsym(libp->dlh, "DMAChannelSHM_getShmPendingData");
  assert(libp->getShmPendingData);

  libp->has_state = (bool (*)(uint32_t, uint32_t))dlsym(libp->dlh, "DMAChannelSHM_has_state");
  assert(libp->has_state);

  libp->has_logging = (bool (*)())dlsym(libp->dlh, "DMAChannelSHM_has_logging");
  assert(libp->has_logging);

  libp->dch = libp->create(mportid, chan);
  assert(libp->dch);

  libp->sig = SIG;

  return libp;
}

void DMAChannelSHM_destroy(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  ((DMAChannelSHMPtr_t*)dch)->destroy(((DMAChannelSHMPtr_t*)dch)->dch);

  dlclose(((DMAChannelSHMPtr_t*)dch)->dlh);

  ((DMAChannelSHMPtr_t*)dch)->sig = 0xDEADBEEF;
  free(dch);
}
int DMAChannelSHM_pingMaster(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->pingMaster(((DMAChannelSHMPtr_t*)dch)->dch);
}
int DMAChannelSHM_checkMasterReady(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->checkMasterReady(((DMAChannelSHMPtr_t*)dch)->dch);
}
int DMAChannelSHM_checkPortOK(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->checkPortOK(((DMAChannelSHMPtr_t*)dch)->dch);
}
int DMAChannelSHM_dmaCheckAbort(void* dch, uint32_t* abort_reason)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->dmaCheckAbort(((DMAChannelSHMPtr_t*)dch)->dch, abort_reason);
}
uint16_t DMAChannelSHM_getDestId(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->getDestId(((DMAChannelSHMPtr_t*)dch)->dch);
}
int DMAChannelSHM_queueSize(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->queueSize(((DMAChannelSHMPtr_t*)dch)->dch);
}
int DMAChannelSHM_queueFull(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->queueFull(((DMAChannelSHMPtr_t*)dch)->dch);
}
uint64_t DMAChannelSHM_getBytesEnqueued(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->getBytesEnqueued(((DMAChannelSHMPtr_t*)dch)->dch);
}
uint64_t DMAChannelSHM_getBytesTxed(void* dch)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->getBytesTxed(((DMAChannelSHMPtr_t*)dch)->dch);
}
int DMAChannelSHM_dequeueFaultedTicket(void* dch, uint64_t* tik)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->dequeueFaultedTicket(((DMAChannelSHMPtr_t*)dch)->dch, tik);
}
int DMAChannelSHM_dequeueDmaNREADT2(void* dch, DMAChannelSHM::NREAD_Result_t* res)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->dequeueDmaNREADT2(((DMAChannelSHMPtr_t*)dch)->dch, res);
}
int DMAChannelSHM_checkTicket(void* dch, const DMAChannelSHM::DmaOptions_t* opt)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->checkTicket(((DMAChannelSHMPtr_t*)dch)->dch, opt);
}

int DMAChannelSHM_queueDmaOpT1(void* dch, dma_rtype rtype, DMAChannelSHM::DmaOptions_t* opt, RioMport::DmaMem_t* mem, uint32_t* abort_reason, struct seq_ts* ts_p)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->queueDmaOpT1(((DMAChannelSHMPtr_t*)dch)->dch, rtype, opt, mem, abort_reason, ts_p);
}
int DMAChannelSHM_queueDmaOpT2(void* dch, dma_rtype rtype, DMAChannelSHM::DmaOptions_t* opt, uint8_t* data, const int data_len, uint32_t* abort_reason, struct seq_ts* ts_p)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  return ((DMAChannelSHMPtr_t*)dch)->queueDmaOpT2(((DMAChannelSHMPtr_t*)dch)->dch, rtype, opt, data, data_len, abort_reason, ts_p);
}

void DMAChannelSHM_getShmPendingData(void* dch, uint64_t* total, DMAShmPendingData::DmaShmPendingData_t* per_client)
{
  assert(((DMAChannelSHMPtr_t*)dch)->sig == SIG);
  ((DMAChannelSHMPtr_t*)dch)->getShmPendingData(((DMAChannelSHMPtr_t*)dch)->dch, total, per_client);
}

#ifdef __cplusplus
}; // END extern "C"
#endif
