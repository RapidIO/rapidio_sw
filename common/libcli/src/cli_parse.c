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

#include "libclidb.h"

int getDecParm(char *token, int defaultData)
{
	unsigned long data;

	if (token == NULL || token[0] == '/')
		data = defaultData;
	else if (sscanf(token, "%ld", &data) <= 0)
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

unsigned long getHex(char *token, unsigned long defaultData)

{
	unsigned long data = defaultData;

	if ((token != NULL) && (token[0] != '/'))
		if (1 != sscanf(token, "%lX", &data))
			data = defaultData;

	return data;
}
