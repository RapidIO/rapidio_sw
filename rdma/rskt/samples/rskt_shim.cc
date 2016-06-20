#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>

#include <map>

volatile int rskt_shim_initialised = 0;
void rskt_shim_main() __attribute__ ((constructor));

static pthread_mutex_t g_rskt_shim_mutex = PTHREAD_MUTEX_INITIALIZER; 

static pthread_mutex_t g_map_mutex = PTHREAD_MUTEX_INITIALIZER; 

typedef struct {
  int  sockp[2];
  struct sockaddr_in laddr;
  struct sockaddr_in daddr;
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
         
DECLARE(close, int, (int));

DECLARE(setsockopt, int, (int, int, int, const void*, socklen_t));
DECLARE(getsockopt, int, (int, int, int, void*));

DECLARE(getpeername, int, (int, struct sockaddr*, socklen_t*));
DECLARE(getsockname, int, (int, struct sockaddr*, socklen_t*));

DECLARE(read, ssize_t, (int, void *, size_t));
DECLARE(pread, ssize_t, (int, void *, size_t, off_t));
DECLARE(recv, ssize_t, (int, void *, size_t, int));
DECLARE(readv, ssize_t, (int, const struct iovec *, int));

DECLARE(write, ssize_t, (int, const void *, size_t));
DECLARE(pwrite, ssize_t, (int, const void *, size_t, off_t));
DECLARE(send, ssize_t, (int, const void *, size_t, int));
DECLARE(writev, ssize_t, (int, const struct iovec *, int));

DECLARE(select, int, (int, fd_set *, fd_set *, fd_set *, struct timeval*));
DECLARE(pselect, int, (int, fd_set *, fd_set *, fd_set *, struct timespec*, const sigset_t*));
DECLARE(poll, int, (struct pollfd *, int, int));

DECLARE(epoll_wait, int, (int, struct epoll_event*, int, int));
DECLARE(epoll_pwait, int, (int, struct epoll_event*, int, int, const sigset_t*));

DECLARE(dup, int, (int));
DECLARE(dup2, int, (int, int));

DECLARE(sendfile, ssize_t, (int, int, off_t *, size_t));

static inline void errx(const char* msg) { if(msg) fprintf(stderr, "%s\n", msg); _exit(42); }

#define DLSYMCAST(name, ret, args)  do { \
	if ((glibc_##name = (ret (*) args) dlsym(dh, UNDERSCORE #name)) == NULL)		\
		errx("RSKT Shim: Failed to get " #name "() address");	\
} while(0);

void rskt_shim_main() __attribute__ ((constructor));

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

  memset(&ZERO_SOCK, 0, sizeof(ZERO_SOCK));
  ZERO_SOCK.sockp[0] = -1;
  ZERO_SOCK.sockp[1] = -1;

  rskt_shim_initialised = 0xf00ff00d;
  pthread_mutex_unlock(&g_rskt_shim_mutex);
}

int socket(int socket_family, int socket_type, int protocol)
{
  int tmp_sock = glibc_socket(socket_family, socket_type, protocol);

  if (tmp_sock >= 0 && socket_family == AF_INET && socket_type == SOCK_STREAM && protocol == IPPROTO_TCP) { // only TCPv4
    printf("TCPv4 sock %d STORE\n", tmp_sock);
    pthread_mutex_lock(&g_map_mutex);
    g_sock_map[tmp_sock] = ZERO_SOCK;
    pthread_mutex_unlock(&g_map_mutex);
  }

  return tmp_sock;
}

int close(int fd)
{
  pthread_mutex_lock(&g_map_mutex);
  std::map<int, SocketTracker_t>::iterator it = g_sock_map.find(fd);
  if (it != g_sock_map.end()) {
    printf("TCPv4 sock %d ERASE\n", fd);
    g_sock_map.erase(it);
  }
  pthread_mutex_unlock(&g_map_mutex);

  glibc_close(fd);
}
