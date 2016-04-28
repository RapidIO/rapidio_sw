#ifndef __MEMOPS_H__
#define __MEMOPS_H__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <stdexcept>

#include "rapidio_mport_dma.h"

typedef enum {
  MEMOPS_MPORT = 1, ///< Use libmport/krenel interfaces
  MEMOPS_UMDD,      ///< Use UMDd/SHM implementation
  MEMOPS_UMD        ///< Use UMD (monolithic, in-app) implementation
} MEMOPSAccess_t;

typedef enum { INVALID = 0, IBWIN = 1, DMAMEM = 2, MALLOC = 3 } DmaMemType_t;

/** \brief Wrapper for all local memory bits of data */
typedef struct {
  DmaMemType_t type;
  uint64_t     rio_address;
  uint64_t     win_handle; ///< Physical mem address, also handle used by kernel to refer to this DMA chunk
                           ///< If 0 this is user memory and NOT dma-able by kernel (directly) or UMD
                           ///< If 0 this cannot be used with UMD
  void*        win_ptr;    ///< Mmaped mem address
  uint32_t     win_size;
  uint64_t     offset;     ///< for data transfers, offset < win_size
} DmaMem_t;

typedef enum { dev08 = 0, dev16 = 1 } TT_t;

/** \brief Request argument for NREAD/NWRITE ops */
typedef struct {
  uint16_t destid;
  //bool     iof;    ///< Note: iof is N/A for any user-mode impl
  bool     crf;
  TT_t     tt_16b; ///< Set to 1 if destid is 16 bytes
  uint8_t  prio:2;
  uint32_t bcount; ///< Size of transfer from \ref mem area
  struct raddr_s {
    uint8_t  msb2;
    uint64_t lsb64;
  } raddr;

  DmaMem_t mem;

  enum riomp_dma_directio_type wr_mode;
  enum riomp_dma_directio_transfer_sync sync;

  uint64_t ticket; ///< For async this is the ticket (UMD) [u64] or kernel cookie [u32]
} MEMOPSRequest_t;

/** \bried Wrapper Interface for NREAD/NWRITE code */
class RIOMemOpsIntf {
public:
  virtual ~RIOMemOpsIntf() {;}

  virtual bool canRestart() { return false; }
  virtual bool restartChannel() { throw std::runtime_error("restartChannel: Operation not supported."); }

  virtual bool queueFull() { return false; } ///< Impl which do not support this SHALL return false

  virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/) = 0;
  virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/) = 0;

  virtual bool wait_async(MEMOPSRequest_t& dmaopt /*only if async flagged*/, int timeout = 0 /*0=blocking*/) = 0;

  virtual bool alloc_dmawin(DmaMem_t& mem /*out*/, const int size) = 0;
  virtual bool alloc_ibwin(DmaMem_t& mem /*out*/, const int size) = 0;

  // TODO: UMD impl will overload alloc_mem and throw an error
  virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
    if (size < 1) return false;
    memset(&mem, 0, sizeof(mem));
    if ((mem.win_ptr = calloc(1, size)) == NULL) return false;
    mem.type = MALLOC;
    return true;
  }

  virtual void free_xwin(DmaMem_t& mem) {
    switch (mem.type) {
      case DMAMEM: free_dmawin(mem); break;
      case IBWIN:  free_ibwin(mem); break;
      case MALLOC: free(mem.win_ptr); break;
      default: throw std::runtime_error("free_xwin: Invalid type!"); break;
    }
    mem.type = INVALID;
  }

  virtual int getAbortReason() = 0;
  virtual const char* abortReasonToStr(const int dma_abort_reason) = 0;

private:
  virtual bool free_dmawin(DmaMem_t& mem) = 0;
  virtual bool free_ibwin(DmaMem_t& mem) = 0;
};

RIOMemOpsIntf* RIOMemOps_classFactory(const MEMOPSAccess_t type, const int mport, const int channel = -1);
#endif // __MEMOPS_H__
