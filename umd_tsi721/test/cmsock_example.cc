#include <unistd.h>
#include <signal.h>

#include "mportcmsock.h"

#define CM_PORT    666

#define abs(x) ((x>=0)? (x): (-x))

volatile int stop_req = 0;

void usage(const char* name)
{
  fprintf(stderr, "usage: %s: <-c destid> or <-s>\n", name);
  exit(0);
}

static void sig_term(int signo) { stop_req = 1; }

const char bufA[256] = {'A'};
const char bufB[256] = {'B'};

int main(int argc, char* argv[])
{
  bool ret = 0;
  int server = -1;
  uint16_t destid = 0xFFFF;
  int mportid = 0;
  int rc = 0;

  int c = 0;
  while ((c = getopt (argc, argv, "hsc:")) != -1) {
    switch(c) {
      case 'h': usage(argv[0]); break;
      case 's': server = 1; break;
      case 'c': server = 0; destid = atoi(optarg); break;
      default: exit(69); break;
    }
  }

  if (server == -1) usage(argv[0]);

  signal(SIGINT, sig_term);
  signal(SIGTERM, sig_term);

  MportCMSocket* cms = new MportCMSocket(mportid, 0);

// SERVER
  if (server == 1) {
    rc = cms->bind(CM_PORT);
    if (rc) { fprintf(stderr, "bind error %d: %s\n", rc, strerror(abs(rc))); ret = 1; goto exit; }
    rc = cms->listen();
    if (rc) { fprintf(stderr, "listen error %d: %s\n", rc, strerror(abs(rc))); ret = 2; goto exit; }

    for(; !stop_req;) {
      MportCMSocket* cli_cms = NULL;
      rc = cms->accept(cli_cms, 60*1000);
      if (rc == ETIME) continue;
      if (rc == EINTR) break;

      if (rc) { fprintf(stderr, "accept error %d: %s\n", rc, strerror(abs(rc))); ret = 3; goto exit; }

      uint8_t buf[257] = {0};
      rc = cli_cms->read(buf, 256, 1000);
      if (rc) { fprintf(stderr, "read error %d: %s\n", rc, strerror(abs(rc))); goto next; }
      if (!memcmp(buf, bufA, 256)) {
        rc = cli_cms->write(bufB, 256);
        if (rc) { fprintf(stderr, "write error %d: %s\n", rc, strerror(abs(rc))); goto next; }
      } else fprintf(stderr, "Invalid data read from client!\n");
 next:
      delete cli_cms;
    }

    goto exit;
  }

// CLIENT
  rc = cms->connect(destid, 0, CM_PORT);
  if (rc) { fprintf(stderr, "connect error %d: %s\n", rc, strerror(abs(rc))); ret = 1; goto exit; }
  {{
    char buf[256] = {0};
    rc = cms->write(bufA, 256);
    if (rc) { fprintf(stderr, "write error %d: %s\n", rc, strerror(abs(rc))); ret = 2; goto exit; }
    rc = cms->read(buf, 256, 1000);
    if (rc) { fprintf(stderr, "read error %d: %s\n", rc, strerror(abs(rc))); ret = 3; goto exit; }
    if (memcmp(buf, bufB, 256)) fprintf(stderr, "Invalid data read from server!\n");
  }}

exit:
  delete cms;
  return ret;
}
