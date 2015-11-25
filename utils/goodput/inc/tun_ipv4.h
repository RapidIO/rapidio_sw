#ifndef __TUN_IPV4_H__
#define __TUN_IPV4_H__

#ifdef __cplusplus
extern "C" {
#endif

int tun_alloc(char* dev, int flags);

int cread(int fd, uint8_t* buf, int n);
int cwrite(int fd, uint8_t* buf, int n);
int read_n(int fd, uint8_t* buf, int n);
int icmp_host_unreachable(uint8_t* l3_in, const int l3_in_size, uint8_t* l3_out, int& l3_out_size);

#ifdef __cplusplus
};
#endif

#endif // __TUN_IPV4_H__
