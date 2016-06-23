#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>

#include <map>
#include <vector>
#include <stdexcept>

volatile uint32_t rskt_shim_initialised = 0;
volatile uint32_t rskt_shim_RSKT_initialised = 0;

void rskt_shim_main() __attribute__ ((constructor));

static pthread_mutex_t g_rskt_shim_mutex = PTHREAD_MUTEX_INITIALIZER; 

static pthread_mutex_t g_map_mutex = PTHREAD_MUTEX_INITIALIZER; 

static uint16_t g_my_destid = 0xFFFF;

typedef struct {
  int                fd; ///< For documentation purposes
  int                sockp[2];
  int                nonblock;
  bool               borked; ///< Closed from beneath us by librskt
  bool               can_read;
  bool               can_write;
  volatile int       stop_req;
  struct sockaddr_in laddr;
  struct sockaddr_in daddr;
  void*              rsock;
} SocketTracker_t;

static SocketTracker_t ZERO_SOCK;

static std::map<int, SocketTracker_t> g_sock_map;

#define UNDERSCORE // "_"

#define DECLARE(name, ret, args) static ret (*glibc_##name) args

DECLARE(socket, int, (int, int, int));
DECLARE(bind, int, (int, const struct sockaddr*, socklen_t));
DECLARE(listen, int, (int, int)); // TBI
DECLARE(accept, int, (int, struct sockaddr*, socklen_t*)); // TBI
DECLARE(accept4, int,(int, struct sockaddr*, socklen_t*, int)); // TBI
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

DECLARE(select, int, (int, fd_set *, fd_set *, fd_set *, struct timeval*)); // TBI
DECLARE(pselect, int, (int, fd_set *, fd_set *, fd_set *, struct timespec*, const sigset_t*)); // TBI
DECLARE(poll, int, (struct pollfd *, int, int)); // TBI

DECLARE(epoll_wait, int, (int, struct epoll_event*, int, int)); // TBI
DECLARE(epoll_pwait, int, (int, struct epoll_event*, int, int, const sigset_t*)); // TBI

DECLARE(dup, int, (int)); // TBI
DECLARE(dup2, int, (int, int)); // TBI

DECLARE(sendfile, ssize_t, (int, int, off_t *, size_t)); // TBI

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

void rskt_shim_init_RSKT()
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

  DLSYMCAST(epoll_wait, int, (int, struct epoll_event*, int, int));
  DLSYMCAST(epoll_pwait, int, (int, struct epoll_event*, int, int, const sigset_t*));

  DLSYMCAST(dup, int, (int));
  DLSYMCAST(dup2, int, (int, int));

  DLSYMCAST(sendfile, ssize_t, (int, int, off_t *, size_t));

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

  pthread_mutex_unlock(&g_rskt_shim_mutex);
}

int socket(int socket_family, int socket_type, int protocol)
{
  int tmp_sock = glibc_socket(socket_family, socket_type, protocol);

  do {
    if (tmp_sock < 0) break;
    if (socket_family != AF_INET) break;
    if (protocol != IPPROTO_TCP) break;
    if ((socket_type & SOCK_STREAM) == 0) break;

    printf("TCPv4 sock %d STORE\n", tmp_sock);

    pthread_mutex_lock(&g_map_mutex);
    g_sock_map[tmp_sock] = ZERO_SOCK;
    g_sock_map[tmp_sock].fd = tmp_sock;
    if (socket_type & SOCK_NONBLOCK) g_sock_map[tmp_sock].nonblock++;
    pthread_mutex_unlock(&g_map_mutex);
  } while(0);

  return tmp_sock;
}

int close(int fd)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    printf("TCPv4 sock %d ERASE\n", fd);

    if (it->second.sockp[0] != -1) glibc_close(it->second.sockp[0]);
    if (it->second.sockp[1] != -1) glibc_close(it->second.sockp[1]);
    if (it->second.rsock != NULL) RSKT_shim_rskt_close(it->second.rsock);
    g_sock_map.erase(it);

    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_close(fd);
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
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

    printf("TCPv4 sock %d bind to RIO addr\n", sockfd);
  } while(0);
  pthread_mutex_unlock(&g_map_mutex);

  // TODO
  // 1. make a socketpair
  // 2. dup2 sockfd->sockp[0]
  // 3. make a RSKT and put it in g_sock_map[sockfd]
  // 4. bind RSKT to ntohs(addr_v4->sin_port)
  return glibc_bind(sockfd, addr, addrlen); // temporary
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  assert(addr);
  assert(addrlen);

  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(sockfd);
  if (it != g_sock_map.end()) {
    *addrlen = sizeof(struct sockaddr_in);
    memcpy(addr, &it->second.daddr, sizeof(struct sockaddr_in));
    
    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  } 
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_getpeername(sockfd, addr, addrlen);
}

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
    printf("TCPv4 sock %d connect to RIO destid %u port %d\n", sockfd, destid, ntohs(addr_v4->sin_port));

    it->second.rsock = RSKT_shim_rskt_socket();
    assert(it->second.rsock);

    int rc = RSKT_shim_rskt_connect(it->second.rsock, destid, ntohs(addr_v4->sin_port));
    if (rc) {
      RSKT_shim_rskt_close(it->second.rsock); it->second.rsock = NULL;
      pthread_mutex_unlock(&g_map_mutex);
      errno = ECONNREFUSED;
      return -1;
    }

    int sockp[2] = { -1, -1 };
    if (0 != socketpair(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, sockp)) {
      RSKT_shim_rskt_close(it->second.rsock); it->second.rsock = NULL;
      glibc_close(sockp[0]); glibc_close(sockp[1]);
      pthread_mutex_unlock(&g_map_mutex);
      throw std::runtime_error("RSKT Shim: socketpair failed!");
    }
    if (-1 == glibc_dup2(sockp[0], sockfd)) {
      static char tmp[129] = {0};
      const int saved_errno = errno;
      RSKT_shim_rskt_close(it->second.rsock); it->second.rsock = NULL;
      glibc_close(sockp[0]); glibc_close(sockp[1]);
      pthread_mutex_unlock(&g_map_mutex);
      snprintf(tmp, 128, "RSKT Shim: dup2 failed: %s", strerror(saved_errno));
      throw std::runtime_error(tmp);
    }

    SocketTracker_t& sock_tr = g_sock_map[sockfd];

    sock_tr.sockp[0] = sockp[0]; sock_tr.sockp[1] = sockp[1];
    memcpy(&sock_tr.daddr, addr_v4, sizeof(sock_tr.daddr));

    pthread_mutex_unlock(&g_map_mutex);
    return 0;
  } while(0);
  pthread_mutex_unlock(&g_map_mutex);

  // TODO
  // 3. make a RSKT and put it in g_sock_map[sockfd]
  // 4. connect RSKT to destid=(u.daddr & 0xFFFF) port ntohs(addr_v4->sin_port)
  // 5. if socket marked nonblocking by socket/ioctl/fcntl then
  // 6. ... ??crickets?? ... rskt_connect is blocking but should return fast
  return glibc_connect(sockfd, addr, addrlen); // temporary
}

/// XXX thttpd does socket/bind/poll....accept

// This seems for documentation purposes for RSKT... Hmm
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

ssize_t write(int fd, const void *buf, size_t count)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    int rc = 0;
    printf("TCPv4 sock %d write %d bytes.%s\n", fd, count, it->second.borked? " BORKED" :"");
    if (it->second.borked) {
      errno = EINVAL;
      rc = -1;
    } else
      rc = RSKT_shim_rskt_write(it->second.rsock, (void*)buf, count);
    pthread_mutex_unlock(&g_map_mutex);
    return !rc? count: -1;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_write(fd, buf, count);
}

ssize_t read(int fd, void *buf, size_t count)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    int rc = 0;
    printf("TCPv4 sock %d read buffer=%d bytes.%s\n", fd, count, it->second.borked? " BORKED" :"");
    if (it->second.borked) {
      errno = EINVAL;
      rc = -1;
    } else {
      rc = RSKT_shim_rskt_read(it->second.rsock, (void*)buf, count);
      printf("TCPv4 sock %d rskt_read %d bytes: %s.\n", fd, rc, strerror(errno));
    }
    pthread_mutex_unlock(&g_map_mutex);
    return rc >= 0? rc: -1;
  }
  pthread_mutex_unlock(&g_map_mutex);

  return glibc_read(fd, buf, count);
}

static void* select_thr(void* arg)
{
  assert(arg);
  SocketTracker_t* sock_tr = (SocketTracker_t*)arg;

  printf("TCPv4 sock %d SELECT minder thread.\n", sock_tr->fd);

  while (!sock_tr->stop_req) {
    const int r = RSKT_shim_rskt_get_avail_bytes(sock_tr->rsock);
    if (r != 0) {
      printf("TCPv4 sock %d SELECT minder thread avail_bytes=%d.\n", sock_tr->fd, r);
      glibc_write(sock_tr->sockp[1], (r > 0? "r" : "q"), 1);
      break;
    }
    struct timespec tv = { 0, 1};
    nanosleep(&tv, NULL);
  }
  
  return NULL;
}

typedef struct {
  int              fd;
  pthread_t        wkr;
  SocketTracker_t* sock_tr;
} KnownFd_t;

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
    tmp.fd      = it->first;
    tmp.sock_tr = &it->second;
    known_fds.push_back(tmp);
  }

  int min_known_fd = max_known_fd + 1;
  for (int i = 0; i < known_fds.size(); i++) {
    if (min_known_fd > known_fds[i].fd) min_known_fd = known_fds[i].fd;
  }
  pthread_mutex_unlock(&g_map_mutex);

  if (min_known_fd >= nfds || known_fds.size() == 0) // All our sockets are too "high" or none in common
    return glibc_select(nfds, readfds, writefds, exceptfds, timeout);

  pthread_mutex_lock(&g_map_mutex);
  for (int i = 0; i < known_fds.size(); i++) {
    known_fds[i].sock_tr->stop_req = 0;

    if (pthread_create(&known_fds[i].wkr, NULL, select_thr, known_fds[i].sock_tr) < 0) {
      static char tmp[129] = {0};
      snprintf(tmp, 128, "RSKT Shim: pthread_create failed: %s", strerror(errno));
      throw std::runtime_error(tmp);
    }
  }
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
    pthread_kill(known_fds[i].wkr, SIGUSR1);
  }

  return nselect;
}
