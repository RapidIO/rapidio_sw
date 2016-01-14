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

	cout << "Press any key to continue\n";
	cin.get();
	return 0;
}
