#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <map>
#include <vector>
#include <stdexcept>

#define Dprintf(fmt, ...) { if (g_debug) printf(fmt, __VA_ARGS__); }

volatile uint32_t rskt_shim_initialised = 0;
volatile uint32_t rskt_shim_RSKT_initialised = 0;

void rskt_shim_main() __attribute__ ((constructor));

static pthread_mutex_t g_rskt_shim_mutex = PTHREAD_MUTEX_INITIALIZER; 

static pthread_mutex_t g_map_mutex = PTHREAD_MUTEX_INITIALIZER; 

static int g_debug = 0;
static uint16_t g_my_destid = 0xFFFF;

typedef struct {
  uint16_t r_destid;
  uint16_t r_port;
  void*    rsock;
  int      rskt_errno;
} Accepted_t;

typedef struct {
  int                fd; ///< For documentation purposes
  int                sockp[2];
  int                nonblock;
  bool               borked; ///< Closed from beneath us by librskt
  bool               can_read;
  bool               listening; ///< listen was called on this socket
  bool               can_write;
  volatile int       stop_req;
  struct sockaddr_in laddr;
  struct sockaddr_in raddr;
  void*              rsock;
  pthread_mutex_t         acc_mutex;
  std::vector<Accepted_t> acc_list; // rskt_accept'ed sockets for nonblocking listening sockets
  std::vector<pthread_t>  acc_thr_list; // rskt_accept minder threads
} SocketTracker_t;

static SocketTracker_t ZERO_SOCK;

static std::map<int, SocketTracker_t> g_sock_map;

#define UNDERSCORE // "_"

#define DECLARE(name, ret, args) static ret (*glibc_##name) args

DECLARE(socket, int, (int, int, int));
DECLARE(bind, int, (int, const struct sockaddr*, socklen_t));
DECLARE(listen, int, (int, int));
DECLARE(accept, int, (int, struct sockaddr*, socklen_t*));
DECLARE(accept4, int,(int, struct sockaddr*, socklen_t*, int));
DECLARE(connect, int, (int, const struct sockaddr*, socklen_t));
         
DECLARE(shutdown, int, (int, int));
DECLARE(close, int, (int));

DECLARE(setsockopt, int, (int, int, int, const void*, socklen_t)); // TBI
DECLARE(getsockopt, int, (int, int, int, void*)); // TBI

DECLARE(getpeername, int, (int, struct sockaddr*, socklen_t*));
DECLARE(getsockname, int, (int, struct sockaddr*, socklen_t*));

DECLARE(read, ssize_t, (int, void *, size_t)); // TBI
DECLARE(pread, ssize_t, (int, void *, size_t, off_t)); // TBI
DECLARE(recv, ssize_t, (int, void *, size_t, int)); // TBI
DECLARE(readv, ssize_t, (int, const struct iovec *, int)); // TBI

DECLARE(write, ssize_t, (int, const void *, size_t)); // TBI
DECLARE(pwrite, ssize_t, (int, const void *, size_t, off_t)); // TBI
DECLARE(send, ssize_t, (int, const void *, size_t, int)); // TBI
DECLARE(writev, ssize_t, (int, const struct iovec *, int)); // TBI

DECLARE(select, int, (int, fd_set *, fd_set *, fd_set *, struct timeval*));
DECLARE(pselect, int, (int, fd_set *, fd_set *, fd_set *, struct timespec*, const sigset_t*)); // TBI

DECLARE(poll, int, (struct pollfd *, int, int));
DECLARE(ppoll, int, (struct pollfd *, int, int, const struct timespec*, const sigset_t*)); // TBI

DECLARE(epoll_wait, int, (int, struct epoll_event*, int, int)); // TBI
DECLARE(epoll_pwait, int, (int, struct epoll_event*, int, int, const sigset_t*)); // TBI

DECLARE(dup, int, (int)); // TBI
DECLARE(dup2, int, (int, int)); // TBI

DECLARE(sendfile, ssize_t, (int, int, off_t *, size_t)); // TBI

DECLARE(ioctl, int, (int fd, unsigned long, ...));
DECLARE(fcntl, int, (int, int, ...));

static inline void errx(const char* msg)
{ if (msg) throw std::runtime_error(msg); _exit(42); }

#define DLSYMCAST(name, ret, args)  do { \
	if ((glibc_##name = (ret (*) args) dlsym(dh, UNDERSCORE #name)) == NULL)		\
		errx("RSKT Shim: Failed to get " #name "() address");	\
} while(0);

#define RSKT_DECLARE(name, ret, args) static ret (*RSKT_##name) args

#define RSKT_DLSYMCAST(name, ret, args)  do { \
	if ((RSKT_##name = (ret (*) args) dlsym(dh, UNDERSCORE #name)) == NULL)		\
		errx("RSKT Shim: Failed to get " #name "() address");	\
} while(0);


RSKT_DECLARE(shim_rskt_init, void, (void));
RSKT_DECLARE(shim_rskt_get_my_destid, uint16_t, (void));
RSKT_DECLARE(shim_rskt_socket, void*, (void));

RSKT_DECLARE(shim_rskt_connect, int, (void*, uint16_t, uint16_t));

RSKT_DECLARE(shim_rskt_listen, int, (void*, int));
RSKT_DECLARE(shim_rskt_bind, int, (void*, uint16_t, uint16_t));
RSKT_DECLARE(shim_rskt_accept, int, (void*, void*, uint16_t*, uint16_t*));

RSKT_DECLARE(shim_rskt_close, int, (void*));

RSKT_DECLARE(shim_rskt_read, int, (void*, void*, int));
RSKT_DECLARE(shim_rskt_get_avail_bytes, int, (void*));

RSKT_DECLARE(shim_rskt_write, int, (void*, void*, int));

void rskt_shim_main() __attribute__ ((constructor));

static void rskt_shim_init_RSKT()
{
  void* dh = NULL;

  if (rskt_shim_RSKT_initialised) return;

  const char* rskt2_path = "./rskt_shim2.so";
  if ((dh = dlopen(rskt2_path, RTLD_LAZY)) == NULL)
	errx("RSKT Shim: Failed to open shim2");

  RSKT_DLSYMCAST(shim_rskt_init, void, (void));
  RSKT_DLSYMCAST(shim_rskt_get_my_destid, uint16_t, (void));
  RSKT_DLSYMCAST(shim_rskt_socket, void*, (void));

  RSKT_DLSYMCAST(shim_rskt_connect, int, (void*, uint16_t, uint16_t));

  RSKT_DLSYMCAST(shim_rskt_listen, int, (void*, int));
  RSKT_DLSYMCAST(shim_rskt_bind, int, (void*, uint16_t, uint16_t));
  RSKT_DLSYMCAST(shim_rskt_accept, int, (void*, void*, uint16_t*, uint16_t*));

  RSKT_DLSYMCAST(shim_rskt_close, int, (void*));

  RSKT_DLSYMCAST(shim_rskt_read, int, (void*, void*, int));
  RSKT_DLSYMCAST(shim_rskt_get_avail_bytes, int, (void*));
  RSKT_DLSYMCAST(shim_rskt_write, int, (void*, void*, int));

  RSKT_shim_rskt_init();

  rskt_shim_RSKT_initialised = 0xfeedbabaL;
}

void rskt_shim_main()
{
  void* dh = NULL;

  pthread_mutex_lock(&g_rskt_shim_mutex);
  if (rskt_shim_initialised == 0xf00ff00d) {
    pthread_mutex_unlock(&g_rskt_shim_mutex);
    return;
  }

  const char* libc_path = access("/lib64/libc.so.6", F_OK)?
                                 "/lib/libc.so.6": // 32-bit
                                 "/lib64/libc.so.6";

  if ((dh = dlopen(libc_path, RTLD_LAZY)) == NULL)
	errx("RSKT Shim: Failed to open libc");
  
  DLSYMCAST(socket, int, (int, int, int));
  DLSYMCAST(bind, int, (int, const struct sockaddr*, socklen_t));
  DLSYMCAST(listen, int, (int, int));
  DLSYMCAST(accept, int, (int, struct sockaddr*, socklen_t*));
  DLSYMCAST(accept4, int,(int, struct sockaddr*, socklen_t*, int));
  DLSYMCAST(connect, int, (int, const struct sockaddr*, socklen_t));

  DLSYMCAST(shutdown, int, (int, int));
  DLSYMCAST(close, int, (int));

  DLSYMCAST(setsockopt, int, (int, int, int, const void*, socklen_t));
  DLSYMCAST(getsockopt, int, (int, int, int, void*));

  DLSYMCAST(getpeername, int, (int, struct sockaddr*, socklen_t*));
  DLSYMCAST(getsockname, int, (int, struct sockaddr*, socklen_t*));

  DLSYMCAST(read, ssize_t, (int, void *, size_t));
  DLSYMCAST(pread, ssize_t, (int, void *, size_t, off_t));
  DLSYMCAST(recv, ssize_t, (int, void *, size_t, int));
  DLSYMCAST(readv, ssize_t, (int, const struct iovec *, int));

  DLSYMCAST(write, ssize_t, (int, const void *, size_t));
  DLSYMCAST(pwrite, ssize_t, (int, const void *, size_t, off_t));
  DLSYMCAST(send, ssize_t, (int, const void *, size_t, int));
  DLSYMCAST(writev, ssize_t, (int, const struct iovec *, int));

  DLSYMCAST(select, int, (int, fd_set *, fd_set *, fd_set *, struct timeval*));
  DLSYMCAST(pselect, int, (int, fd_set *, fd_set *, fd_set *, struct timespec*, const sigset_t*));
  DLSYMCAST(poll, int, (struct pollfd *, int, int));
  DLSYMCAST(ppoll, int, (struct pollfd *, int, int, const struct timespec*, const sigset_t*)); // TBI

  DLSYMCAST(epoll_wait, int, (int, struct epoll_event*, int, int));
  DLSYMCAST(epoll_pwait, int, (int, struct epoll_event*, int, int, const sigset_t*));

  DLSYMCAST(dup, int, (int));
  DLSYMCAST(dup2, int, (int, int));

  DLSYMCAST(sendfile, ssize_t, (int, int, off_t *, size_t));

  DLSYMCAST(ioctl, int, (int fd, unsigned long, ...));
  DLSYMCAST(fcntl, int, (int, int, ...));

  g_sock_map.clear();

  rskt_shim_initialised = 0xf00ff00d;

  memset(&ZERO_SOCK, 0, sizeof(ZERO_SOCK));
  ZERO_SOCK.fd       = -1;
  ZERO_SOCK.sockp[0] = -1;
  ZERO_SOCK.sockp[1] = -1;
  ZERO_SOCK.can_read = true;
  ZERO_SOCK.can_write= true;
  ZERO_SOCK.laddr.sin_family     = AF_INET;

  rskt_shim_init_RSKT();
  g_my_destid = htonl(RSKT_shim_rskt_get_my_destid());
  ZERO_SOCK.laddr.sin_addr.s_addr = ntohs(g_my_destid);

  char* cRDMA_LL = getenv("RDMA_LL");
  if (cRDMA_LL != NULL && atoi(cRDMA_LL) > 6) g_debug = 1;

  pthread_mutex_unlock(&g_rskt_shim_mutex);
}

static void spawn_accept_minder_thr(const int sockfd);

extern "C"
int socket(int socket_family, int socket_type, int protocol)
{
  int tmp_sock = glibc_socket(socket_family, socket_type, protocol);

  do {
    if (tmp_sock < 0) break;
    if (socket_family != AF_INET) break;
    if (protocol != IPPROTO_TCP) break;
    if ((socket_type & SOCK_STREAM) == 0) break;

    Dprintf("TCPv4 sock %d STORE\n", tmp_sock);

    SocketTracker_t sock_tr = ZERO_SOCK;

    sock_tr.fd = tmp_sock;
    sock_tr.acc_list.clear();
    sock_tr.acc_thr_list.clear();
    pthread_mutex_init(&sock_tr.acc_mutex, NULL);

    if (socket_type & SOCK_NONBLOCK) sock_tr.nonblock++;

    pthread_mutex_lock(&g_map_mutex);
      g_sock_map[tmp_sock] = sock_tr;
    pthread_mutex_unlock(&g_map_mutex);
  } while(0);

  return tmp_sock;
}

extern "C"
int close(int fd)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    Dprintf("TCPv4 sock %d ERASE\n", fd);
 
    SocketTracker_t& sock_tr = it->second;

    if (sock_tr.fd >= 0) glibc_close(sock_tr.fd);
    if (sock_tr.sockp[0] != -1) glibc_close(sock_tr.sockp[0]);
    if (sock_tr.sockp[1] != -1) glibc_close(sock_tr.sockp[1]);
    if (sock_tr.rsock != NULL) RSKT_shim_rskt_close(sock_tr.rsock);
    pthread_mutex_lock(&sock_tr.acc_mutex);
      for (int i = 0; i < sock_tr.acc_thr_list.size(); i++) 
      pthread_kill(sock_tr.acc_thr_list[i], SIGUSR1);
    pthread_mutex_unlock(&sock_tr.acc_mutex);

    pthread_mutex_unlock(&g_map_mutex);

    g_sock_map.erase(it);

    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_close(fd);
}

// int shim_rskt_bind(void* listen_sock, const uint16_t destid /*=0*/, const uint16_t port);

///< \note This will ONLY bind to 0.0.x.y, all other addresses passthru
extern "C"
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  assert(addr);
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) do {
    if (addrlen != sizeof(struct sockaddr_in)) break;

    const struct sockaddr_in* addr_v4 = (struct sockaddr_in*)addr;
    if (addr_v4->sin_family != AF_INET) break;

    int ret = 0;

    union {
      uint32_t saddr;
      uint8_t  bytes[4];
    } u;

    u.saddr = ntohl(addr_v4->sin_addr.s_addr);
    if (u.bytes[3] != 0 || u.bytes[2] != 0) break; // We want 0.0.x.y
    if (((uint16_t)u.bytes[1] + (uint16_t)u.bytes[0]) == 0) break; // We want 0.0.x.y

    Dprintf("TCPv4 sock %d bind to RIO addr\n", sockfd);

    int rc = RSKT_shim_rskt_bind(it->second.rsock, g_my_destid, ntohs(addr_v4->sin_port));
    if (rc) {
      errno = EINVAL;
      ret = -1;
    } else
      it->second.laddr = *addr_v4;

    pthread_mutex_unlock(&g_map_mutex);
    return ret;
  } while(0);
  pthread_mutex_unlock(&g_map_mutex);

  // TODO
  // 1. make a socketpair
  // 2. dup2 sockfd->sockp[0]
  // 3. make a RSKT and put it in g_sock_map[sockfd]
  // 4. bind RSKT to ntohs(addr_v4->sin_port)
  return glibc_bind(sockfd, addr, addrlen); // temporary
}

extern "C"
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  assert(addr);
  assert(addrlen);

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    *addrlen = sizeof(struct sockaddr_in);
    memcpy(addr, &it->second.raddr, sizeof(struct sockaddr_in));
    
    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  } 
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_getpeername(sockfd, addr, addrlen);
}

extern "C"
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  assert(addr);
  assert(addrlen);

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    *addrlen = sizeof(struct sockaddr_in);
    memcpy(addr, &it->second.laddr, sizeof(struct sockaddr_in));
    
    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  } 
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_getpeername(sockfd, addr, addrlen);
}

static void make_sockpair(const int sockfd, int sockp[2]) throw()
{
  if (0 != socketpair(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, sockp)) {
    glibc_close(sockp[0]); glibc_close(sockp[1]);
    throw std::runtime_error("RSKT Shim: socketpair failed!");
  }

  if (sockfd >=0 && -1 == glibc_dup2(sockp[0], sockfd)) {
    static char tmp[129] = {0};
    const int saved_errno = errno;
    glibc_close(sockp[0]); glibc_close(sockp[1]);
    snprintf(tmp, 128, "RSKT Shim: dup2 failed: %s", strerror(saved_errno));
    throw std::runtime_error(tmp);
  }

  if (glibc_fcntl(sockp[0], F_SETFL, glibc_fcntl(sockp[0], F_GETFL, 0) | O_NONBLOCK) ||
      glibc_fcntl(sockp[1], F_SETFL, glibc_fcntl(sockp[1], F_GETFL, 0) | O_NONBLOCK)) {
    static char tmp[129] = {0};
    const int saved_errno = errno;
    glibc_close(sockp[0]); glibc_close(sockp[1]);
    snprintf(tmp, 128, "RSKT Shim: fcntl(socketpair, O_NONBLOCK) failed: %s", strerror(saved_errno));
    throw std::runtime_error(tmp);
  }
}

///< \note This will ONLY connect to 0.0.x.y, all other addresses passthru
extern "C"
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  assert(addr);
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) do {
    if (addrlen != sizeof(struct sockaddr_in)) break;

    const struct sockaddr_in* addr_v4 = (struct sockaddr_in*)addr;
    if (addr_v4->sin_family != AF_INET) break;

    union {
      uint32_t saddr;
      uint8_t  bytes[4];
    } u;

    u.saddr = ntohl(addr_v4->sin_addr.s_addr);
    if (u.bytes[3] != 0 || u.bytes[2] != 0) break; // We want 0.0.x.y
    if (((uint16_t)u.bytes[1] + (uint16_t)u.bytes[0]) == 0) break; // We want 0.0.x.y

    uint16_t destid = u.bytes[1] << 8 | u.bytes[0];
    Dprintf("TCPv4 sock %d connect to RIO destid %u port %d\n", sockfd, destid, ntohs(addr_v4->sin_port));

    it->second.rsock = RSKT_shim_rskt_socket();
    assert(it->second.rsock);

    int rc = RSKT_shim_rskt_connect(it->second.rsock, destid, ntohs(addr_v4->sin_port));
    if (rc) {
      //RSKT_shim_rskt_close(it->second.rsock); it->second.rsock = NULL;
      pthread_mutex_unlock(&g_map_mutex);
      errno = ECONNREFUSED;
      return -1;
    }

    int sockp[2] = { -1, -1 };
    try { make_sockpair(sockfd, sockp); }
    catch(std::runtime_error ex) {
      pthread_mutex_unlock(&g_map_mutex);
      throw ex;
    }

    SocketTracker_t& sock_tr = g_sock_map[sockfd];

    sock_tr.sockp[0] = sockp[0]; sock_tr.sockp[1] = sockp[1];
    memcpy(&sock_tr.raddr, addr_v4, sizeof(sock_tr.raddr));

    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  } while(0);
  pthread_mutex_unlock(&g_map_mutex);

  // TODO
  // 3. make a RSKT and put it in g_sock_map[sockfd]
  // 4. connect RSKT to destid=(u.raddr & 0xFFFF) port ntohs(addr_v4->sin_port)
  // 5. if socket marked nonblocking by socket/ioctl/fcntl then
  // 6. ... ??crickets?? ... rskt_connect is blocking but should return fast
  return glibc_connect(sockfd, addr, addrlen); // temporary
}

/// XXX thttpd does socket/bind/poll....accept

// This seems for documentation purposes for RSKT... Hmm
extern "C"
int shutdown(int sockfd, int how)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    if (how == SHUT_WR) it->second.can_write = false;
    if (how == SHUT_RD) it->second.can_read  = false;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_shutdown(sockfd, how);
}

extern "C"
ssize_t write(int fd, const void *buf, size_t count)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    int rc = 0;
    Dprintf("TCPv4 sock %d write %d bytes.%s\n", fd, count, it->second.borked? " BORKED" :"");
    if (it->second.borked) {
      errno = EPIPE;
      rc = -1;
    } else
      rc = RSKT_shim_rskt_write(it->second.rsock, (void*)buf, count);
    pthread_mutex_unlock(&g_map_mutex);
    if (rc < 0) errno = EPIPE;
    return !rc? count: -1;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_write(fd, buf, count);
}

extern "C"
ssize_t read(int fd, void *buf, size_t count)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    int rc = 0;
    Dprintf("TCPv4 sock %d read buffer=%d bytes.%s\n", fd, count, it->second.borked? " BORKED" :"");
    if (it->second.borked) {
      errno = EINVAL;
      rc = -1;
    } else {
      rc = RSKT_shim_rskt_read(it->second.rsock, (void*)buf, count);
      Dprintf("TCPv4 sock %d rskt_read %d bytes: %s.\n", fd, rc, strerror(errno));
    }
    pthread_mutex_unlock(&g_map_mutex);
    return rc >= 0? rc: 0; // 0 = read fron closed socket
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_read(fd, buf, count);
}

static void* select_thr(void* arg)
{
  assert(arg);
  SocketTracker_t* sock_tr = (SocketTracker_t*)arg;

  Dprintf("TCPv4 sock %d SELECT minder thread.\n", sock_tr->fd);

  while (!sock_tr->stop_req) {
    const int r = RSKT_shim_rskt_get_avail_bytes(sock_tr->rsock);
    if (r != 0) {
      Dprintf("TCPv4 sock %d SELECT minder thread avail_bytes=%d.\n", sock_tr->fd, r);
      glibc_write(sock_tr->sockp[1], (r > 0? "r" : "e"), 1);
      break;
    }
    struct timespec tv = { 0, 1};
    nanosleep(&tv, NULL);
  }
  
  return NULL;
}

typedef struct {
  int              fd;
  bool             listening;
  pthread_t        wkr;
  SocketTracker_t* sock_tr;
} KnownFd_t;

extern "C"
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  if (readfds == NULL) // We can only do readable sockets!
    return glibc_select(nfds, readfds, writefds, exceptfds, timeout);

  int max_known_fd = -1;

  std::vector<KnownFd_t> known_fds;

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.begin();
  for(; it != g_sock_map.end(); it++) {
    if (it->second.sockp[0] == -1) continue;
    if (it->first > max_known_fd) max_known_fd = it->first;
    if (! FD_ISSET(it->first, readfds)) continue;

    KnownFd_t tmp;
    tmp.wkr       = 0;
    tmp.fd        = it->first;
    tmp.listening = it->second.listening;
    tmp.sock_tr   = &it->second;
    known_fds.push_back(tmp);
  }

  int min_known_fd = max_known_fd + 1;
  for (int i = 0; i < known_fds.size(); i++) {
    if (min_known_fd > known_fds[i].fd) min_known_fd = known_fds[i].fd;
  }

  if (min_known_fd >= nfds || known_fds.size() == 0) { // All our sockets are too "high" or none in common
    pthread_mutex_unlock(&g_map_mutex);
    return glibc_select(nfds, readfds, writefds, exceptfds, timeout);
  }

  for (int i = 0; i < known_fds.size(); i++) {
    known_fds[i].sock_tr->stop_req = 0;

    if (known_fds[i].listening) {
      spawn_accept_minder_thr(known_fds[i].fd);
      continue;
    }
 
    if (pthread_create(&known_fds[i].wkr, NULL, select_thr, known_fds[i].sock_tr) < 0) {
      static char tmp[129] = {0};
      snprintf(tmp, 128, "RSKT Shim: pthread_create failed: %s", strerror(errno));
      throw std::runtime_error(tmp);
    }

  } // END for known_fds

  pthread_mutex_unlock(&g_map_mutex);

  int nselect = glibc_select(nfds, readfds, writefds, exceptfds, timeout);

  for (int i = 0; i < known_fds.size(); i++) known_fds[i].sock_tr->stop_req = 1;

  usleep(1);

  // Suck standin from socketpair
  for (int i = 0; i < known_fds.size(); i++) {
    if (!FD_ISSET(known_fds[i].fd, readfds)) continue;
    char c = 0;
    glibc_read(known_fds[i].sock_tr->sockp[0], &c, 1);
    if (c == 'e') known_fds[i].sock_tr->borked = 1;
  }

  for (int i = 0; i < known_fds.size(); i++) {
    if (known_fds[i].wkr) pthread_kill(known_fds[i].wkr, SIGUSR1);
  }

  Dprintf("TCPv4 select %d hijacked fd(s) END => %d\n", known_fds.size(), nselect);

  return nselect;
}

extern "C"
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
  assert(fds);

  std::vector<KnownFd_t> known_fds;

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.begin();
  for(; it != g_sock_map.end(); it++) {
    if (it->second.sockp[0] == -1) continue;

    for (int i = 0; i < nfds; i++) {
      if (fds[i].fd != it->second.fd) continue;
      KnownFd_t tmp;
      tmp.wkr       = 0;
      tmp.fd        = it->first;
      tmp.listening = it->second.listening;
      tmp.sock_tr   = &it->second;
      known_fds.push_back(tmp);
      break;
    }
  }

  if (known_fds.size() == 0) {
    pthread_mutex_unlock(&g_map_mutex);
    return glibc_poll(fds, nfds, timeout);
  }

  for (int i = 0; i < known_fds.size(); i++) {
    known_fds[i].sock_tr->stop_req = 0;

    if (known_fds[i].listening) {
      spawn_accept_minder_thr(known_fds[i].fd);
      continue;
    }

    if (pthread_create(&known_fds[i].wkr, NULL, select_thr, known_fds[i].sock_tr) < 0) {
      static char tmp[129] = {0};
      snprintf(tmp, 128, "RSKT Shim: pthread_create failed: %s", strerror(errno));
      throw std::runtime_error(tmp);
    }
  } // END for known_fds

  pthread_mutex_unlock(&g_map_mutex);

  int npoll = glibc_poll(fds, nfds, timeout);

  for (int i = 0; i < known_fds.size(); i++) known_fds[i].sock_tr->stop_req = 1;

  usleep(1);

  // Suck standin from socketpair
  for (int i = 0; i < known_fds.size(); i++) {
    bool data_ready = false;
    for (int j = 0; i < nfds; j++) {
      if (fds[j].fd != known_fds[i].fd) continue;
      if (fds[j].revents & POLLIN != POLLIN) continue;
      data_ready = true;
      break;
    }

    if (!data_ready) continue;

    char c = 0;
    glibc_read(known_fds[i].sock_tr->sockp[0], &c, 1);
    if (c == 'e') known_fds[i].sock_tr->borked = 1;
  }

  for (int i = 0; i < known_fds.size(); i++) {
    if (known_fds[i].wkr) pthread_kill(known_fds[i].wkr, SIGUSR1);
  }

  return npoll;
}

// Use cases for nonblock:
//   int flags = fcntl(fd, F_GETFL, 0);
//   fcntl(fd, F_SETFL, flags | O_NONBLOCK);
// or
//   int opt = 1;
//   ioctl(fd, FIONBIO, &opt);

extern "C"
int ioctl(int fd, unsigned long request, ...)
{
  int ret = 0;

  va_list argp;
  va_start(argp, request);

  bool my_fd = false;
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    my_fd = true;
    if (request == FIONBIO) {
      int opt = va_arg (argp, int);
      it->second.nonblock = !!opt;
      Dprintf("TCPv4 sock %d ioctl(FIONBIO, %d)\n", fd, !!opt);
    }
  }
  pthread_mutex_unlock(&g_map_mutex);
  
  if (!my_fd) ret = glibc_ioctl(fd, request, argp);

  va_end(argp);

  return ret;
}

extern "C"
int fcntl(int fd, int cmd, ... /* arg */ )
{
  int ret = 0;

  va_list argp;
  va_start(argp, cmd);

  bool my_fd = false;
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    my_fd = true;
    if (cmd == F_SETFL) {
      int flags = va_arg (argp, int);
      if (flags & O_NONBLOCK) it->second.nonblock = 1;
      Dprintf("TCPv4 sock %d fcntl(O_NONBLOCK)\n", fd);
    }
  }
  pthread_mutex_unlock(&g_map_mutex);
  
  if (!my_fd) ret = glibc_fcntl(fd, cmd, argp);

  va_end(argp);

  return ret;
}

extern "C"
int listen(int sockfd, int backlog)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    int ret = 0;
    int rc = RSKT_shim_rskt_listen(it->second.rsock, backlog);
    if (rc) {
      errno = EINVAL;
      ret = -1;
    }
    it->second.listening = true;
    pthread_mutex_unlock(&g_map_mutex);
    Dprintf("TCPv4 sock %d listening\n", listen);
    return ret;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_listen(sockfd, backlog);
}

static inline int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, bool nonblocking);

extern "C"
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  assert(addr);
  assert(addrlen);

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    return _accept(sockfd, addr, addrlen, false); // LOCKED!
  }
  pthread_mutex_unlock(&g_map_mutex);
  
  return glibc_accept(sockfd, addr, addrlen);
}

extern "C"
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
  assert(addr);
  assert(addrlen);

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    return _accept(sockfd, addr, addrlen, ((flags & SOCK_NONBLOCK) == SOCK_NONBLOCK)); // LOCKED!
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_accept4(sockfd, addr, addrlen, flags);
}

// int shim_rskt_accept(void* listen_sock, void* accept_socket, uint16_t* remote_destid, uint16_t* remote_port);

static void* accept_thr(void* arg)
{
  assert(arg);

  SocketTracker_t* sock_tr = (SocketTracker_t*)arg;

  const int sockfd = sock_tr->fd;

  Dprintf("TCPv4 sock %d ACCEPT minder thread.\n", sockfd);

  Accepted_t res; memset(&res, 0, sizeof(res));

  while (!sock_tr->stop_req) {
    void* arsock = RSKT_shim_rskt_socket();
    assert (arsock);

    uint16_t remote_destid = 0xFFFF;
    uint16_t remote_port = 0;
    const int rc = RSKT_shim_rskt_accept(sock_tr->rsock, arsock, &remote_destid, &remote_port);
    if (rc) {
      res.rskt_errno = errno ?: EINVAL;
      RSKT_shim_rskt_close(arsock);

      int sockp1 = -1;
      pthread_mutex_lock(&g_map_mutex);
        std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
        if (it != g_sock_map.end()) {
          pthread_mutex_lock(&it->second.acc_mutex);
            it->second.acc_list.push_back(res);
            sockp1 = it->second.sockp[1];
          pthread_mutex_unlock(&it->second.acc_mutex);
        }
      pthread_mutex_unlock(&g_map_mutex);

      assert(sockp1 != -1);
      glibc_write(sockp1, "e", 1);
      break;
    } 

    assert(remote_destid != 0xFFFF);

    res.rsock    = arsock;
    res.r_destid = remote_destid;
    res.r_port   = remote_port;

    int sockp1 = -1;
    pthread_mutex_lock(&g_map_mutex);
      std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
      if (it != g_sock_map.end()) {
        pthread_mutex_lock(&it->second.acc_mutex);
          it->second.acc_list.push_back(res);
          sockp1 = it->second.sockp[1];
        pthread_mutex_unlock(&it->second.acc_mutex);
      }
    pthread_mutex_unlock(&g_map_mutex);

    assert(sockp1 != -1);
    glibc_write(sockp1, "r", 1);
  }

  return NULL;
}

static void spawn_accept_minder_thr(const int sockfd)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  assert(it != g_sock_map.end());

  SocketTracker_t& sock_tr = it->second;

  if (!sock_tr.nonblock || !sock_tr.listening) {
    pthread_mutex_unlock(&g_map_mutex);
    return;
  }

  bool have_minder = false;
  pthread_mutex_lock(&sock_tr.acc_mutex);
    if (sock_tr.acc_thr_list.size() > 0) have_minder = true;
  pthread_mutex_unlock(&sock_tr.acc_mutex);

  if (have_minder) {
    pthread_mutex_unlock(&g_map_mutex);
    return;
  }

  // NO minder, start minder thread -- XXX close must kill this!
  pthread_t wkr;
  if (pthread_create(&wkr, NULL, accept_thr, &it->second) < 0) {
    static char tmp[129] = {0};
    snprintf(tmp, 128, "RSKT Shim: pthread_create failed: %s", strerror(errno));
    throw std::runtime_error(tmp);
  }

  pthread_mutex_lock(&sock_tr.acc_mutex);
    sock_tr.acc_thr_list.push_back(wkr);
  pthread_mutex_unlock(&sock_tr.acc_mutex);

  pthread_mutex_unlock(&g_map_mutex);

  Dprintf("TCPv4 sock %d spawned accept minder thread.\n", sockfd);
}

///< \note This is entered in LOCKED context wrt g_map_mutex
static inline int
_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, bool nonblocking /* IGNORED, socketpair marked nonblock*/)
{
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  assert(it != g_sock_map.end());

  SocketTracker_t& sock_tr = it->second;

  const bool nonblock = it->second.nonblock;

  const int sockp0 = it->second.sockp[0];

  if (nonblock) {
    Accepted_t res;
    bool pending = false;

    pthread_mutex_lock(&sock_tr.acc_mutex);
    if (sock_tr.acc_list.size()) {
      pending = true;
      res = sock_tr.acc_list.front();
      sock_tr.acc_list.erase(sock_tr.acc_list.begin());
    }
    pthread_mutex_unlock(&sock_tr.acc_mutex);
    
    pthread_mutex_unlock(&g_map_mutex);

    if (pending) {
      char c = '\0';
      glibc_read(sockp0, &c, 1);

      if (c == 0) {
        errno = res.rskt_errno?: EINVAL; return -1;
      }

      if (res.rskt_errno) { // Bizarre... glibc_read might have not returned anything
        errno = res.rskt_errno; return -1;
      }

      // OK, we have a connection....
      int sockp[2] = { -1, -1 };
      make_sockpair(-1, sockp); // will throw!
      SocketTracker_t sock_tr_new = ZERO_SOCK;
      sock_tr_new.acc_list.clear();
      sock_tr_new.acc_thr_list.clear();
      pthread_mutex_init(&sock_tr_new.acc_mutex, NULL);
      sock_tr_new.fd = dup(sockp[0]);
      sock_tr_new.sockp[0] = sockp[0]; sock_tr.sockp[1] = sockp[1];
      sock_tr_new.rsock = res.rsock;

      sock_tr_new.raddr.sin_family      = AF_INET;
      sock_tr_new.raddr.sin_port        = htons(res.r_port);
      sock_tr_new.raddr.sin_addr.s_addr = htonl(res.r_destid);

      // XXX sock_tr_new.laddr.sin_port = ??? // local port of accepted RSKT?
 
      pthread_mutex_lock(&g_map_mutex);
        g_sock_map[sock_tr_new.fd] = sock_tr_new;
      pthread_mutex_unlock(&g_map_mutex);
   
      return sock_tr_new.fd;
    } // END if pending
    
    spawn_accept_minder_thr(sockfd);

    errno = EAGAIN;
    return -1;
  } // END if nonblock

// BLOCKING accept

  pthread_mutex_unlock(&g_map_mutex);

  uint16_t remote_destid = 0;
  uint16_t remote_port   = 0;

  void* arsock = RSKT_shim_rskt_socket();
  assert(arsock);

  int rc = RSKT_shim_rskt_accept(it->second.rsock, arsock, &remote_destid, &remote_port); // BLOCKING!!
  if (rc) {
    RSKT_shim_rskt_close(arsock);
    errno = EINVAL;
    return -1;
  }

  assert(remote_destid != 0xFFFF);

  // OK, we have a connection....
  int sockp[2] = { -1, -1 };
  make_sockpair(-1, sockp); // will throw!
  SocketTracker_t sock_tr_new = ZERO_SOCK;
  sock_tr_new.acc_list.clear();
  sock_tr_new.acc_thr_list.clear();
  pthread_mutex_init(&sock_tr_new.acc_mutex, NULL);
  sock_tr_new.fd = dup(sockp[0]);
  sock_tr_new.sockp[0] = sockp[0]; sock_tr.sockp[1] = sockp[1];
  sock_tr_new.rsock = arsock;

  sock_tr_new.raddr.sin_family      = AF_INET;
  sock_tr_new.raddr.sin_port        = htons(remote_port);
  sock_tr_new.raddr.sin_addr.s_addr = htonl(remote_destid);

  // XXX sock_tr_new.laddr.sin_port = ??? // local port of accepted RSKT?

  pthread_mutex_lock(&g_map_mutex);
    g_sock_map[sock_tr_new.fd] = sock_tr_new;
  pthread_mutex_unlock(&g_map_mutex);

  return sock_tr_new.fd;
}
