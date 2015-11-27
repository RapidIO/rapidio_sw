#ifndef __UDMA_TUN_H__
#define __UDMA_TUN_H__

#include <stdint.h>

/** \brief This is the L2 header we use for transporting Tun L3 frames over RIO via DMA */
struct DMA_L2_s {
        uint8_t  RO;     ///< Reader Owned flag(s), not "read only"
        uint16_t destid; ///< Destid of sender in network format, network order
        uint32_t len;    ///< Length of this write, L2+data. Unlike MBOX, DMA is Fu**ed, network order
        uint8_t  padding;///< Barry dixit "It's much better to have headers be a multiple of 4 bytes for RapidIO purposes"
} __attribute__ ((packed));
typedef struct DMA_L2_s DMA_L2_t;

const int DMA_L2_SIZE = sizeof(DMA_L2_t);

#endif // __UDMA_TUN_H__
