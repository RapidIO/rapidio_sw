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
#include <stdint.h>
#include <errno.h>

#include "did.h"

#ifdef __cplusplus
extern "C" {
#endif

struct did_storage {
	uint16_t inuse;
	uint16_t did;
};

/* 255 is reserved, 1 - 254 are available for use */
#define NUMBER_OF_IDS 256

struct did_storage ids[NUMBER_OF_IDS];
uint16_t idx = 0;

/**
 * Get the next available device Id
 * @param[out] did the device Id
 * @retval 0 the device Id is valid
 * @retval -EINVAL the provided did reference is NULL
 * @retval -ENOBUFS there were no device Ids available
 */
int get_did(uint16_t *did) {

	int i;

	if (NULL == did) {
		return -EINVAL;
	}

	// lazy initialization
	if (0 == idx) {
		for (i = 0; i < NUMBER_OF_IDS; i++) {
			ids[i].inuse = 0;
			ids[i].did = i;
		}
		idx = 1;
	}

	// find the next available did from the last used position
	for (i = idx; i < NUMBER_OF_IDS-1; i++) {
		if (0 == ids[i].inuse) {
			idx = i;
			goto found;
		}
	}

	// look for a free did from the beginning of the structure
	for (i = 1; i < idx; i++) {
		if (0 == ids[i].inuse) {
			idx = i;
			goto found;
		}
	}

	// no available ids
	*did = 0;
	return -ENOBUFS;

found:
	// return current, then incr to next available
	ids[idx].inuse = 1;
	*did = idx++;
	return 0;
}

/**
 * Set the specified device Id
 * @param did the device Id
 * @retval 0 the device Id is valid
 * @retval -ENOTUNIQ the specified device Id was already in use
 */
int set_did(uint16_t did) {
	if (0 == ids[did].inuse) {
		// do not update the index
		ids[did].inuse = 1;
		return 0;
	}
	return -ENOTUNIQ;
}

/**
 * Release a device Id
 * @param[in] did the device Id
 * @retval 0 the device Id is valid
 * @retval -EKEYEXPIRED the provided device Id was not in use
 */
int release_did(uint16_t did) {
	if (0 == ids[did].inuse) {
		return -EKEYEXPIRED;
	}

	ids[did].inuse = 0;
	return 0;
}


#ifdef __cplusplus
}
#endif
