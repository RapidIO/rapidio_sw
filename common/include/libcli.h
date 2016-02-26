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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/* parsing support routines */
extern uint64_t getHexParm(char *dollarParameters[], 
			unsigned int nDollarParms,
			char *token, unsigned int defaultData);

extern unsigned long getHex(char *token, unsigned long defaultData);
int getDecParm(char *token, int defaultData);
float getFloatParm(char *token, float defaultData);

/* Routines to manage environment variables within the CLI */
char* GetEnv(char* var);
void SetEnvVar(char* arg);
char* SubstituteParam(char* arg);

/* parsing support routines that support parameter substitution */
int GetDecParm(char* arg, int dflt);
uint64_t GetHex(char* arg, int dflt);
float GetFloatParm(char* arg, float dflt);

/* CLI initialization/command binding routine.
 * The console_cleanup function is invoked by the "quit" command
 * on exit from the CLI.
 */
extern int cli_init_base(void (*console_cleanup)(struct cli_env *env));

extern int add_commands_to_cmd_db(int num_cmds,
				  struct cli_cmd **cmd_list);

/* Display help for a command */
extern int cli_print_help(struct cli_env *env, struct cli_cmd *cmd);
extern const char *delimiter;

/* Display routines for start of a console or cli_terminal call */
/* NOTE: These messages are sent to stdout */
extern void splashScreen(char *app_name);

/* Send string to one/all of the many output streams supported by cli_env */
extern void logMsg(struct cli_env *env);

/* UNTESTED */
extern int send_cmd(struct cli_env *env, int argc, char **argv, 
			int cmd(struct cli_env *env, char *cmd_line),
			char *saved_cmd_line, int max_cmd_len);

/* Command processing:
 * process_command accepts a string and processes a command, if any is present
 * cli_terminal processes commands from a specified input stream until "quit"
 *              is entered.
 * console runs cli_terminal in a separate thread, accepting commands from
 *          stdin, until "quit" is entered.
 * cli_script executes a script file passed in based on the the environment.
 */
extern int process_command(struct cli_env *env, char *input);

extern int cli_terminal(struct cli_env *env);

extern int cli_script(struct cli_env *env, char *script, int verbose);

/* cons_parm must be "void" to match up with pthread types.
 * The cons_parm should be the prompt string to be used.
 */
void *console(void *cons_parm);

#ifdef __cplusplus
}
#endif

#endif /* __LIBCLIDB_H__ */
