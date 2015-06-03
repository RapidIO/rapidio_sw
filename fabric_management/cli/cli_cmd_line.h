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

#ifndef __CLI_CMD_LINE_H__
#define __CLI_CMD_LINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __CLI_WINDOWS__
#include <windows.h>
#include <io.h>
#include "tsi721api.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

extern char *delimiter;

#define BUFLEN 512

#define PROMPTLEN 29

struct cli_env {
	int	sess_socket;	/* Socket to use for input/output.
				 *    Only valid if >0
				 */
	char	*script;	/* Name of script file to be processed.
				 *    If null, process commands from stdin.
				 */
	FILE	*fout;		/* Log file to be written to.
			 	 *    If null, output is only written to stdout.
				 */
	char	prompt[PROMPTLEN+1]; 	/* Prompt to print at command line,
					 * preserve last byte for NULL termination.
					 */
	char	output[BUFLEN];  /* Buffered output string for echo */
	char    input[BUFLEN];   /* Input read from socket etc */
	int	DebugLevel;      /* Debug level of the current environment */
	int	progressState;   /* Use to output spinning bar progress
				  * execution
				  */
	void   *h;		/* Device which is the subject of 
				 	 *    the current command.
				 	 */
	const struct cli_cmd *cmd_prev; /* store last valid command */
};

void splashScreen(struct cli_env *e);

/******************************************************************************
 *  FUNCTION: cli_terminal()
 *
 *  DESCRIPTION:
 *		main UI read and execute loop from the terminal.
 *
 *  PARAMETERS:
 *      None
 *
 *  return VALUE:
 *      0 = no error logged
 *      1 = error logged
 *
 *****************************************************************************/

int cli_terminal(struct cli_env *env);

/******************************************************************************
 *  FUNCTION: logMsg()
 *
 *  DESCRIPTION:
 *    This routine should be used for all command output.
 *
 *    Prints the string found in env->output to stdout.
 *    If env->fout is non-NULL, also prints env->output string to this file.
 *
 *  PARAMETERS:
 *      env - structure which includes file and output string
 *
 *  return VALUE:
 *      nothing
 *
 *****************************************************************************/
void logMsg(struct cli_env *env);

/******************************************************************************
 *  FUNCTION: int bind_cli_cmd_line_cmds()
 *
 *  DESCRIPTION:
 *    This routine adds various command line environment control commands to the
 *       command database.
 *
 *  PARAMETERS:
 *      env - structure which includes file and output string
 *
 *  return VALUE:
 *      nothing
 *
 *****************************************************************************/
int bind_cli_cmd_line_cmds(void);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_CMD_LINE_H__ */
