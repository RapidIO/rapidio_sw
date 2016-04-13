#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<arpa/inet.h>
//#include<netdb.h>
#include<fcntl.h>

#include <sstream>

#include "worker.h"

//start server
static int  startServer (const int port)
{
  int listenfd = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listenfd == -1) {
    CRIT("\n\tsocket error: %s\n", strerror(errno));
    return -1;
  }

  struct sockaddr_in serv_addr; memset(&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port        = htons(port);
   
  if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    CRIT("\n\tbind() error: %s\n", strerror(errno));
    return -1;
  }

  if (listen (listenfd, SOMAXCONN) != 0) {
    CRIT("\n\tlisten() error: %s\n", strerror(errno));
    return -1;
  }

  return listenfd;
}

static void respond(int fd, std::stringstream& resp)
{
  char mesg[4097] = {0};

  int rcvd = recv(fd, mesg, sizeof(mesg)-1 , 0);

  if (rcvd < 0) {		// receive error
    INFO("\n\trecv() error: %d\n", strerror(errno));
    return;
  }

  if (rcvd == 0) {		// receive socket closed
    INFO("\n\tClient disconnected upexpectedly.\n");
    return;
  }

  static const char HDR[] = "HTTP/1.0 200 OK\n" \
                            "Cache-Control: post-check=0, pre-check=0, no-store, no-cache, must-revalidate\n" \
                            "Expires: -1\n" \
                            "Content-type: text/plain\n" \
                            "\n";

  send(fd, HDR, sizeof(HDR), 0);

  const char* rsp = resp.str().c_str();
  send(fd, rsp, strlen(rsp), 0);

  close(fd);
}

extern void UMD_DD_SS(struct worker* info, std::stringstream& out);

extern "C"
void UMD_DD_WWW(struct worker* info)
{
  const int PORT = 8080;

  INFO("\n\tHTTP Server started on port %d\n", PORT);

  int listenfd = startServer (PORT);

  // ACCEPT connections
  while (! info->stop_req) {
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof (clientaddr);
    const int client = accept (listenfd, (struct sockaddr *)&clientaddr, &addrlen);
    if (client < 0) {
      CRIT("\n\taccept() error: %s", strerror(errno));
      break;
    }

    std::stringstream ss;
    UMD_DD_SS(info, ss);

    if (fork() == 0) { respond (client, ss); exit (0); }

    close(client);
  }

  close(listenfd);
}
