#ifndef __RSKT_SHIM2_H__
#define __RSKT_SHIM2_H__

#include <stdint.h>

extern "C" {

void shim_rskt_init();

uint16_t shim_rskt_get_my_destid();

void* shim_rskt_socket();

int shim_rskt_connect(void* sock, uint16_t destid, uint16_t port);

int shim_rskt_listen(void* sock, int max_backlog = 50);
int shim_rskt_bind(void* listen_sock, const uint16_t destid /*=0*/, const uint16_t port);
int shim_rskt_accept(void* listen_sock, void* accept_socket, uint16_t* remote_destid, uint16_t* remote_port);

int shim_rskt_close(void* sock);

int shim_rskt_read(void* sock, void* data, const int data_len);
int shim_rskt_get_avail_bytes(void* sock);

int shim_rskt_write(void* sock, void* data, const int data_len);

};
#endif // __RSKT_SHIM2_H__
