#ifndef __UDMA_TUN_H__
#define __UDMA_TUN_H__

#include <stdint.h>

struct DmaPeerUpdateRP_s {
  volatile uint32_t RP;
  volatile uint32_t UC; // S/n of last update of peerRP
} __attribute__ ((packed));

typedef struct DmaPeerUpdateRP_s DmaPeerUpdateRP_t;

#define DMAPEER_SIG	0xb000baaaL

/** \brief This is where we keep the local and remote RP */
struct DmaPeerRP_s {
  volatile uint32_t RP; ///< Must keep this at offset &IBWin+0 for NREAD to work
  volatile uint32_t sig;
  volatile uint64_t rpeerLS; ///< Last time we detected peer.UC change. This is a rdtsc ts.
  DmaPeerUpdateRP_t rpeer;
} __attribute__ ((packed));

typedef struct DmaPeerRP_s DmaPeerRP_t;

/** \brief This is the L2 header we use for transporting Tun L3 frames over RIO via DMA */
struct DMA_L2_s {
        uint8_t  RO;     ///< Reader Owned flag(s), not "read only"
        uint16_t destid; ///< Destid of sender in network format, network order
        uint32_t len;    ///< Length of this write, L2+data. Unlike MBOX, DMA is Fu**ed, network order
        uint8_t  padding;///< Barry dixit "It's much better to have headers be a multiple of 4 bytes for RapidIO purposes"
} __attribute__ ((packed));
typedef struct DMA_L2_s DMA_L2_t;

const int DMA_L2_SIZE = sizeof(DMA_L2_t);

/** \brief This is the L2 header we use for transporting IBwin mapping data over MBOX */
struct DMA_MBOX_L2_s {
	uint8_t  mbox_src; ///< MBOX IB BD is borked and this is always 0
        uint16_t destid_src; ///< Destid of sender in network format, network order
        uint16_t destid_dst; ///< Destid of destination in network format, network order
        uint16_t len;    ///< Length of this write, L2+data, network order
} __attribute__ ((packed));
typedef struct DMA_MBOX_L2_s DMA_MBOX_L2_t;

const int DMA_MBOX_L2_SIZE = sizeof(DMA_MBOX_L2_t);

enum { ACTION_ADD = 0, ACTION_DEL = 0xff };

/** \brief These are the IBwin mapping data sent over MBOX */
struct DMA_MBOX_PAYLOAD_s { // All below in network order!
	uint64_t rio_addr;      ///< RIO address allocated for peer
	uint32_t ibwin_size;    ///< Sizeof IBwin mapping
	uint64_t base_rio_addr; ///< IBwin base RIO address -- for documentation purposes
	uint32_t base_size;     ///< IBwin base size -- for documentation purposes
	uint16_t bufc;
        uint32_t MTU;
	uint8_t  action;
} __attribute__ ((packed));

typedef struct DMA_MBOX_PAYLOAD_s DMA_MBOX_PAYLOAD_t;

const int DMA_MBOX_PAYLOAD_SIZE = sizeof(DMA_MBOX_PAYLOAD_t);

bool umd_check_dma_tun_thr_running(struct worker* info);

#endif // __UDMA_TUN_H__
