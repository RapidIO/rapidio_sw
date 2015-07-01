/* Data Structure for connection to FMD in slave mode */
/* A Slave is an FMD that accepts commands and returns responses */
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

#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>
#include "fmd_peer_msg.h"
#include "riodp_mport_lib.h"

#ifndef __FMD_MGMT_SLAVE_H__
#define __FMD_MGMT_SLAVE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct fmd_slave {
	int fd; /* MPORT file descriptor for register access */
	pthread_t slave_thr; /* Slave FMDR, handles Master FMD cmds */
	sem_t started; 
	int slave_alive;
	int slave_must_die;

        uint32_t mp_num;
	uint32_t mast_did;
        uint32_t mast_skt_num; /* Socket number to connect to */
        uint32_t mb_valid;
        riodp_mailbox_t mb;
	uint32_t skt_valid;
        riodp_socket_t skt_h; /* Connected socket */
        int tx_buff_used;
        int tx_rc;
        union {
                void *tx_buff;
                struct fmd_slv_to_mast_msg *s2m; /* alias for tx_buff */
        };
        int rx_buff_used;
        int rx_rc;
        union {
                void *rx_buff;
                struct fmd_mast_to_slv_msg *m2s; /* alias for rx_buff */
        };
	struct fmd_p_hello m_h_rsp;
};

extern int start_peer_mgmt_slave(uint32_t mast_acc_skt_num, uint32_t mast_did,
			uint32_t  mp_num, struct fmd_slave *slave, int fd);

extern void shutdown_slave_mgmt(void);

#ifdef __cplusplus
}
#endif

#endif /* __FMD_MGMT_SLAVE_H__ */