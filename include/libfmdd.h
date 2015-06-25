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

typedef void *fmdd_h;
fmdd_h fmdd_get_handle(char *my_name);
void fmdd_destroy_handle(fmdd_h *dd_h);

#define CHK_NOK -1
#define CHK_OK 0
#define CHK_OK_MP 1

int fmdd_check_ct(fmdd_h h, uint32_t ct); /* OK if >= 0 */
int fmdd_check_did(fmdd_h h, uint32_t did); /* OK if >= 0 */
int fmdd_get_did_list(fmdd_h h, uint32_t *did_list_sz, uint32_t **did_list);
int fmdd_free_did_list(fmdd_h h, uint32_t **did_list);
int fmdd_wait_for_dd_change(fmdd_h h); /* Blocks until the DD changes */

void fmdd_bind_dbg_cmds(void *fmdd_h);

#ifdef __cplusplus
}
#endif

#endif /* _LIBFMDD_H_ */
