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
#include <DAR_DB_Private.h>
#include <IDT_DSF_DB_Private.h>
// Converts idt_pc_pw_t to lane count
int         pw_to_lanes[ (int)(idt_pc_pw_last)+1] = {1, 2, 4, 1, 2, 4, 0}; 
// Converts idt_pc_pw_t to string
char       *pw_to_str[(int)(idt_pc_pw_last)+1] = {"1x", "2x", "4x", "1xL0", "1xL1", "1xL2", "FAIL"}; 
// Converts lane count to idt_pc_pw_t
idt_pc_pw_t lanes_to_pw[5] = { idt_pc_pw_last, // 0, illegal
	                       idt_pc_pw_1x  , // 1
			       idt_pc_pw_2x  , // 2
			       idt_pc_pw_last, // 3, illegal
			       idt_pc_pw_4x  , // 4
			     };

// Converts lane speed to a string
char  *ls_to_str[(int)(idt_pc_ls_last )  +1] = {"1.25", "2.5", "3.125", "5.0", "6.25", "FAIL" };

// Converts reset configuration to a string
char *rst_to_str[(int)(idt_pc_rst_ignore)+1] = {"Dev ", "Port", "Int ", "PtWr", "Ignr"};

STATUS idt_pc_get_config( DAR_DEV_INFO_t           *dev_info, 
                          idt_pc_get_config_in_t   *in_parms, 
                          idt_pc_get_config_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_get_config(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


STATUS idt_pc_set_config( DAR_DEV_INFO_t           *dev_info, 
                          idt_pc_set_config_in_t   *in_parms, 
                          idt_pc_set_config_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_set_config(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


STATUS idt_pc_get_status( DAR_DEV_INFO_t           *dev_info, 
                          idt_pc_get_status_in_t   *in_parms, 
                          idt_pc_get_status_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_get_status(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


STATUS idt_pc_reset_port( DAR_DEV_INFO_t           *dev_info, 
                          idt_pc_reset_port_in_t   *in_parms, 
                          idt_pc_reset_port_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_reset_port(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


STATUS idt_pc_reset_link_partner( DAR_DEV_INFO_t                   *dev_info, 
                                  idt_pc_reset_link_partner_in_t   *in_parms, 
                                  idt_pc_reset_link_partner_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_reset_link_partner(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


STATUS idt_pc_clr_errs( DAR_DEV_INFO_t         *dev_info, 
                        idt_pc_clr_errs_in_t   *in_parms, 
                        idt_pc_clr_errs_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_clr_errs(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


STATUS idt_pc_secure_port( DAR_DEV_INFO_t            *dev_info, 
                           idt_pc_secure_port_in_t   *in_parms, 
                           idt_pc_secure_port_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_secure_port(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}

STATUS idt_pc_dev_reset_config( DAR_DEV_INFO_t                 *dev_info, 
                                idt_pc_dev_reset_config_in_t   *in_parms, 
                                idt_pc_dev_reset_config_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_dev_reset_config(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}

STATUS idt_pc_probe( DAR_DEV_INFO_t      *dev_info, 
                     idt_pc_probe_in_t   *in_parms, 
                     idt_pc_probe_out_t  *out_parms )
{
    STATUS rc = DAR_DB_INVALID_HANDLE;

    NULL_CHECK 

    if ( VALIDATE_DEV_INFO(dev_info) )
    {
        if ( IDT_DSF_INDEX(dev_info) < DAR_DB_MAX_DRIVERS )
            rc = IDT_DB[IDT_DSF_INDEX(dev_info)].idt_pc_probe(
                    dev_info, in_parms, out_parms
                 );
    }

    return rc;
}


