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
#include <pthread.h>

#include <map>
#include <stdexcept>

#include "lockfile.h"

class ChannelLock;

/** \brief This is a sub-class of \ref ChannelLock
 * \note Unlike Java this must be declared side-by side
 */
class LockChannel {
private:
  friend class ChannelLock;
  LockChannel(LockFile* lockf, const char* module, uint32_t mportid, uint32_t instance) :
    m_lockf(lockf), m_module(module), m_mportid(mportid), m_instance(instance)
  {}

public:
  ~LockChannel(); // Not coded here to keep g++/ld happy!

private:
  LockFile*   m_lockf;
  std::string m_module;
  uint32_t    m_mportid;
  uint32_t    m_instance;
};

/** \brief Lock other processes out of this UMD module/channel
 * \note This is a process-singleton
 */
class ChannelLock {
public:
  /** \brief Static locker function
   * \note Due to POSIX locking semantics it has no effect on the current process
   * \note Using the same channel twice in this process will be prevented via singleton registry apparatus
   * \param[in] module DMA or Mbox, ASCII string
   * \param instance Channel number
   * \return a pointer to LockFile, will throw std::runtime_error, std::logic_error on error
   */
  static inline LockChannel* TakeLock(const char* module, const uint32_t mport,  const uint32_t instance)
  {
    if (module == NULL || module[0] == '\0' || mport < 0 || instance < 0)
      throw std::runtime_error("ChannelLock::TakeLock: Invalid parameter!");

    ChannelLock::getInstance()->TakeLockInproc(module, mport, instance);
    // NOT catching std::logic_error

    LockFile* lock = NULL;
    char lock_name[81] = {0};
    snprintf(lock_name, 80, "/var/lock/UMD-%s-%u:%u..LCK", module, mport, instance);

    try { lock = new LockFile(lock_name); }
    catch(std::logic_error ex) {
      ChannelLock::getInstance()->ReleaseLockInproc(module, mport, instance);
      throw ex;
    }
    // NOT catching std::logic_error

    return new LockChannel(lock, module, mport, instance);
  }

private:
  inline void mutexInit()
  {
    pthread_mutexattr_t mutex_attr;
    memset(&mutex_attr, 0, sizeof(mutex_attr));

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

    memset(&m_mutex, 0, sizeof(m_mutex));
    pthread_mutex_init(&m_mutex, &mutex_attr);
  }

  friend class LockChannel;

  typedef std::string RegistryKey_t;

  static inline RegistryKey_t MakeKey(const char* module, const uint32_t mport,  const uint32_t instance) {
    char key[33] = {0};
    snprintf(key, 32, "%s:%u:%u", module, mport, instance);
    std::string KEY(key);
    return KEY;
  }
  
  inline void TakeLockInproc(const char* module, const uint32_t mport,  const uint32_t instance) {
    return TakeLockInproc(MakeKey(module, mport, instance));
  }
  inline void TakeLockInproc(const RegistryKey_t& key) {
    pthread_mutex_lock(&m_mutex);
      std::map<RegistryKey_t, bool>::iterator it = m_registry.find(key);
      if (it != m_registry.end()) {
        pthread_mutex_unlock(&m_mutex);
        throw std::logic_error("ChannelLock::TakeLockInproc: Channel already used in this process!");
      }
      m_registry[key] = true;
    pthread_mutex_unlock(&m_mutex);
  }

  inline void ReleaseLockInproc(const char* module, const uint32_t mport,  const uint32_t instance) {
    return ReleaseLockInproc(MakeKey(module, mport, instance));
  }
  inline void ReleaseLockInproc(const RegistryKey_t& key) {
    pthread_mutex_lock(&m_mutex);
      std::map<RegistryKey_t, bool>::iterator it = m_registry.find(key);
      if (it != m_registry.end()) m_registry.erase(it);
    pthread_mutex_unlock(&m_mutex);
  }

  ChannelLock() {}; // Keep this private so class cannot be new'ed

  static ChannelLock* getInstance(); // Keep this private so class cannot be instantiated

private:
  static ChannelLock*           m_instance;
  pthread_mutex_t               m_mutex; ///< Registry aaccess, not singleton mutex
  std::map<RegistryKey_t, bool> m_registry;
};

#endif // __CHANLLOCK_H__
