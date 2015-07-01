#ifndef RDMAD_UNIX_MSG_H
#define RDMAD_UNIX_MSG_H

/* Max memory space name length */
#define	UNIX_MS_NAME_MAX_LEN	31

#define CREATE_MSO		0x0001
#define CREATE_MSO_ACK		0x8001
#define CREATE_MS		0x0002
#define CREATE_MS_ACK		0x8002
#define CREATE_MSUB		0x0003
#define CREATE_MSUB_ACK		0x8003
#define ACCEPT_MS		0x0004
#define ACCEPT_MS_ACK		0x8004
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
#define UNDO_ACCEPT		0x000C
#define UNDO_ACCEPT_ACK		0x800C
#define SEND_CONNECT		0x000D
#define SEND_CONNECT_ACK	0x800D
#define UNDO_CONNECT		0x000E
#define UNDO_CONNECT_ACK	0x800E
#define GET_MPORT_ID		0x000F
#define GET_MPORT_ID_ACK	0x800F
#define SEND_DISCONNECT		0x0010
#define SEND_DISCONNECT_ACK	0x8010

/* get_mport_id() arguments */
struct get_mport_id_input {
	int dummy;
};
struct get_mport_id_output {
	int mport_id;
	int status;
};

/* create_mso() arguments */
struct create_mso_input {
	char owner_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct create_mso_output {
	uint32_t msoid;
	int	status;
};

/* open_mso() arguments */
struct open_mso_input {
	char owner_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct open_mso_output {
	uint32_t msoid;
	uint32_t mso_conn_id;
	int	status;
};

/* close_mso() arguments */
struct close_mso_input {
	uint32_t msoid;
	uint32_t mso_conn_id;
};
struct close_mso_output {
	int	status;
};

/* destroy_mso() arguments */
struct destroy_mso_input {
	uint32_t msoid;
};
struct destroy_mso_output {
	int	status;
};

/* create_ms() arguments */
struct create_ms_input {
	char ms_name[UNIX_MS_NAME_MAX_LEN+1];
	uint32_t msoid;
	uint32_t bytes;
	uint32_t flags;
};
struct create_ms_output {
	uint32_t msid;
	int	status;
};

/* open_ms() arguments */
struct open_ms_input {
	char ms_name[UNIX_MS_NAME_MAX_LEN+1];
	uint32_t msoid;
	uint32_t flags;
};
struct open_ms_output {
	uint32_t msid;
	uint32_t ms_conn_id;
	uint32_t bytes;
	int	status;
};

/* close_ms() arguments */
struct close_ms_input {
	uint32_t msid;
	uint32_t ms_conn_id;
};
struct close_ms_output {
	int	status;
};

/* destroy_ms() arguments */
struct destroy_ms_input {
	uint32_t msoid;
	uint32_t msid;
};
struct destroy_ms_output {
	int	status;
};

/* create_msub_h() arguments */
struct create_msub_input {
	uint32_t msid;
	uint32_t offset;
	uint32_t req_bytes;
};
struct create_msub_output {
	int	status;
	uint32_t msubid;
	uint32_t bytes;
	uint64_t rio_addr;
	uint64_t phys_addr;
};

/* destroy_msub() arguments */
struct destroy_msub_input {
	uint32_t msid;
	uint32_t msubid;
};
struct destroy_msub_output {
	int	status;
};


/* accept() arguments */
struct accept_input {
	char loc_ms_name[UNIX_MS_NAME_MAX_LEN+1];
	uint32_t loc_msid;
	uint32_t loc_msubid;
	uint32_t loc_bytes;
	uint8_t	 loc_rio_addr_len;
	uint64_t loc_rio_addr_lo;
	uint8_t  loc_rio_addr_hi;
};

struct accept_output {
	int	status;
};

/* undo_accept() arguments */
struct undo_accept_input {
	char server_ms_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct undo_accept_output {
	int	status;
};

/* send_connect() arguments */
struct send_connect_input {
	char server_msname[UNIX_MS_NAME_MAX_LEN+1];
	uint8_t  server_destid_len;
	uint32_t server_destid;
	uint8_t  client_destid_len;
	uint32_t client_destid;
	uint32_t client_msid;
	uint32_t client_msubid;
	uint32_t client_bytes;
	uint8_t	 client_rio_addr_len;
	uint64_t client_rio_addr_lo;
	uint8_t  client_rio_addr_hi;
};
struct send_connect_output {
	int	status;
};

/* undo_connect() arguments */
struct undo_connect_input {
	char server_ms_name[UNIX_MS_NAME_MAX_LEN+1];
};
struct undo_connect_output {
	int	status;
};

/* send_disconnect() arguments */
struct send_disconnect_input {
	uint32_t loc_msubid;
	uint32_t rem_msid;
	uint32_t rem_destid_len;
	uint32_t rem_destid;
};
struct send_disconnect_output {
	int	status;
};

/* Unix message structure */
struct unix_msg_t {
	uint32_t	type;
	union {
		struct get_mport_id_input	get_mport_id_in;
		struct get_mport_id_output	get_mport_id_out;
		struct create_mso_input		create_mso_in;
		struct create_mso_output	create_mso_out;
		struct open_mso_input		open_mso_in;
		struct open_mso_output		open_mso_out;
		struct close_mso_input		close_mso_in;
		struct close_mso_output 	close_mso_out;
		struct destroy_mso_input	destroy_mso_in;
		struct destroy_mso_output	destroy_mso_out;
		struct create_ms_input		create_ms_in;
		struct create_ms_output		create_ms_out;
		struct open_ms_input		open_ms_in;
		struct open_ms_output		open_ms_out;
		struct close_ms_input		close_ms_in;
		struct close_ms_output		close_ms_out;
		struct destroy_ms_input		destroy_ms_in;
		struct destroy_ms_output	destroy_ms_out;
		struct create_msub_input	create_msub_in;
		struct create_msub_output	create_msub_out;
		struct destroy_msub_input	destroy_msub_in;
		struct destroy_msub_output	destroy_msub_out;
		struct accept_input		accept_in;
		struct accept_output		accept_out;
		struct undo_accept_input	undo_accept_in;
		struct undo_accept_output	undo_accept_out;
		struct send_connect_input	send_connect_in;
		struct send_connect_output	send_connect_out;
		struct undo_connect_input	undo_connect_in;
		struct undo_connect_output	undo_connect_out;
		struct send_disconnect_input	send_disconnect_in;
		struct send_disconnect_output	send_disconnect_out;
	};
};

#endif
