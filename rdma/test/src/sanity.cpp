#include <iostream>

#include "librdma.h"

using namespace std;

int main()
{
	auto rc = 0;

	/* Create mso */
	mso_h	msoh;
	rc = rdma_create_mso_h("sherif", &msoh);
	if (rc)
		cerr << "create_mso_h() failed" << endl;
	else
		cout << "msoh created successfully" << endl;

	/* Create ms */
	cout << "Press any key to create ms\n";
	cin.get();
	ms_h	msh;
	rc = rdma_create_ms_h("fareed", msoh, 128*1024, 0, &msh, nullptr);
	if (rc)
		cerr << "create_ms_h_failed" << endl;
	else
		cout << "msh created successfully" << endl;

	cout << "Press any key to destroy mso\n";
	cin.get();

	/* Destroy mso */
	rc = rdma_destroy_mso_h(msoh);
	if (rc)
		cerr << "destroy_mso_h() failed" << endl;
	else
		cout << "msoh destroyed successfully" << endl;

	cout << "Press any key to EXIT\n";
	cin.get();
	return 0;
}
