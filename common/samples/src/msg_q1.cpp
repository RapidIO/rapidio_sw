#include <cstdio>
#include <cstdlib>

#include "msg_q.h"


struct msg_t {
	char s[100];
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		puts("msg_q2 <num_of_iterations>");
		exit(1);
	}

	/* Number of iterations */
	unsigned n = atoi(argv[1]);

	/* Initialize the logger */
	rdma_log_init("msg_mq1.log", 0);

	string qname = string("space1");
	qname.insert(0, 1, '/');

	msg_q<msg_t>	*q1;
	try {
		q1 = new msg_q<msg_t>(qname, MQ_CREATE);
	}
	catch(msg_q_exception& e) {
		puts(e.msg.c_str());
		return 1;
	}

	msg_t	*msg;
	
	q1->get_recv_buffer(&msg);

	for (unsigned i = 0; i < n; i++) {
		q1->receive();
		printf("%s", msg->s);
	}
	puts("");

	delete q1;

	puts("Press ENTER to quit");
	getchar();

	return 0;
}
