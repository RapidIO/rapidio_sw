#include <cstdio>
#include <cstring>

#include <string>

#include "msg_q.h"

using namespace std;

struct msg_t {
	char s[100];
};

int main()
{
	string qname = string("space1");
	qname.insert(0, 1, '/');

	msg_q<msg_t> *q1;
	try {
		q1 = new msg_q<msg_t>(qname, MQ_OPEN);
	}
	catch(msg_q_exception e) {
		puts(e.msg.c_str());
		return 1;
	}

	msg_t *msg;

	q1->get_send_buffer(&msg);
	strcpy(msg->s, "Hello, world");

	q1->send();

	puts("Press ENTER to delete message queue");
	getchar();

	delete q1;

	puts("Press ENTER to quit");
	getchar();

	return 0;
}
