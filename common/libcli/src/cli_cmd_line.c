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
#ifdef USE_READLINE
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "cli_cmd_db.h"
#include "rdma_logger.h"

void (*cons_cleanup)(struct cli_env *env);

void splashScreen(char *app_name)
{
        printf("-----------------------------------------------------------\n");
        printf("---      %s     ---\n", app_name);

	printf("-----------------------------------------------------------\n");
	printf("---            Version: %2s.%2s (%s-%s)      ---\n",
                CLI_VERSION_YR, CLI_VERSION_MO, __DATE__, __TIME__);
        printf("-----------------------------------------------------------\n");
        printf("\t\tRapidIO Trade Association\n");
        printf("\t\tCopyright 2015\n");

        fflush(stdout);
};

const char *delimiter = " ,\t\n";   /* Input token delimiter */

void logMsg(struct cli_env *env)
{
	uint8_t  use_skt = (env->sess_socket >= 0)?TRUE:FALSE;
	
	if (use_skt) {
		if (write(env->sess_socket, env->output, strlen(env->output)))
			bzero(env->output, BUFLEN-1);
		bzero(env->output, BUFLEN);
	} else {
		printf("%s", env->output);
	}
	if (env->fout != NULL)  /* is logging enabled? */
		fprintf(env->fout, "%s", env->output);
}

int send_cmd(struct cli_env *env, int argc, char **argv,
                        int cmd(struct cli_env *env, char *cmd_line),
                        char *saved_cmd_line, int max_cmd_len)
{
        char *cmd_line = (char *)malloc(max_cmd_len+1);
        int i, len = 0, tlen = 0;

        memset(cmd_line, 0, max_cmd_len+1);
        if (!argc) {
                memcpy(cmd_line, saved_cmd_line, max_cmd_len);
                len = strlen(cmd_line);
        };
        /* ELSE */
        for (i = 0; (i < argc) && (len < (max_cmd_len)); i++) {
                tlen = strlen(argv[i]);
                if ((len + tlen + 1) < (max_cmd_len-1)) {
                        if (i)
                                cmd_line[len++] = ' ';
                        memcpy(&cmd_line[len], argv[i], tlen);
                }
                len += tlen;
        };

        if (len && (len < max_cmd_len)) {
                memcpy(saved_cmd_line, cmd_line, max_cmd_len);
                cmd(env, cmd_line);
        } else {
                sprintf(env->output,
                        "Error: Max %d characters, actual %d, not executed\n",
                        max_cmd_len, len);
                logMsg(env);
        }

        return 0;
};

int process_command(struct cli_env *env, char *input)
{
	char *cmd;
	int rc;
	struct cli_cmd *cmd_p;
	int  argc;
	char *argv[30];
	int   exitStat = 0;

	if (env->fout != NULL)
		fprintf((FILE *) env->fout, "%s", input); /* Log command file */

	cmd = strtok(input, delimiter);	/* Tokenize input array */

	if ((cmd    != NULL) && (cmd[0] != '/') && (cmd[0] != '\n') &&
	    (cmd[0] != '\r')) {
		rc = find_cmd(cmd, &cmd_p);

		if (rc == -1) {
			printf(
			"Unknown command: Type '?' for a list of commands\n");
		} else if (rc == -2) {
			printf(
			"Ambiguous command: Type '?' for command usage\n");
		} else if (!rc && (cmd_p->func != NULL)) {
			argc = 0;
			while ((argv[argc] = strtok(NULL, delimiter)) != NULL)
				argc++;

			if (argc < cmd_p->min_parms) {
				sprintf(env->output,
				"FAILED: Need %d parameters for command \"%s\"",
					   cmd_p->min_parms, cmd_p->name);
				logMsg(env);
				sprintf(env->output,
				"\n%s not executed.\nHelp:\n", cmd_p->name);
				logMsg(env);
				cli_print_help(env, cmd_p);
			} else {
				exitStat = (cmd_p->func(env, argc, argv));
				/* store the command */
				if (cmd_p->attributes & ATTR_RPT) {
					env->cmd_prev = cmd_p;
				} else {
					env->cmd_prev = NULL;
				};
			}
		}
	} else {
		/* empty string passed to CLI */
		if (env->cmd_prev != NULL)
			exitStat = env->cmd_prev->func(env, 0, NULL);
	}

	return exitStat;
}

int cli_terminal(struct cli_env *env)
{
	/* State variables */
	char *input;
#ifndef USE_READLINE
	char  input_buf[BUFLEN];
	int   lastChar = 0;
#endif

	char  skt_inp[BUFLEN];
	unsigned int  errorStat = 0;
	int  use_skt = (env->sess_socket >= 0)?TRUE:FALSE;
	int one = 1;
	int zero = 0;

	while (!errorStat) {
		env->output[0] = '\0';

		if (use_skt) {
     setsockopt (env->sess_socket, IPPROTO_TCP, TCP_CORK, &one, sizeof (one));
			lastChar = write(env->sess_socket, env->prompt, 
				strlen(env->prompt));
			if (lastChar < 0) {
				errorStat = 2;
				goto exit;
			};
     setsockopt (env->sess_socket, IPPROTO_TCP, TCP_CORK, &zero, sizeof (zero));
			bzero(skt_inp,BUFLEN);
			lastChar = read(env->sess_socket, skt_inp, BUFLEN);
			if (lastChar < 0) {
				errorStat = 2;
			} else {
				if (skt_inp[lastChar - 1] == '\n')
					skt_inp[lastChar - 1] = '\0';
     setsockopt (env->sess_socket, IPPROTO_TCP, TCP_CORK, &one, sizeof (one));
				errorStat = process_command(env, skt_inp);
     setsockopt (env->sess_socket, IPPROTO_TCP, TCP_CORK, &zero, sizeof (zero));
			};
		} else {

			fflush(stdin);
#ifdef USE_READLINE
			/* Readline returns a buffer that it allocates.
			 * Free previous buffer if it exists.
			 */
			if (input != NULL) {
				free(input);
				input = NULL;
			}

			input = readline(env->prompt);

			if (input && *input)
				add_history(input);
#else
			/* when using mingw, command line parsing seems to be
			 * built into fgets.  No need for readline in this case
			 */
			input = input_buf;
			input[0] = '\0';
			printf("%s", env->prompt); /* Display prompt */
			if (NULL == fgets(input, BUFLEN - 1, stdin))
				goto exit;
	
			/* have to remove the \n at the end of the line
			 * when using fgets
			 */
			lastChar = strlen(input);
			if (input[lastChar - 1] == '\n')
				input[lastChar - 1] = '\0';
#endif
			errorStat = process_command(env, input);
	
#ifdef USE_READLINE
			free(input);
			input = NULL;
#endif
		}
	}

exit:
	if (use_skt)
     		setsockopt (env->sess_socket, IPPROTO_TCP, TCP_CORK, 
					&zero, sizeof (zero));
	return errorStat;  /* no error */
}

/******************************************************************************
 *  FUNCTION: cli_script()
 *
 *  DESCRIPTION:
 *		main UI read and execute loop from the terminal.
 *
 *  PARAMETERS:
 *      None
 *
 *
 *  return VALUE:
 *      0 = no error logged
 *      1 = error logged
 *
 *****************************************************************************/

int cli_script(struct cli_env *env, char *script, int verbose)
{
	FILE *fin;
	int end = FALSE; /* End of command processing loop? */
	char input[BUFLEN]; /* Input buffer */

	unsigned int  errorStat = 0;
	struct cli_env temp_env = *env;

	temp_env.script = script;

	fin = fopen(script, "r");
	if (fin == NULL) {
		sprintf(&temp_env.output[0],
			"\t/*Error: cannot open file named \"%s\"/\n", script);
		logMsg(&temp_env);
		end = TRUE;
	}

	while (!end && !errorStat) {
		/* Initialize all local variables and flush streams */
		fflush(stdin);
		temp_env.output[0] = '\0';

		if (fgets(input, BUFLEN-1, fin) == NULL) {
			end = TRUE;
			break;
		} else if (verbose) {
			sprintf(&temp_env.output[0], "%s\n", input);
			logMsg(&temp_env);
		};

		errorStat = process_command(&temp_env, input);
	};

	if (fin != NULL)
		fclose(fin);

	return errorStat;
}

int CLIDebugCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc) {
		env->DebugLevel = getHex(argv[0], 0);
	};
	sprintf(env->output, "Debug level: %d\n", env->DebugLevel);
	logMsg(env);
	return 0;
}

struct cli_cmd CLIDebug = {
"debug",
1,
0,
"Set/query CLI debug output level",
"<debug level>\n"
	"<debug level> is the new debug level.\n"
	"If <debug_level> is omitted, print the current debug level.\n",
CLIDebugCmd,
ATTR_NONE
};

int CLIOpenLogFileCmd(struct cli_env *env, int argc, char **argv);

struct cli_cmd CLIOpenLogFile = {
"log",
3,
1,
"Open a log file",
"<file name> <verbose>\n"
"<file name> is the log file name.  No spaces are allowed in the name.\n"
	"<verbose> is 0 or non-zero, and controls how much detail is\n"
	"printed to the log file.\n",
CLIOpenLogFileCmd,
ATTR_NONE
};

int CLIOpenLogFileCmd(struct cli_env *env, int argc, char **argv)
{
	int errorStat = 0;

	if (argc > 1) {
		sprintf(env->output, "FAILED: Extra parms ignored: \"%s\"\n",
					   argv[1]);
				logMsg(env);
		cli_print_help(env, &CLIOpenLogFile);
		goto exit;
	};
	if (env->fout != NULL) {
		fclose(env->fout);
		env->fout = NULL;
	}
	/* Make sure cariage return is not included in string,
	 * otherwise fopen will fail
	 */
	if (argv[0][strlen(argv[0])-1] == 0xD)
		argv[0][strlen(argv[0])-1] = '\0';

	env->fout = fopen(argv[0], "w"); /* Open log file for writing */
	if (env->fout == NULL) {
		sprintf(env->output,
			"\t/*FAILED: Log file \"%s\" could not be opened*/\n",
			argv[0]);
		logMsg(env);
		errorStat = 1;
	} else {
		sprintf(env->output, "\t/*Log file %s opened*/\n", argv[0]);
		logMsg(env);
	}
exit:
	return errorStat;
}

int CLICloseLogFileCmd(struct cli_env *env, int argc, char **argv);

struct cli_cmd CLICloseLogFile = {
"close",
1,
0,
"Close a currently open log file",
"No Parameters\n"
	"Close the currently open log file, if one exists.",
CLICloseLogFileCmd,
ATTR_NONE
};


int CLICloseLogFileCmd(struct cli_env *env, int argc, char **argv)
{
	int errorStat = 0;

	if (argc > 1) {
		sprintf(env->output, "FAILED: Extra parms ignored: \"%s\"\n",
					   argv[1]);
				logMsg(env);
		cli_print_help(env, &CLICloseLogFile);
		goto exit;
	};

	if (env->fout == NULL) {
		sprintf(env->output, "\t/*FAILED: No log file to close */\n");
		logMsg(env);
		errorStat = 1;
	} else {
		sprintf(env->output, "\t/*Log file closed*/\n");
		logMsg(env);
		fclose(env->fout); /* Finish writes to file before closing */
		env->fout = NULL;
	}
exit:
	return errorStat;
}

int CLIScriptCmd(struct cli_env *env, int argc, char **argv)
{
	int errorStat = 0, i;
	int verbose = 0;

	if (argc > 1) {
		verbose = getHex(argv[0], 0);
	};

	sprintf(env->output, "\t/*Script: %s ", argv[0]);
	logMsg(env);

	for (i = 1; i < argc; i++) {
		sprintf(env->output, " %s", argv[i]);
		logMsg(env);
	}

	sprintf(env->output, "*/\n");
	logMsg(env);

	errorStat = cli_script(env, argv[0], verbose);

	env->output[0] = '\0';
	sprintf(env->output, "script %s completed, status %x\n",
		argv[0], errorStat);
	logMsg(env);

	return errorStat;
}

struct cli_cmd CLIRScript = {
"script",
3,
1,
"execute commands in a script file",
"<filename> <verbose>\n"
	"<filename> : File name of the script\n"
	"<verbose>  : zero/non-zero value to control amount of output.",
CLIScriptCmd,
ATTR_NONE
};

int CLIQuitCmd(struct cli_env *env, int argc, char **argv);

struct cli_cmd CLIQuit = {
"quit",
4,
0,
"exit the CLI session",
"No Parameters\n"
"Exit from the current CLI",
CLIQuitCmd,
ATTR_NONE
};

int CLIQuitCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc) {
		sprintf(env->output, "FAILED: Extra parms ignored: \"%s\"\n",
					   argv[0]);
				logMsg(env);
		cli_print_help(env, &CLIQuit);
		goto exit;
	};
	(*cons_cleanup)(env);
	return 1;
exit:
	return 0;
}


void printProgress(struct cli_env *env)

{
	char progress[] = {'/', '-', '|', '-', '\\'};

	printf("%c\b", progress[env->progressState++]);
	if (env->progressState >= sizeof(progress) / sizeof(progress[0]))
		env->progressState = 0;
}

struct cli_cmd *cmd_line_cmds[] = {
&CLIDebug,
&CLIOpenLogFile,
&CLICloseLogFile,
&CLIRScript,
&CLIQuit
};

int bind_cli_cmd_line_cmds(void)
{
	add_commands_to_cmd_db(sizeof(cmd_line_cmds)/sizeof(struct cli_cmd *),
				cmd_line_cmds);
	return 0;
};
