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
#include <IDT_DSF_DB_Private.h>
#include <IDT_Statistics_Counter_API.h>

#ifdef __cplusplus
extern "C" {
#endif

char *sc_names[(uint8_t)(idt_sc_last)+2] = {
    (char *)"Disabled  ",
    (char *)"Enabled   ",
    (char *)"UnicastReq",
    (char *)"UnicastPkt",
    (char *)"Retry   CS",
    (char *)"All     CS",
    (char *)"UC 4B Data",
    (char *)"MltcastPkt",
    (char *)"MECS    CS",
    (char *)"MC 4B Data",
    (char *)"PktAcc  CS",
    (char *)"ALL    Pkt",
    (char *)"PktNotA CS",
    (char *)"CPB    Pkt",
    (char *)"Drop   Pkt",
    (char *)"DropTTLPkt",
    (char *)"RIO    PKT",
    (char *)"FAB    PKT",
    (char *)"RIO PKTCTR",
    (char *)"FAB PKTCTR",
    (char *)"RIO TTLPKT",
    (char *)"CPL   SMSG",
    (char *)"TLP   SMSG",
    (char *)"CPL   BDMA",
    (char *)"TLP   BDMA",
    (char *)"TLP    BRG",
    (char *)"TLP    BRG",
    (char *)"PCIE NWRTR",
    (char *)"PKT   SMSG",
    (char *)"PKT   SMSG",
    (char *)"RETRY  GEN",
    (char *)"RETRY  RES",
    (char *)"PKT   BDMA",
    (char *)"RSP   BDMA",
    (char *)"PKT    BRG",
    (char *)"PKT    BRG",
    (char *)"BRG PKTERR",
    (char *)"MAINT  NWR",
    (char *)"Last      ",
    (char *)"Invalid   "
};

/* User function calls for a routing table configuration */
uint32_t idt_sc_init_dev_ctrs (
    DAR_DEV_INFO_t             *dev_info,
    idt_sc_init_dev_ctrs_in_t  *in_parms,
    idt_sc_init_dev_ctrs_out_t *out_parms )
{
    uint32_t rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK;

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_sc_init_dev_ctrs(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}

uint32_t idt_sc_read_ctrs(
    DAR_DEV_INFO_t           *dev_info,
    idt_sc_read_ctrs_in_t    *in_parms,
    idt_sc_read_ctrs_out_t   *out_parms )
{
    uint32_t rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK;

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_sc_read_ctrs(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}

#ifdef __cplusplus
}
#endif
