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

/** \file hash.cc Implementation of SHM hash map */

#include "pshm.h"

extern "C" uint32_t crc32(uint32_t crc, const void *buf, size_t size);

/** \brief Private gettid(2) implementation */
static inline pid_t gettid() { return syscall(__NR_gettid); }

/** \brief Template for a singleton hashmap with data stored entirely in POSIX SHM
 * Uses an instance of \ref POSIXShm to manipulate SHM
 * \note This is a singleton (based on name) across multiple processes.
 */
template<class CKey, class CVal>
class ShmHashMap
{
public:
  /** \brief Make a SHM hash map of pre-allocated TOTAL size
   * \throws std::runtime_error
   * \param[in] name Name of hash. refer to sem_open(3) for legal names
   * \param max_size Max size in bytes of the SHM area
   */
  ShmHashMap(const char* name, const int max_size)
  {
    if(name == NULL || name[0] == '\0')
      throw std::runtime_error("ShmHashMap: Invalid name!");
    if(max_size < 1)
      throw std::runtime_error("ShmHashMap: Invalid size!");

    const int FUDGE = HASH_INT;

    const int SHM_SIZE = (sizeof(CKey) + sizeof(CVal) + FUDGE) * max_size + sizeof(HashMap_t);

    m_shm  = new POSIXShm(name, SHM_SIZE); // this zeros'es the pages at start
    m_hash = (HashMap_t*)m_shm->getMem();

    m_hash_name = name;

    m_pid = getpid();
    m_tid = gettid();

    m_shm->lock();
    if(! m_hash->initialised) {

      m_hash->bucket_size = (SHM_SIZE - sizeof(HashMap_t)) / (sizeof(HashMapMember_t) * HASH_INT);

      for(int i = 0; i < HASH_INT; i++) {
        const int k = i * m_hash->bucket_size;
        m_hash->buckets[i] = (uint8_t*)&m_hash->members[0] - (uint8_t*)&m_hash->members[0]; // OFFSET in SHM
      }
      m_hash->initialised = true;
    }
    m_shm->unlock();
  }

  ~ShmHashMap() { delete m_shm; }

  /** \brief Add a key-val pair in SHM map
   * \throws std::runtime_error
   * \param k key -- a bit-wise copy of it will be stored
   * \param v value -- a bit-wise copy of it will be stored, beware of pointers, consumers may live in other processes
   * \return true if NOT found & inserted, false otherwise
   */
  bool insert(const CKey& k, CVal& v)
  {
    bool is_owner = false;
    if(find(k, is_owner) != NULL) return false;

    bool hash_full = true;
    const uint32_t hash = Crc32(k) % HASH_INT;

    m_shm->lock();
    int i = 0;
    for(HashMapMember_t* it = (HashMapMember_t*)((uint8_t*)&m_hash->members[0] + m_hash->buckets[hash]); i < m_hash->bucket_size; it++, i++) {
      if(it->occupied) continue;

      it->occupied = true;
      it->hash     = hash;
      it->key      = k;
      it->val      = v;
      it->pid      = m_pid;
      it->tid      = m_tid;

      hash_full    = false;
      break;
    }
    m_shm->unlock();

    if(hash_full)
      throw std::runtime_error("ShmHashMap::insert: hash is full, please adjust FUDGE!");

    return true;
  }

  /** \brief Nuke a key in SHM map
   * \param k key to be erased
   * \return true if found & erased, false otherwise
   */
  bool erase(const CKey& k)
  {
    bool found = false;

    int pos = -1;
    m_shm->lock();
    if(find(k, pos, true /*locked*/)) {
      HashMapMember_t* it = &m_hash->members[pos]; 

      memset(&it->key, 0, sizeof(it->key));
      memset(&it->val, 0, sizeof(it->val));
      it->pid      = 0;
      it->tid      = 0;
      it->hash     = 0;
      it->occupied = false;

      found = true;
    }
    m_shm->unlock();

    return found;
  }

  /** \brief Find a key in SHM map
   * \param k key to be found
   * \param[out] is_owner indicates whether this process stored the key in map, beweare of pointers stored in CVal
   * \return NULL if key not found, a pointer to a CKey value in SHM -- do NOT free, use \ref erase
   */
  CVal* find(const CKey& k, bool& is_owner)
  {
     CVal* ret = NULL;

     int pos = -1;
     m_shm->lock();
     if(find(k, pos, true /*locked*/)) {
       ret = &m_hash->members[pos].val;
       is_owner = m_hash->members[pos].pid == m_pid;
     }
     m_shm->unlock();

     return ret;
  }

  /** \brief Hashmap dumper */
  void dumpHashMap(std::string& out)
  {
    std::stringstream ss;

    ss << "Name: \"" << m_hash_name << "\" BucketSize: " << m_hash->bucket_size << "\n";

    m_shm->lock();
    for(int i = 0; i < (HASH_INT * m_hash->bucket_size); i++) {
      if(!  m_hash->members[i].occupied) continue;

      ss << "  members["<<i<<"] pid=" << m_hash->members[i].pid << " tid=" << m_hash->members[i].tid << " hash=" << m_hash->members[i].hash;
      kill(m_hash->members[i].pid, 0) == 0 || ss << " DEAD";
      ss << "\n";
      ss << "                   key=" << m_hash->members[i].key << " val @" << std::hex << &m_hash->members[i].val << std::dec << "\n";
    }
    m_shm->unlock();

    out = ss.str();
  }

private:
  static const int HASH_INT = 19; ///< This should be a prime number to hash upon

  ///< Hash map member
  typedef struct {
    volatile bool occupied;
    uint32_t      hash; ///< Hashed value of key
    CKey          key; 
    CVal          val; 
    pid_t         pid, tid;
  } HashMapMember_t;

  typedef struct {
    volatile bool    initialised;
    int              bucket_size;
    uint32_t         buckets[HASH_INT]; ///< points (OFFSET) to every other HASH_INT in members[]
    HashMapMember_t  members[0]; ///< This repeats as needed based on allocated SHM size
  } HashMap_t;

  POSIXShm*   m_shm;
  HashMap_t*  m_hash;
  std::string m_hash_name;
  pid_t       m_pid; ///< Process ID of this instance
  pid_t       m_tid; ///< Thread (LWP) ID of this instance (\ref gettid)

  /** \brief Dummy wrapper around C crc32 */
  uint32_t Crc32(const CKey& k) { return crc32(0, &k, sizeof(k)); }

  /** \brief Find a key in SHM map, private version
   * \param k key to be found
   * \param[out] pos index in SHM array of member (if found)
   * \param locked Called from locked context?
   * \return true if key found, false otherwise
   */
  bool find(const CKey& k, int& pos, const bool locked)
  {
    bool ret = false;

    const uint32_t hash = Crc32(k) % HASH_INT;

    if(! locked) m_shm->lock();
    int i = 0;
    for(HashMapMember_t* it = (HashMapMember_t*)((uint8_t*)&m_hash->members[0] + m_hash->buckets[hash]); i < m_hash->bucket_size; it++, i++) {
      if(! it->occupied) continue;
      if(it->hash != hash) continue; // PANIC?
      if(it->key  != k) continue;

      pos = (it - &m_hash->members[0]);
      ret = true;
      break;
    }
    if(! locked) m_shm->unlock();

    return ret;
  }
};

#ifdef TEST_HASH
struct work
{
  void* func;
  int   arg1;
  uint64_t   arg2;
};

int main()
{
  ShmHashMap<uint64_t, struct work> map("DMA Completion Work", 1024);

  const pid_t p = getpid();

  struct work wk1; wk1.func = (void*)&main; wk1.arg1 = 1; wk1.arg2 = (uint64_t)"efef";
  struct work wk2; wk2.func = (void*)&exit; wk2.arg1 = 69; wk2.arg2 = 0;

  map.insert(p*10 + 1, wk1);
  map.insert(p*10 + 2, wk2);

  map.erase(p*10 + 1);

  bool is_owner = false;
  struct work* found = map.find(p*10 + 2, is_owner);
  printf("Actual func=%p found func=%p owner? %s\n", wk2.func, found->func, is_owner? "YES": "no");

  write(STDOUT_FILENO, "Enter:", 6);
  int c;
  read(STDIN_FILENO, &c, 1);

  map.erase(p*10 + 2);
  map.erase(p*10 + 2);

  std::string s;
  map.dumpHashMap(s);
  puts(s.c_str());

  return 0;
}
#endif
