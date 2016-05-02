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

#include <memops.h>
#include <memops_mport.h>
#include <memops_umdd.h>
#include <memops_umd.h>

RIOMemOpsIntf* RIOMemOps_classFactory(const MEMOPSAccess_t type, const int mport, const int channel)
{
  if (mport < 0) throw std::runtime_error("RIOMemOps_classFactory: Invalid mport!");

  switch (type) {
    case MEMOPS_MPORT: return new RIOMemOpsMport(mport); break;
    case MEMOPS_UMDD: if (channel < 2 || channel > 7) // Chan 0 used by kern
                      throw std::runtime_error("RIOMemOps_classFactory: Invalid channel!");
                    return new RIOMemOpsUMDd(mport, channel);
                    break;
    case MEMOPS_UMD: return new RIOMemOpsUMD(mport, channel); break;
    default: throw std::runtime_error("RIOMemOps_classFactory: Invalid access type!"); break;
  }

  throw std::runtime_error("RIOMemOps_classFactory: BUG");

  /*NOTREACHED*/
  return NULL;
}

