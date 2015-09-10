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
#include <sys/mman.h>
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
#include <assert.h>

#include <string>
#include <sstream>
#include <stdexcept>

#include "psem.h"
#include "pshm.h"

/** \file pshm.cc Implementation of POSIX shared memory wrapper class */

/** \brief Private gettid(2) implementation */
static inline pid_t gettid() { return syscall(__NR_gettid); }

/** \brief Makes a name conforming with sem_open(3) */
std::string
POSIXShm::mkname(const char* name)
{
  if(name == NULL || name[0] == '\0')
    throw std::runtime_error("Invalid semaphore name!");

  std::stringstream ss;
  ss << "/shm." << name;

  return ss.str();
}

/** \brief Create a named POSIX SHM area of size 
 * \throws std::runtime_error
 * The size is rounded up to the next full page. Also a secret page is tacked before the "official" memory area.
 */
POSIXShm::POSIXShm(const char* name, const int size)
{
  m_shm_name = mkname(name);

  if(size < 1) throw std::runtime_error("Invalid mapping size!");

  PAGE_SIZE = sysconf(_SC_PAGESIZE);

  memset(&NOBODY, 0, sizeof(NOBODY));

  m_shm_size = size; // for record-keeping

  const int shm_sz = PAGE_SIZE+size;

  m_mutex = new POSIXSem(name);

  int m_shm_fd = shm_open(m_shm_name.c_str(), O_RDWR | O_CREAT | O_EXCL,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if (m_shm_fd < 0) {
    if (EEXIST != errno) {
      static char shmerr[256] = {0}; // YUCK for throwing exceptions
      snprintf(shmerr, sizeof(shmerr)-1, "POSIXShm: shm_open error: %s", sys_errlist[errno]);
      throw std::runtime_error(shmerr);
    }

    m_shm_fd = shm_open(m_shm_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  }

  int k = shm_sz / PAGE_SIZE;
  if((shm_sz % PAGE_SIZE > 0)) k++;
  k++; // our crap

  MAP_SIZE = k*PAGE_SIZE;

  if(ftruncate(m_shm_fd, MAP_SIZE) < 0) {
    shm_unlink(m_shm_name.c_str());
    static char shmerr[256] = {0}; // YUCK for throwing exceptions
    snprintf(shmerr, sizeof(shmerr)-1, "POSIXShm: ftruncate error: %s", sys_errlist[errno]);
    throw std::runtime_error(shmerr);
    exit(1);
  };

  m_shm = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, m_shm_fd, 0);
  if(m_shm == NULL) {
    shm_unlink(m_shm_name.c_str());
    throw std::runtime_error("mmap failed!");
  }

  MYSELF.pid = getpid(); MYSELF.tid = gettid();

  ProcRegistry_t* reg = (ProcRegistry_t*)m_shm;

  m_mutex->lock();
  do {
    if(reg->sig != REGISTRY_SIG) { // 1st time
      memset(m_shm, 0, MAP_SIZE);
      reg->sig              = REGISTRY_SIG;
      reg->locker           = MYSELF;
      reg->max_count        = (PAGE_SIZE - sizeof(ProcRegistry_t)) / sizeof(ProcRecord_t);
      m_reg_index           = reg->max_count - 1;
      reg->reg[m_reg_index] = MYSELF;
      reg->count            = 1;
      break;
    }
    // Hunt down an empty slot, in effect count backwards from last entry!!
    int found = 0;
    for(int i = 1; i < reg->max_count; i++) {
      if(reg->reg[i].pid == 0 && reg->reg[i].tid == 0)         { found = i; }
      if(reg->reg[i].pid == MYSELF.pid && reg->reg[i].tid == MYSELF.tid) { found = -i; }
    }
    if(found == 0) {
      m_mutex->unlock();
      throw std::runtime_error("Registry is full!");
    }
    if(found < 0) {
      m_mutex->unlock();
      throw std::runtime_error("Duplicate registry entry!");
    }

    m_reg_index = found;
    reg->reg[m_reg_index] = MYSELF;
    reg->count++;
  } while(0);
  reg->locker = NOBODY;
  m_mutex->unlock();
}

POSIXShm::~POSIXShm()
{
  ProcRegistry_t* reg = (ProcRegistry_t*)m_shm;

  m_mutex->lock();
  ((ProcRegistry_t*)m_shm)->locker = MYSELF;
  reg->reg[m_reg_index].pid = 0;
  reg->reg[m_reg_index].tid = 0;
  reg->count--;
  ((ProcRegistry_t*)m_shm)->locker = NOBODY;
  m_mutex->unlock();

  munmap(m_shm, MAP_SIZE);

  delete m_mutex;
}

/** \brief Remove from disk a named POSIX SHM */
void POSIXShm::unlink(const char* name)
{
  std::string shm_path = "/dev/shm";
  std::string shm_name = mkname(name);
  shm_path.append(shm_name);
  unlink(shm_path.c_str());
}

void POSIXShm::dumpRegistry(std::string& out)
{
  std::stringstream ss;

  ss << "Name: " << getName() << " Size: " << getMemSize() << " TotalSize: " << MAP_SIZE << "\n";

  ProcRegistry_t* reg = (ProcRegistry_t*)m_shm;
  m_mutex->lock();
  ((ProcRegistry_t*)m_shm)->locker = MYSELF;
  ss << "Registry count: " << reg->count << "\n";
  for(int i = 0; i < reg->max_count; i++) {
    if(reg->reg[i].pid == 0) continue;
    ss << "  [" << i <<"] pid = " << reg->reg[i].pid << " tid = " << reg->reg[i].tid;
    kill(reg->reg[i].pid, 0) == 0 || ss << " DEAD";
    ss << "\n";
  }
  ((ProcRegistry_t*)m_shm)->locker = NOBODY;
  m_mutex->unlock();

  out = ss.str();
}

extern "C" void* POSIXShm_new(const char* name, const int size)
{
  try {
    POSIXShm* shm = new POSIXShm(name, size);
    return shm;
  } catch(std::runtime_error e) { fprintf(stderr, "Exception: %s\n", e.what()); }

  return NULL;
}
extern "C" void POSIXShm_delete(void* shm)
{
  assert(shm != NULL);

  POSIXShm* shm_ = (POSIXShm*)shm;
  delete shm_;
}
extern "C" void* POSIXShm_getMem(void* shm)
{
  assert(shm != NULL);
  POSIXShm* shm_ = (POSIXShm*)shm;
  return shm_->getMem();
}
extern "C" void POSIXShm_lock(void* shm)
{
  assert(shm != NULL);
  POSIXShm* shm_ = (POSIXShm*)shm;
  return shm_->lock();
}
extern "C" void POSIXShm_unlock(void* shm)
{
  assert(shm != NULL);
  POSIXShm* shm_ = (POSIXShm*)shm;
  return shm_->unlock();
}

#ifdef TEST_SHM
int main()
{
  try {
    POSIXShm shm("/", 5000);
  } catch(std::runtime_error e) { fprintf(stderr, "Exception: %s\n", e.what()); }

  try {
    POSIXShm shm("xxx", 0);
  } catch(std::runtime_error e) { fprintf(stderr, "Exception: %s\n", e.what()); }

  try {
    POSIXShm shm("mumma", 5000);

    std::string s;
    shm.dumpRegistry(s);
    puts(s.c_str());
    write(STDOUT_FILENO, "Enter:", 6);
    int c;
    read(STDIN_FILENO, &c, 1);
  } catch(std::runtime_error e) { fprintf(stderr, "Exception: %s\n", e.what()); }

  return 0;
}
#endif
