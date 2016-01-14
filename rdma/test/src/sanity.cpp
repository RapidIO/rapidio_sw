#include <iostream>

#include "librdma.h"

using namespace std;

int main()
{
	auto rc = 0;

	mso_h	msoh;
	rc = rdma_create_mso_h("sherif", &msoh);
	if (rc)
		cerr << "create_mso_h() failed" << endl;
	else
		cout << "msoh created successfully" << endl;

	cout << "Press any key to destroy mso\n";
	cin.get();

	rc = rdma_destroy_mso_h(msoh);
	if (rc)
		cerr << "destroy_mso_h() failed" << endl;
	else
		cout << "msoh destroyed successfully" << endl;

	cout << "Press any key to EXIT\n";
	cin.get();
	return 0;
}
