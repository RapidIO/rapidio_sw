#include <cstdio>

#include "librdma.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		puts("Not enough args");
		return 1;
	}

	puts(argv[1]);

	/* Create a client mso */
	mso_h	client_msoh;

	return rdma_create_mso_h(argv[1], &client_msoh);
}
