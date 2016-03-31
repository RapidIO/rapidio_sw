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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <sstream>
#include <stdexcept>

#include "mapfile.h"

MapFile::MapFile(const char* filename)
{
  if(filename == NULL || filename[0] == '\0')
    throw std::runtime_error("MapFile: Invalid BAR filename!");
  
  m_size = 0;
  m_ptr  = NULL;
  m_filename = filename;

  m_fd = open(filename, O_RDWR | O_SYNC);

  if(m_fd >= 0) return;

  static char tmp[257] = {0};
  snprintf(tmp, 256, "MapFile: Failed to open BAR file %s: %s", filename, sys_errlist[errno]);

  throw std::runtime_error(tmp);
}

MapFile::~MapFile()
{
  unmap_file();
  close(m_fd);
}

/** \brief Maps a PCIE BAR to a virtual pointer
 * \throws std::runtime_error
 * \param size if 0 then the file's size will be used [as reported by fstat], otherwise size
 * \return if OK address in memory where file was mapped
 */
void* MapFile::map_file(uint64_t size)
{
  if(m_ptr != NULL)
    unmap_file();

  if(size > 0)
    m_size = size;
  else {
    struct stat st;
    if(fstat(m_fd, &st) < 0) {
      static char tmp[257] = {0};
      snprintf(tmp, 256, "MapFile: Failed to stat %s: %s", m_filename.c_str(), sys_errlist[errno]);

      throw std::runtime_error(tmp);
    }

    m_size = st.st_size;
  }

  m_ptr = mmap(NULL,                       /* Kernel picks starting addr */
               m_size,                 /* Length */
               PROT_READ | PROT_WRITE, /* For reading & writing */
               MAP_SHARED,             /* Must be MAP_SHARED */
               m_fd,                   /* File descriptor */
               0);                    

  if(m_ptr == NULL) {
    m_size = 0;

    static char tmp[257] = {0};
    snprintf(tmp, 256, "MapFile: Failed to mmap: %s", sys_errlist[errno]);

    throw std::runtime_error(tmp);
  }

  return (void*)m_ptr;
}

void MapFile::unmap_file()
{
  if(m_ptr == NULL) return;

  if(munmap((void*)m_ptr, m_size) != -1) {
    m_ptr  = NULL;
    m_size = 0;
    return;
  }

  static char tmp[257] = {0};
  snprintf(tmp, 256, "MapFile: Failed to unmap: %s", sys_errlist[errno]);

  throw std::runtime_error(tmp);
}

std::string MapFile::toString()
{
  std::stringstream ss;
  ss << m_filename << " mapped to " << std::hex << m_ptr << std::dec
     << " with size = " << m_size;
  return ss.str();
}
