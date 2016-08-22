/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

/**
 * \file cmsock_example.cc
 *
 * \brief Example code for CM Socket Wrapper (RapidIO MBOX-based API).
 *
 * The file supports a client and server.  To execute the demo, first run
 * the server on a node as follows:
 *
 * sudo ./cmsock_demo -s
 *
 * Then, run the client on another node as follows:
 *
 * sudo ./cmsock_demo -c destid, where destid is the destination ID of the
 * node executing the server.  To determine the destid of the server, use
 * the following command on the node executing the client,
 * where NAME is the name of the node 
 * executing the server.
 *
 * cat /sys/bus/rapidio/devices/NAME/destid
 *
 * \note Do not use this API for high-thruput messaging.
 *
 * \note Unlike BSD sockets this API is fixed (and known-in-advance) size reads.
 *
 * \note Max data to be transmitted is (4096-20) bytes.
 *
 * This file implements an example client and server.
 */

#include <unistd.h>
#include <signal.h>

#include <string>

#include "rio_misc.h"
#include "mportcmsock.h"

#define DATA_SZ	   32

#define CM_PORT    666

#define ACCEPT_TO  60*1000 ///< 60s, 0=wait forever
#define READ_TO    1000    ///< 1000=1s, 0=wait forever

#define abs(x) ((x>=0)? (x): (-x))

volatile int stop_req = 0;
static void sig_term(int UNUSED_PARM(signo)) { stop_req = 1; }

void usage_and_exit(const char* name)
{
	fprintf(stderr, "usage: %s: <-c destid> or <-s>\n", name);
	exit(0);
}

char bufA[DATA_SZ], bufB[DATA_SZ];

inline std::string hexdump(uint8_t* data, const int len)
{
	std::string out;
	for (int i = 0; i < len; i++) {
		char tmp[9] = {0};
		snprintf(tmp, 8, "%02x", data[i]);
		out.append(tmp).append(" ");
	}
	return out;
}

inline std::string hexdump(const char* data, const int len)
{
	return hexdump((uint8_t*)data, len);
}

/** \brief Sample client and server loops for the Channelized Messaging
 * socket wrapper.
 *
 * The main procedure is the entry point for the demonstration.  It supports
 * a server loop, which echos messages back to the client, and a client 
 * application which sends a continuous stream of messages to the server and
 * checks that the responses match the original message sent.
 *
 * The procedure performs the following steps:
 */

int main(int argc, char* argv[])
{
	bool ret = 0;
	int server = -1;
	uint16_t destid = 0xFFFF;
	int mportid = 0;
	int rc = 0;

	memset(bufA, 'A', sizeof(bufA));
	memset(bufB, 'B', sizeof(bufA));

	/** - Parse input parameters */
	int c = 0;
	while ((c = getopt (argc, argv, "hsc:")) != -1) {
		switch(c) {
		case 'h': usage_and_exit(argv[0]);
			break;
		case 's': server = 1;
			break;
		case 'c': server = 0;
			destid = atoi(optarg);
			break;
		default: exit(9);
			break;
		}
	}

	if (server == -1)
		usage_and_exit(argv[0]);

	signal(SIGINT, sig_term);
	signal(SIGTERM, sig_term);

	/** - Create a CM socket */
	MportCMSocket* cms = new MportCMSocket(mportid, 0);

	/** - for a server */
	if (server == 1) {
		/**   - bind the socket to the socket number   */
		rc = cms->bind(CM_PORT);
		if (rc) {
			fprintf(stderr, "bind error %d: %s\n",
				rc, strerror(abs(rc)));
			ret = 1;
			goto exit;
		}
		/**   - set the socket to the listen for connect requests */
		rc = cms->listen();
		if (rc) {
			fprintf(stderr, "listen error %d: %s\n",
				rc, strerror(abs(rc)));
			ret = 2;
			goto exit;
		}

		/**   - loop forever */
		for (; !stop_req;) {
			MportCMSocket* cli_cms = NULL;

			/**     - accept connect request */
			rc = cms->accept(cli_cms, ACCEPT_TO);
			if (rc == ETIME)
				continue;
			if (rc == EINTR)
				break;

			if (rc) {
				fprintf(stderr, "accept error %d: %s\n",
					rc, strerror(abs(rc)));
				ret = 3;
				goto exit;
			}

			/**     - read a message from the client */
			uint8_t buf[DATA_SZ+1] = {0};
			rc = cli_cms->read(buf, DATA_SZ, READ_TO);
			if (rc) {
				fprintf(stderr, "read error %d: %s got [%s]\n", 
					rc, strerror(abs(rc)),
					hexdump(buf, DATA_SZ).c_str());
				goto next;
			}
			/**     - if the message has the expected contents */
			if (!memcmp(buf, bufA, DATA_SZ)) {
				/**     - send the message back to the client */
				rc = cli_cms->write(bufB, DATA_SZ);
				if (rc) {
					fprintf(stderr, "write error %d: %s\n",
						 rc, strerror(abs(rc)));
					goto next;
				}
			} else {
				/**     - or output an error message */
				fprintf(stderr, "Invalid data! got=[%s]\n",
					hexdump(buf, DATA_SZ).c_str());
			};
 next:
			delete cli_cms;
		}

		goto exit;
	}

// CLIENT
	/** - for the client, connect to the server */
	rc = cms->connect(destid, CM_PORT);
	if (rc) {
		fprintf(stderr, "connect error %d: %s\n",
			rc, strerror(abs(rc)));
		ret = 1;
		goto exit;
	}
	/** - Sending one message and receive response */
	{{
		char buf[DATA_SZ] = {0};
		/** - Send a  message to teh server */
		rc = cms->write(bufA, DATA_SZ);
		if (rc) {
			fprintf(stderr, "write error %d: %s\n",
				rc, strerror(abs(rc)));
			ret = 2;
			goto exit;
		}
		/** - Read the response back */
		rc = cms->read(buf, DATA_SZ, READ_TO);
		if (rc) {
			fprintf(stderr, "read error %d: %s got [%s]\n",
				rc, strerror(abs(rc)),
				hexdump(buf, DATA_SZ).c_str());
			ret = 3;
			goto exit;
		}
		/** - If the received message contents are wrong, print an
		 * error message.
		 */
		if (memcmp(buf, bufB, DATA_SZ))
			fprintf(stderr, "Invalid data read from server!\n");
	}}

exit:
	delete cms;

	return ret;
}
