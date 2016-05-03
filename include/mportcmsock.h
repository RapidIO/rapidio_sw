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
 * \note Unlike BSD sockets the CM cariety does not support select
 * \note read can be used in polling mode with a short timeout
 */
class MportCMSocket {
public:
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

  inline int bind(uint16_t lport) {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    int rc = riomp_sock_bind(m_sock, lport);
    if (rc == 0) m_bound = true;
    return rc;
  }

  inline int listen() {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    return riomp_sock_listen(m_sock);
  }

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

  inline int connect(uint16_t ddestid, uint16_t dmbox, uint16_t dport) {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    int rc = riomp_sock_connect(m_sock, ddestid, dmbox, dport);
    if (rc == 0) m_connected = true;
    return rc;
  }
 
  inline int write(void* data, const int data_len) {
    if (!m_open || !m_connected) return -(errno=ENOTCONN);
    return riomp_sock_send(m_sock, data, data_len);
  }
  inline int read(void* data_in, const int data_max_len, uint32_t timeout = 0) {
    if (!m_open || !m_connected) return -(errno=ENOTCONN);
    return riomp_sock_receive(m_sock, &data_in, data_max_len, timeout);
  }

  inline void close() {
    if (!m_open) return;
    m_open = false; m_connected = false;
    riomp_sock_close(&m_sock);
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
