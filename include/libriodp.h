/*
****************************************************************************
Copyright (c) 2015, Prodrive Technologies  
Copyright (c) 2015, Concurrent Technology Plc 
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

#ifndef RIODP_H_
#define RIODP_H_

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

typedef uint64_t rio_addr_t;

enum riodp_event_type {
	RIODP_EVENT_DOORBELL_RECV,	/* Doorbell received */
};

/* RIO event */
typedef struct riodp_event {
	enum riodp_event_type type;
	union {
		struct {
			uint16_t info;	/* doorbell payload */
		} doorbell;
	} payload;
} riodp_event_t;

/* Opaque pointers */
typedef struct riodp_mport *riodp_mport_t;
typedef struct riodp_endpoint *riodp_endpoint_t;

#define RIODP_WU __attribute__((warn_unused_result))

/******************************************************************************
*
* riodp_mport_create_handle
*
* Parameters:
*
* mport_id - Master Port identifier.
*
* mport    - Opaque handle that needs to be used with all he corresponding
*                       riodp_mport_* APIs.
*
* Description:
*
* Obtains a unique handle  mport for the master port being identified by mport_id.
*
* Returns:
*
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion, a valid handle will be returned in mport parameter.
*
******************************************************************************/
int RIODP_WU riodp_mport_create_handle(uint8_t mport_id, riodp_mport_t *mport);

/******************************************************************************
*
* riodp_mport_destroy_handle
*
* Parameter:
*
*  mport           - Opaque handle obtained from riodp_mport_create_handle API
*
* Description:
*
* Releases the master port handle. 
*
* Returns:
*
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int riodp_mport_destroy_handle(riodp_mport_t *mport);


/******************************************************************************
*
* riodp_mport_mmap_inb
*
* Parameters:
*
* mport         - Opaque handle obtained from riodp_mport_create_handle API
* rio_addr      - RapidIO Address
* size          - Size of the inbound window
* data          - Mapped pointer for data access
*
* Description:
* Open an inbound window and returns a mapped pointer for data access. The
* inbound window maps RapidIO address region with start address and size
* defined by rio_addr and size input parameters to a memory buffer. Mapped
* pointer data returns an user space handle to this mapped memory
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion of this API, the parameter data contains a user
* mapped address to the mapped memory buffer.
******************************************************************************/
int RIODP_WU riodp_mport_mmap_inb(riodp_mport_t mport, rio_addr_t rio_addr,
	rio_addr_t size, void **data);

/******************************************************************************
*
* riodp_mport_munmap_inb
*
* Parameters:
*
* mport         - Opaque handle obtained from riodp_mport_create_handle API
* rio_addr      - RapidIO Address
* size          - Size of the inbound window
* data          - Mapped pointer for data access
*
* Description:
* Closes an inbound window and releases any associated user mapping.
*
* Return:
* This API returns 0 on successfully completion or standard error codes on failure.
******************************************************************************/
int riodp_mport_munmap_inb(riodp_mport_t mport, rio_addr_t rio_addr,
	rio_addr_t size, void **data);

/******************************************************************************
*
* riodp_mport_alloc_dma_buf
*
* Parameter:
* mport         - Opaque handle obtained from riodp_mport_create_handle API
* size          - Size of the DMA buffer to be allocated.
* dma_ptr       - Mapped pointer for data access
*
* Description:
*
* Allocates and maps a DMA capable buffer that can be used with riodp_ep_*_async and
* riodp_ep_*_faf API.
*
* Return:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion of this API, the dma_ptr contains the user mapped address
* to the DMA buffer.
*
******************************************************************************/
int RIODP_WU riodp_mport_alloc_dma_buf(riodp_mport_t mport, rio_addr_t size,
	void **dma_ptr);

/******************************************************************************
*
* riodp_mport_free_dma_buf
*
* Parameter:
* mport         - Opaque handle obtained from riodp_mport_create_handle API
* dma_ptr       - Mapped pointer for data access
*
* Description:
*
* Deallocates DMA capable buffer allocated by  riodp_mport_alloc_dma_buf API
*
* Return:
* This API returns 0 on successfully completion or standard error codes on failure.
*
*
******************************************************************************/
int riodp_mport_free_dma_buf(riodp_mport_t mport, void **dma_ptr);

/******************************************************************************
*
* riodp_mport_get_ep_list
*
* Parameters:
*
* mport   - Opaque handle obtained from riodp_mport_create_handle API
* destids - Array of detected endpoint destination ids on the corresponding
*                   master port
* number_of_eps - Number of element in the destids array
*
*
* Description:
*
* Obtains the list of destination identifier corresponding to endpoint detected
* on the RIO network which the mport is connected to.
*
* Returns:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion, the destid parameter contains an array of number_of_eps
* elements.
*
******************************************************************************/
int RIODP_WU riodp_mport_get_ep_list(riodp_mport_t mport, uint32_t **destids, uint32_t *number_of_eps);

/******************************************************************************
*
* riodp_mport_free_ep_list
*
* Parameters:
*
* mport   - Opaque handle obtained from riodp_mport_create_handle API
* destids - Array of detected endpoint destination ids on the corresponding
*                   master port returned by riodp_mport_get_ep_list
*
*
* Returns:
*
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int riodp_mport_free_ep_list(riodp_mport_t mport,uint32_t **destids);

/******************************************************************************
*
* riodp_ep_create_handle
*
* Parameter:
* mport         - Opaque handle obtained from riodp_mport_create_handle API
* destid        - Destination identifier of the endpoint
* endpoint  - Opaque handle for addressing the endpoint
*
* Description:
* Obtained an opaque handle for the endpoint addressed by destination identifier
* destid.
*
* Returns:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion of the API, endpoint parameter will contain the handle for
* the endpoint addressed by destid input parameter.
*
******************************************************************************/
int RIODP_WU riodp_ep_create_handle(riodp_mport_t mport, uint32_t destid,
	riodp_endpoint_t *endpoint);

/******************************************************************************
*
* riodp_ep_destroy_handle
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
*
* Description:
* Releases the handle allocated by riodp_ep_create_handle
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
******************************************************************************/
int riodp_ep_destroy_handle(riodp_endpoint_t *endpoint);

/******************************************************************************
*
* riodp_ep_recv_event
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* timeout   - Timeout in milli-seconds to wait for an event to occur
* event         - Pointer to a structure of type riodp_event_t which contains
*                         details of the event received.
*
* Description:
*
* Waits for a doorbell event to be received from the corresponding endpoint.
* A timeout value of less than 0 causes the API to wait for an event indefinately.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion of this API, the event parameter will be populated with
* the details of the event received.
*
******************************************************************************/
int RIODP_WU riodp_ep_recv_event(riodp_endpoint_t endpoint,int32_t timeout,riodp_event_t *event);

/******************************************************************************
*
* riodp_ep_get_rio_id
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* destid        - Destination identifier of the endpoint
*
* Description:
*
* Gets the destination identifier of the endpoint.
*
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion of this API, destid will contain the destination identifier
* of the endpoint.
*
******************************************************************************/
int RIODP_WU riodp_ep_get_rio_id(riodp_endpoint_t endpoint, uint32_t *destid);

/******************************************************************************
*
* riodp_ep_mmap
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* rio_addr      - RapidIO Address
* size          - Size of the Outbound window
* data          - Mapped pointer for data access
*
* Description:
* Opens an outbound window and returns a mapped pointer for data access. The
* outbound window maps a memory buffer to RapidIO address region with start
* address and size  defined by rio_addr and size input parameters.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
* On successful completion of this API, the parameter data contains a user
* mapped address to the mapped memory buffer.
*
*
******************************************************************************/
int RIODP_WU riodp_ep_mmap(riodp_endpoint_t endpoint, rio_addr_t rio_addr,
	rio_addr_t size, void **data);

/******************************************************************************
*
* riodp_ep_munmap
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* rio_addr      - RapidIO Address
* size          - Size of the Outbound window
* data          - Mapped pointer for data access
*
* Description:
* Unmaps an outbound window mapping opened by riodp_ep_mmap
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
*
******************************************************************************/
int riodp_ep_munmap(riodp_endpoint_t endpoint, rio_addr_t rio_addr,
	rio_addr_t size, void **data);

/******************************************************************************
*
* riodp_ep_write_sync
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* rio_addr      - RapidIO Address
* size          - length of the DMA transfer
* data          - Mapped pointer for data access
*
* Description:
* Transfers data using DMA engines from user buffer to end point buffer addressed
* by the corresponding handle and RapidIO address.
*
* RETURNS:
*This API returns 0 on successfully completion or standard error codes on failure.

******************************************************************************/
int RIODP_WU riodp_ep_write_sync(riodp_endpoint_t endpoint, const void *data,
	size_t size, rio_addr_t rio_addr);

/******************************************************************************
*
* riodp_ep_read_sync
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* rio_addr      - RapidIO Address
* size          - length of the DMA transfer
* data          - Mapped pointer for data access
*
* Description:
* Transfers data using DMA engines from end point buffer addressed  by the
* corresponding handle and RapidIO address to user buffer.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_read_sync(riodp_endpoint_t endpoint, void *data,
	size_t size, rio_addr_t rio_addr);

/******************************************************************************
*
* riodp_ep_write_async
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* dma_ptr       - Pointer to the data buffer
* dma_offset - Offset into the data buffer
* rio_addr      - RapidIO Address
* size          - Length of the DMA transfer
* token         - Token to track DMA completion
*
* Description:
* Transfers data asynchronously using DMA engines from user buffer to
* end point buffer addressed  by the corresponding handle and RapidIO address.
* A token is returned which can be used with riodp_ep_wait_for_async API
* to track the completion of corresponding DMA transfers.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_write_async(riodp_endpoint_t endpoint,
	const void *dma_ptr, uint64_t dma_offset, size_t size,
	rio_addr_t rio_addr,uint32_t *token );

/******************************************************************************
*
* riodp_ep_read_async
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* dma_ptr       - Pointer to the data buffer
* dma_offset - Offset into the data buffer
* rio_addr      - RapidIO Address
* size          - Length of the DMA transfer
* token         - Token to track DMA completion
*
* Description:
* Transfers data asynchronously using DMA engines from end point buffer addressed
* by the corresponding handle and RapidIO address to user buffer. A token is returned
* which can be used with riodp_ep_wait_for_async API * to track the completion of
* corresponding DMA transfers.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_read_async(riodp_endpoint_t endpoint, void *dma_ptr,
		uint64_t dma_offset, size_t size, rio_addr_t rio_addr, uint32_t *token);

/******************************************************************************
*
* riodp_ep_wait_for_async
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* token         - Token to track DMA completion
* timeout   - Timeout in milliseconds
*
* Description:
* Waits for an asychronous DMA transfer specified by token parameter
* to get completed. The token parameter is an output from the riodp_ep_*_async
* APIs
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_wait_for_async(riodp_endpoint_t endpoint, uint32_t token,uint32_t timeout );

/******************************************************************************
*
* riodp_ep_write_faf
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* dma_ptr       - Pointer to the data buffer
* dma_offset - Offset into the data buffer
* rio_addr      - RapidIO Address
* size          - Length of the DMA transfer
*
*
* Description:
* Transfers data asynchronously without the ability to track the completion
* using DMA engines from user buffer to end point buffer addressed  by the
* corresponding handle and RapidIO address.
*
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_write_faf(riodp_endpoint_t endpoint, const void *dma_ptr,
                uint64_t dma_offset, size_t size, rio_addr_t rio_addr);

/******************************************************************************
*
* riodp_ep_read_faf
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* dma_ptr       - Pointer to the data buffer
* dma_offset - Offset into the data buffer
* rio_addr      - RapidIO Address
* size          - Length of the DMA transfer
*
*
* Description:
* Transfers data asynchronously without the ability to track the completion
* using DMA engines from user buffer to end point buffer addressed  by the
* corresponding handle and RapidIO address.
*
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_read_faf(riodp_endpoint_t endpoint, const void *dma_ptr,
                uint64_t dma_offset, size_t size, rio_addr_t rio_addr);

/******************************************************************************
*
* riodp_ep_db_register_range
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* start         - Doorbell range start
* end           - Doorbell range end
*
* Description:
* Enables reception of a range of doorbell event from the corresponding endpoint.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_db_register_range(riodp_endpoint_t endpoint,
	uint16_t start, uint16_t end);

/******************************************************************************
*
* riodp_ep_db_unregister_range
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* start         - Doorbell range start
* end           - Doorbell range end
*
* Description:
* Disables reception of a range of doorbell event from the corresponding endpoint.
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_db_unregister_range(riodp_endpoint_t endpoint,
	uint16_t start, uint16_t end);

/******************************************************************************
*
* riodp_ep_db_send
*
* Parameters:
* endpoint      - Opaque handle returned by riodp_ep_create_handle API
* value         - Payload for the doorbell
*
* Description:
* Sends a doorbell event with the payload value to the corresponding endpoint
*
* RETURNS:
* This API returns 0 on successfully completion or standard error codes on failure.
*
******************************************************************************/
int RIODP_WU riodp_ep_db_send(riodp_endpoint_t endpoint, uint16_t value);

#endif /* RIODP_H_ */
