#include <cstdio>
#include <cstdlib>

#include "tok_parse.h"
#include "msg_q.h"


struct msg_t {
	char s[100];
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		puts("msg_q1 <num_of_iterations>");
		exit(1);
	}

	/* Number of iterations */
	uint32_t n;
	if (tok_parse_ul(argv[1], &n, 0)) {
		printf(TOK_ERR_UL_HEX_MSG_FMT, "Number of repetitions");
	}

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

	struct timespec tm;


	for (unsigned i = 0; i < n; i++) {
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += 1;
		int ret = q1->timed_receive(&tm);
		if (ret == MSG_Q_TIMED_RECV_ERR)
			continue;
		printf("%s", msg->s);
	}
	puts("");

	delete q1;

	return 0;
}
