/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file riocp_pe_internal.h
 * Internal API for RapidIO processing element manager
 */
#ifndef RIOCP_PE_INTERNAL_H__
#define RIOCP_PE_INTERNAL_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <riocp_pe.h>
#include <rapidio_mport_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIOCP_PE_HANDLE_REV 1

#define RIOCP_PE_ANY_PORT 0xff

#define RIOCP_PE_IS_MPORT(pe) ((pe)->minfo) /**< Check if pe is a master port handle */
#define RIOCP_PE_IS_HOST(pe) ((pe)->mport->minfo->is_host) /**< Check if PE is host */
#define RIOCP_SW_DRV_NAME(pe) ((pe)->sw->name) /**< Switch driver name */
/* lightweight shifts instead of pow(), should work with positive numbers only */
#define RIOCP_PE_ANY_ID(pe) (uint32_t)((1L<<((1<<(pe)->mport->minfo->prop.sys_size)*8))-1) /**< ANY_ID based on PE mport sys_size */

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
	riomp_mport_t maint;		/**< Maintenance access handle for mport e.g /dev/rio_mport0 */
	struct riomp_mgmt_mport_properties prop;	/**< Mport properties */
	bool is_host;				/**< Is mport host/agent */
	struct riocp_pe *any_id_target;		/**< Current programmed ANY_ID route to this PE*/
	struct riocp_pe_llist_item handles;	/**< Handles of PEs behind this mport */
	struct riocp_pe **comptag_pool;		/**< Pool of assigned component tags */
	size_t comptag_pool_size;		/**< Pool size of assigned component tags */
	void *private_data;			/**< Mport private data */
};

/** RapidIO Processing element switch driver */
struct riocp_pe_switch {
	int fd;			/**< File descriptor for switch library events */
	const char *name;	/**< Name of switch */
	void *private_data;	/**< Switch driver implementation specific private data */
	struct riocp_pe_device_id *id_table; /**< Driver support for matching DID/VID */
	int (*init)(struct riocp_pe *sw);
	int (*init_em)(struct riocp_pe *sw);
	int (*set_route_entry)(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint8_t port);
	int (*get_route_entry)(struct riocp_pe *sw, uint8_t lut, uint32_t destid, uint8_t *port);
	int (*clear_lut)(struct riocp_pe *sw, uint8_t lut);
	int (*get_lane_speed)(struct riocp_pe *sw, uint8_t port, uint32_t *speed);
	int (*get_lane_width)(struct riocp_pe *sw, uint8_t port, uint8_t *width);
	int (*get_port_state)(struct riocp_pe *sw, uint8_t port, riocp_pe_port_state_t *state);
	int (*event_handler)(struct riocp_pe *sw, struct riomp_mgmt_event *revent, struct riocp_pe_event *event);
	int (*destroy)(struct riocp_pe *sw);	/**< Cleanup of switch driver */
	int (*set_port_speed)(struct riocp_pe *sw, uint8_t port, enum riocp_pe_speed speed);
};

/** RapidIO Processing element */
struct riocp_pe {
	uint32_t version;			/**< Internal handle revision */
	riomp_mport_t mp_hnd;   	/**< mport handle for this PE */
	uint8_t hopcount;			/**< RapidIO hopcount */
	uint32_t destid;			/**< RapidIO destination ID */
	uint32_t comptag;			/**< RapidIO component tag */
	uint8_t *address;			/**< RapidIO address used to access this PE */
	struct riocp_pe_capabilities cap;	/**< RapidIO Capabilities */
	uint16_t efptr;				/**< RapidIO extended feature pointer */
	uint32_t efptr_phys;			/**< RapidIO Physical extended feature pointer */
//	uint32_t efptr_em;			/**< RapidIO Error Management feature pointer */
	riocp_pe_event_mask_t *port_event_mask;	/**< Event mask */
	//struct rio_pw_filter pwfilter; 		/**< TODO Mport driver port-write filter */
	struct riocp_pe *mport;			/**< Mport that created this PE */
	struct riocp_pe_mport *minfo;		/**< Mport information (set when PE is mport) */
	struct riocp_pe_switch *sw;		/**< Switch specific operations */
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
int riocp_pe_handle_set_private(riocp_pe_handle pe, void *data);
int riocp_pe_handle_get_private(riocp_pe_handle pe, void **data);

/* Dot graph */
int riocp_pe_dot_dump(char *filename, riocp_pe_handle mport);

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_INTERNAL_H__ */
