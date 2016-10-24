/* Test code for librskt */
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <string.h>
#include <netinet/in.h>
#include "memops.h"

#include "librskt_mock.c"
#include "librskt.c"

#ifdef __cplusplus
extern "C" {
#endif

#include "liblog.h"
#include "librskt_buff.h"
#include "librskt_utils.h"
#include "librskt_socket.h"
#include "librskt_list.h"
#include "librskt_info.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "libcli.h"
#include "librskt.h"

int test_librskt_init(void)
{
	struct librskt_app_to_rsktd_msg *app2d;
	struct librskt_rsktd_to_app_msg *d2app;

	app2d = (struct librskt_app_to_rsktd_msg *)
			calloc(1, sizeof(struct librskt_app_to_rsktd_msg));
	d2app = (struct librskt_rsktd_to_app_msg *)
			calloc(1, sizeof(struct librskt_rsktd_to_app_msg));

	// Success 
	test_fail = 0;
	lib.portno = 0;
	lib.mpnum = -1;
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = 0;
	alloc_app2d_rc = app2d;
	alloc_d2app_rc = d2app;
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.hello.ct = htonl(0x1234);
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.hello.use_mport = htonl(1);
	librskt_dmsg_req_resp_rc = 0;
	free_app2d_rc = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_rc = 0;
	free_d2app_ptr_idx = 0;
	riomp_mgmt_mport_create_handle_rc = 0;

	if (librskt_init(3333, 3)) {
		goto fail;
	};

	if (lib.portno != 3333) {
		goto fail;
	};
	if (lib.mpnum != 3) {
		goto fail;
	};
	if (lib.init_ok != 3333) {
		goto fail;
	};
	if (lib.ct != 0x1234) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	// Fail on socket alloc
	lib.init_ok = -1;

	socket_rc = -1;
	socket_errno = ENOMEM;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};

	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on socket connect
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = -1;
	connect_errno = EBADFD;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};

	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on librskt_init_threads_rc
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = -1;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};

	if (lib.portno != 3333) {
		goto fail;
	};
	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on alloc_app2d_rc
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = 0;
	alloc_app2d_rc = NULL;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};
	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on alloc_d2app2d
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = 0;
	alloc_app2d_rc = app2d;
	alloc_app2d_rc = NULL;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};
	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on librskt_dmsg_req_resp`
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = 0;
	alloc_app2d_rc = app2d;
	alloc_d2app_rc = d2app;
	librskt_dmsg_req_resp_errno = EINVAL;
	librskt_dmsg_req_resp_rc = -1;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};
	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on free_d2app;
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = 0;
	alloc_app2d_rc = app2d;
	alloc_d2app_rc = d2app;
	librskt_dmsg_req_resp_errno = EINVAL;
	librskt_dmsg_req_resp_rc = 0;
	d2app->a_rsp.msg.hello.ct = htonl(0x1234);
	free_d2app_errno = EPERM;
	free_d2app_rc = -1;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};
	if (lib.init_ok != 0) {
		goto fail;
	};

	// Fail on riomp_mgmt_mport_create_handle`
	lib.init_ok = -1;

	socket_rc = 0;
	connect_rc = 0;
	librskt_init_threads_rc = 0;
	alloc_app2d_rc = app2d;
	alloc_d2app_rc = d2app;
	librskt_dmsg_req_resp_errno = EINVAL;
	librskt_dmsg_req_resp_rc = 0;
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.hello.ct = htonl(0x1234);
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.hello.use_mport = htonl(1);
	free_d2app_rc = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	riomp_mgmt_mport_create_handle_errno = ENOSYS;
	riomp_mgmt_mport_create_handle_rc = -1;

	if (!librskt_init(3333, 3)) {
		goto fail;
	};
	if (lib.init_ok != 0) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_librskt_finish(void)
{
	close_rc = 0;
	close_errno = 0;
	lib.fd = 3;
	librskt_finish();

	if (lib.fd) {
		goto fail;
	};

	lib.fd = 3;
	close_rc = -1;
	close_errno = EINTR;
	librskt_finish();

	if (lib.fd) {
		goto fail;
	};

	lib.fd = 3;
	close_rc = -1;
	close_errno = EBADF;
	librskt_finish();

	if (lib.fd) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_create_socket(void)
{
	rskt_h skt;

	lib.init_ok = lib.portno = 0;
	skt = rskt_create_socket();
	if (LIBRSKT_H_INVALID != skt) {
		goto fail;
	};

	lib.init_ok = lib.portno = 3333;
	lib.mpnum = 3;

	rsktl_get_socket_rc = LIBRSKT_H_INVALID;
	rsktl_get_socket_errno = EBADFD;
	skt = rskt_create_socket();
	if (LIBRSKT_H_INVALID != skt) {
		goto fail;
	};
	
	rsktl_get_socket_rc = 0x990011;
	rsktl_get_socket_errno = 0;
	skt = rskt_create_socket();
	if (0x990011 != skt) {
		goto fail;
	};
	
	return 0;
fail:
	return 1;
};

int test_rskt_destroy_socket(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	struct rskt_socket_t sock;
	struct rc_entry rskt_close_locked_rc = {0, 0, 0, 0};

	lib.init_ok = lib.portno = 0;
	lib.init_ok = lib.portno = 3333;
	lib.mpnum = 3;
	lib.ct = 0x1234;
	lib.use_mport = 1;
	if (rsktl_init(&lib.skts)) {
		goto fail;
	};

	rskt_destroy_socket(NULL);

	rskt_destroy_socket(&skt);

	rsktl_sock_ptr_rc = NULL;
	skt = 0x889911;
	
	rskt_destroy_socket(&skt);
	if (0x889911 != skt) {
		goto fail;
	};

	rsktl_sock_ptr_rc = &sock;
	skt = 0x887711;
	rskt_close_locked_rcs = &rskt_close_locked_rc;
	rskt_close_locked_rc_idx = 0;
	rsktl_put_socket_rc = 0;
	rsktl_put_socket_errno = 0;
	rskt_destroy_socket(&skt);
	if (LIBRSKT_H_INVALID != skt) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_bind(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	struct rskt_sockaddr sock_addr;
	struct librskt_app_to_rsktd_msg *app2d;
	struct librskt_rsktd_to_app_msg *d2app;
	struct rc_entry rsktl_atomic_set_st_pass[] = {
		{(int)rskt_reqbound, 0, (int)rskt_alloced, (int)rskt_reqbound},
		{(int)rskt_bound, 0, (int)rskt_reqbound, (int)rskt_bound}
	};
	struct rc_entry rsktl_atomic_set_st_rcs_fail1[] = {
		{(int)rskt_max_state, ENOSYS, (int)rskt_alloced, (int)rskt_reqbound},
		{(int)rskt_max_state, ENOSYS, (int)rskt_reqbound, (int)rskt_bound}
	};

	struct rc_entry rsktl_atomic_set_st_rcs_fail2[] = {
		{(int)rskt_reqbound, 0, (int)rskt_alloced, (int)rskt_reqbound},
		{(int)rskt_max_state, ENOSYS, (int)rskt_reqbound, (int)rskt_bound}
	};
	struct rskt_socket_t sock;

	test_fail = 0;

	/* Fail, lib uninit */
	lib.portno = lib.init_ok = 0;
	if (!rskt_bind(skt, NULL)) {
		goto fail;
	};
	lib.portno = lib.init_ok = 3333;

	/* Fail, rsktl_atomic_set_st first call fails */
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail1;
	rsktl_atomic_set_st_rc_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* Fail, can't get sock ptr */
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail2;
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	rsktl_sock_ptr_rc = NULL;
	rsktl_sock_ptr_errno = ENOSYS;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_sock_ptr_rc = &sock;
	rsktl_sock_ptr_errno = 0;

	/* Fail, can't allocate tx or rx buffer */
	rsktl_atomic_set_st_rc_idx = 0;
	alloc_app2d_rc = NULL;
	alloc_d2app_rc = NULL;
	alloc_app2d_errno = ENOMEM;
	alloc_d2app_errno = ENOMEM;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	app2d = (struct librskt_app_to_rsktd_msg *)
			calloc(1, sizeof(struct librskt_app_to_rsktd_msg));
	d2app = (struct librskt_rsktd_to_app_msg *)
			calloc(1, sizeof(struct librskt_rsktd_to_app_msg));

	/* Fail, can't allocate rx buffer */
	rsktl_atomic_set_st_rc_idx = 0;
	alloc_app2d_rc = app2d;
	alloc_app2d_errno = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	
	/* Fail, message exchange failure  */
	rsktl_atomic_set_st_rc_idx = 0;
	alloc_d2app_rc = d2app;
	alloc_d2app_errno = 0;
	librskt_dmsg_req_resp_rc = 1;
	librskt_dmsg_req_resp_errno = ECONNRESET;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	librskt_dmsg_req_resp_rc = 0;

	/* Fail, daemon response with error status */
	rsktl_atomic_set_st_rc_idx = 0;
	librskt_rsktd_to_app_msg_rx.a_rsp.err = 1;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	librskt_rsktd_to_app_msg_rx.a_rsp.err = 0;

	/* Fail, can't free response message */
	rsktl_atomic_set_st_rc_idx = 0;
	free_d2app_rc = 1;
	
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	free_d2app_rc = 0;
	free_d2app_errno = 0;

	/* Fail, rsktl_atomic_set_st second call fails */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* SUCCESS */
	rsktl_atomic_set_st_rc_idx = 0;
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_pass;
	sock.sa.sn = 0;
	sock.sa.ct = 0;
	lib.ct = 0x4321;
	sock_addr.sn = 0x4343;
	sock_addr.ct = 0x6676;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (rskt_bind(skt, &sock_addr)) {
		goto fail;
	};
	if (app2d->msg_type != LIBRSKTD_BIND) {
		goto fail;
	};
	if (app2d->a_rq.msg.bind.sn != htonl(sock_addr.sn)) {
		goto fail;
	};
	if (0x4343 != sock.sa.sn) {
		goto fail;
	};
	if (0x4321 != sock.sa.ct) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_listen(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	struct librskt_app_to_rsktd_msg *app2d;
	struct librskt_rsktd_to_app_msg *d2app;
	struct rc_entry rsktl_atomic_set_st_pass[] = {
		{(int)rskt_reqlisten, 0, (int)rskt_bound, (int)rskt_reqlisten},
		{(int)rskt_listening, 0, (int)rskt_reqlisten, (int)rskt_listening}
	};
	struct rc_entry rsktl_atomic_set_st_rcs_fail1[] = {
		{(int)rskt_max_state, 0, (int)rskt_bound, (int)rskt_reqlisten},
		{(int)rskt_max_state, 0, (int)rskt_reqlisten, (int)rskt_listening}
	};

	struct rc_entry rsktl_atomic_set_st_rcs_fail2[] = {
		{(int)rskt_reqlisten, 0, (int)rskt_bound, (int)rskt_reqlisten},
		{(int)rskt_max_state, 0, (int)rskt_reqlisten, (int)rskt_listening}
	};
	struct rskt_socket_t sock;

	test_fail = 0;

	/* Fail, lib uninit */
	lib.portno = lib.init_ok = 0;
	if (!rskt_listen(skt, 0)) {
		goto fail;
	};
	lib.portno = lib.init_ok = 3333;

	/* Fail, illegal backlog*/
	if (!rskt_listen(skt, 0)) {
		goto fail;
	};
	if (!rskt_listen(skt, -1)) {
		goto fail;
	};

	/* Fail, rsktl_atomic_set_st first call fails */
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail1;
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* Fail, can't get sock ptr */
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail2;
	rsktl_atomic_set_st_rc_idx = 0;
	rsktl_sock_ptr_rc = NULL;
	rsktl_sock_ptr_errno = ENOSYS;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_sock_ptr_rc = &sock;
	rsktl_sock_ptr_errno = 0;

	/* Fail, can't allocate tx or rx buffer */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	alloc_app2d_rc = NULL;
	alloc_d2app_rc = NULL;
	alloc_app2d_errno = ENOMEM;
	alloc_d2app_errno = ENOMEM;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	app2d = (struct librskt_app_to_rsktd_msg *)
			calloc(1, sizeof(struct librskt_app_to_rsktd_msg));

	/* Fail, can't allocate rx buffer */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	alloc_app2d_rc = app2d;
	alloc_app2d_errno = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	
	/* Fail, message exchange failure  */
	d2app = (struct librskt_rsktd_to_app_msg *)
			calloc(1, sizeof(struct librskt_rsktd_to_app_msg));
	alloc_d2app_rc = d2app;
	alloc_d2app_errno = 0;
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	librskt_dmsg_req_resp_rc = 1;
	librskt_dmsg_req_resp_errno = ECONNRESET;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	librskt_dmsg_req_resp_rc = 0;

	/* Fail, daemon response with error status */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	librskt_rsktd_to_app_msg_rx.a_rsp.err = 1;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	librskt_rsktd_to_app_msg_rx.a_rsp.err = 0;

	/* Fail, can't free response message */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_rc = 1;
	free_d2app_errno = ENOSYS;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	
	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	free_d2app_rc = 0;

	/* Fail, rsktl_atomic_set_st second call fails */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (!rskt_listen(skt, 50)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_pass;

	/* SUCCESS */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	sock.sa.sn = 0x4343;
	sock.sa.ct = 0x4321;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;

	if (rskt_listen(skt, 50)) {
		goto fail;
	};
	if (app2d->msg_type != LIBRSKTD_LISTEN) {
		goto fail;
	};
	if (app2d->a_rq.msg.listen.sn != htonl(0x4343)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_accept(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	rskt_h l_skt = LIBRSKT_H_INVALID;
	struct rskt_sockaddr sktaddr;
	struct rc_entry rskt_accept_msg_rc_fail0[] = {
		{1, ENOMEM, 0x1001, 0}
	};
	struct rc_entry rskt_accept_msg_rc_fail1[] = {
		{0, 0, 0x1001, 0},
		{1, ENOMEM, 0x1002, 0}
	};
	struct rc_entry rskt_accept_msg_rc_pass1[] = {
		{0, 0, 0x1001, 0},
		{0, 0, 0x1002, 0}
	};
	struct rc_entry rskt_accept_init_rc_fail0[] = {
		{1, ENOSYS, 0, 0}
	};
	struct rc_entry rskt_accept_init_rc_pass1[] = {
		{1, ENOSYS, 0, 0},
		{0, 0, 0, 0}
	};
	struct rc_entry rskt_close_locked_rc_fail0[] = {
		{1, ENOSYS, 0, 0}
	};

	if (!rskt_accept(l_skt, NULL, NULL)) {
		goto fail;
	};
	if (!rskt_accept(l_skt, &skt, NULL)) {
		goto fail;
	};
	if (!rskt_accept(l_skt, &skt, &sktaddr)) {
		goto fail;
	};

	rskt_accept_msg_rc_idx = 0;
	rskt_accept_msg_rcs = rskt_accept_msg_rc_fail0;
	l_skt = 0x1005;

	if (!rskt_accept(l_skt, &skt, &sktaddr)) {
		goto fail;
	};
	if (skt != 0x1001) {
		goto fail;
	};
		
	rskt_accept_msg_rc_idx = 0;
	rskt_accept_msg_rcs = rskt_accept_msg_rc_fail1;
	rskt_accept_init_rc_idx = 0;
	rskt_accept_init_rcs = rskt_accept_init_rc_fail0;
	rskt_close_locked_rc_idx = 0;
	rskt_close_locked_rcs = rskt_close_locked_rc_fail0;
	if (!rskt_accept(l_skt, &skt, &sktaddr)) {
		goto fail;
	};
	if (skt != 0x1002) {
		goto fail;
	};
	if ((2 != rskt_accept_msg_rc_idx) || !rskt_accept_init_rc_idx ||
		!rskt_close_locked_rc_idx) {
		goto fail;
	};

	rskt_accept_msg_rc_idx = 0;
	rskt_accept_msg_rcs = rskt_accept_msg_rc_pass1;
	rskt_accept_init_rc_idx = 0;
	rskt_accept_init_rcs = rskt_accept_init_rc_pass1;
	rskt_close_locked_rc_idx = 0;
	rskt_close_locked_rcs = rskt_close_locked_rc_fail0;
	if (rskt_accept(l_skt, &skt, &sktaddr)) {
		goto fail;
	};
	if (skt != 0x1002) {
		goto fail;
	};
	if ((2 != rskt_accept_msg_rc_idx) || (2 != rskt_accept_init_rc_idx) ||
		!rskt_close_locked_rc_idx) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_connect(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	struct rskt_sockaddr sock_addr;
	struct librskt_app_to_rsktd_msg *app2d;
	struct librskt_rsktd_to_app_msg *d2app;
	struct rc_entry rsktl_atomic_set_st_pass[] = {
		{(int)rskt_reqconnect, 0, (int)rskt_alloced, (int)rskt_reqconnect},
		{(int)rskt_connecting, 0, (int)rskt_reqconnect, (int)rskt_connecting},
		{(int)rskt_connected, 0, (int)rskt_connecting, (int)rskt_connected}
	};
	struct rc_entry rsktl_atomic_set_st_rcs_fail1[] = {
		{(int)rskt_max_state, ECONNRESET, (int)rskt_alloced, (int)rskt_reqconnect},
		{(int)rskt_max_state, 0, (int)rskt_reqconnect, (int)rskt_connecting}
	};

	struct rc_entry rsktl_atomic_set_st_rcs_fail2[] = {
		{(int)rskt_reqconnect, 0, (int)rskt_alloced, (int)rskt_reqconnect},
		{(int)rskt_max_state, EINVAL, (int)rskt_reqconnect, (int)rskt_connecting},
		{(int)rskt_max_state, 0, (int)rskt_connecting, (int)rskt_connected}
	};
	struct rc_entry rsktl_atomic_set_st_rcs_fail3[] = {
		{(int)rskt_reqconnect, 0, (int)rskt_alloced, (int)rskt_reqconnect},
		{(int)rskt_connecting, 0, (int)rskt_reqconnect, (int)rskt_connecting},
		{(int)rskt_max_state, ENOSYS, (int)rskt_connecting, (int)rskt_connected}
	};
	struct rskt_socket_t sock;
	uint8_t buffer[0x1000];

	test_fail = 0;

	/* Fail, lib uninit */
	lib.portno = lib.init_ok = 0;
	free_d2app_rc = 0;
	free_app2d_rc = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	lib.portno = lib.init_ok = 3333;

	/* Fail, Null parameter*/
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, NULL)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* Fail, rsktl_atomic_set_st first call fails */
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail1;
	rsktl_atomic_set_st_rc_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail2;

	/* Fail, can't allocate tx or rx buffer */
	rsktl_atomic_set_st_rc_idx = 0;
	alloc_app2d_rc = NULL;
	alloc_d2app_rc = NULL;
	alloc_app2d_errno = ENOMEM;
	alloc_d2app_errno = ENOMEM;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	app2d = (struct librskt_app_to_rsktd_msg *)
			calloc(1, sizeof(struct librskt_app_to_rsktd_msg));

	/* Fail, can't allocate rx buffer */
	rsktl_atomic_set_st_rc_idx = 0;
	alloc_app2d_rc = app2d;
	alloc_app2d_errno = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	
	/* Fail, message exchange failure  */
	d2app = (struct librskt_rsktd_to_app_msg *)
			calloc(1, sizeof(struct librskt_rsktd_to_app_msg));
	alloc_d2app_rc = d2app;
	alloc_d2app_errno = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail2;
	rsktl_atomic_set_st_rc_idx = 0;
	librskt_dmsg_req_resp_rc = 1;
	librskt_dmsg_req_resp_errno = ECONNRESET;
	sock_addr.sn = 0x1234;
	sock_addr.ct = 0x5555;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	librskt_dmsg_req_resp_rc = 0;
	if (LIBRSKTD_CONN != app2d->msg_type) {
		goto fail;
	};
	if (htonl(0x1234) != app2d->a_rq.msg.conn.sn) {
		goto fail;
	};
	if (htonl(0x5555) != app2d->a_rq.msg.conn.ct) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* Fail, daemon response with error status */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	librskt_rsktd_to_app_msg_rx.a_rsp.err = 1;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	librskt_rsktd_to_app_msg_rx.a_rsp.err = 0;
	if (test_fail) {
		goto fail;
	};

	/* Fail, inconsistent use_mport status */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	lib.use_mport = 4444;
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.use_addr = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	lib.use_mport = 0;
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.use_addr = 1;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	lib.use_mport = 1;
	if (test_fail) {
		goto fail;
	};

	/* Fail, rsktl_atomic_set_st second call fails */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_rcs_fail3;

	/* Fail, cannot get sock pointer */
	rsktl_atomic_set_st_rc_idx = 0;
	rsktl_sock_ptr_rc = NULL;
	rsktl_sock_ptr_errno = ENOSYS;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_sock_ptr_rc = &sock;
	rsktl_sock_ptr_errno = 0;

	/* Fail, riomp_dma_map_memory failure */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.new_ct = htonl(0x1111);
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.new_sn = htonl(0x2222);
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.rem_sn = htonl(0x3333);
	librskt_rsktd_to_app_msg_rx.a_rsp.req.msg.conn.ct = htonl(0x5555);
	PACK_PTR(0x123456789abcdef0,
		librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.r_addr_u,
		librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.r_addr_l);
	PACK_PTR(0xaaaabbbbccccdddd,
		librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.p_addr_u,
		librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.p_addr_l);
	snprintf(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.mso, MAX_MS_NAME,
		"TEST_MSO_NAME");
	snprintf(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.ms, MAX_MS_NAME,
		"TEST_LOC_MS_NAME_1");
	snprintf(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.rem_ms, MAX_MS_NAME,
		"TEST_REM_MS_NAME_1");
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.msub_sz = htonl(0x1000);
	
	riomp_dma_map_memory_rc = 1;
	riomp_dma_map_memory_errno = ENOSYS;
	riomp_dma_map_memory_vaddr = (void *)buffer;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (skt_rdma_connector != sock.connector) {
		goto fail;
	};
	if (0x1111 != sock.sa.ct) {
		goto fail;
	};
	if (0x2222 != sock.sa.sn) {
		goto fail;
	};
	if (0x3333 != sock.sai.sa.sn) {
		goto fail;
	};
	if (0x5555 != sock.sai.sa.ct) {
		goto fail;
	};
	if (0x123456789abcdef0 != sock.rio_addr) {
		goto fail;
	};
	if (0xaaaabbbbccccdddd != sock.phy_addr) {
		goto fail;
	};
	if (strncmp(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.mso,
						sock.msoh_name, MAX_MS_NAME)) {
		goto fail;
	};
	if (strlen(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.mso) != 
						strlen(sock.msoh_name)) {
		goto fail;
	};
	if (strncmp(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.ms,
						sock.msh_name, MAX_MS_NAME)) {
		goto fail;
	};
	if (strlen(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.ms) != 
						strlen(sock.msh_name)) {
		goto fail;
	};
	if (strncmp(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.rem_ms,
					sock.con_msh_name, MAX_MS_NAME)) {
		goto fail;
	};
	if (strlen(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.rem_ms) != 
						strlen(sock.con_msh_name)) {
		goto fail;
	};
	if (0x1000 != sock.msub_sz) {
		goto fail;
	};
	if (0x1000 != sock.con_sz) {
		goto fail;
	};
	if (sock.max_backlog) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* Fail, free_d2app failure after rskt_init_memops */
	lib.use_mport = SIX6SIX_FLAG;
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	free_d2app_errno = ENOMEM;
	free_d2app_rc = -1;
	librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.new_ct = htonl(0x1111);
	memset(&sock, 0, sizeof(sock));
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	free_d2app_errno = 0;
	free_d2app_rc = 0;
	if (skt_rdma_connector != sock.connector) {
		goto fail;
	};
	if (0x1111 != sock.sa.ct) {
		goto fail;
	};
	if (0x2222 != sock.sa.sn) {
		goto fail;
	};
	if (0x3333 != sock.sai.sa.sn) {
		goto fail;
	};
	if (0x5555 != sock.sai.sa.ct) {
		goto fail;
	};
	if (0x123456789abcdef0 != sock.rio_addr) {
		goto fail;
	};
	if (0xaaaabbbbccccdddd != sock.phy_addr) {
		goto fail;
	};
	if (strncmp(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.mso,
						sock.msoh_name, MAX_MS_NAME)) {
		goto fail;
	};
	if (strlen(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.mso) != 
						strlen(sock.msoh_name)) {
		goto fail;
	};
	if (strncmp(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.ms,
						sock.msh_name, MAX_MS_NAME)) {
		goto fail;
	};
	if (strlen(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.ms) != 
						strlen(sock.msh_name)) {
		goto fail;
	};
	if (strncmp(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.rem_ms,
					sock.con_msh_name, MAX_MS_NAME)) {
		goto fail;
	};
	if (strlen(librskt_rsktd_to_app_msg_rx.a_rsp.msg.conn.rem_ms) != 
						strlen(sock.con_msh_name)) {
		goto fail;
	};
	if (0x1000 != sock.msub_sz) {
		goto fail;
	};
	if (0x1000 != sock.con_sz) {
		goto fail;
	};
	if (sock.max_backlog) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	lib.all_must_die = 0;

	/* Fail, rskt_connect_rdma_open */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	rskt_connect_rdma_open_errnum = EBADFD;
	rskt_connect_rdma_open_rc = -1;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rskt_connect_rdma_open_errnum = 0;
	rskt_connect_rdma_open_rc = 0;

	/* Fail, setup_skt_ptrs fails */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	setup_skt_ptrs_rc = -1;
	setup_skt_ptrs_errnum = ENOMEM;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	setup_skt_ptrs_rc = 0;
	setup_skt_ptrs_errnum = 0;

	/* Fail, rsktl_atomic_set_st third call fails */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (!rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_atomic_set_st_rcs = rsktl_atomic_set_st_pass;

	/* SUCCESS */
	rsktl_atomic_set_st_rc_idx = 0;
	free_app2d_ptr_idx = 0;
	free_d2app_ptr_idx = 0;
	if (rskt_connect(skt, &sock_addr)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_write(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	struct rskt_socket_t sock;
	uint8_t buff[0x1000];
	uint8_t skt_buff[0x2020];
	struct rc_entry rsktl_atomic_set_wip_fail1[] = {
		{0, ENOMEM, (int)false, (int)true},
		{0, 0, (int)true, (int)false}
	};
	struct rc_entry rsktl_atomic_set_wip_pass[] = {
		{1, 0, (int)false, (int)true},
		{0, 0, (int)true, (int)false}
	};
	struct rc_entry get_free_bytes_rc_closed3[] = {
		{ 999, 0, 0, 0},
		{ 999, 0, 0, 0},
		{1000, 0, htonl(RSKT_BUF_HDR_FLAG_CLOSING), 0}
	};
	struct rc_entry get_free_bytes_pass[] = {
		{ 0xFFE, 0, 0, 0},
		{ 0xFFE, 0, 0, 0},
		{ 0xFFE, 0, 0, 0},
		{ 0xFFF, 0, 0, 0}
	};

	struct rc_entry send_bytes_rc_fail1[] = {
		{ 1, ENOEXEC, 0, 0}
	};
	struct rc_entry send_bytes_rc_fail2[] = {
		{ 0, 0, 0, 0},
		{ 1, ECHILD, 0, 0}
	};
	struct rc_entry send_bytes_pass[] = {
		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0}
	};

	test_fail = 0;
	/* Fail, lib uninit */
	lib.portno = lib.init_ok = 0;
	free_d2app_rc = 0;
	free_app2d_rc = 0;
	if (-1 != rskt_write(skt, buff, 0x1000)) {
		goto fail;
	};
	lib.portno = lib.init_ok = 3333;

	/* Fail, Null parameter*/
	if (-1 != rskt_write(skt, NULL, 0x1000)) {
		goto fail;
	};

	/* Fail, set_wip fails first time */
	rsktl_atomic_set_wip_rc_idx = 0;
	rsktl_atomic_set_wip_rcs = rsktl_atomic_set_wip_fail1;
	if (-1 != rskt_write(skt, buff, 0x1000)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_atomic_set_wip_rcs = rsktl_atomic_set_wip_pass;
	
	/* Fail, rsktl_sock_ptr fail */
	rsktl_atomic_set_wip_rc_idx = 0;
	rsktl_sock_ptr_rc = NULL;
	rsktl_sock_ptr_errno = ENOENT;
	if (-1 != rskt_write(skt, buff, 0x1000)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_sock_ptr_rc = &sock;
	rsktl_sock_ptr_errno = 0;

	/* Fail, Bad size parameter values */
	rsktl_atomic_set_wip_rc_idx = 0;
	if (-1 != rskt_write(skt, buff, 0)) {
		goto fail;
	};
	if (EINVAL != errno) {
		goto fail;
	};
	rsktl_atomic_set_wip_rc_idx = 0;
	if (-1 != rskt_write(skt, buff, -1)) {
		goto fail;
	};
	rsktl_atomic_set_wip_rc_idx = 0;
	if (-1 != rskt_write(skt, buff, 0x1000)) {
		goto fail;
	};

	/* Fail, socket closed for writing. */
	rsktl_atomic_set_wip_rc_idx = 0;
	sock.msub_p = (volatile uint8_t *)skt_buff;
	sock.tx_buf = (volatile uint8_t *)&(skt_buff[0x20]);
	sock.rx_buf = (volatile uint8_t *)&(skt_buff[0x1020]);
	sock.buf_sz = 0x1000;

	sock.hdr->loc_tx_wr_ptr = 0;
	sock.hdr->loc_tx_wr_flags = htonl(RSKT_BUF_HDR_FLAG_CLOSING);
	sock.hdr->loc_rx_rd_ptr = 0;
	sock.hdr->loc_rx_rd_flags = 0;
	sock.hdr->rem_rx_wr_ptr = 0;
	sock.hdr->rem_rx_wr_flags = 0;
	sock.hdr->rem_tx_rd_ptr = 0;
	sock.hdr->rem_tx_rd_flags = 0;

	if (-1 != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};
	sock.hdr->loc_tx_wr_flags = 0;

	/* Fail, socket closed for writing after 3rd get_free_bytes call */
	rsktl_atomic_set_wip_rc_idx = 0;
	get_free_bytes_rcs = get_free_bytes_rc_closed3;
	get_free_bytes_idx = 0;
	if (-1 != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};
	get_free_bytes_rcs = get_free_bytes_pass;
	sock.hdr->loc_tx_wr_flags = htonl(0);

	/* Fail on first send_bytes call, contiguous transfer */
	rsktl_atomic_set_wip_rc_idx = 0;
	get_free_bytes_idx = 0;
	send_bytes_rcs = send_bytes_rc_fail1;
	send_bytes_idx = 0;
	if (-1 != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};
	
	/* Fail on first send_bytes call, discontiguous transfer */
	rsktl_atomic_set_wip_rc_idx = 0;
	sock.hdr->loc_tx_wr_flags = 0;
	get_free_bytes_idx = 0;
	send_bytes_rcs = send_bytes_rc_fail1;
	send_bytes_idx = 0;
	sock.hdr->loc_tx_wr_ptr = 0x800;
	errno = 0;
	if (-1 != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};

	/* Fail on second send_bytes call, discontiguous transfer */
	rsktl_atomic_set_wip_rc_idx = 0;
	sock.hdr->loc_tx_wr_flags = 0;
	get_free_bytes_idx = 0;
	send_bytes_rcs = send_bytes_rc_fail2;
	send_bytes_idx = 0;
	sock.hdr->loc_tx_wr_ptr = 0x800;
	errno = 0;
	if (-1 != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};
	send_bytes_rcs = send_bytes_pass;

	/* Fail on update_remote_hdr */
	rsktl_atomic_set_wip_rc_idx = 0;
	sock.hdr->loc_tx_wr_flags = 0;
	get_free_bytes_idx = 0;
	send_bytes_idx = 0;
	update_remote_hdr_errnum = EAGAIN;
	update_remote_hdr_rc = 1;
	errno = 0;
	if (-1 != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};
	update_remote_hdr_errnum = 0;
	update_remote_hdr_rc = 0;
	
	/* Success */
	rsktl_atomic_set_wip_rc_idx = 0;
	sock.hdr->loc_tx_wr_flags = 0;
	get_free_bytes_idx = 0;
	send_bytes_idx = 0;
	errno = 0;
	if (0x0FFF != rskt_write(skt, buff, 0x0FFF)) {
		goto fail;
	};
	if (errno) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int test_rskt_read(void)
{
	rskt_h skt = LIBRSKT_H_INVALID;
	struct rskt_socket_t sock;
	uint8_t buff[0x1000];
	uint8_t skt_buff[0x2020];
	struct rc_entry rsktl_atomic_set_rip_fail1[] = {
		{0, ENOMEM, (int)false, (int)true},
		{0, 0, (int)true, (int)false}
	};
	struct rc_entry rsktl_atomic_set_rip_fail2[] = {
		{1, 0, (int)false, (int)true},
		{0, ENOMEM, (int)true, (int)false},
		{0, 0, (int)true, (int)false}
	};
	struct rc_entry rsktl_atomic_set_rip_pass2[] = {
		{1, 0, (int)false, (int)true},
		{1, 0, (int)true, (int)true},
		{0, 0, (int)true, (int)false}
	};
	struct rc_entry rsktl_atomic_set_rip_pass[] = {
		{1, 0, (int)false, (int)true},
		{1, 0, (int)true, (int)true},
		{1, 0, (int)true, (int)true},
		{0, 0, (int)true, (int)false}
	};
	struct rc_entry get_avail_bytes_rc_closed1[] = {
		{ AVAIL_BYTES_END, 0, 0, 0}
	};
	struct rc_entry get_avail_bytes_rc_error1[] = {
		{ AVAIL_BYTES_ERROR, 0, 0, 0}
	};
	struct rc_entry get_avail_bytes_rc_closed2[] = {
		{ 0, 0, 0, 0},
		{ AVAIL_BYTES_END, 0, 0, 0}
	};
	struct rc_entry get_avail_bytes_rc_error2[] = {
		{ 0, 0, 0, 0},
		{ AVAIL_BYTES_ERROR, 0, 0, 0}
	};
	struct rc_entry get_avail_bytes_rc[] = {
		{ 0, 0, 0, 0},
		{ 0x0FFF, 0, 0, 0}
	};

	struct rc_entry read_bytes_cont[] = {
		{ 0x0FFF, 0, 0, 0},
		{ -1, 0, 0, 0}
	};
	struct rc_entry read_bytes_disc[] = {
		{ 0x7FF, 0, 0, 0},
		{ 0x800, 0, 0, 0},
		{ -1, 0, 0, 0}
	};

	test_fail = 0;

	/* Fail, lib uninit */
	lib.portno = lib.init_ok = 0;
	free_d2app_rc = 0;
	free_app2d_rc = 0;
	if (-1 != rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	lib.portno = lib.init_ok = 3333;

	/* Fail, Null parameter*/
	if (-1 != rskt_read(skt, NULL, 0x1000)) {
		goto fail;
	};

	/* Fail, Bad size parameter*/
	if (-1 != rskt_read(skt, buff, 0)) {
		goto fail;
	};

	/* Fail, set_rip fails first time */
	rsktl_atomic_set_rip_rc_idx = 0;
	rsktl_atomic_set_rip_rcs = rsktl_atomic_set_rip_fail1;
	if (-1 != rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	rsktl_atomic_set_rip_rcs = rsktl_atomic_set_rip_fail2;
	
	/* Fail, rsktl_sock_ptr fail */
	rsktl_atomic_set_rip_rc_idx = 0;
	rsktl_sock_ptr_rc = NULL;
	rsktl_sock_ptr_errno = ENOENT;
	if (-1 != rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	rsktl_sock_ptr_rc = &sock;
	rsktl_sock_ptr_errno = 0;

	/* Fail, get_avail_bytes fail on first call */
	sock.msub_p = (volatile uint8_t *)skt_buff;
	sock.tx_buf = (volatile uint8_t *)&(skt_buff[0x20]);
	sock.rx_buf = (volatile uint8_t *)&(skt_buff[0x1020]);
	sock.buf_sz = 0x1000;

	sock.hdr->loc_tx_wr_ptr = 0;
	sock.hdr->loc_tx_wr_flags = 0;
	sock.hdr->loc_rx_rd_ptr = 0;
	sock.hdr->loc_rx_rd_flags = 0;
	sock.hdr->rem_rx_wr_ptr = 0;
	sock.hdr->rem_rx_wr_flags = 0;
	sock.hdr->rem_tx_rd_ptr = 0;
	sock.hdr->rem_tx_rd_flags = 0;

	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	get_avail_bytes_rcs = get_avail_bytes_rc_closed1;
	if (rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	
	/* Fail, get_avail_bytes fail on first call */
	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	get_avail_bytes_rcs = get_avail_bytes_rc_error1;
	if (-1 != rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	
	/* Fail, set_rip fails second  time */
	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	if (-1 != rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	rsktl_atomic_set_rip_rcs = rsktl_atomic_set_rip_pass2;
	
	/* Fail, get_avail_bytes fail on second call */
	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	get_avail_bytes_rcs = get_avail_bytes_rc_closed2;
	if (rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	
	/* Fail, get_avail_bytes fail on second call */
	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	get_avail_bytes_rcs = get_avail_bytes_rc_error2;
	if (-1 != rskt_read(skt, buff, 0x1000)) {
		goto fail;
	};
	get_avail_bytes_rcs = get_avail_bytes_rc;
	
	/* Fail on update_remote_hdr, contiguous transfer */
	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	update_remote_hdr_errnum = EAGAIN;
	sock.hdr->loc_rx_rd_ptr = htonl(0xFFF);
	update_remote_hdr_rc = 1;
	read_bytes_idx = 0;
	read_bytes_rcs = read_bytes_cont;
	errno = 0;

	if (-1 != rskt_read(skt, buff, 0x2000)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};

	/* Fail on update_remote_hdr, discontiguous transfer */
	rsktl_atomic_set_rip_rc_idx = 0;
	get_avail_bytes_idx = 0;
	update_remote_hdr_errnum = EAGAIN;
	update_remote_hdr_rc = 1;
	read_bytes_idx = 0;
	read_bytes_rcs = read_bytes_disc;
	sock.hdr->loc_rx_rd_ptr = ntohl(0x800);
	sock.hdr->rem_rx_wr_ptr = ntohl(0x801);
	errno = 0;
	if (-1 != rskt_read(skt, buff, 0x2000)) {
		goto fail;
	};
	if (test_fail) {
		goto fail;
	};
	update_remote_hdr_errnum = 0;
	update_remote_hdr_rc = 0;
	
	/* Success, discontiguous transfer */
	rsktl_atomic_set_rip_rc_idx = 0;
	rsktl_atomic_set_rip_rcs = rsktl_atomic_set_rip_pass;
	get_avail_bytes_idx = 0;
	read_bytes_idx = 0;
	read_bytes_rcs = read_bytes_disc;
	errno = 0;
	rsktl_atomic_set_rip_rc_idx = 0;
	get_free_bytes_idx = 0;
	errno = 0;
	if (0x0FFF != rskt_read(skt, buff, 0x2000)) {
		goto fail;
	};
	if (errno) {
		goto fail;
	};

	return 0;
fail:
	return 1;
};

typedef int (* test_func)(void);

test_func array_of_tests[] = {
	test_librskt_init, /* Pass */
	test_librskt_finish, /* Pass */
	test_rskt_create_socket, /* Pass */
	test_rskt_destroy_socket, /* Pass */
	test_rskt_bind, /* Pass */
	test_rskt_listen, /* Pass */
	test_rskt_accept, /* Pass */
	test_rskt_connect, /* Pass */
	test_rskt_read, /* Pass */
	test_rskt_write
};

int main(int argc, char *argv[])
{
	uint32_t i, rc;

	rdma_log_init("librskt_test", true);
	g_level = 0;
	for (i = 0; i < sizeof(array_of_tests)/sizeof(test_func); i++) {
		rc = array_of_tests[i]();
		if (rc) {
			goto fail;
		};
	};
	printf("\nPASSED\n");
	rdma_log_close();
	return 0;
fail:
	printf("\nFAILED: %d %d\n", i, rc);
	rdma_log_close();
	return 1;
};

#ifdef __cplusplus
}
#endif
