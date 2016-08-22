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
/* LIN_RTA_CLI.cpp : Defines "main" for the Linux console application.  */

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdarg.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "rio_misc.h"
#include <rapidio_mport_dma.h>

#include "libcli.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct cli_cmd CLIConnect;

int CLIConnectCmd(struct cli_env *UNUSED(env), int UNUSED(argc), char **argv)
{
	int sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char tx_buffer[256];
	char rx_buffer[256];
	int one = 1;
	int zero = 0;
	uint8_t session_over = 0;

	server = gethostbyname(argv[0]);
	if (server == NULL) {
		printf("ERROR, host \"%s\" does not exit.\n", argv[0]);
		goto exit;
	}

	portno = atoi(argv[1]);
	printf("\nAttempting connection to host \"%s\" socket %d.\n", argv[0], 
		portno );
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("ERROR opening socket\n");
		goto exit;
	};
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
		(char *)&serv_addr.sin_addr.s_addr,
		server->h_length);

	serv_addr.sin_port = htons(portno);
	setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one));

	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) 
		< 0) {
	        printf("ERROR connecting\n");
		goto cleanup;
	};

	bzero(rx_buffer,256);
	n = read(sockfd,rx_buffer,255);
	while (rx_buffer[n-2] != '\n') {
		printf("%s", rx_buffer);
		bzero((char *) &tx_buffer, sizeof(tx_buffer));
		if (NULL == fgets(tx_buffer,255,stdin)) {
			printf("\nFile closed, closing connection...\n");
			goto cleanup;
		};
		if (!strncmp(tx_buffer, "quit", 4))
			session_over = 1;

		setsockopt (sockfd, IPPROTO_TCP, TCP_CORK, &one, sizeof (one));
		n = write(sockfd,tx_buffer,strlen(tx_buffer));
		if (n < 0) {
			printf("\nERROR writing to socket\n");
			goto cleanup;
		}
		setsockopt (sockfd, IPPROTO_TCP, TCP_CORK, &zero, 
			sizeof (zero));
		if (session_over) {
			printf("\nClosing connection...\n");
			goto cleanup;
		};
		bzero(rx_buffer,256);
		n = read(sockfd,rx_buffer,255);
		if (n < 0) {
			printf("ERROR reading from socket");
			goto cleanup;
		}
		while (rx_buffer[n-2] != '>') {
			printf("%s",rx_buffer);
			bzero((char *) &rx_buffer, sizeof(rx_buffer));
			n = read(sockfd,rx_buffer,255);
			if (n < 0) {
				printf("ERROR reading from socket");
				goto cleanup;
			}
		}
	};
cleanup:
	setsockopt (sockfd, IPPROTO_TCP, TCP_CORK, &zero, sizeof (zero));
	shutdown(sockfd, SHUT_WR);
	close(sockfd);
exit:
	return 0;
};

struct cli_cmd CLIConnect = {
"connect",
3,
2,
"Connects CLI session to File Transfer Server",
"connect <addr> <skt>\n" 
"<addr>: File Transfer Server IP host address\n"
"<skt> : CLI socket number for the File Transfer Server.\n"
"	Enter \"status\" command on server to get the CLI socket number",
CLIConnectCmd,
ATTR_NONE
};

void set_prompt(struct cli_env *e)
{
        if (e != NULL) {
        };
};
int main(int UNUSED(argc), char **UNUSED(argv))
{
	
	struct cli_env env;
	struct cli_cmd *temp_ptr = &CLIConnect;
	

	cli_init_base(NULL);
	add_commands_to_cmd_db(1, &temp_ptr);

        env.script = NULL;
        env.fout = NULL;
        bzero(env.output, BUFLEN);
        bzero(env.input, BUFLEN);
        env.DebugLevel = 0;
        env.progressState = 0;
        env.sess_socket = -1;
        env.cmd_prev = NULL;
        bzero(env.prompt, PROMPTLEN+1);
	strncpy(env.prompt, "RemDbg> ", PROMPTLEN);

	splashScreen((char *)"Remote Debug Session Client");
	cli_terminal(&env);

	exit(EXIT_SUCCESS);
}

#ifdef __cplusplus
}
#endif

