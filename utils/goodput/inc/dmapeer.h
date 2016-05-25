#ifndef __DMA_PEER_H__
#define __DMA_PEER_H__

#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include <sstream>

#include "tun_ipv4.h"
//#include "worker.h"

#define PEER_SIG_INIT       0x66666666L
#define PEER_SIG_UP         0xbaaddeedL
#define PEER_SIG_CLONE      0xc00cc00cL	
#define PEER_SIG_DESTROYED  0xdeadbeefL

extern bool umd_dma_tun_update_peer_RP(struct worker* info, DmaPeer* peer);

/** \brief Encapsulate Peer IB window logic and Tun TX */
class DmaPeer {
public:
  typedef struct {
    volatile uint64_t  tx_cnt; ///< How many L3 frames successfully sent to this peer on RIO
    volatile uint64_t  rx_cnt; ///< How many L3 frames found in IBwin from peer on RIO

    volatile uint64_t  tun_rx_cnt; ///< How many L3 frames came from this peer's tun
    volatile uint64_t  tun_tx_cnt; ///< How many L3 frames successfully sent to this peer's tun
    volatile uint64_t  tun_tx_err; ///< How many L3 frames failed to be sent to this peer's tun
    volatile uint64_t  rio_tx_peer_full; ///< How many times the peer's IBwin window was full based on peer RP

    volatile uint64_t  rio_rx_peer_full_ts; ///< Time at which we detected that this peer filled up our IBwin window
    volatile uint64_t  rio_rx_peer_full_ticks_total; ///< How many ticks we spent with peer's IBwin window filled up

    volatile uint64_t  rio_isol_rx_pass; ///< How many times we got called from isolcpu thread
    volatile uint64_t  rio_isol_rx_pass_add; ///< How many times/passes we detected RO=1 in IB BDs
    volatile uint64_t  rio_rx_pass; ///< How many times we received (non-empty pass of) IB BDs

#ifdef UDMA_TUN_DEBUG_SPLOCK
    volatile uint64_t  rio_isol_rx_pass_spl_ts; ///< Total number of ticks spent acquiring spinlock [cumulative]
    volatile uint64_t  rio_isol_rx_pass_spl_ts_max; ///< Max number of ticks spent acquiring spinlock
    volatile uint64_t  rio_isol_rx_pass_spl; /// How many times we acquired spinlock in top half
#endif

    uint64_t           total_ticks_rx; ///< How many ticks (total) between IB RO detection and write into Tun

    uint64_t           nread_ts; ///< rdtsc timestamp of last NREAD
    volatile uint64_t  push_rp_cnt; ///< How many times we pushed the RP 
    volatile uint64_t  push_rp_force_cnt; ///< How many times we pushed the RP 
  } DmaPeerStats_t;

public:
  volatile uint32_t  sig;
  volatile int       stop_req; ///< For the thread minding this peer
  volatile uint64_t* m_ib_histo;

private:
  pthread_mutex_t    m_mutex;
  uint16_t           m_destid; ///< RIO destid of peer
  uint64_t           m_rio_addr; ///< Peer's IBwin mapping (REMOTE)
  uint32_t           m_ibwin_size;; ///< Peer's IBwin mapping size (REMOTE)  

  void*              m_ib_ptr; ///< Pointer to some place in LOCAL IBwin

  int                m_tun_fd;
  char               m_tun_name[33];
  int                m_tun_MTU;

  DmaPeerRP_t*       m_pRP;

  uint32_t           m_WP; ///< This is what we've done to the remote peer's IBwin "BD"
  uint32_t           m_rpeer_UC; ///< Last update count we've seen from peer

  sem_t              m_rio_rx_work; ///< Isolcpu thread signals per-Tun, per-destid RIO thread that it has ready IB BDs
  DMA_L2_t**         m_rio_rx_bd_L2_ptr; ///< Location in mem of all RO bits for IB BDs, per-destid
  uint32_t*          m_rio_rx_bd_ready; ///< List of all IB BDs that have fresh data in them, per-destid
  volatile int       m_rio_rx_bd_ready_size;
  uint64_t*          m_rio_rx_bd_ready_ts; ///< IB BD RX timestamp
  pthread_spinlock_t m_rio_rx_bd_ready_splock; ///< This should be better than mutex as it spins and would not trip into a futex

  uint32_t           m_serial; ///< A monotonic serial number we keep

  struct worker*     m_info; ///< "this" of the controlling struct worker object

  bool               m_copy; ///< This is a copy of a living object

public:
  DmaPeerStats_t     m_stats; ///< Public peer stats

private:
  inline void splock() { pthread_spin_lock(&m_rio_rx_bd_ready_splock); }
  inline void spunlock() { pthread_spin_unlock(&m_rio_rx_bd_ready_splock); }

public:
  DmaPeer() :
    sig(0),
    stop_req(0),
    m_ib_histo(NULL),
    m_rio_addr(0),
    m_ibwin_size(0),
    m_ib_ptr(NULL),
    m_tun_fd(-1), m_tun_MTU(0),
    m_pRP(NULL),
    m_WP(0), m_rpeer_UC(0),
    m_rio_rx_bd_L2_ptr(NULL),
    m_rio_rx_bd_ready(NULL), m_rio_rx_bd_ready_size(0),
    m_rio_rx_bd_ready_ts(NULL),
    m_serial(0),
    m_info(NULL),
    m_copy(false)
  {
    sem_init(&m_rio_rx_work, 0, 0);
    pthread_mutex_init(&m_mutex, NULL);
    pthread_spin_init(&m_rio_rx_bd_ready_splock, PTHREAD_PROCESS_PRIVATE);

    memset(&m_stats, 0, sizeof(m_stats));
    memset(m_tun_name, 0, sizeof(m_tun_name));
  }

  ~DmaPeer() { destroy(); }

private:
  inline void copyFieldsFrom(const DmaPeer& other)
  {
    sig        = PEER_SIG_CLONE; //other.sig;
    m_copy     = true;
    stop_req   = other.stop_req;
    m_destid   = other.m_destid;
    m_rio_addr = other.m_rio_addr;
    m_ibwin_size= other.m_ibwin_size;
    m_ib_ptr   = other.m_ib_ptr;
    m_tun_fd   = -1;
    m_tun_MTU  = other.m_tun_MTU;
    strncpy(m_tun_name, other.m_tun_name, 32); m_tun_name[32] = '\0';
    m_WP       = other.m_WP;
    m_rpeer_UC = other.m_rpeer_UC;
    m_rio_rx_bd_ready = NULL;
    m_rio_rx_bd_ready_size = 0;
    m_rio_rx_bd_ready_ts = NULL;
    m_info     = NULL;
    m_ib_histo = NULL;
    memcpy(&m_stats, &other.m_stats, sizeof(m_stats));
  }

public:
  DmaPeer(const DmaPeer& other) { copyFieldsFrom(other); }

  DmaPeer& operator=(const DmaPeer& other) // copy assignment -- we just clone data
  {
    if (this == &other) return *this; // self-assignment

    copyFieldsFrom(other);

    return *this;
  }

  inline void     set_destid(const uint16_t destid) { m_destid = destid; }
  inline uint16_t get_destid() { return m_destid; }
  inline void*    get_ib_ptr() { return m_ib_ptr; }
  inline int      get_rio_rx_bd_ready_size() { return m_rio_rx_bd_ready_size; }
  inline int      get_tun_fd() { return m_tun_fd; }
  inline uint32_t get_WP() { ASSERT_BUFC(m_WP); return m_WP; }  
  inline uint32_t set_WP(const uint32_t wp) { ASSERT_BUFC(wp); return m_WP=wp; }  
  inline uint32_t inc_WP() { ++m_WP; ASSERT_BUFC(m_WP-1); return m_WP; }  

  inline uint32_t set_RP(const uint32_t rp)
  {
    ASSERT_BUFC(rp);
    m_rpeer_UC = 0;
    ((DmaPeerRP_t*)m_ib_ptr)->rpeer.UC = 0;
    ((DmaPeerRP_t*)m_ib_ptr)->rpeerLS  = 0; // destroy time stamp
    return ((DmaPeerRP_t*)m_ib_ptr)->rpeer.RP = rp;
  }

  inline uint32_t get_RP() { return ((DmaPeerRP_t*)m_ib_ptr)->rpeer.RP; }
  inline uint32_t get_RP_serial()   { return ((DmaPeerRP_t*)m_ib_ptr)->rpeer.UC; }
  inline uint64_t get_RP_lastSeen() { return ((DmaPeerRP_t*)m_ib_ptr)->rpeerLS; }

  /** \brief Return IB RP that we keep in shared memory */
  inline uint32_t get_IB_RP()
  {
    register uint32_t rp = m_pRP->RP;
    ASSERT_BUFC(rp);
    return rp;
  }  

  inline uint32_t get_serial() { return ++m_serial; }

  inline void set_copy() { m_copy = true; }

  const char* get_tun_name() { return m_tun_name; }
  inline struct worker* get_info() { return m_info; }

  pthread_mutex_t& get_mutex() { return m_mutex; }

  inline uint64_t get_rio_addr() { return m_rio_addr; }
  inline uint64_t set_rio_addr(const uint64_t rio_addr) { return m_rio_addr=rio_addr; }
  inline uint32_t get_ibwin_size() { return m_ibwin_size; }
  inline uint32_t set_ibwin_size(const uint32_t ibwin_size) { return m_ibwin_size=ibwin_size; }

  inline void rx_work_sem_post() { sem_post(&m_rio_rx_work); }
  inline void rx_work_sem_wait() { sem_wait(&m_rio_rx_work); }

  inline void lock()   { pthread_mutex_lock(&m_mutex); }
  inline void unlock() { pthread_mutex_unlock(&m_mutex); }

  /** \brief Init this peer. Set up array of pointers to IB L2 headers
   * \param[in] info
   * \param[in] ib_ptr IB window sub-mapping for this peer. See \ref IBwinMap
   */
  inline bool init(const struct worker* info, const void* ib_ptr)
  {
    assert(this);

    bool ret = false;

    if (ib_ptr == NULL) return false;

    lock();

    m_tun_fd = -1;

    // Set up array of pointers to IB L2 headers

    m_ib_ptr = (void*)ib_ptr;
    m_rio_rx_bd_L2_ptr = (DMA_L2_t**)calloc(info->umd_tx_buf_cnt, sizeof(DMA_L2_t*)); // +1 secret cell
    if (m_rio_rx_bd_L2_ptr == NULL) goto error;

    m_rio_rx_bd_ready = (uint32_t*)calloc(info->umd_tx_buf_cnt, sizeof(uint32_t)); // +1 secret cell
    if (m_rio_rx_bd_ready == NULL) goto error;

    m_rio_rx_bd_ready_ts = (uint64_t*)calloc(info->umd_tx_buf_cnt, sizeof(uint64_t)); // +1 secret cell
    if (m_rio_rx_bd_ready_ts == NULL) goto error;

    m_ib_histo = (uint64_t*)calloc(info->umd_tx_buf_cnt, sizeof(uint64_t));
    if (m_ib_histo == NULL) goto error;

    m_rio_rx_bd_ready_size = 0;

    {{ // RX: Pre-populate the locations of the RO bit in IB BDs, and zero RO
      std::stringstream ss;
      uint8_t* p = sizeof(DmaPeerRP_t) + (uint8_t*)ib_ptr;
      for (int i = 0; i < (info->umd_tx_buf_cnt-1); i++, p += BD_PAYLOAD_SIZE(info)) {
        m_rio_rx_bd_L2_ptr[i] = (DMA_L2_t*)p;
        m_rio_rx_bd_L2_ptr[i]->RO = 0;
#ifdef UDMA_TUN_DEBUG_IB
        char tmp[129] = {0};
        snprintf(tmp, 128, "\tL2_ptr[%d] = %p\n", i, p);
        ss << tmp;
#endif // UDMA_TUN_DEBUG_IB
      }
#ifdef UDMA_TUN_DEBUG_IB
      DBG("\n\tL2 acceleration array (base=%p, size=%d):\n", info->ib_ptr, info->umd_tx_buf_cnt-1);
      write(STDOUT_FILENO, ss.str().c_str(), ss.str().size());
#endif // UDMA_TUN_DEBUG_IB
    }}

    m_pRP = (DmaPeerRP_t*)m_ib_ptr;
    m_pRP->sig = DMAPEER_SIG;

    m_info = (struct worker*)info;

    ret = true;

  unlock:
    unlock();
    return ret;

  error:
    if (m_rio_rx_bd_L2_ptr != NULL)   { free(m_rio_rx_bd_L2_ptr); m_rio_rx_bd_L2_ptr = NULL; }
    if (m_rio_rx_bd_ready != NULL)    { free(m_rio_rx_bd_ready); m_rio_rx_bd_ready = NULL; }
    if (m_rio_rx_bd_ready_ts != NULL) { free(m_rio_rx_bd_ready_ts); m_rio_rx_bd_ready_ts = NULL; }
    if (m_ib_histo != NULL) { free((void*)m_ib_histo); m_ib_histo = NULL; }
    goto unlock;
  }

  inline void destroy()
  {
    assert(this);

    if (m_copy) return;

    lock();
    sig = PEER_SIG_DESTROYED;

    if (m_tun_fd >= 0) { close(m_tun_fd); m_tun_fd = -1; }
    m_tun_name[0] = '\0';

    if (m_info == NULL) return; // ::init was not called?

    free(m_rio_rx_bd_L2_ptr);   m_rio_rx_bd_L2_ptr = NULL;
    free(m_rio_rx_bd_ready);    m_rio_rx_bd_ready = NULL;
    free(m_rio_rx_bd_ready_ts); m_rio_rx_bd_ready_ts = NULL;
    m_rio_rx_bd_ready_size = -1;
    free((void*)m_ib_histo);    m_ib_histo = NULL;

    m_info = NULL;

    unlock();
  }

  /** \brief Set up a Tun for a Peer
   * \note 169.254.x.y is set up like so: A := (m_destid + DESTID_TRANSLATE); x := A>>8; y := A & 0xF
   * \param my_destid RIO destid of this box
   * \param DESTID_TRANSLATE translation quantity of destid to 169.254.x.y
   */
  inline bool setup_TUN(const uint16_t my_destid, const int DESTID_TRANSLATE)
  {
    assert(this);

    bool ret = false;
    char if_name[IFNAMSIZ] = {0};
    char Tap_Ifconfig_Cmd[257] = {0};
    int flags = IFF_TUN | IFF_NO_PI;

    if (m_info == NULL) return false;

    lock();

    if (my_destid == m_destid) goto error;

    memset(m_tun_name, 0, sizeof(m_tun_name));

    // Initialize tun/tap interface
    if ((m_tun_fd = tun_alloc(if_name, flags)) < 0) {
        CRIT("Error connecting to tun/tap interface %s!\n", if_name);
        goto error;
    }
    strncpy(m_tun_name, if_name, sizeof(m_tun_name)-1);

    {{
      const int flags = fcntl(m_tun_fd, F_GETFL, 0);
      fcntl(m_tun_fd, F_SETFL, flags | O_NONBLOCK);
    }}

    // Configure tun/tap interface for pointo-to-point IPv4, L2, no ARP, no multicast

    {{
      const uint16_t my_destid_tun   = my_destid + DESTID_TRANSLATE;
      const uint16_t peer_destid_tun = m_destid + DESTID_TRANSLATE;

      snprintf(Tap_Ifconfig_Cmd, 256, "169.254.%d.%d pointopoint 169.254.%d.%d",
         (my_destid_tun >> 8) & 0xFF,   my_destid_tun & 0xFF,
         (peer_destid_tun >> 8) & 0xFF, peer_destid_tun & 0xFF);

      char ifconfig_cmd[257] = {0};
      snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s %s mtu %d up",
                  if_name, Tap_Ifconfig_Cmd, m_info->umd_tun_MTU);
      const int rr = system(ifconfig_cmd);
      if(rr >> 8) {
        m_tun_name[0] = '\0';
        CRIT("system() failed with error %d\n", rr);
        // No need to remove from epoll set, close does that as it isn't dup(2)'ed
        close(m_tun_fd); m_tun_fd = -1;
        goto error;
      }

      snprintf(ifconfig_cmd, 256, "/sbin/ifconfig %s -multicast", if_name);
      system(ifconfig_cmd);
    }}

    sig = PEER_SIG_UP;

    {{
      struct epoll_event event;
      event.data.ptr = this;
      event.events = EPOLLIN; // | EPOLLET;
      if (epoll_ctl (m_info->umd_epollfd, EPOLL_CTL_ADD, m_tun_fd, &event) < 0) {
        CRIT("\n\tFailed to add tun_fd %d for peer destid %u to epoll set %d\n",
           m_tun_fd, m_destid, m_info->umd_epollfd);
        close(m_tun_fd); m_tun_fd = -1;
        goto error;
      }
    }}

    ret = true;

unlock:
    unlock();

    if (ret) {
        INFO("\n\t%s %s mtu %d on DMA Chan=%d,...,Chan_n=%d Chan2=%d my_destid=%u peer_destid=%u #buf=%d #fifo=%d\n",
           if_name, Tap_Ifconfig_Cmd, m_info->umd_tun_MTU,
           m_info->umd_chan, m_info->umd_chan_n, m_info->umd_chan2,
           my_destid, m_destid,
           m_info->umd_tx_buf_cnt, m_info->umd_sts_entries);
    }

    return ret;

error:
    goto unlock;
  }

  /** \brief Scan & Count the RO IB "BDs" for this Peer
   * \note This is done UNLOCKED and intended only for reporting
   * \return Number of ready IB BDs
   */
  inline int count_RO()
  {
    if (m_info == NULL || m_rio_rx_bd_L2_ptr == NULL) return -42;

    assert(m_pRP->sig == DMAPEER_SIG);

    int cnt = 0;
    for (int i = 0; i < (m_info->umd_tx_buf_cnt-1); i++) {
      if(1 == m_rio_rx_bd_L2_ptr[i]->RO) cnt++;
    }

    return cnt;
  }

  /** \brief Update last seen timestamp if peer pushed a new update counter */
  inline void update_RP_LastSeen()
  {
    assert(m_pRP->sig == DMAPEER_SIG);
    if (m_rpeer_UC != m_pRP->rpeer.UC) {
      m_rpeer_UC = m_pRP->rpeer.UC;
      m_pRP->rpeerLS = rdtsc();
    }
  }

  /** \brief Scan the IB "BDs" for this Peer and append them to a locked array [drained by \ref service_TUN_TX]
   * \note This is called in islcpu thread so the code must be brief
   * \note "RO" stands for "Reader Owned IB BD" aka fresh data indicator
   * \return Number of BDs that have fresh data
   */
  inline int scan_RO()
  {
    int cnt = 0;

    assert(m_info); // Not initlialised?

    assert(sig == PEER_SIG_UP);

    uint32_t k = m_pRP->RP;
    assert(k >= 0);
    ASSERT_BUFC(k);

    m_stats.rio_isol_rx_pass++;

    update_RP_LastSeen();

    uint64_t now = rdtsc();

    if (m_rio_rx_bd_ready_size >= (m_info->umd_tx_buf_cnt-1)) { // Quick peek while unlocked -- Receiver too slow, go to next peer!
      if (m_stats.rio_rx_peer_full_ts == 0) m_stats.rio_rx_peer_full_ts = now;
      return 0;
    }
    if (m_stats.rio_rx_peer_full_ts > 0 && now > m_stats.rio_rx_peer_full_ts) { // We just drained some of IBwin window for this peer
      m_stats.rio_rx_peer_full_ticks_total += now - m_stats.rio_rx_peer_full_ts;
    }
    m_stats.rio_rx_peer_full_ts = 0;

#ifdef UDMA_TUN_DEBUG_SPLOCK
    const uint64_t spl_ts_s = rdtsc();
    splock();
    const uint64_t spl_ts_a = rdtsc();
    if (spl_ts_a > spl_ts_s) {
      const uint64_t dT = (spl_ts_a - spl_ts_s);
      m_stats.rio_isol_rx_pass_spl_ts += dT;
      if (dT > m_stats.rio_isol_rx_pass_spl_ts_max) m_stats.rio_isol_rx_pass_spl_ts_max = dT;
      m_stats.rio_isol_rx_pass_spl++;
    }
#else
    splock();
#endif

    int idx = m_rio_rx_bd_ready_size; // If not zero then RIO RX thr is sloow
    assert(idx >= 0);

#ifdef UDMA_TUN_DEBUG_IB_BD
    const int N_pending = count_RO();
    const uint32_t saved_RP = k;
#endif
    for (int i = 0; i < (m_info->umd_tx_buf_cnt-1); i++) {
      if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
      assert(sig == PEER_SIG_UP);

      update_RP_LastSeen();
      if (idx >= (m_info->umd_tx_buf_cnt-1)) break;

      if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
      assert(sig == PEER_SIG_UP);

      if(0 != m_rio_rx_bd_L2_ptr[k]->RO) {
        if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
        assert(sig == PEER_SIG_UP);

        if (42 == m_rio_rx_bd_L2_ptr[k]->RO) goto next; // Not yet cleared by "bottom half"

        const uint64_t now = rdtsc();
#ifdef UDMA_TUN_DEBUG_IB
        DBG("\n\tFound ready buffer at RP=%u\n", k);
#endif

        ASSERT_BUFC(k);
        ASSERT_BUFC(idx);

        m_rio_rx_bd_ready[idx] = k;
        m_rio_rx_bd_ready_ts[idx] = now;
        m_rio_rx_bd_L2_ptr[k]->RO = 42; // So we won't revisit
        idx++;
        cnt++;
      } else {
        if (cnt > 0) break; // Stop at 1st non-ready IB bd?? But... RP might have been moved
      }

 next:
      k++; if(k == (m_info->umd_tx_buf_cnt-1)) k = 0; // RP wrap-around
      ASSERT_BUFC(k);
    }

    if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;

    if (cnt > 0) {
      assert(cnt <= (m_info->umd_tx_buf_cnt-1)); // XXX Cannot exceed that!
      m_rio_rx_bd_ready_size += cnt;
      assert(m_rio_rx_bd_ready_size <= (m_info->umd_tx_buf_cnt-1));
      m_stats.rio_isol_rx_pass_add++;
      if (m_ib_histo != NULL) m_ib_histo[m_rio_rx_bd_ready_size]++;
    }
#ifdef UDMA_TUN_DEBUG_IB_BD
    else {
      if (N_pending > 0) {
	char buf[81920+1] = {0};
        std::stringstream ss;
        for (int i = 0; i < (m_info->umd_tx_buf_cnt-1); i++) {
          char tmp[17] = {0};
          snprintf(tmp, 16, "%d ", m_rio_rx_bd_L2_ptr[i]->RO);
          strncat(buf, tmp, 81920);
        }
        CRIT("\n\tBUG: IBBD[RP]->RO==0 k=%d savRP=%u volRP=%u pending %d IB BDs: %s\n", k, saved_RP, m_pRP->RP, N_pending, buf);
        usleep(10 * 1000); fflush(NULL);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
        assert("IB BD BUG" == "IT'S ALIVE");
#pragma GCC diagnostic pop
      }
    }
#endif // UDMA_TUN_DEBUG_IB

    spunlock();

    update_RP_LastSeen();

    if (cnt == 0) return 0;

    rx_work_sem_post();
    return cnt;

  stop_req:
    spunlock();
    return -1;
  }

  /** \brief Process all IB "BDs" identified by \ref scan_RO
   * \return Number of IB "BDs" decapped of L2 and successfully written into Tun
   */
  inline int service_TUN_TX(const uint64_t MAX_RP_INTERVAL)
  {
    assert(this);
    assert(m_info);
    assert(m_rio_rx_bd_ready_size <= (m_info->umd_tx_buf_cnt-1));

    int rx_ok = 0; // TX'ed into Tun device
    uint64_t last_ts = 0;

    int cnt = 0;
    int ready_bd_list[m_info->umd_tx_buf_cnt]; memset(ready_bd_list, 0xff, sizeof(ready_bd_list));

    if (stop_req || (m_info != NULL && m_info->stop_req)) return -1;
    assert(sig == PEER_SIG_UP);

    splock();
    // Make this quick & sweet so we don't hose the FIFO thread for long
    for (int i = 0; i < m_rio_rx_bd_ready_size; i++) {
      assert(m_rio_rx_bd_ready[i] < (m_info->umd_tx_buf_cnt-1));
      ready_bd_list[cnt++] = m_rio_rx_bd_ready[i];
    }
    m_rio_rx_bd_ready_size = 0;
    spunlock();

    if (cnt == 0) return 0;

    if (stop_req || (m_info != NULL && m_info->stop_req)) return -1;
    assert(sig == PEER_SIG_UP);

    m_stats.rio_rx_pass++;

#ifdef UDMA_TUN_DEBUG_IB
    DBG("\n\tInbound %d buffers(s) will be processed from destid %u RP=%u\n", cnt, destid, m_pRP->RP);
#endif

    bool last_pkt_acked = false;

    if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
    assert(sig == PEER_SIG_UP);

    for (int i = 0; i < cnt && !stop_req && m_info != NULL && !m_info->stop_req; i++) {
      int rp = ready_bd_list[i];
      assert(rp >= 0);
      ASSERT_BUFC(rp);

      if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
      assert(sig == PEER_SIG_UP);

      DMA_L2_t* pL2 = m_rio_rx_bd_L2_ptr[rp];

      rx_ok++;
      const int payload_size = ntohl(pL2->len) - DMA_L2_SIZE;

      assert(payload_size > 0);
      assert(payload_size <= m_info->umd_tun_MTU);

      m_stats.rx_cnt++;

      uint8_t* payload = (uint8_t*)pL2 + DMA_L2_SIZE;
      uint64_t tx_ts = rdtsc();
      const int nwrite = cwrite(m_tun_fd, payload, payload_size);
#ifdef UDMA_TUN_DEBUG_IB
      const uint32_t crc = crc32(0, payload, payload_size);
      DBG("\n\tGot a msg of size %d from RIO destid %u (L7 CRC32 0x%x) cnt=%llu, wrote %d to %s -- rp=%d\n",
         ntohl(pL2->len), ntohs(pL2->destid), crc, rx_ok, nwrite, m_tun_name, rp);
#endif

      if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
      assert(sig == PEER_SIG_UP);

      if (nwrite == payload_size) {
           m_stats.tun_tx_cnt++;
           if (tx_ts > m_rio_rx_bd_ready_ts[i]) m_stats.total_ticks_rx += tx_ts - m_rio_rx_bd_ready_ts[i];
      } else m_stats.tun_tx_err++;

      m_rio_rx_bd_ready_ts[i] = 0;

      m_rio_rx_bd_L2_ptr[rp]->RO = 0; // Signal "top half" that we're done with this

      rp++; if (rp == (m_info->umd_tx_buf_cnt-1)) rp = 0;
      ASSERT_BUFC(rp);

#ifdef UDMA_TUN_DEBUG_IB
      DBG("\n\tUpdating old RP %d to %d\n", pRP->RP, rp);
#endif

      if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
      assert(sig == PEER_SIG_UP);

      m_pRP->RP = rp;

      bool force_rp_push = false;
      const uint64_t now = rdtsc();
      if (MAX_RP_INTERVAL > 0 && last_ts != 0 && now > last_ts) {
        if ((now - last_ts) > MAX_RP_INTERVAL) {
          force_rp_push = true;
          m_stats.push_rp_force_cnt++;
        }
      }
      last_ts = now;

      if (stop_req || (m_info != NULL && m_info->stop_req)) goto stop_req;
      assert(sig == PEER_SIG_UP);

      // THIS is gasping at straws!
      if (force_rp_push || m_info->umd_push_rp_thr == 0 || \
          cnt == 1 ||
          (i == 0) || i == (cnt-1) ||
          (i % m_info->umd_push_rp_thr) == 0) {
        umd_dma_tun_update_peer_RP(m_info, this); m_stats.push_rp_cnt++;
        last_pkt_acked = true;
      } else {
        last_pkt_acked = false;
      }
    } // END for all ready BDs

stop_req:
    do {
      if (last_pkt_acked) break;
      if (stop_req || (m_info != NULL && m_info->stop_req)) break;
      assert(m_pRP->sig == DMAPEER_SIG);
      umd_dma_tun_update_peer_RP(m_info, this); m_stats.push_rp_cnt++;
      assert(m_pRP->sig == DMAPEER_SIG);
    } while(0);
    return rx_ok;
  }

  inline void ASSERT_BUFC(const int n)
  {
    if(m_copy) return;
    assert(m_pRP->sig == DMAPEER_SIG);
    assert(n < (m_info->umd_tx_buf_cnt-1));
  }
}; // END class DmaPeer

#endif // __DMA_PEER_H__
