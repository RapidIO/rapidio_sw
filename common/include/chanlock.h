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

#ifndef __CHANLLOCK_H__
#define __CHANLLOCK_H__

#include <stdio.h>
#include <stdexcept>

#include "lockfile.h"


/** \brief Lock other processes out of this UMD module/channel */
class ChannelLock {
public:
  /** \brief Static locker function
   * \note Due to POSIX locking semantics this has no effect on the current process
   * \note Using the same channel twice in this process will NOT be prevented
   * \param[in] module DMA or Mbox, ASCII string
   * \param instance Channel number
   * \return a pointer to LockFile, will throw std::runtime_error, std::logic_error on error
   */
  static inline LockFile* TakeLock(const char* module, const int mport, const int instance)
  {
    if (module == NULL || module[0] == '\0' || mport < 0 || instance < 0)
      throw std::runtime_error("ChannelLock::TakeLock: Invalid parameter!");

    LockFile* lock = NULL;
    char lock_name[81] = {0};
    snprintf(lock_name, 80, "/var/lock/UMD-%s-%d:%d..LCK", module, mport, instance);

    lock = new LockFile(lock_name);

    // NOT catching std::logic_error

    return lock;
  }
};

#endif // __CHANLLOCK_H__
