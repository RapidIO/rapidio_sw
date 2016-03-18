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

#ifndef __PSHM2_H__
#define __PSHM2_H__

#include <sys/stat.h>        /* For mode constants */
#include <sys/types.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <signal.h>
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/** \file pshm.h Definition of POSIX shared memory wrapper class */

#include <string>
#include <sstream>
#include <stdexcept>

#include "psem.h"

/** \brief Locked wrapper around POSIX shared memory
 * Uses an instance of \ref POSIXSem for locking
 * \note This is a singleton (based on name) across multiple processes.
 */
class POSIXShm2 {
public:
  POSIXShm2(const char* name, const uint64_t size, bool& first_opener);
  ~POSIXShm2();

  void* getMem()        { return (char*)m_shm + PAGE_SIZE; }
  int   getMemSize()    { return m_shm_size; }
  const char* getName() { return m_shm_name.c_str(); }
  static std::string mkname(const char* name);
  static void unlink(const char* name);

  void dumpRegistry(std::string& out);

  void lock()   { m_mutex->lock(); ((ProcRegistry_t*)m_shm)->locker = MYSELF; }
  void unlock() { ((ProcRegistry_t*)m_shm)->locker = NOBODY; m_mutex->unlock(); }

private:
  ///< Per-client record
  typedef struct {
    pid_t pid; ///< Process ID
    pid_t tid; ///< Thread ID (LWP) -- this is not the same as pthread_t
  } ProcRecord_t;

  static const uint32_t REGISTRY_SIG = 0xf00dfabeL;

  ///< Process (clients') registry
  typedef struct {
    volatile uint32_t sig;
    volatile uint32_t count;
    uint32_t          max_count;
    ProcRecord_t      locker;
    ProcRecord_t      reg[0];
  } ProcRegistry_t;

  int         PAGE_SIZE;
  uint64_t    MAP_SIZE;  ///< True size of shm mapping
  int         m_shm_fd;
  uint64_t    m_shm_size;
  void*       m_shm;
  std::string m_shm_name;
  POSIXSem*   m_mutex;   ///< For access to the secret users' Registry
  uint32_t    m_reg_index; ///< Index of this instance in the SHM registry

  ProcRecord_t MYSELF, NOBODY;
};

#ifdef __cplusplus
extern "C" {
#endif

void* POSIXShm2_new(const char* name, const int size, int* first_opener);
void POSIXShm2_delete(void* shm);
void* POSIXShm2_getMem(void* shm);
void POSIXShm2_lock(void* shm);
void POSIXShm2_unlock(void* shm);

#ifdef __cplusplus
};
#endif

#endif // __PSHM2_H__
