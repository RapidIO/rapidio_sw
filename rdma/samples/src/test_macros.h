#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#define CHECK(status,str) if (status) { \
	fprintf(stderr, "%s failed with code %d\n", str, status); \
}

#define CHECK_AND_RET(status,str) if (status) { \
	fprintf(stderr, "%s failed with code %d\n", str, status); \
	return; \
}

#define CHECK_AND_GOTO(status,str,label) if (status) { \
	fprintf(stderr, "%s failed with code %d\n", str, status); \
	goto label; \
}

#endif

