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
#ifndef __LIBCLIDB_H__
#define __LIBCLIDB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#define BUFLEN 512

#define PROMPTLEN 29

#define CLI_VERSION_YR "15"
#define CLI_VERSION_MO "01"

struct cli_cmd;

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
	uint8_t	progressState;   /* Use to output spinning bar progress
				  * execution
				  */
	void   *h;		/* Device which is the subject of 
				 	 *    the current command.
				 	 */
	struct cli_cmd *cmd_prev; /* store last valid command */
};

#define ATTR_NONE  0x0
#define ATTR_RPT   0x1

struct cli_cmd {
	const char *name;
	uint8_t min_match; /* Minimum # of characters to match for this cmd */
	uint8_t min_parms; /* Minimum # of parameters to enter for this cmd */
	const char *shortHelp;
	const char *longHelp;
	int (*func)(struct cli_env *env, int argc, char **argv);
	int attributes;
};

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

/******************************************************************************
 *  FUNCTION: getHexParm()
 *
 *  DESCRIPTION:
 *		parse next token as a hex parameter and return value
 *
 *  PARAMETERS:
 *      parameters    (for $0, $1, ...)
 *      nParms        (number of non-null parameters)
 *      defaultData   data returned if parse successful
 *
 *  return VALUE:
 *      parameter value decoded or defaultValue
 *
 *****************************************************************************/
extern unsigned long getHexParm(char *dollarParameters[], 
			unsigned int nDollarParms,
			char *token, unsigned int defaultData);

extern unsigned long getHex(char *token, unsigned long defaultData);
int getDecParm(char *token, int defaultData);

extern int cli_init_base(void);

extern int add_commands_to_cmd_db(int num_cmds,
				  struct cli_cmd **cmd_list);

extern int cli_print_help(struct cli_env *env, struct cli_cmd *cmd);

extern const char *delimiter;

void __attribute__((weak)) splashScreen(struct cli_env *e);

extern int cli_terminal(struct cli_env *env);

extern void logMsg(struct cli_env *env);
extern int send_cmd(struct cli_env *env, int argc, char **argv, 
			int cmd(struct cli_env *env, char *cmd_line),
			char *saved_cmd_line, int max_cmd_len);

extern int process_command(struct cli_env *env, char *input);

#ifdef __cplusplus
}
#endif

#endif /* __LIBCLIDB_H__ */
