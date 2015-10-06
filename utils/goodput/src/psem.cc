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

#include <semaphore.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <stdexcept>

/** \file psem.cc Implementation of a POSIX name semaphore
 */

#include "psem.h"

/** \brief Makes a name conforming with sem_open(3) */
std::string
POSIXSem::mkname(const char* name)
{
  if(name == NULL || name[0] == '\0')
    throw std::runtime_error("Invalid semaphore name!");

  std::stringstream ss;
  ss << "/" << name << getpid();

  return ss.str();
}

/** \brief Create a named POSIX semaphore
 * \throws std::runtime_error
 */
POSIXSem::POSIXSem(const char* name)
{
  m_semaname = mkname(name);

  if ((m_sem = sem_open(m_semaname.c_str(), O_CREAT, 0644, 1)) == SEM_FAILED) {
    static char semaerr[256] = {0}; // YUCK for throwing exceptions
    snprintf(semaerr, sizeof(semaerr)-1, "POSIXSem: semaphore initilization error: %s", sys_errlist[errno]);
    throw std::runtime_error(semaerr);
  }
}

/** \brief Remove from disk a named POSIX semaphore */
void POSIXSem::unlink(const char* name)
{
  std::string sema_name = mkname(name);
  sem_unlink(sema_name.c_str());
}

POSIXSem::~POSIXSem()
{
  sem_close(m_sem);
}

#ifdef TEST_SEM
int main(int argc, char* argv[])
{
  try {
    POSIXSem s("/"); // only this throws
  } catch(std::runtime_error e) { fprintf(stderr, "Exception: %s\n", e.what()); }

  write(STDOUT_FILENO, "LOCK?\n", 6);

  try {
    POSIXSem s("bubba"); // only this throws
    s.lock();
    write(STDOUT_FILENO, "Enter:", 6);
    int c;
    read(STDIN_FILENO, &c, 1);
    s.unlock();
  } catch(std::runtime_error e) { fprintf(stderr, "Exception: %s\n", e.what()); }

  write(STDOUT_FILENO, "UNLOCK\n", 7);
  return 0;
}
#endif
