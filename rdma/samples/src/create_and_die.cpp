#include <cstdio>

#include "librdma.h"
#include "test_macros.h"
#define MSO_NAME	"CREATENDIE"

void run_test() {
	mso_h msoh;
	ms_h msh;
	msub_h msubh;
	int status;

	/* Create owner */
	status = rdma_create_mso_h(MSO_NAME, &msoh);
	CHECK_AND_RET(status, "rdma_create_mso_h");

	puts("Press ENTER to create memory space & subspace");
	getchar();

	/* Create memory space1 */
	status = rdma_create_ms_h("sspace1", msoh, 1024 * 1024, 0, &msh, NULL);
	CHECK_AND_GOTO(status, "rdma_create_ms_h", destroy_msoh);

	/* Create memory sub-space. This subspace will also be created
	 * by rdma_user and at the same offset within the mspace. Then
	 * rdma_user can map the space and access the same data */
	status = rdma_create_msub_h(msh, 0, 4096, 0, &msubh);
	CHECK_AND_GOTO(status, "rdma_create_msub_h", destroy_msoh);

	goto normal_exit;

	destroy_msoh: rdma_destroy_mso_h(msoh);

	normal_exit: return;
}

int main() {
	run_test();
	return 0;
}
