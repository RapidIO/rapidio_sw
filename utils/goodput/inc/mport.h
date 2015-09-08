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

#ifndef __MPORT_H__
#define __MPORT_H__

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <arpa/inet.h> // for htons

#include <map>
#include <stdexcept>
#include <errno.h>
#include <string>

#include "pciebar.h"
#include "local_endian.h"

#include "liblog.h"
#include "IDT_Tsi721.h"
#include "tsi721_dma.h"

#include "rapidio_mport_dma.h"
#include "rapidio_mport_mgmt.h"
#include "rapidio_mport_sock.h"

#include "debug.h"

/** \brief A class to encapsulate mport_cdev and low level DMA buffer ops */
class RioMport {
public:
	static const int DMA_CHAN_COUNT  = 8;
	static const int MBOX_CHAN_COUNT = 8;

	typedef enum { IBWIN = 1, DMAMEM = 2 } DmaMemType_t;

	typedef struct {
		DmaMemType_t type;
		uint64_t     rio_address;
		uint64_t     win_handle; ///< physical mem address
		void*        win_ptr;    ///< mmaped   mem address
		uint32_t     win_size;
	} DmaMem_t;

	RioMport(const int mportid);
	RioMport(const int mportid, riomp_mport_t mp_h_in);
	~RioMport()
	{
		delete m_bar0;
		if (is_my_mport_handle)
			riomp_mgmt_mport_destroy_handle(&mp_h);
	}

	int getDeviceId() { return m_props.hdid; }
	uint8_t getLinkSpeed() { return m_props.link_speed; }

	#define rd32(o) _rd32((o), #o)
	/** \brief Read a 32 bit mmaped register
	* \param offset Tsi721 register to read
	* \return contents of register
	*/
	inline uint32_t _rd32(const uint32_t offset, const char* offset_str)
	{
		uint32_t ret = le32(*((volatile uint32_t *)
					((uint8_t *)m_bar0_base_ptr + offset)));
		INFO("\n\trd32 offset %s (0x%x) = 0x%x\n", offset_str, offset, ret);
		return ret;
	}

	inline uint32_t __rd32(const uint32_t offset)
	{
		uint32_t ret = le32(*((volatile uint32_t *)
					((uint8_t *)m_bar0_base_ptr + offset)));
		INFO("\n\trd32 offset (0x%x) = 0x%x\n", offset, ret);
		return ret;
	}

	#define wr32(o, d) _wr32((o), #o, (d), #d)
	/** \brief Write a 32 bit mmaped register
	* \param offset Tsi721 register to write
	* \param data data to be written to register
	*/
	inline void _wr32(const uint32_t offset, const char* offset_str,
				const uint32_t data, const char* data_str)
	{
		INFO("\n\twr32 offset %s (0x%x) :=  %s (0x%x)\n",
				offset_str, offset, data_str, data);
		*((volatile uint32_t *)
			((uint8_t *)m_bar0_base_ptr + offset)) = le32(data);
	}

	inline void     __wr32(const uint32_t offset, const uint32_t data)
	{
		INFO("\n\twr32 offset 0x%x := 0x%x\n", offset, data);
		*((volatile uint32_t *)
			((uint8_t *)m_bar0_base_ptr + offset)) = le32(data);
	}


	/** \brief Read a 32 bit DMA channel mmaped register
	* \throws std::runtime_error
	* \param chan DMA channel 0..7 to read
	* \param offset Tsi721 DMA register to read
	* \return contents of register
	*/
	inline uint32_t __rd32dma(const uint32_t chan, const uint32_t offset)
	{
		if (chan >= DMA_CHAN_COUNT)
  			throw std::runtime_error("RioMport: Invalid DMA channel to read!");
		return __rd32(TSI721_DMAC_BASE(chan) + offset);
	}

	/** \brief Write a 32 bit DMA channel mmaped register
	 * \throws std::runtime_error
	 * \param chan DMA channel 0..7 to write
	 * \param offset Tsi721 DMA register to write
	 * \param data data write
	 * \return contents of register
	 */
	inline void __wr32dma(const uint32_t chan, const uint32_t offset,
							const uint32_t data)
	{
		if (chan >= DMA_CHAN_COUNT)
			throw std::runtime_error(
				"RioMport: Invalid DMA channel to write!");
		__wr32(TSI721_DMAC_BASE(chan) + offset, data);
	}

	bool map_ibwin(const uint32_t size, DmaMem_t& ibwin);
	bool unmap_ibwin(DmaMem_t& ibwin);
	bool check_ibwin_reg();

	int lcfg_read(uint32_t offset, uint32_t size, uint32_t* data);
	int lcfg_read_u32(uint32_t offset, uint32_t* data);

	bool map_dma_buf(uint32_t size, DmaMem_t& mem);
	bool unmap_dma_buf(DmaMem_t& mem);
	bool check_dma_buf(DmaMem_t& mem);

private:
	int is_my_mport_handle; ///< 0 if shared handle, 1 if private handle
	int m_portid; 		///< mport_cdev port ID
	riomp_mport_t mp_h;	///< mport handle
	struct riomp_mgmt_mport_properties  m_props;
	void* m_bar0_base_ptr;
	int m_bar0_size;
	PCIeBAR *m_bar0;

	std::map <uint64_t, DmaMem_t> m_dmaibwin_reg; ///< registry of ibwin allocated by this instance
	std::map <uint64_t, DmaMem_t> m_dmatxmem_reg; ///< registry of CMA memory allocated by this instance
};

#endif // __MPORT_H__
