/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RIOCP_H_
#define RIOCP_H_

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_LIB_REV 0
#define RIOCP_WU __attribute__((warn_unused_result))

/* Processing element features */
#define RIOCP_PE_PEF_STD_RT            (1<<8)      /* Standard routing */
#define RIOCP_PE_PEF_EXT_RT            (1<<9)      /* Extended routing */
#define RIOCP_PE_PEF_SWITCH            (1<<28)
#define RIOCP_PE_PEF_PROCESSOR         (1<<29)
#define RIOCP_PE_PEF_MEMORY            (1<<30)
#define RIOCP_PE_PEF_BRIDGE            (1<<31)
#define RIOCP_PE_IS_SWITCH(cap)        ((cap).pe_feat & RIOCP_PE_PEF_SWITCH)
#define RIOCP_PE_IS_PROCESSOR(cap)     ((cap).pe_feat & RIOCP_PE_PEF_PROCESSOR)
#define RIOCP_PE_IS_MEMORY(cap)        ((cap).pe_feat & RIOCP_PE_PEF_MEMORY)
#define RIOCP_PE_IS_BRIDGE(cap)        ((cap).pe_feat & RIOCP_PE_PEF_BRIDGE)
#define RIOCP_PE_PORT_COUNT(cap) \
	(RIOCP_PE_IS_SWITCH(cap) ? (cap).sw_port >> 8 & 0xff : 1)
#define RIOCP_PE_VID(cap)              ((cap).dev_id & 0xffff)
#define RIOCP_PE_DID(cap)              (((cap).dev_id >> 16) & 0xffff)
#define RIOCP_PE_SW_PORT(cap)          ((cap).sw_port & 0xff)

/* Flags */
#define RIOCP_PE_FLAG_FORCE  (1<<0)  /* Force operation */

/* Routing table definitions */
#define RIOCP_PE_ANY_PORT 0xff /* Use the global LUT */

/* Routing table entry values */
#define RIOCP_PE_EGRESS_PORT(n)          (0x000 + ((n) & 0xff)) /* Egress Port Number */
#define RIOCP_PE_MULTICAST_MASK(n)       (0x100 + ((n) & 0xff)) /* Multicast Mask Number */
#define RIOCP_PE_NEXT_LEVEL_GROUP(n)     (0x200 + ((n) & 0xff)) /* Next Level Group Number */
#define RIOCP_PE_NO_ROUTE                (0x300)                /* Drop packet */
#define RIOCP_PE_DEFAULT_ROUTE           (0x301)                /* Use default route */

#define RIOCP_PE_IS_EGRESS_PORT(n)       ((n) <= 0xff)
#define RIOCP_PE_IS_MULTICAST_MASK(n)    ((n) >= 0x100 && (n) <= 0x1ff)
#define RIOCP_PE_IS_NEXT_LEVEL_GROUP(n)  ((n) >= 0x200 && (n) <= 0x2ff)

#define RIOCP_PE_GET_EGRESS_PORT(n)      ((n) & 0xff)
#define RIOCP_PE_GET_MULTICAST_MASK(n)   (((n) - 0x100) & 0xff)
#define RIOCP_PE_GET_NEXT_LEVEL_GROUP(n) (((n) - 0x200) & 0xff)

/* Structure describing standard RapidIO capabilities registers (CARs) */
struct riocp_pe_capabilities {
	uint32_t dev_id;         /* 0x00 Device identity */
	uint32_t dev_info;       /* 0x04 Device information */
	uint32_t asbly_id;       /* 0x08 Assembly identity */
	uint32_t asbly_info;     /* 0x0c Assembly information */
	uint32_t pe_feat;        /* 0x10 Processing element features */
	uint32_t sw_port;        /* 0x14 Switch port information */
	uint32_t src_op;         /* 0x18 Source operation */
	uint32_t dst_op;         /* 0x1c Destination operation */
	uint32_t lut_size;       /* 0x34 Route LUT size */
};

/* Opaque handle for PE objects */
typedef struct riocp_pe *riocp_pe_handle;

/* Maximum port width that libriocp_pe supports */
#define RIOCP_PE_PORT_MAX_WIDTH			(4)

/* RapidIO port and status */
#define RIOCP_PE_PORT_STATE_UNINITIALIZED	(1<<0) /* Port uninitialized */
#define RIOCP_PE_PORT_STATE_OK			(1<<1) /* Port OK */
#define RIOCP_PE_PORT_STATE_ERROR		(1<<2) /* Port in error */
typedef uint32_t riocp_pe_port_state_t;

/* link, lane, port speed defintions */
enum riocp_pe_speed {
	RIOCP_SPEED_AUTO = -1,		/* automatic speed selection */
	RIOCP_SPEED_UNKNOWN = 0,		/* automatic speed selection */
	RIOCP_SPEED_1_25G = 1250,	/* 1.25 GBaud */
	RIOCP_SPEED_2_5G = 2500,	/* 2.5 GBaud */
	RIOCP_SPEED_3_125G = 3125,	/* 3.125 GBaud */
	RIOCP_SPEED_5_0G = 5000,	/* 5.0 GBaud */
	RIOCP_SPEED_6_25G = 6250	/* 6.25 GBaud */
};

/* Supported port speeds  */
#define RIOCP_SUPPORTED_SPEED_UNKNOWN  (0x0000)
#define RIOCP_SUPPORTED_SPEED_6_25G    (0x0001)
#define RIOCP_SUPPORTED_SPEED_5_0G     (0x0002)
#define RIOCP_SUPPORTED_SPEED_3_125G   (0x0004)
#define RIOCP_SUPPORTED_SPEED_2_5G     (0x0008)
#define RIOCP_SUPPORTED_SPEED_1_25G    (0x0010)

/* Structure describing a RapidIO port and its status */
struct riocp_pe_port {
	riocp_pe_handle pe;		/* Owner of this port */
	struct riocp_pe_port *peer;	/* Peer port of this port (NULL=no peer) */
	uint8_t id;			/* Physical port number */
	uint8_t width;			/* Port lane width (x1, x2, x4, ...) */
	enum riocp_pe_speed speed;		/* Lane speed in Mbaud (0=no link, 1250, ...) */
	riocp_pe_port_state_t state;	/* Port state */
};

/*
 * Enumeration describing default routing action for inbound packets
 * that are not matched in LUT
 */
enum riocp_sw_default_route_action {
	RIOCP_SW_DEFAULT_ROUTE_UNICAST,  /* Route to egress port number */
	RIOCP_SW_DEFAULT_ROUTE_DROP      /* Drop packet */
};

/* Port based management events */
#define RIOCP_PE_EVENT_NONE             0      /* no event */
#define RIOCP_PE_EVENT_LINK_UP          (1<<0) /* link up event */
#define RIOCP_PE_EVENT_LINK_DOWN        (1<<1) /* link down event */
#define RIOCP_PE_EVENT_RETRY_LIMIT      (1<<2) /* retry limit event */

typedef uint32_t riocp_pe_event_mask_t;

struct riocp_pe_event {
	uint8_t port;                /* port identifier */
	riocp_pe_event_mask_t event;    /* bitmask of events */
	uint32_t counter;			/* event counter */
};

/** RapidIO control plane loglevels */
enum riocp_log_level {
	RIOCP_LOG_NONE  = 0,
	RIOCP_LOG_ERROR = 1,
	RIOCP_LOG_WARN  = 2,
	RIOCP_LOG_INFO  = 3,
	RIOCP_LOG_DEBUG = 4,
	RIOCP_LOG_TRACE = 5
};

/* RapidIO control plane log callback function */
typedef int (*riocp_log_output_func_t)(enum riocp_log_level, const char *);

/* define the as well the content and the order of the switch capability
 * structures (see below)
 */
#define ALL_CAPABILITIES(CAPABILITY) \
    CAPABILITY(PORT_X_VC0_PA_TX_CNTR) \
    CAPABILITY(PORT_X_VC0_NACK_TX_CNTR) \
    CAPABILITY(PORT_X_VC0_RTRY_TX_CNTR) \
    CAPABILITY(PORT_X_VC0_PKT_TX_CNTR) \
    CAPABILITY(PORT_X_VC0_PA_RX_CNTR) \
    CAPABILITY(PORT_X_VC0_NACK_RX_CNTR) \
    CAPABILITY(PORT_X_VC0_RTRY_RX_CNTR) \
    CAPABILITY(PORT_X_VC0_PKT_RX_CNTR) \
    CAPABILITY(PORT_X_VC0_CPB_TX_CNTR) \
    CAPABILITY(PORT_X_VC0_PKT_DROP_RX_CNTR) \
    CAPABILITY(PORT_X_VC0_PKT_DROP_TX_CNTR) \
    CAPABILITY(PORT_X_VC0_TTL_DROP_CNTR) \
    CAPABILITY(PORT_X_VC0_CRC_LIMIT_DROP_CNTR) \
    CAPABILITY(PORT_X_TRC_MATCH_0) \
    CAPABILITY(PORT_X_TRC_MATCH_1) \
    CAPABILITY(PORT_X_TRC_MATCH_2) \
    CAPABILITY(PORT_X_TRC_MATCH_3) \
    CAPABILITY(PORT_X_FIL_MATCH_0) \
    CAPABILITY(PORT_X_FIL_MATCH_1) \
    CAPABILITY(PORT_X_FIL_MATCH_2) \
    CAPABILITY(PORT_X_FIL_MATCH_3) \
    CAPABILITY(PORT_X_LANE_0_ERR_8B10B) \
    CAPABILITY(PORT_X_LANE_1_ERR_8B10B) \
    CAPABILITY(PORT_X_LANE_2_ERR_8B10B) \
    CAPABILITY(PORT_X_LANE_3_ERR_8B10B) \
    CAPABILITY(LAST_CAPABILITY) \

#define CREATE_CAP_ENUM(CAP_ENUM) CAP_ENUM,
#define CREATE_CAP_STRING(CAP_STRING) #CAP_STRING,

typedef enum riocp_switch_capabilities {
    ALL_CAPABILITIES(CREATE_CAP_ENUM)
} riocp_switch_capabilities_t;

typedef struct cap_if
{
        riocp_switch_capabilities_t idx;
        int (*get_counter)(struct riocp_pe *sw, uint8_t port, uint32_t *counter_val);
} cap_if_t;

#define CREATE_CAP_STRUCT(CAP_STRUCT) \
        {CAP_STRUCT, NULL}, \

extern const char *riocp_switch_cap_desc[];

/* External locking callback function */
typedef int (*riocp_lock_func_t)(int lock);


/** Trace filter match will forward packet to trace port */
#define RIOCP_PE_TRACE_FILTER_FORWARD 1
/** Trace filter match will drop packet */
#define RIOCP_PE_TRACE_FILTER_DROP 2
/** trace filter match will send a port write event to the host */
#define RIOCP_PE_TRACE_FILTER_NOTIFY 4
/** trace port is used exclusive for trace */
#define RIOCP_PE_TRACE_PORT_EXCLUSIVE 1

/** Trace filter capabilities */
struct riocp_pe_trace_filter_caps {
	int match_unit_count;	/**< Amount of trace match units per port */
	int match_unit_words;	/**< Amount of 32 bit words per patch unit */
	uint32_t filter_caps;	/**< capability flags the trace filter is able to do */
	uint32_t port_caps;		/**< capability flags the trace port is able to do */
};

/** Used for undefined serdes value that should not be set */
#define RIOCP_SERDES_NOVAL		(-1000)

struct riocp_pe_serdes_idtgen2_tx {
	int amplitude;
	int pos1_tap;
	int neg1_tap;
};
struct riocp_pe_serdes_idtgen2_rx {
	int dfe_tap0;
	int dfe_tap1;
	int dfe_tap2;
	int dfe_tap3;
	int dfe_tap4;
	int dfe_offs;
};
struct riocp_pe_serdes_idtgen2 {
	struct riocp_pe_serdes_idtgen2_tx tx;
	struct riocp_pe_serdes_idtgen2_rx rx;
};

union riocp_pe_serdes_values {
	struct riocp_pe_serdes_idtgen2 idtgen2;
	/* add more vendors here */
};

/** SERDES data */
struct riocp_pe_serdes {
	enum riocp_pe_speed speed;
	union riocp_pe_serdes_values val[RIOCP_PE_PORT_MAX_WIDTH];
};

/** port write data */
struct riocp_pe_pw_data {
	uint32_t pw[4];
};


/*
 * API functions
 */

/* Discovery and enumeration */
int RIOCP_WU riocp_mport_get_port_list(size_t *count, uint8_t **ports);
int RIOCP_WU riocp_mport_free_port_list(uint8_t **ports);
int RIOCP_WU riocp_mport_get_pe_list(riocp_pe_handle mport, size_t *count, riocp_pe_handle *pes[]);
int RIOCP_WU riocp_mport_free_pe_list(riocp_pe_handle *pes[]);

int RIOCP_WU riocp_pe_create_host_handle(riocp_pe_handle *handle, uint8_t mport, unsigned int rev);
int RIOCP_WU riocp_pe_create_agent_handle(riocp_pe_handle *handle, uint8_t mport, unsigned int rev);
int RIOCP_WU riocp_pe_discover(riocp_pe_handle pe, uint8_t port, riocp_pe_handle *peer);
int RIOCP_WU riocp_pe_probe(riocp_pe_handle pe, uint8_t port, riocp_pe_handle *peer);
int RIOCP_WU riocp_pe_probe_sync(riocp_pe_handle pe, uint8_t port, uint8_t peer_port, riocp_pe_handle *peer);
int RIOCP_WU riocp_pe_verify(riocp_pe_handle pe);
riocp_pe_handle riocp_pe_peek(riocp_pe_handle pe, uint8_t port);
int RIOCP_WU riocp_pe_restore(riocp_pe_handle pe);
int riocp_pe_destroy_handle(riocp_pe_handle *pe);
int RIOCP_WU riocp_pe_get_capabilities(riocp_pe_handle pe,
	struct riocp_pe_capabilities *capabilities);
int RIOCP_WU riocp_pe_get_ports(riocp_pe_handle pe, struct riocp_pe_port ports[]);
int RIOCP_WU riocp_pe_get_port_supported_speeds(riocp_pe_handle pe, uint8_t port, uint8_t *speeds);
int RIOCP_WU riocp_pe_set_port_speed(riocp_pe_handle pe, uint8_t port, enum riocp_pe_speed speed, struct riocp_pe_serdes *serdes);
int RIOCP_WU riocp_pe_lock(riocp_pe_handle pe, int flags);
int RIOCP_WU riocp_pe_unlock(riocp_pe_handle pe);
int RIOCP_WU riocp_pe_get_destid(riocp_pe_handle pe, uint32_t *destid);
int RIOCP_WU riocp_pe_set_destid(riocp_pe_handle pe, uint32_t destid);
int RIOCP_WU riocp_pe_get_comptag(riocp_pe_handle pe, uint32_t *comptag);
int RIOCP_WU riocp_pe_get_hopcount(riocp_pe_handle pe, uint8_t *hopcount);
int RIOCP_WU riocp_pe_update_comptag(riocp_pe_handle pe, uint32_t *comptag, uint32_t did, uint32_t wr_did);
int RIOCP_WU riocp_pe_get_peer_pe(riocp_pe_handle pe, uint8_t port, riocp_pe_handle *peer);

/* Routing */
int RIOCP_WU riocp_sw_get_default_route_action(riocp_pe_handle sw,
		enum riocp_sw_default_route_action *action, uint8_t *port);
int RIOCP_WU riocp_sw_set_default_route_action(riocp_pe_handle sw,
		enum riocp_sw_default_route_action action, uint8_t port);
int RIOCP_WU riocp_sw_get_route_entry(riocp_pe_handle sw, uint8_t lut, uint32_t destid,
		uint16_t *value);
int RIOCP_WU riocp_sw_set_route_entry(riocp_pe_handle sw, uint8_t lut, uint32_t destid,
		uint16_t value);
int RIOCP_WU riocp_sw_clear_lut(riocp_pe_handle sw, uint8_t lut);
int RIOCP_WU riocp_pe_set_sw_domain(riocp_pe_handle sw, uint8_t domain);
int RIOCP_WU riocp_sw_set_port_enable(riocp_pe_handle sw, uint8_t port, bool enable);
int RIOCP_WU riocp_sw_set_multicast_mask(riocp_pe_handle sw, uint8_t lut, uint8_t maskid,
		uint16_t port_mask, bool clear);
int RIOCP_WU riocp_sw_set_congestion_limit(riocp_pe_handle sw, uint8_t port, uint16_t limit);

/* Counters */
int RIOCP_WU riocp_pe_get_sw_counter_capabilites(struct riocp_pe *sw,
        uint8_t port, cap_if_t *caps);
int RIOCP_WU riocp_pe_get_sw_counters(struct riocp_pe *sw, uint8_t port, uint32_t *counter_val,
        uint32_t counter_val_size, cap_if_t *caps, uint32_t caps_cnt);

/* Trace and Filters */
int RIOCP_WU riocp_pe_get_sw_trace_filter_caps(struct riocp_pe *sw, struct riocp_pe_trace_filter_caps *caps);
int RIOCP_WU riocp_pe_set_sw_trace_filter(struct riocp_pe *sw, uint8_t port, uint8_t filter, uint32_t flags, uint32_t *match_val, uint32_t *match_mask);
int RIOCP_WU riocp_pe_set_sw_trace_port(struct riocp_pe *sw, uint8_t port, uint32_t flags);

/* Event management functions */
int RIOCP_WU riocp_pe_get_event_mask(riocp_pe_handle pe, uint8_t port, riocp_pe_event_mask_t *mask);
int RIOCP_WU riocp_pe_set_event_mask(riocp_pe_handle pe, uint8_t port, riocp_pe_event_mask_t mask);
int RIOCP_WU riocp_pe_receive_event(riocp_pe_handle pe, struct riocp_pe_event *e);
int RIOCP_WU riocp_pe_event_mport(riocp_pe_handle mport, riocp_pe_handle *pe,
		struct riocp_pe_event *ev, int timeout);
int RIOCP_WU riocp_pe_get_pw_event_mport(riocp_pe_handle mport, struct riocp_pe_pw_data *pw_ev, int timeout);
int RIOCP_WU riocp_pe_pw_event_to_pe(riocp_pe_handle mport, struct riocp_pe_pw_data *pw_ev, riocp_pe_handle *pe);
int RIOCP_WU riocp_pe_handle_pw_event(struct riocp_pe_pw_data *pw_ev, riocp_pe_handle pe, struct riocp_pe_event *ev);

/* Debug functions */
int riocp_pe_maint_read(riocp_pe_handle pe, uint32_t offset, uint32_t *val);
int riocp_pe_maint_write(riocp_pe_handle pe, uint32_t offset, uint32_t val);
const char *riocp_pe_get_device_name(riocp_pe_handle pe);
const char *riocp_pe_get_vendor_name(riocp_pe_handle pe);

/* PE managment */
int RIOCP_WU riocp_pe_announce(riocp_pe_handle pe);
int RIOCP_WU riocp_pe_revoke(riocp_pe_handle pe);

/* Library synchronization */
int riocp_lock_register_callback(riocp_lock_func_t callback);

int riocp_log_register_callback(enum riocp_log_level level, riocp_log_output_func_t outputfunc);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_H_ */
