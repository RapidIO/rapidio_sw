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
