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
#include "rdmad_functors.h"

using namespace std;

/* Owners are 12-bits */
#define MSOID_MAX	0xFFF

struct has_msoid {
	has_msoid(uint32_t msoid) : msoid(msoid) {}
	bool operator()(ms_owner *mso) {
		return *mso == msoid;
	}
private:
	uint32_t msoid;
};

struct has_mso_name {
	has_mso_name(const char *name) : name(name) {}
	bool operator()(ms_owner *mso) {
		return *mso == name;	/* Use operator==(const char *s) */
	}
private:
	const char *name;
};

class ms_owners
{
public:
	ms_owners()
	{
		/* Initially all memory space owner handles are free */
		fill(msoid_free_list,msoid_free_list + MSOID_MAX + 1, true);
	} /* Constructor */

	~ms_owners()
	{
		/* Delete owners */
		for_each(begin(owners), end(owners), [](ms_owner *p) { if (p) delete p;});
	}

	void dump_info()
	{
		printf("%8s %32s %8s\n", "msoid", "name", "MSIDs owned by mso");
		printf("%8s %32s %8s\n", "-----", "----", "------------------");
		for_each(owners.begin(), owners.end(), call_dump_info<ms_owner *>());
	} /* dump_info() */

	int create_mso(const char *name, uint32_t *msoid)
	{
		(void)name;
		(void)msoid;
#if 0
		/* Find a free memory space owner handle */
		bool *fmsoid = find(msoid_free_list,
				    msoid_free_list + MSOID_MAX + 1,
				    true);

		/* Not found, return with error */
		if (fmsoid == (msoid_free_list + MSOID_MAX + 1)) {
			fprintf(stderr, "%s: Too many memory space owners!\n",
								__func__);
			return -1;
		}
		
		/* Get the free handle */
		*msoid = fmsoid - msoid_free_list;
	
		/* Mark msoid as used */
		*fmsoid = false;

		/* Create an owner with the free ID */
		ms_owner *mso = new ms_owner(name, *msoid);

		/* Store in owners list */
		owners.push_back(mso);	
#endif
		return 1;
	} /* get_mso() */

	int open_mso(const char *name, uint32_t *msoid, uint32_t *mso_conn_id)
	{
		has_mso_name	hmn(name);

		/* Find the owner having specified name */
		auto mso_it = find_if(owners.begin(), owners.end(), hmn);
		if (mso_it == owners.end()) {
			ERR("%s is not a memory space owner's name\n", name);
			return -1;
		}

		/* Open the memory space owner */
		if ((*mso_it)->open(msoid, mso_conn_id) < 0) {
			ERR("Failed to open memory space owner %s\n", name);
			return -2;
		}

		return 1;
	} /* open_mso() */

	int close_mso(uint32_t msoid, uint32_t mso_conn_id)
	{
		has_msoid	hmi(msoid);

		/* Find the mso */
		auto it = find_if(begin(owners), end(owners), hmi);
		if (it == end(owners)) {
			ERR("msoid(0x%X) not found\n", msoid);
			return -1;
		}

		/* Close the connection */
		if ((*it)->close(mso_conn_id) < 0)
			return -1;

		return 1;
	} /* close_mso() */

	int destroy_mso(uint32_t msoid)
	{
		has_msoid	hmi(msoid);

		/* Find the owner belonging to msoid */
		auto mso_it = find_if(owners.begin(), owners.end(), hmi);
		
		/* Not found, return error */
		if (mso_it == owners.end()) {
			fprintf(stderr, "%s: 0x%X not found\n", __func__, msoid);
			return -1;
		}	

		/* Check if owner still owns memory spaces */
		if ((*mso_it)->owns_mspaces()) {
			fprintf(stderr, "%s: 0x%X still owns memory spaces\n",
								__func__, msoid);
			return -2;
		}

		/* Remove owner */
		delete *mso_it;
		owners.erase(mso_it);

		/* Mark msoid as being free */
		msoid_free_list[msoid] = true;

		return 1;
	} /* destroy_msoid() */

	ms_owner* operator[](uint32_t msoid)
	{
		has_msoid	hmi(msoid);
		return *find_if(begin(owners), end(owners), hmi);
	}
private:
	bool msoid_free_list[MSOID_MAX+1];
	vector<ms_owner *>	owners;
};


#endif


