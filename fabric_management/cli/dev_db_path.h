/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
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

#ifndef __DEV_DB_PATH_H__
#define __DEV_DB_PATH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "IDT_Port_Config_API.h"
#include "IDT_Routing_Table_Config_API.h"
#include "IDT_Statistics_Counter_API.h"
#include "IDT_Error_Management_API.h"
#include "dev_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <time.h>

struct path_step {
	UINT32	ct;  /* Component tag of device for this step */
	UINT8	out_port;	/* Output port for device for this step.
				 * RIO_ALL_PORTS is used for last step in path.
				 */
	struct path_step *next;
	struct path_step *prev;
};

struct route { /* A route always accesses the *tail device. */
	UINT32 destID;		/* DestID to be used for this path */
	UINT8  hc;		/* Hopcount for maintenance transactions */
	struct path_step *tail;
	struct path_step *head;
};

struct route_w_resp {
	struct route req;	/* Note: It's expected that req->head */
	struct route resp;	/* == resp->tail, and resp->head ==   */
				/* == req->tail                      */
};

#define MP_HC 0xFF

void init_route(UINT32 destID, UINT8 hc, struct route *rout);

/* All routines clean up after themselves if they fail. */
STATUS add_step_tail(struct route *rte, struct path_step *pth);
STATUS add_step_head(struct route *rte, struct path_step *pth);
STATUS clone_path(struct route *rte_in, struct route *new_rte);
void drop_path(struct route *rte_in);
STATUS set_route(struct route *rte); /* Updates HW with route info */

STATUS new_probe_path(struct route_w_resp *i_mpr, struct route_w_resp *o_mpr,
                        UINT8 i_port);
STATUS get_discover_path(struct route_w_resp *i_mpr,
                          struct route_w_resp *o_mpr,
                        UINT8 i_port);
STATUS chk_route(struct route *rte);

struct dev_db_entry;
STATUS set_discover_path_ids(struct dev_db_entry *st_dbe,
                          struct dev_db_entry *new_dbe,
                        UINT8 x_port);


#ifdef __cplusplus
}
#endif

#endif /* __DEV_DB_PATH_H__ */
