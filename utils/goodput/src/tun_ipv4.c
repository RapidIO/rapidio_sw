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

// Note: Some of thic code comes from Stevens UNP and Linux Kernel tun.c documentiation

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>

#include "string_util.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            needs to reserve enough space in *dev.                      *
 **************************************************************************/
int tun_alloc(char* dev, int flags)
{

  struct ifreq ifr;
  int fd, err;

  if( (fd = open("/dev/net/tun", O_RDWR | O_CLOEXEC)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    SAFE_STRNCPY(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    CRIT("\n\tioctl(TUNSETIFF): %s\n", strerror(errno));
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);

  return fd;
}

/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread(int fd, uint8_t* buf, int n)
{
  int nread;

  if((nread=read(fd, buf, n))<0){
    CRIT("\n\tReading data: %s\n", strerror(errno));
    abort();
  }
  return nread;
}

/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
int cwrite(int fd, uint8_t* buf, int n)
{
  int nwrite;

  if((nwrite=write(fd, buf, n))<0){
    CRIT("\n\tWriting data (n=%d): %s\n", n, strerror(errno));
    abort();
  }
  return nwrite;
}

/**************************************************************************
 * read_n: ensures we read exactly n bytes, and puts those into "buf".    *
 *         (unless EOF, of course)                                        *
 **************************************************************************/
int read_n(int fd, uint8_t* buf, int n)
{
  int nread, left = n;

  while(left > 0) {
    if ((nread = cread(fd, buf, left))==0){
      return 0 ;
    }else {
      left -= nread;
      buf += nread;
    }
  }
  return n;
}

static inline unsigned short in_cksum(unsigned short *addr, int len)
{
  register int sum = 0;
  u_short answer = 0;
  register u_short *w = addr;
  register int nleft = len;
  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }
  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(u_char *) (&answer) = *(u_char *) w;
    sum += answer;
  }
  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
  sum += (sum >> 16);             /* add carry */
  answer = ~sum;              /* truncate to 16 bits */
  return answer;
}

static inline int min(int a, int b) { return a < b? a: b; }

int icmp_host_unreachable(uint8_t* l3_in, const int l3_in_size, uint8_t* l3_out, int& l3_out_size)
{
  if(l3_in == NULL || l3_in_size < 20) return 0;
  if(l3_out == NULL || l3_out_size < 64) return 0;

  memcpy(l3_out, l3_in, sizeof(struct iphdr)); // IPv4 header

  const int MAX_COPY = min(l3_in_size, 64); // RFC requires only 8 bytes of original frame

  struct iphdr* ip = (struct iphdr*)l3_out;
  ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr) + MAX_COPY);
  uint32_t tmp = ip->daddr;
  ip->daddr = ip->saddr;
  ip->saddr = tmp;
  ip->protocol = 1; // ICMP
  ip->check = 0;
  ip->check = in_cksum((unsigned short *)ip, sizeof(struct iphdr));

  uint8_t* p = l3_out + sizeof(struct iphdr) + sizeof(struct icmphdr);
  memcpy(p, l3_in, MAX_COPY);

  struct icmphdr* icmp = (struct icmphdr*)(l3_out + sizeof(struct iphdr));
  memset(icmp, 0, sizeof(struct icmphdr));
  icmp->type = 0x3; // Type: Destination unreachable
  icmp->code = 0x1; // Code: Host unreachable
  icmp->checksum = 0;
  icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr) + MAX_COPY);

  l3_out_size = sizeof(struct iphdr) + sizeof(struct icmphdr) + MAX_COPY; // HAAACK adjust length in ip header!!

  return 1;
}

bool send_icmp_host_unreachable(const int tun_fd, uint8_t* l3_in, const int l3_in_size)
{
        if(tun_fd < 0) return false;
        if(l3_in == NULL || l3_in_size < 20) return false;

        const int BUFSIZE = 8192;
        uint8_t buffer_unreach[BUFSIZE] = {0};

        int out_size = BUFSIZE;

        if (! icmp_host_unreachable(l3_in, l3_in_size, buffer_unreach, out_size)) return false;

        cwrite(tun_fd, buffer_unreach, out_size);
        return true;
}

#ifdef __cplusplus
};
#endif
