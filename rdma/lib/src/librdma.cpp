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
#include <thread>
#include <memory>
#include "memory_supp.h"

#include <rapidio_mport_mgmt.h>

#include "rdma_types.h"
#include "liblog.h"
#include "librdma_rx_engine.h"
#include "librdma_tx_engine.h"
#include "librdma_msg_processor.h"
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
using std::unique_ptr;


static unix_msg_processor	msg_proc;

static unique_ptr<unix_rx_engine> rx_eng;
static unique_ptr<unix_tx_engine> tx_eng;

static thread *engine_monitoring_thread;

#ifdef __cplusplus
extern "C" {
#endif

static auto init = false; /* Global flag indicating library initialization */

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

/**
 * Call a function in the daemon.
 *
 * @param in_msg
 * @param out_msg
 *
 * @return 0 if successful
 */
static int daemon_call(unique_ptr<unix_msg_t> in_msg, unix_msg_t *out_msg)
{
	int rc;
	constexpr uint32_t MSG_SEQ_NO_START = 0x0000000A;
	static rdma_msg_seq_no seq_no = MSG_SEQ_NO_START;

	/* First check that engines are still valid */
	if ((tx_eng == nullptr) || (rx_eng == nullptr)) {
		CRIT("Connection to daemon severed");
		return RDMA_DAEMON_UNREACHABLE;
	}

	/* Prepare for reply */
	auto reply_sem = make_shared<sem_t>();
	sem_init(reply_sem.get(), 0, 0);

	auto reply_type = in_msg->type | 0x8000;
	auto reply_cat  = RDMA_CALL;
	rx_eng->set_notify(reply_type, reply_cat, seq_no, reply_sem);

	/* Send message */
	in_msg->seq_no = seq_no;
	DBG("Queuing for sending: type='%s',0x%X, cat='%s',0x%X, seq_no = 0x%X",
		type_name(in_msg->type), in_msg->type, cat_name(in_msg->category),
		in_msg->category, in_msg->seq_no);
	tx_eng->send_message(move(in_msg));

	/* Wait for reply */
	DBG("Waiting for notification (type='%s',0x%X, cat='%s',0x%X, seq_no=0x%X)",
		type_name(reply_type), reply_type, cat_name(RDMA_CALL),
					RDMA_CALL, seq_no);
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 5;	/* 5 second timeout */
	rc = sem_timedwait(reply_sem.get(), &timeout);
	if (rc) {
		ERR("reply_sem failed: %s", strerror(errno));
		if (errno == ETIMEDOUT) {
			ERR("Timeout occurred");
			rc = ETIMEDOUT;
		}
	} else {
		DBG("Got reply!");
		rc = rx_eng->get_message(reply_type, reply_cat, seq_no, out_msg);
		if (rc) {
			ERR("Failed to obtain reply message, rc = %d", rc);
		}
	}
	if (rc == ETIMEDOUT) {
		rc = RDMA_API_TIMEOUT;
	}
	/* Now increment seq_no for next call */
	seq_no++;
 	return rc;
} /* daemon_call() */

int rdmad_kill_daemon()
{
	int rc;

	auto in_msg = make_unique<unix_msg_t>();

	in_msg->type = RDMAD_KILL_DAEMON;
	in_msg->category = RDMA_REQ_RESP;
	in_msg->rdmad_kill_daemon_in.dummy = 0x666;

	unix_msg_t  	out_msg;
	rc = daemon_call(move(in_msg), &out_msg);
	if (rc ) {
		ERR("Failed in daemon call to get kill daemon");
	}

	/* The daemon dies. It cannot send us back a reply */

	return rc;
} /* rdmad_kill_daemon() */

/**
 * For testing only. Not exposed in librdma.h.
 */
int rdma_get_ibwin_properties(unsigned *num_ibwins, uint32_t *ibwin_size)
{
	auto rc = 0;

	try {
		auto in_msg = make_unique<unix_msg_t>();

		in_msg->type = GET_IBWIN_PROPERTIES;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->get_ibwin_properties_in.dummy = 0xc0de;

		unix_msg_t  	out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in daemon call to get ibwin props");
			throw rc;
		}

		if (out_msg.get_ibwin_properties_out.status) {
			ERR("Failed to get ibwin props from daemon");
			throw out_msg.get_ibwin_properties_out.status;
		}

		DBG("num_ibwins = %u, ibwin_size = %uKB",
			out_msg.get_ibwin_properties_out.num_ibwins,
			out_msg.get_ibwin_properties_out.ibwin_size/1024);

		*num_ibwins = out_msg.get_ibwin_properties_out.num_ibwins;
		*ibwin_size = out_msg.get_ibwin_properties_out.ibwin_size;
	}
	catch(int& e) {
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
		ERR("NULL parameter.");
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
 * Useful ONLY for APIs that don't already call into the daemon such
 * as the ones that map msubs.
 */
static bool rdmad_is_alive()
{
	return true;
/*
	auto in_msg = make_unique<unix_msg_t>();

	in_msg->type = RDMAD_IS_ALIVE;
	in_msg->category = RDMA_REQ_RESP;
	in_msg->rdmad_is_alive_in.dummy = 0x1234;

	unix_msg_t  out_msg;
	return daemon_call(move(in_msg), &out_msg) != RDMA_DAEMON_UNREACHABLE;
*/
} /* rdmad_is_alive() */

static int open_mport(void)
{
	auto rc = 0;
	DBG("ENTER");

	/* Set up Unix message parameters */
	auto in_msg = make_unique<unix_msg_t>();
	unix_msg_t	out_msg;

	in_msg->type = GET_MPORT_ID;
	in_msg->category = RDMA_REQ_RESP;
	in_msg->get_mport_id_in.dummy = 0x1234;

	rc = daemon_call(move(in_msg), &out_msg);
	if (rc ) {
		ERR("Failed to obtain mport ID");
		return rc;
	}

	/* Get the mport ID */
	peer.mport_id = out_msg.get_mport_id_out.mport_id;
	INFO("Using mport_id = %d", peer.mport_id);

	/* Now open the port */
	auto flags = 0;
	rc = riomp_mgmt_mport_create_handle(peer.mport_id, flags,
							&peer.mport_hnd);
	if (rc) {
		CRIT("riomp_mgmt_mport_create_handle(): %s", strerror(errno));
		CRIT("Cannot open mport%d, is rio_mport_cdev loaded?",
								peer.mport_id);
		return RDMA_MPORT_OPEN_FAIL;
	}

	/* Read the properties. */
	struct riomp_mgmt_mport_properties prop;
	if (!riomp_mgmt_query(peer.mport_hnd, &prop)) {
		riomp_mgmt_display_info(&prop);
		if (prop.flags &RIO_MPORT_DMA) {
			INFO("DMA is ENABLED");
		} else {
			CRIT("DMA capability DISABLED");
			riomp_mgmt_mport_destroy_handle(&peer.mport_hnd);
			memset(&peer.mport_hnd, 0, sizeof(peer.mport_hnd));
			return RDMA_NOT_SUPPORTED;
		}
		peer.destid = prop.hdid;
	} else {
		/* Unlikely we fail on reading properties, but warn! */
		WARN("Error reading properties from mport%d", peer.mport_id);
	}
	return 0;
} /* open_mport() */

/**
 * @brief Engine monitoring thread.
 */
void engine_monitoring_thread_f(sem_t *engine_cleanup_sem)
{
	/* Wait until there is a reason to perform cleanup */
	HIGH("Waiting for engine_cleanup_sem");
	sem_wait(engine_cleanup_sem);

	HIGH("Cleaning up dead engines!");
	if ((tx_eng != nullptr) && tx_eng->isdead()) {
		HIGH("Killing tx_eng");
		tx_eng.reset();
	}

	if ((rx_eng != nullptr) && rx_eng->isdead()) {
		HIGH("Killing rx_eng");
		rx_eng.reset();
	}

	/* Purge database and set state to uninitialized */
	purge_local_database();
	init = false;

	sem_destroy(engine_cleanup_sem);
	free(engine_cleanup_sem);
} /* engine_monitoring_thread_f() */

/**
 * Initialize RDMA library
 *
 * initialize() is automatically called once, when the library is loaded
 * in response to a call to one the RDMA APIs. initialize() may be
 * called again whenever an API fails to retry or get a reason code for failure.
 *
 * @return: 0 if successful
 */
static int initialize(void)
{
	auto ret = 0;

	if (init) {
		WARN("RDMA library already initialized");
		return ret;
	}

	sem_init(&rdma_lock, 0, 1);

	/* Create a client */
	DBG("Creating client object...");
	try {
		client = make_shared<unix_client>("librdma", nullptr);

		/* Connect to server */
		if (client->connect() == 0) {
			INFO("Successfully connected to RDMA daemon");

			/* Engine cleanup semaphore. Posted by engines that die
			 * so we can clean up after them. */
			sem_t* engine_cleanup_sem = (sem_t*)calloc(1, sizeof(sem_t));
			sem_init(engine_cleanup_sem, 0, 0);

			/* Create Tx and Rx engines */
			tx_eng = make_unique<unix_tx_engine>(
					"librdma_tx_eng",
					client,
					engine_cleanup_sem);
			rx_eng = make_unique<unix_rx_engine>(
					"librdma_rx_eng",
					client,
					msg_proc,
					tx_eng.get(),
					engine_cleanup_sem);

			/* Start engine monitoring thread */
			engine_monitoring_thread =
					new thread(engine_monitoring_thread_f,
						   engine_cleanup_sem);
			/* Open the master port */
			ret = open_mport();
			if (ret == 0) {
				/* Success */
				INFO("MPORT successfully opened");
				init = true;
				INFO("RDMA library fully initialized ");
			} else {
				CRIT("Failed to open mport");
				throw RDMA_MPORT_OPEN_FAIL;
			}
		} else {
			CRIT("Connect failed. Daemon running?");
			throw RDMA_DAEMON_UNREACHABLE;
		}
	} /* try */
	catch(unix_sock_exception& e) {
		CRIT("%s", e.what());
		client = nullptr;
		ret = RDMA_MALLOC_FAIL;
	}
	catch(exception& e) {
		ERR("Failed to create Tx/Rx engines: %s", e.what());
		client.reset();
		ret = RDMA_MALLOC_FAIL;
	}
	catch(int& e) {
		switch (e) {
		case RDMA_MPORT_OPEN_FAIL:
		case RDMA_DAEMON_UNREACHABLE:
			tx_eng.reset();
			rx_eng.reset();
			ret = e;
			break;
		default:
			ret = -1;
		}
		client.reset();
	}

	DBG("ret = %d", ret);
	return ret;
} /* initialize() */

__attribute__((constructor)) int lib_init(void)
{
	/* Initialize the logger */
	if (rdma_log_init("librdma.log", 0)) {
		CRIT("Failed to initialize the logger.Exiting");
		exit(1);
	}

	/* Library initialization */
	if (initialize()) {
		CRIT("Failed to initialize library. Exiting.");
		exit(2);
	}

	return 0;
} /* rdma_lib_init() */

/**
 * If the daemon is restarted, then the connection we have will be bad,
 * and the tx/rx engines will be dead. We therefore need to re-run the
 * initialization code again. 'init' is set to false when the engines
 * are killed to indicate the need for re-init.
 */
#define LIB_INIT_CHECK(rc) if (!init) { \
		WARN("RDMA library not initialized, re-initializing"); \
		rc = initialize(); \
		if (rc != 0) { \
			ERR("Failed to re-initialize RDMA library"); \
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
			ERR("NULL param: owner_name=%p, msoh=%p",
							owner_name, msoh);
			throw RDMA_NULL_PARAM;
		}

		/* Check that owner does not already exist */
		if (find_mso_by_name(owner_name)) {
			ERR("Cannot create another owner named '%s'", owner_name);
			throw RDMA_DUPLICATE_MSO;
		}

		/* Prevent buffer overflow due to very long name */
		size_t len = strlen(owner_name);
		if (len > UNIX_MS_NAME_MAX_LEN) {
			ERR("String 'owner_name' is too long (%d)", len);
			throw RDMA_NAME_TOO_LONG;
		}

		/* Set up Unix message parameters */
		auto in_msg = make_unique<unix_msg_t>();
		unix_msg_t	out_msg;
		in_msg->type     = CREATE_MSO;
		in_msg->category = RDMA_REQ_RESP;
		strcpy(in_msg->create_mso_in.owner_name, owner_name);

		/* Call into daemon */
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in CREATE_MSO daemon_call, rc = %d", rc);
			throw rc;
		}

		/* Failed to create mso? */
		if (out_msg.create_mso_out.status) {
			ERR("Failed to create mso '%s' in daemon, status = 0x%X",
				owner_name, out_msg.create_mso_out.status);
			throw out_msg.create_mso_out.status;
		}

		/* Store in database, with owned = true */
		*msoh = add_loc_mso(owner_name, out_msg.create_mso_out.msoid, true);
		if (!*msoh) {
			WARN("add_loc_mso() failed, msoid = 0x%X",
						out_msg.create_mso_out.msoid);
			throw RDMA_DB_ADD_FAIL;
		}
	} /* try */
	catch(int& e) {
		rc = e;
	}

	sem_post(&rdma_lock);
	return rc;
} /* rdma_create_mso_h() */

int rdma_open_mso_h(const char *owner_name, mso_h *msoh)
{
	auto rc = 0;

	DBG("ENTER");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!owner_name || !msoh) {
			ERR("NULL param: owner_name=%p, msoh=%p",
							owner_name, msoh);
			throw RDMA_NULL_PARAM;
		}

		/* Check if the mso was created by same app, or already open */
		*msoh = find_mso_by_name(owner_name);
		if (*msoh) {
			loc_mso *mso = (loc_mso *)(*msoh);
			/* Don't allow opening from the same app that created the mso */
			if (mso->owned) {
				ERR("Cannot open mso '%s' from creator app",
								owner_name);
				throw RDMA_CANNOT_OPEN_MSO;
			} else {
				/* Already open, just return the handle */
				WARN("%s is already open!", owner_name);
				throw RDMA_SUCCESS; /* Not an error */
			}
		}

		/* Set up input parameters */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type = OPEN_MSO;
		in_msg->category = RDMA_REQ_RESP;
		strcpy(in_msg->open_mso_in.owner_name, owner_name);

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc) {
			ERR("Failed in OPEN_MSO daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to open mso? */
		if (out_msg.open_mso_out.status) {
			ERR("Failed to open mso '%s', status = 0x%X",
				owner_name, out_msg.open_mso_out.status);
			throw out_msg.open_mso_out.status;
		}

		/* Store in database */
		*msoh = add_loc_mso(owner_name,
			    out_msg.open_mso_out.msoid,
			    /* owned is */false);
		if (!*msoh) {
			WARN("add_loc_mso() failed, msoid = 0x%X",
					out_msg.open_mso_out.msoid);
			throw RDMA_DB_ADD_FAIL;
		}
	} /* try */

	catch(int& e) {
		rc = e;
	}
	DBG("EXIT");
	sem_post(&rdma_lock);
	return rc;
} /* rdma_open_mso_h() */

int rdma_close_ms_h_locked(mso_h msoh, ms_h msh);

int rdma_close_mso_h(mso_h msoh)
{
	auto rc = 0;

	DBG("ENTER");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL msoh */
		if (!msoh) {
			WARN("msoh is NULL");
			throw RDMA_NULL_PARAM;
		}

		/* Check if msoh has already been removed (as a result of the
		 *  owner destroying, the owner dying, the daemon dying..etc.)
		 *  or has already been closed once before. */
		if (!mso_h_exists(msoh)) {
			WARN("msoh no longer exists");
			throw RDMA_SUCCESS;
		}

		/* Get list of memory spaces opened by this owner */
		list<loc_ms *>	ms_list(get_num_ms_by_msoh(msoh));
		get_list_msh_by_msoh(msoh, ms_list);

		DBG("ms_list now has %d elements", ms_list.size());

		/* For each one of the memory spaces in this mso, close */
		bool	ok =  true;
		for_each(begin(ms_list),
			 end(ms_list),
			[&ok, msoh](loc_ms *ms)
			{
				if (rdma_close_ms_h_locked(msoh, ms_h(ms))) {
					WARN("rdma_close_ms_h_locked failed: msoh = 0x%"
						PRIx64 ", msh = 0x%" PRIx64 "",
								msoh, ms_h(ms));
					ok = false;	/* Modify on error only */
				} else {
					INFO("msh(0x%" PRIx64 ") owned by msoh(0x%"
						PRIx64 ") closed", ms_h(ms), msoh);
				}
			});

		/* Fail on error. We can't close an mso if there are ms's open */
		if (!ok) {
			ERR("Failed to close one or more mem spaces");
			throw RDMA_MS_CLOSE_FAIL;
		}

		/* Set up Unix message parameters */
		auto in_msg = make_unique<unix_msg_t>();

		in_msg->close_mso_in.msoid = ((loc_mso *)msoh)->msoid;
		in_msg->type 	= CLOSE_MSO;
		in_msg->category = RDMA_REQ_RESP;

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc) {
			ERR("Failed in CLOSE_MSO daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Check status returned by command on the daemon side */
		if (out_msg.close_mso_out.status != 0) {
			ERR("Failed to close mso(0x%X) in daemon, status = 0x%X",
						((loc_mso *)msoh)->msoid,
						out_msg.close_mso_out.status);
			throw out_msg.close_mso_out.status;
		}

		/* Take it out of database */
		if (remove_loc_mso(msoh) < 0) {
			WARN("Failed to find 0x%" PRIx64 " in db", msoh);
			throw RDMA_DB_REM_FAIL;
		}

		INFO("msoh(0x%" PRIx64 ") removed from local database", msoh);
	} /* try */
	catch(int& e) {
		rc = e;
	} /* catch */

	DBG("EXIT");
	sem_post(&rdma_lock);

	return rc;
} /* rdma_close_mso_h() */

int rdma_destroy_ms_h_locked(mso_h msoh, ms_h msh);

int rdma_destroy_mso_h(mso_h msoh)
{
	auto rc = 0;

	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL msoh */
		if (!msoh) {
			WARN("msoh is NULL");
			throw RDMA_NULL_PARAM;
		}

		/* Check if msoh has already been destroyed. If so, success */
		if (!mso_h_exists(msoh)) {
			WARN("msoh no longer exists");
			throw RDMA_SUCCESS;
		}

		/* Get list of memory spaces owned by this owner */
		list<loc_ms *>	ms_list(get_num_ms_by_msoh(msoh));
		get_list_msh_by_msoh(msoh, ms_list);

		INFO("ms_list now has %d elements", ms_list.size());

		/* For each one of the memory spaces, call destroy */
		bool	ok = true;
		for_each(begin(ms_list),end(ms_list),
			[&ok, msoh](loc_ms *ms)
			{
				if (rdma_destroy_ms_h_locked(msoh, ms_h(ms))) {
					WARN("rdma_destroy_ms_h_locked failed: msoh = 0x%"
						PRIx64 ", msh = 0x%" PRIx64 "",
						msoh, ms_h(ms));
					ok = false;
				} else {
					DBG("msh(0x%" PRIx64 ") owned by msoh(0x%"
							PRIx64 ") destroyed",
							ms_h(ms), msoh);
				}
			}
		);

		/* Fail on any error */
		if (!ok) {
			ERR("Failed in destroy");
			throw RDMA_MS_DESTROY_FAIL;
		}

		/* Set up input parameters */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->destroy_mso_in.msoid = ((loc_mso *)msoh)->msoid;
		in_msg->type = DESTROY_MSO;
		in_msg->category = RDMA_REQ_RESP;

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc) {
			ERR("Failed in DESTROY_MSO daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to destroy mso? */
		if (out_msg.destroy_mso_out.status) {
			ERR("Failed to destroy msoid(0x%X), status = 0x%X",
					((loc_mso *)msoh)->msoid,
					out_msg.destroy_mso_out.status);
			throw out_msg.destroy_mso_out.status;
		}

		/* Remove from database */
		if (remove_loc_mso(msoh)) {
			CRIT("Failed to remove mso from database");
			throw RDMA_DB_REM_FAIL;
		}
	} /* try */
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	DBG("EXIT");
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

	DBG("ENTER with ms_name = '%s'", ms_name);
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
			ERR("String 'ms_name' is too long (%d)", len);
			throw RDMA_NAME_TOO_LONG;
		}

		/* Disallow creating a duplicate ms name */
		if (find_loc_ms_by_name(ms_name)) {
			WARN("A memory space named '%s' exists", ms_name);
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
		auto in_msg = make_unique<unix_msg_t>();
		unix_msg_t	out_msg;
		in_msg->type     = CREATE_MS;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->create_ms_in.bytes = *bytes;
		in_msg->create_ms_in.flags = flags;
		in_msg->create_ms_in.msoid = ((loc_mso *)msoh)->msoid;
		strcpy(in_msg->create_ms_in.ms_name, ms_name);

		/* Call into daemon */
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in CREATE_MS daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to create mso? */
		if (out_msg.create_ms_out.status) {
			ERR("Failed to create ms '%s' in daemon, status = 0x%X",
				ms_name, out_msg.create_ms_out.status);
			throw out_msg.create_ms_out.status;
		}

		/* Store in local database */
		*msh = add_loc_ms(ms_name,
				*bytes,
				msoh,
				out_msg.create_ms_out.msid,
				out_msg.create_ms_out.phys_addr,
				out_msg.create_ms_out.rio_addr,
				true);
		if (!*msh) {
			ERR("Failed to store ms in database");
			throw RDMA_DB_ADD_FAIL;
		}
		INFO("Stored info about '%s' in database as msh(0x%" PRIx64 ")",
								ms_name, *msh);
	} /* try */
	catch(int& e) {
		rc = e;
	}

	sem_post(&rdma_lock);

	return rc;
} /* rdma_create_ms_h() */

int rdma_destroy_msub_h_locked(ms_h msh, msub_h msubh);

int destroy_msubs_in_msh(ms_h msh)
{
	int rc;
	uint32_t 	msid = ((struct loc_ms *)msh)->msid;

	/* Get list of memory sub-spaces belonging to this msh */
	list<struct loc_msub *>	msub_list(get_num_loc_msub_in_ms(msid));
	get_list_loc_msub_in_msid(msid, msub_list);

	DBG("%d msub(s) in msh(0x%" PRIx64 "):", msub_list.size(), msh);

	/* For each one of the memory sub-spaces, call destroy */
	bool	ok = true;
	for_each(msub_list.begin(), msub_list.end(),
		[&ok, msh](loc_msub * msub)
		{
			if (rdma_destroy_msub_h_locked(msh, msub_h(msub))) {
				WARN("rdma_destroy_msub_h_locked failed: msh=0x%"
					PRIx64 ", msubh=0x%" PRIx64 "",
							msh, msub_h(msub));
				ok = false;
			}
		});

	/* Fail on any error */
	if (!ok) {
		ERR("Failure during destroying one of the msubs");
		rc = RDMA_MSUB_DESTROY_FAIL;
	} else {
		rc = 0;
	}

	return rc;
} /* destroy_msubs_in_msh */

int rdma_open_ms_h(const char *ms_name, mso_h msoh, uint32_t flags,
						uint32_t *bytes, ms_h *msh)
{
	auto rc = 0;

	DBG("ENTER, ms_name = %s", ms_name);
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
			ERR("String 'ms_name' is too long (%d)", len);

			throw RDMA_NAME_TOO_LONG;
		}

		/* If already open, just return the handle, and the size */
		*msh = find_loc_ms_by_name(ms_name);
		if (*msh != (ms_h)NULL) {
			INFO("'%s' already open by this app", ms_name);
			*bytes = ((loc_ms *)msh)->bytes;
			throw 0;	/* Not an error */
		}

		/* Set up Unix message parameters */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type     = OPEN_MS;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->open_ms_in.flags = flags;
		in_msg->open_ms_in.msoid = ((loc_mso *)msoh)->msoid;
		strcpy(in_msg->open_ms_in.ms_name, ms_name);

		/* Call into daemon */
		unix_msg_t	out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in OPEN_MS daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to open ms? */
		if (out_msg.open_ms_out.status) {
			ERR("Failed to open ms '%s' in daemon, status = 0x%X",
				ms_name, out_msg.open_ms_out.status);
			throw out_msg.open_ms_out.status;
		}

		INFO("Opened '%s' in the daemon", ms_name);

		/* Store memory space info in database */
		*msh = add_loc_ms(ms_name,
				out_msg.open_ms_out.bytes,
				msoh,
				out_msg.open_ms_out.msid,
				out_msg.open_ms_out.phys_addr,
				out_msg.open_ms_out.rio_addr,
				false);
		if (!*msh) {
			CRIT("Failed to store ms in database");
			throw RDMA_DB_ADD_FAIL;
		}
		INFO("Stored info about '%s' in database as msh(0x%" PRIx64 ")",
								ms_name, *msh);
		*bytes = out_msg.open_ms_out.bytes;
	}
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	return rc;
} /* rdma_open_ms_h() */

int rdma_close_ms_h_locked(mso_h msoh, ms_h msh)
{
	auto rc = 0;
	DBG("ENTER with msoh=0x%" PRIx64 ", msh = 0x%" PRIx64 "", msoh, msh);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!msoh || !msh) {
			ERR("Invalid param(s). Failing.");
			throw RDMA_NULL_PARAM;
		}

		/* Check if msh has already been closed (as a result of the owner
		 * destroying it and sending a close message to this user) */
		if (!loc_ms_exists(msh)) {
			/* This is NOT an error; the memory space was destroyed
			 * by its owner. Consider it closed by just warning and
			 * returning a success code */
			WARN("msh no longer exists");
			throw 0;
		}

		/* Destroy msubs opened under this msh */
		if (destroy_msubs_in_msh(msh)) {
			ERR("Failed to destroy msubs of msh(0x%" PRIx64 ")",
									msh);
			throw RDMA_MSUB_DESTROY_FAIL;
		}

		/* Set up Unix message parameters */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type     = CLOSE_MS;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->close_ms_in.msid = ((loc_ms *)msh)->msid;

		/* Call into daemon */
		unix_msg_t	out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in CLOSE_MS daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to close ms? */
		if (out_msg.close_ms_out.status) {
			ERR("Failed to close ms '%s' in daemon, status = 0x%X",
			((loc_ms *)msh)->name.c_str(), out_msg.close_ms_out.status);
			throw out_msg.close_ms_out.status;
		}

		INFO("Closed '%s' in the daemon", ((loc_ms *)msh)->name.c_str());

		/* Take it out of databse */
		if (remove_loc_ms(msh) < 0) {
			ERR("Failed to find msh(0x%" PRIx64 ") in db", msh);
			throw RDMA_DB_REM_FAIL;
		}
		INFO("msh(0x%" PRIx64 ") removed from local database", msh);
	}
	catch(int& e) {
		rc = e;
	}
	DBG("EXIT");
	return rc;
} /* rdma_close_ms_h_locked() */

int rdma_close_ms_h(mso_h msoh, ms_h msh)
{
	sem_wait(&rdma_lock);
	auto rc = rdma_close_ms_h_locked(msoh, msh);
	sem_post(&rdma_lock);

	return rc;
};

int rdma_destroy_ms_h_locked(mso_h msoh, ms_h msh)
{
	auto rc = 0;

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL parameters */
		if (!msoh || !msh) {
			ERR("Invalid param(s): msoh=0x%" PRIx64 ", msh=0x%" PRIx64
								"", msoh, msh);
			return RDMA_NULL_PARAM;
		}

		loc_ms *ms = (loc_ms *)msh;
		DBG("msid = 0x%X, ms_name = '%s', msh = 0x%" PRIx64 "",
						ms->msid, ms->name.c_str(), msh);
		/* Destroy msubs in this msh */
		if (destroy_msubs_in_msh(msh)) {
			ERR("Failed to destroy msubs belonging to msh(0x%" PRIx64 ")",
										msh);
			throw RDMA_MSUB_DESTROY_FAIL;
		}

		/* Set up input parameters */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type     = DESTROY_MS;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->destroy_ms_in.msid = ms->msid;
		in_msg->destroy_ms_in.msoid= ((loc_mso *)msoh)->msoid;

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in DESTROY_MS daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to destroy ms? */
		if (out_msg.destroy_ms_out.status) {
			ERR("Failed to destroy ms '%s' in daemon, status = 0x%X",
				ms->name.c_str(), out_msg.destroy_ms_out.status);
			throw out_msg.destroy_ms_out.status;
		}

		/* Memory space removed in daemon, remove from database as well */
		if (remove_loc_ms(msh) < 0) {
			WARN("Failed to remove 0x%" PRIx64 " from database", msh);
			dump_loc_ms();
			throw RDMA_DB_REM_FAIL;
		}

		INFO("msh(0x%" PRIx64 ") removed from local database", msh);
	}
	catch(int& e) {
		rc = e;
	}
	return rc;
} /* rdma_destroy_ms_h_locked() */

int rdma_destroy_ms_h(mso_h msoh, ms_h msh)
{
	DBG("ENTER");
	sem_wait(&rdma_lock);
	auto rc = rdma_destroy_ms_h_locked(msoh, msh);
	sem_post(&rdma_lock);
	DBG("EXIT");

	return rc;
};

int rdma_create_msub_h(ms_h	msh,
		       uint32_t	offset,
		       uint32_t	size,
		       uint32_t	flags,
		       msub_h	*msubh)
{
	(void)flags;	/* Disables warning until 'flags' have a use! */
	auto rc = 0;
	
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL */
		if (!msh || !msubh) {
			ERR("NULL param: msh=0x%" PRIx64 ", msubh=%p",
								msh, msubh);
			throw RDMA_NULL_PARAM;
		}

		/* In order for mmap() to work, the phys addr must be aligned
		 * at 4K. This means the mspace must be aligned, the msub
		 * must have a size that is a multiple of 4K, and the offset
		 * of the msub within the mspace must be a multiple of 4K. */
		if (!aligned_at_4k(offset) || !aligned_at_4k(size)) {
			ERR("Offset & size must be multiples of 4K");
			throw RDMA_ALIGN_ERROR;
		}

		/* Set up input parameters */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type     = CREATE_MSUB;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->create_msub_in.msid 	= ((loc_ms *)msh)->msid;
		in_msg->create_msub_in.offset	= offset;
		in_msg->create_msub_in.size 	= size;
		DBG("msid = 0x%X, offset = 0x%X, req_bytes = 0x%x",
						in_msg->create_msub_in.msid,
						in_msg->create_msub_in.offset,
						in_msg->create_msub_in.size);
		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in CREATE_MSUB daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to create msub? */
		if (out_msg.create_msub_out.status) {
			ERR("Failed to create_msub in daemon, status = 0x%X",
				out_msg.create_msub_out.status);
			throw out_msg.create_msub_out.status;
		}

		INFO("out.phys_addr=0x%016" PRIx64 ", out.rio_addr=0x%016" PRIx64 "",
					out_msg.create_msub_out.phys_addr,
					out_msg.create_msub_out.rio_addr);

		/* Store msubh in database, obtain pointer thereto, convert to msub_h */
		*msubh = (msub_h)add_loc_msub(out_msg.create_msub_out.msubid,
				      	      out_msg.create_msub_in.msid,
				      	      size,
				      	      64,	/* 64-bit RIO address */
				      	      out_msg.create_msub_out.rio_addr,
				      	      0,	/* Bits 66 and 65 */
				      	      out_msg.create_msub_out.phys_addr);

		/* Adding to database should never fail, but just in case... */
		if (!*msubh) {
			WARN("Failed to add msub to database");
			throw RDMA_DB_ADD_FAIL;
		}
	
		DBG("msubh = 0x%" PRIx64 "", msubh);
	} /* try */
	catch(int& e) {
		rc = e;
	} /* catch */
	sem_post(&rdma_lock);
	return rc;
} /* rdma_create_msub_h() */

int rdma_destroy_msub_h_locked(ms_h msh, msub_h msubh)
{
	int rc;

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for NULL handles */
		if (!msh || !msubh) {
			ERR("Invalid param(s):msh=0x%" PRIx64 ",msubh=0x%" PRIx64 "",
								msh, msubh);
			throw RDMA_NULL_PARAM;
		}

		DBG("msubh = 0x%" PRIx64 "", msubh);

		/* Set up input parameters */
		auto in_msg = make_unique<unix_msg_t>();

		in_msg->type     = DESTROY_MSUB;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->destroy_msub_in.msid = ((loc_ms *)msh)->msid;
		in_msg->destroy_msub_in.msubid = ((loc_msub *)msubh)->msubid;
		DBG("Attempting to destroy msubid(0x%X) in msid(0x%X)",
					in_msg->destroy_msub_in.msid,
					in_msg->destroy_msub_in.msubid);

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in DESTROY_MSUB daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to destroy msub? */
		if (out_msg.destroy_msub_out.status) {
			ERR("Failed to destroy_msub_out in daemon, status = 0x%X",
				out_msg.destroy_msub_out.status);
			throw out_msg.destroy_msub_out.status;
		}

		/* Remove msub from database */
		if (remove_loc_msub(msubh) < 0) {
			WARN("Failed to remove %p from database", msubh);
			throw RDMA_DB_REM_FAIL;
		}

		DBG("Destroyed msub in msh = 0x%" PRIx64 ", msubh = 0x%"
						PRIx64 "", msh, msubh);
	}
	catch(int& e) {
		rc = e;
	}
	return rc;
} /* rdma_destroy_msub_h_locked() */

int rdma_destroy_msub_h(ms_h msh, msub_h msubh)
{
	DBG("ENTER");
	sem_wait(&rdma_lock);
	auto rc = rdma_destroy_msub_h_locked(msh, msubh);
	sem_post(&rdma_lock);
	DBG("EXIT");

	return rc;
};

int rdma_mmap_msub(msub_h msubh, void **vaddr)
{
	loc_msub *pmsub = (loc_msub *)msubh;
	int rc;
	
	sem_wait(&rdma_lock);

	try {
		if (!pmsub) {
			ERR("msubh is NULL");
			throw RDMA_NULL_PARAM;
		}

		if (!vaddr) {
			ERR("vaddr is NULL");
			throw RDMA_NULL_PARAM;
		}

		/* Check the daemon hasn't died since we established
		 * its socket connection */

		if (!rdmad_is_alive()) {
			WARN("Local RDMA daemon has died.");
		}

		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		rc = riomp_dma_map_memory(peer.mport_hnd,
					  pmsub->bytes,
					  pmsub->paddr,
					  vaddr);
		if (rc) {
			ERR("map(0x%" PRIx64 ") failed: %s",
						pmsub->paddr, strerror(-rc));
			throw rc;
		}
		INFO("phys_addr = 0x%" PRIx64 ", virt_addr = %p, size = 0x%x",
						pmsub->paddr, *vaddr, pmsub->bytes);
	} /* try */
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	return rc;
} /* rdma_mmap_msub() */

int rdma_munmap_msub(msub_h msubh, void *vaddr)
{
	loc_msub *pmsub = (loc_msub *)msubh;
	int rc;

	sem_wait(&rdma_lock);
	try {
		if (!pmsub) {
			ERR("msubh is NULL");
			throw RDMA_NULL_PARAM;
		}

		if (!vaddr) {
			ERR("vaddr is NULL");
			throw RDMA_NULL_PARAM;
		}

		/* Check the daemon hasn't died since connection established */
		if (!rdmad_is_alive()) {
			WARN("Local RDMA daemon has died.");
		}

		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		if (munmap(vaddr, pmsub->bytes) == -1) {
			ERR("munmap(): %s", strerror(errno));
			throw RDMA_UNMAP_ERROR;
		}

		INFO("Unmapped vaddr(%p), of size %u", vaddr, pmsub->bytes);
		rc = 0;
	}
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);

	return rc;
} /* rdma_unmap_msub() */

/**
 * If it fails due to timeout, return value is ETIMEDOUT
 */
int rdma_accept_ms_h(ms_h loc_msh,
		     msub_h loc_msubh,
		     conn_h *connh,
		     msub_h *rem_msubh,
		     uint32_t *rem_msub_len,
		     uint64_t timeout_secs)
{
	int rc;

	DBG("ENTER with timeout = %u", (unsigned)timeout_secs);
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check that parameters are not NULL. However, allow rem_msubh
		 * and rem_msub_len to be NULL. If the server knows the client
		 * will not be providing an msub then it can pass NULL here */
		if (!loc_msh || !loc_msubh) {
			ERR("loc_msh=0x%" PRIx64 ",loc_msubh=0x%" PRIx64 "",
				loc_msh, loc_msubh);
			throw RDMA_NULL_PARAM;
		}

		/* Ensure timeout is > 0 or things will fail */
		if (timeout_secs == 0) {
			ERR("Timeout cannot be 0");
			throw RDMA_NULL_PARAM;
		}

		loc_ms	 *server_ms = (loc_ms *)loc_msh;
		loc_msub *server_msub = (loc_msub *)loc_msubh;

		/* Tell the daemon to flag this memory space as accepting
		 * connections for this application. */
		auto accept_in_msg = make_unique<unix_msg_t>();
		accept_in_msg->type     = ACCEPT_MS;
		accept_in_msg->category = RDMA_REQ_RESP;
		accept_in_msg->accept_in.server_msid = server_ms->msid;
		accept_in_msg->accept_in.server_msubid
						= server_msub->msubid;

		DBG("name = '%s'", server_ms->name.c_str());

		/* Set notification for CONNECT_MS_REQ */
		auto reply_sem = make_shared<sem_t>();
		sem_init(reply_sem.get(), 0, 0);
		rx_eng->set_notify(CONNECT_MS_REQ, RDMA_CALL , 0, reply_sem);

		/* Call into daemon */
		unix_msg_t  accept_out_msg;
		rc = daemon_call(move(accept_in_msg), &accept_out_msg);
		if (rc ) {
			ERR("Failed in ACCEPT_MS daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed in daemon? */
		if (accept_out_msg.accept_out.status) {
			ERR("Failed to accept (ms) in daemon, status = 0x%X",
				accept_out_msg.accept_out.status);
			throw accept_out_msg.accept_out.status;
		}

		/* Await CONNECT_MS_REQ */
		struct timespec timeout;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += timeout_secs;
		rc = sem_timedwait(reply_sem.get(), &timeout);
		if (rc) {
			ERR("reply_sem failed: %s", strerror(errno));
			if (timeout_secs && errno == ETIMEDOUT) {
				ERR("Timeout occurred");
			} else {
				ERR("UNKNOWN ERROR");
			}
			ERR("Failed to receive CONNECT_MS_REQ for '%s'",
					server_ms->name.c_str());

			/* Switch back the ms to non-accepting mode */
			auto undo_accept_in_msg = make_unique<unix_msg_t>();
			undo_accept_in_msg->type     = UNDO_ACCEPT;
			undo_accept_in_msg->category = RDMA_REQ_RESP;
			undo_accept_in_msg->accept_in.server_msid
							= server_ms->msid;
			/* Call into daemon */
			unix_msg_t  undo_accept_out_msg;
			rc = daemon_call(move(undo_accept_in_msg), &undo_accept_out_msg);
			if (rc ) {
				ERR("Failed in UNDO_ACCEPT daemon_call, rc = 0x%X", rc);
				throw rc;
			}

			/* Failed in daemon? */
			if (undo_accept_out_msg.undo_accept_out.status) {
				ERR("Failed to undo accept in daemon, status = 0x%X",
					undo_accept_out_msg.undo_accept_out.status);
				throw undo_accept_out_msg.undo_accept_out.status;
			}
			throw RDMA_ACCEPT_TIMEOUT;
		}

		/* Notification arrived fine; read the CONNECT_MS_REQ message */
		unix_msg_t connect_ms_req_msg;

		rc = rx_eng->get_message(CONNECT_MS_REQ,
				   RDMA_CALL,
				   0,
				   &connect_ms_req_msg);
		if (rc) {
			CRIT("Message disappeared after notification!!!");
			throw RDMA_ACCEPT_FAIL;
		}

		/* Shorter form */
		connect_to_ms_req_input *conn_req_msg
					= &connect_ms_req_msg.connect_to_ms_req;

		DBG("CONNECT_MS_REQ message received from daemon:");
		conn_req_msg->dump();
		*connh = conn_req_msg->connh;	/* Conn. handle sent by client */

		/* Now reply to the CONNECT_MS_REQ sith CONNECT_MS_RESP */
		auto connect_ms_resp_in_msg = make_unique<unix_msg_t>();
		connect_to_ms_resp_input *conn_to_ms_resp =
				&connect_ms_resp_in_msg->connect_to_ms_resp_in;
		connect_ms_resp_in_msg->type = CONNECT_MS_RESP;
		connect_ms_resp_in_msg->category = RDMA_REQ_RESP;
		connect_ms_resp_in_msg->seq_no   = conn_req_msg->seq_num;
		conn_to_ms_resp->client_msid    = conn_req_msg->client_msid;
		conn_to_ms_resp->client_msubid	= conn_req_msg->client_msubid;
		conn_to_ms_resp->server_msid = server_ms->msid;
		conn_to_ms_resp->server_msubid  = server_msub->msubid;
		conn_to_ms_resp->server_msub_bytes = server_msub->bytes;
		conn_to_ms_resp->server_rio_addr_len = server_msub->rio_addr_len;
		conn_to_ms_resp->server_rio_addr_lo = server_msub->rio_addr_lo;
		conn_to_ms_resp->server_rio_addr_hi = server_msub->rio_addr_hi;
		conn_to_ms_resp->client_destid_len =
						conn_req_msg->client_destid_len;
		conn_to_ms_resp->client_destid = conn_req_msg->client_destid;
		conn_to_ms_resp->client_to_lib_tx_eng_h =
					conn_req_msg->client_to_lib_tx_eng_h;

		DBG("CONNECT_MS_RESP to server daemon");
		conn_to_ms_resp->dump();

		/* Call into daemon */
		unix_msg_t connect_ms_resp_out_msg;
		rc = daemon_call(move(connect_ms_resp_in_msg), &connect_ms_resp_out_msg);
		if (rc ) {
			ERR("Failed in CONNECT_MS_RESP daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed in daemon? */
		if (connect_ms_resp_out_msg.connect_to_ms_resp_out.status) {
			ERR("Failed to CONNECT_MS_RESP (ms) in daemon, status = 0x%X",
				connect_ms_resp_out_msg.connect_to_ms_resp_out.status);
			throw connect_ms_resp_out_msg.connect_to_ms_resp_out.status;
		}

		/* Store connection handle with server_msub */
		DBG("Storing connh = 0x%" PRIx64 ", in server_msub's database entry",
								*connh);
		server_msub->connections.emplace_back(*connh,
					conn_req_msg->client_to_lib_tx_eng_h);

		/* Store info about client msub in database and return handle,
		 * but only if the client msub is NOT NULL_MSUBID */

		if (conn_req_msg->client_msubid != NULL_MSUBID) {
			if (!rem_msubh || !rem_msub_len) {
				ERR("Client provided an msub, but rem_msubh and/or rem_msub_len is NULL");
				throw RDMA_NULL_PARAM;
			}
			*rem_msubh = (msub_h)add_rem_msub(
					  conn_req_msg->client_msubid,
					  conn_req_msg->client_msid,
					  conn_req_msg->client_msub_bytes,
					  conn_req_msg->client_rio_addr_len,
					  conn_req_msg->client_rio_addr_lo,
					  conn_req_msg->client_rio_addr_hi,
					  conn_req_msg->client_destid_len,
					  conn_req_msg->client_destid,
					  loc_msh);
			if (*rem_msubh == (msub_h)NULL) {
				WARN("Failed to add rem_msub to database");
				throw RDMA_DB_ADD_FAIL;
			}
			INFO("rem_bytes = %d, rio_addr = 0x%" PRIx64 "",
					conn_req_msg->client_msub_bytes,
					conn_req_msg->client_rio_addr_lo);

			/* Return remote msub length to application */
			*rem_msub_len = conn_req_msg->client_msub_bytes;
		}
	}
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	return rc;
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
static struct timespec time_difference( struct timespec start,
					struct timespec end )
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
		   conn_h *connh,
		   msub_h *rem_msubh,
		   uint32_t *rem_msub_len,
		   ms_h	  *rem_msh,
		   uint64_t timeout_secs)
{
	static uint32_t seq_num = 0x1234;
	int rc;

	DBG("ENTER");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check for null parameters */
		if (!rem_msubh || !rem_msname || !rem_msub_len || !rem_msh) {
			ERR("NULL parameter(s) passed.");
			ERR("rem_msubh=%p,rem_msname=%p,rem_msub_len=%p,rem_msh=%p",
				rem_msubh, rem_msname, rem_msub_len, rem_msh);
			throw RDMA_NULL_PARAM;
		}

		/* Ensure timeout is > 0 or things will fail */
		if (timeout_secs == 0) {
			ERR("Timeout cannot be 0");
			throw RDMA_NULL_PARAM;
		}

		/* Check for invalid destid */
		if (rem_destid_len == 16 && rem_destid==0xFFFF) {
			WARN("Invalid destid 0x%X", rem_destid);
			throw RDMA_INVALID_DESTID;
		}

		/* Prevent buffer overflow due to very long name */
		size_t len = strlen(rem_msname);
		if (len > UNIX_MS_NAME_MAX_LEN) {
			ERR("String 'ms_name' is too long (%d)", len);
			throw RDMA_NAME_TOO_LONG;
		}

		INFO("Connecting to '%s' on destid(0x%X)", rem_msname, rem_destid);

		/* Use the client LIBRDMA tx_eng as the connection handle to the ms */
		*connh = (conn_h)tx_eng.get();

		/* Set up parameters for daemon call */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type 	= SEND_CONNECT;
		in_msg->category = RDMA_REQ_RESP;
		send_connect_input *connect_msg = &in_msg->send_connect_in;

		strcpy(connect_msg->server_msname, rem_msname);
		connect_msg->server_destid_len	= rem_destid_len;
		connect_msg->server_destid 	= rem_destid;
		connect_msg->client_destid_len 	= 16;
		connect_msg->client_destid	= peer.destid;
		connect_msg->seq_num		= seq_num++;
		connect_msg->connh		= *connh;

		loc_msub *client_msub = nullptr;
		if (loc_msubh != 0) {
			client_msub = (loc_msub *)loc_msubh;

			connect_msg->client_msid 	 = client_msub->msid;
			connect_msg->client_msubid	 = client_msub->msubid;
			connect_msg->client_bytes	 = client_msub->bytes;
			connect_msg->client_rio_addr_len = client_msub->rio_addr_len;
			connect_msg->client_rio_addr_lo  = client_msub->rio_addr_lo;
			connect_msg->client_rio_addr_hi  = client_msub->rio_addr_hi;
		} else {
			HIGH("Client has provided a 0 msubh");
			connect_msg->client_msubid = NULL_MSUBID;
			/* Populate the fields with 0s instead of garbage */
			connect_msg->client_msid = 0;
			connect_msg->client_bytes = 0;
			connect_msg->client_rio_addr_len = 0;
			connect_msg->client_rio_addr_lo = 0;
			connect_msg->client_rio_addr_hi = 0;
		}

		DBG("Contents of SEND_CONNECT message to daemon:");
		connect_msg->dump();

		struct timespec	before, after, rtt;

		/* Mark the time before sending the 'connect' */
		__sync_synchronize();
		clock_gettime( CLOCK_MONOTONIC, &before );
		__sync_synchronize();

		/* Prepare for reply */
		auto reply_sem = make_shared<sem_t>();
		sem_init(reply_sem.get(), 0, 0);
		rx_eng->set_notify(ACCEPT_FROM_MS_REQ, RDMA_CALL, 0, reply_sem);

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in SEND_CONNECT daemon_call, rc = 0x%X", rc);
			throw rc;
		}

		/* Failed to send connect? */
		if (out_msg.send_connect_out.status) {
			ERR("Failed to connect in daemon, status = 0x%X",
					out_msg.send_connect_out.status);
			throw out_msg.send_connect_out.status;
		}

		INFO(" Waiting for connect response (accept) message...");
		struct timespec timeout;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += timeout_secs;
		rc = sem_timedwait(reply_sem.get(), &timeout);
		if (rc) {
			ERR("reply_sem failed: %s", strerror(errno));
			if (timeout_secs && errno == ETIMEDOUT) {
				ERR("Timeout occurred");
				rc = ETIMEDOUT;
			}
		} else {
			DBG("Got reply!");
			rc = rx_eng->get_message(ACCEPT_FROM_MS_REQ, RDMA_CALL, 0, &out_msg);
			if (rc) {
				ERR("Failed to obtain reply message, rc = %d", rc);
			}
		}
		if (rc || out_msg.sub_type == ACCEPT_FROM_MS_REQ_NACK) {
			if (rc == ETIMEDOUT) {
				ERR("Timeout before getting response to 'connect'");
				rc = RDMA_CONNECT_TIMEOUT;
			} else if(out_msg.sub_type == ACCEPT_FROM_MS_REQ_NACK) {
				ERR("Connection rejected by remote daemon/server");
				rc = RDMA_CONNECT_FAIL;
			} else {
				ERR("Unknown failure");
			}
			/* We failed out, so undo the accept */
			auto undo_in_msg = make_unique<unix_msg_t>();
			undo_in_msg->type     = UNDO_CONNECT;
			undo_in_msg->category = RDMA_REQ_RESP;
			strcpy(undo_in_msg->undo_connect_in.server_ms_name, rem_msname);

			auto temp_rc = rc;	/* Save 'rc' */
			rc = daemon_call(move(undo_in_msg), &out_msg);
			if (rc ) {
				ERR("Failed in UNDO_CONNECT daemon_call, rc = %d", rc);
				throw rc;
			}

			/* Failed to undo the accept */
			if (out_msg.undo_connect_out.status) {
				ERR("Failed to undo acceptin daemon, status=0x%X",
					out_msg.undo_connect_out.status);
				throw out_msg.undo_connect_out.status;
			}
			rc = temp_rc;		/* Restore 'rc' */
			throw rc;
		}

		/* Compute round-trip time between issuing 'connect' and
		 * getting a 'connect response' ('accept') */
		__sync_synchronize();
		clock_gettime(CLOCK_MONOTONIC, &after);
		__sync_synchronize();
		rtt = time_difference(before, after);
		HIGH("Round-trip-time for accept/connect = %u seconds and %u microseconds",
							rtt.tv_sec, rtt.tv_nsec/1000);

		/* Point to the accept message */
		accept_from_ms_req_input *accept_msg = &out_msg.accept_from_ms_req_in;

		/* Some validation of the 'accept' (response-to-connect) message */
		if (accept_msg->server_destid != rem_destid) {
			WARN("WRONG destid(0x%X) in accept message!",
							accept_msg->server_destid);
		}

		/* Store info about remote msub in database and return handle */
		uint32_t client_msid;
		if (client_msub != nullptr) {
			client_msid = client_msub->msid;
		} else {
			client_msid = NULL_MSID;
		}
		*rem_msubh = (msub_h)add_rem_msub(accept_msg->server_msubid,
						accept_msg->server_msid,
						accept_msg->server_msub_bytes,
						accept_msg->server_rio_addr_len,
						accept_msg->server_rio_addr_lo,
						accept_msg->server_rio_addr_hi,
						accept_msg->server_destid_len,
						accept_msg->server_destid,
						client_msid);
		if (*rem_msubh == (msub_h)NULL) {
			throw RDMA_DB_ADD_FAIL;
		}
		INFO("Remote msubh has size %d, rio_addr = 0x%016" PRIx64 "",
			accept_msg->server_msub_bytes, accept_msg->server_rio_addr_lo);
		INFO("rem_msub has destid = 0x%X, destid_len = 0x%X",
			accept_msg->server_destid, accept_msg->server_destid_len);

		/* Remote memory subspace length */
		*rem_msub_len = accept_msg->server_msub_bytes;

		/* Save server_msid since we need to store that in the databse */
		uint32_t server_msid = accept_msg->server_msid;

		/* Remote memory space handle */
		*rem_msh = (ms_h)add_rem_ms(rem_msname, server_msid);
		if (*rem_msh == (ms_h)NULL) {
			ERR("Failed to store rem_ms in database");
			throw RDMA_DB_ADD_FAIL;
		} else {
			DBG("Entry for '%s' stored in DB, handle = 0x%016" PRIx64 "",
							rem_msname, *rem_msh);
		}
	} /* try */
	catch(int& e) {
		rc = e;
		ERR("Exiting due to failure. rc = 0x%X", rc);
	}
	INFO("EXIT");
	sem_post(&rdma_lock);
	return rc;
} /* rdma_conn_ms_h() */

int client_disc_ms_h(conn_h connh, ms_h server_msh, msub_h client_msubh,
		     uint64_t timeout_secs)
{
	int rc;

	(void)connh;
	DBG("ENTER");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check that server_msh is not NULL. (client_msubh CAN be NULL) */
		if (!server_msh) {
			ERR("server_msh is NULL");
			throw RDMA_NULL_PARAM;
		}

		/* If not in database, so it was destroyed. Not an error */
		if (!rem_ms_exists(server_msh)) {
			WARN("rem_msh(0x%" PRIx64 ") not in database.", server_msh);
			throw 0;	/* Not an error */
		}

		rem_ms *server_ms = (rem_ms *)server_msh;
		DBG("rem_msh exists and has msid(0x%X), rem_msh(%" PRIx64 ")",
						server_ms->msid, server_msh);

		/* Check that we indeed have remote msubs belonging to that server_msh.
		 * If we don't that is a serious problem because:
		 * 1. Even if we don't provide an msub, the server must provide one.
		 * 2. If we did provide an msub, we cannot tell the server to remove it
		 *    unless we know the server's destid. That destid is in the locally
		 *    stored remote msub. */
		/* NOTE: assumption is that there is only ONE msub provided to the remote
		 * memory space per connection . */
		rem_msub *msubp =
			(rem_msub *)find_any_rem_msub_in_ms(server_ms->msid);
		if (!msubp) {
			CRIT("No msubs for server_msh(0x%" PRIx64 "). IMPOSSIBLE",
									server_msh);
			throw RDMA_INVALID_MS;
		}

		auto in_msg = make_unique<unix_msg_t>();

		in_msg->type 	= SEND_DISCONNECT;
		in_msg->category = RDMA_REQ_RESP;

		/* Get destid info BEFORE we delete the msub. Otherwise we don't
		 * know where that remote msub is (we could make it an input parameter
		 * to the function, but the msubh should have it!). */
		in_msg->send_disconnect_in.server_destid_len = msubp->destid_len;
		in_msg->send_disconnect_in.server_destid     = msubp->destid;

		/* Send the MSID of the remote memory space we wish to disconnect from;
		 * this will be used by the SERVER daemon to remove CLIENT destid from
		 * that memory space's object */
		in_msg->send_disconnect_in.server_msid	 = server_ms->msid;

		/* If we had provided a local msub to the server, we need to
		 * tell the server its msubid so the server can delete the msub
		 * from tis remote databse. Otherwise send a NULL_MSUBID */
		if (client_msubh != 0)
			in_msg->send_disconnect_in.client_msubid =
						((loc_msub *)client_msubh)->msubid;
		else
			in_msg->send_disconnect_in.client_msubid = NULL_MSUBID;

		/**
		 * NOTE: Set the notification for DISCONNECT_MS_ACK. This way
		 * the message won't be discarded if it arrives promplty */
		auto reply_sem = make_shared<sem_t>();
		sem_init(reply_sem.get(), 0, 0);
		rx_eng->set_notify(DISCONNECT_MS_ACK, RDMA_CALL, 0, reply_sem);

		/* Call into daemon */
		unix_msg_t  out_msg;
		rc = daemon_call(move(in_msg), &out_msg);
		if (rc ) {
			ERR("Failed in SEND_DISCONNECT daemon_call, rc=0x%X",
									rc);
			throw rc;
		}

		/* Failed to send disconnect? */
		if (out_msg.send_disconnect_out.status) {
			ERR("Failed to send_disconnect in daemon, status=0x%X",
					out_msg.send_disconnect_out.status);
			throw out_msg.send_disconnect_out.status;
		}

		struct timespec timeout;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += timeout_secs;
		rc = sem_timedwait(reply_sem.get(), &timeout);
		if (rc) {
			ERR("reply_sem failed: %s", strerror(errno));
			if (errno == ETIMEDOUT) {
				ERR("Timeout occurred");
				/* Don't throw an exception here since we
				 * still need to clean up the database. */
				rc = RDMA_DISCONNECT_TIMEOUT;
			}
		}

		/* Remove all remote msubs belonging to 'rem_msh'. At this point,
		 * "there can be only one". */
		remove_rem_msubs_in_ms(server_ms->msid);
		DBG("Removed msub(s) connected to msid(0x%X)", server_ms->msid);

		/* Remove entry for the remote memory space from the database */
		if (remove_rem_ms(server_msh)) {
			ERR("Failed to remove remote msid(0x%X) from database",
								server_ms->msid);
			/* If we already had an RDMA_DISCONNECT_TIMEOUT error, then
			 * don't throw anything since rc == RDMA_DISCONNECT_TIMEOUT
			 * which is the higher priority error. Otherwise indicate
			 * if there are database update problems. */
			if (rc == 0)
				throw RDMA_DB_REM_FAIL;
		}
	} /* try */
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	return rc;
} /* client_disc_ms_h() */

int server_disc_ms_h(conn_h connh, ms_h server_msh, msub_h client_msubh)
{
	int rc;

	DBG("ENTER");
	sem_wait(&rdma_lock);

	try {
		/* Check that library has been initialized */
		LIB_INIT_CHECK(rc);

		/* Check that server_msh is not NULL. (client_msubh CAN be NULL) */
		if (!server_msh) {
			ERR("server_msh is NULL");
			throw RDMA_NULL_PARAM;
		}

		/* Get client msubid, if not null */
		uint32_t client_msubid = NULL_MSUBID;
		if (client_msubh) {
			client_msubid = ((rem_msub *)client_msubh)->msubid;
		}

		/* Get server_msid */
		uint32_t server_msid = ((loc_ms *)server_msh)->msid;

		/* Get server_msubid */
		msub_h server_msubh = find_loc_msub_by_connh(connh);
		if (!server_msubh) {
			ERR("Invalid connh: %" PRIx64
				". No related server msubs", connh);
			throw -1;
		}
		loc_msub *server_msub = (loc_msub *)server_msubh;
		uint32_t server_msubid = server_msub->msubid;

		/* Locate connection in the msub */
		auto connection_it = find_if(begin(server_msub->connections),
				 	     end(server_msub->connections),
				 	     [connh](client_connection& c)
				 	     { return c.connh == connh; });
		if (connection_it == end(server_msub->connections)) {
			ERR("Invalid connh. Not found in server_msub");
			throw -2;
		}

		/* Prepare for SERVER_DISCONNECT_MS_ACK reception BEFORE
		 * sending SERVER_DISCONNECT_MS	 */
		auto reply_sem = make_shared<sem_t>();
		sem_init(reply_sem.get(), 0, 0);
		rx_eng->set_notify(SERVER_DISCONNECT_MS_ACK, RDMA_CALL, 0, reply_sem);

		/* Tell daemon to relay to remote client that we disconnected
		 * them from the memory space.  */
		auto in_msg = make_unique<unix_msg_t>();
		in_msg->type 	= SERVER_DISCONNECT_MS;
		in_msg->category = RDMA_REQ_RESP;
		in_msg->server_disconnect_ms_in.client_to_lib_tx_eng_h =
					connection_it->client_to_lib_tx_eng_h;
		in_msg->server_disconnect_ms_in.server_msid = server_msid;
		in_msg->server_disconnect_ms_in.server_msubid = server_msubid;
		in_msg->server_disconnect_ms_in.client_msubid = client_msubid;
		tx_eng->send_message(move(in_msg));

		/* Now wait for a SERVER_DISCONNECT_MS_ACK */
		INFO(" Waiting for SERVER_DISCONNECT_MS_ACK ...");
		struct timespec timeout;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += 5;	/* 5 second timeout */
		rc = sem_timedwait(reply_sem.get(), &timeout);
		if (rc) {
			ERR("reply_sem failed: %s", strerror(errno));
			if (errno == ETIMEDOUT) {
				ERR("Timeout occurred");
				throw RDMA_DISCONNECT_TIMEOUT;
			}
		} else {
			DBG("Got SERVER_DISCONNECT_MS_ACK");
		}

		/* Remove connection from server msub database entry */
		server_msub->connections.erase(connection_it);

		/* Remove client msub from database, if applicable */
		if (client_msubh) {
			if (remove_rem_msub(client_msubh) != 0) {
				ERR("Failed to remove client msub");
				throw RDMA_DB_REM_FAIL;
			}
		}
	} /* try */
	catch(int& e) {
		rc = e;
	}
	sem_post(&rdma_lock);
	DBG("EXIT");

	return rc;
} /* server_disc_ms_h() */

int rdma_disc_ms_h(conn_h connh, ms_h server_msh, msub_h client_msubh)
{
	int rc;
	if (connh == (conn_h)tx_eng.get()) {
		HIGH("CLIENT DISCONNECTING");
		rc = client_disc_ms_h(connh, server_msh, client_msubh, 5);
	} else {
		HIGH("SERVER DISCONNECTING");
		rc = server_disc_ms_h(connh, server_msh, client_msubh);
	}
	return rc;
} /* rdma_disc_ms_h() */

constexpr bool PUSH_MODE = true;
constexpr bool PULL_MODE = false;

/**
 * @brief Common DMA code used by both the push_ & pull_msub RDMA functions
 */
static int dma_push_pull_msub(bool is_push,
			      const struct rdma_xfer_ms_in *in,
			      struct rdma_xfer_ms_out *out)
{
	int rc;

	loc_msub *lmsub;
	rem_msub *rmsub;

	try {
		/* Check for NULL pointers */
		if (!in || !out) {
			ERR("%s: NULL. in=%p, out=%p", in, out);
			out->in_param_ok = RDMA_NULL_PARAM;
			throw RDMA_NULL_PARAM;
		}

		lmsub = (loc_msub *)in->loc_msubh;
		rmsub = (rem_msub *)in->rem_msubh;
		if (!lmsub || !rmsub) {
			ERR("%s: NULL. lmsub=%p, rmsub=%p", lmsub, rmsub);
			out->in_param_ok = RDMA_NULL_PARAM;
			throw RDMA_NULL_PARAM;
		}

		/* Check for destid > 16 */
		if (rmsub->destid_len > 16) {
			ERR("destid_len=%u unsupported", rmsub->destid_len);
			out->in_param_ok = RDMA_INVALID_DESTID;
			throw RDMA_INVALID_DESTID;
		}

		/* Check for RIO address > 64-bits */
		if (rmsub->rio_addr_len > 64) {
			ERR("rio_addr_len=%u unsupported", rmsub->rio_addr_len);
			out->in_param_ok = RDMA_INVALID_RIO_ADDR;
			throw RDMA_INVALID_RIO_ADDR;
		}

		/* Check if local daemon is alive */

		DBG("Check if local daemon is alive");
		if (!rdmad_is_alive()) {
			ERR("Local RDMA daemon is dead. Exiting");
			throw RDMA_DAEMON_UNREACHABLE;
		}

		/* Check if remote daemon is alive */
		/*
		DBG("Check if remote daemon is alive");
		if (fm_alive && (dd_h != NULL))
			if (!fmdd_check_did(dd_h, rmsub->destid, FMDD_RDMA_FLAG)) {
				ERR("Remote destination daemon NOT running!");
				throw RDMA_REMOTE_UNREACHABLE;
			}
		*/

		/* Determine sync type */
		enum riomp_dma_directio_transfer_sync rd_sync;
		switch (in->sync_type) {

		case rdma_no_wait:
			rd_sync = RIO_DIRECTIO_TRANSFER_FAF;
			INFO("RIO_DIRECTIO_TRANSFER_FAF");
		break;
		case rdma_sync_chk:
			rd_sync = RIO_DIRECTIO_TRANSFER_SYNC;
			INFO("RIO_DIRECTIO_TRANSFER_SYNC");
		break;
		case rdma_async_chk:
			rd_sync = RIO_DIRECTIO_TRANSFER_ASYNC;
			INFO("RIO_DIRECTIO_TRANSFER_ASYNC");
		break;
		default:
			ERR("Invalid sync_type", in->sync_type);
			out->in_param_ok = RDMA_INVALID_SYNC_TYPE;
			throw RDMA_INVALID_SYNC_TYPE;
		}

		/* All input parameters are OK */
		out->in_param_ok = rc = 0;

		if (is_push) {
			/* Echo parameters for debugging */
			INFO("Sending %u bytes over DMA to destid=0x%X",
						in->num_bytes, rmsub->destid);
			INFO("Dest RIO addr =  %016" PRIx64
					", lmsub->paddr = %016" PRIx64 "",
				rmsub->rio_addr_lo + in->rem_offset,
				lmsub->paddr);

			do {
				rc = riomp_dma_write_d(peer.mport_hnd,
				    (uint16_t)rmsub->destid,
				    rmsub->rio_addr_lo + in->rem_offset,
				    lmsub->paddr,
				    in->loc_offset,
				    in->num_bytes,
					RIO_DIRECTIO_TYPE_NWRITE_R,
				    rd_sync);
			} while (rc && ((EINTR == errno) || (EBUSY == errno)));

			if (rc < 0) {
				ERR("riomp_dma_write_d() failed:(%d) %s",
						rc, strerror(errno));
			}
		} else { /* is pull */

			INFO("Receiving %u bytes over DMA from destid=0x%X",
						in->num_bytes, rmsub->destid);
			INFO("Source RIO addr = 0x%X, lmsub->paddr = 0x%" PRIx64 "",
						rmsub->rio_addr_lo + in->rem_offset,
						lmsub->paddr);

			do {
				rc = riomp_dma_read_d(peer.mport_hnd,
					(uint16_t)rmsub->destid,
					rmsub->rio_addr_lo + in->rem_offset,
					lmsub->paddr,
					in->loc_offset,
					in->num_bytes,
					rd_sync);
			} while (rc && ((EINTR == errno) || (EBUSY == errno)));

			if (rc < 0) {
				ERR("riomp_dma_read_d() failed:(%d) %s",
						rc, strerror(rc));
			}
		}
		/* If synchronous, the return value is the xfer status. If async,
		 * the return value of riomp_dma_read_d() is the token (if >= 0) */
		if (in->sync_type == rdma_sync_chk)
			out->dma_xfr_status = rc;
		else if (in->sync_type == rdma_async_chk && rc >= 0) {
			out->chk_handle = rc; /* token */
			rc = 0; /* success */
		}
	}
	catch(int& e) {
		rc = e;
	}

	return rc;
} /* dma_push_pull_msub() */



int rdma_push_msub(const struct rdma_xfer_ms_in *in,
		   struct rdma_xfer_ms_out *out)
{
	DBG("ENTER");
	sem_wait(&rdma_lock);
	int rc =  dma_push_pull_msub(PUSH_MODE, in, out);;
	sem_post(&rdma_lock);
	return rc;
} /* rdma_push_msub() */

int rdma_pull_msub(const struct rdma_xfer_ms_in *in,
		   struct rdma_xfer_ms_out *out)
{
	DBG("ENTER");
	sem_wait(&rdma_lock);
	int rc =  dma_push_pull_msub(PULL_MODE, in, out);;
	sem_post(&rdma_lock);
	return rc;
} /* rdma_pull_msub() */

static int dma_push_pull_buf(bool is_push, void *buf, int num_bytes,
	msub_h rem_msubh, int rem_offset, int priority,
	rdma_sync_type_t sync_type, struct rdma_xfer_ms_out *out)
{
	int rc;
	rem_msub *rmsub;
	(void)priority;

	try {
		/* Check for NULL pointer */
		if (!buf || !out || !rem_msubh) {
			ERR("NULL param(s). buf=%p, out=%p, rem_msubh = %u",
							buf, out, rem_msubh);
			out->in_param_ok = RDMA_NULL_PARAM;
			throw RDMA_NULL_PARAM;
		}
		rmsub = (rem_msub *)rem_msubh;

		/* Check for destid > 16 */
		if (rmsub->destid_len > 16) {
			ERR("destid_len=%u unsupported", rmsub->destid_len);
			out->in_param_ok = RDMA_NULL_PARAM;
			throw RDMA_INVALID_DESTID;
		}

		/* Check for RIO address > 64-bits */
		if (rmsub->rio_addr_len > 64) {
			ERR("rio_addr_len=%u unsupported", rmsub->rio_addr_len);
			out->in_param_ok = RDMA_INVALID_RIO_ADDR;
			throw RDMA_INVALID_RIO_ADDR;
		}

		/* Check if remote daemon is alive */
		if (fm_alive && (dd_h != NULL))
			if (!fmdd_check_did(dd_h, rmsub->destid, FMDD_RDMA_FLAG)) {
				ERR("Remote destination daemon NOT running!");
				throw RDMA_REMOTE_UNREACHABLE;
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
			ERR("Invalid sync_type: %d", sync_type);
			out->in_param_ok = RDMA_INVALID_SYNC_TYPE;
			throw RDMA_INVALID_SYNC_TYPE;
		}

		/* All input parameters are OK */
		out->in_param_ok = 0;

		if (is_push) {
			INFO("Sending %u bytes over DMA to destid=0x%X, rio_addr = 0x%X",
				num_bytes,
				rmsub->destid,
				rmsub->rio_addr_lo + rem_offset);

			rc = riomp_dma_write(peer.mport_hnd,
				  (uint16_t)rmsub->destid,
				  rmsub->rio_addr_lo + rem_offset,
				  buf,
				  num_bytes,
				  RIO_DIRECTIO_TYPE_NWRITE_R,
				  rd_sync);
			if (rc < 0) {
				ERR("riomp_dma_write() failed:(%d) %s", rc, strerror(rc));
			}
		} else {
			INFO("Receiving %u DMA bytes from destid=0x%X, RIO addr = 0x%X",
							num_bytes,
							rmsub->destid,
							rmsub->rio_addr_lo + rem_offset);

			rc = riomp_dma_read(peer.mport_hnd,
						 (uint16_t)rmsub->destid,
						 rmsub->rio_addr_lo + rem_offset,
						 buf,
						 num_bytes,
						 rd_sync);
			if (rc < 0) {
				ERR("riomp_dma_read() failed:(%d) %s", rc, strerror(rc));
			}
		}
		/* If synchronous, the return value is the xfer status. If async,
		 * the return value riomp_dma_write() is the token (if >= 0) */
		if (sync_type == rdma_sync_chk)
			out->dma_xfr_status = rc;
		else if (sync_type == rdma_async_chk && rc >= 0 ) {
			out->chk_handle = rc;	/* token */
			rc = 0;	/* success */
		}
	}
	catch(int& e) {
		rc = e;
	}
	return rc;
} /* dma_push_pull_buf() */

int rdma_push_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	int rc;
	sem_wait(&rdma_lock);
	rc = dma_push_pull_buf(PUSH_MODE, buf, num_bytes, rem_msubh, rem_offset,
		priority, sync_type, out);
	sem_post(&rdma_lock);
	return rc;
} /* rdma_push_buf() */

int rdma_pull_buf(void *buf, int num_bytes, msub_h rem_msubh, int rem_offset,
		  int priority, rdma_sync_type_t sync_type,
		  struct rdma_xfer_ms_out *out)
{
	int rc;
	sem_wait(&rdma_lock);
	rc = dma_push_pull_buf(PULL_MODE, buf, num_bytes, rem_msubh, rem_offset,
		priority, sync_type, out);
	sem_post(&rdma_lock);
	return rc;
} /* rdma_pull_buf() */

struct dma_async_wait_param {
	uint32_t token;		/* DMA transaction ID token */
	uint32_t timeout;
	int err;
};

/**
 * @brief Completion thread function. In this thread we wait for DMA
 *	  completion by calling riomp_dma_wait_sync() which blocks
 *	  until the DMA transfer is completed or a timeout occurs.
 */
void *compl_thread_f(void *arg)
{
	dma_async_wait_param	*wait_param = (dma_async_wait_param *)arg;

	/* Wait for transfer to complete or times out (-ETIMEDOUT returned) */
	wait_param->err = riomp_dma_wait_async(peer.mport_hnd,
					   wait_param->token,
					   wait_param->timeout);

	/* Exit the thread; it is no longer needed */
	pthread_exit(0);
} /* compl_thread_f() */

int rdma_sync_chk_push_pull(rdma_chk_handle chk_handle,
			    const struct timespec *wait)
{
	dma_async_wait_param	wait_param;
	int rc;

	DBG("ENTER");
	sem_wait(&rdma_lock);

	try {
		/* Make sure handle is valid */
		if (!chk_handle) {
			ERR("Invalid chk_handle(%d)", chk_handle);
			throw RDMA_NULL_PARAM;
		}

		/* Check for NULL or 0 wait times */
		if (!wait || !(wait->tv_sec | wait->tv_nsec)) {
			wait_param.timeout = 0;
			WARN("Timeout not specified, using default value");
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
			ERR("pthread_create(): %s", strerror(errno));
			throw RDMA_PTHREAD_FAIL;
		}

		/* Wait for transfer completion or timeout in thread */
		if (pthread_join(compl_thread, NULL)) {
			ERR("pthread_join(): %s", strerror(errno));
			throw RDMA_PTHREAD_FAIL;
		}
		rc = wait_param.err;
	}
	catch(int& e) {
		rc = e;
	}

	/* wait_param->err was populated with result (including timeout) */
	sem_post(&rdma_lock);
	return rc;
} /* rdma_sync_chk_push_pull() */

#ifdef __cplusplus
}
#endif
