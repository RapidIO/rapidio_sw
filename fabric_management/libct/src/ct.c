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
#include <errno.h>

#include "ct.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 - UINT16_MAX are available for use */
#define NUMBER_OF_CTS (UINT16_MAX+1)

struct dt_storage {
	uint16_t inuse;
	uint16_t nr;
};

struct dt_storage ids[NUMBER_OF_CTS];
uint32_t idx = 0;

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

	// lazy initialization
	if(0 == idx) {
		for(i = 0; i < NUMBER_OF_CTS; i++) {
			ids[i].inuse = 0;
			ids[i].nr = i;
		}
		idx = 1;
	}

	// find the next available nr from the last used position
	for(i = idx; i < NUMBER_OF_CTS; i++) {
		if(0 == ids[i].inuse) {
			idx = i;
			goto found;
		}
	}

	// look for a free nr from the beginning of the structure
	for(i = 1; i < idx; i++) {
		if(0 == ids[i].inuse) {
			idx = i;
			goto found;
		}
	}

	// no available cts
	*ct = COMPTAG_UNSET;
	return -ENOBUFS;

found:
	if(0 == get_did(&did)) {
		// return current, then incr to next available
		ids[idx].inuse = 1;
		*ct = ((idx << 16) & 0xffff0000) | (0x0000ffff & did);
		idx++;
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

	if(1 == ids[nr].inuse) {
		*ct = COMPTAG_UNSET;
		return -ENOTUNIQ;
	}

	if(0 != set_did(did)) {
		*ct = COMPTAG_UNSET;
		return -ENOTUNIQ;
	}

	// do not incr idx
	ids[nr].inuse = 1;
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
