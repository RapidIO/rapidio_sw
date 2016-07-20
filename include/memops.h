/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

#ifndef __MEMOPS_H__
#define __MEMOPS_H__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <stdexcept>

#include "rapidio_mport_dma.h"

/** \file memops.h
 * \brief Unified DMA memory operations interface.  Includes memory allocation,
 * memory freeing, physical-to-virtual mapping, and read/write routines.
 *
 * This interface unifies 
 * the use of kernel driver DMA common/include/rapidio_mport_dma.h interfaces,
 * with the shared memory DMA user mode driver and the
 * dedicated user mode driver.
 *
 * Note that this interface uses a handle allocated using the 
 * include/mportmgmt.h interface.
 *
 */

/** \brief Supported drivers 
 */
typedef enum {
	MEMOPS_MPORT = 1, ///< Use libmport/kernel driver
	MEMOPS_UMDD, ///< Use UMDd/SHM driver -- SHARED use of a channel
	MEMOPS_UMD ///< Use UMD (monolithic, in-process implementation 
		///< -- EXCLUSIVE use of a channel
} MEMOPSAccess_t;

/** \brief Memory buffer types supported
 */
typedef enum {
	INVALID = 0, ///< Uninitialized, or unallocated */
	IBWIN = 1, ///< Inbound memory window, accessible locally and remotely
	IBWIN_FIXD = 2, ///< Inbound memory "window" managed by RSKTD , accessible locally and remotely
	DMAMEM = 3, ///< Local physically contiguous memory, suitable for DMA
	MALLOC = 4 ///< Locally allocated memory, low performance, must be
		///< used with common/include/rapidio_mport_dma.h driver
} DmaMemType_t;

/** \brief Wrapper for all local memory bits of data */
typedef struct {
	DmaMemType_t type;      ///< Must be set to DMAMEM or MALLOC when 
				///<   calling alloc_dmawin
	uint64_t rio_address;	///< RapidIO adddress for internal memory.
				///<   Only valid for IBWIN buffers.  
				///<   Set to RIO_ANY_ADDRESS to allow 
				///<   software to choose RapidIO address,
				///<   otherwise set to required RapidIO address
	uint64_t win_handle;	///< Physical mem address, also handle used by 
				///<   kernel to refer to this DMA chunk
				///<   0 : user memory, cannot be used with UMD
				///<  !0 : IBWIN, can be used with UMD
	void* win_ptr;		///< Virtual (mmap()ed) memory address
	uint32_t win_size;	///< Size in bytes of window.
	uint64_t offset;	///< Offset, in bytes, from the win_ptr.
				///<     Used  for data transfers,
				///<     offset < win_size
} DmaMem_t;

/** \brief Device identifier size, either 8 or 16 bits for now */
typedef enum {
	dev08 = 0, 
	dev16 = 1
} TT_t;

/** \brief Request argument for NREAD/NWRITE ops */
typedef struct {
	TT_t tt_16b; ///< Size of destination ID of remote node
	uint16_t destid; ///< RapidIO destination ID of remote node
	uint8_t	prio:2; ///< Physical priority of RapidIO requests.
	bool crf; ///< Critical Request Flow, suffix bit for physical priority 
		///< If CRF is set, the packet may pass packets of the same
		///< priority without the CRF bit set.
	uint32_t bcount; ///< Size of transfer from \ref mem area
	struct raddr_s {
		uint8_t	msb2; ///< Most significant bits of 66 bit address
		uint64_t lsb64; ///< Least significant 64 bits of 66 bit address
				///< or entire 50 and 34 bit address.
	} raddr;

	DmaMem_t mem; ///< Local memory area for the nread/nwrite operation

	enum riomp_dma_directio_type wr_mode; ///< RapidIO write type.
	enum riomp_dma_directio_transfer_sync sync; ///< Block/async/FAF

	uint64_t ticket; ///< For async this is the ticket (UMD) [u64]
			///< or kernel cookie [u32] to be checked 
} MEMOPSRequest_t;

/** \brief Wrapper Interface for NREAD/NWRITE code */
class RIOMemOpsIntf {
public:
	static const int MAX_TIMEOUT = 6100; // 6.1 sec

public:
	virtual ~RIOMemOpsIntf() {;}

	/** \brief MEMOPS_MPORT cannot be restarted by the application, they are
	 * restarted automatically by the kernel.
	 */
	virtual bool canRestart() { return false; }

	/** \brief non-MEMOPS_MPORT can be restarted by the application. */
	virtual bool restartChannel() { 
		throw std::runtime_error("restartChannel: "
					"Operation not supported.");
	}

	/** \brief non-MEMOPS_MPORT can be checked for access failures by the application
	*/
	virtual bool checkAbort() {
		throw std::runtime_error("checkAbort: "
					"Operation not supported.");
	}

	/** \brief Check to see if the transmit transaction queue is full. */
	virtual bool queueFull() { return false; } 

	/** Read and write memory remotely */
	virtual bool nread_mem(MEMOPSRequest_t& dmaopt /*inout*/) = 0;
	virtual bool nwrite_mem(MEMOPSRequest_t& dmaopt /*inout*/) = 0;

	/** \brief Used to confirm when a
	 * MEMOPSRequest_t::sync == RIO_DIRECTIO_TRANSFER_ASYNC transaction
	 * has completed.  Returns immediately for RIO_DIRECTIO_TRANSFER_SYNC
	 * 
	 * @param[in] dmaopt Options used for nread_mem or nwrite_mem.
	 * @param[in] timeout 0 - block forever until transaction is complete,
	 *            otherwise timeout after this number of seconds.
	 * return false - transaction has completed successfully.
	 * 	  true - transaction has not completed, or a timeout occurred.
	 * 	  check errno to determine reason for failure.
	 */
	virtual bool wait_async(MEMOPSRequest_t& dmaopt, int timeout = 0) = 0;

	/** \brief Used to allocate a local memory buffer to act as a
	 * source/target for nread_mem and nwrite_mem operations.  The buffer
	 * does not have a RapidIO address, so it cannot be accessed by other
	 * nodes.
	 * 
	 * \note This is done via kernel CMA allocator. Beware of alignment.
	 *
	 * @param[in] mem Parameters used to request memory te_mem.
	 * @param[in] size number of bytes to allocate. 0 is illegal.
	 *                 Will be rounded up to at least the next power of
	 *                 2.
	 * return false - memory allocated successfully.
	 * 	  true - memory could not be allocated.
	 * 	  	Check errno to determine reason for failure.
	 */
	virtual bool alloc_dmawin(DmaMem_t& mem, const int size) = 0;

	/** \brief Used to allocate a local memory buffer and map it to a RapidIO
	 * address.  This memory is accessible from other nodes using nread_mem and
	 * nwrite_mem.
	 *
	 * \note This is done via kernel CMA allocator. Beware of alignment.
	 *
	 * @param[in] mem Parameters used to request memory te_mem.
	 * @param[in] size number of bytes to allocate. 0 is illegal.
	 *                 Will be rounded up to at least the next power of
	 *                 2.
	 * return false - memory allocated successfully.
	 * 	  true - memory could not be allocated.
	 * 	  	Check errno to determine reason for failure.
	 */
	virtual bool alloc_ibwin(DmaMem_t& mem /*out*/, const int size) = 0;

	/** \brief Used to allocate a local memory buffer and map it to a RapidIO
	 * address.  This memory is accessible from other nodes using nread_mem and
	 * nwrite_mem.
	 *
	 * \note This is done via kernel "memmap=" memory area.
	 * \note This can be done only once per node (no locking/exclusion). Use external locking.
	 *
	 * \note Size of memory allocated may be bigger than \ref size -- prescribed in /etc/rapidio/rsvd_phys_mem.conf
	 *
	 * @param[in] mem Parameters used to request memory te_mem.
	 * @param[in] size number of bytes to allocate. 0 is illegal.
	 *                 Will be rounded up to at least the next power of
	 *                 2.
	 * @param[in] RegionName Eg. "DMATUN" -- see /etc/rapidio/rsvd_phys_mem.conf
	 *
	 * return false - memory allocated successfully.
	 * 	  true - memory could not be allocated.
	 * 	  	Check errno to determine reason for failure.
	 */
	virtual bool alloc_ibwin_rsvd(DmaMem_t& mem /*out*/, const int size, const char* RegionName) = 0;

	/** \brief Used to map a local memory buffer.  This memory is accessible from other nodes using nread_mem and nwrite_mem.
	 *
	 * \note This should be used with a handle obtained from RSKTD which had already programmed the Tsi721 IBwin register for the region which includes the handle/size.
	 *
	 * @param[in] mem Parameters used to request memory te_mem.
	 * @param[in] rio_address For documentation purposes -- passed in from RSKTD
	 * @param[in] handle Physical memory address mapped as IBwin
	 * @param[in] size number of bytes to allocate. 0 is illegal.
	 *                 Will be rounded up to at least the next power of
	 *                 2.
	 * \note This is done via kernel "memmap=" memory area.
	 * return false - memory allocated successfully.
	 * 	  true - memory could not be allocated.
	 * 	  	Check errno to determine reason for failure.
	 */
	virtual bool alloc_ibwin_fixd(DmaMem_t& mem /*out*/, const uint64_t rio_address, const uint64_t handle, const int size) = 0;

	// TODO: UMD impl will overload alloc_mem and throw an error
	/** \brief allocate buffer in virtual memory.  Memory allocated with this routine
	 * can be used by MEMOPS_MPORT but not by other drivers. */
	virtual bool alloc_umem(DmaMem_t& mem /*out*/, const int size) {
		if (size < 1) return false;
		memset(&mem, 0, sizeof(mem));
		if ((mem.win_ptr = calloc(1, size)) == NULL) return false;
		mem.type = MALLOC;
		return true;
	}

	/** \brief All buffer types are release/freed by this routine.
	 */
	virtual void free_xwin(DmaMem_t& mem) {
		switch (mem.type) {
			case DMAMEM: free_dmawin(mem); break;
			case IBWIN:  free_ibwin(mem); break;
			case IBWIN_FIXD: free_fixd(mem); break;
			case MALLOC: free(mem.win_ptr); break;
			default: throw std::runtime_error("free_xwin: Invalid type!"); break;
		}
		mem.type = INVALID;
	}

	/** \brief Returns generic abort reason. */
	virtual int getAbortReason() = 0;
	virtual const char* abortReasonToStr(const int dma_abort_reason) = 0;

private:
	virtual bool free_dmawin(DmaMem_t& mem) = 0;
	virtual bool free_ibwin(DmaMem_t& mem) = 0;
	virtual bool free_fixd(DmaMem_t& mem) = 0;
};

RIOMemOpsIntf* RIOMemOps_classFactory(const MEMOPSAccess_t type, const int mport, const int channel = -1);

#define ANY_CHANNEL	-1

/** \brief Create a RIOMemOpsIntf, if possible
 * \note Channel 7 is reserved for use by the kernel. Attempting to use it will throw
 * \verbatim
Following combinations are valid:
- !shared + CHAN# - UMD	=> care a lot about performance, so no malloc’d buffers and UMD
 - Shared + ANY_CHANNEL: 
 - Shared + CHAN# - ShM UMD => want guaranteed performance for an application/facility, so no malloc’ed buffers and ShM UMD
\endverbatim
 * \param[in] mport_id Mport id; use \ref MportMgmt::get_mport_list to discover valid ids
 * \param[in] shared is application willing to share channel?
 * \param[in] channel ANY_CHANNEL or 1..7
 * \return NULL if UMD channel in use OR UMDd/SHM not running for channel, an instace of \ref RIOMemOpsIntf otherwise
 */
RIOMemOpsIntf* RIOMemOpsChanMgr(uint32_t mport_id, bool shared, int channel);

#endif // __MEMOPS_H__
