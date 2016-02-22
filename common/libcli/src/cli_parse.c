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
/* cli_parse.c : routines for parsing command line parameters
 * Note: this excludes recognition of a command, which is done by the
 * command database.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include "limits.h"

#include "libcli.h"
#include "cli_cmd_line.h"

#ifdef __cplusplus
extern "C" {
#endif

int getDecParm(char *token, int defaultData)
{
	unsigned long data;

	if (token == NULL || token[0] == '/')
		data = defaultData;
	else if (sscanf(token, "%ld", &data) <= 0)
		data = defaultData;
	return data;
}

float getFloatParm(char *token, float defaultData)
{
	float data;

	if (token == NULL || token[0] == '/')
		data = defaultData;
	else if (sscanf(token, "%f", &data) <= 0)
		data = defaultData;
	return data;
}

unsigned long getHexParm(char *dollarParameters[], unsigned int nDollarParms,
				char *token, unsigned int defaultData)
{
	unsigned long dollarIndex;
	unsigned long data;

	if (token == NULL || token[0] == '/') {
		data = defaultData;
		goto exit;
	};

	if (sscanf(token, "%lX", &data) > 0)
		goto exit;

	if ((sscanf(token, "$%ld", &dollarIndex) > 0) &&
	    (dollarIndex < nDollarParms)) {
		if (1 == sscanf(dollarParameters[dollarIndex], "%lX", &data))
			goto exit;
	}

	data = defaultData;
exit:
	return data;
}

uint64_t getHex(char *token, unsigned long defaultData)

{
	uint64_t data = defaultData;
	char *end;

	errno = 0;
	if ((token != NULL) && (token[0] != '/')) {
		data = strtoull(token, &end, 16);
		if (!data && (end == token)) {
			data = defaultData;
		} else if (data == ULLONG_MAX && errno) {
    			data = defaultData;
		} else if (*end) {
    			data = defaultData;
		}
	};

	return data;
}

char* GetEnv(char* var)
{
        if (var == NULL || var[0] == '\0') return NULL;

        std::map<std::string, std::string>::iterator it = SET_VARS.find(var);
        if (it == SET_VARS.end()) return NULL;

        return (char *)it->second.c_str();
}

void SetEnvVar(char* arg)
{
        if(arg == NULL || arg[0] == '\0') return;

        char* tmp = strdup(arg);

        char* sep = strstr(tmp, "=");
        if (sep == NULL) goto exit;
        sep[0] = '\0';
        SET_VARS[tmp] = (sep+1);

exit:
        free(tmp);
}

char* SubstituteParam(char* arg)
{
	if (arg == NULL || arg[0] == '\0')
		return arg;

	if (arg[0] != '$')
		return arg;

        std::map<std::string, std::string>::iterator it = SET_VARS.find(arg+1);
        if (it == SET_VARS.end())
		return arg;

        return (char *)it->second.c_str();
}

int GetDecParm(char* arg, int dflt)
{
        return getDecParm((char*)SubstituteParam(arg), dflt);
};

uint64_t GetHex(char* arg, int dflt)
{
        return getHex((char*)SubstituteParam(arg), dflt);
};

float GetFloatParm(char* arg, float dflt)
{
        return getFloatParm((char*)SubstituteParam(arg), dflt);
};

void update_string(char **target, char *new_str, int len)
{
        if (NULL != *target)
                free(*target);
        *target = (char *)malloc(len+1);
        (*target)[len] = 0;
        memcpy(*target, new_str, len);
};

int get_v_str(char **target, char *parm, int chk_slash)
{
        int len;

        if (NULL == parm)
                return 1;
        len = strlen(parm);

        if (len < 2)
                return 1;
        if (chk_slash && ((parm[0] != '/') || (parm[1] == '/')))
                return 1;
        update_string(target, parm, len);
        return 0;
};

#ifdef __cplusplus
}
#endif
