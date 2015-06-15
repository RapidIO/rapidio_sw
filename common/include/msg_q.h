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
#ifndef MSG_Q_H
#define MSG_Q_H

#include <stdint.h>
#include <errno.h>
#include <mqueue.h>

#include <cstring>
#include <iostream>
#include <typeinfo>
#include <string>

#include "rdma_mq_msg.h"
#include "rdma_logger.h"

/* TODO: Copy from rdma_mq_msg.h */
#define MQ_SEND_BUF_SIZE	MQ_RCV_BUF_SIZE

using namespace std;

const unsigned MSG_Q_RECV_ERR = -1;
const unsigned MSG_Q_SEND_ERR = -2;
const unsigned MSG_Q_TIMED_RECV_ERR = -3;
const unsigned MSG_Q_DATA_TOO_LARGE = -4;


class msg_q_exception {

public:
	msg_q_exception(const char *msg) : msg(msg) {}
	void print() { cout << msg << endl; }
private:
	string msg;
};

enum mq_open_flags { MQ_CREATE=1, MQ_OPEN=2 };

template <typename T>
class msg_q {

public:
	msg_q(const string& name, mq_open_flags mq_of) :
					name(name),
					send_buf(new char[MQ_SEND_BUF_SIZE]),
					recv_buf(new char[MQ_RCV_BUF_SIZE])
	{
		if (sizeof(T) == 1)
			throw msg_q_exception("char is not a supported type");

		/* Determine open flags and ownership */
		if (mq_of == MQ_CREATE) {
			INFO("Creating '%s'\n", name.c_str());
			is_owner = true;
			oflag = O_RDWR | O_CREAT;
		} else if (mq_of == MQ_OPEN) {
			INFO("Opening '%s'\n", name.c_str());
			is_owner = false;
			oflag = O_RDWR;
		} else {
			throw msg_q_exception("Invalid open flag");
		}

		/* Default attributes */
		attr.mq_flags	= 0;
		attr.mq_maxmsg	= 1;	/* one message at a time */ 
		attr.mq_msgsize	= MQ_MSG_SIZE;
		attr.mq_curmsgs	= 0;

		mq = mq_open(name.c_str(), oflag, 0644, &attr);
		if (mq == (mqd_t)-1) {
			ERR("mq_open('%s') failed: %s\n", name.c_str(), strerror(errno));
			throw msg_q_exception("mq_open() failed");
		}
	}

	~msg_q()
	{
		if (mq_close(mq)) {
			ERR("mq_close('%s') failed: %s\n", name.c_str(), strerror(errno));
		}
		if (is_owner)
			if (mq_unlink((const char *)name.c_str())) {
				ERR("mq_unlink('%s') failed: %s\n", name.c_str(), strerror(errno));
			}
		delete [] send_buf;
		delete [] recv_buf;
	}

	const string& get_name() const { return name; }

	int receive()
	{
		/* On success, mq_receive() returns number of bytes received */
		if (mq_receive(mq, recv_buf, MQ_RCV_BUF_SIZE, NULL) == -1) {
			ERR("mq_receive('%s') failed: %s\n", name.c_str(), strerror(errno));
			return MSG_Q_RECV_ERR;
		}
		return 0;
	}

	int timed_receive(struct timespec *tm)
	{
		/* On success, mq_timedreceive() returns number of bytes received */
		if (mq_timedreceive(mq, recv_buf, MQ_RCV_BUF_SIZE, NULL, tm) == -1) {
			ERR("mq_timedreceive('%s') failed: %s\n", name.c_str(), strerror(errno));
			return MSG_Q_TIMED_RECV_ERR;
		}
		return 0;
	}

	int send()
	{
		/* TODO: Move this to constructor and use default args */
		if (sizeof(T) > MQ_SEND_BUF_SIZE) {
			ERR("Data size (%u) larger than send buffer (%u)\n",
					sizeof(T), MQ_SEND_BUF_SIZE);
			return MSG_Q_DATA_TOO_LARGE;
		}

		if (mq_send(mq, send_buf, sizeof(T), 1) == -1) {
			ERR("mq_send('%s') failed: %s\n", name.c_str(), strerror(errno));
			return MSG_Q_SEND_ERR;
		}

		return 0;
	}

	void get_recv_buffer(T **recv_buf)
	{
		*recv_buf = (T *)this->recv_buf;
		
	}

	void get_send_buffer(T **send_buf)
	{
		*send_buf = (T *)this->send_buf;
	}


private:
	string	name;
	int	oflag;
	mqd_t	mq;
	mq_attr	attr;
	char	*send_buf;
	char	*recv_buf;
	bool	is_owner;
};

#endif


