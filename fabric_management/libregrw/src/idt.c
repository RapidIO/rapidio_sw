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
#include CPS1848.h
#include CPS1616.h
#include RXS2448.h

#ifdef __cplusplus
extern "C" {
#endif

int idt_fill_in_handle(regrw_i *h)
{
	int rc = 0;

	switch(GET_DEV_IDENT(h)) {
	case RIO_DEVI_IDT_CPS1848:
	case RIO_DEVI_IDT_CPS1432:
	case RIO_DEVI_IDT_CPS1616:
	case RIO_DEVI_IDT_SPS1616:
		rc = idt_cps_fill_in_handle(regrw_i *h);
		break;

	case RIO_DEVI_IDT_TSI721:
		rc = idt_721_fill_in_handle(regrw_i *h);
		break;

	case RIO_DEVI_IDT_RXS2448:
	case RIO_DEVI_IDT_RXS1632:
		rc = idt_rxs_fill_in_handle(regrw_i *h);
		break;
	default: rc = -1;
		errno = ENOSYS;
		break;
	};

	return rc;
};

#ifdef __cplusplus
}
#endif

