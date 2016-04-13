#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>

#include <sstream>

#include "worker.h"
#include "udma_tun.h"

static int setupListeningSocket(const int port)
{
  int listen_fd = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listen_fd == -1) {
    CRIT("\n\tsocket error: %s\n", strerror(errno));
    return -1;
  }

  int optval = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

  struct sockaddr_in serv_addr; memset(&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port        = htons(port);
   
  if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    CRIT("\n\tbind() error: %s\n", strerror(errno));
    return -1;
  }

  if (listen (listen_fd, SOMAXCONN) != 0) {
    CRIT("\n\tlisten() error: %s\n", strerror(errno));
    return -1;
  }

  return listen_fd;
}

static void respond(int fd, const char* rsp, const int rsp_size)
{
  assert(fd >= 0);
  assert(rsp);

  char mesg[4097] = {0};

  int rcvd = recv(fd, mesg, sizeof(mesg)-1 , 0);

  if (rcvd < 0) {		// receive error
    fprintf(stderr, "\n\trecv() error: %d\n", strerror(errno));
    return;
  }
  if (rcvd == 0) {		// receive socket closed
    fprintf(stderr, "\n\tClient disconnected upexpectedly.\n");
    return;
  }

  static const char HDR[] = "HTTP/1.0 200 OK\n" \
                            "Cache-Control: post-check=0, pre-check=0, no-store, no-cache, must-revalidate\n" \
                            "Expires: -1\n" \
                            "Content-type: text/plain\n" \
                            "\n";

  struct iovec iov[2]; memset(&iov, 0, sizeof(iov));
  iov[0].iov_base = (void*)HDR;
  iov[0].iov_len  = sizeof(HDR);
  iov[1].iov_base = (void*)rsp;
  iov[1].iov_len  = rsp_size;

  writev(fd, (struct iovec*)&iov, 2);
}

extern void UMD_DD_SS(struct worker* info, std::stringstream& out);

extern "C"
void UMD_DD_WWW(struct worker* info, struct worker* udmatun_info, const int PORT)
{
  assert(info);
  assert(udmatun_info);

  const int quit_fd = udmatun_info->umd_sockp_quit[1];
  assert(quit_fd >= 0);

  int listen_fd = setupListeningSocket(PORT);
  if (listen_fd < 0) return;

  INFO("\n\tHTTP Server started on port %d\n", PORT);

  // ACCEPT connections
  while (! info->stop_req) {
    fd_set rd_set;

    FD_ZERO(&rd_set);
    FD_SET(listen_fd, &rd_set); FD_SET(quit_fd, &rd_set);

    const int maxfd = listen_fd > quit_fd? listen_fd: quit_fd;

    // We piggyback on Main Battle Tank quit file descriptor.

    struct timeval to = { 0, 100 * 1000 }; // wake up every 100 ms
    const int ret = select(maxfd + 1, &rd_set, NULL, NULL, &to);

    if (ret == 0) continue; // timeout, check stop_req

    if (ret < 0 && errno == EINTR) continue;
    if (ret < 0) { CRIT("\n\tselect(): %s\n", strerror(errno)); break; }

    if(FD_ISSET(quit_fd, &rd_set)) {
      INFO("\n\tHTTP Server exiting on quit_fd.\n");
      break;
    }

    if(! FD_ISSET(listen_fd, &rd_set)) continue; // XXX really?

    if (info->stop_req) break;

    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof (clientaddr);
    const int client_fd = accept(listen_fd, (struct sockaddr *)&clientaddr, &addrlen);
    if (client_fd < 0) {
      CRIT("\n\taccept() error: %s", strerror(errno));
      break;
    }

    if (info->stop_req) break;

    std::stringstream ss;
    display_gen_status_ss(ss);
    display_ibwin_status_ss(ss);
    ss << "\n";
    UMD_DD_SS(udmatun_info, ss);

    if (info->stop_req) break;

    char* rsp = strdup(ss.str().c_str());
    const int rsp_size = strlen(rsp);

    if (fork() == 0) { // Child
      close(listen_fd);
      respond(client_fd, rsp, rsp_size);
      exit(0);
    }

    free(rsp);
    close(client_fd);
  }

  close(listen_fd);

  INFO("\n\tHTTP Server ended.\n");
}
