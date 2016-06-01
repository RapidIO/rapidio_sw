/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RIOCP_H_
#define RIOCP_H_

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include <riocp_pe.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_LIB_REV 0
#define RIOCP_WU __attribute__((warn_unused_result))

/* Flags */
#define RIOCP_PE_FLAG_FORCE  (1<<0)  /* Force operation */

#define RIOCP_PE_HANDLE_REV 1

#define ANY_ID 0xff /* @todo To be removed, any_id will be stored in mport handle after get/calculation of sys_size */
#define RIOCP_PE_ANY_PORT 0xff

#define RIOCP_PE_DEV_DIR  "/dev"
#define RIOCP_PE_DEV_NAME "rio_mport"

#define RIOCP_PE_IS_MPORT(pe) ((pe)->minfo) /**< Check if pe is a master port handle */
#define RIOCP_PE_IS_HOST(pe) ((pe)->mport->minfo->is_host) /**< Check if PE is host */
#define RIOCP_PE_DRV_NAME(pe) ((pe)->name) /**< PE driver name */

#define RIOCP_PE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define RIOCP_PE_GET_DESTID(mport, x) \
	((mport)->minfo->prop->sys_size ? (x & 0xffff) : ((x & 0x00ff0000) >> 16))
#define RIOCP_PE_SET_DESTID(mport, x) \
	((mport)->minfo->prop->sys_size ? (x & 0xffff) : ((x & 0x000000ff) << 16))

#define RIOCP_SO_ATTR __attribute__((visibility("default")))

/* RapidIO control plane logging facility */
#ifndef RIOCP_DEBUG_ENABLE
#define RIOCP_DEBUG_ENABLE 1
#endif

#ifdef RIOCP_DEBUG_ENABLE
/* RapidIO control plane logging facility macros */
#define RIOCP_ERROR(fmt, args...) \
	riocp_log(RIOCP_LOG_ERROR, __func__, __FILE__, __LINE__, fmt, ## args)
#define RIOCP_WARN(fmt, args...) \
	riocp_log(RIOCP_LOG_WARN, __func__, __FILE__, __LINE__, fmt, ## args)
#define RIOCP_INFO(fmt, args...) \
	riocp_log(RIOCP_LOG_INFO, __func__, __FILE__, __LINE__, fmt, ## args)
#define RIOCP_DEBUG(fmt, args...) \
	riocp_log(RIOCP_LOG_DEBUG, __func__, __FILE__, __LINE__, fmt, ## args)
#define RIOCP_TRACE(fmt, args...) \
	riocp_log(RIOCP_LOG_TRACE, __func__, __FILE__, __LINE__, fmt, ## args)
#else
#define RIOCP_ERROR(fmt, args...)
#define RIOCP_WARN(fmt, args...)
#define RIOCP_INFO(fmt, args...)
#define RIOCP_DEBUG(fmt, args...)
#define RIOCP_TRACE(fmt, args...)
#endif

#define riocp_pe_llist_foreach(item, list) \
	for (item = list; item != NULL; item = item->next)

#define riocp_pe_llist_foreach_safe(item, next, list) \
	for (item = list, next = item->next; item->next != NULL; item = next, next = item->next)

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

/* RapidIO port and status */

struct riocp_pe_port_state_t
{
	int port_ok; /** 0 - port not initialized, 1 - port initialized */
	int port_max_width; /** Maximum number of lanes for the port */
	int port_cur_width; /** Current operating width of the port */
	int port_lane_speed; /** Lane speed in Mbaud*/
				/** Values: 1250, 2500, 3125, 5000, 6250 */
	int link_errs; /** 0 - Can exchange packets, 1 - errors stop exchange */
};

/* Structure describing a RapidIO port and its status */
struct riocp_pe_port {
	riocp_pe_handle pe;		/* Owner of this port */
	struct riocp_pe_port *peer;	/* Peer port of this port (NULL=no peer) */
	uint8_t id;			/* Physical port number */
	riocp_pe_port_state_t state;	/* Port state */
};


/*
 * Device Driver Functions
 */

#define ALL_PE_PORTS ((uint8_t)(0xFF))

typedef uint32_t pe_rt_val;
#define RT_VAL_FIRST_PORT	((pe_rt_val)(0x00000000))
#define RT_VAL_LAST_PORT	((pe_rt_val)(0x000000FF))
#define RT_VAL_FIRST_MC		((pe_rt_val)(0x00000100))
#define RT_VAL_LAST_MC		((pe_rt_val)(0x000001FF))
#define RT_VAL_FIRST_NEXT_LVL 	((pe_rt_val)(0x00000200))
#define RT_VAL_LAST_NEXT_LVL 	((pe_rt_val)(0x000002FF))
#define RT_VAL_DROP		((pe_rt_val)(0x00000300))
#define RT_VAL_DEFAULT_ROUTE	((pe_rt_val)(0x00000301))
#define RT_VAL_BAD		((pe_rt_val)(0x0FFFFFFF))

/* Routing table definitions */
typedef uint8_t pe_port_t;
#define RIOCP_PE_ALL_PORTS (pe_port_t)0xff /* Use the global LUT */

/* Routing table entry values */
#define RIOCP_PE_EGRESS_PORT(n)	((pe_rt_val)(RT_VAL_FIRST_PORT + ((n) & 0xff)))
#define RIOCP_PE_MULTICAST_MASK(n) ((pe_rt_val)(RT_VAL_FIRST_MC + ((n) & 0xff)))
#define RIOCP_PE_NEXT_LEVEL_GROUP(n) ((pe_rt_val)(RT_VAL_FIRST_NEXT_LVL + ((n) & 0xff)))
#define RIOCP_PE_NO_ROUTE           ((pe_rt_val)(0x300))
#define RIOCP_PE_DEFAULT_ROUTE      ((pe_rt_val)(0x301))

#define RIOCP_PE_IS_EGRESS_PORT(n)       ((n) <= 0xff)
#define RIOCP_PE_IS_MULTICAST_MASK(n)    ((n) >= 0x100 && (n) <= 0x1ff)
#define RIOCP_PE_IS_NEXT_LEVEL_GROUP(n)  ((n) >= 0x200 && (n) <= 0x2ff)

#define RIOCP_PE_GET_EGRESS_PORT(n)      (RIOCP_PE_IS_EGRESS_PORT(n)?(((n) & 0xff)):RT_VAL_BAD)
#define RIOCP_PE_GET_MULTICAST_MASK(n)   (RIOCP_PE_IS_MULTICAST_MASK(n)?(((n) - 0x100) & 0xff):RT_VAL_BAD)
#define RIOCP_PE_GET_NEXT_LEVEL_GROUP(n) (RIOCP_PE_IS_NEXT_LEVEL_GROUP(n)?(((n) - 0x200) & 0xff):RT_VAL_BAD)



struct riocp_pe_driver {
	int RIOCP_WU (* init_pe)(struct riocp_pe *pe, uint32_t *ct,
				struct riocp_pe *peer, char *name);
	int RIOCP_WU (* init_pe_em)(struct riocp_pe *pe, bool en_em);
	int RIOCP_WU (* destroy_pe)(struct riocp_pe *pe);

	int RIOCP_WU (* recover_port)(struct riocp_pe *pe, pe_port_t port,
				pe_port_t lp_port);
	int RIOCP_WU (* set_port_speed)(struct riocp_pe *pe,
				pe_port_t port, uint32_t lane_speed);
	int RIOCP_WU (* get_port_state)(struct riocp_pe *pe,
				pe_port_t port,
				struct riocp_pe_port_state_t *state);
	int RIOCP_WU (* port_start)(struct riocp_pe *pe, uint8_t port);
	int RIOCP_WU (* port_stop)(struct riocp_pe *pe, uint8_t port);

	int RIOCP_WU (* set_route_entry)(struct riocp_pe *pe,
			pe_port_t port, uint32_t did, pe_rt_val rt_val);
	int RIOCP_WU (* get_route_entry)(struct riocp_pe *pe,
			pe_port_t port, uint32_t did, pe_rt_val *rt_val);
	int RIOCP_WU (* alloc_mcast_mask)(struct riocp_pe *sw, pe_port_t port, 
			pe_rt_val *rt_val, int32_t port_mask);
	int RIOCP_WU (* free_mcast_mask)(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val);
	int RIOCP_WU (* change_mcast_mask)(struct riocp_pe *sw, pe_port_t port,
			pe_rt_val rt_val, uint32_t port_mask);
};

struct riocp_reg_rw_driver {
	int RIOCP_WU (* reg_rd)(struct riocp_pe *pe,
			uint32_t offset, uint32_t *val);
	int RIOCP_WU (* reg_wr)(struct riocp_pe *pe,
			uint32_t offset, uint32_t val);
	int RIOCP_WU (* raw_reg_rd)(struct riocp_pe *pe, 
			uint32_t did, uint8_t hc,
			uint32_t addr, uint32_t *val);
	int RIOCP_WU (* raw_reg_wr)(struct riocp_pe *pe,
			uint32_t did, uint8_t hc,
			uint32_t addr, uint32_t val);
};

int RIOCP_WU riocp_bind_driver(struct riocp_pe_driver *driver);

int RIOCP_WU riocp_pe_handle_set_private(riocp_pe_handle pe, void *data);
int RIOCP_WU riocp_pe_handle_get_private(riocp_pe_handle pe, void **data);

/*
 * API functions
 */

/* Discovery and enumeration */
int RIOCP_WU riocp_mport_get_port_list(size_t *count, uint8_t **ports);
int RIOCP_WU riocp_mport_free_port_list(uint8_t **ports);
int RIOCP_WU riocp_mport_get_pe_list(riocp_pe_handle mport, size_t *count, riocp_pe_handle *pes[]);
int RIOCP_WU riocp_mport_free_pe_list(riocp_pe_handle *pes[]);

int RIOCP_WU riocp_pe_create_host_handle(riocp_pe_handle *handle, uint8_t mport, unsigned int rev, struct riocp_reg_rw_driver *drv, uint32_t *ct, char *name);
int RIOCP_WU riocp_pe_create_agent_handle(riocp_pe_handle *handle, uint8_t mport, unsigned int rev, struct riocp_reg_rw_driver *drv, uint32_t *ct, char *name);

int RIOCP_WU riocp_pe_discover(riocp_pe_handle pe, uint8_t port,
				riocp_pe_handle *peer, char *name);
int RIOCP_WU riocp_pe_probe(riocp_pe_handle pe, uint8_t port,
				riocp_pe_handle *peer, uint32_t *comptag_in,
				char *name);
int RIOCP_WU riocp_pe_verify(riocp_pe_handle pe);
riocp_pe_handle riocp_pe_peek(riocp_pe_handle pe, uint8_t port);
int RIOCP_WU riocp_pe_restore(riocp_pe_handle pe);
int riocp_pe_destroy_handle(riocp_pe_handle *pe);
int RIOCP_WU riocp_pe_get_capabilities(riocp_pe_handle pe,
	struct riocp_pe_capabilities *capabilities);
int RIOCP_WU riocp_pe_get_ports(riocp_pe_handle pe, struct riocp_pe_port ports[]);
int RIOCP_WU riocp_pe_port_enable(riocp_pe_handle pe, uint8_t port);
int RIOCP_WU riocp_pe_port_disable(riocp_pe_handle pe, uint8_t port);
int RIOCP_WU riocp_pe_lock(riocp_pe_handle pe, int flags);
int RIOCP_WU riocp_pe_unlock(riocp_pe_handle pe);
int RIOCP_WU riocp_pe_get_destid(riocp_pe_handle pe, uint32_t *destid);
int RIOCP_WU riocp_pe_set_destid(riocp_pe_handle pe, uint32_t destid);
int RIOCP_WU riocp_pe_get_comptag(riocp_pe_handle pe, uint32_t *comptag);
int RIOCP_WU riocp_pe_update_comptag(riocp_pe_handle pe, uint32_t *comptag,
					uint32_t did, uint32_t wr_did);

int RIOCP_WU riocp_pe_clear_enumerated(struct riocp_pe *pe);

/* Routing */
int RIOCP_WU riocp_sw_get_route_entry(riocp_pe_handle sw, pe_port_t port,
		uint32_t destid, pe_rt_val *rt_val);
int RIOCP_WU riocp_sw_set_route_entry(riocp_pe_handle sw, pe_port_t port,
		uint32_t destid, pe_rt_val rt_val);
int RIOCP_WU riocp_sw_alloc_mcast_mask(riocp_pe_handle sw, pe_port_t port,
		pe_rt_val *rt_val, uint32_t port_mask);
int RIOCP_WU riocp_sw_free_mcast_mask(riocp_pe_handle sw, pe_port_t port,
		pe_rt_val rt_val);
int RIOCP_WU riocp_sw_change_mcast_mask(riocp_pe_handle sw, pe_port_t port,
		pe_rt_val rt_val, uint32_t port_mask);

/* Debug functions */
int riocp_pe_maint_read(riocp_pe_handle pe, uint32_t offset, uint32_t *val);
int riocp_pe_maint_write(riocp_pe_handle pe, uint32_t offset, uint32_t val);
const char *riocp_pe_get_device_name(riocp_pe_handle pe);
const char *riocp_pe_get_vendor_name(riocp_pe_handle pe);


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

/** Linked list item */
struct riocp_pe_llist_item {
	void *data; /**< Item opaque data */
	struct riocp_pe_llist_item *next; /**< Next element */
};

/** RapidIO Processing element peer */
struct riocp_pe_peer {
	struct riocp_pe *peer; /**< Pointer to peer handle */
	uint8_t remote_port;   /**< Remote port of peer */
};

/** RapidIO Master port information */
struct riocp_pe_mport {
	uint32_t ref;				/**< Reference counter */
	uint8_t id;					/**< Device node id e.g /dev/rio_mport0 */
	bool is_host;				/**< Is mport host/agent */
	struct riocp_pe *any_id_target;		/**< Current programmed ANY_ID route to this PE*/
	struct riocp_pe_llist_item handles;	/**< Handles of PEs behind this mport */
	struct riocp_pe **comptag_pool;		/**< Pool of assigned component tags */
	size_t comptag_pool_size;		/**< Pool size of assigned component tags */
	riocp_reg_rw_driver reg_acc;		/**< Driver to allow local reads and writes of register space, and register reads/writes to other devices from this mport */
	void *private_data;			/**< Mport private data */
};

/** RapidIO Processing element */
struct riocp_pe {
	uint32_t version;			/**< Internal handle revision */
	const char *name;			/**< Name of device */
	uint8_t hopcount;			/**< RapidIO hopcount */
	uint32_t destid;			/**< RapidIO destination ID */
	uint32_t comptag;			/**< RapidIO component tag */
	uint8_t *address;			/**< RapidIO address used to access this PE */
	struct riocp_pe_capabilities cap;	/**< RapidIO Capabilities */
	uint16_t efptr;				/**< RapidIO extended feature pointer */
	uint32_t efptr_phys;			/**< RapidIO Physical extended feature pointer */
	uint32_t efptr_em;			/**< RapidIO Error Management feature pointer */
	struct riocp_pe *mport;			/**< Mport that created this PE */
	struct riocp_pe_mport *minfo;		/**< Mport information (set when PE is mport) */
	struct riocp_pe_peer *peers;		/**< Connected peers (size RIOCP_PE_PORT_COUNT(pe->cap)) */
	struct riocp_pe_port *port;		/**< Port (peer) info of this PE, used in riocp_pe_get_ports peer field */
	void *private_data;			/**< PE private data */
};

/* RapidIO control plane logging facility */
int riocp_log(enum riocp_log_level level, const char *func, const char *file,
	const unsigned int line, const char *format, ...);
int riocp_log_register_callback(enum riocp_log_level level, riocp_log_output_func_t outputfunc);
void riocp_log_exit(void);

/* Handle administration and information */
int RIOCP_WU riocp_pe_handle_check(riocp_pe_handle handle);
int RIOCP_WU riocp_pe_get_peer_list(riocp_pe_handle pe,
	riocp_pe_handle **peer_list, size_t *peer_list_size);
int RIOCP_WU riocp_pe_free_peer_list(riocp_pe_handle *pes[]);
int RIOCP_WU riocp_pe_handle_set_private(riocp_pe_handle pe, void *data);
int RIOCP_WU riocp_pe_handle_get_private(riocp_pe_handle pe, void **data);
int RIOCP_WU riocp_mport_set_private(riocp_pe_handle mport, void *data);
int RIOCP_WU riocp_mport_get_private(riocp_pe_handle mport, void **data);

const char *riocp_pe_handle_get_device_str(riocp_pe_handle pe);
const char *riocp_pe_handle_get_vendor_str(riocp_pe_handle pe);
int riocp_pe_handle_get_list(riocp_pe_handle mport, riocp_pe_handle **pe_list,
        size_t *pe_list_size);
int riocp_pe_handle_get_peer_list(riocp_pe_handle pe,
        riocp_pe_handle **pe_peer_list, size_t *pe_peer_list_size);
int riocp_pe_handle_free_list(riocp_pe_handle **list);

/* Dot graph */
int riocp_pe_dot_dump(char *filename, riocp_pe_handle mport);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_INTERNAL_H__ */
