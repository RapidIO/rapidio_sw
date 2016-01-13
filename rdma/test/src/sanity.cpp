#include <iostream>

#include "librdma.h"

using namespace std;

extern "C" /*static*/ int open_mport(void/*struct peer_info *peer*/);

int main()
{
	auto rc = 0;
	rc = open_mport();
	if (rc)
		cerr << "open_mport() failed" << endl;

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
