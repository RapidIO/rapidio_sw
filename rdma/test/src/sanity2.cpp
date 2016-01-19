#include <iostream>

#include "librdma.h"

using namespace std;

int main()
{
	auto rc = 0;

	/* Open mso */
	mso_h	msoh;

	cout << "Press any key to open mso\n";
	cin.get();
	rc = rdma_open_mso_h("sherif", &msoh);
	if (rc)
		cerr << "open_mso_h() failed" << endl;
	else
		cout << "msoh opened successfully" << endl;

	/* Open ms */
	ms_h	msh;
	uint32_t msh_size;
	cout << "Press any key to open mso\n";
	cin.get();
	rc = rdma_open_ms_h("fareed", msoh, 0, &msh_size, &msh);
	if (rc)
		cerr << "open_mso_h() failed" << endl;
	else
		cout << "msh opened successfully" << endl;

	cout << "Press any key to close mso\n";
	cin.get();
	rc = rdma_close_mso_h(msoh);
	if (rc)
		cerr << "close_mso_h() failed" << endl;
	else
		cout << "msoh closed successfully" << endl;

	cout << "Press any key to EXIT\n";
	cin.get();
	return 0;
}
