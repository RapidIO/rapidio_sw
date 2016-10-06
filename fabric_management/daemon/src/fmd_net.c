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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "fmd_net.h"
#include "did.h"
#include "ct.h"
#include "rio_ecosystem.h"
#include "riocp_pe.h"
#include "riocp_pe_internal.h"
#include "cfg.h"
#include "liblist.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

int fmd_traverse_network(riocp_pe_handle mport_pe, struct cfg_dev *c_dev)
{
	return fmd_traverse_network_from_pe_port(mport_pe, RIO_ANY_PORT, c_dev);
}

int fmd_traverse_network_from_pe_port(riocp_pe_handle pe, rio_port_t port_num, struct cfg_dev *c_dev)
{
	struct l_head_t sw_list;

	riocp_pe_handle new_pe, curr_pe;
	rio_port_t port_st, port_cnt, pnum;
	int conn_pt, rc;
	uint32_t comptag;
	struct cfg_dev curr_dev, conn_dev;

	l_init(&sw_list);

	/* Enumerated device connected to master port */
	curr_pe = pe;
	curr_dev = *c_dev;

	if (RIO_ANY_PORT == port_num) {
		port_st = 0;
		port_cnt = RIOCP_PE_PORT_COUNT(curr_pe->cap);
	} else {
		port_st = port_num;
		port_cnt = port_num + 1;
	}

	do {
		for (pnum = port_st; pnum < port_cnt; pnum++) {
			new_pe = NULL;
		
			if (cfg_get_conn_dev(curr_pe->comptag, pnum, &conn_dev, &conn_pt)) {
				HIGH("PE 0x%0x Port %d NO CONFIG\n", curr_pe->comptag, pnum);
				continue;
			};

			rc = riocp_pe_probe(curr_pe, pnum, &new_pe, &conn_dev.ct,
					(char *)conn_dev.name);

			if (rc) {
				if ((-ENODEV != rc) && (-EIO != rc)) {
					CRIT("PE 0x%0x Port %d probe failed %d\n", 
						curr_pe->comptag, pnum, rc);
					goto fail;
				};
				HIGH("PE 0x%x Port %d NO DEVICE, expected %x\n",
					curr_pe->comptag, pnum, conn_dev.ct);
				continue;
			};

			if (NULL == new_pe) {
				HIGH("PE 0x%x Port %d ALREADY CONNECTED\n",
					curr_pe->comptag, pnum);
				continue;
			}

			rc = riocp_pe_get_comptag(new_pe, &comptag);
			if (rc) {
				CRIT("Get new comptag failed, rc %d\n", rc);
				goto fail;
			};

			if (comptag != conn_dev.ct) {
				DBG("Probed ep ct 0x%x != 0x%x config ct port %d\n",
					comptag, conn_dev.ct, pnum);
				goto fail;
			};

			HIGH("PE 0x%x Port %d Connected: DEVICE %s CT 0x%x DID 0x%x\n",
				curr_pe->comptag, pnum, 
				new_pe->name, new_pe->comptag, new_pe->destid);

			if (RIOCP_PE_IS_SWITCH(new_pe->cap)) {
				void *pe;
				l_item_t *li;
				bool found = false;

				pe = l_head(&sw_list, &li);
				while (NULL != pe && !found) {
					if (((struct riocp_pe *)pe)->comptag
						== new_pe->comptag) {
						found = true;	
						continue;
					}
					pe = l_next(&li);
				};
				if (!found) {
					HIGH("Adding PE 0x%08x to search\n",
						new_pe->comptag);
					l_push_tail(&sw_list, (void *)new_pe);
				};
			}
		}

		curr_pe = (riocp_pe_handle)l_pop_head(&sw_list);
		if (NULL != curr_pe) {
			rc = cfg_find_dev_by_ct(curr_pe->comptag, &curr_dev);
			if (rc) {
				CRIT("cfg_find_dev_by_ct fail, ct 0x%x rc %d\n",
					curr_pe->comptag, rc);
				goto fail;
			};
			port_st = 0;
			port_cnt = RIOCP_PE_PORT_COUNT(curr_pe->cap);
			HIGH("Now probing PE CT 0x%08x ports %d to %d\n",
					curr_pe->comptag, port_st, port_cnt);

		}
	} while (curr_pe != NULL);

	return 0;
fail:
	return -1;
};

#ifdef __cplusplus
}
#endif
