#ifndef BAT_COMMON_H
#define BAT_COMMON_H

/* CM channel to use for test messages */
#define BAT_CM_CHANNEL	9
#define BAT_MPORT_ID	0
#define BAT_MBOX_ID	1

/* Max memory space name length */
#define	CM_MS_NAME_MAX_LEN	31

#define CREATE_MSO		0x0001
#define CREATE_MSO_ACK		0x8001
#define CREATE_MS		0x0002
#define CREATE_MS_ACK		0x8002
#define CREATE_MSUB		0x0003
#define CREATE_MSUB_ACK		0x8003
#define ACCEPT_MS		0x0004
/* No ack for rdma_accept_ms_h() since it is blocking */
#define DESTROY_MSUB		0x0005
#define DESTROY_MSUB_ACK 	0x8005
#define DESTROY_MS		0x0006
#define DESTROY_MS_ACK		0x8006
#define DESTROY_MSO		0x0007
#define DESTROY_MSO_ACK		0x8007
#define OPEN_MSO		0x0008
#define OPEN_MSO_ACK		0x8008
#define CLOSE_MSO		0x0009
#define CLOSE_MSO_ACK		0x8009
#define OPEN_MS			0x000A
#define OPEN_MS_ACK		0x800A
#define CLOSE_MS		0x000B
#define CLOSE_MS_ACK		0x800B
#define KILL_REMOTE_APP		0x000C
#define KILL_REMOTE_DAEMON	0x000D
#define ACCEPT_MS_THREAD	0x000E

/* End the tests */
#define BAT_END		0x8888

/* Create an mso on the server */
struct create_mso_t {
	char name[CM_MS_NAME_MAX_LEN + 1];
};
/* mso created; here is the mso_h */
struct create_mso_ack_t {
	uint64_t	ret;
	uint64_t	msoh;
};

/* Destroy mso on server */
struct destroy_mso_t {
	uint64_t	msoh;
};
/* Ack mso destroyed */
struct destroy_mso_ack_t {
	uint64_t	ret;
};

/* Open mso on a user */
struct open_mso_t {
	char name[CM_MS_NAME_MAX_LEN + 1];
};
/* mso opened; here is the mso_h */
struct open_mso_ack_t {
	uint64_t	ret;
	uint64_t	msoh;
};

/* Close mso on server */
struct close_mso_t {
	uint64_t	msoh;
};
/* Ack mso closed */
struct close_mso_ack_t {
	uint64_t	ret;
};

/* Create an ms with specified name and mso_h */
struct create_ms_t {
	char name[CM_MS_NAME_MAX_LEN + 1];
	uint64_t	msoh;
	uint64_t	req_size;
	uint64_t	flags;
};
/* ms created; here is the ms_h */
struct create_ms_ack_t {
	uint64_t	ret;
	uint64_t	msh;
	uint64_t	size;
};

/* Destroy ms on server */
struct destroy_ms_t {
	uint64_t	ret;
	uint64_t	msoh;
	uint64_t	msh;
};
/* Ack ms destroyed */
struct destroy_ms_ack_t {
	uint64_t	ret;
};

/* Open an ms with specified name and mso_h */
struct open_ms_t {
	char name[CM_MS_NAME_MAX_LEN + 1];
	uint64_t	msoh;
	uint64_t	flags;
};
/* ms opened; here is the ms_h */
struct open_ms_ack_t {
	uint64_t	ret;
	uint64_t	msh;
	uint64_t	size;
};

/* Close ms on user */
struct close_ms_t {
	uint64_t	msoh;
	uint64_t	msh;
};
/* Ack ms closed */
struct close_ms_ack_t {
	uint64_t	ret;
};

/* Create an msub in msh, of specified size */
struct create_msub_t {
	uint64_t	msh;
	uint64_t	offset;
	uint64_t	req_size;
	uint64_t	flags;
};
/* msub created, here is msubh */
struct create_msub_ack_t {
	uint64_t	ret;
	uint64_t	msubh;
};

/* Destroy msub on server */
struct destroy_msub_t {
	uint64_t	msubh;
};
/* Ack msub destroyed */
struct destroy_msub_ack_t {
	uint64_t	ret;
};

/* Initiate accept on specified ms_h */
struct accept_ms_t {
	uint64_t	server_msh;
	uint64_t	server_msubh;
};

/* End of test */
struct bat_end_t {
	uint64_t	dummy;
};

/* BAT message structure */
struct bat_msg_t {
	uint64_t	type;
	union {
		struct create_mso_t create_mso;
		struct create_mso_ack_t create_mso_ack;

		struct destroy_mso_t destroy_mso;
		struct destroy_mso_ack_t destroy_mso_ack;

		struct open_mso_t open_mso;
		struct open_mso_ack_t open_mso_ack;

		struct close_mso_t close_mso;
		struct close_mso_ack_t close_mso_ack;

		struct create_ms_t create_ms;
		struct create_ms_ack_t create_ms_ack;

		struct destroy_ms_t destroy_ms;
		struct destroy_ms_ack_t destroy_ms_ack;

		struct open_ms_t open_ms;
		struct open_ms_ack_t open_ms_ack;

		struct close_ms_t close_ms;
		struct close_ms_ack_t close_ms_ack;

		struct create_msub_t create_msub;
		struct create_msub_ack_t create_msub_ack;

		struct destroy_msub_t destroy_msub;
		struct destroy_msub_ack_t destroy_msub_ack;

		struct accept_ms_t accept_ms;

		struct bat_end_t bat_end;
	};
};

#endif
