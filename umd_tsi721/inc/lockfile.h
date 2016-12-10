#ifndef __LOCKFILE_H__
#define __LOCKFILE_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <stdexcept>

class LockFile {
private:
  int m_fd;
	int rc;

  /** \brief Private gettid(2) implementation */
  static inline pid_t gettid() { return syscall(__NR_gettid); }

public:
  ~LockFile() { close(m_fd); }
  LockFile(const char* path)
  {
    if(path == NULL || path[0] == '\0') throw std::logic_error("LockFile: Invalid argument!");

    m_fd = open(path, O_WRONLY|O_NOFOLLOW|O_CLOEXEC|O_CREAT, 0600);
    if(m_fd == -1)
      throw std::runtime_error("LockFile: Unable to open the lock file!");

    static struct flock lock; memset(&lock, 0, sizeof(lock));

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_pid = getpid();

    int ret = fcntl(m_fd, F_SETLK, &lock);
    if (ret) {
      close(m_fd);
      throw std::runtime_error("LockFile: Lock file taken!");
    }

    char tmp[81] = {0};
    snprintf(tmp, 80, "Locker pid %d tid %d", getpid(), gettid());
    rc = write(m_fd, tmp, strlen(tmp));
    if (strlen(tmp) != (unsigned)rc) {
      close(m_fd);
      throw std::runtime_error("LockFile: write returned unexpected value!");
    }
  }
};

#endif // __LOCKFILE_H__
