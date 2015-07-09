/* Fabric Management Daemon Device Directory Library for applications */
/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef _LIBFMDD_H_
#define _LIBFMDD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FMDD_FLAG_NOK  0x00
#define FMDD_FLAG_OK   0x01
#define FMDD_FLAG_MP   0x02
#define FMDD_RSVD_FLAG 0x04
#define FMDD_RSKT_FLAG 0x08
#define FMDD_RDMA_FLAG 0x10
#define FMDD_APP1_FLAG 0x20
#define FMDD_APP2_FLAG 0x40
#define FMDD_APP3_FLAG 0x80

#define FMDD_FLAG_OK_MP (FMDD_FLAG_OK | 2)

#define FMDD_NO_FLAG   0x00
#define FMDD_ANY_FLAG  0xFF

typedef void *fmdd_h;
fmdd_h fmdd_get_handle(char *my_name, uint8_t flag);
void fmdd_destroy_handle(fmdd_h *dd_h);


uint8_t fmdd_check_ct(fmdd_h h, uint32_t ct, uint8_t flag); /* OK if > 0 */
uint8_t fmdd_check_did(fmdd_h h, uint32_t did, uint8_t flag); /* OK if > 0 */

/* Blocks until the device list changes, or a requested flag changes
* for at least one node.
*/
int fmdd_wait_for_dd_change(fmdd_h h);
int fmdd_get_did_list(fmdd_h h, uint32_t *did_list_sz, uint32_t **did_list);
int fmdd_free_did_list(fmdd_h h, uint32_t **did_list);

void fmdd_bind_dbg_cmds(void *fmdd_h);

#ifdef __cplusplus
}
#endif

#endif /* _LIBFMDD_H_ */
