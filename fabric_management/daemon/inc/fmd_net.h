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

#include "rio_ecosystem.h"
#include "riocp_pe.h"
#include "cfg.h"

#ifndef _FMD_NET_H_
#define _FMD_NET_H_

#ifdef __cplusplus
extern "C" {
#endif

#define AUTO_NAME_PREFIX "rio_dev_"

int fmd_traverse_network(riocp_pe_handle mport_pe, struct cfg_dev *c_dev);
int fmd_traverse_network_from_pe_port(riocp_pe_handle pe, rio_port_t port_num, struct cfg_dev *c_dev);
int fmd_enable_all_endpoints(riocp_pe_handle mport_pe);

#ifdef __cplusplus
}
#endif

#endif /* _FMD_NET_H_ */
