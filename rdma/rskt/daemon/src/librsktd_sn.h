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

#ifndef __RSKTD_SN_H__
#define __RSKTD_SN_H__

#include <stdint.h>
#include <unistd.h>
#include "librskt_states.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RSKTD_MAX_SKT_NUM 0xFFFF
#define RSKTD_INVALID_SKT 0x0
#define RSKTD_DYNAMIC_SKT 0x1000
#define RSKTD_NUM_SKTS (RSKTD_MAX_SKT_NUM + 1)

extern enum rskt_state skts[RSKTD_NUM_SKTS];

/** @brief Initialize RSKTD socket numbers database.
 * @param[in] max_skt_num Maximum socket number to be used by the deamon.
 * @return None.
 */
void		rsktd_sn_init(uint16_t max_skt_num);

/** @brief Returns the current state of a socket number
 * @param[in] skt_num for which current state is returned 
 * @return rskt_state
 */
enum rskt_state rsktd_sn_get(uint32_t skt_num);

/** @brief Sets the current state of a socket number
 * @param[in] skt_num whose state is changed to st
 * @param[in] st State value for the skt_num
 * @return rskt_state
 */
void 		rsktd_sn_set(uint32_t skt_num, enum rskt_state st);

/** @brief Returns an unused socket number
 * @return Socket number whose state is now "allocated"
 */
uint32_t	rsktd_sn_find_free(void);

/** @brief Binds CLI commands into a database, as requested
 * @return None
 */
void		librsktd_bind_sn_cli_cmds(void);

#ifdef __cplusplus
}
#endif

#endif /* __RSKTD_SN_H__ */
