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

#ifndef MS_OWNERS_H
#define MS_OWNERS_H

#include <string>
#include <vector>
#include <algorithm>

#include "rdmad_ms_owner.h"

#include "libcli.h"
#include "liblog.h"
#include "unix_sock.h"

using namespace std;

/* Owners are 12-bits */
#define MSOID_MAX	0xFFF

class ms_owners
{
public:
	ms_owners();

	~ms_owners();

	void dump_info(struct cli_env *env);


	int create_mso(const char *name, unix_server *other_server, uint32_t *msoid);
	
	int open_mso(const char *name, uint32_t *msoid, uint32_t *mso_conn_id,
			unix_server *user_server);

	int close_mso(uint32_t msoid, uint32_t mso_conn_id);

	void close_mso(unix_server *other_server);

	int destroy_mso(unix_server *other_server);

	int destroy_mso(uint32_t msoid);

	ms_owner* operator[](uint32_t msoid);

private:
	bool msoid_free_list[MSOID_MAX+1];
	vector<ms_owner *>	owners;
	pthread_mutex_t		lock;
};


#endif


