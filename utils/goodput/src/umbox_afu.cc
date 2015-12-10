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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <linux/limits.h> // PATH_MAX

#include <pthread.h>
#include <sstream>

#include <sched.h>

#include "libcli.h"
#include "liblog.h"

#include "time_utils.h"
#include "worker.h"
#include "goodput.h"

#include "mport.h"
#include "tun_ipv4.h"
#include "umbox_afu.h"

extern "C" {
        void zero_stats(struct worker *info);
        int migrate_thread_to_cpu(struct thread_cpu *info);
        bool umd_check_cpu_allocation(struct worker *info);
        bool TakeLock(struct worker* info, const char* module, int instance);
        uint32_t crc32(uint32_t crc, const void *buf, size_t size);
};

extern bool umd_dma_goodput_tun_ep_has_peer(struct worker* info, const uint16_t destid);

#define PAGE_4K    4096

void umd_afu_watch_add_rx_fd(struct worker* info, const int fd)
{
	assert(info);
	assert(fd >= 0);
	assert(info->umd_epollfd >= 0);
	{{
	  struct epoll_event event;
	  event.data.fd = fd;
	  event.events = EPOLLIN;
	  if(epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, fd, &event) < 0) assert(0);
	}}

	info->umd_mbox_rx_fd = fd;
}

extern "C"
void umd_afu_watch_demo(struct worker* info)
{
	assert(info);

	info->owner_func = umd_afu_watch_demo;

	info->umd_set_rx_fd = umd_afu_watch_add_rx_fd;

	{{
	  RioMport* mport = new RioMport(info->mp_num, info->mp_h);
	  info->my_destid = mport->getDeviceId();
	  delete mport;
	}}

	const int max_tag = info->umd_chan;

	struct epoll_event* events = NULL;

	std::map<int, int> socket_tag_list; // socket->tag
	std::map<int, std::vector<int>> tag_socket_list; // tag->sockets (connected)

	int* listen_list = (int*)calloc(max_tag, sizeof(int));
	if (listen_list == NULL) goto exit;
	for (int i = 0; i < max_tag; i++) listen_list[i] = -1;

	info->umd_epollfd = epoll_create1 (0);
        if (info->umd_epollfd < 0) goto exit;

	// Set up all listening sockets
	for (int i = 0; i < max_tag; i++) {
		int s = -1;
		socklen_t len = -1;
		struct sockaddr_un sa; memset(&sa, 0 , sizeof(sa));

		char sock_name[PATH_MAX + 1] = {0};
		snprintf(sock_name, PATH_MAX, "%s%d", AFU_PATH, i);
		unlink(sock_name);

		if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) < 0) {
		    CRIT("socket error: %s", strerror(errno));
		    goto exit;
		}

		// Create the address of the server
		sa.sun_family = AF_UNIX;
		strncpy (sa.sun_path, sock_name, sizeof(sa.sun_path)-1);
		len = sizeof(sa.sun_family) + strlen(sa.sun_path);

		// Bind the socket to the address.
		if (bind (s, (struct sockaddr*) &sa, len) < 0) {
		    CRIT("bind to %s error: %s", sock_name, strerror(errno));
		    goto exit;
		}

		// Listen for connections
		if (listen (s, SOMAXCONN) < 0) {
		    CRIT("listen on %s error: %s", sock_name, strerror(errno));
		    goto exit;
		}

		// Huraah! We have a listening socket
		listen_list[i] = s;
		socket_tag_list[s] = i;

		{{
		  struct epoll_event event;
		  event.data.fd = s;
		  event.events = EPOLLIN;
		  if(epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, s, &event) < 0) goto exit;
		}}
	}

	if ((events = (struct epoll_event*)calloc(MAX_EPOLL_EVENTS, sizeof(struct epoll_event))) == NULL) goto exit;

	while (! info->stop_req) {
		uint8_t buf[PAGE_4K];

		const int epoll_cnt = epoll_wait (info->umd_epollfd, events, MAX_EPOLL_EVENTS, -1);
		
		for (int epi = 0; epi < epoll_cnt; epi++) {
			const int fd = events[epi].data.fd;

			// XXX When info->umd_mbox_rx_fd is closed by MboxWatch we should end up here
			if ((events[epi].events & EPOLLERR) || (events[epi].events & EPOLLHUP) || (!(events[epi].events & EPOLLIN))) {
				CRIT("\n\tepoll error for fd=%d: %s. Closing it!\n", fd, strerror(errno));

				// Cleanup fd from various maps
				socket_tag_list.erase(fd);

				for (int k = 0; k < tag_socket_list.size(); k++) { // We must hutnt this fd among all tags :(
					std::vector<int>& tag_vec = tag_socket_list[k];

					// Clunky way to erase element from vector
					std::vector<int>::iterator itv = tag_vec.begin();
					for (; itv != tag_vec.end() && *itv != fd; itv++) {;}
					if (itv != tag_vec.end()) tag_vec.erase(itv);
				}

				{{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
				  struct epoll_event event;
				  event.data.fd = fd;
				  event.events = EPOLLIN;
				  epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, fd, &event);
				}}

				close(fd);
				continue;
			}

			// MBOX RX has data for us
			if (fd == info->umd_mbox_rx_fd) {
				const int nread = cread(fd, buf, PAGE_4K);
				if (nread <= 0) continue; // XXX what happens if info->umd_mbox_rx_fd is closed and marked -1??
				DMA_MBOX_L2_t* pL2 = (DMA_MBOX_L2_t*)buf;
				DMA_MBOX_L3_t* pL3 = (DMA_MBOX_L3_t*)(buf + sizeof(DMA_MBOX_L2_t));
				assert(nread == ntohs(pL2->len));

				const uint16_t tag = ntohs(pL3->tag);
			
				std::map<int, std::vector<int>>::iterator itl = tag_socket_list.find(tag); 
				if (itl == tag_socket_list.end()) {
					INFO("\n\tHave no connected AF_UNIX sockets for tag=%u\n", tag);
					// XXX Signal back to RIO situation?
					continue;
				}

				pL3->destid = pL2->destid_src;

				std::vector<int> bad_vec;
				std::vector<int>& tag_vec = itl->second;
				for (int i = 0; i < tag_vec.size(); i++) {
					const int conn_fd = tag_vec[i];
					const int nwrite = write(conn_fd, buf + sizeof(DMA_MBOX_L2_t), nread - sizeof(DMA_MBOX_L2_t));
					if (nwrite < 0) { // Dud socket!!
						INFO("\n\tFailed to write to connected fd=%d with tag=%d: %s. Closing it!\n", conn_fd, tag, strerror(errno));
						bad_vec.push_back(conn_fd);
						continue;
					}
				}

				// Nuke dud sockets. ¡Hasta la vista! said Arnold
				for (int j = 0; j < bad_vec.size(); j++) {
					const int badfd = tag_vec[bad_vec[j]];
					socket_tag_list.erase(badfd);

					{{ // Clunky way to erase element from vector
					  std::vector<int>::iterator itv = tag_vec.begin();
					  for (; itv != tag_vec.end() && *itv != badfd; itv++) {;}
					  if (itv != tag_vec.end()) tag_vec.erase(itv);
					}}

					{{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
					  struct epoll_event event;
					  event.data.fd = badfd;
					  event.events = EPOLLIN;
					  epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, badfd, &event);
					}}

					close(badfd);
				}

				continue;
			}

			std::map<int, int>::iterator itm = socket_tag_list.find(fd);
			if (itm == socket_tag_list.end()) {
				CRIT("\n\tBUG: Cannot find fd=%d in socket_tag_list. Bailing out!\n", fd);
				goto exit;
			}

			const int tag = itm->second;
			assert(tag >= 0);
			assert(tag < max_tag);

			// A listening socket
			if (fd == listen_list[tag]) {
				struct sockaddr_un connect_sa; memset(&connect_sa, 0 , sizeof(connect_sa));
				socklen_t len = sizeof(connect_sa);
				const int connect_fd = accept(fd, (struct sockaddr*)&connect_sa, &len);
				if (connect_fd < 0) {
					ERR("\n\taccept error on fd=%d tag=%d: %s", fd, tag, strerror(errno));
					continue;
				}

				socket_tag_list[connect_fd] = tag;
				tag_socket_list[tag].push_back(connect_fd);

				{{
				  struct epoll_event event;
				  event.data.fd = connect_fd;
				  event.events = EPOLLIN;
				  if(epoll_ctl (info->umd_epollfd, EPOLL_CTL_ADD, connect_fd, &event) < 0) goto exit;
				}}
				
				DBG("\n\tNew connection on fd=%d tag=%d\n", fd, tag);
				continue;
			}

			// A connected AF_UNIX socket
			const int nread = recv(fd, buf, PAGE_4K, 0);
			if (nread == 0) { // Socket closed?
				continue;
			}

			if (nread < 0) {
				ERR("\n\trecv error on connected fd=%d tag=%d: %s. Closing it!", fd, tag, strerror(errno));

                                // Cleanup fd from various maps
                                socket_tag_list.erase(fd);

                                for (int k = 0; k < tag_socket_list.size(); k++) { // We must hutnt this fd among all tags :(
                                        std::vector<int>& tag_vec = tag_socket_list[k];

                                        // Clunky way to erase element from vector
                                        std::vector<int>::iterator itv = tag_vec.begin();
                                        for (; itv != tag_vec.end() && *itv != fd; itv++) {;}
                                        if (itv != tag_vec.end()) tag_vec.erase(itv);
                                }

                                {{ // A close removes tun_fd from poll set but NOT if it was dupe(2)'ed
                                  struct epoll_event event;
                                  event.data.fd = fd;
                                  event.events = EPOLLIN;
                                  epoll_ctl(info->umd_epollfd, EPOLL_CTL_DEL, fd, &event);
                                }}

                                close(fd);

				continue;
			}

			if (info->umd_mbox_tx_fd < 0) {
				ERR("\n\tMboxWatch thread not ready tag=%d\n", tag);
				// XXX Signal error? ENOTREADY
				continue;
			}
			DMA_MBOX_L3_t* pL3 = (DMA_MBOX_L3_t*)buf;
			const uint16_t destid = ntohs(pL3->destid);
			const uint16_t tagL3  = ntohs(pL3->tag);

			if (tagL3 != tag) {
				INFO("\n\tTag mismatch between socket.tag=%u and L3.tag=%u destid %u fd=%d. Ignoring\n", tag, tagL3, destid, fd);
				continue;
			}
			if (! umd_dma_goodput_tun_ep_has_peer(info, destid)) { // No point spamming MBOX with invalid destid
				INFO("\n\tNo peer for destid %u exists fd=%d tag=%d\n", destid, fd, tagL3);
				// XXX Signal error? ENONET
				continue;
			}

			DMA_MBOX_L2_t L2hdr; memset(&L2hdr, 0, sizeof(L2hdr));

			L2hdr.destid_src = htons(info->my_destid);
			L2hdr.destid_dst = htons(destid);
			L2hdr.len        = htons(sizeof(DMA_MBOX_L2_t)) + nread;

			struct iovec iov[2]; memset(&iov, 0, sizeof(iov));

			iov[0].iov_base = &L2hdr;
			iov[0].iov_len  = sizeof(L2hdr);

			iov[1].iov_base = buf;
			iov[1].iov_len  = nread;

			if (writev(info->umd_mbox_tx_fd, (struct iovec*)&iov, 2) < 0) {
				ERR("\n\twritev failed tag=%d [tx_fd=%d]: %s\n", tag, info->umd_mbox_tx_fd, strerror(errno));
				// XXX Cannot talk to MboxWatch thread. Signal error? ENOLINK
				continue;
			}
		}
	}

exit:
	// Close listening sockets
	if (listen_list != NULL) {
		for (int i = 0; i < max_tag; i++) {
			close(listen_list[i]);

			char sock_name[PATH_MAX + 1] = {0};
			snprintf(sock_name, PATH_MAX, "%s%d", AFU_PATH, i);
			unlink(sock_name);
		}
		free(listen_list);
	}

	// Close connected sockets
	for (std::map<int, int>::iterator itm = socket_tag_list.begin(); itm != socket_tag_list.end(); itm++) {
		if (itm->second == -1) continue; // not a connected socket
		close(itm->first);
	}

	if (events != NULL) free(events);

	close(info->umd_epollfd); info->umd_epollfd = -1;
}
