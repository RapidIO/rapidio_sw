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

#include <sys/mman.h>
#include <sys/time.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <mqueue.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>
#include <unistd.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>

#include "rapidio_mport_lib.h"
#include "rdma_types.h"
#include "liblog.h"
#include "rdmad.h"
#include "rdma_mq_msg.h"
#include "msg_q.h"

#include "librdma.h"
#include "librdma_db.h"

/* Macro to simplify RPC calls */
#define CALL_RPC(func, in, out, ret_if_null, ret_if_fail) \
out = func##_1(&in, client);			  \
if (out == (func##_output *)NULL) {	\
	ERR(#func "_1() call failed\n");	\
	clnt_perror(client, "call failed");	\
	return ret_if_null; \
}	\
if (out->status) { \
	ERR(#func "_1() failed with code: %d\n", out->status);	\
	return ret_if_fail;	\
}

using namespace std;

static unsigned init = 0;	/* Global flag indicating library initialized */

static const char *rpc_host = "localhost";

static CLIENT *client;	/* RPC client */

/** 
 * Global info related to mports and channelized messages.
 */
struct peer_info {
	int mport_id;
	int mport_fd;
	uint16_t destid;
}; /* peer_info */

static struct peer_info peer;

static bool aligned_at_4k(uint32_t n)
{
	const auto FOUR_K = 4*1024;
	return (n % FOUR_K) == 0;
} /* aligned_at_4k() */

static uint32_t round_up_to_4k(uint32_t length)
{
	const auto FOUR_K = 4*1024;

	auto q = length / FOUR_K;
	auto r = length % FOUR_K;

	return (r == 0) ? FOUR_K*q : FOUR_K*(q + 1);
} /* round_up_to_4k() */

static int open_mport(struct peer_info *peer)
{
	get_mport_id_output	*out;
	get_mport_id_input	in;
	int flags = 0;
	struct rio_mport_properties prop;

	/* RPC call, and check return value */
	CALL_RPC(get_mport_id, in, out, -1, -2);

	/* Get the mport ID */
	peer->mport_id = out->mport_id;
	INFO("Using mport_id = %d\n", peer->mport_id);

	/* Now open the port */
	peer->mport_fd = riodp_mport_open(peer->mport_id, flags);
	if (peer->mport_fd <= 0) {
		CRIT("riodp_mport_open(): %s\n", strerror(errno));
		CRIT("Cannot open mport%d, is rio_mport_cdev loaded?\n",
								peer->mport_id);
		return -errno;
	}
	INFO("mport_fd = %d\n", peer->mport_fd);

	/* Read the properties. */
	if (!riodp_query_mport(peer->mport_fd, &prop)) {
		display_mport_info(&prop);
		if (prop.flags &RIO_MPORT_DMA) {
			INFO("DMA is ENABLED\n");
		} else {
			CRIT("DMA capability DISABLED\n");
			close(peer->mport_fd);
			return -3;
		}
		peer->destid = prop.hdid;
	} else {
		/* Unlikely we fail on reading properties, but warn! */
		WARN("%s: Error reading properties from mport!\n");
	}

	return 0;
} /* open_mport() */

/**
 * client_wait_for_destroy_thread_f
 *
 * Thread that runs on the client and waits for an mq_destroy_msg from
 * the client's daemon indicating that a memory space to which the client
 * had connected has now been destroyed.
 *
 * @arg		memory space name
 */
static void *client_wait_for_destroy_thread_f(void *arg)
{
	INFO("STARTED\n");

	if (!arg) {
		CRIT("Destroy message queue not passed. EXITING\n");
		pthread_exit(0);
	}

	msg_q<mq_destroy_msg>	*destroy_mq = (msg_q<mq_destroy_msg> *)arg;

	/* Message buffer for destroy POSIX message */
	mq_destroy_msg	*dm;
	destroy_mq->get_recv_buffer(&dm);

	/* Wait for destroy POSIX message */
	INFO("Waiting for destroy POSIX message on %s\n",
						destroy_mq->get_name().c_str());
	if (destroy_mq->receive()) {
		ERR("Failed to receive 'destroy' POSIX message\n");
		pthread_exit(0);
	}
	INFO("Got 'destroy' POSIX message\n");

	/* Remove the msubs belonging to that ms */
	remove_rem_msubs_in_ms(dm->server_msid);

	/* Remove the ms itself */
	ms_h	msh = find_rem_ms(dm->server_msid);
	if (msh == (ms_h)NULL) {
		CRIT("Failed to find rem_ms with msid(0x%X)\n",
							dm->server_msid);
	} else if (remove_rem_ms(msh) < 0) {
		WARN("Failed to remove msh(0x%lX)\n", msh);
	} else {
		DBG("Removed rem_ms with msid(0x%X) from remote database\n",
							dm->server_msid);
		if (rem_ms_exists(msh)) {
			DBG("IMPOSSIBLE!!!! Still in database!\n");
		}
	}

	/* Send back a destroy_ack message to the local daemon */
	mq_destroy_msg	*dam;
	destroy_mq->get_send_buffer(&dam);
	dam->server_msid = dm->server_msid;
	if (destroy_mq->send()) {
		ERR("Failed to send destroy ack on %s\n", destroy_mq->get_name().c_str());
		delete destroy_mq;
		pthread_exit(0);
	}
	INFO("Sent mq_destroy_ack message to local daemon\n");

	/* Close and unlink the message queue */
	delete destroy_mq;

	pthread_exit(0);
} /* client_wait_for_destroy_thread_f() */

/**
 * wait_for_disc_thread_f
 *
 * Runs on the server, and handles the case when the client disconnects
 * from the memory space on the server.
 *
 * @arg		message queue for disconnect message
 */
static void *wait_for_disc_thread_f(void *arg)
{
	char	mq_rcv_buf[MQ_RCV_BUF_SIZE];
	struct  mq_disconnect_msg *disc_msg;
	mqd_t mq = *((mqd_t *)arg);

	/* Wait for the POSIX disconnect message containing rem_msh */
	INFO("Waiting for DISconnect message...\n");
	if (mq_receive(mq, mq_rcv_buf, MQ_RCV_BUF_SIZE, NULL) == -1) {
		ERR("mq_receive() failed: %s", strerror(errno));
		pthread_exit(0);
	}

	/* Extract message contents */
	disc_msg  = (struct mq_disconnect_msg *)mq_rcv_buf;
	INFO("Rx disconnect to remove client_msubid(0x%X)\n",
						disc_msg->client_msubid);
	/* Find the msub in the database */
	msub_h client_msubh = find_rem_msub(disc_msg->client_msubid);
	if (!client_msubh) {
		WARN("client_msubid(0x%X) not found!\n",
						disc_msg->client_msubid);
	} else {
		/* Find the memory space and mark it as not 'accepted' */
		rem_msub *msub = (rem_msub *)client_msubh;
		ms_h loc_msh = msub->loc_msh;
		if (!loc_msh)
			WARN("Could not find the memory space!!!!\n");
		else
			((loc_ms *)loc_msh)->accepted = false;

		/* Remove client subspace from remote msub database */
		remove_rem_msub(client_msubh);
		INFO("client_msubid(0x%X) removed from database\n",
						disc_msg->client_msubid);
	}

	INFO("Exiting\n");
	pthread_exit(0);
} /* wait_for_disc_thread_f() */

__attribute__((constructor)) int rdma_lib_init(void)
{
	struct timeval	timeout;

	/* Initialize the logger */
	rdma_log_init("librdma.log", 0);

	/* Create RPC client */
	client = clnt_create(rpc_host, RDMAD, RDMAD_1, "udp");
	if (client == NULL) {
		CRIT("RPC clnt_create() call failed\n");
		clnt_pcreateerror(rpc_host);
		return -1;
	}

	/* Set RPC timeout value */
	timeout.tv_sec = 3600;	/* One hour! */	
	timeout.tv_usec = 0;
	clnt_control(client, CLSET_TIMEOUT, (char *)&timeout);

	/* Read back and verify RPC timeout value */
	timeout.tv_sec = 0;	/* Clear before reading */
	clnt_control(client, CLGET_TIMEOUT, (char *)&timeout);
	DBG("RPC timeout = %lu seconds\n", timeout.tv_sec);

	/* Initialize message queue attributes */
	init_mq_attribs();

	if (open_mport(&peer))
		return -2;

	/* Make threads cancellable at some points (e.g. mq_receive) */
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)) {
		WARN("Failed to set cancel state:%s\n",strerror(errno));
		return -3;
	}

	/* Set initialization flag */
	init = 1;
	DBG("Library fully initialized\n ");

	return 0;
} /* rdma_lib_init() */

int rdma_create_mso_h(const char *owner_name, mso_h *msoh)
{
	create_mso_input	in;
	create_mso_output	*out;

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check that owner does not already exist */
	if (find_mso_by_name(owner_name)) {
		CRIT("Cannot create another owner named \'%s\'\n", owner_name);
		return -2;
	}

	/* Set up input parameters */
	in.owner_name 	= (char *)owner_name;

	/* RPC call, and check return value */
	CALL_RPC(create_mso, in, out, -3, -4);

	/* Store in database. mso_conn_id = 0 and owned = true */
	*msoh = add_loc_mso(owner_name, out->msoid, 0, true, (pthread_t)0, nullptr);
	if (!*msoh) {
		WARN("add_loc_mso() failed, msoid = 0x%X\n", out->msoid);
		return -5;
	}

	return out->status;
} /* rdma_create_mso_h() */

/**
 * This thread receives mq_close_mso_msg from the daemon when the owner
 * of the mso destroys the mso while there is an open connection to this
 * user.
 */
static void *mso_close_thread_f(void *arg)
{
	msg_q<mq_close_mso_msg>	*mq = (msg_q<mq_close_mso_msg> *)arg;

	/* Wait for the POSIX mq_close_mso_msg */
	INFO("Waiting for mq_close_mso_msg message...\n");
	mq_close_mso_msg 	*close_msg;

	mq->get_recv_buffer(&close_msg);

	if (mq->receive())
		pthread_exit(0);

	INFO("Got mq_close_mso_msg for msoid= 0x%X\n", close_msg->msoid);

	/* Find the mso in the local database by its msoid */
	mso_h msoh = find_mso(close_msg->msoid);
	if (!msoh) {
		CRIT("Could not find msoid(0x%X) in database\n", close_msg->msoid);
		delete mq;
		pthread_exit(0);
	}

	/* Remove mso with specified msoid, msoh from database */
	if (remove_loc_mso(msoh))
		WARN("Failed removing msoid(0x%X) msoh(0x%lX)\n",
							close_msg->msoid, msoh);
	else {
		INFO("mq_close_mso_msg successfully processed\n");
	}

	/* Delete the message queue */
	INFO("Deleting '%s'\n", mq->get_name().c_str());
	delete mq;

	pthread_exit(0);
} /* mso_close_thread_f() */


int rdma_open_mso_h(const char *owner_name, mso_h *msoh)
{
	open_mso_input	in;
	open_mso_output	*out;

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check that this owner isn't already open */
	if (mso_is_open(owner_name)) {
		WARN("%s is already open!\n", owner_name);
		return -2;
	}

	/* Set up input parameters */
	in.owner_name 	= (char *)owner_name;

	/* RPC call, and check return value */
	CALL_RPC(open_mso, in, out, -3, -4);

	/* Open message queue for receiving mso close messages. Such messages are
	 * sent when the owner of an 'mso' decides to destroy the mso. The close
	 * messages are sent to users of the mso who have opened the mso.
	 * The message instruct those users to close the mso since it will be gone. */
	stringstream	qname;
	qname << '/' << owner_name << out->mso_conn_id;
	msg_q<mq_close_mso_msg>	*mso_close_mq;
	try {
		mso_close_mq = new msg_q<mq_close_mso_msg>(qname.str(), MQ_OPEN);
	}
	catch(msg_q_exception e) {
		e.print();
		return -5;
	}
	INFO("Opened message queue '%s'\n", qname.str().c_str());

	/* Create thread for handling mso close requests */
	pthread_t mso_close_thread;
	if (pthread_create(&mso_close_thread, NULL, mso_close_thread_f, (void *)mso_close_mq)) {
		WARN("Failed to create mso_close_thread: %s\n", strerror(errno));
		delete mso_close_mq;
		return -7;
	}
	INFO("Created mso_close_thread with argument %d passed to it\n", mso_close_mq);

	/* Store in database, mso_conn_id from daemon, 'owned' is false, save the
	 * thread and message queue to be used to notify app when mso is destroyed
	 * by its owner. */
	*msoh = add_loc_mso(owner_name,
			    out->msoid,
			    out->mso_conn_id,
			    false,
			    mso_close_thread,
			    mso_close_mq);
	if (!*msoh) {
		WARN("add_loc_mso() failed, msoid = 0x%X\n", out->msoid);
		return -4;
	}

	return 0;
} /* rdma_open_mso_h() */

/**
 * Closes memory space for a particular user (non-owner)
 */
struct close_ms {
	close_ms(mso_h msoh) : ok(true), msoh(msoh) {}

	void operator()(struct loc_ms *ms) {
		if (rdma_close_ms_h(msoh, ms_h(ms))) {
			WARN("rdma_close_ms_h failed: msoh = 0x%lX, msh = 0x%lX\n",
								msoh, ms_h(ms));
			ok = false;
		} else {
			INFO("msh(0x%lX) owned by msoh(0x%lX) closed\n",
								ms_h(ms), msoh);
		}
	}
	bool	ok;

private:
	mso_h	msoh;
}; /* close_ms */

int rdma_close_mso_h(mso_h msoh)
{
	close_mso_input		in;
	close_mso_output	*out;
	close_ms		cms(msoh);

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check for NULL msoh */
	if (!msoh) {
		WARN("msoh is NULL\n");
		return -2;
	}

	/* Check if msoh has already been closed (as a result of the owner
	 * destroying it and sending a close message to this user */
	if (!mso_h_exists(msoh)) {
		WARN("msoh no longer exists\n");
		return -3;
	}

	/* Get list of memory spaces opened by this owner */
	list<struct loc_ms *>	ms_list(get_num_ms_by_msoh(msoh));
	get_list_msh_by_msoh(msoh, ms_list);

	DBG("ms_list now has %d elements\n", ms_list.size());

	/* For each one of the memory spaces, close */
	for_each(ms_list.begin(), ms_list.end(), cms);

	/* Fail on any error */
	if (!cms.ok)
		return -3;

	/* Set up input parameters */
	in.msoid = ((struct loc_mso *)msoh)->msoid;
	in.mso_conn_id = ((struct loc_mso *)msoh)->mso_conn_id;

	/* Since we are closing the mso ourselves, the mso_close_thread_f
	 * should be killed. We do this before closing the message queue
	 * since otherwise I'm not sure what the mq_receive() will do.
	 */
	pthread_t  close_notify_thread = loc_mso_get_close_notify_thread(msoh);
	if (!close_notify_thread)
		WARN("close_notify_thread is NULL!!\n");
	else if (pthread_cancel(close_notify_thread)) {
		WARN("Failed to cancel close_notify_thread for msoh(0x%X)\n", msoh);
	}

	/**
	 * Probably good practice to close the message queue here before
	 * calling close_mso_1() since close_mso_1() will close and unlink
	 * the message queue from the daemon. Unlink probably needs a queue
	 * that is not open by anyone in order to succeed.
	 */
	msg_q<mq_close_mso_msg>	*close_notify_mq = loc_mso_get_close_notify_mq(msoh);
	if (close_notify_mq == nullptr)
		WARN("close_notify_mq is NULL\n");
	else
		delete close_notify_mq;

	/* close_mso_1() will remove the message queue corresponding to the mso
	 * user from the ms_owner struct in the daemon */
	CALL_RPC(close_mso, in, out, -4, -5);

	/* Take it out of databse */
	if (remove_loc_mso(msoh) < 0) {
		WARN("Failed to find 0x%lX in db\n", msoh);
		return -6;
	}
	INFO("msoh(0x%lX) removed from local database\n", msoh);

	return out->status;
} /* rdma_close_mso_h() */

/**
 * Destroys memory space for a particular owner.
 */
struct destroy_ms {
	destroy_ms(mso_h msoh) :  ok(true), msoh(msoh) {}

	void operator()(struct loc_ms * ms) {
		if (rdma_destroy_ms_h(msoh, ms_h(ms))) {
			WARN("rdma_destroy_ms_h failed: msoh = 0x%lX, msh = 0x%lX\n",
								msoh, ms_h(ms));
			ok = false;
		} else {
			DBG("msh(0x%lX) owned by msoh(0x%lX) destroy\n",
								ms_h(ms), msoh);
		}
	}

	bool	ok;

private:
	mso_h	msoh;
}; /* destroy_ms */

int rdma_destroy_mso_h(mso_h msoh)
{
	destroy_mso_input	in;
	destroy_mso_output	*out;
	destroy_ms		dms(msoh);

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}
	
	/* Check for NULL msoh */
	if (!msoh) {
		WARN("msoh is NULL\n");
		return -2;
	}

	/* Get list of memory spaces owned by this owner */
	list<struct loc_ms *>	ms_list(get_num_ms_by_msoh(msoh));
	get_list_msh_by_msoh(msoh, ms_list);

	INFO("ms_list now has %d elements\n", ms_list.size());

	/* For each one of the memory spaces, call destroy */
	for_each(ms_list.begin(), ms_list.end(), dms);

	/* Fail on any error */
	if (!dms.ok) {
		ERR("Failed in destroy_ms::operator()\n");
		return -3;
	}

	/* Set up input parameters */
	in.msoid = ((struct loc_mso *)msoh)->msoid;

	/* RPC call, and check return value */
	CALL_RPC(destroy_mso, in, out, -4, -5);

	/* Remove from database */
	if (remove_loc_mso(msoh)) {
		CRIT("Failed to remove mso from database\n");
		return -6;
	}
	return out->status;
} /* rdma_destroy_mso_h() */

int rdma_create_ms_h(const char *ms_name,
		  mso_h msoh,
		  uint32_t req_bytes,
		  uint32_t flags,
		  ms_h *msh,
		  uint32_t *bytes)
{
	create_ms_input	 in;
	create_ms_output *out;
	uint32_t	dummy_bytes;

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check for NULL parameters */
	if (!ms_name || !msoh || !msh) {
		ERR("NULL param: ms_name=%p, msoh=%u, msh=%p",
			ms_name, msoh, msh);
		return -2;
	}

	/* Disallow creating a duplicate ms name */
	if (find_loc_ms_by_name(ms_name)) {
		WARN("A memory space named \'%s\' exists\n", ms_name);
		return -3;
	}

	/* A memory space must be aligned at 4K, therefore any previous
	 * memory space must be rounded up in size to the nearest 4K.
	 * But if caller knows their 'req_bytes' are aligned, allow them
	 * to pass NULL for 'bytes' */
	if (!bytes)
		bytes = &dummy_bytes;
	*bytes = round_up_to_4k(req_bytes);

	/* Set up input parameters */
	in.ms_name = (char *)ms_name;
	in.msoid   = ((struct loc_mso *)msoh)->msoid;
	in.bytes   = *bytes;
	in.flags   = flags;

	/* RPC call, and check return value */
	CALL_RPC(create_ms, in, out, -3, -4);

	*msh = add_loc_ms(ms_name,*bytes, msoh, out->msid, 0, true, 
				0, -1,
				0, nullptr);
	if (!*msh) {
		ERR("Failed to store ms in database\n");
		return -7;
	}
	INFO("Info for '%s' stored in database\n", ms_name);

	return 0;
} /* rdma_create_ms_h() */

static void *ms_close_thread_f(void *arg)
{
	msg_q<mq_close_ms_msg>	*close_mq = (msg_q<mq_close_ms_msg> *)arg;

	/* Check for NULL */
	if (!arg) {
		CRIT("Null 'arg' passed. Failing\n");
		pthread_exit(0);
	}

	/* Wait for the POSIX mq_close_ms_msg */
	INFO("Waiting for mq_close_ms_msg message...\n");
	mq_close_ms_msg *close_msg;
	close_mq->get_recv_buffer(&close_msg);
	if (close_mq->receive()) {
		delete close_mq;
		pthread_exit(0);
	}

	INFO("mq_close_ms_msg for msid= 0x%X\n", close_msg->msid);
	/* Find the ms in local database */
	ms_h msh = find_loc_ms(close_msg->msid);
	if (!msh) {
		ERR("Could not find ms(0x%X)\n", close_msg->msid);
		delete close_mq;
		pthread_exit(0);
	}

	/* Remove ms with specified msid from database */
	if (remove_loc_ms(msh)) {
		WARN("Failed for msid(0x%X)\n", close_msg->msid);
	}
	INFO("msid(0x%X) removed from database\n", close_msg->msid);

	/* Close the queue */
	delete close_mq;

	INFO("mq_close_ms_msg processed successfully\n");
	pthread_exit(0);
} /* ms_close_thread_f() */

int rdma_open_ms_h(const char *ms_name,
		   mso_h msoh,
		   uint32_t flags,
		   uint32_t *bytes,
		   ms_h *msh)
{
	open_ms_input in;
	open_ms_output *out;

	/* Check that library has been initialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Set up input parameters */
	in.ms_name = (char *)ms_name;
	in.msoid   = ((struct loc_mso *)msoh)->msoid;
	in.flags   = flags;

	/* RPC call, and check return value */
	CALL_RPC(open_ms, in, out, -2, -3);
	INFO("Opened '%s' in the daemon\n", ms_name);

	/* Create message queue for receiving ms close messages. */
	stringstream  qname;
	qname << '/' << ms_name << out->ms_conn_id;
	msg_q<mq_close_ms_msg>	*close_mq;
	try {
		/* Call to open_ms_1() creates the message queue, so open it */
		close_mq = new msg_q<mq_close_ms_msg>(qname.str(), MQ_OPEN);
	}
	catch(msg_q_exception e) {
		e.print();
		return -6;
	}
	INFO("Created message queue '%s'\n", qname.str().c_str());

	/* Create thread for handling ms close requests (destory notifications) */
	pthread_t  ms_close_thread;
	if (pthread_create(&ms_close_thread, NULL, ms_close_thread_f, close_mq)) {
		WARN("Failed to create ms_close_thread: %s\n", strerror(errno));
		return -7;
	}
	INFO("Created ms_close_thread\n");

	/* Store memory space info in database */
	*msh = add_loc_ms(ms_name,
			  out->bytes,
			  msoh,
			  out->msid,
			  out->ms_conn_id,
			  false,
			  0,
			  -1,
			  ms_close_thread,
			  close_mq);
	if (!*msh) {
		WARN("Failed to store ms in database\n");
		return -3;
	}
	INFO("Stored info about '%s' in database\n", ms_name);

	*bytes = out->bytes;

	return 0;
} /* rdma_open_ms_h() */

/**
 * Destroys memory sub-space for a particular space.
 */
struct destroy_msub {
	destroy_msub(ms_h msh) :  ok(true), msh(msh) {}

	void operator()(struct loc_msub * msub) {
		if (rdma_destroy_msub_h(msh, msub_h(msub))) {
			WARN("rdma_destroy_msub_h failed: msh=0x%lX, msubh=0x%lX",
							msh, msub_h(msub));
			ok = false;
		} else {
			INFO("msubh(0x%lX) of msh(0x%lX) destroyed\n", msub_h(msub), msh);
		}
	}

	bool	ok;

private:
	ms_h	msh;
}; /* destroy_msub */

static int destroy_msubs_in_msh(ms_h msh)
{
	uint32_t 	msid = ((struct loc_ms *)msh)->msid;
	destroy_msub	dmsub(msh);

	/* Get list of memory sub-spaces belonging to this msh */
	list<struct loc_msub *>	msub_list(get_num_loc_msub_in_ms(msid));
	get_list_loc_msub_in_msid(msid, msub_list);

	DBG("%d msub(s) in msh(0x%lX):\n", msub_list.size(), msh);
#ifdef DEBUG
	copy(msub_list.begin(),
	     msub_list.end(),
	     ostream_iterator<loc_msub *>(cout, "\n"));
#endif

	/* For each one of the memory sub-spaces, call destroy */
	for_each(msub_list.begin(), msub_list.end(), dmsub);

	/* Fail on any error */
	if (!dmsub.ok)
		return -2;

	return 0;
} /* destroy_msubs_in_msh */

int rdma_close_ms_h(mso_h msoh, ms_h msh)
{
	close_ms_input	in;
	close_ms_output	*out;

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check for NULL parameters */
	if (!msoh || !msh) {
		ERR("Invalid param(s): msoh=0x%lX, msh=0x%lX\n", msoh, msh);
		return -2;
	}

	/* Check if msh has already been closed (as a result of the owner
	 * destroying it and sending a close message to this user) */
	if (!loc_ms_exists(msh)) {
		/* This is NOT an error; the memory space was destroyed
		 * by its owner. Consider it closed by just warning and
		 * returning a success code */
		WARN("msh no longer exists\n");
		return 0;
	}

	/* Destroy msubs opened under this msh */
	if (destroy_msubs_in_msh(msh)) {
		ERR("Failed to destroy msubs belonging to msh(0x%lX)\n", msh);
		return -3;
	}

	/* Kill the disconnection thread, if it exists */
	pthread_t  disc_thread = loc_ms_get_disc_thread(msh);
	if (!disc_thread)
		WARN("disc_thread is NULL.\n");
	else
		if (pthread_cancel(disc_thread)) {
			WARN("Failed to kill disc_thread for msh(0x%X):%s\n",
						msh, strerror(errno));
		}

	/* Since we are closing the ms ourselves, the ms_close_thread_f
	 * should be killed. We do this before closing the message queue
	 * since otherwise I'm not sure what the mq_receive() will do.
	 */
	pthread_t  close_thread = loc_ms_get_close_thread(msh);
	if (!close_thread) {
		WARN("close_thread is NULL!!\n");
	} else {
		if (pthread_cancel(close_thread)) {
			WARN("phread_cancel(close_thread): %s\n",
							strerror(errno)); }
	}

	/* Since the daemon created the 'close_mq', closing it BEFORE
	 * calling close_ms_1() ensures it can be unlinked there withour error
	 */
	msg_q<mq_close_ms_msg> *close_mq = loc_ms_get_destroy_notify_mq(msh);
	delete close_mq;

	/* Set up input parameters */
	in.msid    	= ((struct loc_ms *)msh)->msid;
	in.ms_conn_id   = ((struct loc_ms *)msh)->ms_conn_id;

	/* close_ms_1() will remove the message queue corresponding to the ms
	 * user from the ms_owner struct in the daemon, and close the queue */
	CALL_RPC(close_ms, in, out, -4, -5);

	/* Take it out of databse */
	if (remove_loc_ms(msh) < 0) {
		WARN("Failed to find msh(0x%lX) in db\n", msh);
		return -6;
	}
	INFO("msh(0x%lX) removed from local database\n", msh);

	return 0;
} /* rdma_close_ms_h() */


int rdma_destroy_ms_h(mso_h msoh, ms_h msh)
{
	destroy_ms_input	in;
	destroy_ms_output	*out;
	destroy_msub		dmsub(msh);
	loc_ms 			*ms;

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check for NULL parameters */
	if (!msoh || !msh) {
		WARN("Invalid param(s): msoh=0x%lX, msh=0x%lX\n", msoh, msh);
		return -2;
	}

	ms = (loc_ms *)msh;

	/* Set up input parameters */
	in.msid = ms->msid;
	in.msoid= ((loc_mso *)msoh)->msoid;

	/* Destroy msubs in this msh */
	if (destroy_msubs_in_msh(msh)) {
		ERR("Failed to destroy msubs belonging to msh(0x%lX)\n", msh);
		return -3;
	}

	/* Remove the remote memory subspace provided by the client
	 * when it connected to the server using rdma_conn_ms_h() */
	remove_rem_msub_by_loc_msh(msh);

	/* RPC call, and check return value */
	CALL_RPC(destroy_ms, in, out, -4, -5);

	/* Kill the disconnection thread, if it exists */
	pthread_t  disc_thread = loc_ms_get_disc_thread(msh);
	if (!disc_thread)
		WARN("disc_thread is NULL.\n");
	else
		if (pthread_cancel(disc_thread)) {
			WARN("Failed to kill disc_thread for msh(0x%X):%s\n",
						msh, strerror(errno));
		}

	/**
	 * Daemon should have closed the message queue so we can close and
	 * unlink it here. */
	mqd_t	disc_mq = loc_ms_get_disc_notify_mq(msh);
	if (disc_mq == -1)
		WARN("disc_mq is -1\n");
	else {
		if (mq_close(disc_mq) == -1)
			WARN("Cannot close disc_mq: %s\n", strerror(errno));
		else {
			INFO("disc_mq successfully closed\n");
			string mq_name(ms->name);
			mq_name.insert(0, 1, '/');
			if (mq_unlink(mq_name.c_str())) {
				WARN("Cannot unlink '%s': %s\n", mq_name.c_str(),
								strerror(errno));
			} else {
				INFO("'%s' unlinked\n", mq_name.c_str());
			}
		}
	}

	/* Memory space removed in daemon, remove from databse as well */
	if (remove_loc_ms(msh) < 0) {
		WARN("Failed to find 0x%lX in db\n", msh);
		return -5;
	}

	INFO("msh(0x%lX) removed from local database\n", msh);

	return 0;
} /* rdma_destroy_ms_h() */

int rdma_create_msub_h(ms_h	msh,
		       uint32_t	offset,
		       uint32_t	req_bytes,
		       uint32_t	flags,
		       msub_h	*msubh)
{
	(void)flags;
	create_msub_input	in;
	create_msub_output	*out;

	/* Check that library has been intialized */
	if (!init) {
		CRIT("RDMA library not initialized\n");
		return -1;
	}

	/* Check for NULL */
	if (!msh || !msubh) {
		ERR("NULL param: msh=0x%lX, msubh=%p\n", msh, msubh);
		return -2;
	}

	/* In order for mmap() to work, the phys addr must be aligned
	 * at 4K. This means the mspace must be aligned, the msub
	 * must have a size that is a multiple of 4K, and the offset
	 * of the msub within the mspace must be a multiple of 4K.
	 */
	if (!aligned_at_4k(offset)) {
		ERR("Offset must be a multiple of 4K\n");
		return -2;
	}

	/* Set up input parameters */
	in.msid		= ((struct loc_ms *)msh)->msid;
	in.offset	= offset;
	in.req_bytes	= req_bytes;

	DBG("msid = 0x%X, offset = 0x%X, req_bytes = 0x%x\n",
		in.msid, in.offset, in.req_bytes);
	/* RPC call to create a memory subspace, and check return value */
	CALL_RPC(create_msub, in, out, -1, -2);

	INFO("out->bytes=0x%X, peer.mport_fd=%d, out->phys_addr=0x%lX\n",
				out->bytes, peer.mport_fd, out->phys_addr);

	/* Store msubh in database, obtain pointer thereto, convert to msub_h */
	*msubh = (msub_h)add_loc_msub(out->msubid,
				      in.msid,
				      out->bytes,
				      64,	/* 64-bit RIO address */
				      out->rio_addr,
				      0,	/* Bits 66 and 65 */
				      out->phys_addr);

	/* Adding to database should never fail, but just in case... */
	if (!*msubh) {
		WARN("Failed to add msub to database\n");
		return -5;
	}
	
	return 0;
} /* rdma_create_msub_h() */

int rdma_destroy_msub_h(ms_h msh, msub_h msubh)
{
	destroy_msub_input	in;
	destroy_msub_output	*out;
	struct loc_msub 	*msub;

	/* Check that library has been intialized */
	if (!init) {
		WARN("RDMA library not initialized\n");
		return -1;
	}

	/* Check for NULL handles */
	if (!msh || !msubh) {
		WARN("Invalid param(s): msh=0x%lX, msubh=0x%lX\n", msh, msubh);
		return -2;
	}

	/* Convert handle to an msub pointer to the element the database */
	msub = (struct loc_msub *)msubh;

	/* Set up input parameters */
	in.msid		= ((struct loc_ms *)msh)->msid;
	in.msubid	= msub->msubid;

	/* RPC call to destroy the memory subspace, and check return value */
	CALL_RPC(destroy_msub, in, out, -5, -6);

	/* Remove msub from database */
	if (remove_loc_msub(msubh) < 0) {
		WARN("Failed to remove %p from database\n", msub);
		return -7;
	}

	return 0;
} /* rdma_destroy_msub() */

int rdma_mmap_msub(msub_h msubh, void **vaddr)
{
	struct loc_msub *pmsub = (struct loc_msub *)msubh;

	if (!pmsub) {
		WARN("msubh is NULL\n");
		return -1;
	}

	if (!vaddr) {
		WARN("vaddr is NULL\n");
		return -2;
	}

	INFO("mmap() mport_fd = %d, phys_addr = 0x%lX\n",
						peer.mport_fd, pmsub->paddr);
	*vaddr = mmap(NULL,
		 pmsub->bytes,
		 PROT_READ|PROT_WRITE,
		 MAP_SHARED,
		 peer.mport_fd,
		 pmsub->paddr);

	if (*vaddr == MAP_FAILED) {
		WARN("mmap(0x%lX) failed: %s\n", pmsub->paddr, strerror(errno));
		return -3;
	}
	DBG("msub mapped to vaddr(%p)\n", *vaddr);

	/* Zero-out a subspace before passing it to the app */
	memset((uint8_t *)*vaddr, 0, pmsub->bytes);

	return 0;
} /* rdma_mmap_msub() */

int rdma_munmap_msub(msub_h msubh, void *vaddr)
{
	struct loc_msub *pmsub = (struct loc_msub *)msubh;

	if (!pmsub) {
		ERR("msubh is NULL\n");
		return -1;
	}

	if (!vaddr) {
		ERR("vaddr is NULL\n");
		return -2;
	}

	DBG("Unmapping vaddr(%p), of size %u\n", vaddr, pmsub->bytes);
	if (munmap(vaddr, pmsub->bytes) == -1) {
	        ERR("munmap(): %s\n", strerror(errno));
		return -3;
	}

	return 0;
} /* rdma_unmap_msub() */

int rdma_accept_ms_h(ms_h loc_msh,
		     msub_h loc_msubh,
		     msub_h *rem_msubh,
		     uint32_t *rem_msub_len,
		     uint64_t timeout_secs)
{
	accept_input		accept_in;
	accept_output		*accept_out;
	undo_accept_input	undo_accept_in;
	undo_accept_output	*undo_accept_out;

	/* Check that library has been intialized */
	if (!init) {
		ERR("RDMA library not initialized\n");
		return -1;
	}

	/* Check that parameters are not NULL */
	if (!loc_msh || !loc_msubh || !rem_msubh) {
		ERR("loc_msh=0x%lX,loc_msubh=0x%lX,rem_msubh=%p\n",
					loc_msh, loc_msubh, rem_msubh);
		return -2;
	}

	loc_ms	*ms = (loc_ms *)loc_msh;

	/* If this application has already accepted a connection, fail */
	if (ms->accepted) {
		ERR("Already accepted a connection and it is still active!\n");
		return -3;
	}

	/* Send the memory space name and the 'accept' parameters to the daemon
	 * over RPC. This triggers channelized message reception waiting for 
	 * a connection on the specified memory space. Then sends the accept
	 * parameters also in a channelized message, to the remote daemon */
	accept_in.loc_ms_name		= (char *)ms->name;
	accept_in.loc_msid		= ms->msid;
	accept_in.loc_msubid		= ((struct loc_msub *)loc_msubh)->msubid;
	accept_in.loc_bytes		= ((struct loc_msub *)loc_msubh)->bytes;
	accept_in.loc_rio_addr_len	= ((struct loc_msub *)loc_msubh)->rio_addr_len;
	accept_in.loc_rio_addr_lo	= ((struct loc_msub *)loc_msubh)->rio_addr_lo;
	accept_in.loc_rio_addr_hi	= ((struct loc_msub *)loc_msubh)->rio_addr_hi;

	/* Create connection/disconnection message queue */
	string mq_name(ms->name);
	mq_name.insert(0, 1, '/');

	DBG("mq_name = %s\n", mq_name.c_str());

	/* Must create the queue before calling the daemon since the
	 * connect request from the remote daemon might come quickly
	 * and try to open the queue to notify us.
	 */
	msg_q<mq_connect_msg>	*connect_mq;
	try {
		connect_mq = new msg_q<mq_connect_msg>(mq_name, MQ_CREATE);
	}
	catch(msg_q_exception e) {
		e.print();
		ERR("Failed to create connect_mq\n");
		return -5;
	}
	INFO("Message queue %s created for connection from %s\n",
						mq_name.c_str(), ms->name);

	/* Call the accept() code in the daemon */
	CALL_RPC(accept, accept_in, accept_out, -3, -4);

	/* Await 'connect()' from client */
	mq_connect_msg	*conn_msg;
	connect_mq->get_recv_buffer(&conn_msg);
	INFO("Waiting for connect message...\n");
	if (timeout_secs) {
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += timeout_secs;

		if (connect_mq->timed_receive(&tm)) {
			ERR("Failed to receive connect message before timeout\n");
			delete connect_mq;
			/* Calling undo_accept() to remove the memory space from the list
			 * of memory spaces awaiting a connect message from a remote daemon.
			 */
			undo_accept_in.server_ms_name = (char *)ms->name;
			CALL_RPC(undo_accept, undo_accept_in, undo_accept_out, -6, -7);
			return -5;
		}
	} else {
		if (connect_mq->receive()) {
			ERR("Failed to receive connect message\n");
			delete connect_mq;
			return -5;
		}
	}
	INFO("Connect message received!\n");

	/* Store info about remote msub in database and return handle */
	*rem_msubh = (msub_h)add_rem_msub(conn_msg->rem_msubid,
					  conn_msg->rem_msid,
					  conn_msg->rem_bytes,
					  conn_msg->rem_rio_addr_len,
					  conn_msg->rem_rio_addr_lo,
					  conn_msg->rem_rio_addr_hi,
					  conn_msg->rem_destid_len,
					  conn_msg->rem_destid,
					  loc_msh);
	INFO("rem_bytes = %d, rio_addr = 0x%lX\n",
			conn_msg->rem_bytes, conn_msg->rem_rio_addr_lo);

	/* Return remote msub length to application */
	*rem_msub_len = conn_msg->rem_bytes;

	/* Done with 'connect' message queue. Delete & create a disconnect mq */
	delete connect_mq;
	static mqd_t mq;
	mq = mq_open(mq_name.c_str(), O_RDWR | O_CREAT, 0644, &attr);
	if (mq == (mqd_t)-1) {
		WARN("Failed to create disconnection mq: %s\n", strerror(errno));
		return -5;
	}

	pthread_t wait_for_disc_thread;
	if (pthread_create(&wait_for_disc_thread, NULL, wait_for_disc_thread_f, &mq)) {
		WARN("Failed to create wait_for_disc_thread: %s\n", strerror(errno));
		return -6;
	}
	INFO("Disconnection thread for '%s' created\n", ms->name);

	/* Add conn/disc message queue and disc thread to database entry */
	ms->disc_thread = wait_for_disc_thread;
	ms->disc_notify_mq = mq;
	ms->accepted = true;

	return 0;
} /* rdma_accept_ms_h() */

int rdma_conn_ms_h(uint8_t rem_destid_len,
		   uint32_t rem_destid,
		   const char *rem_msname,
		   msub_h loc_msubh,
		   msub_h *rem_msubh,
		   uint32_t *rem_msub_len,
		   ms_h	  *rem_msh,
		   uint64_t timeout_secs)
{
	send_connect_input	in;
	send_connect_output	*out;
	struct loc_msub		*loc_msub = (struct loc_msub *)loc_msubh;

	INFO("ENTER\n");

	/* Check that library has been intialized */
	if (!init) {
		WARN("RDMA library not initialized\n");
		return -1;
	}

	/* Remote msubh pointer cannot point to NULL */
	if (!rem_msubh) {
		WARN("rem_msubh cannot be NULL\n");
		return -2;
	}

	/* Check for invalid destid */
	if (rem_destid_len == 16 && rem_destid==0xFFFF) {
		WARN("Invalid destid 0x%X\n", rem_destid);
		return -3;
	}
	INFO("Connecting to '%s' on destid(0x%X)\n", rem_msname, rem_destid);
	/* Set up parameters for RPC call */
	in.server_msname	= (char *)rem_msname;
	in.server_destid_len	= rem_destid_len;
	in.server_destid	= rem_destid;
	in.client_destid_len	= 16;
	in.client_destid	= peer.destid;

	if (loc_msubh) {
		in.client_msid		= loc_msub->msid;
		in.client_msubid	= loc_msub->msubid;
		in.client_bytes		= loc_msub->bytes;
		in.client_rio_addr_len	= loc_msub->rio_addr_len;
		in.client_rio_addr_lo	= loc_msub->rio_addr_lo;
		in.client_rio_addr_hi	= loc_msub->rio_addr_hi;
	}
	
	/* NOTE: MUST create the message queue before the RPC call to
	 * send_connect_1() since the call may result in creating a thread
	 * in the daemon that attempts to access the message queue BEFORE
	 * it is created here resulting in an error. */
	string mq_name(rem_msname);
	mq_name.insert(0, 1, '/');
	msg_q<mq_accept_msg>	*accept_mq;
	try {
		accept_mq = new msg_q<mq_accept_msg>(mq_name, MQ_CREATE);
	}
	catch(msg_q_exception e) {
		e.print();
		return -6;
	}
	INFO("Created 'accept' message queue: '%s'\n", mq_name.c_str());

	/* Call send_connect_1(). If it fails delete the message queue */
	out = send_connect_1(&in, client);
	if (out == (send_connect_output *)NULL) {
		ERR("send_connect_1() call failed\n");
		clnt_perror(client, "call failed");
		delete accept_mq;
		return -4;
	}
	if (out->status) {
		ERR("send_connect_1() failed with code: %d\n", out->status);
		delete accept_mq;
		return -5;
	}

	/* Map the accept_msg to the receive buffer of the queue */
	mq_accept_msg 	*accept_msg;
	accept_mq->get_recv_buffer(&accept_msg);

	INFO(" Waiting for accept message...\n");
	if (timeout_secs) {
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += timeout_secs;

		if (accept_mq->timed_receive(&tm)) {
			ERR("Failed to receive accept message after timeout\n");
			/* Remove the message queue from the awaiting-accept list */
			undo_connect_input	undo_connect_in;
			undo_connect_output *undo_connect_out;

			undo_connect_in.server_ms_name = (char *)rem_msname;
			CALL_RPC(undo_connect, undo_connect_in, undo_connect_out, -5, -6);
			delete accept_mq;
			return -7;
		}
	} else {
		if (accept_mq->receive()) {
			ERR("Failed to receive accept message\n");
			delete accept_mq;
			return -7;
		}
	}
	INFO(" Accept message received!\n");

	if (accept_msg->server_destid != rem_destid) {
		WARN("WRONG destid(0x%X) in accept message!\n", accept_msg->server_destid);
		accept_msg->server_destid = rem_destid;	/* FIXME: should not need to do that */
		accept_msg->server_destid_len = 16;	/* FIXME: should not need to do that */
	}
	/* Store info about remote msub in database and return handle */
	*rem_msubh = (msub_h)add_rem_msub(accept_msg->server_msubid,
					  accept_msg->server_msid,
					  accept_msg->server_bytes,
					  accept_msg->server_rio_addr_len,
					  accept_msg->server_rio_addr_lo,
					  accept_msg->server_rio_addr_hi,
					  accept_msg->server_destid_len,
					  accept_msg->server_destid,
					  0);
	INFO("Remote msubh has size %d, rio_addr = 0x%lX\n",
			accept_msg->server_bytes, accept_msg->server_rio_addr_lo);
	INFO("rem_msub has destid = 0x%X, destid_len = 0x%X\n",
			accept_msg->server_destid, accept_msg->server_destid_len);

	/* Remote memory subspace length */
	*rem_msub_len = accept_msg->server_bytes;

	/* Save server_msid since we need to store that in the databse */
	uint32_t server_msid = accept_msg->server_msid;

	/* Accept message queue is no longer needed */
	delete accept_mq;

	/* Create a message queue for the 'destroy' message */
	msg_q<mq_destroy_msg>	*destroy_mq;
	try {
		destroy_mq = new msg_q<mq_destroy_msg>(mq_name, MQ_CREATE);
	}
	catch(msg_q_exception e) {
		e.print();
		return -6;
	}
	INFO("destroy_mq (%s) created\n", destroy_mq->get_name().c_str());

	/* Create a thread for the 'destroy' messages */
	pthread_t client_wait_for_destroy_thread;
	if (pthread_create(&client_wait_for_destroy_thread,
			   NULL,
			   client_wait_for_destroy_thread_f,
			   destroy_mq)) {
		WARN("Failed to create client_wait_for_destroy_thread: %s\n",
							strerror(errno));
		delete destroy_mq;
		return -7;
	}
	INFO("client_wait_for_destroy_thread created.\n");

	/* Remote memory space handle */
	*rem_msh = (ms_h)add_rem_ms(rem_msname,
				    server_msid,
				    client_wait_for_destroy_thread,
				    destroy_mq);
	INFO("EXIT\n");
	return 0;
} /* rdma_conn_ms_h() */

int rdma_disc_ms_h(ms_h rem_msh, msub_h loc_msubh)
{
	send_disconnect_input	in;
	send_disconnect_output	*out;

	/* Check that library has been intialized */
	if (!init) {
		WARN("RDMA library not initialized\n");
		return -1;
	}

	/* Check that parameters are not NULL */
	if (!rem_msh) {
		WARN("rem_msh=0x%lX\n", rem_msh);
		return -2;
	}

	/* If the memory space was destroyed, it should not be in the database */
	if (!rem_ms_exists(rem_msh)) {
		WARN("rem_msh(0x%lX) not in database. Returning\n", rem_msh);
		/* Not an error if the memory space was destroyed */
		return 0;
	}

	rem_ms *ms = (rem_ms *)rem_msh;
	uint32_t msid = ms->msid;
	INFO("rem_msh exists and has msid(0x%X)\n", msid);

	/* Once we disconnect from the remote memory space we should no longer
	 * keep any record of its subspaces */

	/* Check that we indeed have remote msubs belonging to that rem_msh.
	 * If we don't that is a serious problem because:
	 * 1. Even if we don't provide an msub, the server must provide one.
	 * 2. If we did provide an msub, we cannot tell the server to remove it
	 *    unless we know the server's destid. That destid is in the locally
	 *    stored remote msub. */
	/* NOTE: assumption is that there is only ONE msub provided to the remote
	 * memory space per connection . */
	rem_msub *msubp = (rem_msub *)find_any_rem_msub_in_ms(msid);
	if (!msubp) {
		CRIT("No msubs for rem_msh(0x%lX). IMPOSSIBLE\n", rem_msh);
		return -3;
	}

	/* Get destid info BEFORE we delete the msub; we need that info to
	 * delete remote copies of the msubs in the server's database */
	in.rem_destid_len = msubp->destid_len;
	in.rem_destid 	  = msubp->destid;

	/* Remove all remote msubs belonging to 'rem_msh', if any */
	remove_rem_msubs_in_ms(msid);
	DBG("Removed msubs in msid(0x%X)\n", msid);

	/* Send the MSID of the remote memory space we wish to disconnect from;
	 * this will be used by the SERVER daemon to remove CLIENT destid from
	 * that memory space's object */
	in.rem_msid = msid;

	/* If we had provided a local msub to the server, we need to tell the server
	 * its msubid so it can delete it from tis remote databse */
	if (loc_msubh) {
		in.loc_msubid = ((loc_msub *)loc_msubh)->msubid;
	}

	/* If we are going to disconnect from the server, then we don't
	 * need the wait_for_destroy thread anymore, nor do we need the
	 * destroy_mq
	 */
	if (pthread_cancel(ms->wait_for_destroy_thread)) {
		WARN("Failed to cancel client_wait_for_destroy_thread\n");
	}

	if (ms->destroy_mq) {
		delete ms->destroy_mq;
	} else {
		WARN("destroy_mq was NULL..already destroyed?\n");
	}

	/* Remove remote entry for the memory space from the database */
	if (remove_rem_ms(rem_msh)) {
		ERR("Failed to remove remote ms(msid=0x%X) from database\n",
									msid);
		return -4;
	}

	/* Call send_disconnect_1() */
	CALL_RPC(send_disconnect, in, out, -5, -6);
	INFO("send_disconnect_1() called, now exiting\n");

	return 0;
} /* rdma_disc_ms_h() */

int rdma_push_msub(const struct rdma_xfer_ms_in *in,
		   struct rdma_xfer_ms_out *out)
{
	struct loc_msub *lmsub;
	struct rem_msub *rmsub;

	/* Check for NULL pointers */
	if (!in || !out) {
		ERR("%s: NULL. in=%p, out=%p", in, out);
		out->in_param_ok = -1;
		return -1;
	}
	lmsub = (struct loc_msub *)in->loc_msubh;
	rmsub = (struct rem_msub *)in->rem_msubh;
	if (!lmsub || !rmsub) {
		ERR("%s: NULL. lmsub=%p, rmsub=%p", lmsub, rmsub);
		out->in_param_ok = -1;
		return -1;
	}

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n", rmsub->destid_len);
		out->in_param_ok = -2;
		return -2;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		return -3;
	}

	/* Determine sync type */
	enum rio_transfer_sync	rd_sync;

	switch (in->sync_type) {

	case rdma_no_wait:
		rd_sync = RIO_TRANSFER_FAF;
		break;
	case rdma_sync_chk:
		rd_sync = RIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type\n", in->sync_type);
		out->in_param_ok = -4;
		return -4;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Sending %u bytes over DMA to destid=0x%X\n", in->num_bytes,
								rmsub->destid);
	INFO("Dest RIO addr = 0x%X\n, lmsub->paddr = 0x%lX\n",
					rmsub->rio_addr_lo + in->rem_offset,
					lmsub->paddr);

	int ret = riodp_dma_write_d(peer.mport_fd,
				    (uint16_t)rmsub->destid,
				    rmsub->rio_addr_lo + in->rem_offset,
				    lmsub->paddr,
				    in->loc_offset,
				    in->num_bytes,
				    RIO_EXCHANGE_NWRITE_R,
				    rd_sync);
	if (ret < 0)
		ERR("riodp_dma_write_d() failed:(%d) %s\n", ret, strerror(ret));

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value of riodp_dma_write_d() is the token (if >= 0) */
	if (in->sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (in->sync_type == rdma_async_chk && ret >= 0) {
		out->chk_handle = ret;	/* token */
		ret = 0;	/* success */
	}

	return ret;
} /* rdma_push_msub() */

int rdma_push_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	(void)priority;
	struct rem_msub *rmsub;

	/* Check for NULL pointer */
	if (!buf || !out || !rem_msubh) {
		ERR("NULL param(s). buf=%p, out=%p, rem_msubh = %u\n",
							buf, out, rem_msubh);
		out->in_param_ok = -1;
		return -1;
	}
	rmsub = (struct rem_msub *)rem_msubh;

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n", rmsub->destid_len);
		out->in_param_ok = -2;
		return -2;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		return -3;
	}

	/* Determine sync type */
	enum rio_transfer_sync	rd_sync;

	switch (sync_type) {

	case rdma_no_wait:
		rd_sync = RIO_TRANSFER_FAF;
		break;
	case rdma_sync_chk:
		rd_sync = RIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type: %d\n", sync_type);
		out->in_param_ok = -4;
		return -4;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Sending %u bytes over DMA to destid=0x%X, rio_addr = 0x%X\n",
				num_bytes,
				rmsub->destid,
				rmsub->rio_addr_lo + rem_offset);

	int ret = riodp_dma_write(peer.mport_fd,
				  (uint16_t)rmsub->destid,
				  rmsub->rio_addr_lo + rem_offset,
				  buf,
				  num_bytes,
				  RIO_EXCHANGE_NWRITE_R,
				  rd_sync);
	if (ret < 0)
		ERR("riodp_dma_write() failed:(%d) %s\n", ret, strerror(ret));

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value riodp_dma_write() is the token (if >= 0) */
	if (sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (sync_type == rdma_async_chk && ret >= 0 ) {
		out->chk_handle = ret;	/* token */
		ret = 0;	/* success */
	}

	return ret;
} /* rdma_push_buf() */

int rdma_pull_msub(const struct rdma_xfer_ms_in *in,
		   struct rdma_xfer_ms_out *out)
{
	struct loc_msub *lmsub;
	struct rem_msub *rmsub;

	/* Check for NULL pointers */
	if (!in || !out) {
		ERR("NULL. in=%p, out=%p\n", in, out);
		out->in_param_ok = -1;
		return -1;
	}
	lmsub = (struct loc_msub *)in->loc_msubh;
	rmsub = (struct rem_msub *)in->rem_msubh;
	if (!lmsub || !rmsub) {
		ERR("%s: NULL. lmsub=%p, rmsub=%p", lmsub, rmsub);
		out->in_param_ok = -1;
		return -1;
	}

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n", rmsub->destid_len);
		out->in_param_ok = -2;
		return -2;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		return -3;
	}

	/* Determine sync type */
	enum rio_transfer_sync	rd_sync;

	switch (in->sync_type) {

	case rdma_no_wait:	/* No FAF in reads, change to synchronous */
	case rdma_sync_chk:
		rd_sync = RIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type: %d\n", in->sync_type);
		out->in_param_ok = -4;
		return -4;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Receiving %u bytes over DMA from destid=0x%X\n",
						in->num_bytes, rmsub->destid);
	INFO("Source RIO addr = 0x%X, lmsub->paddr = 0x%lX\n",
				rmsub->rio_addr_lo + in->rem_offset,
				lmsub->paddr);

	int ret = riodp_dma_read_d(peer.mport_fd,
				   (uint16_t)rmsub->destid,
				   rmsub->rio_addr_lo + in->rem_offset,
				   lmsub->paddr,
				   in->loc_offset,
				   in->num_bytes,
				   rd_sync);
	if (ret < 0)
		ERR("riodp_dma_read_d() failed:(%d) %s\n", ret, strerror(ret));

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value of riodp_dma_read_d() is the token (if >= 0) */
	if (in->sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (in->sync_type == rdma_async_chk && ret >= 0) {
		out->chk_handle = ret; /* token */
		ret = 0; /* success */
	}

	return ret;
} /* rdma_pull_msub() */

int rdma_pull_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	(void)priority;
	struct rem_msub *rmsub;

	/* Check for NULL pointers */
	if (!buf || !out || !rem_msubh) {
		ERR("NULL param(s): buf=%p, out=%p, rem_msubh=%u\n",
							buf, out, rem_msubh);
		out->in_param_ok = -1;
		return -1;
	}
	rmsub = (struct rem_msub *)rem_msubh;
	if (!rmsub) {
		ERR("NULL param(s): rem_msubh=%u\n", rem_msubh);
		out->in_param_ok = -1;
		return -1;
	}

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n",rmsub->destid_len);
		out->in_param_ok = -2;
		return -2;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		return -3;
	}

	/* Determine sync type */
	enum rio_transfer_sync	rd_sync;

	switch (sync_type) {

	case rdma_no_wait:	/* No FAF in reads, change to synchronous */
	case rdma_sync_chk:
		rd_sync = RIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type: %d\n", sync_type);
		out->in_param_ok = -4;
		return -4;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Receiving %u DMA bytes from destid=0x%X, RIO addr = 0x%X\n",
					num_bytes,
					rmsub->destid,
					rmsub->rio_addr_lo + rem_offset);

	int ret = riodp_dma_read(peer.mport_fd,
				 (uint16_t)rmsub->destid,
				 rmsub->rio_addr_lo + rem_offset,
				 buf,
				 num_bytes,
				 rd_sync);
	if (ret < 0)
		ERR("riodp_dma_read() failed:(%d) %s\n", ret, strerror(ret));

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value of riodp_dma_read() is the token (if >= 0) */
	if (sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (sync_type == rdma_async_chk && ret >= 0) {
		out->chk_handle = ret; /* token */
		ret = 0; /* success */
	}

	return ret;
} /* rdma_pull_buf() */

struct dma_async_wait_param {
	uint32_t token;		/* DMA transaction ID token */
	uint32_t timeout;
	int err;
};

void *compl_thread_f(void *arg)
{
	dma_async_wait_param	*wait_param = (dma_async_wait_param *)arg;

	/* Wait for transfer to complete or times out (-ETIMEDOUT returned) */
	wait_param->err = riodp_wait_async(peer.mport_fd,
					   wait_param->token,
					   wait_param->timeout);

	/* Exit the thread; it is no longer needed */
	pthread_exit(0);
}

int rdma_sync_chk_push_pull(rdma_chk_handle chk_handle,
			    const struct timespec *wait)
{
	dma_async_wait_param	wait_param;

	/* Make sure handle is valid */
	if (!chk_handle) {
		ERR("Invalid chk_handle(%d)\n", chk_handle);
		return -1;
	}

	/* Check for NULL or 0 wait times */
	if (!wait || !(wait->tv_sec | wait->tv_nsec)) {
		wait_param.timeout = 0;
		WARN("Timeout not specified, using default value\n");
		/* as indicated in rio_mport_cdev.h (rio_async_tx_wait) */
	} else {
		/* Timeout is specified as a timespec. The mport library
		 * expects the the timeout in milliseconds so convert to ms */
		wait_param.timeout = wait->tv_sec * 1000 + wait->tv_nsec / 1000000;
	}
	wait_param.token = chk_handle;
	wait_param.err = -1;

	/* Now create a thread for checking the DMA transfer completion */
	pthread_t compl_thread;
	if (pthread_create(&compl_thread, NULL, compl_thread_f, (void *)&wait_param)) {
		ERR("pthread_create(): %s\n", strerror(errno));
		return -3;
	}

	/* Wait for transfer completion or timeout in thread */
	if (pthread_join(compl_thread, NULL)) {
		ERR("pthread_join(): %s\n", strerror(errno));
		return -4;
	}

	/* wait_param->err was populated with result (including timeout) */
	return wait_param.err;
} /* rdma_sync_chk_push_pull() */

