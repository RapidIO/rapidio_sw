#ifndef __UMBOX_AFU_H__
#define __UMBOX_AFU_H__

#include <stdint.h>

#define AFU_PATH        "/tmp/RIO_CM_DG_"

/** \brief This is the L3 header we use for transporting CM-tagged data over MBOX */
struct DMA_MBOX_L3_s {
	uint16_t destid; // When TX: destid of destination, When RX: destid of source, network order
	uint16_t tag; // network order
} __attribute__ ((packed));
typedef struct DMA_MBOX_L3_s DMA_MBOX_L3_t;

const int DMA_MBOX_L3_SIZE = sizeof(DMA_MBOX_L3_t);

#endif // __UMBOX_AFU_H__
