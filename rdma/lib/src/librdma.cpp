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
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <exception>
#include <vector>
#include <iterator>
#include <memory>

#include <rapidio_mport_mgmt.h>

#include "rdma_types.h"
#include "liblog.h"
#include "rdma_mq_msg.h"
#include "librdma_rx_engine.h"
#include "librdma_tx_engine.h"
#include "librdma_msg_processor.h"
#include "msg_q.h"

#include "librdma.h"

#include "rapidio_mport_dma.h"
#include "librdma_db.h"
#include "unix_sock.h"
#include "rdmad_unix_msg.h"
#include "libfmdd.h"

using std::iterator;
using std::exception;
using std::vector;
using std::make_shared;
using std::thread;

static unix_msg_processor	msg_proc;

static unix_rx_engine *rx_eng;
static unix_tx_engine *tx_eng;

static thread *engine_monitoring_thread;

#ifdef __cplusplus
extern "C" {
#endif

static unsigned init = 0;	/* Global flag indicating library initialized */

/* Unix socket client */
static shared_ptr<unix_client> client;
static fmdd_h 	    dd_h;
static uint32_t	    fm_alive;
static sem_t	    rdma_lock;

/** 
 * Global info related to mports and channelized messages.
 */
struct peer_info {
	int mport_id;
	riomp_mport_t mport_hnd;
	uint16_t destid;
}; /* peer_info */

static peer_info peer;

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

static int alt_rpc_call(unix_msg_t *in_msg, unix_msg_t **out_msg)
{
	auto ret = 0;

	size_t received_len;

	/* Send input parameters */
	ret = client->send(sizeof(*in_msg));
	if (ret == 0) {
		/* Receive output parameters */
		ret = client->receive(&received_len);
		if (ret) {
			ERR("Failed to receive output from RDMA daemon\n");
		} else {
			/* For API calls that don't require any return (output)
			 * parameters, they just pass NULL for out_msg.
			 */
			if (out_msg != NULL)
				client->get_recv_buffer((void **)out_msg);
		}
	} else {
		ERR("Failed to send message to RDMA daemon, ret = %d\n", ret);
	}

	if (ret) {
		CRIT("Daemon has died. Terminating socket connection\n");
		client.reset();
		CRIT("Daemon has died. Purging local database!\n");
		purge_local_database();
		init = 0;
		ret = RDMA_DAEMON_UNREACHABLE;
	}

	return ret;
} /* alt_rpc_call() */

int rdmad_kill_daemon()
{
	struct rdmad_kill_daemon_input	in;
	unix_msg_t  *in_msg;

	client->get_send_buffer((void **)&in_msg);
	in.dummy = 0x666;
	in_msg->type = RDMAD_KILL_DAEMON;
	in_msg->rdmad_kill_daemon_in = in;

	return alt_rpc_call(in_msg, NULL);
} /* rdmad_kill_daemon() */

/**
 * Call a function in the daemon.
 *
 * @param in_msg
 * @param out_msg
 *
 * @return 0 if successful
 */
static int daemon_call(unix_msg_t *in_msg, unix_msg_t *out_msg)
{
	constexpr uint32_t MSG_SEQ_NO_START = 0x0000000A;
	static rdma_msg_seq_no seq_no = MSG_SEQ_NO_START;

	/* First check that engines are still valid */
	if ((tx_eng == nullptr) || (rx_eng == nullptr)) {
		CRIT("Connection to daemon severed");
		return RDMA_DAEMON_UNREACHABLE;
	}

	/* Send message */
	in_msg->seq_no = seq_no;
	tx_eng->send_message(in_msg);
	DBG("Message queued\n");

	/* Prepare for reply */
	auto reply_sem = make_shared<sem_t>();
	sem_init(reply_sem.get(), 0, 0);
	auto rc = 0;
	rc = rx_eng->set_notify(in_msg->type | 0x8000,
			       RDMA_LIB_DAEMON_CALL, seq_no, reply_sem);

	/* Wait for reply */
	DBG("Notify configured...WAITING...\n");
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec++;
	rc = sem_timedwait(reply_sem.get(), &timeout);
	if (rc) {
		ERR("reply_sem failed: %s\n", strerror(errno));
		if (errno == ETIMEDOUT) {
			ERR("Returning RDMA_DAEMON_UNREACHABLE\n");
			rc = RDMA_DAEMON_UNREACHABLE;
		}
	} else {
		/* Retrieve the reply */
		DBG("Got reply!\n");
		rc = rx_eng->get_message(in_msg->type | 0x8000, RDMA_LIB_DAEMON_CALL,
							seq_no, out_msg);
		if (rc) {
			ERR("Failed to obtain reply message, rc = %d\n", rc);
		}
	}

	/* Now increment seq_no for next call */
	seq_no++;

	return rc;
} /* daemon_call() */

/**
 * For testing only. Not exposed in librdma.h.
 */
int rdma_get_ibwin_properties(unsigned *num_ibwins,
			      uint32_t *ibwin_size)
{
	auto rc = 0;

	try {
		unix_msg_t	in_msg;

		in_msg.type = GET_IBWIN_PROPERTIES;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.get_ibwin_properties_in.dummy = 0xc0de;

		unix_msg_t  	out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc ) {
			ERR("Failed in daemon call to get ibwin props\n");
			throw rc;
		}

		if (out_msg.get_ibwin_properties_out.status) {
			ERR("Failed to get ibwin props from daemon\n");
			throw out_msg.get_ibwin_properties_out.status;
		}

		DBG("num_ibwins = %u, ibwin_size = %uKB\n",
			out_msg.get_ibwin_properties_out.num_ibwins,
			out_msg.get_ibwin_properties_out.ibwin_size/1024);

		*num_ibwins = out_msg.get_ibwin_properties_out.num_ibwins;
		*ibwin_size = out_msg.get_ibwin_properties_out.ibwin_size;
	}
	catch(int e) {
		rc = e;
	}

	return rc;
} /* rdma_get_inbwin_properties() */

/**
 * For testing only. Not exposed in librdma.h.
 */
int rdma_get_msh_properties(ms_h msh, uint64_t *rio_addr, uint32_t *bytes)
{
	auto rc = 0;

	if (!msh || !rio_addr || !bytes) {
		ERR("NULL parameter.\n");
		rc = -1;
	} else {
		loc_ms *msp = (loc_ms *)msh;

		*rio_addr = msp->rio_addr;
		*bytes	  = msp->bytes;
	}
	return rc;
} /* rdma_get_msh_properties() */

/**
 * Internal function to determine whether the daemon is still reachable.
 * I think this is redundant now; in daemon_call() we consider the daemon
 * to be unreachable if we timeout on the reply semaphore. This is precisely
 * what this function will do as well.
 * FIXME: REMOVE.
 */
static bool rdmad_is_alive()
{
	rdmad_is_alive_input	in;
	unix_msg_t  *in_msg;

	client->get_send_buffer((void **)&in_msg);

	in.dummy = 0x1234;

	in_msg->type = RDMAD_IS_ALIVE;
	in_msg->rdmad_is_alive_in = in;

	return alt_rpc_call(in_msg, NULL) != RDMA_DAEMON_UNREACHABLE;
} /* rdmad_is_alive() */

static int open_mport(void)
{
	auto rc = 0;
	DBG("ENTER\n");

	/* Set up Unix message parameters */
	unix_msg_t	in_msg;
	unix_msg_t	out_msg;

	in_msg.type = GET_MPORT_ID;
	in_msg.category = RDMA_REQ_RESP;
	in_msg.get_mport_id_in.dummy = 0x1234;

	rc = daemon_call(&in_msg, &out_msg);
	if (rc ) {
		ERR("Failed to obtain mport ID\n");
		return rc;
	}

	/* Get the mport ID */
	peer.mport_id = out_msg.get_mport_id_out.mport_id;
	INFO("Using mport_id = %d\n", peer.mport_id);

	/* Now open the port */
	auto flags = 0;
	rc = riomp_mgmt_mport_create_handle(peer.mport_id, flags,
							&peer.mport_hnd);
	if (rc) {
		CRIT("riomp_mgmt_mport_create_handle(): %s\n", strerror(errno));
		CRIT("Cannot open mport%d, is rio_mport_cdev loaded?\n",
								peer.mport_id);
		return RDMA_MPORT_OPEN_FAIL;
	}

	/* Read the properties. */
	struct riomp_mgmt_mport_properties prop;
	if (!riomp_mgmt_query(peer.mport_hnd, &prop)) {
		riomp_mgmt_display_info(&prop);
		if (prop.flags &RIO_MPORT_DMA) {
			INFO("DMA is ENABLED\n");
		} else {
			CRIT("DMA capability DISABLED\n");
			riomp_mgmt_mport_destroy_handle(&peer.mport_hnd);
			return RDMA_NOT_SUPPORTED;
		}
		peer.destid = prop.hdid;
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
	INFO("Waiting for destroy ms POSIX message on %s\n",
						destroy_mq->get_name().c_str());
	if (destroy_mq->receive()) {
		CRIT("Failed to receive 'destroy' POSIX message\n");
		pthread_exit(0);
	}
	INFO("Got 'destroy ms' POSIX message\n");

	/* Remove the msubs belonging to that ms */
	DBG("Removing msubs in server_msid(0x%X)\n", dm->server_msid);
	remove_rem_msubs_in_ms(dm->server_msid);

	/* Remove the ms itself */
	DBG("Removing server_msid(0x%X) from database\n", dm->server_msid);
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
	DBG("Sending back DESTROY ACK 4 server_msid(0x%X)\n", dm->server_msid);
	if (destroy_mq->send()) {
		CRIT("Failed to send destroy ack on %s\n",
					destroy_mq->get_name().c_str());
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
	if (!arg) {
		CRIT("Null arg passed to thread function. Exiting\n");
		pthread_exit(0);
	}

	msg_q<mq_rdma_msg> *mq = (msg_q<mq_rdma_msg> *)arg;

	/* Wait for the POSIX disconnect message containing rem_msh */
	INFO("Waiting for DISconnect message...\n");
	if (mq->receive()) {
		CRIT("mq->receive() failed: %s", strerror(errno));
		delete mq;
		pthread_exit(0);
	}

	/* Extract message contents */
	mq_rdma_msg *rdma_msg;
	mq->get_recv_buffer(&rdma_msg);

	if (rdma_msg->type != MQ_DISCONNECT_MS) {
		CRIT("** Invalid message type: 0x%X **\n", rdma_msg->type);
		raise(SIGABRT);
		delete mq;
		pthread_exit(0);
	}
	mq_disconnect_msg *disc_msg = &rdma_msg->disconnect_msg;
	INFO("Received mq_disconnect on '%s' with client_msubid(0x%X)\n",
			mq->get_name().c_str(),	disc_msg->client_msubid);

	/* Find the msub in the database, and remove it */
	msub_h client_msubh = find_rem_msub(disc_msg->client_msubid);
	if (!client_msubh) {
		ERR("client_msubid(0x%X) not found!\n",
						disc_msg->client_msubid);
	} else {
		/* Remove client subspace from remote msub database */
		remove_rem_msub(client_msubh);
		INFO("client_msubid(0x%X) removed from database\n",
						disc_msg->client_msubid);
	}

	/* Obtain memory space name from queue name and locate in database */
	string ms_name = mq->get_name();
	ms_name.erase(0, 1);
	ms_h loc_msh = find_loc_ms_by_name(ms_name.c_str());
	if (!loc_msh) {
		CRIT("Failed to find ms(%s) in database\n", ms_name.c_str());
	} else {
		/* Mark memory space as NOT accepted */
		((loc_ms *)loc_msh)->accepted = false;

		/* Delete message queue and mark as nullptr */
		delete mq;
		((loc_ms *)loc_msh)->disc_notify_mq = nullptr;

		/* Mark thread as dead */
		((loc_ms *)loc_msh)->disc_thread = 0;
	}

	INFO("Exiting\n");
	pthread_exit(0);
} /* wait_for_disc_thread_f() */

/**
 * FIXME: Rename to something more general.
 */
void engine_monitoring_thread_f(sem_t *engine_cleanup_sem)
{
	while(1) {
		/* Wait until there is a reason to perform cleanup */
		HIGH("Waiting for engine_cleanup_sem\n");
		sem_wait(engine_cleanup_sem);

		HIGH("Cleaning up dead engines!\n");
		if (tx_eng->isdead()) {
			HIGH("Killing tx_eng\n");
			delete tx_eng;
			tx_eng = nullptr;
		}

		if (rx_eng->isdead()) {
			HIGH("Killing rx_eng\n");
			delete rx_eng;
			rx_eng = nullptr;
		}

		/* Purge database and set state to uninitialized */
		purge_local_database();
		init = 0;
	}
} /* engine_monitoring_thread_f() */

/**
 * Initialize RDMA library
 *
 * rdma_lib_init() is automatically called once, when the library is loaded
 * in response to a call to one the RDMA APIs. rdma_lib_init() may be
 * called again whenever an API fails to retry or get a reason code for failure.
 *
 * @return: 0 if successful
 */
static int rdma_lib_init(void)
{
	auto ret = 0;

	if (init == 1) {
		WARN("RDMA library already initialized\n");
		return ret;
	}

	sem_init(&rdma_lock, 0, 1);

	/* Create a client */
	DBG("Creating client object...\n");
	try {
		client = make_shared<unix_client>();

		/* Connect to server */
		if (client->connect() == 0) {
			INFO("Successfully connected to RDMA daemon\n");

			/* Engine cleanup semaphore. Posted by engines that die
			 * so we can clean up after them. */
			auto engine_cleanup_sem = new sem_t();
			sem_init(engine_cleanup_sem, 0, 0);

			/* Create Tx and Rx engines */
			tx_eng = new unix_tx_engine(client, engine_cleanup_sem);
			rx_eng = new unix_rx_engine(client, msg_proc, tx_eng,
							engine_cleanup_sem);

			/* Start engine monitoring thread */
			engine_monitoring_thread =
					new thread(engine_monitoring_thread_f,
						   engine_cleanup_sem);
			/* Open the master port */
			ret = open_mport();
			if (ret == 0) {
				/* Success */
				INFO("MPORT successfully opened\n");
				init = 1;
				INFO("RDMA library fully initialized\n ");
			} else {
				CRIT("Failed to open mport\n");
				throw RDMA_MPORT_OPEN_FAIL;
			}
		} else {
			CRIT("Connect failed. Daemon running?\n");
			throw RDMA_DAEMON_UNREACHABLE;
		}
	} /* try */
	catch(unix_sock_exception& e) {
		CRIT("%s\n", e.what());
		client = nullptr;
		ret = RDMA_MALLOC_FAIL;
	}
	catch(exception& e) {
		ERR("Failed to create Tx/Rx engines: %s\n", e.what());
		client.reset();
		ret = RDMA_MALLOC_FAIL;
	}
	catch(int e) {
		switch (e) {
		case RDMA_MPORT_OPEN_FAIL:
		case RDMA_DAEMON_UNREACHABLE:
			ret = e;
			break;
		default:
			ret = -1;
		}
		client.reset();
	}

	DBG("ret = %d\n", ret);
	return ret;
} /* rdma_lib_init() */

__attribute__((constructor)) int lib_init(void)
{
	auto rc = 0;

	/* Initialize the logger */
	rdma_log_init("librdma.log", 0);

	/* Initialize message queue attributes */
	init_mq_attribs();

	/* Make threads cancellable at some points (e.g. mq_receive) */
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)) {
		WARN("Failed to set cancel state:%s. Exiting.\n",strerror(errno));
		exit(RDMA_ERRNO);
	}

	/* Iinitialize the database */
	if (rdma_db_init()) {
		CRIT("Failed to initialized RDMA database. Exiting.\n");
		exit(RDMA_ERRNO);
	}
	HIGH("Database initialized\n");

	/* Library initialization */
	if (rdma_lib_init()) {
		CRIT("Failed to connect to daemon. Exiting.\n");
		exit(rc);
	}
	return rc;
} /* rdma_lib_init() */

/**
 * If the daemon is restarted, then the connection we have will be bad,
 * and the tx/rx engines will be dead. We therefore need to re-run the
 * initialization code again. 'init' is set to false when the engines
 * are killed to indicate the need for re-init.
 */
#define CHECK_LIB_INIT() if (!init) { \
				WARN("RDMA library not initialized, re-initializing\n"); \
				ret = rdma_lib_init(); \
				if (ret) { \
					ERR("Failed to re-initialize RDMA library\n"); \
					return ret; \
				} \
			}

#define LIB_INIT_CHECK(rc) if (!init) { \
		WARN("RDMA library not initialized, re-initializing\n"); \
		rc = rdma_lib_init(); \
		if (rc != 0) { \
			ERR("Failed to re-initialize RDMA library\n"); \
			throw RDMA_LIB_INIT_FAIL; \
		} \
	}

void rdma_set_fmd_handle(fmdd_h dd_h)
{
	::dd_h = dd_h;
}

int rdma_create_mso_h(const char *owner_name, mso_h *msoh)
{
	auto rc = 0;

	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!owner_name || !msoh) {
			ERR("NULL param: owner_name=%p, msoh=%p\n",
							owner_name, msoh);
			throw RDMA_NULL_PARAM;
		}

		/* Check that owner does not already exist */
		if (find_mso_by_name(owner_name)) {
			ERR("Cannot create another owner named '%s'\n", owner_name);
			throw RDMA_DUPLICATE_MSO;
		}

		/* Prevent buffer overflow due to very long name */
		size_t len = strlen(owner_name);
		if (len > UNIX_MS_NAME_MAX_LEN) {
			ERR("String 'owner_name' is too long (%d)\n", len);
			throw RDMA_NAME_TOO_LONG;
		}

		/* Set up Unix message parameters */
		unix_msg_t	in_msg;
		unix_msg_t	out_msg;
		in_msg.type     = CREATE_MSO;
		in_msg.category = RDMA_REQ_RESP;
		strcpy(in_msg.create_mso_in.owner_name, owner_name);

		/* Call into daemon */
		rc = daemon_call(&in_msg, &out_msg);
		if (rc ) {
			ERR("Failed in CREATE_MSO daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to create mso? */
		if (out_msg.create_mso_out.status) {
			ERR("Failed to create mso '%s' in daemon\n", owner_name);
			throw out_msg.create_mso_out.status;
		}

		/* Store in database. mso_conn_id = 0 and owned = true */
		*msoh = add_loc_mso(owner_name, out_msg.create_mso_out.msoid, 0, true);
		if (!*msoh) {
			WARN("add_loc_mso() failed, msoid = 0x%X\n",
						out_msg.create_mso_out.msoid);
			throw RDMA_DB_ADD_FAIL;
		}
	} /* try */
	catch(int e) {
		rc = e;
	}

	sem_post(&rdma_lock);
	return rc;
} /* rdma_create_mso_h() */

int rdma_open_mso_h(const char *owner_name, mso_h *msoh)
{
	auto rc = 0;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!owner_name || !msoh) {
			ERR("NULL param: owner_name=%p, msoh=%p\n",
							owner_name, msoh);
			throw RDMA_NULL_PARAM;
		}

		/* Check if the mso was created by same app, or already open */
		*msoh = find_mso_by_name(owner_name);
		if (*msoh) {
			loc_mso *mso = (loc_mso *)(*msoh);
			/* Don't allow opening from the same app that created the mso */
			if (mso->owned) {
				ERR("Cannot open mso '%s' from creator app\n",
								owner_name);
				throw RDMA_CANNOT_OPEN_MSO;
			} else {
				/* Already open, just return the handle */
				WARN("%s is already open!\n", owner_name);
				throw RDMA_ALREADY_OPEN;
			}
		}

		/* Set up input parameters */
		unix_msg_t  in_msg;
		in_msg.type = OPEN_MSO;
		in_msg.category = RDMA_REQ_RESP;
		strcpy(in_msg.open_mso_in.owner_name, owner_name);

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc) {
			ERR("Failed in OPEN_MSO daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to open mso? */
		if (out_msg.open_mso_out.status) {
			ERR("Failed to open mso '%s', rc = %d\n", owner_name, rc);
			throw out_msg.open_mso_out.status;
		}

		/* Store in database */
		*msoh = add_loc_mso(owner_name,
			    out_msg.open_mso_out.msoid,
			    out_msg.open_mso_out.mso_conn_id,
			    /* owned is */false);
		if (!*msoh) {
			WARN("add_loc_mso() failed, msoid = 0x%X\n", out_msg.open_mso_out.msoid);
			throw RDMA_DB_ADD_FAIL;
		}
	} /* try */

	catch(int e) {
		rc = e;
	}
	DBG("EXIT\n");
	sem_post(&rdma_lock);
	return rc;
} /* rdma_open_mso_h() */

int rdma_close_mso_h(mso_h msoh)
{
	auto rc = 0;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL msoh */
		if (!msoh) {
			WARN("msoh is NULL\n");
			throw RDMA_NULL_PARAM;
		}

		/* Check if msoh has already been removed (as a result of the
		 *  owner destroying, the owner dying, the daemon dying..etc.) */
		if (!mso_h_exists(msoh)) {
			WARN("msoh no longer exists\n");
			throw 0;
		}

		/* Get list of memory spaces opened by this owner */
		list<loc_ms *>	ms_list(get_num_ms_by_msoh(msoh));
		get_list_msh_by_msoh(msoh, ms_list);

		DBG("ms_list now has %d elements\n", ms_list.size());

		/* For each one of the memory spaces in this mso, close */
		bool	ok =  true;
		for_each(begin(ms_list),
			 begin(ms_list),
			[&](loc_ms *ms)
			{
				if (rdma_close_ms_h(msoh, ms_h(ms))) {
					WARN("rdma_close_ms_h failed: msoh = 0x%"
						PRIx64 ", msh = 0x%" PRIx64 "\n",
								msoh, ms_h(ms));
					ok = false;	/* Modify on error only */
				} else {
					INFO("msh(0x%" PRIx64 ") owned by msoh(0x%"
						PRIx64 ") closed\n", ms_h(ms), msoh);
				}
			});

		/* Fail on error. We can't close an mso if there are ms's open */
		if (!ok) {
			ERR("Failed to close one or more mem spaces\n");
			throw RDMA_MS_CLOSE_FAIL;
		}

		/* Set up Unix message parameters */
		unix_msg_t  in_msg;

		in_msg.close_mso_in.msoid = ((loc_mso *)msoh)->msoid;
		in_msg.close_mso_in.mso_conn_id = ((loc_mso *)msoh)->mso_conn_id;
		in_msg.type 	= CLOSE_MSO;
		in_msg.category = RDMA_REQ_RESP;

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc) {
			ERR("Failed in CLOSE_MSO daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Check status returned by command on the daemon side */
		if (out_msg.close_mso_out.status != 0) {
			ERR("Failed to close mso(0x%X) in daemon\n",
						in_msg.close_mso_in.msoid);
			throw out_msg.close_mso_out.status;
		}

		/* Take it out of database */
		if (remove_loc_mso(msoh) < 0) {
			WARN("Failed to find 0x%" PRIx64 " in db\n", msoh);
			throw RDMA_DB_REM_FAIL;
		}

		INFO("msoh(0x%" PRIx64 ") removed from local database\n", msoh);
	} /* try */
	catch(int e) {
		rc = e;
	} /* catch */

	DBG("EXIT\n");
	sem_post(&rdma_lock);

	return rc;
} /* rdma_close_mso_h() */

int rdma_destroy_mso_h(mso_h msoh)
{
	auto rc = 0;

	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL msoh */
		if (!msoh) {
			WARN("msoh is NULL\n");
			throw RDMA_NULL_PARAM;
		}

		/* Get list of memory spaces owned by this owner */
		list<loc_ms *>	ms_list(get_num_ms_by_msoh(msoh));
		get_list_msh_by_msoh(msoh, ms_list);

		INFO("ms_list now has %d elements\n", ms_list.size());

		/* For each one of the memory spaces, call destroy */
		bool	ok = true;
		for_each(begin(ms_list), end(ms_list),
			[&](loc_ms *ms)
			{
				if (rdma_destroy_ms_h(msoh, ms_h(ms))) {
					WARN("rdma_destroy_ms_h failed: msoh = 0x%"
						PRIx64 ", msh = 0x%" PRIx64 "\n",
						msoh, ms_h(ms));
					ok = false;
				} else {
					DBG("msh(0x%" PRIx64 ") owned by msoh(0x%"
							PRIx64 ") destroyed\n",
							ms_h(ms), msoh);
				}
			}
		);

		/* Fail on any error */
		if (!ok) {
			ERR("Failed in destroy\n");
			throw RDMA_MS_DESTROY_FAIL;
		}

		/* Set up input parameters */
		unix_msg_t	in_msg;
		in_msg.destroy_mso_in.msoid = ((loc_mso *)msoh)->msoid;
		in_msg.type = DESTROY_MSO;
		in_msg.category = RDMA_REQ_RESP;

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc) {
			ERR("Failed in DESTROY_MSO daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to destroy mso? */
		if (out_msg.destroy_mso_out.status) {
			ERR("Failed to destroy msoid(0x%X), rc = %d\n",
						((loc_mso *)msoh)->msoid, rc);
			throw out_msg.destroy_mso_out.status;
		}

		/* Remove from database */
		if (remove_loc_mso(msoh)) {
			CRIT("Failed to remove mso from database\n");
			throw RDMA_DB_REM_FAIL;
		}
	} /* try */
	catch(int e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	DBG("EXIT\n");
	return rc;
} /* rdma_destroy_mso_h() */

int rdma_create_ms_h(const char *ms_name,
		  mso_h msoh,
		  uint32_t req_bytes,
		  uint32_t flags,
		  ms_h *msh,
		  uint32_t *bytes)
{
	auto rc = 0;

	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!ms_name || !msoh || !msh) {
			ERR("NULL param: ms_name=%p, msoh=%u, msh=%p",
					ms_name, msoh, msh);
			throw RDMA_NULL_PARAM;
		}

		/* Prevent buffer overflow due to very long name */
		size_t len = strlen(ms_name);
		if (len > UNIX_MS_NAME_MAX_LEN) {
			ERR("String 'ms_name' is too long (%d)\n", len);
			throw RDMA_NAME_TOO_LONG;
		}

		/* Disallow creating a duplicate ms name */
		if (find_loc_ms_by_name(ms_name)) {
			WARN("A memory space named '%s' exists\n", ms_name);
			throw RDMA_DUPLICATE_MS;
		}

		/* A memory space must be aligned at 4K, therefore any previous
		 * memory space must be rounded up in size to the nearest 4K.
		 * But if caller knows their 'req_bytes' are aligned, allow them
		 * to pass NULL for 'bytes' */
		uint32_t dummy_bytes;
		if (bytes == NULL)
			bytes = &dummy_bytes;
		*bytes = round_up_to_4k(req_bytes);

		/* Set up Unix message parameters */
		unix_msg_t	in_msg;
		unix_msg_t	out_msg;
		in_msg.type     = CREATE_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.create_ms_in.bytes = *bytes;
		in_msg.create_ms_in.flags = flags;
		in_msg.create_ms_in.msoid = ((loc_mso *)msoh)->msoid;
		strcpy(in_msg.create_ms_in.ms_name, ms_name);

		/* Call into daemon */
		rc = daemon_call(&in_msg, &out_msg);
		if (rc ) {
			ERR("Failed in CREATE_MS daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to create mso? */
		if (out_msg.create_ms_out.status) {
			ERR("Failed to create ms '%s' in daemon\n", ms_name);
			throw out_msg.create_ms_out.status;
		}

		/* Store in local database */
		*msh = add_loc_ms(ms_name,
				*bytes,
				msoh,
				out_msg.create_ms_out.msid,
				out_msg.create_ms_out.phys_addr,
				out_msg.create_ms_out.rio_addr,
				0, true,
				0, nullptr);
		if (!*msh) {
			ERR("Failed to store ms in database\n");
			throw RDMA_DB_ADD_FAIL;
		}
		INFO("Stored info about '%s' in database as msh(0x%" PRIx64 ")\n",
								ms_name, *msh);
	} /* try */
	catch(int e) {
		rc = e;
	}

	sem_post(&rdma_lock);

	return rc;
} /* rdma_create_ms_h() */

int destroy_msubs_in_msh(ms_h msh)
{
	uint32_t 	msid = ((struct loc_ms *)msh)->msid;

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
	bool	ok = true;
	for_each(msub_list.begin(), msub_list.end(),
		[&](loc_msub * msub)
		{
			if (rdma_destroy_msub_h(msh, msub_h(msub))) {
				WARN("rdma_destroy_msub_h failed: msh=0x%"
					PRIx64 ", msubh=0x%" PRIx64 "\n",
							msh, msub_h(msub));
				ok = false;
			}
		});

	/* Fail on any error */
	if (!ok) {
		ERR("Failure during destroying one of the msubs\n");
		return RDMA_MSUB_DESTROY_FAIL;
	}

	return 0;
} /* destroy_msubs_in_msh */

int rdma_open_ms_h(const char *ms_name, mso_h msoh, uint32_t flags,
						uint32_t *bytes, ms_h *msh)
{
	auto rc = 0;

	DBG("ENTER, ms_name = %s\n", ms_name);
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!ms_name || !msoh || !msh) {
			ERR("NULL param: ms_name=%p, msoh=%u, msh=%p",
					ms_name, msoh, msh);
			throw RDMA_NULL_PARAM;
		}

		/* Prevent buffer overflow due to very long name */
		size_t len = strlen(ms_name);
		if (len > UNIX_MS_NAME_MAX_LEN) {
			ERR("String 'ms_name' is too long (%d)\n", len);

			throw RDMA_NAME_TOO_LONG;
		}

		/* Set up Unix message parameters */
		unix_msg_t	in_msg;
		in_msg.type     = OPEN_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.open_ms_in.flags = flags;
		in_msg.open_ms_in.msoid = ((loc_mso *)msoh)->msoid;
		strcpy(in_msg.open_ms_in.ms_name, ms_name);

		/* Call into daemon */
		unix_msg_t	out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc ) {
			ERR("Failed in OPEN_MS daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to open ms? */
		if (out_msg.open_ms_out.status) {
			ERR("Failed to open ms '%s' in daemon\n", ms_name);
			throw out_msg.open_ms_out.status;
		}

		INFO("Opened '%s' in the daemon\n", ms_name);

		/* Store memory space info in database */
		*msh = add_loc_ms(ms_name,
				out_msg.open_ms_out.bytes,
				msoh,
				out_msg.open_ms_out.msid,
				out_msg.open_ms_out.phys_addr,
				out_msg.open_ms_out.rio_addr,
				out_msg.open_ms_out.ms_conn_id,
				false,
				0,
				nullptr);
		if (!*msh) {
			CRIT("Failed to store ms in database\n");
			throw RDMA_DB_ADD_FAIL;
		}
		INFO("Stored info about '%s' in database as msh(0x%" PRIx64 ")\n",
								ms_name, *msh);
		*bytes = out_msg.open_ms_out.bytes;
	}
	catch(int e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	return rc;
} /* rdma_open_ms_h() */

int rdma_close_ms_h(mso_h msoh, ms_h msh)
{
	auto rc = 0;
	DBG("ENTER with msoh=0x%" PRIx64 ", msh = 0x%" PRIx64 "\n", msoh, msh);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!msoh || !msh) {
			ERR("Invalid param(s). Failing.\n");
			throw RDMA_NULL_PARAM;
		}

		/* Check if msh has already been closed (as a result of the owner
		 * destroying it and sending a close message to this user) */
		if (!loc_ms_exists(msh)) {
			/* This is NOT an error; the memory space was destroyed
			 * by its owner. Consider it closed by just warning and
			 * returning a success code */
			WARN("msh no longer exists\n");
			throw 0;
		}

		/* Destroy msubs opened under this msh */
		if (destroy_msubs_in_msh(msh)) {
			ERR("Failed to destroy msubs belonging to msh(0x%" PRIx64 ")\n", msh);
			throw RDMA_MSUB_DESTROY_FAIL;
		}

		/* Kill the disconnection thread, if it exists */
		pthread_t  disc_thread = loc_ms_get_disc_thread(msh);
		if (!disc_thread) {
			WARN("disc_thread is NULL.\n");
		} else {
			HIGH("Killing the wait-for-disconnection thread!!\n");
			if (pthread_cancel(disc_thread)) {
				WARN("Failed to cancel disc_thread for msh(0x%X):%s\n",
							msh, strerror(errno));
			}
		}

		/* Kill the disconnection message queue */
		msg_q<mq_rdma_msg> *disc_mq = loc_ms_get_disc_notify_mq(msh);
		if (disc_mq == nullptr) {
			WARN("disc_mq is NULL\n");
		} else {
			delete disc_mq;
		}

		/* Set up Unix message parameters */
		unix_msg_t	in_msg;
		in_msg.type     = CLOSE_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.close_ms_in.msid = ((loc_ms *)msh)->msid;
		in_msg.close_ms_in.ms_conn_id = ((loc_ms *)msh)->ms_conn_id;

		/* Call into daemon */
		unix_msg_t	out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc ) {
			ERR("Failed in CLOSE_MS daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to open ms? */
		if (out_msg.close_ms_out.status) {
			ERR("Failed to close ms '%s' in daemon\n",
						((loc_ms *)msh)->name);
			throw out_msg.close_ms_out.status;
		}

		INFO("Opened '%s' in the daemon\n", ((loc_ms *)msh)->name);

		/* Take it out of databse */
		if (remove_loc_ms(msh) < 0) {
			ERR("Failed to find msh(0x%" PRIx64 ") in db\n", msh);
			throw RDMA_DB_REM_FAIL;
		}
		INFO("msh(0x%" PRIx64 ") removed from local database\n", msh);
	}
	catch(int e) {
		rc = e;
	}
	DBG("EXIT - success\n");
	return rc;
} /* rdma_close_ms_h() */

int rdma_destroy_ms_h(mso_h msoh, ms_h msh)
{
	auto rc = 0;

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!msoh || !msh) {
			ERR("Invalid param(s): msoh=0x%" PRIx64 ", msh=0x%" PRIx64
								"\n", msoh, msh);
			return RDMA_NULL_PARAM;
		}

		loc_ms *ms = (loc_ms *)msh;
		DBG("msid = 0x%X, ms_name = '%s', msh = 0x%" PRIx64 "\n",
							ms->msid, ms->name, msh);
		/* Destroy msubs in this msh */
		if (destroy_msubs_in_msh(msh)) {
			ERR("Failed to destroy msubs belonging to msh(0x%" PRIx64 ")\n",
										msh);
			throw RDMA_MSUB_DESTROY_FAIL;
		}

		/* Set up input parameters */
		unix_msg_t  in_msg;
		in_msg.type     = DESTROY_MS;
		in_msg.category = RDMA_REQ_RESP;
		in_msg.destroy_ms_in.msid = ms->msid;
		in_msg.destroy_ms_in.msoid= ((loc_mso *)msoh)->msoid;

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(&in_msg, &out_msg);
		if (rc ) {
			ERR("Failed in DESTROY_MS daemon_call, rc = %d\n", rc);
			throw rc;
		}

		/* Failed to destroy ms? */
		if (out_msg.destroy_ms_out.status) {
			ERR("Failed to destroy ms '%s' in daemon\n", ms->name);
			throw out_msg.destroy_ms_out.status;
		}

		/* Kill the disconnection thread, if it exists */
		pthread_t  disc_thread = loc_ms_get_disc_thread(msh);
		if (!disc_thread) {
			WARN("disc_thread is NULL.\n");
		} else {
			if (pthread_cancel(disc_thread)) {
				WARN("Failed to cancel disc_thread for msh(0x%" PRIx64 "):%s\n",
						msh, strerror(errno));
			}
		}

		/**
		 * Daemon should have closed the message queue so we can close and
		 * unlink it here. */
		msg_q<mq_rdma_msg> *disc_mq = loc_ms_get_disc_notify_mq(msh);
		if (disc_mq == nullptr) {
			WARN("disc_mq is NULL\n");
		} else {
			delete disc_mq;
		}

		/* Memory space removed in daemon, remove from database as well */
		if (remove_loc_ms(msh) < 0) {
			WARN("Failed to remove 0x%" PRIx64 " from database\n", msh);
			dump_loc_ms();
			throw RDMA_DB_REM_FAIL;
		}

		INFO("msh(0x%" PRIx64 ") removed from local database\n", msh);
	}
	catch(int e) {
		rc = e;
	}
	return rc;
} /* rdma_destroy_ms_h() */

int rdma_create_msub_h(ms_h	msh,
		       uint32_t	offset,
		       uint32_t	req_bytes,
		       uint32_t	flags,
		       msub_h	*msubh)
{
	(void)flags;
	create_msub_input	in;
	create_msub_output	out;
	int			ret;
	
	sem_wait(&rdma_lock);

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	/* Check for NULL */
	if (!msh || !msubh) {
		ERR("NULL param: msh=0x%" PRIx64 ", msubh=%p\n", msh, msubh);
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* In order for mmap() to work, the phys addr must be aligned
	 * at 4K. This means the mspace must be aligned, the msub
	 * must have a size that is a multiple of 4K, and the offset
	 * of the msub within the mspace must be a multiple of 4K.
	 */
	if (!aligned_at_4k(offset) || !aligned_at_4k(req_bytes)) {
		ERR("Offset & size must be multiples of 4K\n");
		sem_post(&rdma_lock);
		return RDMA_ALIGN_ERROR;
	}

	/* Set up input parameters */
	unix_msg_t  *in_msg;
	unix_msg_t  *out_msg;
	client->get_send_buffer((void **)&in_msg);
	in.msid		= ((struct loc_ms *)msh)->msid;
	in.offset	= offset;
	in.req_bytes	= req_bytes;

	DBG("msid = 0x%X, offset = 0x%X, req_bytes = 0x%x\n",
		in.msid, in.offset, in.req_bytes);

	/* Set up Unix message parameters */
	in_msg->type = CREATE_MSUB;
	in_msg->create_msub_in = in;

	ret = alt_rpc_call(in_msg, &out_msg);
	if (ret) {
		ERR("Call to RDMA daemon failed\n");
		sem_post(&rdma_lock);
		return ret;
	}

	out = out_msg->create_msub_out;

	/* Check status returned by command on the daemon side */
	if (out.status != 0) {
		ERR("Failed to create msub in daemon\n");
		sem_post(&rdma_lock);
		return out.status;
	}

	INFO("out->bytes=0x%X, out.phys_addr=0x%016" PRIx64 ", out.rio_addr=0x%016" PRIx64 "\n",
				out.bytes, out.phys_addr, out.rio_addr);

	/* Store msubh in database, obtain pointer thereto, convert to msub_h */
	*msubh = (msub_h)add_loc_msub(out.msubid,
				      in.msid,
				      out.bytes,
				      64,	/* 64-bit RIO address */
				      out.rio_addr,
				      0,	/* Bits 66 and 65 */
				      out.phys_addr);

	/* Adding to database should never fail, but just in case... */
	if (!*msubh) {
		WARN("Failed to add msub to database\n");
		sem_post(&rdma_lock);
		return RDMA_DB_ADD_FAIL;
	}
	
	DBG("msubh = 0x%" PRIx64 "\n", msubh);
	sem_post(&rdma_lock);
	return 0;
} /* rdma_create_msub_h() */

int rdma_destroy_msub_h(ms_h msh, msub_h msubh)
{
	destroy_msub_input	in;
	destroy_msub_output	out;
	struct loc_msub 	*msub;
	int			ret;

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	/* Check for NULL handles */
	if (!msh || !msubh) {
		ERR("Invalid param(s):msh=0x%" PRIx64 ",msubh=0x%" PRIx64 "\n",
								msh, msubh);
		return RDMA_NULL_PARAM;
	}

	DBG("msubh = 0x%" PRIx64 "\n", msubh);

	/* Convert handle to an msub pointer to the element the database */
	msub = (struct loc_msub *)msubh;

	/* Set up input parameters */
	in.msid		= ((struct loc_ms *)msh)->msid;
	in.msubid	= msub->msubid;
	DBG("Attempting to destroy msubid(0x%X) in msid(0x%X)\n", in.msubid,
								  in.msid);
	/* Set up Unix message parameters */
	unix_msg_t  *in_msg;
	unix_msg_t  *out_msg;
	client->get_send_buffer((void **)&in_msg);
	in_msg->type = DESTROY_MSUB;
	in_msg->destroy_msub_in = in;

	ret = alt_rpc_call(in_msg, &out_msg);
	if (ret) {
		ERR("Call to RDMA daemon failed\n");
		return ret;
	}

	out = out_msg->destroy_msub_out;

	/* Check status returned by command on the daemon side */
	if (out.status != 0) {
		ERR("Failed to destroy msub(0x%X)in daemon\n", in.msubid);
		return out.status;
	}

	/* Remove msub from database */
	if (remove_loc_msub(msubh) < 0) {
		WARN("Failed to remove %p from database\n", msub);
		return RDMA_DB_REM_FAIL;
	}

	DBG("Destroyed msub in msh = 0x%" PRIx64 ", msubh = 0x%" PRIx64 "\n", msh, msubh);

	return 0;
} /* rdma_destroy_msub() */

int rdma_mmap_msub(msub_h msubh, void **vaddr)
{
	struct loc_msub *pmsub = (struct loc_msub *)msubh;
	int ret;
	
	sem_wait(&rdma_lock);

	if (!pmsub) {
		ERR("msubh is NULL\n");
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	if (!vaddr) {
		ERR("vaddr is NULL\n");
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	ret = riomp_dma_map_memory(peer.mport_hnd, pmsub->bytes, pmsub->paddr, vaddr);

	if (ret) {
		ERR("map(0x%" PRIx64 ") failed: %s\n", pmsub->paddr, strerror(-ret));
		sem_post(&rdma_lock);
		return ret;
	}
	INFO("phys_addr = 0x%" PRIx64 ", virt_addr = %p, size = 0x%x\n",
						pmsub->paddr, *vaddr, pmsub->bytes);
	sem_post(&rdma_lock);
	return 0;
} /* rdma_mmap_msub() */

int rdma_munmap_msub(msub_h msubh, void *vaddr)
{
	struct loc_msub *pmsub = (struct loc_msub *)msubh;
	int ret;

	sem_wait(&rdma_lock);

	if (!pmsub) {
		ERR("msubh is NULL\n");
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	if (!vaddr) {
		ERR("vaddr is NULL\n");
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	if (munmap(vaddr, pmsub->bytes) == -1) {
	        ERR("munmap(): %s\n", strerror(errno));
		sem_post(&rdma_lock);
		return RDMA_UNMAP_ERROR;
	}

	INFO("Unmapped vaddr(%p), of size %u\n", vaddr, pmsub->bytes);

	sem_post(&rdma_lock);
	return 0;
} /* rdma_unmap_msub() */

int rdma_accept_ms_h(ms_h loc_msh,
		     msub_h loc_msubh,
		     msub_h *rem_msubh,
		     uint32_t *rem_msub_len,
		     uint64_t timeout_secs)
{
	accept_input		accept_in;
	accept_output		accept_out;
	undo_accept_input	undo_accept_in;
	undo_accept_output	undo_accept_out;
	int			ret;
	
	DBG("ENTER with timeout = %u\n", (unsigned)timeout_secs);
	sem_wait(&rdma_lock);

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	/* Check that parameters are not NULL */
	if (!loc_msh || !loc_msubh || !rem_msubh) {
		ERR("loc_msh=0x%" PRIx64 ",loc_msubh=0x%" PRIx64 ",rem_msubh=%p\n",
					loc_msh, loc_msubh, rem_msubh);
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	loc_ms	*ms = (loc_ms *)loc_msh;

	/* If this application has already accepted a connection, fail */
	if (ms->accepted) {
		ERR("Already accepted a connection and it is still active!\n");
		sem_post(&rdma_lock);
		return RDMA_DUPLICATE_ACCEPT;
	}

	/* Send the memory space name and the 'accept' parameters to the daemon
	 * over RPC. This triggers channelized message reception waiting for 
	 * a connection on the specified memory space. Then sends the accept
	 * parameters also in a channelized message, to the remote daemon */
	strcpy(accept_in.loc_ms_name, ms->name);
	accept_in.loc_msid		= ms->msid;
	accept_in.loc_msubid		= ((struct loc_msub *)loc_msubh)->msubid;
	accept_in.loc_bytes		= ((struct loc_msub *)loc_msubh)->bytes;
	accept_in.loc_rio_addr_len	= ((struct loc_msub *)loc_msubh)->rio_addr_len;
	accept_in.loc_rio_addr_lo	= ((struct loc_msub *)loc_msubh)->rio_addr_lo;
	accept_in.loc_rio_addr_hi	= ((struct loc_msub *)loc_msubh)->rio_addr_hi;

	/* Create connection/disconnection message queue */
	string mq_name(ms->name);
	mq_name.insert(0, 1, '/');

	/* Must create the queue before calling the daemon since the
	 * connect request from the remote daemon might come quickly
	 * and try to open the queue to notify us.
	 */
	msg_q<mq_rdma_msg>	*connect_disconnect_mq;
	try {
		connect_disconnect_mq = new msg_q<mq_rdma_msg>(mq_name, MQ_CREATE);
	}
	catch(msg_q_exception& e) {
		ERR("Failed to create connect_disconnect_mq '%s': %s\n",
						mq_name.c_str(),e.msg.c_str());
		sem_post(&rdma_lock);
		return RDMA_MALLOC_FAIL;
	}
	DBG("Message queue %s created for connection from %s\n",
						mq_name.c_str(), ms->name);
	/* Set up Unix message parameters */
	unix_msg_t  *in_msg;
	unix_msg_t  *out_msg;
	client->get_send_buffer((void **)&in_msg);
	in_msg->type = ACCEPT_MS;
	in_msg->accept_in = accept_in;

	ret = alt_rpc_call(in_msg, &out_msg);
	if (ret) {
		ERR("Call to RDMA daemon failed\n");
		delete connect_disconnect_mq;
		sem_post(&rdma_lock);
		return ret;
	}
	accept_out = out_msg->accept_out;

	/* Check status returned by command on the daemon side */
	if (accept_out.status != 0) {
		ERR("Failed to accept on '%s' in daemon\n", ms->name);
		delete connect_disconnect_mq;
		sem_post(&rdma_lock);
		return accept_out.status;
	}

	/* Await 'connect()' from client */
	mq_rdma_msg *mq_rdma_msg;
	connect_disconnect_mq->get_recv_buffer(&mq_rdma_msg);
	mq_connect_msg	*conn_msg = &mq_rdma_msg->connect_msg;
	INFO("Waiting for connect message...\n");
	if (timeout_secs) {
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += timeout_secs;

		if (connect_disconnect_mq->timed_receive(&tm)) {
			ERR("Failed to receive connect message before timeout\n");
			delete connect_disconnect_mq;
			/* Calling undo_accept() to remove the memory space from the list
			 * of memory spaces awaiting a connect message from a remote daemon.
			 */
			strcpy(undo_accept_in.server_ms_name, ms->name);

			/* Set up Unix message parameters */
			client->flush_send_buffer();
			client->flush_recv_buffer();
			in_msg->type = UNDO_ACCEPT;
			in_msg->undo_accept_in = undo_accept_in;
			ret = alt_rpc_call(in_msg, &out_msg);
			if (ret) {
				ERR("Call to RDMA daemon failed\n");
				sem_post(&rdma_lock);
				return ret;
			}
			undo_accept_out = out_msg->undo_accept_out;
			sem_post(&rdma_lock);
			return RDMA_ACCEPT_TIMEOUT;
		}
	} else {
		unsigned retries = 10;
		do {
			if (connect_disconnect_mq->receive()) {
				ERR("Failed to receive MQ_CONNECT_MS message\n");
				delete connect_disconnect_mq;
				sem_post(&rdma_lock);
				return RDMA_ACCEPT_FAIL;
			}
			/* Ensure that it is indeed an MQ_CONNECT_MS message or else fail */
			if (mq_rdma_msg->type == MQ_CONNECT_MS) {
				INFO("*** Connect message received! ***\n");
				DBG("conn_msg->seq_num = 0x%X\n", conn_msg->seq_num);
				break;
			} else {
				ERR("Received message of type 0x%X. DISCARDING & RETRYING\n",
								mq_rdma_msg->type);

				continue;
			}
		} while (retries--);
	}

	/* Validate the message contents based on known values */
	if (
		(conn_msg->rem_rio_addr_len < 16) ||
		(conn_msg->rem_rio_addr_len > 65) ||
		(conn_msg->rem_destid_len < 16) ||
		(conn_msg->rem_destid_len > 64) ||
		(conn_msg->rem_destid >= 0xFFFF)
	   ) {
		CRIT("** INVALID CONNECT MESSAGE CONTENTS** \n");
		connect_disconnect_mq->dump_recv_buffer();
		DBG("conn_msg->rem_msid = 0x%X\n", conn_msg->rem_msid);
		DBG("conn_msg->rem_msubsid = 0x%X\n", conn_msg->rem_msubid);
		DBG("conn_msg->rem_bytes = 0x%X\n", conn_msg->rem_bytes);
		DBG("conn_msg->rem_rio_addr_len = 0x%X\n", conn_msg->rem_rio_addr_len);
		DBG("conn_msg->rem_rio_addr_lo = 0x%X\n", conn_msg->rem_rio_addr_lo);
		DBG("conn_msg->rem_rio_addr_hi = 0x%X\n", conn_msg->rem_rio_addr_hi);
		DBG("conn_msg->rem_destid_len = 0x%X\n", conn_msg->rem_destid_len);
		DBG("conn_msg->rem_destid = 0x%X\n", conn_msg->rem_destid);
		delete connect_disconnect_mq;
		sem_post(&rdma_lock);
		return RDMA_ACCEPT_FAIL;
	}

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
	if (*rem_msubh == (msub_h)NULL) {
		WARN("Failed to add rem_msub to database\n");
		delete connect_disconnect_mq;
		sem_post(&rdma_lock);
		return RDMA_DB_ADD_FAIL;
	}
	INFO("rem_bytes = %d, rio_addr = 0x%lX\n",
			conn_msg->rem_bytes, conn_msg->rem_rio_addr_lo);

	/* Return remote msub length to application */
	*rem_msub_len = conn_msg->rem_bytes;

	pthread_t wait_for_disc_thread;
	if (pthread_create(&wait_for_disc_thread, NULL, wait_for_disc_thread_f, connect_disconnect_mq)) {
		WARN("Failed to create wait_for_disc_thread: %s\n", strerror(errno));
		delete connect_disconnect_mq;
		sem_post(&rdma_lock);
		return RDMA_PTHREAD_FAIL;
	}
	INFO("Disconnection thread for '%s' created\n", ms->name);

	/* Add conn/disc message queue and disc thread to database entry */
	ms->disc_thread = wait_for_disc_thread;
	ms->disc_notify_mq = connect_disconnect_mq;
	ms->accepted = true;

	sem_post(&rdma_lock);
	return 0;
} /* rdma_accept_ms_h() */

/**
 * Given two timespec structs, subtract them and return a timespec containing
 * the difference
 *
 * @start     start time
 * @end       end time
 *
 * @returns         difference
 */
static struct timespec time_difference( struct timespec start, struct timespec end )
{
	struct timespec temp;

	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
} /* time_difference() */

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
	send_connect_output	out;
	struct timespec	before, after, rtt;
	int			ret;
	struct loc_msub		*loc_msub = (struct loc_msub *)loc_msubh;
	static uint32_t seq_num = 0x1234;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	/* Remote msubh pointer cannot point to NULL */
	if (!rem_msubh) {
		WARN("rem_msubh cannot be NULL\n");
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* Check for invalid destid */
	if (rem_destid_len == 16 && rem_destid==0xFFFF) {
		WARN("Invalid destid 0x%X\n", rem_destid);
		sem_post(&rdma_lock);
		return RDMA_INVALID_DESTID;
	}

	/* Prevent buffer overflow due to very long name */
	size_t len = strlen(rem_msname);
	if (len > UNIX_MS_NAME_MAX_LEN) {
		ERR("String 'ms_name' is too long (%d)\n", len);
		sem_post(&rdma_lock);
		return RDMA_NAME_TOO_LONG;
	}

	INFO("Connecting to '%s' on destid(0x%X)\n", rem_msname, rem_destid);
	/* Set up parameters for RPC call */
	strcpy(in.server_msname, rem_msname);
	in.server_destid_len	= rem_destid_len;
	in.server_destid	= rem_destid;
	in.client_destid_len	= 16;
	in.client_destid	= peer.destid;
	in.seq_num		= seq_num++;

	DBG("in.server_msname     = %s\n", rem_msname);
	DBG("in.server_destid_len = 0x%X\n", in.server_destid_len);
	DBG("in.server_destid     = 0x%X\n", in.server_destid);
	DBG("in.client_destid_len = 0x%X\n", in.client_destid_len);
	DBG("in.client_destid     = 0x%X\n", in.client_destid);
	DBG("in.seq_num           = 0x%X\n", in.seq_num);

	if (loc_msubh) {
		in.client_msid		= loc_msub->msid;
		in.client_msubid	= loc_msub->msubid;
		in.client_bytes		= loc_msub->bytes;
		in.client_rio_addr_len	= loc_msub->rio_addr_len;
		in.client_rio_addr_lo	= loc_msub->rio_addr_lo;
		in.client_rio_addr_hi	= loc_msub->rio_addr_hi;
		DBG("in.client_msid = 0x%X\n", in.client_msid);
		DBG("in.client_msubid = 0x%X\n", in.client_msubid);
		DBG("in.client_bytes = 0x%X\n", in.client_bytes);
		DBG("in.client_rio_addr_len = 0x%X\n", in.client_rio_addr_len);
		DBG("in.client_rio_addr_lo = 0x%016" PRIx64 "\n", in.client_rio_addr_lo);
		DBG("in.client_rio_addr_hi = 0x%X\n", in.client_rio_addr_hi);
	} else {
		HIGH("Client has provided a NULL msubh\n");
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
		CRIT("Failed to create accept_mq: %s\n", e.msg.c_str());
		sem_post(&rdma_lock);
		return RDMA_MALLOC_FAIL;
	}
	INFO("Created 'accept' message queue: '%s'\n", mq_name.c_str());

__sync_synchronize();
	clock_gettime( CLOCK_MONOTONIC, &before );
__sync_synchronize();

	/* Set up Unix message parameters */
	unix_msg_t  *in_msg;
	unix_msg_t  *out_msg;
	client->get_send_buffer((void **)&in_msg);
	in_msg->type = SEND_CONNECT;
	in_msg->send_connect_in = in;
	ret = alt_rpc_call(in_msg, &out_msg);
	if (ret) {
		ERR("Call to RDMA daemon failed\n");
		delete accept_mq;
		sem_post(&rdma_lock);
		return ret;
	}
	out = out_msg->send_connect_out;
	if (out.status) {
		ERR("Connection to destid(0x%X) failed\n", rem_destid);
		delete accept_mq;
		sem_post(&rdma_lock);
		return out.status;
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
			undo_connect_output	undo_connect_out;

			strcpy(undo_connect_in.server_ms_name, rem_msname);

			/* Set up Unix message parameters */
			in_msg->type = UNDO_CONNECT;
			in_msg->undo_connect_in = undo_connect_in;
			ret = alt_rpc_call(in_msg, &out_msg);
			if (ret) {
				ERR("Call to RDMA daemon failed\n");
				delete accept_mq;
				sem_post(&rdma_lock);
				return ret;
			}
			undo_connect_out = out_msg->undo_connect_out;
			delete accept_mq;
			sem_post(&rdma_lock);
			return RDMA_CONNECT_TIMEOUT;
		}
	} else {
#ifdef CONNECT_BEFORE_ACCEPT_HACK
#error Do not use this feature anymore!
		auto retries = 10;
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += 1;

		if (accept_mq->timed_receive(&tm) && retries--) {
			DBG("Retrying...\n");
			ret = alt_rpc_call();
			if (ret) {
				ERR("Call to RDMA daemon failed\n");
				delete accept_mq;
				sem_post(&rdma_lock);
				return ret;
			}
			out = out_msg->send_connect_out;
			if (out.status) {
				ERR("Connection to destid(0x%X) failed\n", rem_destid);
				delete accept_mq;
				sem_post(&rdma_lock);
				return out.status;
			}
		}
		if (retries == 0) {
#else
		if (accept_mq->receive()) {
#endif
			ERR("Failed to receive accept message\n");
			delete accept_mq;
			sem_post(&rdma_lock);
			return RDMA_CONNECT_FAIL;
		}
	}
	INFO(" Accept message received!\n");
	accept_mq->dump_recv_buffer();

__sync_synchronize();
	clock_gettime(CLOCK_MONOTONIC, &after);
__sync_synchronize();
	rtt = time_difference(before, after);
	HIGH("Round-trip-time for accept/connect = %u seconds and %u microseconds\n",
			rtt.tv_sec, rtt.tv_nsec/1000);
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
	if (*rem_msubh == (msub_h)NULL) {
		ERR("Failed to store rem_msub in database\n");
		delete accept_mq;
		sem_post(&rdma_lock);
		return RDMA_DB_ADD_FAIL;
	}
	INFO("Remote msubh has size %d, rio_addr = 0x%016" PRIx64 "\n",
			accept_msg->server_bytes, accept_msg->server_rio_addr_lo);
	INFO("rem_msub has destid = 0x%X, destid_len = 0x%X\n",
			accept_msg->server_destid, accept_msg->server_destid_len);

	/* Remote memory subspace length */
	*rem_msub_len = accept_msg->server_bytes;

	/* Save server_msid since we need to store that in the databse */
	uint32_t server_msid = accept_msg->server_msid;

	/* Create a message queue for the 'destroy' message */
	mq_name.insert(1, "dest-");
	msg_q<mq_destroy_msg>	*destroy_mq;
	DBG("Creating destroy_mq named '%s'\n", mq_name.c_str());
	try {
		destroy_mq = new msg_q<mq_destroy_msg>(mq_name, MQ_CREATE);
	}
	catch(msg_q_exception& e) {
		CRIT("Failed to create destroy_mq: %s\n", e.msg.c_str());
		sem_post(&rdma_lock);
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
		delete accept_mq;
		sem_post(&rdma_lock);
		return RDMA_MALLOC_FAIL;
	}
	INFO("client_wait_for_destroy_thread created.\n");

	/* Remote memory space handle */
	*rem_msh = (ms_h)add_rem_ms(rem_msname,
				    server_msid,
				    client_wait_for_destroy_thread,
				    destroy_mq);
	if (*rem_msh == (ms_h)NULL) {
		ERR("Failed to store rem_ms in database\n");
		pthread_cancel(client_wait_for_destroy_thread);
		delete destroy_mq;
		sem_post(&rdma_lock);
		return RDMA_DB_ADD_FAIL;
	} else {
		DBG("Entry for '%s' stored in DB, handle = 0x%016" PRIx64 "\n",
							rem_msname, *rem_msh);
	}

	/* Accept message queue is no longer needed */
	delete accept_mq;

	INFO("EXIT\n");
	sem_post(&rdma_lock);
	return 0;
} /* rdma_conn_ms_h() */

int rdma_disc_ms_h(ms_h rem_msh, msub_h loc_msubh)
{
	send_disconnect_input	in;
	send_disconnect_output	out;
	int			ret;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	/* Check the daemon hasn't died since we established its socket connection */
	if (!rdmad_is_alive()) {
		WARN("Local RDMA daemon has died.\n");
	}

	/* Check that library has been initialized */
	CHECK_LIB_INIT();

	/* Check that parameters are not NULL */
	if (!rem_msh) {
		ERR("rem_msh=0x%016" PRIx64 ". FAILING (NULL parameter)!!\n",
								rem_msh);
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* If the memory space was destroyed, it should not be in the database */
	if (!rem_ms_exists(rem_msh)) {
		WARN("rem_msh(0x%lX) not in database. Returning\n", rem_msh);
		/* Not an error if the memory space was destroyed */
		sem_post(&rdma_lock);
		return 0;
	}

	rem_ms *ms = (rem_ms *)rem_msh;
	uint32_t msid = ms->msid;
	INFO("rem_msh exists and has msid(0x%X), rem_msh = %" PRIx64 "\n",
								msid, rem_msh);

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
		sem_post(&rdma_lock);
		return RDMA_INVALID_MS;
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
		ERR("Failed to cancel client_wait_for_destroy_thread\n");
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
		sem_post(&rdma_lock);
		return RDMA_DB_REM_FAIL;
	}

	/* Set up Unix message parameters */
	unix_msg_t  *in_msg;
	unix_msg_t  *out_msg;
	client->get_send_buffer((void **)&in_msg);
	in_msg->type = SEND_DISCONNECT;
	in_msg->send_disconnect_in = in;
	ret = alt_rpc_call(in_msg, &out_msg);
	if (ret) {
		ERR("Call to RDMA daemon failed\n");
		sem_post(&rdma_lock);
		return ret;
	}
	out = out_msg->send_disconnect_out;

	/* Check status returned by command on the daemon side */
	if (out.status != 0) {
		ERR("Failed to send disconnect from ms in daemon\n");
		sem_post(&rdma_lock);
		return out.status;
	}
	sem_post(&rdma_lock);
	return 0;
} /* rdma_disc_ms_h() */

int rdma_push_msub(const struct rdma_xfer_ms_in *in,
		   struct rdma_xfer_ms_out *out)
{
	struct loc_msub *lmsub;
	struct rem_msub *rmsub;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	/* Check for NULL pointers */
	if (!in || !out) {
		ERR("%s: NULL. in=%p, out=%p\n", in, out);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}
	lmsub = (struct loc_msub *)in->loc_msubh;
	rmsub = (struct rem_msub *)in->rem_msubh;
	if (!lmsub || !rmsub) {
		ERR("%s: NULL. lmsub=%p, rmsub=%p\n", lmsub, rmsub);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n", rmsub->destid_len);
		out->in_param_ok = -2;
		sem_post(&rdma_lock);
		return RDMA_INVALID_DESTID;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		sem_post(&rdma_lock);
		return RDMA_INVALID_RIO_ADDR;
	}

	/* Check if local daemon is alive */
	DBG("Check if local daemon is alive\n");
	if (!rdmad_is_alive()) {
		ERR("Local RDMA daemon is dead. Exiting\n");
		sem_post(&rdma_lock);
		return RDMA_DAEMON_UNREACHABLE;
	}

	/* Check if remote daemon is alive */
	DBG("Check if remote daemon is alive\n");
	if (fm_alive && (dd_h != NULL))
		if (!fmdd_check_did(dd_h, rmsub->destid, FMDD_RDMA_FLAG)) {
			ERR("Remote destination daemon NOT running!\n");
			sem_post(&rdma_lock);
			return RDMA_REMOTE_UNREACHABLE;
		}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	/* Echo parameters for debugging */
	INFO("Sending %u bytes over DMA to destid=0x%X\n", in->num_bytes,
								rmsub->destid);
	INFO("Dest RIO addr =  %016" PRIx64 ", lmsub->paddr = %016" PRIx64 "\n",
					rmsub->rio_addr_lo + in->rem_offset,
					lmsub->paddr);
	/* Determine sync type */
	enum riomp_dma_directio_transfer_sync rd_sync;

	switch (in->sync_type) {

	case rdma_no_wait:
		rd_sync = RIO_DIRECTIO_TRANSFER_FAF;
		INFO("RIO_DIRECTIO_TRANSFER_FAF\n");
		break;
	case rdma_sync_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_SYNC;
		INFO("RIO_DIRECTIO_TRANSFER_SYNC\n");
		break;
	case rdma_async_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_ASYNC;
		INFO("RIO_DIRECTIO_TRANSFER_ASYNC\n");
		break;
	default:
		ERR("Invalid sync_type\n", in->sync_type);
		out->in_param_ok = RDMA_INVALID_SYNC_TYPE;
		sem_post(&rdma_lock);
		return RDMA_INVALID_SYNC_TYPE;
	}

	int ret = riomp_dma_write_d(peer.mport_hnd,
				    (uint16_t)rmsub->destid,
				    rmsub->rio_addr_lo + in->rem_offset,
				    lmsub->paddr,
				    in->loc_offset,
				    in->num_bytes,
					RIO_DIRECTIO_TYPE_NWRITE_R,
				    rd_sync);
	if (ret) {
		ERR("riomp_dma_write_d() failed:(%d) %s\n", ret, strerror(errno));
		sem_post(&rdma_lock);
		return ret;
	}

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value of riomp_dma_write_d() is the token (if >= 0) */
	if (in->sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (in->sync_type == rdma_async_chk && ret >= 0) {
		out->chk_handle = ret;	/* token */
		ret = 0;	/* success */
	}

	sem_post(&rdma_lock);
	return ret;
} /* rdma_push_msub() */

int rdma_push_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	(void)priority;
	struct rem_msub *rmsub;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	/* Check for NULL pointer */
	if (!buf || !out || !rem_msubh) {
		ERR("NULL param(s). buf=%p, out=%p, rem_msubh = %u\n",
							buf, out, rem_msubh);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}
	rmsub = (struct rem_msub *)rem_msubh;

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n", rmsub->destid_len);
		out->in_param_ok = -2;
		sem_post(&rdma_lock);
		return RDMA_INVALID_DESTID;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		sem_post(&rdma_lock);
		return RDMA_INVALID_RIO_ADDR;
	}

	/* Check if remote daemon is alive */
	if (fm_alive && (dd_h != NULL))
		if (!fmdd_check_did(dd_h, rmsub->destid, FMDD_RDMA_FLAG)) {
			ERR("Remote destination daemon NOT running!\n");
			sem_post(&rdma_lock);
			return RDMA_REMOTE_UNREACHABLE;
		}

	/* Determine sync type */
	enum riomp_dma_directio_transfer_sync rd_sync;

	switch (sync_type) {

	case rdma_no_wait:
		rd_sync = RIO_DIRECTIO_TRANSFER_FAF;
		break;
	case rdma_sync_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type: %d\n", sync_type);
		out->in_param_ok = RDMA_INVALID_SYNC_TYPE;
		sem_post(&rdma_lock);
		return RDMA_INVALID_SYNC_TYPE;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Sending %u bytes over DMA to destid=0x%X, rio_addr = 0x%X\n",
				num_bytes,
				rmsub->destid,
				rmsub->rio_addr_lo + rem_offset);

	int ret = riomp_dma_write(peer.mport_hnd,
				  (uint16_t)rmsub->destid,
				  rmsub->rio_addr_lo + rem_offset,
				  buf,
				  num_bytes,
				  RIO_DIRECTIO_TYPE_NWRITE_R,
				  rd_sync);
	if (ret < 0) {
		ERR("riomp_dma_write() failed:(%d) %s\n", ret, strerror(ret));
	}

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value riomp_dma_write() is the token (if >= 0) */
	if (sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (sync_type == rdma_async_chk && ret >= 0 ) {
		out->chk_handle = ret;	/* token */
		ret = 0;	/* success */
	}

	sem_post(&rdma_lock);
	return ret;
} /* rdma_push_buf() */

int rdma_pull_msub(const struct rdma_xfer_ms_in *in,
		   struct rdma_xfer_ms_out *out)
{
	struct loc_msub *lmsub;
	struct rem_msub *rmsub;

	DBG("ENTER\n");
	
	sem_wait(&rdma_lock);
	/* Check for NULL pointers */
	if (!in || !out) {
		ERR("NULL. in=%p, out=%p\n", in, out);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}
	lmsub = (struct loc_msub *)in->loc_msubh;
	rmsub = (struct rem_msub *)in->rem_msubh;
	if (!lmsub || !rmsub) {
		ERR("%s: NULL. lmsub=%p, rmsub=%p", lmsub, rmsub);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n", rmsub->destid_len);
		out->in_param_ok = -2;
		sem_post(&rdma_lock);
		return RDMA_INVALID_DESTID;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		sem_post(&rdma_lock);
		return RDMA_INVALID_RIO_ADDR;
	}

	/* Check if local daemon is alive */
	if (!rdmad_is_alive()) {
		ERR("Local RDMA daemon is dead. Exiting\n");
		sem_post(&rdma_lock);
		return RDMA_DAEMON_UNREACHABLE;
	}

	/* Check if remote daemon is alive */
	if (fm_alive && (dd_h != NULL))
		if (!fmdd_check_did(dd_h, rmsub->destid, FMDD_RDMA_FLAG)) {
			ERR("Remote destination daemon NOT running!\n");
			sem_post(&rdma_lock);
			return RDMA_REMOTE_UNREACHABLE;
		}

	/* Determine sync type */
	enum riomp_dma_directio_transfer_sync rd_sync;

	switch (in->sync_type) {

	case rdma_no_wait:	/* No FAF in reads, change to synchronous */
	case rdma_sync_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type: %d\n", in->sync_type);
		out->in_param_ok = RDMA_INVALID_SYNC_TYPE;
		sem_post(&rdma_lock);
		return RDMA_INVALID_SYNC_TYPE;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Receiving %u bytes over DMA from destid=0x%X\n",
						in->num_bytes, rmsub->destid);
	INFO("Source RIO addr = 0x%X, lmsub->paddr = 0x%lX\n",
				rmsub->rio_addr_lo + in->rem_offset,
				lmsub->paddr);

	int ret = riomp_dma_read_d(peer.mport_hnd,
				   (uint16_t)rmsub->destid,
				   rmsub->rio_addr_lo + in->rem_offset,
				   lmsub->paddr,
				   in->loc_offset,
				   in->num_bytes,
				   rd_sync);
	if (ret < 0) {
		ERR("riomp_dma_read_d() failed:(%d) %s\n", ret, strerror(ret));
	}

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value of riomp_dma_read_d() is the token (if >= 0) */
	if (in->sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (in->sync_type == rdma_async_chk && ret >= 0) {
		out->chk_handle = ret; /* token */
		ret = 0; /* success */
	}

	sem_post(&rdma_lock);
	return ret;
} /* rdma_pull_msub() */

int rdma_pull_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	(void)priority;
	struct rem_msub *rmsub;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	/* Check for NULL pointers */
	if (!buf || !out || !rem_msubh) {
		ERR("NULL param(s): buf=%p, out=%p, rem_msubh=%u\n",
							buf, out, rem_msubh);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}
	rmsub = (struct rem_msub *)rem_msubh;
	if (!rmsub) {
		ERR("NULL param(s): rem_msubh=%u\n", rem_msubh);
		out->in_param_ok = -1;
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
	}

	/* Check for destid > 16 */
	if (rmsub->destid_len > 16) {
		ERR("destid_len=%u unsupported\n",rmsub->destid_len);
		out->in_param_ok = -2;
		sem_post(&rdma_lock);
		return RDMA_INVALID_DESTID;
	}

	/* Check for RIO address > 64-bits */
	if (rmsub->rio_addr_len > 64) {
		ERR("rio_addr_len=%u unsupported\n", rmsub->rio_addr_len);
		out->in_param_ok = -3;
		sem_post(&rdma_lock);
		return RDMA_INVALID_RIO_ADDR;
	}

	/* Check if remote daemon is alive */
	if (fm_alive && (dd_h != NULL))
		if (!fmdd_check_did(dd_h, rmsub->destid, FMDD_RDMA_FLAG)) {
			ERR("Remote destination daemon NOT running!\n");
			sem_post(&rdma_lock);
			return RDMA_REMOTE_UNREACHABLE;
		}

	/* Determine sync type */
	enum riomp_dma_directio_transfer_sync rd_sync;

	switch (sync_type) {

	case rdma_no_wait:	/* No FAF in reads, change to synchronous */
	case rdma_sync_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_SYNC;
		break;
	case rdma_async_chk:
		rd_sync = RIO_DIRECTIO_TRANSFER_ASYNC;
		break;
	default:
		ERR("Invalid sync_type: %d\n", sync_type);
		out->in_param_ok = RDMA_INVALID_SYNC_TYPE;
		sem_post(&rdma_lock);
		return RDMA_INVALID_SYNC_TYPE;
	}

	/* All input parameters are OK */
	out->in_param_ok = 0;

	INFO("Receiving %u DMA bytes from destid=0x%X, RIO addr = 0x%X\n",
					num_bytes,
					rmsub->destid,
					rmsub->rio_addr_lo + rem_offset);

	int ret = riomp_dma_read(peer.mport_hnd,
				 (uint16_t)rmsub->destid,
				 rmsub->rio_addr_lo + rem_offset,
				 buf,
				 num_bytes,
				 rd_sync);
	if (ret < 0) {
		ERR("riomp_dma_read() failed:(%d) %s\n", ret, strerror(ret));
	}

	/* If synchronous, the return value is the xfer status. If async,
	 * the return value of riomp_dma_read() is the token (if >= 0) */
	if (sync_type == rdma_sync_chk)
		out->dma_xfr_status = ret;
	else if (sync_type == rdma_async_chk && ret >= 0) {
		out->chk_handle = ret; /* token */
		ret = 0; /* success */
	}

	sem_post(&rdma_lock);
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
	wait_param->err = riomp_dma_wait_async(peer.mport_hnd,
					   wait_param->token,
					   wait_param->timeout);

	/* Exit the thread; it is no longer needed */
	pthread_exit(0);
}

int rdma_sync_chk_push_pull(rdma_chk_handle chk_handle,
			    const struct timespec *wait)
{
	dma_async_wait_param	wait_param;

	DBG("ENTER\n");
	sem_wait(&rdma_lock);

	/* Make sure handle is valid */
	if (!chk_handle) {
		ERR("Invalid chk_handle(%d)\n", chk_handle);
		sem_post(&rdma_lock);
		return RDMA_NULL_PARAM;
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
		sem_post(&rdma_lock);
		return RDMA_PTHREAD_FAIL;
	}

	/* Wait for transfer completion or timeout in thread */
	if (pthread_join(compl_thread, NULL)) {
		ERR("pthread_join(): %s\n", strerror(errno));
		sem_post(&rdma_lock);
		return RDMA_PTHREAD_FAIL;
	}

	/* wait_param->err was populated with result (including timeout) */
	sem_post(&rdma_lock);
	return wait_param.err;
} /* rdma_sync_chk_push_pull() */

#ifdef __cplusplus
}
#endif
