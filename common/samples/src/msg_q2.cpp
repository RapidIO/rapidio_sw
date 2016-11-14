#include <cstdio>
#include <cstring>
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
	unsigned n = (unsigned)strtoul(argv[1], NULL, 10);

	/* Initialize the logger */
	rdma_log_init("msg_mq2.log", 0);

	string qname = string("space1");
	qname.insert(0, 1, '/');

	for (unsigned i = 0; i < n; i++ ) {
		msg_q<msg_t> *q1;
		try {
			q1 = new msg_q<msg_t>(qname, MQ_OPEN);
		}
		catch(msg_q_exception& e) {
			printf("i = %u: %s\n", i, e.msg.c_str());
			return 1;
		}

		msg_t *msg;

		q1->get_send_buffer(&msg);

		sprintf(msg->s, "M%u ", i);
		q1->send();

		delete q1;
	}

	return 0;
}
