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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "ct.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 - UINT16_MAX are available for use */
#define NUMBER_OF_CTS (UINT16_MAX+1)

bool ct_ids[NUMBER_OF_CTS];
uint32_t ct_idx = 1;

/**
 * Get the next available component tag
 * @param[out] ct the component tag
 * @retval 0 the component tag is valid
 * @retval -EINVAL the provided ct reference is NULL
 * @retval -ENOBUFS there were no component tags available
 */
int get_ct(uint32_t *ct)
{
	uint16_t did;
	uint32_t i;

	if (NULL == ct) {
		return -EINVAL;
	}

	// find the next available nr from the last used position
	for(i = ct_idx; i < NUMBER_OF_CTS; i++) {
		if(false == ct_ids[i]) {
			ct_idx = i;
			goto found;
		}
	}

	// look for a free nr from the beginning of the structure
	for(i = 1; i < ct_idx; i++) {
		if(false == ct_ids[i]) {
			ct_idx = i;
			goto found;
		}
	}

	// no available cts
	*ct = COMPTAG_UNSET;
	return -ENOBUFS;

found:
	if(0 == get_did(&did)) {
		// return current, then incr to next available
		ct_ids[ct_idx] = true;
		*ct = ((ct_idx << 16) & 0xffff0000) | (0x0000ffff & did);
		ct_idx++;
		return 0;
	}

	// no available cts
	*ct = COMPTAG_UNSET;
	return -ENOBUFS;
}

/**
 * Set the specified component tag
 * @param[out] ct the component tag
 * @param[in] nr the index value of the component tag
 * @param[in] did the device Id value of the component tag
 * @retval 0 the component tag is valid
 * @retval -EINVAL invalid parameter values
 * @retval -ENOTUNIQ the specified component tag was already in use
 */
int set_ct(uint32_t *ct, uint16_t nr, uint16_t did)
{
	if (NULL == ct) {
		return -EINVAL;
	}

	if((0 == nr) || (0 == did) || (255 == did)) {
		*ct = COMPTAG_UNSET;
		return -EINVAL;
	}

	if(ct_ids[nr]) {
		*ct = COMPTAG_UNSET;
		return -ENOTUNIQ;
	}

	if(0 != set_did(did)) {
		*ct = COMPTAG_UNSET;
		return -ENOTUNIQ;
	}

	// do not incr ct_idx
	ct_ids[nr] = true;
	*ct = ((nr << 16) & 0xffff0000) | (0x0000ffff & did);
	return 0;
}

/**
 * Return the destination id of the component tag
 * @param[in] the component tag
 * @retval the destination id
 */
uint16_t get_destid(uint32_t ct) {
	return (uint16_t) (0x0000ffff & ct);
}

/**
 * Return the nr of the component tag
 * @param[in] the component tag
 * @retval the nr
 */
uint16_t get_nr(uint32_t ct) {
	return (uint16_t) (0x0000ffff & (ct >> 16));
}

#ifdef __cplusplus
}
#endif
