#include <unistd.h>
#include <signal.h>

#include <string>

#include "mportcmsock.h"

#define DATA_SZ	   16
#define CM_PORT    666

#define abs(x) ((x>=0)? (x): (-x))

volatile int stop_req = 0;

void usage(const char* name)
{
  fprintf(stderr, "usage: %s: <-c destid> or <-s>\n", name);
  exit(0);
}

static void sig_term(int signo) { stop_req = 1; }

const char bufA[DATA_SZ] = {'A'};
const char bufB[DATA_SZ] = {'B'};

inline std::string hexdump(uint8_t* data, const int len)
{
  std::string out;
  for (int i = 0; i < len; i++) {
    char tmp[9] = {0};
    snprintf(tmp, 8, "%02x", data[i]);
    out.append(tmp).append(" ");
  }
  return out;
}

inline std::string hexdump(const char* data, const int len) { return hexdump((uint8_t*)data, len); }

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

      uint8_t buf[DATA_SZ+1] = {0};
      rc = cli_cms->read(buf, DATA_SZ, 1000);
      if (rc) { fprintf(stderr, "read error %d: %s got [%s]\n", rc, strerror(abs(rc)), hexdump(buf, DATA_SZ).c_str()); goto next; }
      if (!memcmp(buf, bufA, DATA_SZ)) {
        rc = cli_cms->write(bufB, DATA_SZ);
        if (rc) { fprintf(stderr, "write error %d: %s\n", rc, strerror(abs(rc))); goto next; }
      } else fprintf(stderr, "Invalid data read from client! got=[%s]\n", hexdump(buf, DATA_SZ).c_str());
 next:
      delete cli_cms;
    }

    goto exit;
  }

// CLIENT
  rc = cms->connect(destid, 0, CM_PORT);
  if (rc) { fprintf(stderr, "connect error %d: %s\n", rc, strerror(abs(rc))); ret = 1; goto exit; }
  {{
    char buf[DATA_SZ] = {0};
    rc = cms->write(bufA, DATA_SZ);
    if (rc) { fprintf(stderr, "write error %d: %s\n", rc, strerror(abs(rc))); ret = 2; goto exit; }
    rc = cms->read(buf, DATA_SZ, 1000);
    if (rc) { fprintf(stderr, "read error %d: %s got [%s]\n", rc, strerror(abs(rc)), hexdump(buf, DATA_SZ).c_str()); ret = 3; goto exit; }
    if (memcmp(buf, bufB, DATA_SZ)) fprintf(stderr, "Invalid data read from server!\n");
  }}

exit:
  delete cms;
  return ret;
}
