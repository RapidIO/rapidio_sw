#include <iostream>

#include "librdma.h"

using namespace std;

extern "C" /*static*/ int open_mport(void/*struct peer_info *peer*/);

int main()
{
	auto rc = 0;
	rc = open_mport();
	if (rc)
		cerr << "open_mport() failed\n";
	cout << "Press any key to continue\n";
	cin.get();
	return 0;
}
