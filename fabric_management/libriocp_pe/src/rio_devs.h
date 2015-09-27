/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RIOCP_PE_VENDOR_H__
#define RIOCP_PE_VENDOR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VIDs & DIDs */
#define RIO_VID_RESERVED	0xffff
#define RIO_DID_RESERVED	0xffff

#define RIO_VID_FREESCALE	0x0002
#define RIO_VID_TUNDRA		0x000d
 #define RIO_DID_TSI500		0x0500
 #define RIO_DID_TSI568		0x0568
 #define RIO_DID_TSI572		0x0572
 #define RIO_DID_TSI574		0x0574
 #define RIO_DID_TSI576		0x0578 /* Same ID as Tsi578 */
 #define RIO_DID_TSI577		0x0577
 #define RIO_DID_TSI578		0x0578
#define RIO_VID_TI		0x0030
#define RIO_VID_IDT		0x0038
 #define RIO_DID_IDT_70K200	0x0310
 #define RIO_DID_IDT_CPS8	0x035c
 #define RIO_DID_IDT_CPS12	0x035d
 #define RIO_DID_IDT_CPS16	0x035b
 #define RIO_DID_IDT_CPS6Q	0x035f
 #define RIO_DID_IDT_CPS10Q	0x035e
 #define RIO_DID_IDT_CPS1848	0x0374
 #define RIO_DID_IDT_CPS1432	0x0375
 #define RIO_DID_IDT_CPS1616	0x0379
 #define RIO_DID_IDT_VPS1616	0x0377
 #define RIO_DID_IDT_SPS1616	0x0378
 #define RIO_DID_IDT_TSI721	0x80ab
#define RIO_VID_PRODRIVE	0x00a4
#define RIO_VID_NOKIA 0x0076
 #define RIO_DID_NOKIA_STC 0x0001
#define RIO_VID_LSI 0x000a
 #define RIO_DID_LSI_AXM5516 0x5120

/* Prodrive custom */
#define RIO_ASSY_IDENT_CAR_ASSY_PRIOCG2		0x70000
#define RIO_ASSY_IDENT_CAR_VENDOR_PRODRIVE	0xa4

struct riocp_pe_vendor {
	uint16_t vid;
	const char *vendor;
};

struct riocp_pe_dev_id {
	uint16_t vid;
	uint16_t did;
	const char *name;
};

static const struct riocp_pe_vendor riocp_pe_vendors[] = {
	{0x0000,		"Reserved"},
	{0x0001,		"Mercury Computer Systems"},
	{RIO_VID_FREESCALE,	"Freescale"},
	{0x0003,		"Alcatel Corporation"},
	{0x0005,		"EMC Corporation"},
	{0x0006,		"Ericsson"},
	{0x0007,		"Alcatel-Lucent Technologies"},
	{0x0008,		"Nortel Networks"},
	{0x0009,		"Altera"},
	{RIO_VID_LSI,		"LSI Corporation"},
	{0x000b,		"Rydal Research"},
	{RIO_VID_TUNDRA,	"Tundra Semiconductor"},
	{0x000e,		"Xilinx"},
	{0x0019,		"Curtiss-Wright Controls Embedded Computing"},
	{0x001f,		"Raytheon Company"},
	{0x0028,		"VMetro"},
	{RIO_VID_TI,		"Texas Instruments"},
	{0x0035,		"Cypress Semiconductor"},
	{0x0037,		"Cadence Design Systems"},
	{RIO_VID_IDT,		"Integrated Device Technology"},
	{0x003d,		"Thales Computer"},
	{0x003f,		"Praesum Communications"},
	{0x0040,		"Lattice Semiconductor"},
	{0x0041,		"Honeywell Inc."},
	{0x005a,		"Jennic, Inc."},
	{0x0064,		"AMCC"},
	{0x0066,		"GDA Technologies"},
	{0x006a,		"Fabric Embedded Tools Corporation"},
	{0x006c,		"Silicon Turnkey Express"},
	{0x006e,		"Micro Memory"},
	{0x0072,		"PA Semi, Inc."},
	{0x0074,		"SRISA - Scientific Research Inst for System Analysis"},
	{RIO_VID_NOKIA,		"Nokia Networks"},
	{0x007c,		"Hisilicon Technologies Co."},
	{0x007e,		"Creatuve Electronix Systems"},
	{0x0080,		"ELVEES"},
	{0x0082,		"GE Fanuc Embedded Systems"},
	{0x0084,		"Wintegra"},
	{0x0088,		"HDL Design House"},
	{0x008a,		"Motorola"},
	{0x008c,		"Cavium Networks"},
	{0x008e,		"Mindspeed Technologies"},
	{0x0094,		"Eclipse Electronic Systems, Inc."},
	{0x009a,		"Sandia National Laboratories"},
	{0x009e,		"HCL Technologies, Ltd."},
	{0x00a2,		"ASML"},
	{RIO_VID_PRODRIVE,	"Prodrive Technologies"},
	{0x00a6,		"BAE Systems"},
	{0x00a8,		"Broadcom"},
	{0x00aa,		"Mobiveil, Inc."},
	{0xffff,		"Reserved"},
};

static const struct riocp_pe_dev_id riocp_pe_device_ids[] = {
	/* Prodrive */
	{0x0000, 0x5130, "QHA (domo capable)"},
	{0x0000, 0x5131, "QHA"},
	{0x0000, 0x5148, "QHA"},
	{0x0000, 0x4130, "AMCBTB"},
	{0x0000, 0x4131, "AMCBTB"},
	{0x0000, 0x0001, "SMA"},
	{0x0000, 0x534d, "SMA"},

	/* Freescale*/
	{RIO_VID_FREESCALE, 0x0012, "MPC8548E"},
	{RIO_VID_FREESCALE, 0x0013, "MPC8548"},
	{RIO_VID_FREESCALE, 0x0014, "MPC8543E"},
	{RIO_VID_FREESCALE, 0x0015, "MPC8543"},
	{RIO_VID_FREESCALE, 0x0018, "MPC8547E"},
	{RIO_VID_FREESCALE, 0x0019, "MPC8545E"},
	{RIO_VID_FREESCALE, 0x001a, "MPC8545"},
	{RIO_VID_FREESCALE, 0x0400, "P4080E"},
	{RIO_VID_FREESCALE, 0x0401, "P4080"},
	{RIO_VID_FREESCALE, 0x0408, "P4040E"},
	{RIO_VID_FREESCALE, 0x0409, "P4040"},
	{RIO_VID_FREESCALE, 0x0420, "P5020E"},
	{RIO_VID_FREESCALE, 0x0421, "P5020"},
	{RIO_VID_FREESCALE, 0x0428, "P5010E"},
	{RIO_VID_FREESCALE, 0x0429, "P5010"},
	{RIO_VID_FREESCALE, 0x1810, "MSC8151, MSC8152, MSC8154, MSC8251, MSC8252 or MSC8254"},
	{RIO_VID_FREESCALE, 0x1812, "MSC8154E"},
	{RIO_VID_FREESCALE, 0x1818, "MSC8156 or MSC8256"},
	{RIO_VID_FREESCALE, 0x181a, "MSC8156E"},

	/* Tundra */
	{RIO_VID_TUNDRA, RIO_DID_TSI500,	"Tsi500"},
	{RIO_VID_TUNDRA, RIO_DID_TSI568,	"Tsi568"},
	{RIO_VID_TUNDRA, RIO_DID_TSI572,	"Tsi572"},
	{RIO_VID_TUNDRA, RIO_DID_TSI574,	"Tsi574"},
	{RIO_VID_TUNDRA, RIO_DID_TSI577,	"Tsi577"},
	{RIO_VID_TUNDRA, RIO_DID_TSI576,	"Tsi576"},
	{RIO_VID_TUNDRA, RIO_DID_TSI578,	"Tsi578"},

	/* IDT */
	{RIO_VID_IDT, RIO_DID_IDT_70K200,	"70K200"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS8,		"CPS8"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS12,	"CPS12"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS16,	"CPS16"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS6Q,	"CPS6Q"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS10Q,	"CPS10Q"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS1432,	"CPS1432"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS1848,	"CPS1848"},
	{RIO_VID_IDT, RIO_DID_IDT_CPS1616,	"CPS1616"},
	{RIO_VID_IDT, RIO_DID_IDT_SPS1616,	"SPS1616"},
	{RIO_VID_IDT, RIO_DID_IDT_VPS1616,	"VPS1616"},
	{RIO_VID_IDT, RIO_DID_IDT_TSI721,	"Tsi721"},

	/* Texas Instruments */
	{RIO_VID_TI, 0x009e, "TMS320C6678"},
	{RIO_VID_TI, 0xb981, "66AK2H12/06"},

	/* LSI Corporation */
	{RIO_VID_LSI, RIO_DID_LSI_AXM5516, "AXM5516"},

	/* Nokia Networks */
	{RIO_VID_NOKIA, RIO_DID_NOKIA_STC, "SRIO Tracer"},

	/* End of list */
	{RIO_VID_RESERVED, RIO_DID_RESERVED, "Unknown"},
};

#ifdef __cplusplus
}
#endif

#endif /* RIOCP_PE_VENDOR_H__ */