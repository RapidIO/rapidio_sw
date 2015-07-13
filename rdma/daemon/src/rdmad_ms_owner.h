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

#ifndef MS_OWNER_H
#define MS_OWNER_H

#include <stdint.h>
#include <errno.h>

#include <cstdio>
#include <cstring>

#include <sstream>
#include <utility>
#include <vector>
#include <string>
#include <algorithm>

#include "msg_q.h"
#include "rdma_types.h"
#include "liblog.h"

#define MSO_CONN_ID_START	0x1

using namespace std;

struct dump_msid {
	void operator ()(uint32_t msid) { printf("%8X ", msid); }
};

class ms_owner
{
public:
	/* Constructor */
	ms_owner(const char *owner_name, uint32_t msoid) :
		name(owner_name), msoid(msoid), mso_conn_id(MSO_CONN_ID_START) {}

	/* Destructor */
	~ms_owner();

	/* Accessor */
	uint32_t get_msoid() const { return msoid; }

	const char *get_mso_name() const { return name.c_str(); }

	/* For finding an ms_owner by its msoid */
	bool operator ==(uint32_t msoid) { return this->msoid == msoid; }

	/* For finding an ms_owner by its name */
	bool operator ==(const char *owner_name) { return this->name == owner_name; }

	/* Stores handle of memory spaces currently owned by owner */
	void add_msid(uint32_t msid) {
		INFO("Adding msid(0x%X) to msoid(0x%X)\n", msid, msoid);
		msid_list.push_back(msid);
	}

	/* Removes handle of memory space from list of owned spaces */
	int remove_msid(uint32_t msid)
	{
		/* Find memory space by the handle, return error if not there */
		auto it = find(begin(msid_list), end(msid_list), msid);
		if (it == end(msid_list)) {
			WARN("0x%X not owned by 0x%X\n", msid, msoid);
			return -1;
		}

		/* Erase memory space handle from list */
		INFO("Removing msid(0x%X) from msoid(0x%X)\n", msid, msoid);
		msid_list.erase(it);

		return 1;
	} /* remove_ms_h() */

	/* Returns whether this owner sill owns some memory spaces */
	bool owns_mspaces()
	{
		return msid_list.size() != 0;
	} /* owns_mspaces() */

	void dump_info()
	{
		printf("%8X %32s\t", msoid, name.c_str());
		for_each(msid_list.begin(), msid_list.end(), dump_msid());
		puts("");	/* New line */
	} /* dump_info() */

	int open(uint32_t *msoid, uint32_t *mso_conn_id);

	int close(uint32_t mso_conn_id);

private:
	int close_connections();

	string		name;
	uint32_t	msoid;
	uint32_t	mso_conn_id;
	vector<uint32_t>	msid_list;
	vector<msg_q<mq_close_mso_msg> *> mq_list;
};


#endif

