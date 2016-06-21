/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, Prodrive Technologies
Copyright (c) 2014, RapidIO Trade Association

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

THIS SOFTWARE IS PROVENDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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
/* Definitions for the RapidIO ecosystem, including vendors/devices and 
 * device port/lane limits.
 */

#ifndef __REGRW_DEVICES_H__
#define __REGRW_DEVICES_H__

#include <stdint.h>
#include "rio_standard.h"
#include "rio_ecosystem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Devices must hook into this routine in order to fill in
 * the handle with their specific information.
 *
 * Driver must respect the regrw.h REGRW_USE_MALLOC flag.
 * If REGRW_USE_MALLOC is defined, then an OS-specific memory allocation
 * facility is called.
 *
 * If REGRW_USE_MALLOC is not defined, then some number of statically 
 * allocated structures may be allocated/managed.
 */

int regrw_fill_in_handle(regrw_i *h, RIO_DEV_IDENT_T vend_devi);

/** \brief Driver is responsible for updaing driver specific information
 * based on data written.
 */
int regrw_update_h_info_on_write(regrw_i *h, uint32_t offset, uint32_t data);

/** \brief Driver may provide cached information without performing a
 * register read.
 */
int regrw_get_info_from_h_on_read(regrw_i *h, uint32_t offset, uint32_t *data);


#ifdef __cplusplus
}
#endif

#endif /* __REGRW_DEVICES__ */

