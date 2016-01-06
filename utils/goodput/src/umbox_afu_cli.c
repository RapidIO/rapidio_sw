#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "umbox_afu.h"

#define      CM_TAG   3
const char* CM_CHAN = "/tmp/RIO_CM_DG_3";

int main(int argc, char* argv[])
{
    if (argc < 2) {
       fprintf(stderr, "usage: %s <destid>\n", argv[0]);
       return 1;
    }

    uint8_t buffer[4096] = {0};

    if (access(CM_CHAN, F_OK)) {
        fprintf(stderr, "Socket %s does not exist.\n", CM_CHAN);
        return 1;
    }

    const uint16_t destid = atoi(argv[1]);

    int is_server = 0;
    if (argc > 2 && tolower(argv[2][0]) == 's') is_server = 1;

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        return 1;
    }

    struct sockaddr_un server;
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, CM_CHAN);

    if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
        close(sock);
        perror("connecting stream socket");
        return 1;
    }

    if (! is_server) {
        DMA_MBOX_L3_t* pL3 = (DMA_MBOX_L3_t*)buffer;
        pL3->destid = htons(destid);
        pL3->tag    = htons(CM_TAG);

        uint8_t* pData = buffer + sizeof(DMA_MBOX_L3_t);
        strncpy(pData, "Snow pants first, then your boots. Mittens last", 4096-sizeof(DMA_MBOX_L3_t));
        const int N = sizeof(DMA_MBOX_L3_t) + strlen(pData);

        write(sock, buffer, N);

	memset(buffer, 0, sizeof(buffer));
        int n = read(sock, buffer, 4096); 
        const uint16_t rxdestid = ntohs(pL3->destid);
        printf("Got %d L7 bytes reply from destid %u: %s\n", (n-sizeof(DMA_MBOX_L3_t)), rxdestid, (char*)buffer+sizeof(DMA_MBOX_L3_t));
    } else while(1) {
	memset(buffer, 0, sizeof(buffer));
        int n = read(sock, buffer, 4096); 

        DMA_MBOX_L3_t* pL3 = (DMA_MBOX_L3_t*)buffer;
        const uint16_t destid = ntohs(pL3->destid);
	printf("Got %d L7 bytes from destid %u: %s\n", (n-sizeof(DMA_MBOX_L3_t)), destid, (char*)buffer+sizeof(DMA_MBOX_L3_t));

	char buffer2[4096] = {0};
	pL3 = (DMA_MBOX_L3_t*)buffer2;
        pL3->destid = htons(destid);
        pL3->tag    = htons(CM_TAG);

	uint8_t* pData = buffer2 + sizeof(DMA_MBOX_L3_t);
	strncpy(pData, "ACK: ", 4096-sizeof(DMA_MBOX_L3_t));
	strncat(pData, (char*)buffer+sizeof(DMA_MBOX_L3_t), 4094-sizeof(DMA_MBOX_L3_t));
        const int N = sizeof(DMA_MBOX_L3_t) + strlen(pData);
        const int ntx = write(sock, buffer2, N);
	printf("Replying %d bytes: %s\n", ntx, pData);
    }

    close(sock);

    return 0;
}
