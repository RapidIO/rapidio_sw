/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/
#ifndef __TSI575_RC_H__
#define __TSI575_RC_H__

#ifdef __cplusplus
extern "C" {
#endif

#define Tsi575_RC_NUM_REGS_TO_TEST                                  0x00000001


#ifndef RC_BASE
#define RC_BASE                                                    (0x0001AC80)
#endif


/* ************************************************ */
/* Tsi575 : Register address offset definitions     */
/* ************************************************ */
#define Tsi575_BLK_RST_CTL                               (RC_BASE + 0x00000000)


/* ************************************************ */
/* Tsi575 : Register Bit Masks and Reset Values     */
/*           definitions for every register         */
/* ************************************************ */


/* Tsi575_BLK_RST_CTL : Register Bits Masks Definitions */
#define Tsi575_BLK_RST_CTL_PCI_SELF_RST                            (0x00080000)
#define Tsi575_BLK_RST_CTL_I2C_BOOT                                (0x00100000)
#define Tsi575_BLK_RST_CTL_I2C                                     (0x00200000)
#define Tsi575_BLK_RST_CTL_PGTSW                                   (0x00400000)
#define Tsi575_BLK_RST_CTL_PGTBR                                   (0x00800000)
#define Tsi575_BLK_RST_CTL_CHIP_RESET                              (0x01000000)
#define Tsi575_BLK_RST_CTL_SISF                                    (0x04000000)
#define Tsi575_BLK_RST_CTL_BISF                                    (0x08000000)
#define Tsi575_BLK_RST_CTL_DO_RESET                                (0x10000000)
#define Tsi575_BLK_RST_CTL_PCI                                     (0x20000000)
#define Tsi575_BLK_RST_CTL_SREP                                    (0x40000000)
#define Tsi575_BLK_RST_CTL_SRIO                                    (0x80000000)

#ifdef __cplusplus
}
#endif

#endif /* _TS_Tsi575_H_ */