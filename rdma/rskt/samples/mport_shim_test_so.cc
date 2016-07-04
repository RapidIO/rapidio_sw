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
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <stdexcept>

static inline void errx(const char* msg)
{ if (msg) throw std::runtime_error(msg); _exit(42); }

#define UNDERSCORE

#define MPORT_DECLARE(name, ret, args) static ret (*MPORT_##name) args

#define MPORT_DLSYMCAST(name, ret, args)  do { \
        if ((MPORT_##name = (ret (*) args) dlsym(dh, UNDERSCORE #name)) == NULL)                \
                errx("RSKT Shim: Failed to get " #name "() address");   \
} while(0);

MPORT_DECLARE(mport_my_destid, uint16_t, (void));

void rskt_shim_main() __attribute__ ((constructor));

static inline uint16_t mport_my_destid()
{
  void* dh = NULL;
  uint16_t did = 0xFFFF;

  const char* mport_shim_path = "./mport_shim.so";
  if ((dh = dlopen(mport_shim_path, RTLD_LAZY)) == NULL)
        errx("RSKT Shim: Failed to open mport_shim");

  MPORT_DLSYMCAST(mport_my_destid, uint16_t, (void));
  did = MPORT_mport_my_destid();

  dlclose(dh);
  return did;
}

int main()
{
  printf("my destid %u\n", mport_my_destid());
  return 0;
}

