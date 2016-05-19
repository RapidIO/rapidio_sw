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

#ifndef __MPORTCMSOCK_H__
#define __MPORTCMSOCK_H__

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <stdexcept>

#include "rapidio_mport_sock.h"

/** \brief Wrapper class for Mport CM sockets
 * \note CM stands for Channelised Messagings
 * \note Unlike BSD sockets the CM variety does not support select
 * \note CM sockets have no flow control so high thruput cannot be expected.
 * \note read can be used in polling mode with a short timeout
 */
class MportCMSocket {
  static const int CM_FUDGE_OFFSET = 20;

public:
	/** \brief Create a mailbox (handle) for sending and receiving messages
	 *
	 * @param[in] mport_id : Index of /devs/rio_mportX device to use.  For a list
	 *					of available mports, use mportmgmt.h::get_mport_list.
	 * @param[in] mbox: Not used.
	 *
	 * @return MportCMSocket handle on success, NULL on failure.
	 */
  MportCMSocket(int mport_id, int mbox) : 
    m_mportid(mport_id), m_mboxid(mbox)
  {
    memset(&m_mbox, 0, sizeof(m_mbox));
    memset(&m_sock, 0, sizeof(m_sock));

    if (riomp_sock_mbox_create_handle(mport_id, mbox, &m_mbox))
       throw std::runtime_error("MportCMSocket: Cannot create mailbox!");

    if (riomp_sock_socket(m_mbox, &m_sock)) {
       riomp_sock_mbox_destroy_handle(&m_mbox);
       throw std::runtime_error("MportCMSocket: Cannot create socket!");
    }
    
    m_open = true;
    m_connected = false;
  }
  ~MportCMSocket() { this->close(); riomp_sock_mbox_destroy_handle(&m_mbox); };

  /** \brief bind socket handle to a particular local channelized messaging port */
  inline int bind(uint16_t lport) {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    int rc = riomp_sock_bind(m_sock, lport);
    if (rc == 0) m_bound = true;
    return rc;
  }

  /** \brief Set bound socket to listen to connect() requests from other nodes */
  inline int listen() {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    return riomp_sock_listen(m_sock);
  }

  /** Block waiting for a remote connect request, or a timeout */
  inline int accept(MportCMSocket*& clisock /*out*/, uint32_t timeout = 0) {
    if (!m_open) return -(errno=EINVAL);
    if (!m_bound) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    MportCMSocket* cli = new MportCMSocket(m_mportid, m_mboxid);
    int rc = riomp_sock_accept(m_sock, &cli->m_sock, timeout);
    if (rc != 0) { delete cli; return rc; }
    cli->m_connected = true;
    clisock = cli;
    return 0;
  }

  /** \brief Connect to a remote channelized messaging socket.
   * 
   * param[in] ddestid 8 bit device ID hosting the remote socket
   * param[in] dport Must be the same value used by the remote bind() call.
   *
   */
  inline int connect(uint16_t ddestid, uint16_t dport) {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    int rc = riomp_sock_connect(m_sock, ddestid, dport);
    if (rc == 0) m_connected = true;
    return rc;
  }

  /** \brief Send data to a connected channelized messaging socket.
   * @param[in] data Pointer to data to be sent.
   * @param[in] data_len Number of bytes to be sent.  Maximum 4076 bytes.
   *
   * returns int Indication of success or failure.  write() always sends all bytes.
   * return 0 Success, message sent.
   * return !0 Failure, check errno for reason.
   *
   * \note Unlike the libc counterpart this returns 0 on success, errno on error
   */
  inline int write(const void* data, const int data_len) {
    if (data == NULL)
      throw std::runtime_error("MportCMSocket::write: NULL data!");
    if (data_len > (4096-CM_FUDGE_OFFSET))
      throw std::runtime_error("MportCMSocket::write: Data are too large!");
    uint8_t buffer[4096+CM_FUDGE_OFFSET] = {0};
    if (!m_open || !m_connected) return -(errno=ENOTCONN);
    memcpy(buffer+CM_FUDGE_OFFSET, data, data_len);
    return riomp_sock_send(m_sock, (void*)buffer, data_len + CM_FUDGE_OFFSET);
  }

  /** \brief Receive data from a connected channelized messaging socket.
  * @param[in] data_in Pointer to data to be sent.
  * @param[in] data_max_len Maximum number of bytes that can be received.
  *			Will never receive more than 4KB (4096 bytes).
  * @param[in] timeout Number of seconds to wait for data.
  *
  * returns int Indication of success or failure.  write() always sends all bytes.
  * return 0 Success, message sent.
  * return !0 Failure, check errno for reason.
  *
  * \note Unlike the libc counterpart this returns 0 on success, errno on error.
  * An implication is that the message format must include the message size.
  */
  inline int read(void* data_in, const int data_max_len, uint32_t timeout = 0) {
    if (data_in == NULL)
      throw std::runtime_error("MportCMSocket::read: NULL data!");
    if (data_max_len > (4096-CM_FUDGE_OFFSET))
      throw std::runtime_error("MportCMSocket::read: Data are too large!");
    if (!m_open || !m_connected) return -(errno=ENOTCONN);
    uint8_t buffer[4096+CM_FUDGE_OFFSET] = {0};
    void* p666 = (void*)buffer;
    int rc = riomp_sock_receive(m_sock, &p666, data_max_len + CM_FUDGE_OFFSET, timeout);
    if (rc) return rc;
    memcpy(data_in, buffer+CM_FUDGE_OFFSET, data_max_len);
    return 0;
  }

  /** \brief Close connected/bound/listening/accepting socket.  This is an empty
  * operation for sockets in other states.
  *
  * returns int Indication of success or failure.  write() always sends all bytes.
  * return 0 Success, message sent.
  * return !0 Failure, check errno for reason.
  *
  */

  inline int close() {
    if (!m_open) return -(errno=ENOTCONN);
    m_open = false; m_connected = false; m_bound = false;
    return riomp_sock_close(&m_sock);
  }

private:
  int             m_mportid;
  int             m_mboxid;
  bool            m_open;
  bool            m_bound; ///< This socket has passed bind()
  bool            m_connected; ///< This socket has passed connect() or accept()
  riomp_mailbox_t m_mbox;
  riomp_sock_t    m_sock;
};

#endif // __MPORTCMSOCK_H__
