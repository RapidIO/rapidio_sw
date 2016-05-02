#ifndef __CMSOCK_H__
#define __CMSOCK_H__

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <stdexcept>

#include "rapidio_mport_sock.h"

class CMSocket {
public:
  CMSocket(int mport_id, int mbox) : 
    m_mportid(mport_id), m_mboxid(mbox)
  {
    memset(&m_mbox, 0, sizeof(m_mbox));
    memset(&m_sock, 0, sizeof(m_sock));

    if (riomp_sock_mbox_create_handle(mport_id, mbox, &m_mbox))
       throw std::runtime_error("CMSocket: Cannot create mailbox!");

    if (riomp_sock_socket(m_mbox, &m_sock)) {
       riomp_sock_mbox_destroy_handle(&m_mbox);
       throw std::runtime_error("CMSocket: Cannot create socket!");
    }
    
    m_open = true;
    m_connected = false;
  }
  ~CMSocket() { this->close(); riomp_sock_mbox_destroy_handle(&m_mbox); };

  inline int bind(uint16_t lport) {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    return riomp_sock_bind(m_sock, lport);
  }

  inline int listen() {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    return riomp_sock_listen(m_sock);
  }

  inline int accept(CMSocket*& clisock /*out*/, uint32_t timeout = 0) {
    if (!m_open) return -(errno=EINVAL);
    if (m_connected) return -(errno=EISCONN);
    CMSocket* cli = new CMSocket(m_mportid, m_mboxid);
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
  bool            m_connected; ///< This socket has passed connect() or accept()
  riomp_mailbox_t m_mbox;
  riomp_sock_t    m_sock;
};

#endif // __CMSOCK_H__
