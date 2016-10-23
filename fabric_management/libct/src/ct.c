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
#include "did.h"
#include "rio_standard.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 - UINT16_MAX are available for use */
#define NUMBER_OF_CTS (1 << 16)
#define CT_NR_MASK (0xffff0000)
#define CT_DID_MASK (0x0000ffff)

bool ct_ids[NUMBER_OF_CTS];
uint32_t ct_idx = 1;

#define CT_FROM_NR_DID(n,d) (((n << 16) & 0xffff0000) | (0x0000ffff & d))

/**
 * Find the next free number.  Note that this routine does not reserve the
 * number - it is up to the calling routine to reserve the number and update
 * ct_idx, as necessary.
 * @returns The next valid component tag number.
 * @retval 0 Invalid component tag number, failure
 * @retval -ENOBUFS There were no component tags available
 */
int ct_next_nr(ct_nr_t *nr)
{
	uint32_t i;

	// find the next available nr from the last used position
	for (i = ct_idx; i < NUMBER_OF_CTS; i++) {
		if (false == ct_ids[i]) {
			*nr = (ct_nr_t)i;
			goto found;
		}
	}

	// look for a free nr from the beginning of the structure
	for(i = 1; i < ct_idx; i++) {
		if (false == ct_ids[i]) {
			*nr = (ct_nr_t)i;
			goto found;
		}
	}

	return -ENOBUFS;
found:
	return 0;
}

/**
 * Create a component tag and device Id for the specified device Id size.
 * @param[out] ct the component tag
 * @param[out] did the device Id associated with the component tag
 * @param[in] size the size of the device Id
 * @retval 0 the component tag and device Id are valid
 * @retval -EINVAL the provided ct or did reference is NULL, or no dids available
 * @retval -EPERM the operation is not supported
 * @retval -ENOBUFS there were no component tags available
 */
int ct_create_all(ct_t *ct, did_t *did, did_sz_t size)
{
	ct_nr_t tmp_idx;

	int rc;

	if((NULL == ct) || (NULL == did)) {
		return -EINVAL;
	}

	rc = ct_next_nr(&tmp_idx);
	if (rc) {
		*ct = COMPTAG_UNSET;
		return rc;
	};
	rc = did_create(did, size);
	if(rc) {
		// no available dids
		*ct = COMPTAG_UNSET;
		return rc;
	}

	// return current, increment to next available
	*ct = CT_FROM_NR_DID(tmp_idx, did_get_value(*did));
	ct_ids[tmp_idx] = true;
	ct_idx = ++tmp_idx;
	return 0;
}

/**
 * Create a component tag and device Id from the specified nr and did values
 * @param[out] ct the component tag
 * @param[out] did the device Id associated with the component tag
 * @param[in] nr the index value of the component tag
 * @param[in] did_value the device Id value of the component tag
 * @param[in] did_size the size of the device Id
 * @retval 0 the component tag is valid
 * @retval -EINVAL invalid parameter values
 * @retval -ENOTUNIQ the specified component tag was already in use
 */
int ct_create_from_data(ct_t *ct, did_t *did, ct_nr_t nr,
		did_val_t did_value, did_sz_t did_size)
{
	int rc = did_create_from_data(did, did_value, did_size);
	if(rc) {
		return rc;
	}
	return ct_create_from_nr_and_did(ct, nr, *did);
}

/**
 * Create a component tag from the specified nr and previously created did
 * @param[out] ct the component tag
 * @param[in] nr the index value of the component tag
 * @param[in] did the device Id of the component tag
 * @retval 0 the component tag is valid
 * @retval -EINVAL invalid parameter values
 * @retval -ENOTUNIQ the specified component tag was already in use
 */
int ct_create_from_nr_and_did(ct_t *ct, ct_nr_t nr, did_t did)
{
	if(NULL == ct) {
		return -EINVAL;
	}

	if(0 == nr) {
		*ct = COMPTAG_UNSET;
		return -EINVAL;
	}

	if(ct_ids[nr]) {
		*ct = COMPTAG_UNSET;
		return -ENOTUNIQ;
	}

	// do not incr ct_idx
	ct_ids[nr] = true;
	*ct = ((nr << 16) & 0xffff0000)
			| (0x0000ffff & did_get_value(did));
	return 0;
}

/**
 * Create a component tag for the previously created did
 * @param[out] ct the component tag
 * @param[in] did the device Id for the component tag
 * @retval 0 the component tag is valid
 * @retval -EINVAL invalid parameter values
 * @retval -ENOBUFS No more CTs available
 */
int ct_create_from_did(ct_t *ct, did_t did)
{
	ct_nr_t nr;
	int rc;

	if(NULL == ct) {
		return -EINVAL;
	}

	if (!did_not_inuse(did)) {
		*ct = COMPTAG_UNSET;
		return -EINVAL;
	}

	rc = ct_next_nr(&nr);
	if (rc) {
		*ct = COMPTAG_UNSET;
		return rc;
	}

	ct_ids[nr] = true;
	*ct = CT_FROM_NR_DID(nr, did_get_value(did));
	ct_idx = ++nr;
	return 0;
}

/**
 * Release the provided component tag and the associated device Id
 * @param[out] ct the component tag to be released
 * @param[in] did the device Id associated with the ct
 * @retval 0 the component tag is valid
 * @retval -EINVAL invalid parameter values
 */
int ct_release(ct_t *ct, did_t did)
{
	int rc;

	if(NULL == ct) {
		return -EINVAL;
	}

	rc = did_release(did);
	if(rc) {
		return rc;
	}

	*ct &= CT_NR_MASK;
	return 0;
}

/**
 * Return the nr of the component tag
 * @param[out] nr the nr
 * @param[in] ct the component tag
 * @retval 0 the nr is valid
 * @retval -EINVAL nr is invalid
 */
int ct_get_nr(ct_nr_t *nr, ct_t ct)
{
	if(NULL == nr) {
		return -EINVAL;
	}

	*nr = (ct_nr_t)(0x0000ffff & (ct >> 16));
	return (0 == *nr ? -EINVAL : 0);
}

/**
 * Return the device Id of the component tag
 * @param[out] did the device Id
 * @param[in] ct the component tag
 * @param[in] size the size of the device Id to retrieve
 * @retval 0 the did is valid
 * @retval -EINVAL the did is null or the did of the provided ct is not in use
 */
int ct_get_destid(did_t *did, ct_t ct, did_sz_t size)
{
	return did_get(did, (CT_DID_MASK & ct), size);
}

/**
 * Determine if the specified component tag is in use.
 * @param[in] ct the component tag
 * @param[in] size the size of the device id associated with the component tag
 * @retval 0 if the ct is not in use
 * @retval 1 if the ct is in use
 * @retval -EINVAL nr or did size is invalid
 * @retval -EPERM the operation is not supported
 *
 * \note A component tag with an nr value of 0 is considered not in use
 */
int ct_not_inuse(ct_t ct, did_sz_t size)
{
	ct_nr_t nr;
	did_t did;
	int rc;
	rc = ct_get_nr(&nr, ct);
	if (rc) {
		return rc;
	}

	if (ct_ids[nr]) {
		did.value = ct & CT_DID_MASK;
		did.size = size;
		return did_not_inuse(did);
	}
	return 0;

}

#ifdef __cplusplus
}
#endif
