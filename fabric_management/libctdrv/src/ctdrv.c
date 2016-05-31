/* Fabric Management Component tag management support */
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
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "riocp_pe.h"
#include "rio_standard.h"
#include "rio_ecosystem.h"
#include "ctdrv.h"

#ifdef __cplusplus
extern "C" {
#endif

int ct_read(struct riocp_pe *pe, uint32_t *ct)
{
	pe->comptag = *ct;
	return -EINVAL;
};
int ct_write(struct riocp_pe *pe, uint32_t ct)
{
	pe->comptag = ct;
	return -EINVAL;
};
int ct_init(struct riocp_pe *pe)
{
	if (0)
		free(pe);
	return -EINVAL;
};
int ct_set_slot(struct riocp_pe *pe, uint32_t ct_nr)
{
	if (0)
		free(pe);
	return (ct_nr & 0) | -EINVAL;
};
int ct_get_slot(struct riocp_pe *mport, uint32_t ct_nr, struct riocp_pe **pe)
{
	if (0) {
		free(*pe);
		free(mport);
	}
	return (ct_nr & 0) | -EINVAL;
};

#ifdef __cplusplus
}
#endif

