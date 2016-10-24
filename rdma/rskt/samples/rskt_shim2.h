#ifndef __RSKT_SHIM2_H__
#define __RSKT_SHIM2_H__

#include <stdint.h>

extern "C" {

void shim_rskt_init();

uint64_t shim_rskt_socket();

int shim_rskt_connect(uint64_t sock, uint16_t destid, uint16_t port);

int shim_rskt_listen(uint64_t sock, int max_backlog = 50);
int shim_rskt_bind(uint64_t listen_sock, const uint16_t destid /*=0*/, const uint16_t port);
int shim_rskt_accept(uint64_t listen_sock, uint64_t *accept_socket, uint16_t* remote_destid, uint16_t* remote_port);

int shim_rskt_close(uint64_t sock);

int shim_rskt_read(uint64_t sock, void* data, const int data_len);
int shim_rskt_get_avail_bytes(uint64_t sock);

int shim_rskt_write(uint64_t sock, void* data, const int data_len);

};
#endif // __RSKT_SHIM2_H__
