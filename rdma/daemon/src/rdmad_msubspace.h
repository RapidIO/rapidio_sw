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
#ifndef MSUBSPACE_H
#define MSUBSPACE_H

#include <stdint.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "liblog.h"
#include "libcli.h"


class msubspace
{
public:
	/* Constructor */
	msubspace(uint32_t msid, uint64_t rio_addr, uint64_t phys_addr,
					uint32_t size, uint32_t msubid) :
		msid(msid), rio_addr(rio_addr), phys_addr(phys_addr),
		size(size), msubid(msubid)
	{
		INFO("msid = 0x%X, rio_addr = 0x%" PRIx64 ", size = 0x%08X\n",
							msid, rio_addr, size);
		INFO("msubid = 0x%X, phys_addr = 0x%" PRIx64 "\n", msubid, phys_addr);
	}

	/* For finding a memory sub-space by its msubid */
	bool operator==(uint32_t msubid) {
		return this->msubid == msubid;
	}

	void dump_info(struct cli_env *env) {
		sprintf(env->output, "%08X %016" PRIx64 " %08X %08X %016" PRIx64
				"\n", msubid, rio_addr, size, msid, phys_addr);
		logMsg(env);
	}

private:
	uint32_t	msid;
	uint64_t	rio_addr;
	uint64_t	phys_addr;
	uint32_t	size;
	uint32_t	msubid;
};

#endif


